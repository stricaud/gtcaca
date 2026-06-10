/*
 * GTCaca System Monitor Demo
 *
 * Demonstrates: ProgressBar (live CPU/memory gauges), Spinner (activity
 *               indicators), Scale (frequency/speed control), SpinButton
 *               (numeric tuning), Frame (grouped stats), Separator,
 *               Label, StatusBar.
 *
 * Controls:
 *   Tab          – next widget
 *   Left/Right   – adjust the CPU frequency scale
 *   Up/Down      – adjust the thread count spinbutton
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
#include <gtcaca/progressbar.h>
#include <gtcaca/spinner.h>
#include <gtcaca/scale.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/frame.h>
#include <gtcaca/separator.h>
#include <gtcaca/statusbar.h>

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t      *win;
  gtcaca_label_widget_t       *lbl;
  gtcaca_progressbar_widget_t *pb_cpu, *pb_mem, *pb_disk, *pb_net;
  gtcaca_spinner_widget_t     *spin_io, *spin_net;
  gtcaca_scale_widget_t       *freq_scale;
  gtcaca_spinbutton_widget_t  *thread_sb;
  gtcaca_frame_widget_t       *frame_cpu, *frame_net;
  gtcaca_separator_widget_t   *sep;
  gtcaca_statusbar_widget_t   *status;
  int cw, ch, ww, wh;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("System Monitor");

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);
  ww = cw > 70 ? 70 : cw;
  wh = ch > 26 ? 26 : ch;

  win = gtcaca_window_new(NULL, "System Monitor", 1, 1, ww, wh);
  int bw = ww - 20;

  /* ---- CPU / Memory frame ---- */
  frame_cpu = gtcaca_frame_new(GTCACA_WIDGET(win), "Resources", 2, 2, ww - 4, 10);

  lbl    = gtcaca_label_new(GTCACA_WIDGET(win), "CPU:    ", 4, 4);
  pb_cpu = gtcaca_progressbar_new(GTCACA_WIDGET(win), 13, 4, bw);
  gtcaca_progressbar_set_value(pb_cpu, 0.62f);

  lbl    = gtcaca_label_new(GTCACA_WIDGET(win), "Memory: ", 4, 6);
  pb_mem = gtcaca_progressbar_new(GTCACA_WIDGET(win), 13, 6, bw);
  gtcaca_progressbar_set_value(pb_mem, 0.45f);

  lbl     = gtcaca_label_new(GTCACA_WIDGET(win), "Disk:   ", 4, 8);
  pb_disk = gtcaca_progressbar_new(GTCACA_WIDGET(win), 13, 8, bw);
  gtcaca_progressbar_set_value(pb_disk, 0.78f);

  lbl     = gtcaca_label_new(GTCACA_WIDGET(win), "I/O:    ", 4, 10);
  spin_io = gtcaca_spinner_new(GTCACA_WIDGET(win), 13, 10);
  gtcaca_spinner_set_spinning(spin_io, 1);

  sep = gtcaca_separator_new(GTCACA_WIDGET(win), 2, 13, ww - 4, 0);

  /* ---- Network frame ---- */
  frame_net = gtcaca_frame_new(GTCACA_WIDGET(win), "Network", 2, 14, ww - 4, 6);

  lbl    = gtcaca_label_new(GTCACA_WIDGET(win), "Traffic:", 4, 16);
  pb_net = gtcaca_progressbar_new(GTCACA_WIDGET(win), 14, 16, bw);
  gtcaca_progressbar_set_value(pb_net, 0.30f);

  lbl      = gtcaca_label_new(GTCACA_WIDGET(win), "Active:  ", 4, 18);
  spin_net = gtcaca_spinner_new(GTCACA_WIDGET(win), 14, 18);
  gtcaca_spinner_set_spinning(spin_net, 1);

  sep = gtcaca_separator_new(GTCACA_WIDGET(win), 2, 21, ww - 4, 0);

  /* ---- Tuning controls ---- */
  lbl        = gtcaca_label_new(GTCACA_WIDGET(win), "CPU freq (GHz):", 4, 22);
  freq_scale = gtcaca_scale_new(GTCACA_WIDGET(win), 21, 22, ww - 28, 0.8, 4.8, 0.2);
  gtcaca_scale_set_value(freq_scale, 3.2);

  lbl       = gtcaca_label_new(GTCACA_WIDGET(win), "Threads:", 4, 23);
  thread_sb = gtcaca_spinbutton_new(GTCACA_WIDGET(win), 14, 23, 1.0, 64.0, 1.0);
  gtcaca_spinbutton_set_value(thread_sb, 8.0);

  status = gtcaca_statusbar_new("Left/Right: freq | Up/Down: threads | Q: quit");

  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(freq_scale));
  gtcaca_main();
  return 0;
}
