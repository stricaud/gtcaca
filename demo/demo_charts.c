/* demo_charts — the termui-style data widgets: Sparkline, Gauge, BarChart.
   All three fill with coloured backgrounds, so they render in any terminal. */
#include <math.h>
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/label.h>
#include <gtcaca/sparkline.h>
#include <gtcaca/gauge.h>
#include <gtcaca/barchart.h>

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_sparkline_widget_t *sl;
  gtcaca_gauge_widget_t *g1, *g2;
  gtcaca_barchart_widget_t *bc;
  float spark[60], bars[7];
  const char *labels[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
  int i;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("gtcaca charts");
  win = gtcaca_window_new((gtcaca_widget_t *)app, "Data widgets: Sparkline / Gauge / BarChart",
                          app->x, app->y, app->width, app->height);

  for (i = 0; i < 60; i++) spark[i] = (float)(sin(i * 0.35) * 0.5 + 0.5) * 100.0f;
  bars[0] = 5; bars[1] = 9; bars[2] = 3; bars[3] = 8; bars[4] = 6; bars[5] = 7; bars[6] = 4;

  gtcaca_label_new(GTCACA_WIDGET(win), "Sparkline", 2, 2);
  sl = gtcaca_sparkline_new(GTCACA_WIDGET(win), 14, 2, 60, 4);
  gtcaca_sparkline_set_data(sl, spark, 60);
  gtcaca_sparkline_set_color(sl, CACA_GREEN);
  gtcaca_sparkline_set_style_auto(sl);   /* blocks where the font has them, else area */

  gtcaca_label_new(GTCACA_WIDGET(win), "Gauge", 2, 7);
  g1 = gtcaca_gauge_new(GTCACA_WIDGET(win), 14, 7, 40);
  gtcaca_gauge_set_percent(g1, 67);
  g2 = gtcaca_gauge_new(GTCACA_WIDGET(win), 14, 8, 40);
  gtcaca_gauge_set_percent(g2, 28);
  gtcaca_gauge_set_colors(g2, CACA_RED, CACA_DARKGRAY);
  gtcaca_gauge_set_label(g2, "disk almost full");

  gtcaca_label_new(GTCACA_WIDGET(win), "BarChart", 2, 10);
  bc = gtcaca_barchart_new(GTCACA_WIDGET(win), 14, 10, 56, 12);
  gtcaca_barchart_set_data(bc, bars, labels, 7);
  gtcaca_barchart_set_bar_width(bc, 6, 1);
  gtcaca_barchart_set_color(bc, CACA_CYAN);

  gtcaca_main();
  return 0;
}
