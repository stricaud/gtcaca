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

int gtcaca_init(int *argc, char ***argv);
void gtcaca_main(void);
void gtcaca_main_quit(void);
void gtcaca_redraw(void);
unsigned int gtcaca_get_newid(void);
/* Best-effort guess of whether the terminal/font can render fine Unicode glyphs
   (fractional block elements, etc.). Uses the locale + known terminal env vars;
   returns 0 (assume ASCII-safe) when unsure. */
int gtcaca_terminal_supports_blocks(void);

#endif // _GTCACA_MAIN_H_
