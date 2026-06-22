/*
 * GTCaca Layout Demo
 *
 * Showcases the vbox / hbox layout helpers (à la Qt's QVBoxLayout /
 * QHBoxLayout). Unlike simple.c, which positions every widget by hand, here
 * widgets are simply added to boxes and the boxes compute the coordinates.
 *
 * Left window  : a form — a vertical box stacking rows, each row a horizontal
 *                box of "label + entry", and a right-aligned button bar.
 * Right window : alignment tricks — stretch() to centre and to push apart.
 *
 * Controls:
 *   Tab / Arrows – move focus
 *   Enter        – submit (default button)
 *   Q / Esc      – quit
 */

#include <stdio.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/label.h>
#include <gtcaca/button.h>
#include <gtcaca/entry.h>
#include <gtcaca/separator.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/box.h>

static gtcaca_statusbar_widget_t *g_statusbar = NULL;
static gtcaca_entry_widget_t     *g_user      = NULL;

static int on_login(gtcaca_button_widget_t *btn, int key, void *ud)
{
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  gtcaca_statusbar_set_text(g_statusbar, "Login pressed — laid out by a vbox/hbox, no manual coordinates");
  return 0;
}

static int on_clear(gtcaca_button_widget_t *btn, int key, void *ud)
{
  if (key != CACA_KEY_RETURN && key != ' ') return 0;
  gtcaca_entry_set_text(g_user, "");
  gtcaca_statusbar_set_text(g_statusbar, "Cleared");
  return 0;
}

/* Build a "Label: [entry]" row as a horizontal box and add it to the form. */
static void add_field(gtcaca_box_t *form, gtcaca_window_widget_t *win,
                      const char *caption, gtcaca_entry_widget_t **out_entry)
{
  gtcaca_box_t          *row = gtcaca_hbox_new();
  gtcaca_label_widget_t *lbl = gtcaca_label_new(GTCACA_WIDGET(win), (char *)caption, 0, 0);
  gtcaca_entry_widget_t *ent = gtcaca_entry_new(GTCACA_WIDGET(win), 0, 0, 10);

  gtcaca_box_set_margin(row, 0);          /* rows hug the form, no extra inset */
  gtcaca_box_add(row, GTCACA_WIDGET(lbl));        /* natural width caption */
  gtcaca_box_add_expand(row, GTCACA_WIDGET(ent)); /* entry soaks up the rest */

  gtcaca_box_add_box(form, row);
  if (out_entry) *out_entry = ent;
}

static gtcaca_window_widget_t *build_form_window(int x, int y, int w, int h)
{
  gtcaca_window_widget_t *win;
  gtcaca_box_t           *form, *buttons;
  gtcaca_label_widget_t  *title;
  gtcaca_separator_widget_t *sep;
  gtcaca_button_widget_t *login, *clear;

  win = gtcaca_window_new(NULL, "Form (vbox)", x, y, w, h);

  title = gtcaca_label_new(GTCACA_WIDGET(win), "Please sign in", 0, 0);
  sep   = gtcaca_separator_new(GTCACA_WIDGET(win), 0, 0, w - 4, 0);

  login = gtcaca_button_new(GTCACA_WIDGET(win), "[ Login ]", 0, 0);
  clear = gtcaca_button_new(GTCACA_WIDGET(win), "[ Clear ]", 0, 0);
  gtcaca_button_key_cb_register(login, on_login);
  gtcaca_button_key_cb_register(clear, on_clear);

  /* Button bar: stretch on the left pushes both buttons to the right. */
  buttons = gtcaca_hbox_new();
  gtcaca_box_set_margin(buttons, 0);
  gtcaca_box_add_stretch(buttons);
  gtcaca_box_add(buttons, GTCACA_WIDGET(clear));
  gtcaca_box_add(buttons, GTCACA_WIDGET(login));

  /* The form: a vertical stack. */
  form = gtcaca_vbox_new();
  gtcaca_box_add(form, GTCACA_WIDGET(title));
  gtcaca_box_add(form, GTCACA_WIDGET(sep));
  add_field(form, win, "User:", &g_user);
  add_field(form, win, "Pass:", NULL);
  gtcaca_box_add_stretch(form);           /* eat slack so buttons sit at bottom */
  gtcaca_box_add_box(form, buttons);

  gtcaca_box_apply_window(form, win);
  gtcaca_box_free(form);                   /* boxes are throwaway once applied */

  gtcaca_window_set_default(win, GTCACA_WIDGET(login));
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(g_user));
  return win;
}

static void build_align_window(int x, int y, int w, int h)
{
  gtcaca_window_widget_t *win;
  gtcaca_box_t           *col, *bar;
  gtcaca_label_widget_t  *l1, *l2, *l3;
  gtcaca_button_widget_t *a, *b, *c;

  win = gtcaca_window_new(NULL, "Align (hbox)", x, y, w, h);

  /* Three labels, centred vertically by stretches above and below. */
  l1 = gtcaca_label_new(GTCACA_WIDGET(win), "Centered by", 0, 0);
  l2 = gtcaca_label_new(GTCACA_WIDGET(win), "two stretches", 0, 0);
  l3 = gtcaca_label_new(GTCACA_WIDGET(win), "above & below", 0, 0);

  /* A horizontal bar that spreads three buttons evenly. */
  a = gtcaca_button_new(GTCACA_WIDGET(win), "[ A ]", 0, 0);
  b = gtcaca_button_new(GTCACA_WIDGET(win), "[ B ]", 0, 0);
  c = gtcaca_button_new(GTCACA_WIDGET(win), "[ C ]", 0, 0);

  bar = gtcaca_hbox_new();
  gtcaca_box_set_margin(bar, 0);
  gtcaca_box_add_stretch(bar);
  gtcaca_box_add(bar, GTCACA_WIDGET(a));
  gtcaca_box_add_stretch(bar);
  gtcaca_box_add(bar, GTCACA_WIDGET(b));
  gtcaca_box_add_stretch(bar);
  gtcaca_box_add(bar, GTCACA_WIDGET(c));
  gtcaca_box_add_stretch(bar);

  col = gtcaca_vbox_new();
  gtcaca_box_set_align(col, GTCACA_ALIGN_CENTER);   /* centre the labels */
  gtcaca_box_add_stretch(col);
  gtcaca_box_add(col, GTCACA_WIDGET(l1));
  gtcaca_box_add(col, GTCACA_WIDGET(l2));
  gtcaca_box_add(col, GTCACA_WIDGET(l3));
  gtcaca_box_add_stretch(col);
  gtcaca_box_add_box(col, bar);

  gtcaca_box_apply_window(col, win);
  gtcaca_box_free(col);

  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(a));
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  int fw, fh, gap;

  gtcaca_init(&argc, &argv);
  app = gtcaca_application_new("Layout Demo");

  fw  = 40;
  fh  = 18;
  gap = 2;

  build_form_window(app->x + 2, app->y + 1, fw, fh);
  build_align_window(app->x + 2 + fw + gap, app->y + 1, fw, fh);

  g_statusbar = gtcaca_statusbar_new(
    "Tab/Arrows: move | Enter: submit | Ctrl+N: next window | Q: quit");

  gtcaca_main();
  return 0;
}
