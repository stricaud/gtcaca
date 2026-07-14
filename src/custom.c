/*
 * custom.c — the "bring your own drawing" widget.
 *
 * WHY THIS EXISTS
 * ---------------
 * Every other widget in gtcaca is *closed*: the button knows how to draw a
 * button, the table knows how to draw a table, and the only way to change what
 * appears on screen is to add a new widget type here in C (a new struct, a new
 * _draw function, a new case in the redraw and key-dispatch switches in
 * main.c) and then expose it through the bindings. That is fine for reusable
 * controls, but it is a wall for an application that needs a *bespoke* view —
 * something the toolkit was never going to ship, like a hex-dump grid with a
 * blinking caret, a byte-selection highlight, and per-byte colouring driven by
 * a parsed file format.
 *
 * The custom widget removes that wall. It is a widget that owns a rectangle of
 * the screen but has no opinion about what goes in it: it forwards drawing to a
 * caller-supplied draw callback and key presses to a caller-supplied key
 * callback. The application (in C, or in Python via the bindings) paints the
 * rectangle itself using the low-level caca canvas primitives
 * (gtcaca put_str / put_char / set_color / fill_box …), and reads the keyboard
 * however it likes.
 *
 * In other words: instead of "subclass the toolkit in C to draw something new",
 * you get "here is a focusable rectangle and the two callbacks the event loop
 * will call — do whatever you want." That is exactly what a hex editor's main
 * pane, its data inspector, and its template tree are built on, entirely from
 * Python, with no further C changes.
 *
 * The widget deliberately does NOT clear or border its rectangle before the
 * draw callback runs — the callback has full control of every cell, including
 * the background, so it can render frames without flicker.
 */

#include <stdio.h>
#include <stdlib.h>

#include <caca.h>

#include <gtcaca/custom.h>
#include <gtcaca/main.h>

gtcaca_custom_widget_t *gtcaca_custom_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_custom_widget_t *c = malloc(sizeof *c);
  if (!c) { fprintf(stderr, "Error: cannot allocate custom widget\n"); return NULL; }

  c->id = gtcaca_get_newid();
  c->has_focus = 0;
  c->is_visible = 1;
  c->type = GTCACA_WIDGET_CUSTOM;
  c->parent = parent;
  c->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(c), x, y);
  c->width = width;
  c->height = height;

  c->color_focus_fg = gmo.theme.text.fg;
  c->color_focus_bg = gmo.theme.text.bg;
  c->color_nonfocus_fg = gmo.theme.text.fg;
  c->color_nonfocus_bg = gmo.theme.text.bg;

  c->draw_cb = NULL;
  c->draw_cb_userdata = NULL;
  c->key_cb = NULL;
  c->key_cb_userdata = NULL;
  c->focusable = 1;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(c));
  return c;
}

void gtcaca_custom_free(gtcaca_custom_widget_t *widget)
{
  if (!widget) return;
  CDL_DELETE(gmo.widgets_list, GTCACA_WIDGET(widget));
  free(widget);
}

void gtcaca_custom_set_draw_cb(gtcaca_custom_widget_t *widget, gtcaca_custom_draw_cb_t cb, void *userdata)
{
  if (!widget) return;
  widget->draw_cb = cb;
  widget->draw_cb_userdata = userdata;
}

void gtcaca_custom_set_key_cb(gtcaca_custom_widget_t *widget, gtcaca_custom_key_cb_t cb, void *userdata)
{
  if (!widget) return;
  widget->key_cb = cb;
  widget->key_cb_userdata = userdata;
}

void gtcaca_custom_set_focusable(gtcaca_custom_widget_t *widget, int focusable)
{
  if (!widget) return;
  widget->focusable = focusable;
}

void gtcaca_custom_draw(gtcaca_custom_widget_t *widget)
{
  if (!widget || !widget->is_visible) return;
  if (widget->draw_cb)
    widget->draw_cb(widget, widget->draw_cb_userdata);
}
