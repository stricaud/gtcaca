#ifndef _GTCACA_TEXTVIEW_H_
#define _GTCACA_TEXTVIEW_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>
#include <gtcaca/utarray.h>

typedef struct _gtcaca_textview_widget_t gtcaca_textview_widget_t;
typedef int (*gtcaca_textview_key_cb_t)(gtcaca_textview_widget_t *widget, int key, void *userdata);

/* Syntax-highlight modes for the textview. */
typedef enum {
  GTCACA_TEXTVIEW_MODE_PLAIN = 0,  /* no highlighting (default) */
  GTCACA_TEXTVIEW_MODE_JSON,       /* JSON: keys cyan, strings green,
                                      numbers magenta, booleans/null red,
                                      braces/brackets yellow */
} gtcaca_textview_mode_t;

struct _gtcaca_textview_widget_t {
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
  struct _gtcaca_textview_widget_t *prev;
  struct _gtcaca_textview_widget_t *next;

  gtcaca_textview_key_cb_t private_key_cb;
  gtcaca_textview_key_cb_t key_cb;
  void *key_cb_userdata;

  UT_array *lines;
  int scroll_pos;
  gtcaca_textview_mode_t mode;
};

gtcaca_textview_widget_t *gtcaca_textview_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_textview_draw(gtcaca_textview_widget_t *tv);
void gtcaca_textview_append(gtcaca_textview_widget_t *tv, const char *line);
void gtcaca_textview_clear(gtcaca_textview_widget_t *tv);
void gtcaca_textview_set_mode(gtcaca_textview_widget_t *tv, gtcaca_textview_mode_t mode);
int gtcaca_textview_key_cb_register(gtcaca_textview_widget_t *widget, gtcaca_textview_key_cb_t key_cb, void *userdata);

#endif /* _GTCACA_TEXTVIEW_H_ */
