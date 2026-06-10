#ifndef _GTCACA_PROGRESSBAR_H_
#define _GTCACA_PROGRESSBAR_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_progressbar_widget_t gtcaca_progressbar_widget_t;

struct _gtcaca_progressbar_widget_t {
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
  struct _gtcaca_progressbar_widget_t *prev;
  struct _gtcaca_progressbar_widget_t *next;

  float value;  /* 0.0 to 1.0 */
};

gtcaca_progressbar_widget_t *gtcaca_progressbar_new(gtcaca_widget_t *parent, int x, int y, int width);
void gtcaca_progressbar_draw(gtcaca_progressbar_widget_t *pb);
void gtcaca_progressbar_set_value(gtcaca_progressbar_widget_t *pb, float value);
float gtcaca_progressbar_get_value(gtcaca_progressbar_widget_t *pb);

#endif /* _GTCACA_PROGRESSBAR_H_ */
