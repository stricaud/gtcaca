#ifndef _GTCACA_FRAME_H_
#define _GTCACA_FRAME_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_frame_widget_t gtcaca_frame_widget_t;

struct _gtcaca_frame_widget_t {
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
  struct _gtcaca_frame_widget_t *prev;
  struct _gtcaca_frame_widget_t *next;

  char *label;
};

gtcaca_frame_widget_t *gtcaca_frame_new(gtcaca_widget_t *parent, const char *label, int x, int y, int width, int height);
void gtcaca_frame_draw(gtcaca_frame_widget_t *frame);

#endif /* _GTCACA_FRAME_H_ */
