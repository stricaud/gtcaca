#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/tree.h>
#include <gtcaca/main.h>

/* ── open-state ────────────────────────────────────────────────────────────────
 * We materialise only expanded nodes. Each `tnode` caches the model's
 * child_count and `flat` = the number of visible rows the node's subtree
 * contributes (its children plus any expanded descendants). Collapsed children
 * are never materialised — a run of them is counted, not stored — so a node
 * with a billion collapsed children costs nothing but a number. */
typedef struct tnode {
  void  *node;            /* model handle (root level: NULL)            */
  long   count;           /* child_count(node)                          */
  long   flat;            /* visible rows: count + Σ(open kids' flat)    */
  long  *kidx;            /* ascending child indices that are expanded  */
  struct tnode **kids;    /* parallel expanded child subtrees           */
  int    nkid, kcap;
  struct tnode *parent;
} tnode;

typedef struct {
  void *node;
  tnode *parent;          /* the open node holding this row             */
  long  index;            /* child index within parent                  */
  int   depth;
  tnode *self;            /* this row's open node, or NULL if collapsed */
  int   has_kids;
  long  parent_row;       /* flattened row of parent's header (-1 root) */
} rowinfo;

static void _free_tnode(tnode *t)
{
  int i;
  if (!t) return;
  for (i = 0; i < t->nkid; i++) _free_tnode(t->kids[i]);
  free(t->kidx); free(t->kids); free(t);
}

static void _add_flat(tnode *t, long d)   /* add d to t and all ancestors */
{
  for (; t; t = t->parent) t->flat += d;
}

static tnode *_make_root(gtcaca_tree_widget_t *t)
{
  tnode *r = calloc(1, sizeof *r);
  if (!r) return NULL;
  r->node = NULL;
  r->count = t->model ? t->model->child_count(t->model, NULL) : 0;
  r->flat = r->count;
  return r;
}

/* expand child `ci` of P (P must be an open node); returns the new tnode */
static tnode *_expand(gtcaca_tree_widget_t *t, tnode *P, long ci)
{
  tnode *C;
  int pos, k;
  for (k = 0; k < P->nkid; k++) if (P->kidx[k] == ci) return P->kids[k]; /* already open */
  C = calloc(1, sizeof *C);
  if (!C) return NULL;
  C->node = t->model->child(t->model, P->node, ci);
  C->count = t->model->child_count(t->model, C->node);
  C->flat = C->count;
  C->parent = P;
  if (P->nkid == P->kcap) {
    int nc = P->kcap ? P->kcap * 2 : 4;
    long *ni = realloc(P->kidx, (size_t)nc * sizeof(long));
    tnode **nk = realloc(P->kids, (size_t)nc * sizeof(tnode *));
    if (!ni || !nk) { free(ni ? ni : NULL); free(C); return NULL; }
    P->kidx = ni; P->kids = nk; P->kcap = nc;
  }
  for (pos = 0; pos < P->nkid && P->kidx[pos] < ci; pos++) ;
  memmove(&P->kidx[pos + 1], &P->kidx[pos], (size_t)(P->nkid - pos) * sizeof(long));
  memmove(&P->kids[pos + 1], &P->kids[pos], (size_t)(P->nkid - pos) * sizeof(tnode *));
  P->kidx[pos] = ci; P->kids[pos] = C; P->nkid++;
  _add_flat(P, C->flat);          /* its descendants become visible */
  return C;
}

/* collapse open node `C` (a child of P) */
static void _collapse(tnode *P, tnode *C)
{
  int k, pos = -1;
  for (k = 0; k < P->nkid; k++) if (P->kids[k] == C) { pos = k; break; }
  if (pos < 0) return;
  _add_flat(P, -C->flat);
  _free_tnode(C);
  memmove(&P->kidx[pos], &P->kidx[pos + 1], (size_t)(P->nkid - pos - 1) * sizeof(long));
  memmove(&P->kids[pos], &P->kids[pos + 1], (size_t)(P->nkid - pos - 1) * sizeof(tnode *));
  P->nkid--;
}

/* locate the node at flattened row `row` */
static int _resolve(gtcaca_tree_widget_t *t, long row, rowinfo *out)
{
  tnode *P = (tnode *)t->root;
  long rem = row;
  int depth = 0;
  long parent_row = -1;
  if (!P || row < 0 || row >= P->flat) return 0;
  for (;;) {
    long child = 0;
    int j = 0;
    for (;;) {
      long next_open = (j < P->nkid) ? P->kidx[j] : P->count;
      long run = next_open - child;
      if (rem < run) {                                   /* a collapsed child */
        long ci = child + rem;
        out->node = t->model->child(t->model, P->node, ci);
        out->parent = P; out->index = ci; out->depth = depth;
        out->self = NULL; out->parent_row = parent_row;
        out->has_kids = t->model->has_children(t->model, out->node);
        return 1;
      }
      rem -= run;
      if (j >= P->nkid) return 0;                        /* out of range guard */
      {
        tnode *C = P->kids[j];
        long hdr = row - rem;                            /* this open node's row */
        if (rem == 0) {                                  /* its own header row */
          out->node = C->node; out->parent = P; out->index = P->kidx[j];
          out->depth = depth; out->self = C; out->parent_row = parent_row;
          out->has_kids = t->model->has_children(t->model, C->node);
          return 1;
        }
        rem -= 1;
        if (rem < C->flat) { parent_row = hdr; P = C; depth++; break; } /* descend */
        rem -= C->flat;
        child = P->kidx[j] + 1; j++;
      }
    }
  }
}

gtcaca_tree_widget_t *gtcaca_tree_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_tree_widget_t *t = malloc(sizeof(*t));
  if (!t) { fprintf(stderr, "Error: Cannot allocate tree\n"); return NULL; }
  t->id = gtcaca_get_newid();
  t->has_focus = 0;
  t->is_visible = 1;
  t->type = GTCACA_WIDGET_TREE;
  t->parent = parent;
  t->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(t), x, y);
  t->width = width; t->height = height;
  t->color_focus_fg = t->color_nonfocus_fg = gmo.theme.textview.fg;
  t->color_focus_bg = t->color_nonfocus_bg = gmo.theme.textview.bg;
  t->model = NULL; t->root = NULL; t->top = 0; t->sel = 0; t->title = NULL;
  t->sel_bg = CACA_BLUE;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(t));
  return t;
}

void gtcaca_tree_set_model(gtcaca_tree_widget_t *t, gtcaca_tree_model_t *model)
{
  _free_tnode((tnode *)t->root);
  t->model = model;
  t->root = _make_root(t);
  t->top = t->sel = 0;
}

void gtcaca_tree_set_title(gtcaca_tree_widget_t *t, const char *title)
{
  free(t->title); t->title = title ? strdup(title) : NULL;
}

void gtcaca_tree_free(gtcaca_tree_widget_t *t)
{
  if (!t) return;
  _free_tnode((tnode *)t->root); free(t->title); free(t);
}

long gtcaca_tree_visible_count(gtcaca_tree_widget_t *t)
{
  return t->root ? ((tnode *)t->root)->flat : 0;
}

void *gtcaca_tree_selected_node(gtcaca_tree_widget_t *t)
{
  rowinfo ri;
  if (_resolve(t, t->sel, &ri)) return ri.node;
  return NULL;
}

void gtcaca_tree_draw(gtcaca_tree_widget_t *t)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = t->x + 1, inner_y = t->y + 1;
  int inner_w = t->width - 2, inner_h = t->height - 2;
  long vis = gtcaca_tree_visible_count(t);
  int i;
  char buf[512];

  /* frame + title */
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, t->x, t->y, t->width, t->height, ' ');
  caca_draw_cp437_box(gmo.cv, t->x, t->y, t->width, t->height);
  if (t->title) caca_printf(gmo.cv, t->x + 2, t->y, "| %s |", t->title);
  if (inner_w <= 0 || inner_h <= 0) return;

  for (i = 0; i < inner_h; i++) {
    long row = t->top + i;
    int y = inner_y + i, indent, col, selected;
    rowinfo ri;
    uint8_t rbg;
    if (row >= vis || !_resolve(t, row, &ri)) break;
    selected = (row == t->sel && t->has_focus);
    rbg = selected ? t->sel_bg : bg;

    /* paint the row background */
    caca_set_color_ansi(gmo.cv, selected ? CACA_WHITE : fg, rbg); caca_set_attr(gmo.cv, 0);
    for (col = 0; col < inner_w; col++) caca_put_char(gmo.cv, inner_x + col, y, ' ');

    indent = ri.depth * 2;
    /* expand marker: '+' collapsed, '-' expanded, space for a leaf. ASCII so it
       renders in every terminal font (same convention as the fold margin). */
    if (indent < inner_w) {
      uint32_t mk = ri.has_kids ? (ri.self ? '-' : '+') : ' ';
      caca_set_color_ansi(gmo.cv, ri.has_kids ? CACA_YELLOW : fg, rbg);
      caca_put_char(gmo.cv, inner_x + indent, y, mk);
    }
    /* row content: a custom cell renderer if the model provides one, else the
       plain text label. The content area is to the right of the marker. */
    {
      int cx = inner_x + indent + 2;
      int cw = inner_w - indent - 2; if (cw < 0) cw = 0;
      caca_set_color_ansi(gmo.cv, selected ? CACA_WHITE : fg, rbg); caca_set_attr(gmo.cv, 0);
      if (t->model->draw_row) {
        t->model->draw_row(t->model, ri.node, cx, y, cw, selected);
      } else {
        t->model->label(t->model, ri.node, buf, sizeof buf);
        caca_printf(gmo.cv, cx, y, "%-.*s", cw, buf);
      }
    }
  }
}

int gtcaca_tree_key(gtcaca_tree_widget_t *t, int key, void *userdata)
{
  long vis = gtcaca_tree_visible_count(t);
  int inner_h = t->height - 2;
  rowinfo ri;
  (void)userdata;
  if (!t->model || !t->root) return 0;

  switch (key) {
  case CACA_KEY_UP:    if (t->sel > 0) t->sel--; break;
  case CACA_KEY_DOWN:  if (t->sel < vis - 1) t->sel++; break;
  case CACA_KEY_PAGEUP:   t->sel -= inner_h; break;
  case CACA_KEY_PAGEDOWN: t->sel += inner_h; break;
  case CACA_KEY_HOME:  t->sel = 0; break;
  case CACA_KEY_END:   t->sel = vis - 1; break;
  case CACA_KEY_RIGHT:
    if (_resolve(t, t->sel, &ri)) {
      if (ri.has_kids && !ri.self) _expand(t, ri.parent, ri.index);
      else if (ri.self && t->sel < vis - 1) t->sel++;     /* into first child */
    }
    break;
  case CACA_KEY_LEFT:
    if (_resolve(t, t->sel, &ri)) {
      if (ri.self) _collapse(ri.parent, ri.self);          /* collapse */
      else if (ri.parent_row >= 0) t->sel = ri.parent_row; /* up to parent */
    }
    break;
  case CACA_KEY_RETURN:
  case 10:
  case ' ':
    if (_resolve(t, t->sel, &ri) && ri.has_kids) {
      if (ri.self) _collapse(ri.parent, ri.self);
      else _expand(t, ri.parent, ri.index);
    }
    break;
  default:
    return 0;
  }

  vis = gtcaca_tree_visible_count(t);                       /* may have changed */
  if (t->sel < 0) t->sel = 0;
  if (t->sel > vis - 1) t->sel = vis - 1;
  if (t->sel < t->top) t->top = t->sel;
  if (t->sel >= t->top + inner_h) t->top = t->sel - inner_h + 1;
  if (t->top < 0) t->top = 0;
  return 1;
}
