#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/sparkline.h>
#include <gtcaca/main.h>

gtcaca_sparkline_widget_t *gtcaca_sparkline_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_sparkline_widget_t *s = malloc(sizeof(*s));
  if (!s) { fprintf(stderr, "Error: Cannot allocate sparkline\n"); return NULL; }

  s->id = gtcaca_get_newid();
  s->has_focus = 0;
  s->is_visible = 1;
  s->type = GTCACA_WIDGET_SPARKLINE;
  s->parent = parent;
  s->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(s), x, y);
  s->width = width;
  s->height = height > 0 ? height : 1;

  s->color_focus_fg = s->color_nonfocus_fg = gmo.theme.textview.fg;
  s->color_focus_bg = s->color_nonfocus_bg = gmo.theme.textview.bg;

  s->data = NULL; s->n = 0; s->cap = 0;
  s->maxval = 0.0f;            /* auto-scale */
  s->colour = CACA_GREEN;
  s->title = NULL;
  s->style = GTCACA_SPARKLINE_BLOCKS;   /* canonical by default */

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(s));
  return s;
}

void gtcaca_sparkline_free(gtcaca_sparkline_widget_t *s)
{
  if (!s) return;
  free(s->data); free(s->title); free(s);
}

void gtcaca_sparkline_set_data(gtcaca_sparkline_widget_t *s, const float *values, int n)
{
  if (n < 0) n = 0;
  if (n > s->cap) {
    float *p = realloc(s->data, (size_t)n * sizeof(float));
    if (!p) return;
    s->data = p; s->cap = n;
  }
  if (n) memcpy(s->data, values, (size_t)n * sizeof(float));
  s->n = n;
}

void gtcaca_sparkline_push(gtcaca_sparkline_widget_t *s, float value)
{
  if (s->n == s->cap) {
    int ncap = s->cap ? s->cap * 2 : 64;
    float *p = realloc(s->data, (size_t)ncap * sizeof(float));
    if (!p) return;
    s->data = p; s->cap = ncap;
  }
  s->data[s->n++] = value;
}

void gtcaca_sparkline_set_max(gtcaca_sparkline_widget_t *s, float maxval) { s->maxval = maxval; }
void gtcaca_sparkline_set_color(gtcaca_sparkline_widget_t *s, uint8_t colour) { s->colour = colour; }
void gtcaca_sparkline_set_style(gtcaca_sparkline_widget_t *s, int style) { s->style = style; }
void gtcaca_sparkline_set_style_auto(gtcaca_sparkline_widget_t *s)
{
  s->style = gtcaca_terminal_supports_blocks() ? GTCACA_SPARKLINE_BLOCKS
                                               : GTCACA_SPARKLINE_AREA;
}

void gtcaca_sparkline_set_title(gtcaca_sparkline_widget_t *s, const char *title)
{
  free(s->title);
  s->title = title ? strdup(title) : NULL;
}

void gtcaca_sparkline_draw(gtcaca_sparkline_widget_t *s)
{
  uint8_t bg = gmo.theme.textview.bg, fg = gmo.theme.textview.fg;
  int chart_y = s->y, chart_h = s->height;
  int i, start, count;
  float mx = s->maxval;

  /* optional title on the first row */
  if (s->title && chart_h > 1) {
    caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
    caca_printf(gmo.cv, s->x, s->y, "%-.*s", s->width, s->title);
    chart_y = s->y + 1; chart_h = s->height - 1;
  }

  /* clear the chart area */
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, s->x, chart_y, s->width, chart_h, ' ');

  if (s->n == 0 || s->width <= 0 || chart_h <= 0) return;

  count = s->n < s->width ? s->n : s->width;    /* most recent, right-aligned */
  start = s->n - count;

  if (mx <= 0.0f) {                             /* auto-scale to the window */
    for (i = start; i < s->n; i++) if (s->data[i] > mx) mx = s->data[i];
    if (mx <= 0.0f) mx = 1.0f;
  }

  for (i = 0; i < count; i++) {
    float v = s->data[start + i];
    int x = s->x + (s->width - count) + i, r;
    if (v < 0.0f) v = 0.0f; if (v > mx) v = mx;

    if (s->style == GTCACA_SPARKLINE_BLOCKS) {
      /* canonical: full cells █ plus a fractional-block top ▁..▇ */
      static const uint32_t bars[8] =
        { 0x2581, 0x2582, 0x2583, 0x2584, 0x2585, 0x2586, 0x2587, 0x2588 };
      int eighths = (int)((v / mx) * (float)chart_h * 8.0f + 0.5f);
      int full = eighths / 8, partial = eighths % 8;
      if (v > 0.0f && eighths == 0) { partial = 1; }
      if (full > chart_h) { full = chart_h; partial = 0; }
      caca_set_color_ansi(gmo.cv, s->colour, bg); caca_set_attr(gmo.cv, 0);
      for (r = 0; r < full; r++)
        caca_put_char(gmo.cv, x, chart_y + chart_h - 1 - r, 0x2588);
      if (partial > 0 && full < chart_h)
        caca_put_char(gmo.cv, x, chart_y + chart_h - 1 - full, bars[partial - 1]);
    } else {
      /* terminal-safe: a column filled from the bottom with a coloured bg */
      int filled = (int)((v / mx) * (float)chart_h + 0.5f);
      if (filled > chart_h) filled = chart_h;
      if (v > 0.0f && filled == 0) filled = 1;
      caca_set_color_ansi(gmo.cv, fg, s->colour); caca_set_attr(gmo.cv, 0);
      for (r = 0; r < filled; r++)
        caca_put_char(gmo.cv, x, chart_y + chart_h - 1 - r, ' ');
    }
  }
}
