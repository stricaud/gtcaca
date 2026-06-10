#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/scale.h>
#include <gtcaca/main.h>

gtcaca_scale_widget_t *gtcaca_scale_new(gtcaca_widget_t *parent, int x, int y, int width, double min, double max, double step)
{
  gtcaca_scale_widget_t *scale;

  scale = malloc(sizeof(gtcaca_scale_widget_t));
  if (!scale) {
    fprintf(stderr, "Error: Cannot allocate scale\n");
    return NULL;
  }

  scale->id = gtcaca_get_newid();
  scale->has_focus = 0;
  scale->is_visible = 1;
  scale->type = GTCACA_WIDGET_SCALE;
  scale->parent = parent;
  scale->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(scale), x, y);
  scale->width  = width;
  scale->height = 1;

  scale->color_focus_fg   = gmo.theme.textfocus.fg;
  scale->color_focus_bg   = gmo.theme.textfocus.bg;
  scale->color_nonfocus_fg = gmo.theme.progressbar.fg;
  scale->color_nonfocus_bg = gmo.theme.progressbar.bg;

  scale->min   = min;
  scale->max   = max;
  scale->step  = step;
  scale->value = min;
  scale->cb    = NULL;
  scale->cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(scale));

  return scale;
}

void gtcaca_scale_draw(gtcaca_scale_widget_t *scale)
{
  /* Layout: [====O----] NNN% */
  int track_w = scale->width - 2;
  int thumb_pos;
  int i;
  double range = scale->max - scale->min;
  int pct;

  if (track_w < 1) track_w = 1;

  if (range <= 0.0) {
    thumb_pos = 0;
    pct = 0;
  } else {
    thumb_pos = (int)((scale->value - scale->min) / range * (track_w - 1));
    pct = (int)((scale->value - scale->min) / range * 100.0);
  }
  if (thumb_pos < 0) thumb_pos = 0;
  if (thumb_pos >= track_w) thumb_pos = track_w - 1;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  gtcaca_widget_colorize(GTCACA_WIDGET(scale));

  caca_put_char(gmo.cv, scale->x, scale->y, '[');
  for (i = 0; i < track_w; i++) {
    char c;
    if (i < thumb_pos)      c = '=';
    else if (i == thumb_pos) c = 'O';
    else                    c = '-';
    caca_put_char(gmo.cv, scale->x + 1 + i, scale->y, c);
  }
  caca_put_char(gmo.cv, scale->x + 1 + track_w, scale->y, ']');
  caca_printf(gmo.cv, scale->x + 2 + track_w, scale->y, " %3d%%", pct);
}

void gtcaca_scale_handle_key(gtcaca_scale_widget_t *scale, int key)
{
  if (key == CACA_KEY_LEFT) {
    scale->value -= scale->step;
    if (scale->value < scale->min) scale->value = scale->min;
  } else if (key == CACA_KEY_RIGHT) {
    scale->value += scale->step;
    if (scale->value > scale->max) scale->value = scale->max;
  }
}

void gtcaca_scale_set_value(gtcaca_scale_widget_t *scale, double value)
{
  if (value < scale->min) value = scale->min;
  if (value > scale->max) value = scale->max;
  scale->value = value;
}

double gtcaca_scale_get_value(gtcaca_scale_widget_t *scale)
{
  return scale->value;
}

void gtcaca_scale_cb_register(gtcaca_scale_widget_t *scale, gtcaca_scale_cb_t cb, void *userdata)
{
  scale->cb = cb;
  scale->cb_userdata = userdata;
}
