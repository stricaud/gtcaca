#ifndef _GTCACA_SPARKLINE_H_
#define _GTCACA_SPARKLINE_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* A compact trend chart (cf. termui's Sparkline). It keeps the most recent
   `width` samples and renders them right-aligned as filled columns using
   background-coloured cells — which render in any terminal, unlike the
   fractional block glyphs some fonts lack. Give it a height of a few rows for a
   smooth area look; gtcaca_sparkline_push() animates a live feed. A title, if
   set, is drawn on the first row with the chart beneath it. */

/* Rendering style. BLOCKS is the canonical sparkline drawn with the eighth-block
   glyphs ▁▂▃▄▅▆▇█ (crisp and instantly recognisable; needs a font with the
   fractional blocks — most modern terminals have them). AREA fills columns with
   a coloured background instead, which renders in every terminal/font. */
typedef enum {
  GTCACA_SPARKLINE_BLOCKS = 0,   /* default */
  GTCACA_SPARKLINE_AREA
} gtcaca_sparkline_style_t;

typedef struct _gtcaca_sparkline_widget_t gtcaca_sparkline_widget_t;

struct _gtcaca_sparkline_widget_t {
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
  struct _gtcaca_sparkline_widget_t *prev;
  struct _gtcaca_sparkline_widget_t *next;

  float  *data;      /* the value series                          */
  int     n;         /* number of values held                     */
  int     cap;       /* allocated slots                           */
  float   maxval;    /* fixed scale max, or 0 to auto-scale       */
  uint8_t colour;    /* glyph/fill colour                         */
  char   *title;     /* optional label drawn above the line       */
  int     style;     /* gtcaca_sparkline_style_t                  */
};

gtcaca_sparkline_widget_t *gtcaca_sparkline_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void  gtcaca_sparkline_draw(gtcaca_sparkline_widget_t *s);
void  gtcaca_sparkline_set_data(gtcaca_sparkline_widget_t *s, const float *values, int n);
void  gtcaca_sparkline_push(gtcaca_sparkline_widget_t *s, float value);
void  gtcaca_sparkline_set_max(gtcaca_sparkline_widget_t *s, float maxval); /* 0 = auto-scale */
void  gtcaca_sparkline_set_color(gtcaca_sparkline_widget_t *s, uint8_t colour);
void  gtcaca_sparkline_set_style(gtcaca_sparkline_widget_t *s, int style); /* gtcaca_sparkline_style_t */
void  gtcaca_sparkline_set_style_auto(gtcaca_sparkline_widget_t *s);       /* pick by terminal */
void  gtcaca_sparkline_set_title(gtcaca_sparkline_widget_t *s, const char *title);
void  gtcaca_sparkline_free(gtcaca_sparkline_widget_t *s);

#endif /* _GTCACA_SPARKLINE_H_ */
