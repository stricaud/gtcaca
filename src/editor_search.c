/*
 * editor_search.c — searching & replacing, modelled on the Scintilla
 * "Searching" API: a target range plus SCI_SEARCHINTARGET / SCI_REPLACETARGET,
 * SCI_FINDTEXT, and the SCI_SEARCHANCHOR / SCI_SEARCHNEXT / SCI_SEARCHPREV
 * trio used for incremental search.
 *
 * Flags: MATCHCASE, WHOLEWORD, WORDSTART are honoured. REGEXP uses Oniguruma
 * when the library is built with it, otherwise it falls back to a literal match.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gtcaca/editor.h>

#ifdef GTCACA_HAVE_ONIG
#include <oniguruma.h>
#endif

static int _is_word(int c) { return isalnum(c) || c == '_'; }

static int _ci_eq(char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); }

/* Does `text` (len tlen) match the buffer at byte `pos`, honouring flags? */
static int _match_at(gtcaca_editor_widget_t *w, int pos, const char *text, int tlen, int flags)
{
  int i, mc = flags & GTCACA_EDITOR_FIND_MATCHCASE;
  if (pos < 0 || pos + tlen > w->length) return 0;
  for (i = 0; i < tlen; i++) {
    char a = w->text[pos + i], b = text[i];
    if (mc ? (a != b) : !_ci_eq(a, b)) return 0;
  }
  if (flags & (GTCACA_EDITOR_FIND_WHOLEWORD | GTCACA_EDITOR_FIND_WORDSTART)) {
    int before = (pos > 0) ? _is_word((unsigned char)w->text[pos - 1]) : 0;
    if (before) return 0;                                  /* must start a word */
    if (flags & GTCACA_EDITOR_FIND_WHOLEWORD) {
      int after = (pos + tlen < w->length) ? _is_word((unsigned char)w->text[pos + tlen]) : 0;
      if (after) return 0;                                 /* …and end one */
    }
  }
  return 1;
}

#ifdef GTCACA_HAVE_ONIG
/* Regex search in [lo, hi); returns start (and *mend) or -1. */
static int _regex_find(gtcaca_editor_widget_t *w, const char *pat, int lo, int hi, int flags, int *mend)
{
  static int inited = 0;
  regex_t *re;
  OnigErrorInfo einfo;
  OnigRegion *region;
  int opt = (flags & GTCACA_EDITOR_FIND_MATCHCASE) ? ONIG_OPTION_NONE : ONIG_OPTION_IGNORECASE;
  int rc, start = -1;
  if (!inited) { OnigEncoding e[1]; e[0] = ONIG_ENCODING_UTF8; onig_initialize(e, 1); inited = 1; }
  if (onig_new(&re, (const OnigUChar *)pat, (const OnigUChar *)pat + strlen(pat),
               opt, ONIG_ENCODING_UTF8, ONIG_SYNTAX_ONIGURUMA, &einfo) != ONIG_NORMAL)
    return -1;
  region = onig_region_new();
  rc = onig_search(re, (const OnigUChar *)w->text, (const OnigUChar *)w->text + w->length,
                   (const OnigUChar *)w->text + lo, (const OnigUChar *)w->text + hi, region, ONIG_OPTION_NONE);
  if (rc >= 0) { start = region->beg[0]; if (mend) *mend = region->end[0]; }
  onig_region_free(region, 1);
  onig_free(re);
  return start;
}
#endif

/* Forward search in [lo, hi); returns start or -1. */
static int _find_fwd(gtcaca_editor_widget_t *w, const char *text, int lo, int hi, int flags, int *mend)
{
  int tlen = (int)strlen(text), p;
#ifdef GTCACA_HAVE_ONIG
  if (flags & GTCACA_EDITOR_FIND_REGEXP) return _regex_find(w, text, lo, hi, flags, mend);
#endif
  if (tlen == 0) return -1;
  for (p = lo; p + tlen <= hi; p++)
    if (_match_at(w, p, text, tlen, flags)) { if (mend) *mend = p + tlen; return p; }
  return -1;
}

/* Backward search: last match starting in [hi, lo] scanning down from lo. */
static int _find_bwd(gtcaca_editor_widget_t *w, const char *text, int lo, int hi, int flags, int *mend)
{
  int tlen = (int)strlen(text), p;
  if (tlen == 0) return -1;
  for (p = lo; p >= hi; p--)
    if (_match_at(w, p, text, tlen, flags)) { if (mend) *mend = p + tlen; return p; }
  return -1;
}

/* ── flags & target ────────────────────────────────────────────────────────── */

void gtcaca_editor_set_search_flags(gtcaca_editor_widget_t *w, int flags) { w->search_flags = flags; }
int  gtcaca_editor_get_search_flags(gtcaca_editor_widget_t *w) { return w->search_flags; }

void gtcaca_editor_set_target_start(gtcaca_editor_widget_t *w, int pos) { w->target_start = pos; }
int  gtcaca_editor_get_target_start(gtcaca_editor_widget_t *w) { return w->target_start; }
void gtcaca_editor_set_target_end(gtcaca_editor_widget_t *w, int pos) { w->target_end = pos; }
int  gtcaca_editor_get_target_end(gtcaca_editor_widget_t *w) { return w->target_end; }
void gtcaca_editor_set_target_range(gtcaca_editor_widget_t *w, int start, int end) { w->target_start = start; w->target_end = end; }
void gtcaca_editor_target_whole_document(gtcaca_editor_widget_t *w) { w->target_start = 0; w->target_end = w->length; }
void gtcaca_editor_target_from_selection(gtcaca_editor_widget_t *w)
{
  w->target_start = gtcaca_editor_get_selection_start(w);
  w->target_end   = gtcaca_editor_get_selection_end(w);
}

/* ── search/replace primitives ─────────────────────────────────────────────── */

int gtcaca_editor_search_in_target(gtcaca_editor_widget_t *w, const char *text)
{
  int lo = w->target_start, hi = w->target_end, mend, start;
  if (lo <= hi) start = _find_fwd(w, text, lo, hi, w->search_flags, &mend);
  else          start = _find_bwd(w, text, lo, hi, w->search_flags, &mend);   /* reversed target */
  if (start < 0) return -1;
  w->target_start = start;
  w->target_end   = mend;
  return start;
}

int gtcaca_editor_replace_target(gtcaca_editor_widget_t *w, const char *text)
{
  int a = w->target_start, b = w->target_end, n = (int)strlen(text);
  if (b < a) { int t = a; a = b; b = t; }
  if (b > a) gtcaca_editor_delete_range(w, a, b - a);
  if (n > 0) gtcaca_editor_insert_text(w, a, text);
  w->target_start = a;
  w->target_end   = a + n;
  return n;
}

int gtcaca_editor_find_text(gtcaca_editor_widget_t *w, int flags, const char *text,
                            int min_pos, int max_pos, int *match_end)
{
  if (min_pos <= max_pos) return _find_fwd(w, text, min_pos, max_pos, flags, match_end);
  return _find_bwd(w, text, min_pos, max_pos, flags, match_end);
}

/* ── anchored (incremental) search ─────────────────────────────────────────── */

void gtcaca_editor_search_anchor(gtcaca_editor_widget_t *w) { w->anchor = w->current_pos; }

int gtcaca_editor_search_next(gtcaca_editor_widget_t *w, int flags, const char *text)
{
  int mend, start = _find_fwd(w, text, w->anchor, w->length, flags, &mend);
  if (start < 0) return -1;
  gtcaca_editor_set_selection(w, mend, start);   /* caret at end, anchor at start */
  return start;
}

int gtcaca_editor_search_prev(gtcaca_editor_widget_t *w, int flags, const char *text)
{
  int mend, start = _find_bwd(w, text, w->anchor, 0, flags, &mend);
  if (start < 0) return -1;
  gtcaca_editor_set_selection(w, start, mend);   /* caret at start, anchor at end */
  return start;
}
