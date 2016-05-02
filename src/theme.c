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

void gtcaca_theme_default(void)
{
  gmo.theme._default.bg = CACA_BLACK;
  gmo.theme._default.fg = CACA_WHITE;

  gmo.theme.windowfocus.bg = CACA_BLUE;
  gmo.theme.windowfocus.fg = CACA_YELLOW;
  gmo.theme.window.bg = CACA_LIGHTBLUE;
  gmo.theme.window.fg = CACA_YELLOW;
  
  gmo.theme.textfocus.bg = CACA_YELLOW;
  gmo.theme.textfocus.fg = CACA_BLUE;
  gmo.theme.text.bg = CACA_BLUE;
  gmo.theme.text.fg = CACA_YELLOW;

  gmo.theme.buttonfocus.bg = CACA_RED;
  gmo.theme.buttonfocus.fg = CACA_YELLOW;
  gmo.theme.button.bg = CACA_LIGHTRED;
  gmo.theme.button.fg = CACA_YELLOW;
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

    /* We are not stoping the machine because we cannot parse theme from a file! */
    gtcaca_theme_default();
  } else {
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
      if (!strcmp(k, "gtcaca_theme.button_bg")) {
	gmo.theme.button.bg = gtcaca_theme_string_to_caca_color(v);
      }
      if (!strcmp(k, "gtcaca_theme.button_fg")) {
	gmo.theme.button.fg = gtcaca_theme_string_to_caca_color(v);
      }
    }
    ini_free(ini);
  }
  
  free(theme_fullpath);
  return 0;
}
