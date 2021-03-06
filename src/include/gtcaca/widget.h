#ifndef _GTCACA_WIDGET_H_
#define _GTCACA_WIDGET_H_

#include <stdint.h>

#define GTCACA_WIDGET(x) ((gtcaca_widget_t *)x)

enum _gtcaca_widget_type_t {
  GTCACA_WIDGET_APPLICATION,
  GTCACA_WIDGET_WINDOW,
  GTCACA_WIDGET_TEXTLIST,
  GTCACA_WIDGET_BUTTON,
  GTCACA_WIDGET_CALENDAR,
  GTCACA_WIDGET_DIALOG,
  GTCACA_WIDGET_ENTRY,
  GTCACA_WIDGET_FILECHOOSERDIALOG,
  GTCACA_WIDGET_IMAGE,
  GTCACA_WIDGET_LABEL,
  GTCACA_WIDGET_MENU,
  GTCACA_WIDGET_PROGRESSBAR,
  GTCACA_WIDGET_RADIOBUTTON,
  GTCACA_WIDGET_STATUSBAR,
  GTCACA_WIDGET_TEXTVIEW,
};
typedef enum _gtcaca_widget_type_t gtcaca_widget_type_t;

/* This MUST be consistent with the preamble of every widget */
struct _gtcaca_widget_t {
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
  
  struct _gtcaca_widget_t *parent;
  struct _gtcaca_widget_t *children; // Any widget can be a children
  
  struct _gtcaca_widget_t *prev;
  struct _gtcaca_widget_t *next;
};
typedef struct _gtcaca_widget_t gtcaca_widget_t;

void gtcaca_widget_debug(gtcaca_widget_t *widget);
void gtcaca_widget_position_size_parent(gtcaca_widget_t *parent, gtcaca_widget_t *widget, int x, int y);
void gtcaca_widget_printall();
void gtcaca_widget_colorize_from_parent(gtcaca_widget_t *widget);
void gtcaca_widget_colorize(gtcaca_widget_t *widget);

#endif // _GTCACA_WIDGET_H_
