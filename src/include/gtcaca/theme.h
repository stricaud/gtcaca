#ifndef _GTCACA_THEME_H_
#define _GTCACA_THEME_H_

#include <caca.h>

struct _gtcaca_theme_color_t {
  enum caca_color fg;
  enum caca_color bg;
};
typedef struct _gtcaca_theme_color_t gtcaca_theme_color_t;

struct _gtcaca_theme_t {
  gtcaca_theme_color_t _default;
  gtcaca_theme_color_t window;
  gtcaca_theme_color_t windowfocus;
  gtcaca_theme_color_t text;
  gtcaca_theme_color_t textfocus;
  gtcaca_theme_color_t button;
  gtcaca_theme_color_t buttonfocus;
  gtcaca_theme_color_t entry;
  gtcaca_theme_color_t entryfocus;
  gtcaca_theme_color_t checkbox;
  gtcaca_theme_color_t checkboxfocus;
  gtcaca_theme_color_t progressbar;
  gtcaca_theme_color_t statusbar;
  gtcaca_theme_color_t textview;
  gtcaca_theme_color_t textviewfocus;
  gtcaca_theme_color_t combobox;
  gtcaca_theme_color_t comboboxfocus;
  gtcaca_theme_color_t menu;
  gtcaca_theme_color_t menuitem;
  gtcaca_theme_color_t menuitemfocus;
};
typedef struct _gtcaca_theme_t gtcaca_theme_t;

/* Defined since theme lookup became relocatable, so an application can compile
   against either gtcaca:

       #ifdef GTCACA_HAVE_THEME_SEARCH
         gtcaca_theme_add_search_dir(my_app_config_dir);
       #endif                                                              */
#define GTCACA_HAVE_THEME_SEARCH 1

/* Load the named theme (NULL means "default") into gmo.theme, starting from the
   built-in palette so a partial theme file only overrides what it mentions.
   Always succeeds: with no theme file anywhere, the built-in palette stands.

   `theme` is searched for as <dir>/themes/<theme> across, in order:

     - directories registered with gtcaca_theme_add_search_dir(), newest first
     - $GTCACA_THEME_DIR
     - the user's config directory — %APPDATA%\gtcaca on Windows, else
       $XDG_CONFIG_HOME/gtcaca or ~/.config/gtcaca
     - <directory of the running executable>/../share/gtcaca, so a relocatable
       bundle finds the theme it shipped with
     - GTCACA_DATA_DIR, the prefix this library was built for

   $GTCACA_THEME overrides all of it with a path to one specific file. */
int gtcaca_theme_parse_ini(char *theme);

/* Prepend a directory to the search list; it is copied. Call before
   gtcaca_init(). At most GTCACA_THEME_MAX_SEARCH_DIRS are kept. */
#define GTCACA_THEME_MAX_SEARCH_DIRS 8
void gtcaca_theme_add_search_dir(const char *dir);

/* Report the search to stderr when no theme file turns up. Off by default:
   falling back to the built-in palette is ordinary, not a fault, and a library
   that complains about it on every start-up is a library that has to be
   silenced by every application that embeds it. */
void gtcaca_theme_set_verbose(int on);

/* The theme file actually loaded, or NULL when the built-in palette is in use.
   Valid until the next gtcaca_theme_parse_ini(). */
const char *gtcaca_theme_loaded_path(void);

#endif /* _GTCACA_THEME_H_ */
