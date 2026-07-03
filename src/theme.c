#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <gtcaca/win_compat.h>   /* asprintf */
#endif

#include <gtcaca/main.h>
#include <gtcaca/iniparse.h>
#include <gtcaca/theme.h>

enum caca_color gtcaca_theme_string_to_caca_color(char *str)
{
  if (!strcmp(str, "black")) {
    return CACA_BLACK;
  }
  if (!strcmp(str, "gray") || !strcmp(str, "grey")) {   /* common alias */
    return CACA_LIGHTGRAY;
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
  /* A consistent blue-desktop / red-button scheme, in the spirit of Debian's
     `dialog`/whiptail and classic ncurses TUIs: surfaces are blue, the active
     control is red, selections are cyan, text inputs are white. Every pair has
     clear fg/bg contrast so no widget is ever invisible. */
  gmo.theme._default.bg      = CACA_BLUE;       gmo.theme._default.fg      = CACA_WHITE;

  gmo.theme.window.bg        = CACA_BLUE;       gmo.theme.window.fg        = CACA_WHITE;
  gmo.theme.windowfocus.bg   = CACA_BLUE;       gmo.theme.windowfocus.fg   = CACA_YELLOW;

  /* text / label / list / expander */
  gmo.theme.text.bg          = CACA_BLUE;       gmo.theme.text.fg          = CACA_WHITE;
  gmo.theme.textfocus.bg     = CACA_CYAN;       gmo.theme.textfocus.fg     = CACA_BLACK;

  /* buttons: inactive blue, ACTIVE = red (the look) */
  gmo.theme.button.bg        = CACA_BLUE;       gmo.theme.button.fg        = CACA_WHITE;
  gmo.theme.buttonfocus.bg   = CACA_RED;        gmo.theme.buttonfocus.fg   = CACA_WHITE;

  /* text inputs: a dark field, white when focused */
  gmo.theme.entry.bg         = CACA_BLACK;      gmo.theme.entry.fg         = CACA_LIGHTGRAY;
  gmo.theme.entryfocus.bg    = CACA_WHITE;      gmo.theme.entryfocus.fg    = CACA_BLACK;

  /* checkbox / radio / switch */
  gmo.theme.checkbox.bg      = CACA_BLUE;       gmo.theme.checkbox.fg      = CACA_WHITE;
  gmo.theme.checkboxfocus.bg = CACA_CYAN;       gmo.theme.checkboxfocus.fg = CACA_BLACK;

  gmo.theme.combobox.bg      = CACA_BLUE;       gmo.theme.combobox.fg      = CACA_WHITE;
  gmo.theme.comboboxfocus.bg = CACA_CYAN;       gmo.theme.comboboxfocus.fg = CACA_BLACK;

  gmo.theme.progressbar.bg   = CACA_BLUE;       gmo.theme.progressbar.fg   = CACA_LIGHTGREEN;

  gmo.theme.statusbar.bg     = CACA_CYAN;       gmo.theme.statusbar.fg     = CACA_BLACK;

  /* the multi-line text/editor surface stays dark (apps like cacamacs want it) */
  gmo.theme.textview.bg      = CACA_BLACK;      gmo.theme.textview.fg      = CACA_LIGHTGRAY;
  gmo.theme.textviewfocus.bg = CACA_BLACK;      gmo.theme.textviewfocus.fg = CACA_WHITE;

  gmo.theme.menu.bg          = CACA_LIGHTGRAY;  gmo.theme.menu.fg          = CACA_BLACK;
  gmo.theme.menuitem.bg      = CACA_LIGHTGRAY;  gmo.theme.menuitem.fg      = CACA_BLACK;
  gmo.theme.menuitemfocus.bg = CACA_RED;        gmo.theme.menuitemfocus.fg = CACA_WHITE;
}

int gtcaca_theme_parse_ini(char *theme)
{
  char *theme_fullpath;
  ini_t *ini;
  int count;
  
  if (!theme) { theme = "default"; }

  /* Seed every colour with the built-in defaults FIRST, so a theme file that
     only sets some keys (e.g. one with no statusbar lines) doesn't leave the
     rest at 0 = CACA_BLACK — which silently made the status bar black-on-black
     (invisible) whenever a partial theme file was installed. The file below
     then overrides only the keys it specifies. */
  gtcaca_theme_default();

  asprintf(&theme_fullpath, "%s/themes/%s", GTCACA_DATA_DIR, theme);

  ini = ini_parse_file(theme_fullpath);
  if (!ini) {
    fprintf(stderr, "No theme file found at %s, using built-in defaults.\n", theme_fullpath);
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
