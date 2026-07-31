/* rope.c — the text storage described in gtcaca/rope.h.
 *
 * An AVL-balanced tree whose leaves hold small chunks of bytes. Every node
 * caches the bytes and the newlines beneath it, which is what makes both
 * editing and line lookup logarithmic instead of linear.
 *
 * The two workhorses are split() and concat(): every edit is expressed as
 * "split the tree, join the pieces back with something in between", which is
 * why there is no special-case code for inserting at a chunk boundary, in the
 * middle of a chunk, or across several of them. */

#include <stdlib.h>
#include <string.h>

#include <gtcaca/rope.h>

#define LEAF_MAX 512      /* bytes per chunk: big enough that the tree stays
                             shallow, small enough that a chunk copy is cheap */

typedef struct rnode {
  struct rnode *left, *right;   /* both NULL in a leaf */
  int   height;
  int   bytes;                  /* in this whole subtree */
  int   lines;                  /* '\n' in this whole subtree */
  char *buf;                    /* leaf only */
  int   len;                    /* leaf only */
} rnode_t;

struct gtcaca_rope { rnode_t *root; };

static int imax(int a, int b) { return a > b ? a : b; }
static int is_leaf(const rnode_t *n) { return n && !n->left && !n->right; }
static int nbytes(const rnode_t *n)  { return n ? n->bytes : 0; }
static int nlines(const rnode_t *n)  { return n ? n->lines : 0; }
static int nheight(const rnode_t *n) { return n ? n->height : 0; }

static int count_lines(const char *s, int len)
{
  int i, n = 0;
  for (i = 0; i < len; i++) if (s[i] == '\n') n++;
  return n;
}

static rnode_t *leaf_new(const char *s, int len)
{
  rnode_t *n;
  if (len <= 0) return NULL;
  n = calloc(1, sizeof *n);
  if (!n) return NULL;
  n->buf = malloc((size_t)len);
  if (!n->buf) { free(n); return NULL; }
  memcpy(n->buf, s, (size_t)len);
  n->len = n->bytes = len;
  n->lines = count_lines(s, len);
  n->height = 1;
  return n;
}

static void node_free(rnode_t *n)
{
  if (!n) return;
  node_free(n->left);
  node_free(n->right);
  free(n->buf);
  free(n);
}

static void update(rnode_t *n)
{
  if (is_leaf(n)) {
    n->bytes = n->len;
    n->lines = count_lines(n->buf, n->len);
    n->height = 1;
    return;
  }
  n->bytes  = nbytes(n->left) + nbytes(n->right);
  n->lines  = nlines(n->left) + nlines(n->right);
  n->height = 1 + imax(nheight(n->left), nheight(n->right));
}

static rnode_t *rotate_right(rnode_t *n)
{
  rnode_t *l = n->left;
  n->left = l->right;
  l->right = n;
  update(n);
  update(l);
  return l;
}

static rnode_t *rotate_left(rnode_t *n)
{
  rnode_t *r = n->right;
  n->right = r->left;
  r->left = n;
  update(n);
  update(r);
  return r;
}

static rnode_t *balance(rnode_t *n)
{
  int bf;
  if (!n || is_leaf(n)) return n;
  update(n);
  bf = nheight(n->left) - nheight(n->right);
  if (bf > 1) {
    if (nheight(n->left->left) < nheight(n->left->right)) n->left = rotate_left(n->left);
    return rotate_right(n);
  }
  if (bf < -1) {
    if (nheight(n->right->right) < nheight(n->right->left)) n->right = rotate_right(n->right);
    return rotate_left(n);
  }
  return n;
}

/* Join two subtrees. Two small leaves become one chunk, which keeps the tree
   from filling up with slivers after lots of small edits. */
static rnode_t *concat(rnode_t *a, rnode_t *b)
{
  rnode_t *n;
  if (!a) return b;
  if (!b) return a;
  if (is_leaf(a) && is_leaf(b) && a->len + b->len <= LEAF_MAX) {
    char *p = realloc(a->buf, (size_t)(a->len + b->len));
    if (p) {
      a->buf = p;
      memcpy(a->buf + a->len, b->buf, (size_t)b->len);
      a->len += b->len;
      update(a);
      node_free(b);
      return a;
    }
  }
  /* Descend into the taller side so the result stays balanced. */
  if (nheight(a) > nheight(b) + 1 && !is_leaf(a)) {
    a->right = concat(a->right, b);
    return balance(a);
  }
  if (nheight(b) > nheight(a) + 1 && !is_leaf(b)) {
    b->left = concat(a, b->left);
    return balance(b);
  }
  n = calloc(1, sizeof *n);
  if (!n) return a;                     /* out of memory: keep what we have */
  n->left = a;
  n->right = b;
  update(n);
  return balance(n);
}

/* Cut the subtree at byte `at`; consumes `n` and hands back both halves. */
static void split(rnode_t *n, int at, rnode_t **out_l, rnode_t **out_r)
{
  if (!n) { *out_l = *out_r = NULL; return; }
  if (is_leaf(n)) {
    if (at <= 0)          { *out_l = NULL; *out_r = n; return; }
    if (at >= n->len)     { *out_l = n;    *out_r = NULL; return; }
    *out_r = leaf_new(n->buf + at, n->len - at);
    n->len = at;
    update(n);
    *out_l = n;
    return;
  }
  {
    int lb = nbytes(n->left);
    rnode_t *l = n->left, *r = n->right;
    free(n);                            /* the shell goes; children live on */
    if (at < lb) {
      rnode_t *ll, *lr;
      split(l, at, &ll, &lr);
      *out_l = ll;
      *out_r = concat(lr, r);
    } else if (at > lb) {
      rnode_t *rl, *rr;
      split(r, at - lb, &rl, &rr);
      *out_l = concat(l, rl);
      *out_r = rr;
    } else {
      *out_l = l;
      *out_r = r;
    }
  }
}

/* ── the public surface ─────────────────────────────────────────────────────*/

gtcaca_rope_t *gtcaca_rope_new(void)
{
  return calloc(1, sizeof(gtcaca_rope_t));
}

gtcaca_rope_t *gtcaca_rope_from(const char *s, int len)
{
  gtcaca_rope_t *r = gtcaca_rope_new();
  if (!r) return NULL;
  if (s) {
    if (len < 0) len = (int)strlen(s);
    gtcaca_rope_insert(r, 0, s, len);
  }
  return r;
}

void gtcaca_rope_free(gtcaca_rope_t *r)
{
  if (!r) return;
  node_free(r->root);
  free(r);
}

void gtcaca_rope_clear(gtcaca_rope_t *r)
{
  if (!r) return;
  node_free(r->root);
  r->root = NULL;
}

int gtcaca_rope_len(const gtcaca_rope_t *r)   { return r ? nbytes(r->root) : 0; }
int gtcaca_rope_lines(const gtcaca_rope_t *r) { return r ? nlines(r->root) : 0; }

void gtcaca_rope_insert(gtcaca_rope_t *r, int pos, const char *s, int len)
{
  rnode_t *l = NULL, *mid = NULL, *rr = NULL;
  int done = 0;

  if (!r || !s) return;
  if (len < 0) len = (int)strlen(s);
  if (len <= 0) return;
  if (pos < 0) pos = 0;
  if (pos > nbytes(r->root)) pos = nbytes(r->root);

  split(r->root, pos, &l, &rr);
  /* Long inserts become several chunks rather than one enormous leaf. */
  while (done < len) {
    int take = len - done > LEAF_MAX ? LEAF_MAX : len - done;
    mid = leaf_new(s + done, take);
    l = concat(l, mid);
    done += take;
  }
  r->root = concat(l, rr);
}

void gtcaca_rope_delete(gtcaca_rope_t *r, int pos, int len)
{
  rnode_t *l = NULL, *rest = NULL, *mid = NULL, *rr = NULL;
  int total;

  if (!r || len <= 0) return;
  total = nbytes(r->root);
  if (pos < 0) pos = 0;
  if (pos >= total) return;
  if (pos + len > total) len = total - pos;

  split(r->root, pos, &l, &rest);
  split(rest, len, &mid, &rr);
  node_free(mid);
  r->root = concat(l, rr);
}

char gtcaca_rope_at(const gtcaca_rope_t *r, int pos)
{
  const rnode_t *n = r ? r->root : NULL;
  if (!n || pos < 0 || pos >= n->bytes) return '\0';
  while (!is_leaf(n)) {
    int lb = nbytes(n->left);
    if (pos < lb) n = n->left;
    else { pos -= lb; n = n->right; }
  }
  return n->buf[pos];
}

static int copy_from(const rnode_t *n, int pos, int len, char *out)
{
  int got = 0;
  if (!n || len <= 0) return 0;
  if (is_leaf(n)) {
    int avail = n->len - pos;
    if (avail <= 0) return 0;
    if (avail < len) len = avail;
    memcpy(out, n->buf + pos, (size_t)len);
    return len;
  }
  {
    int lb = nbytes(n->left);
    if (pos < lb) {
      got = copy_from(n->left, pos, len, out);
      len -= got;
      pos = lb;
    }
    if (len > 0) got += copy_from(n->right, pos - lb, len, out + got);
  }
  return got;
}

int gtcaca_rope_copy(const gtcaca_rope_t *r, int pos, int len, char *out)
{
  int total = gtcaca_rope_len(r);
  if (!r || !out || len <= 0 || pos < 0 || pos >= total) return 0;
  if (pos + len > total) len = total - pos;
  return copy_from(r->root, pos, len, out);
}

char *gtcaca_rope_str(const gtcaca_rope_t *r)
{
  int len = gtcaca_rope_len(r);
  char *s = malloc((size_t)len + 1);
  if (!s) return NULL;
  if (len) copy_from(r->root, 0, len, s);
  s[len] = '\0';
  return s;
}

int gtcaca_rope_line_of(const gtcaca_rope_t *r, int pos)
{
  const rnode_t *n = r ? r->root : NULL;
  int line = 0, i;
  if (!n) return 0;
  if (pos > n->bytes) pos = n->bytes;
  while (n && !is_leaf(n)) {
    int lb = nbytes(n->left);
    if (pos <= lb) n = n->left;
    else { line += nlines(n->left); pos -= lb; n = n->right; }
  }
  if (!n) return line;
  for (i = 0; i < pos && i < n->len; i++) if (n->buf[i] == '\n') line++;
  return line;
}

int gtcaca_rope_line_start(const gtcaca_rope_t *r, int line)
{
  const rnode_t *n = r ? r->root : NULL;
  int off = 0, need = line, i;
  if (!n || line <= 0) return 0;
  if (line > nlines(n)) return nbytes(n);        /* past the last line */
  while (!is_leaf(n)) {
    if (nlines(n->left) >= need) n = n->left;
    else { need -= nlines(n->left); off += nbytes(n->left); n = n->right; }
  }
  for (i = 0; i < n->len; i++)
    if (n->buf[i] == '\n' && --need == 0) return off + i + 1;
  return off + n->len;
}

int gtcaca_rope_find(const gtcaca_rope_t *r, int from, char byte)
{
  int total = gtcaca_rope_len(r), i;
  if (from < 0) from = 0;
  for (i = from; i < total; i++)                 /* chunk-wise would be faster;
                                                    callers scan a line at most */
    if (gtcaca_rope_at(r, i) == byte) return i;
  return total;
}

int gtcaca_rope_rfind(const gtcaca_rope_t *r, int from, char byte)
{
  int i, total = gtcaca_rope_len(r);
  if (from > total) from = total;
  for (i = from - 1; i >= 0; i--)
    if (gtcaca_rope_at(r, i) == byte) return i;
  return -1;
}
