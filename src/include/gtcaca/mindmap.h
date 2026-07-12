#ifndef _GTCACA_MINDMAP_H_
#define _GTCACA_MINDMAP_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Mind-map widget (FreeMind-style) ────────────────────────────────────────
 *
 * A tree of text nodes laid out left-to-right: the root sits on the left and
 * children branch to the right, joined by box-drawing connectors. Any node may
 * be folded (its subtree hidden). The widget owns the node tree; callers build
 * it with add_child / add_sibling and walk it through the public node fields
 * (so an app can serialise it, e.g. to a FreeMind .mm file).
 *
 * Keys (gtcaca_mindmap_key): Up/Down move between visible nodes, Left folds or
 * climbs to the parent, Right unfolds or descends to the first child. */

typedef struct gtcaca_mm_node {
  char    *text;
  uint8_t  colour;
  int      folded;
  void    *userdata;         /* caller-owned association (e.g. a flow handle) */
  struct gtcaca_mm_node  *parent;
  struct gtcaca_mm_node **kids;
  int      nkids, capkids;
  /* layout scratch, recomputed on every draw */
  int      row, depth, px;
} gtcaca_mm_node_t;

typedef struct _gtcaca_mindmap_widget_t gtcaca_mindmap_widget_t;
struct _gtcaca_mindmap_widget_t {
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
  struct _gtcaca_mindmap_widget_t *prev;
  struct _gtcaca_mindmap_widget_t *next;

  gtcaca_mm_node_t *root;
  gtcaca_mm_node_t *sel;      /* current node                 */
  int   top;                  /* first visible layout row     */
  int   xoff;                 /* horizontal scroll (columns)  */
  char *title;
  uint8_t sel_fg, sel_bg;     /* highlight of the current node */
};

gtcaca_mindmap_widget_t *gtcaca_mindmap_new(gtcaca_widget_t *parent, int x, int y, int width, int height);

/* the root node (created automatically; set its text via gtcaca_mindmap_set_text) */
gtcaca_mm_node_t *gtcaca_mindmap_root(gtcaca_mindmap_widget_t *m);
/* append a child to `parent`; returns the new node (selected stays put) */
gtcaca_mm_node_t *gtcaca_mindmap_add_child(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *parent, const char *text);
/* insert a sibling after `node` (cannot add a sibling to the root) */
gtcaca_mm_node_t *gtcaca_mindmap_add_sibling(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *node, const char *text);
/* remove `node` and its subtree (no-op on the root); fixes the selection */
void gtcaca_mindmap_delete(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *node);

void gtcaca_mindmap_set_text(gtcaca_mm_node_t *node, const char *text);
void gtcaca_mindmap_toggle_fold(gtcaca_mm_node_t *node);

gtcaca_mm_node_t *gtcaca_mindmap_selected(gtcaca_mindmap_widget_t *m);
void gtcaca_mindmap_select(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *node);

void gtcaca_mindmap_set_title(gtcaca_mindmap_widget_t *m, const char *title);
void gtcaca_mindmap_draw(gtcaca_mindmap_widget_t *m);
int  gtcaca_mindmap_key(gtcaca_mindmap_widget_t *m, int key, void *userdata);
/* free every node and start over with a single empty root */
void gtcaca_mindmap_clear(gtcaca_mindmap_widget_t *m, const char *root_text);
void gtcaca_mindmap_free(gtcaca_mindmap_widget_t *m);

#endif /* _GTCACA_MINDMAP_H_ */
