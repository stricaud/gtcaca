#ifndef _GTCACA_MENU_H_
#define _GTCACA_MENU_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

#define GTCACA_MENU_MAX_ENTRIES 8
#define GTCACA_MENU_MAX_ITEMS   20
#define GTCACA_MENU_LABEL_MAX   32
#define GTCACA_MENU_SHORTCUT_MAX 12

typedef void (*gtcaca_menu_action_t)(void *userdata);

typedef struct {
  char label[GTCACA_MENU_LABEL_MAX];
  char shortcut[GTCACA_MENU_SHORTCUT_MAX];
  int is_separator;
  int enabled;                 /* 0 = greyed out and not selectable (default 1) */
  gtcaca_menu_action_t action;
  void *userdata;
} gtcaca_menu_item_t;

typedef struct {
  char title[GTCACA_MENU_LABEL_MAX];
  gtcaca_menu_item_t items[GTCACA_MENU_MAX_ITEMS];
  int n_items;
  int x_pos;
} gtcaca_menu_entry_t;

typedef struct _gtcaca_menu_widget_t gtcaca_menu_widget_t;

struct _gtcaca_menu_widget_t {
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
  struct _gtcaca_menu_widget_t *prev;
  struct _gtcaca_menu_widget_t *next;

  gtcaca_menu_entry_t entries[GTCACA_MENU_MAX_ENTRIES];
  int n_entries;
  int active_entry;
  int active_item;
  int is_open;
};

gtcaca_menu_widget_t *gtcaca_menu_new(void);
void gtcaca_menu_draw(gtcaca_menu_widget_t *menu);
int gtcaca_menu_add_entry(gtcaca_menu_widget_t *menu, const char *title);
int gtcaca_menu_add_item(gtcaca_menu_widget_t *menu, int entry_idx, const char *label,
                         const char *shortcut, gtcaca_menu_action_t action, void *userdata);
int gtcaca_menu_add_separator(gtcaca_menu_widget_t *menu, int entry_idx);
int gtcaca_menu_handle_key(gtcaca_menu_widget_t *menu, int key);

/* Give (or remove) keyboard focus to the menu bar. While focused, the menu bar
 * highlights its active entry and `gtcaca_menu_handle_key` navigates/opens it;
 * removing focus also closes any open dropdown. This lets an application drive
 * the menu (e.g. from an F9/F10 handler) without reaching into the struct. */
void gtcaca_menu_set_focus(gtcaca_menu_widget_t *menu, int on);
int  gtcaca_menu_is_focused(gtcaca_menu_widget_t *menu);

/* Enable or disable a menu item. A disabled item is drawn greyed out, skipped
 * by keyboard navigation, and its action is not run if somehow reached. */
void gtcaca_menu_set_item_enabled(gtcaca_menu_widget_t *menu, int entry_idx,
                                  int item_idx, int enabled);

#endif /* _GTCACA_MENU_H_ */
