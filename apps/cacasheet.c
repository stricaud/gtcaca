/*
 * cacasheet — a tiny terminal CSV spreadsheet, in the spirit of sc-im, built on
 * the gtcaca Table widget with Emacs-style keys.
 *
 *   cacasheet [file.csv]
 *
 * Move with the arrows (or C-f C-b C-n C-p), Enter edits the current cell, g
 * jumps to a row, C-x C-s saves, C-x C-c (or q) quits. The grid is held in
 * memory; the Table's lazy model reads cells from it on demand.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/table.h>
#include <gtcaca/statusbar.h>

#define KEY_CTRL_S 0x13   /* libcaca has no CACA_KEY_CTRL_S constant */

typedef struct { char **f; int n; } csvrow;

static csvrow *g_rows = NULL;
static int     g_nrows = 0, g_cap = 0, g_ncols = 1;
static char   *g_path = NULL;
static int     g_dirty = 0;
static gtcaca_table_widget_t  *g_tab;
static gtcaca_statusbar_widget_t *g_bar;

/* ── CSV parsing/writing (handles "quoted, fields" with "" escapes) ─────────── */

static char *parse_field(const char **pp, int *last)
{
  const char *p = *pp;
  char *out = malloc(strlen(p) + 1), *o = out;
  if (!out) { *last = 1; return NULL; }
  if (*p == '"') {                       /* quoted field */
    p++;
    while (*p) {
      if (*p == '"' && p[1] == '"') { *o++ = '"'; p += 2; }
      else if (*p == '"') { p++; break; }
      else *o++ = *p++;
    }
    while (*p && *p != ',') p++;         /* skip to delimiter */
  } else {
    while (*p && *p != ',') *o++ = *p++;
  }
  *o = '\0';
  if (*p == ',') { p++; *last = 0; } else *last = 1;
  *pp = p;
  return out;
}

static void add_row(char **fields, int n)
{
  if (g_nrows == g_cap) {
    int nc = g_cap ? g_cap * 2 : 256;
    csvrow *r = realloc(g_rows, (size_t)nc * sizeof(csvrow));
    if (!r) return;
    g_rows = r; g_cap = nc;
  }
  g_rows[g_nrows].f = fields; g_rows[g_nrows].n = n;
  if (n > g_ncols) g_ncols = n;
  g_nrows++;
}

static void load_csv(const char *path)
{
  FILE *f = fopen(path, "r");
  char line[8192];
  if (!f) return;
  while (fgets(line, sizeof line, f)) {
    int len = (int)strlen(line), last = 0, n = 0, cap = 8;
    char **fields = malloc((size_t)cap * sizeof(char *));
    const char *p = line;
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
    if (!fields) break;
    do {
      char *fld = parse_field(&p, &last);
      if (n == cap) { cap *= 2; fields = realloc(fields, (size_t)cap * sizeof(char *)); }
      fields[n++] = fld ? fld : strdup("");
    } while (!last);
    add_row(fields, n);
  }
  fclose(f);
  if (g_nrows == 0) { char **e = malloc(sizeof(char *)); e[0] = strdup(""); add_row(e, 1); }
}

static void write_field(FILE *f, const char *s)
{
  if (s && (strchr(s, ',') || strchr(s, '"') || strchr(s, '\n'))) {
    const char *p; fputc('"', f);
    for (p = s; *p; p++) { if (*p == '"') fputc('"', f); fputc(*p, f); }
    fputc('"', f);
  } else if (s) fputs(s, f);
}

static int save_csv(void)
{
  FILE *f; int r, c;
  if (!g_path) return 0;
  f = fopen(g_path, "w");
  if (!f) return 0;
  for (r = 0; r < g_nrows; r++) {
    for (c = 0; c < g_rows[r].n; c++) { if (c) fputc(',', f); write_field(f, g_rows[r].f[c]); }
    fputc('\n', f);
  }
  fclose(f);
  g_dirty = 0;
  return 1;
}

/* ── grid access + the Table's model ───────────────────────────────────────── */

static const char *cell_get(int r, int c)
{
  if (r < 0 || r >= g_nrows || c < 0 || c >= g_rows[r].n) return "";
  return g_rows[r].f[c] ? g_rows[r].f[c] : "";
}

static void cell_set(int r, int c, const char *v)
{
  if (r < 0 || r >= g_nrows) return;
  if (c >= g_rows[r].n) {                 /* grow the row to reach column c */
    int nn = c + 1, i;
    char **nf = realloc(g_rows[r].f, (size_t)nn * sizeof(char *));
    if (!nf) return;
    for (i = g_rows[r].n; i < nn; i++) nf[i] = strdup("");
    g_rows[r].f = nf; g_rows[r].n = nn;
    if (nn > g_ncols) g_ncols = nn;
  }
  free(g_rows[r].f[c]); g_rows[r].f[c] = strdup(v ? v : "");
  g_dirty = 1;
}

static void colname(int c, char *buf, int len)   /* A, B, …, Z, AA, AB, … */
{
  char tmp[8]; int n = 0;
  do { tmp[n++] = (char)('A' + c % 26); c = c / 26 - 1; } while (c >= 0 && n < 7);
  { int i, j = 0; for (i = n - 1; i >= 0 && j < len - 1; i--) buf[j++] = tmp[i]; buf[j] = 0; }
}

static long m_rowcount(gtcaca_table_model_t *m) { (void)m; return g_nrows; }
static int  m_colcount(gtcaca_table_model_t *m) { (void)m; return g_ncols; }
static void m_header(gtcaca_table_model_t *m, int c, char *b, int l) { (void)m; colname(c, b, l); }
static void m_cell(gtcaca_table_model_t *m, long r, int c, char *b, int l)
{ (void)m; snprintf(b, l, "%s", cell_get((int)r, c)); }

/* ── a one-line prompt at the bottom (for cell edit and goto) ───────────────── */

static int prompt(const char *label, char *out, int outlen, const char *initial)
{
  int W = caca_get_canvas_width(gmo.cv), H = caca_get_canvas_height(gmo.cv);
  int len = 0;
  caca_event_t ev;
  out[0] = '\0';
  if (initial) { snprintf(out, (size_t)outlen, "%s", initial); len = (int)strlen(out); }
  for (;;) {
    int x;
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_WHITE); caca_set_attr(gmo.cv, 0);
    for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, H - 1, ' ');
    caca_printf(gmo.cv, 0, H - 1, "%s%s", label, out);
    caca_refresh_display(gmo.dp);
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    {
      int k = caca_get_event_key_ch(&ev);
      if (k == CACA_KEY_RETURN || k == 10) return 1;
      if (k == CACA_KEY_ESCAPE || k == CACA_KEY_CTRL_G) return 0;
      if ((k == CACA_KEY_BACKSPACE || k == CACA_KEY_DELETE) && len > 0) out[--len] = '\0';
      else if (k >= 32 && k < 127 && len < outlen - 1) { out[len++] = (char)k; out[len] = '\0'; }
    }
  }
}

static void refresh_bar(void)
{
  char ref[16], text[256];
  long r = gtcaca_table_selected_row(g_tab);
  int  c = gtcaca_table_current_col(g_tab);
  colname(c, ref, sizeof ref);
  snprintf(text, sizeof text, " %s%ld  %s   [%-.40s]   (Enter edit, g goto, C-x C-s save, q quit)",
           ref, r + 1, g_dirty ? "*" : "-", cell_get((int)r, c));
  gtcaca_statusbar_set_text(g_bar, text);
}

/* move the current cell to the grid position under (mx,my) */
static void sheet_click(int mx, int my)
{
  int inner_x = g_tab->x + 1, inner_w = g_tab->width - 2;
  int data_y0 = g_tab->y + 3;          /* border + header + separator */
  int ncols = g_ncols > 0 ? g_ncols : 1;
  int colw = inner_w / ncols, col;
  long row;
  if (colw < 1) colw = 1;
  if (my < data_y0 || mx < inner_x) return;
  row = g_tab->top + (my - data_y0);
  col = (mx - inner_x) / colw;
  if (row < 0 || row >= g_nrows) return;
  if (col < 0) col = 0; if (col >= ncols) col = ncols - 1;
  gtcaca_table_set_current(g_tab, row, col);
}

int main(int argc, char **argv)
{
  static gtcaca_table_model_t model = { m_rowcount, m_colcount, m_header, m_cell, NULL };
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  caca_event_t ev;
  int ctrl_x = 0, quit = 0;

  if (argc > 1) { g_path = argv[1]; load_csv(g_path); }
  if (g_nrows == 0) { char **e = malloc(sizeof(char *)); e[0] = strdup(""); add_row(e, 1); g_ncols = 4; }

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("cacasheet");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height - 1);
  g_tab = gtcaca_table_new(GTCACA_WIDGET(win), 0, 0, win->width, win->height);
  gtcaca_table_set_model(g_tab, &model);
  gtcaca_table_set_title(g_tab, g_path ? g_path : "untitled.csv");
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(g_tab));
  g_bar = gtcaca_statusbar_new("");

  caca_set_mouse(gmo.dp, 1);
  while (!quit) {
    int k, t;
    refresh_bar();
    gtcaca_redraw();
    if (!caca_get_event(gmo.dp, CACA_EVENT_ANY, &ev, -1)) continue;
    t = caca_get_event_type(&ev);

    /* mouse: wheel scrolls, left-click moves the current cell. libcaca's
       per-event coordinates are unreliable on some terminals, so we use the
       display's tracked mouse position for the click. */
    if (t & CACA_EVENT_MOUSE_PRESS) {
      int b = caca_get_event_mouse_button(&ev);
      if      (b == 4) gtcaca_table_key(g_tab, CACA_KEY_DOWN, NULL);
      else if (b == 5) gtcaca_table_key(g_tab, CACA_KEY_UP, NULL);
      else if (b == 1) sheet_click(caca_get_mouse_x(gmo.dp), caca_get_mouse_y(gmo.dp));
      continue;
    }
    if (!(t & CACA_EVENT_KEY_PRESS)) continue;
    k = caca_get_event_key_ch(&ev);

    if (ctrl_x) {
      ctrl_x = 0;
      if (k == KEY_CTRL_S) { gtcaca_statusbar_set_text(g_bar, save_csv() ? " Saved" : " Save failed"); }
      else if (k == CACA_KEY_CTRL_C) quit = 1;
      continue;
    }
    switch (k) {
    case CACA_KEY_CTRL_X: ctrl_x = 1; break;
    case 'q': case 'Q': quit = 1; break;
    case CACA_KEY_RETURN: case 10: {       /* edit the current cell */
      long r = gtcaca_table_selected_row(g_tab); int c = gtcaca_table_current_col(g_tab);
      char buf[1024];
      if (prompt("Cell: ", buf, sizeof buf, cell_get((int)r, c))) cell_set((int)r, c, buf);
      break;
    }
    case 'g': case 'G': {                  /* go to a row */
      char buf[32];
      if (prompt("Go to row: ", buf, sizeof buf, "") && buf[0])
        gtcaca_table_set_current(g_tab, atol(buf) - 1, gtcaca_table_current_col(g_tab));
      break;
    }
    default: {                             /* movement (arrows + Emacs keys) */
      int tk = k;
      if      (k == CACA_KEY_CTRL_N) tk = CACA_KEY_DOWN;
      else if (k == CACA_KEY_CTRL_P) tk = CACA_KEY_UP;
      else if (k == CACA_KEY_CTRL_F) tk = CACA_KEY_RIGHT;
      else if (k == CACA_KEY_CTRL_B) tk = CACA_KEY_LEFT;
      else if (k == CACA_KEY_CTRL_V) tk = CACA_KEY_PAGEDOWN;
      else if (k == CACA_KEY_CTRL_A) tk = CACA_KEY_HOME;
      else if (k == CACA_KEY_CTRL_E) tk = CACA_KEY_END;
      gtcaca_table_key(g_tab, tk, NULL);
      break;
    }
    }
  }
  caca_set_mouse(gmo.dp, 0);      /* restore mouse reporting + cursor + screen */
  caca_free_display(gmo.dp);
  return 0;
}
