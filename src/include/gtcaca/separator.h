#ifndef _GTCACA_SEPARATOR_H_
#define _GTCACA_SEPARATOR_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_separator_widget_t gtcaca_separator_widget_t;

struct _gtcaca_separator_widget_t {
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
  struct _gtcaca_separator_widget_t *prev;
  struct _gtcaca_separator_widget_t *next;

  int vertical;
};

gtcaca_separator_widget_t *gtcaca_separator_new(gtcaca_widget_t *parent, int x, int y, int length, int vertical);
void gtcaca_separator_draw(gtcaca_separator_widget_t *sep);

#endif /* _GTCACA_SEPARATOR_H_ */
