#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/tabs.h>
#include <gtcaca/main.h>

gtcaca_tabs_widget_t *gtcaca_tabs_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_tabs_widget_t *t = malloc(sizeof(*t));
  if (!t) { fprintf(stderr, "Error: Cannot allocate tabs\n"); return NULL; }
  t->id = gtcaca_get_newid();
  t->has_focus = 0;
  t->is_visible = 1;
  t->type = GTCACA_WIDGET_TABS;
  t->parent = parent;
  t->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(t), x, y);
  t->width = width; t->height = height;
  t->color_focus_fg = t->color_nonfocus_fg = gmo.theme.textview.fg;
  t->color_focus_bg = t->color_nonfocus_bg = gmo.theme.textview.bg;
  t->titles = NULL; t->ntitles = 0; t->sel = 0; t->title = NULL;
  t->sel_fg = CACA_BLACK; t->sel_bg = CACA_YELLOW;   /* tui-rs-style highlight */
  t->divider = 0x2502;                               /* │ */
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(t));
  return t;
}

static void _free_titles(gtcaca_tabs_widget_t *t)
{
  int i;
  for (i = 0; i < t->ntitles; i++) free(t->titles[i]);
  free(t->titles); t->titles = NULL; t->ntitles = 0;
}

void gtcaca_tabs_set_titles(gtcaca_tabs_widget_t *t, const char **titles, int n)
{
  int i;
  _free_titles(t);
  if (n <= 0 || !titles) { t->sel = 0; return; }
  t->titles = malloc((size_t)n * sizeof(char *));
  if (!t->titles) return;
  for (i = 0; i < n; i++) t->titles[i] = strdup(titles[i] ? titles[i] : "");
  t->ntitles = n;
  if (t->sel >= n) t->sel = n - 1;
  if (t->sel < 0) t->sel = 0;
}

void gtcaca_tabs_set_title(gtcaca_tabs_widget_t *t, const char *title)
{
  free(t->title); t->title = title ? strdup(title) : NULL;
}

void gtcaca_tabs_set_selected(gtcaca_tabs_widget_t *t, int idx)
{
  if (t->ntitles <= 0) { t->sel = 0; return; }
  if (idx < 0) idx = 0;
  if (idx > t->ntitles - 1) idx = t->ntitles - 1;
  t->sel = idx;
}

int gtcaca_tabs_selected(gtcaca_tabs_widget_t *t) { return t->sel; }

void gtcaca_tabs_draw(gtcaca_tabs_widget_t *t)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = t->x + 1, inner_y = t->y + 1, inner_w = t->width - 2;
  int cx = 0, i;

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, t->x, t->y, t->width, t->height, ' ');
  caca_draw_cp437_box(gmo.cv, t->x, t->y, t->width, t->height);
  if (t->title) caca_printf(gmo.cv, t->x + 2, t->y, "| %s |", t->title);
  if (inner_w <= 0) return;

  for (i = 0; i < t->ntitles && cx < inner_w; i++) {
    const char *s = t->titles[i];
    int len = (int)strlen(s), k;
    if (i > 0) {                                   /* divider between tabs */
      caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
      caca_put_char(gmo.cv, inner_x + cx, inner_y, t->divider ? t->divider : 0x2502);
      cx++;
    }
    if (i == t->sel) {
      caca_set_color_ansi(gmo.cv, t->sel_fg, t->sel_bg);
      caca_set_attr(gmo.cv, CACA_BOLD);
    } else {
      caca_set_color_ansi(gmo.cv, fg, bg);
      caca_set_attr(gmo.cv, 0);
    }
    /* draw " label " so the highlight has a one-cell pad either side */
    if (cx < inner_w) caca_put_char(gmo.cv, inner_x + cx, inner_y, ' ');
    cx++;
    for (k = 0; k < len && cx < inner_w; k++, cx++)
      caca_put_char(gmo.cv, inner_x + cx, inner_y, (uint32_t)(unsigned char)s[k]);
    if (cx < inner_w) caca_put_char(gmo.cv, inner_x + cx, inner_y, ' ');
    cx++;
  }
  caca_set_attr(gmo.cv, 0);
}

int gtcaca_tabs_key(gtcaca_tabs_widget_t *t, int key, void *userdata)
{
  (void)userdata;
  if (t->ntitles <= 0) return 0;
  switch (key) {
  case CACA_KEY_RIGHT:
  case CACA_KEY_TAB:
    t->sel = (t->sel + 1) % t->ntitles;
    return 1;
  case CACA_KEY_LEFT:
    t->sel = (t->sel - 1 + t->ntitles) % t->ntitles;
    return 1;
  default:
    return 0;
  }
}

void gtcaca_tabs_free(gtcaca_tabs_widget_t *t)
{
  if (!t) return;
  _free_titles(t);
  free(t->title);
  free(t);
}
