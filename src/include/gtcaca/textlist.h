#ifndef _GTCACA_TEXTLIST_H_
#define _GTCACA_TEXTLIST_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

#define GTCACA_TEXTLIST_MAX 255

typedef struct _gtcaca_textlist_widget_t gtcaca_textlist_widget_t;

/*
 * Callback definitions
 */
typedef int (*gtcaca_textlist_key_cb_t)(gtcaca_textlist_widget_t *widget, int key, void *userdata);

struct _gtcaca_textlist_widget_t {
  /* Properties shared accross all widgets */
  gtcaca_widget_type_t type;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  void *children; // Any widget can be a children
  struct _gtcaca_textlist_widget_t *next;

  /* Widget callbacks */
  gtcaca_textlist_key_cb_t private_key_cb; /* Things the widget must do */
  gtcaca_textlist_key_cb_t key_cb;

  /* Now starts custom widget properties */
  unsigned int selected_item;
  char *list[GTCACA_TEXTLIST_MAX];
  unsigned int list_len;
};

/*
 * Functions
 */
gtcaca_textlist_widget_t *gtcaca_textlist_new(int x, int y);
int gtcaca_textlist_key_cb_register(gtcaca_textlist_widget_t *widget, gtcaca_textlist_key_cb_t key_cb);
void gtcaca_textlist_append(gtcaca_textlist_widget_t *textlist, char *item);
void gtcaca_textlist_selection_up(gtcaca_textlist_widget_t *textlist);
void gtcaca_textlist_selection_down(gtcaca_textlist_widget_t *textlist);
char *gtcaca_textlist_get_selected(gtcaca_textlist_widget_t *textlist);
void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist);

#endif // _GTCACA_TEXTLIST_H_
