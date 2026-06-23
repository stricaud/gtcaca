/* demo_tabs — the Tabs widget (cf. tui-rs' tabs example). Left/Right or Tab
   switch tabs; the body below shows the selected tab's content. q quits. */
#include <string.h>
#include <caca.h>
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/tabs.h>
#include <gtcaca/textview.h>

/* replace the textview contents with `text` (split into lines) */
static void set_body(gtcaca_textview_widget_t *tv, const char *text)
{
  char buf[1024], *line, *save = NULL;
  strncpy(buf, text, sizeof buf - 1); buf[sizeof buf - 1] = '\0';
  gtcaca_textview_clear(tv);
  for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save))
    gtcaca_textview_append(tv, line);
}

int main(int argc, char **argv)
{
  static const char *titles[] = { "Tab0", "Tab1", "Tab2", "Tab3" };
  static const char *bodies[] = {
    "Welcome to the first tab.\nUse Left/Right or Tab to move between tabs.",
    "This is the second tab.\nEach tab can show its own content.",
    "Third tab here.\nThe Tabs widget only draws the bar; the body is yours.",
    "Last tab.\nPress q to quit.",
  };
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_tabs_widget_t *tabs;
  gtcaca_textview_widget_t *body;
  caca_event_t ev;
  int last = -1;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("gtcaca tabs");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height);

  tabs = gtcaca_tabs_new(GTCACA_WIDGET(win), 0, 0, win->width, 3);
  gtcaca_tabs_set_title(tabs, "Tabs  (Left/Right or Tab, q quits)");
  gtcaca_tabs_set_titles(tabs, titles, 4);

  body = gtcaca_textview_new(GTCACA_WIDGET(win), 0, 3, win->width, win->height - 3);
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(tabs));

  for (;;) {
    int sel = gtcaca_tabs_selected(tabs), k;
    if (sel != last) { set_body(body, bodies[sel]); last = sel; }
    gtcaca_redraw();
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    k = caca_get_event_key_ch(&ev);
    if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) break;
    gtcaca_tabs_key(tabs, k, NULL);
  }
  caca_free_display(gmo.dp);
  return 0;
}
