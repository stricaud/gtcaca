/*
 * GTCaca Login Form Demo
 *
 * Demonstrates: Entry (normal + secret/password), Button, Spinner,
 *               Frame, Separator, dynamic error dialog,
 *               fan-shrink animation on dialog dismiss.
 *
 * The correct password is "gtcaca".
 *
 * Controls:
 *   Tab        – next widget
 *   Arrow keys – navigate between widgets
 *   Enter      – activate focused button
 *   Q / Esc    – quit
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
#include <gtcaca/spinner.h>
#include <gtcaca/frame.h>
#include <gtcaca/separator.h>
#include <gtcaca/statusbar.h>

static gtcaca_window_widget_t    *g_win         = NULL;
static gtcaca_window_widget_t    *g_error_win   = NULL;
static gtcaca_window_widget_t    *g_success_win = NULL;
static gtcaca_statusbar_widget_t *g_statusbar   = NULL;
static gtcaca_spinner_widget_t   *g_spinner     = NULL;
static gtcaca_entry_widget_t     *g_user        = NULL;
static gtcaca_entry_widget_t     *g_pass        = NULL;
static gtcaca_button_widget_t    *g_login_btn   = NULL;

static int on_ok(gtcaca_button_widget_t *btn, int key, void *ud)
{
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  if (!g_error_win) return 0;

  gtcaca_window_close(g_error_win);
  g_error_win = NULL;
  gtcaca_window_set_focus(g_win);
  gtcaca_window_set_focused_child(g_win, GTCACA_WIDGET(g_login_btn));
  gtcaca_statusbar_set_text(g_statusbar, "Try again — password hint: it's the project name");
  return 0;
}

static void show_error_dialog(void)
{
  gtcaca_button_widget_t *ok_btn;
  int cw, ch, ew, eh, ex, ey;

  if (g_error_win) return;   /* already shown */

  ew = 44;
  eh = 10;  /* button at row 5: shadow at 8, border at 9 — no overlap */
  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);
  ex = (cw - ew) / 2;
  ey = (ch - eh) / 2;
  if (ex < 0) ex = 0;
  if (ey < 0) ey = 0;

  g_error_win = gtcaca_window_new(NULL, "Error", ex, ey, ew, eh);
  gtcaca_window_set_close_animation(g_error_win, GTCACA_WINDOW_ANIM_SHRINK,
                                    GTCACA_WIDGET(g_login_btn));

  gtcaca_label_new(GTCACA_WIDGET(g_error_win),
                   "The password is incorrect,", 2, 2);
  gtcaca_label_new(GTCACA_WIDGET(g_error_win),
                   "it must be 'gtcaca'", 2, 3);

  ok_btn = gtcaca_button_new(GTCACA_WIDGET(g_error_win),
                             "[ OK ]", ew / 2 - 5, 5);
  gtcaca_button_key_cb_register(ok_btn, on_ok);

  gtcaca_window_set_focus(g_error_win);
  gtcaca_window_set_focused_child(g_error_win, GTCACA_WIDGET(ok_btn));
}

static int on_ok_success(gtcaca_button_widget_t *btn, int key, void *ud)
{
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  if (!g_success_win) return 0;

  gtcaca_window_close(g_success_win);
  g_success_win = NULL;
  gtcaca_window_set_focus(g_win);
  gtcaca_window_set_focused_child(g_win, GTCACA_WIDGET(g_login_btn));
  gtcaca_statusbar_set_text(g_statusbar, "Logged in — press Q to quit");
  return 0;
}

static void show_success_dialog(const char *username)
{
  gtcaca_button_widget_t *ok_btn;
  char welcome[64];
  int cw, ch, ew, eh, ex, ey;

  if (g_success_win) return;

  snprintf(welcome, sizeof(welcome), "Welcome, %s!", username);

  ew = 44;
  eh = 10;  /* same rule: button at row 5, shadow at 8, border at 9 */
  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);
  ex = (cw - ew) / 2;
  ey = (ch - eh) / 2;
  if (ex < 0) ex = 0;
  if (ey < 0) ey = 0;

  g_success_win = gtcaca_window_new(NULL, "Success", ex, ey, ew, eh);

  gtcaca_label_new(GTCACA_WIDGET(g_success_win), "Login correct!", 2, 2);
  gtcaca_label_new(GTCACA_WIDGET(g_success_win), welcome, 2, 3);

  ok_btn = gtcaca_button_new(GTCACA_WIDGET(g_success_win),
                             "[ OK ]", ew / 2 - 5, 5);
  gtcaca_button_key_cb_register(ok_btn, on_ok_success);

  gtcaca_window_set_focus(g_success_win);
  gtcaca_window_set_focused_child(g_success_win, GTCACA_WIDGET(ok_btn));
}

static int on_login(gtcaca_button_widget_t *btn, int key, void *ud)
{
  const char *user, *pass;

  if (key != CACA_KEY_RETURN && key != ' ') return 0;

  user = gtcaca_entry_get_text(g_user);
  pass = gtcaca_entry_get_text(g_pass);

  if (!user || user[0] == '\0') {
    gtcaca_statusbar_set_text(g_statusbar, "Error: username is empty");
    return 0;
  }
  if (!pass || strcmp(pass, "gtcaca") != 0) {
    show_error_dialog();
    return 0;
  }

  show_success_dialog(user);
  return 0;
}

static int on_clear(gtcaca_button_widget_t *btn, int key, void *ud)
{
  if (key != CACA_KEY_RETURN && key != ' ') return 0;

  gtcaca_entry_set_text(g_user, "");
  gtcaca_entry_set_text(g_pass, "");
  gtcaca_spinner_set_spinning(g_spinner, 0);
  gtcaca_statusbar_set_text(g_statusbar, "Form cleared");
  return 0;
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_frame_widget_t       *frame;
  gtcaca_label_widget_t       *lbl;
  gtcaca_separator_widget_t   *sep;
  gtcaca_button_widget_t      *btn_clear;
  int cw, ch, ww, wh, wx, wy;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("Login Demo");

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);
  ww = 44;
  wh = 18;   /* tall enough for button shadow (buttons are visually 4 rows) */
  wx = (cw - ww) / 2;
  wy = (ch - wh) / 2;
  if (wx < 0) wx = 0;
  if (wy < 0) wy = 0;

  g_win = gtcaca_window_new(NULL, "Login", wx, wy, ww, wh);

  /* Credentials frame: rows 1–9 */
  frame  = gtcaca_frame_new(GTCACA_WIDGET(g_win), "Credentials", 1, 1, ww - 2, 9);

  lbl    = gtcaca_label_new(GTCACA_WIDGET(g_win), "Username:", 3, 3);
  g_user = gtcaca_entry_new(GTCACA_WIDGET(g_win), 14, 3, ww - 18);

  lbl    = gtcaca_label_new(GTCACA_WIDGET(g_win), "Password:", 3, 5);
  g_pass = gtcaca_entry_new(GTCACA_WIDGET(g_win), 14, 5, ww - 18);
  gtcaca_entry_set_secret(g_pass, 1);

  lbl        = gtcaca_label_new(GTCACA_WIDGET(g_win), "Status:",  3, 7);
  g_spinner  = gtcaca_spinner_new(GTCACA_WIDGET(g_win), 12, 7);

  /* Separator at row 10 */
  sep = gtcaca_separator_new(GTCACA_WIDGET(g_win), 1, 10, ww - 2, 0);

  /* Buttons at row 12; shadow lands at row 15, safely inside border at row 17 */
  g_login_btn = gtcaca_button_new(GTCACA_WIDGET(g_win), "[ Login ]", 3, 12);
  gtcaca_button_key_cb_register(g_login_btn, on_login);

  btn_clear = gtcaca_button_new(GTCACA_WIDGET(g_win), "[ Clear ]", 20, 12);
  gtcaca_button_key_cb_register(btn_clear, on_clear);

  g_statusbar = gtcaca_statusbar_new(
    "Tab: next | Enter: submit | Password: gtcaca | Q: quit");

  /* Enter from any field (e.g. the username/password entry) submits */
  gtcaca_window_set_default(g_win, GTCACA_WIDGET(g_login_btn));

  gtcaca_window_set_focused_child(g_win, GTCACA_WIDGET(g_user));
  gtcaca_main();
  return 0;
}
