/* demo_tree — a Tree backed by a *lazy* model of one billion nodes.
 *
 * The model is purely computed: a node is a (depth, ordinal) pair packed into
 * the opaque handle, and children are generated on demand. Nothing is ever
 * allocated for the dataset — the view only asks about rows that are on screen
 * or subtrees the user expanded, so 1,000,000,000 nodes browse instantly. */
#include <stdint.h>
#include <stdio.h>
#include <gtcaca/main.h>
#include <gtcaca/window.h>
#include <gtcaca/tree.h>

#define BRANCH 1000          /* children per internal node            */
#define DEPTH  3             /* 1000^3 = 1,000,000,000 leaf nodes      */

static int  ndepth(void *n) { return n ? (int)((uintptr_t)n >> 56) : 0; }
static unsigned long nord(void *n) { return n ? ((uintptr_t)n & 0x00FFFFFFFFFFFFFFULL) : 0UL; }
static void *mknode(int d, unsigned long ord) { return (void *)(((uintptr_t)d << 56) | ord); }

static long m_child_count(gtcaca_tree_model_t *m, void *node)
{ (void)m; return ndepth(node) < DEPTH ? BRANCH : 0; }

static void *m_child(gtcaca_tree_model_t *m, void *node, long i)
{ (void)m; return mknode(ndepth(node) + 1, nord(node) * BRANCH + (unsigned long)i); }

static int m_has_children(gtcaca_tree_model_t *m, void *node)
{ (void)m; return ndepth(node) < DEPTH; }

static void m_label(gtcaca_tree_model_t *m, void *node, char *buf, int len)
{
  int d = ndepth(node);
  const char *kind = d == 1 ? "Group" : d == 2 ? "Subgroup" : "Item";
  (void)m;
  snprintf(buf, len, "%s %lu", kind, nord(node));
}

int main(int argc, char **argv)
{
  static gtcaca_tree_model_t model = { m_child_count, m_child, m_label, m_has_children, NULL };
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  gtcaca_tree_widget_t *tree;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("gtcaca tree");
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, app->x, app->y, app->width, app->height);

  tree = gtcaca_tree_new(GTCACA_WIDGET(win), 0, 0, win->width, win->height);
  gtcaca_tree_set_model(tree, &model);
  gtcaca_tree_set_title(tree,
    "1,000,000,000 nodes (lazy)  -  up/down move, right/Enter expand, left collapse, q quit");
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(tree));

  gtcaca_main();
  return 0;
}
