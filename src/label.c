#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/label.h>
#include <gtcaca/main.h>

gtcaca_label_widget_t *gtcaca_label_new(gtcaca_widget_t *parent, char *text, int x, int y)
{
  gtcaca_label_widget_t *label;

  label = malloc(sizeof(gtcaca_label_widget_t));
  if (!label) {
    fprintf(stderr, "Error: Cannot allocate new label\n");
    return NULL;
  }

  label->id = gtcaca_get_newid();
  label->has_focus = 0;
  label->is_visible = 1;
  label->label = text;
  label->x = parent ? parent->x + x : x;
  label->y = parent ? parent->y + y : y;
  label->width = strlen(text);
  label->height = 1;
  label->color_focus_fg = gmo.theme.textfocus.fg;
  label->color_focus_bg = gmo.theme.textfocus.bg;
  label->color_nonfocus_fg = gmo.theme.text.fg;
  label->color_nonfocus_bg = gmo.theme.text.bg;

  label->type = GTCACA_WIDGET_LABEL;
  label->parent = parent;
  label->children = NULL;

  gtcaca_label_draw(label);

  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)label);

  return label;
}


void gtcaca_label_draw(gtcaca_label_widget_t *label)
{
  gtcaca_widget_colorize_from_parent(GTCACA_WIDGET(label));
  caca_printf(gmo.cv, label->x, label->y, "%s", label->label);
}
