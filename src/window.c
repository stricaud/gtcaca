#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/window.h>
#include <gtcaca/main.h>

gtcaca_window_widget_t *gtcaca_window_new(gtcaca_widget_t *parent, char *window_title, int x, int y, int width, int height)
{
  gtcaca_window_widget_t *win;
  gtcaca_widget_t *widget = NULL;
  int i;

  win = malloc(sizeof(gtcaca_window_widget_t));
  if (!win) {
    fprintf(stderr, "Error: Cannot allocate new window\n");
    return NULL;
  }

  win->has_focus = 1;
  win->is_visible = 1;
  win->window_title = window_title;
  win->x = x;
  win->y = y;
  win->width = width;
  win->height = height;
  win->type = GTCACA_WIDGET_WINDOW;
  win->parent = parent;
  win->children = NULL;

  gtcaca_window_set_focus(win);
  
  LL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)win);

  /* if (parent) { */
  /*   LL_APPEND(parent->children, (gtcaca_widget_t *)win); */
  /* } */

  gtcaca_window_draw(win);

  return win;
}

void gtcaca_window_draw(gtcaca_window_widget_t *win)
{
  if (win->has_focus) {
    caca_set_color_ansi(gmo.cv, gmo.theme.windowfocus.fg, gmo.theme.windowfocus.bg);
  } else {
    caca_set_color_ansi(gmo.cv, gmo.theme.window.fg, gmo.theme.window.bg);
  }
  caca_fill_box(gmo.cv, win->x, win->y, win->width, win->height, ' ');
  caca_draw_cp437_box(gmo.cv, win->x, win->y, win->width, win->height);

  if (win->window_title) {
    caca_printf(gmo.cv, win->x + 2, win->y, "| %s |", win->window_title);
  }

  caca_refresh_display(gmo.dp);
}

void gtcaca_window_set_focus(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *widget;
  
  /* Disable focus of previous windows, since this one takes it */
  LL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW) {
      widget->has_focus = 0;
    }
  }
  win->has_focus = 1;
}
