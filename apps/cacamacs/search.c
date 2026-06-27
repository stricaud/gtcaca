#include "cacamacs.h"
/* ── incremental search (C-s / C-r), à la Emacs isearch ────────────────────── */

int g_isearch = 0;
static int   g_isearch_dir = 1, g_isearch_fail = 0;
static int   g_isearch_origin = 0, g_isearch_from = 0, g_isearch_last = 0;
static char  g_isearch_query[128];
static int   g_isearch_qlen = 0;

void isearch_status(void)
{
  snprintf(g_message, sizeof g_message, "%sI-search%s: %s",
           g_isearch_fail ? "Failing " : "", g_isearch_dir < 0 ? " backward" : "", g_isearch_query);
}

void isearch_redo(gtcaca_editor_widget_t *ed)
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

void start_isearch(gtcaca_editor_widget_t *ed, int dir)
{
  g_isearch = 1; g_isearch_dir = dir;
  g_isearch_origin = g_isearch_from = g_isearch_last = gtcaca_editor_get_current_pos(ed);
  g_isearch_qlen = 0; g_isearch_query[0] = '\0';
  isearch_status();
}

int isearch_key(gtcaca_editor_widget_t *ed, int key)
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

int g_mb_active = 0;
static char  g_mb_prompt[64], g_mb_buf[PATH_MAX];
static int   g_mb_len = 0;
static int   g_mb_point = 0;           /* caret index within g_mb_buf (0..len) */
static int   g_mb_complete = 0;        /* Tab does filename completion */
static int   g_mb_meta = 0;            /* an Esc (Meta) prefix is pending */
static void (*g_mb_cb)(const char *) = NULL;

/* Render the prompt + text with a caret bar (▏) drawn at point. */
void mb_status(void)
{
  char shown[200];
  int p = g_mb_point; if (p < 0) p = 0; if (p > g_mb_len) p = g_mb_len;
  snprintf(shown, sizeof shown, "%.*s\xe2\x96\x8f%s", p, g_mb_buf, g_mb_buf + p);
  snprintf(g_message, sizeof g_message, "%s%s", g_mb_prompt, shown);
}

/* A word is a run of alphanumerics; `_`, `/`, `.`, … are boundaries, so on
   "foo_bar" backward-kill leaves "foo_" and a second one removes the "_". */
static void mb_kill_word_left(void)
{
  int i = g_mb_point, start = g_mb_point;
  if (i <= 0) return;
  if (isalnum((unsigned char)g_mb_buf[i - 1]))
    while (i > 0 &&  isalnum((unsigned char)g_mb_buf[i - 1])) i--;   /* the word; stop at _ / . */
  else
    while (i > 0 && !isalnum((unsigned char)g_mb_buf[i - 1])) i--;   /* a run of separators */
  memmove(g_mb_buf + i, g_mb_buf + start, (size_t)(g_mb_len - start) + 1);
  g_mb_len -= (start - i); g_mb_point = i;
}

/* Forward-kill the word right of point, with the same `_`/`/` boundaries. */
static void mb_kill_word_right(void)
{
  int e = g_mb_point;
  if (e >= g_mb_len) return;
  if (isalnum((unsigned char)g_mb_buf[e]))
    while (e < g_mb_len &&  isalnum((unsigned char)g_mb_buf[e])) e++;
  else
    while (e < g_mb_len && !isalnum((unsigned char)g_mb_buf[e])) e++;
  memmove(g_mb_buf + g_mb_point, g_mb_buf + e, (size_t)(g_mb_len - e) + 1);
  g_mb_len -= (e - g_mb_point);
}

/* Optional initial content for the prompt. */
void start_minibuffer_init(const char *prompt, void (*cb)(const char *), int complete, const char *initial)
{
  g_mb_active = 1; g_mb_cb = cb; g_mb_complete = complete; g_mb_meta = 0;
  strncpy(g_mb_prompt, prompt, sizeof g_mb_prompt - 1); g_mb_prompt[sizeof g_mb_prompt - 1] = '\0';
  if (initial) { strncpy(g_mb_buf, initial, sizeof g_mb_buf - 1); g_mb_buf[sizeof g_mb_buf - 1] = '\0'; }
  else g_mb_buf[0] = '\0';
  g_mb_len = (int)strlen(g_mb_buf);
  g_mb_point = g_mb_len;
  mb_status();
}
void start_minibuffer(const char *prompt, void (*cb)(const char *)) { start_minibuffer_init(prompt, cb, 0, NULL); }

/* Expand a leading "~/" to $HOME. */
void expand_tilde(const char *in, char *out, size_t outsz)
{
  const char *home = getenv("HOME");
  if (in[0] == '~' && in[1] == '/' && home) snprintf(out, outsz, "%s%s", home, in + 1);
  else snprintf(out, outsz, "%s", in);
}

/* Tab completion for the path in the minibuffer (longest common prefix). */
void minibuffer_complete_path(void)
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

/* M-x command names offered by completion (aliases still run, see mx_done) */
static const char *g_mx_commands[] = { "help", "snake", "sokoban", "describe-bindings",
                                       "query-replace", "query-replace-regexp",
                                       "replace-string", "goto-line", NULL };

void minibuffer_complete_command(void)
{
  const char *matches[32];
  char common[64];
  int i, n = 0;
  size_t plen = strlen(g_mb_buf);
  for (i = 0; g_mx_commands[i]; i++)
    if (!strncmp(g_mx_commands[i], g_mb_buf, plen) && n < 32) matches[n++] = g_mx_commands[i];
  if (n == 0) { snprintf(g_message, sizeof g_message, "%s%s   [no match]", g_mb_prompt, g_mb_buf); return; }
  /* extend to the longest common prefix of the matches */
  strncpy(common, matches[0], sizeof common - 1); common[sizeof common - 1] = '\0';
  for (i = 1; i < n; i++) { int j = 0; while (common[j] && matches[i][j] == common[j]) j++; common[j] = '\0'; }
  strncpy(g_mb_buf, common, sizeof g_mb_buf - 1); g_mb_buf[sizeof g_mb_buf - 1] = '\0';
  g_mb_len = (int)strlen(g_mb_buf);
  if (n == 1) { mb_status(); return; }
  {                                              /* several: show the candidates */
    char list[160] = ""; size_t off = 0; int k;
    for (k = 0; k < n && off < sizeof list - 1; k++)
      off += (size_t)snprintf(list + off, sizeof list - off, "%s%s", k ? "  " : "", matches[k]);
    snprintf(g_message, sizeof g_message, "%s%s   {%s}", g_mb_prompt, g_mb_buf, list);
  }
}

int minibuffer_key(int key)
{
  if (g_mb_meta) {                 /* Esc (Meta) prefix is pending */
    g_mb_meta = 0;
    switch (key) {
    case CACA_KEY_BACKSPACE: case CACA_KEY_DELETE:               /* M-DEL backward-kill-word */
      mb_kill_word_left();  mb_status(); return 1;
    case 'd': case 'D':                                          /* M-d forward-kill-word */
      mb_kill_word_right(); mb_status(); return 1;
    case 'b': case 'B':                                          /* M-b / M-f word motion */
      while (g_mb_point > 0 && !isalnum((unsigned char)g_mb_buf[g_mb_point - 1])) g_mb_point--;
      while (g_mb_point > 0 &&  isalnum((unsigned char)g_mb_buf[g_mb_point - 1])) g_mb_point--;
      mb_status(); return 1;
    case 'f': case 'F':
      while (g_mb_point < g_mb_len && !isalnum((unsigned char)g_mb_buf[g_mb_point])) g_mb_point++;
      while (g_mb_point < g_mb_len &&  isalnum((unsigned char)g_mb_buf[g_mb_point])) g_mb_point++;
      mb_status(); return 1;
    default:                                                     /* Esc-anything else cancels */
      g_mb_active = 0; snprintf(g_message, sizeof g_message, "Quit"); return 1;
    }
  }
  switch (key) {
  case CACA_KEY_ESCAPE: g_mb_meta = 1; mb_status(); return 1;    /* Meta prefix (Esc Esc cancels) */
  case CACA_KEY_RETURN:
  case 10: {
    void (*cb)(const char *) = g_mb_cb;
    char tmp[PATH_MAX];
    g_mb_active = 0; g_message[0] = '\0';
    strncpy(tmp, g_mb_buf, sizeof tmp - 1); tmp[sizeof tmp - 1] = '\0';
    if (cb) cb(tmp);
    return 1;
  }
  case CACA_KEY_TAB:    if (g_mb_complete == 2) minibuffer_complete_command();
                       else if (g_mb_complete) minibuffer_complete_path();
                       g_mb_point = g_mb_len; return 1;
  case CACA_KEY_CTRL_G: g_mb_active = 0; snprintf(g_message, sizeof g_message, "Quit"); return 1;
  case CACA_KEY_LEFT:  case 2 /* C-b */: if (g_mb_point > 0)        g_mb_point--; mb_status(); return 1;
  case CACA_KEY_RIGHT: case 6 /* C-f */: if (g_mb_point < g_mb_len) g_mb_point++; mb_status(); return 1;
  case CACA_KEY_HOME:  case 1 /* C-a */: g_mb_point = 0;       mb_status(); return 1;
  case CACA_KEY_END:   case 5 /* C-e */: g_mb_point = g_mb_len; mb_status(); return 1;
  case CACA_KEY_BACKSPACE:                                          /* delete char before point */
    if (g_mb_point > 0) {
      memmove(g_mb_buf + g_mb_point - 1, g_mb_buf + g_mb_point, (size_t)(g_mb_len - g_mb_point) + 1);
      g_mb_len--; g_mb_point--;
    }
    mb_status(); return 1;
  case CACA_KEY_DELETE: case 4 /* C-d */:                           /* delete char at point */
    if (g_mb_point < g_mb_len) {
      memmove(g_mb_buf + g_mb_point, g_mb_buf + g_mb_point + 1, (size_t)(g_mb_len - g_mb_point));
      g_mb_len--;
    }
    mb_status(); return 1;
  default:
    if (key >= 32 && key <= 126 && g_mb_len < (int)sizeof g_mb_buf - 1) {  /* insert at point */
      memmove(g_mb_buf + g_mb_point + 1, g_mb_buf + g_mb_point, (size_t)(g_mb_len - g_mb_point) + 1);
      g_mb_buf[g_mb_point] = (char)key; g_mb_len++; g_mb_point++;
    }
    mb_status();
    return 1;
  }
}

/* ── find file (C-x C-f) ───────────────────────────────────────────────────── */

void find_file_done(const char *input)
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

void start_find_file(void)
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

/* ── goto-line (M-g M-g / M-g g / M-x goto-line) ────────────────────────────── */

void goto_line_done(const char *input)
{
  long ln;
  int nlines;
  char *end;
  if (!g_ed || !input || !input[0]) { g_message[0] = '\0'; return; }
  ln = strtol(input, &end, 10);
  if (end == input) { snprintf(g_message, sizeof g_message, "Not a line number: %s", input); return; }
  nlines = gtcaca_editor_get_line_count(g_ed);
  if (ln < 1) ln = 1;
  if (ln > nlines) ln = nlines;
  gtcaca_editor_goto_line(g_ed, (int)(ln - 1));   /* goto_line is 0-based */
  snprintf(g_message, sizeof g_message, "Line %ld", ln);
}

void start_goto_line(void) { start_minibuffer("Goto line: ", goto_line_done); }

/* ── replace (two-phase prompt; uses the search/replace API) ───────────────── */

static char g_replace_search[256];

void replace_phase2(const char *with)
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

void replace_phase1(const char *search)
{
  if (!search[0]) { snprintf(g_message, sizeof g_message, "Nothing to replace"); return; }
  strncpy(g_replace_search, search, sizeof g_replace_search - 1);
  g_replace_search[sizeof g_replace_search - 1] = '\0';
  start_minibuffer("Replace with: ", replace_phase2);
}

void start_replace(void) { start_minibuffer("Replace: ", replace_phase1); }

/* ── query replace (M-%): step through matches, ask y/n/!/./q at each ───────── */

int g_qr_active = 0;
static char g_qr_search[256], g_qr_with[256];
static int  g_qr_count = 0, g_qr_regexp = 0;

static void qr_prompt(void)
{
  snprintf(g_message, sizeof g_message,
           "Query replac%s %s with %s  (y replace, n skip, ! all, . last, q quit)",
           g_qr_regexp ? "ing regexp" : "ing", g_qr_search, g_qr_with);
}

/* Find the next match in [from, end]; on success select it (so it's highlighted)
   and return 1, else 0. Honours the regexp flag chosen for this session. */
static int qr_search_from(int from)
{
  int s, e, len = gtcaca_editor_get_length(g_ed);
  gtcaca_editor_set_search_flags(g_ed, g_qr_regexp ? GTCACA_EDITOR_FIND_REGEXP : 0);
  gtcaca_editor_set_target_range(g_ed, from, len);
  if (gtcaca_editor_search_in_target(g_ed, g_qr_search) < 0) return 0;
  s = gtcaca_editor_get_target_start(g_ed);
  e = gtcaca_editor_get_target_end(g_ed);
  gtcaca_editor_set_selection(g_ed, e, s);   /* caret at end, anchor at start */
  return 1;
}

static void qr_finish(void)
{
  g_qr_active = 0;
  gtcaca_editor_set_search_flags(g_ed, 0);
  gtcaca_editor_set_empty_selection(g_ed, gtcaca_editor_get_current_pos(g_ed));
  snprintf(g_message, sizeof g_message, "Replaced %d occurrence%s",
           g_qr_count, g_qr_count == 1 ? "" : "s");
}

int query_replace_key(gtcaca_editor_widget_t *ed, int key)
{
  int from;
  switch (key) {
  case 'y': case ' ':                                   /* replace this, go to next */
    gtcaca_editor_replace_target(ed, g_qr_with); g_qr_count++;
    from = gtcaca_editor_get_target_end(ed);
    if (qr_search_from(from)) qr_prompt(); else qr_finish();
    return 1;
  case 'n': case CACA_KEY_DELETE: case CACA_KEY_BACKSPACE:
    from = gtcaca_editor_get_target_end(ed);           /* skip this one */
    if (qr_search_from(from)) qr_prompt(); else qr_finish();
    return 1;
  case '!':                                             /* replace this and all the rest */
    for (;;) {
      gtcaca_editor_replace_target(ed, g_qr_with); g_qr_count++;
      from = gtcaca_editor_get_target_end(ed);
      if (!qr_search_from(from)) break;
    }
    qr_finish();
    return 1;
  case '.':                                             /* replace this one, then stop */
    gtcaca_editor_replace_target(ed, g_qr_with); g_qr_count++;
    qr_finish();
    return 1;
  case 'q': case 10: case CACA_KEY_RETURN: case CACA_KEY_ESCAPE:
    qr_finish();
    return 1;
  default:
    return 1;                                           /* swallow other keys while stepping */
  }
}

static void qr_get_with(const char *with)
{
  strncpy(g_qr_with, with, sizeof g_qr_with - 1); g_qr_with[sizeof g_qr_with - 1] = '\0';
  g_qr_count = 0;
  if (qr_search_from(gtcaca_editor_get_current_pos(g_ed))) { g_qr_active = 1; qr_prompt(); }
  else snprintf(g_message, sizeof g_message, "No occurrences of %s", g_qr_search);
}

static void qr_get_search(const char *search)
{
  char prompt[160];
  if (!search[0]) { snprintf(g_message, sizeof g_message, "Nothing to replace"); return; }
  strncpy(g_qr_search, search, sizeof g_qr_search - 1); g_qr_search[sizeof g_qr_search - 1] = '\0';
  snprintf(prompt, sizeof prompt, "Query replace %s%s with: ", g_qr_regexp ? "regexp " : "", g_qr_search);
  start_minibuffer(prompt, qr_get_with);
}

void start_query_replace(void)        { g_qr_regexp = 0; start_minibuffer("Query replace: ", qr_get_search); }
void start_query_replace_regexp(void) { g_qr_regexp = 1; start_minibuffer("Query replace regexp: ", qr_get_search); }

/* C-x r t — prompt for a string and insert it into the rectangle on each line */
void string_rect_done(const char *s)
{
  gtcaca_editor_rect_string_insert(g_ed, s);
  g_mark_active = 0;
  snprintf(g_message, sizeof g_message, "String rectangle inserted");
}
void start_string_rectangle(void) { start_minibuffer("String rectangle: ", string_rect_done); }

/* M-x — run a command by name (a small set, extend as needed) */
void mx_done(const char *cmd)
{
  if (!strcmp(cmd, "help") || !strcmp(cmd, "describe-bindings") || !strcmp(cmd, "?"))
    show_help();
  else if (!strcmp(cmd, "snake"))
    run_snake();
  else if (!strcmp(cmd, "sokoban"))
    run_sokoban();
  else if (!strcmp(cmd, "query-replace"))
    start_query_replace();
  else if (!strcmp(cmd, "query-replace-regexp"))
    start_query_replace_regexp();
  else if (!strcmp(cmd, "replace-string"))
    start_replace();
  else if (!strcmp(cmd, "goto-line"))
    start_goto_line();
  else if (cmd[0])
    snprintf(g_message, sizeof g_message, "No command: %s  (try: help, snake, sokoban, query-replace)", cmd);
}
void start_mx(void) { start_minibuffer_init("M-x ", mx_done, 2, NULL); }  /* 2 = command completion */

