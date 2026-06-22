#ifndef _GTCACA_GAUGE_H_
#define _GTCACA_GAUGE_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* A horizontal percentage bar with a label centered *inside* it (cf. termui's
   Gauge). The filled portion is drawn in a highlight colour; the label (the
   percentage by default, or a caller-supplied string) reads over both. */

typedef struct _gtcaca_gauge_widget_t gtcaca_gauge_widget_t;

struct _gtcaca_gauge_widget_t {
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
  struct _gtcaca_gauge_widget_t *prev;
  struct _gtcaca_gauge_widget_t *next;

  float   value;       /* 0.0 .. 1.0                                  */
  char   *label;       /* custom centred label, or NULL for "NN%"     */
  uint8_t filled_bg;   /* background of the filled portion            */
  uint8_t empty_bg;    /* background of the empty portion             */
};

gtcaca_gauge_widget_t *gtcaca_gauge_new(gtcaca_widget_t *parent, int x, int y, int width);
void  gtcaca_gauge_draw(gtcaca_gauge_widget_t *g);
void  gtcaca_gauge_set_value(gtcaca_gauge_widget_t *g, float value);  /* 0..1 */
void  gtcaca_gauge_set_percent(gtcaca_gauge_widget_t *g, int percent);/* 0..100 */
float gtcaca_gauge_get_value(gtcaca_gauge_widget_t *g);
void  gtcaca_gauge_set_label(gtcaca_gauge_widget_t *g, const char *label); /* NULL = show % */
void  gtcaca_gauge_set_colors(gtcaca_gauge_widget_t *g, uint8_t filled_bg, uint8_t empty_bg);
void  gtcaca_gauge_free(gtcaca_gauge_widget_t *g);

#endif /* _GTCACA_GAUGE_H_ */
