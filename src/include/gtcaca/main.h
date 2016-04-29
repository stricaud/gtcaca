#ifndef _GTCACA_MAIN_H_
#define _GTCACA_MAIN_H_

#include <caca.h>

#include <gtcaca/theme.h>
#include <gtcaca/window.h>
#include <gtcaca/utlist.h>

/* gtcaca main object */
struct _gmo_t {
  caca_canvas_t *cv;
  caca_display_t *dp;
  gtcaca_theme_t theme;

  /* widgets list */
  gtcaca_widget_t *widgets_list;
};
typedef struct _gmo_t gmo_t;
gmo_t gmo;


int gtcaca_init(int *argc, char ***argv);
void gtcaca_main(void);
void gtcaca_redraw(void);

#endif // _GTCACA_MAIN_H_
