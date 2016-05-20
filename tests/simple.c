#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/textlist.h>
#include <gtcaca/button.h>
#include <gtcaca/label.h>

int textlist_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:
    caca_printf(gmo.cv, widget->x, widget->y + 20, "Value:%s", gtcaca_textlist_get_text_selected(widget));
    break;
  }
}

int button_key_press(gtcaca_button_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:
    caca_printf(gmo.cv, widget->x+5, widget->y + 4, "Widget returned");    
    break;
  }
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_textlist_widget_t *textlist;
  gtcaca_button_widget_t *button;
  gtcaca_label_widget_t *label;

  gtcaca_init(&argc, &argv);

  app = gtcaca_application_new("The simple test");
  
  gtcaca_window_new((gtcaca_widget_t *)app, "coucou", app->x, app->y, app->width/2, app->height / 2);
  win = gtcaca_window_new((gtcaca_widget_t *)app, "I steal the focus", app->x + app->width/2, app->y, app->width/2, app->height / 2);
  gtcaca_window_new((gtcaca_widget_t *)app, "Window 3", app->x, app->y + (app->height/2), app->width/2, app->height / 2);
  gtcaca_window_new((gtcaca_widget_t *)app, "Window 4", app->x + app->width/2, app->y + (app->height/2), app->width/2, app->height / 2);

  /* label = gtcaca_label_new(NULL, "Hello", 15, 15); */
  
  /* textlist = gtcaca_textlist_new(2, 2); */
  /* gtcaca_textlist_append(textlist, "myfirst"); */
  /* gtcaca_textlist_append(textlist, "mysecond"); */
  /* gtcaca_textlist_append(textlist, "mythird"); */

  /* gtcaca_textlist_key_cb_register(textlist, textlist_key_press); */

  /* button = gtcaca_button_new((gtcaca_widget_t *)win, "Press me", 5, 5); */
  /* gtcaca_button_key_cb_register(button, button_key_press); */

  /* label = gtcaca_label_new("Hello", 15, 15); */
  
  gtcaca_main();
  return 0;
}
