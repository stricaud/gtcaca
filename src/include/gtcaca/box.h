#ifndef _GTCACA_BOX_H_
#define _GTCACA_BOX_H_

#include <gtcaca/widget.h>
#include <gtcaca/window.h>

/*
 * Box layouts (à la Qt's QVBoxLayout / QHBoxLayout).
 *
 * A box does not draw anything itself: it is a positioning helper. You add
 * already-created widgets (and nested boxes) to it, then "apply" it over a
 * rectangle (or a window). Applying overwrites the x/y/width/height of every
 * managed widget so they line up in a vertical or horizontal stack.
 *
 * Typical use:
 *
 *   gtcaca_box_t *vb = gtcaca_vbox_new();
 *   gtcaca_box_add(vb, GTCACA_WIDGET(label));
 *   gtcaca_box_add(vb, GTCACA_WIDGET(entry));
 *   gtcaca_box_add(vb, GTCACA_WIDGET(button));
 *   gtcaca_box_apply_window(vb, win);   // lay out inside the window
 *
 * Boxes nest: build an hbox of buttons and gtcaca_box_add_box() it into a vbox.
 *
 * Sizing model (GTK/Qt-like):
 *   - By default a widget is placed at its natural size.
 *   - "expand" lets an item share the leftover space along the box's main axis
 *     (vertical for a vbox, horizontal for an hbox).
 *   - "fill" resizes the widget to fill the cell it was given.
 *   gtcaca_box_add_expand() turns both on, which is what you usually want for a
 *   widget that should soak up the remaining room (a text view, a list, ...).
 *
 * Free a box tree with gtcaca_box_free(); it never frees the widgets.
 */

typedef enum {
  GTCACA_BOX_VERTICAL,
  GTCACA_BOX_HORIZONTAL,
} gtcaca_box_orientation_t;

/* Cross-axis alignment of items that do not fill the cell. */
typedef enum {
  GTCACA_ALIGN_START,
  GTCACA_ALIGN_CENTER,
  GTCACA_ALIGN_END,
} gtcaca_align_t;

typedef struct _gtcaca_box_t gtcaca_box_t;

/* Constructors. spacing defaults to 1, margin to 1, alignment to START. */
gtcaca_box_t *gtcaca_vbox_new(void);
gtcaca_box_t *gtcaca_hbox_new(void);
gtcaca_box_t *gtcaca_box_new(gtcaca_box_orientation_t orientation);

/* Add an already-created widget at its natural size. */
void gtcaca_box_add(gtcaca_box_t *box, gtcaca_widget_t *widget);

/* Add a widget that expands to share leftover space and fills its cell. */
void gtcaca_box_add_expand(gtcaca_box_t *box, gtcaca_widget_t *widget);

/* Full control: expand = share leftover main-axis space; fill = resize widget
   to its cell. */
void gtcaca_box_add_full(gtcaca_box_t *box, gtcaca_widget_t *widget,
                         int expand, int fill);

/* Nest another box. It spans the full cross axis and takes its natural size
   along the main axis (add a stretch if you want it to absorb slack). */
void gtcaca_box_add_box(gtcaca_box_t *box, gtcaca_box_t *child);

/* Flexible spacer that pushes neighbours apart (Qt's addStretch). */
void gtcaca_box_add_stretch(gtcaca_box_t *box);

/* Fixed-size empty gap of `size` cells along the main axis. */
void gtcaca_box_add_spacing(gtcaca_box_t *box, int size);

/* Tunables. */
void gtcaca_box_set_spacing(gtcaca_box_t *box, int spacing);
void gtcaca_box_set_margin(gtcaca_box_t *box, int margin);
void gtcaca_box_set_align(gtcaca_box_t *box, gtcaca_align_t align);

/* Natural size the box would like (includes margins/spacing). */
int gtcaca_box_preferred_width(gtcaca_box_t *box);
int gtcaca_box_preferred_height(gtcaca_box_t *box);

/* Lay the box out over an explicit rectangle. */
void gtcaca_box_apply(gtcaca_box_t *box, int x, int y, int width, int height);

/* Lay the box out inside a window's interior (inside the border). */
void gtcaca_box_apply_window(gtcaca_box_t *box, gtcaca_window_widget_t *win);

/* Free the box and any nested boxes. Does not touch the widgets. */
void gtcaca_box_free(gtcaca_box_t *box);

#endif /* _GTCACA_BOX_H_ */
