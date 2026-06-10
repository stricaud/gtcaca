#ifndef _GTCACA_ENTRY_H_
#define _GTCACA_ENTRY_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

#define GTCACA_ENTRY_MAX_LEN 255

typedef struct _gtcaca_entry_widget_t gtcaca_entry_widget_t;
typedef int (*gtcaca_entry_key_cb_t)(gtcaca_entry_widget_t *widget, int key, void *userdata);

struct _gtcaca_entry_widget_t {
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
  struct _gtcaca_entry_widget_t *prev;
  struct _gtcaca_entry_widget_t *next;

  gtcaca_entry_key_cb_t private_key_cb;
  gtcaca_entry_key_cb_t key_cb;
  void *key_cb_userdata;

  char text[GTCACA_ENTRY_MAX_LEN + 1];
  int cursor_pos;
  int scroll_offset;
  int max_length;
};

gtcaca_entry_widget_t *gtcaca_entry_new(gtcaca_widget_t *parent, int x, int y, int width);
void gtcaca_entry_draw(gtcaca_entry_widget_t *entry);
int gtcaca_entry_key_cb_register(gtcaca_entry_widget_t *widget, gtcaca_entry_key_cb_t key_cb, void *userdata);
const char *gtcaca_entry_get_text(gtcaca_entry_widget_t *entry);
void gtcaca_entry_set_text(gtcaca_entry_widget_t *entry, const char *text);

#endif /* _GTCACA_ENTRY_H_ */
