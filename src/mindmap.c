#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/mindmap.h>
#include <gtcaca/main.h>

/* line-direction bits for the connector grid */
enum { LU = 1, LD = 2, LL = 4, LR = 8 };

#define CELL_CAP 24      /* widest a node cell may grow */
#define GAP      4       /* columns between a cell and its children */

/* ── node helpers ──────────────────────────────────────────────────────────── */
static gtcaca_mm_node_t *node_new(const char *text, gtcaca_mm_node_t *parent)
{
  gtcaca_mm_node_t *n = calloc(1, sizeof *n);
  if (!n) return NULL;
  n->text = strdup(text ? text : "");
  n->colour = CACA_LIGHTGREEN;
  n->parent = parent;
  return n;
}

static void node_attach(gtcaca_mm_node_t *parent, gtcaca_mm_node_t *child, int at)
{
  int i;
  if (parent->nkids == parent->capkids) {
    parent->capkids = parent->capkids ? parent->capkids * 2 : 4;
    parent->kids = realloc(parent->kids, (size_t)parent->capkids * sizeof *parent->kids);
  }
  if (at < 0 || at > parent->nkids) at = parent->nkids;
  for (i = parent->nkids; i > at; i--) parent->kids[i] = parent->kids[i - 1];
  parent->kids[at] = child;
  parent->nkids++;
  child->parent = parent;
}

static void node_free(gtcaca_mm_node_t *n)
{
  int i;
  if (!n) return;
  for (i = 0; i < n->nkids; i++) node_free(n->kids[i]);
  free(n->kids); free(n->text); free(n);
}

static int node_index(gtcaca_mm_node_t *n)
{
  int i; gtcaca_mm_node_t *p = n->parent;
  if (!p) return -1;
  for (i = 0; i < p->nkids; i++) if (p->kids[i] == n) return i;
  return -1;
}

static int node_is_descendant(gtcaca_mm_node_t *n, gtcaca_mm_node_t *anc)
{
  for (; n; n = n->parent) if (n == anc) return 1;
  return 0;
}

/* ── public node ops ───────────────────────────────────────────────────────── */
gtcaca_mindmap_widget_t *gtcaca_mindmap_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_mindmap_widget_t *m = malloc(sizeof(*m));
  if (!m) { fprintf(stderr, "Error: Cannot allocate mindmap\n"); return NULL; }
  m->id = gtcaca_get_newid();
  m->has_focus = 0; m->is_visible = 1; m->type = GTCACA_WIDGET_MINDMAP;
  m->parent = parent; m->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(m), x, y);
  m->width = width; m->height = height;
  m->color_focus_fg = m->color_nonfocus_fg = gmo.theme.textview.fg;
  m->color_focus_bg = m->color_nonfocus_bg = gmo.theme.textview.bg;
  m->root = node_new("New Mindmap", NULL);
  m->sel = m->root; m->top = 0; m->xoff = 0; m->title = NULL;
  m->sel_fg = CACA_BLACK; m->sel_bg = CACA_CYAN;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(m));
  return m;
}

gtcaca_mm_node_t *gtcaca_mindmap_root(gtcaca_mindmap_widget_t *m) { return m->root; }
gtcaca_mm_node_t *gtcaca_mindmap_selected(gtcaca_mindmap_widget_t *m) { return m->sel; }
void gtcaca_mindmap_select(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *n) { if (n) m->sel = n; }
void gtcaca_mindmap_set_title(gtcaca_mindmap_widget_t *m, const char *t)
{ free(m->title); m->title = t ? strdup(t) : NULL; }

void gtcaca_mindmap_set_text(gtcaca_mm_node_t *n, const char *text)
{ if (!n) return; free(n->text); n->text = strdup(text ? text : ""); }

void gtcaca_mindmap_toggle_fold(gtcaca_mm_node_t *n)
{ if (n && n->nkids) n->folded = !n->folded; }

gtcaca_mm_node_t *gtcaca_mindmap_add_child(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *parent, const char *text)
{
  gtcaca_mm_node_t *n;
  if (!parent) parent = m->root;
  n = node_new(text, parent);
  if (!n) return NULL;
  node_attach(parent, n, -1);
  return n;
}

gtcaca_mm_node_t *gtcaca_mindmap_add_sibling(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *node, const char *text)
{
  gtcaca_mm_node_t *n;
  (void)m;
  if (!node || !node->parent) return NULL;
  n = node_new(text, node->parent);
  if (!n) return NULL;
  node_attach(node->parent, n, node_index(node) + 1);
  return n;
}

void gtcaca_mindmap_delete(gtcaca_mindmap_widget_t *m, gtcaca_mm_node_t *node)
{
  gtcaca_mm_node_t *p;
  int i, idx;
  if (!node || node == m->root) return;
  p = node->parent; idx = node_index(node);
  if (idx < 0) return;
  if (node_is_descendant(m->sel, node))             /* keep a valid selection */
    m->sel = (p->nkids > 1) ? p->kids[idx > 0 ? idx - 1 : 1] : p;
  for (i = idx; i < p->nkids - 1; i++) p->kids[i] = p->kids[i + 1];
  p->nkids--;
  node_free(node);
}

void gtcaca_mindmap_clear(gtcaca_mindmap_widget_t *m, const char *root_text)
{
  node_free(m->root);
  m->root = node_new(root_text ? root_text : "New Mindmap", NULL);
  m->sel = m->root; m->top = m->xoff = 0;
}

void gtcaca_mindmap_free(gtcaca_mindmap_widget_t *m)
{
  if (!m) return;
  node_free(m->root); free(m->title); free(m);
}

/* ── layout ────────────────────────────────────────────────────────────────── */
static int  g_maxdepth;
static int  g_cellw[256];     /* per-depth cell width */
static int  g_colx[256];      /* per-depth x          */

static int node_cellw(gtcaca_mm_node_t *n)
{
  int w = (int)strlen(n->text);
  if (w > CELL_CAP) w = CELL_CAP;
  if (w < 1) w = 1;
  return w;
}

/* assign each visible node a row (leaves consume one row, parents centre on
   their children); returns the node's row. *next is the running row counter. */
static int layout_rows(gtcaca_mm_node_t *n, int depth, int *next)
{
  int i, first = -1, last = -1;
  n->depth = depth;
  if (depth > g_maxdepth) g_maxdepth = depth;
  { int w = node_cellw(n); if (w > g_cellw[depth]) g_cellw[depth] = w; }
  if (n->folded || n->nkids == 0) { n->row = (*next)++; return n->row; }
  for (i = 0; i < n->nkids; i++) {
    int r = layout_rows(n->kids[i], depth + 1, next);
    if (first < 0) first = r;
    last = r;
  }
  n->row = (first + last) / 2;
  return n->row;
}

/* collect visible nodes (pre-order = top-to-bottom) into out[] */
static void collect_visible(gtcaca_mm_node_t *n, gtcaca_mm_node_t **out, int *cnt)
{
  int i;
  out[(*cnt)++] = n;
  if (!n->folded) for (i = 0; i < n->nkids; i++) collect_visible(n->kids[i], out, cnt);
}

static int count_nodes(gtcaca_mm_node_t *n)
{
  int i, c = 1;
  for (i = 0; i < n->nkids; i++) c += count_nodes(n->kids[i]);
  return c;
}

static uint32_t boxglyph(unsigned b)
{
  switch (b) {
  case LL|LR:             return 0x2500; /* ─ */
  case LU|LD:             return 0x2502; /* │ */
  case LD|LR:             return 0x250C; /* ┌ */
  case LD|LL:             return 0x2510; /* ┐ */
  case LU|LR:             return 0x2514; /* └ */
  case LU|LL:             return 0x2518; /* ┘ */
  case LU|LD|LR:          return 0x251C; /* ├ */
  case LU|LD|LL:          return 0x2524; /* ┤ */
  case LD|LL|LR:          return 0x252C; /* ┬ */
  case LU|LL|LR:          return 0x2534; /* ┴ */
  case LU|LD|LL|LR:       return 0x253C; /* ┼ */
  case LU: case LD:       return 0x2502;
  case LL: case LR:       return 0x2500;
  default:                return 0;
  }
}

void gtcaca_mindmap_draw(gtcaca_mindmap_widget_t *m)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = m->x + 1, inner_y = m->y + 1, inner_w = m->width - 2, inner_h = m->height - 2;
  int rows = 0, total, nvis = 0, d, i, gw, gh, r, c;
  gtcaca_mm_node_t **vis;
  uint8_t *grid;

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, m->x, m->y, m->width, m->height, ' ');
  caca_draw_cp437_box(gmo.cv, m->x, m->y, m->width, m->height);
  if (m->title) caca_printf(gmo.cv, m->x + 2, m->y, "| %s |", m->title);
  if (inner_w <= 0 || inner_h <= 0 || !m->root) return;

  /* layout */
  g_maxdepth = 0;
  for (i = 0; i < 256; i++) { g_cellw[i] = 0; g_colx[i] = 0; }
  layout_rows(m->root, 0, &rows);
  g_colx[0] = 0;
  for (d = 1; d <= g_maxdepth; d++) g_colx[d] = g_colx[d - 1] + g_cellw[d - 1] + GAP;
  gw = g_colx[g_maxdepth] + g_cellw[g_maxdepth] + 1;
  gh = rows;

  total = count_nodes(m->root);
  vis = malloc((size_t)total * sizeof *vis);
  collect_visible(m->root, vis, &nvis);

  /* keep the selection on screen (vertical) */
  if (m->sel) {
    if (m->sel->row < m->top) m->top = m->sel->row;
    if (m->sel->row >= m->top + inner_h) m->top = m->sel->row - inner_h + 1;
  }
  if (m->top < 0) m->top = 0;

  /* connector grid (skip if the map is enormous) */
  grid = ((long)gw * gh > 0 && (long)gw * gh < 4000000) ? calloc((size_t)gw * gh, 1) : NULL;
  if (grid) {
    for (i = 0; i < nvis; i++) {
      gtcaca_mm_node_t *p = vis[i];
      int pr, sx, ce, r0, rn;
      if (p->folded || p->nkids == 0) continue;
      pr = p->row;
      ce = g_colx[p->depth] + g_cellw[p->depth];   /* cell end */
      sx = ce + GAP - 2;                            /* spine column */
      r0 = p->kids[0]->row; rn = p->kids[p->nkids - 1]->row;
      /* parent stub: from cell end to the spine, on the parent's row */
      for (c = ce; c <= sx; c++) {
        if (c > ce)      grid[pr * gw + c] |= LL;
        if (c < sx)      grid[pr * gw + c] |= LR;
      }
      /* vertical spine spanning parent row .. all child rows */
      { int a = pr < r0 ? pr : r0, b = pr > rn ? pr : rn;
        for (r = a; r <= b; r++) {
          if (r > a) grid[r * gw + sx] |= LU;
          if (r < b) grid[r * gw + sx] |= LD;
        } }
      /* child stubs: from spine to each child cell */
      for (r = 0; r < p->nkids; r++) {
        int cr = p->kids[r]->row, cx = g_colx[p->depth + 1];
        for (c = sx; c < cx; c++) {
          if (c > sx) grid[cr * gw + c] |= LL;
          if (c < cx - 0) grid[cr * gw + c] |= LR;
        }
      }
    }
    /* draw connector glyphs (clipped to the visible window) */
    caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
    for (r = 0; r < gh; r++) {
      int sy = inner_y + (r - m->top);
      if (r < m->top || sy >= inner_y + inner_h) continue;
      for (c = 0; c < gw; c++) {
        int sx2 = inner_x + (c - m->xoff);
        uint32_t g;
        if (c < m->xoff || sx2 >= inner_x + inner_w) continue;
        g = boxglyph(grid[r * gw + c]);
        if (g) caca_put_char(gmo.cv, sx2, sy, g);
      }
    }
    free(grid);
  }

  /* draw node labels on top of the connectors */
  for (i = 0; i < nvis; i++) {
    gtcaca_mm_node_t *n = vis[i];
    int sy = inner_y + (n->row - m->top);
    int lx = g_colx[n->depth], k, avail, len, drawn;
    int sel = (n == m->sel && m->has_focus);
    if (n->row < m->top || sy >= inner_y + inner_h) continue;
    /* label */
    if (sel) { caca_set_color_ansi(gmo.cv, m->sel_fg, m->sel_bg); caca_set_attr(gmo.cv, CACA_BOLD); }
    else     { caca_set_color_ansi(gmo.cv, n->colour ? n->colour : fg, bg); caca_set_attr(gmo.cv, 0); }
    len = (int)strlen(n->text);
    avail = g_cellw[n->depth];
    for (k = 0; k < len && k < avail; k++) {
      int cx = inner_x + (lx + k - m->xoff);
      if (lx + k < m->xoff || cx >= inner_x + inner_w) continue;
      caca_put_char(gmo.cv, cx, sy, (uint32_t)(unsigned char)n->text[k]);
    }
    /* a folded node hides a subtree — mark it with a '+' on the children side */
    drawn = len < avail ? len : avail;
    if (n->folded && n->nkids) {
      int mx = inner_x + (lx + drawn + 1 - m->xoff);
      if (lx + drawn + 1 >= m->xoff && mx < inner_x + inner_w) {
        caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
        caca_put_char(gmo.cv, mx, sy, '+');
      }
    }
  }
  caca_set_attr(gmo.cv, 0);
  free(vis);
}

/* ── keys ──────────────────────────────────────────────────────────────────── */
int gtcaca_mindmap_key(gtcaca_mindmap_widget_t *m, int key, void *userdata)
{
  gtcaca_mm_node_t **vis;
  int total, nvis = 0, idx = -1, i, rows = 0;
  (void)userdata;
  if (!m->root) return 0;

  /* (re)compute rows so navigation matches what is on screen */
  g_maxdepth = 0; for (i = 0; i < 256; i++) g_cellw[i] = 0;
  layout_rows(m->root, 0, &rows);
  total = count_nodes(m->root);
  vis = malloc((size_t)total * sizeof *vis);
  collect_visible(m->root, vis, &nvis);
  for (i = 0; i < nvis; i++) if (vis[i] == m->sel) { idx = i; break; }
  if (idx < 0) { m->sel = m->root; idx = 0; }

  switch (key) {
  case CACA_KEY_UP:    if (idx > 0)         m->sel = vis[idx - 1]; break;
  case CACA_KEY_DOWN:  if (idx < nvis - 1)  m->sel = vis[idx + 1]; break;
  case CACA_KEY_LEFT:
    if (m->sel->nkids && !m->sel->folded) m->sel->folded = 1;   /* fold */
    else if (m->sel->parent)              m->sel = m->sel->parent;
    break;
  case CACA_KEY_RIGHT:
    if (m->sel->nkids && m->sel->folded)  m->sel->folded = 0;   /* unfold */
    else if (m->sel->nkids)               m->sel = m->sel->kids[0];
    break;
  case ' ':
    gtcaca_mindmap_toggle_fold(m->sel);
    break;
  case '+': case '=':                       /* unfold */
    if (m->sel->nkids) m->sel->folded = 0;
    break;
  case '-': case '_':                       /* fold */
    if (m->sel->nkids) m->sel->folded = 1;
    break;
  default:
    free(vis); return 0;
  }
  free(vis);
  return 1;
}
