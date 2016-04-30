#include <stdio.h>

#include <caca.h>

#include <gtcaca/theme.h>
#include <gtcaca/main.h>
#include <gtcaca/textlist.h>
#include <gtcaca/window.h>

int gtcaca_init(int *argc, char ***argv)
{

  int retval;

  retval = gtcaca_theme_parse_ini(NULL);
  if (retval < 0) {
    return -1;
  }

  gmo.dp = caca_create_display(NULL);
  if (!gmo.dp) {
    fprintf(stderr, "Error, cannot create display!\n");
    return -1;
  }

  gmo.cv = caca_get_canvas(gmo.dp);

  gmo.widgets_list = NULL;
  
  return 0;
}

void _gtcaca_widget_redraw(gtcaca_widget_t *widget)
{
  switch (widget->type) {
  case GTCACA_WIDGET_WINDOW:
    gtcaca_window_draw((gtcaca_window_widget_t *)widget);
    break;
  case GTCACA_WIDGET_TEXTLIST:
    gtcaca_textlist_draw((gtcaca_textlist_widget_t *)widget);
    break;
  case GTCACA_WIDGET_BUTTON:
    break;
  case GTCACA_WIDGET_CALENDAR:
    break;
  case GTCACA_WIDGET_DIALOG:
    break;
  case GTCACA_WIDGET_ENTRY:
    break;
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
    break;
  case GTCACA_WIDGET_IMAGE:
    break;
  case GTCACA_WIDGET_LABEL:
    break;
  case GTCACA_WIDGET_MENU:
    break;
  case GTCACA_WIDGET_PROGRESSBAR:
    break;
  case GTCACA_WIDGET_RADIOBUTTON:
    break;
  case GTCACA_WIDGET_STATUSBAR:
    break;
  case GTCACA_WIDGET_TEXTVIEW:
    break;
  }

  if (widget->children) {
    _gtcaca_widget_redraw((gtcaca_widget_t *)widget->children);
  }

}

void gtcaca_redraw(void)
{
  gtcaca_widget_t *widget = NULL;

  LL_FOREACH(gmo.widgets_list, widget) {
    _gtcaca_widget_redraw(widget);
  }

  caca_refresh_display(gmo.dp);
}

int _gtcaca_widget_handle_key_press(gtcaca_widget_t *widget, int key)
{
  gtcaca_textlist_widget_t *textlist;

  if (!widget->has_focus) { return 0; }
  
  switch (widget->type) {
  case GTCACA_WIDGET_WINDOW:    
    break;
  case GTCACA_WIDGET_TEXTLIST:
    textlist = (gtcaca_textlist_widget_t *)widget;
    textlist->private_key_cb(textlist, key, NULL);

    if (textlist->key_cb) {
      textlist->key_cb(textlist, key, NULL);
    }
    gtcaca_redraw();
    break;
  case GTCACA_WIDGET_BUTTON:
    break;
  case GTCACA_WIDGET_CALENDAR:
    break;
  case GTCACA_WIDGET_DIALOG:
    break;
  case GTCACA_WIDGET_ENTRY:
    break;
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
    break;
  case GTCACA_WIDGET_IMAGE:
    break;
  case GTCACA_WIDGET_LABEL:
    break;
  case GTCACA_WIDGET_MENU:
    break;
  case GTCACA_WIDGET_PROGRESSBAR:
    break;
  case GTCACA_WIDGET_RADIOBUTTON:
    break;
  case GTCACA_WIDGET_STATUSBAR:
    break;
  case GTCACA_WIDGET_TEXTVIEW:
    break;
  }

  if (widget->children) {
    _gtcaca_widget_handle_key_press((gtcaca_widget_t *)widget->children, key);
  }

}


int gtcaca_widgets_handle_key_press(int key)
{
  gtcaca_widget_t *widget = NULL;

  LL_FOREACH(gmo.widgets_list, widget) {
    _gtcaca_widget_handle_key_press(widget, key);
  }

  caca_refresh_display(gmo.dp);

}

void gtcaca_main(void)
{
  int quit = 0;
  int key;
  
    while (!quit) {
    caca_event_t ev;
    
    while(caca_get_event(gmo.dp, CACA_EVENT_ANY, &ev, 0)) {
      if (caca_get_event_type(&ev) & CACA_EVENT_KEY_PRESS) {
	key = caca_get_event_key_ch(&ev);
	switch(key) {
	case 'q':
	case 'Q':
	case CACA_KEY_ESCAPE:
	  quit = 1;
	  break;
	default:
	  gtcaca_widgets_handle_key_press(key);
	  break;
	  /* case CACA_KEY_RETURN: */
	/*   break; */
	/* case CACA_KEY_UP: */
	/*   break; */
	/* case CACA_KEY_DOWN: */
	/*   break; */
	}
      }
      
      gtcaca_redraw();
    }
  }
  
  caca_free_display(gmo.dp);

}

