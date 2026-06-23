/*
 * rtodo — a calendar-driven to-do list built on the gtcaca `calendar` widget.
 * To-dos are attached to days; days that have to-dos are dotted on the calendar.
 * Saves a small native file and exports iCalendar (.ics) that Google Calendar
 * (and any iCal app) can import — each to-do becomes an all-day event.
 *
 * Usage: rtodo [todos.txt]
 *
 * Keys:
 *   Arrows        move the day (Left/Right), week (Up/Down)
 *   PgUp/PgDn     previous / next month
 *   Tab           switch focus between the calendar and the day's list
 *   a             add a to-do to the selected day
 *   Enter         toggle done (when the list is focused)
 *   e             edit the selected to-do
 *   d / Del       delete the selected to-do
 *   C-s           save (native)        x  export .ics
 *   q             quit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include <caca.h>
#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/calendar.h>
#include <gtcaca/dialog.h>
#include <gtcaca/filechooser.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define KEY_CTRL_S 0x13
#define MAXTODO 8192

typedef struct { int y, m, d, done; char text[256]; } todo_t;
static todo_t g_todos[MAXTODO];
static int    g_ntodo = 0;
static char   g_path[PATH_MAX] = "todos.txt";
static gtcaca_calendar_widget_t *g_cal;
static int    g_focus_list = 0;     /* 0 = calendar, 1 = the day's list */
static int    g_list_sel = 0;
static int    g_dirty = 0;

/* ── model ──────────────────────────────────────────────────────────────────── */
static int day_has_todo(int y, int m, int d, void *ud)
{
  int i; (void)ud;
  for (i = 0; i < g_ntodo; i++)
    if (g_todos[i].y == y && g_todos[i].m == m && g_todos[i].d == d) return 1;
  return 0;
}

/* indices of the to-dos on (y,m,d), in order; returns the count */
static int todos_for(int y, int m, int d, int *idx, int max)
{
  int i, n = 0;
  for (i = 0; i < g_ntodo && n < max; i++)
    if (g_todos[i].y == y && g_todos[i].m == m && g_todos[i].d == d) idx[n++] = i;
  return n;
}

static void add_todo(int y, int m, int d, const char *text)
{
  if (g_ntodo >= MAXTODO) return;
  g_todos[g_ntodo].y = y; g_todos[g_ntodo].m = m; g_todos[g_ntodo].d = d;
  g_todos[g_ntodo].done = 0;
  snprintf(g_todos[g_ntodo].text, sizeof g_todos[0].text, "%s", text);
  g_ntodo++; g_dirty = 1;
}

static void delete_todo(int gi)
{
  int i;
  if (gi < 0 || gi >= g_ntodo) return;
  for (i = gi; i < g_ntodo - 1; i++) g_todos[i] = g_todos[i + 1];
  g_ntodo--; g_dirty = 1;
}

/* ── native load / save ─────────────────────────────────────────────────────── */
static int save_native(const char *path)
{
  FILE *f = fopen(path, "w"); int i;
  if (!f) return 0;
  for (i = 0; i < g_ntodo; i++)
    fprintf(f, "%04d-%02d-%02d|%d|%s\n", g_todos[i].y, g_todos[i].m, g_todos[i].d,
            g_todos[i].done, g_todos[i].text);
  fclose(f); return 1;
}

static void load_native(const char *path)
{
  FILE *f = fopen(path, "r"); char line[512];
  if (!f) return;
  g_ntodo = 0;
  while (fgets(line, sizeof line, f) && g_ntodo < MAXTODO) {
    int y, m, d, done; char *p;
    if (sscanf(line, "%d-%d-%d|%d|", &y, &m, &d, &done) != 4) continue;
    p = strchr(line, '|'); if (p) p = strchr(p + 1, '|'); if (!p) continue;
    p++;
    { char *nl = strchr(p, '\n'); if (nl) *nl = '\0'; }
    g_todos[g_ntodo].y = y; g_todos[g_ntodo].m = m; g_todos[g_ntodo].d = d;
    g_todos[g_ntodo].done = done;
    snprintf(g_todos[g_ntodo].text, sizeof g_todos[0].text, "%s", p);
    g_ntodo++;
  }
  fclose(f);
}

/* ── iCalendar (.ics) export ────────────────────────────────────────────────── */
static void ics_escape(FILE *f, const char *s)
{
  for (; *s; s++) {
    if (*s == '\\' || *s == ',' || *s == ';') { fputc('\\', f); fputc(*s, f); }
    else if (*s == '\n') fputs("\\n", f);
    else fputc(*s, f);
  }
}

static int export_ics(const char *path)
{
  FILE *f = fopen(path, "w");
  time_t now = time(NULL);
  struct tm *g = gmtime(&now);
  char stamp[32];
  int i;
  if (!f) return 0;
  strftime(stamp, sizeof stamp, "%Y%m%dT%H%M%SZ", g);
  fputs("BEGIN:VCALENDAR\r\n", f);
  fputs("VERSION:2.0\r\n", f);
  fputs("PRODID:-//gtcaca//rtodo//EN\r\n", f);
  fputs("CALSCALE:GREGORIAN\r\n", f);
  for (i = 0; i < g_ntodo; i++) {
    todo_t *t = &g_todos[i];
    int ny = t->y, nm = t->m, nd = t->d + 1;   /* DTEND = next day (all-day) */
    if (nd > gtcaca_calendar_days_in_month(ny, nm)) { nd = 1; if (++nm > 12) { nm = 1; ny++; } }
    fputs("BEGIN:VEVENT\r\n", f);
    fprintf(f, "UID:rtodo-%d-%04d%02d%02d@gtcaca\r\n", i, t->y, t->m, t->d);
    fprintf(f, "DTSTAMP:%s\r\n", stamp);
    fprintf(f, "DTSTART;VALUE=DATE:%04d%02d%02d\r\n", t->y, t->m, t->d);
    fprintf(f, "DTEND;VALUE=DATE:%04d%02d%02d\r\n", ny, nm, nd);
    fputs("SUMMARY:", f);
    if (t->done) fputs("[done] ", f);
    ics_escape(f, t->text);
    fputs("\r\n", f);
    fprintf(f, "STATUS:%s\r\n", t->done ? "CONFIRMED" : "TENTATIVE");
    fputs("END:VEVENT\r\n", f);
  }
  fputs("END:VCALENDAR\r\n", f);
  fclose(f);
  return 1;
}

/* ── minibuffer prompt ──────────────────────────────────────────────────────── */
static int prompt(const char *label, char *out, int outlen, const char *initial)
{
  int W = caca_get_canvas_width(gmo.cv), H = caca_get_canvas_height(gmo.cv), len = 0;
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
      if (k == CACA_KEY_ESCAPE) return 0;
      if ((k == CACA_KEY_BACKSPACE || k == CACA_KEY_DELETE) && len > 0) out[--len] = '\0';
      else if (k >= 32 && k < 127 && len < outlen - 1) { out[len++] = (char)k; out[len] = '\0'; }
    }
  }
}

/* ── the day's to-do panel (drawn directly, to the right of the calendar) ────── */
static void draw_panel(int px, int py, int pw, int ph)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int y, m, d, idx[256], n, i;
  char title[64];
  gtcaca_calendar_get_date(g_cal, &y, &m, &d);
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, px, py, pw, ph, ' ');
  caca_draw_cp437_box(gmo.cv, px, py, pw, ph);
  snprintf(title, sizeof title, "%04d-%02d-%02d", y, m, d);
  caca_printf(gmo.cv, px + 2, py, "| %s |", title);

  n = todos_for(y, m, d, idx, 256);
  if (g_list_sel >= n) g_list_sel = n > 0 ? n - 1 : 0;
  if (n == 0) {
    caca_set_color_ansi(gmo.cv, CACA_DARKGRAY, bg);
    caca_printf(gmo.cv, px + 2, py + 1, "(no to-dos — press 'a' to add)");
    return;
  }
  for (i = 0; i < n && i < ph - 2; i++) {
    todo_t *t = &g_todos[idx[i]];
    int sel = (g_focus_list && i == g_list_sel);
    if (sel) { caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN); caca_set_attr(gmo.cv, CACA_BOLD); }
    else { caca_set_color_ansi(gmo.cv, t->done ? CACA_DARKGRAY : fg, bg); caca_set_attr(gmo.cv, 0); }
    caca_printf(gmo.cv, px + 1, py + 1 + i, "[%c] %-.*s", t->done ? 'x' : ' ', pw - 6, t->text);
  }
  caca_set_attr(gmo.cv, 0);
}

static void status(const char *msg)
{
  int W = caca_get_canvas_width(gmo.cv), H = caca_get_canvas_height(gmo.cv), x;
  caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN); caca_set_attr(gmo.cv, 0);
  for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, H - 1, ' ');
  caca_printf(gmo.cv, 0, H - 1, " rtodo  %s%s | Tab focus  a add  Enter done  e edit  d del  o open  C-s save  x export  q quit",
              g_path, g_dirty ? " *" : "");
  (void)msg;
}

/* the index in g_todos of the currently-selected list row, or -1 */
static int selected_global(void)
{
  int y, m, d, idx[256], n;
  gtcaca_calendar_get_date(g_cal, &y, &m, &d);
  n = todos_for(y, m, d, idx, 256);
  if (n == 0 || g_list_sel < 0 || g_list_sel >= n) return -1;
  return idx[g_list_sel];
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  caca_event_t ev;
  int quit = 0, ctrl_x = 0, cw, chh, calw;

  gtcaca_init(&argc, &argv);
  gmo.theme.textview.bg = CACA_BLACK; gmo.theme.textview.fg = CACA_LIGHTGRAY;
  app = gtcaca_application_new("rtodo");
  cw = caca_get_canvas_width(gmo.cv); chh = caca_get_canvas_height(gmo.cv);
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, 0, 0, cw, chh);

  calw = 30; if (calw > cw - 20) calw = cw / 2;
  g_cal = gtcaca_calendar_new(GTCACA_WIDGET(win), 0, 0, calw, chh - 1);
  gtcaca_calendar_set_title(g_cal, "rtodo");
  gtcaca_calendar_set_marker(g_cal, day_has_todo, NULL);
  g_cal->has_focus = 1;

  if (argc > 1) snprintf(g_path, sizeof g_path, "%s", argv[1]);
  load_native(g_path);

  while (!quit) {
    int k;
    /* draw everything ourselves (calendar widget + the day panel + status) */
    caca_set_color_ansi(gmo.cv, gmo.theme.textview.fg, gmo.theme.textview.bg);
    caca_clear_canvas(gmo.cv);
    g_cal->has_focus = !g_focus_list;
    gtcaca_calendar_draw(g_cal);
    draw_panel(calw + 1, 0, cw - calw - 1, chh - 1);
    status("");
    caca_refresh_display(gmo.dp);

    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    k = caca_get_event_key_ch(&ev);

    if (ctrl_x) { ctrl_x = 0; if (k == CACA_KEY_CTRL_C) quit = 1; continue; }

    switch (k) {
    case CACA_KEY_CTRL_X: ctrl_x = 1; break;
    case 'q': case 'Q':
      if (g_dirty && !gtcaca_dialog_confirm("Quit", "Discard unsaved changes?")) break;
      quit = 1; break;
    case '\t': g_focus_list = !g_focus_list; g_list_sel = 0; break;
    case 'o': case 'O': {                     /* open a to-do file (chooser) */
      char path[PATH_MAX];
      if (gtcaca_filechooser_run(".", path, sizeof path, GTCACA_FILECHOOSER_OPEN)) {
        snprintf(g_path, sizeof g_path, "%s", path);
        load_native(g_path); g_dirty = 0; g_focus_list = 0;
      }
      break;
    }
    case KEY_CTRL_S:
      save_native(g_path);
      g_dirty = 0; status(""); caca_refresh_display(gmo.dp);
      caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, 600000);
      break;
    case 'x': case 'X': {                    /* export .ics (save chooser) */
      char ics[PATH_MAX];
      if (gtcaca_filechooser_run(".", ics, sizeof ics, GTCACA_FILECHOOSER_SAVE)) {
        if (!strstr(ics, ".ics")) { size_t n = strlen(ics); snprintf(ics + n, sizeof ics - n, ".ics"); }
        if (export_ics(ics)) gtcaca_dialog_message("Export", "Exported iCalendar (.ics).\nImport it into Google Calendar.");
      }
      break;
    }
    case 'a': case 'A': {                     /* add a to-do to the selected day */
      char buf[256]; int y, m, d;
      gtcaca_calendar_get_date(g_cal, &y, &m, &d);
      if (prompt("New to-do: ", buf, sizeof buf, "") && buf[0]) {
        add_todo(y, m, d, buf);
        g_focus_list = 1;
      }
      break;
    }
    case 'e': case 'E': {                     /* edit selected to-do */
      int gi = selected_global();
      if (gi >= 0) { char buf[256];
        if (prompt("Edit: ", buf, sizeof buf, g_todos[gi].text) && buf[0]) {
          snprintf(g_todos[gi].text, sizeof g_todos[0].text, "%s", buf); g_dirty = 1; } }
      break;
    }
    case 'd': case CACA_KEY_DELETE: case CACA_KEY_BACKSPACE: {
      int gi = selected_global();
      if (gi >= 0) delete_todo(gi);
      break;
    }
    case CACA_KEY_RETURN: case 10: {          /* toggle done */
      int gi = selected_global();
      if (gi >= 0) { g_todos[gi].done = !g_todos[gi].done; g_dirty = 1; }
      break;
    }
    case CACA_KEY_UP:
      if (g_focus_list) { if (g_list_sel > 0) g_list_sel--; }
      else gtcaca_calendar_key(g_cal, k, NULL);
      break;
    case CACA_KEY_DOWN:
      if (g_focus_list) g_list_sel++;
      else gtcaca_calendar_key(g_cal, k, NULL);
      break;
    default:
      if (!g_focus_list) gtcaca_calendar_key(g_cal, k, NULL);
      break;
    }
  }
  caca_set_mouse(gmo.dp, 0);
  caca_free_display(gmo.dp);
  return 0;
}
