#include <gtcaca/main.h>

int main(int argc, char **argv)
{
  gtcaca_init(&argc, &argv);

  gtcaca_window_new("coucou", 1, 1, 50, 20);

  gtcaca_main();
  return 0;
}
