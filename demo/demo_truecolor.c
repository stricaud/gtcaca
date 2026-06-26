/*
 * demo_truecolor — proof that we can bypass libcaca's 16-colour ceiling.
 *
 * libcaca only ever emits the 16 ANSI colours to a terminal, but the terminal
 * itself can do 256 colours (e.g. macOS Terminal.app) or 24-bit truecolour
 * (iTerm2, kitty, alacritty, most Linux terminals). This program writes its own
 * escape sequences directly — exactly what a gtcaca renderer would do instead of
 * caca_refresh_display — so you can see the real range your terminal supports.
 *
 * Run it with:  ./demo.sh truecolor
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_true;   /* terminal claims 24-bit colour */

static int has_truecolor(void)
{
  const char *c = getenv("COLORTERM");
  return c && (strstr(c, "truecolor") || strstr(c, "24bit"));
}

/* 24-bit if available, else nearest of the xterm 256 6x6x6 cube */
static void bg(int r, int g, int b)
{
  if (g_true) printf("\033[48;2;%d;%d;%dm", r, g, b);
  else        printf("\033[48;5;%dm", 16 + 36*(r*5/255) + 6*(g*5/255) + (b*5/255));
}
static void fg(int r, int g, int b)
{
  if (g_true) printf("\033[38;2;%d;%d;%dm", r, g, b);
  else        printf("\033[38;5;%dm", 16 + 36*(r*5/255) + 6*(g*5/255) + (b*5/255));
}
static void reset(void) { printf("\033[0m"); }

int main(void)
{
  int i;
  static const struct { const char *name; int r, g, b; } SW[] = {
    { "dark purple", 0x30, 0x19, 0x34 }, { "indigo",     0x4b, 0x00, 0x82 },
    { "royal blue",  0x41, 0x69, 0xe1 }, { "teal",       0x00, 0x80, 0x80 },
    { "forest",      0x22, 0x8b, 0x22 }, { "olive",      0x80, 0x80, 0x00 },
    { "salmon",      0xfa, 0x80, 0x72 }, { "slate blue", 0x6a, 0x5a, 0xcd },
  };

  g_true = has_truecolor();
  printf("\033[2J\033[H");
  printf("COLORTERM=%s   ->   %s\n\n",
         getenv("COLORTERM") ? getenv("COLORTERM") : "(unset)",
         g_true ? "24-bit truecolour" : "256-colour (truecolour fallback)");

  printf("What libcaca limits you to today — 16 colours:\n  ");
  for (i = 0; i < 16; i++) { printf("\033[48;5;%dm   ", i); reset(); }
  printf("\n\n");

  printf("Bypassing libcaca, the terminal can do a smooth ramp:\n  ");
  for (i = 0; i < 64; i++) { int v = i * 255 / 63; bg(v/4, 0, v); printf(" "); }   /* black -> purple */
  reset(); printf("\n  ");
  for (i = 0; i < 64; i++) { int v = i * 255 / 63; bg(v, v, v); printf(" "); }      /* greyscale */
  reset(); printf("\n\n");

  printf("...and named colours the 16 can't reach:\n");
  for (i = 0; i < (int)(sizeof SW / sizeof SW[0]); i++) {
    bg(SW[i].r, SW[i].g, SW[i].b); printf("        "); reset();
    fg(SW[i].r, SW[i].g, SW[i].b);
    printf("  %-12s #%02x%02x%02x", SW[i].name, SW[i].r, SW[i].g, SW[i].b);
    reset(); printf("\n");
  }

  printf("\nIf those look like distinct colours (not banded to ~16), gtcaca can use\n");
  printf("them by rendering cells this way instead of caca_refresh_display.\n");
  printf("\nPress Enter to quit.\n");
  getchar();
  reset();
  return 0;
}
