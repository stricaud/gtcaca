#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/switch.h>
#include <gtcaca/main.h>

gtcaca_switch_widget_t *gtcaca_switch_new(gtcaca_widget_t *parent, int x, int y)
{
  gtcaca_switch_widget_t *sw;

  sw = malloc(sizeof(gtcaca_switch_widget_t));
  if (!sw) {
    fprintf(stderr, "Error: Cannot allocate switch\n");
    return NULL;
  }

  sw->id = gtcaca_get_newid();
  sw->has_focus = 0;
  sw->is_visible = 1;
  sw->type = GTCACA_WIDGET_SWITCH;
  sw->parent = parent;
  sw->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(sw), x, y);
  sw->width  = 5;
  sw->height = 1;

  sw->color_focus_fg   = gmo.theme.checkboxfocus.fg;
  sw->color_focus_bg   = gmo.theme.checkboxfocus.bg;
  sw->color_nonfocus_fg = gmo.theme.checkbox.fg;
  sw->color_nonfocus_bg = gmo.theme.checkbox.bg;

  sw->active = 0;
  sw->cb = NULL;
  sw->cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(sw));

  return sw;
}

void gtcaca_switch_draw(gtcaca_switch_widget_t *sw)
{
  gtcaca_widget_colorize(GTCACA_WIDGET(sw));

  if (sw->active)
    caca_printf(gmo.cv, sw->x, sw->y, "[ON ]");
  else
    caca_printf(gmo.cv, sw->x, sw->y, "[OFF]");
}

void gtcaca_switch_set_active(gtcaca_switch_widget_t *sw, int active)
{
  sw->active = active ? 1 : 0;
}

int gtcaca_switch_get_active(gtcaca_switch_widget_t *sw)
{
  return sw->active;
}

void gtcaca_switch_cb_register(gtcaca_switch_widget_t *sw, gtcaca_switch_cb_t cb, void *userdata)
{
  sw->cb = cb;
  sw->cb_userdata = userdata;
}
