#ifndef _GTCACA_BUTTON_H_
#define _GTCACA_BUTTON_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_button_widget_t gtcaca_button_widget_t;

/*
 * Callback definitions
 */
typedef int (*gtcaca_button_key_cb_t)(gtcaca_button_widget_t *widget, int key, void *userdata);

struct _gtcaca_button_widget_t {

  /* Properties shared accross all widgets */
  gtcaca_widget_type_t type;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  void *children; // Any widget can be a children
  struct _gtcaca_button_widget_t *next;

  /* Widget callbacks */
  gtcaca_button_key_cb_t key_cb;
  
  /* Now starts custom widget properties */
  char *button_label;
};
typedef struct _gtcaca_button_widget_t gtcaca_button_widget_t;

gtcaca_button_widget_t *gtcaca_button_new(char *button_label, int x, int y);
int gtcaca_button_key_cb_register(gtcaca_button_widget_t *widget, gtcaca_button_key_cb_t key_cb);
void gtcaca_button_draw(gtcaca_button_widget_t *button);

#endif // _GTCACA_BUTTON_H_
