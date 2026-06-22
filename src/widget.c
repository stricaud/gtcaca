#include <stdio.h>

#include <gtcaca/log.h>
#include <gtcaca/widget.h>
#include <gtcaca/window.h>

char *gtcaca_widget_type_to_string(gtcaca_widget_type_t type)
{
  switch (type) {
  case GTCACA_WIDGET_APPLICATION:   return "Application";
  case GTCACA_WIDGET_WINDOW:        return "Window";
  case GTCACA_WIDGET_TEXTLIST:      return "TextList";
  case GTCACA_WIDGET_BUTTON:        return "Button";
  case GTCACA_WIDGET_CALENDAR:      return "Calendar";
  case GTCACA_WIDGET_CHECKBOX:      return "Checkbox";
  case GTCACA_WIDGET_COMBOBOX:      return "ComboBox";
  case GTCACA_WIDGET_DIALOG:        return "Dialog";
  case GTCACA_WIDGET_ENTRY:         return "Entry";
  case GTCACA_WIDGET_FILECHOOSERDIALOG: return "FileChooserDialog";
  case GTCACA_WIDGET_IMAGE:         return "Image";
  case GTCACA_WIDGET_LABEL:         return "Label";
  case GTCACA_WIDGET_MENU:          return "Menu";
  case GTCACA_WIDGET_PROGRESSBAR:   return "ProgressBar";
  case GTCACA_WIDGET_RADIOBUTTON:   return "RadioButton";
  case GTCACA_WIDGET_STATUSBAR:     return "StatusBar";
  case GTCACA_WIDGET_TEXTVIEW:      return "TextView";
  case GTCACA_WIDGET_EDITOR:        return "Editor";
  default:                          return "Unknown";
  }
}

void gtcaca_widget_debug(gtcaca_widget_t *widget)
{
  gtcaca_log_write("=======\n");
  gtcaca_log_write("type:%s\n", gtcaca_widget_type_to_string(widget->type));
  gtcaca_log_write("id:%d\n", widget->id);
  gtcaca_log_write("has_focus:%d\n", widget->has_focus);
  gtcaca_log_write("is_visible:%d\n", widget->is_visible);
  gtcaca_log_write("x:%d\n", widget->x);
  gtcaca_log_write("y:%d\n", widget->y);
  gtcaca_log_write("width:%d\n", widget->width);
  gtcaca_log_write("height:%d\n", widget->height);
  if (widget->type == GTCACA_WIDGET_WINDOW) {
    gtcaca_window_widget_t *window = (gtcaca_window_widget_t *)widget;
    gtcaca_log_write("window title:%s\n", window->window_title);
  }
  gtcaca_log_write(".\n");
}

void gtcaca_widget_position_size_parent(gtcaca_widget_t *parent, gtcaca_widget_t *widget, int x, int y)
{
  if (parent) {
    widget->x = parent->x + x;
    widget->y = parent->y + y;
    widget->width = parent->width - x - 1;
    widget->height = parent->height - y - 1;
  } else {
    widget->x = x;
    widget->y = y;
    widget->width = 0;
    widget->height = 0;
  }
}

void gtcaca_widget_printall(void)
{
  gtcaca_widget_t *widget = NULL;
  CDL_FOREACH(gmo.widgets_list, widget) {
    gtcaca_widget_debug(widget);
  }
}

void gtcaca_widget_colorize_from_parent(gtcaca_widget_t *widget)
{
  if (widget->parent) {
    if (widget->parent->has_focus) {
      caca_set_color_ansi(gmo.cv, widget->parent->color_focus_fg, widget->parent->color_focus_bg);
    } else {
      caca_set_color_ansi(gmo.cv, widget->parent->color_nonfocus_fg, widget->parent->color_nonfocus_bg);
    }
  } else {
    caca_set_color_ansi(gmo.cv, widget->color_nonfocus_fg, widget->color_nonfocus_bg);
  }
}

void gtcaca_widget_colorize(gtcaca_widget_t *widget)
{
  if (widget->has_focus) {
    caca_set_color_ansi(gmo.cv, widget->color_focus_fg, widget->color_focus_bg);
  } else {
    caca_set_color_ansi(gmo.cv, widget->color_nonfocus_fg, widget->color_nonfocus_bg);
  }
}

void gtcaca_widget_show(gtcaca_widget_t *widget)
{
  widget->is_visible = 1;
}

void gtcaca_widget_hide(gtcaca_widget_t *widget)
{
  widget->is_visible = 0;
  if (widget->has_focus)
    widget->has_focus = 0;
}

int gtcaca_widget_is_focusable(gtcaca_widget_t *widget)
{
  switch (widget->type) {
  case GTCACA_WIDGET_BUTTON:
  case GTCACA_WIDGET_TEXTLIST:
  case GTCACA_WIDGET_ENTRY:
  case GTCACA_WIDGET_CHECKBOX:
  case GTCACA_WIDGET_RADIOBUTTON:
  case GTCACA_WIDGET_COMBOBOX:
  case GTCACA_WIDGET_TEXTVIEW:
  case GTCACA_WIDGET_EDITOR:
    return 1;
  default:
    return 0;
  }
}
