#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/button.h>

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
  gtcaca_button_widget_t *button;  
  
  gtcaca_init(&argc, &argv);

  app = gtcaca_application_new("Testing buttons");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height);
  button = gtcaca_button_new((gtcaca_widget_t *)win, "Press me", 5, 5);
  gtcaca_button_key_cb_register(button, button_key_press);

  gtcaca_main();
  return 0;
}
