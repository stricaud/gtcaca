/*
 * GTCaca JSON TextView Demo
 *
 * Demonstrates GTCACA_TEXTVIEW_MODE_JSON syntax highlighting:
 *   - Keys:              cyan
 *   - String values:     green
 *   - Numbers:           magenta
 *   - true / false / null: red
 *   - Braces / brackets: yellow
 *   - Colons / commas:   light gray
 *
 * Usage:  ./tests/demo_json [path/to/file.json]
 * If no file is given a built-in sample JSON is shown.
 *
 * Controls:
 *   Up / Down – scroll
 *   Home / End – jump to top/bottom
 *   Q / Esc   – quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/textview.h>
#include <gtcaca/statusbar.h>

/* Built-in sample that exercises every token type. */
static const char *sample_json[] = {
  "{",
  "  \"name\": \"gtcaca\",",
  "  \"version\": \"1.0.0\",",
  "  \"description\": \"A libcaca-based TUI widget toolkit\",",
  "  \"active\": true,",
  "  \"deprecated\": false,",
  "  \"license\": null,",
  "  \"stats\": {",
  "    \"widgets\": 20,",
  "    \"lines_of_code\": 4200,",
  "    \"test_coverage\": 0.73",
  "  },",
  "  \"authors\": [",
  "    {",
  "      \"name\": \"Seb Tricaud\",",
  "      \"role\": \"maintainer\",",
  "      \"active\": true",
  "    }",
  "  ],",
  "  \"dependencies\": [],",
  "  \"tags\": [\"tui\", \"caca\", \"widget\", \"terminal\"],",
  "  \"config\": {",
  "    \"max_width\": 256,",
  "    \"max_height\": 64,",
  "    \"escape_timeout_ms\": 100,",
  "    \"mouse_enabled\": true,",
  "    \"theme\": null",
  "  }",
  "}",
  NULL
};

static void load_file(gtcaca_textview_widget_t *tv, const char *path)
{
  FILE *f = fopen(path, "r");
  char line[1024];

  if (!f) {
    gtcaca_textview_append(tv, "{\"error\": \"cannot open file\"}");
    return;
  }
  while (fgets(line, sizeof(line), f)) {
    /* strip trailing newline */
    int len = (int)strlen(line);
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
      line[--len] = '\0';
    gtcaca_textview_append(tv, line);
  }
  fclose(f);
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t      *win;
  gtcaca_textview_widget_t    *tv;
  gtcaca_statusbar_widget_t   *sb;
  int cw, ch;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("JSON Viewer");

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);

  win = gtcaca_window_new(NULL, "JSON Viewer", 0, 0, cw, ch - 1);
  tv  = gtcaca_textview_new(GTCACA_WIDGET(win), 1, 1, cw - 2, ch - 3);

  /* Enable JSON syntax highlighting */
  gtcaca_textview_set_mode(tv, GTCACA_TEXTVIEW_MODE_JSON);

  if (argc > 1) {
    load_file(tv, argv[1]);
  } else {
    const char **p;
    for (p = sample_json; *p; p++)
      gtcaca_textview_append(tv, *p);
  }

  sb = gtcaca_statusbar_new("Up/Down: scroll | Home/End: top/bottom | Q: quit");

  gtcaca_window_set_focus(win);
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(tv));

  gtcaca_main();
  return 0;
}
