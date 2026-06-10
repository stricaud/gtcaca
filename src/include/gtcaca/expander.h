#ifndef _GTCACA_EXPANDER_H_
#define _GTCACA_EXPANDER_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_expander_widget_t gtcaca_expander_widget_t;
typedef int (*gtcaca_expander_cb_t)(gtcaca_expander_widget_t *exp, int expanded, void *userdata);

struct _gtcaca_expander_widget_t {
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
  struct _gtcaca_expander_widget_t *prev;
  struct _gtcaca_expander_widget_t *next;

  char *label;
  int expanded;
  gtcaca_widget_t *managed[32];
  int n_managed;
  gtcaca_expander_cb_t cb;
  void *cb_userdata;
};

gtcaca_expander_widget_t *gtcaca_expander_new(gtcaca_widget_t *parent, const char *label, int x, int y, int width);
void gtcaca_expander_draw(gtcaca_expander_widget_t *exp);
void gtcaca_expander_handle_key(gtcaca_expander_widget_t *exp, int key);
void gtcaca_expander_add_managed(gtcaca_expander_widget_t *exp, gtcaca_widget_t *widget);
void gtcaca_expander_set_expanded(gtcaca_expander_widget_t *exp, int expanded);
int  gtcaca_expander_get_expanded(gtcaca_expander_widget_t *exp);
void gtcaca_expander_cb_register(gtcaca_expander_widget_t *exp, gtcaca_expander_cb_t cb, void *userdata);

#endif /* _GTCACA_EXPANDER_H_ */
