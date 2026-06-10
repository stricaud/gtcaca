/*
 * GTCaca New Widgets Demo
 *
 * Shows: Scale, SpinButton, Switch, Spinner, Frame, Separator, Expander,
 *        PasswordEntry, StatusBar
 *
 * Controls:
 *   Tab        – next widget
 *   Arrow keys – navigate / adjust values
 *   Space/Enter– toggle switch, expander
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
#include <gtcaca/entry.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/spinner.h>
#include <gtcaca/scale.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/switch.h>
#include <gtcaca/frame.h>
#include <gtcaca/separator.h>
#include <gtcaca/expander.h>

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t      *win;
  gtcaca_label_widget_t       *lbl;
  gtcaca_scale_widget_t       *scale;
  gtcaca_spinbutton_widget_t  *spinbtn;
  gtcaca_switch_widget_t      *sw;
  gtcaca_spinner_widget_t     *spinner;
  gtcaca_frame_widget_t       *frame;
  gtcaca_checkbox_widget_t    *chk1, *chk2;
  gtcaca_separator_widget_t   *sep;
  gtcaca_expander_widget_t    *expander;
  gtcaca_label_widget_t       *exp_lbl1, *exp_lbl2;
  gtcaca_entry_widget_t       *passentry;
  gtcaca_statusbar_widget_t   *statusbar;

  int canvas_w, canvas_h, win_w, win_h;

  gtcaca_init(&argc, &argv);

  app = gtcaca_application_new("New Widgets Demo");

  canvas_w = caca_get_canvas_width(gmo.cv);
  canvas_h = caca_get_canvas_height(gmo.cv);
  win_w    = canvas_w > 60 ? 60 : canvas_w;
  win_h    = canvas_h > 28 ? 28 : canvas_h;

  win = gtcaca_window_new(NULL, "New Widgets Demo", 1, 1, win_w, win_h);

  /* --- Scale --- */
  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Volume:", 2, 2);
  scale = gtcaca_scale_new(GTCACA_WIDGET(win), 11, 2, win_w - 20, 0.0, 100.0, 1.0);
  gtcaca_scale_set_value(scale, 50.0);

  /* --- SpinButton --- */
  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Count:", 2, 4);
  spinbtn = gtcaca_spinbutton_new(GTCACA_WIDGET(win), 11, 4, 0.0, 99.0, 1.0);
  gtcaca_spinbutton_set_value(spinbtn, 5.0);

  /* --- Switch --- */
  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Feature:", 2, 6);
  sw = gtcaca_switch_new(GTCACA_WIDGET(win), 11, 6);
  gtcaca_switch_set_active(sw, 1);

  /* --- Spinner --- */
  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Loading:", 2, 8);
  spinner = gtcaca_spinner_new(GTCACA_WIDGET(win), 11, 8);
  gtcaca_spinner_set_spinning(spinner, 1);

  /* --- Frame containing two checkboxes --- */
  frame = gtcaca_frame_new(GTCACA_WIDGET(win), "Options", 2, 10, 24, 4);
  chk1 = gtcaca_checkbox_new(GTCACA_WIDGET(win), "Auto-connect", 4, 11);
  gtcaca_checkbox_set_checked(chk1, 1);
  chk2 = gtcaca_checkbox_new(GTCACA_WIDGET(win), "Dark mode", 4, 12);

  /* --- Horizontal Separator --- */
  sep = gtcaca_separator_new(GTCACA_WIDGET(win), 2, 15, win_w - 6, 0);

  /* --- Expander with two labels --- */
  expander = gtcaca_expander_new(GTCACA_WIDGET(win), "Advanced Settings", 2, 16, win_w - 6);
  exp_lbl1 = gtcaca_label_new(GTCACA_WIDGET(win), "  Debug mode: off", 2, 17);
  exp_lbl2 = gtcaca_label_new(GTCACA_WIDGET(win), "  Log level: info", 2, 18);
  gtcaca_expander_add_managed(expander, GTCACA_WIDGET(exp_lbl1));
  gtcaca_expander_add_managed(expander, GTCACA_WIDGET(exp_lbl2));

  /* --- PasswordEntry --- */
  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Password:", 2, 20);
  passentry = gtcaca_entry_new(GTCACA_WIDGET(win), 12, 20, 16);
  gtcaca_entry_set_secret(passentry, 1);
  gtcaca_entry_set_text(passentry, "secret123");

  /* --- Status bar --- */
  statusbar = gtcaca_statusbar_new("Tab: next | Q: quit");

  /* Focus the scale first */
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(scale));

  gtcaca_main();

  return 0;
}
