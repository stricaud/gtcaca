#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/textview.h>
#include <gtcaca/main.h>
#include <gtcaca/utarray.h>

static int _gtcaca_textview_private_key_press(gtcaca_textview_widget_t *tv, int key, void *userdata)
{
  int n_lines = (int)utarray_len(tv->lines);
  int view_height = tv->height - 2;

  switch (key) {
  case CACA_KEY_UP:
    if (tv->scroll_pos > 0) tv->scroll_pos--;
    break;
  case CACA_KEY_DOWN:
    if (tv->scroll_pos < n_lines - view_height) tv->scroll_pos++;
    break;
  case CACA_KEY_HOME:
    tv->scroll_pos = 0;
    break;
  case CACA_KEY_END:
    if (n_lines > view_height) tv->scroll_pos = n_lines - view_height;
    else tv->scroll_pos = 0;
    break;
  }
  return 0;
}

gtcaca_textview_widget_t *gtcaca_textview_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_textview_widget_t *tv;

  tv = malloc(sizeof(gtcaca_textview_widget_t));
  if (!tv) {
    fprintf(stderr, "Error: Cannot allocate textview\n");
    return NULL;
  }

  tv->id = gtcaca_get_newid();
  tv->has_focus = 0;
  tv->is_visible = 1;
  tv->type = GTCACA_WIDGET_TEXTVIEW;
  tv->parent = parent;
  tv->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(tv), x, y);
  tv->width = width;
  tv->height = height;

  tv->color_focus_fg = gmo.theme.textviewfocus.fg;
  tv->color_focus_bg = gmo.theme.textviewfocus.bg;
  tv->color_nonfocus_fg = gmo.theme.textview.fg;
  tv->color_nonfocus_bg = gmo.theme.textview.bg;

  utarray_new(tv->lines, &ut_str_icd);
  tv->scroll_pos = 0;
  tv->private_key_cb = _gtcaca_textview_private_key_press;
  tv->key_cb = NULL;
  tv->key_cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(tv));

  return tv;
}

void gtcaca_textview_draw(gtcaca_textview_widget_t *tv)
{
  int view_height = tv->height - 2;
  int view_width = tv->width - 2;
  int n_lines = (int)utarray_len(tv->lines);
  int i, row;
  char buf[256];

  gtcaca_widget_colorize(GTCACA_WIDGET(tv));
  caca_fill_box(gmo.cv, tv->x, tv->y, tv->width, tv->height, ' ');
  caca_draw_cp437_box(gmo.cv, tv->x, tv->y, tv->width, tv->height);

  if (view_height <= 0 || view_width <= 0) return;

  row = 0;
  for (i = tv->scroll_pos; i < n_lines && row < view_height; i++) {
    char **line = (char **)utarray_eltptr(tv->lines, i);
    if (line && *line) {
      int copy_len = (int)strlen(*line);
      if (copy_len > view_width) copy_len = view_width;
      memset(buf, ' ', view_width);
      buf[view_width] = '\0';
      if (copy_len > 0) memcpy(buf, *line, copy_len);
      caca_printf(gmo.cv, tv->x + 1, tv->y + 1 + row, "%s", buf);
    }
    row++;
  }

  /* Scrollbar indicator */
  if (n_lines > view_height) {
    int sb_pos = tv->y + 1;
    if (n_lines > view_height)
      sb_pos += (tv->scroll_pos * (view_height - 1)) / (n_lines - view_height);
    caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_DARKGRAY);
    caca_put_char(gmo.cv, tv->x + tv->width - 1, sb_pos, '#');
  }
}

void gtcaca_textview_append(gtcaca_textview_widget_t *tv, const char *line)
{
  int n_lines, view_height;
  utarray_push_back(tv->lines, &line);

  /* Auto-scroll to bottom */
  n_lines = (int)utarray_len(tv->lines);
  view_height = tv->height - 2;
  if (n_lines > view_height)
    tv->scroll_pos = n_lines - view_height;
}

void gtcaca_textview_clear(gtcaca_textview_widget_t *tv)
{
  utarray_clear(tv->lines);
  tv->scroll_pos = 0;
}

int gtcaca_textview_key_cb_register(gtcaca_textview_widget_t *widget, gtcaca_textview_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
  return 0;
}
