#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/statusbar.h>
#include <gtcaca/main.h>

gtcaca_statusbar_widget_t *gtcaca_statusbar_new(const char *text)
{
  gtcaca_statusbar_widget_t *sb;

  sb = malloc(sizeof(gtcaca_statusbar_widget_t));
  if (!sb) {
    fprintf(stderr, "Error: Cannot allocate statusbar\n");
    return NULL;
  }

  sb->id = gtcaca_get_newid();
  sb->has_focus = 0;
  sb->is_visible = 1;
  sb->type = GTCACA_WIDGET_STATUSBAR;
  sb->parent = NULL;
  sb->children = NULL;

  sb->x = 0;
  sb->y = caca_get_canvas_height(gmo.cv) - 1;
  sb->width = caca_get_canvas_width(gmo.cv);
  sb->height = 1;

  sb->color_focus_fg = gmo.theme.statusbar.fg;
  sb->color_focus_bg = gmo.theme.statusbar.bg;
  sb->color_nonfocus_fg = gmo.theme.statusbar.fg;
  sb->color_nonfocus_bg = gmo.theme.statusbar.bg;

  sb->text = text ? strdup(text) : NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(sb));

  return sb;
}

void gtcaca_statusbar_draw(gtcaca_statusbar_widget_t *sb)
{
  int canvas_w = caca_get_canvas_width(gmo.cv);
  int canvas_h = caca_get_canvas_height(gmo.cv);

  /* Always pin to bottom of canvas */
  sb->y = canvas_h - 1;
  sb->width = canvas_w;

  caca_set_color_ansi(gmo.cv, gmo.theme.statusbar.fg, gmo.theme.statusbar.bg);
  caca_fill_box(gmo.cv, sb->x, sb->y, sb->width, 1, ' ');

  if (sb->text) {
    caca_printf(gmo.cv, sb->x + 1, sb->y, "%s", sb->text);
  }
}

void gtcaca_statusbar_set_text(gtcaca_statusbar_widget_t *sb, const char *text)
{
  free(sb->text);
  sb->text = text ? strdup(text) : NULL;
}
