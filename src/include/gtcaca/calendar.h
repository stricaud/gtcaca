#ifndef _GTCACA_CALENDAR_H_
#define _GTCACA_CALENDAR_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Month calendar widget ───────────────────────────────────────────────────
 *
 * Shows one month as a grid of days with a highlighted selection and a marked
 * "today". Arrows move by day/week, PageUp/PageDown change month. Days can be
 * dotted via a marker callback (e.g. days that have to-dos). */

typedef int (*gtcaca_calendar_marker_cb)(int year, int month, int day, void *userdata);

typedef struct _gtcaca_calendar_widget_t gtcaca_calendar_widget_t;
struct _gtcaca_calendar_widget_t {
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  uint8_t color_focus_fg;
  uint8_t color_focus_bg;
  uint8_t color_nonfocus_fg;
  uint8_t color_nonfocus_bg;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children;
  struct _gtcaca_calendar_widget_t *prev;
  struct _gtcaca_calendar_widget_t *next;

  int year, month, day;            /* the selected date (month 1-12)        */
  int today_y, today_m, today_d;   /* highlighted as "today"                */
  char *title;
  gtcaca_calendar_marker_cb marker; /* returns nonzero to dot a day         */
  void *marker_ud;
  uint8_t sel_fg, sel_bg;
};

gtcaca_calendar_widget_t *gtcaca_calendar_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_calendar_set_date(gtcaca_calendar_widget_t *c, int year, int month, int day);
void gtcaca_calendar_get_date(gtcaca_calendar_widget_t *c, int *year, int *month, int *day);
void gtcaca_calendar_set_marker(gtcaca_calendar_widget_t *c, gtcaca_calendar_marker_cb cb, void *userdata);
void gtcaca_calendar_set_title(gtcaca_calendar_widget_t *c, const char *title);
void gtcaca_calendar_draw(gtcaca_calendar_widget_t *c);
/* arrows move by day (Left/Right) and week (Up/Down); PageUp/Down change month */
int  gtcaca_calendar_key(gtcaca_calendar_widget_t *c, int key, void *userdata);
void gtcaca_calendar_free(gtcaca_calendar_widget_t *c);

/* helpers (also handy for apps) */
int  gtcaca_calendar_days_in_month(int year, int month);
int  gtcaca_calendar_day_of_week(int year, int month, int day);  /* 0=Sun..6=Sat */

#endif /* _GTCACA_CALENDAR_H_ */
