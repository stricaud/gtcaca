#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/barchart.h>
#include <gtcaca/main.h>

gtcaca_barchart_widget_t *gtcaca_barchart_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_barchart_widget_t *b = malloc(sizeof(*b));
  if (!b) { fprintf(stderr, "Error: Cannot allocate barchart\n"); return NULL; }

  b->id = gtcaca_get_newid();
  b->has_focus = 0;
  b->is_visible = 1;
  b->type = GTCACA_WIDGET_BARCHART;
  b->parent = parent;
  b->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(b), x, y);
  b->width = width;
  b->height = height;

  b->color_focus_fg = b->color_nonfocus_fg = gmo.theme.textview.fg;
  b->color_focus_bg = b->color_nonfocus_bg = gmo.theme.textview.bg;

  b->data = NULL; b->labels = NULL; b->n = 0; b->cap = 0;
  b->maxval = 0.0f;
  b->bar_width = 3;
  b->gap = 1;
  b->bar_colour = CACA_CYAN;
  b->show_values = 1;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(b));
  return b;
}

static void _free_labels(gtcaca_barchart_widget_t *b)
{
  int i;
  if (b->labels) { for (i = 0; i < b->n; i++) free(b->labels[i]); free(b->labels); b->labels = NULL; }
}

void gtcaca_barchart_free(gtcaca_barchart_widget_t *b)
{
  if (!b) return;
  _free_labels(b); free(b->data); free(b);
}

void gtcaca_barchart_set_data(gtcaca_barchart_widget_t *b, const float *values,
                              const char *const *labels, int n)
{
  int i;
  if (n < 0) n = 0;
  _free_labels(b);
  if (n > b->cap) {
    float *p = realloc(b->data, (size_t)n * sizeof(float));
    if (!p) return;
    b->data = p; b->cap = n;
  }
  if (n) memcpy(b->data, values, (size_t)n * sizeof(float));
  b->n = n;
  if (labels) {
    b->labels = calloc((size_t)n, sizeof(char *));
    if (b->labels) for (i = 0; i < n; i++) b->labels[i] = labels[i] ? strdup(labels[i]) : NULL;
  }
}

void gtcaca_barchart_set_max(gtcaca_barchart_widget_t *b, float maxval) { b->maxval = maxval; }
void gtcaca_barchart_set_bar_width(gtcaca_barchart_widget_t *b, int bar_width, int gap)
{ b->bar_width = bar_width < 1 ? 1 : bar_width; b->gap = gap < 0 ? 0 : gap; }
void gtcaca_barchart_set_color(gtcaca_barchart_widget_t *b, uint8_t colour) { b->bar_colour = colour; }
void gtcaca_barchart_set_show_values(gtcaca_barchart_widget_t *b, int on) { b->show_values = on ? 1 : 0; }

void gtcaca_barchart_draw(gtcaca_barchart_widget_t *b)
{
  uint8_t bg = gmo.theme.textview.bg, fg = gmo.theme.textview.fg;
  int slot = b->bar_width + b->gap;
  int label_h = 1;                        /* bottom row holds bar labels */
  int chart_h = b->height - label_h;
  int i, col;
  float mx = b->maxval;

  if (b->width <= 0 || b->height <= 0) return;
  if (chart_h < 1) { chart_h = b->height; label_h = 0; }

  /* clear */
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, b->x, b->y, b->width, b->height, ' ');
  if (b->n == 0) return;

  if (mx <= 0.0f) { for (i = 0; i < b->n; i++) if (b->data[i] > mx) mx = b->data[i]; if (mx <= 0.0f) mx = 1.0f; }

  for (i = 0, col = 0; i < b->n && col + b->bar_width <= b->width; i++, col += slot) {
    float v = b->data[i];
    int full, bx = b->x + col, r, k;
    if (v < 0.0f) v = 0.0f; if (v > mx) v = mx;
    /* whole-cell bar height filled with a coloured background — renders in any
       terminal (no fractional-block glyphs that some fonts lack) */
    full = (int)((v / mx) * (float)chart_h + 0.5f);
    if (full > chart_h) full = chart_h;
    if (v > 0.0f && full == 0) full = 1;        /* always show a non-zero bar */

    caca_set_color_ansi(gmo.cv, fg, b->bar_colour); caca_set_attr(gmo.cv, 0);
    for (k = 0; k < b->bar_width; k++)
      for (r = 0; r < full; r++)
        caca_put_char(gmo.cv, bx + k, b->y + chart_h - 1 - r, ' ');

    /* value just above the bar */
    if (b->show_values && chart_h >= 1) {
      char vb[16]; int top = b->y + chart_h - 1 - full;
      snprintf(vb, sizeof vb, "%g", b->data[i]);
      if (top < b->y) top = b->y;
      caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
      caca_printf(gmo.cv, bx, top, "%-.*s", b->bar_width, vb);
    }

    /* label row beneath the chart */
    if (label_h && b->labels && b->labels[i]) {
      caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
      caca_printf(gmo.cv, bx, b->y + b->height - 1, "%-.*s", b->bar_width, b->labels[i]);
    }
  }
}
