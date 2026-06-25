#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/dialog.h>
#include <gtcaca/main.h>

gtcaca_dialog_widget_t *gtcaca_dialog_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_dialog_widget_t *d = malloc(sizeof(*d));
  if (!d) { fprintf(stderr, "Error: Cannot allocate dialog\n"); return NULL; }
  d->id = gtcaca_get_newid();
  d->has_focus = 1; d->is_visible = 1; d->type = GTCACA_WIDGET_DIALOG;
  d->parent = parent; d->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(d), x, y);
  d->width = width; d->height = height;
  d->color_focus_fg = d->color_nonfocus_fg = gmo.theme.window.fg;
  d->color_focus_bg = d->color_nonfocus_bg = gmo.theme.window.bg;
  d->title = NULL; d->message = NULL; d->buttons = NULL; d->nbuttons = 0;
  d->sel = 0; d->result = GTCACA_DIALOG_ONGOING;
  d->sel_fg = CACA_BLACK; d->sel_bg = CACA_CYAN;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(d));
  return d;
}

static void free_buttons(gtcaca_dialog_widget_t *d)
{
  int i;
  for (i = 0; i < d->nbuttons; i++) free(d->buttons[i]);
  free(d->buttons); d->buttons = NULL; d->nbuttons = 0;
}

void gtcaca_dialog_set(gtcaca_dialog_widget_t *d, const char *title, const char *message,
                       const char **buttons, int nbuttons)
{
  int i;
  free(d->title); d->title = title ? strdup(title) : NULL;
  free(d->message); d->message = strdup(message ? message : "");
  free_buttons(d);
  if (nbuttons > 0) {
    d->buttons = malloc((size_t)nbuttons * sizeof(char *));
    for (i = 0; i < nbuttons; i++) d->buttons[i] = strdup(buttons[i] ? buttons[i] : "");
    d->nbuttons = nbuttons;
  }
  d->sel = 0; d->result = GTCACA_DIALOG_ONGOING;
}

int gtcaca_dialog_result(gtcaca_dialog_widget_t *d) { return d->result; }

/* count message lines and the widest one */
static void measure(const char *msg, int *lines, int *maxw)
{
  int l = 1, w = 0, c = 0;
  const char *p;
  for (p = msg; *p; p++) {
    if (*p == '\n') { if (c > w) w = c; c = 0; l++; }
    else c++;
  }
  if (c > w) w = c;
  *lines = l; *maxw = w;
}

void gtcaca_dialog_draw(gtcaca_dialog_widget_t *d)
{
  uint8_t fg = gmo.theme.window.fg, bg = gmo.theme.window.bg;
  int inner_x = d->x + 2, inner_y = d->y + 1, i, by, bx, btot = 0;
  const char *p, *start;
  int line = 0;

  /* shadow + box */
  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, d->x, d->y, d->width, d->height, ' ');
  caca_draw_cp437_box(gmo.cv, d->x, d->y, d->width, d->height);
  if (d->title) {
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, d->x + 2, d->y, "| %s |", d->title);
    caca_set_attr(gmo.cv, 0);
  }

  /* message lines */
  caca_set_color_ansi(gmo.cv, fg, bg);
  start = d->message ? d->message : "";
  for (p = start; ; p++) {
    if (*p == '\n' || *p == '\0') {
      int len = (int)(p - start);
      if (len > d->width - 4) len = d->width - 4;
      caca_printf(gmo.cv, inner_x, inner_y + line, "%-.*s", len, start);
      line++; start = p + 1;
      if (*p == '\0') break;
    }
  }

  /* buttons row, centred along the bottom inner line */
  for (i = 0; i < d->nbuttons; i++) btot += (int)strlen(d->buttons[i]) + 4 + 1;
  if (btot > 0) btot -= 1;
  by = d->y + d->height - 2;
  bx = d->x + (d->width - btot) / 2; if (bx < d->x + 1) bx = d->x + 1;
  for (i = 0; i < d->nbuttons; i++) {
    int sel = (i == d->sel);
    /* reuse the shared button theme: active = buttonfocus (red), others = button */
    if (sel) { caca_set_color_ansi(gmo.cv, gmo.theme.buttonfocus.fg, gmo.theme.buttonfocus.bg); caca_set_attr(gmo.cv, CACA_BOLD); }
    else     { caca_set_color_ansi(gmo.cv, gmo.theme.button.fg, gmo.theme.button.bg); caca_set_attr(gmo.cv, 0); }
    caca_printf(gmo.cv, bx, by, "[ %s ]", d->buttons[i]);
    bx += (int)strlen(d->buttons[i]) + 4 + 1;
  }
  caca_set_attr(gmo.cv, 0);
}

int gtcaca_dialog_key(gtcaca_dialog_widget_t *d, int key, void *userdata)
{
  (void)userdata;
  if (d->nbuttons <= 0) { if (key == CACA_KEY_RETURN || key == 10 || key == CACA_KEY_ESCAPE) d->result = -1; return 1; }
  switch (key) {
  case CACA_KEY_LEFT:  case CACA_KEY_UP:   d->sel = (d->sel - 1 + d->nbuttons) % d->nbuttons; break;
  case CACA_KEY_RIGHT: case CACA_KEY_DOWN:
  case CACA_KEY_TAB:                       d->sel = (d->sel + 1) % d->nbuttons; break;
  case CACA_KEY_RETURN: case 10: case ' ': d->result = d->sel; break;
  case CACA_KEY_ESCAPE:                    d->result = -1; break;
  default: return 0;
  }
  return 1;
}

void gtcaca_dialog_free(gtcaca_dialog_widget_t *d)
{
  if (!d) return;
  CDL_DELETE(gmo.widgets_list, GTCACA_WIDGET(d));   /* take it back out of the draw list */
  free(d->title); free(d->message); free_buttons(d);
  free(d);
}

/* ── blocking helpers ───────────────────────────────────────────────────────── */
int gtcaca_dialog_run(const char *title, const char *message, const char **buttons, int nbuttons)
{
  int cw = caca_get_canvas_width(gmo.cv), chh = caca_get_canvas_height(gmo.cv);
  int lines, maxw, bw = 0, w, h, i, res;
  gtcaca_dialog_widget_t *d;
  caca_event_t ev;

  measure(message ? message : "", &lines, &maxw);
  for (i = 0; i < nbuttons; i++) bw += (int)strlen(buttons[i]) + 5;
  w = maxw + 6; if (bw + 2 > w) w = bw + 2;
  if (title && (int)strlen(title) + 6 > w) w = (int)strlen(title) + 6;
  if (w > cw - 2) w = cw - 2; if (w < 16) w = 16;
  h = lines + 4; if (h > chh - 2) h = chh - 2; if (h < 5) h = 5;

  d = gtcaca_dialog_new(NULL, (cw - w) / 2, (chh - h) / 2, w, h);
  if (!d) return -1;
  gtcaca_dialog_set(d, title, message, buttons, nbuttons);

  while (d->result == GTCACA_DIALOG_ONGOING) {
    gtcaca_redraw();
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    gtcaca_dialog_key(d, caca_get_event_key_ch(&ev), NULL);
  }
  res = d->result;
  gtcaca_dialog_free(d);
  gtcaca_redraw();
  return res;
}

int gtcaca_dialog_confirm(const char *title, const char *message)
{
  const char *b[] = { "OK", "Cancel" };
  return gtcaca_dialog_run(title, message, b, 2) == 0 ? 1 : 0;
}

void gtcaca_dialog_message(const char *title, const char *message)
{
  const char *b[] = { "OK" };
  gtcaca_dialog_run(title, message, b, 1);
}
