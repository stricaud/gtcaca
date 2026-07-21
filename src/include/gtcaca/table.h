#ifndef _GTCACA_TABLE_H_
#define _GTCACA_TABLE_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Table widget with a lazy model/view (cf. Qt's QTableView, termui Table) ──
 *
 * Rows live behind a *model*: the view asks for `row_count` (which may be in the
 * billions) and renders only the rows currently on screen, calling `cell` for
 * each visible cell. Nothing is loaded up front, so scrolling a billion-row
 * table costs only the visible window. */

typedef struct gtcaca_table_model gtcaca_table_model_t;
struct gtcaca_table_model {
  long (*row_count)(gtcaca_table_model_t *m);                       /* total rows */
  int  (*col_count)(gtcaca_table_model_t *m);                       /* total columns */
  void (*header)(gtcaca_table_model_t *m, int col, char *buf, int buflen);
  void (*cell)(gtcaca_table_model_t *m, long row, int col, char *buf, int buflen);
  void *userdata;

  /* Optional per-row colors (Wireshark-style coloring rules). Return non-zero
     and fill in the fg and bg arguments to paint the row; return 0 for the theme
     default. Called only for rows actually on screen, so it may compute lazily.
     The selected row keeps the selection color regardless, so the cursor stays
     visible on top of a colored row. NULL = no coloring. */
  int  (*row_color)(gtcaca_table_model_t *m, long row, uint8_t *fg, uint8_t *bg);
};

typedef struct _gtcaca_table_widget_t gtcaca_table_widget_t;
struct _gtcaca_table_widget_t {
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
  struct _gtcaca_table_widget_t *prev;
  struct _gtcaca_table_widget_t *next;

  gtcaca_table_model_t *model;
  long  top;            /* first visible row                          */
  long  sel;            /* selected (current) row                     */
  int   cur_col;        /* current column (cell navigation)           */
  int  *colw;           /* per-column widths (NULL = auto, equal)     */
  int   ncolw;
  char *title;
  uint8_t sel_bg;
};

gtcaca_table_widget_t *gtcaca_table_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void  gtcaca_table_set_model(gtcaca_table_widget_t *t, gtcaca_table_model_t *model);
void  gtcaca_table_set_column_widths(gtcaca_table_widget_t *t, const int *widths, int n);
void  gtcaca_table_draw(gtcaca_table_widget_t *t);
int   gtcaca_table_key(gtcaca_table_widget_t *t, int key, void *userdata);
long  gtcaca_table_selected_row(gtcaca_table_widget_t *t);
long  gtcaca_table_current_row(gtcaca_table_widget_t *t); /* the current cell's row (pairs with current_col) */
int   gtcaca_table_current_col(gtcaca_table_widget_t *t);
void  gtcaca_table_set_current(gtcaca_table_widget_t *t, long row, int col);
void  gtcaca_table_set_title(gtcaca_table_widget_t *t, const char *title);
void  gtcaca_table_free(gtcaca_table_widget_t *t);

#endif /* _GTCACA_TABLE_H_ */
