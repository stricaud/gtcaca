#ifndef _GTCACA_HEXVIEW_H_
#define _GTCACA_HEXVIEW_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Hex/ASCII byte viewer and editor ────────────────────────────────────────
 *
 * Renders a buffer as rows of "OFFSET  hh hh … hh  |ascii|". Out of the box it
 * is a viewer (Wireshark's packet-bytes pane): give it data, optionally
 * highlight a range, and let the arrow keys walk a byte cursor.
 *
 * It also edits. The widget never owns the bytes and never mutates them
 * itself: typing reports the intended change through `edit_cb` / `splice_cb`
 * and the application applies it to its own model. That is what lets an editor
 * keep its undo history, its layers, or a memory-mapped file behind the view
 * instead of fighting a second copy inside the widget.
 *
 * Data reaches the widget one of two ways:
 *   gtcaca_hexview_set_data()     a borrowed flat buffer (not copied)
 *   gtcaca_hexview_set_read_cb()  a provider, for data too big to hold at once
 */

#define GTCACA_HEXVIEW_MAX_BPR   64   /* upper bound on bytes per row     */
#define GTCACA_HEXVIEW_TAG_NAME  32

typedef struct _gtcaca_hexview_widget_t gtcaca_hexview_widget_t;

/* ── callbacks ─────────────────────────────────────────────────────────────*/

/* Per-cell colour hook. Called for every painted byte — once for its hex pair
 * and once for its ASCII glyph (`is_ascii` tells them apart) — so bytes can be
 * coloured by meaning rather than by the single highlight range: parsed field
 * regions, bookmarks, a selection, a cursor of the application's own.
 *
 * Return non-zero after filling `fg`/`bg` (12-bit 0xRGB, a nibble per channel)
 * to paint the cell; return 0 to fall through to the widget's own colouring,
 * which is: cursor, then selection, then tags, then the highlight range. */
typedef int (*gtcaca_hexview_cell_cb)(gtcaca_hexview_widget_t *h, long offset,
                                      int is_ascii, uint16_t *fg, uint16_t *bg,
                                      void *userdata);

/* A byte was typed over. Return non-zero to accept it; the widget then re-reads
 * the buffer, so the application is free to reject or transform the write. */
typedef int (*gtcaca_hexview_edit_cb)(gtcaca_hexview_widget_t *h, long offset,
                                      uint8_t value, void *userdata);

/* A byte is to be inserted at (is_insert=1) or deleted from `offset`. Return
 * non-zero to accept. The application must resize its buffer and call
 * gtcaca_hexview_set_data() again if the pointer moved. */
typedef int (*gtcaca_hexview_splice_cb)(gtcaca_hexview_widget_t *h, long offset,
                                        int is_insert, void *userdata);

/* Fill `out` with up to `n` bytes starting at `offset`; return how many were
 * written. Lets the widget show data it does not hold — a file on disk, a
 * remote target, a decompressed stream produced on demand. */
typedef long (*gtcaca_hexview_read_cb)(gtcaca_hexview_widget_t *h, long offset,
                                       uint8_t *out, long n, void *userdata);

/* A named, coloured region (cf. ImHex bookmarks / wxHexEditor tags). */
typedef struct {
  long     off, len;
  uint16_t fg, bg;                          /* 12-bit 0xRGB */
  char     name[GTCACA_HEXVIEW_TAG_NAME];
} gtcaca_hexview_tag_t;

struct _gtcaca_hexview_widget_t {
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
  struct _gtcaca_hexview_widget_t *prev;
  struct _gtcaca_hexview_widget_t *next;

  const uint8_t *data;      /* borrowed buffer, or NULL when read_cb is set */
  long           len;
  long           top;       /* first visible row                            */
  long           hl_off;    /* highlight range start, -1 = none             */
  long           hl_len;
  long           cursor;    /* byte cursor offset                           */
  int            nibble;    /* 0 = high nibble, 1 = low                     */
  int            ascii_pane;/* cursor sits in the ASCII pane                */
  long           anchor;    /* selection anchor, -1 = no selection          */
  int            bytes_per_row; /* 0 = fit to the widget width              */
  int            group_size;    /* blank column every N bytes, 0 = none     */
  long           base_addr;     /* added to offsets in the address column   */
  int            addr_digits;   /* 0 = derive from len                      */
  int            show_ascii;
  int            show_box;
  int            editable;
  int            insert_mode;
  char          *title;

  gtcaca_hexview_tag_t *tags;
  int                   n_tags, cap_tags;

  gtcaca_hexview_cell_cb   cell_cb;   void *cell_ud;
  gtcaca_hexview_edit_cb   edit_cb;   void *edit_ud;
  gtcaca_hexview_splice_cb splice_cb; void *splice_ud;
  gtcaca_hexview_read_cb   read_cb;   void *read_ud;
};

/* ── construction ──────────────────────────────────────────────────────────*/
gtcaca_hexview_widget_t *gtcaca_hexview_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_hexview_free(gtcaca_hexview_widget_t *h);
void gtcaca_hexview_draw(gtcaca_hexview_widget_t *h);
/* Arrows/PageUp/PageDown/Home/End move the cursor; TAB swaps panes. When the
 * widget is editable, hex digits, printable ASCII, DELETE, BACKSPACE and INSERT
 * edit through the callbacks. Returns 1 if the key was consumed. */
int  gtcaca_hexview_key(gtcaca_hexview_widget_t *h, int key, void *userdata);

/* ── data ──────────────────────────────────────────────────────────────────*/
void gtcaca_hexview_set_data(gtcaca_hexview_widget_t *h, const uint8_t *data, long len);
void gtcaca_hexview_set_read_cb(gtcaca_hexview_widget_t *h, gtcaca_hexview_read_cb cb,
                                void *userdata, long len);
long gtcaca_hexview_length(gtcaca_hexview_widget_t *h);

/* ── cursor and selection ──────────────────────────────────────────────────*/
int  gtcaca_hexview_cursor(gtcaca_hexview_widget_t *h);   /* -1 if no data */
void gtcaca_hexview_set_cursor(gtcaca_hexview_widget_t *h, long off);
/* Scroll by whole rows without touching the cursor — what a mouse wheel wants.
 * Positive scrolls down. Clamped so at least one row stays on screen. */
void gtcaca_hexview_scroll(gtcaca_hexview_widget_t *h, int rows);
long gtcaca_hexview_top(gtcaca_hexview_widget_t *h);
void gtcaca_hexview_set_top(gtcaca_hexview_widget_t *h, long row);
int  gtcaca_hexview_nibble(gtcaca_hexview_widget_t *h);
void gtcaca_hexview_set_nibble(gtcaca_hexview_widget_t *h, int low);
int  gtcaca_hexview_pane(gtcaca_hexview_widget_t *h);     /* 1 = ASCII pane */
void gtcaca_hexview_set_pane(gtcaca_hexview_widget_t *h, int ascii_pane);
void gtcaca_hexview_set_selection(gtcaca_hexview_widget_t *h, long anchor, long caret);
void gtcaca_hexview_clear_selection(gtcaca_hexview_widget_t *h);
/* 0 when there is no selection, else 1 with *lo/*hi filled (inclusive). */
int  gtcaca_hexview_selection(gtcaca_hexview_widget_t *h, long *lo, long *hi);

/* ── appearance ────────────────────────────────────────────────────────────*/
void gtcaca_hexview_set_title(gtcaca_hexview_widget_t *h, const char *title);
void gtcaca_hexview_set_bytes_per_row(gtcaca_hexview_widget_t *h, int n); /* 0 = auto */
int  gtcaca_hexview_bytes_per_row(gtcaca_hexview_widget_t *h);            /* effective */
void gtcaca_hexview_set_group_size(gtcaca_hexview_widget_t *h, int bytes);
void gtcaca_hexview_set_base_addr(gtcaca_hexview_widget_t *h, long base);
void gtcaca_hexview_set_addr_digits(gtcaca_hexview_widget_t *h, int digits); /* 0 = auto */
void gtcaca_hexview_set_show_ascii(gtcaca_hexview_widget_t *h, int on);
void gtcaca_hexview_set_box(gtcaca_hexview_widget_t *h, int on);

/* ── highlighting ──────────────────────────────────────────────────────────*/
void gtcaca_hexview_set_highlight(gtcaca_hexview_widget_t *h, long off, long len);
int  gtcaca_hexview_add_tag(gtcaca_hexview_widget_t *h, long off, long len,
                            uint16_t fg, uint16_t bg, const char *name);
void gtcaca_hexview_clear_tags(gtcaca_hexview_widget_t *h);
/* Name of the last tag covering `off`, or NULL. */
const char *gtcaca_hexview_tag_at(gtcaca_hexview_widget_t *h, long off);
void gtcaca_hexview_set_cell_cb(gtcaca_hexview_widget_t *h,
                                gtcaca_hexview_cell_cb cb, void *userdata);

/* ── editing ───────────────────────────────────────────────────────────────*/
void gtcaca_hexview_set_editable(gtcaca_hexview_widget_t *h, int on);
int  gtcaca_hexview_editable(gtcaca_hexview_widget_t *h);
void gtcaca_hexview_set_insert_mode(gtcaca_hexview_widget_t *h, int on);
int  gtcaca_hexview_insert_mode(gtcaca_hexview_widget_t *h);
void gtcaca_hexview_set_edit_cb(gtcaca_hexview_widget_t *h,
                                gtcaca_hexview_edit_cb cb, void *userdata);
void gtcaca_hexview_set_splice_cb(gtcaca_hexview_widget_t *h,
                                  gtcaca_hexview_splice_cb cb, void *userdata);

/* ── search ────────────────────────────────────────────────────────────────*/
/* Offset of `needle` at or after `from` (or at or before, when backwards), or
 * -1. Wrapping is the caller's business. */
long gtcaca_hexview_find(gtcaca_hexview_widget_t *h, const uint8_t *needle,
                         int nlen, long from, int backwards);

#endif /* _GTCACA_HEXVIEW_H_ */
