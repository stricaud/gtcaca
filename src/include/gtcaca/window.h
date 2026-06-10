#ifndef _GTCACA_WINDOW_H_
#define _GTCACA_WINDOW_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

struct _gtcaca_window_widget_t {

  /* Properties shared accross all widgets */
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
  struct _gtcaca_window_widget_t *prev;
  struct _gtcaca_window_widget_t *next;

  /* Custom widget properties */
  char *window_title;
  gtcaca_widget_t *focused_child;
};
typedef struct _gtcaca_window_widget_t gtcaca_window_widget_t;

gtcaca_window_widget_t *gtcaca_window_new(gtcaca_widget_t *parent, char *window_title, int x, int y, int width, int height);
void gtcaca_window_draw(gtcaca_window_widget_t *win);
void gtcaca_window_set_focus(gtcaca_window_widget_t *win);
gtcaca_window_widget_t *gtcaca_window_get_current_focus(void);
gtcaca_window_widget_t *gtcaca_window_get_next(gtcaca_window_widget_t *win);
gtcaca_window_widget_t *gtcaca_window_get_prev(gtcaca_window_widget_t *win);
void gtcaca_window_set_focused_child(gtcaca_window_widget_t *win, gtcaca_widget_t *child);
void gtcaca_window_focus_next_child(gtcaca_window_widget_t *win);
void gtcaca_window_focus_prev_child(gtcaca_window_widget_t *win);

#endif /* _GTCACA_WINDOW_H_ */
