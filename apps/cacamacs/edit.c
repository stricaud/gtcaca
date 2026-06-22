#include "cacamacs.h"
/* ── kill ring (single register) ───────────────────────────────────────────── */

void kill_set(const char *text, int len)
{
  free(g_killbuf);
  g_killbuf = malloc((size_t)len + 1);
  if (g_killbuf) { memcpy(g_killbuf, text, (size_t)len); g_killbuf[len] = '\0'; }
}

/* ── commands ──────────────────────────────────────────────────────────────── */


/* Move, extending the selection instead when the mark is active. */
void move(gtcaca_editor_widget_t *ed, mv_t plain, mv_t extend)
{
  if (g_mark_active) extend(ed);
  else               plain(ed);
}

void set_mark(gtcaca_editor_widget_t *ed)
{
  gtcaca_editor_set_rectangular_selection(ed, 0);   /* plain mark = linear region */
  gtcaca_editor_set_empty_selection(ed, gtcaca_editor_get_current_pos(ed));
  g_mark_active = 1;
  snprintf(g_message, sizeof(g_message), "Mark set");
}

/* C-x SPC — like set-mark, but the region is a rectangular column box */
void set_rectangle_mark(gtcaca_editor_widget_t *ed)
{
  gtcaca_editor_set_empty_selection(ed, gtcaca_editor_get_current_pos(ed));
  gtcaca_editor_set_rectangular_selection(ed, 1);
  g_mark_active = 1;
  snprintf(g_message, sizeof(g_message), "Rectangle mark set — move, then C-x r t/k/y/d/c");
}

void keyboard_quit(gtcaca_editor_widget_t *ed)
{
  gtcaca_editor_set_rectangular_selection(ed, 0);
  gtcaca_editor_set_empty_selection(ed, gtcaca_editor_get_current_pos(ed));
  g_mark_active = 0;
  snprintf(g_message, sizeof(g_message), "Quit");
}

void kill_region(gtcaca_editor_widget_t *ed)
{
  int a, b;
  if (gtcaca_editor_get_rectangular_selection(ed)) {   /* C-w on a rectangle */
    gtcaca_editor_rect_kill(ed);
    g_mark_active = 0;
    snprintf(g_message, sizeof g_message, "Killed rectangle");
    return;
  }
  a = gtcaca_editor_get_selection_start(ed);
  b = gtcaca_editor_get_selection_end(ed);
  if (a == b) { snprintf(g_message, sizeof(g_message), "No region"); return; }
  {
    int n = b - a;
    char *buf = malloc((size_t)n + 1);
    if (buf) { gtcaca_editor_get_text_range(ed, a, b, buf, n + 1); kill_set(buf, n); free(buf); }
  }
  gtcaca_editor_delete_range(ed, a, b - a);
  gtcaca_editor_set_empty_selection(ed, a);
  g_mark_active = 0;
}

/* M-w: copy the region to the kill ring without deleting it. */
void copy_region(gtcaca_editor_widget_t *ed)
{
  int a, b;
  if (gtcaca_editor_get_rectangular_selection(ed)) {   /* M-w on a rectangle */
    gtcaca_editor_rect_copy(ed);                        /* -> rectangle clipboard (C-x r y) */
    g_mark_active = 0;
    snprintf(g_message, sizeof g_message, "Copied rectangle");
    return;
  }
  a = gtcaca_editor_get_selection_start(ed);
  b = gtcaca_editor_get_selection_end(ed);
  if (a == b) { snprintf(g_message, sizeof g_message, "No region"); return; }
  {
    int n = b - a;
    char *buf = malloc((size_t)n + 1);
    if (buf) { gtcaca_editor_get_text_range(ed, a, b, buf, n + 1); kill_set(buf, n); free(buf); }
  }
  gtcaca_editor_set_empty_selection(ed, gtcaca_editor_get_current_pos(ed));
  g_mark_active = 0;
  snprintf(g_message, sizeof g_message, "Saved to kill ring");
}

/* M-d / M-Backspace: kill the word forward/back, saving it to the kill ring. */
void kill_word_right(gtcaca_editor_widget_t *ed)
{
  int from = gtcaca_editor_get_current_pos(ed);
  int to   = gtcaca_editor_word_right_position(ed, from);
  if (to > from) { int n = to - from; char *b = malloc((size_t)n + 1); if (b) { gtcaca_editor_get_text_range(ed, from, to, b, n + 1); kill_set(b, n); free(b); } gtcaca_editor_del_word_right(ed); }
}
void kill_word_left(gtcaca_editor_widget_t *ed)
{
  int to   = gtcaca_editor_get_current_pos(ed);
  int from = gtcaca_editor_word_left_position(ed, to);
  if (from < to) { int n = to - from; char *b = malloc((size_t)n + 1); if (b) { gtcaca_editor_get_text_range(ed, from, to, b, n + 1); kill_set(b, n); free(b); } gtcaca_editor_del_word_left(ed); }
}

/* M-u / M-l: upcase/downcase the region, or the word forward from point. */
void case_word(gtcaca_editor_widget_t *ed, int upper)
{
  if (gtcaca_editor_get_selection_start(ed) == gtcaca_editor_get_selection_end(ed)) {
    int from = gtcaca_editor_get_current_pos(ed);
    int to   = gtcaca_editor_word_right_position(ed, from);
    if (to <= from) return;
    gtcaca_editor_set_selection(ed, to, from);
    if (upper) gtcaca_editor_upper_case(ed); else gtcaca_editor_lower_case(ed);
    gtcaca_editor_set_empty_selection(ed, to);
  } else {
    if (upper) gtcaca_editor_upper_case(ed); else gtcaca_editor_lower_case(ed);
  }
}

/* C-t: transpose the two characters around point (Emacs transpose-chars). */
void transpose_chars(gtcaca_editor_widget_t *ed)
{
  int p = gtcaca_editor_get_current_pos(ed), len = gtcaca_editor_get_length(ed), a, b;
  char ca, cb, two[3];
  if (len < 2) return;
  if (p >= len) { a = len - 2; b = len - 1; } else if (p == 0) { a = 0; b = 1; } else { a = p - 1; b = p; }
  ca = gtcaca_editor_get_char_at(ed, a); cb = gtcaca_editor_get_char_at(ed, b);
  if (ca == '\n' || cb == '\n') return;
  two[0] = cb; two[1] = ca; two[2] = '\0';
  gtcaca_editor_delete_range(ed, a, 2);
  gtcaca_editor_insert_text(ed, a, two);
  gtcaca_editor_set_empty_selection(ed, (p >= len) ? p : p + 1);
}

/* ── comment / uncomment (M-;), Emacs comment-dwim ─────────────────────────── */

/* The line-comment prefix for the current file (from the language config, with
   a small fallback by extension). NULL if unknown. */
const char *line_comment_for_ext(const char *ext)
{
  if (!ext) return NULL;
  if (!strcmp(ext, ".c") || !strcmp(ext, ".h") || !strcmp(ext, ".cpp") || !strcmp(ext, ".cc") ||
      !strcmp(ext, ".hpp") || !strcmp(ext, ".js") || !strcmp(ext, ".mjs") || !strcmp(ext, ".ts") ||
      !strcmp(ext, ".java") || !strcmp(ext, ".go") || !strcmp(ext, ".rs") ||
      !strcmp(ext, ".json") || !strcmp(ext, ".jsonc")) return "//";
  if (!strcmp(ext, ".py") || !strcmp(ext, ".pyw") || !strcmp(ext, ".sh") || !strcmp(ext, ".rb") ||
      !strcmp(ext, ".yaml") || !strcmp(ext, ".yml") || !strcmp(ext, ".toml") ||
      !strcmp(ext, ".cfg") || !strcmp(ext, ".conf")) return "#";
  if (!strcmp(ext, ".lua") || !strcmp(ext, ".sql")) return "--";
  if (!strcmp(ext, ".el") || !strcmp(ext, ".lisp") || !strcmp(ext, ".scm") || !strcmp(ext, ".clj")) return ";;";
  return NULL;
}

const char *current_line_comment(void)
{
  const char *lc = g_langcfg ? gtcaca_editor_langcfg_get_line_comment(g_langcfg) : NULL;
  if (lc && lc[0]) return lc;
  return line_comment_for_ext(g_filename ? strrchr(g_filename, '.') : NULL);
}

int line_first_nonws(gtcaca_editor_widget_t *ed, int line)
{
  int p = gtcaca_editor_position_from_line(ed, line);
  int e = gtcaca_editor_get_line_end_position(ed, line);
  while (p < e) { char c = gtcaca_editor_get_char_at(ed, p); if (c != ' ' && c != '\t') break; p++; }
  return p;
}
int line_is_blank(gtcaca_editor_widget_t *ed, int line)
{
  return line_first_nonws(ed, line) >= gtcaca_editor_get_line_end_position(ed, line);
}
int line_is_commented(gtcaca_editor_widget_t *ed, int line, const char *lc, int lclen)
{
  int p = line_first_nonws(ed, line), i;
  if (p + lclen > gtcaca_editor_get_line_end_position(ed, line)) return 0;
  for (i = 0; i < lclen; i++) if (gtcaca_editor_get_char_at(ed, p + i) != lc[i]) return 0;
  return 1;
}

void comment_dwim(gtcaca_editor_widget_t *ed)
{
  const char *lc = current_line_comment();
  int lclen, first, last, L, any_content = 0, any_uncommented = 0, do_comment;
  char cstr[40];

  if (!lc || !lc[0]) { snprintf(g_message, sizeof g_message, "No comment syntax for this file"); return; }
  lclen = (int)strlen(lc);

  if (gtcaca_editor_get_selection_start(ed) != gtcaca_editor_get_selection_end(ed)) {
    int a = gtcaca_editor_get_selection_start(ed), b = gtcaca_editor_get_selection_end(ed);
    first = gtcaca_editor_line_from_position(ed, a);
    last  = gtcaca_editor_line_from_position(ed, b);
    if (last > first && b == gtcaca_editor_position_from_line(ed, last)) last--;  /* trailing line at col 0 */
  } else {
    first = last = gtcaca_editor_get_current_line(ed);
  }

  /* comment unless every non-blank line in the range is already commented */
  for (L = first; L <= last; L++) {
    if (line_is_blank(ed, L)) continue;
    any_content = 1;
    if (!line_is_commented(ed, L, lc, lclen)) any_uncommented = 1;
  }
  do_comment = !any_content || any_uncommented;

  snprintf(cstr, sizeof cstr, "%s ", lc);

  /* edit bottom-to-top so earlier line positions stay valid */
  for (L = last; L >= first; L--) {
    int p = line_first_nonws(ed, L);
    if (do_comment) {
      if (line_is_blank(ed, L)) continue;
      gtcaca_editor_insert_text(ed, p, cstr);
    } else if (line_is_commented(ed, L, lc, lclen)) {
      int del = lclen;
      if (gtcaca_editor_get_char_at(ed, p + lclen) == ' ') del++;   /* eat one following space */
      gtcaca_editor_delete_range(ed, p, del);
    }
  }
  snprintf(g_message, sizeof g_message, "%s %d line%s",
           do_comment ? "Commented" : "Uncommented", last - first + 1, last == first ? "" : "s");
}

/* show-paren: highlight the brace under/just-before the caret and its match. */
void show_paren(gtcaca_editor_widget_t *ed)
{
  int pos = gtcaca_editor_get_current_pos(ed), bp = -1;
  char c0 = gtcaca_editor_get_char_at(ed, pos);
  char c1 = pos > 0 ? gtcaca_editor_get_char_at(ed, pos - 1) : 0;
  if (c0 && strchr("()[]{}", c0))      bp = pos;
  else if (c1 && strchr("()[]{}", c1)) bp = pos - 1;
  if (bp >= 0) {
    int m = gtcaca_editor_brace_match(ed, bp);
    if (m >= 0) gtcaca_editor_brace_highlight(ed, bp, m);
    else        gtcaca_editor_brace_badlight(ed, bp);
  } else gtcaca_editor_brace_highlight(ed, -1, -1);
}

void kill_line(gtcaca_editor_widget_t *ed)
{
  int cur  = gtcaca_editor_get_current_pos(ed);
  int line = gtcaca_editor_get_current_line(ed);
  int eol  = gtcaca_editor_get_line_end_position(ed, line);
  int len  = gtcaca_editor_get_length(ed);
  int from = cur, to;

  if (cur < eol)        to = eol;        /* kill to end of line  */
  else if (cur < len)   to = cur + 1;    /* at EOL: kill newline */
  else                  return;          /* end of buffer        */

  {
    int n = to - from;
    char *buf = malloc((size_t)n + 1);
    if (buf) { gtcaca_editor_get_text_range(ed, from, to, buf, n + 1); kill_set(buf, n); free(buf); }
  }
  gtcaca_editor_delete_range(ed, from, to - from);
}

void yank(gtcaca_editor_widget_t *ed)
{
  if (!g_killbuf) { snprintf(g_message, sizeof(g_message), "Kill ring is empty"); return; }
  gtcaca_editor_insert_text(ed, gtcaca_editor_get_current_pos(ed), g_killbuf);
  g_mark_active = 0;
}

void exchange_point_and_mark(gtcaca_editor_widget_t *ed)
{
  int point  = gtcaca_editor_get_current_pos(ed);
  int anchor = gtcaca_editor_get_anchor(ed);
  gtcaca_editor_set_selection(ed, anchor, point);  /* caret <- anchor, anchor <- point */
  g_mark_active = 1;
}

void recenter(gtcaca_editor_widget_t *ed)
{
  int line = gtcaca_editor_get_current_line(ed);
  /* rows of text in the view; fall back to the widget height if we have not
     been drawn yet, so the very first C-l still centres. */
  int vh   = ed->view_lines > 0 ? ed->view_lines : ed->height - 2;
  int top  = line - (vh > 0 ? vh / 2 : 0);
  ed->first_visible_line = top < 0 ? 0 : top;   /* near the top it can't centre */
  snprintf(g_message, sizeof g_message, "Recentered");
}

/* ── view toggles (line numbers, folding, annotations) ─────────────────────── */

void toggle_wrap(gtcaca_editor_widget_t *ed)
{
  int on = !gtcaca_editor_get_wrap(ed);
  gtcaca_editor_set_wrap(ed, on);
  snprintf(g_message, sizeof g_message, "Line wrap %s", on ? "on" : "off (lines scroll)");
}

void toggle_line_numbers(gtcaca_editor_widget_t *ed)
{
  int on = !gtcaca_editor_get_line_numbers(ed);
  gtcaca_editor_set_line_numbers(ed, on);
  snprintf(g_message, sizeof g_message, "Line numbers %s", on ? "on" : "off");
}

void toggle_folding(gtcaca_editor_widget_t *ed)
{
  g_folding = !g_folding;
  gtcaca_editor_set_fold_margin(ed, g_folding);
  if (g_folding) gtcaca_editor_fold_by_indentation(ed);
  else           gtcaca_editor_fold_all(ed, 1);   /* reveal everything */
  snprintf(g_message, sizeof g_message, "Folding %s", g_folding ? "on" : "off");
}

void toggle_fold_here(gtcaca_editor_widget_t *ed)
{
  if (!g_folding) { toggle_folding(ed); return; }
  gtcaca_editor_toggle_fold(ed, gtcaca_editor_get_current_line(ed));
}

void toggle_annotation_here(gtcaca_editor_widget_t *ed)
{
  int line = gtcaca_editor_get_current_line(ed);
  char buf[8];
  if (gtcaca_editor_annotation_get_text(ed, line, buf, sizeof buf) > 0) {
    gtcaca_editor_annotation_set_text(ed, line, NULL);
    snprintf(g_message, sizeof g_message, "Annotation removed");
  } else {
    gtcaca_editor_annotation_set_text(ed, line, "note: this line is annotated");
    gtcaca_editor_annotation_set_style(ed, line, GTCACA_EDITOR_STYLE_COMMENT);
    snprintf(g_message, sizeof g_message, "Annotation added");
  }
}

/* Pretty-print JSON: the selection, else the whole buffer if it is one JSON
   value, else the current line (handy for a JSONL record). */
void pretty_print_json(gtcaca_editor_widget_t *ed)
{
  int a, b, n;
  char *src, *pretty;
  gtcaca_json_value *v;

  if (gtcaca_editor_get_selection_start(ed) != gtcaca_editor_get_selection_end(ed)) {
    a = gtcaca_editor_get_selection_start(ed);
    b = gtcaca_editor_get_selection_end(ed);
  } else {
    int len = gtcaca_editor_get_length(ed);
    char *all = malloc((size_t)len + 1);
    gtcaca_json_value *whole = NULL;
    if (all) { gtcaca_editor_get_text(ed, all, len + 1); whole = gtcaca_json_parse(all); free(all); }
    if (whole) { a = 0; b = len; gtcaca_json_free(whole); }
    else {
      int line = gtcaca_editor_get_current_line(ed);
      a = gtcaca_editor_position_from_line(ed, line);
      b = gtcaca_editor_get_line_end_position(ed, line);
    }
  }

  n = b - a;
  src = malloc((size_t)n + 1);
  if (!src) return;
  gtcaca_editor_get_text_range(ed, a, b, src, n + 1);
  v = gtcaca_json_parse(src);
  free(src);
  if (!v) { snprintf(g_message, sizeof g_message, "Not valid JSON"); return; }

  pretty = gtcaca_json_stringify(v, 2);
  gtcaca_json_free(v);
  if (!pretty) return;

  gtcaca_editor_delete_range(ed, a, b - a);
  gtcaca_editor_insert_text(ed, a, pretty);
  gtcaca_editor_set_empty_selection(ed, a + (int)strlen(pretty));
  free(pretty);

  if (gtcaca_editor_get_json_mode(ed)) gtcaca_editor_fold_json(ed);
  snprintf(g_message, sizeof g_message, "Pretty-printed JSON");
}

void save_file(gtcaca_editor_widget_t *ed)
{
  FILE *f;
  int len = gtcaca_editor_get_length(ed);
  char *buf;

  if (!g_filename) { start_save_as(); return; }   /* scratch buffer: ask for a name */

  buf = malloc((size_t)len + 1);
  if (!buf) return;
  gtcaca_editor_get_text(ed, buf, len + 1);

  f = fopen(g_filename, "w");
  if (!f) { snprintf(g_message, sizeof(g_message), "Cannot write %s", g_filename); free(buf); return; }
  fwrite(buf, 1, (size_t)len, f);
  fclose(f);
  free(buf);

  gtcaca_editor_set_save_point(ed);
  snprintf(g_message, sizeof(g_message), "Wrote %s", g_filename);
}

