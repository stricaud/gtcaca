#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/editor.h>
#include <gtcaca/main.h>

/* ── small helpers ─────────────────────────────────────────────────────────── */

static int _clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void _ensure_cap(gtcaca_editor_widget_t *w, int needed)
{
  if (needed <= w->cap) return;
  int ncap = w->cap ? w->cap * 2 : 256;
  if (ncap < needed) ncap = needed;
  char *p = realloc(w->text, ncap);
  if (!p) { fprintf(stderr, "Error: editor buffer realloc failed\n"); return; }
  w->text = p;
  w->cap  = ncap;
}

/* ── raw buffer ops (no undo, no caret adjust) ─────────────────────────────── */

static void _raw_insert(gtcaca_editor_widget_t *w, int pos, const char *s, int len)
{
  if (len <= 0) return;
  pos = _clampi(pos, 0, w->length);
  _ensure_cap(w, w->length + len + 1);
  memmove(w->text + pos + len, w->text + pos, (size_t)(w->length - pos) + 1); /* +1 NUL */
  memcpy(w->text + pos, s, (size_t)len);
  w->length += len;
  w->colorize_dirty = 1;
  w->fold_dirty = 1;
}

static void _raw_delete(gtcaca_editor_widget_t *w, int pos, int len)
{
  if (len <= 0) return;
  pos = _clampi(pos, 0, w->length);
  if (pos + len > w->length) len = w->length - pos;
  memmove(w->text + pos, w->text + pos + len, (size_t)(w->length - pos - len) + 1);
  w->length -= len;
  w->colorize_dirty = 1;
  w->fold_dirty = 1;
}

/* ── undo / redo stacks ────────────────────────────────────────────────────── */

static void _stack_clear(gtcaca_editor_action_t **st, int *count, int *cap)
{
  int i;
  for (i = 0; i < *count; i++) free((*st)[i].text);
  *count = 0;
  (void)cap;
}

static void _stack_push(gtcaca_editor_action_t **st, int *count, int *cap,
                        int is_insert, int pos, const char *text, int len, int group)
{
  if (*count == *cap) {
    int ncap = *cap ? *cap * 2 : 32;
    gtcaca_editor_action_t *p = realloc(*st, ncap * sizeof(**st));
    if (!p) return;
    *st = p; *cap = ncap;
  }
  gtcaca_editor_action_t *a = &(*st)[(*count)++];
  a->is_insert = is_insert;
  a->pos = pos;
  a->len = len;
  a->group = group;
  a->text = malloc((size_t)len + 1);
  if (a->text) { memcpy(a->text, text, (size_t)len); a->text[len] = '\0'; }
}

/* id stamped on edits recorded while a transaction is open (0 = standalone) */
static int _cur_undo_group(gtcaca_editor_widget_t *w)
{
  return w->undo_group_depth > 0 ? w->undo_group_id : 0;
}

void gtcaca_editor_begin_undo_action(gtcaca_editor_widget_t *w)
{
  if (w->undo_group_depth == 0) w->undo_group_id = ++w->undo_seq;
  w->undo_group_depth++;
}
void gtcaca_editor_end_undo_action(gtcaca_editor_widget_t *w)
{
  if (w->undo_group_depth > 0) w->undo_group_depth--;
}

/* ── caret adjustment after edits ──────────────────────────────────────────── */

static void _adjust_after_insert(gtcaca_editor_widget_t *w, int pos, int len)
{
  if (w->current_pos >= pos) w->current_pos += len;
  if (w->anchor      >= pos) w->anchor      += len;
}

static void _adjust_after_delete(gtcaca_editor_widget_t *w, int pos, int len)
{
  if (w->current_pos > pos + len) w->current_pos -= len;
  else if (w->current_pos > pos)  w->current_pos = pos;
  if (w->anchor > pos + len)      w->anchor -= len;
  else if (w->anchor > pos)       w->anchor = pos;
}

/* ── length-aware insert/delete with undo (the public primitives) ──────────── */

static void _insert_len(gtcaca_editor_widget_t *w, int pos, const char *s, int len)
{
  if (len <= 0 || w->read_only) return;
  pos = _clampi(pos, 0, w->length);
  _raw_insert(w, pos, s, len);
  _stack_clear(&w->redo, &w->redo_count, &w->redo_cap);
  _stack_push(&w->undo, &w->undo_count, &w->undo_cap, 1, pos, s, len, _cur_undo_group(w));
  _adjust_after_insert(w, pos, len);
  w->modified = 1;
  w->goal_col = -1;
}

void gtcaca_editor_insert_text(gtcaca_editor_widget_t *w, int pos, const char *s)
{
  if (s) _insert_len(w, pos, s, (int)strlen(s));
}

void gtcaca_editor_delete_range(gtcaca_editor_widget_t *w, int pos, int len)
{
  if (w->read_only) return;
  pos = _clampi(pos, 0, w->length);
  if (pos + len > w->length) len = w->length - pos;
  if (len <= 0) return;
  _stack_clear(&w->redo, &w->redo_count, &w->redo_cap);
  _stack_push(&w->undo, &w->undo_count, &w->undo_cap, 0, pos, w->text + pos, len, _cur_undo_group(w));
  _raw_delete(w, pos, len);
  _adjust_after_delete(w, pos, len);
  w->modified = 1;
  w->goal_col = -1;
}

/* ── line / column geometry ────────────────────────────────────────────────── */

int gtcaca_editor_get_line_count(gtcaca_editor_widget_t *w)
{
  int i, n = 1;
  for (i = 0; i < w->length; i++) if (w->text[i] == '\n') n++;
  return n;
}

int gtcaca_editor_line_from_position(gtcaca_editor_widget_t *w, int pos)
{
  int i, line = 0;
  pos = _clampi(pos, 0, w->length);
  for (i = 0; i < pos; i++) if (w->text[i] == '\n') line++;
  return line;
}

int gtcaca_editor_position_from_line(gtcaca_editor_widget_t *w, int line)
{
  int i, cur = 0;
  if (line <= 0) return 0;
  for (i = 0; i < w->length; i++) {
    if (w->text[i] == '\n') {
      if (++cur == line) return i + 1;
    }
  }
  return w->length;
}

int gtcaca_editor_get_line_end_position(gtcaca_editor_widget_t *w, int line)
{
  int p = gtcaca_editor_position_from_line(w, line);
  while (p < w->length && w->text[p] != '\n') p++;
  return p;
}

/* visual column of byte position pos within its line, expanding tabs */
static int _visual_col(gtcaca_editor_widget_t *w, int pos)
{
  int start = gtcaca_editor_position_from_line(
                w, gtcaca_editor_line_from_position(w, pos));
  int i, col = 0;
  for (i = start; i < pos; i++) {
    if (w->text[i] == '\t') col += w->tab_width - (col % w->tab_width);
    else col++;
  }
  return col;
}

/* first byte position on `line` whose visual column is >= vcol */
static int _pos_from_visual_col(gtcaca_editor_widget_t *w, int line, int vcol)
{
  int start = gtcaca_editor_position_from_line(w, line);
  int end   = gtcaca_editor_get_line_end_position(w, line);
  int i = start, col = 0;
  while (i < end && col < vcol) {
    if (w->text[i] == '\t') col += w->tab_width - (col % w->tab_width);
    else col++;
    i++;
  }
  return i;
}

int gtcaca_editor_get_column(gtcaca_editor_widget_t *w, int pos)
{
  return _visual_col(w, _clampi(pos, 0, w->length));
}

int gtcaca_editor_get_current_line(gtcaca_editor_widget_t *w)
{
  return gtcaca_editor_line_from_position(w, w->current_pos);
}

int gtcaca_editor_find_column(gtcaca_editor_widget_t *w, int line, int column)
{
  int lc = gtcaca_editor_get_line_count(w);
  if (line < 0) line = 0;
  if (line >= lc) line = lc - 1;
  return _pos_from_visual_col(w, line, column);
}

void gtcaca_editor_set_rectangular_selection(gtcaca_editor_widget_t *w, int on)
{
  w->rect_select = on ? 1 : 0;
}
int gtcaca_editor_get_rectangular_selection(gtcaca_editor_widget_t *w)
{
  return w->rect_select;
}

/* ── caret & selection ─────────────────────────────────────────────────────── */

static void _set_caret(gtcaca_editor_widget_t *w, int pos, int extend)
{
  pos = _clampi(pos, 0, w->length);
  w->current_pos = pos;
  if (!extend) w->anchor = pos;
}

int  gtcaca_editor_get_current_pos(gtcaca_editor_widget_t *w) { return w->current_pos; }
void gtcaca_editor_set_current_pos(gtcaca_editor_widget_t *w, int pos) { _set_caret(w, pos, 1); w->goal_col = -1; }
int  gtcaca_editor_get_anchor(gtcaca_editor_widget_t *w) { return w->anchor; }
void gtcaca_editor_set_anchor(gtcaca_editor_widget_t *w, int pos) { w->anchor = _clampi(pos, 0, w->length); }

void gtcaca_editor_set_selection(gtcaca_editor_widget_t *w, int caret, int anchor)
{
  w->current_pos = _clampi(caret, 0, w->length);
  w->anchor      = _clampi(anchor, 0, w->length);
  w->goal_col = -1;
}

void gtcaca_editor_set_empty_selection(gtcaca_editor_widget_t *w, int pos)
{
  _set_caret(w, pos, 0);
  w->goal_col = -1;
}

int gtcaca_editor_get_selection_start(gtcaca_editor_widget_t *w)
{
  return w->anchor < w->current_pos ? w->anchor : w->current_pos;
}
int gtcaca_editor_get_selection_end(gtcaca_editor_widget_t *w)
{
  return w->anchor > w->current_pos ? w->anchor : w->current_pos;
}

int gtcaca_editor_get_text_range(gtcaca_editor_widget_t *w, int start, int end, char *buf, int len)
{
  int n;
  start = _clampi(start, 0, w->length);
  end   = _clampi(end,   0, w->length);
  if (end < start) { int t = start; start = end; end = t; }
  n = end - start;
  if (n > len - 1) n = len - 1;
  if (n < 0) n = 0;
  memcpy(buf, w->text + start, (size_t)n);
  buf[n] = '\0';
  return n;
}

int gtcaca_editor_get_selected_text(gtcaca_editor_widget_t *w, char *buf, int len)
{
  return gtcaca_editor_get_text_range(w,
           gtcaca_editor_get_selection_start(w),
           gtcaca_editor_get_selection_end(w), buf, len);
}

void gtcaca_editor_goto_pos(gtcaca_editor_widget_t *w, int pos)
{
  _set_caret(w, pos, 0);
  w->goal_col = -1;
}
void gtcaca_editor_goto_line(gtcaca_editor_widget_t *w, int line)
{
  gtcaca_editor_goto_pos(w, gtcaca_editor_position_from_line(w, line));
}

/* ── movement key commands ─────────────────────────────────────────────────── */

static void _move_vertical(gtcaca_editor_widget_t *w, int delta, int extend)
{
  int last = gtcaca_editor_get_line_count(w) - 1;
  int dir = delta < 0 ? -1 : 1;
  int steps = delta < 0 ? -delta : delta;
  int target = gtcaca_editor_get_current_line(w);
  int s;

  /* Step over collapsed (hidden) lines so motion follows what is on screen. */
  for (s = 0; s < steps; s++) {
    int nl = target + dir;
    while (nl >= 0 && nl <= last && !gtcaca_editor_get_line_visible(w, nl)) nl += dir;
    if (nl < 0 || nl > last) break;
    target = nl;
  }
  if (w->goal_col < 0) w->goal_col = _visual_col(w, w->current_pos);
  _set_caret(w, _pos_from_visual_col(w, target, w->goal_col), extend);
}

#define _MOVE_H(name, newpos_expr)                                          \
  void gtcaca_editor_##name(gtcaca_editor_widget_t *w)                \
  { w->goal_col = -1; _set_caret(w, (newpos_expr), 0); }                     \
  void gtcaca_editor_##name##_extend(gtcaca_editor_widget_t *w)       \
  { w->goal_col = -1; _set_caret(w, (newpos_expr), 1); }

_MOVE_H(char_left,  w->current_pos > 0 ? w->current_pos - 1 : 0)
_MOVE_H(char_right, w->current_pos < w->length ? w->current_pos + 1 : w->length)
_MOVE_H(home, gtcaca_editor_position_from_line(w, gtcaca_editor_get_current_line(w)))
_MOVE_H(line_end, gtcaca_editor_get_line_end_position(w, gtcaca_editor_get_current_line(w)))
_MOVE_H(document_start, 0)
_MOVE_H(document_end, w->length)

void gtcaca_editor_line_up(gtcaca_editor_widget_t *w)          { _move_vertical(w, -1, 0); }
void gtcaca_editor_line_up_extend(gtcaca_editor_widget_t *w)   { _move_vertical(w, -1, 1); }
void gtcaca_editor_line_down(gtcaca_editor_widget_t *w)        { _move_vertical(w, +1, 0); }
void gtcaca_editor_line_down_extend(gtcaca_editor_widget_t *w) { _move_vertical(w, +1, 1); }

void gtcaca_editor_page_up(gtcaca_editor_widget_t *w)          { _move_vertical(w, -(w->view_lines > 0 ? w->view_lines : 10), 0); }
void gtcaca_editor_page_up_extend(gtcaca_editor_widget_t *w)   { _move_vertical(w, -(w->view_lines > 0 ? w->view_lines : 10), 1); }
void gtcaca_editor_page_down(gtcaca_editor_widget_t *w)        { _move_vertical(w, +(w->view_lines > 0 ? w->view_lines : 10), 0); }
void gtcaca_editor_page_down_extend(gtcaca_editor_widget_t *w) { _move_vertical(w, +(w->view_lines > 0 ? w->view_lines : 10), 1); }

/* ── editing key commands ──────────────────────────────────────────────────── */

static int _delete_selection(gtcaca_editor_widget_t *w)
{
  int a = gtcaca_editor_get_selection_start(w);
  int b = gtcaca_editor_get_selection_end(w);
  if (a == b) return 0;
  gtcaca_editor_delete_range(w, a, b - a);
  _set_caret(w, a, 0);
  return 1;
}

void gtcaca_editor_add_char(gtcaca_editor_widget_t *w, char c)
{
  char s[2]; s[0] = c; s[1] = '\0';
  if (!_delete_selection(w) && w->overtype &&
      w->current_pos < w->length && w->text[w->current_pos] != '\n')
    gtcaca_editor_delete_range(w, w->current_pos, 1);   /* overwrite the char */
  _insert_len(w, w->current_pos, s, 1);  /* caret auto-advances past it */
}

void gtcaca_editor_new_line(gtcaca_editor_widget_t *w)
{
  _delete_selection(w);
  _insert_len(w, w->current_pos, "\n", 1);
}

void gtcaca_editor_delete_back(gtcaca_editor_widget_t *w)
{
  if (_delete_selection(w)) return;
  if (w->current_pos > 0)
    gtcaca_editor_delete_range(w, w->current_pos - 1, 1);
}

void gtcaca_editor_clear(gtcaca_editor_widget_t *w)
{
  if (_delete_selection(w)) return;
  if (w->current_pos < w->length)
    gtcaca_editor_delete_range(w, w->current_pos, 1);
}

void gtcaca_editor_append_text(gtcaca_editor_widget_t *w, const char *s, int len)
{
  _insert_len(w, w->length, s, len);
}

/* ── undo / redo ───────────────────────────────────────────────────────────── */

void gtcaca_editor_undo(gtcaca_editor_widget_t *w)
{
  int group;
  if (w->undo_count == 0) return;
  /* a whole transaction (same non-zero group, contiguous on top) reverts at once */
  group = w->undo[w->undo_count - 1].group;
  do {
    gtcaca_editor_action_t a = w->undo[--w->undo_count];  /* take ownership of a.text */
    if (a.is_insert) {            /* text had been inserted → remove it */
      _raw_delete(w, a.pos, a.len);
      _set_caret(w, a.pos, 0);
    } else {                      /* text had been deleted → put it back */
      _raw_insert(w, a.pos, a.text, a.len);
      _set_caret(w, a.pos + a.len, 0);
    }
    _stack_push(&w->redo, &w->redo_count, &w->redo_cap, a.is_insert, a.pos, a.text, a.len, a.group);
    free(a.text);
  } while (group != 0 && w->undo_count > 0 && w->undo[w->undo_count - 1].group == group);
  w->modified = 1;
  w->goal_col = -1;
}

void gtcaca_editor_redo(gtcaca_editor_widget_t *w)
{
  int group;
  if (w->redo_count == 0) return;
  group = w->redo[w->redo_count - 1].group;
  do {
    gtcaca_editor_action_t a = w->redo[--w->redo_count];
    if (a.is_insert) {
      _raw_insert(w, a.pos, a.text, a.len);
      _set_caret(w, a.pos + a.len, 0);
    } else {
      _raw_delete(w, a.pos, a.len);
      _set_caret(w, a.pos, 0);
    }
    _stack_push(&w->undo, &w->undo_count, &w->undo_cap, a.is_insert, a.pos, a.text, a.len, a.group);
    free(a.text);
  } while (group != 0 && w->redo_count > 0 && w->redo[w->redo_count - 1].group == group);
  w->modified = 1;
  w->goal_col = -1;
}

int  gtcaca_editor_can_undo(gtcaca_editor_widget_t *w) { return w->undo_count > 0; }
int  gtcaca_editor_can_redo(gtcaca_editor_widget_t *w) { return w->redo_count > 0; }

void gtcaca_editor_empty_undo_buffer(gtcaca_editor_widget_t *w)
{
  _stack_clear(&w->undo, &w->undo_count, &w->undo_cap);
  _stack_clear(&w->redo, &w->redo_count, &w->redo_cap);
}

/* ── whole-document text & state ───────────────────────────────────────────── */

void gtcaca_editor_set_text(gtcaca_editor_widget_t *w, const char *text)
{
  int len = text ? (int)strlen(text) : 0;
  gtcaca_editor_empty_undo_buffer(w);
  _ensure_cap(w, len + 1);
  if (len) memcpy(w->text, text, (size_t)len);
  w->length = len;
  w->text[len] = '\0';
  w->current_pos = w->anchor = 0;
  w->first_visible_line = w->x_offset = 0;
  w->goal_col = -1;
  w->modified = 0;
  w->colorize_dirty = 1;
  gtcaca_editor_autoc_cancel(w);
  gtcaca_editor_annotation_clear_all(w);
  w->lines_meta_len = 0;   /* metadata is rebuilt for the new document */
  w->fold_dirty = 1;
}

int gtcaca_editor_get_text(gtcaca_editor_widget_t *w, char *buf, int len)
{
  int n = w->length;
  if (n > len - 1) n = len - 1;
  if (n < 0) n = 0;
  memcpy(buf, w->text, (size_t)n);
  buf[n] = '\0';
  return n;
}

int  gtcaca_editor_get_length(gtcaca_editor_widget_t *w) { return w->length; }

void gtcaca_editor_clear_all(gtcaca_editor_widget_t *w)
{
  if (w->length > 0) gtcaca_editor_delete_range(w, 0, w->length);
  _set_caret(w, 0, 0);
}

char gtcaca_editor_get_char_at(gtcaca_editor_widget_t *w, int pos)
{
  if (pos < 0 || pos >= w->length) return '\0';
  return w->text[pos];
}

void gtcaca_editor_set_save_point(gtcaca_editor_widget_t *w) { w->modified = 0; }
int  gtcaca_editor_get_modify(gtcaca_editor_widget_t *w)     { return w->modified; }
void gtcaca_editor_set_line_numbers(gtcaca_editor_widget_t *w, int on) { w->show_line_numbers = on ? 1 : 0; }
void gtcaca_editor_set_tab_width(gtcaca_editor_widget_t *w, int n) { w->tab_width = n > 0 ? n : 8; }
int  gtcaca_editor_get_tab_width(gtcaca_editor_widget_t *w) { return w->tab_width; }

int gtcaca_editor_key_cb_register(gtcaca_editor_widget_t *w, gtcaca_editor_key_cb_t cb, void *userdata)
{
  w->key_cb = cb;
  w->key_cb_userdata = userdata;
  return 0;
}

void gtcaca_editor_set_update_cb(gtcaca_editor_widget_t *w, gtcaca_editor_update_cb_t cb, void *userdata)
{
  w->update_cb = cb;
  w->update_cb_userdata = userdata;
}

/* ── default key bindings ──────────────────────────────────────────────────── */

static int _editor_private_key(gtcaca_editor_widget_t *w, int key, void *userdata)
{
  (void)userdata;
  switch (key) {
  case CACA_KEY_LEFT:      gtcaca_editor_char_left(w);   break;
  case CACA_KEY_RIGHT:     gtcaca_editor_char_right(w);  break;
  case CACA_KEY_UP:        gtcaca_editor_line_up(w);     break;
  case CACA_KEY_DOWN:      gtcaca_editor_line_down(w);   break;
  case CACA_KEY_HOME:      gtcaca_editor_home(w);        break;
  case CACA_KEY_END:       gtcaca_editor_line_end(w);    break;
  case CACA_KEY_PAGEUP:    gtcaca_editor_page_up(w);     break;
  case CACA_KEY_PAGEDOWN:  gtcaca_editor_page_down(w);   break;
  case CACA_KEY_RETURN:
  case 10:                 gtcaca_editor_new_line(w);    break;
  case CACA_KEY_BACKSPACE:                       /* 0x08 */
  case CACA_KEY_DELETE:    gtcaca_editor_delete_back(w); break;  /* 0x7f (mac backspace) */
  case CACA_KEY_TAB:       gtcaca_editor_add_char(w, '\t'); break;
  default:
    if (key >= 32 && key <= 126) gtcaca_editor_add_char(w, (char)key);
    break;
  }
  return 0;
}

/* ── colourization: styles & language config ───────────────────────────────── */

static void _set_style(gtcaca_editor_widget_t *w, int s, uint8_t fore, int back_set, uint8_t back)
{
  w->style_table[s].fore = fore;
  w->style_table[s].back = back;
  w->style_table[s].back_set = back_set;
  w->style_table[s].bold = 0;
  w->style_table[s].italic = 0;
  w->style_table[s].underline = 0;
  w->style_table[s].visible = 1;
}

static void _editor_default_styles(gtcaca_editor_widget_t *w)
{
  _set_style(w, GTCACA_EDITOR_STYLE_DEFAULT,  CACA_LIGHTGRAY, 0, 0);
  _set_style(w, GTCACA_EDITOR_STYLE_COMMENT,  CACA_GREEN,     0, 0);
  _set_style(w, GTCACA_EDITOR_STYLE_STRING,   CACA_BROWN,     0, 0);
  _set_style(w, GTCACA_EDITOR_STYLE_NUMBER,   CACA_MAGENTA,   0, 0);
  _set_style(w, GTCACA_EDITOR_STYLE_KEYWORD,  CACA_CYAN,      0, 0);
  _set_style(w, GTCACA_EDITOR_STYLE_BRACKET,  CACA_YELLOW,    0, 0);
  _set_style(w, GTCACA_EDITOR_STYLE_OPERATOR, CACA_LIGHTGRAY, 0, 0);
}

void gtcaca_editor_style_set_fore(gtcaca_editor_widget_t *w, int style, uint8_t color)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) { w->style_table[style].fore = color; w->colorize_dirty = 1; }
}
void gtcaca_editor_style_set_back(gtcaca_editor_widget_t *w, int style, uint8_t color)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) {
    w->style_table[style].back = color;
    w->style_table[style].back_set = 1;
    w->colorize_dirty = 1;
  }
}
void gtcaca_editor_style_clear_back(gtcaca_editor_widget_t *w, int style)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) w->style_table[style].back_set = 0;
}
void gtcaca_editor_style_set_bold(gtcaca_editor_widget_t *w, int style, int on)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) w->style_table[style].bold = on ? 1 : 0;
}
void gtcaca_editor_style_set_italic(gtcaca_editor_widget_t *w, int style, int on)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) w->style_table[style].italic = on ? 1 : 0;
}
void gtcaca_editor_style_set_underline(gtcaca_editor_widget_t *w, int style, int on)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) w->style_table[style].underline = on ? 1 : 0;
}
void gtcaca_editor_style_set_visible(gtcaca_editor_widget_t *w, int style, int on)
{
  if (style >= 0 && style < GTCACA_EDITOR_STYLE_COUNT) w->style_table[style].visible = on ? 1 : 0;
}
void gtcaca_editor_style_reset_default(gtcaca_editor_widget_t *w)
{
  _set_style(w, GTCACA_EDITOR_STYLE_DEFAULT, CACA_LIGHTGRAY, 0, 0);
  w->colorize_dirty = 1;
}
void gtcaca_editor_style_clear_all(gtcaca_editor_widget_t *w)
{
  int i;
  for (i = 0; i < GTCACA_EDITOR_STYLE_COUNT; i++) w->style_table[i] = w->style_table[GTCACA_EDITOR_STYLE_DEFAULT];
  w->colorize_dirty = 1;
}

/* ── margins ───────────────────────────────────────────────────────────────── */
int  gtcaca_editor_get_line_numbers(gtcaca_editor_widget_t *w) { return w->show_line_numbers; }
void gtcaca_editor_set_fold_margin(gtcaca_editor_widget_t *w, int on) { w->show_fold_margin = on ? 1 : 0; }
int  gtcaca_editor_get_fold_margin(gtcaca_editor_widget_t *w) { return w->show_fold_margin; }
void gtcaca_editor_set_selection_colors(gtcaca_editor_widget_t *w, uint8_t fore, uint8_t back)
{
  w->sel_fore = fore; w->sel_back = back; w->sel_set = 1;
}
void gtcaca_editor_colourize(gtcaca_editor_widget_t *w) { w->colorize_dirty = 1; }

void gtcaca_editor_set_langcfg(gtcaca_editor_widget_t *w, gtcaca_editor_langcfg_t *cfg)
{
  w->langcfg = cfg;
  w->colorize_enabled = (cfg != NULL);
  w->colorize_dirty = 1;
}

/* ── construction / teardown ───────────────────────────────────────────────── */

gtcaca_editor_widget_t *gtcaca_editor_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_editor_widget_t *w = calloc(1, sizeof(*w));
  if (!w) { fprintf(stderr, "Error: Cannot allocate editor\n"); return NULL; }

  w->id = gtcaca_get_newid();
  w->has_focus = 0;
  w->is_visible = 1;
  w->type = GTCACA_WIDGET_EDITOR;
  w->parent = parent;
  w->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(w), x, y);
  w->width  = width;
  w->height = height;

  w->color_focus_fg = gmo.theme.textviewfocus.fg;
  w->color_focus_bg = gmo.theme.textviewfocus.bg;
  w->color_nonfocus_fg = gmo.theme.textview.fg;
  w->color_nonfocus_bg = gmo.theme.textview.bg;

  _ensure_cap(w, 1);
  w->text[0] = '\0';
  w->length = 0;
  w->current_pos = w->anchor = 0;
  w->goal_col = -1;
  w->first_visible_line = w->x_offset = 0;
  w->view_lines = w->view_cols = 0;
  w->tab_width = 8;
  w->show_line_numbers = 1;
  w->modified = 0;

  w->private_key_cb = _editor_private_key;
  w->key_cb = NULL;
  w->key_cb_userdata = NULL;
  w->update_cb = NULL;
  w->update_cb_userdata = NULL;

  /* Colourization: off until a language config is attached. Default style
     colours roughly follow common editor themes. */
  w->langcfg = NULL;
  w->colorize_enabled = 0;
  w->colorize_dirty = 1;
  w->styles = NULL;
  w->styles_cap = 0;
  w->sel_set = 0;
  _editor_default_styles(w);

  /* Autocompletion: inactive, with Scintilla-like defaults. */
  w->autoc.separator = ' ';
  w->autoc.auto_hide = 1;
  w->autoc.cancel_at_start = 1;
  w->autoc.max_height = 9;
  w->autoc_cb = NULL;

  /* Modes / display options / brace highlight */
  w->read_only = w->overtype = w->view_ws = w->caret_line_visible = 0;
  w->caret_line_back_set = 0;
  w->caret_line_back = CACA_BLUE;
  w->edge_column = 0;
  w->edge_colour = CACA_DARKGRAY;
  w->brace_hl1 = w->brace_hl2 = w->brace_bad = -1;

  /* Margins / folding / annotations */
  w->show_fold_margin = 0;
  w->annotation_visible = GTCACA_EDITOR_ANNOTATION_STANDARD;
  w->lines_meta = NULL;
  w->lines_meta_cap = 0;
  w->lines_meta_len = 0;
  w->fold_dirty = 1;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(w));
  return w;
}

void gtcaca_editor_free(gtcaca_editor_widget_t *w)
{
  if (!w) return;
  gtcaca_editor_empty_undo_buffer(w);
  gtcaca_editor_autoc_cancel(w);
  free(w->autoc.fillups);
  free(w->autoc.stops);
  gtcaca_editor_annotation_clear_all(w);
  free(w->lines_meta);
  free(w->undo);
  free(w->redo);
  free(w->styles);
  free(w->text);
  free(w);
}

/* ── drawing ───────────────────────────────────────────────────────────────── */

static int _digits(int n) { int d = 1; while (n >= 10) { n /= 10; d++; } return d; }

/* style flags → caca attribute mask */
static uint32_t _style_attr(gtcaca_editor_style_t *st)
{
  return (st->bold ? CACA_BOLD : 0) | (st->italic ? CACA_ITALICS : 0) |
         (st->underline ? CACA_UNDERLINE : 0);
}

/* number of display rows occupied by visible lines in [from, to) (incl. annotations) */
static int _count_rows(gtcaca_editor_widget_t *w, int from, int to)
{
  int line, rows = 0;
  for (line = from; line < to; line++) {
    if (!gtcaca_editor_get_line_visible(w, line)) continue;
    rows += 1 + gtcaca_editor_annotation_get_lines(w, line);
  }
  return rows;
}

void gtcaca_editor_draw(gtcaca_editor_widget_t *w)
{
  uint8_t nf = w->has_focus ? w->color_focus_fg : w->color_nonfocus_fg;
  uint8_t nb = w->has_focus ? w->color_focus_bg : w->color_nonfocus_bg;
  int text_h  = w->height - 2;
  int total_w = w->width  - 2;
  int line_count = gtcaca_editor_get_line_count(w);
  int ln_w   = w->show_line_numbers ? _digits(line_count) + 1 : 0;
  int fold_w = w->show_fold_margin ? 1 : 0;
  int margin_w = ln_w + fold_w;
  int text_w  = total_w - margin_w;
  int sel_start = gtcaca_editor_get_selection_start(w);
  int sel_end   = gtcaca_editor_get_selection_end(w);
  int caret_line = gtcaca_editor_get_current_line(w);
  /* rectangular selection box (visual columns), when active */
  int r_top = 0, r_bot = -1, r_left = 0, r_right = 0;   /* r_bot < r_top = inactive */
  if (w->rect_select) {
    int la = gtcaca_editor_line_from_position(w, w->anchor);
    int lc = gtcaca_editor_line_from_position(w, w->current_pos);
    int ca = gtcaca_editor_get_column(w, w->anchor);
    int cc = gtcaca_editor_get_column(w, w->current_pos);
    r_top = la < lc ? la : lc;  r_bot = la < lc ? lc : la;
    r_left = ca < cc ? ca : cc; r_right = ca < cc ? cc : ca;
    if (r_right == r_left) r_right = r_left + 1;   /* zero-width: show a 1-col bar */
  }
  int inner_x = w->x + 1, inner_y = w->y + 1, origin_x;
  int row, line;
  /* Margin colours: use the non-focus pair so the digits stay legible even
     when the focused editor background is dark. */
  uint8_t mfg = w->color_nonfocus_fg;
  uint8_t mbg = w->color_nonfocus_bg;

  /* frame */
  gtcaca_widget_colorize(GTCACA_WIDGET(w));
  caca_fill_box(gmo.cv, w->x, w->y, w->width, w->height, ' ');
  caca_draw_cp437_box(gmo.cv, w->x, w->y, w->width, w->height);
  caca_set_attr(gmo.cv, 0);
  if (text_h <= 0 || text_w <= 0) return;

  _gtcaca_editor_ensure_meta(w);
  if (w->colorize_dirty) {
    if (w->json_mode)             _gtcaca_editor_colorize_json(w);
    else if (w->grammar)          _gtcaca_editor_colorize_tm(w);
    else if (w->colorize_enabled) _gtcaca_editor_colorize(w);
    w->colorize_dirty = 0;
  }
  origin_x = inner_x + margin_w;

  /* keep the caret on screen, counting folded-away lines and annotations */
  w->view_lines = text_h;
  w->view_cols  = text_w;
  if (!gtcaca_editor_get_line_visible(w, caret_line)) {
    while (caret_line > 0 && !gtcaca_editor_get_line_visible(w, caret_line)) caret_line--;
  }
  if (w->first_visible_line < 0) w->first_visible_line = 0;
  if (w->first_visible_line >= line_count) w->first_visible_line = line_count - 1;
  if (caret_line < w->first_visible_line) w->first_visible_line = caret_line;
  while (w->first_visible_line > 0 && !gtcaca_editor_get_line_visible(w, w->first_visible_line))
    w->first_visible_line--;
  while (_count_rows(w, w->first_visible_line, caret_line) >= text_h) {
    int nl = w->first_visible_line + 1;
    while (nl < line_count && !gtcaca_editor_get_line_visible(w, nl)) nl++;
    if (nl >= line_count) break;
    w->first_visible_line = nl;
  }
  {
    int caret_col = _visual_col(w, w->current_pos);
    if (caret_col < w->x_offset) w->x_offset = caret_col;
    if (caret_col >= w->x_offset + text_w) w->x_offset = caret_col - text_w + 1;
    if (w->x_offset < 0) w->x_offset = 0;
  }

  /* walk visible document lines, interleaving annotation rows */
  row = 0;
  line = w->first_visible_line;
  while (line < line_count && !gtcaca_editor_get_line_visible(w, line)) line++;

  while (row < text_h && line < line_count) {
    int draw_y = inner_y + row;
    int line_start = gtcaca_editor_position_from_line(w, line);
    int line_end   = gtcaca_editor_get_line_end_position(w, line);
    int pos, vcol;

    /* line-number margin */
    if (ln_w > 0) {
      char numbuf[16];
      snprintf(numbuf, sizeof numbuf, "%*d", ln_w - 1, line + 1);
      caca_set_color_ansi(gmo.cv, mfg, mbg);
      caca_set_attr(gmo.cv, 0);
      caca_printf(gmo.cv, inner_x, draw_y, "%s", numbuf);
      caca_put_char(gmo.cv, inner_x + ln_w - 1, draw_y, ' ');
    }
    /* fold margin: +/- on headers */
    if (fold_w > 0) {
      int fl = gtcaca_editor_get_fold_level(w, line);
      char mk = (fl & GTCACA_EDITOR_FOLDLEVELHEADERFLAG)
                  ? (gtcaca_editor_get_fold_expanded(w, line) ? '-' : '+') : ' ';
      caca_set_color_ansi(gmo.cv, CACA_YELLOW, mbg);
      caca_set_attr(gmo.cv, 0);
      caca_put_char(gmo.cv, inner_x + ln_w, draw_y, mk);
    }

    /* current-line highlight: paint the row background first */
    uint8_t rowbg = (w->caret_line_visible && line == caret_line && w->has_focus)
                      ? w->caret_line_back : nb;
    if (rowbg != nb) {
      int cx;
      caca_set_color_ansi(gmo.cv, nf, rowbg); caca_set_attr(gmo.cv, 0);
      for (cx = 0; cx < text_w; cx++) caca_put_char(gmo.cv, origin_x + cx, draw_y, ' ');
    }

    /* line text, expanding tabs and applying horizontal scroll */
    vcol = 0;
    for (pos = line_start; pos < line_end; pos++) {
      char c = w->text[pos];
      int cells = (c == '\t') ? (w->tab_width - (vcol % w->tab_width)) : 1;
      int k;
      for (k = 0; k < cells; k++) {
        int vc = vcol + k;
        if (vc >= w->x_offset && vc < w->x_offset + text_w) {
          int sx = origin_x + (vc - w->x_offset);
          int in_sel   = w->rect_select
                           ? (line >= r_top && line <= r_bot && vc >= r_left && vc < r_right)
                           : (pos >= sel_start && pos < sel_end);
          int is_caret = (w->has_focus && pos == w->current_pos && k == 0);
          int is_brace = (pos == w->brace_hl1 || pos == w->brace_hl2);
          int is_bad   = (pos == w->brace_bad);
          uint32_t outc = (c == '\t') ? ' ' : (uint32_t)(unsigned char)c;
          if (is_caret) {
            caca_set_color_ansi(gmo.cv, nb, nf); caca_set_attr(gmo.cv, 0);
          } else if (in_sel) {
            caca_set_color_ansi(gmo.cv, w->sel_set ? w->sel_fore : nb, w->sel_set ? w->sel_back : nf);
            caca_set_attr(gmo.cv, 0);
          } else if (is_bad) {
            caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_RED);   caca_set_attr(gmo.cv, CACA_BOLD);
          } else if (is_brace) {
            caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_GREEN); caca_set_attr(gmo.cv, CACA_BOLD);
          } else if (w->view_ws && (c == ' ' || c == '\t')) {
            caca_set_color_ansi(gmo.cv, CACA_DARKGRAY, rowbg); caca_set_attr(gmo.cv, 0);
            outc = (c == '\t') ? (k == 0 ? 0xBB : (uint32_t)' ') : 0xB7;  /* » and · */
          } else if ((w->colorize_enabled || w->json_mode || w->grammar) && w->styles && pos < w->length) {
            gtcaca_editor_style_t *st = &w->style_table[w->styles[pos]];
            caca_set_color_ansi(gmo.cv, st->fore, st->back_set ? st->back : rowbg);
            caca_set_attr(gmo.cv, _style_attr(st));
            if (!st->visible) outc = ' ';
          } else {
            caca_set_color_ansi(gmo.cv, nf, rowbg); caca_set_attr(gmo.cv, 0);
          }
          caca_put_char(gmo.cv, sx, draw_y, outc);
        }
      }
      vcol += cells;
    }

    /* rectangular selection over virtual space: highlight box columns that lie
       past this line's text (and the 1-col bar for a zero-width box). */
    if (w->rect_select && line >= r_top && line <= r_bot) {
      int vc;
      for (vc = (vcol > r_left ? vcol : r_left); vc < r_right; vc++) {
        if (vc >= w->x_offset && vc < w->x_offset + text_w) {
          caca_set_color_ansi(gmo.cv, w->sel_set ? w->sel_fore : nb,
                                      w->sel_set ? w->sel_back : nf);
          caca_set_attr(gmo.cv, 0);
          caca_put_char(gmo.cv, origin_x + (vc - w->x_offset), draw_y, ' ');
        }
      }
    }

    /* edge column marker, where the line is shorter than the edge */
    if (w->edge_column > 0 && w->edge_column >= vcol &&
        w->edge_column >= w->x_offset && w->edge_column < w->x_offset + text_w) {
      caca_set_color_ansi(gmo.cv, w->edge_colour, rowbg); caca_set_attr(gmo.cv, 0);
      caca_put_char(gmo.cv, origin_x + (w->edge_column - w->x_offset), draw_y, 0x2502); /* │ */
    }
    /* caret at end-of-line */
    if (w->has_focus && caret_line == line && w->current_pos == line_end) {
      int vc = vcol;
      if (vc >= w->x_offset && vc < w->x_offset + text_w) {
        caca_set_color_ansi(gmo.cv, nb, nf); caca_set_attr(gmo.cv, 0);
        caca_put_char(gmo.cv, origin_x + (vc - w->x_offset), draw_y, ' ');
      }
    }
    row++;

    /* annotation rows beneath this line */
    if (w->annotation_visible != GTCACA_EDITOR_ANNOTATION_HIDDEN &&
        line < w->lines_meta_len && w->lines_meta[line].annotation) {
      gtcaca_editor_style_t *st = &w->style_table[w->lines_meta[line].annotation_style];
      const char *p = w->lines_meta[line].annotation;
      while (*p && row < text_h) {
        const char *nl = strchr(p, '\n');
        int seg = nl ? (int)(nl - p) : (int)strlen(p);
        int col;
        caca_set_color_ansi(gmo.cv, st->fore, st->back_set ? st->back : nb);
        caca_set_attr(gmo.cv, _style_attr(st));
        for (col = 0; col < text_w; col++)
          caca_put_char(gmo.cv, origin_x + col, inner_y + row, col < seg ? p[col] : ' ');
        row++;
        if (!nl) break;
        p = nl + 1;
      }
    }

    /* advance to the next visible line */
    line++;
    while (line < line_count && !gtcaca_editor_get_line_visible(w, line)) line++;
  }

  caca_set_attr(gmo.cv, 0);   /* don't leak bold/italic to other widgets */

  /* autocompletion pop-up draws on top of the text */
  if (w->autoc.active) _gtcaca_editor_draw_autoc(w);
}
