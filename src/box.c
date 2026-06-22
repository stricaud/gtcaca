#include <stdio.h>
#include <stdlib.h>

#include <gtcaca/box.h>
#include <gtcaca/widget.h>
#include <gtcaca/window.h>

typedef enum {
  ITEM_WIDGET,
  ITEM_BOX,
  ITEM_STRETCH,
  ITEM_SPACING,
} item_type_t;

typedef struct {
  item_type_t       type;
  gtcaca_widget_t  *widget;   /* ITEM_WIDGET */
  gtcaca_box_t     *box;      /* ITEM_BOX    */
  int               size;     /* ITEM_SPACING fixed size */
  int               expand;   /* share leftover main-axis space */
  int               fill;     /* resize widget/box to fill its cell */
} box_item_t;

struct _gtcaca_box_t {
  gtcaca_box_orientation_t orientation;
  gtcaca_align_t           align;
  int                      spacing;
  int                      margin;
  box_item_t              *items;
  int                      n_items;
  int                      cap;
};

/* ---- construction ---- */

gtcaca_box_t *gtcaca_box_new(gtcaca_box_orientation_t orientation)
{
  gtcaca_box_t *box = malloc(sizeof(gtcaca_box_t));
  if (!box) {
    fprintf(stderr, "Error: Cannot allocate box layout\n");
    return NULL;
  }
  box->orientation = orientation;
  box->align       = GTCACA_ALIGN_START;
  box->spacing     = 1;
  box->margin      = 1;
  box->items       = NULL;
  box->n_items     = 0;
  box->cap         = 0;
  return box;
}

gtcaca_box_t *gtcaca_vbox_new(void) { return gtcaca_box_new(GTCACA_BOX_VERTICAL); }
gtcaca_box_t *gtcaca_hbox_new(void) { return gtcaca_box_new(GTCACA_BOX_HORIZONTAL); }

static box_item_t *box_push(gtcaca_box_t *box)
{
  if (box->n_items == box->cap) {
    int ncap = box->cap ? box->cap * 2 : 8;
    box_item_t *items = realloc(box->items, ncap * sizeof(box_item_t));
    if (!items) {
      fprintf(stderr, "Error: Cannot grow box layout\n");
      return NULL;
    }
    box->items = items;
    box->cap   = ncap;
  }
  box_item_t *it = &box->items[box->n_items++];
  it->type   = ITEM_WIDGET;
  it->widget = NULL;
  it->box    = NULL;
  it->size   = 0;
  it->expand = 0;
  it->fill   = 0;
  return it;
}

void gtcaca_box_add_full(gtcaca_box_t *box, gtcaca_widget_t *widget,
                         int expand, int fill)
{
  box_item_t *it;
  if (!box || !widget) return;
  it = box_push(box);
  if (!it) return;
  it->type   = ITEM_WIDGET;
  it->widget = widget;
  it->expand = expand;
  it->fill   = fill;
}

void gtcaca_box_add(gtcaca_box_t *box, gtcaca_widget_t *widget)
{
  gtcaca_box_add_full(box, widget, 0, 0);
}

void gtcaca_box_add_expand(gtcaca_box_t *box, gtcaca_widget_t *widget)
{
  gtcaca_box_add_full(box, widget, 1, 1);
}

void gtcaca_box_add_box(gtcaca_box_t *box, gtcaca_box_t *child)
{
  box_item_t *it;
  if (!box || !child) return;
  it = box_push(box);
  if (!it) return;
  it->type   = ITEM_BOX;
  it->box    = child;
  it->expand = 0;   /* natural size unless a stretch claims the slack */
  it->fill   = 1;   /* always spans the full cross axis */
}

void gtcaca_box_add_stretch(gtcaca_box_t *box)
{
  box_item_t *it;
  if (!box) return;
  it = box_push(box);
  if (!it) return;
  it->type   = ITEM_STRETCH;
  it->expand = 1;
}

void gtcaca_box_add_spacing(gtcaca_box_t *box, int size)
{
  box_item_t *it;
  if (!box) return;
  it = box_push(box);
  if (!it) return;
  it->type = ITEM_SPACING;
  it->size = size;
}

void gtcaca_box_set_spacing(gtcaca_box_t *box, int spacing) { if (box) box->spacing = spacing; }
void gtcaca_box_set_margin(gtcaca_box_t *box, int margin)   { if (box) box->margin  = margin; }
void gtcaca_box_set_align(gtcaca_box_t *box, gtcaca_align_t align) { if (box) box->align = align; }

/* ---- measurement ---- */

/* On-screen footprint of a single widget, including the drop shadow a button
   paints one row below and one column to the right of its box. */
static int widget_footprint_w(gtcaca_widget_t *w)
{
  return (w->type == GTCACA_WIDGET_BUTTON) ? w->width + 1 : w->width;
}
static int widget_footprint_h(gtcaca_widget_t *w)
{
  return (w->type == GTCACA_WIDGET_BUTTON) ? w->height + 1 : w->height;
}

static int item_pref_w(box_item_t *it, gtcaca_box_orientation_t parent);
static int item_pref_h(box_item_t *it, gtcaca_box_orientation_t parent);

int gtcaca_box_preferred_width(gtcaca_box_t *box)
{
  int i, total = 0, maxw = 0;
  if (!box) return 0;
  for (i = 0; i < box->n_items; i++) {
    int w = item_pref_w(&box->items[i], box->orientation);
    if (box->orientation == GTCACA_BOX_HORIZONTAL) total += w;
    else if (w > maxw) maxw = w;
  }
  if (box->orientation == GTCACA_BOX_HORIZONTAL) {
    if (box->n_items > 1) total += box->spacing * (box->n_items - 1);
    return total + 2 * box->margin;
  }
  return maxw + 2 * box->margin;
}

int gtcaca_box_preferred_height(gtcaca_box_t *box)
{
  int i, total = 0, maxh = 0;
  if (!box) return 0;
  for (i = 0; i < box->n_items; i++) {
    int h = item_pref_h(&box->items[i], box->orientation);
    if (box->orientation == GTCACA_BOX_VERTICAL) total += h;
    else if (h > maxh) maxh = h;
  }
  if (box->orientation == GTCACA_BOX_VERTICAL) {
    if (box->n_items > 1) total += box->spacing * (box->n_items - 1);
    return total + 2 * box->margin;
  }
  return maxh + 2 * box->margin;
}

static int item_pref_w(box_item_t *it, gtcaca_box_orientation_t parent)
{
  switch (it->type) {
  case ITEM_WIDGET:  return widget_footprint_w(it->widget);
  case ITEM_BOX:     return gtcaca_box_preferred_width(it->box);
  case ITEM_SPACING: return parent == GTCACA_BOX_HORIZONTAL ? it->size : 0;
  case ITEM_STRETCH: return 0;
  }
  return 0;
}

static int item_pref_h(box_item_t *it, gtcaca_box_orientation_t parent)
{
  switch (it->type) {
  case ITEM_WIDGET:  return widget_footprint_h(it->widget);
  case ITEM_BOX:     return gtcaca_box_preferred_height(it->box);
  case ITEM_SPACING: return parent == GTCACA_BOX_VERTICAL ? it->size : 0;
  case ITEM_STRETCH: return 0;
  }
  return 0;
}

/* main-axis natural size of an item, given the parent box orientation */
static int item_main(box_item_t *it, gtcaca_box_orientation_t o)
{
  return o == GTCACA_BOX_VERTICAL ? item_pref_h(it, o) : item_pref_w(it, o);
}

/* ---- layout ---- */

static void place_widget(gtcaca_widget_t *w, gtcaca_box_orientation_t o,
                         gtcaca_align_t align, int fill,
                         int main_pos, int main_size,
                         int cross_pos, int cross_avail)
{
  int nat_cross = (o == GTCACA_BOX_VERTICAL) ? widget_footprint_w(w)
                                             : widget_footprint_h(w);
  int off = 0;

  if (!fill) {
    if (align == GTCACA_ALIGN_CENTER)    off = (cross_avail - nat_cross) / 2;
    else if (align == GTCACA_ALIGN_END)  off = cross_avail - nat_cross;
    if (off < 0) off = 0;
  }

  if (o == GTCACA_BOX_VERTICAL) {
    w->y = main_pos;
    w->x = cross_pos + off;
    if (fill) {
      if (cross_avail > 0) w->width  = cross_avail;
      if (main_size  > 0)  w->height = main_size;
    }
  } else {
    w->x = main_pos;
    w->y = cross_pos + off;
    if (fill) {
      if (cross_avail > 0) w->height = cross_avail;
      if (main_size  > 0)  w->width  = main_size;
    }
  }
}

void gtcaca_box_apply(gtcaca_box_t *box, int x, int y, int width, int height)
{
  int ix, iy, iw, ih;
  int main_avail, cross_avail, cross_pos;
  int fixed = 0, grow_units = 0, leftover, i;
  int pos;

  if (!box || box->n_items == 0) return;

  ix = x + box->margin;
  iy = y + box->margin;
  iw = width  - 2 * box->margin;
  ih = height - 2 * box->margin;
  if (iw < 0) iw = 0;
  if (ih < 0) ih = 0;

  if (box->orientation == GTCACA_BOX_VERTICAL) {
    main_avail  = ih;  cross_avail = iw;  cross_pos = ix;
  } else {
    main_avail  = iw;  cross_avail = ih;  cross_pos = iy;
  }

  /* Main-axis demand. Non-expanding items reserve their natural size;
     expanding items reserve nothing and instead share whatever is left. */
  for (i = 0; i < box->n_items; i++) {
    box_item_t *it = &box->items[i];
    if (it->expand) grow_units++;
    else            fixed += item_main(it, box->orientation);
  }
  if (box->n_items > 1) fixed += box->spacing * (box->n_items - 1);

  leftover = main_avail - fixed;
  if (leftover < 0) leftover = 0;

  pos = (box->orientation == GTCACA_BOX_VERTICAL) ? iy : ix;

  for (i = 0; i < box->n_items; i++) {
    box_item_t *it = &box->items[i];
    int natural = it->expand ? 0 : item_main(it, box->orientation);
    int grow = 0;
    int cell;

    /* Hand each expanding item an even share of the remaining leftover,
       giving the extra remainder cells to the earliest expanders. */
    if (it->expand && grow_units > 0) {
      grow = leftover / grow_units;
      if (leftover % grow_units != 0) grow++;
      leftover    -= grow;
      grow_units  -= 1;
    }

    cell = natural + grow;

    switch (it->type) {
    case ITEM_WIDGET:
      place_widget(it->widget, box->orientation, box->align, it->fill,
                   pos, cell, cross_pos, cross_avail);
      break;
    case ITEM_BOX:
      if (box->orientation == GTCACA_BOX_VERTICAL)
        gtcaca_box_apply(it->box, cross_pos, pos, cross_avail, cell);
      else
        gtcaca_box_apply(it->box, pos, cross_pos, cell, cross_avail);
      break;
    case ITEM_STRETCH:
    case ITEM_SPACING:
      break;
    }

    pos += cell;
    if (i < box->n_items - 1) pos += box->spacing;
  }
}

void gtcaca_box_apply_window(gtcaca_box_t *box, gtcaca_window_widget_t *win)
{
  if (!box || !win) return;
  /* Interior is inside the window border (1-cell box on each side). */
  gtcaca_box_apply(box, win->x + 1, win->y + 1, win->width - 2, win->height - 2);
}

void gtcaca_box_free(gtcaca_box_t *box)
{
  int i;
  if (!box) return;
  for (i = 0; i < box->n_items; i++) {
    if (box->items[i].type == ITEM_BOX)
      gtcaca_box_free(box->items[i].box);
  }
  free(box->items);
  free(box);
}
