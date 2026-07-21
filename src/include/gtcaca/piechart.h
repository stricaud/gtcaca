#ifndef _GTCACA_PIECHART_H_
#define _GTCACA_PIECHART_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Pie / donut chart ───────────────────────────────────────────────────────
 *
 * Draws categorical proportions as coloured slices of a circle, with an
 * optional legend listing each slice's label and percentage. Cells are twice
 * as tall as they are wide, so the disc is drawn with a 2:1 aspect correction
 * to look round. Set `donut` for a hollow centre (a ring chart).
 *
 * A staple of data-analytics dashboards: use it for the share of a total
 * (protocols in a capture, spend by category, …); use a bar chart for
 * magnitudes and a line chart for trends over time. */

typedef struct _gtcaca_piechart_widget_t gtcaca_piechart_widget_t;

struct _gtcaca_piechart_widget_t {
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
  struct _gtcaca_piechart_widget_t *prev;
  struct _gtcaca_piechart_widget_t *next;

  float   *data;         /* slice values (need not sum to 1)      */
  char   **labels;       /* per-slice labels (may be NULL)        */
  uint8_t *colours;      /* per-slice colours (NULL = palette)    */
  int      n;
  int      cap;
  int      donut;        /* non-zero draws a hollow centre        */
  int      show_legend;  /* list labels + percentages to the side */
  char    *title;
};

gtcaca_piechart_widget_t *gtcaca_piechart_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_piechart_draw(gtcaca_piechart_widget_t *p);
/* Set the slice values and (optionally) their labels. */
void gtcaca_piechart_set_data(gtcaca_piechart_widget_t *p, const float *values, const char *const *labels, int n);
/* Override the slice colours (else a built-in palette is cycled). `n` colours. */
void gtcaca_piechart_set_colors(gtcaca_piechart_widget_t *p, const uint8_t *colours, int n);
void gtcaca_piechart_set_donut(gtcaca_piechart_widget_t *p, int on);
void gtcaca_piechart_set_show_legend(gtcaca_piechart_widget_t *p, int on);
void gtcaca_piechart_set_title(gtcaca_piechart_widget_t *p, const char *title);
/* The palette colour used for slice `i` when none was set explicitly. */
uint8_t gtcaca_piechart_palette(int i);
void gtcaca_piechart_free(gtcaca_piechart_widget_t *p);

#endif /* _GTCACA_PIECHART_H_ */
