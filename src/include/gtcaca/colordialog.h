#ifndef _GTCACA_COLORDIALOG_H_
#define _GTCACA_COLORDIALOG_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Colour picker dialog (≈ GtkColorDialog) ─────────────────────────────────
 *
 * A centred modal that shows libcaca's 16 ANSI colours as a swatch grid. Use it
 * as a widget (gtcaca_colordialog_new + draw), or — more commonly — through the
 * blocking helper that runs its own loop and returns the chosen colour index
 * (CACA_BLACK … CACA_WHITE, 0–15) or -1 when cancelled. */

#define GTCACA_COLORDIALOG_ONGOING (-100)   /* result while still open */

typedef struct _gtcaca_colordialog_widget_t gtcaca_colordialog_widget_t;
struct _gtcaca_colordialog_widget_t {
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
  struct _gtcaca_colordialog_widget_t *prev;
  struct _gtcaca_colordialog_widget_t *next;

  char *title;
  int   sel;       /* highlighted swatch, 0–15                          */
  int   result;    /* GTCACA_COLORDIALOG_ONGOING, a colour index, or -1 */
};

gtcaca_colordialog_widget_t *gtcaca_colordialog_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_colordialog_set_title(gtcaca_colordialog_widget_t *d, const char *title);
void gtcaca_colordialog_draw(gtcaca_colordialog_widget_t *d);
int  gtcaca_colordialog_key(gtcaca_colordialog_widget_t *d, int key, void *userdata);
int  gtcaca_colordialog_result(gtcaca_colordialog_widget_t *d);
void gtcaca_colordialog_free(gtcaca_colordialog_widget_t *d);

/* Name of an ANSI colour index (0–15), e.g. "Light Cyan"; "?" if out of range. */
const char *gtcaca_color_name(int idx);

/* Blocking helper (needs an initialised display). `initial` (0–15) pre-selects a
   swatch; pass -1 for none. Returns the chosen colour index or -1 if cancelled. */
int gtcaca_colordialog_run(const char *title, int initial);

#endif /* _GTCACA_COLORDIALOG_H_ */
