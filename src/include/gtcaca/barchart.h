#ifndef _GTCACA_BARCHART_H_
#define _GTCACA_BARCHART_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* A vertical bar chart drawn with block glyphs, one labelled bar per value
   (cf. termui's BarChart). Bars fill from the bottom using eighth-block
   resolution; a label row sits beneath them. */

typedef struct _gtcaca_barchart_widget_t gtcaca_barchart_widget_t;

struct _gtcaca_barchart_widget_t {
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
  struct _gtcaca_barchart_widget_t *prev;
  struct _gtcaca_barchart_widget_t *next;

  float  *data;        /* bar values                       */
  char  **labels;      /* per-bar labels (may be NULL)     */
  int     n;
  int     cap;
  float   maxval;      /* fixed scale max, or 0 to auto    */
  int     bar_width;   /* columns per bar                  */
  int     gap;         /* blank columns between bars       */
  uint8_t bar_colour;  /* bar foreground colour            */
  int     show_values; /* draw the numeric value atop bars */
};

gtcaca_barchart_widget_t *gtcaca_barchart_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_barchart_draw(gtcaca_barchart_widget_t *b);
void gtcaca_barchart_set_data(gtcaca_barchart_widget_t *b, const float *values, const char *const *labels, int n);
void gtcaca_barchart_set_max(gtcaca_barchart_widget_t *b, float maxval);     /* 0 = auto */
void gtcaca_barchart_set_bar_width(gtcaca_barchart_widget_t *b, int bar_width, int gap);
void gtcaca_barchart_set_color(gtcaca_barchart_widget_t *b, uint8_t colour);
void gtcaca_barchart_set_show_values(gtcaca_barchart_widget_t *b, int on);
void gtcaca_barchart_free(gtcaca_barchart_widget_t *b);

#endif /* _GTCACA_BARCHART_H_ */
