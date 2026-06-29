#ifndef _GTCACA_TREE_H_
#define _GTCACA_TREE_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Tree widget with a lazy model/view (cf. Qt's QAbstractItemModel + a
 *    QTreeView, and termui's Tree) ───────────────────────────────────────────
 *
 * The data lives behind a *model*: a small vtable the view calls on demand.
 * Nodes are opaque handles (void *) the model defines — an index, an id, or a
 * pointer. The view never materialises the whole dataset: it caches only the
 * nodes the user has expanded (a sparse structure with per-subtree visible-row
 * counts), and asks the model for a node's children/label only when a row is
 * actually on screen. So a tree of a billion nodes costs memory proportional to
 * what is expanded and visible, not to the dataset.
 *
 * A model must be O(1)-ish per call and must be stable (children don't shift)
 * while displayed. The invisible super-root is passed to the callbacks as NULL.
 */

typedef struct gtcaca_tree_model gtcaca_tree_model_t;
struct gtcaca_tree_model {
  /* number of children of `node` (NULL = the root level) */
  long  (*child_count)(gtcaca_tree_model_t *m, void *node);
  /* opaque handle of the `index`-th child of `node` */
  void *(*child)(gtcaca_tree_model_t *m, void *node, long index);
  /* write `node`'s display label into buf (NUL-terminated) */
  void  (*label)(gtcaca_tree_model_t *m, void *node, char *buf, int buflen);
  /* non-zero if `node` can be expanded (draws a ▸/▾ marker) */
  int   (*has_children)(gtcaca_tree_model_t *m, void *node);
  void  *userdata;
  /* Optional cell renderer (à la GTK CellRenderer / Qt item delegate). When
     non-NULL it is called instead of label() to paint a row's content area —
     the rectangle to the right of the indent + expand marker. The caller may
     draw text columns, or position and draw real widgets (e.g. a progressbar)
     within [x, x+width). The row background is already painted; `selected`
     indicates the highlighted row. */
  void  (*draw_row)(gtcaca_tree_model_t *m, void *node, int x, int y, int width, int selected);
};

typedef struct _gtcaca_tree_widget_t gtcaca_tree_widget_t;
struct _gtcaca_tree_widget_t {
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
  struct _gtcaca_tree_widget_t *prev;
  struct _gtcaca_tree_widget_t *next;

  gtcaca_tree_model_t *model;
  void  *root;          /* opaque open-state root (internal)        */
  long   top;           /* first visible row (flattened index)      */
  long   sel;           /* selected row (flattened index)           */
  char  *title;         /* optional title drawn in the top border   */
  uint8_t sel_bg;       /* selected-row highlight background        */
};

gtcaca_tree_widget_t *gtcaca_tree_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void  gtcaca_tree_set_model(gtcaca_tree_widget_t *t, gtcaca_tree_model_t *model);
void  gtcaca_tree_draw(gtcaca_tree_widget_t *t);
/* handle a key (Up/Down/PageUp/PageDown/Home/End move, Left/Right/Enter/Space
   collapse/expand). Returns 1 if the key was consumed. */
int   gtcaca_tree_key(gtcaca_tree_widget_t *t, int key, void *userdata);
long  gtcaca_tree_visible_count(gtcaca_tree_widget_t *t);   /* flattened rows now shown */
void *gtcaca_tree_selected_node(gtcaca_tree_widget_t *t);   /* model handle of selected row */
void  gtcaca_tree_set_title(gtcaca_tree_widget_t *t, const char *title);
void  gtcaca_tree_free(gtcaca_tree_widget_t *t);

#endif /* _GTCACA_TREE_H_ */
