#ifndef _GTCACA_SPINNER_H_
#define _GTCACA_SPINNER_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_spinner_widget_t gtcaca_spinner_widget_t;

struct _gtcaca_spinner_widget_t {
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  uint8_t color_focus_fg;
  uint8_t color_focus_bg;
  uint8_t color_nonfocus_fg;
  uint8_t color_nonfocus_bg;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children;
  struct _gtcaca_spinner_widget_t *prev;
  struct _gtcaca_spinner_widget_t *next;

  int frame;
  int spinning;
};

gtcaca_spinner_widget_t *gtcaca_spinner_new(gtcaca_widget_t *parent, int x, int y);
void gtcaca_spinner_draw(gtcaca_spinner_widget_t *sp);
void gtcaca_spinner_set_spinning(gtcaca_spinner_widget_t *sp, int spinning);
void gtcaca_spinner_step(gtcaca_spinner_widget_t *sp);

#endif /* _GTCACA_SPINNER_H_ */
