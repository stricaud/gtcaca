#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <caca.h>

#include <gtcaca/calendar.h>
#include <gtcaca/main.h>

static const char *MONTHS[] = { "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December" };

int gtcaca_calendar_days_in_month(int y, int m)
{
  static const int d[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
  if (m < 1 || m > 12) return 30;
  return d[m - 1];
}

/* Sakamoto's algorithm: 0 = Sunday .. 6 = Saturday */
int gtcaca_calendar_day_of_week(int y, int m, int d)
{
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

gtcaca_calendar_widget_t *gtcaca_calendar_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_calendar_widget_t *c = malloc(sizeof(*c));
  time_t now; struct tm *lt;
  if (!c) { fprintf(stderr, "Error: Cannot allocate calendar\n"); return NULL; }
  c->id = gtcaca_get_newid();
  c->has_focus = 0; c->is_visible = 1; c->type = GTCACA_WIDGET_CALENDAR;
  c->parent = parent; c->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(c), x, y);
  c->width = width; c->height = height;
  c->color_focus_fg = c->color_nonfocus_fg = gmo.theme.textview.fg;
  c->color_focus_bg = c->color_nonfocus_bg = gmo.theme.textview.bg;
  now = time(NULL); lt = localtime(&now);
  c->year = c->today_y = lt->tm_year + 1900;
  c->month = c->today_m = lt->tm_mon + 1;
  c->day = c->today_d = lt->tm_mday;
  c->title = NULL; c->marker = NULL; c->marker_ud = NULL;
  c->sel_fg = CACA_BLACK; c->sel_bg = CACA_CYAN;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(c));
  return c;
}

void gtcaca_calendar_set_date(gtcaca_calendar_widget_t *c, int y, int m, int d)
{
  int dim;
  if (m < 1) m = 1; if (m > 12) m = 12;
  dim = gtcaca_calendar_days_in_month(y, m);
  if (d < 1) d = 1; if (d > dim) d = dim;
  c->year = y; c->month = m; c->day = d;
}

void gtcaca_calendar_get_date(gtcaca_calendar_widget_t *c, int *y, int *m, int *d)
{ if (y) *y = c->year; if (m) *m = c->month; if (d) *d = c->day; }

void gtcaca_calendar_set_marker(gtcaca_calendar_widget_t *c, gtcaca_calendar_marker_cb cb, void *ud)
{ c->marker = cb; c->marker_ud = ud; }

void gtcaca_calendar_set_title(gtcaca_calendar_widget_t *c, const char *t)
{ free(c->title); c->title = t ? strdup(t) : NULL; }

void gtcaca_calendar_draw(gtcaca_calendar_widget_t *c)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = c->x + 1, inner_y = c->y + 1, inner_w = c->width - 2;
  int cw, first, dim, d, hx;
  char hdr[64];
  static const char *WD[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, c->x, c->y, c->width, c->height, ' ');
  caca_draw_cp437_box(gmo.cv, c->x, c->y, c->width, c->height);
  if (c->title) caca_printf(gmo.cv, c->x + 2, c->y, "| %s |", c->title);
  if (inner_w < 20) return;

  cw = inner_w / 7; if (cw < 3) cw = 3; if (cw > 5) cw = 5;

  /* month / year heading, centred */
  snprintf(hdr, sizeof hdr, "%s %d", MONTHS[c->month - 1], c->year);
  caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
  caca_printf(gmo.cv, inner_x + (7 * cw - (int)strlen(hdr)) / 2, inner_y, "%s", hdr);
  caca_set_attr(gmo.cv, 0);

  /* weekday header */
  for (d = 0; d < 7; d++) {
    caca_set_color_ansi(gmo.cv, (d == 0 || d == 6) ? CACA_LIGHTRED : fg, bg);
    caca_printf(gmo.cv, inner_x + d * cw, inner_y + 1, "%*s", cw, WD[d]);
  }

  first = gtcaca_calendar_day_of_week(c->year, c->month, 1);
  dim = gtcaca_calendar_days_in_month(c->year, c->month);
  for (d = 1; d <= dim; d++) {
    int idx = first + (d - 1), col = idx % 7, week = idx / 7;
    int dx = inner_x + col * cw, dy = inner_y + 2 + week;
    int is_sel = (d == c->day);
    int is_today = (c->year == c->today_y && c->month == c->today_m && d == c->today_d);
    int marked = c->marker && c->marker(c->year, c->month, d, c->marker_ud);
    uint8_t dfg = (col == 0 || col == 6) ? CACA_LIGHTRED : fg, dbg = bg;
    if (dy >= c->y + c->height - 1) break;
    if (is_sel && c->has_focus) { dfg = c->sel_fg; dbg = c->sel_bg; }
    else if (is_today)          { dfg = CACA_LIGHTGREEN; }
    caca_set_color_ansi(gmo.cv, dfg, dbg);
    caca_set_attr(gmo.cv, (is_today || is_sel) ? CACA_BOLD : 0);
    caca_printf(gmo.cv, dx, dy, "%*d", cw - 1, d);
    /* marker dot in the cell's last column */
    if (marked) {
      caca_set_color_ansi(gmo.cv, is_sel && c->has_focus ? c->sel_fg : CACA_LIGHTMAGENTA, dbg);
      caca_put_char(gmo.cv, dx + cw - 1, dy, '*');
    }
  }
  caca_set_attr(gmo.cv, 0);
  (void)hx;
}

/* shift the selected date by `delta` days, rolling months/years over */
static void shift_days(gtcaca_calendar_widget_t *c, int delta)
{
  int y = c->year, m = c->month, d = c->day + delta;
  while (d < 1) { m--; if (m < 1) { m = 12; y--; } d += gtcaca_calendar_days_in_month(y, m); }
  while (d > gtcaca_calendar_days_in_month(y, m)) { d -= gtcaca_calendar_days_in_month(y, m); m++; if (m > 12) { m = 1; y++; } }
  c->year = y; c->month = m; c->day = d;
}

static void shift_month(gtcaca_calendar_widget_t *c, int delta)
{
  int m = c->month - 1 + delta, y = c->year, dim;
  y += m / 12; m %= 12; if (m < 0) { m += 12; y--; }
  c->year = y; c->month = m + 1;
  dim = gtcaca_calendar_days_in_month(c->year, c->month);
  if (c->day > dim) c->day = dim;
}

int gtcaca_calendar_key(gtcaca_calendar_widget_t *c, int key, void *userdata)
{
  (void)userdata;
  switch (key) {
  case CACA_KEY_LEFT:     shift_days(c, -1);  break;
  case CACA_KEY_RIGHT:    shift_days(c, +1);  break;
  case CACA_KEY_UP:       shift_days(c, -7);  break;
  case CACA_KEY_DOWN:     shift_days(c, +7);  break;
  case CACA_KEY_PAGEUP:   shift_month(c, -1); break;
  case CACA_KEY_PAGEDOWN: shift_month(c, +1); break;
  default: return 0;
  }
  return 1;
}

void gtcaca_calendar_free(gtcaca_calendar_widget_t *c)
{
  if (!c) return;
  free(c->title); free(c);
}
