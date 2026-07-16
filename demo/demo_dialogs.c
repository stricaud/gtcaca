/*
 * GTCaca Dialogs Demo
 *
 * A launcher for the dialog family — the TUI counterparts of GTK4's dialog
 * widgets (https://docs.gtk.org/gtk4/section-dialogs.html):
 *
 *   Colour picker   – gtcaca_colordialog_run   (≈ GtkColorDialog)
 *   Message / OK     – gtcaca_dialog_message    (≈ GtkAlertDialog)
 *   Confirm          – gtcaca_dialog_confirm    (≈ GtkAlertDialog)
 *   Open / Save file – gtcaca_filechooser_run   (≈ GtkFileDialog)
 *
 * Each button opens a modal dialog and reports its result in the status bar.
 * The colour swatch below the buttons updates to the colour you pick.
 *
 * Controls:
 *   Tab / Arrows – move between buttons
 *   Enter/Space  – activate focused button
 *   Q / Esc      – quit
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
#include <gtcaca/frame.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/colordialog.h>
#include <gtcaca/dialog.h>
#include <gtcaca/filechooser.h>

static gtcaca_statusbar_widget_t *g_status = NULL;
static gtcaca_label_widget_t     *g_swatch = NULL;
static int                        g_color  = CACA_CYAN;

static uint8_t contrast_fg(int color)
{
  switch (color) {
  case CACA_LIGHTGRAY: case CACA_LIGHTGREEN: case CACA_LIGHTCYAN:
  case CACA_YELLOW:     case CACA_WHITE:
    return CACA_BLACK;
  default:
    return CACA_WHITE;
  }
}

static void set_label(gtcaca_label_widget_t *l, const char *text)
{
  if (!l) return;
  free(l->label);
  l->label = strdup(text ? text : "");
}

static int on_color(gtcaca_button_widget_t *btn, int key, void *ud)
{
  int picked;
  char msg[64];
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;

  picked = gtcaca_colordialog_run("Pick a colour", g_color);
  if (picked < 0) { gtcaca_statusbar_set_text(g_status, "Colour: cancelled"); return 0; }

  g_color = picked;
  set_label(g_swatch, gtcaca_color_name(picked));
  g_swatch->color_focus_bg = g_swatch->color_nonfocus_bg = (uint8_t)picked;
  g_swatch->color_focus_fg = g_swatch->color_nonfocus_fg = contrast_fg(picked);
  snprintf(msg, sizeof msg, "Colour: %d (%s)", picked, gtcaca_color_name(picked));
  gtcaca_statusbar_set_text(g_status, msg);
  return 0;
}

static int on_message(gtcaca_button_widget_t *btn, int key, void *ud)
{
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  gtcaca_dialog_message("Information", "This is an alert dialog.\nPress OK to dismiss it.");
  gtcaca_statusbar_set_text(g_status, "Message dialog dismissed");
  return 0;
}

static int on_confirm(gtcaca_button_widget_t *btn, int key, void *ud)
{
  int ok;
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  ok = gtcaca_dialog_confirm("Confirm", "Do you want to proceed?");
  gtcaca_statusbar_set_text(g_status, ok ? "Confirmed: OK" : "Confirmed: Cancel");
  return 0;
}

static int on_open(gtcaca_button_widget_t *btn, int key, void *ud)
{
  char path[1024], msg[1100];
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  if (gtcaca_filechooser_run(NULL, path, sizeof path, 0) == 1) {
    snprintf(msg, sizeof msg, "Open: %s", path);
    gtcaca_statusbar_set_text(g_status, msg);
  } else {
    gtcaca_statusbar_set_text(g_status, "Open: cancelled");
  }
  return 0;
}

static int on_save(gtcaca_button_widget_t *btn, int key, void *ud)
{
  char path[1024], msg[1100];
  (void)btn; (void)ud;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  if (gtcaca_filechooser_run(NULL, path, sizeof path, 1) == 1) {
    snprintf(msg, sizeof msg, "Save: %s", path);
    gtcaca_statusbar_set_text(g_status, msg);
  } else {
    gtcaca_statusbar_set_text(g_status, "Save: cancelled");
  }
  return 0;
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t      *win;
  gtcaca_button_widget_t      *b_color, *b_msg, *b_confirm, *b_open, *b_save;
  int cw, ch, ww = 40, wh = 18, wx, wy;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("Dialogs Demo");
  (void)app;

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);
  wx = (cw - ww) / 2; if (wx < 0) wx = 0;
  wy = (ch - wh) / 2; if (wy < 0) wy = 0;

  win = gtcaca_window_new(NULL, "Dialogs", wx, wy, ww, wh);

  gtcaca_frame_new(GTCACA_WIDGET(win), "Open a dialog", 1, 1, ww - 2, wh - 5);

  b_color   = gtcaca_button_new(GTCACA_WIDGET(win), "[ Pick Colour ]", 3, 3);
  gtcaca_button_key_cb_register(b_color, on_color);
  b_msg     = gtcaca_button_new(GTCACA_WIDGET(win), "[ Message ]",     20, 3);
  gtcaca_button_key_cb_register(b_msg, on_message);

  b_confirm = gtcaca_button_new(GTCACA_WIDGET(win), "[ Confirm ]",      3, 6);
  gtcaca_button_key_cb_register(b_confirm, on_confirm);
  b_open    = gtcaca_button_new(GTCACA_WIDGET(win), "[ Open File ]",   20, 6);
  gtcaca_button_key_cb_register(b_open, on_open);

  b_save    = gtcaca_button_new(GTCACA_WIDGET(win), "[ Save File ]",    3, 9);
  gtcaca_button_key_cb_register(b_save, on_save);

  gtcaca_label_new(GTCACA_WIDGET(win), "Colour:", 3, wh - 3);
  g_swatch = gtcaca_label_new(GTCACA_WIDGET(win), gtcaca_color_name(g_color), 11, wh - 3);
  g_swatch->color_focus_bg = g_swatch->color_nonfocus_bg = (uint8_t)g_color;
  g_swatch->color_focus_fg = g_swatch->color_nonfocus_fg = contrast_fg(g_color);

  g_status = gtcaca_statusbar_new("Tab: move | Enter: open dialog | Q: quit");

  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(b_color));
  gtcaca_main();
  return 0;
}
