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
  gtcaca_theme_color_t text;
  gtcaca_theme_color_t button;
  gtcaca_theme_color_t label;
};
typedef struct _gtcaca_theme_t gtcaca_theme_t;

int gtcaca_theme_parse_ini(char *theme);

#endif // _GTCACA_THEME_H_
