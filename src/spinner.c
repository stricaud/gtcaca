#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/spinner.h>
#include <gtcaca/main.h>

gtcaca_spinner_widget_t *gtcaca_spinner_new(gtcaca_widget_t *parent, int x, int y)
{
  gtcaca_spinner_widget_t *sp;

  sp = malloc(sizeof(gtcaca_spinner_widget_t));
  if (!sp) {
    fprintf(stderr, "Error: Cannot allocate spinner\n");
    return NULL;
  }

  sp->id = gtcaca_get_newid();
  sp->has_focus = 0;
  sp->is_visible = 1;
  sp->type = GTCACA_WIDGET_SPINNER;
  sp->parent = parent;
  sp->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(sp), x, y);
  sp->width  = 1;
  sp->height = 1;

  sp->color_focus_fg   = gmo.theme.text.fg;
  sp->color_focus_bg   = gmo.theme.text.bg;
  sp->color_nonfocus_fg = gmo.theme.text.fg;
  sp->color_nonfocus_bg = gmo.theme.text.bg;

  sp->frame   = 0;
  sp->spinning = 0;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(sp));

  return sp;
}

void gtcaca_spinner_draw(gtcaca_spinner_widget_t *sp)
{
  static const char frames[] = "|/-\\";
  char c;

  gtcaca_widget_colorize(GTCACA_WIDGET(sp));

  if (sp->spinning) {
    c = frames[sp->frame % 4];
    sp->frame++;
  } else {
    c = '.';
  }

  caca_put_char(gmo.cv, sp->x, sp->y, c);
}

void gtcaca_spinner_set_spinning(gtcaca_spinner_widget_t *sp, int spinning)
{
  sp->spinning = spinning ? 1 : 0;
}

void gtcaca_spinner_step(gtcaca_spinner_widget_t *sp)
{
  sp->frame++;
}
