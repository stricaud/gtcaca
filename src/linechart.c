#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <caca.h>

#include <gtcaca/linechart.h>
#include <gtcaca/main.h>

gtcaca_linechart_widget_t *gtcaca_linechart_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_linechart_widget_t *c = malloc(sizeof(*c));
  if (!c) { fprintf(stderr, "Error: Cannot allocate linechart\n"); return NULL; }
  c->id = gtcaca_get_newid();
  c->has_focus = 0; c->is_visible = 1; c->type = GTCACA_WIDGET_LINECHART;
  c->parent = parent; c->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(c), x, y);
  c->width = width; c->height = height;
  c->color_focus_fg = c->color_nonfocus_fg = gmo.theme.textview.fg;
  c->color_focus_bg = c->color_nonfocus_bg = gmo.theme.textview.bg;
  c->nseries = 0; c->title = NULL; c->have_range = 0; c->ymin = 0; c->ymax = 0;
  c->have_xspan = 0; c->xspan = 0; c->xunit = NULL; c->log_y = 0;
  memset(c->series, 0, sizeof c->series);
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(c));
  return c;
}

int gtcaca_linechart_add_series_styled(gtcaca_linechart_widget_t *c, const double *y, int n,
                                       uint8_t colour, int style, const char *name)
{
  gtcaca_linechart_series_t *s;
  if (c->nseries >= GTCACA_LINECHART_MAX_SERIES || n <= 0) return -1;
  s = &c->series[c->nseries];
  s->y = malloc((size_t)n * sizeof(double));
  if (!s->y) return -1;
  memcpy(s->y, y, (size_t)n * sizeof(double));
  s->n = n; s->colour = colour; s->style = style;
  snprintf(s->name, sizeof s->name, "%s", name ? name : "");
  return c->nseries++;
}

int gtcaca_linechart_add_series(gtcaca_linechart_widget_t *c, const double *y, int n, uint8_t colour)
{ return gtcaca_linechart_add_series_styled(c, y, n, colour, GTCACA_LC_LINE, ""); }

void gtcaca_linechart_clear(gtcaca_linechart_widget_t *c)
{
  int i;
  for (i = 0; i < c->nseries; i++) { free(c->series[i].y); c->series[i].y = NULL; }
  c->nseries = 0;
}

void gtcaca_linechart_set_range(gtcaca_linechart_widget_t *c, double ymin, double ymax)
{ c->have_range = 1; c->ymin = ymin; c->ymax = ymax; }

void gtcaca_linechart_set_xspan(gtcaca_linechart_widget_t *c, double xspan, const char *unit)
{ c->have_xspan = 1; c->xspan = xspan; c->xunit = unit; }

void gtcaca_linechart_set_log_y(gtcaca_linechart_widget_t *c, int on) { c->log_y = on; }

void gtcaca_linechart_set_title(gtcaca_linechart_widget_t *c, const char *t)
{ free(c->title); c->title = t ? strdup(t) : NULL; }

void gtcaca_linechart_draw(gtcaca_linechart_widget_t *c)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = c->x + 1, inner_y = c->y + 1, inner_w = c->width - 2, inner_h = c->height - 2;
  int axisw = 7, px, py, pw, ph, s, i, maxn = 0;
  double lo, hi, span;
  char lbl[32];

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, c->x, c->y, c->width, c->height, ' ');
  caca_draw_cp437_box(gmo.cv, c->x, c->y, c->width, c->height);
  if (c->title) caca_printf(gmo.cv, c->x + 2, c->y, "| %s |", c->title);
  if (inner_w <= axisw + 2 || inner_h <= 2 || c->nseries == 0) return;

  /* Y range */
  if (c->have_range) { lo = c->ymin; hi = c->ymax; }
  else {
    lo = 1e300; hi = -1e300;
    for (s = 0; s < c->nseries; s++)
      for (i = 0; i < c->series[s].n; i++) {
        double v = c->series[s].y[i];
        if (v < lo) lo = v; if (v > hi) hi = v;
      }
    if (lo > hi) { lo = 0; hi = 1; }
  }
  if (c->log_y) { if (lo < 1) lo = 1; if (hi < 10) hi = 10; }
  else if (lo > 0) lo = 0;                 /* baseline at zero for counts */
  if (hi - lo < 1e-9) { hi = lo + 1; }
  span = hi - lo;
  for (s = 0; s < c->nseries; s++) if (c->series[s].n > maxn) maxn = c->series[s].n;

  /* plot rectangle (leave the left axisw columns for labels, last row for X) */
  px = inner_x + axisw; py = inner_y;
  pw = inner_w - axisw;  ph = inner_h - 1;
  if (pw < 2 || ph < 2) return;

#define MAPY(v) (py + (ph - 1) - (int)( \
    (c->log_y ? (log10((v) < 1 ? 1 : (v)) - log10(lo)) / (log10(hi) - log10(lo)) \
              : ((v) - lo) / span) * (ph - 1) + 0.5))

  /* Y axis + min/max/mid labels */
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  for (i = 0; i < ph; i++) caca_put_char(gmo.cv, px - 1, py + i, 0x2502);       /* │ */
  for (i = 0; i < pw; i++) caca_put_char(gmo.cv, px + i, py + ph, 0x2500);      /* ─ */
  caca_put_char(gmo.cv, px - 1, py + ph, 0x2514);                              /* └ */
  snprintf(lbl, sizeof lbl, "%.3g", hi);  caca_printf(gmo.cv, inner_x, py, "%.6s", lbl);
  snprintf(lbl, sizeof lbl, "%.3g", lo + span / 2); caca_printf(gmo.cv, inner_x, py + ph / 2, "%.6s", lbl);
  snprintf(lbl, sizeof lbl, "%.3g", lo);  caca_printf(gmo.cv, inner_x, py + ph - 1, "%.6s", lbl);

  /* series */
  for (s = 0; s < c->nseries; s++) {
    gtcaca_linechart_series_t *ser = &c->series[s];
    int prevx = -1, prevy = -1, base = py + ph - 1;
    caca_set_color_ansi(gmo.cv, ser->colour, bg); caca_set_attr(gmo.cv, 0);
    for (i = 0; i < ser->n; i++) {
      double t = ser->n > 1 ? (double)i / (ser->n - 1) : 0.0;
      double v = ser->y[i];
      int sx = px + (int)(t * (pw - 1) + 0.5);
      int sy = MAPY(v);
      if (sy < py) sy = py; if (sy > base) sy = base;
      switch (ser->style) {
      case GTCACA_LC_IMPULSE: {
        int yy; for (yy = sy; yy <= base; yy++) caca_put_char(gmo.cv, sx, yy, 0x2502); /* │ */
        break; }
      case GTCACA_LC_BAR: {
        int yy; for (yy = sy; yy <= base; yy++) caca_put_char(gmo.cv, sx, yy, 0x2588); /* █ */
        break; }
      case GTCACA_LC_DOT:
        caca_put_char(gmo.cv, sx, sy, 0x2022);                                        /* • */
        break;
      default: /* GTCACA_LC_LINE */
        if (prevx >= 0) caca_draw_thin_line(gmo.cv, prevx, prevy, sx, sy);
        else            caca_put_char(gmo.cv, sx, sy, 0x2022);
        break;
      }
      prevx = sx; prevy = sy;
    }
  }

  /* X-axis span labels (0 .. xspan unit) */
  if (c->have_xspan) {
    caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
    caca_printf(gmo.cv, px, py + ph, "0");
    snprintf(lbl, sizeof lbl, "%.3g%s", c->xspan, c->xunit ? c->xunit : "");
    caca_printf(gmo.cv, px + pw - (int)strlen(lbl), py + ph, "%s", lbl);
  }

  /* legend (top-right): a colour swatch + name per named series */
  {
    int row = py, any = 0;
    for (s = 0; s < c->nseries; s++) if (c->series[s].name[0]) any = 1;
    if (any) for (s = 0; s < c->nseries && row < py + ph; s++) {
      gtcaca_linechart_series_t *ser = &c->series[s];
      int lw, lx;
      if (!ser->name[0]) continue;
      lw = (int)strlen(ser->name) + 2;
      lx = px + pw - lw; if (lx < px) lx = px;
      caca_set_color_ansi(gmo.cv, ser->colour, bg); caca_set_attr(gmo.cv, 0);
      caca_put_char(gmo.cv, lx, row, 0x25A0);                 /* ■ */
      caca_printf(gmo.cv, lx + 2, row, "%s", ser->name);
      row++;
    }
  }
#undef MAPY
  caca_set_attr(gmo.cv, 0);
}

void gtcaca_linechart_free(gtcaca_linechart_widget_t *c)
{
  if (!c) return;
  gtcaca_linechart_clear(c);
  free(c->title); free(c);
}
