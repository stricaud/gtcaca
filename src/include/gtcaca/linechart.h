#ifndef _GTCACA_LINECHART_H_
#define _GTCACA_LINECHART_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Line chart (cf. termdash's LineChart) ───────────────────────────────────
 *
 * Plots one or more Y-series against their sample index, auto-scaled, with a
 * labelled Y axis and an X baseline. Series are joined with caca's thin-line
 * glyphs so the curve reads cleanly in any terminal. */

#define GTCACA_LINECHART_MAX_SERIES 8

/* Plot styles (cf. Wireshark's IO-graph styles). */
enum {
  GTCACA_LC_LINE = 0,   /* points joined by a thin line   */
  GTCACA_LC_IMPULSE,    /* a vertical stem per sample      */
  GTCACA_LC_BAR,        /* a filled column per sample      */
  GTCACA_LC_DOT         /* a marker per sample, unjoined   */
};

typedef struct {
  double *y;
  int     n;
  uint8_t colour;
  int     style;        /* GTCACA_LC_*                     */
  char    name[48];     /* legend label ("" = no legend)   */
} gtcaca_linechart_series_t;

typedef struct _gtcaca_linechart_widget_t gtcaca_linechart_widget_t;
struct _gtcaca_linechart_widget_t {
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
  struct _gtcaca_linechart_widget_t *prev;
  struct _gtcaca_linechart_widget_t *next;

  gtcaca_linechart_series_t series[GTCACA_LINECHART_MAX_SERIES];
  int   nseries;
  char *title;
  int   have_range;          /* use fixed ymin/ymax instead of auto */
  double ymin, ymax;
  int   have_xspan;          /* label the X axis 0..xspan           */
  double xspan;
  const char *xunit;         /* X-axis unit suffix, e.g. "s"        */
  int   log_y;               /* logarithmic Y axis                  */
};

gtcaca_linechart_widget_t *gtcaca_linechart_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
/* append a series (the data is copied); returns its index or -1 if full */
int  gtcaca_linechart_add_series(gtcaca_linechart_widget_t *c, const double *y, int n, uint8_t colour);
/* append a styled, named series (for the legend); returns its index or -1 */
int  gtcaca_linechart_add_series_styled(gtcaca_linechart_widget_t *c, const double *y, int n,
                                        uint8_t colour, int style, const char *name);
void gtcaca_linechart_clear(gtcaca_linechart_widget_t *c);
void gtcaca_linechart_set_range(gtcaca_linechart_widget_t *c, double ymin, double ymax); /* fixed Y range */
void gtcaca_linechart_set_xspan(gtcaca_linechart_widget_t *c, double xspan, const char *unit);
void gtcaca_linechart_set_log_y(gtcaca_linechart_widget_t *c, int on);
void gtcaca_linechart_set_title(gtcaca_linechart_widget_t *c, const char *title);
void gtcaca_linechart_draw(gtcaca_linechart_widget_t *c);
void gtcaca_linechart_free(gtcaca_linechart_widget_t *c);

#endif /* _GTCACA_LINECHART_H_ */
