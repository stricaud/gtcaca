#ifndef _GTCACA_THEME_H_
#define _GTCACA_THEME_H_

struct _gtcaca_theme_color_t {
  int fg;
  int bg;
};
typedef struct _gtcaca_theme_color_t gtcaca_theme_color_t;

struct _gtcaca_theme_t {
  gtcaca_theme_color_t defaults;
  gtcaca_theme_color_t window;
  gtcaca_theme_color_t text;
};
typedef struct _gtcaca_theme_t gtcaca_theme_t;

gtcaca_theme_t theme;

#endif // _GTCACA_THEME_H_
