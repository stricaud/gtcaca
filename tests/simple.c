#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/textlist.h>

int main(int argc, char **argv)
{
  gtcaca_textlist_widget_t *textlist;
  
  gtcaca_init(&argc, &argv);

  gtcaca_window_new("coucou", 1, 1, 50, 20);

  textlist = gtcaca_textlist_new(2, 2);
  gtcaca_textlist_append(textlist, "premier");
  gtcaca_textlist_append(textlist, "deuxième");
  gtcaca_textlist_append(textlist, "troisième");
  
  gtcaca_main();
  return 0;
}
