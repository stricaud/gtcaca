#include <stdio.h>

#include <gtcaca/log.h>
#include <gtcaca/widget.h>
#include <gtcaca/window.h>

char *gtcaca_widget_type_to_string(gtcaca_widget_type_t type)
{
  switch(type) {
  case GTCACA_WIDGET_APPLICATION:
    return "Application";
  case GTCACA_WIDGET_WINDOW:
    return "Window";
  case GTCACA_WIDGET_TEXTLIST:
    return "TextList";
  case GTCACA_WIDGET_BUTTON:
    return "Button";
  case GTCACA_WIDGET_CALENDAR:
    return "Calendar";
  case GTCACA_WIDGET_DIALOG:
    return "Dialog";
  case GTCACA_WIDGET_ENTRY:
    return "Entry";
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
    return "Filechooserdialog";
  case GTCACA_WIDGET_IMAGE:
    return "Image";
  case GTCACA_WIDGET_LABEL:
    return "Label";
  case GTCACA_WIDGET_MENU:
    return "Menu";
  case GTCACA_WIDGET_PROGRESSBAR:
    return "Progressbar";
  case GTCACA_WIDGET_RADIOBUTTON:
    return "Radiobutton";
  case GTCACA_WIDGET_STATUSBAR:
    return "Statusbar";
  case GTCACA_WIDGET_TEXTVIEW:
  default:
    return "unknown";
  }
}

void gtcaca_widget_debug(gtcaca_widget_t *widget)
{
  gtcaca_log_write("=======");
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
    gtcaca_log_write("*\n");
    gtcaca_log_write("window title:%s\n", window->window_title);
  }
  gtcaca_log_write(".\n");
}
