#ifndef _GTCACA_TEXTLIST_H_
#define _GTCACA_TEXTLIST_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

#include <gtcaca/utarray.h>

#define GTCACA_TEXTLIST_MAX 1024

typedef struct _gtcaca_textlist_widget_t gtcaca_textlist_widget_t;

/*
 * Callback definitions
 */
typedef int (*gtcaca_textlist_key_cb_t)(gtcaca_textlist_widget_t *widget, int key, void *userdata);

struct _gtcaca_textlist_widget_t {
  /* Properties shared accross all widgets */
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children; // Any widget can be a children
  struct _gtcaca_application_widget_t *prev;
  struct _gtcaca_textlist_widget_t *next;

  /* Widget callbacks */
  gtcaca_textlist_key_cb_t private_key_cb; /* Things the widget must do */
  gtcaca_textlist_key_cb_t key_cb;
  void *key_cb_userdata;

  /* Now starts custom widget properties */
  unsigned int selected_item;
  UT_array *list;
  
  // When we show less than what the widget has
  unsigned int view_size;
};

/*
 * Functions
 */
gtcaca_textlist_widget_t *gtcaca_textlist_new(gtcaca_widget_t *parent, int x, int y);
void gtcaca_textlist_widget_set_view_size(gtcaca_textlist_widget_t *widget, unsigned int view_size);
int gtcaca_textlist_key_cb_register(gtcaca_textlist_widget_t *widget, gtcaca_textlist_key_cb_t key_cb, void *userdata);
void gtcaca_textlist_append(gtcaca_textlist_widget_t *textlist, char *item);
void gtcaca_textlist_selection_up(gtcaca_textlist_widget_t *textlist);
void gtcaca_textlist_selection_down(gtcaca_textlist_widget_t *textlist);
char *gtcaca_textlist_get_selected(gtcaca_textlist_widget_t *textlist);
void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist);

#endif // _GTCACA_TEXTLIST_H_
