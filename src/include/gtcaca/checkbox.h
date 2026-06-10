#ifndef _GTCACA_CHECKBOX_H_
#define _GTCACA_CHECKBOX_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_checkbox_widget_t gtcaca_checkbox_widget_t;
typedef int (*gtcaca_checkbox_key_cb_t)(gtcaca_checkbox_widget_t *widget, int key, void *userdata);

struct _gtcaca_checkbox_widget_t {
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
  struct _gtcaca_checkbox_widget_t *prev;
  struct _gtcaca_checkbox_widget_t *next;

  gtcaca_checkbox_key_cb_t private_key_cb;
  gtcaca_checkbox_key_cb_t key_cb;
  void *key_cb_userdata;

  char *label;
  int checked;
};

gtcaca_checkbox_widget_t *gtcaca_checkbox_new(gtcaca_widget_t *parent, const char *label, int x, int y);
void gtcaca_checkbox_draw(gtcaca_checkbox_widget_t *checkbox);
int gtcaca_checkbox_key_cb_register(gtcaca_checkbox_widget_t *widget, gtcaca_checkbox_key_cb_t key_cb, void *userdata);
int gtcaca_checkbox_get_checked(gtcaca_checkbox_widget_t *checkbox);
void gtcaca_checkbox_set_checked(gtcaca_checkbox_widget_t *checkbox, int checked);

#endif /* _GTCACA_CHECKBOX_H_ */
