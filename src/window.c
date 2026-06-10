#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/window.h>
#include <gtcaca/main.h>

#define MAX_FOCUSABLE_CHILDREN 64

static int _gtcaca_widget_is_focusable(gtcaca_widget_t *widget)
{
  switch (widget->type) {
  case GTCACA_WIDGET_BUTTON:
  case GTCACA_WIDGET_TEXTLIST:
  case GTCACA_WIDGET_ENTRY:
  case GTCACA_WIDGET_CHECKBOX:
  case GTCACA_WIDGET_RADIOBUTTON:
  case GTCACA_WIDGET_COMBOBOX:
  case GTCACA_WIDGET_TEXTVIEW:
  case GTCACA_WIDGET_SCALE:
  case GTCACA_WIDGET_SPINBUTTON:
  case GTCACA_WIDGET_SWITCH:
  case GTCACA_WIDGET_EXPANDER:
    return 1;
  default:
    return 0;
  }
}

gtcaca_window_widget_t *gtcaca_window_new(gtcaca_widget_t *parent, char *window_title, int x, int y, int width, int height)
{
  gtcaca_window_widget_t *win;

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
  win->focused_child = NULL;

  win->color_focus_bg = gmo.theme.windowfocus.bg;
  win->color_focus_fg = gmo.theme.windowfocus.fg;
  win->color_nonfocus_bg = gmo.theme.window.bg;
  win->color_nonfocus_fg = gmo.theme.window.fg;

  gtcaca_window_set_focus(win);

  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)win);

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
}

void gtcaca_window_set_focus(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *widget;

  /* Remove focus from all windows and children of windows */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW ||
        (widget->parent && widget->parent->type == GTCACA_WIDGET_WINDOW)) {
      widget->has_focus = 0;
    }
  }

  win->has_focus = 1;

  /* Restore focused_child or find first focusable child */
  if (win->focused_child) {
    win->focused_child->has_focus = 1;
  } else {
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (widget->parent == (gtcaca_widget_t *)win && _gtcaca_widget_is_focusable(widget)) {
        widget->has_focus = 1;
        win->focused_child = widget;
        break;
      }
    }
  }
}

gtcaca_window_widget_t *gtcaca_window_get_current_focus(void)
{
  gtcaca_widget_t *widget;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW) {
      if (widget->has_focus) return (gtcaca_window_widget_t *)widget;
    }
  }

  return NULL;
}

gtcaca_window_widget_t *gtcaca_window_get_next(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *widget;
  int found = 0;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (found && widget->type == GTCACA_WIDGET_WINDOW)
      return (gtcaca_window_widget_t *)widget;
    if (widget == (gtcaca_widget_t *)win)
      found = 1;
  }

  /* Wrap: return first window */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW)
      return (gtcaca_window_widget_t *)widget;
  }

  return NULL;
}

gtcaca_window_widget_t *gtcaca_window_get_prev(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *widget;
  gtcaca_window_widget_t *prev = NULL, *last = NULL;

  /* First pass: find the last window for wrap-around */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_WINDOW)
      last = (gtcaca_window_widget_t *)widget;
  }

  /* Second pass: find the window before win */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type != GTCACA_WIDGET_WINDOW) continue;
    if (widget == (gtcaca_widget_t *)win)
      return prev ? prev : last;
    prev = (gtcaca_window_widget_t *)widget;
  }

  return NULL;
}

void gtcaca_window_set_focused_child(gtcaca_window_widget_t *win, gtcaca_widget_t *child)
{
  gtcaca_widget_t *widget;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->parent == (gtcaca_widget_t *)win) {
      widget->has_focus = 0;
    }
  }

  if (child) {
    child->has_focus = 1;
    win->focused_child = child;
  }
}

void gtcaca_window_focus_next_child(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *focusable[MAX_FOCUSABLE_CHILDREN];
  int n = 0, current = -1, i;
  gtcaca_widget_t *widget;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->parent == (gtcaca_widget_t *)win &&
        _gtcaca_widget_is_focusable(widget) &&
        n < MAX_FOCUSABLE_CHILDREN) {
      if (widget->has_focus) current = n;
      focusable[n++] = widget;
    }
  }

  if (n == 0) return;

  if (current >= 0) focusable[current]->has_focus = 0;

  i = (current + 1) % n;
  focusable[i]->has_focus = 1;
  win->focused_child = focusable[i];
}

void gtcaca_window_focus_prev_child(gtcaca_window_widget_t *win)
{
  gtcaca_widget_t *focusable[MAX_FOCUSABLE_CHILDREN];
  int n = 0, current = -1, i;
  gtcaca_widget_t *widget;

  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->parent == (gtcaca_widget_t *)win &&
        _gtcaca_widget_is_focusable(widget) &&
        n < MAX_FOCUSABLE_CHILDREN) {
      if (widget->has_focus) current = n;
      focusable[n++] = widget;
    }
  }

  if (n == 0) return;

  if (current >= 0) focusable[current]->has_focus = 0;

  i = (current - 1 + n) % n;
  focusable[i]->has_focus = 1;
  win->focused_child = focusable[i];
}
