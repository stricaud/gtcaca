#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <caca.h>

#include <gtcaca/textlist.h>
#include <gtcaca/main.h>
#include <gtcaca/log.h>

#include <gtcaca/utarray.h>

static char *_item(gtcaca_textlist_widget_t *t, unsigned int i)
{
  char **p = (char **)utarray_eltptr(t->list, i);
  return p ? *p : NULL;
}

/* Case-insensitive substring test. */
static int _contains_ci(const char *hay, const char *needle)
{
  size_t nl = strlen(needle);
  if (nl == 0) return 1;
  for (; *hay; hay++) {
    size_t k = 0;
    while (k < nl && hay[k] && tolower((unsigned char)hay[k]) == tolower((unsigned char)needle[k])) k++;
    if (k == nl) return 1;
  }
  return 0;
}

/* Does item `i` match the active search query? (Always yes when not searching
   or the query is empty.) */
static int _matches(gtcaca_textlist_widget_t *t, unsigned int i)
{
  char *s;
  if (!t->searching || t->search_len == 0) return 1;
  s = _item(t, i);
  return s ? _contains_ci(s, t->search_query) : 0;
}

/* Move the selection to the first matching item at or after (dir>=0) / before
   (dir<0) the current one, so the selection never rests on a filtered-out row. */
static void _snap_to_match(gtcaca_textlist_widget_t *t)
{
  unsigned int n = utarray_len(t->list), i;
  if (n == 0 || _matches(t, t->selected_item)) return;
  for (i = 0; i < n; i++) if (_matches(t, i)) { t->selected_item = i; return; }
}

/* Private functions */
static int _gtcaca_textlist_private_key_press(gtcaca_textlist_widget_t *widget, int key, void *userdata)
{
  (void)userdata;

  if (widget->searching) {
    switch (key) {
    case CACA_KEY_RETURN: case 10:                 /* accept: keep the match, exit */
    case CACA_KEY_ESCAPE:                          /* cancel: restore the full list */
      if (key == CACA_KEY_ESCAPE) { widget->search_len = 0; widget->search_query[0] = '\0'; }
      widget->searching = 0;
      return 0;
    case CACA_KEY_UP:   gtcaca_textlist_selection_up(widget);   return 0;
    case CACA_KEY_DOWN: gtcaca_textlist_selection_down(widget); return 0;
    case CACA_KEY_BACKSPACE: case CACA_KEY_DELETE:
      if (widget->search_len > 0) widget->search_query[--widget->search_len] = '\0';
      _snap_to_match(widget);
      return 0;
    default:
      if (key >= 32 && key < 127 && widget->search_len < (int)sizeof widget->search_query - 1) {
        widget->search_query[widget->search_len++] = (char)key;
        widget->search_query[widget->search_len] = '\0';
        _snap_to_match(widget);
      }
      return 0;
    }
  }

  switch(key) {
  case '/':
    if (widget->search_enabled) {
      widget->searching = 1;
      widget->search_len = 0;
      widget->search_query[0] = '\0';
    }
    break;
  case CACA_KEY_UP:
    gtcaca_textlist_selection_up(widget);
    break;
  case CACA_KEY_DOWN:
    gtcaca_textlist_selection_down(widget);
    break;
  }
  return 0;
}

void gtcaca_textlist_set_search_enabled(gtcaca_textlist_widget_t *textlist, int enabled)
{
  textlist->search_enabled = enabled ? 1 : 0;
  if (!enabled) { textlist->searching = 0; textlist->search_len = 0; textlist->search_query[0] = '\0'; }
}

int gtcaca_textlist_is_searching(gtcaca_textlist_widget_t *textlist)
{
  return textlist->searching;
}

/* Public functions */
gtcaca_textlist_widget_t *gtcaca_textlist_new(gtcaca_widget_t *parent, int x, int y)
{
  gtcaca_textlist_widget_t *textlist;
  int i;

  textlist = malloc(sizeof(gtcaca_textlist_widget_t));
  if (!textlist) {
    fprintf(stderr, "Error: Cannot allocate new textlist\n");
    return NULL;
  }

  textlist->id = gtcaca_get_newid();
  textlist->has_focus = parent ? parent->has_focus : 1;
  textlist->is_visible = parent ? parent->is_visible : 1;

  gtcaca_widget_position_size_parent(parent, (gtcaca_widget_t *)textlist, x, y);

  textlist->type = GTCACA_WIDGET_TEXTLIST;
  textlist->parent = parent;
  textlist->children = NULL;

  textlist->color_focus_bg = gmo.theme.textfocus.bg;
  textlist->color_focus_fg = gmo.theme.textfocus.fg;
  textlist->color_nonfocus_bg = gmo.theme.text.bg;
  textlist->color_nonfocus_fg = gmo.theme.text.fg;

  textlist->selected_item = 0;
  textlist->private_key_cb = _gtcaca_textlist_private_key_press;
  textlist->key_cb = NULL;
  textlist->key_cb_userdata = NULL;
  textlist->search_enabled = 1;
  textlist->searching = 0;
  textlist->search_len = 0;
  textlist->search_query[0] = '\0';
  
  utarray_new(textlist->list, &ut_str_icd);

  textlist->view_size = textlist->height - 2;
  textlist->height = textlist->view_size;
  
  gtcaca_textlist_draw(textlist);

  CDL_APPEND(gmo.widgets_list, (gtcaca_widget_t *)textlist);

  return textlist;
}

void gtcaca_textlist_widget_set_view_size(gtcaca_textlist_widget_t *widget, unsigned int view_size)
{
  widget->view_size = view_size;
}

void gtcaca_textlist_widget_destroy(gtcaca_textlist_widget_t *widget)
{
  utarray_free(widget->list);
  free(widget);
}

int gtcaca_textlist_key_cb_register(gtcaca_textlist_widget_t *widget, gtcaca_textlist_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
  return 0;
}

void gtcaca_textlist_append(gtcaca_textlist_widget_t *textlist, char *item)
{
  size_t item_len;
  
  utarray_push_back(textlist->list, &item);

  item_len = strlen(item);
  if (item_len > textlist->width) { textlist->width = item_len; }
}

void gtcaca_textlist_selection_up(gtcaca_textlist_widget_t *textlist)
{
  unsigned int n = utarray_len(textlist->list), steps;
  if (n == 0) return;
  /* Step to the previous item, skipping ones filtered out by the search. */
  for (steps = 0; steps < n; steps++) {
    textlist->selected_item = (textlist->selected_item == 0) ? n - 1 : textlist->selected_item - 1;
    if (_matches(textlist, textlist->selected_item)) return;
  }
}

void gtcaca_textlist_selection_down(gtcaca_textlist_widget_t *textlist)
{
  unsigned int n = utarray_len(textlist->list), steps;
  if (n == 0) return;
  for (steps = 0; steps < n; steps++) {
    textlist->selected_item = (textlist->selected_item >= n - 1) ? 0 : textlist->selected_item + 1;
    if (_matches(textlist, textlist->selected_item)) return;
  }
}

char *gtcaca_textlist_get_text_selected(gtcaca_textlist_widget_t *textlist)
{
  /* The list uses ut_str_icd, so each element is a (copied) char*. eltptr()
     returns the address of that element (a char**); dereference it to get the
     string itself. Returning the char** directly handed callers garbage. */
  if (utarray_len(textlist->list) == 0) return NULL;
  char **p = (char **)utarray_eltptr(textlist->list, textlist->selected_item);
  return p ? *p : NULL;
}

int gtcaca_textlist_can_draw(gtcaca_textlist_widget_t *textlist, unsigned int i)
{
  unsigned int current_view  = (textlist->view_size > 0) ? i / textlist->view_size : 0;
  unsigned int selected_view = (textlist->view_size > 0)
                               ? (unsigned int)textlist->selected_item / textlist->view_size : 0;
  return (current_view == selected_view) ? 1 : 0;
}

void gtcaca_textlist_selected_color(gtcaca_textlist_widget_t *textlist, unsigned int i)
{
  if (i == textlist->selected_item) {
    if (textlist->parent) {
      caca_set_color_ansi(gmo.cv, textlist->parent->color_focus_fg, textlist->parent->color_focus_bg);
    } else {
      caca_set_color_ansi(gmo.cv, textlist->color_focus_fg, textlist->color_focus_bg);
    }
  } else {
    if (textlist->parent) {
      caca_set_color_ansi(gmo.cv, textlist->parent->color_nonfocus_fg, textlist->parent->color_nonfocus_bg);
    } else {
      caca_set_color_ansi(gmo.cv, textlist->color_nonfocus_fg, textlist->color_nonfocus_bg);
    }
  }
}

void gtcaca_textlist_clear(gtcaca_textlist_widget_t *textlist)
{
  utarray_clear(textlist->list);
  textlist->selected_item = 0;
}

void gtcaca_textlist_draw(gtcaca_textlist_widget_t *textlist)
{
  unsigned int n = utarray_len(textlist->list), i;
  unsigned int vis = 0, sel_pos = 0, page, top;
  int view = (int)textlist->view_size;

  /* We draw the background */
  gtcaca_widget_colorize_from_parent(GTCACA_WIDGET(textlist));
  if (textlist->view_size > 0)
    caca_fill_box(gmo.cv, textlist->x, textlist->y,
                  textlist->width, (int)textlist->view_size, ' ');

  if (view <= 0) return;

  /* Position of the selected item among the currently-visible (matching) rows,
     so paging follows the filtered view rather than the full list. */
  for (i = 0; i < n; i++) {
    if (!_matches(textlist, i)) continue;
    if (i == textlist->selected_item) sel_pos = vis;
    vis++;
  }
  page = sel_pos / (unsigned int)view;
  top = page * (unsigned int)view;

  /* Draw the matching rows on the current page. */
  {
    unsigned int shown = 0, seen = 0;
    for (i = 0; i < n; i++) {
      if (!_matches(textlist, i)) continue;
      if (seen++ < top) continue;
      if (shown >= (unsigned int)view) break;
      gtcaca_textlist_selected_color(textlist, i);
      caca_printf(gmo.cv, textlist->x, textlist->y + shown, "%s", _item(textlist, i));
      shown++;
    }
  }

  /* Search prompt on the row just below the list. */
  if (textlist->searching) {
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_WHITE);
    caca_printf(gmo.cv, textlist->x, textlist->y + view, "/%s ", textlist->search_query);
  }
}
