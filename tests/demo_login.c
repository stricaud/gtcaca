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
#include <unistd.h>

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

/* Hide every widget that belongs to the error dialog. */
static void _dismiss_error_win(void)
{
  gtcaca_widget_t *w = NULL;
  if (!g_error_win) return;
  CDL_FOREACH(gmo.widgets_list, w) {
    if (w == GTCACA_WIDGET(g_error_win) ||
        w->parent == GTCACA_WIDGET(g_error_win))
      w->is_visible = 0;
  }
  g_error_win = NULL;
  gtcaca_window_set_focus(g_win);
  gtcaca_window_set_focused_child(g_win, GTCACA_WIDGET(g_login_btn));
}

/* Fan-shrink animation: the error dialog collapses toward the Login button. */
static void _animate_dismiss(int sx, int sy, int sw, int sh)
{
  /* Target: center of the Login button */
  int tx = g_login_btn->x + g_login_btn->width / 2;
  int ty = g_login_btn->y + 1;

  int steps = 16;
  int i;
  for (i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;

    /* Box shrinks to zero while its center tracks toward the button */
    int cw = (int)(sw * (1.0f - t));
    int ch = (int)(sh * (1.0f - t));
    int cx = (int)((sx + sw / 2) + ((tx) - (sx + sw / 2)) * t);
    int cy = (int)((sy + sh / 2) + ((ty) - (sy + sh / 2)) * t);
    int bx = cx - cw / 2;
    int by = cy - ch / 2;

    /* Redraw scene (dialog already invisible) */
    gtcaca_redraw();

    /* Draw the shrinking frame on top */
    if (cw >= 2 && ch >= 2) {
      caca_set_color_ansi(gmo.cv, CACA_YELLOW, CACA_RED);
      caca_fill_box(gmo.cv, bx, by, cw, ch, ' ');
      caca_draw_cp437_box(gmo.cv, bx, by, cw, ch);
    } else if (cw >= 1 && ch >= 1) {
      caca_set_color_ansi(gmo.cv, CACA_YELLOW, CACA_RED);
      caca_put_char(gmo.cv, bx, by, '*');
    }

    caca_refresh_display(gmo.dp);
    usleep(35000);   /* ~28 fps */
  }
}

static int on_ok(gtcaca_button_widget_t *btn, int key, void *ud)
{
  if (key != CACA_KEY_RETURN && key != ' ') return 0;

  if (!g_error_win) return 0;

  /* Capture dialog geometry before hiding */
  int sx = g_error_win->x;
  int sy = g_error_win->y;
  int sw = g_error_win->width;
  int sh = g_error_win->height;

  _dismiss_error_win();       /* hide widgets   */
  _animate_dismiss(sx, sy, sw, sh);  /* animate        */

  /* Final clean redraw */
  gtcaca_redraw();
  caca_refresh_display(gmo.dp);

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
  gtcaca_widget_t *w = NULL;
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  if (!g_success_win) return 0;
  CDL_FOREACH(gmo.widgets_list, w) {
    if (w == GTCACA_WIDGET(g_success_win) ||
        w->parent == GTCACA_WIDGET(g_success_win))
      w->is_visible = 0;
  }
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

  gtcaca_window_set_focused_child(g_win, GTCACA_WIDGET(g_user));
  gtcaca_main();
  return 0;
}
