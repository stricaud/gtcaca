#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/entry.h>
#include <gtcaca/main.h>

static int _gtcaca_entry_private_key_press(gtcaca_entry_widget_t *entry, int key, void *userdata)
{
  int tlen = (int)strlen(entry->text);
  int visible = entry->width - 2;

  if (key == CACA_KEY_LEFT) {
    if (entry->cursor_pos > 0) entry->cursor_pos--;
  } else if (key == CACA_KEY_RIGHT) {
    if (entry->cursor_pos < tlen) entry->cursor_pos++;
  } else if (key == CACA_KEY_HOME) {
    entry->cursor_pos = 0;
  } else if (key == CACA_KEY_END) {
    entry->cursor_pos = tlen;
  } else if (key == CACA_KEY_BACKSPACE || key == 8 || key == 127) {
    if (entry->cursor_pos > 0) {
      memmove(entry->text + entry->cursor_pos - 1,
              entry->text + entry->cursor_pos,
              tlen - entry->cursor_pos + 1);
      entry->cursor_pos--;
    }
  } else if (key >= 32 && key <= 126) {
    if (tlen < entry->max_length) {
      memmove(entry->text + entry->cursor_pos + 1,
              entry->text + entry->cursor_pos,
              tlen - entry->cursor_pos + 1);
      entry->text[entry->cursor_pos] = (char)key;
      entry->cursor_pos++;
    }
  }

  /* Adjust scroll to keep cursor visible */
  if (entry->cursor_pos < entry->scroll_offset)
    entry->scroll_offset = entry->cursor_pos;
  if (visible > 0 && entry->cursor_pos >= entry->scroll_offset + visible)
    entry->scroll_offset = entry->cursor_pos - visible + 1;

  return 0;
}

gtcaca_entry_widget_t *gtcaca_entry_new(gtcaca_widget_t *parent, int x, int y, int width)
{
  gtcaca_entry_widget_t *entry;

  entry = malloc(sizeof(gtcaca_entry_widget_t));
  if (!entry) {
    fprintf(stderr, "Error: Cannot allocate entry\n");
    return NULL;
  }

  entry->id = gtcaca_get_newid();
  entry->has_focus = 0;
  entry->is_visible = 1;
  entry->type = GTCACA_WIDGET_ENTRY;
  entry->parent = parent;
  entry->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(entry), x, y);
  entry->width = width;
  entry->height = 1;

  entry->color_focus_fg = gmo.theme.entryfocus.fg;
  entry->color_focus_bg = gmo.theme.entryfocus.bg;
  entry->color_nonfocus_fg = gmo.theme.entry.fg;
  entry->color_nonfocus_bg = gmo.theme.entry.bg;

  memset(entry->text, 0, sizeof(entry->text));
  entry->cursor_pos = 0;
  entry->scroll_offset = 0;
  entry->max_length = GTCACA_ENTRY_MAX_LEN;
  entry->secret = 0;
  entry->private_key_cb = _gtcaca_entry_private_key_press;
  entry->key_cb = NULL;
  entry->key_cb_userdata = NULL;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(entry));

  return entry;
}

void gtcaca_entry_draw(gtcaca_entry_widget_t *entry)
{
  int visible = entry->width - 2;
  int tlen = (int)strlen(entry->text);
  int i;
  char c;

  if (visible <= 0) return;

  gtcaca_widget_colorize(GTCACA_WIDGET(entry));

  caca_put_char(gmo.cv, entry->x, entry->y, '[');

  for (i = 0; i < visible; i++) {
    int ti = i + entry->scroll_offset;
    c = (ti < tlen) ? entry->text[ti] : ' ';
    if (entry->secret && ti < tlen) c = '*';

    if (entry->has_focus && ti == entry->cursor_pos) {
      caca_set_color_ansi(gmo.cv, entry->color_focus_bg, entry->color_focus_fg);
      caca_put_char(gmo.cv, entry->x + 1 + i, entry->y, (tlen == 0 || ti >= tlen) ? '_' : c);
      gtcaca_widget_colorize(GTCACA_WIDGET(entry));
    } else {
      caca_put_char(gmo.cv, entry->x + 1 + i, entry->y, c);
    }
  }

  caca_put_char(gmo.cv, entry->x + visible + 1, entry->y, ']');
}

int gtcaca_entry_key_cb_register(gtcaca_entry_widget_t *widget, gtcaca_entry_key_cb_t key_cb, void *userdata)
{
  widget->key_cb = key_cb;
  widget->key_cb_userdata = userdata;
  return 0;
}

const char *gtcaca_entry_get_text(gtcaca_entry_widget_t *entry)
{
  return entry->text;
}

void gtcaca_entry_set_secret(gtcaca_entry_widget_t *entry, int secret)
{
  entry->secret = secret ? 1 : 0;
}

void gtcaca_entry_set_text(gtcaca_entry_widget_t *entry, const char *text)
{
  int visible = entry->width - 2;
  strncpy(entry->text, text, entry->max_length);
  entry->text[entry->max_length] = '\0';
  entry->cursor_pos = (int)strlen(entry->text);
  entry->scroll_offset = 0;
  if (visible > 0 && entry->cursor_pos >= visible)
    entry->scroll_offset = entry->cursor_pos - visible + 1;
}
