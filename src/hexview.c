#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/hexview.h>
#include <gtcaca/main.h>

gtcaca_hexview_widget_t *gtcaca_hexview_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_hexview_widget_t *h = malloc(sizeof *h);
  if (!h) { fprintf(stderr, "Error: cannot allocate hexview\n"); return NULL; }

  h->id = gtcaca_get_newid();
  h->has_focus = 0;
  h->is_visible = 1;
  h->type = GTCACA_WIDGET_HEXVIEW;
  h->parent = parent;
  h->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(h), x, y);
  h->width = width;
  h->height = height;

  h->color_focus_fg = gmo.theme.text.fg;
  h->color_focus_bg = gmo.theme.text.bg;
  h->color_nonfocus_fg = gmo.theme.text.fg;
  h->color_nonfocus_bg = gmo.theme.text.bg;

  h->data = NULL;
  h->len = 0;
  h->top = 0;
  h->hl_off = -1;
  h->hl_len = 0;
  h->title = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(h));
  return h;
}

void gtcaca_hexview_set_data(gtcaca_hexview_widget_t *h, const uint8_t *data, int len)
{
  if (!h) return;
  h->data = data;
  h->len = len < 0 ? 0 : len;
  if (h->top * 16 >= h->len) h->top = 0;
}

void gtcaca_hexview_set_highlight(gtcaca_hexview_widget_t *h, int off, int len)
{
  if (!h) return;
  h->hl_off = off;
  h->hl_len = len;
  /* scroll the highlight into view */
  if (len > 0 && off >= 0) {
    int rows = (h->height - 2) > 0 ? h->height - 2 : 1;
    int row = off / 16;
    if (row < h->top) h->top = row;
    else if (row >= h->top + rows) h->top = row - rows + 1;
  }
}

void gtcaca_hexview_set_title(gtcaca_hexview_widget_t *h, const char *title)
{
  if (!h) return;
  free(h->title);
  h->title = title ? strdup(title) : NULL;
}

void gtcaca_hexview_draw(gtcaca_hexview_widget_t *h)
{
  int innerh, r, i;
  uint8_t fg, bg;
  if (!h || !h->is_visible) return;

  fg = h->has_focus ? h->color_focus_fg : h->color_nonfocus_fg;
  bg = h->has_focus ? h->color_focus_bg : h->color_nonfocus_bg;

  caca_set_color_ansi(gmo.cv, fg, bg);
  caca_fill_box(gmo.cv, h->x, h->y, h->width, h->height, ' ');
  caca_draw_cp437_box(gmo.cv, h->x, h->y, h->width, h->height);
  if (h->title)
    caca_printf(gmo.cv, h->x + 2, h->y, " %s ", h->title);

  innerh = h->height - 2;
  for (r = 0; r < innerh; r++) {
    long base = (long)(h->top + r) * 16;
    int cx = h->x + 1, cy = h->y + 1 + r;
    char tmp[8];
    if (!h->data || base >= h->len) break;

    caca_set_color_ansi(gmo.cv, fg, bg);
    snprintf(tmp, sizeof tmp, "%04lx", base);
    caca_put_str(gmo.cv, cx, cy, tmp);
    cx += 4;
    if (cx + 2 < h->x + h->width) { caca_put_str(gmo.cv, cx, cy, "  "); }
    cx += 2;

    /* hex columns */
    for (i = 0; i < 16; i++) {
      long idx = base + i;
      int hot = (h->hl_len > 0 && idx >= h->hl_off && idx < h->hl_off + h->hl_len);
      if (i == 8) { caca_set_color_ansi(gmo.cv, fg, bg); cx += 1; }
      if (cx + 2 >= h->x + h->width) break;
      if (idx < h->len) {
        snprintf(tmp, sizeof tmp, "%02x", h->data[idx]);
        if (hot) caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
        else     caca_set_color_ansi(gmo.cv, fg, bg);
        caca_put_char(gmo.cv, cx, cy, tmp[0]);
        caca_put_char(gmo.cv, cx + 1, cy, tmp[1]);
      }
      cx += 3;
    }

    /* ascii gutter */
    caca_set_color_ansi(gmo.cv, fg, bg);
    if (cx + 2 < h->x + h->width) { caca_put_str(gmo.cv, cx, cy, " |"); cx += 2; }
    for (i = 0; i < 16; i++) {
      long idx = base + i;
      int hot = (h->hl_len > 0 && idx >= h->hl_off && idx < h->hl_off + h->hl_len);
      uint8_t b;
      if (idx >= h->len) break;
      if (cx >= h->x + h->width - 1) break;
      b = h->data[idx];
      if (hot) caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
      else     caca_set_color_ansi(gmo.cv, fg, bg);
      caca_put_char(gmo.cv, cx, cy, (b >= 32 && b < 127) ? (char)b : '.');
      cx += 1;
    }
    caca_set_color_ansi(gmo.cv, fg, bg);
    if (cx < h->x + h->width - 1) caca_put_char(gmo.cv, cx, cy, '|');
  }
}

int gtcaca_hexview_key(gtcaca_hexview_widget_t *h, int key, void *userdata)
{
  int rows = (h->height - 2) > 0 ? h->height - 2 : 1;
  int total = (h->len + 15) / 16;
  (void)userdata;
  switch (key) {
  case CACA_KEY_UP:       if (h->top > 0) h->top--; return 1;
  case CACA_KEY_DOWN:     if (h->top < total - 1) h->top++; return 1;
  case CACA_KEY_PAGEUP:   h->top -= rows; if (h->top < 0) h->top = 0; return 1;
  case CACA_KEY_PAGEDOWN: h->top += rows; if (h->top > total - 1) h->top = total - 1; if (h->top < 0) h->top = 0; return 1;
  case CACA_KEY_HOME:     h->top = 0; return 1;
  case CACA_KEY_END:      h->top = total - rows; if (h->top < 0) h->top = 0; return 1;
  }
  return 0;
}

void gtcaca_hexview_free(gtcaca_hexview_widget_t *h)
{
  if (!h) return;
  free(h->title);
  free(h);
}
