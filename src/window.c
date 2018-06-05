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

  win->id = gtcaca_get_newid();
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

  win->color_focus_bg = gmo.theme.windowfocus.bg;
  win->color_focus_fg = gmo.theme.windowfocus.fg;
  win->color_nonfocus_bg = gmo.theme.window.bg;
  win->color_nonfocus_fg = gmo.theme.window.fg;
  
  gtcaca_window_set_focus(win);
  
  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)win);

  /* if (parent) { */
  /*   CDL_APPEND(parent->children, (gtcaca_widget_t *)win); */
  /* } */

  gtcaca_window_draw(win);

  return win;
}

void gtcaca_window_draw(gtcaca_window_widget_t *win)
{
  gtcaca_widget_colorize(GTCACA_WIDGET(win));

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
  gtcaca_widget_t *widget_children;

  /* Disable focus of previous windows, since this one takes it */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW) {
      widget->has_focus = 0;
      if (widget->children) {
	CDL_FOREACH(widget->children, widget_children) {
	  widget->has_focus = 0;
	}
      }
    }
  }

  /* gtcaca_log_write("win title:%s\n", win->window_title); */
  win->has_focus = 1;

  /* FIXME, have a child widgets focus strategy. For now we give the focus to all of them. */
  /* if (widget->children) { */
  /*   CDL_FOREACH(widget->children, widget_children) { */
  /*     widget_children->has_focus = 1; */
  /*     break; */
  /*   } */
  /* } */

}

gtcaca_window_widget_t *gtcaca_window_get_current_focus(void)
{
  gtcaca_widget_t *widget;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW) {
      if (widget->has_focus) { return (gtcaca_window_widget_t *)widget; }
    }
  }

  return NULL;
}

gtcaca_window_widget_t *gtcaca_window_get_next(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *widget;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW) {
      /* if (widget == (gtcaca_widget_t *)win) { */
      /* if (widget == (gtcaca_widget_t *)win) { */
	gtcaca_log_write("next title:%s\n", win->next->window_title);
	return win->next;
      /* } */
    }
  }

  return NULL;
}
