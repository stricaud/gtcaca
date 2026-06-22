#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/gauge.h>
#include <gtcaca/main.h>

gtcaca_gauge_widget_t *gtcaca_gauge_new(gtcaca_widget_t *parent, int x, int y, int width)
{
  gtcaca_gauge_widget_t *g = malloc(sizeof(*g));
  if (!g) { fprintf(stderr, "Error: Cannot allocate gauge\n"); return NULL; }

  g->id = gtcaca_get_newid();
  g->has_focus = 0;
  g->is_visible = 1;
  g->type = GTCACA_WIDGET_GAUGE;
  g->parent = parent;
  g->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(g), x, y);
  g->width = width;
  g->height = 1;

  g->color_focus_fg = g->color_nonfocus_fg = gmo.theme.textview.fg;
  g->color_focus_bg = g->color_nonfocus_bg = gmo.theme.textview.bg;

  g->value = 0.0f;
  g->label = NULL;
  g->filled_bg = CACA_GREEN;
  g->empty_bg  = CACA_DARKGRAY;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(g));
  return g;
}

void gtcaca_gauge_free(gtcaca_gauge_widget_t *g)
{
  if (!g) return;
  free(g->label); free(g);
}

void gtcaca_gauge_set_value(gtcaca_gauge_widget_t *g, float value)
{
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  g->value = value;
}
void gtcaca_gauge_set_percent(gtcaca_gauge_widget_t *g, int percent)
{
  gtcaca_gauge_set_value(g, (float)percent / 100.0f);
}
float gtcaca_gauge_get_value(gtcaca_gauge_widget_t *g) { return g->value; }

void gtcaca_gauge_set_label(gtcaca_gauge_widget_t *g, const char *label)
{
  free(g->label);
  g->label = label ? strdup(label) : NULL;
}
void gtcaca_gauge_set_colors(gtcaca_gauge_widget_t *g, uint8_t filled_bg, uint8_t empty_bg)
{
  g->filled_bg = filled_bg; g->empty_bg = empty_bg;
}

void gtcaca_gauge_draw(gtcaca_gauge_widget_t *g)
{
  int w = g->width;
  int filled, label_x, label_len, i;
  char buf[32];
  const char *label;

  if (w <= 0) return;
  if (g->value < 0.0f) g->value = 0.0f;
  if (g->value > 1.0f) g->value = 1.0f;

  filled = (int)(g->value * w + 0.5f);
  if (filled > w) filled = w;

  if (g->label) label = g->label;
  else { snprintf(buf, sizeof buf, "%d%%", (int)(g->value * 100.0f + 0.5f)); label = buf; }
  label_len = (int)strlen(label);
  label_x = (w - label_len) / 2;
  if (label_x < 0) label_x = 0;

  for (i = 0; i < w; i++) {
    int over_fill = (i < filled);
    uint8_t bg = over_fill ? g->filled_bg : g->empty_bg;
    /* label glyph for this cell, else a blank */
    int li = i - label_x;
    char ch = (li >= 0 && li < label_len) ? label[li] : ' ';
    /* contrast the text against whichever half it sits on */
    uint8_t fg = over_fill ? CACA_BLACK : CACA_WHITE;
    caca_set_color_ansi(gmo.cv, fg, bg);
    caca_set_attr(gmo.cv, 0);
    caca_put_char(gmo.cv, g->x + i, g->y, (uint32_t)(unsigned char)ch);
  }
}
