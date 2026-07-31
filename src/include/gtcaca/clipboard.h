#ifndef _GTCACA_CLIPBOARD_H_
#define _GTCACA_CLIPBOARD_H_

#include <stddef.h>

/* ── The system clipboard ────────────────────────────────────────────────────
 *
 * A clipboard does not hold "a string" — it holds *some bytes, and a name for
 * what they are*. Every platform models it that way: UTIs on macOS, MIME
 * targets on X11 and Wayland, CF_* formats on Windows. So the primitive here is
 * (mime, bytes, length), and text is one flavour of it rather than the only
 * thing on offer. Copy a PNG out of a browser and gtcaca_clipboard_get_data()
 * with GTCACA_CLIP_PNG hands you the file, ready for gtcaca_image_load_memory().
 *
 * Routes are tried in order and the first that works wins:
 *
 *   1. the platform's clipboard — pbcopy/pbpaste and osascript on macOS,
 *      wl-copy/wl-paste or xclip/xsel on Unix, the Win32 clipboard on Windows;
 *   2. OSC 52 (text only), the escape that asks the *terminal* to hold it,
 *      which is what makes copy work over ssh and inside tmux;
 *   3. an in-process store, which is typed too, so an app can put an image on
 *      "the clipboard" and read it back even where the system has none.
 *
 * What each platform manages for non-text is uneven — Wayland and X11 take any
 * MIME type, macOS handles images through osascript, Windows uses its "PNG"
 * format — so ask gtcaca_clipboard_has() rather than assuming. */

#define GTCACA_CLIP_TEXT "text/plain;charset=utf-8"
#define GTCACA_CLIP_PNG  "image/png"
#define GTCACA_CLIP_URIS "text/uri-list"          /* dragged/copied file names */

/* One flavour of clipboard content. `data` is owned by the caller after a
   successful get and is always NUL-terminated one byte past `len`, so text can
   be used as a C string without copying. */
typedef struct {
  char   *mime;
  void   *data;
  size_t  len;
} gtcaca_clip_t;

/* Put `len` bytes of `mime` on the clipboard, replacing what was there. */
int  gtcaca_clipboard_set_data(const char *mime, const void *data, size_t len);
/* Read the clipboard as `mime`. Returns 0 and fills `out` on success. */
int  gtcaca_clipboard_get_data(const char *mime, gtcaca_clip_t *out);
void gtcaca_clip_free(gtcaca_clip_t *c);
/* Is that flavour available? Cheap enough to call before offering a "paste
   image" action. */
int  gtcaca_clipboard_has(const char *mime);

/* Text, the common case: these are thin wrappers over the two above. */
int   gtcaca_clipboard_set(const char *text, int len);   /* len < 0: strlen */
char *gtcaca_clipboard_get(void);                        /* caller frees     */

/* Which route the last call took: "system", "osc52" or "internal". */
const char *gtcaca_clipboard_backend(void);

#endif /* _GTCACA_CLIPBOARD_H_ */
