#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/textlist.h>
#include <gtcaca/label.h>

gtcaca_label_widget_t *label;

int textlist_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_RETURN:
    //    label->label = gtcaca_textlist_get_text_selected(widget);
    break;
  }
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_textlist_widget_t *textlist;
  gtcaca_window_widget_t *win2;
  gtcaca_textlist_widget_t *textlist2;

  gtcaca_init(&argc, &argv);
  
  app = gtcaca_application_new("Testing buttons");

  //  label = gtcaca_label_new(NULL, "<return to get value>", 10, 10);

  win = gtcaca_window_new(GTCACA_WIDGET(app), NULL, app->x, app->y, app->width / 2, app->height);
  win2 = gtcaca_window_new(GTCACA_WIDGET(app), "The second window", app->x + (app->width / 2), app->y, app->width / 2, app->height);

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

  textlist2 = gtcaca_textlist_new((gtcaca_widget_t *)win2, 2, 2);
  /* gtcaca_textlist_widget_set_view_size(textlist, 10); */
  gtcaca_textlist_append(textlist2, "foo");
  gtcaca_textlist_append(textlist2, "bar");
  gtcaca_textlist_append(textlist2, "camp");
  gtcaca_textlist_key_cb_register(textlist2, textlist_key_press, NULL);
  
  gtcaca_main();
  return 0;
}
