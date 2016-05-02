#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/label.h>
#include <gtcaca/main.h>

gtcaca_label_widget_t *gtcaca_label_new(gtcaca_widget_t *parent, char *text, int x, int y)
{
  gtcaca_label_widget_t *label;
  int i;

  label = malloc(sizeof(gtcaca_label_widget_t));
  if (!label) {
    fprintf(stderr, "Error: Cannot allocate new label\n");
    return NULL;
  }

  label->has_focus = 1;
  label->is_visible = 1;
  label->label = text;
  label->x = x;
  label->y = y;
  label->width = strlen(text);
  label->height = 1;
  label->type = GTCACA_WIDGET_LABEL;
  label->parent = parent;
  label->children = NULL;
  /* if (parent) { */
  /*   LL_APPEND(parent->children, (gtcaca_widget_t *)label); */
  /* } */

  gtcaca_label_draw(label);

  LL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)label);

  return label;
}


void gtcaca_label_draw(gtcaca_label_widget_t *label)
{
  int i;

  /* Can never have focus*/
  caca_set_color_ansi(gmo.cv, gmo.theme.text.fg, gmo.theme.text.bg);
  caca_printf(gmo.cv, label->x + 1, label->y, "%s", label->label);

  caca_refresh_display(gmo.dp);
}
