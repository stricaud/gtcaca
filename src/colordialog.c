#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/colordialog.h>
#include <gtcaca/main.h>

/* 4×4 swatch grid: colours 0–15, row-major. */
#define COLS 4
#define ROWS 4
#define SW   6   /* swatch inner width  */
#define SH   2   /* swatch inner height */
#define GAPX 2   /* horizontal gap (also holds the selection outline) */
#define GAPY 1   /* vertical gap                                       */

#define GRID_W (COLS * SW + (COLS - 1) * GAPX)
#define GRID_H (ROWS * SH + (ROWS - 1) * GAPY)

static const char *const color_names[16] = {
  "Black",       "Blue",         "Green",       "Cyan",
  "Red",         "Magenta",      "Brown",       "Light Gray",
  "Dark Gray",   "Light Blue",   "Light Green", "Light Cyan",
  "Light Red",   "Light Magenta","Yellow",      "White",
};

const char *gtcaca_color_name(int idx)
{
  return (idx >= 0 && idx < 16) ? color_names[idx] : "?";
}

/* A pale swatch needs dark text on it to stay readable; everything else white. */
static uint8_t label_fg(int color)
{
  switch (color) {
  case CACA_LIGHTGRAY: case CACA_LIGHTGREEN: case CACA_LIGHTCYAN:
  case CACA_YELLOW:     case CACA_WHITE:
    return CACA_BLACK;
  default:
    return CACA_WHITE;
  }
}

gtcaca_colordialog_widget_t *gtcaca_colordialog_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_colordialog_widget_t *d = calloc(1, sizeof(*d));
  if (!d) { fprintf(stderr, "Error: Cannot allocate colordialog\n"); return NULL; }
  d->id = gtcaca_get_newid();
  d->has_focus = 1; d->is_visible = 1; d->type = GTCACA_WIDGET_COLORDIALOG;
  d->parent = parent; d->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(d), x, y);
  d->width = width; d->height = height;
  d->color_focus_fg = d->color_nonfocus_fg = gmo.theme.window.fg;
  d->color_focus_bg = d->color_nonfocus_bg = gmo.theme.window.bg;
  d->title = NULL;
  d->sel = 0; d->result = GTCACA_COLORDIALOG_ONGOING;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(d));
  return d;
}

void gtcaca_colordialog_set_title(gtcaca_colordialog_widget_t *d, const char *title)
{
  if (!d) return;
  free(d->title); d->title = title ? strdup(title) : NULL;
}

int gtcaca_colordialog_result(gtcaca_colordialog_widget_t *d) { return d->result; }

void gtcaca_colordialog_draw(gtcaca_colordialog_widget_t *d)
{
  uint8_t fg = gmo.theme.window.fg, bg = gmo.theme.window.bg;
  int grid_x = d->x + (d->width - GRID_W) / 2;
  int grid_y = d->y + 2;
  int i;

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, d->x, d->y, d->width, d->height, ' ');
  caca_draw_cp437_box(gmo.cv, d->x, d->y, d->width, d->height);
  if (d->title) {
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, d->x + 2, d->y, "| %s |", d->title);
    caca_set_attr(gmo.cv, 0);
  }

  /* swatch grid */
  for (i = 0; i < 16; i++) {
    int r = i / COLS, c = i % COLS;
    int cx = grid_x + c * (SW + GAPX);
    int cy = grid_y + r * (SH + GAPY);
    int nx = cx + (SW - 2) / 2;               /* centre the 2-char index label */

    caca_set_color_ansi(gmo.cv, label_fg(i), (uint8_t)i); caca_set_attr(gmo.cv, 0);
    caca_fill_box(gmo.cv, cx, cy, SW, SH, ' ');
    caca_printf(gmo.cv, nx, cy + (SH - 1) / 2, "%2d", i);

    if (i == d->sel) {   /* white outline drawn into the surrounding gap */
      caca_set_color_ansi(gmo.cv, CACA_WHITE, bg); caca_set_attr(gmo.cv, CACA_BOLD);
      caca_draw_cp437_box(gmo.cv, cx - 1, cy - 1, SW + 2, SH + 2);
      caca_set_attr(gmo.cv, 0);
    }
  }

  /* footer: selected colour name + key hint */
  caca_set_color_ansi(gmo.cv, CACA_LIGHTCYAN, bg); caca_set_attr(gmo.cv, CACA_BOLD);
  caca_printf(gmo.cv, d->x + 2, d->y + d->height - 3, "%2d  %-.*s",
              d->sel, d->width - 8, gtcaca_color_name(d->sel));
  caca_set_attr(gmo.cv, 0);
  caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, bg);
  caca_printf(gmo.cv, d->x + 2, d->y + d->height - 2, "%-.*s",
              d->width - 4, "Enter: pick   Esc: cancel");
  caca_set_color_ansi(gmo.cv, fg, bg);
}

int gtcaca_colordialog_key(gtcaca_colordialog_widget_t *d, int key, void *userdata)
{
  int r = d->sel / COLS, c = d->sel % COLS;
  (void)userdata;
  switch (key) {
  case CACA_KEY_LEFT:  d->sel = (d->sel - 1 + 16) % 16; break;
  case CACA_KEY_RIGHT:
  case CACA_KEY_TAB:   d->sel = (d->sel + 1) % 16; break;
  case CACA_KEY_UP:    d->sel = ((r - 1 + ROWS) % ROWS) * COLS + c; break;
  case CACA_KEY_DOWN:  d->sel = ((r + 1) % ROWS) * COLS + c; break;
  case CACA_KEY_RETURN: case 10: case ' ': d->result = d->sel; break;
  case CACA_KEY_ESCAPE:                    d->result = -1; break;
  default: return 0;
  }
  return 1;
}

void gtcaca_colordialog_free(gtcaca_colordialog_widget_t *d)
{
  if (!d) return;
  CDL_DELETE(gmo.widgets_list, GTCACA_WIDGET(d));
  free(d->title);
  free(d);
}

/* ── blocking helper ─────────────────────────────────────────────────────────── */
int gtcaca_colordialog_run(const char *title, int initial)
{
  int cw = caca_get_canvas_width(gmo.cv), chh = caca_get_canvas_height(gmo.cv);
  int w = GRID_W + 6, h = GRID_H + 5, res;
  gtcaca_colordialog_widget_t *d;
  caca_event_t ev;

  if (title && (int)strlen(title) + 6 > w) w = (int)strlen(title) + 6;
  if (w > cw - 2) w = cw - 2;
  if (h > chh - 2) h = chh - 2;

  d = gtcaca_colordialog_new(NULL, (cw - w) / 2, (chh - h) / 2, w, h);
  if (!d) return -1;
  gtcaca_colordialog_set_title(d, title);
  if (initial >= 0 && initial < 16) d->sel = initial;

  while (d->result == GTCACA_COLORDIALOG_ONGOING) {
    gtcaca_redraw();
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    gtcaca_colordialog_key(d, caca_get_event_key_ch(&ev), NULL);
  }
  res = d->result;
  gtcaca_colordialog_free(d);
  gtcaca_redraw();
  return res;
}
