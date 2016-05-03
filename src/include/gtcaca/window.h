#ifndef _GTCACA_WINDOW_H_
#define _GTCACA_WINDOW_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

struct _gtcaca_window_widget_t {

  /* Properties shared accross all widgets */
  gtcaca_widget_type_t type;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children; // Any widget can be a children
  struct _gtcaca_application_widget_t *prev;
  struct _gtcaca_window_widget_t *next;

  /* Now starts custom widget properties */
  char *window_title;
};
typedef struct _gtcaca_window_widget_t gtcaca_window_widget_t;

gtcaca_window_widget_t *gtcaca_window_new(gtcaca_widget_t *parent, char *window_title, int x, int y, int width, int height);
void gtcaca_window_draw(gtcaca_window_widget_t *win);
void gtcaca_window_set_focus(gtcaca_window_widget_t *win);

#endif // _GTCACA_WINDOW_H_
