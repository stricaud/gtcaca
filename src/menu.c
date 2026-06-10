#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/menu.h>
#include <gtcaca/main.h>

static int _skip_separators_fwd(gtcaca_menu_entry_t *entry, int start)
{
  int i = start;
  while (i < entry->n_items && entry->items[i].is_separator) i++;
  if (i >= entry->n_items) {
    i = 0;
    while (i < entry->n_items && entry->items[i].is_separator) i++;
  }
  return i;
}

static int _skip_separators_bwd(gtcaca_menu_entry_t *entry, int start)
{
  int i = start;
  while (i >= 0 && entry->items[i].is_separator) i--;
  if (i < 0) {
    i = entry->n_items - 1;
    while (i >= 0 && entry->items[i].is_separator) i--;
  }
  return (i < 0) ? 0 : i;
}

static int _gtcaca_menu_private_key_press(gtcaca_menu_widget_t *menu, int key, void *userdata)
{
  if (key == CACA_KEY_ESCAPE) {
    menu->is_open = 0;
    menu->has_focus = 0;
    return 0;
  }

  if (!menu->is_open) {
    if (key == CACA_KEY_RIGHT) {
      menu->active_entry = (menu->active_entry + 1) % menu->n_entries;
    } else if (key == CACA_KEY_LEFT) {
      menu->active_entry = (menu->active_entry - 1 + menu->n_entries) % menu->n_entries;
    } else if (key == CACA_KEY_DOWN || key == CACA_KEY_RETURN) {
      menu->is_open = 1;
      menu->active_item = _skip_separators_fwd(&menu->entries[menu->active_entry], 0);
    }
  } else {
    gtcaca_menu_entry_t *entry = &menu->entries[menu->active_entry];
    if (key == CACA_KEY_DOWN) {
      menu->active_item = _skip_separators_fwd(entry, menu->active_item + 1);
    } else if (key == CACA_KEY_UP) {
      menu->active_item = _skip_separators_bwd(entry, menu->active_item - 1);
    } else if (key == CACA_KEY_RIGHT) {
      menu->active_entry = (menu->active_entry + 1) % menu->n_entries;
      menu->active_item = _skip_separators_fwd(&menu->entries[menu->active_entry], 0);
    } else if (key == CACA_KEY_LEFT) {
      menu->active_entry = (menu->active_entry - 1 + menu->n_entries) % menu->n_entries;
      menu->active_item = _skip_separators_fwd(&menu->entries[menu->active_entry], 0);
    } else if (key == CACA_KEY_RETURN) {
      gtcaca_menu_item_t *item = &entry->items[menu->active_item];
      if (item->action)
        item->action(item->userdata);
      menu->is_open = 0;
      menu->has_focus = 0;
    }
  }
  return 0;
}

gtcaca_menu_widget_t *gtcaca_menu_new(void)
{
  gtcaca_menu_widget_t *menu;

  menu = malloc(sizeof(gtcaca_menu_widget_t));
  if (!menu) {
    fprintf(stderr, "Error: Cannot allocate menu\n");
    return NULL;
  }

  menu->id = gtcaca_get_newid();
  menu->has_focus = 0;
  menu->is_visible = 1;
  menu->type = GTCACA_WIDGET_MENU;
  menu->parent = NULL;
  menu->children = NULL;

  menu->x = 0;
  menu->y = 0;
  menu->width = caca_get_canvas_width(gmo.cv);
  menu->height = 1;

  menu->color_focus_fg = gmo.theme.menuitemfocus.fg;
  menu->color_focus_bg = gmo.theme.menuitemfocus.bg;
  menu->color_nonfocus_fg = gmo.theme.menu.fg;
  menu->color_nonfocus_bg = gmo.theme.menu.bg;

  menu->n_entries = 0;
  menu->active_entry = 0;
  menu->active_item = 0;
  menu->is_open = 0;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(menu));

  return menu;
}

void gtcaca_menu_draw(gtcaca_menu_widget_t *menu)
{
  int i, x_pos, title_len;
  int canvas_w = caca_get_canvas_width(gmo.cv);

  menu->width = canvas_w;

  /* Menu bar background */
  caca_set_color_ansi(gmo.cv, gmo.theme.menu.fg, gmo.theme.menu.bg);
  caca_fill_box(gmo.cv, menu->x, menu->y, menu->width, 1, ' ');

  /* Draw menu titles */
  x_pos = 1;
  for (i = 0; i < menu->n_entries; i++) {
    menu->entries[i].x_pos = x_pos;
    title_len = (int)strlen(menu->entries[i].title);

    if (menu->has_focus && i == menu->active_entry) {
      caca_set_color_ansi(gmo.cv, gmo.theme.menuitemfocus.fg, gmo.theme.menuitemfocus.bg);
    } else {
      caca_set_color_ansi(gmo.cv, gmo.theme.menu.fg, gmo.theme.menu.bg);
    }
    caca_printf(gmo.cv, menu->x + x_pos, menu->y, " %s ", menu->entries[i].title);
    x_pos += title_len + 2;
  }

  /* Draw open dropdown */
  if (menu->is_open && menu->active_entry >= 0 && menu->active_entry < menu->n_entries) {
    gtcaca_menu_entry_t *entry = &menu->entries[menu->active_entry];
    int n_items = entry->n_items;
    int dropdown_x = menu->x + entry->x_pos;
    int dropdown_y = menu->y + 1;
    int dropdown_w = 22;
    int j;

    /* Find max label width */
    for (j = 0; j < n_items; j++) {
      int sc_len = (int)strlen(entry->items[j].shortcut);
      int lbl_len = (int)strlen(entry->items[j].label);
      int needed = lbl_len + (sc_len > 0 ? sc_len + 2 : 0) + 3;
      if (needed > dropdown_w) dropdown_w = needed;
    }

    caca_set_color_ansi(gmo.cv, gmo.theme.menuitem.fg, gmo.theme.menuitem.bg);
    caca_fill_box(gmo.cv, dropdown_x, dropdown_y, dropdown_w, n_items + 2, ' ');
    caca_draw_cp437_box(gmo.cv, dropdown_x, dropdown_y, dropdown_w, n_items + 2);

    for (j = 0; j < n_items; j++) {
      gtcaca_menu_item_t *item = &entry->items[j];
      int item_y = dropdown_y + 1 + j;

      if (item->is_separator) {
        int k;
        caca_set_color_ansi(gmo.cv, gmo.theme.menuitem.fg, gmo.theme.menuitem.bg);
        for (k = 1; k < dropdown_w - 1; k++)
          caca_put_char(gmo.cv, dropdown_x + k, item_y, '-');
      } else {
        if (j == menu->active_item) {
          caca_set_color_ansi(gmo.cv, gmo.theme.menuitemfocus.fg, gmo.theme.menuitemfocus.bg);
        } else {
          caca_set_color_ansi(gmo.cv, gmo.theme.menuitem.fg, gmo.theme.menuitem.bg);
        }
        /* Clear the line */
        int k;
        for (k = 1; k < dropdown_w - 1; k++)
          caca_put_char(gmo.cv, dropdown_x + k, item_y, ' ');
        /* Label */
        caca_printf(gmo.cv, dropdown_x + 2, item_y, "%s", item->label);
        /* Shortcut right-aligned */
        if (strlen(item->shortcut) > 0) {
          int sc_x = dropdown_x + dropdown_w - (int)strlen(item->shortcut) - 2;
          caca_printf(gmo.cv, sc_x, item_y, "%s", item->shortcut);
        }
      }
    }
  }
}

int gtcaca_menu_add_entry(gtcaca_menu_widget_t *menu, const char *title)
{
  int idx;
  if (menu->n_entries >= GTCACA_MENU_MAX_ENTRIES) return -1;
  idx = menu->n_entries++;
  strncpy(menu->entries[idx].title, title, GTCACA_MENU_LABEL_MAX - 1);
  menu->entries[idx].title[GTCACA_MENU_LABEL_MAX - 1] = '\0';
  menu->entries[idx].n_items = 0;
  menu->entries[idx].x_pos = 0;
  return idx;
}

int gtcaca_menu_add_item(gtcaca_menu_widget_t *menu, int entry_idx, const char *label,
                         const char *shortcut, gtcaca_menu_action_t action, void *userdata)
{
  gtcaca_menu_entry_t *entry;
  gtcaca_menu_item_t *item;
  int idx;

  if (entry_idx < 0 || entry_idx >= menu->n_entries) return -1;
  entry = &menu->entries[entry_idx];
  if (entry->n_items >= GTCACA_MENU_MAX_ITEMS) return -1;

  idx = entry->n_items++;
  item = &entry->items[idx];
  strncpy(item->label, label, GTCACA_MENU_LABEL_MAX - 1);
  item->label[GTCACA_MENU_LABEL_MAX - 1] = '\0';
  if (shortcut) {
    strncpy(item->shortcut, shortcut, GTCACA_MENU_SHORTCUT_MAX - 1);
    item->shortcut[GTCACA_MENU_SHORTCUT_MAX - 1] = '\0';
  } else {
    item->shortcut[0] = '\0';
  }
  item->is_separator = 0;
  item->action = action;
  item->userdata = userdata;
  return idx;
}

int gtcaca_menu_add_separator(gtcaca_menu_widget_t *menu, int entry_idx)
{
  gtcaca_menu_entry_t *entry;
  gtcaca_menu_item_t *item;
  int idx;

  if (entry_idx < 0 || entry_idx >= menu->n_entries) return -1;
  entry = &menu->entries[entry_idx];
  if (entry->n_items >= GTCACA_MENU_MAX_ITEMS) return -1;

  idx = entry->n_items++;
  item = &entry->items[idx];
  item->label[0] = '\0';
  item->shortcut[0] = '\0';
  item->is_separator = 1;
  item->action = NULL;
  item->userdata = NULL;
  return idx;
}

/* Called from main.c key dispatch to expose private key handler */
int gtcaca_menu_handle_key(gtcaca_menu_widget_t *menu, int key)
{
  return _gtcaca_menu_private_key_press(menu, key, NULL);
}
