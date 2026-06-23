#ifndef _GTCACA_SEGDISPLAY_H_
#define _GTCACA_SEGDISPLAY_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Seven-segment display (cf. termdash's SegmentDisplay) ───────────────────
 *
 * Renders a short string as big "LED" seven-segment glyphs built from solid
 * block cells, sized to fill the widget — great for clocks, counters and
 * dashboard read-outs. Understands 0-9, A-F (and a few more letters), space,
 * '-' and ':'. */

typedef struct _gtcaca_segdisplay_widget_t gtcaca_segdisplay_widget_t;
struct _gtcaca_segdisplay_widget_t {
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
  struct _gtcaca_segdisplay_widget_t *prev;
  struct _gtcaca_segdisplay_widget_t *next;

  char   *text;
  uint8_t colour;     /* lit-segment colour          */
  uint8_t dim;        /* unlit-segment colour (0 = off / background) */
  char   *title;
  int     box;        /* draw a surrounding box (1) or not (0) */
};

gtcaca_segdisplay_widget_t *gtcaca_segdisplay_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_segdisplay_set_text(gtcaca_segdisplay_widget_t *s, const char *text);
void gtcaca_segdisplay_set_colour(gtcaca_segdisplay_widget_t *s, uint8_t colour);
void gtcaca_segdisplay_set_title(gtcaca_segdisplay_widget_t *s, const char *title);
void gtcaca_segdisplay_set_box(gtcaca_segdisplay_widget_t *s, int on);
void gtcaca_segdisplay_draw(gtcaca_segdisplay_widget_t *s);
void gtcaca_segdisplay_free(gtcaca_segdisplay_widget_t *s);

#endif /* _GTCACA_SEGDISPLAY_H_ */
