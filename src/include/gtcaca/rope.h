#ifndef _GTCACA_ROPE_H_
#define _GTCACA_ROPE_H_

#include <stddef.h>

/* ── Rope: text storage that does not care how big the document is ───────────
 *
 * A flat char buffer makes every insert and delete memmove the tail, so editing
 * near the top of a large file — or pasting into one — costs O(n) per edit and
 * O(n²) for a run of them. Worse, finding "which line is offset p on?" meant
 * scanning the whole buffer, which every caret move and repaint did.
 *
 * A rope is a balanced tree of small chunks. Each node caches the number of
 * bytes *and* the number of newlines beneath it, so insert, delete, byte
 * lookup and line↔offset conversion are all O(log n). This is the same shape
 * xi-editor uses, and the reason it stays responsive on huge files.
 *
 * Offsets are byte offsets; lines are 0-based and counted by '\n'. Nothing here
 * knows about UTF-8 — callers step over multi-byte sequences themselves — and
 * nothing here allocates on read. */

typedef struct gtcaca_rope gtcaca_rope_t;

gtcaca_rope_t *gtcaca_rope_new(void);
gtcaca_rope_t *gtcaca_rope_from(const char *s, int len);   /* len < 0: strlen */
void           gtcaca_rope_free(gtcaca_rope_t *r);

int  gtcaca_rope_len(const gtcaca_rope_t *r);        /* bytes                */
int  gtcaca_rope_lines(const gtcaca_rope_t *r);      /* '\n' count           */

void gtcaca_rope_insert(gtcaca_rope_t *r, int pos, const char *s, int len);
void gtcaca_rope_delete(gtcaca_rope_t *r, int pos, int len);
void gtcaca_rope_clear(gtcaca_rope_t *r);

/* The byte at `pos`, or 0 past the end. */
char gtcaca_rope_at(const gtcaca_rope_t *r, int pos);
/* Copy `len` bytes from `pos` into `out` (not NUL-terminated by this call).
   Returns how many bytes were actually copied. */
int  gtcaca_rope_copy(const gtcaca_rope_t *r, int pos, int len, char *out);
/* Whole contents as a fresh NUL-terminated string the caller frees. */
char *gtcaca_rope_str(const gtcaca_rope_t *r);

/* Line ↔ offset, both O(log n) thanks to the per-node newline counts. */
int  gtcaca_rope_line_of(const gtcaca_rope_t *r, int pos);    /* line holding pos */
int  gtcaca_rope_line_start(const gtcaca_rope_t *r, int line);
/* Offset of the next `byte` at or after `from` (rope length if none), and the
   previous one before `from` (-1 if none). Used for line ends and scanning. */
int  gtcaca_rope_find(const gtcaca_rope_t *r, int from, char byte);
int  gtcaca_rope_rfind(const gtcaca_rope_t *r, int from, char byte);

#endif /* _GTCACA_ROPE_H_ */
