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

int gtcaca_theme_parse_ini(char *theme);

#endif /* _GTCACA_THEME_H_ */
