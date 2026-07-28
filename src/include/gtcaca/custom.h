#ifndef _GTCACA_CUSTOM_H_
#define _GTCACA_CUSTOM_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/*
 * The "custom" widget is a canvas escape hatch: it owns a rectangle of the
 * screen and delegates both drawing and key handling to caller-supplied
 * callbacks.  It lets bindings (and C callers) render arbitrary content with
 * the low-level caca canvas primitives without having to add a bespoke widget
 * type to the toolkit -- ideal for a hex grid, a template tree, or any bespoke
 * view.
 */

typedef struct _gtcaca_custom_widget_t gtcaca_custom_widget_t;

/* What a mouse callback is being told about. MOTION only arrives while a button
   is held (the terminal reports button-event tracking), i.e. during a drag, and
   only between the PRESS that started it and its RELEASE. */
typedef enum {
  GTCACA_MOUSE_PRESS = 0,
  GTCACA_MOUSE_MOTION,
  GTCACA_MOUSE_RELEASE,
  GTCACA_MOUSE_WHEEL     /* button 4 = wheel down, 5 = wheel up */
} gtcaca_mouse_event_t;

typedef void (*gtcaca_custom_draw_cb_t)(gtcaca_custom_widget_t *widget, void *userdata);
typedef int  (*gtcaca_custom_key_cb_t)(gtcaca_custom_widget_t *widget, int key, void *userdata);
/* x/y are canvas coordinates (subtract widget->x / widget->y for widget-local
   ones); button is 1 left, 2 middle, 3 right, and 4/5 for the wheel. RELEASE
   reports the last known position and button 0. Return non-zero if handled. */
typedef int  (*gtcaca_custom_mouse_cb_t)(gtcaca_custom_widget_t *widget, gtcaca_mouse_event_t event,
                                         int x, int y, int button, void *userdata);

struct _gtcaca_custom_widget_t {

  /* Properties shared accross all widgets */
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  uint8_t color_focus_fg;
  uint8_t color_focus_bg;
  uint8_t color_nonfocus_fg;
  uint8_t color_nonfocus_bg;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children;
  struct _gtcaca_custom_widget_t *prev;
  struct _gtcaca_custom_widget_t *next;

  /* Now starts custom widget properties */
  gtcaca_custom_draw_cb_t draw_cb;
  void *draw_cb_userdata;
  gtcaca_custom_key_cb_t key_cb;
  void *key_cb_userdata;
  gtcaca_custom_mouse_cb_t mouse_cb;
  void *mouse_cb_userdata;
  int focusable;   /* participates in tab focus when non-zero (default 1) */
};

gtcaca_custom_widget_t *gtcaca_custom_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_custom_free(gtcaca_custom_widget_t *widget);
void gtcaca_custom_draw(gtcaca_custom_widget_t *widget);
void gtcaca_custom_set_draw_cb(gtcaca_custom_widget_t *widget, gtcaca_custom_draw_cb_t cb, void *userdata);
void gtcaca_custom_set_key_cb(gtcaca_custom_widget_t *widget, gtcaca_custom_key_cb_t cb, void *userdata);
/* Register a mouse callback: the widget then receives clicks that land inside
   it, every motion event until the button is released, and the wheel while it
   is focused. Without one the widget is only focused on a click, as before. */
void gtcaca_custom_set_mouse_cb(gtcaca_custom_widget_t *widget, gtcaca_custom_mouse_cb_t cb, void *userdata);
void gtcaca_custom_set_focusable(gtcaca_custom_widget_t *widget, int focusable);

#endif // _GTCACA_CUSTOM_H_
