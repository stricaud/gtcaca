#ifndef _GTCACA_SPINBUTTON_H_
#define _GTCACA_SPINBUTTON_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_spinbutton_widget_t gtcaca_spinbutton_widget_t;
typedef int (*gtcaca_spinbutton_cb_t)(gtcaca_spinbutton_widget_t *sb, double value, void *userdata);

struct _gtcaca_spinbutton_widget_t {
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
  struct _gtcaca_spinbutton_widget_t *prev;
  struct _gtcaca_spinbutton_widget_t *next;

  double value;
  double min;
  double max;
  double step;
  int digits;
  gtcaca_spinbutton_cb_t cb;
  void *cb_userdata;
};

gtcaca_spinbutton_widget_t *gtcaca_spinbutton_new(gtcaca_widget_t *parent, int x, int y, double min, double max, double step);
void gtcaca_spinbutton_draw(gtcaca_spinbutton_widget_t *sb);
void gtcaca_spinbutton_handle_key(gtcaca_spinbutton_widget_t *sb, int key);
void gtcaca_spinbutton_set_value(gtcaca_spinbutton_widget_t *sb, double value);
double gtcaca_spinbutton_get_value(gtcaca_spinbutton_widget_t *sb);
void gtcaca_spinbutton_cb_register(gtcaca_spinbutton_widget_t *sb, gtcaca_spinbutton_cb_t cb, void *userdata);

#endif /* _GTCACA_SPINBUTTON_H_ */
