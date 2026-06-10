#ifndef _GTCACA_COMBOBOX_H_
#define _GTCACA_COMBOBOX_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>
#include <gtcaca/utarray.h>

typedef struct _gtcaca_combobox_widget_t gtcaca_combobox_widget_t;
typedef int (*gtcaca_combobox_key_cb_t)(gtcaca_combobox_widget_t *widget, int key, void *userdata);

struct _gtcaca_combobox_widget_t {
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
  struct _gtcaca_combobox_widget_t *prev;
  struct _gtcaca_combobox_widget_t *next;

  gtcaca_combobox_key_cb_t private_key_cb;
  gtcaca_combobox_key_cb_t key_cb;
  void *key_cb_userdata;

  UT_array *items;
  int selected_item;
  int active_item;  /* highlighted item in open dropdown */
  int is_open;
};

gtcaca_combobox_widget_t *gtcaca_combobox_new(gtcaca_widget_t *parent, int x, int y, int width);
void gtcaca_combobox_draw(gtcaca_combobox_widget_t *cb);
void gtcaca_combobox_append(gtcaca_combobox_widget_t *cb, const char *item);
int gtcaca_combobox_key_cb_register(gtcaca_combobox_widget_t *widget, gtcaca_combobox_key_cb_t key_cb, void *userdata);
const char *gtcaca_combobox_get_selected(gtcaca_combobox_widget_t *cb);
int gtcaca_combobox_get_selected_index(gtcaca_combobox_widget_t *cb);

#endif /* _GTCACA_COMBOBOX_H_ */
