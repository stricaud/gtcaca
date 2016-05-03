#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/textlist.h>
#include <gtcaca/main.h>


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

  textlist->has_focus = 1;
  textlist->is_visible = 1;
  textlist->x = x;
  textlist->y = y;
  textlist->width = -1;
  textlist->height = -1;
  textlist->type = GTCACA_WIDGET_TEXTLIST;
  textlist->parent = parent;
  textlist->children = NULL;
  /* if (parent) { */
  /*   CDL_APPEND(parent->children, (gtcaca_widget_t *)textlist); */
  /* } */

  textlist->selected_item = 0;
  textlist->list_len = 0;
  textlist->private_key_cb = _gtcaca_textlist_private_key_press;
  textlist->key_cb = NULL;
  
  gtcaca_textlist_draw(textlist);

  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)textlist);

  return textlist;
}

int gtcaca_textlist_key_cb_register(gtcaca_textlist_widget_t *widget, gtcaca_textlist_key_cb_t key_cb)
{
  widget->key_cb = key_cb;
}

void gtcaca_textlist_append(gtcaca_textlist_widget_t *textlist, char *item)
{
  if (textlist->list_len == GTCACA_TEXTLIST_MAX) {
    fprintf(stderr, "Error, cannot add new item to list: maximum reached\n");
    return;
  }
  textlist->list[textlist->list_len] = item;
  textlist->list_len++;
}

void gtcaca_textlist_selection_up(gtcaca_textlist_widget_t *textlist)
{
  if (textlist->selected_item <= 0) { return; }
  textlist->selected_item--;
}

void gtcaca_textlist_selection_down(gtcaca_textlist_widget_t *textlist)
{
  if (textlist->selected_item == textlist->list_len - 1) { return; }
  textlist->selected_item++;
}

char *gtcaca_textlist_get_selected(gtcaca_textlist_widget_t *textlist)
{
  return textlist->list[textlist->selected_item];
}

void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist)
{
  unsigned int i;

  /* caca_printf(gmo.cv, textlist->x, textlist->y + 20, "selected: %d", textlist->selected_item); */

  for (i = 0; i < textlist->list_len; i++) {
    if (i == textlist->selected_item) {
      caca_set_color_ansi(gmo.cv, gmo.theme.textfocus.fg, gmo.theme.textfocus.bg);
    } else {
      caca_set_color_ansi(gmo.cv, gmo.theme.text.fg, gmo.theme.text.bg);
    }
    caca_printf(gmo.cv, textlist->x, textlist->y + i, "%s", textlist->list[i]);
  }
  
  caca_refresh_display(gmo.dp);
}
