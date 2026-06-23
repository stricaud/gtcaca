#ifndef _GTCACA_TABS_H_
#define _GTCACA_TABS_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Tabs widget (cf. tui-rs' Tabs) ──────────────────────────────────────────
 *
 * A single-line row of titled tabs inside a box, with one tab highlighted as
 * the selection and the rest separated by a divider. Like tui-rs, the widget
 * only draws the tab bar — the caller renders the body for the selected tab in
 * its own area. Left/Right (and Tab) move the selection; gtcaca_tabs_selected()
 * reports the current index. */

typedef struct _gtcaca_tabs_widget_t gtcaca_tabs_widget_t;
struct _gtcaca_tabs_widget_t {
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
  struct _gtcaca_tabs_widget_t *prev;
  struct _gtcaca_tabs_widget_t *next;

  char  **titles;        /* tab labels (owned copies)       */
  int     ntitles;
  int     sel;           /* selected tab index              */
  char   *title;         /* optional box title              */
  uint8_t sel_fg, sel_bg;/* colours of the selected tab     */
  uint32_t divider;      /* glyph drawn between tabs (0 => │)*/
};

gtcaca_tabs_widget_t *gtcaca_tabs_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
/* set the tab labels (copied); resets the selection if it falls out of range */
void gtcaca_tabs_set_titles(gtcaca_tabs_widget_t *t, const char **titles, int n);
void gtcaca_tabs_set_title(gtcaca_tabs_widget_t *t, const char *title);   /* box title */
void gtcaca_tabs_set_selected(gtcaca_tabs_widget_t *t, int idx);
int  gtcaca_tabs_selected(gtcaca_tabs_widget_t *t);
void gtcaca_tabs_draw(gtcaca_tabs_widget_t *t);
/* Left/Right (and Tab) change the selected tab; returns 1 if the key was used */
int  gtcaca_tabs_key(gtcaca_tabs_widget_t *t, int key, void *userdata);
void gtcaca_tabs_free(gtcaca_tabs_widget_t *t);

#endif /* _GTCACA_TABS_H_ */
