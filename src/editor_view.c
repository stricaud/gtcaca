#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gtcaca/editor.h>
#include <gtcaca/main.h>

/* ── per-line metadata: sizing & fold visibility ───────────────────────────── */

static void _init_meta(gtcaca_editor_line_t *m)
{
  m->fold_level = GTCACA_EDITOR_FOLDLEVELBASE;
  m->fold_expanded = 1;
  m->visible = 1;
  m->annotation = NULL;
  m->annotation_style = GTCACA_EDITOR_STYLE_DEFAULT;
}

static void _recompute_visibility(gtcaca_editor_widget_t *w)
{
  int n = w->lines_meta_len, i;
  struct { int level; int collapsed; } stack[256];
  int sp = 0;

  for (i = 0; i < n; i++) {
    int lvl    = w->lines_meta[i].fold_level & GTCACA_EDITOR_FOLDLEVELNUMBERMASK;
    int header = w->lines_meta[i].fold_level & GTCACA_EDITOR_FOLDLEVELHEADERFLAG;
    int vis = 1, j;

    while (sp > 0 && lvl <= stack[sp - 1].level) sp--;   /* exited those headers */
    for (j = 0; j < sp; j++) if (stack[j].collapsed) { vis = 0; break; }
    w->lines_meta[i].visible = vis;

    if (header && sp < (int)(sizeof stack / sizeof *stack)) {
      stack[sp].level = lvl;
      stack[sp].collapsed = !w->lines_meta[i].fold_expanded;
      sp++;
    }
  }
}

void _gtcaca_editor_ensure_meta(gtcaca_editor_widget_t *w)
{
  int lc = gtcaca_editor_get_line_count(w), i;

  if (lc > w->lines_meta_cap) {
    int ncap = w->lines_meta_cap ? w->lines_meta_cap * 2 : 64;
    if (ncap < lc) ncap = lc;
    gtcaca_editor_line_t *p = realloc(w->lines_meta, (size_t)ncap * sizeof(*p));
    if (!p) return;
    w->lines_meta = p;
    w->lines_meta_cap = ncap;
  }
  if (lc > w->lines_meta_len) {
    for (i = w->lines_meta_len; i < lc; i++) _init_meta(&w->lines_meta[i]);
  } else if (lc < w->lines_meta_len) {
    for (i = lc; i < w->lines_meta_len; i++) free(w->lines_meta[i].annotation);
  }
  w->lines_meta_len = lc;

  if (w->fold_dirty) { _recompute_visibility(w); w->fold_dirty = 0; }
}

/* ── folding API ───────────────────────────────────────────────────────────── */

void gtcaca_editor_set_fold_level(gtcaca_editor_widget_t *w, int line, int level)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return;
  w->lines_meta[line].fold_level = level;
  w->fold_dirty = 1;
}

int gtcaca_editor_get_fold_level(gtcaca_editor_widget_t *w, int line)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return GTCACA_EDITOR_FOLDLEVELBASE;
  return w->lines_meta[line].fold_level;
}

void gtcaca_editor_set_fold_expanded(gtcaca_editor_widget_t *w, int line, int expanded)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return;
  w->lines_meta[line].fold_expanded = expanded ? 1 : 0;
  w->fold_dirty = 1;
}

int gtcaca_editor_get_fold_expanded(gtcaca_editor_widget_t *w, int line)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return 1;
  return w->lines_meta[line].fold_expanded;
}

void gtcaca_editor_toggle_fold(gtcaca_editor_widget_t *w, int line)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return;
  if (!(w->lines_meta[line].fold_level & GTCACA_EDITOR_FOLDLEVELHEADERFLAG)) return;
  w->lines_meta[line].fold_expanded = !w->lines_meta[line].fold_expanded;
  w->fold_dirty = 1;
  _gtcaca_editor_ensure_meta(w);   /* recompute now so we can fix the caret */

  /* If the caret was hidden by collapsing, move it onto the (visible) header. */
  if (!gtcaca_editor_get_line_visible(w, gtcaca_editor_get_current_line(w)))
    gtcaca_editor_goto_pos(w, gtcaca_editor_position_from_line(w, line));
}

int gtcaca_editor_get_line_visible(gtcaca_editor_widget_t *w, int line)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return 1;
  return w->lines_meta[line].visible;
}

void gtcaca_editor_fold_all(gtcaca_editor_widget_t *w, int expanded)
{
  int i;
  _gtcaca_editor_ensure_meta(w);
  for (i = 0; i < w->lines_meta_len; i++)
    if (w->lines_meta[i].fold_level & GTCACA_EDITOR_FOLDLEVELHEADERFLAG)
      w->lines_meta[i].fold_expanded = expanded ? 1 : 0;
  w->fold_dirty = 1;
}

/* Indentation of a line in columns (tabs expanded); -1 for blank/whitespace. */
static int _indent_of(gtcaca_editor_widget_t *w, int line)
{
  int p   = gtcaca_editor_position_from_line(w, line);
  int end = gtcaca_editor_get_line_end_position(w, line);
  int col = 0;
  while (p < end) {
    char c = gtcaca_editor_get_char_at(w, p);
    if (c == ' ') col++;
    else if (c == '\t') col += w->tab_width - (col % w->tab_width);
    else return col;   /* first non-blank */
    p++;
  }
  return -1;   /* blank line */
}

void gtcaca_editor_fold_by_indentation(gtcaca_editor_widget_t *w)
{
  int n, i;
  _gtcaca_editor_ensure_meta(w);
  n = w->lines_meta_len;

  for (i = 0; i < n; i++) {
    int indent = _indent_of(w, i);
    int level, j, next_indent = -1, header = 0;

    if (indent < 0) {
      /* blank line: inherit the next non-blank line's level so it doesn't
         break the enclosing fold */
      for (j = i + 1; j < n; j++) { next_indent = _indent_of(w, j); if (next_indent >= 0) break; }
      indent = next_indent >= 0 ? next_indent : 0;
    } else {
      for (j = i + 1; j < n; j++) { int ni = _indent_of(w, j); if (ni >= 0) { next_indent = ni; break; } }
      if (next_indent > indent) header = 1;
    }

    /* The level number is the indentation column itself, so deeper lines get a
       strictly greater level (independent of the tab size). */
    if (indent > GTCACA_EDITOR_FOLDLEVELNUMBERMASK) indent = GTCACA_EDITOR_FOLDLEVELNUMBERMASK;
    level = GTCACA_EDITOR_FOLDLEVELBASE + indent;
    if (header) level |= GTCACA_EDITOR_FOLDLEVELHEADERFLAG;
    /* preserve the expanded flag already on the line */
    w->lines_meta[i].fold_level = level;
  }
  w->fold_dirty = 1;
}

/* ── annotations API ───────────────────────────────────────────────────────── */

void gtcaca_editor_annotation_set_text(gtcaca_editor_widget_t *w, int line, const char *text)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return;
  free(w->lines_meta[line].annotation);
  w->lines_meta[line].annotation = (text && text[0]) ? strdup(text) : NULL;
}

int gtcaca_editor_annotation_get_text(gtcaca_editor_widget_t *w, int line, char *buf, int len)
{
  const char *a;
  _gtcaca_editor_ensure_meta(w);
  if (len > 0) buf[0] = '\0';
  if (line < 0 || line >= w->lines_meta_len || !w->lines_meta[line].annotation) return 0;
  a = w->lines_meta[line].annotation;
  strncpy(buf, a, (size_t)len - 1);
  buf[len - 1] = '\0';
  return (int)strlen(buf);
}

void gtcaca_editor_annotation_set_style(gtcaca_editor_widget_t *w, int line, int style)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return;
  w->lines_meta[line].annotation_style = style;
}

int gtcaca_editor_annotation_get_style(gtcaca_editor_widget_t *w, int line)
{
  _gtcaca_editor_ensure_meta(w);
  if (line < 0 || line >= w->lines_meta_len) return GTCACA_EDITOR_STYLE_DEFAULT;
  return w->lines_meta[line].annotation_style;
}

void gtcaca_editor_annotation_clear_all(gtcaca_editor_widget_t *w)
{
  int i;
  for (i = 0; i < w->lines_meta_len; i++) { free(w->lines_meta[i].annotation); w->lines_meta[i].annotation = NULL; }
}

void gtcaca_editor_annotation_set_visible(gtcaca_editor_widget_t *w, int mode) { w->annotation_visible = mode; }
int  gtcaca_editor_annotation_get_visible(gtcaca_editor_widget_t *w)           { return w->annotation_visible; }

int gtcaca_editor_annotation_get_lines(gtcaca_editor_widget_t *w, int line)
{
  const char *a, *p;
  int n;
  _gtcaca_editor_ensure_meta(w);
  if (w->annotation_visible == GTCACA_EDITOR_ANNOTATION_HIDDEN) return 0;
  if (line < 0 || line >= w->lines_meta_len || !w->lines_meta[line].annotation) return 0;
  a = w->lines_meta[line].annotation;
  n = 1;
  for (p = a; *p; p++) if (*p == '\n') n++;
  return n;
}
