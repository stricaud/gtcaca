#ifndef _GTCACA_WINDOW_H_
#define _GTCACA_WINDOW_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef enum {
  GTCACA_WINDOW_ANIM_NONE = 0,
  GTCACA_WINDOW_ANIM_SHRINK,
} gtcaca_window_close_anim_t;

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

  /* Widget activated when Enter is pressed and the focused child does not
     consume Enter itself (e.g. the default button of a form). */
  gtcaca_widget_t *default_widget;

  /* Close animation */
  gtcaca_window_close_anim_t  close_anim;
  gtcaca_widget_t            *close_anim_target;
};
typedef struct _gtcaca_window_widget_t gtcaca_window_widget_t;

gtcaca_window_widget_t *gtcaca_window_new(gtcaca_widget_t *parent, char *window_title, int x, int y, int width, int height);
/* Like gtcaca_window_new, but centred on the canvas (a pop-up / dialog). */
gtcaca_window_widget_t *gtcaca_window_new_centered(gtcaca_widget_t *parent, char *window_title, int width, int height);
void gtcaca_window_draw(gtcaca_window_widget_t *win);
void gtcaca_window_set_focus(gtcaca_window_widget_t *win);
gtcaca_window_widget_t *gtcaca_window_get_current_focus(void);
gtcaca_window_widget_t *gtcaca_window_get_next(gtcaca_window_widget_t *win);
gtcaca_window_widget_t *gtcaca_window_get_prev(gtcaca_window_widget_t *win);
void gtcaca_window_set_focused_child(gtcaca_window_widget_t *win, gtcaca_widget_t *child);
void gtcaca_window_set_default(gtcaca_window_widget_t *win, gtcaca_widget_t *widget);
void gtcaca_window_focus_next_child(gtcaca_window_widget_t *win);
void gtcaca_window_focus_prev_child(gtcaca_window_widget_t *win);
void gtcaca_window_set_close_animation(gtcaca_window_widget_t *win, gtcaca_window_close_anim_t anim, gtcaca_widget_t *target);
void gtcaca_window_close(gtcaca_window_widget_t *win);

#endif /* _GTCACA_WINDOW_H_ */
