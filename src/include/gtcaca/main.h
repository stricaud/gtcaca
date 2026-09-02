#ifndef _GTCACA_MAIN_H_
#define _GTCACA_MAIN_H_

#include <caca.h>

#include <gtcaca/application.h>
#include <gtcaca/log.h>
#include <gtcaca/theme.h>
#include <gtcaca/widget.h>
#include <gtcaca/utlist.h>

/* gtcaca main object */
struct _gmo_t {
  caca_canvas_t *cv;
  caca_display_t *dp;
  gtcaca_theme_t theme;
  gtcaca_log_t *log;

  unsigned int id;

  /* widgets list */
  gtcaca_widget_t *widgets_list;
};
typedef struct _gmo_t gmo_t;

extern gmo_t gmo;

/* Directory holding the running executable, or NULL if it cannot be determined.
   An application that ships data files beside its binary — grammars, themes,
   help text — resolves them against this rather than against a prefix baked in
   at compile time, which is what makes a relocatable bundle work. */
const char *gtcaca_executable_dir(void);

/* Key-event tag: a printable non-ASCII Unicode codepoint is delivered to key
   callbacks as (codepoint | GTCACA_KEY_UNICODE). The flag keeps codepoints in
   the Latin-Extended range (U+0111 'đ', U+0119 'ę', …) from being mistaken for
   the CACA_KEY_* special keys they collide with numerically (CACA_KEY_UP=0x111).
   Bit 30 sits far above both Unicode (max U+10FFFF) and every CACA_KEY_* value. */
#define GTCACA_KEY_UNICODE 0x40000000
#define GTCACA_KEY_IS_UNICODE(k) (((k) & GTCACA_KEY_UNICODE) != 0)
#define GTCACA_KEY_CODEPOINT(k)  ((uint32_t)((k) & ~GTCACA_KEY_UNICODE))

int gtcaca_init(int *argc, char ***argv);
void gtcaca_main(void);
void gtcaca_main_quit(void);
void gtcaca_redraw(void);
/* Clear the whole canvas (custom render loops call this at the top of a frame,
 * then draw individual widgets, then gtcaca_refresh). */
void gtcaca_clear_screen(void);
/* Flush the canvas to the terminal without redrawing every widget. Pair with
 * per-widget draw calls to paint exactly what you want (cf. gtcaca_redraw,
 * which paints the whole widget list). */
void gtcaca_refresh(void);
/* Draw a single widget, dispatching to its type-specific draw. Lets a custom
 * render loop paint exactly the widgets it wants without switching on type. */
void gtcaca_widget_draw(gtcaca_widget_t *widget);
void gtcaca_present_shutdown(void);   /* restore terminal if the truecolour presenter is active */
void gtcaca_shutdown(void);           /* full teardown (free display) for custom event loops */
unsigned int gtcaca_get_newid(void);
/* Best-effort guess of whether the terminal/font can render fine Unicode glyphs
   (fractional block elements, etc.). Uses the locale + known terminal env vars;
   returns 0 (assume ASCII-safe) when unsure. */
int gtcaca_terminal_supports_blocks(void);

/* How close together two presses must be to count as a double click, in
   milliseconds (default 400). Applies to GTCACA_MOUSE_DOUBLE. */
void gtcaca_set_double_click_time(int ms);
int  gtcaca_get_double_click_time(void);

/* ── mouse modifiers ────────────────────────────────────────────────────────
 * Which of Shift/Alt/Ctrl was down for the mouse event being delivered right
 * now. An SGR report carries them in its button byte; gtcaca strips them out
 * before working out which button was pressed, and keeps them here so a mouse
 * callback can tell a plain click from a Shift-click (extend a selection),
 * a Ctrl-click, and so on.
 *
 * Call it from inside a gtcaca_custom_mouse_cb_t: the value belongs to the
 * event in flight and is cleared once the callback returns. Outside one it
 * reads 0. It stays 0 on terminals that keep modified clicks for themselves
 * (xterm and most of its imitators use Shift-click for their own selection
 * when mouse reporting is on), so treat a modifier as a shortcut for something
 * the user can also do another way rather than the only route to it. */
enum {
  GTCACA_MOD_SHIFT = 0x04,
  GTCACA_MOD_ALT   = 0x08,   /* Meta/Option */
  GTCACA_MOD_CTRL  = 0x10
};
int gtcaca_mouse_modifiers(void);

/* ── pasting ────────────────────────────────────────────────────────────────
 * Terminals deliver a paste as plain keystrokes, so a 2000-line paste arrives
 * as 150,000 key events — and an editor that inserts one character at a time
 * spends O(n²) doing it. gtcaca turns on the terminal's *bracketed paste* mode,
 * collects the whole block, and hands it over in one piece, which makes a paste
 * a single edit (and stops auto-indent from cascading through pasted code).
 *
 * Register a callback to decide what a paste means in your app; without one the
 * text is inserted into the focused editor, if there is one. */
typedef void (*gtcaca_paste_cb_t)(const char *text, int len, void *userdata);
/* Does the Del key erase forward on this terminal? See main.c: 0x7f is Del
   unless this tty's erase character is not 0x7f, in which case 0x7f is its
   Backspace. Widgets use this to keep the two keys apart. */
int gtcaca_del_deletes_forward(void);

void gtcaca_set_paste_cb(gtcaca_paste_cb_t cb, void *userdata);

#endif // _GTCACA_MAIN_H_
