#include <caca.h>

#include <gtcaca/theme.h>

void gtcaca_window_new(caca_canvas_t *cv, char *window_title, int x, int y, int width, int height)
{
  int i;

  caca_set_color_ansi(cv, theme.window.fg, theme.window.bg);
  caca_fill_box(cv, x, y, width, height, ' ');
  caca_draw_cp437_box(cv, x, y, width, height);

  if (window_title) {
    caca_printf(cv, x + 2, y, "| %s |", window_title);
  }

}
