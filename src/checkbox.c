#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/checkbox.h>
#include <gtcaca/main.h>

static int _gtcaca_checkbox_private_key_press(gtcaca_checkbox_widget_t *checkbox, int key, void *userdata)
{
  switch (key) {
  case ' ':
    checkbox->checked = !checkbox->checked;
    break;
  }
  return 0;
}

gtcaca_checkbox_widget_t *gtcaca_checkbox_new(gtcaca_widget_t *parent, const char *label, int x, int y)
{
  gtcaca_checkbox_widget_t *checkbox;

  checkbox = malloc(sizeof(gtcaca_checkbox_widget_t));
  if (!checkbox) {
    fprintf(stderr, "Error: Cannot allocate checkbox\n");
    return NULL;
  }

  checkbox->id = gtcaca_get_newid();
  checkbox->has_focus = 0;
  checkbox->is_visible = 1;
  checkbox->type = GTCACA_WIDGET_CHECKBOX;
  checkbox->parent = parent;
  checkbox->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(checkbox), x, y);
  checkbox->width = (label ? (int)strlen(label) : 0) + 4; /* [x] + space + label */
  checkbox->height = 1;

  checkbox->color_focus_fg = gmo.theme.checkboxfocus.fg;
  checkbox->color_focus_bg = gmo.theme.checkboxfocus.bg;
  checkbox->color_nonfocus_fg = gmo.theme.checkbox.fg;
  checkbox->color_nonfocus_bg = gmo.theme.checkbox.bg;

  checkbox->label = label ? strdup(label) : NULL;
  checkbox->checked = 0;
  checkbox->private_key_cb = _gtcaca_checkbox_private_key_press;
  checkbox->key_cb = NULL;
  checkbox->key_cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(checkbox));

  return checkbox;
}

void gtcaca_checkbox_draw(gtcaca_checkbox_widget_t *checkbox)
{
  gtcaca_widget_colorize(GTCACA_WIDGET(checkbox));

  caca_put_char(gmo.cv, checkbox->x, checkbox->y, '[');
  caca_put_char(gmo.cv, checkbox->x + 1, checkbox->y, checkbox->checked ? 'x' : ' ');
  caca_put_char(gmo.cv, checkbox->x + 2, checkbox->y, ']');

  if (checkbox->label) {
    caca_printf(gmo.cv, checkbox->x + 4, checkbox->y, "%s", checkbox->label);
  }
}

int gtcaca_checkbox_key_cb_register(gtcaca_checkbox_widget_t *widget, gtcaca_checkbox_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
  return 0;
}

int gtcaca_checkbox_get_checked(gtcaca_checkbox_widget_t *checkbox)
{
  return checkbox->checked;
}

void gtcaca_checkbox_set_checked(gtcaca_checkbox_widget_t *checkbox, int checked)
{
  checkbox->checked = checked ? 1 : 0;
}
