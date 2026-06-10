#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/expander.h>
#include <gtcaca/main.h>

gtcaca_expander_widget_t *gtcaca_expander_new(gtcaca_widget_t *parent, const char *label, int x, int y, int width)
{
  gtcaca_expander_widget_t *exp;

  exp = malloc(sizeof(gtcaca_expander_widget_t));
  if (!exp) {
    fprintf(stderr, "Error: Cannot allocate expander\n");
    return NULL;
  }

  exp->id = gtcaca_get_newid();
  exp->has_focus = 0;
  exp->is_visible = 1;
  exp->type = GTCACA_WIDGET_EXPANDER;
  exp->parent = parent;
  exp->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(exp), x, y);
  exp->width  = width;
  exp->height = 1;

  exp->color_focus_fg   = gmo.theme.textfocus.fg;
  exp->color_focus_bg   = gmo.theme.textfocus.bg;
  exp->color_nonfocus_fg = gmo.theme.text.fg;
  exp->color_nonfocus_bg = gmo.theme.text.bg;

  exp->label     = label ? strdup(label) : NULL;
  exp->expanded  = 0;
  exp->n_managed = 0;
  exp->cb        = NULL;
  exp->cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(exp));

  return exp;
}

void gtcaca_expander_draw(gtcaca_expander_widget_t *exp)
{
  gtcaca_widget_colorize(GTCACA_WIDGET(exp));

  caca_printf(gmo.cv, exp->x, exp->y, "%s %s",
    exp->expanded ? "v" : ">",
    exp->label ? exp->label : "");
}

void gtcaca_expander_handle_key(gtcaca_expander_widget_t *exp, int key)
{
  if (key == ' ' || key == CACA_KEY_RETURN) {
    gtcaca_expander_set_expanded(exp, !exp->expanded);
    if (exp->cb)
      exp->cb(exp, exp->expanded, exp->cb_userdata);
  }
}

void gtcaca_expander_add_managed(gtcaca_expander_widget_t *exp, gtcaca_widget_t *widget)
{
  if (exp->n_managed >= 32) return;
  exp->managed[exp->n_managed++] = widget;
  widget->is_visible = exp->expanded;
}

void gtcaca_expander_set_expanded(gtcaca_expander_widget_t *exp, int expanded)
{
  int i;

  exp->expanded = expanded ? 1 : 0;
  for (i = 0; i < exp->n_managed; i++) {
    exp->managed[i]->is_visible = exp->expanded;
  }
}

int gtcaca_expander_get_expanded(gtcaca_expander_widget_t *exp)
{
  return exp->expanded;
}

void gtcaca_expander_cb_register(gtcaca_expander_widget_t *exp, gtcaca_expander_cb_t cb, void *userdata)
{
  exp->cb = cb;
  exp->cb_userdata = userdata;
}
