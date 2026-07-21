#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <caca.h>

#include <gtcaca/piechart.h>
#include <gtcaca/main.h>

/* A pleasant, high-contrast default palette cycled across slices. */
static const uint8_t PALETTE[] = {
  CACA_CYAN, CACA_LIGHTGREEN, CACA_YELLOW, CACA_LIGHTRED, CACA_LIGHTMAGENTA,
  CACA_LIGHTBLUE, CACA_GREEN, CACA_RED, CACA_BLUE, CACA_MAGENTA, CACA_BROWN,
  CACA_LIGHTCYAN,
};
#define NPAL ((int)(sizeof PALETTE / sizeof PALETTE[0]))

uint8_t gtcaca_piechart_palette(int i)
{
  if (i < 0) i = 0;
  return PALETTE[i % NPAL];
}

gtcaca_piechart_widget_t *gtcaca_piechart_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_piechart_widget_t *p = malloc(sizeof *p);
  if (!p) { fprintf(stderr, "Error: cannot allocate piechart\n"); return NULL; }

  p->id = gtcaca_get_newid();
  p->has_focus = 0;
  p->is_visible = 1;
  p->type = GTCACA_WIDGET_PIECHART;
  p->parent = parent;
  p->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(p), x, y);
  p->width = width;
  p->height = height;

  p->color_focus_fg = p->color_nonfocus_fg = gmo.theme.textview.fg;
  p->color_focus_bg = p->color_nonfocus_bg = gmo.theme.textview.bg;

  p->data = NULL; p->labels = NULL; p->colours = NULL;
  p->n = 0; p->cap = 0;
  p->donut = 0;
  p->show_legend = 1;
  p->title = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(p));
  return p;
}

static void _free_labels(gtcaca_piechart_widget_t *p)
{
  int i;
  if (p->labels) { for (i = 0; i < p->n; i++) free(p->labels[i]); free(p->labels); p->labels = NULL; }
}

void gtcaca_piechart_free(gtcaca_piechart_widget_t *p)
{
  if (!p) return;
  _free_labels(p); free(p->data); free(p->colours); free(p->title); free(p);
}

void gtcaca_piechart_set_data(gtcaca_piechart_widget_t *p, const float *values,
                              const char *const *labels, int n)
{
  int i;
  if (n < 0) n = 0;
  _free_labels(p);
  if (n > p->cap) {
    float *d = realloc(p->data, (size_t)n * sizeof(float));
    if (!d) return;
    p->data = d; p->cap = n;
  }
  if (n) memcpy(p->data, values, (size_t)n * sizeof(float));
  p->n = n;
  if (labels) {
    p->labels = calloc((size_t)n, sizeof(char *));
    if (p->labels) for (i = 0; i < n; i++) p->labels[i] = labels[i] ? strdup(labels[i]) : NULL;
  }
}

void gtcaca_piechart_set_colors(gtcaca_piechart_widget_t *p, const uint8_t *colours, int n)
{
  free(p->colours); p->colours = NULL;
  if (colours && n > 0) {
    p->colours = malloc((size_t)n * sizeof(uint8_t));
    if (p->colours) memcpy(p->colours, colours, (size_t)n * sizeof(uint8_t));
  }
}

void gtcaca_piechart_set_donut(gtcaca_piechart_widget_t *p, int on) { p->donut = on ? 1 : 0; }
void gtcaca_piechart_set_show_legend(gtcaca_piechart_widget_t *p, int on) { p->show_legend = on ? 1 : 0; }
void gtcaca_piechart_set_title(gtcaca_piechart_widget_t *p, const char *title)
{ free(p->title); p->title = title ? strdup(title) : NULL; }

static uint8_t slice_colour(gtcaca_piechart_widget_t *p, int i)
{
  if (p->colours) return p->colours[i];
  return gtcaca_piechart_palette(i);
}

void gtcaca_piechart_draw(gtcaca_piechart_widget_t *p)
{
  uint8_t bg = gmo.theme.textview.bg, fg = gmo.theme.textview.fg;
  int y0 = p->y, h = p->height, w = p->width;
  int legend_w = 0, chart_w, i, row, col;
  float total = 0.0f;
  double r, cx, cy;

  if (w <= 0 || h <= 0) return;

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, p->x, p->y, w, h, ' ');

  if (p->title) {
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, p->x, y0, "%-.*s", w, p->title);
    caca_set_attr(gmo.cv, 0);
    y0 += 1; h -= 1;
  }
  if (h <= 0) return;

  for (i = 0; i < p->n; i++) if (p->data[i] > 0.0f) total += p->data[i];
  if (total <= 0.0f || p->n == 0) return;

  /* Reserve a legend column band on the right when there's room. */
  if (p->show_legend) {
    int maxlbl = 0;
    for (i = 0; i < p->n; i++) {
      int l = p->labels && p->labels[i] ? (int)strlen(p->labels[i]) : 4;
      if (l > maxlbl) maxlbl = l;
    }
    legend_w = maxlbl + 9;              /* "## nn.n% label" */
    if (legend_w > w - 8) legend_w = 0; /* not enough room; drop the legend */
  }
  chart_w = w - legend_w;

  /* Disc geometry. Each cell is split into two vertical sub-pixels drawn with
     an upper-half block, doubling the vertical resolution — a half-cell is then
     about as tall as a column is wide, so the disc comes out round (top and
     bottom curve as smoothly as the sides) with no aspect fudging. Work in
     sub-pixel Y (0..2h) and column X (0..chart_w), which share one unit. */
  {
    int subh = h * 2;
    cx = (chart_w - 1) / 2.0;
    cy = (subh - 1) / 2.0;
    r = (subh - 1) / 2.0;
    if ((chart_w - 1) / 2.0 < r) r = (chart_w - 1) / 2.0;
    if (r < 1.0) r = 1.0;

    for (row = 0; row < h; row++) {
      for (col = 0; col < chart_w; col++) {
        int half;                 /* 0 = top sub-pixel, 1 = bottom */
        int scol[2] = { -1, -1 }; /* slice index per sub-pixel, -1 = outside */
        for (half = 0; half < 2; half++) {
          double nx = col - cx;
          double ny = (row * 2 + half) - cy;
          double dist = sqrt(nx * nx + ny * ny);
          double ang, frac, acc;
          int s = -1;
          if (dist <= r && !(p->donut && dist < r * 0.45)) {
            ang = atan2(nx, -ny);                 /* 0 at 12 o'clock, clockwise */
            if (ang < 0) ang += 2.0 * M_PI;
            frac = ang / (2.0 * M_PI);
            acc = 0.0;
            for (i = 0; i < p->n; i++) {
              if (p->data[i] <= 0.0f) continue;
              s = i;
              acc += p->data[i] / total;
              if (frac < acc) break;
            }
          }
          scol[half] = s;
        }
        if (scol[0] < 0 && scol[1] < 0) continue;
        {
          uint8_t top = scol[0] >= 0 ? slice_colour(p, scol[0]) : bg;
          uint8_t bot = scol[1] >= 0 ? slice_colour(p, scol[1]) : bg;
          /* U+2580 upper-half block: fg paints the top half, bg the bottom. */
          caca_set_color_ansi(gmo.cv, top, bot);
          caca_put_char(gmo.cv, p->x + col, y0 + row, 0x2580);
        }
      }
    }
  }

  /* Legend */
  if (legend_w > 0) {
    int lx = p->x + chart_w + 1, ly = y0;
    for (i = 0; i < p->n && ly < y0 + h; i++) {
      float pct;
      if (p->data[i] <= 0.0f) continue;
      pct = 100.0f * p->data[i] / total;
      caca_set_color_ansi(gmo.cv, CACA_BLACK, slice_colour(p, i));
      caca_put_str(gmo.cv, lx, ly, "  ");
      caca_set_color_ansi(gmo.cv, fg, bg);
      caca_printf(gmo.cv, lx + 3, ly, "%5.1f%% %-.*s", pct,
                  legend_w - 9, p->labels && p->labels[i] ? p->labels[i] : "");
      ly++;
    }
  }
}
