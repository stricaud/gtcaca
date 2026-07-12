#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/table.h>
#include <gtcaca/main.h>

gtcaca_table_widget_t *gtcaca_table_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_table_widget_t *t = malloc(sizeof(*t));
  if (!t) { fprintf(stderr, "Error: Cannot allocate table\n"); return NULL; }
  t->id = gtcaca_get_newid();
  t->has_focus = 0;
  t->is_visible = 1;
  t->type = GTCACA_WIDGET_TABLE;
  t->parent = parent;
  t->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(t), x, y);
  t->width = width; t->height = height;
  t->color_focus_fg = t->color_nonfocus_fg = gmo.theme.textview.fg;
  t->color_focus_bg = t->color_nonfocus_bg = gmo.theme.textview.bg;
  t->model = NULL; t->top = 0; t->sel = 0; t->cur_col = 0; t->colw = NULL; t->ncolw = 0;
  t->title = NULL; t->sel_bg = CACA_BLUE;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(t));
  return t;
}

void gtcaca_table_set_model(gtcaca_table_widget_t *t, gtcaca_table_model_t *model)
{
  t->model = model; t->top = t->sel = 0;
}

void gtcaca_table_set_column_widths(gtcaca_table_widget_t *t, const int *widths, int n)
{
  free(t->colw); t->colw = NULL; t->ncolw = 0;
  if (n > 0) {
    t->colw = malloc((size_t)n * sizeof(int));
    if (t->colw) { memcpy(t->colw, widths, (size_t)n * sizeof(int)); t->ncolw = n; }
  }
}

void gtcaca_table_set_title(gtcaca_table_widget_t *t, const char *title)
{
  free(t->title); t->title = title ? strdup(title) : NULL;
}

void gtcaca_table_free(gtcaca_table_widget_t *t)
{
  if (!t) return;
  free(t->colw); free(t->title); free(t);
}

long gtcaca_table_selected_row(gtcaca_table_widget_t *t) { return t->sel; }
int  gtcaca_table_current_col(gtcaca_table_widget_t *t) { return t->cur_col; }
void gtcaca_table_set_current(gtcaca_table_widget_t *t, long row, int col)
{
  long rows = t->model ? t->model->row_count(t->model) : 0;
  int  cols = t->model ? t->model->col_count(t->model) : 0;
  if (row < 0) row = 0; if (rows && row > rows - 1) row = rows - 1;
  if (col < 0) col = 0; if (cols && col > cols - 1) col = cols - 1;
  t->sel = row; t->cur_col = col;
}

/* width of column `c`; falls back to an equal split of the inner width */
static int _colwidth(gtcaca_table_widget_t *t, int c, int ncol, int inner_w)
{
  if (t->colw && c < t->ncolw) return t->colw[c];
  return ncol > 0 ? inner_w / ncol : inner_w;
}

void gtcaca_table_draw(gtcaca_table_widget_t *t)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = t->x + 1, inner_y = t->y + 1;
  int inner_w = t->width - 2, inner_h = t->height - 2;
  int ncol, c, i, header_h = 1;
  long rows;
  char buf[256];

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, t->x, t->y, t->width, t->height, ' ');
  caca_draw_cp437_box(gmo.cv, t->x, t->y, t->width, t->height);
  if (t->title) caca_printf(gmo.cv, t->x + 2, t->y, "| %s |", t->title);
  if (!t->model || inner_w <= 0 || inner_h <= 1) return;

  ncol = t->model->col_count(t->model);
  rows = t->model->row_count(t->model);

  /* header row (bold), then a separator line */
  {
    int cx = 0;
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
    for (c = 0; c < ncol; c++) {
      int w = _colwidth(t, c, ncol, inner_w);
      if (cx >= inner_w) break;
      t->model->header(t->model, c, buf, sizeof buf);
      caca_printf(gmo.cv, inner_x + cx, inner_y, "%-.*s", w - 1 < inner_w - cx ? w - 1 : inner_w - cx, buf);
      cx += w;
    }
    caca_set_attr(gmo.cv, 0);
  }
  /* separator under the header (uses the box tee glyphs visually via ─) */
  caca_set_color_ansi(gmo.cv, fg, bg);
  for (c = 0; c < inner_w; c++) caca_put_char(gmo.cv, inner_x + c, inner_y + header_h, 0x2500); /* ─ */

  /* keep the selected row on-screen (so programmatic set_current — e.g. a live
     capture following new packets — scrolls the window, like the tree does) */
  {
    int data_rows = inner_h - header_h - 1;
    if (data_rows < 1) data_rows = 1;
    if (t->sel < t->top)                    t->top = t->sel;
    else if (t->sel >= t->top + data_rows)  t->top = t->sel - data_rows + 1;
    if (t->top < 0) t->top = 0;
  }

  /* visible data rows (windowed: only on-screen rows are fetched) */
  for (i = 0; i + header_h + 1 < inner_h + 0 && i < inner_h - header_h - 1; i++) {
    long row = t->top + i;
    int y = inner_y + header_h + 1 + i, cx = 0, selected, is_sel;
    uint8_t rbg;
    if (row >= rows) break;
    is_sel   = (row == t->sel);
    selected = is_sel && t->has_focus;         /* active selection drives cell cursor */
    /* The selected row stays visible even when the table isn't focused, drawn
       with a muted background so the active pane is still distinguishable. */
    rbg = is_sel ? (t->has_focus ? t->sel_bg : CACA_DARKGRAY) : bg;
    caca_set_color_ansi(gmo.cv, is_sel ? CACA_WHITE : fg, rbg); caca_set_attr(gmo.cv, 0);
    { int col; for (col = 0; col < inner_w; col++) caca_put_char(gmo.cv, inner_x + col, y, ' '); }
    for (c = 0; c < ncol; c++) {
      int w = _colwidth(t, c, ncol, inner_w);
      int cell_cur = selected && c == t->cur_col, avail = inner_w - cx, k;
      if (cx >= inner_w) break;
      if (cell_cur) {                          /* highlight the current cell */
        caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN); caca_set_attr(gmo.cv, 0);
        for (k = 0; k < w && cx + k < inner_w; k++) caca_put_char(gmo.cv, inner_x + cx + k, y, ' ');
      } else {
        caca_set_color_ansi(gmo.cv, is_sel ? CACA_WHITE : fg, rbg); caca_set_attr(gmo.cv, 0);
      }
      t->model->cell(t->model, row, c, buf, sizeof buf);
      caca_printf(gmo.cv, inner_x + cx, y, "%-.*s", w - 1 < avail ? w - 1 : avail, buf);
      cx += w;
    }
  }
}

int gtcaca_table_key(gtcaca_table_widget_t *t, int key, void *userdata)
{
  long rows;
  int page = t->height - 4;          /* inner minus header+separator */
  (void)userdata;
  if (!t->model) return 0;
  rows = t->model->row_count(t->model);
  if (page < 1) page = 1;

  switch (key) {
  case CACA_KEY_UP:       if (t->sel > 0) t->sel--; break;
  case CACA_KEY_DOWN:     if (t->sel < rows - 1) t->sel++; break;
  case CACA_KEY_PAGEUP:   t->sel -= page; break;
  case CACA_KEY_PAGEDOWN: t->sel += page; break;
  case CACA_KEY_HOME:     t->sel = 0; break;
  case CACA_KEY_END:      t->sel = rows - 1; break;
  case CACA_KEY_LEFT:     if (t->cur_col > 0) t->cur_col--; break;
  case CACA_KEY_RIGHT: {
    int cols = t->model->col_count(t->model);
    if (t->cur_col < cols - 1) t->cur_col++;
    break;
  }
  default: return 0;
  }
  if (t->sel < 0) t->sel = 0;
  if (t->sel > rows - 1) t->sel = rows - 1;
  if (t->sel < t->top) t->top = t->sel;
  if (t->sel >= t->top + page) t->top = t->sel - page + 1;
  if (t->top < 0) t->top = 0;
  return 1;
}
