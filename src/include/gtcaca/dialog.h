#ifndef _GTCACA_DIALOG_H_
#define _GTCACA_DIALOG_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Modal dialog (message / confirm / choice) ───────────────────────────────
 *
 * A centred box with a message and a row of buttons. Use it as a widget
 * (gtcaca_dialog_new + draw/key), or — more commonly — through the blocking
 * helpers that run their own loop and return the chosen button index. */

#define GTCACA_DIALOG_ONGOING (-100)   /* result while still open */

typedef struct _gtcaca_dialog_widget_t gtcaca_dialog_widget_t;
struct _gtcaca_dialog_widget_t {
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
  struct _gtcaca_dialog_widget_t *prev;
  struct _gtcaca_dialog_widget_t *next;

  char  *title;
  char  *message;
  char **buttons;
  int    nbuttons;
  int    sel;       /* highlighted button             */
  int    result;    /* GTCACA_DIALOG_ONGOING, an index, or -1 (cancelled) */
  uint8_t sel_fg, sel_bg;
};

gtcaca_dialog_widget_t *gtcaca_dialog_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_dialog_set(gtcaca_dialog_widget_t *d, const char *title, const char *message,
                       const char **buttons, int nbuttons);
void gtcaca_dialog_draw(gtcaca_dialog_widget_t *d);
int  gtcaca_dialog_key(gtcaca_dialog_widget_t *d, int key, void *userdata);
int  gtcaca_dialog_result(gtcaca_dialog_widget_t *d);
void gtcaca_dialog_free(gtcaca_dialog_widget_t *d);

/* blocking helpers (need an initialised display) */
int  gtcaca_dialog_run(const char *title, const char *message, const char **buttons, int nbuttons);
int  gtcaca_dialog_confirm(const char *title, const char *message);  /* OK/Cancel -> 1/0 */
void gtcaca_dialog_message(const char *title, const char *message);  /* single OK */

#endif /* _GTCACA_DIALOG_H_ */
