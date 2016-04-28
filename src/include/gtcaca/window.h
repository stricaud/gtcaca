#ifndef _GTCACA_WINDOW_H_
#define _GTCACA_WINDOW_H_

struct _gtcaca_window_widget_t {
  int has_focus;
  int is_visible;
  char *window_title;
  int x;
  int y;
  int width;
  int height;
  struct _gtcaca_window_widget_t *next;
};
typedef struct _gtcaca_window_widget_t gtcaca_window_widget_t;

void gtcaca_window_new(caca_canvas_t *cv, char *window_title, int x, int y, int width, int height);

#endif // _GTCACA_WINDOW_H_
