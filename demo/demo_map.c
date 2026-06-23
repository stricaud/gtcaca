/* demo_map — the geoview/Map widget: a low-res world outline with labelled
   cities you can navigate (arrows pick the nearest city, Tab cycles). */
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/map.h>

int main(int argc, char **argv)
{
  /* Plot well-known places straight from the built-in gazetteer — no need to
     spell out coordinates (see gtcaca_map_add_city / gtcaca_map_find_city). */
  static const char *places[] = {
    "London", "New York", "San Francisco", "Tokyo", "Sydney",
    "Rio de Janeiro", "Sao Paulo", "Cairo", "Moscow", "Cape Town",
    "Singapore", "Mumbai", "Buenos Aires", "Nairobi", "Reykjavik",
  };
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_map_widget_t *map;
  int i;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("gtcaca map");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height);

  map = gtcaca_map_new(GTCACA_WIDGET(win), 0, 0, win->width, win->height);
  gtcaca_map_set_title(map, "World map - arrows move between cities, Tab cycles, q quits");
  gtcaca_map_add_world(map, CACA_GREEN);
  for (i = 0; i < (int)(sizeof places / sizeof places[0]); i++)
    gtcaca_map_add_city(map, places[i], 'o', CACA_LIGHTGREEN);

  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(map));
  gtcaca_main();
  return 0;
}
