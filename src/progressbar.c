#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/progressbar.h>
#include <gtcaca/main.h>

gtcaca_progressbar_widget_t *gtcaca_progressbar_new(gtcaca_widget_t *parent, int x, int y, int width)
{
  gtcaca_progressbar_widget_t *pb;

  pb = malloc(sizeof(gtcaca_progressbar_widget_t));
  if (!pb) {
    fprintf(stderr, "Error: Cannot allocate progressbar\n");
    return NULL;
  }

  pb->id = gtcaca_get_newid();
  pb->has_focus = 0;
  pb->is_visible = 1;
  pb->type = GTCACA_WIDGET_PROGRESSBAR;
  pb->parent = parent;
  pb->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(pb), x, y);
  pb->width = width;
  pb->height = 1;

  pb->color_focus_fg = gmo.theme.progressbar.fg;
  pb->color_focus_bg = gmo.theme.progressbar.bg;
  pb->color_nonfocus_fg = gmo.theme.progressbar.fg;
  pb->color_nonfocus_bg = gmo.theme.progressbar.bg;

  pb->value = 0.0f;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(pb));

  return pb;
}

void gtcaca_progressbar_draw(gtcaca_progressbar_widget_t *pb)
{
  /* Layout: [######----] 60% */
  /* Reserve 6 chars for " XXX%" and 2 for brackets, rest is bar */
  int pct_width = 5;  /* " XXX%" */
  int bar_inner = pb->width - 2 - pct_width;
  int pct;
  int filled, empty, i;

  if (bar_inner < 1) bar_inner = 1;

  if (pb->value < 0.0f) pb->value = 0.0f;
  if (pb->value > 1.0f) pb->value = 1.0f;

  pct = (int)(pb->value * 100.0f);
  filled = (int)(pb->value * bar_inner);
  empty = bar_inner - filled;

  caca_set_color_ansi(gmo.cv, gmo.theme.progressbar.fg, gmo.theme.progressbar.bg);
  caca_put_char(gmo.cv, pb->x, pb->y, '[');

  caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_GREEN);
  for (i = 0; i < filled; i++)
    caca_put_char(gmo.cv, pb->x + 1 + i, pb->y, ' ');

  caca_set_color_ansi(gmo.cv, CACA_DARKGRAY, gmo.theme.progressbar.bg);
  for (i = 0; i < empty; i++)
    caca_put_char(gmo.cv, pb->x + 1 + filled + i, pb->y, '-');

  caca_set_color_ansi(gmo.cv, gmo.theme.progressbar.fg, gmo.theme.progressbar.bg);
  caca_put_char(gmo.cv, pb->x + 1 + bar_inner, pb->y, ']');
  caca_printf(gmo.cv, pb->x + 2 + bar_inner, pb->y, " %3d%%", pct);
}

void gtcaca_progressbar_set_value(gtcaca_progressbar_widget_t *pb, float value)
{
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;
  pb->value = value;
}

float gtcaca_progressbar_get_value(gtcaca_progressbar_widget_t *pb)
{
  return pb->value;
}
