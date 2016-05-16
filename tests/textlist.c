#include <gtcaca/main.h>
#include <gtcaca/textlist.h>

int textlist_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:
    caca_printf(gmo.cv, widget->x, widget->y + 20, "Value:%s", gtcaca_textlist_get_selected(widget));
    break;
  }
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_textlist_widget_t *textlist;

  gtcaca_init(&argc, &argv);

  textlist = gtcaca_textlist_new(NULL, 2, 2);
  gtcaca_textlist_widget_set_view_size(textlist, 10);
  gtcaca_textlist_append(textlist, "myfirst");
  gtcaca_textlist_append(textlist, "mysecond");
  gtcaca_textlist_append(textlist, "mythird");

  gtcaca_textlist_key_cb_register(textlist, textlist_key_press);

  gtcaca_main();
  return 0;
}
