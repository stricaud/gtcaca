#ifndef _GTCACA_HEXVIEW_H_
#define _GTCACA_HEXVIEW_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Hex/ASCII byte viewer (cf. Wireshark's packet bytes pane) ───────────────
 *
 * Renders a buffer as rows of "OFFSET  hh hh … hh  |ascii|" and can reverse-
 * highlight a byte range (e.g. the field selected in a detail tree). The buffer
 * is borrowed, not copied — keep it alive while the widget is shown. */

typedef struct _gtcaca_hexview_widget_t gtcaca_hexview_widget_t;
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

  const uint8_t *data;   /* borrowed buffer */
  int            len;
  int            top;    /* first visible row (16 bytes/row) */
  int            hl_off; /* highlight range start, -1 = none */
  int            hl_len;
  int            cursor; /* byte cursor offset (arrow keys); -1 = none */
  char          *title;
};

gtcaca_hexview_widget_t *gtcaca_hexview_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_hexview_set_data(gtcaca_hexview_widget_t *h, const uint8_t *data, int len);
void gtcaca_hexview_set_highlight(gtcaca_hexview_widget_t *h, int off, int len);
/* The current byte-cursor offset (moved by the arrow keys), or -1 if there is
 * no data. Lets an application map the cursor to a protocol field. */
int  gtcaca_hexview_cursor(gtcaca_hexview_widget_t *h);
void gtcaca_hexview_set_title(gtcaca_hexview_widget_t *h, const char *title);
void gtcaca_hexview_draw(gtcaca_hexview_widget_t *h);
/* Up/Down/PageUp/PageDown/Home/End scroll. Returns 1 if the key was consumed. */
int  gtcaca_hexview_key(gtcaca_hexview_widget_t *h, int key, void *userdata);
void gtcaca_hexview_free(gtcaca_hexview_widget_t *h);

#endif /* _GTCACA_HEXVIEW_H_ */
