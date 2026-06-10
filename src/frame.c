#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/frame.h>
#include <gtcaca/main.h>

gtcaca_frame_widget_t *gtcaca_frame_new(gtcaca_widget_t *parent, const char *label, int x, int y, int width, int height)
{
  gtcaca_frame_widget_t *frame;

  frame = malloc(sizeof(gtcaca_frame_widget_t));
  if (!frame) {
    fprintf(stderr, "Error: Cannot allocate frame\n");
    return NULL;
  }

  frame->id = gtcaca_get_newid();
  frame->has_focus = 0;
  frame->is_visible = 1;
  frame->type = GTCACA_WIDGET_FRAME;
  frame->parent = parent;
  frame->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(frame), x, y);
  frame->width  = width;
  frame->height = height;

  frame->color_focus_fg   = gmo.theme.windowfocus.fg;
  frame->color_focus_bg   = gmo.theme.windowfocus.bg;
  frame->color_nonfocus_fg = gmo.theme.window.fg;
  frame->color_nonfocus_bg = gmo.theme.window.bg;

  frame->label = label ? strdup(label) : NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(frame));

  return frame;
}

void gtcaca_frame_draw(gtcaca_frame_widget_t *frame)
{
  gtcaca_widget_colorize(GTCACA_WIDGET(frame));

  caca_draw_cp437_box(gmo.cv, frame->x, frame->y, frame->width, frame->height);

  if (frame->label) {
    caca_printf(gmo.cv, frame->x + 2, frame->y, " %s ", frame->label);
  }
}
