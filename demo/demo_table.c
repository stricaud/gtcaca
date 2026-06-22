/* demo_table — a Table backed by a *lazy* model of one billion rows.
 *
 * row_count() reports a billion, but only the rows currently on screen are ever
 * fetched (cell() is called per visible cell), so scrolling is instant and uses
 * no memory for the data. */
#include <stdint.h>
#include <stdio.h>
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/table.h>

static long m_rows(gtcaca_table_model_t *m) { (void)m; return 1000000000L; }
static int  m_cols(gtcaca_table_model_t *m) { (void)m; return 4; }

static void m_header(gtcaca_table_model_t *m, int col, char *buf, int len)
{
  static const char *h[] = { "ID", "Name", "Value", "Status" };
  (void)m;
  snprintf(buf, len, "%s", col >= 0 && col < 4 ? h[col] : "");
}

static void m_cell(gtcaca_table_model_t *m, long row, int col, char *buf, int len)
{
  (void)m;
  switch (col) {
  case 0: snprintf(buf, len, "%ld", row); break;
  case 1: snprintf(buf, len, "row-%ld", row); break;
  case 2: snprintf(buf, len, "%lu", (unsigned long)((row * 2654435761UL) % 100000UL)); break;
  default: snprintf(buf, len, "%s", row % 3 == 0 ? "OK" : row % 3 == 1 ? "WARN" : "FAIL"); break;
  }
}

int main(int argc, char **argv)
{
  static gtcaca_table_model_t model = { m_rows, m_cols, m_header, m_cell, NULL };
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_table_widget_t *table;
  int widths[4] = { 14, 18, 12, 10 };

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("gtcaca table");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height);

  table = gtcaca_table_new(GTCACA_WIDGET(win), 0, 0, win->width, win->height);
  gtcaca_table_set_model(table, &model);
  gtcaca_table_set_column_widths(table, widths, 4);
  gtcaca_table_set_title(table, "1,000,000,000 rows (lazy)  -  up/down/PageUp/PageDown move, q quit");
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(table));

  gtcaca_main();
  return 0;
}
