#ifndef _GTCACA_LABEL_H_
#define _GTCACA_LABEL_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

struct _gtcaca_label_widget_t {

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
  struct _gtcaca_label_widget_t *next;
  
  /* Now starts custom widget properties */
  char *label;
};
typedef struct _gtcaca_label_widget_t gtcaca_label_widget_t;

gtcaca_label_widget_t *gtcaca_label_new(gtcaca_widget_t *parent, char *text, int x, int y);
void gtcaca_label_draw(gtcaca_label_widget_t *label);

#endif // _GTCACA_LABEL_H_
