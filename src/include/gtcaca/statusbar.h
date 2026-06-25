#ifndef _GTCACA_STATUSBAR_H_
#define _GTCACA_STATUSBAR_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_statusbar_widget_t gtcaca_statusbar_widget_t;

struct _gtcaca_statusbar_widget_t {
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
  struct _gtcaca_statusbar_widget_t *prev;
  struct _gtcaca_statusbar_widget_t *next;

  char *text;
  int   rows_from_bottom;   /* draw on canvas_h - rows_from_bottom (default 1 = last row) */
};

gtcaca_statusbar_widget_t *gtcaca_statusbar_new(const char *text);
void gtcaca_statusbar_draw(gtcaca_statusbar_widget_t *sb);
void gtcaca_statusbar_set_text(gtcaca_statusbar_widget_t *sb, const char *text);
/* keep the bar off the very last row (some terminals don't render it); n rows up */
void gtcaca_statusbar_set_rows_from_bottom(gtcaca_statusbar_widget_t *sb, int n);

#endif /* _GTCACA_STATUSBAR_H_ */
