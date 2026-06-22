/*
 * GTCaca Settings Panel Demo
 *
 * Demonstrates: Expander (collapsible sections), Scale (volume/brightness),
 *               Switch (feature toggles), SpinButton (numeric settings),
 *               Separator, Frame, Label, StatusBar.
 *
 * Controls:
 *   Tab          – next widget
 *   Arrow keys   – navigate / adjust scales, spinbuttons
 *   Space / Enter– toggle switch, expand/collapse section
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
#include <gtcaca/scale.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/switch.h>
#include <gtcaca/expander.h>
#include <gtcaca/separator.h>
#include <gtcaca/frame.h>
#include <gtcaca/statusbar.h>

static gtcaca_statusbar_widget_t *g_status = NULL;

static int on_volume(gtcaca_scale_widget_t *s, double v, void *ud)
{
  char msg[64];
  snprintf(msg, sizeof(msg), "Volume: %.0f%%", v);
  gtcaca_statusbar_set_text(g_status, msg);
  return 0;
}

static int on_brightness(gtcaca_scale_widget_t *s, double v, void *ud)
{
  char msg[64];
  snprintf(msg, sizeof(msg), "Brightness: %.0f%%", v);
  gtcaca_statusbar_set_text(g_status, msg);
  return 0;
}

static int on_wifi(gtcaca_switch_widget_t *sw, int active, void *ud)
{
  gtcaca_statusbar_set_text(g_status, active ? "WiFi: enabled" : "WiFi: disabled");
  return 0;
}

static int on_bluetooth(gtcaca_switch_widget_t *sw, int active, void *ud)
{
  gtcaca_statusbar_set_text(g_status, active ? "Bluetooth: enabled" : "Bluetooth: disabled");
  return 0;
}

static int on_timeout(gtcaca_spinbutton_widget_t *sb, double v, void *ud)
{
  char msg[64];
  snprintf(msg, sizeof(msg), "Session timeout: %.0f minutes", v);
  gtcaca_statusbar_set_text(g_status, msg);
  return 0;
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t      *win;
  gtcaca_label_widget_t       *lbl;
  gtcaca_scale_widget_t       *vol, *bright;
  gtcaca_spinbutton_widget_t  *timeout_sb, *retry_sb;
  gtcaca_switch_widget_t      *wifi_sw, *bt_sw, *notif_sw;
  gtcaca_expander_widget_t    *audio_exp, *display_exp, *net_exp, *sec_exp;
  gtcaca_separator_widget_t   *sep;
  int cw, ch;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("Settings Demo");

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);

  win = gtcaca_window_new(NULL, "Settings", 1, 1, cw - 2, ch - 3);
  int ww = cw - 2;

  int row = 2;

  /* ---- Audio section ---- */
  audio_exp = gtcaca_expander_new(GTCACA_WIDGET(win), "Audio", 2, row, ww - 4);
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Volume:", 4, row);
  vol = gtcaca_scale_new(GTCACA_WIDGET(win), 14, row, ww - 20, 0.0, 100.0, 5.0);
  gtcaca_scale_set_value(vol, 70.0);
  gtcaca_scale_cb_register(vol, on_volume, NULL);
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Mute:", 4, row);
  gtcaca_switch_widget_t *mute_sw = gtcaca_switch_new(GTCACA_WIDGET(win), 14, row);
  row++;

  gtcaca_expander_add_managed(audio_exp, GTCACA_WIDGET(lbl));
  /* we add the label above (Mute:) and switches in order */
  /* Note: add vol label separately – grab the volume label */
  gtcaca_expander_add_managed(audio_exp, GTCACA_WIDGET(vol));
  gtcaca_expander_add_managed(audio_exp, GTCACA_WIDGET(mute_sw));

  sep = gtcaca_separator_new(GTCACA_WIDGET(win), 2, row, ww - 4, 0);
  row++;

  /* ---- Display section ---- */
  display_exp = gtcaca_expander_new(GTCACA_WIDGET(win), "Display", 2, row, ww - 4);
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Brightness:", 4, row);
  bright = gtcaca_scale_new(GTCACA_WIDGET(win), 17, row, ww - 23, 10.0, 100.0, 10.0);
  gtcaca_scale_set_value(bright, 80.0);
  gtcaca_scale_cb_register(bright, on_brightness, NULL);
  gtcaca_expander_add_managed(display_exp, GTCACA_WIDGET(lbl));
  gtcaca_expander_add_managed(display_exp, GTCACA_WIDGET(bright));
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Night mode:", 4, row);
  gtcaca_switch_widget_t *night_sw = gtcaca_switch_new(GTCACA_WIDGET(win), 17, row);
  gtcaca_expander_add_managed(display_exp, GTCACA_WIDGET(lbl));
  gtcaca_expander_add_managed(display_exp, GTCACA_WIDGET(night_sw));
  row++;

  sep = gtcaca_separator_new(GTCACA_WIDGET(win), 2, row, ww - 4, 0);
  row++;

  /* ---- Network section ---- */
  net_exp = gtcaca_expander_new(GTCACA_WIDGET(win), "Network", 2, row, ww - 4);
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "WiFi:", 4, row);
  wifi_sw = gtcaca_switch_new(GTCACA_WIDGET(win), 14, row);
  gtcaca_switch_set_active(wifi_sw, 1);
  gtcaca_switch_cb_register(wifi_sw, on_wifi, NULL);
  gtcaca_expander_add_managed(net_exp, GTCACA_WIDGET(lbl));
  gtcaca_expander_add_managed(net_exp, GTCACA_WIDGET(wifi_sw));
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Bluetooth:", 4, row);
  bt_sw = gtcaca_switch_new(GTCACA_WIDGET(win), 16, row);
  gtcaca_switch_cb_register(bt_sw, on_bluetooth, NULL);
  gtcaca_expander_add_managed(net_exp, GTCACA_WIDGET(lbl));
  gtcaca_expander_add_managed(net_exp, GTCACA_WIDGET(bt_sw));
  row++;

  sep = gtcaca_separator_new(GTCACA_WIDGET(win), 2, row, ww - 4, 0);
  row++;

  /* ---- Security section ---- */
  sec_exp = gtcaca_expander_new(GTCACA_WIDGET(win), "Security", 2, row, ww - 4);
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Session timeout:", 4, row);
  timeout_sb = gtcaca_spinbutton_new(GTCACA_WIDGET(win), 22, row, 1.0, 120.0, 1.0);
  gtcaca_spinbutton_set_value(timeout_sb, 15.0);
  gtcaca_spinbutton_cb_register(timeout_sb, on_timeout, NULL);
  gtcaca_expander_add_managed(sec_exp, GTCACA_WIDGET(lbl));
  gtcaca_expander_add_managed(sec_exp, GTCACA_WIDGET(timeout_sb));
  row++;

  lbl = gtcaca_label_new(GTCACA_WIDGET(win), "Notifications:", 4, row);
  notif_sw = gtcaca_switch_new(GTCACA_WIDGET(win), 20, row);
  gtcaca_switch_set_active(notif_sw, 1);
  gtcaca_expander_add_managed(sec_exp, GTCACA_WIDGET(lbl));
  gtcaca_expander_add_managed(sec_exp, GTCACA_WIDGET(notif_sw));

  g_status = gtcaca_statusbar_new("Tab: next | Arrows: adjust | Space: toggle | Q: quit");

  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(audio_exp));
  gtcaca_main();
  return 0;
}
