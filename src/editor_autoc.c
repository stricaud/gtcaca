#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <caca.h>

#include <gtcaca/editor.h>
#include <gtcaca/main.h>

#define AUTOC_DEFAULT_MAX_H 9
#define AUTOC_MAX_ITEM_W    30

static int _digits(int n) { int d = 1; while (n >= 10) { n /= 10; d++; } return d; }

/* ── small helpers ─────────────────────────────────────────────────────────── */

static int _ci_eq(char a, char b, int ignore_case)
{
  if (!ignore_case) return a == b;
  return tolower((unsigned char)a) == tolower((unsigned char)b);
}

/* The text already typed for the word being completed. */
static int _prefix(gtcaca_editor_widget_t *w, char *buf, int sz)
{
  int n = w->current_pos - w->autoc.pos_start;
  if (n < 0) n = 0;
  return gtcaca_editor_get_text_range(w, w->autoc.pos_start, w->current_pos, buf, sz < n + 1 ? sz : n + 1);
}

static int _item_matches(const char *item, const char *prefix, int plen, int ignore_case)
{
  int i;
  for (i = 0; i < plen; i++)
    if (!item[i] || !_ci_eq(item[i], prefix[i], ignore_case)) return 0;
  return 1;
}

static void _scroll_to_sel(gtcaca_editor_autoc_t *a)
{
  if (a->sel < a->top) a->top = a->sel;
  if (a->sel >= a->top + a->max_height) a->top = a->sel - a->max_height + 1;
  if (a->top < 0) a->top = 0;
}

/* Rebuild the filtered index list from the current prefix. */
static void _refilter(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  char prefix[256];
  int plen = _prefix(w, prefix, sizeof prefix);
  int i;

  a->n_filtered = 0;
  for (i = 0; i < a->n_items; i++)
    if (_item_matches(a->items[i], prefix, plen, a->ignore_case))
      a->filtered[a->n_filtered++] = i;

  if (a->sel >= a->n_filtered) a->sel = a->n_filtered > 0 ? a->n_filtered - 1 : 0;
  if (a->sel < 0) a->sel = 0;
  _scroll_to_sel(a);
}

/* ── lifecycle ─────────────────────────────────────────────────────────────── */

void gtcaca_editor_autoc_cancel(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  int i;
  for (i = 0; i < a->n_items; i++) free(a->items[i]);
  free(a->items);
  free(a->filtered);
  a->items = NULL;
  a->filtered = NULL;
  a->n_items = a->n_filtered = a->sel = a->top = 0;
  a->active = 0;
}

void gtcaca_editor_autoc_show(gtcaca_editor_widget_t *w, int len_entered, const char *item_list)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  const char *p, *start;
  int cap = 0;

  gtcaca_editor_autoc_cancel(w);
  if (!item_list) return;

  a->pos_start = w->current_pos - (len_entered > 0 ? len_entered : 0);
  if (a->pos_start < 0) a->pos_start = 0;

  /* split item_list on the separator, skipping empty fields */
  p = start = item_list;
  for (;;) {
    if (*p == a->separator || *p == '\0') {
      int len = (int)(p - start);
      if (len > 0) {
        if (a->n_items == cap) {
          cap = cap ? cap * 2 : 16;
          char **ni = realloc(a->items, (size_t)cap * sizeof(char *));
          int   *nf = realloc(a->filtered, (size_t)cap * sizeof(int));
          if (!ni || !nf) { free(ni); free(nf); gtcaca_editor_autoc_cancel(w); return; }
          a->items = ni; a->filtered = nf;
        }
        a->items[a->n_items] = malloc((size_t)len + 1);
        if (a->items[a->n_items]) { memcpy(a->items[a->n_items], start, (size_t)len); a->items[a->n_items][len] = '\0'; a->n_items++; }
      }
      if (*p == '\0') break;
      start = p + 1;
    }
    p++;
  }

  a->active = 1;
  a->sel = a->top = 0;
  _refilter(w);
  if (a->auto_hide && a->n_filtered == 0) gtcaca_editor_autoc_cancel(w);
}

int  gtcaca_editor_autoc_active(gtcaca_editor_widget_t *w)    { return w->autoc.active; }
int  gtcaca_editor_autoc_pos_start(gtcaca_editor_widget_t *w) { return w->autoc.pos_start; }

int gtcaca_editor_autoc_get_current(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  return (a->active && a->n_filtered > 0) ? a->filtered[a->sel] : -1;
}

int gtcaca_editor_autoc_get_current_text(gtcaca_editor_widget_t *w, char *buf, int len)
{
  int idx = gtcaca_editor_autoc_get_current(w);
  if (idx < 0 || len <= 0) { if (len > 0) buf[0] = '\0'; return 0; }
  strncpy(buf, w->autoc.items[idx], (size_t)len - 1);
  buf[len - 1] = '\0';
  return (int)strlen(buf);
}

void gtcaca_editor_autoc_select(gtcaca_editor_widget_t *w, const char *prefix)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  int plen = prefix ? (int)strlen(prefix) : 0, i;
  for (i = 0; i < a->n_filtered; i++) {
    if (_item_matches(a->items[a->filtered[i]], prefix, plen, a->ignore_case)) { a->sel = i; _scroll_to_sel(a); return; }
  }
}

void gtcaca_editor_autoc_complete(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  char chosen[256];
  int start;

  if (!a->active) return;
  if (a->n_filtered == 0) { gtcaca_editor_autoc_cancel(w); return; }

  strncpy(chosen, a->items[a->filtered[a->sel]], sizeof chosen - 1);
  chosen[sizeof chosen - 1] = '\0';
  start = a->pos_start;

  if (w->current_pos > start) gtcaca_editor_delete_range(w, start, w->current_pos - start);
  gtcaca_editor_insert_text(w, start, chosen);
  gtcaca_editor_set_empty_selection(w, start + (int)strlen(chosen));

  gtcaca_editor_autoc_cancel(w);
  if (w->autoc_cb) w->autoc_cb(w, chosen, w->autoc_cb_userdata);
}

/* ── options ───────────────────────────────────────────────────────────────── */

void gtcaca_editor_autoc_set_separator(gtcaca_editor_widget_t *w, char sep) { w->autoc.separator = sep ? sep : ' '; }
char gtcaca_editor_autoc_get_separator(gtcaca_editor_widget_t *w)           { return w->autoc.separator; }
void gtcaca_editor_autoc_set_ignore_case(gtcaca_editor_widget_t *w, int on) { w->autoc.ignore_case = on ? 1 : 0; }
void gtcaca_editor_autoc_set_auto_hide(gtcaca_editor_widget_t *w, int on)   { w->autoc.auto_hide = on ? 1 : 0; }
void gtcaca_editor_autoc_set_cancel_at_start(gtcaca_editor_widget_t *w, int on) { w->autoc.cancel_at_start = on ? 1 : 0; }
void gtcaca_editor_autoc_set_max_height(gtcaca_editor_widget_t *w, int rows) { w->autoc.max_height = rows > 0 ? rows : 1; }
void gtcaca_editor_autoc_set_fillups(gtcaca_editor_widget_t *w, const char *chars) { free(w->autoc.fillups); w->autoc.fillups = chars ? strdup(chars) : NULL; }
void gtcaca_editor_autoc_stops(gtcaca_editor_widget_t *w, const char *chars)       { free(w->autoc.stops);   w->autoc.stops   = chars ? strdup(chars) : NULL; }
void gtcaca_editor_set_autoc_cb(gtcaca_editor_widget_t *w, gtcaca_editor_autoc_cb_t cb, void *userdata) { w->autoc_cb = cb; w->autoc_cb_userdata = userdata; }

/* ── key handling while the list is up ─────────────────────────────────────── */

int _gtcaca_editor_autoc_key(gtcaca_editor_widget_t *w, int key)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  if (!a->active) return 0;

  switch (key) {
  case CACA_KEY_UP:
  case CACA_KEY_CTRL_P:
    if (a->sel > 0) a->sel--;
    _scroll_to_sel(a);
    return 1;
  case CACA_KEY_DOWN:
  case CACA_KEY_CTRL_N:
    if (a->sel < a->n_filtered - 1) a->sel++;
    _scroll_to_sel(a);
    return 1;
  case CACA_KEY_PAGEUP:
    a->sel -= a->max_height; if (a->sel < 0) a->sel = 0; _scroll_to_sel(a);
    return 1;
  case CACA_KEY_PAGEDOWN:
    a->sel += a->max_height; if (a->sel >= a->n_filtered) a->sel = a->n_filtered - 1; if (a->sel < 0) a->sel = 0; _scroll_to_sel(a);
    return 1;
  case CACA_KEY_RETURN:
  case 10:
  case CACA_KEY_TAB:
    gtcaca_editor_autoc_complete(w);
    return 1;
  case CACA_KEY_ESCAPE:
  case CACA_KEY_CTRL_G:
    gtcaca_editor_autoc_cancel(w);
    return 1;
  case CACA_KEY_BACKSPACE:
  case CACA_KEY_DELETE:                       /* 0x7f mac backspace */
    if (w->current_pos <= a->pos_start) {     /* deleting past the word start */
      if (a->cancel_at_start) gtcaca_editor_autoc_cancel(w);
      return 0;                               /* let the delete happen normally */
    }
    gtcaca_editor_delete_back(w);
    _refilter(w);
    if (a->auto_hide && a->n_filtered == 0) gtcaca_editor_autoc_cancel(w);
    return 1;
  case CACA_KEY_LEFT:
  case CACA_KEY_RIGHT:
  case CACA_KEY_HOME:
  case CACA_KEY_END:
    gtcaca_editor_autoc_cancel(w);            /* caret leaves the word */
    return 0;
  default:
    if (key >= 32 && key <= 126) {
      char c = (char)key;
      if (a->stops && strchr(a->stops, c))   { gtcaca_editor_autoc_cancel(w); return 0; }
      if (a->fillups && strchr(a->fillups, c)) { gtcaca_editor_autoc_complete(w); return 0; }
      if (isalnum((unsigned char)c) || c == '_' || c == '$') {
        gtcaca_editor_add_char(w, c);
        _refilter(w);
        if (a->auto_hide && a->n_filtered == 0) gtcaca_editor_autoc_cancel(w);
        return 1;
      }
      gtcaca_editor_autoc_cancel(w);          /* any other char cancels */
      return 0;
    }
    return 0;
  }
}

/* ── pop-up rendering (called at the end of gtcaca_editor_draw) ─────────────── */

void _gtcaca_editor_draw_autoc(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_autoc_t *a = &w->autoc;
  int line_count = gtcaca_editor_get_line_count(w);
  int text_h  = w->height - 2;
  int total_w = w->width - 2;
  int margin_w = w->show_line_numbers ? _digits(line_count) + 1 : 0;
  int text_w  = total_w - margin_w;
  int inner_x = w->x + 1, inner_y = w->y + 1, origin_x = inner_x + margin_w;
  int caret_line = gtcaca_editor_line_from_position(w, w->current_pos);
  int word_col = gtcaca_editor_get_column(w, a->pos_start);
  int nshow, popw, pop_x, pop_y, r, i, maxlen = 0;

  if (!a->active || a->n_filtered == 0 || text_h <= 0 || text_w <= 0) return;

  nshow = a->n_filtered < a->max_height ? a->n_filtered : a->max_height;

  for (i = 0; i < a->n_filtered; i++) {
    int l = (int)strlen(a->items[a->filtered[i]]);
    if (l > maxlen) maxlen = l;
  }
  popw = maxlen; if (popw > AUTOC_MAX_ITEM_W) popw = AUTOC_MAX_ITEM_W; if (popw < 6) popw = 6;
  if (popw > text_w) popw = text_w;

  pop_x = origin_x + (word_col - w->x_offset);
  if (pop_x + popw > inner_x + text_w) pop_x = inner_x + text_w - popw;
  if (pop_x < inner_x) pop_x = inner_x;

  pop_y = inner_y + (caret_line - w->first_visible_line) + 1;   /* below the caret line */
  if (pop_y + nshow > inner_y + text_h)                          /* …unless it overflows */
    pop_y = inner_y + (caret_line - w->first_visible_line) - nshow;
  if (pop_y < inner_y) pop_y = inner_y;

  for (r = 0; r < nshow; r++) {
    int idx = a->top + r;
    int col;
    const char *item;
    int selected;
    if (idx >= a->n_filtered) break;
    item = a->items[a->filtered[idx]];
    selected = (idx == a->sel);

    if (selected) caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLUE);
    else          caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_LIGHTGRAY);

    for (col = 0; col < popw; col++) {
      char ch = (col < (int)strlen(item)) ? item[col] : ' ';
      caca_put_char(gmo.cv, pop_x + col, pop_y + r, ch);
    }
    /* scrollbar tick on the right edge when the list is longer than the box */
    if (a->n_filtered > nshow) {
      int thumb = (a->sel * (nshow - 1)) / (a->n_filtered - 1);
      caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_DARKGRAY);
      caca_put_char(gmo.cv, pop_x + popw - 1, pop_y + r, r == thumb ? '#' : ' ');
    }
  }
}
