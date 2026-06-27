#include "cacamacs.h"
/* ── file browser (open a directory to navigate it) ────────────────────────── */

typedef struct { char name[256]; int isdir; } bent_t;

int _cmp_bent(const void *a, const void *b)
{
  const bent_t *x = a, *y = b;
  if (x->isdir != y->isdir) return y->isdir - x->isdir;   /* directories first */
  return strcmp(x->name, y->name);
}

/* Render the directory listing into the read-only browser editor. */
void populate_browser(const char *dir)
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

void show_browser(void)
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

void hide_browser(void)
{
  gtcaca_widget_hide(GTCACA_WIDGET(g_browser_ed));
  gtcaca_widget_hide(GTCACA_WIDGET(g_browser_win));
  g_browser_open = 0;
  pane_show_all();                 /* restore the editor panes and focus */
}

/* ── help viewer (M-x help) ─────────────────────────────────────────────────── */

const char *help_text(void)
{
  return
  "cacamacs key bindings        (Up/Down/PageUp/PageDown or C-v scroll, C-s search,\n"
  "                              q or Esc closes this window)\n"
  "\n"
  "Motion   C-f C-b C-n C-p   C-a C-e   C-v   arrows / Home / End / PageUp / PageDown\n"
  "         M-< / M->   beginning / end of buffer   M-g M-g (or M-x goto-line) jump to line\n"
  "         C-n / C-p step through wrapped rows when line wrap is on\n"
  "Edit     C-d delete-fwd   C-k kill-line   Backspace   Tab indent / complete\n"
  "Words    M-f / M-b move    M-d / M-DEL kill    M-u / M-l upcase / downcase\n"
  "Region   C-Space mark   C-w kill   M-w (Esc w) copy   C-y yank   C-g cancel\n"
  "Rect     C-x SPC rectangle mark, move to opposite corner, then:\n"
  "         C-x r t insert string per line   C-x r k kill   C-x r y yank\n"
  "         C-x r d delete   C-x r c blank   (C-w / M-w act on the box too)\n"
  "Comment  M-;  comment / uncomment the line or region\n"
  "Lines    C-x C-t transpose lines   C-t transpose chars\n"
  "Search   C-s isearch fwd   C-r isearch back\n"
  "Replace  M-% query-replace: y replace, n skip, ! all, . last, q quit\n"
  "         M-x query-replace-regexp (regex)   M-x replace-string (all)\n"
  "Spell    M-$ check word at point   (0-9 pick a suggestion, SPC skip)\n"
  "Mouse    click to move point   drag to select   wheel to scroll\n"
  "Macros   C-x ( record   C-x ) finish   C-x e replay   (C-u N C-x e = N times)\n"
  "Repeat   C-u N <command>   runs the next command N times (bare C-u = 4)\n"
  "Windows  C-x 2 split below   C-x 3 split right   C-x 1 one   C-x 0 close\n"
  "         C-x o other window   C-x b switch buffer\n"
  "Files    C-x C-f find file   C-x C-s save   C-x C-w write-as   C-x C-c quit\n"
  "         C-x d directory browser\n"
  "View     C-x l line numbers   C-x f folding   C-x t toggle fold   C-x a annotate\n"
  "         C-x C-l line wrap (on by default)   C-x w whitespace   C-l recenter\n"
  "JSON     C-x p pretty-print (.json/.jsonl open in JSON mode)\n"
  "Undo     C-/   (also C-x u)        Help   M-x help\n";
}

void show_help(void)
{
  pane_hide_all();                 /* modal: only the help window is visible */
  gtcaca_widget_show(GTCACA_WIDGET(g_help_win));
  gtcaca_widget_show(GTCACA_WIDGET(g_help_ed));
  gtcaca_editor_set_read_only(g_help_ed, 0);
  gtcaca_editor_set_text(g_help_ed, help_text());
  gtcaca_editor_set_read_only(g_help_ed, 1);
  gtcaca_editor_goto_pos(g_help_ed, 0);
  gtcaca_window_set_focus(g_help_win);
  gtcaca_window_set_focused_child(g_help_win, GTCACA_WIDGET(g_help_ed));
  g_help_open = 1;
}

void hide_help(void)
{
  gtcaca_widget_hide(GTCACA_WIDGET(g_help_ed));
  gtcaca_widget_hide(GTCACA_WIDGET(g_help_win));
  g_help_open = 0;
  pane_show_all();
}

/* Open whatever entry the caret is on: descend into a directory, or open a file. */
void browser_open_current(void)
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
void browser_modeline(gtcaca_editor_widget_t *ed, void *ud)
{
  char t[256];
  (void)ed; (void)ud;
  if (g_message[0]) snprintf(t, sizeof t, "  Browser  %s   %s", g_curdir, g_message);
  else snprintf(t, sizeof t, "  Browser  %s   arrows move  C-s search  Enter open  q quit", g_curdir);
  gtcaca_statusbar_set_text(g_modeline, t);
}

/* ── buffers & split panes ─────────────────────────────────────────────────── */

int buf_index_of(gtcaca_editor_widget_t *ed)
{
  int i; for (i = 0; i < g_nbuf; i++) if (g_buffers[i].ed == ed) return i; return -1;
}

/* The focused buffer's per-file state lives in the globals while focused; these
   move it to/from the buffer record when focus changes. */
void buffer_store_globals(int bi)
{
  buffer_t *b;
  if (bi < 0 || bi >= g_nbuf) return;
  b = &g_buffers[bi];
  b->langcfg = g_langcfg; b->grammar = g_grammar;
  b->keywords = g_keywords; b->n_keywords = g_n_keywords; b->folding = g_folding;
  strncpy(b->langname, g_langname, sizeof b->langname - 1); b->langname[sizeof b->langname - 1] = '\0';
}
void buffer_load_globals(int bi)
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
int buffer_create(const char *path)
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
  gtcaca_editor_set_wrap(b->ed, 1);              /* wrap long lines by default */
  gtcaca_editor_key_cb_register(b->ed, on_key, NULL);
  gtcaca_editor_set_update_cb(b->ed, refresh_modeline, NULL);
  gtcaca_editor_set_caret_line_visible(b->ed, 1);
  gtcaca_editor_set_caret_line_back(b->ed, CACA_BLACK);
  gtcaca_editor_set_edge_column(b->ed, g_cfg_edge);
  gtcaca_editor_set_tab_width(b->ed, g_cfg_tab > 0 ? g_cfg_tab : 8);
  ccm_theme_apply_editor(b->ed);                 /* ~/.ccm/theme colours, if any */
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

int node_alloc(void)
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
int win_alloc(void) { int i; for (i = 0; i < MAXLEAVES; i++) if (!g_win_used[i]) { g_win_used[i] = 1; return i; } return -1; }

/* Collect leaf node indices, left-to-right (in the order they tile). */
void collect_leaves(int n, int *out, int *cnt)
{
  if (n < 0 || !g_nodes[n].used) return;
  if (g_nodes[n].is_leaf) { if (*cnt < MAXLEAVES) out[(*cnt)++] = n; return; }
  collect_leaves(g_nodes[n].child[0], out, cnt);
  collect_leaves(g_nodes[n].child[1], out, cnt);
}
int leftmost_leaf(int n) { while (n >= 0 && !g_nodes[n].is_leaf) n = g_nodes[n].child[0]; return n; }

void position_leaf(int n)
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

void layout_node(int n, int x, int y, int w, int h)
{
  node_t *nd = &g_nodes[n];
  nd->x = x; nd->y = y; nd->w = w; nd->h = h;
  if (nd->is_leaf) { position_leaf(n); return; }
  if (nd->orient) { int h0 = h / 2; layout_node(nd->child[0], x, y, w, h0); layout_node(nd->child[1], x, y + h0, w, h - h0); }
  else            { int w0 = w / 2; layout_node(nd->child[0], x, y, w0, h); layout_node(nd->child[1], x + w0, y, w - w0, h); }
}

void relayout(void)
{
  int cw = caca_get_canvas_width(gmo.cv), ch = caca_get_canvas_height(gmo.cv), i;
  if (g_root >= 0) layout_node(g_root, 0, 0, cw, ch - 1 - g_bottom_reserve);
  for (i = 0; i < MAXLEAVES; i++) if (!g_win_used[i] && g_winpool[i]) gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[i]));
}

/* Show buffer bi in leaf node n. */
void pane_show_buffer(int n, int bi)
{
  if (g_buffers[bi].pane >= 0 && g_buffers[bi].pane != n) g_nodes[g_buffers[bi].pane].buf = -1;
  if (g_nodes[n].buf >= 0 && g_nodes[n].buf != bi) {
    gtcaca_widget_hide(GTCACA_WIDGET(g_buffers[g_nodes[n].buf].ed));
    g_buffers[g_nodes[n].buf].pane = -1;
  }
  g_nodes[n].buf = bi; g_buffers[bi].pane = n;
  position_leaf(n);
}

void focus_pane(int n)   /* focus a leaf node */
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

void pane_hide_all(void)
{
  int leaves[MAXLEAVES], cnt = 0, i;
  collect_leaves(g_root, leaves, &cnt);
  for (i = 0; i < cnt; i++) {
    node_t *nd = &g_nodes[leaves[i]];
    gtcaca_widget_hide(GTCACA_WIDGET(g_winpool[nd->win]));
    if (nd->buf >= 0) gtcaca_widget_hide(GTCACA_WIDGET(g_buffers[nd->buf].ed));
  }
}
void pane_show_all(void) { relayout(); focus_pane(g_focus_leaf); }

/* Open `path` into the focused window (creating/reusing a buffer). */
void open_path_in_editor(const char *path)
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
void pane_split(int rows)
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

void pane_unsplit_one(void)
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

void pane_delete_current(void)
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

void pane_other_window(void)
{
  int leaves[MAXLEAVES], cnt = 0, i, idx = 0;
  collect_leaves(g_root, leaves, &cnt);
  if (cnt <= 1) { snprintf(g_message, sizeof g_message, "Only one window"); return; }
  for (i = 0; i < cnt; i++) if (leaves[i] == g_focus_leaf) { idx = i; break; }
  focus_pane(leaves[(idx + 1) % cnt]);
}

void pane_switch_buffer(void)
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

/* ── save-as (C-x C-w, and C-x C-s on a scratch buffer) ────────────────────── */

void save_as_done(const char *input)
{
  char path[PATH_MAX];
  FILE *f;
  int len;
  char *buf;
  buffer_t *b;

  if (!input[0]) return;
  if (input[0] == '~') expand_tilde(input, path, sizeof path);
  else if (input[0] == '/') snprintf(path, sizeof path, "%s", input);
  else if (g_curdir[0]) snprintf(path, sizeof path, "%s/%s", g_curdir, input);
  else snprintf(path, sizeof path, "%s", input);

  len = gtcaca_editor_get_length(g_ed);
  buf = malloc((size_t)len + 1);
  if (!buf) return;
  gtcaca_editor_get_text(g_ed, buf, len + 1);
  f = fopen(path, "w");
  if (!f) { snprintf(g_message, sizeof g_message, "Cannot write %s", path); free(buf); return; }
  fwrite(buf, 1, (size_t)len, f);
  fclose(f);
  free(buf);

  /* attach the file to the focused buffer and re-detect its language */
  b = &g_buffers[g_cur_buf];
  strncpy(b->path, path, sizeof b->path - 1); b->path[sizeof b->path - 1] = '\0';
  b->has_file = 1;
  g_filename = b->path;
  if (g_langcfg) { gtcaca_editor_set_langcfg(g_ed, NULL); gtcaca_editor_langcfg_free(g_langcfg); g_langcfg = NULL; }
  if (g_grammar) { gtcaca_editor_set_grammar(g_ed, NULL); gtcaca_editor_grammar_free(g_grammar); g_grammar = NULL; }
  g_keywords = NULL; g_n_keywords = 0; strcpy(g_langname, "fundamental");
  gtcaca_editor_set_json_mode(g_ed, 0);
  setup_language(g_ed, g_filename, NULL);
  buffer_store_globals(g_cur_buf);

  gtcaca_editor_set_save_point(g_ed);
  snprintf(g_message, sizeof g_message, "Wrote %s", path);
}

/* Bring a widget (and so its window) to the front of the draw order. */
void widget_to_front(gtcaca_widget_t *w)
{
  CDL_DELETE(gmo.widgets_list, w);
  CDL_APPEND(gmo.widgets_list, w);
}

void hide_save_dialog(void)
{
  gtcaca_widget_hide(GTCACA_WIDGET(g_save_win));
  gtcaca_widget_hide(GTCACA_WIDGET(g_save_label));
  gtcaca_widget_hide(GTCACA_WIDGET(g_save_entry));
  gtcaca_widget_hide(GTCACA_WIDGET(g_save_ok));
  gtcaca_widget_hide(GTCACA_WIDGET(g_save_cancel));
  g_save_open = 0;
  focus_pane(g_focus_leaf);           /* return focus to the editor */
}

int dlg_ok(gtcaca_button_widget_t *btn, int key, void *ud)
{
  char tmp[PATH_MAX];
  const char *t;
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  t = gtcaca_entry_get_text(g_save_entry);
  strncpy(tmp, t ? t : "", sizeof tmp - 1); tmp[sizeof tmp - 1] = '\0';
  save_as_done(tmp);     /* sets g_message = "Wrote <path>" before we redraw */
  hide_save_dialog();    /* focus_pane → refresh_modeline shows that message */
  return 0;
}
int dlg_cancel(gtcaca_button_widget_t *btn, int key, void *ud)
{
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  hide_save_dialog();
  snprintf(g_message, sizeof g_message, "Cancelled");
  return 0;
}
/* C-g inside the entry cancels the dialog too. */
int dlg_entry_key(gtcaca_entry_widget_t *e, int key, void *ud)
{
  (void)e; (void)ud;
  if (key == CACA_KEY_CTRL_G) { hide_save_dialog(); snprintf(g_message, sizeof g_message, "Cancelled"); }
  return 0;
}

void start_save_as(void)
{
  char defdir[PATH_MAX];
  if (g_cur_buf >= 0 && g_buffers[g_cur_buf].has_file) {
    strncpy(defdir, g_buffers[g_cur_buf].path, sizeof defdir - 1); defdir[sizeof defdir - 1] = '\0';
  } else if (g_curdir[0]) {
    snprintf(defdir, sizeof defdir, "%s/", g_curdir);
  } else {
    if (getcwd(defdir, sizeof defdir - 1)) strncat(defdir, "/", sizeof defdir - strlen(defdir) - 1);
    else defdir[0] = '\0';
  }
  gtcaca_entry_set_text(g_save_entry, defdir);
  widget_to_front(GTCACA_WIDGET(g_save_win));
  widget_to_front(GTCACA_WIDGET(g_save_label));
  widget_to_front(GTCACA_WIDGET(g_save_entry));
  widget_to_front(GTCACA_WIDGET(g_save_ok));
  widget_to_front(GTCACA_WIDGET(g_save_cancel));
  gtcaca_widget_show(GTCACA_WIDGET(g_save_win));
  gtcaca_widget_show(GTCACA_WIDGET(g_save_label));
  gtcaca_widget_show(GTCACA_WIDGET(g_save_entry));
  gtcaca_widget_show(GTCACA_WIDGET(g_save_ok));
  gtcaca_widget_show(GTCACA_WIDGET(g_save_cancel));
  gtcaca_window_set_focus(g_save_win);
  gtcaca_window_set_focused_child(g_save_win, GTCACA_WIDGET(g_save_entry));
  g_save_open = 1;
  snprintf(g_message, sizeof g_message, "Write file — Enter/OK to save, Cancel or C-g to abort");
}

