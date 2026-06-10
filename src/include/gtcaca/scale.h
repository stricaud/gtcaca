#ifndef _GTCACA_SCALE_H_
#define _GTCACA_SCALE_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_scale_widget_t gtcaca_scale_widget_t;
typedef int (*gtcaca_scale_cb_t)(gtcaca_scale_widget_t *scale, double value, void *userdata);

struct _gtcaca_scale_widget_t {
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
  struct _gtcaca_scale_widget_t *prev;
  struct _gtcaca_scale_widget_t *next;

  double value;
  double min;
  double max;
  double step;
  gtcaca_scale_cb_t cb;
  void *cb_userdata;
};

gtcaca_scale_widget_t *gtcaca_scale_new(gtcaca_widget_t *parent, int x, int y, int width, double min, double max, double step);
void gtcaca_scale_draw(gtcaca_scale_widget_t *scale);
void gtcaca_scale_handle_key(gtcaca_scale_widget_t *scale, int key);
void gtcaca_scale_set_value(gtcaca_scale_widget_t *scale, double value);
double gtcaca_scale_get_value(gtcaca_scale_widget_t *scale);
void gtcaca_scale_cb_register(gtcaca_scale_widget_t *scale, gtcaca_scale_cb_t cb, void *userdata);

#endif /* _GTCACA_SCALE_H_ */
