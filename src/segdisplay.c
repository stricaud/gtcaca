#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <caca.h>

#include <gtcaca/segdisplay.h>
#include <gtcaca/main.h>

/* segment bits:    aaa
 *                 f   b
 *                 f   b
 *                  ggg
 *                 e   c
 *                 e   c
 *                  ddd                      */
enum { SA = 1, SB = 2, SC = 4, SD = 8, SE = 16, SF = 32, SG = 64 };

/* map a character to its lit segments */
static int seg_mask(int ch)
{
  switch (toupper(ch)) {
  case '0': return SA|SB|SC|SD|SE|SF;
  case '1': return SB|SC;
  case '2': return SA|SB|SG|SE|SD;
  case '3': return SA|SB|SG|SC|SD;
  case '4': return SF|SG|SB|SC;
  case '5': return SA|SF|SG|SC|SD;
  case '6': return SA|SF|SG|SE|SC|SD;
  case '7': return SA|SB|SC;
  case '8': return SA|SB|SC|SD|SE|SF|SG;
  case '9': return SA|SB|SC|SD|SF|SG;
  case 'A': return SA|SB|SC|SE|SF|SG;
  case 'B': return SC|SD|SE|SF|SG;
  case 'C': return SA|SD|SE|SF;
  case 'D': return SB|SC|SD|SE|SG;
  case 'E': return SA|SD|SE|SF|SG;
  case 'F': return SA|SE|SF|SG;
  case 'G': return SA|SC|SD|SE|SF;
  case 'H': return SB|SC|SE|SF|SG;
  case 'I': return SE|SF;
  case 'J': return SB|SC|SD|SE;
  case 'L': return SD|SE|SF;
  case 'O': return SA|SB|SC|SD|SE|SF;
  case 'P': return SA|SB|SE|SF|SG;
  case 'S': return SA|SF|SG|SC|SD;
  case 'U': return SB|SC|SD|SE|SF;
  case '-': return SG;
  case '_': return SD;
  case '=': return SD|SG;
  default:  return 0;   /* space and unknowns: blank */
  }
}

gtcaca_segdisplay_widget_t *gtcaca_segdisplay_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_segdisplay_widget_t *s = malloc(sizeof(*s));
  if (!s) { fprintf(stderr, "Error: Cannot allocate segdisplay\n"); return NULL; }
  s->id = gtcaca_get_newid();
  s->has_focus = 0; s->is_visible = 1; s->type = GTCACA_WIDGET_SEGDISPLAY;
  s->parent = parent; s->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(s), x, y);
  s->width = width; s->height = height;
  s->color_focus_fg = s->color_nonfocus_fg = gmo.theme.textview.fg;
  s->color_focus_bg = s->color_nonfocus_bg = gmo.theme.textview.bg;
  s->text = strdup(""); s->colour = CACA_LIGHTGREEN; s->dim = CACA_BLACK;
  s->title = NULL; s->box = 1;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(s));
  return s;
}

void gtcaca_segdisplay_set_text(gtcaca_segdisplay_widget_t *s, const char *text)
{ free(s->text); s->text = strdup(text ? text : ""); }
void gtcaca_segdisplay_set_colour(gtcaca_segdisplay_widget_t *s, uint8_t colour) { s->colour = colour; }
void gtcaca_segdisplay_set_title(gtcaca_segdisplay_widget_t *s, const char *t)
{ free(s->title); s->title = t ? strdup(t) : NULL; }
void gtcaca_segdisplay_set_box(gtcaca_segdisplay_widget_t *s, int on) { s->box = on; }

static void hseg(int x, int y, int len)
{ int i; for (i = 0; i < len; i++) caca_put_char(gmo.cv, x + i, y, 0x2588); }
static void vseg(int x, int y0, int y1)
{ int y; for (y = y0; y <= y1; y++) caca_put_char(gmo.cv, x, y, 0x2588); }

/* draw one glyph in a dw x dh cell whose top-left is (ox,oy) */
static void draw_glyph(int mask, int ox, int oy, int dw, int dh)
{
  int mid = oy + dh / 2, right = ox + dw - 1, bot = oy + dh - 1, hl = dw - 2;
  if (hl < 1) hl = 1;
  if (mask & SA) hseg(ox + 1, oy,  hl);
  if (mask & SG) hseg(ox + 1, mid, hl);
  if (mask & SD) hseg(ox + 1, bot, hl);
  if (mask & SF) vseg(ox,    oy + 1, mid - 1);
  if (mask & SB) vseg(right, oy + 1, mid - 1);
  if (mask & SE) vseg(ox,    mid + 1, bot - 1);
  if (mask & SC) vseg(right, mid + 1, bot - 1);
}

void gtcaca_segdisplay_draw(gtcaca_segdisplay_widget_t *s)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = s->x + (s->box ? 1 : 0), inner_y = s->y + (s->box ? 1 : 0);
  int inner_w = s->width - (s->box ? 2 : 0), inner_h = s->height - (s->box ? 2 : 0);
  int dh, dw, gap, n, total_w, ox, i, cx;

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, s->x, s->y, s->width, s->height, ' ');
  if (s->box) {
    caca_draw_cp437_box(gmo.cv, s->x, s->y, s->width, s->height);
    if (s->title) caca_printf(gmo.cv, s->x + 2, s->y, "| %s |", s->title);
  }
  if (inner_w <= 0 || inner_h <= 0 || !s->text[0]) return;

  n = (int)strlen(s->text);
  dh = inner_h; if (dh > 13) dh = 13;          /* keep proportions sane */
  dw = (dh * 3) / 4 + 2; if (dw < 4) dw = 4;
  gap = 1;
  /* shrink the digit width if the string would not fit */
  while (n * (dw + gap) - gap > inner_w && dw > 3) dw--;
  total_w = n * (dw + gap) - gap;
  ox = inner_x + (inner_w - total_w) / 2; if (ox < inner_x) ox = inner_x;
  cx = ox + (inner_h > dh ? 0 : 0);

  caca_set_color_ansi(gmo.cv, s->colour, bg); caca_set_attr(gmo.cv, 0);
  for (i = 0; i < n; i++) {
    int ch = (unsigned char)s->text[i];
    int gx = ox + i * (dw + gap);
    int oy = inner_y + (inner_h - dh) / 2;
    if (gx + dw > inner_x + inner_w + 1) break;        /* clip */
    if (ch == ':') {                                   /* clock colon: two dots */
      int q = oy + dh / 3, h = oy + (2 * dh) / 3;
      caca_put_char(gmo.cv, gx + dw / 2, q, 0x2588);
      caca_put_char(gmo.cv, gx + dw / 2, h, 0x2588);
      continue;
    }
    draw_glyph(seg_mask(ch), gx, oy, dw, dh);
  }
  (void)cx;
}

void gtcaca_segdisplay_free(gtcaca_segdisplay_widget_t *s)
{
  if (!s) return;
  free(s->text); free(s->title); free(s);
}
