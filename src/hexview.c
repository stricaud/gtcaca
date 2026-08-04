#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/hexview.h>
#include <gtcaca/main.h>

/* Colours the widget falls back on when the application installs no cell hook.
   Kept as 12-bit RGB so every path through _pen() speaks one language. */
#define RGB_WHITE   0xFFF
#define RGB_BLACK   0x000
#define RGB_CYAN    0x0CC
#define RGB_SELBG   0x448

static long _clamp(long v, long lo, long hi)
{
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/* ── data access ───────────────────────────────────────────────────────────*/

/* Read up to `n` bytes at `off` into `out`; returns how many. Hides whether the
   bytes come from a borrowed buffer or an application provider. */
static long _read(gtcaca_hexview_widget_t *h, long off, uint8_t *out, long n)
{
  if (off < 0 || off >= h->len) return 0;
  if (n > h->len - off) n = h->len - off;
  if (n <= 0) return 0;
  if (h->read_cb)
    return h->read_cb(h, off, out, n, h->read_ud);
  if (!h->data) return 0;
  memcpy(out, h->data + off, (size_t)n);
  return n;
}

static int _byte(gtcaca_hexview_widget_t *h, long off, uint8_t *out)
{
  return _read(h, off, out, 1) == 1;
}

/* ── layout ────────────────────────────────────────────────────────────────*/

static int _addr_digits(gtcaca_hexview_widget_t *h)
{
  long max;
  int d;
  if (h->addr_digits > 0) return h->addr_digits;
  max = h->base_addr + (h->len > 0 ? h->len - 1 : 0);
  for (d = 4; d < 16; d += 2)
    if (max < (1L << (d * 4)) || d == 14) return d;
  return 8;
}

/* Columns one row of `bpr` bytes needs, given the current options. */
static int _row_width(gtcaca_hexview_widget_t *h, int bpr, int addr_w)
{
  int gaps = (h->group_size > 0 && bpr > h->group_size)
                 ? (bpr - 1) / h->group_size
                 : 0;
  int w = addr_w + 2 + bpr * 3 + gaps;      /* "addr  hh hh hh" */
  if (h->show_ascii)
    w += 2 + bpr + 1;                       /* " |ascii|" */
  return w;
}

/* Effective bytes per row: the configured value, or the widest that fits. */
static int _bpr(gtcaca_hexview_widget_t *h)
{
  int inner, addr_w, step, n, best;
  if (h->bytes_per_row > 0)
    return h->bytes_per_row > GTCACA_HEXVIEW_MAX_BPR ? GTCACA_HEXVIEW_MAX_BPR
                                                     : h->bytes_per_row;
  inner = h->show_box ? h->width - 2 : h->width;
  addr_w = _addr_digits(h);
  step = h->group_size > 0 ? h->group_size : 1;
  best = step;
  for (n = step; n <= GTCACA_HEXVIEW_MAX_BPR; n += step)
    if (_row_width(h, n, addr_w) <= inner) best = n;
  return best;
}

int gtcaca_hexview_bytes_per_row(gtcaca_hexview_widget_t *h)
{
  return h ? _bpr(h) : 0;
}

static int _rows(gtcaca_hexview_widget_t *h)
{
  int r = h->show_box ? h->height - 2 : h->height;
  return r > 0 ? r : 1;
}

/* Scroll so `off` is on screen. */
static void _reveal(gtcaca_hexview_widget_t *h, long off)
{
  int rows = _rows(h), bpr = _bpr(h);
  long row = off / bpr;
  if (row < h->top) h->top = row;
  else if (row >= h->top + rows) h->top = row - rows + 1;
  if (h->top < 0) h->top = 0;
}

/* ── construction ──────────────────────────────────────────────────────────*/

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
  h->cursor = 0;
  h->nibble = 0;
  h->ascii_pane = 0;
  h->anchor = -1;
  h->bytes_per_row = 16;
  h->group_size = 8;
  h->base_addr = 0;
  h->addr_digits = 0;
  h->show_ascii = 1;
  h->show_box = 1;
  h->editable = 0;
  h->insert_mode = 0;
  h->title = NULL;
  h->tags = NULL;
  h->n_tags = h->cap_tags = 0;
  h->cell_cb = NULL;   h->cell_ud = NULL;
  h->edit_cb = NULL;   h->edit_ud = NULL;
  h->splice_cb = NULL; h->splice_ud = NULL;
  h->read_cb = NULL;   h->read_ud = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(h));
  return h;
}

void gtcaca_hexview_free(gtcaca_hexview_widget_t *h)
{
  if (!h) return;
  free(h->title);
  free(h->tags);
  free(h);
}

/* ── data ──────────────────────────────────────────────────────────────────*/

void gtcaca_hexview_set_data(gtcaca_hexview_widget_t *h, const uint8_t *data, long len)
{
  if (!h) return;
  h->data = data;
  h->read_cb = NULL;
  h->len = len < 0 ? 0 : len;
  if (h->top * _bpr(h) >= h->len) h->top = 0;
  if (h->cursor >= h->len) h->cursor = h->len > 0 ? h->len - 1 : 0;
  if (h->anchor >= h->len) h->anchor = -1;
}

void gtcaca_hexview_set_read_cb(gtcaca_hexview_widget_t *h, gtcaca_hexview_read_cb cb,
                                void *userdata, long len)
{
  if (!h) return;
  h->read_cb = cb;
  h->read_ud = userdata;
  h->data = NULL;
  h->len = len < 0 ? 0 : len;
  if (h->cursor >= h->len) h->cursor = h->len > 0 ? h->len - 1 : 0;
}

long gtcaca_hexview_length(gtcaca_hexview_widget_t *h) { return h ? h->len : 0; }

/* ── cursor and selection ──────────────────────────────────────────────────*/

int gtcaca_hexview_cursor(gtcaca_hexview_widget_t *h)
{
  if (!h || h->len <= 0) return -1;
  return (int)h->cursor;
}

void gtcaca_hexview_set_cursor(gtcaca_hexview_widget_t *h, long off)
{
  if (!h || h->len <= 0) return;
  h->cursor = _clamp(off, 0, h->len - 1);
  h->nibble = 0;
  _reveal(h, h->cursor);
}

int  gtcaca_hexview_nibble(gtcaca_hexview_widget_t *h) { return h ? h->nibble : 0; }
void gtcaca_hexview_set_nibble(gtcaca_hexview_widget_t *h, int low)
{
  if (h) h->nibble = low ? 1 : 0;
}

int  gtcaca_hexview_pane(gtcaca_hexview_widget_t *h) { return h ? h->ascii_pane : 0; }
void gtcaca_hexview_set_pane(gtcaca_hexview_widget_t *h, int ascii_pane)
{
  if (!h) return;
  h->ascii_pane = ascii_pane ? 1 : 0;
  h->nibble = 0;
}

void gtcaca_hexview_set_selection(gtcaca_hexview_widget_t *h, long anchor, long caret)
{
  if (!h || h->len <= 0) return;
  h->anchor = _clamp(anchor, 0, h->len - 1);
  h->cursor = _clamp(caret, 0, h->len - 1);
  _reveal(h, h->cursor);
}

void gtcaca_hexview_clear_selection(gtcaca_hexview_widget_t *h)
{
  if (h) h->anchor = -1;
}

int gtcaca_hexview_selection(gtcaca_hexview_widget_t *h, long *lo, long *hi)
{
  if (!h || h->anchor < 0) return 0;
  if (lo) *lo = h->anchor < h->cursor ? h->anchor : h->cursor;
  if (hi) *hi = h->anchor < h->cursor ? h->cursor : h->anchor;
  return 1;
}

/* ── appearance ────────────────────────────────────────────────────────────*/

void gtcaca_hexview_set_title(gtcaca_hexview_widget_t *h, const char *title)
{
  if (!h) return;
  free(h->title);
  h->title = title ? strdup(title) : NULL;
}

void gtcaca_hexview_set_bytes_per_row(gtcaca_hexview_widget_t *h, int n)
{
  if (!h) return;
  h->bytes_per_row = n < 0 ? 0 : (n > GTCACA_HEXVIEW_MAX_BPR ? GTCACA_HEXVIEW_MAX_BPR : n);
}
void gtcaca_hexview_set_group_size(gtcaca_hexview_widget_t *h, int bytes)
{
  if (h) h->group_size = bytes < 0 ? 0 : bytes;
}
void gtcaca_hexview_set_base_addr(gtcaca_hexview_widget_t *h, long base)
{
  if (h) h->base_addr = base;
}
void gtcaca_hexview_set_addr_digits(gtcaca_hexview_widget_t *h, int digits)
{
  if (h) h->addr_digits = digits < 0 ? 0 : (digits > 16 ? 16 : digits);
}
void gtcaca_hexview_set_show_ascii(gtcaca_hexview_widget_t *h, int on)
{
  if (h) h->show_ascii = on ? 1 : 0;
}
void gtcaca_hexview_set_box(gtcaca_hexview_widget_t *h, int on)
{
  if (h) h->show_box = on ? 1 : 0;
}

/* ── highlighting ──────────────────────────────────────────────────────────*/

void gtcaca_hexview_set_highlight(gtcaca_hexview_widget_t *h, long off, long len)
{
  if (!h) return;
  h->hl_off = off;
  h->hl_len = len;
  if (len > 0 && off >= 0) _reveal(h, off);
}

int gtcaca_hexview_add_tag(gtcaca_hexview_widget_t *h, long off, long len,
                           uint16_t fg, uint16_t bg, const char *name)
{
  gtcaca_hexview_tag_t *t;
  if (!h || len <= 0) return -1;
  if (h->n_tags == h->cap_tags) {
    int cap = h->cap_tags ? h->cap_tags * 2 : 16;
    gtcaca_hexview_tag_t *nt = realloc(h->tags, (size_t)cap * sizeof *nt);
    if (!nt) return -1;
    h->tags = nt;
    h->cap_tags = cap;
  }
  t = &h->tags[h->n_tags];
  t->off = off;
  t->len = len;
  t->fg = fg;
  t->bg = bg;
  t->name[0] = '\0';
  if (name) {
    strncpy(t->name, name, sizeof t->name - 1);
    t->name[sizeof t->name - 1] = '\0';
  }
  return h->n_tags++;
}

void gtcaca_hexview_clear_tags(gtcaca_hexview_widget_t *h)
{
  if (h) h->n_tags = 0;
}

/* The last tag wins, so a tag added later paints over an earlier one. */
static const gtcaca_hexview_tag_t *_tag_at(gtcaca_hexview_widget_t *h, long off)
{
  int i;
  for (i = h->n_tags - 1; i >= 0; i--)
    if (off >= h->tags[i].off && off < h->tags[i].off + h->tags[i].len)
      return &h->tags[i];
  return NULL;
}

const char *gtcaca_hexview_tag_at(gtcaca_hexview_widget_t *h, long off)
{
  const gtcaca_hexview_tag_t *t;
  if (!h) return NULL;
  t = _tag_at(h, off);
  return t ? t->name : NULL;
}

long gtcaca_hexview_top(gtcaca_hexview_widget_t *h) { return h ? h->top : 0; }

void gtcaca_hexview_set_top(gtcaca_hexview_widget_t *h, long row)
{
  long last;
  if (!h || h->len <= 0) return;
  last = (h->len - 1) / _bpr(h);          /* last row that holds a byte */
  h->top = _clamp(row, 0, last);
}

void gtcaca_hexview_scroll(gtcaca_hexview_widget_t *h, int rows)
{
  if (h) gtcaca_hexview_set_top(h, h->top + rows);
}

void gtcaca_hexview_set_cell_cb(gtcaca_hexview_widget_t *h,
                                gtcaca_hexview_cell_cb cb, void *userdata)
{
  if (!h) return;
  h->cell_cb = cb;
  h->cell_ud = userdata;
}

/* ── editing ───────────────────────────────────────────────────────────────*/

void gtcaca_hexview_set_editable(gtcaca_hexview_widget_t *h, int on)
{
  if (h) h->editable = on ? 1 : 0;
}
int gtcaca_hexview_editable(gtcaca_hexview_widget_t *h) { return h ? h->editable : 0; }

void gtcaca_hexview_set_insert_mode(gtcaca_hexview_widget_t *h, int on)
{
  if (h) h->insert_mode = on ? 1 : 0;
}
int gtcaca_hexview_insert_mode(gtcaca_hexview_widget_t *h) { return h ? h->insert_mode : 0; }

void gtcaca_hexview_set_edit_cb(gtcaca_hexview_widget_t *h,
                                gtcaca_hexview_edit_cb cb, void *userdata)
{
  if (!h) return;
  h->edit_cb = cb;
  h->edit_ud = userdata;
}

void gtcaca_hexview_set_splice_cb(gtcaca_hexview_widget_t *h,
                                  gtcaca_hexview_splice_cb cb, void *userdata)
{
  if (!h) return;
  h->splice_cb = cb;
  h->splice_ud = userdata;
}

/* ── search ────────────────────────────────────────────────────────────────*/

long gtcaca_hexview_find(gtcaca_hexview_widget_t *h, const uint8_t *needle,
                         int nlen, long from, int backwards)
{
  long i, j;
  uint8_t b;
  if (!h || !needle || nlen <= 0 || h->len < nlen) return -1;
  if (backwards) {
    if (from > h->len - nlen) from = h->len - nlen;
    for (i = from; i >= 0; i--) {
      for (j = 0; j < nlen; j++)
        if (!_byte(h, i + j, &b) || b != needle[j]) break;
      if (j == nlen) return i;
    }
    return -1;
  }
  if (from < 0) from = 0;
  for (i = from; i <= h->len - nlen; i++) {
    for (j = 0; j < nlen; j++)
      if (!_byte(h, i + j, &b) || b != needle[j]) break;
    if (j == nlen) return i;
  }
  return -1;
}

/* ── drawing ───────────────────────────────────────────────────────────────*/

/* Set the pen for one byte cell. The application's hook wins outright; without
   it the order is cursor, selection, tag, highlight range, theme. */
static void _pen(gtcaca_hexview_widget_t *h, long idx, int is_ascii,
                 uint8_t fg, uint8_t bg)
{
  const gtcaca_hexview_tag_t *tag;
  long lo, hi;
  int cur;

  if (h->cell_cb) {
    uint16_t cfg = 0, cbg = 0;
    if (h->cell_cb(h, idx, is_ascii, &cfg, &cbg, h->cell_ud)) {
      caca_set_color_argb(gmo.cv, (uint16_t)(0xF000u | (cfg & 0x0FFFu)),
                          (uint16_t)(0xF000u | (cbg & 0x0FFFu)));
      return;
    }
  }

  cur = h->has_focus && idx == h->cursor &&
        (is_ascii ? h->ascii_pane : !h->ascii_pane);
  if (cur) {
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_WHITE);
    return;
  }
  /* the cursor's other half is dimmed, so both panes stay locatable */
  if (h->has_focus && idx == h->cursor) {
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_LIGHTGRAY);
    return;
  }
  if (gtcaca_hexview_selection(h, &lo, &hi) && idx >= lo && idx <= hi) {
    caca_set_color_argb(gmo.cv, 0xF000u | RGB_WHITE, 0xF000u | RGB_SELBG);
    return;
  }
  if ((tag = _tag_at(h, idx)) != NULL) {
    caca_set_color_argb(gmo.cv, (uint16_t)(0xF000u | (tag->fg & 0x0FFFu)),
                        (uint16_t)(0xF000u | (tag->bg & 0x0FFFu)));
    return;
  }
  if (h->hl_len > 0 && idx >= h->hl_off && idx < h->hl_off + h->hl_len) {
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN);
    return;
  }
  caca_set_color_ansi(gmo.cv, fg, bg);
}

void gtcaca_hexview_draw(gtcaca_hexview_widget_t *h)
{
  int rows, bpr, addr_w, r, i, x0, y0, right;
  uint8_t fg, bg;
  uint8_t row[GTCACA_HEXVIEW_MAX_BPR];
  char tmp[32];

  if (!h || !h->is_visible) return;

  fg = h->has_focus ? h->color_focus_fg : h->color_nonfocus_fg;
  bg = h->has_focus ? h->color_focus_bg : h->color_nonfocus_bg;
  bpr = _bpr(h);
  rows = _rows(h);
  addr_w = _addr_digits(h);
  x0 = h->show_box ? h->x + 1 : h->x;
  y0 = h->show_box ? h->y + 1 : h->y;
  right = h->x + h->width - (h->show_box ? 1 : 0);

  caca_set_color_ansi(gmo.cv, fg, bg);
  caca_fill_box(gmo.cv, h->x, h->y, h->width, h->height, ' ');
  if (h->show_box) {
    caca_draw_cp437_box(gmo.cv, h->x, h->y, h->width, h->height);
    if (h->title)
      caca_printf(gmo.cv, h->x + 2, h->y, " %s ", h->title);
  } else if (h->title) {
    caca_printf(gmo.cv, h->x, h->y, "%s", h->title);
  }

  for (r = 0; r < rows; r++) {
    long base = (h->top + r) * bpr;
    long got;
    int cx = x0, cy = y0 + r;
    if (base >= h->len) break;

    got = _read(h, base, row, bpr);

    caca_set_color_ansi(gmo.cv, fg, bg);
    snprintf(tmp, sizeof tmp, "%0*lx", addr_w, (unsigned long)(h->base_addr + base));
    caca_put_str(gmo.cv, cx, cy, tmp);
    cx += addr_w + 2;

    for (i = 0; i < bpr; i++) {
      if (h->group_size > 0 && i > 0 && i % h->group_size == 0) {
        caca_set_color_ansi(gmo.cv, fg, bg);
        cx += 1;
      }
      if (cx + 2 > right) break;
      if (i < got) {
        snprintf(tmp, sizeof tmp, "%02x", row[i]);
        _pen(h, base + i, 0, fg, bg);
        caca_put_char(gmo.cv, cx, cy, tmp[0]);
        caca_put_char(gmo.cv, cx + 1, cy, tmp[1]);
      }
      cx += 3;
    }

    if (!h->show_ascii) continue;

    caca_set_color_ansi(gmo.cv, fg, bg);
    if (cx + 2 <= right) { caca_put_str(gmo.cv, cx, cy, " |"); cx += 2; }
    for (i = 0; i < got; i++) {
      uint8_t b = row[i];
      if (cx >= right - 1) break;
      _pen(h, base + i, 1, fg, bg);
      caca_put_char(gmo.cv, cx, cy, (b >= 32 && b < 127) ? (char)b : '.');
      cx += 1;
    }
    caca_set_color_ansi(gmo.cv, fg, bg);
    if (cx < right) caca_put_char(gmo.cv, cx, cy, '|');
  }
}

/* ── keys ──────────────────────────────────────────────────────────────────*/

static int _hexval(int key)
{
  if (key >= '0' && key <= '9') return key - '0';
  if (key >= 'a' && key <= 'f') return key - 'a' + 10;
  if (key >= 'A' && key <= 'F') return key - 'A' + 10;
  return -1;
}

/* Ask the application to put `value` at `off`; returns whether it accepted. */
static int _write(gtcaca_hexview_widget_t *h, long off, uint8_t value)
{
  if (!h->edit_cb) return 0;
  return h->edit_cb(h, off, value, h->edit_ud) != 0;
}

static int _splice(gtcaca_hexview_widget_t *h, long off, int is_insert)
{
  if (!h->splice_cb) return 0;
  return h->splice_cb(h, off, is_insert, h->splice_ud) != 0;
}

static int _key_edit(gtcaca_hexview_widget_t *h, int key)
{
  uint8_t cur = 0;
  int v;

  if (key == CACA_KEY_INSERT) {
    h->insert_mode = !h->insert_mode;
    return 1;
  }
  if (key == CACA_KEY_DELETE) {
    if (_splice(h, h->cursor, 0)) {
      if (h->cursor >= h->len && h->cursor > 0) h->cursor--;
      h->nibble = 0;
    }
    return 1;
  }
  if (key == CACA_KEY_BACKSPACE) {
    if (h->cursor > 0 && _splice(h, h->cursor - 1, 0)) {
      h->cursor--;
      h->nibble = 0;
      _reveal(h, h->cursor);
    }
    return 1;
  }

  if (h->ascii_pane) {
    if (key < 32 || key > 126) return 0;
    if (h->insert_mode && !_splice(h, h->cursor, 1)) return 1;
    if (_write(h, h->cursor, (uint8_t)key) && h->cursor + 1 < h->len) {
      h->cursor++;
      _reveal(h, h->cursor);
    }
    return 1;
  }

  v = _hexval(key);
  if (v < 0) return 0;
  /* A fresh byte is only spliced in on the high nibble, so typing both digits
     of one byte inserts once rather than twice. */
  if (h->insert_mode && h->nibble == 0 && !_splice(h, h->cursor, 1)) return 1;
  _byte(h, h->cursor, &cur);
  if (h->nibble == 0) {
    if (_write(h, h->cursor, (uint8_t)((v << 4) | (cur & 0x0F))))
      h->nibble = 1;
  } else {
    if (_write(h, h->cursor, (uint8_t)((cur & 0xF0) | v))) {
      h->nibble = 0;
      if (h->cursor + 1 < h->len) {
        h->cursor++;
        _reveal(h, h->cursor);
      }
    }
  }
  return 1;
}

int gtcaca_hexview_key(gtcaca_hexview_widget_t *h, int key, void *userdata)
{
  int rows, bpr;
  long c;
  (void)userdata;
  if (!h || h->len <= 0) return 0;

  rows = _rows(h);
  bpr = _bpr(h);
  c = h->cursor;

  switch (key) {
  case CACA_KEY_LEFT:     c -= 1; break;
  case CACA_KEY_RIGHT:    c += 1; break;
  case CACA_KEY_UP:       c -= bpr; break;
  case CACA_KEY_DOWN:     c += bpr; break;
  case CACA_KEY_PAGEUP:   c -= (long)rows * bpr; break;
  case CACA_KEY_PAGEDOWN: c += (long)rows * bpr; break;
  case CACA_KEY_HOME:     c -= c % bpr; break;
  case CACA_KEY_END:      c += bpr - 1 - (c % bpr); break;
  case CACA_KEY_TAB:
    if (!h->show_ascii) return 0;
    h->ascii_pane = !h->ascii_pane;
    h->nibble = 0;
    return 1;
  default:
    return h->editable ? _key_edit(h, key) : 0;
  }

  h->cursor = _clamp(c, 0, h->len - 1);
  h->nibble = 0;
  _reveal(h, h->cursor);
  return 1;
}
