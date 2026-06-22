/*
 * GTCaca Demo — shows all available widgets in a two-panel layout:
 *
 *  ┌─ Menu bar ────────────────────────────────────────────────────┐
 *  │ File  Edit  View  Help                                         │
 *  ├─ Settings ──────────┬─ System Status ────────────────────────┤
 *  │ Username: [       ] │ CPU:    [######      ]  60%             │
 *  │ Email:    [       ] │ Memory: [###         ]  35%             │
 *  │                     │ Disk:   [##########  ]  78%             │
 *  │ [x] Notifications   │                                         │
 *  │ [x] Auto-save       │ Network mode:                           │
 *  │ [ ] Dark mode       │  (*) Online                             │
 *  │                     │  ( ) Offline                            │
 *  │ Language:           │  ( ) Airplane                           │
 *  │ [English         v] │                                         │
 *  │                     │ Event log:                              │
 *  │  [  Apply  ]        │ ┌──────────────────────────────────┐    │
 *  │  [  Reset  ]        │ │ Application started.              │   │
 *  │                     │ │ Configuration loaded.             │   │
 *  │                     │ │ System ready.                     │   │
 *  │                     │ └──────────────────────────────────┘   │
 *  ├─────────────────────────────────────────────────────────────── │
 *  │ Ready | Tab: next widget | Ctrl+N/P: switch window | Q: quit  │
 *  └────────────────────────────────────────────────────────────────┘
 *
 *  Controls:
 *    Tab        – move focus to next widget in window
 *    Ctrl+N/P   – switch between windows
 *    F10        – open/close menu bar
 *    Arrow keys – navigate menus, lists, textview
 *    Space/Enter– toggle checkbox / select radio / open combobox
 *    Q/Esc      – quit (Esc also closes open menus/dropdowns)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/label.h>
#include <gtcaca/button.h>
#include <gtcaca/entry.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/radiobutton.h>
#include <gtcaca/progressbar.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/textview.h>
#include <gtcaca/combobox.h>
#include <gtcaca/menu.h>

/* Global status bar for callbacks to update */
static gtcaca_statusbar_widget_t *g_statusbar = NULL;
static gtcaca_textview_widget_t  *g_log = NULL;

/* ---------- Menu callbacks ---------- */

static void menu_new_cb(void *userdata)
{
  if (g_log) gtcaca_textview_append(g_log, "File > New triggered.");
  if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "New file.");
  gtcaca_redraw();
}

static void menu_open_cb(void *userdata)
{
  if (g_log) gtcaca_textview_append(g_log, "File > Open triggered.");
  if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "Open file...");
  gtcaca_redraw();
}

static void menu_save_cb(void *userdata)
{
  if (g_log) gtcaca_textview_append(g_log, "File > Save triggered.");
  if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "File saved.");
  gtcaca_redraw();
}

static void menu_prefs_cb(void *userdata)
{
  if (g_log) gtcaca_textview_append(g_log, "Edit > Preferences triggered.");
  if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "Preferences...");
  gtcaca_redraw();
}

static void menu_refresh_cb(void *userdata)
{
  if (g_log) gtcaca_textview_append(g_log, "View > Refresh triggered.");
  gtcaca_redraw();
}

static void menu_about_cb(void *userdata)
{
  if (g_log) gtcaca_textview_append(g_log, "GTCaca Demo v0.1  (libcaca-based UI)");
  if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "GTCaca Demo v0.1");
  gtcaca_redraw();
}

/* ---------- Widget callbacks ---------- */

static int apply_cb(gtcaca_button_widget_t *btn, int key, void *userdata)
{
  if (key == CACA_KEY_RETURN) {
    if (g_log) gtcaca_textview_append(g_log, "Settings applied.");
    if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "Settings applied.");
    gtcaca_redraw();
  }
  return 0;
}

static int reset_cb(gtcaca_button_widget_t *btn, int key, void *userdata)
{
  if (key == CACA_KEY_RETURN) {
    if (g_log) gtcaca_textview_append(g_log, "Settings reset to defaults.");
    if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, "Settings reset.");
    gtcaca_redraw();
  }
  return 0;
}

static int entry_cb(gtcaca_entry_widget_t *entry, int key, void *userdata)
{
  if (key == CACA_KEY_RETURN) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Entry: %s", gtcaca_entry_get_text(entry));
    if (g_log) gtcaca_textview_append(g_log, msg);
  }
  return 0;
}

static int checkbox_cb(gtcaca_checkbox_widget_t *cb, int key, void *userdata)
{
  if (key == ' ') {
    char msg[64];
    snprintf(msg, sizeof(msg), "Checkbox '%s': %s",
             cb->label ? cb->label : "?",
             cb->checked ? "checked" : "unchecked");
    if (g_log) gtcaca_textview_append(g_log, msg);
    if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, msg);
  }
  return 0;
}

static int combo_cb(gtcaca_combobox_widget_t *cb, int key, void *userdata)
{
  if (key == CACA_KEY_RETURN && !cb->is_open) {
    char msg[64];
    const char *sel = gtcaca_combobox_get_selected(cb);
    snprintf(msg, sizeof(msg), "Language selected: %s", sel ? sel : "none");
    if (g_log) gtcaca_textview_append(g_log, msg);
    if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, msg);
  }
  return 0;
}

static int radio_cb(gtcaca_radiobutton_widget_t *rb, int key, void *userdata)
{
  if ((key == ' ' || key == CACA_KEY_RETURN) && rb->active) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Network: %s selected.", rb->label ? rb->label : "?");
    if (g_log) gtcaca_textview_append(g_log, msg);
    if (g_statusbar) gtcaca_statusbar_set_text(g_statusbar, msg);
  }
  return 0;
}

/* ---------- main ---------- */

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_menu_widget_t        *menu;
  gtcaca_window_widget_t      *win_left, *win_right;
  gtcaca_label_widget_t       *lbl;
  gtcaca_entry_widget_t       *entry_name, *entry_email;
  gtcaca_checkbox_widget_t    *chk_notif, *chk_autosave, *chk_dark;
  gtcaca_combobox_widget_t    *combo_lang;
  gtcaca_button_widget_t      *btn_apply, *btn_reset;
  gtcaca_progressbar_widget_t *pb_cpu, *pb_mem, *pb_disk;
  gtcaca_radiobutton_widget_t *rb_online, *rb_offline, *rb_air;
  gtcaca_textview_widget_t    *tv_log;
  gtcaca_statusbar_widget_t   *statusbar;
  int canvas_w, canvas_h, left_w, right_w, content_h;
  int file_m, edit_m, view_m, help_m;

  /* ---- Init ---- */
  gtcaca_init(&argc, &argv);

  app = gtcaca_application_new("GTCaca Demo");

  canvas_w  = caca_get_canvas_width(gmo.cv);
  canvas_h  = caca_get_canvas_height(gmo.cv);
  left_w    = canvas_w / 2;
  right_w   = canvas_w - left_w;
  content_h = canvas_h - 2;   /* -1 menu row, -1 status row */

  /* ---- Menu bar (y=0) ---- */
  menu = gtcaca_menu_new();

  file_m = gtcaca_menu_add_entry(menu, "File");
  gtcaca_menu_add_item(menu, file_m, "New",       "Ctrl+N", menu_new_cb,  NULL);
  gtcaca_menu_add_item(menu, file_m, "Open...",   "Ctrl+O", menu_open_cb, NULL);
  gtcaca_menu_add_item(menu, file_m, "Save",      "Ctrl+S", menu_save_cb, NULL);
  gtcaca_menu_add_separator(menu, file_m);
  gtcaca_menu_add_item(menu, file_m, "Quit",      "Q",      NULL,         NULL);

  edit_m = gtcaca_menu_add_entry(menu, "Edit");
  gtcaca_menu_add_item(menu, edit_m, "Cut",       "Ctrl+X", NULL, NULL);
  gtcaca_menu_add_item(menu, edit_m, "Copy",      "Ctrl+C", NULL, NULL);
  gtcaca_menu_add_item(menu, edit_m, "Paste",     "Ctrl+V", NULL, NULL);
  gtcaca_menu_add_separator(menu, edit_m);
  gtcaca_menu_add_item(menu, edit_m, "Preferences...", "",  menu_prefs_cb, NULL);

  view_m = gtcaca_menu_add_entry(menu, "View");
  gtcaca_menu_add_item(menu, view_m, "Refresh",   "Ctrl+R", menu_refresh_cb, NULL);

  help_m = gtcaca_menu_add_entry(menu, "Help");
  gtcaca_menu_add_item(menu, help_m, "About",     "F1",     menu_about_cb, NULL);

  /* ---- Left window: Settings (x=0, y=1) ---- */
  win_left = gtcaca_window_new(NULL, "Settings", 0, 1, left_w, content_h);

  gtcaca_label_new(GTCACA_WIDGET(win_left), "Username:", 1, 2);
  entry_name = gtcaca_entry_new(GTCACA_WIDGET(win_left), 11, 2, left_w - 14);
  gtcaca_entry_set_text(entry_name, "jdoe");
  gtcaca_entry_key_cb_register(entry_name, entry_cb, NULL);

  gtcaca_label_new(GTCACA_WIDGET(win_left), "Email:   ", 1, 4);
  entry_email = gtcaca_entry_new(GTCACA_WIDGET(win_left), 11, 4, left_w - 14);
  gtcaca_entry_set_text(entry_email, "jdoe@example.com");
  gtcaca_entry_key_cb_register(entry_email, entry_cb, NULL);

  gtcaca_label_new(GTCACA_WIDGET(win_left), "Options:", 1, 6);
  chk_notif   = gtcaca_checkbox_new(GTCACA_WIDGET(win_left), "Notifications", 2, 7);
  gtcaca_checkbox_set_checked(chk_notif, 1);
  gtcaca_checkbox_key_cb_register(chk_notif, checkbox_cb, NULL);

  chk_autosave = gtcaca_checkbox_new(GTCACA_WIDGET(win_left), "Auto-save", 2, 8);
  gtcaca_checkbox_set_checked(chk_autosave, 1);
  gtcaca_checkbox_key_cb_register(chk_autosave, checkbox_cb, NULL);

  chk_dark = gtcaca_checkbox_new(GTCACA_WIDGET(win_left), "Dark mode", 2, 9);
  gtcaca_checkbox_key_cb_register(chk_dark, checkbox_cb, NULL);

  gtcaca_label_new(GTCACA_WIDGET(win_left), "Language:", 1, 11);
  combo_lang = gtcaca_combobox_new(GTCACA_WIDGET(win_left), 2, 12, left_w - 5);
  gtcaca_combobox_append(combo_lang, "English");
  gtcaca_combobox_append(combo_lang, "Francais");
  gtcaca_combobox_append(combo_lang, "Deutsch");
  gtcaca_combobox_append(combo_lang, "Espanol");
  gtcaca_combobox_append(combo_lang, "Italiano");
  gtcaca_combobox_key_cb_register(combo_lang, combo_cb, NULL);

  btn_apply = gtcaca_button_new(GTCACA_WIDGET(win_left), "  Apply  ", 2, 14);
  gtcaca_button_key_cb_register(btn_apply, apply_cb);

  btn_reset = gtcaca_button_new(GTCACA_WIDGET(win_left), "  Reset  ", 2, 17);
  gtcaca_button_key_cb_register(btn_reset, reset_cb);

  /* Set initial focus to username entry */
  gtcaca_window_set_focused_child(win_left, GTCACA_WIDGET(entry_name));

  /* ---- Right window: System Status (x=left_w, y=1) ---- */
  win_right = gtcaca_window_new(NULL, "System Status", left_w, 1, right_w, content_h);

  gtcaca_label_new(GTCACA_WIDGET(win_right), "CPU Usage:", 1, 2);
  pb_cpu = gtcaca_progressbar_new(GTCACA_WIDGET(win_right), 2, 3, right_w - 6);
  gtcaca_progressbar_set_value(pb_cpu, 0.60f);

  gtcaca_label_new(GTCACA_WIDGET(win_right), "Memory:   ", 1, 5);
  pb_mem = gtcaca_progressbar_new(GTCACA_WIDGET(win_right), 2, 6, right_w - 6);
  gtcaca_progressbar_set_value(pb_mem, 0.35f);

  gtcaca_label_new(GTCACA_WIDGET(win_right), "Disk:     ", 1, 8);
  pb_disk = gtcaca_progressbar_new(GTCACA_WIDGET(win_right), 2, 9, right_w - 6);
  gtcaca_progressbar_set_value(pb_disk, 0.78f);

  gtcaca_label_new(GTCACA_WIDGET(win_right), "Network mode:", 1, 11);

  rb_online  = gtcaca_radiobutton_new(GTCACA_WIDGET(win_right), "Online",   1, 2, 12);
  rb_offline = gtcaca_radiobutton_new(GTCACA_WIDGET(win_right), "Offline",  1, 2, 13);
  rb_air     = gtcaca_radiobutton_new(GTCACA_WIDGET(win_right), "Airplane", 1, 2, 14);
  gtcaca_radiobutton_set_active(rb_online);

  gtcaca_radiobutton_key_cb_register(rb_online,  radio_cb, NULL);
  gtcaca_radiobutton_key_cb_register(rb_offline, radio_cb, NULL);
  gtcaca_radiobutton_key_cb_register(rb_air,     radio_cb, NULL);

  gtcaca_label_new(GTCACA_WIDGET(win_right), "Event log:", 1, 16);

  tv_log = gtcaca_textview_new(GTCACA_WIDGET(win_right), 2, 17,
                               right_w - 5, content_h - 19);
  g_log = tv_log;

  gtcaca_textview_append(tv_log, "Application started.");
  gtcaca_textview_append(tv_log, "Configuration loaded.");
  gtcaca_textview_append(tv_log, "Theme applied.");
  gtcaca_textview_append(tv_log, "Widgets initialized.");
  gtcaca_textview_append(tv_log, "System ready.  Use Tab to navigate.");

  /* Set initial focus to online radio */
  gtcaca_window_set_focused_child(win_right, GTCACA_WIDGET(rb_online));

  /* ---- Status bar (bottom row) ---- */
  statusbar = gtcaca_statusbar_new(
    "Ready | Tab: next widget | Ctrl+N/P: switch window | F10: menu | Esc: quit");
  g_statusbar = statusbar;

  /* Start with the Settings window and username entry focused */
  gtcaca_window_set_focus(win_left);

  /* ---- Run ---- */
  gtcaca_main();

  return 0;
}
