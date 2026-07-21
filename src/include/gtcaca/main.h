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
void gtcaca_present_shutdown(void);   /* restore terminal if the truecolour presenter is active */
void gtcaca_shutdown(void);           /* full teardown (free display) for custom event loops */
unsigned int gtcaca_get_newid(void);
/* Best-effort guess of whether the terminal/font can render fine Unicode glyphs
   (fractional block elements, etc.). Uses the locale + known terminal env vars;
   returns 0 (assume ASCII-safe) when unsure. */
int gtcaca_terminal_supports_blocks(void);

#endif // _GTCACA_MAIN_H_
