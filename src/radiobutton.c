#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/radiobutton.h>
#include <gtcaca/main.h>

static int _gtcaca_radiobutton_private_key_press(gtcaca_radiobutton_widget_t *rb, int key, void *userdata)
{
  switch (key) {
  case ' ':
  case CACA_KEY_RETURN:
    gtcaca_radiobutton_set_active(rb);
    break;
  }
  return 0;
}

gtcaca_radiobutton_widget_t *gtcaca_radiobutton_new(gtcaca_widget_t *parent, const char *label, int group_id, int x, int y)
{
  gtcaca_radiobutton_widget_t *rb;

  rb = malloc(sizeof(gtcaca_radiobutton_widget_t));
  if (!rb) {
    fprintf(stderr, "Error: Cannot allocate radiobutton\n");
    return NULL;
  }

  rb->id = gtcaca_get_newid();
  rb->has_focus = 0;
  rb->is_visible = 1;
  rb->type = GTCACA_WIDGET_RADIOBUTTON;
  rb->parent = parent;
  rb->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(rb), x, y);
  rb->width = (label ? (int)strlen(label) : 0) + 4; /* (*) + space + label */
  rb->height = 1;

  rb->color_focus_fg = gmo.theme.checkboxfocus.fg;
  rb->color_focus_bg = gmo.theme.checkboxfocus.bg;
  rb->color_nonfocus_fg = gmo.theme.checkbox.fg;
  rb->color_nonfocus_bg = gmo.theme.checkbox.bg;

  rb->label = label ? strdup(label) : NULL;
  rb->active = 0;
  rb->group_id = group_id;
  rb->private_key_cb = _gtcaca_radiobutton_private_key_press;
  rb->key_cb = NULL;
  rb->key_cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(rb));

  return rb;
}

void gtcaca_radiobutton_draw(gtcaca_radiobutton_widget_t *rb)
{
  gtcaca_widget_colorize(GTCACA_WIDGET(rb));

  caca_put_char(gmo.cv, rb->x, rb->y, '(');
  caca_put_char(gmo.cv, rb->x + 1, rb->y, rb->active ? '*' : ' ');
  caca_put_char(gmo.cv, rb->x + 2, rb->y, ')');

  if (rb->label) {
    caca_printf(gmo.cv, rb->x + 4, rb->y, "%s", rb->label);
  }
}

int gtcaca_radiobutton_key_cb_register(gtcaca_radiobutton_widget_t *widget, gtcaca_radiobutton_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
  return 0;
}

int gtcaca_radiobutton_get_active(gtcaca_radiobutton_widget_t *rb)
{
  return rb->active;
}

void gtcaca_radiobutton_set_active(gtcaca_radiobutton_widget_t *rb)
{
  gtcaca_widget_t *widget;

  /* Deactivate all radiobuttons in the same group */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_RADIOBUTTON) {
      gtcaca_radiobutton_widget_t *other = (gtcaca_radiobutton_widget_t *)widget;
      if (other->group_id == rb->group_id) {
        other->active = 0;
      }
    }
  }

  rb->active = 1;
}
