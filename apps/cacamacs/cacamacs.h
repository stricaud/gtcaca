#ifndef CACAMACS_H
#define CACAMACS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <limits.h>
#include <caca.h>
#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/textlist.h>
#include <gtcaca/label.h>
#include <gtcaca/button.h>
#include <gtcaca/entry.h>
#include <gtcaca/editor.h>
#include <gtcaca/box.h>
#include <gtcaca/json.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#define KEY_CTRL_SPACE 0x00
#define KEY_CTRL_S     0x13
#define KEY_UNDO       0x1f
#define MAXBUF    32
#define MAXLEAVES 8
#define MAXNODES  (2 * MAXLEAVES)
#define MACRO_MAX 8192

typedef struct {
  gtcaca_editor_widget_t  *ed;
  char                     path[PATH_MAX];
  int                      has_file;
  gtcaca_editor_langcfg_t *langcfg;
  gtcaca_editor_grammar_t *grammar;
  char                     langname[64];
  const char             **keywords;
  int                      n_keywords;
  int                      folding;
  int                      pane;
} buffer_t;

typedef struct {
  int used, is_leaf, parent;
  int buf, win;
  int orient, child[2];
  int x, y, w, h;
} node_t;

typedef struct { char ext[16]; int tab_size, insert_spaces, indent_size; int has_ts, has_is, has_id; } lang_override_t;

typedef void (*mv_t)(gtcaca_editor_widget_t *);

static inline int is_word_char(int c) { return isalnum((unsigned char)c) || c == '_' || c == '$'; }

/* globals (defined in cacamacs.c, search.c) */
extern gtcaca_editor_widget_t    *g_ed;
extern gtcaca_statusbar_widget_t *g_modeline;
extern gtcaca_window_widget_t    *g_win;
extern const char                *g_filename;
extern char                       g_open_path[PATH_MAX];
extern buffer_t g_buffers[MAXBUF];
extern int      g_nbuf;
extern int      g_cur_buf;
extern node_t g_nodes[MAXNODES];
extern int    g_root, g_focus_leaf;
extern gtcaca_window_widget_t *g_winpool[MAXLEAVES];
extern int    g_win_used[MAXLEAVES];
extern gtcaca_window_widget_t    *g_browser_win;
extern gtcaca_editor_widget_t    *g_browser_ed;
extern char                       g_curdir[PATH_MAX];
extern gtcaca_window_widget_t    *g_help_win;
extern gtcaca_editor_widget_t    *g_help_ed;
extern int                        g_help_open;
extern gtcaca_window_widget_t    *g_save_win;
extern gtcaca_label_widget_t     *g_save_label;
extern gtcaca_entry_widget_t     *g_save_entry;
extern gtcaca_button_widget_t    *g_save_ok;
extern gtcaca_button_widget_t    *g_save_cancel;
extern int                        g_save_open;
extern gtcaca_editor_langcfg_t   *g_langcfg;
extern gtcaca_editor_grammar_t   *g_grammar;
extern char                       g_langname[64];
extern const char               **g_keywords;
extern int                        g_n_keywords;
extern int                        g_folding;
extern int             g_cfg_tab, g_cfg_spaces, g_cfg_indent;
extern int             g_cfg_edge;
extern lang_override_t g_overrides[32];
extern int             g_noverrides;
extern int             g_tab_size, g_insert_spaces, g_indent_size;
extern int   g_mark_active;
extern int   g_ctrl_x;
extern int   g_rect_prefix;
extern int   g_meta;
extern char *g_killbuf;
extern int   g_macro[MACRO_MAX];
extern int   g_macro_len;
extern int   g_macro_recording;
extern int   g_macro_executing;
extern int   g_prefix_reading;
extern int   g_prefix_pending;
extern int   g_prefix_have;
extern long  g_prefix_arg;
extern char g_message[128];
extern int  g_bottom_reserve;   /* rows kept clear above the status bar */
extern int g_isearch;
extern int g_qr_active;          /* query-replace stepping is in progress */
extern int g_spell_active;       /* spell-check suggestion picker is up */
extern int g_mb_active;

/* games (games.c) */
void run_snake(void);
void run_sokoban(void);

/* functions */
/* edit.c */
void case_word(gtcaca_editor_widget_t *ed, int upper);
void comment_dwim(gtcaca_editor_widget_t *ed);
void copy_region(gtcaca_editor_widget_t *ed);
const char *current_line_comment(void);
void exchange_point_and_mark(gtcaca_editor_widget_t *ed);
void keyboard_quit(gtcaca_editor_widget_t *ed);
void kill_line(gtcaca_editor_widget_t *ed);
void kill_region(gtcaca_editor_widget_t *ed);
void kill_set(const char *text, int len);
void kill_word_left(gtcaca_editor_widget_t *ed);
void kill_word_right(gtcaca_editor_widget_t *ed);
const char *line_comment_for_ext(const char *ext);
int line_first_nonws(gtcaca_editor_widget_t *ed, int line);
int line_is_blank(gtcaca_editor_widget_t *ed, int line);
int line_is_commented(gtcaca_editor_widget_t *ed, int line, const char *lc, int lclen);
void move(gtcaca_editor_widget_t *ed, mv_t plain, mv_t extend);
void pretty_print_json(gtcaca_editor_widget_t *ed);
void recenter(gtcaca_editor_widget_t *ed);
void save_file(gtcaca_editor_widget_t *ed);
void set_mark(gtcaca_editor_widget_t *ed);
void set_rectangle_mark(gtcaca_editor_widget_t *ed);
void show_paren(gtcaca_editor_widget_t *ed);
void toggle_annotation_here(gtcaca_editor_widget_t *ed);
void toggle_fold_here(gtcaca_editor_widget_t *ed);
void toggle_folding(gtcaca_editor_widget_t *ed);
void toggle_line_numbers(gtcaca_editor_widget_t *ed);
void toggle_wrap(gtcaca_editor_widget_t *ed);
void transpose_chars(gtcaca_editor_widget_t *ed);
void yank(gtcaca_editor_widget_t *ed);
/* search.c */
void expand_tilde(const char *in, char *out, size_t outsz);
void find_file_done(const char *input);
int isearch_key(gtcaca_editor_widget_t *ed, int key);
void isearch_redo(gtcaca_editor_widget_t *ed);
void isearch_status(void);
void mb_status(void);
void minibuffer_complete_command(void);
void minibuffer_complete_path(void);
int minibuffer_key(int key);
void mx_done(const char *cmd);
void replace_phase1(const char *search);
void replace_phase2(const char *with);
void start_find_file(void);
void start_isearch(gtcaca_editor_widget_t *ed, int dir);
void start_mx(void);
void start_replace(void);
int query_replace_key(gtcaca_editor_widget_t *ed, int key);
void start_query_replace(void);
void start_query_replace_regexp(void);
void spell_word(gtcaca_editor_widget_t *ed);
int  spell_key(gtcaca_editor_widget_t *ed, int key);
void start_string_rectangle(void);
void string_rect_done(const char *s);
void start_minibuffer(const char *prompt, void (*cb)(const char *));
void start_minibuffer_init(const char *prompt, void (*cb)(const char *), int complete, const char *initial);
/* keymap.c */
void editor_default_key(gtcaca_editor_widget_t *ed, int key);
void macro_execute(gtcaca_editor_widget_t *ed);
void macro_feed(gtcaca_editor_widget_t *ed, int key);
int on_key(gtcaca_editor_widget_t *ed, int key, void *ud);
int take_prefix(void);
/* lang.c */
void _cand_add(char ***arr, int *n, int *cap, const char *s, int slen);
int _cmp_str(const void *a, const void *b);
int _json_bool(gtcaca_json_value *o, const char *key, int dflt);
int _json_int(gtcaca_json_value *o, const char *key, int dflt);
void apply_indent_settings(const char *ext);
void apply_keywords(gtcaca_editor_langcfg_t *cfg, const char *langid);
void complete_at_point(gtcaca_editor_widget_t *ed);
int discover_grammar(const char *filename, char *out_path, int psz, char *out_id, int idsz);
gtcaca_editor_langcfg_t *discover_langcfg(const char *filename, char *out_id, int idsz);
const char *langid_from_ext(const char *ext);
void load_config(void);
char *read_file(const char *path);
void refresh_modeline(gtcaca_editor_widget_t *ed, void *ud);
int scan_root_grammar(const char *root, const char *ext, const char *base, char *out_path, int psz, char *out_id, int idsz);
gtcaca_editor_langcfg_t *scan_root_langcfg(const char *root, const char *ext, const char *base, char *out_id, int idsz);
void setup_language(gtcaca_editor_widget_t *ed, const char *filename, const char *explicit_cfg);
gtcaca_editor_langcfg_t *try_extension(const char *dir, const char *ext, const char *base, char *out_id, int idsz);
int try_grammar(const char *dir, const char *ext, const char *base, char *out_path, int psz, char *out_id, int idsz);
/* ui.c */
int _cmp_bent(const void *a, const void *b);
void browser_modeline(gtcaca_editor_widget_t *ed, void *ud);
void browser_open_current(void);
int buf_index_of(gtcaca_editor_widget_t *ed);
int buffer_create(const char *path);
void buffer_load_globals(int bi);
void buffer_store_globals(int bi);
void collect_leaves(int n, int *out, int *cnt);
int dlg_cancel(gtcaca_button_widget_t *btn, int key, void *ud);
int dlg_entry_key(gtcaca_entry_widget_t *e, int key, void *ud);
int dlg_ok(gtcaca_button_widget_t *btn, int key, void *ud);
void focus_pane(int n);
const char *help_text(void);
void hide_browser(void);
void hide_help(void);
void hide_save_dialog(void);
void layout_node(int n, int x, int y, int w, int h);
int leftmost_leaf(int n);
int node_alloc(void);
void open_path_in_editor(const char *path);
void pane_delete_current(void);
void pane_hide_all(void);
void pane_other_window(void);
void pane_show_all(void);
void pane_show_buffer(int n, int bi);
void pane_split(int rows);
void pane_switch_buffer(void);
void pane_unsplit_one(void);
void populate_browser(const char *dir);
void position_leaf(int n);
void relayout(void);
void save_as_done(const char *input);
void show_browser(void);
void show_help(void);
void start_save_as(void);
void widget_to_front(gtcaca_widget_t *w);
int win_alloc(void);

#endif /* CACAMACS_H */
