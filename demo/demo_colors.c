/*
 * demo_colors — the palette you can use in ~/.ccm/theme (and the gtcaca theme).
 * A libcaca terminal is 16-colour, so these 16 are everything available; each
 * row shows the colour as a background swatch and as foreground text, with all
 * the names the theme parser accepts. Press any key to quit.
 *
 * Run it with:  ./demo.sh colors
 */
#include <stdio.h>
#include <caca.h>

static const struct { int caca; int light; const char *names; } PAL[] = {
  { CACA_BLACK,        0, "black" },
  { CACA_RED,          0, "red" },
  { CACA_GREEN,        0, "green" },
  { CACA_BROWN,        0, "brown, orange" },
  { CACA_BLUE,         0, "blue" },
  { CACA_MAGENTA,      0, "magenta, purple, violet" },
  { CACA_CYAN,         0, "cyan" },
  { CACA_LIGHTGRAY,    1, "gray, grey, lightgray, lightgrey" },
  { CACA_DARKGRAY,     0, "darkgray, darkgrey, brightblack" },
  { CACA_LIGHTRED,     0, "lightred, brightred" },
  { CACA_LIGHTGREEN,   1, "lightgreen, brightgreen" },
  { CACA_YELLOW,       1, "yellow, brightyellow" },
  { CACA_LIGHTBLUE,    0, "lightblue, brightblue" },
  { CACA_LIGHTMAGENTA, 1, "lightmagenta, brightmagenta, lightpurple" },
  { CACA_LIGHTCYAN,    1, "lightcyan, brightcyan" },
  { CACA_WHITE,        1, "white" },
};
#define NP ((int)(sizeof PAL / sizeof PAL[0]))

int main(void)
{
  caca_display_t *dp = caca_create_display(NULL);
  caca_canvas_t *cv;
  caca_event_t ev;
  int i, fy;

  if (!dp) { fprintf(stderr, "cannot create display\n"); return 1; }
  cv = caca_get_canvas(dp);
  caca_set_display_title(dp, "gtcaca colours");

  caca_set_color_ansi(cv, CACA_WHITE, CACA_BLACK); caca_clear_canvas(cv);
  caca_set_color_ansi(cv, CACA_BLACK, CACA_CYAN);
  caca_printf(cv, 0, 0,
    " ~/.ccm/theme colours  -  the 16 terminal colours          press any key to quit ");

  for (i = 0; i < NP; i++) {
    int y = 2 + i;
    /* swatch: the colour as a background block, with a readable label on it */
    caca_set_color_ansi(cv, PAL[i].light ? CACA_BLACK : CACA_WHITE, PAL[i].caca);
    caca_printf(cv, 2, y, "  Aa Bb 123  ");
    /* the same colour as foreground text (how a syntax colour looks on dark bg) */
    caca_set_color_ansi(cv, PAL[i].caca, CACA_BLACK);
    caca_printf(cv, 17, y, "%s", PAL[i].names);
  }

  fy = 2 + NP + 1;
  caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
  caca_printf(cv, 2, fy,     "In ~/.ccm/theme, e.g.:   background = #301934    keyword = #c080ff");
  caca_printf(cv, 2, fy + 1, "These names work everywhere; #rrggbb / #rgb / rgb(r,g,b) now render in");
  caca_printf(cv, 2, fy + 2, "up to 4096 colours (a real dark purple!).  See the range:  ./demo.sh truecolor");

  caca_refresh_display(dp);
  caca_get_event(dp, CACA_EVENT_KEY_PRESS, &ev, -1);
  caca_free_display(dp);
  return 0;
}
