/* clipboard.c — typed clipboard access, and whatever this machine can manage.
 *
 * Deliberately dependency-free: the Unix backends shell out to the usual small
 * helpers rather than linking X11, Wayland or Cocoa, and Windows uses the Win32
 * API it already has. Everything falls back to OSC 52 for text (which works
 * over ssh) and then to an in-process store, so copy/paste never silently does
 * nothing — and the in-process store is typed too, so images still round-trip
 * inside one app on a machine with no clipboard at all. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <gtcaca/win_compat.h>
#else
#include <unistd.h>
#endif

#include <gtcaca/clipboard.h>

static const char *g_backend = "internal";

const char *gtcaca_clipboard_backend(void) { return g_backend; }

void gtcaca_clip_free(gtcaca_clip_t *c)
{
  if (!c) return;
  free(c->mime);
  free(c->data);
  c->mime = NULL;
  c->data = NULL;
  c->len = 0;
}

/* ── the in-process store ───────────────────────────────────────────────────
   Holds one item at a time, mirroring what a real clipboard does. */

static gtcaca_clip_t g_local;

static void *dupbytes(const void *p, size_t len)
{
  char *copy = malloc(len + 1);
  if (!copy) return NULL;
  if (len) memcpy(copy, p, len);
  copy[len] = '\0';                       /* text can be used as a C string */
  return copy;
}

static void local_set(const char *mime, const void *data, size_t len)
{
  char *m = mime ? strdup(mime) : NULL;
  void *d = dupbytes(data, len);
  if (!m || !d) { free(m); free(d); return; }
  gtcaca_clip_free(&g_local);
  g_local.mime = m;
  g_local.data = d;
  g_local.len = len;
}

static int mime_eq(const char *a, const char *b)
{
  if (!a || !b) return 0;
  if (strcmp(a, b) == 0) return 1;
  /* "text/plain" and "text/plain;charset=utf-8" are the same thing. */
  { size_t na = strcspn(a, ";"), nb = strcspn(b, ";");
    return na == nb && strncmp(a, b, na) == 0; }
}

static int is_text(const char *mime) { return mime_eq(mime, GTCACA_CLIP_TEXT); }

/* ── OSC 52: ask the terminal to hold it (text only) ────────────────────────*/

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int osc52_set(const char *text, size_t len)
{
  size_t olen = (len + 2) / 3 * 4;
  char *b64;
  size_t i;
  int o = 0;

  if (!len || len > 100000) return -1;          /* beyond what terminals take */
  b64 = malloc(olen + 1);
  if (!b64) return -1;
  for (i = 0; i + 2 < len; i += 3) {
    unsigned v = ((unsigned char)text[i] << 16) | ((unsigned char)text[i+1] << 8)
               | (unsigned char)text[i+2];
    b64[o++] = B64[(v >> 18) & 63]; b64[o++] = B64[(v >> 12) & 63];
    b64[o++] = B64[(v >> 6) & 63];  b64[o++] = B64[v & 63];
  }
  if (i < len) {
    unsigned v = (unsigned char)text[i] << 16;
    size_t rest = len - i;
    if (rest == 2) v |= (unsigned char)text[i+1] << 8;
    b64[o++] = B64[(v >> 18) & 63];
    b64[o++] = B64[(v >> 12) & 63];
    b64[o++] = rest == 2 ? B64[(v >> 6) & 63] : '=';
    b64[o++] = '=';
  }
  b64[o] = '\0';
  fprintf(stdout, "\033]52;c;%s\a", b64);
  fflush(stdout);
  free(b64);
  return 0;
}

/* ── the platform ───────────────────────────────────────────────────────────*/

#ifdef _WIN32

/* Windows names its formats; PNG is a registered one that browsers and image
   editors agree on. */
static UINT win_format(const char *mime)
{
  if (is_text(mime)) return CF_UNICODETEXT;
  if (mime_eq(mime, GTCACA_CLIP_PNG)) return RegisterClipboardFormatA("PNG");
  return 0;
}

static int system_set(const char *mime, const void *data, size_t len)
{
  UINT fmt = win_format(mime);
  HGLOBAL h;
  void *p;
  if (!fmt || !OpenClipboard(NULL)) return -1;
  EmptyClipboard();
  if (fmt == CF_UNICODETEXT) {
    int wide = MultiByteToWideChar(CP_UTF8, 0, (const char *)data, (int)len, NULL, 0);
    h = GlobalAlloc(GMEM_MOVEABLE, ((SIZE_T)wide + 1) * sizeof(WCHAR));
    if (!h) { CloseClipboard(); return -1; }
    p = GlobalLock(h);
    MultiByteToWideChar(CP_UTF8, 0, (const char *)data, (int)len, (LPWSTR)p, wide);
    ((WCHAR *)p)[wide] = 0;
  } else {
    h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)len);
    if (!h) { CloseClipboard(); return -1; }
    p = GlobalLock(h);
    memcpy(p, data, len);
  }
  GlobalUnlock(h);
  SetClipboardData(fmt, h);
  CloseClipboard();
  return 0;
}

static int system_get(const char *mime, gtcaca_clip_t *out)
{
  UINT fmt = win_format(mime);
  HANDLE h;
  int rc = -1;
  if (!fmt || !OpenClipboard(NULL)) return -1;
  h = GetClipboardData(fmt);
  if (h) {
    const void *p = GlobalLock(h);
    if (p) {
      if (fmt == CF_UNICODETEXT) {
        int n = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)p, -1, NULL, 0, NULL, NULL);
        char *utf8 = malloc((size_t)n + 1);
        if (utf8) {
          WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)p, -1, utf8, n, NULL, NULL);
          out->data = utf8; out->len = (size_t)(n > 0 ? n - 1 : 0);
          out->mime = strdup(mime); rc = 0;
        }
      } else {
        SIZE_T n = GlobalSize(h);
        out->data = dupbytes(p, (size_t)n);
        out->len = (size_t)n;
        out->mime = strdup(mime);
        rc = out->data ? 0 : -1;
      }
      GlobalUnlock(h);
    }
  }
  CloseClipboard();
  return rc;
}

static int system_has(const char *mime)
{
  UINT fmt = win_format(mime);
  return fmt && IsClipboardFormatAvailable(fmt);
}

#else  /* Unix: hand it to whichever helper is installed */

static int have(const char *cmd)
{
  char probe[128];
  snprintf(probe, sizeof probe, "command -v %s >/dev/null 2>&1", cmd);
  return system(probe) == 0;
}

enum { HELPER_NONE = 0, HELPER_MAC, HELPER_WAYLAND, HELPER_XCLIP, HELPER_XSEL };

static int helper(void)
{
  static int cached = -1;
  if (cached >= 0) return cached;
  if (have("pbcopy"))       cached = HELPER_MAC;
  else if (have("wl-copy")) cached = HELPER_WAYLAND;
  else if (have("xclip"))   cached = HELPER_XCLIP;
  else if (have("xsel"))    cached = HELPER_XSEL;
  else                      cached = HELPER_NONE;
  return cached;
}

/* Run `cmd`, feeding it `data`. */
static int pipe_out(const char *cmd, const void *data, size_t len)
{
  FILE *f = popen(cmd, "w");
  int rc;
  if (!f) return -1;
  if (len && fwrite(data, 1, len, f) != len) { pclose(f); return -1; }
  rc = pclose(f);
  return rc == 0 ? 0 : -1;
}

/* Run `cmd` and collect everything it prints. */
static int pipe_in(const char *cmd, void **out, size_t *out_len)
{
  FILE *f = popen(cmd, "r");
  char *buf = NULL;
  size_t cap = 0, len = 0;
  if (!f) return -1;
  for (;;) {
    size_t got;
    if (len + 65536 + 1 > cap) {
      size_t ncap = cap ? cap * 2 : 131072;
      char *bigger = realloc(buf, ncap);
      if (!bigger) break;
      buf = bigger; cap = ncap;
    }
    got = fread(buf + len, 1, 65536, f);
    len += got;
    if (got < 65536) break;
  }
  pclose(f);
  if (!buf || !len) { free(buf); return -1; }
  buf[len] = '\0';
  *out = buf;
  *out_len = len;
  return 0;
}

/* macOS keeps images under its own type names, reachable through osascript via
   a temp file — clumsy, but it is the only route that needs no extra install. */
static int mac_image_set(const void *data, size_t len)
{
  char path[] = "/tmp/gtcaca-clip-XXXXXX";
  char cmd[512];
  int fd = mkstemp(path), rc;
  FILE *f;
  if (fd < 0) return -1;
  f = fdopen(fd, "wb");
  if (!f) { close(fd); unlink(path); return -1; }
  if (fwrite(data, 1, len, f) != len) { fclose(f); unlink(path); return -1; }
  fclose(f);
  snprintf(cmd, sizeof cmd,
           "osascript -e 'set the clipboard to (read (POSIX file \"%s\") as "
           "\xc2\xab" "class PNGf" "\xc2\xbb)' >/dev/null 2>&1", path);
  rc = system(cmd);
  unlink(path);
  return rc == 0 ? 0 : -1;
}

static int mac_image_get(void **out, size_t *out_len)
{
  char path[] = "/tmp/gtcaca-clip-XXXXXX";
  char cmd[768];
  int fd = mkstemp(path);
  FILE *f;
  long size;
  if (fd < 0) return -1;
  close(fd);
  snprintf(cmd, sizeof cmd,
           "osascript -e 'set f to open for access POSIX file \"%s\" with write permission' "
           "-e 'write (the clipboard as \xc2\xab" "class PNGf" "\xc2\xbb) to f' "
           "-e 'close access f' >/dev/null 2>&1", path);
  if (system(cmd) != 0) { unlink(path); return -1; }
  f = fopen(path, "rb");
  if (!f) { unlink(path); return -1; }
  fseek(f, 0, SEEK_END); size = ftell(f); fseek(f, 0, SEEK_SET);
  if (size <= 0) { fclose(f); unlink(path); return -1; }
  *out = malloc((size_t)size + 1);
  if (!*out) { fclose(f); unlink(path); return -1; }
  *out_len = fread(*out, 1, (size_t)size, f);
  ((char *)*out)[*out_len] = '\0';
  fclose(f);
  unlink(path);
  return *out_len ? 0 : -1;
}

static int system_set(const char *mime, const void *data, size_t len)
{
  char cmd[256];
  switch (helper()) {
  case HELPER_MAC:
    if (is_text(mime)) return pipe_out("pbcopy", data, len);
    if (mime_eq(mime, GTCACA_CLIP_PNG)) return mac_image_set(data, len);
    return -1;
  case HELPER_WAYLAND:
    snprintf(cmd, sizeof cmd, "wl-copy --type '%s'", mime);
    return pipe_out(cmd, data, len);
  case HELPER_XCLIP:
    snprintf(cmd, sizeof cmd, "xclip -selection clipboard -t '%s' -i", mime);
    return pipe_out(cmd, data, len);
  case HELPER_XSEL:
    if (!is_text(mime)) return -1;              /* xsel is text-only */
    return pipe_out("xsel --clipboard --input", data, len);
  default:
    return -1;
  }
}

static int system_get(const char *mime, gtcaca_clip_t *out)
{
  char cmd[256];
  void *data = NULL;
  size_t len = 0;
  int rc = -1;

  switch (helper()) {
  case HELPER_MAC:
    if (is_text(mime))                       rc = pipe_in("pbpaste", &data, &len);
    else if (mime_eq(mime, GTCACA_CLIP_PNG)) rc = mac_image_get(&data, &len);
    break;
  case HELPER_WAYLAND:
    snprintf(cmd, sizeof cmd, "wl-paste --no-newline --type '%s' 2>/dev/null", mime);
    rc = pipe_in(cmd, &data, &len);
    break;
  case HELPER_XCLIP:
    snprintf(cmd, sizeof cmd, "xclip -selection clipboard -t '%s' -o 2>/dev/null", mime);
    rc = pipe_in(cmd, &data, &len);
    break;
  case HELPER_XSEL:
    if (is_text(mime)) rc = pipe_in("xsel --clipboard --output", &data, &len);
    break;
  default:
    break;
  }
  if (rc != 0) return -1;
  out->mime = strdup(mime);
  out->data = data;
  out->len = len;
  return 0;
}

static int system_has(const char *mime)
{
  char cmd[256];
  switch (helper()) {
  case HELPER_MAC:
    if (is_text(mime)) return 1;
    if (mime_eq(mime, GTCACA_CLIP_PNG))
      return system("osascript -e 'clipboard info' 2>/dev/null | grep -qi 'PNGf'") == 0;
    return 0;
  case HELPER_WAYLAND:
    snprintf(cmd, sizeof cmd, "wl-paste --list-types 2>/dev/null | grep -qx '%s'", mime);
    return system(cmd) == 0;
  case HELPER_XCLIP:
    snprintf(cmd, sizeof cmd,
             "xclip -selection clipboard -t TARGETS -o 2>/dev/null | grep -qx '%s'", mime);
    return system(cmd) == 0;
  case HELPER_XSEL:
    return is_text(mime);
  default:
    return 0;
  }
}

#endif

/* ── the public surface ─────────────────────────────────────────────────────*/

int gtcaca_clipboard_set_data(const char *mime, const void *data, size_t len)
{
  if (!mime || !data) return -1;
  local_set(mime, data, len);                   /* always keep our own copy */

  if (system_set(mime, data, len) == 0) {
    g_backend = "system";
    if (is_text(mime)) osc52_set((const char *)data, len);   /* and the terminal */
    return 0;
  }
  if (is_text(mime) && osc52_set((const char *)data, len) == 0) {
    g_backend = "osc52";
    return 0;
  }
  g_backend = "internal";
  return 0;                                     /* the in-process store has it */
}

int gtcaca_clipboard_get_data(const char *mime, gtcaca_clip_t *out)
{
  if (!mime || !out) return -1;
  memset(out, 0, sizeof *out);
  if (system_get(mime, out) == 0) { g_backend = "system"; return 0; }
  if (g_local.data && mime_eq(g_local.mime, mime)) {
    out->mime = strdup(g_local.mime);
    out->data = dupbytes(g_local.data, g_local.len);
    out->len = g_local.len;
    g_backend = "internal";
    return out->data ? 0 : -1;
  }
  return -1;
}

int gtcaca_clipboard_has(const char *mime)
{
  if (!mime) return 0;
  if (system_has(mime)) return 1;
  return g_local.data && mime_eq(g_local.mime, mime);
}

int gtcaca_clipboard_set(const char *text, int len)
{
  if (!text) return -1;
  if (len < 0) len = (int)strlen(text);
  return gtcaca_clipboard_set_data(GTCACA_CLIP_TEXT, text, (size_t)len);
}

char *gtcaca_clipboard_get(void)
{
  gtcaca_clip_t c;
  if (gtcaca_clipboard_get_data(GTCACA_CLIP_TEXT, &c) != 0) return NULL;
  free(c.mime);
  return (char *)c.data;                        /* NUL-terminated by dupbytes */
}
