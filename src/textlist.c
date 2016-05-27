#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <caca.h>

#include <gtcaca/textlist.h>
#include <gtcaca/main.h>
#include <gtcaca/log.h>

#include <gtcaca/utarray.h>

/* Private functions */
static int _gtcaca_textlist_private_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  switch(key) {
  case CACA_KEY_UP:
    gtcaca_textlist_selection_up(widget);
    break;
  case CACA_KEY_DOWN:
    gtcaca_textlist_selection_down(widget);
    break;
  }  
}

/* Public functions */
gtcaca_textlist_widget_t *gtcaca_textlist_new(gtcaca_widget_t *parent, int x, int y)
{
  gtcaca_textlist_widget_t *textlist;
  int i;

  textlist = malloc(sizeof(gtcaca_textlist_widget_t));
  if (!textlist) {
    fprintf(stderr, "Error: Cannot allocate new textlist\n");
    return NULL;
  }

  textlist->id = gtcaca_get_newid();
  textlist->has_focus = 1;
  textlist->is_visible = 1;

  gtcaca_widget_position_size_parent(parent, (gtcaca_widget_t *)textlist, x, y);

  textlist->type = GTCACA_WIDGET_TEXTLIST;
  textlist->parent = parent;
  textlist->children = NULL;

  textlist->color_focus_bg = gmo.theme.textfocus.bg;
  textlist->color_focus_fg = gmo.theme.textfocus.fg;
  textlist->color_nonfocus_bg = gmo.theme.text.bg;
  textlist->color_nonfocus_fg = gmo.theme.text.fg;

  /* if (parent) { */
  /*   CDL_APPEND(parent->children, (gtcaca_widget_t *)textlist); */
  /* } */

  textlist->selected_item = 0;
  textlist->private_key_cb = _gtcaca_textlist_private_key_press;
  textlist->key_cb = NULL;
  textlist->key_cb_userdata;
  
  utarray_new(textlist->list, &ut_str_icd);

  textlist->view_size = textlist->height - 2;
  textlist->height = textlist->view_size;
  
  gtcaca_textlist_draw(textlist);

  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)textlist);

  return textlist;
}

void gtcaca_textlist_widget_set_view_size(gtcaca_textlist_widget_t *widget, unsigned int view_size)
{
  widget->view_size = view_size;
}

void gtcaca_textlist_widget_destroy(gtcaca_textlist_widget_t *widget)
{
  utarray_free(widget->list);
  free(widget);
}

int gtcaca_textlist_key_cb_register(gtcaca_textlist_widget_t *widget, gtcaca_textlist_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
}

void gtcaca_textlist_append(gtcaca_textlist_widget_t *textlist, char *item)
{
  size_t item_len;
  
  utarray_push_back(textlist->list, &item);

  item_len = strlen(item);
  if (item_len > textlist->width) { textlist->width = item_len; }
}

void gtcaca_textlist_selection_up(gtcaca_textlist_widget_t *textlist)
{
  if (textlist->selected_item <= 0) { return; }
  textlist->selected_item--;
}

void gtcaca_textlist_selection_down(gtcaca_textlist_widget_t *textlist)
{
  if (textlist->selected_item == utarray_len(textlist->list) - 1) { return; }
  textlist->selected_item++;
}

char *gtcaca_textlist_get_text_selected(gtcaca_textlist_widget_t *textlist)
{
  return utarray_eltptr(textlist->list, textlist->selected_item);
}

int gtcaca_textlist_can_draw(gtcaca_textlist_widget_t *textlist, unsigned int i)
{
  unsigned int views;
  unsigned int current_view;
  unsigned int selected_view;

  views = (unsigned int)round((float)utarray_len(textlist->list) / textlist->view_size);
  current_view = (unsigned int)round((float)(i+1) / textlist->view_size);
  selected_view = (unsigned int)round((float)(textlist->selected_item + 1) / textlist->view_size);

  if (current_view == selected_view) {
    return 1;
  }
  
  return 0;
}

void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist)
{
  unsigned int i, elem_view;
  char **p;

  /* We draw the background */
  if (textlist->parent) {
    if (textlist->parent->has_focus) {
      caca_set_color_ansi(gmo.cv, textlist->parent->color_focus_fg, textlist->parent->color_focus_bg);
    } else {
      caca_set_color_ansi(gmo.cv, textlist->parent->color_nonfocus_fg, textlist->parent->color_nonfocus_bg);
    }
  } else {
    caca_set_color_ansi(gmo.cv, gmo.theme.text.fg, gmo.theme.text.bg);
  }
  
  caca_fill_box(gmo.cv, textlist->x, textlist->y + 1, textlist->width, textlist->height, ' ');
  
  p = NULL;
  i = 0;
  elem_view = 0;
  while ( (p=(char**)utarray_next(textlist->list,p))) {
    if (gtcaca_textlist_can_draw(textlist, i)) {
      if (i == textlist->selected_item) {
	caca_set_color_ansi(gmo.cv, gmo.theme.textfocus.fg, gmo.theme.textfocus.bg);
      } else {
	caca_set_color_ansi(gmo.cv, gmo.theme.text.fg, gmo.theme.text.bg);
      }
      caca_printf(gmo.cv, textlist->x, textlist->y + elem_view, "%s", *p);
      elem_view++;
    }
    i++;
  }
  
  caca_refresh_display(gmo.dp);
}
