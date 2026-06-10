#ifndef _GTCACA_SWITCH_H_
#define _GTCACA_SWITCH_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_switch_widget_t gtcaca_switch_widget_t;
typedef int (*gtcaca_switch_cb_t)(gtcaca_switch_widget_t *sw, int active, void *userdata);

struct _gtcaca_switch_widget_t {
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
  struct _gtcaca_switch_widget_t *prev;
  struct _gtcaca_switch_widget_t *next;

  int active;
  gtcaca_switch_cb_t cb;
  void *cb_userdata;
};

gtcaca_switch_widget_t *gtcaca_switch_new(gtcaca_widget_t *parent, int x, int y);
void gtcaca_switch_draw(gtcaca_switch_widget_t *sw);
void gtcaca_switch_set_active(gtcaca_switch_widget_t *sw, int active);
int  gtcaca_switch_get_active(gtcaca_switch_widget_t *sw);
void gtcaca_switch_cb_register(gtcaca_switch_widget_t *sw, gtcaca_switch_cb_t cb, void *userdata);

#endif /* _GTCACA_SWITCH_H_ */
