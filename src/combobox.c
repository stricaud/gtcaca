#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/combobox.h>
#include <gtcaca/main.h>
#include <gtcaca/utarray.h>

static int _gtcaca_combobox_private_key_press(gtcaca_combobox_widget_t *cb, int key, void *userdata)
{
  int n_items = (int)utarray_len(cb->items);

  if (!cb->is_open) {
    if (key == CACA_KEY_RETURN || key == ' ') {
      cb->is_open = 1;
      cb->active_item = cb->selected_item;
    } else if (key == CACA_KEY_UP) {
      if (cb->selected_item > 0) cb->selected_item--;
    } else if (key == CACA_KEY_DOWN) {
      if (cb->selected_item < n_items - 1) cb->selected_item++;
    }
  } else {
    if (key == CACA_KEY_UP) {
      if (cb->active_item > 0) cb->active_item--;
    } else if (key == CACA_KEY_DOWN) {
      if (cb->active_item < n_items - 1) cb->active_item++;
    } else if (key == CACA_KEY_RETURN || key == ' ') {
      cb->selected_item = cb->active_item;
      cb->is_open = 0;
    } else if (key == CACA_KEY_ESCAPE) {
      cb->is_open = 0;
    }
  }
  return 0;
}

gtcaca_combobox_widget_t *gtcaca_combobox_new(gtcaca_widget_t *parent, int x, int y, int width)
{
  gtcaca_combobox_widget_t *cb;

  cb = malloc(sizeof(gtcaca_combobox_widget_t));
  if (!cb) {
    fprintf(stderr, "Error: Cannot allocate combobox\n");
    return NULL;
  }

  cb->id = gtcaca_get_newid();
  cb->has_focus = 0;
  cb->is_visible = 1;
  cb->type = GTCACA_WIDGET_COMBOBOX;
  cb->parent = parent;
  cb->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(cb), x, y);
  cb->width = width;
  cb->height = 1;

  cb->color_focus_fg = gmo.theme.comboboxfocus.fg;
  cb->color_focus_bg = gmo.theme.comboboxfocus.bg;
  cb->color_nonfocus_fg = gmo.theme.combobox.fg;
  cb->color_nonfocus_bg = gmo.theme.combobox.bg;

  utarray_new(cb->items, &ut_str_icd);
  cb->selected_item = 0;
  cb->active_item = 0;
  cb->is_open = 0;
  cb->private_key_cb = _gtcaca_combobox_private_key_press;
  cb->key_cb = NULL;
  cb->key_cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(cb));

  return cb;
}

void gtcaca_combobox_draw(gtcaca_combobox_widget_t *cb)
{
  int text_width = cb->width - 4; /* [, space for 'v', ], and chevron char */
  int n_items = (int)utarray_len(cb->items);
  char **sel_text;
  char buf[128];
  int i, tlen;

  if (text_width < 1) text_width = 1;

  /* Draw the collapsed bar */
  gtcaca_widget_colorize(GTCACA_WIDGET(cb));
  caca_put_char(gmo.cv, cb->x, cb->y, '[');

  memset(buf, ' ', text_width);
  buf[text_width] = '\0';
  if (n_items > 0) {
    sel_text = (char **)utarray_eltptr(cb->items, cb->selected_item);
    if (sel_text && *sel_text) {
      tlen = (int)strlen(*sel_text);
      if (tlen > text_width) tlen = text_width;
      memcpy(buf, *sel_text, tlen);
    }
  }
  caca_printf(gmo.cv, cb->x + 1, cb->y, "%-*s", text_width, buf);

  caca_put_char(gmo.cv, cb->x + text_width + 1, cb->y, 'v');
  caca_put_char(gmo.cv, cb->x + text_width + 2, cb->y, ']');

  /* Draw open dropdown */
  if (cb->is_open && n_items > 0) {
    int dw = cb->width;
    int dy = cb->y + 1;
    int inner_w = dw - 2;

    caca_set_color_ansi(gmo.cv, gmo.theme.combobox.fg, gmo.theme.combobox.bg);
    caca_fill_box(gmo.cv, cb->x, dy, dw, n_items + 2, ' ');
    caca_draw_cp437_box(gmo.cv, cb->x, dy, dw, n_items + 2);

    for (i = 0; i < n_items; i++) {
      char **item = (char **)utarray_eltptr(cb->items, i);
      if (i == cb->active_item) {
        caca_set_color_ansi(gmo.cv, gmo.theme.comboboxfocus.fg, gmo.theme.comboboxfocus.bg);
      } else {
        caca_set_color_ansi(gmo.cv, gmo.theme.combobox.fg, gmo.theme.combobox.bg);
      }
      memset(buf, ' ', inner_w);
      buf[inner_w] = '\0';
      if (item && *item) {
        tlen = (int)strlen(*item);
        if (tlen > inner_w) tlen = inner_w;
        memcpy(buf, *item, tlen);
      }
      caca_printf(gmo.cv, cb->x + 1, dy + 1 + i, "%-*s", inner_w, buf);
    }
  }
}

void gtcaca_combobox_append(gtcaca_combobox_widget_t *cb, const char *item)
{
  utarray_push_back(cb->items, &item);
}

int gtcaca_combobox_key_cb_register(gtcaca_combobox_widget_t *widget, gtcaca_combobox_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
  return 0;
}

const char *gtcaca_combobox_get_selected(gtcaca_combobox_widget_t *cb)
{
  int n = (int)utarray_len(cb->items);
  char **item;
  if (n == 0 || cb->selected_item >= n) return NULL;
  item = (char **)utarray_eltptr(cb->items, cb->selected_item);
  return item ? *item : NULL;
}

int gtcaca_combobox_get_selected_index(gtcaca_combobox_widget_t *cb)
{
  return cb->selected_item;
}
