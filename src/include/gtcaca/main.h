#ifndef _GTCACA_MAIN_H_
#define _GTCACA_MAIN_H_

#include <caca.h>

#include <gtcaca/utlist.h>

/* gtcaca main object */
struct _gmo_t {
  caca_canvas_t *cv;
  caca_display_t *dp;
};
typedef struct _gmo_t gmo_t;
gmo_t gmo;

int gtcaca_init(int *argc, char ***argv);
void gtcaca_main(void);

#endif // _GTCACA_MAIN_H_
