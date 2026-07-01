#ifndef _GTCACA_WIN_COMPAT_H_
#define _GTCACA_WIN_COMPAT_H_

/*
 * Windows (MSVC) compatibility shims for the handful of POSIX APIs the gtcaca C
 * sources use. Included only on _WIN32 (the three files that need it pull it in
 * behind #ifdef _WIN32; POSIX builds use the real <unistd.h>/<dirent.h>). This
 * keeps the Windows port contained to this one header + a few include guards.
 *
 * Covered:
 *   main.c / window.c : isatty, write, usleep, ssize_t
 *   filechooser.c     : opendir/readdir/closedir (d_name only), PATH_MAX,
 *                       S_ISDIR, strcasecmp
 */
#ifdef _WIN32

#include <windows.h>
#include <io.h>        /* _isatty, _write */
#include <stdio.h>     /* snprintf */
#include <stdlib.h>    /* malloc, free */
#include <string.h>    /* _stricmp, strncpy */
#include <sys/stat.h>  /* _S_IFDIR, struct stat, stat() */
#include <basetsd.h>   /* SSIZE_T */

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

/* --- limits (filechooser.c) --- */
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

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
