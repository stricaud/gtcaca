#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gtcaca/editor.h>
#include <gtcaca/main.h>

/* ── JSON colouriser ───────────────────────────────────────────────────────── */

/* End of a JSON string starting at p ('"'); returns index just past the close. */
static int _str_end(const char *t, int len, int p)
{
  int q = p + 1;
  while (q < len) {
    if (t[q] == '\\' && q + 1 < len) { q += 2; continue; }
    if (t[q] == '"') return q + 1;
    if (t[q] == '\n') return q;     /* JSON strings don't span lines */
    q++;
  }
  return q;
}

void _gtcaca_editor_colorize_json(gtcaca_editor_widget_t *w)
{
  const char *t = w->text;
  int len = w->length, i;

  if (w->styles_cap < len) {
    unsigned char *p = realloc(w->styles, (size_t)(len > 0 ? len : 1));
    if (!p) return;
    w->styles = p;
    w->styles_cap = len > 0 ? len : 1;
  }

  i = 0;
  while (i < len) {
    char c = t[i];
    if (c == '"') {
      int end = _str_end(t, len, i), look = end, style;
      while (look < len && (t[look] == ' ' || t[look] == '\t')) look++;
      style = (look < len && t[look] == ':') ? GTCACA_EDITOR_STYLE_KEYWORD   /* property key */
                                             : GTCACA_EDITOR_STYLE_STRING;   /* string value */
      while (i < end) w->styles[i++] = (unsigned char)style;
    } else if (c == '{' || c == '}' || c == '[' || c == ']') {
      w->styles[i++] = GTCACA_EDITOR_STYLE_BRACKET;
    } else if (c == ':' || c == ',') {
      w->styles[i++] = GTCACA_EDITOR_STYLE_OPERATOR;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
      while (i < len) {
        char n = t[i];
        if ((n >= '0' && n <= '9') || n == '.' || n == '-' || n == '+' || n == 'e' || n == 'E')
          w->styles[i++] = GTCACA_EDITOR_STYLE_NUMBER;
        else break;
      }
    } else if (!strncmp(t + i, "true", 4) || !strncmp(t + i, "null", 4)) {
      int k; for (k = 0; k < 4 && i < len; k++) w->styles[i++] = GTCACA_EDITOR_STYLE_NUMBER;
    } else if (!strncmp(t + i, "false", 5)) {
      int k; for (k = 0; k < 5 && i < len; k++) w->styles[i++] = GTCACA_EDITOR_STYLE_NUMBER;
    } else {
      w->styles[i++] = GTCACA_EDITOR_STYLE_DEFAULT;
    }
  }
}

/* ── JSON folder: fold levels from brace/bracket nesting ───────────────────── */

void gtcaca_editor_fold_json(gtcaca_editor_widget_t *w)
{
  int n, i, level_current = 0;

  _gtcaca_editor_ensure_meta(w);
  n = w->lines_meta_len;

  for (i = 0; i < n; i++) {
    int start = gtcaca_editor_position_from_line(w, i);
    int end   = gtcaca_editor_get_line_end_position(w, i);
    int level_line = level_current;
    int level_next = level_current;
    int min_level  = level_current;
    int p, in_str = 0;
    int lvl, fl;

    for (p = start; p < end; p++) {
      char c = w->text[p];
      if (in_str) {
        if (c == '\\' && p + 1 < end) { p++; continue; }
        if (c == '"') in_str = 0;
        continue;
      }
      if (c == '"') { in_str = 1; continue; }
      if (c == '{' || c == '[') level_next++;
      else if (c == '}' || c == ']') { level_next--; if (level_next < min_level) min_level = level_next; }
    }

    /* A line is a fold header when it ends deeper than it started. The level
       number is the shallower of where it starts/ends so closing-brace lines
       sit at the parent level. */
    lvl = (min_level < level_line ? min_level : level_line);
    if (lvl < 0) lvl = 0;
    if (lvl > GTCACA_EDITOR_FOLDLEVELNUMBERMASK) lvl = GTCACA_EDITOR_FOLDLEVELNUMBERMASK;
    fl = GTCACA_EDITOR_FOLDLEVELBASE + lvl;
    if (level_next > level_line) fl |= GTCACA_EDITOR_FOLDLEVELHEADERFLAG;

    w->lines_meta[i].fold_level = fl;
    level_current = level_next;
  }
  w->fold_dirty = 1;
}

/* ── mode toggle ───────────────────────────────────────────────────────────── */

void gtcaca_editor_set_json_mode(gtcaca_editor_widget_t *w, int on)
{
  w->json_mode = on ? 1 : 0;
  w->colorize_dirty = 1;
}
int gtcaca_editor_get_json_mode(gtcaca_editor_widget_t *w) { return w->json_mode; }
