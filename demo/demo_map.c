/* demo_map — the geoview/Map widget: a low-res world outline with labelled
   cities you can navigate (arrows pick the nearest city, Tab cycles). */
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/map.h>

struct city { const char *name; double lat, lon; uint8_t colour; };

int main(int argc, char **argv)
{
  static const struct city cities[] = {
    { "London",        51.5,   -0.13, CACA_YELLOW },
    { "New York",      40.7,  -74.0,  CACA_LIGHTGREEN },
    { "San Francisco", 37.8, -122.4,  CACA_LIGHTGREEN },
    { "Tokyo",         35.7,  139.7,  CACA_LIGHTRED },
    { "Sydney",       -33.9,  151.2,  CACA_LIGHTCYAN },
    { "Rio",          -22.9,  -43.2,  CACA_LIGHTMAGENTA },
    { "Cairo",         30.0,   31.2,  CACA_WHITE },
    { "Moscow",        55.8,   37.6,  CACA_YELLOW },
    { "Cape Town",    -33.9,   18.4,  CACA_WHITE },
    { "Singapore",      1.35, 103.8,  CACA_LIGHTBLUE },
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
  for (i = 0; i < (int)(sizeof cities / sizeof cities[0]); i++)
    gtcaca_map_add_point(map, cities[i].lat, cities[i].lon, 'o', cities[i].colour, cities[i].name);

  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(map));
  gtcaca_main();
  return 0;
}
