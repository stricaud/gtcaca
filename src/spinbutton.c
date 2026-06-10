#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/spinbutton.h>
#include <gtcaca/main.h>

gtcaca_spinbutton_widget_t *gtcaca_spinbutton_new(gtcaca_widget_t *parent, int x, int y, double min, double max, double step)
{
  gtcaca_spinbutton_widget_t *sb;

  sb = malloc(sizeof(gtcaca_spinbutton_widget_t));
  if (!sb) {
    fprintf(stderr, "Error: Cannot allocate spinbutton\n");
    return NULL;
  }

  sb->id = gtcaca_get_newid();
  sb->has_focus = 0;
  sb->is_visible = 1;
  sb->type = GTCACA_WIDGET_SPINBUTTON;
  sb->parent = parent;
  sb->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(sb), x, y);
  sb->width  = 10;
  sb->height = 1;

  sb->color_focus_fg   = gmo.theme.entryfocus.fg;
  sb->color_focus_bg   = gmo.theme.entryfocus.bg;
  sb->color_nonfocus_fg = gmo.theme.entry.fg;
  sb->color_nonfocus_bg = gmo.theme.entry.bg;

  sb->min    = min;
  sb->max    = max;
  sb->step   = step;
  sb->value  = min;
  sb->digits = (step < 1.0) ? 2 : 0;
  sb->cb     = NULL;
  sb->cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(sb));

  return sb;
}

void gtcaca_spinbutton_draw(gtcaca_spinbutton_widget_t *sb)
{
  char buf[32];

  gtcaca_widget_colorize(GTCACA_WIDGET(sb));

  if (sb->digits > 0)
    snprintf(buf, sizeof(buf), "< %.*f >", sb->digits, sb->value);
  else
    snprintf(buf, sizeof(buf), "< %d >", (int)sb->value);

  caca_printf(gmo.cv, sb->x, sb->y, "%-*s", sb->width, buf);
}

void gtcaca_spinbutton_handle_key(gtcaca_spinbutton_widget_t *sb, int key)
{
  if (key == CACA_KEY_RIGHT) {
    sb->value += sb->step;
    if (sb->value > sb->max) sb->value = sb->max;
  } else if (key == CACA_KEY_LEFT) {
    sb->value -= sb->step;
    if (sb->value < sb->min) sb->value = sb->min;
  }
}

void gtcaca_spinbutton_set_value(gtcaca_spinbutton_widget_t *sb, double value)
{
  if (value < sb->min) value = sb->min;
  if (value > sb->max) value = sb->max;
  sb->value = value;
}

double gtcaca_spinbutton_get_value(gtcaca_spinbutton_widget_t *sb)
{
  return sb->value;
}

void gtcaca_spinbutton_cb_register(gtcaca_spinbutton_widget_t *sb, gtcaca_spinbutton_cb_t cb, void *userdata)
{
  sb->cb = cb;
  sb->cb_userdata = userdata;
}
