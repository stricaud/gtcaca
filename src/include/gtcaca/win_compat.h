#ifndef _GTCACA_WIN_COMPAT_H_
#define _GTCACA_WIN_COMPAT_H_

/*
 * Windows compatibility shims for the handful of POSIX APIs the gtcaca C
 * sources use. Included only on _WIN32 (the files that need it pull it in
 * behind #ifdef _WIN32; POSIX builds use the real <unistd.h>/<dirent.h>). This
 * keeps the Windows port contained to this one header + a few include guards.
 *
 * Two very different Windows toolchains reach this header:
 *
 *   MinGW-w64 (MSYS2, how the release binaries are built) ships real POSIX
 *   headers: usleep, isatty, write, ssize_t, asprintf, strcasecmp and the whole
 *   dirent API are already declared. Substituting our own there is not merely
 *   redundant, it FAILS TO COMPILE — "static declaration of 'asprintf' follows
 *   non-static declaration" — and <caca.h> transitively includes <unistd.h>, so
 *   we cannot avoid the collision by not including it ourselves. Use the
 *   toolchain's.
 *
 *   MSVC has none of them, and needs every shim below.
 *
 * realpath is the exception: absent from BOTH, so it is defined unconditionally.
 *
 * Covered:
 *   main.c / window.c : isatty, write, usleep, ssize_t
 *   iniparse.c        : asprintf
 *   filechooser.c     : opendir/readdir/closedir (d_name only), realpath,
 *                       S_ISDIR, strcasecmp
 */
#ifdef _WIN32

/* Keep <windows.h> lean + drop the min/max macros so this header is safe to
   include broadly (e.g. into iniparse.c) without clobbering other code. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>        /* MinGW: isatty, write. MSVC: _isatty, _write */
#include <stdio.h>     /* snprintf, _vscprintf, vsnprintf. MinGW: asprintf */
#include <stdlib.h>    /* malloc, free, _fullpath, _MAX_PATH */
#include <stdarg.h>    /* va_list (asprintf) */
#include <string.h>    /* _stricmp, strncpy. MinGW: strcasecmp */
#include <sys/stat.h>  /* _S_IFDIR, struct stat, stat() */
#include <basetsd.h>   /* SSIZE_T */

#if defined(__MINGW32__)   /* also defined for mingw-w64 / x86_64 */

/* MinGW-w64: take the toolchain's declarations for everything it has. */
#include <unistd.h>    /* usleep, ssize_t (and isatty/write via io.h) */
#include <dirent.h>    /* DIR, struct dirent, opendir/readdir/closedir */

#else /* MSVC */

#ifndef _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DEFINED
#endif

/* --- <unistd.h> bits (main.c, window.c) --- */
#define isatty(fd)   _isatty(fd)
#define write(fd, buf, n) _write((fd), (buf), (unsigned int)(n))
static __inline void usleep(unsigned long usec) { Sleep((DWORD)(usec / 1000UL)); }

/* --- <string.h> POSIX name (filechooser.c) --- */
#ifndef strcasecmp
#define strcasecmp(a, b) _stricmp((a), (b))
#endif

/* Deliberately do NOT define PATH_MAX here: filechooser.h defines it (4096) and
   we must not shadow it with a smaller MAX_PATH, or the curdir[PATH_MAX] struct
   field would differ across translation units. realpath() below uses _MAX_PATH. */

/* --- <sys/stat.h> POSIX macro (filechooser.c) --- */
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif

/* --- minimal <dirent.h> — filechooser.c only reads de->d_name --- */
struct dirent { char d_name[MAX_PATH]; };

typedef struct GTCACA_DIR_s {
  HANDLE           handle;
  WIN32_FIND_DATAA find;
  int              first;
  struct dirent    entry;
} DIR;

static __inline DIR *opendir(const char *path) {
  DIR  *d;
  char  pattern[MAX_PATH];
  d = (DIR *)malloc(sizeof(DIR));
  if (!d) return NULL;
  snprintf(pattern, sizeof pattern, "%s\\*", path);
  d->handle = FindFirstFileA(pattern, &d->find);
  if (d->handle == INVALID_HANDLE_VALUE) { free(d); return NULL; }
  d->first = 1;
  return d;
}

static __inline struct dirent *readdir(DIR *d) {
  if (!d->first && !FindNextFileA(d->handle, &d->find)) return NULL;
  d->first = 0;
  strncpy(d->entry.d_name, d->find.cFileName, MAX_PATH - 1);
  d->entry.d_name[MAX_PATH - 1] = '\0';
  return &d->entry;
}

static __inline int closedir(DIR *d) {
  if (d) {
    if (d->handle != INVALID_HANDLE_VALUE) FindClose(d->handle);
    free(d);
  }
  return 0;
}

#endif /* MSVC */

/* --- allocate-and-format printf (GNU/BSD; used by iniparse.c) ---
   MinGW-w64's <stdio.h> declares asprintf, but only behind
   `__USE_MINGW_ANSI_STDIO && _GNU_SOURCE` — exactly the configuration the MSYS2
   build uses, which is why defining our own there collided with it. Mirror that
   condition so we supply asprintf if and only if the toolchain does not.
   (An undefined __USE_MINGW_ANSI_STDIO evaluates to 0 in #if, as intended.) */
#if defined(__MINGW32__) && defined(_GNU_SOURCE) && __USE_MINGW_ANSI_STDIO
#  define GTCACA_HAVE_ASPRINTF 1
#endif

#ifndef GTCACA_HAVE_ASPRINTF
static __inline int asprintf(char **strp, const char *fmt, ...) {
  va_list ap;
  int len;
  va_start(ap, fmt);
  len = _vscprintf(fmt, ap);
  va_end(ap);
  if (len < 0) { *strp = NULL; return -1; }
  *strp = (char *)malloc((size_t)len + 1);
  if (!*strp) return -1;
  va_start(ap, fmt);
  len = vsnprintf(*strp, (size_t)len + 1, fmt, ap);
  va_end(ap);
  return len;
}
#endif

/* --- canonicalize a path (POSIX realpath; used by filechooser.c) ---
   Neither MSVC nor MinGW-w64 provides realpath, so this one is unconditional.
   _fullpath canonicalizes lexically (doesn't require the path to exist, which
   is fine here). With resolved==NULL it mallocs, matching POSIX.1-2008. */
static __inline char *realpath(const char *path, char *resolved) {
  return _fullpath(resolved, path, _MAX_PATH);
}

/* Turn on ANSI/VT escape processing so gtcaca's SGR + alt-screen output renders
   in modern Windows consoles (Windows 10 1511+, Windows Terminal). Call once at
   startup. No-op if not attached to a console that supports it. */
static __inline void gtcaca_win_enable_vt(void) {
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD  mode = 0;
  if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode))
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#endif /* _WIN32 */
#endif /* _GTCACA_WIN_COMPAT_H_ */
