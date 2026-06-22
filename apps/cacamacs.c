/*
 * cacamacs.c — a tiny text editor built on the gtcaca `editor` widget, driven
 *           with Emacs key bindings.
 *
 * Usage:
 *   cacamacs [file | directory] [language-configuration.json]
 *
 * Given a directory (or C-x d), a file browser opens: Up/Down to move, Enter to
 * descend into a folder ("../" goes up) or open a file in the editor.
 *
 * The editor widget provides the document model, caret/selection, undo and
 * the default key bindings (arrows, Home/End, PageUp/Down, typing, Backspace,
 * Enter). This app layers an Emacs keymap on top through the widget's key
 * callback: keys it handles are consumed, everything else falls through to the
 * widget's own editing keys.
 *
 * Bindings:
 *   Motion : C-f C-b C-n C-p  C-a C-e  C-v (page down)  arrows  M-f/M-b word
 *   Edit   : C-d (delete fwd)  C-k (kill line)  Backspace  C-t transpose chars
 *            M-d / M-DEL kill word  M-u/M-l upcase/downcase word  Insert overtype
 *   Lines  : C-x C-t transpose lines   M-; comment/uncomment line or region
 *   Modes  : C-x C-q read-only   C-x w show whitespace   (matching braces glow)
 *   Region : C-Space set mark  C-w kill  M-w (Esc w) copy  C-y yank  C-g cancel
 *   Search : C-s incremental search forward, C-r backward (Enter accepts,
 *            C-g cancels); M-% (Esc %) or C-x r replace-all
 *   Meta   : Esc acts as the Meta prefix (Esc then a key = M-key)
 *   Undo   : C-/  (also  C-x u)
 *   Files  : C-x C-s save      C-x C-c quit       C-x C-x exchange point/mark
 *   Complete: Tab — complete the word before the caret (else insert a soft tab).
 *             In the list: Up/Down choose, Enter/Tab accept, Esc cancel.
 *   View   : C-x l line numbers   C-x f folding mode   C-x t toggle fold
 *            C-x a toggle annotation on the current line
 *   Files  : C-x d open the file browser (navigate the current directory)
 *   JSON   : .json/.jsonl open in JSON mode (colour + fold); C-x p pretty-prints
 *            the selection / whole buffer / current JSONL line.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/textlist.h>
#include <gtcaca/editor.h>
#include <gtcaca/box.h>
#include <gtcaca/json.h>

/* Control keys that libcaca does not expose as named constants. */
#define KEY_CTRL_SPACE 0x00
#define KEY_CTRL_S     0x13
#define KEY_UNDO       0x1f   /* C-/ and C-_ both arrive as 0x1f */

/* The globals below always mirror the *focused* buffer; switching focus stores
   them into the old buffer and loads them from the new one (buffer_*_globals). */
static gtcaca_editor_widget_t    *g_ed       = NULL;   /* focused editor       */
static gtcaca_statusbar_widget_t *g_modeline = NULL;
static gtcaca_window_widget_t    *g_win      = NULL;   /* focused pane window  */
static const char                *g_filename = NULL;
static char                       g_open_path[PATH_MAX] = "";

/* ── buffers & a binary split-window tree ──────────────────────────────────── */
#define MAXBUF    32
#define MAXLEAVES 8
#define MAXNODES  (2 * MAXLEAVES)
typedef struct {
  gtcaca_editor_widget_t  *ed;            /* the document widget (the buffer) */
  char                     path[PATH_MAX];
  int                      has_file;
  gtcaca_editor_langcfg_t *langcfg;
  gtcaca_editor_grammar_t *grammar;
  char                     langname[64];
  const char             **keywords;
  int                      n_keywords;
  int                      folding;
  int                      pane;          /* leaf node displaying it, or -1 */
} buffer_t;
static buffer_t g_buffers[MAXBUF];
static int      g_nbuf = 0;
static int      g_cur_buf = -1;           /* focused buffer index */

/* A window layout node: a leaf (one window showing a buffer) or a split with
   two children. Splitting only ever divides the focused leaf. */
typedef struct {
  int used, is_leaf, parent;
  int buf, win;                 /* leaf: buffer index + window-pool slot   */
  int orient, child[2];         /* split: 1 = stacked rows, 0 = side cols   */
  int x, y, w, h;               /* computed rectangle                       */
} node_t;
static node_t g_nodes[MAXNODES];
static int    g_root = -1, g_focus_leaf = -1;
static gtcaca_window_widget_t *g_winpool[MAXLEAVES];
static int    g_win_used[MAXLEAVES];

/* file browser — a read-only editor showing the directory listing */
static gtcaca_window_widget_t    *g_browser_win = NULL;
static gtcaca_editor_widget_t    *g_browser_ed  = NULL;
static char                       g_curdir[PATH_MAX] = "";
static gtcaca_editor_langcfg_t   *g_langcfg  = NULL;
static gtcaca_editor_grammar_t   *g_grammar  = NULL;
static char                       g_langname[64] = "fundamental";
static const char               **g_keywords = NULL;   /* language keywords, for completion */
static int                        g_n_keywords = 0;
static int                        g_folding = 0;         /* folding mode on? */

/* indentation config (~/.cacamacs/config.json, with per-extension overrides) */
typedef struct { char ext[16]; int tab_size, insert_spaces, indent_size; int has_ts, has_is, has_id; } lang_override_t;
static int             g_cfg_tab = 8, g_cfg_spaces = 1, g_cfg_indent = 2;   /* global defaults */
static int             g_cfg_edge = 0;                                       /* edge/ruler column */
static lang_override_t g_overrides[32];
static int             g_noverrides = 0;
static int             g_tab_size = 8, g_insert_spaces = 1, g_indent_size = 2; /* effective */

static int  is_word_char(int c) { return isalnum((unsigned char)c) || c == '_' || c == '$'; }
static void complete_at_point(gtcaca_editor_widget_t *ed);   /* defined below */
static void show_browser(void);                              /* defined below */
static void hide_browser(void);
static void browser_open_current(void);
static void open_path_in_editor(const char *path);
/* windows / buffers / split panes (defined below) */
static void pane_split(int rows);
static void pane_unsplit_one(void);
static void pane_delete_current(void);
static void pane_other_window(void);
static void pane_switch_buffer(void);
static void pane_hide_all(void);
static void pane_show_all(void);
static int  buf_index_of(gtcaca_editor_widget_t *ed);
static void focus_pane(int slot);

static int   g_mark_active   = 0;
static int   g_ctrl_x        = 0;   /* C-x prefix pending */
static int   g_meta          = 0;   /* Meta (Esc) prefix pending */
static char *g_killbuf       = NULL;
static char  g_message[128]  = "";

/* ── kill ring (single register) ───────────────────────────────────────────── */

static void kill_set(const char *text, int len)
{
  free(g_killbuf);
  g_killbuf = malloc((size_t)len + 1);
  if (g_killbuf) { memcpy(g_killbuf, text, (size_t)len); g_killbuf[len] = '\0'; }
}

/* ── commands ──────────────────────────────────────────────────────────────── */

typedef void (*mv_t)(gtcaca_editor_widget_t *);

/* Move, extending the selection instead when the mark is active. */
static void move(gtcaca_editor_widget_t *ed, mv_t plain, mv_t extend)
{
  if (g_mark_active) extend(ed);
  else               plain(ed);
}

static void set_mark(gtcaca_editor_widget_t *ed)
{
  gtcaca_editor_set_empty_selection(ed, gtcaca_editor_get_current_pos(ed));
  g_mark_active = 1;
  snprintf(g_message, sizeof(g_message), "Mark set");
}

static void keyboard_quit(gtcaca_editor_widget_t *ed)
{
  gtcaca_editor_set_empty_selection(ed, gtcaca_editor_get_current_pos(ed));
  g_mark_active = 0;
  snprintf(g_message, sizeof(g_message), "Quit");
}

static void kill_region(gtcaca_editor_widget_t *ed)
{
  int a = gtcaca_editor_get_selection_start(ed);
  int b = gtcaca_editor_get_selection_end(ed);
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
static void copy_region(gtcaca_editor_widget_t *ed)
{
  int a = gtcaca_editor_get_selection_start(ed);
  int b = gtcaca_editor_get_selection_end(ed);
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
static void kill_word_right(gtcaca_editor_widget_t *ed)
{
  int from = gtcaca_editor_get_current_pos(ed);
  int to   = gtcaca_editor_word_right_position(ed, from);
  if (to > from) { int n = to - from; char *b = malloc((size_t)n + 1); if (b) { gtcaca_editor_get_text_range(ed, from, to, b, n + 1); kill_set(b, n); free(b); } gtcaca_editor_del_word_right(ed); }
}
static void kill_word_left(gtcaca_editor_widget_t *ed)
{
  int to   = gtcaca_editor_get_current_pos(ed);
  int from = gtcaca_editor_word_left_position(ed, to);
  if (from < to) { int n = to - from; char *b = malloc((size_t)n + 1); if (b) { gtcaca_editor_get_text_range(ed, from, to, b, n + 1); kill_set(b, n); free(b); } gtcaca_editor_del_word_left(ed); }
}

/* M-u / M-l: upcase/downcase the region, or the word forward from point. */
static void case_word(gtcaca_editor_widget_t *ed, int upper)
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
static void transpose_chars(gtcaca_editor_widget_t *ed)
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
static const char *line_comment_for_ext(const char *ext)
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

static const char *current_line_comment(void)
{
  const char *lc = g_langcfg ? gtcaca_editor_langcfg_get_line_comment(g_langcfg) : NULL;
  if (lc && lc[0]) return lc;
  return line_comment_for_ext(g_filename ? strrchr(g_filename, '.') : NULL);
}

static int line_first_nonws(gtcaca_editor_widget_t *ed, int line)
{
  int p = gtcaca_editor_position_from_line(ed, line);
  int e = gtcaca_editor_get_line_end_position(ed, line);
  while (p < e) { char c = gtcaca_editor_get_char_at(ed, p); if (c != ' ' && c != '\t') break; p++; }
  return p;
}
static int line_is_blank(gtcaca_editor_widget_t *ed, int line)
{
  return line_first_nonws(ed, line) >= gtcaca_editor_get_line_end_position(ed, line);
}
static int line_is_commented(gtcaca_editor_widget_t *ed, int line, const char *lc, int lclen)
{
  int p = line_first_nonws(ed, line), i;
  if (p + lclen > gtcaca_editor_get_line_end_position(ed, line)) return 0;
  for (i = 0; i < lclen; i++) if (gtcaca_editor_get_char_at(ed, p + i) != lc[i]) return 0;
  return 1;
}

static void comment_dwim(gtcaca_editor_widget_t *ed)
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
static void show_paren(gtcaca_editor_widget_t *ed)
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

static void kill_line(gtcaca_editor_widget_t *ed)
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

static void yank(gtcaca_editor_widget_t *ed)
{
  if (!g_killbuf) { snprintf(g_message, sizeof(g_message), "Kill ring is empty"); return; }
  gtcaca_editor_insert_text(ed, gtcaca_editor_get_current_pos(ed), g_killbuf);
  g_mark_active = 0;
}

static void exchange_point_and_mark(gtcaca_editor_widget_t *ed)
{
  int point  = gtcaca_editor_get_current_pos(ed);
  int anchor = gtcaca_editor_get_anchor(ed);
  gtcaca_editor_set_selection(ed, anchor, point);  /* caret <- anchor, anchor <- point */
  g_mark_active = 1;
}

static void recenter(gtcaca_editor_widget_t *ed)
{
  int line = gtcaca_editor_get_current_line(ed);
  int top  = line - (ed->view_lines > 0 ? ed->view_lines / 2 : 0);
  ed->first_visible_line = top < 0 ? 0 : top;
}

/* ── view toggles (line numbers, folding, annotations) ─────────────────────── */

static void toggle_line_numbers(gtcaca_editor_widget_t *ed)
{
  int on = !gtcaca_editor_get_line_numbers(ed);
  gtcaca_editor_set_line_numbers(ed, on);
  snprintf(g_message, sizeof g_message, "Line numbers %s", on ? "on" : "off");
}

static void toggle_folding(gtcaca_editor_widget_t *ed)
{
  g_folding = !g_folding;
  gtcaca_editor_set_fold_margin(ed, g_folding);
  if (g_folding) gtcaca_editor_fold_by_indentation(ed);
  else           gtcaca_editor_fold_all(ed, 1);   /* reveal everything */
  snprintf(g_message, sizeof g_message, "Folding %s", g_folding ? "on" : "off");
}

static void toggle_fold_here(gtcaca_editor_widget_t *ed)
{
  if (!g_folding) { toggle_folding(ed); return; }
  gtcaca_editor_toggle_fold(ed, gtcaca_editor_get_current_line(ed));
}

static void toggle_annotation_here(gtcaca_editor_widget_t *ed)
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
static void pretty_print_json(gtcaca_editor_widget_t *ed)
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

static void save_file(gtcaca_editor_widget_t *ed)
{
  FILE *f;
  int len = gtcaca_editor_get_length(ed);
  char *buf;

  if (!g_filename) { snprintf(g_message, sizeof(g_message), "No file name"); return; }

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

/* ── incremental search (C-s / C-r), à la Emacs isearch ────────────────────── */

static int   g_isearch = 0, g_isearch_dir = 1, g_isearch_fail = 0;
static int   g_isearch_origin = 0, g_isearch_from = 0, g_isearch_last = 0;
static char  g_isearch_query[128];
static int   g_isearch_qlen = 0;

static void isearch_status(void)
{
  snprintf(g_message, sizeof g_message, "%sI-search%s: %s",
           g_isearch_fail ? "Failing " : "", g_isearch_dir < 0 ? " backward" : "", g_isearch_query);
}

static void isearch_redo(gtcaca_editor_widget_t *ed)
{
  int len = gtcaca_editor_get_length(ed), me, p, from;
  if (g_isearch_qlen == 0) { gtcaca_editor_goto_pos(ed, g_isearch_origin); g_isearch_last = g_isearch_origin; g_isearch_fail = 0; isearch_status(); return; }
  from = g_isearch_from; if (from < 0) from = 0; if (from > len) from = len;
  if (g_isearch_dir > 0) p = gtcaca_editor_find_text(ed, 0, g_isearch_query, from, len, &me);
  else                   p = gtcaca_editor_find_text(ed, 0, g_isearch_query, from, 0, &me);
  if (p < 0)   /* wrap around */
    p = (g_isearch_dir > 0) ? gtcaca_editor_find_text(ed, 0, g_isearch_query, 0, len, &me)
                            : gtcaca_editor_find_text(ed, 0, g_isearch_query, len, 0, &me);
  if (p < 0) g_isearch_fail = 1;
  else {
    g_isearch_fail = 0; g_isearch_last = p;
    if (g_isearch_dir > 0) gtcaca_editor_set_selection(ed, me, p);   /* caret at end   */
    else                   gtcaca_editor_set_selection(ed, p, me);   /* caret at start */
  }
  isearch_status();
}

static void start_isearch(gtcaca_editor_widget_t *ed, int dir)
{
  g_isearch = 1; g_isearch_dir = dir;
  g_isearch_origin = g_isearch_from = g_isearch_last = gtcaca_editor_get_current_pos(ed);
  g_isearch_qlen = 0; g_isearch_query[0] = '\0';
  isearch_status();
}

static int isearch_key(gtcaca_editor_widget_t *ed, int key)
{
  switch (key) {
  case KEY_CTRL_S:        g_isearch_dir = 1;  g_isearch_from = g_isearch_last + 1; isearch_redo(ed); return 1;
  case CACA_KEY_CTRL_R:   g_isearch_dir = -1; g_isearch_from = g_isearch_last - 1; isearch_redo(ed); return 1;
  case CACA_KEY_RETURN:
  case 10:                g_isearch = 0; g_message[0] = '\0'; return 1;          /* accept */
  case CACA_KEY_CTRL_G:
  case CACA_KEY_ESCAPE:   g_isearch = 0; gtcaca_editor_goto_pos(ed, g_isearch_origin); snprintf(g_message, sizeof g_message, "Quit"); return 1;
  case CACA_KEY_BACKSPACE:
  case CACA_KEY_DELETE:
    if (g_isearch_qlen > 0) { g_isearch_query[--g_isearch_qlen] = '\0'; g_isearch_from = g_isearch_origin; isearch_redo(ed); }
    else { g_isearch = 0; gtcaca_editor_goto_pos(ed, g_isearch_origin); }
    return 1;
  default:
    if (key >= 32 && key <= 126) {
      if (g_isearch_qlen < (int)sizeof g_isearch_query - 1) {
        g_isearch_query[g_isearch_qlen++] = (char)key; g_isearch_query[g_isearch_qlen] = '\0';
        g_isearch_from = g_isearch_last; isearch_redo(ed);
      }
      return 1;
    }
    g_isearch = 0;          /* any other key ends the search (caret stays put) */
    return 0;
  }
}

/* ── minibuffer (used by replace) ──────────────────────────────────────────── */

static int   g_mb_active = 0;
static char  g_mb_prompt[64], g_mb_buf[PATH_MAX];
static int   g_mb_len = 0;
static int   g_mb_complete = 0;        /* Tab does filename completion */
static void (*g_mb_cb)(const char *) = NULL;

static void mb_status(void) { snprintf(g_message, sizeof g_message, "%s%s", g_mb_prompt, g_mb_buf); }

/* Optional initial content for the prompt. */
static void start_minibuffer_init(const char *prompt, void (*cb)(const char *), int complete, const char *initial)
{
  g_mb_active = 1; g_mb_cb = cb; g_mb_complete = complete;
  strncpy(g_mb_prompt, prompt, sizeof g_mb_prompt - 1); g_mb_prompt[sizeof g_mb_prompt - 1] = '\0';
  if (initial) { strncpy(g_mb_buf, initial, sizeof g_mb_buf - 1); g_mb_buf[sizeof g_mb_buf - 1] = '\0'; }
  else g_mb_buf[0] = '\0';
  g_mb_len = (int)strlen(g_mb_buf);
  mb_status();
}
static void start_minibuffer(const char *prompt, void (*cb)(const char *)) { start_minibuffer_init(prompt, cb, 0, NULL); }

/* Expand a leading "~/" to $HOME. */
static void expand_tilde(const char *in, char *out, size_t outsz)
{
  const char *home = getenv("HOME");
  if (in[0] == '~' && in[1] == '/' && home) snprintf(out, outsz, "%s%s", home, in + 1);
  else snprintf(out, outsz, "%s", in);
}

/* Tab completion for the path in the minibuffer (longest common prefix). */
static void minibuffer_complete_path(void)
{
  char dir[PATH_MAX], realdir[PATH_MAX], partial[NAME_MAX + 1], common[NAME_MAX + 1] = "";
  char *slash = strrchr(g_mb_buf, '/');
  int plen, nmatch = 0, need;
  DIR *d; struct dirent *de;

  if (slash) { size_t dl = (size_t)(slash - g_mb_buf) + 1; if (dl >= sizeof dir) return; memcpy(dir, g_mb_buf, dl); dir[dl] = '\0'; snprintf(partial, sizeof partial, "%s", slash + 1); }
  else { dir[0] = '\0'; snprintf(partial, sizeof partial, "%s", g_mb_buf); }
  expand_tilde(dir[0] ? dir : "./", realdir, sizeof realdir);

  d = opendir(realdir);
  if (!d) { snprintf(g_message, sizeof g_message, "No such directory"); return; }
  plen = (int)strlen(partial);
  while ((de = readdir(d)) != NULL) {
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
    if (de->d_name[0] == '.' && (plen == 0 || partial[0] != '.')) continue;   /* hidden */
    if (strncmp(de->d_name, partial, (size_t)plen) == 0) {
      if (nmatch == 0) snprintf(common, sizeof common, "%s", de->d_name);
      else { int i = 0; while (common[i] && de->d_name[i] && common[i] == de->d_name[i]) i++; common[i] = '\0'; }
      nmatch++;
    }
  }
  closedir(d);
  if (nmatch == 0) { snprintf(g_message, sizeof g_message, "No match"); return; }

  need = snprintf(g_mb_buf, sizeof g_mb_buf, "%s%s", dir, common);
  if (need < 0 || need >= (int)sizeof g_mb_buf) return;
  g_mb_len = (int)strlen(g_mb_buf);
  if (nmatch == 1) {                          /* unique: add '/' if a directory */
    char full[PATH_MAX]; struct stat st;
    expand_tilde(g_mb_buf, full, sizeof full);
    if (stat(full, &st) == 0 && S_ISDIR(st.st_mode) && g_mb_len + 1 < (int)sizeof g_mb_buf) { g_mb_buf[g_mb_len++] = '/'; g_mb_buf[g_mb_len] = '\0'; }
    mb_status();
  } else snprintf(g_message, sizeof g_message, "%s%s   (%d matches)", g_mb_prompt, g_mb_buf, nmatch);
}

static int minibuffer_key(int key)
{
  switch (key) {
  case CACA_KEY_RETURN:
  case 10: {
    void (*cb)(const char *) = g_mb_cb;
    char tmp[PATH_MAX];
    g_mb_active = 0; g_message[0] = '\0';
    strncpy(tmp, g_mb_buf, sizeof tmp - 1); tmp[sizeof tmp - 1] = '\0';
    if (cb) cb(tmp);
    return 1;
  }
  case CACA_KEY_TAB:    if (g_mb_complete) minibuffer_complete_path(); return 1;
  case CACA_KEY_CTRL_G:
  case CACA_KEY_ESCAPE: g_mb_active = 0; snprintf(g_message, sizeof g_message, "Quit"); return 1;
  case CACA_KEY_BACKSPACE:
  case CACA_KEY_DELETE:  if (g_mb_len > 0) g_mb_buf[--g_mb_len] = '\0'; mb_status(); return 1;
  default:
    if (key >= 32 && key <= 126 && g_mb_len < (int)sizeof g_mb_buf - 1) { g_mb_buf[g_mb_len++] = (char)key; g_mb_buf[g_mb_len] = '\0'; }
    mb_status();
    return 1;
  }
}

/* ── find file (C-x C-f) ───────────────────────────────────────────────────── */

static void find_file_done(const char *input)
{
  char path[PATH_MAX];
  struct stat st;
  if (!input[0]) return;
  if (input[0] == '~') expand_tilde(input, path, sizeof path);
  else if (input[0] == '/') snprintf(path, sizeof path, "%s", input);
  else if (g_curdir[0]) snprintf(path, sizeof path, "%s/%s", g_curdir, input);
  else snprintf(path, sizeof path, "%s", input);

  /* A directory opens the file browser (like Emacs dired). */
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) { strncpy(g_curdir, resolved, sizeof g_curdir - 1); g_curdir[sizeof g_curdir - 1] = '\0'; }
    else { strncpy(g_curdir, path, sizeof g_curdir - 1); g_curdir[sizeof g_curdir - 1] = '\0'; }
    show_browser();
    return;
  }
  open_path_in_editor(path);   /* a non-existent path opens an empty new file */
}

static void start_find_file(void)
{
  char defdir[PATH_MAX];
  if (g_cur_buf >= 0 && g_buffers[g_cur_buf].has_file) {
    char *s;
    strncpy(defdir, g_buffers[g_cur_buf].path, sizeof defdir - 1); defdir[sizeof defdir - 1] = '\0';
    s = strrchr(defdir, '/'); if (s) s[1] = '\0'; else defdir[0] = '\0';
  } else if (g_curdir[0]) {
    snprintf(defdir, sizeof defdir, "%s/", g_curdir);
  } else {
    if (getcwd(defdir, sizeof defdir - 1)) strncat(defdir, "/", sizeof defdir - strlen(defdir) - 1);
    else defdir[0] = '\0';
  }
  start_minibuffer_init("Find file: ", find_file_done, 1, defdir);
}

/* ── replace (two-phase prompt; uses the search/replace API) ───────────────── */

static char g_replace_search[256];

static void replace_phase2(const char *with)
{
  int count = 0;
  gtcaca_editor_set_target_range(g_ed, 0, gtcaca_editor_get_length(g_ed));
  while (gtcaca_editor_search_in_target(g_ed, g_replace_search) >= 0) {
    gtcaca_editor_replace_target(g_ed, with);
    gtcaca_editor_set_target_range(g_ed, gtcaca_editor_get_target_end(g_ed), gtcaca_editor_get_length(g_ed));
    count++;
  }
  snprintf(g_message, sizeof g_message, "Replaced %d occurrence%s", count, count == 1 ? "" : "s");
}

static void replace_phase1(const char *search)
{
  if (!search[0]) { snprintf(g_message, sizeof g_message, "Nothing to replace"); return; }
  strncpy(g_replace_search, search, sizeof g_replace_search - 1);
  g_replace_search[sizeof g_replace_search - 1] = '\0';
  start_minibuffer("Replace with: ", replace_phase2);
}

static void start_replace(void) { start_minibuffer("Replace: ", replace_phase1); }

/* ── Emacs keymap ──────────────────────────────────────────────────────────── */

static int on_key(gtcaca_editor_widget_t *ed, int key, void *ud)
{
  (void)ud;

  /* if focus landed on a different pane's editor, sync our notion of focus */
  if (ed != g_ed && ed != g_browser_ed) {
    int bi = buf_index_of(ed);
    if (bi >= 0 && g_buffers[bi].pane >= 0) focus_pane(g_buffers[bi].pane);
  }

  /* incremental search and the minibuffer take keys first */
  if (g_isearch)   return isearch_key(ed, key);
  if (g_mb_active) return minibuffer_key(key);

  /* browser mode: the listing is a read-only editor — Enter opens the entry,
     q/Esc close it, and everything else (arrows, C-s search, …) falls through
     to the normal editor handling (edits are no-ops because it is read-only). */
  if (ed == g_browser_ed) {
    if (key == CACA_KEY_RETURN || key == 10) { browser_open_current(); return 1; }
    if (key == 'q' || key == CACA_KEY_ESCAPE) { hide_browser(); return 1; }
    if (key == CACA_KEY_CTRL_X) return 1;   /* swallow C-x (no save/quit chords here) */
  }

  /* second key of a C-x chord */
  if (g_ctrl_x) {
    g_ctrl_x = 0;
    switch (key) {
    case KEY_CTRL_S:        save_file(ed);                return 1;  /* C-x C-s */
    case CACA_KEY_CTRL_F:   start_find_file();            return 1;  /* C-x C-f */
    case CACA_KEY_CTRL_C:   gtcaca_main_quit();           return 1;  /* C-x C-c */
    case CACA_KEY_CTRL_X:   exchange_point_and_mark(ed);  return 1;  /* C-x C-x */
    case 'u':               gtcaca_editor_undo(ed); g_mark_active = 0; return 1; /* C-x u */
    case 'l':               toggle_line_numbers(ed);      return 1;  /* C-x l */
    case 'f':               toggle_folding(ed);           return 1;  /* C-x f */
    case 't':               toggle_fold_here(ed);         return 1;  /* C-x t */
    case 'a':               toggle_annotation_here(ed);   return 1;  /* C-x a */
    case 'd':               show_browser();               return 1;  /* C-x d (dired) */
    case 'p':               pretty_print_json(ed);        return 1;  /* C-x p */
    case 'r':               start_replace();              return 1;  /* C-x r */
    case '2':               pane_split(1);                return 1;  /* C-x 2 split below */
    case '3':               pane_split(0);                return 1;  /* C-x 3 split right */
    case '1':               pane_unsplit_one();           return 1;  /* C-x 1 one window */
    case '0':               pane_delete_current();        return 1;  /* C-x 0 delete window */
    case 'o':               pane_other_window();          return 1;  /* C-x o other window */
    case 'b':               pane_switch_buffer();         return 1;  /* C-x b switch buffer */
    case CACA_KEY_CTRL_T:   gtcaca_editor_line_transpose(ed); return 1;  /* C-x C-t transpose lines */
    case CACA_KEY_CTRL_Q:
      gtcaca_editor_set_read_only(ed, !gtcaca_editor_get_read_only(ed));
      snprintf(g_message, sizeof g_message, "Read-only %s", gtcaca_editor_get_read_only(ed) ? "on" : "off");
      return 1;
    case 'w':
      gtcaca_editor_set_view_whitespace(ed, !gtcaca_editor_get_view_whitespace(ed));
      snprintf(g_message, sizeof g_message, "Whitespace %s", gtcaca_editor_get_view_whitespace(ed) ? "shown" : "hidden");
      return 1;
    default:
      snprintf(g_message, sizeof(g_message), "C-x %c is undefined", key >= 32 ? key : '?');
      return 1;
    }
  }

  /* second key of a Meta (Esc) chord — Esc then a key acts as M-<key> */
  if (g_meta) {
    g_meta = 0;
    switch (key) {
    case '%':               start_replace();   return 1;  /* M-% query replace */
    case 'w': case 'W':     copy_region(ed);   return 1;  /* M-w copy region   */
    case 'f': case 'F':     move(ed, gtcaca_editor_word_right, gtcaca_editor_word_right_extend); return 1; /* M-f */
    case 'b': case 'B':     move(ed, gtcaca_editor_word_left,  gtcaca_editor_word_left_extend);  return 1; /* M-b */
    case 'd': case 'D':     kill_word_right(ed); return 1;  /* M-d  kill word fwd  */
    case CACA_KEY_BACKSPACE:
    case CACA_KEY_DELETE:   kill_word_left(ed);  return 1;  /* M-DEL kill word back */
    case 'u': case 'U':     case_word(ed, 1);    return 1;  /* M-u  upcase word    */
    case 'l': case 'L':     case_word(ed, 0);    return 1;  /* M-l  downcase word  */
    case ';':               comment_dwim(ed);    return 1;  /* M-;  comment-dwim   */
    case CACA_KEY_ESCAPE:   keyboard_quit(ed); return 1;  /* Esc Esc cancels   */
    default:
      snprintf(g_message, sizeof(g_message), "M-%c is undefined", key >= 32 ? key : '?');
      return 1;
    }
  }

  g_message[0] = '\0';

  switch (key) {
  case CACA_KEY_CTRL_X: g_ctrl_x = 1; return 1;
  case CACA_KEY_ESCAPE: g_meta = 1; snprintf(g_message, sizeof g_message, "ESC-"); return 1;

  /* search */
  case KEY_CTRL_S:      start_isearch(ed, 1);  return 1;   /* C-s */
  case CACA_KEY_CTRL_R: start_isearch(ed, -1); return 1;   /* C-r */

  /* misc editing */
  case CACA_KEY_CTRL_T: transpose_chars(ed);   return 1;   /* C-t transpose chars */
  case CACA_KEY_INSERT:
    gtcaca_editor_set_overtype(ed, !gtcaca_editor_get_overtype(ed));
    snprintf(g_message, sizeof g_message, "Overtype %s", gtcaca_editor_get_overtype(ed) ? "on" : "off");
    return 1;

  /* motion */
  case CACA_KEY_CTRL_F: move(ed, gtcaca_editor_char_right, gtcaca_editor_char_right_extend); return 1;
  case CACA_KEY_CTRL_B: move(ed, gtcaca_editor_char_left,  gtcaca_editor_char_left_extend);  return 1;
  case CACA_KEY_CTRL_N: move(ed, gtcaca_editor_line_down,  gtcaca_editor_line_down_extend);  return 1;
  case CACA_KEY_CTRL_P: move(ed, gtcaca_editor_line_up,    gtcaca_editor_line_up_extend);    return 1;
  case CACA_KEY_CTRL_A: move(ed, gtcaca_editor_home,       gtcaca_editor_home_extend);       return 1;
  case CACA_KEY_CTRL_E: move(ed, gtcaca_editor_line_end,   gtcaca_editor_line_end_extend);   return 1;
  case CACA_KEY_CTRL_V: move(ed, gtcaca_editor_page_down,  gtcaca_editor_page_down_extend);  return 1;

  /* editing */
  case CACA_KEY_CTRL_D: gtcaca_editor_clear(ed);      return 1;  /* delete forward */
  case CACA_KEY_CTRL_K: kill_line(ed);                return 1;
  case CACA_KEY_CTRL_W: kill_region(ed);              return 1;
  case CACA_KEY_CTRL_Y: yank(ed);                     return 1;

  /* region / misc */
  case KEY_CTRL_SPACE:  set_mark(ed);                 return 1;
  case CACA_KEY_CTRL_G: keyboard_quit(ed);            return 1;
  case CACA_KEY_CTRL_L: recenter(ed);                 return 1;
  case KEY_UNDO:        gtcaca_editor_undo(ed); g_mark_active = 0; return 1;

  /* Tab: complete the word before the caret, otherwise indent (per config). */
  case CACA_KEY_TAB: {
    int pos = gtcaca_editor_get_current_pos(ed);
    if (pos > 0 && is_word_char(gtcaca_editor_get_char_at(ed, pos - 1)))
      complete_at_point(ed);
    else if (g_insert_spaces) { int k; for (k = 0; k < g_indent_size; k++) gtcaca_editor_add_char(ed, ' '); }
    else gtcaca_editor_add_char(ed, '\t');
    return 1;
  }

  /* When a mark is active, the plain arrow/Home/End keys extend the region. */
  case CACA_KEY_LEFT:  if (g_mark_active) { gtcaca_editor_char_left_extend(ed);  return 1; } return 0;
  case CACA_KEY_RIGHT: if (g_mark_active) { gtcaca_editor_char_right_extend(ed); return 1; } return 0;
  case CACA_KEY_UP:    if (g_mark_active) { gtcaca_editor_line_up_extend(ed);    return 1; } return 0;
  case CACA_KEY_DOWN:  if (g_mark_active) { gtcaca_editor_line_down_extend(ed);  return 1; } return 0;
  case CACA_KEY_HOME:  if (g_mark_active) { gtcaca_editor_home_extend(ed);       return 1; } return 0;
  case CACA_KEY_END:   if (g_mark_active) { gtcaca_editor_line_end_extend(ed);   return 1; } return 0;

  default:
    return 0;  /* printable chars, Enter, Backspace: handled by the widget */
  }
}

/* ── modeline ──────────────────────────────────────────────────────────────── */

static void refresh_modeline(gtcaca_editor_widget_t *ed, void *ud)
{
  char text[256];
  int line = gtcaca_editor_get_current_line(ed) + 1;
  int col  = gtcaca_editor_get_column(ed, gtcaca_editor_get_current_pos(ed)) + 1;
  const char *name = g_filename ? g_filename : "*scratch*";
  const char *mod  = gtcaca_editor_get_modify(ed) ? "**" : "--";

  (void)ud;
  show_paren(ed);
  /* keep fold structure in step with edits */
  if (gtcaca_editor_get_json_mode(ed)) gtcaca_editor_fold_json(ed);
  else if (g_folding)                  gtcaca_editor_fold_by_indentation(ed);
  if (g_message[0])
    snprintf(text, sizeof(text), " %s %s  (%s)  L%d C%d   %s", mod, name, g_langname, line, col, g_message);
  else
    snprintf(text, sizeof(text), " %s %s  (%s)  L%d C%d   C-x C-s save  C-x C-c quit  C-g cancel",
             mod, name, g_langname, line, col);

  gtcaca_statusbar_set_text(g_modeline, text);
}

/* ── file loading ──────────────────────────────────────────────────────────── */

static char *read_file(const char *path)
{
  FILE *f = fopen(path, "r");
  long n;
  char *buf;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) n = 0;
  buf = malloc((size_t)n + 1);
  if (buf) { size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
  fclose(f);
  return buf;
}

/* ── configuration (~/.cacamacs/config.json) ───────────────────────────────── */

static int _json_int(gtcaca_json_value *o, const char *key, int dflt)
{
  gtcaca_json_value *v = gtcaca_json_object_get(o, key);
  return (v && v->type == GTCACA_JSON_NUMBER) ? (int)v->u.number : dflt;
}
static int _json_bool(gtcaca_json_value *o, const char *key, int dflt)
{
  gtcaca_json_value *v = gtcaca_json_object_get(o, key);
  return (v && v->type == GTCACA_JSON_BOOL) ? v->u.boolean : dflt;
}

static void load_config(void)
{
  const char *home = getenv("HOME");
  char path[1024];
  gtcaca_json_value *root, *langs;
  if (!home) return;
  snprintf(path, sizeof path, "%s/.cacamacs/config.json", home);
  root = gtcaca_json_parse_file(path);
  if (!root) return;   /* keep built-in defaults */

  g_cfg_tab    = _json_int(root, "tabSize", g_cfg_tab);
  g_cfg_spaces = _json_bool(root, "insertSpaces", g_cfg_spaces);
  g_cfg_indent = _json_int(root, "indentSize", g_cfg_indent);
  g_cfg_edge   = _json_int(root, "edgeColumn", g_cfg_edge);

  /* per-language overrides keyed by file extension, e.g. ".py": { ... } */
  langs = gtcaca_json_object_get(root, "languages");
  if (langs && langs->type == GTCACA_JSON_OBJECT) {
    int i;
    for (i = 0; i < langs->u.object.count && g_noverrides < 32; i++) {
      gtcaca_json_value *o = langs->u.object.vals[i];
      lang_override_t *ov = &g_overrides[g_noverrides];
      if (!o || o->type != GTCACA_JSON_OBJECT) continue;
      strncpy(ov->ext, langs->u.object.keys[i], sizeof ov->ext - 1);
      ov->ext[sizeof ov->ext - 1] = '\0';
      ov->has_ts = gtcaca_json_object_get(o, "tabSize") != NULL;     ov->tab_size = _json_int(o, "tabSize", g_cfg_tab);
      ov->has_is = gtcaca_json_object_get(o, "insertSpaces") != NULL; ov->insert_spaces = _json_bool(o, "insertSpaces", g_cfg_spaces);
      ov->has_id = gtcaca_json_object_get(o, "indentSize") != NULL;  ov->indent_size = _json_int(o, "indentSize", g_cfg_indent);
      g_noverrides++;
    }
  }
  gtcaca_json_free(root);
}

/* Compute the effective indentation for a file extension and apply tab width. */
static void apply_indent_settings(const char *ext)
{
  int i;
  g_tab_size = g_cfg_tab; g_insert_spaces = g_cfg_spaces; g_indent_size = g_cfg_indent;
  if (ext) {
    for (i = 0; i < g_noverrides; i++) {
      if (!strcmp(g_overrides[i].ext, ext)) {
        if (g_overrides[i].has_ts) g_tab_size      = g_overrides[i].tab_size;
        if (g_overrides[i].has_is) g_insert_spaces = g_overrides[i].insert_spaces;
        if (g_overrides[i].has_id) g_indent_size   = g_overrides[i].indent_size;
        break;
      }
    }
  }
  if (g_ed) gtcaca_editor_set_tab_width(g_ed, g_tab_size > 0 ? g_tab_size : 8);
}

/* ── language detection (VSCode-style extensions) ──────────────────────────── */

/* Built-in keyword lists, keyed by VSCode language id. Comments/brackets/
   strings come from the language-configuration.json; keywords are not part of
   that file (they live in TextMate grammars), so we ship a few common sets. */
static void apply_keywords(gtcaca_editor_langcfg_t *cfg, const char *langid)
{
  static const char *kw_c[] = {
    "auto","break","case","char","const","continue","default","do","double",
    "else","enum","extern","float","for","goto","if","inline","int","long",
    "register","restrict","return","short","signed","sizeof","static","struct",
    "switch","typedef","union","unsigned","void","volatile","while","bool" };
  static const char *kw_js[] = {
    "async","await","break","case","catch","class","const","continue","debugger",
    "default","delete","do","else","export","extends","false","finally","for",
    "function","if","import","in","instanceof","let","new","null","return","super",
    "switch","this","throw","true","try","typeof","var","void","while","with","yield" };
  static const char *kw_py[] = {
    "and","as","assert","async","await","break","class","continue","def","del",
    "elif","else","except","False","finally","for","from","global","if","import",
    "in","is","lambda","None","nonlocal","not","or","pass","raise","return","True",
    "try","while","with","yield" };

  const char **chosen = NULL;
  int n = 0;

  if (!langid) return;
  if (!strcmp(langid, "c") || !strcmp(langid, "cpp") || !strcmp(langid, "c++")) {
    chosen = kw_c; n = (int)(sizeof kw_c / sizeof *kw_c);
  } else if (!strcmp(langid, "javascript") || !strcmp(langid, "typescript") ||
             !strcmp(langid, "javascriptreact") || !strcmp(langid, "typescriptreact")) {
    chosen = kw_js; n = (int)(sizeof kw_js / sizeof *kw_js);
  } else if (!strcmp(langid, "python")) {
    chosen = kw_py; n = (int)(sizeof kw_py / sizeof *kw_py);
  }

  if (chosen) {
    gtcaca_editor_langcfg_set_keywords(cfg, chosen, n);   /* for colourization */
    g_keywords = chosen; g_n_keywords = n;                /* and for completion */
  }
}

/* ── completion: gather candidates from the document + language keywords ───── */

static int _cmp_str(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

static void _cand_add(char ***arr, int *n, int *cap, const char *s, int slen)
{
  int i;
  for (i = 0; i < *n; i++) if (strncmp((*arr)[i], s, (size_t)slen) == 0 && (*arr)[i][slen] == '\0') return; /* dup */
  if (*n == *cap) {
    int nc = *cap ? *cap * 2 : 32;
    char **na = realloc(*arr, (size_t)nc * sizeof(char *));
    if (!na) return;
    *arr = na; *cap = nc;
  }
  (*arr)[*n] = malloc((size_t)slen + 1);
  if ((*arr)[*n]) { memcpy((*arr)[*n], s, (size_t)slen); (*arr)[*n][slen] = '\0'; (*n)++; }
}

static void complete_at_point(gtcaca_editor_widget_t *ed)
{
  int pos = gtcaca_editor_get_current_pos(ed);
  int start = pos, plen, len, i;
  char prefix[128], *doc;
  char **cand = NULL; int ncand = 0, cap = 0;

  while (start > 0 && is_word_char(gtcaca_editor_get_char_at(ed, start - 1))) start--;
  plen = pos - start;
  if (plen <= 0) { snprintf(g_message, sizeof g_message, "Type a prefix to complete"); return; }
  gtcaca_editor_get_text_range(ed, start, pos, prefix, sizeof prefix);

  /* candidates from identifiers already in the document */
  len = gtcaca_editor_get_length(ed);
  doc = malloc((size_t)len + 1);
  if (doc) {
    gtcaca_editor_get_text(ed, doc, len + 1);
    i = 0;
    while (i < len) {
      if (isalpha((unsigned char)doc[i]) || doc[i] == '_' || doc[i] == '$') {
        int s = i;
        while (i < len && is_word_char(doc[i])) i++;
        if (i - s > plen && strncmp(doc + s, prefix, (size_t)plen) == 0)
          _cand_add(&cand, &ncand, &cap, doc + s, i - s);
      } else i++;
    }
    free(doc);
  }

  /* candidates from the language's keyword list */
  for (i = 0; i < g_n_keywords; i++) {
    int kl = (int)strlen(g_keywords[i]);
    if (kl > plen && strncmp(g_keywords[i], prefix, (size_t)plen) == 0)
      _cand_add(&cand, &ncand, &cap, g_keywords[i], kl);
  }

  if (ncand == 0) { snprintf(g_message, sizeof g_message, "No completions for '%s'", prefix); }
  else {
    /* join (sorted) into a single space-separated list and show it */
    int total = 0, off = 0;
    char *list;
    qsort(cand, (size_t)ncand, sizeof(char *), _cmp_str);
    for (i = 0; i < ncand; i++) total += (int)strlen(cand[i]) + 1;
    list = malloc((size_t)total + 1);
    if (list) {
      for (i = 0; i < ncand; i++) { int l = (int)strlen(cand[i]); if (i) list[off++] = ' '; memcpy(list + off, cand[i], (size_t)l); off += l; }
      list[off] = '\0';
      gtcaca_editor_autoc_show(ed, plen, list);
      free(list);
      snprintf(g_message, sizeof g_message, "%d completion%s", ncand, ncand == 1 ? "" : "s");
    }
  }
  for (i = 0; i < ncand; i++) free(cand[i]);
  free(cand);
}

/* Guess a language id from a file extension (incl. dot), for keyword selection
   when none was reported by the extension manifest. */
static const char *langid_from_ext(const char *ext)
{
  if (!ext) return NULL;
  if (!strcmp(ext, ".c") || !strcmp(ext, ".h")) return "c";
  if (!strcmp(ext, ".cpp") || !strcmp(ext, ".cc") || !strcmp(ext, ".hpp")) return "cpp";
  if (!strcmp(ext, ".js") || !strcmp(ext, ".mjs")) return "javascript";
  if (!strcmp(ext, ".ts")) return "typescript";
  if (!strcmp(ext, ".py")) return "python";
  return NULL;
}

/*
 * Look through ~/.cacamacs/extensions for a contributed language whose
 * `extensions` include the opened file's suffix, then load its `configuration`
 * (the language-configuration.json). Mirrors how VSCode resolves a language
 * from installed extensions.
 */

/* Extension roots, scanned in order. The first is cacamacs' own; the rest are
   real editor extension folders so installed VSCode extensions work directly. */
static const char *g_ext_roots[] = {
  "/.cacamacs/extensions",
  "/.vscode/extensions",
  "/.vscode-oss/extensions",
  "/.cursor/extensions",
};
#define N_EXT_ROOTS ((int)(sizeof g_ext_roots / sizeof *g_ext_roots))

/* Try one extension directory: read <dir>/package.json and, if one of its
   contributed languages matches `ext`, load its configuration relative to dir. */
static gtcaca_editor_langcfg_t *try_extension(const char *dir, const char *ext,
                                              char *out_id, int idsz)
{
  char pkgpath[1024];
  gtcaca_json_value *pkg, *langs;
  gtcaca_editor_langcfg_t *result = NULL;
  int i, nlang;

  snprintf(pkgpath, sizeof pkgpath, "%s/package.json", dir);
  pkg = gtcaca_json_parse_file(pkgpath);
  if (!pkg) return NULL;

  langs = gtcaca_json_object_get(gtcaca_json_object_get(pkg, "contributes"), "languages");
  nlang = gtcaca_json_array_size(langs);
  for (i = 0; i < nlang && !result; i++) {
    gtcaca_json_value *L = gtcaca_json_array_get(langs, i);
    gtcaca_json_value *exts = gtcaca_json_object_get(L, "extensions");
    int j, m = gtcaca_json_array_size(exts), matched = 0;
    const char *cfg, *id;

    for (j = 0; j < m; j++) {
      const char *s = gtcaca_json_string(gtcaca_json_array_get(exts, j));
      if (s && strcmp(s, ext) == 0) { matched = 1; break; }
    }
    if (!matched) continue;

    cfg = gtcaca_json_string(gtcaca_json_object_get(L, "configuration"));
    id  = gtcaca_json_string(gtcaca_json_object_get(L, "id"));
    if (cfg) {
      char cfgpath[1024];
      const char *rel = cfg;
      gtcaca_editor_langcfg_t *lc;
      if (rel[0] == '.' && rel[1] == '/') rel += 2;
      snprintf(cfgpath, sizeof cfgpath, "%s/%s", dir, rel);
      lc = gtcaca_editor_langcfg_new();
      if (lc && gtcaca_editor_langcfg_load_json(lc, cfgpath) == 0) {
        result = lc;
        if (id && out_id) { strncpy(out_id, id, idsz - 1); out_id[idsz - 1] = '\0'; }
      } else {
        gtcaca_editor_langcfg_free(lc);
      }
    }
  }
  gtcaca_json_free(pkg);
  return result;
}

/* Scan one root dir: a package.json dropped straight in, then one folder per
   extension (the normal VSCode layout). */
static gtcaca_editor_langcfg_t *scan_root_langcfg(const char *root, const char *ext,
                                                  char *out_id, int idsz)
{
  DIR *d;
  struct dirent *de;
  gtcaca_editor_langcfg_t *result = try_extension(root, ext, out_id, idsz);
  if (result) return result;
  d = opendir(root);
  if (!d) return NULL;
  while ((de = readdir(d)) != NULL && !result) {
    char sub[1024];
    if (de->d_name[0] == '.') continue;
    snprintf(sub, sizeof sub, "%s/%s", root, de->d_name);
    result = try_extension(sub, ext, out_id, idsz);
  }
  closedir(d);
  return result;
}

static gtcaca_editor_langcfg_t *discover_langcfg(const char *filename,
                                                 char *out_id, int idsz)
{
  const char *home = getenv("HOME");
  const char *ext  = filename ? strrchr(filename, '.') : NULL;
  int r;
  if (!home || !ext) return NULL;
  for (r = 0; r < N_EXT_ROOTS; r++) {
    char extdir[1024];
    gtcaca_editor_langcfg_t *res;
    snprintf(extdir, sizeof extdir, "%s%s", home, g_ext_roots[r]);
    res = scan_root_langcfg(extdir, ext, out_id, idsz);
    if (res) return res;
  }
  return NULL;
}

/* Find a TextMate grammar (contributes.grammars[].path) in <dir> whose language
   matches the file's extension. Fills out_path / out_id; returns 1 if found. */
static int try_grammar(const char *dir, const char *ext, char *out_path, int psz, char *out_id, int idsz)
{
  char pkgpath[1024];
  gtcaca_json_value *pkg, *langs, *grammars;
  char id[64] = "";
  int i, nlang, ng, found = 0;

  snprintf(pkgpath, sizeof pkgpath, "%s/package.json", dir);
  pkg = gtcaca_json_parse_file(pkgpath);
  if (!pkg) return 0;

  /* which language id owns this extension? */
  langs = gtcaca_json_object_get(gtcaca_json_object_get(pkg, "contributes"), "languages");
  nlang = gtcaca_json_array_size(langs);
  for (i = 0; i < nlang && !id[0]; i++) {
    gtcaca_json_value *L = gtcaca_json_array_get(langs, i);
    gtcaca_json_value *exts = gtcaca_json_object_get(L, "extensions");
    int j, m = gtcaca_json_array_size(exts);
    for (j = 0; j < m; j++) {
      const char *s = gtcaca_json_string(gtcaca_json_array_get(exts, j));
      if (s && !strcmp(s, ext)) {
        const char *lid = gtcaca_json_string(gtcaca_json_object_get(L, "id"));
        if (lid) { strncpy(id, lid, sizeof id - 1); id[sizeof id - 1] = '\0'; }
        break;
      }
    }
  }
  if (!id[0]) { gtcaca_json_free(pkg); return 0; }

  /* a grammar contributed for that language id */
  grammars = gtcaca_json_object_get(gtcaca_json_object_get(pkg, "contributes"), "grammars");
  ng = gtcaca_json_array_size(grammars);
  for (i = 0; i < ng && !found; i++) {
    gtcaca_json_value *G = gtcaca_json_array_get(grammars, i);
    const char *glang = gtcaca_json_string(gtcaca_json_object_get(G, "language"));
    const char *gpath = gtcaca_json_string(gtcaca_json_object_get(G, "path"));
    if (glang && gpath && !strcmp(glang, id)) {
      const char *rel = gpath;
      if (rel[0] == '.' && rel[1] == '/') rel += 2;
      snprintf(out_path, psz, "%s/%s", dir, rel);
      if (out_id) { strncpy(out_id, id, idsz - 1); out_id[idsz - 1] = '\0'; }
      found = 1;
    }
  }
  gtcaca_json_free(pkg);
  return found;
}

static int scan_root_grammar(const char *root, const char *ext, char *out_path, int psz, char *out_id, int idsz)
{
  DIR *d;
  struct dirent *de;
  int found;
  if (try_grammar(root, ext, out_path, psz, out_id, idsz)) return 1;
  d = opendir(root);
  if (!d) return 0;
  found = 0;
  while ((de = readdir(d)) != NULL && !found) {
    char sub[1024];
    if (de->d_name[0] == '.') continue;
    snprintf(sub, sizeof sub, "%s/%s", root, de->d_name);
    found = try_grammar(sub, ext, out_path, psz, out_id, idsz);
  }
  closedir(d);
  return found;
}

static int discover_grammar(const char *filename, char *out_path, int psz, char *out_id, int idsz)
{
  const char *home = getenv("HOME");
  const char *ext  = filename ? strrchr(filename, '.') : NULL;
  int r;
  if (!home || !ext) return 0;
  for (r = 0; r < N_EXT_ROOTS; r++) {
    char extdir[1024];
    snprintf(extdir, sizeof extdir, "%s%s", home, g_ext_roots[r]);
    if (scan_root_grammar(extdir, ext, out_path, psz, out_id, idsz)) return 1;
  }
  return 0;
}

/* Attach colourization to the editor for `filename`, optionally forcing a
   specific language-configuration.json (explicit_cfg). */
static void setup_language(gtcaca_editor_widget_t *ed, const char *filename,
                           const char *explicit_cfg)
{
  const char *ext = filename ? strrchr(filename, '.') : NULL;
  char langid[64] = "", gid[64] = "", gpath[1024] = "";

  /* indentation (tab/space + sizes) for this file's extension */
  apply_indent_settings(ext);

  /* reset modes left over from a previously-open file */
  gtcaca_editor_set_json_mode(ed, 0);
  gtcaca_editor_set_grammar(ed, NULL);
  if (g_grammar) { gtcaca_editor_grammar_free(g_grammar); g_grammar = NULL; }

  /* built-in JSON mode: colour + structural folding, no extension needed */
  if (ext && (!strcmp(ext, ".json") || !strcmp(ext, ".jsonl"))) {
    gtcaca_editor_set_json_mode(ed, 1);
    gtcaca_editor_fold_json(ed);
    gtcaca_editor_set_fold_margin(ed, 1);
    g_folding = 1;
    strncpy(g_langname, !strcmp(ext, ".jsonl") ? "jsonl" : "json", sizeof g_langname - 1);
    g_langname[sizeof g_langname - 1] = '\0';
    return;
  }

  /* Best colourization: a TextMate grammar from an installed extension. */
  if (!explicit_cfg && discover_grammar(filename, gpath, sizeof gpath, gid, sizeof gid)) {
    g_grammar = gtcaca_editor_grammar_load(gpath);
    if (g_grammar) {
      gtcaca_editor_set_grammar(ed, g_grammar);
      gtcaca_editor_style_set_bold(ed, GTCACA_EDITOR_STYLE_KEYWORD, 1);
    }
  }

  /* Language configuration: brackets/comments/strings (fallback colour when no
     grammar) and keywords for completion. */
  if (explicit_cfg) {
    g_langcfg = gtcaca_editor_langcfg_new();
    if (g_langcfg && gtcaca_editor_langcfg_load_json(g_langcfg, explicit_cfg) != 0) {
      gtcaca_editor_langcfg_free(g_langcfg);
      g_langcfg = NULL;
    }
  } else {
    g_langcfg = discover_langcfg(filename, langid, sizeof langid);
  }

  if (g_langcfg) {
    if (!langid[0] && gid[0]) { strncpy(langid, gid, sizeof langid - 1); langid[sizeof langid - 1] = '\0'; }
    if (!langid[0]) { const char *guess = langid_from_ext(ext); if (guess) strncpy(langid, guess, sizeof langid - 1); }
    apply_keywords(g_langcfg, langid);
    gtcaca_editor_set_langcfg(ed, g_langcfg);
    if (!g_grammar) gtcaca_editor_style_set_bold(ed, GTCACA_EDITOR_STYLE_KEYWORD, 1);
  }

  /* language name in the modeline */
  if (gid[0])        { strncpy(g_langname, gid, sizeof g_langname - 1); g_langname[sizeof g_langname - 1] = '\0'; }
  else if (langid[0]){ strncpy(g_langname, langid, sizeof g_langname - 1); g_langname[sizeof g_langname - 1] = '\0'; }

  if (!g_grammar && !g_langcfg && ext)
    snprintf(g_message, sizeof g_message,
             "No language for '%s' in ~/.cacamacs/extensions — colourization off", ext);
}

/* ── file browser (open a directory to navigate it) ────────────────────────── */

typedef struct { char name[256]; int isdir; } bent_t;

static int _cmp_bent(const void *a, const void *b)
{
  const bent_t *x = a, *y = b;
  if (x->isdir != y->isdir) return y->isdir - x->isdir;   /* directories first */
  return strcmp(x->name, y->name);
}

/* Render the directory listing into the read-only browser editor. */
static void populate_browser(const char *dir)
{
  DIR *d = opendir(dir);
  struct dirent *de;
  bent_t ents[2048];
  int n = 0, i, total, len;
  char *listing;
  char resolved[PATH_MAX];

  /* Canonicalise into a SEPARATE buffer first: `dir` may alias g_curdir
     (show_browser passes g_curdir), and realpath() with src==dst is UB. */
  if (realpath(dir, resolved)) { strncpy(g_curdir, resolved, sizeof g_curdir - 1); g_curdir[sizeof g_curdir - 1] = '\0'; }
  else if (dir != g_curdir)    { strncpy(g_curdir, dir, sizeof g_curdir - 1);      g_curdir[sizeof g_curdir - 1] = '\0'; }

  if (d) {
    while ((de = readdir(d)) != NULL && n < 2048) {
      int isdir;
      if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
      isdir = (de->d_type == DT_DIR);
      if (de->d_type == DT_UNKNOWN) {
        struct stat st; char p[PATH_MAX];
        int need = snprintf(p, sizeof p, "%s/%s", g_curdir, de->d_name);
        if (need > 0 && need < (int)sizeof p && stat(p, &st) == 0) isdir = S_ISDIR(st.st_mode);
      }
      strncpy(ents[n].name, de->d_name, sizeof ents[n].name - 1);
      ents[n].name[sizeof ents[n].name - 1] = '\0';
      ents[n].isdir = isdir;
      n++;
    }
    closedir(d);
    qsort(ents, (size_t)n, sizeof(bent_t), _cmp_bent);
  }

  /* one entry per line; ".." first so you can always go up */
  total = 3;  /* "../" */
  for (i = 0; i < n; i++) total += 1 + (int)strlen(ents[i].name) + (ents[i].isdir ? 1 : 0);
  listing = malloc((size_t)total + 1);
  if (!listing) return;
  strcpy(listing, "../"); len = 3;
  for (i = 0; i < n; i++) {
    listing[len++] = '\n';
    strcpy(listing + len, ents[i].name); len += (int)strlen(ents[i].name);
    if (ents[i].isdir) listing[len++] = '/';
  }
  listing[len] = '\0';

  gtcaca_editor_set_read_only(g_browser_ed, 0);
  gtcaca_editor_set_text(g_browser_ed, listing);
  gtcaca_editor_set_read_only(g_browser_ed, 1);
  gtcaca_editor_goto_pos(g_browser_ed, 0);
  free(listing);

  g_browser_win->window_title = g_curdir;
  if (!d) snprintf(g_message, sizeof g_message, "Cannot read %s", dir);
  else    snprintf(g_message, sizeof g_message, "%d item%s", n, n == 1 ? "" : "s");
}

static int g_browser_open = 0;

static void show_browser(void)
{
  if (!g_curdir[0]) { if (!realpath(".", g_curdir)) strcpy(g_curdir, "."); }
  pane_hide_all();                 /* modal: only the browser is visible */
  gtcaca_widget_show(GTCACA_WIDGET(g_browser_win));
  gtcaca_widget_show(GTCACA_WIDGET(g_browser_ed));
  populate_browser(g_curdir);
  gtcaca_window_set_focus(g_browser_win);
  gtcaca_window_set_focused_child(g_browser_win, GTCACA_WIDGET(g_browser_ed));
  g_browser_open = 1;
}

static void hide_browser(void)
{
  gtcaca_widget_hide(GTCACA_WIDGET(g_browser_ed));
  gtcaca_widget_hide(GTCACA_WIDGET(g_browser_win));
  g_browser_open = 0;
  pane_show_all();                 /* restore the editor panes and focus */
}

/* Open whatever entry the caret is on: descend into a directory, or open a file. */
static void browser_open_current(void)
{
  gtcaca_editor_widget_t *ed = g_browser_ed;
  int line = gtcaca_editor_get_current_line(ed);
  int s = gtcaca_editor_position_from_line(ed, line);
  int e = gtcaca_editor_get_line_end_position(ed, line);
  char entry[NAME_MAX + 2], path[PATH_MAX];
  struct stat st;
  size_t l;
  int need;

  gtcaca_editor_get_text_range(ed, s, e, entry, sizeof entry);
  if (entry[0] == '\0') return;
  if (!strcmp(entry, "../")) need = snprintf(path, sizeof path, "%s/..", g_curdir);
  else {
    l = strlen(entry); if (l && entry[l - 1] == '/') entry[l - 1] = '\0';
    need = snprintf(path, sizeof path, "%s/%s", g_curdir, entry);
  }
  if (need < 0 || need >= (int)sizeof path) { snprintf(g_message, sizeof g_message, "Path too long"); return; }
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) populate_browser(path);
  else                                             open_path_in_editor(path);
}

/* Modeline shown while the browser is focused. */
static void browser_modeline(gtcaca_editor_widget_t *ed, void *ud)
{
  char t[256];
  (void)ed; (void)ud;
  if (g_message[0]) snprintf(t, sizeof t, "  Browser  %s   %s", g_curdir, g_message);
  else snprintf(t, sizeof t, "  Browser  %s   arrows move  C-s search  Enter open  q quit", g_curdir);
  gtcaca_statusbar_set_text(g_modeline, t);
}

/* ── buffers & split panes ─────────────────────────────────────────────────── */

static int buf_index_of(gtcaca_editor_widget_t *ed)
{
  int i; for (i = 0; i < g_nbuf; i++) if (g_buffers[i].ed == ed) return i; return -1;
}

/* The focused buffer's per-file state lives in the globals while focused; these
   move it to/from the buffer record when focus changes. */
static void buffer_store_globals(int bi)
{
  buffer_t *b;
  if (bi < 0 || bi >= g_nbuf) return;
  b = &g_buffers[bi];
  b->langcfg = g_langcfg; b->grammar = g_grammar;
  b->keywords = g_keywords; b->n_keywords = g_n_keywords; b->folding = g_folding;
  strncpy(b->langname, g_langname, sizeof b->langname - 1); b->langname[sizeof b->langname - 1] = '\0';
}
static void buffer_load_globals(int bi)
{
  buffer_t *b;
  if (bi < 0 || bi >= g_nbuf) return;
  b = &g_buffers[bi];
  g_ed = b->ed; g_langcfg = b->langcfg; g_grammar = b->grammar;
  g_keywords = b->keywords; g_n_keywords = b->n_keywords; g_folding = b->folding;
  strncpy(g_langname, b->langname, sizeof g_langname - 1); g_langname[sizeof g_langname - 1] = '\0';
  g_filename = b->has_file ? b->path : NULL;
}

/* Create a buffer for `path` (NULL = scratch); dedups by path. Sets up the
   buffer's language without disturbing the currently-focused globals. */
static int buffer_create(const char *path)
{
  buffer_t *b;
  int bi, i;

  if (path) for (i = 0; i < g_nbuf; i++)
    if (g_buffers[i].has_file && !strcmp(g_buffers[i].path, path)) return i;
  if (g_nbuf >= MAXBUF) return -1;
  bi = g_nbuf++;
  b = &g_buffers[bi];
  memset(b, 0, sizeof *b);
  b->pane = -1;
  strcpy(b->langname, "fundamental");
  b->ed = gtcaca_editor_new(NULL, 0, 0, 1, 1);
  gtcaca_editor_set_line_numbers(b->ed, 1);
  gtcaca_editor_key_cb_register(b->ed, on_key, NULL);
  gtcaca_editor_set_update_cb(b->ed, refresh_modeline, NULL);
  gtcaca_editor_set_caret_line_visible(b->ed, 1);
  gtcaca_editor_set_caret_line_back(b->ed, CACA_BLACK);
  gtcaca_editor_set_edge_column(b->ed, g_cfg_edge);
  gtcaca_editor_set_tab_width(b->ed, g_cfg_tab > 0 ? g_cfg_tab : 8);
  gtcaca_widget_hide(GTCACA_WIDGET(b->ed));
  if (path) {
    char *c = read_file(path);
    gtcaca_editor_set_text(b->ed, c ? c : ""); free(c);
    strncpy(b->path, path, sizeof b->path - 1); b->path[sizeof b->path - 1] = '\0';
    b->has_file = 1;
  }
  gtcaca_editor_empty_undo_buffer(b->ed);
  gtcaca_editor_set_save_point(b->ed);

  if (b->has_file) {
    /* run language setup with globals temporarily pointed at this buffer */
    gtcaca_editor_widget_t *sed = g_ed; const char *sfn = g_filename;
    gtcaca_editor_langcfg_t *slc = g_langcfg; gtcaca_editor_grammar_t *sgr = g_grammar;
    const char **skw = g_keywords; int snk = g_n_keywords, sfold = g_folding; char sln[64];
    strncpy(sln, g_langname, sizeof sln - 1); sln[sizeof sln - 1] = '\0';

    g_ed = b->ed; g_filename = b->path; g_langcfg = NULL; g_grammar = NULL;
    g_keywords = NULL; g_n_keywords = 0; strcpy(g_langname, "fundamental"); g_folding = 0;
    setup_language(b->ed, b->path, NULL);
    buffer_store_globals(bi);

    g_ed = sed; g_filename = sfn; g_langcfg = slc; g_grammar = sgr;
    g_keywords = skw; g_n_keywords = snk; g_folding = sfold;
    strncpy(g_langname, sln, sizeof g_langname - 1); g_langname[sizeof g_langname - 1] = '\0';
  }
  return bi;
}

/* Position a pane's buffer editor to fill its slot window. */
/* ── split-tree node pool ──────────────────────────────────────────────────── */

static int node_alloc(void)
{
  int i;
  for (i = 0; i < MAXNODES; i++) if (!g_nodes[i].used) {
    memset(&g_nodes[i], 0, sizeof g_nodes[i]);
    g_nodes[i].used = 1; g_nodes[i].parent = -1; g_nodes[i].buf = -1; g_nodes[i].win = -1;
    g_nodes[i].child[0] = g_nodes[i].child[1] = -1;
    return i;
  }
  return -1;
}
static int win_alloc(void) { int i; for (i = 0; i < MAXLEAVES; i++) if (!g_win_used[i]) { g_win_used[i] = 1; return i; } return -1; }

/* Collect leaf node indices, left-to-right (in the order they tile). */
static void collect_leaves(int n, int *out, int *cnt)
{
  if (n < 0 || !g_nodes[n].used) return;
  if (g_nodes[n].is_leaf) { if (*cnt < MAXLEAVES) out[(*cnt)++] = n; return; }
  collect_leaves(g_nodes[n].child[0], out, cnt);
  collect_leaves(g_nodes[n].child[1], out, cnt);
}
static int leftmost_leaf(int n) { while (n >= 0 && !g_nodes[n].is_leaf) n = g_nodes[n].child[0]; return n; }

static void position_leaf(int n)
{
  node_t *nd = &g_nodes[n];
  gtcaca_window_widget_t *win = g_winpool[nd->win];
  gtcaca_box_t *bx;
  win->x = nd->x; win->y = nd->y; win->width = nd->w; win->height = nd->h;
  gtcaca_widget_show(GTCACA_WIDGET(win));
  if (nd->buf < 0) return;
  {
    buffer_t *b = &g_buffers[nd->buf];
    b->ed->parent = GTCACA_WIDGET(win); b->pane = n;
    win->focused_child = GTCACA_WIDGET(b->ed);   /* never leave it stale */
    gtcaca_widget_show(GTCACA_WIDGET(b->ed));
    bx = gtcaca_vbox_new(); gtcaca_box_set_margin(bx, 0);
    gtcaca_box_add_expand(bx, GTCACA_WIDGET(b->ed));
    gtcaca_box_apply(bx, nd->x, nd->y, nd->w, nd->h);
    gtcaca_box_free(bx);
    win->window_title = b->has_file ? b->path : "*scratch*";
  }
}

static void layout_node(int n, int x, int y, int w, int h)
{
  node_t *nd = &g_nodes[n];
  nd->x = x; nd->y = y; nd->w = w; nd->h = h;
  if (nd->is_leaf) { position_leaf(n); return; }
  if (nd->orient) { int h0 = h / 2; layout_node(nd->child[0], x, y, w, h0); layout_node(nd->child[1], x, y + h0, w, h - h0); }
  else            { int w0 = w / 2; layout_node(nd->child[0], x, y, w0, h); layout_node(nd->child[1], x + w0, y, w - w0, h); }
}

static void relayout(void)
{
  int cw = caca_get_canvas_width(gmo.cv), ch = caca_get_canvas_height(gmo.cv), i;
  if (g_root >= 0) layout_node(g_root, 0, 0, cw, ch - 1);
  for (i = 0; i < MAXLEAVES; i++) if (!g_win_used[i] && g_winpool[i]) gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[i]));
}

/* Show buffer bi in leaf node n. */
static void pane_show_buffer(int n, int bi)
{
  if (g_buffers[bi].pane >= 0 && g_buffers[bi].pane != n) g_nodes[g_buffers[bi].pane].buf = -1;
  if (g_nodes[n].buf >= 0 && g_nodes[n].buf != bi) {
    gtcaca_widget_hide(GTCACA_WIDGET(g_buffers[g_nodes[n].buf].ed));
    g_buffers[g_nodes[n].buf].pane = -1;
  }
  g_nodes[n].buf = bi; g_buffers[bi].pane = n;
  position_leaf(n);
}

static void focus_pane(int n)   /* focus a leaf node */
{
  int bi, i;
  if (n < 0 || !g_nodes[n].used || !g_nodes[n].is_leaf) return;
  buffer_store_globals(g_cur_buf);
  g_focus_leaf = n; bi = g_nodes[n].buf; g_cur_buf = bi;
  buffer_load_globals(bi);
  g_win = g_winpool[g_nodes[n].win]; g_ed = g_buffers[bi].ed;
  apply_indent_settings(g_buffers[bi].has_file ? strrchr(g_buffers[bi].path, '.') : NULL);

  /* Focus exactly one window + editor. We manage this directly rather than via
     gtcaca_window_set_focus: window->focused_child pointers go stale when buffer
     editors are reparented across the pool, which can leave two editors focused
     (so a key reaches both — e.g. C-x gets set then consumed). */
  for (i = 0; i < g_nbuf; i++) g_buffers[i].ed->has_focus = 0;
  for (i = 0; i < MAXLEAVES; i++) if (g_winpool[i]) g_winpool[i]->has_focus = 0;
  if (g_browser_win) g_browser_win->has_focus = 0;
  if (g_browser_ed)  g_browser_ed->has_focus = 0;
  g_win->focused_child = GTCACA_WIDGET(g_ed);
  g_win->has_focus = 1;
  g_ed->has_focus = 1;
  refresh_modeline(g_ed, NULL);
}

static void pane_hide_all(void)
{
  int leaves[MAXLEAVES], cnt = 0, i;
  collect_leaves(g_root, leaves, &cnt);
  for (i = 0; i < cnt; i++) {
    node_t *nd = &g_nodes[leaves[i]];
    gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[nd->win]));
    if (nd->buf >= 0) gtcaca_widget_hide(GTCACA_WIDGET(g_buffers[nd->buf].ed));
  }
}
static void pane_show_all(void) { relayout(); focus_pane(g_focus_leaf); }

/* Open `path` into the focused window (creating/reusing a buffer). */
static void open_path_in_editor(const char *path)
{
  int bi = buffer_create(path);
  if (bi < 0) { snprintf(g_message, sizeof g_message, "Too many buffers"); return; }
  if (g_browser_open) {
    gtcaca_widget_hide(GTCACA_WIDGET(g_browser_ed));
    gtcaca_widget_hide(GTCACA_WIDGET(g_browser_win));
    g_browser_open = 0;
  }
  pane_show_buffer(g_focus_leaf, bi);
  relayout();
  focus_pane(g_focus_leaf);
}

/* Split the focused leaf into two; the original stays as the first child. */
static void pane_split(int rows)
{
  int L = g_focus_leaf, c0, c1, wi, newbuf = -1, i;
  if (L < 0 || !g_nodes[L].is_leaf) return;

  for (i = 0; i < g_nbuf; i++) if (g_buffers[i].pane < 0 && i != g_cur_buf) { newbuf = i; break; }
  if (newbuf < 0) { newbuf = buffer_create(NULL); if (newbuf < 0) return; }

  c0 = node_alloc(); c1 = node_alloc(); wi = win_alloc();
  if (c0 < 0 || c1 < 0 || wi < 0) {
    if (c0 >= 0) g_nodes[c0].used = 0; if (c1 >= 0) g_nodes[c1].used = 0; if (wi >= 0) g_win_used[wi] = 0;
    snprintf(g_message, sizeof g_message, "Too many windows"); return;
  }
  /* c0 inherits the original window + buffer */
  g_nodes[c0].is_leaf = 1; g_nodes[c0].parent = L;
  g_nodes[c0].buf = g_nodes[L].buf; g_nodes[c0].win = g_nodes[L].win;
  if (g_nodes[c0].buf >= 0) g_buffers[g_nodes[c0].buf].pane = c0;
  /* c1 is the new window with another buffer */
  g_nodes[c1].is_leaf = 1; g_nodes[c1].parent = L; g_nodes[c1].buf = newbuf; g_nodes[c1].win = wi;
  g_buffers[newbuf].pane = c1;
  /* L becomes the split */
  g_nodes[L].is_leaf = 0; g_nodes[L].orient = rows; g_nodes[L].child[0] = c0; g_nodes[L].child[1] = c1;
  g_nodes[L].buf = -1; g_nodes[L].win = -1;

  g_focus_leaf = c0;
  relayout();
  focus_pane(c0);
}

static void pane_unsplit_one(void)
{
  int leaves[MAXLEAVES], cnt = 0, i, L = g_focus_leaf, buf, wi;
  if (L < 0) return;
  buffer_store_globals(g_cur_buf);
  buf = g_nodes[L].buf; wi = g_nodes[L].win;
  collect_leaves(g_root, leaves, &cnt);
  if (cnt <= 1) return;
  for (i = 0; i < cnt; i++) if (leaves[i] != L) {
    node_t *nd = &g_nodes[leaves[i]];
    if (nd->buf >= 0) { gtcaca_widget_hide(GTCACA_WIDGET(g_buffers[nd->buf].ed)); g_buffers[nd->buf].pane = -1; }
    if (nd->win >= 0) { g_win_used[nd->win] = 0; gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[nd->win])); }
  }
  memset(g_nodes, 0, sizeof g_nodes);
  for (i = 0; i < MAXLEAVES; i++) g_win_used[i] = 0;
  g_win_used[wi] = 1;
  g_root = 0;
  g_nodes[0].used = 1; g_nodes[0].is_leaf = 1; g_nodes[0].parent = -1;
  g_nodes[0].buf = buf; g_nodes[0].win = wi; g_nodes[0].child[0] = g_nodes[0].child[1] = -1;
  if (buf >= 0) g_buffers[buf].pane = 0;
  g_focus_leaf = 0;
  relayout();
  focus_pane(0);
}

static void pane_delete_current(void)
{
  int L = g_focus_leaf, P, sib, GP, f;
  node_t s;
  if (L < 0) return;
  P = g_nodes[L].parent;
  if (P < 0) { snprintf(g_message, sizeof g_message, "Only one window"); return; }
  sib = (g_nodes[P].child[0] == L) ? g_nodes[P].child[1] : g_nodes[P].child[0];
  buffer_store_globals(g_cur_buf);
  if (g_nodes[L].buf >= 0) { gtcaca_widget_hide(GTCACA_WIDGET(g_buffers[g_nodes[L].buf].ed)); g_buffers[g_nodes[L].buf].pane = -1; }
  if (g_nodes[L].win >= 0) { g_win_used[g_nodes[L].win] = 0; gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[g_nodes[L].win])); }
  g_nodes[L].used = 0;
  /* collapse: the sibling replaces the parent in place */
  GP = g_nodes[P].parent;
  s = g_nodes[sib];
  g_nodes[P] = s; g_nodes[P].used = 1; g_nodes[P].parent = GP;
  if (g_nodes[P].is_leaf) { if (g_nodes[P].buf >= 0) g_buffers[g_nodes[P].buf].pane = P; }
  else { g_nodes[g_nodes[P].child[0]].parent = P; g_nodes[g_nodes[P].child[1]].parent = P; }
  g_nodes[sib].used = 0;
  f = leftmost_leaf(P);
  g_focus_leaf = f;
  relayout();
  focus_pane(f);
}

static void pane_other_window(void)
{
  int leaves[MAXLEAVES], cnt = 0, i, idx = 0;
  collect_leaves(g_root, leaves, &cnt);
  if (cnt <= 1) { snprintf(g_message, sizeof g_message, "Only one window"); return; }
  for (i = 0; i < cnt; i++) if (leaves[i] == g_focus_leaf) { idx = i; break; }
  focus_pane(leaves[(idx + 1) % cnt]);
}

static void pane_switch_buffer(void)
{
  int k, bi = -1;
  for (k = 1; k <= g_nbuf; k++) {
    int c = (g_cur_buf + k) % g_nbuf;
    if (c != g_cur_buf && g_buffers[c].pane < 0) { bi = c; break; }
  }
  if (bi < 0) { snprintf(g_message, sizeof g_message, "No other buffer"); return; }
  pane_show_buffer(g_focus_leaf, bi);
  focus_pane(g_focus_leaf);
  snprintf(g_message, sizeof g_message, "%s", g_buffers[bi].has_file ? g_buffers[bi].path : "*scratch*");
}

int main(int argc, char **argv)
{
  int cw, ch, i, b0;
  const char *arg = argc > 1 ? argv[1] : NULL;
  const char *open_file_path = NULL;
  int open_dir = 0;

  gtcaca_init(&argc, &argv);
  gtcaca_application_new("cacamacs");
  load_config();

  if (arg) {
    struct stat st;
    if (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) open_dir = 1;
    else open_file_path = arg;
  }

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);

  /* pane window pool (hidden); a leaf uses one of these by index */
  for (i = 0; i < MAXLEAVES; i++) {
    g_winpool[i] = gtcaca_window_new(NULL, "", 0, 0, cw, ch - 1);
    gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[i]));
    g_win_used[i] = 0;
  }

  /* browser pop-up (created after the panes; modal, so order is fine) */
  {
    int bw = cw * 4 / 5, bh = (ch - 1) * 4 / 5;
    if (bw < 24) bw = cw > 24 ? cw - 2 : cw;
    if (bh < 6)  bh = ch - 2;
    g_browser_win = gtcaca_window_new_centered(NULL, "Browser", bw, bh);
    g_browser_ed = gtcaca_editor_new(GTCACA_WIDGET(g_browser_win), 0, 0, 1, 1);
    gtcaca_editor_set_line_numbers(g_browser_ed, 0);
    gtcaca_editor_set_caret_line_visible(g_browser_ed, 1);
    gtcaca_editor_set_caret_line_back(g_browser_ed, CACA_BLUE);
    gtcaca_editor_set_read_only(g_browser_ed, 1);
    gtcaca_editor_key_cb_register(g_browser_ed, on_key, NULL);
    gtcaca_editor_set_update_cb(g_browser_ed, browser_modeline, NULL);
    { gtcaca_box_t *bb = gtcaca_vbox_new(); gtcaca_box_set_margin(bb, 0);
      gtcaca_box_add_expand(bb, GTCACA_WIDGET(g_browser_ed));
      gtcaca_box_apply(bb, g_browser_win->x, g_browser_win->y,
                       g_browser_win->width, g_browser_win->height);
      gtcaca_box_free(bb); }
    gtcaca_widget_hide(GTCACA_WIDGET(g_browser_ed));
    gtcaca_widget_hide(GTCACA_WIDGET(g_browser_win));
  }

  g_modeline = gtcaca_statusbar_new("");

  /* initial buffer in a single (root) leaf window */
  g_cur_buf = -1;
  b0 = buffer_create(open_file_path);   /* NULL => scratch */
  if (b0 < 0) b0 = buffer_create(NULL);
  memset(g_nodes, 0, sizeof g_nodes);
  g_root = 0;
  g_nodes[0].used = 1; g_nodes[0].is_leaf = 1; g_nodes[0].parent = -1;
  g_nodes[0].buf = b0; g_nodes[0].win = win_alloc();
  g_nodes[0].child[0] = g_nodes[0].child[1] = -1;
  g_buffers[b0].pane = 0;
  g_focus_leaf = 0;
  relayout();
  focus_pane(0);

  if (open_dir) {
    if (!realpath(arg, g_curdir)) { strncpy(g_curdir, arg, sizeof g_curdir - 1); g_curdir[sizeof g_curdir - 1] = '\0'; }
    show_browser();
  }

  gtcaca_main();

  buffer_store_globals(g_cur_buf);
  for (i = 0; i < g_nbuf; i++) {
    gtcaca_editor_langcfg_free(g_buffers[i].langcfg);
    gtcaca_editor_grammar_free(g_buffers[i].grammar);
  }
  free(g_killbuf);
  return 0;
}
