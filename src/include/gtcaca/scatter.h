#ifndef _GTCACA_SCATTER_H_
#define _GTCACA_SCATTER_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Scatter plot ────────────────────────────────────────────────────────────
 *
 * Plots (x, y) points inside framed axes — the go-to view for correlation and
 * distribution in a data-analytics dashboard (a bar chart shows magnitude, a
 * line chart a trend over an ordered X, a scatter the relationship between two
 * variables). Points carry a per-point colour so several series can share one
 * plot. Axis bounds auto-fit the data unless pinned with set_bounds. */

typedef struct _gtcaca_scatter_widget_t gtcaca_scatter_widget_t;

struct _gtcaca_scatter_widget_t {
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
  struct _gtcaca_scatter_widget_t *prev;
  struct _gtcaca_scatter_widget_t *next;

  double  *xs;
  double  *ys;
  uint8_t *cols;         /* per-point colour */
  int      n;
  int      cap;
  double   xmin, xmax, ymin, ymax;
  int      autoscale;    /* fit bounds to the data (default on) */
  uint32_t glyph;        /* marker character (default '*')      */
  char    *title;
};

gtcaca_scatter_widget_t *gtcaca_scatter_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_scatter_draw(gtcaca_scatter_widget_t *s);
/* Append one point in the given colour. */
void gtcaca_scatter_add_point(gtcaca_scatter_widget_t *s, double x, double y, uint8_t colour);
/* Append `n` points (parallel xs/ys arrays), all in `colour`. */
void gtcaca_scatter_add_series(gtcaca_scatter_widget_t *s, const double *xs, const double *ys, int n, uint8_t colour);
/* Replace all points with `n` points in `colour`. */
void gtcaca_scatter_set_data(gtcaca_scatter_widget_t *s, const double *xs, const double *ys, int n, uint8_t colour);
void gtcaca_scatter_clear(gtcaca_scatter_widget_t *s);
/* Pin the axis range (turns off autoscaling). */
void gtcaca_scatter_set_bounds(gtcaca_scatter_widget_t *s, double xmin, double xmax, double ymin, double ymax);
void gtcaca_scatter_set_autoscale(gtcaca_scatter_widget_t *s, int on);
void gtcaca_scatter_set_glyph(gtcaca_scatter_widget_t *s, uint32_t glyph);
void gtcaca_scatter_set_title(gtcaca_scatter_widget_t *s, const char *title);
void gtcaca_scatter_free(gtcaca_scatter_widget_t *s);

#endif /* _GTCACA_SCATTER_H_ */
