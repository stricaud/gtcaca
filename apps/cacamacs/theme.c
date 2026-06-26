#include "cacamacs.h"
#include <strings.h>   /* strcasecmp */
/* ── per-user cacamacs colour theme (~/.ccm/theme) ──────────────────────────────
 *
 * A small INI-ish file:  key = colour   (# starts a comment)
 *
 * Colours may be names (the 16 terminal colours), or #rrggbb / #rgb hex, or
 * rgb(r,g,b). On a normal terminal gtcaca's truecolour presenter renders them in
 * up to 4096 colours, so e.g. "background = #301934" is a real dark purple; on a
 * plain 16-colour libcaca terminal each colour falls back to the nearest of 16.
 *
 * Keys: background/bg, foreground/fg, comment, string, number, keyword,
 *       bracket, operator, selection_fg, selection_bg, current_line.
 */

typedef struct { int set; uint8_t a; uint16_t c12; } col_t;   /* ANSI fallback + 12-bit */

static struct {
  int   loaded;
  col_t bg, fg, comment, string, number, keyword, bracket, op, sel_fg, sel_bg, curline;
} T;

/* the 16 ANSI colours as 12-bit (0x0RGB) */
static const uint16_t ANSI12[16] = {
  0x000,0x00a,0x0a0,0x0aa,0xa00,0xa0a,0xa50,0xaaa,
  0x555,0x55f,0x5f5,0x5ff,0xf55,0xf5f,0xff5,0xfff
};

/* nearest of the 16 terminal colours for an r,g,b triple */
static uint8_t nearest_ansi(int r, int g, int b)
{
  static const struct { uint8_t c; int r, g, b; } P[16] = {
    {CACA_BLACK,0,0,0},       {CACA_BLUE,0,0,170},      {CACA_GREEN,0,170,0},      {CACA_CYAN,0,170,170},
    {CACA_RED,170,0,0},       {CACA_MAGENTA,170,0,170}, {CACA_BROWN,170,85,0},     {CACA_LIGHTGRAY,170,170,170},
    {CACA_DARKGRAY,85,85,85}, {CACA_LIGHTBLUE,85,85,255},{CACA_LIGHTGREEN,85,255,85},{CACA_LIGHTCYAN,85,255,255},
    {CACA_LIGHTRED,255,85,85},{CACA_LIGHTMAGENTA,255,85,255},{CACA_YELLOW,255,255,85},{CACA_WHITE,255,255,255}
  };
  int i, best = 0; long bd = -1;
  for (i = 0; i < 16; i++) {
    long d = (long)(r-P[i].r)*(r-P[i].r) + (long)(g-P[i].g)*(g-P[i].g) + (long)(b-P[i].b)*(b-P[i].b);
    if (bd < 0 || d < bd) { bd = d; best = i; }
  }
  return P[best].c;
}

static uint16_t rgb_to_12(int r, int g, int b) { return (uint16_t)(((r*15/255)<<8) | ((g*15/255)<<4) | (b*15/255)); }

/* a colour name, #rgb/#rrggbb hex, or rgb(r,g,b). Fills both the ANSI fallback
   and the 12-bit (4096) value. */
static int parse_color(const char *s, col_t *out)
{
  static const struct { const char *name; uint8_t c; } N[] = {
    {"black",CACA_BLACK},{"red",CACA_RED},{"green",CACA_GREEN},{"yellow",CACA_YELLOW},
    {"brown",CACA_BROWN},{"orange",CACA_BROWN},{"blue",CACA_BLUE},{"magenta",CACA_MAGENTA},
    {"purple",CACA_MAGENTA},{"violet",CACA_MAGENTA},{"cyan",CACA_CYAN},{"white",CACA_WHITE},
    {"gray",CACA_LIGHTGRAY},{"grey",CACA_LIGHTGRAY},{"lightgray",CACA_LIGHTGRAY},{"lightgrey",CACA_LIGHTGRAY},
    {"darkgray",CACA_DARKGRAY},{"darkgrey",CACA_DARKGRAY},{"brightblack",CACA_DARKGRAY},
    {"lightred",CACA_LIGHTRED},{"brightred",CACA_LIGHTRED},{"lightgreen",CACA_LIGHTGREEN},{"brightgreen",CACA_LIGHTGREEN},
    {"lightblue",CACA_LIGHTBLUE},{"brightblue",CACA_LIGHTBLUE},{"lightcyan",CACA_LIGHTCYAN},{"brightcyan",CACA_LIGHTCYAN},
    {"lightmagenta",CACA_LIGHTMAGENTA},{"brightmagenta",CACA_LIGHTMAGENTA},{"lightpurple",CACA_LIGHTMAGENTA},
    {"brightyellow",CACA_YELLOW},{NULL,0}
  };
  int i;
  unsigned int r, g, b;
  if (!s || !*s) return 0;

  if (*s == '#') {                                   /* HTML hex */
    const char *h = s + 1; size_t l = strlen(h);
    if      (l == 3 && sscanf(h, "%1x%1x%1x", &r, &g, &b) == 3) { r *= 17; g *= 17; b *= 17; }
    else if (l == 6 && sscanf(h, "%2x%2x%2x", &r, &g, &b) == 3) { /* as-is */ }
    else return 0;
    out->a = nearest_ansi((int)r, (int)g, (int)b); out->c12 = rgb_to_12((int)r, (int)g, (int)b); out->set = 1;
    return 1;
  }
  if (!strncasecmp(s, "rgb(", 4)) {                  /* rgb(r, g, b) */
    int rr, gg, bb;
    if (sscanf(s + 4, "%d , %d , %d", &rr, &gg, &bb) != 3) return 0;
    if (rr<0) rr=0; if (rr>255) rr=255; if (gg<0) gg=0; if (gg>255) gg=255; if (bb<0) bb=0; if (bb>255) bb=255;
    out->a = nearest_ansi(rr, gg, bb); out->c12 = rgb_to_12(rr, gg, bb); out->set = 1;
    return 1;
  }
  for (i = 0; N[i].name; i++)
    if (!strcasecmp(s, N[i].name)) { out->a = N[i].c; out->c12 = ANSI12[N[i].c & 15]; out->set = 1; return 1; }
  return 0;
}

static char *trim(char *s)
{
  char *e;
  while (*s && isspace((unsigned char)*s)) s++;
  e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) *--e = '\0';
  return s;
}

void ccm_theme_load(void)
{
  const char *home = getenv("HOME");
  char path[PATH_MAX], line[256], badbuf[120] = "";
  int  nbad = 0, lineno = 0;
  FILE *f;

  memset(&T, 0, sizeof T);
  if (!home) return;
  snprintf(path, sizeof path, "%s/.ccm/theme", home);
  f = fopen(path, "r");
  if (!f) return;

  while (fgets(line, sizeof line, f)) {
    char *p = line, *eq, *k, *v, *c;
    col_t col = {0,0,0};
    int known = 1;
    lineno++;
    eq = strchr(p, '=');
    if (!eq) continue;
    *eq = '\0'; k = trim(p); v = trim(eq + 1);   /* trim first: a leading '#' is a hex value, not a comment */
    for (c = v; *c; c++)
      if (*c == '#' && c > v && isspace((unsigned char)c[-1])) { *c = '\0'; break; }
    v = trim(v);
    if (!*k || !*v) continue;
    if (!parse_color(v, &col)) {                 /* e.g. a typo like #00gg00 (g isn't hex) */
      if (!nbad++) snprintf(badbuf, sizeof badbuf, "line %d: invalid colour \"%s\" for \"%s\"", lineno, v, k);
      continue;
    }

    if      (!strcasecmp(k, "background") || !strcasecmp(k, "bg"))        T.bg = col;
    else if (!strcasecmp(k, "foreground") || !strcasecmp(k, "fg") || !strcasecmp(k, "default")) T.fg = col;
    else if (!strcasecmp(k, "comment"))                                  T.comment = col;
    else if (!strcasecmp(k, "string"))                                   T.string = col;
    else if (!strcasecmp(k, "number") || !strcasecmp(k, "constant"))     T.number = col;
    else if (!strcasecmp(k, "keyword"))                                  T.keyword = col;
    else if (!strcasecmp(k, "bracket"))                                  T.bracket = col;
    else if (!strcasecmp(k, "operator"))                                 T.op = col;
    else if (!strcasecmp(k, "selection_fg") || !strcasecmp(k, "selectionfg")) T.sel_fg = col;
    else if (!strcasecmp(k, "selection_bg") || !strcasecmp(k, "selectionbg") || !strcasecmp(k, "selection")) T.sel_bg = col;
    else if (!strcasecmp(k, "current_line") || !strcasecmp(k, "currentline") || !strcasecmp(k, "cursorline")) T.curline = col;
    else known = 0;
    if (!known && !nbad++) snprintf(badbuf, sizeof badbuf, "line %d: unknown setting \"%s\"", lineno, k);
  }
  fclose(f);
  T.loaded = 1;

  if (nbad) {   /* surface mistakes: on the status line at startup and on stderr */
    snprintf(g_message, sizeof g_message, "~/.ccm/theme: %s%s", badbuf, nbad > 1 ? "  (+ more)" : "");
    fprintf(stderr, "ccm: ~/.ccm/theme: %s%s\n", badbuf, nbad > 1 ? " (and more)" : "");
  }
}

/* run before any editors are created: tints the shared editor surface (ANSI) */
void ccm_theme_apply_global(void)
{
  if (!T.loaded) return;
  if (T.bg.set) { gmo.theme.textview.bg = T.bg.a; gmo.theme.textviewfocus.bg = T.bg.a; }
  if (T.fg.set) { gmo.theme.textview.fg = T.fg.a; gmo.theme.textviewfocus.fg = T.fg.a; }
}

/* run per editor: ANSI fallback + the 12-bit (4096-colour) surface and palette */
void ccm_theme_apply_editor(gtcaca_editor_widget_t *ed)
{
  if (!T.loaded || !ed) return;
  if (T.bg.set) {
    ed->color_focus_bg = ed->color_nonfocus_bg = T.bg.a;
    gtcaca_editor_set_bg_rgb12(ed, T.bg.c12);
  }
  if (T.fg.set) {
    ed->color_focus_fg = ed->color_nonfocus_fg = T.fg.a;
    gtcaca_editor_set_fg_rgb12(ed, T.fg.c12);
    gtcaca_editor_style_set_fore(ed, GTCACA_EDITOR_STYLE_DEFAULT, T.fg.a);
    gtcaca_editor_style_set_fore_rgb12(ed, GTCACA_EDITOR_STYLE_DEFAULT, T.fg.c12);
  }
  #define STY(field, S) do { if ((field).set) { \
      gtcaca_editor_style_set_fore(ed, S, (field).a); \
      gtcaca_editor_style_set_fore_rgb12(ed, S, (field).c12); } } while (0)
  STY(T.comment,  GTCACA_EDITOR_STYLE_COMMENT);
  STY(T.string,   GTCACA_EDITOR_STYLE_STRING);
  STY(T.number,   GTCACA_EDITOR_STYLE_NUMBER);
  STY(T.keyword,  GTCACA_EDITOR_STYLE_KEYWORD);
  STY(T.bracket,  GTCACA_EDITOR_STYLE_BRACKET);
  STY(T.op,       GTCACA_EDITOR_STYLE_OPERATOR);
  #undef STY
  if (T.sel_fg.set || T.sel_bg.set)
    gtcaca_editor_set_selection_colors(ed, T.sel_fg.set ? T.sel_fg.a : CACA_WHITE,
                                           T.sel_bg.set ? T.sel_bg.a : CACA_BLUE);
  if (T.curline.set) gtcaca_editor_set_caret_line_back(ed, T.curline.a);
}
