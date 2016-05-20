#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/textlist.h>

int textlist_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:
    caca_printf(gmo.cv, widget->x, widget->y + 20, "Value:%s", gtcaca_textlist_get_text_selected(widget));
    break;
  }
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_textlist_widget_t *textlist;

  gtcaca_init(&argc, &argv);

  app = gtcaca_application_new("Testing buttons");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height);

  textlist = gtcaca_textlist_new((gtcaca_widget_t *)win, 2, 2);
  /* gtcaca_textlist_widget_set_view_size(textlist, 10); */
  gtcaca_textlist_append(textlist, "myfirst");
  gtcaca_textlist_append(textlist, "mysecond");
  gtcaca_textlist_append(textlist, "mythird");
  gtcaca_textlist_append(textlist, "my4");
  gtcaca_textlist_append(textlist, "my5");
  gtcaca_textlist_append(textlist, "my6");
  gtcaca_textlist_append(textlist, "my7");
  gtcaca_textlist_append(textlist, "my8");
  gtcaca_textlist_append(textlist, "my9");
  gtcaca_textlist_append(textlist, "my10");
  gtcaca_textlist_append(textlist, "my11");
  gtcaca_textlist_append(textlist, "my12");
  gtcaca_textlist_append(textlist, "my13");
  gtcaca_textlist_append(textlist, "my14");
  gtcaca_textlist_append(textlist, "my15");
  gtcaca_textlist_append(textlist, "my16");
  gtcaca_textlist_append(textlist, "my17");
  gtcaca_textlist_append(textlist, "my18");
  gtcaca_textlist_append(textlist, "my19");
  gtcaca_textlist_append(textlist, "my20");
  gtcaca_textlist_append(textlist, "my21");
  gtcaca_textlist_append(textlist, "my22");
  gtcaca_textlist_append(textlist, "my23");
  gtcaca_textlist_append(textlist, "my24");
  gtcaca_textlist_append(textlist, "my25");

  gtcaca_textlist_key_cb_register(textlist, textlist_key_press, NULL);

  gtcaca_main();
  return 0;
}
