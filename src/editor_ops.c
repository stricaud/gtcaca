/*
 * editor_ops.c — additional editing operations modelled on Scintilla: word
 * movement/deletion, brace matching, line commands, case conversion, and the
 * read-only / overtype modes plus a few display toggles.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gtcaca/editor.h>

static int _is_word(int c) { return isalnum(c) || c == '_' || c == '$'; }

/* ── word positions & movement ─────────────────────────────────────────────── */

int gtcaca_editor_word_right_position(gtcaca_editor_widget_t *w, int pos)
{
  int p = pos, len = w->length;
  while (p < len && !_is_word((unsigned char)w->text[p])) p++;   /* skip gap   */
  while (p < len && _is_word((unsigned char)w->text[p]))  p++;   /* skip word  */
  return p;
}

int gtcaca_editor_word_left_position(gtcaca_editor_widget_t *w, int pos)
{
  int p = pos;
  while (p > 0 && !_is_word((unsigned char)w->text[p - 1])) p--;
  while (p > 0 && _is_word((unsigned char)w->text[p - 1]))  p--;
  return p;
}

static void _word_move(gtcaca_editor_widget_t *w, int newpos, int extend)
{
  w->goal_col = -1;
  newpos = newpos < 0 ? 0 : (newpos > w->length ? w->length : newpos);
  w->current_pos = newpos;
  if (!extend) w->anchor = newpos;
}

void gtcaca_editor_word_left(gtcaca_editor_widget_t *w)        { _word_move(w, gtcaca_editor_word_left_position(w, w->current_pos), 0); }
void gtcaca_editor_word_left_extend(gtcaca_editor_widget_t *w) { _word_move(w, gtcaca_editor_word_left_position(w, w->current_pos), 1); }
void gtcaca_editor_word_right(gtcaca_editor_widget_t *w)        { _word_move(w, gtcaca_editor_word_right_position(w, w->current_pos), 0); }
void gtcaca_editor_word_right_extend(gtcaca_editor_widget_t *w) { _word_move(w, gtcaca_editor_word_right_position(w, w->current_pos), 1); }

void gtcaca_editor_del_word_right(gtcaca_editor_widget_t *w)
{
  int to = gtcaca_editor_word_right_position(w, w->current_pos);
  if (to > w->current_pos) gtcaca_editor_delete_range(w, w->current_pos, to - w->current_pos);
}

void gtcaca_editor_del_word_left(gtcaca_editor_widget_t *w)
{
  int from = gtcaca_editor_word_left_position(w, w->current_pos);
  if (from < w->current_pos) gtcaca_editor_delete_range(w, from, w->current_pos - from);
}

/* ── brace matching ────────────────────────────────────────────────────────── */

static int _brace_partner(char c, int *dir)
{
  switch (c) {
  case '(': *dir = 1;  return ')';
  case '[': *dir = 1;  return ']';
  case '{': *dir = 1;  return '}';
  case ')': *dir = -1; return '(';
  case ']': *dir = -1; return '[';
  case '}': *dir = -1; return '{';
  }
  return 0;
}

/* Ignore braces sitting in string/comment styles when colourization is active. */
static int _brace_styled_out(gtcaca_editor_widget_t *w, int pos)
{
  if (!w->styles || pos >= w->styles_cap) return 0;
  return w->styles[pos] == GTCACA_EDITOR_STYLE_STRING ||
         w->styles[pos] == GTCACA_EDITOR_STYLE_COMMENT;
}

int gtcaca_editor_brace_match(gtcaca_editor_widget_t *w, int pos)
{
  int dir, depth = 1, p;
  char c, partner;
  if (pos < 0 || pos >= w->length) return -1;
  c = w->text[pos];
  partner = (char)_brace_partner(c, &dir);
  if (!partner) return -1;
  for (p = pos + dir; p >= 0 && p < w->length; p += dir) {
    char d = w->text[p];
    if (_brace_styled_out(w, p)) continue;
    if (d == c)        depth++;
    else if (d == partner && --depth == 0) return p;
  }
  return -1;
}

void gtcaca_editor_brace_highlight(gtcaca_editor_widget_t *w, int pos1, int pos2)
{
  w->brace_hl1 = pos1; w->brace_hl2 = pos2; w->brace_bad = -1;
}
void gtcaca_editor_brace_badlight(gtcaca_editor_widget_t *w, int pos)
{
  w->brace_bad = pos; w->brace_hl1 = w->brace_hl2 = -1;
}

/* ── line commands ─────────────────────────────────────────────────────────── */

void gtcaca_editor_line_duplicate(gtcaca_editor_widget_t *w)
{
  int line = gtcaca_editor_get_current_line(w);
  int s = gtcaca_editor_position_from_line(w, line);
  int e = gtcaca_editor_get_line_end_position(w, line);
  int n = e - s, caret = w->current_pos;
  char *buf = malloc((size_t)n + 2);
  if (!buf) return;
  buf[0] = '\n';
  gtcaca_editor_get_text_range(w, s, e, buf + 1, n + 1);
  gtcaca_editor_set_empty_selection(w, e);
  gtcaca_editor_insert_text(w, e, buf);     /* "\n<line>" after the current one */
  gtcaca_editor_set_empty_selection(w, caret);
  free(buf);
}

void gtcaca_editor_line_delete(gtcaca_editor_widget_t *w)
{
  int line = gtcaca_editor_get_current_line(w);
  int s = gtcaca_editor_position_from_line(w, line);
  int e = gtcaca_editor_get_line_end_position(w, line);
  if (e < w->length) e++;                   /* take the trailing newline too */
  else if (s > 0)    s--;                    /* last line: take the leading one */
  gtcaca_editor_delete_range(w, s, e - s);
  gtcaca_editor_set_empty_selection(w, s);
}

void gtcaca_editor_line_transpose(gtcaca_editor_widget_t *w)
{
  int line = gtcaca_editor_get_current_line(w);
  int s1, e1, s2, e2, len1, len2, col;
  char *l1, *l2;
  if (line == 0) return;
  s1 = gtcaca_editor_position_from_line(w, line - 1);
  e1 = gtcaca_editor_get_line_end_position(w, line - 1);
  s2 = gtcaca_editor_position_from_line(w, line);
  e2 = gtcaca_editor_get_line_end_position(w, line);
  len1 = e1 - s1; len2 = e2 - s2;
  col = w->current_pos - s2;
  l1 = malloc((size_t)len1 + 1); l2 = malloc((size_t)len2 + 1);
  if (l1 && l2) {
    gtcaca_editor_get_text_range(w, s1, e1, l1, len1 + 1);
    gtcaca_editor_get_text_range(w, s2, e2, l2, len2 + 1);
    gtcaca_editor_delete_range(w, s1, e2 - s1);          /* both lines + the gap */
    gtcaca_editor_insert_text(w, s1, l2);
    gtcaca_editor_insert_text(w, s1 + len2, "\n");
    gtcaca_editor_insert_text(w, s1 + len2 + 1, l1);
    gtcaca_editor_set_empty_selection(w, s1 + col);       /* keep caret on its text */
  }
  free(l1); free(l2);
}

void gtcaca_editor_selection_duplicate(gtcaca_editor_widget_t *w)
{
  int a = gtcaca_editor_get_selection_start(w);
  int b = gtcaca_editor_get_selection_end(w);
  if (a == b) { gtcaca_editor_line_duplicate(w); return; }
  {
    int n = b - a;
    char *buf = malloc((size_t)n + 1);
    if (buf) { gtcaca_editor_get_text_range(w, a, b, buf, n + 1); gtcaca_editor_insert_text(w, b, buf); free(buf); }
  }
}

/* ── case conversion (the selection) ──────────────────────────────────────── */

static void _case_selection(gtcaca_editor_widget_t *w, int upper)
{
  int a = gtcaca_editor_get_selection_start(w);
  int b = gtcaca_editor_get_selection_end(w);
  int n = b - a, i;
  char *buf;
  if (n <= 0) return;
  buf = malloc((size_t)n + 1);
  if (!buf) return;
  gtcaca_editor_get_text_range(w, a, b, buf, n + 1);
  for (i = 0; i < n; i++) buf[i] = (char)(upper ? toupper((unsigned char)buf[i]) : tolower((unsigned char)buf[i]));
  gtcaca_editor_delete_range(w, a, n);
  gtcaca_editor_insert_text(w, a, buf);
  gtcaca_editor_set_selection(w, a + n, a);
  free(buf);
}
void gtcaca_editor_upper_case(gtcaca_editor_widget_t *w) { _case_selection(w, 1); }
void gtcaca_editor_lower_case(gtcaca_editor_widget_t *w) { _case_selection(w, 0); }

/* ── modes & display options ──────────────────────────────────────────────── */

void gtcaca_editor_set_read_only(gtcaca_editor_widget_t *w, int on) { w->read_only = on ? 1 : 0; }
int  gtcaca_editor_get_read_only(gtcaca_editor_widget_t *w) { return w->read_only; }
void gtcaca_editor_set_overtype(gtcaca_editor_widget_t *w, int on) { w->overtype = on ? 1 : 0; }
int  gtcaca_editor_get_overtype(gtcaca_editor_widget_t *w) { return w->overtype; }

void gtcaca_editor_set_view_whitespace(gtcaca_editor_widget_t *w, int on) { w->view_ws = on ? 1 : 0; }
int  gtcaca_editor_get_view_whitespace(gtcaca_editor_widget_t *w) { return w->view_ws; }
void gtcaca_editor_set_caret_line_visible(gtcaca_editor_widget_t *w, int on) { w->caret_line_visible = on ? 1 : 0; }
void gtcaca_editor_set_caret_line_back(gtcaca_editor_widget_t *w, uint8_t colour) { w->caret_line_back = colour; w->caret_line_back_set = 1; }
void gtcaca_editor_set_edge_column(gtcaca_editor_widget_t *w, int column) { w->edge_column = column; }
int  gtcaca_editor_get_edge_column(gtcaca_editor_widget_t *w) { return w->edge_column; }
void gtcaca_editor_set_edge_colour(gtcaca_editor_widget_t *w, uint8_t colour) { w->edge_colour = colour; }

/* ── rectangular (column-box) operations ───────────────────────────────────────
 * The box has the caret and anchor as opposite corners; columns are visual
 * (tabs expanded). Ops loop the spanned lines, editing the fixed column range.
 * A shared clipboard holds a killed rectangle (one string per line), Emacs-style.
 */

static char **g_rect_kill   = NULL;
static int    g_rect_kill_n = 0;

static void _rect_bounds(gtcaca_editor_widget_t *w,
                         int *top, int *bot, int *left, int *right)
{
  int la = gtcaca_editor_line_from_position(w, w->anchor);
  int lc = gtcaca_editor_line_from_position(w, w->current_pos);
  int ca = gtcaca_editor_get_column(w, w->anchor);
  int cc = gtcaca_editor_get_column(w, w->current_pos);
  *top  = la < lc ? la : lc;  *bot   = la < lc ? lc : la;
  *left = ca < cc ? ca : cc;  *right = ca < cc ? cc : ca;
}

/* byte range [*p0,*p1) covering visual columns [left,right) on `line` (clamped
   to the line end where the line is shorter than the column). */
static void _rect_line_span(gtcaca_editor_widget_t *w, int line,
                            int left, int right, int *p0, int *p1)
{
  int end   = gtcaca_editor_get_line_end_position(w, line);
  int width = gtcaca_editor_get_column(w, end);
  *p0 = (left  <= width) ? gtcaca_editor_find_column(w, line, left)  : end;
  *p1 = (right <= width) ? gtcaca_editor_find_column(w, line, right) : end;
  if (*p1 < *p0) *p1 = *p0;
}

/* pad `line` with spaces up to visual column `col`; return the byte pos there */
static int _rect_pad_to(gtcaca_editor_widget_t *w, int line, int col)
{
  int end   = gtcaca_editor_get_line_end_position(w, line);
  int width = gtcaca_editor_get_column(w, end);
  if (width >= col) return gtcaca_editor_find_column(w, line, col);
  {
    char sp[129]; int n = col - width;
    while (n > 0) {
      int c = n > 128 ? 128 : n, i;
      for (i = 0; i < c; i++) sp[i] = ' ';
      sp[c] = '\0';
      gtcaca_editor_insert_text(w, end, sp);
      end += c; n -= c;
    }
  }
  return end;
}

static void _rect_insert_spaces(gtcaca_editor_widget_t *w, int at, int n)
{
  char sp[129];
  while (n > 0) {
    int c = n > 128 ? 128 : n, i;
    for (i = 0; i < c; i++) sp[i] = ' ';
    sp[c] = '\0';
    gtcaca_editor_insert_text(w, at, sp);
    at += c; n -= c;
  }
}

static void _rect_free_kill(void)
{
  int i;
  for (i = 0; i < g_rect_kill_n; i++) free(g_rect_kill[i]);
  free(g_rect_kill); g_rect_kill = NULL; g_rect_kill_n = 0;
}

/* copy each spanned line's box text into the rectangle clipboard (no edit) */
static void _rect_save(gtcaca_editor_widget_t *w)
{
  int top, bot, left, right, line;
  _rect_bounds(w, &top, &bot, &left, &right);
  _rect_free_kill();
  g_rect_kill_n = bot - top + 1;
  g_rect_kill = calloc((size_t)g_rect_kill_n, sizeof(char *));
  for (line = top; line <= bot; line++) {
    int p0, p1, n; char *s;
    _rect_line_span(w, line, left, right, &p0, &p1);
    n = p1 - p0;
    s = malloc((size_t)n + 1);
    gtcaca_editor_get_text_range(w, p0, p1, s, n + 1);
    g_rect_kill[line - top] = s;
  }
}

/* delete the box on every spanned line (bottom-to-top so upper positions stay
   valid); if `save`, first copy it into the rectangle clipboard. */
static void _rect_remove(gtcaca_editor_widget_t *w, int save)
{
  int top, bot, left, right, line;
  if (save) _rect_save(w);
  _rect_bounds(w, &top, &bot, &left, &right);
  for (line = bot; line >= top; line--) {
    int p0, p1;
    _rect_line_span(w, line, left, right, &p0, &p1);
    if (p1 > p0) gtcaca_editor_delete_range(w, p0, p1 - p0);
  }
}

/* every multi-line rectangle edit is one undo step (begin/end_undo_action) */

void gtcaca_editor_rect_delete(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_begin_undo_action(w); _rect_remove(w, 0); gtcaca_editor_end_undo_action(w);
  w->rect_select = 0;
}
void gtcaca_editor_rect_kill(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_begin_undo_action(w); _rect_remove(w, 1); gtcaca_editor_end_undo_action(w);
  w->rect_select = 0;
}
void gtcaca_editor_rect_copy(gtcaca_editor_widget_t *w) { _rect_save(w); w->rect_select = 0; }

void gtcaca_editor_rect_clear(gtcaca_editor_widget_t *w)
{
  int top, bot, left, right, line, width;
  _rect_bounds(w, &top, &bot, &left, &right);
  width = right - left;
  gtcaca_editor_begin_undo_action(w);
  for (line = bot; line >= top; line--) {
    int p0, p1, at;
    _rect_line_span(w, line, left, right, &p0, &p1);
    if (p1 > p0) gtcaca_editor_delete_range(w, p0, p1 - p0);
    if (width > 0) { at = _rect_pad_to(w, line, left); _rect_insert_spaces(w, at, width); }
  }
  gtcaca_editor_end_undo_action(w);
  w->rect_select = 0;
}

void gtcaca_editor_rect_string_insert(gtcaca_editor_widget_t *w, const char *s)
{
  int top, bot, left, right, line;
  _rect_bounds(w, &top, &bot, &left, &right);
  gtcaca_editor_begin_undo_action(w);
  for (line = bot; line >= top; line--) {
    int p0, p1, at;
    _rect_line_span(w, line, left, right, &p0, &p1);
    if (p1 > p0) gtcaca_editor_delete_range(w, p0, p1 - p0);  /* replace the box */
    at = _rect_pad_to(w, line, left);                         /* pad short lines */
    if (s && *s) gtcaca_editor_insert_text(w, at, s);
  }
  gtcaca_editor_end_undo_action(w);
  w->rect_select = 0;
}

void gtcaca_editor_rect_yank(gtcaca_editor_widget_t *w)
{
  int cl  = gtcaca_editor_line_from_position(w, w->current_pos);
  int col = gtcaca_editor_get_column(w, w->current_pos);
  int i;
  if (g_rect_kill_n == 0) return;
  gtcaca_editor_begin_undo_action(w);
  for (i = 0; i < g_rect_kill_n; i++) {
    int line = cl + i, at;
    while (line >= gtcaca_editor_get_line_count(w))            /* grow the buffer */
      gtcaca_editor_insert_text(w, w->length, "\n");
    at = _rect_pad_to(w, line, col);
    if (g_rect_kill[i] && g_rect_kill[i][0]) gtcaca_editor_insert_text(w, at, g_rect_kill[i]);
  }
  gtcaca_editor_end_undo_action(w);
  w->rect_select = 0;
}
