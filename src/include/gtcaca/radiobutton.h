#ifndef _GTCACA_RADIOBUTTON_H_
#define _GTCACA_RADIOBUTTON_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_radiobutton_widget_t gtcaca_radiobutton_widget_t;
typedef int (*gtcaca_radiobutton_key_cb_t)(gtcaca_radiobutton_widget_t *widget, int key, void *userdata);

struct _gtcaca_radiobutton_widget_t {
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
  struct _gtcaca_radiobutton_widget_t *prev;
  struct _gtcaca_radiobutton_widget_t *next;

  gtcaca_radiobutton_key_cb_t private_key_cb;
  gtcaca_radiobutton_key_cb_t key_cb;
  void *key_cb_userdata;

  char *label;
  int active;
  int group_id;
};

gtcaca_radiobutton_widget_t *gtcaca_radiobutton_new(gtcaca_widget_t *parent, const char *label, int group_id, int x, int y);
void gtcaca_radiobutton_draw(gtcaca_radiobutton_widget_t *rb);
int gtcaca_radiobutton_key_cb_register(gtcaca_radiobutton_widget_t *widget, gtcaca_radiobutton_key_cb_t key_cb, void *userdata);
int gtcaca_radiobutton_get_active(gtcaca_radiobutton_widget_t *rb);
void gtcaca_radiobutton_set_active(gtcaca_radiobutton_widget_t *rb);

#endif /* _GTCACA_RADIOBUTTON_H_ */
