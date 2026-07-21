#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/scatter.h>
#include <gtcaca/main.h>

gtcaca_scatter_widget_t *gtcaca_scatter_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_scatter_widget_t *s = malloc(sizeof *s);
  if (!s) { fprintf(stderr, "Error: cannot allocate scatter\n"); return NULL; }

  s->id = gtcaca_get_newid();
  s->has_focus = 0;
  s->is_visible = 1;
  s->type = GTCACA_WIDGET_SCATTER;
  s->parent = parent;
  s->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(s), x, y);
  s->width = width;
  s->height = height;

  s->color_focus_fg = s->color_nonfocus_fg = gmo.theme.textview.fg;
  s->color_focus_bg = s->color_nonfocus_bg = gmo.theme.textview.bg;

  s->xs = NULL; s->ys = NULL; s->cols = NULL;
  s->n = 0; s->cap = 0;
  s->xmin = s->ymin = 0.0; s->xmax = s->ymax = 1.0;
  s->autoscale = 1;
  s->glyph = '*';
  s->title = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(s));
  return s;
}

void gtcaca_scatter_free(gtcaca_scatter_widget_t *s)
{
  if (!s) return;
  free(s->xs); free(s->ys); free(s->cols); free(s->title); free(s);
}

void gtcaca_scatter_clear(gtcaca_scatter_widget_t *s) { s->n = 0; }

static int _reserve(gtcaca_scatter_widget_t *s, int add)
{
  if (s->n + add > s->cap) {
    int nc = s->cap ? s->cap : 16;
    double *nx, *ny; uint8_t *nco;
    while (nc < s->n + add) nc *= 2;
    nx = realloc(s->xs, (size_t)nc * sizeof(double));
    ny = realloc(s->ys, (size_t)nc * sizeof(double));
    nco = realloc(s->cols, (size_t)nc * sizeof(uint8_t));
    if (!nx || !ny || !nco) { free(nx); free(ny); free(nco); return 0; }
    s->xs = nx; s->ys = ny; s->cols = nco; s->cap = nc;
  }
  return 1;
}

void gtcaca_scatter_add_point(gtcaca_scatter_widget_t *s, double x, double y, uint8_t colour)
{
  if (!_reserve(s, 1)) return;
  s->xs[s->n] = x; s->ys[s->n] = y; s->cols[s->n] = colour; s->n++;
}

void gtcaca_scatter_add_series(gtcaca_scatter_widget_t *s, const double *xs, const double *ys, int n, uint8_t colour)
{
  int i;
  if (n < 0 || !_reserve(s, n)) return;
  for (i = 0; i < n; i++) { s->xs[s->n] = xs[i]; s->ys[s->n] = ys[i]; s->cols[s->n] = colour; s->n++; }
}

void gtcaca_scatter_set_data(gtcaca_scatter_widget_t *s, const double *xs, const double *ys, int n, uint8_t colour)
{
  s->n = 0;
  gtcaca_scatter_add_series(s, xs, ys, n, colour);
}

void gtcaca_scatter_set_bounds(gtcaca_scatter_widget_t *s, double xmin, double xmax, double ymin, double ymax)
{
  s->xmin = xmin; s->xmax = xmax; s->ymin = ymin; s->ymax = ymax; s->autoscale = 0;
}
void gtcaca_scatter_set_autoscale(gtcaca_scatter_widget_t *s, int on) { s->autoscale = on ? 1 : 0; }
void gtcaca_scatter_set_glyph(gtcaca_scatter_widget_t *s, uint32_t glyph) { s->glyph = glyph ? glyph : '*'; }
void gtcaca_scatter_set_title(gtcaca_scatter_widget_t *s, const char *title)
{ free(s->title); s->title = title ? strdup(title) : NULL; }

void gtcaca_scatter_draw(gtcaca_scatter_widget_t *s)
{
  uint8_t bg = gmo.theme.textview.bg, fg = gmo.theme.textview.fg;
  int y0 = s->y, h = s->height, w = s->width;
  int gutter = 8;                        /* left band for Y tick labels */
  int px0, py0, pw, ph, i;
  double xmin, xmax, ymin, ymax, xr, yr;
  char lbl[32];

  if (w <= 0 || h <= 0) return;
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, s->x, s->y, w, h, ' ');

  if (s->title) {
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, s->x, y0, "%-.*s", w, s->title);
    caca_set_attr(gmo.cv, 0);
    y0 += 1; h -= 1;
  }
  if (h < 3 || w < gutter + 4) return;

  /* plot area: framed box to the right of the gutter, one row reserved at the
     bottom for X labels */
  px0 = s->x + gutter; py0 = y0;
  pw = w - gutter - 1; ph = h - 1;
  caca_set_color_ansi(gmo.cv, CACA_DARKGRAY, bg);
  caca_draw_cp437_box(gmo.cv, px0, py0, pw, ph);

  /* bounds */
  xmin = s->xmin; xmax = s->xmax; ymin = s->ymin; ymax = s->ymax;
  if (s->autoscale && s->n > 0) {
    xmin = xmax = s->xs[0]; ymin = ymax = s->ys[0];
    for (i = 1; i < s->n; i++) {
      if (s->xs[i] < xmin) xmin = s->xs[i]; if (s->xs[i] > xmax) xmax = s->xs[i];
      if (s->ys[i] < ymin) ymin = s->ys[i]; if (s->ys[i] > ymax) ymax = s->ys[i];
    }
  }
  xr = xmax - xmin; yr = ymax - ymin;
  if (xr <= 0.0) { xr = 1.0; xmin -= 0.5; }
  if (yr <= 0.0) { yr = 1.0; ymin -= 0.5; }

  /* Y tick labels (top and bottom of the plot) */
  caca_set_color_ansi(gmo.cv, fg, bg);
  snprintf(lbl, sizeof lbl, "%.3g", ymin + yr);
  caca_printf(gmo.cv, s->x, py0, "%*.*s", gutter - 1, gutter - 1, lbl);
  snprintf(lbl, sizeof lbl, "%.3g", ymin);
  caca_printf(gmo.cv, s->x, py0 + ph - 2, "%*.*s", gutter - 1, gutter - 1, lbl);
  /* X tick labels (left and right, on the bottom row) */
  snprintf(lbl, sizeof lbl, "%.3g", xmin);
  caca_printf(gmo.cv, px0, y0 + h - 1, "%s", lbl);
  snprintf(lbl, sizeof lbl, "%.3g", xmin + xr);
  caca_printf(gmo.cv, px0 + pw - (int)strlen(lbl), y0 + h - 1, "%s", lbl);

  /* points inside the frame interior */
  {
    int ix0 = px0 + 1, iy0 = py0 + 1, iw = pw - 2, ih = ph - 2;
    if (iw < 1 || ih < 1) return;
    for (i = 0; i < s->n; i++) {
      int cx = ix0 + (int)((s->xs[i] - xmin) / xr * (iw - 1) + 0.5);
      int cy = iy0 + (ih - 1) - (int)((s->ys[i] - ymin) / yr * (ih - 1) + 0.5);
      if (cx < ix0 || cx >= ix0 + iw || cy < iy0 || cy >= iy0 + ih) continue;
      caca_set_color_ansi(gmo.cv, s->cols[i], bg);
      caca_put_char(gmo.cv, cx, cy, s->glyph);
    }
  }
}
