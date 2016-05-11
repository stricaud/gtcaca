#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <caca.h>

#include <gtcaca/button.h>
#include <gtcaca/main.h>

static int _gtcaca_button_private_key_press(gtcaca_button_widget_t *button, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:    
    /* button->is_visible = 0; */
    /* gtcaca_redraw(); */
    /* gtcaca_button_draw_press_noshade(button); */
    break;
  }
}

gtcaca_button_widget_t *gtcaca_button_new(gtcaca_widget_t *parent, char *button_label, int x, int y)
{
  gtcaca_button_widget_t *button;
  int i;

  button = malloc(sizeof(gtcaca_button_widget_t));
  if (!button) {
    fprintf(stderr, "Error: Cannot allocate new button\n");
    return NULL;
  }

  button->id = gtcaca_get_newid();
  button->has_focus = 1;
  button->is_visible = 1;
  button->button_label = button_label;
  button->x = parent ? parent->x + x : x;
  button->y = parent ? parent->y + y : y;
  button->width = strlen(button_label) + 2;
  button->height = 3;
  button->type = GTCACA_WIDGET_BUTTON;
  button->parent = parent;
  button->children = NULL;
  if (parent) {
    CDL_APPEND(parent->children, (gtcaca_widget_t *)button);
  }

  button->private_key_cb = _gtcaca_button_private_key_press;
  button->key_cb = NULL;

  gtcaca_button_draw(button);

  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)button);

  return button;
}

int gtcaca_button_key_cb_register(gtcaca_button_widget_t *widget, gtcaca_button_key_cb_t key_cb)
{
  widget->key_cb = key_cb;
}

void gtcaca_button_draw_press_noshade(gtcaca_button_widget_t *button)
{
  if (button->has_focus) {
    caca_set_color_ansi(gmo.cv, gmo.theme.buttonfocus.fg, gmo.theme.buttonfocus.bg);
  } else {
    caca_set_color_ansi(gmo.cv, gmo.theme.button.fg, gmo.theme.button.bg);
  }
    
  caca_fill_box(gmo.cv, button->x + 1, button->y + 1, button->width, button->height, ' ');
  caca_draw_cp437_box(gmo.cv, button->x + 1, button->y + 1, button->width, button->height);

  if (button->button_label) {
    caca_printf(gmo.cv, button->x + 2, button->y + 2, "%s", button->button_label);
  }

  caca_refresh_display(gmo.dp);
}

void gtcaca_button_draw(gtcaca_button_widget_t *button)
{
  int i;

  if (button->has_focus) {
    caca_set_color_ansi(gmo.cv, gmo.theme.buttonfocus.fg, gmo.theme.buttonfocus.bg);
  } else {
    caca_set_color_ansi(gmo.cv, gmo.theme.button.fg, gmo.theme.button.bg);
  }
    
  caca_fill_box(gmo.cv, button->x, button->y, button->width, button->height, ' ');
  caca_draw_cp437_box(gmo.cv, button->x, button->y, button->width, button->height);

  if (button->button_label) {
    caca_printf(gmo.cv, button->x + 1, button->y + 1, "%s", button->button_label);
  }

  caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_BLACK);
  for (i = 1; i <= button->width; i++) {
    caca_put_char(gmo.cv, button->x + i, button->y + 3, ' ');
  }
  caca_put_char(gmo.cv, button->x + button->width, button->y + 2, ' ');
  caca_put_char(gmo.cv, button->x + button->width, button->y + 1, ' ');


  caca_refresh_display(gmo.dp);
}
