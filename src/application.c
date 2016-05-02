#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/application.h>
#include <gtcaca/main.h>
#include <gtcaca/label.h>
#include <gtcaca/window.h>

/* Private functions */
static int _gtcaca_application_private_key_press(gtcaca_application_widget_t *application, int key, void *userdata)
{
  gtcaca_widget_t *widget;
  gtcaca_window_widget_t *window;
  int found_window = 0;
  int widget_count = 0;
  int counter = 0;

  switch(key) {
  case CACA_KEY_TAB:
    /* First search the current selected window */
    LL_FOREACH(gmo.widgets_list, widget) {
      if (widget->type == GTCACA_WIDGET_WINDOW) {
    	if (widget->has_focus) {
    	  break;
    	}
      }
      widget_count++;
    }

    /* Step 2: select the following window */
    /* LL_FOREACH(gmo.widgets_list, widget) { */
    /*   if (widget_count > counter) { */
    /*   	if (widget->type == GTCACA_WIDGET_WINDOW) { */
    /*   	  gtcaca_window_set_focus((gtcaca_window_widget_t *)widget); */
    /* 	  return 0; */
    /*   	} */
    /*   } */
    /*   counter++; */
    /* } */

    /* Step 3: there was no following window. We select the first one */
    if (!found_window) {
    LL_FOREACH(gmo.widgets_list, widget) {
	if (widget->type == GTCACA_WIDGET_WINDOW) {
	  gtcaca_window_set_focus((gtcaca_window_widget_t *)widget);
	  break;
	}
    }
      
    }
    
    break; // case CACA_KEY_TAB:
  } // switch(key) {

  return 0;
}

int gtcaca_application_exists(void)
{
  gtcaca_widget_t *widget = NULL;
  int exists = 0;
  
  LL_FOREACH(gmo.widgets_list, widget) {
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
  LL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)application);

  return application;

}

void gtcaca_application_draw(gtcaca_application_widget_t *application)
{
  application->width = caca_get_canvas_width(gmo.cv);
  application->height = caca_get_canvas_height(gmo.cv);
}
