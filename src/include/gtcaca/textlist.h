#ifndef _GTCACA_TEXTLIST_H_
#define _GTCACA_TEXTLIST_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

#define GTCACA_TEXTLIST_MAX 255

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

  /* Now starts custom widget properties */
  unsigned int selected_item;
  char *list[GTCACA_TEXTLIST_MAX];
  unsigned int list_len;
};
typedef struct _gtcaca_textlist_widget_t gtcaca_textlist_widget_t;

gtcaca_textlist_widget_t *gtcaca_textlist_new(int x, int y);
void gtcaca_textlist_append(gtcaca_textlist_widget_t *textlist, char *item);
void gtcaca_textlist_selection_up(gtcaca_textlist_widget_t *textlist);
void gtcaca_textlist_selection_down(gtcaca_textlist_widget_t *textlist);
void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist);

#endif // _GTCACA_TEXTLIST_H_
