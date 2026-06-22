# Layout: vbox / hbox

GTCaca lets you position widgets two ways:

1. **By hand** — pass explicit `x, y` (and sizes) to each widget constructor, as
   in [`tests/simple.c`](../tests/simple.c) and `tests/demo_login.c`. This is
   fully supported and never goes away.
2. **With a box layout** — add widgets to a vertical or horizontal box and let
   it compute the coordinates, the way Qt's `QVBoxLayout` / `QHBoxLayout` work.
   See [`tests/simple_layout.c`](../tests/simple_layout.c).

This document explains the second one.

## Mental model

A box is **not a widget**. It draws nothing and is never added to the global
widget list. It is a throwaway helper that, when *applied*, overwrites the
`x / y / width / height` of the widgets you gave it so they line up.

Because `gtcaca_redraw()` paints every widget from those fields on each frame,
all the layout has to do is set them once:

```
create widgets  ──▶  add them to a box  ──▶  gtcaca_box_apply*  ──▶  free the box
                                              (sets x/y/w/h)        (widgets stay)
```

The widgets you add must already exist. The `x, y` you pass to their
constructors don't matter — the layout overwrites them — so just pass `0, 0`.

## A first example

```c
#include <gtcaca/box.h>

gtcaca_window_widget_t *win = gtcaca_window_new(NULL, "Sign in", 4, 2, 40, 12);

gtcaca_label_widget_t  *lbl = gtcaca_label_new (GTCACA_WIDGET(win), "Name:", 0, 0);
gtcaca_entry_widget_t  *ent = gtcaca_entry_new (GTCACA_WIDGET(win), 0, 0, 10);
gtcaca_button_widget_t *btn = gtcaca_button_new(GTCACA_WIDGET(win), "[ OK ]", 0, 0);

gtcaca_box_t *vb = gtcaca_vbox_new();
gtcaca_box_add(vb, GTCACA_WIDGET(lbl));
gtcaca_box_add(vb, GTCACA_WIDGET(ent));
gtcaca_box_add(vb, GTCACA_WIDGET(btn));

gtcaca_box_apply_window(vb, win);   /* compute positions inside the window */
gtcaca_box_free(vb);                /* the box is no longer needed */
```

That stacks the three widgets vertically, top to bottom, inside the window's
border.

## Main axis and cross axis

Every box has an orientation:

| Box                  | Main axis (items flow along this) | Cross axis |
| -------------------- | --------------------------------- | ---------- |
| `gtcaca_vbox_new()`  | vertical (top → bottom)           | horizontal |
| `gtcaca_hbox_new()`  | horizontal (left → right)         | vertical   |

Items are placed one after another along the **main axis**. The **cross axis**
is the perpendicular direction; that's where alignment and cross-filling apply.

## Sizing: expand and fill

Each item carries two flags, following the GTK/Qt model:

- **expand** — the item does *not* reserve its natural size; instead it takes
  an equal share of the space left over by the non-expanding items along the
  main axis. If several items expand, that leftover is split between them
  evenly. (This is what lets an expanding list or text view simply fill "the
  rest" of a window.)
- **fill** — the *widget* is resized to fill the cell it was given (both axes).
  Without fill, the widget keeps its natural size and is aligned in the cell.

The helpers map onto those flags:

| Call                                      | expand | fill | Use it for…                          |
| ----------------------------------------- | :----: | :--: | ------------------------------------ |
| `gtcaca_box_add(box, w)`                  |   no   |  no  | a widget at its natural size         |
| `gtcaca_box_add_expand(box, w)`           |  yes   | yes  | the widget that soaks up the room    |
| `gtcaca_box_add_full(box, w, exp, fill)`  | choose | choose | full manual control               |

Example — a "label + entry" row where the entry takes all the remaining width:

```c
gtcaca_box_t *row = gtcaca_hbox_new();
gtcaca_box_add(row, GTCACA_WIDGET(label));         /* fixed natural width */
gtcaca_box_add_expand(row, GTCACA_WIDGET(entry));  /* stretches to fill   */
```

## Spacers: stretch and spacing

Two empty items help with positioning:

- `gtcaca_box_add_stretch(box)` — an infinitely flexible spacer (Qt's
  `addStretch`). It claims an equal share of the leftover space, so it pushes
  its neighbours around:

  ```c
  /* Right-align two buttons in a horizontal bar */
  gtcaca_box_add_stretch(bar);              /*  <-- absorbs the slack  */
  gtcaca_box_add(bar, GTCACA_WIDGET(clear));
  gtcaca_box_add(bar, GTCACA_WIDGET(login));

  /* Center a label vertically: stretch above and below */
  gtcaca_box_add_stretch(col);
  gtcaca_box_add(col, GTCACA_WIDGET(label));
  gtcaca_box_add_stretch(col);
  ```

- `gtcaca_box_add_spacing(box, n)` — a fixed empty gap of `n` cells along the
  main axis, on top of the regular inter-item spacing.

## Spacing, margin, alignment

These tune a box (sensible defaults shown):

| Setter                                       | Default        | Effect                                            |
| -------------------------------------------- | -------------- | ------------------------------------------------- |
| `gtcaca_box_set_spacing(box, n)`             | `1`            | blank cells between adjacent items                |
| `gtcaca_box_set_margin(box, n)`              | `1`            | blank border kept inside the box on every side    |
| `gtcaca_box_set_align(box, align)`           | `ALIGN_START`  | cross-axis placement of non-filling items         |

Alignment values: `GTCACA_ALIGN_START`, `GTCACA_ALIGN_CENTER`,
`GTCACA_ALIGN_END`.

## Nesting boxes

A box can contain other boxes via `gtcaca_box_add_box()`. This is how you build
real forms: a vertical box of rows, each row a horizontal box.

A nested box always spans the full cross axis of its parent and takes its
**natural** main-axis size — it does **not** expand on its own. If you want a
nested box to absorb slack, put a `stretch` next to it (or inside it). This
matches Qt's `addLayout`, whose default stretch factor is 0.

```c
/* Form: rows stacked vertically, buttons pinned to the bottom */
gtcaca_box_t *form = gtcaca_vbox_new();

gtcaca_box_t *row1 = gtcaca_hbox_new();
gtcaca_box_set_margin(row1, 0);
gtcaca_box_add(row1, GTCACA_WIDGET(user_label));
gtcaca_box_add_expand(row1, GTCACA_WIDGET(user_entry));
gtcaca_box_add_box(form, row1);

gtcaca_box_add_stretch(form);          /* eats vertical slack */

gtcaca_box_t *buttons = gtcaca_hbox_new();
gtcaca_box_set_margin(buttons, 0);
gtcaca_box_add_stretch(buttons);       /* right-align the buttons */
gtcaca_box_add(buttons, GTCACA_WIDGET(cancel));
gtcaca_box_add(buttons, GTCACA_WIDGET(ok));
gtcaca_box_add_box(form, buttons);

gtcaca_box_apply_window(form, win);
gtcaca_box_free(form);                 /* frees form AND its nested rows */
```

`gtcaca_box_free()` walks the tree and frees nested boxes too; it never frees
the widgets.

## Applying a layout

| Function                                         | Lays out over…                                    |
| ------------------------------------------------ | ------------------------------------------------- |
| `gtcaca_box_apply(box, x, y, w, h)`              | an explicit canvas rectangle                      |
| `gtcaca_box_apply_window(box, win)`              | a window's interior, just inside its border       |

Apply once after the box is fully built. If you later change widget content or
resize the window, call apply again to re-flow.

You can ask a box for its natural size before applying, which is handy for
sizing a window to its content:

```c
int w = gtcaca_box_preferred_width(form);
int h = gtcaca_box_preferred_height(form);
```

## Notes and gotchas

- **Buttons reserve a shadow.** A button paints a drop shadow one row below and
  one column to its right. The layout counts that extra row/column in the
  button's footprint, so stacked buttons never clip each other.
- **Pass `0, 0` at construction.** Constructor coordinates are overwritten by
  the layout; an entry still needs a sensible initial `width`, but it will be
  stretched if you add it with `expand`/`fill`.
- **Boxes are cheap and disposable.** Build, apply, free. They hold no
  reference the rest of the toolkit depends on.
- **Colors and focus are unchanged.** Layout only moves and resizes widgets;
  parent/child relationships, theming and Tab focus order work exactly as with
  hand-placed widgets.

## See also

- [`tests/simple_layout.c`](../tests/simple_layout.c) — a runnable demo with a
  vbox form and an hbox alignment showcase.
- [`src/include/gtcaca/box.h`](../src/include/gtcaca/box.h) — the full API with
  per-function comments.
