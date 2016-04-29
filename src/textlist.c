#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/textlist.h>
#include <gtcaca/main.h>

gtcaca_textlist_widget_t *gtcaca_textlist_new(int x, int y)
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
  textlist->children = NULL;

  textlist->selected_item = 0;
  textlist->list_len = 0;
  
  gtcaca_textlist_draw(textlist);

  LL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)textlist);

  return textlist;
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
  if (textlist->selected_item == textlist->list_len) { return; }
  textlist->selected_item--;
}


void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist)
{
  unsigned int i;

  for (i = 0; i < textlist->list_len; i++) {
    if (i == textlist->selected_item) {
      caca_set_color_ansi(gmo.cv, gmo.theme.text.bg, gmo.theme.text.fg);
    } else {
      caca_set_color_ansi(gmo.cv, gmo.theme.text.fg, gmo.theme.text.bg);
    }
    caca_printf(gmo.cv, textlist->x, textlist->y + i, "%s", textlist->list[i]);
  }
  
  caca_refresh_display(gmo.dp);
}
