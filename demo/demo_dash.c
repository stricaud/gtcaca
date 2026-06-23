/* demo_dash — termdash-style dashboard: a seven-segment read-out over a live
   line chart. Press q to quit. */
#include <math.h>
#include <caca.h>
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/segdisplay.h>
#include <gtcaca/linechart.h>

#define N 60

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_segdisplay_widget_t *seg;
  gtcaca_linechart_widget_t *chart;
  double a[N], b[N];
  caca_event_t ev;
  int cw, chh, frame = 0, i;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("gtcaca dashboard");
  cw = caca_get_canvas_width(gmo.cv); chh = caca_get_canvas_height(gmo.cv);
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, 0, 0, cw, chh);

  seg = gtcaca_segdisplay_new(GTCACA_WIDGET(win), 0, 0, win->width, 9);
  gtcaca_segdisplay_set_title(seg, "value");
  gtcaca_segdisplay_set_colour(seg, CACA_LIGHTGREEN);
  chart = gtcaca_linechart_new(GTCACA_WIDGET(win), 0, 9, win->width, win->height - 9);
  gtcaca_linechart_set_title(chart, "live  (q quits)");
  gtcaca_linechart_set_range(chart, -1.2, 1.2);

  for (;;) {
    char txt[16];
    for (i = 0; i < N; i++) {
      a[i] = sin((i + frame) * 0.2);
      b[i] = 0.7 * cos((i + frame) * 0.13);
    }
    gtcaca_linechart_clear(chart);
    gtcaca_linechart_add_series(chart, a, N, CACA_LIGHTGREEN);
    gtcaca_linechart_add_series(chart, b, N, CACA_LIGHTCYAN);
    snprintf(txt, sizeof txt, "%5.2f", a[N - 1]);
    gtcaca_segdisplay_set_text(seg, txt);

    gtcaca_redraw();
    if (caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, 120000)) {
      int k = caca_get_event_key_ch(&ev);
      if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) break;
    }
    frame++;
  }
  caca_free_display(gmo.dp);
  return 0;
}
