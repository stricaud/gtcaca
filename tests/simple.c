#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/textlist.h>
#include <gtcaca/button.h>

int textlist_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:
    caca_printf(gmo.cv, widget->x, widget->y + 20, "Value:%s", gtcaca_textlist_get_selected(widget));
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
  gtcaca_textlist_widget_t *textlist;
  gtcaca_button_widget_t *button;
  
  gtcaca_init(&argc, &argv);

  gtcaca_window_new("coucou", 1, 1, 50, 20);

  textlist = gtcaca_textlist_new(2, 2);
  gtcaca_textlist_append(textlist, "myfirst");
  gtcaca_textlist_append(textlist, "mysecond");
  gtcaca_textlist_append(textlist, "mythird");

  gtcaca_textlist_key_cb_register(textlist, textlist_key_press);

  button = gtcaca_button_new("Press me", 5, 5);
  gtcaca_button_key_cb_register(button, button_key_press);

  gtcaca_main();
  return 0;
}
