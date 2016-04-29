#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtcaca/main.h>
#include <gtcaca/iniparse.h>
#include <gtcaca/theme.h>

enum caca_color gtcaca_theme_string_to_caca_color(char *str)
{
  if (!strcmp(str, "black")) {
    return CACA_BLACK;
  }
  if (!strcmp(str, "blue")) {
    return CACA_BLUE;
  }
  if (!strcmp(str, "green")) {
    return CACA_GREEN;
  }
  if (!strcmp(str, "cyan")) {
    return CACA_CYAN;
  }
  if (!strcmp(str, "red")) {
    return CACA_RED;
  }
  if (!strcmp(str, "magenta")) {
    return CACA_MAGENTA;
  }
  if (!strcmp(str, "brown")) {
    return CACA_BROWN;
  }
  if (!strcmp(str, "lightgray")) {
    return CACA_LIGHTGRAY;
  }
  if (!strcmp(str, "darkgray")) {
    return CACA_DARKGRAY;
  }
  if (!strcmp(str, "lightblue")) {
    return CACA_LIGHTBLUE;
  }
  if (!strcmp(str, "lightgreen")) {
    return CACA_LIGHTGREEN;
  }
  if (!strcmp(str, "lightcyan")) {
    return CACA_LIGHTCYAN;
  }
  if (!strcmp(str, "lightred")) {
    return CACA_LIGHTRED;
  }
  if (!strcmp(str, "lightmagenta")) {
    return CACA_LIGHTMAGENTA;
  }
  if (!strcmp(str, "yellow")) {
    return CACA_YELLOW;
  }
  if (!strcmp(str, "white")) {
    return CACA_WHITE;
  }
  if (!strcmp(str, "default")) {
    return CACA_DEFAULT;
  }
  if (!strcmp(str, "transparent")) {
    return CACA_TRANSPARENT;
  }

  return CACA_DEFAULT;
}

int gtcaca_theme_parse_ini(char *theme)
{
  char *theme_fullpath;
  ini_t *ini;
  int count;
  
  if (!theme) { theme = "default"; }

  asprintf(&theme_fullpath, "%s/themes/%s", GTCACA_DATA_DIR, theme);

  ini = ini_parse_file(theme_fullpath);
  if (!ini) {
    fprintf(stderr, "Error, cannot open theme file %s\n", theme_fullpath);
    free(theme_fullpath);
    return -1;
  }

  for (count = 0; count < ini->n_items; count += 2) {
    char *k = ini->keyvals[count];
    char *v = ini->keyvals[count+1];   

    if (!strcmp(k, "gtcaca_theme.default_bg")) {
      gmo.theme._default.bg = gtcaca_theme_string_to_caca_color(v);
    }
    if (!strcmp(k, "gtcaca_theme.default_fg")) {
      gmo.theme._default.fg = gtcaca_theme_string_to_caca_color(v);
    }
    if (!strcmp(k, "gtcaca_theme.window_bg")) {
      gmo.theme.window.bg = gtcaca_theme_string_to_caca_color(v);
    }
    if (!strcmp(k, "gtcaca_theme.window_fg")) {
      gmo.theme.window.fg = gtcaca_theme_string_to_caca_color(v);
    }
    if (!strcmp(k, "gtcaca_theme.text_bg")) {
      gmo.theme.text.bg = gtcaca_theme_string_to_caca_color(v);
    }
    if (!strcmp(k, "gtcaca_theme.text_fg")) {
      gmo.theme.text.fg = gtcaca_theme_string_to_caca_color(v);
    }

  }
  
  free(theme_fullpath);
  ini_free(ini);
  return 0;
}
