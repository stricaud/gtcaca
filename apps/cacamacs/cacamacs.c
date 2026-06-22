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

#include "cacamacs.h"

/* global definitions (declared extern in cacamacs.h) */
char g_message[128] = "";   /* status-line message; shared with games.c */
gtcaca_editor_widget_t    *g_ed       = NULL;   /* focused editor       */
gtcaca_statusbar_widget_t *g_modeline = NULL;
gtcaca_window_widget_t    *g_win      = NULL;   /* focused pane window  */
const char                *g_filename = NULL;
char                       g_open_path[PATH_MAX] = "";
buffer_t g_buffers[MAXBUF];
int      g_nbuf = 0;
int      g_cur_buf = -1;           /* focused buffer index */
node_t g_nodes[MAXNODES];
int    g_root = -1, g_focus_leaf = -1;
gtcaca_window_widget_t *g_winpool[MAXLEAVES];
int    g_win_used[MAXLEAVES];
gtcaca_window_widget_t    *g_browser_win = NULL;
gtcaca_editor_widget_t    *g_browser_ed  = NULL;
char                       g_curdir[PATH_MAX] = "";
gtcaca_window_widget_t    *g_help_win = NULL;
gtcaca_editor_widget_t    *g_help_ed  = NULL;
int                        g_help_open = 0;
gtcaca_window_widget_t    *g_save_win    = NULL;
gtcaca_label_widget_t     *g_save_label  = NULL;
gtcaca_entry_widget_t     *g_save_entry  = NULL;
gtcaca_button_widget_t    *g_save_ok     = NULL;
gtcaca_button_widget_t    *g_save_cancel = NULL;
int                        g_save_open   = 0;
gtcaca_editor_langcfg_t   *g_langcfg  = NULL;
gtcaca_editor_grammar_t   *g_grammar  = NULL;
char                       g_langname[64] = "fundamental";
const char               **g_keywords = NULL;   /* language keywords, for completion */
int                        g_n_keywords = 0;
int                        g_folding = 0;         /* folding mode on? */
int             g_cfg_tab = 8, g_cfg_spaces = 1, g_cfg_indent = 2;   /* global defaults */
int             g_cfg_edge = 0;                                       /* edge/ruler column */
lang_override_t g_overrides[32];
int             g_noverrides = 0;
int             g_tab_size = 8, g_insert_spaces = 1, g_indent_size = 2; /* effective */
int   g_mark_active   = 0;
int   g_ctrl_x        = 0;   /* C-x prefix pending */
int   g_rect_prefix   = 0;   /* C-x r rectangle sub-prefix pending */
int   g_meta          = 0;   /* Meta (Esc) prefix pending */
char *g_killbuf       = NULL;
int   g_macro[MACRO_MAX];
int   g_macro_len       = 0;
int   g_macro_recording = 0;
int   g_macro_executing = 0;
int   g_prefix_reading  = 0; /* C-u seen, accumulating an optional count */
int   g_prefix_pending  = 0; /* a count is armed for the next command    */
int   g_prefix_have     = 0; /* digits were actually typed (else default) */
long  g_prefix_arg      = 0;

int main(int argc, char **argv)
{
  int cw, ch, i, b0;
  const char *arg = argc > 1 ? argv[1] : NULL;
  const char *open_file_path = NULL;
  int open_dir = 0;

  gtcaca_init(&argc, &argv);
  /* Emacs-like editor colours: a near-black background with light-grey text,
     the same whether or not the pane is focused (focus is shown by the cursor,
     not a background tint). The cyan statusbar is left as-is. */
  gmo.theme.textviewfocus.bg = CACA_BLACK;
  gmo.theme.textviewfocus.fg = CACA_LIGHTGRAY;
  gmo.theme.textview.bg      = CACA_BLACK;
  gmo.theme.textview.fg      = CACA_LIGHTGRAY;
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

  /* help viewer pop-up (M-x help) — a modal, scrollable, read-only editor */
  {
    int hw = cw * 4 / 5, hh = (ch - 1) * 4 / 5;
    if (hw < 24) hw = cw > 24 ? cw - 2 : cw;
    if (hh < 6)  hh = ch - 2;
    g_help_win = gtcaca_window_new_centered(NULL, "Help — key bindings", hw, hh);
    g_help_ed  = gtcaca_editor_new(GTCACA_WIDGET(g_help_win), 0, 0, 1, 1);
    gtcaca_editor_set_line_numbers(g_help_ed, 0);
    gtcaca_editor_set_read_only(g_help_ed, 1);
    gtcaca_editor_key_cb_register(g_help_ed, on_key, NULL);
    { gtcaca_box_t *hb = gtcaca_vbox_new(); gtcaca_box_set_margin(hb, 0);
      gtcaca_box_add_expand(hb, GTCACA_WIDGET(g_help_ed));
      gtcaca_box_apply(hb, g_help_win->x, g_help_win->y,
                       g_help_win->width, g_help_win->height);
      gtcaca_box_free(hb); }
    gtcaca_widget_hide(GTCACA_WIDGET(g_help_ed));
    gtcaca_widget_hide(GTCACA_WIDGET(g_help_win));
  }

  /* save-as dialog (created hidden; brought to front when shown) */
  {
    g_save_win   = gtcaca_window_new_centered(NULL, "Write file", 56, 10);
    g_save_label = gtcaca_label_new(GTCACA_WIDGET(g_save_win), "File name:", 2, 1);
    g_save_entry = gtcaca_entry_new(GTCACA_WIDGET(g_save_win), 2, 3, 52);
    g_save_ok    = gtcaca_button_new(GTCACA_WIDGET(g_save_win), "[ OK ]", 4, 5);
    g_save_cancel= gtcaca_button_new(GTCACA_WIDGET(g_save_win), "[ Cancel ]", 16, 5);
    gtcaca_button_key_cb_register(g_save_ok, dlg_ok);
    gtcaca_button_key_cb_register(g_save_cancel, dlg_cancel);
    gtcaca_entry_key_cb_register(g_save_entry, dlg_entry_key, NULL);
    gtcaca_window_set_default(g_save_win, GTCACA_WIDGET(g_save_ok));  /* Enter = OK */
    gtcaca_widget_hide(GTCACA_WIDGET(g_save_win));
    gtcaca_widget_hide(GTCACA_WIDGET(g_save_label));
    gtcaca_widget_hide(GTCACA_WIDGET(g_save_entry));
    gtcaca_widget_hide(GTCACA_WIDGET(g_save_ok));
    gtcaca_widget_hide(GTCACA_WIDGET(g_save_cancel));
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

