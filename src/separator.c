#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/separator.h>
#include <gtcaca/main.h>

gtcaca_separator_widget_t *gtcaca_separator_new(gtcaca_widget_t *parent, int x, int y, int length, int vertical)
{
  gtcaca_separator_widget_t *sep;

  sep = malloc(sizeof(gtcaca_separator_widget_t));
  if (!sep) {
    fprintf(stderr, "Error: Cannot allocate separator\n");
    return NULL;
  }

  sep->id = gtcaca_get_newid();
  sep->has_focus = 0;
  sep->is_visible = 1;
  sep->type = GTCACA_WIDGET_SEPARATOR;
  sep->parent = parent;
  sep->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(sep), x, y);
  sep->width  = vertical ? 1 : length;
  sep->height = vertical ? length : 1;

  sep->color_focus_fg   = CACA_DEFAULT;
  sep->color_focus_bg   = CACA_DEFAULT;
  sep->color_nonfocus_fg = CACA_DEFAULT;
  sep->color_nonfocus_bg = CACA_DEFAULT;

  sep->vertical = vertical;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(sep));

  return sep;
}

void gtcaca_separator_draw(gtcaca_separator_widget_t *sep)
{
  int i;

  caca_set_color_ansi(gmo.cv, CACA_DEFAULT, CACA_DEFAULT);

  if (sep->vertical) {
    for (i = 0; i < sep->height; i++)
      caca_put_char(gmo.cv, sep->x, sep->y + i, '|');
  } else {
    for (i = 0; i < sep->width; i++)
      caca_put_char(gmo.cv, sep->x + i, sep->y, '-');
  }
}
