#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/application.h>
#include <gtcaca/main.h>
#include <gtcaca/label.h>
#include <gtcaca/log.h>
#include <gtcaca/window.h>

/* Private functions */
static int _gtcaca_application_private_key_press(gtcaca_application_widget_t *application, int key, void *userdata)
{
  gtcaca_widget_t *widget;
  gtcaca_window_widget_t *window;
  gtcaca_window_widget_t *newwindow;
  int found_window = 0;
  int widget_count = 0;
  int counter = 0;

  switch(key) {
  case CACA_KEY_CTRL_N:
    window = gtcaca_window_get_current_focus();
    if (window) {
      while(window = (gtcaca_window_widget_t *)window->next) {
	if (window->type == GTCACA_WIDGET_WINDOW) {
	  gtcaca_window_set_focus(window);
	  break;
	}
      }
    }
    break; // case CACA_KEY_CTRL_N:
  case CACA_KEY_CTRL_P:
    window = gtcaca_window_get_current_focus();
    if (window) {
      while(window = (gtcaca_window_widget_t *)window->prev) {
	if (window->type == GTCACA_WIDGET_WINDOW) {
	  gtcaca_window_set_focus(window);
	  break;
	}
      }
    }
    break; // case CACA_KEY_CTRL_P:
  } // switch(key) {

  return 0;
}

int gtcaca_application_exists(void)
{
  gtcaca_widget_t *widget = NULL;
  int exists = 0;
  
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type == GTCACA_WIDGET_APPLICATION) {
      exists = 1;
      break;
    }
  }

  return exists;
}

gtcaca_application_widget_t *gtcaca_application_new(char *application_title)
{
  gtcaca_application_widget_t *application;
  int i;


  application = malloc(sizeof(gtcaca_application_widget_t));
  if (!application) {
    fprintf(stderr, "Error: Cannot allocate new application\n");
    return NULL;
  }

  application->id = gtcaca_get_newid();
  application->has_focus = 1;
  application->is_visible = 1;
  application->application_title = application_title;
  application->x = 0;
  application->y = 0;
  application->width = caca_get_canvas_width(gmo.cv);
  application->height = caca_get_canvas_height(gmo.cv);
  application->type = GTCACA_WIDGET_APPLICATION;
  application->parent = NULL;
  application->children = NULL;

  application->private_key_cb = _gtcaca_application_private_key_press;

  caca_set_display_title(gmo.dp, application_title);
  /* gtcaca_application_draw(application); */
  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)application);

  return application;

}

void gtcaca_application_draw(gtcaca_application_widget_t *application)
{
  application->width = caca_get_canvas_width(gmo.cv);
  application->height = caca_get_canvas_height(gmo.cv);
}
