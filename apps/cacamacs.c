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
 *   Motion : C-f C-b C-n C-p  C-a C-e  C-v (page down)  arrows
 *   Edit   : C-d (delete fwd)  C-k (kill line)  Backspace
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

static gtcaca_editor_widget_t    *g_ed       = NULL;
static gtcaca_statusbar_widget_t *g_modeline = NULL;
static gtcaca_window_widget_t    *g_win      = NULL;   /* the editor window  */
static const char                *g_filename = NULL;
static char                       g_open_path[1024] = "";

/* file browser */
static gtcaca_window_widget_t    *g_browser_win = NULL;
static gtcaca_textlist_widget_t  *g_browser     = NULL;
static char                       g_curdir[1024] = "";
static char                     **g_browser_items = NULL;  /* owned entry strings */
static int                        g_browser_n     = 0;
static gtcaca_editor_langcfg_t   *g_langcfg  = NULL;
static gtcaca_editor_grammar_t   *g_grammar  = NULL;
static char                       g_langname[64] = "fundamental";
static const char               **g_keywords = NULL;   /* language keywords, for completion */
static int                        g_n_keywords = 0;
static int                        g_folding = 0;         /* folding mode on? */

/* indentation config (~/.cacamacs/config.json, with per-extension overrides) */
typedef struct { char ext[16]; int tab_size, insert_spaces, indent_size; int has_ts, has_is, has_id; } lang_override_t;
static int             g_cfg_tab = 8, g_cfg_spaces = 1, g_cfg_indent = 2;   /* global defaults */
static lang_override_t g_overrides[32];
static int             g_noverrides = 0;
static int             g_tab_size = 8, g_insert_spaces = 1, g_indent_size = 2; /* effective */

static int  is_word_char(int c) { return isalnum((unsigned char)c) || c == '_' || c == '$'; }
static void complete_at_point(gtcaca_editor_widget_t *ed);   /* defined below */
static void show_browser(void);                              /* defined below */

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
static char  g_mb_prompt[64], g_mb_buf[256];
static int   g_mb_len = 0;
static void (*g_mb_cb)(const char *) = NULL;

static void mb_status(void) { snprintf(g_message, sizeof g_message, "%s%s", g_mb_prompt, g_mb_buf); }

static void start_minibuffer(const char *prompt, void (*cb)(const char *))
{
  g_mb_active = 1; g_mb_cb = cb; g_mb_len = 0; g_mb_buf[0] = '\0';
  strncpy(g_mb_prompt, prompt, sizeof g_mb_prompt - 1); g_mb_prompt[sizeof g_mb_prompt - 1] = '\0';
  mb_status();
}

static int minibuffer_key(int key)
{
  switch (key) {
  case CACA_KEY_RETURN:
  case 10: {
    void (*cb)(const char *) = g_mb_cb;
    char tmp[256];
    g_mb_active = 0; g_message[0] = '\0';
    strncpy(tmp, g_mb_buf, sizeof tmp - 1); tmp[sizeof tmp - 1] = '\0';
    if (cb) cb(tmp);
    return 1;
  }
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

  /* incremental search and the minibuffer take keys first */
  if (g_isearch)   return isearch_key(ed, key);
  if (g_mb_active) return minibuffer_key(key);

  /* second key of a C-x chord */
  if (g_ctrl_x) {
    g_ctrl_x = 0;
    switch (key) {
    case KEY_CTRL_S:        save_file(ed);                return 1;  /* C-x C-s */
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

/* Load the editor with the file at `path` and hide the browser. */
static void open_path_in_editor(const char *path)
{
  char *contents = read_file(path);

  strncpy(g_open_path, path, sizeof g_open_path - 1);
  g_open_path[sizeof g_open_path - 1] = '\0';
  g_filename = g_open_path;

  gtcaca_editor_set_text(g_ed, contents ? contents : "");
  free(contents);
  gtcaca_editor_empty_undo_buffer(g_ed);
  gtcaca_editor_set_save_point(g_ed);
  g_win->window_title = g_open_path;

  /* re-detect the language for the newly opened file */
  if (g_langcfg) {
    gtcaca_editor_set_langcfg(g_ed, NULL);
    gtcaca_editor_langcfg_free(g_langcfg);
    g_langcfg = NULL;
  }
  g_keywords = NULL; g_n_keywords = 0;
  strcpy(g_langname, "fundamental");
  setup_language(g_ed, g_filename, NULL);

  gtcaca_widget_hide(GTCACA_WIDGET(g_browser));
  gtcaca_widget_hide(GTCACA_WIDGET(g_browser_win));
  gtcaca_window_set_focus(g_win);
  gtcaca_window_set_focused_child(g_win, GTCACA_WIDGET(g_ed));
}

static void populate_browser(const char *dir)
{
  DIR *d = opendir(dir);
  struct dirent *de;
  bent_t ents[2048];
  int n = 0, i, rows;

  if (!d) { snprintf(g_message, sizeof g_message, "Cannot open %s", dir); return; }
  if (!realpath(dir, g_curdir)) { strncpy(g_curdir, dir, sizeof g_curdir - 1); g_curdir[sizeof g_curdir - 1] = '\0'; }

  while ((de = readdir(d)) != NULL && n < 2048) {
    int isdir;
    if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
    isdir = (de->d_type == DT_DIR);
    if (de->d_type == DT_UNKNOWN) {
      struct stat st; char p[2300];
      snprintf(p, sizeof p, "%s/%s", g_curdir, de->d_name);
      if (stat(p, &st) == 0) isdir = S_ISDIR(st.st_mode);
    }
    strncpy(ents[n].name, de->d_name, sizeof ents[n].name - 1);
    ents[n].name[sizeof ents[n].name - 1] = '\0';
    ents[n].isdir = isdir;
    n++;
  }
  closedir(d);
  qsort(ents, (size_t)n, sizeof(bent_t), _cmp_bent);

  /* rebuild the list, owning the strings (textlist keeps the pointers) */
  gtcaca_textlist_clear(g_browser);
  for (i = 0; i < g_browser_n; i++) free(g_browser_items[i]);
  free(g_browser_items);
  g_browser_items = malloc((size_t)(n + 1) * sizeof(char *));
  g_browser_n = 0;

  g_browser_items[g_browser_n] = strdup("../");
  gtcaca_textlist_append(g_browser, g_browser_items[g_browser_n++]);
  for (i = 0; i < n; i++) {
    char line[300];
    snprintf(line, sizeof line, "%s%s", ents[i].name, ents[i].isdir ? "/" : "");
    g_browser_items[g_browser_n] = strdup(line);
    gtcaca_textlist_append(g_browser, g_browser_items[g_browser_n++]);
  }

  rows = g_browser_win->height - 2;
  gtcaca_textlist_widget_set_view_size(g_browser, (unsigned int)(rows > 0 ? rows : 1));
  g_browser_win->window_title = g_curdir;
  snprintf(g_message, sizeof g_message, "%d item%s", n, n == 1 ? "" : "s");
}

static void show_browser(void)
{
  if (!g_curdir[0]) { if (!realpath(".", g_curdir)) strcpy(g_curdir, "."); }
  gtcaca_widget_show(GTCACA_WIDGET(g_browser_win));
  gtcaca_widget_show(GTCACA_WIDGET(g_browser));
  populate_browser(g_curdir);
  gtcaca_window_set_focus(g_browser_win);
  gtcaca_window_set_focused_child(g_browser_win, GTCACA_WIDGET(g_browser));
}

static int on_browser_select(gtcaca_textlist_widget_t *tl, int key, void *ud)
{
  char *sel;
  char path[2300];
  struct stat st;
  (void)ud;

  if (key != CACA_KEY_RETURN) return 0;
  sel = gtcaca_textlist_get_text_selected(tl);
  if (!sel) return 0;

  if (!strcmp(sel, "../")) snprintf(path, sizeof path, "%s/..", g_curdir);
  else {
    char name[256];
    strncpy(name, sel, sizeof name - 1); name[sizeof name - 1] = '\0';
    { size_t l = strlen(name); if (l && name[l - 1] == '/') name[l - 1] = '\0'; }  /* strip dir slash */
    snprintf(path, sizeof path, "%s/%s", g_curdir, name);
  }

  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) populate_browser(path);
  else                                             open_path_in_editor(path);
  return 0;
}

int main(int argc, char **argv)
{
  gtcaca_window_widget_t *win;
  gtcaca_box_t           *box;
  int cw, ch;
  char *contents = NULL;
  const char *arg = argc > 1 ? argv[1] : NULL;
  int open_dir = 0;

  gtcaca_init(&argc, &argv);
  gtcaca_application_new("cacamacs");
  load_config();

  if (arg) {
    struct stat st;
    if (stat(arg, &st) == 0 && S_ISDIR(st.st_mode)) open_dir = 1;
    else { g_filename = arg; contents = read_file(arg); }   /* NULL = new file */
  }

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);

  /* editor window */
  win = gtcaca_window_new(NULL, g_filename ? (char *)g_filename : "*scratch*",
                          0, 0, cw, ch - 1);
  g_win = win;

  g_ed = gtcaca_editor_new(GTCACA_WIDGET(win), 0, 0, 1, 1);
  gtcaca_editor_set_line_numbers(g_ed, 1);
  gtcaca_editor_key_cb_register(g_ed, on_key, NULL);
  gtcaca_editor_set_update_cb(g_ed, refresh_modeline, NULL);
  apply_indent_settings(g_filename ? strrchr(g_filename, '.') : NULL);

  if (contents) { gtcaca_editor_set_text(g_ed, contents); free(contents); }
  gtcaca_editor_empty_undo_buffer(g_ed);
  gtcaca_editor_set_save_point(g_ed);

  if (g_filename)   /* a config (argv[2]) wins, else resolve from extensions */
    setup_language(g_ed, g_filename, argc > 2 ? argv[2] : NULL);

  box = gtcaca_vbox_new();
  gtcaca_box_set_margin(box, 0);
  gtcaca_box_add_expand(box, GTCACA_WIDGET(g_ed));
  gtcaca_box_apply_window(box, win);
  gtcaca_box_free(box);

  /* browser window, stacked on top of the editor (initially hidden) */
  g_browser_win = gtcaca_window_new(NULL, "Browser", 0, 0, cw, ch - 1);
  g_browser = gtcaca_textlist_new(GTCACA_WIDGET(g_browser_win), 1, 1);
  gtcaca_textlist_key_cb_register(g_browser, on_browser_select, NULL);
  gtcaca_textlist_widget_set_view_size(g_browser, (unsigned int)(ch - 3 > 0 ? ch - 3 : 1));

  g_modeline = gtcaca_statusbar_new("");

  if (open_dir) {
    if (!realpath(arg, g_curdir)) { strncpy(g_curdir, arg, sizeof g_curdir - 1); g_curdir[sizeof g_curdir - 1] = '\0'; }
    show_browser();
  } else {
    gtcaca_widget_hide(GTCACA_WIDGET(g_browser));
    gtcaca_widget_hide(GTCACA_WIDGET(g_browser_win));
    gtcaca_window_set_focus(win);
    gtcaca_window_set_focused_child(win, GTCACA_WIDGET(g_ed));
  }

  refresh_modeline(g_ed, NULL);
  gtcaca_main();

  gtcaca_editor_langcfg_free(g_langcfg);
  gtcaca_editor_grammar_free(g_grammar);
  free(g_killbuf);
  { int i; for (i = 0; i < g_browser_n; i++) free(g_browser_items[i]); free(g_browser_items); }
  return 0;
}
