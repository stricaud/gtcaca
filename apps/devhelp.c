/*
 * devhelp.c — gtcaca API documentation browser
 *
 * Layout (80x24):
 *   Left panel  (~32 cols): live-filtering function list with search entry
 *   Right panel (rest):     scrollable TextView showing selected function docs
 *
 * Controls:
 *   Tab         – cycle focus within the current panel
 *   Ctrl+N/P    – switch between left and right panel
 *   Up/Down     – navigate list / scroll doc view
 *   Enter       – select function in list (show docs)
 *   Q           – quit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/entry.h>
#include <gtcaca/textlist.h>
#include <gtcaca/textview.h>
#include <gtcaca/label.h>
#include <gtcaca/separator.h>
#include <gtcaca/statusbar.h>

#include "gtcaca_api.h"

/* ── Global widget pointers ──────────────────────────────────────────────── */

static gtcaca_window_widget_t   *g_left_win   = NULL;
static gtcaca_window_widget_t   *g_right_win  = NULL;
static gtcaca_entry_widget_t    *g_search     = NULL;
static gtcaca_textlist_widget_t *g_funclist   = NULL;
static gtcaca_textview_widget_t *g_detail     = NULL;

/* ── Filter state ────────────────────────────────────────────────────────── */

static char g_filter[256];
static char g_filtered_names[1024][64];
static int  g_filtered_count = 0;

/* ── Word-wrap helper ────────────────────────────────────────────────────── */

/*
 * Append `text` to `tv` line by line, each line prefixed with `prefix`.
 * Lines are broken on whitespace boundaries to fit within `width` chars
 * (including the prefix).
 */
static void _append_wrapped(gtcaca_textview_widget_t *tv,
                             const char *prefix,
                             const char *text,
                             int width)
{
  int prefix_len = (int)strlen(prefix);
  int max_line   = width - prefix_len;
  char linebuf[512];

  if (max_line < 8) max_line = 8;  /* safety floor */

  /* Walk through the text respecting embedded newlines. */
  const char *p = text;
  while (*p) {
    /* find end of logical line (next \n or end of string) */
    const char *nl = strchr(p, '\n');
    int seg_len = nl ? (int)(nl - p) : (int)strlen(p);

    /* word-wrap the segment */
    while (seg_len > 0) {
      int take = seg_len;
      if (take > max_line) {
        /* look for last space within max_line */
        take = max_line;
        int i;
        for (i = max_line - 1; i > 0; i--) {
          if (p[i] == ' ') { take = i; break; }
        }
      }
      snprintf(linebuf, sizeof(linebuf), "%s%.*s", prefix, take, p);
      gtcaca_textview_append(tv, linebuf);
      /* skip leading spaces on next piece */
      p       += take;
      seg_len -= take;
      while (*p == ' ' && seg_len > 0) { p++; seg_len--; }
    }

    if (nl) p = nl + 1;
    else    break;
  }
}

/* ── Show a single API entry in the detail view ──────────────────────────── */

static void _show_entry(const gtcaca_api_entry_t *e)
{
  int detail_width;

  gtcaca_textview_clear(g_detail);

  /* Derive the usable width from the textview */
  detail_width = g_detail->width - 2;
  if (detail_width < 20) detail_width = 20;

  gtcaca_textview_append(g_detail, "");
  {
    char hdr[128];
    snprintf(hdr, sizeof(hdr), "  %s", e->name);
    gtcaca_textview_append(g_detail, hdr);
  }
  gtcaca_textview_append(g_detail, "");

  {
    char mod[64];
    snprintf(mod, sizeof(mod), "  Module: %s", e->module);
    gtcaca_textview_append(g_detail, mod);
  }
  gtcaca_textview_append(g_detail, "");

  gtcaca_textview_append(g_detail, "  Signature:");
  _append_wrapped(g_detail, "    ", e->signature, detail_width);
  gtcaca_textview_append(g_detail, "");

  gtcaca_textview_append(g_detail, "  Description:");
  _append_wrapped(g_detail, "    ", e->description, detail_width);
  gtcaca_textview_append(g_detail, "");

  /* Parameters — each encoded as "name\tdesc\n" */
  if (e->params && e->params[0]) {
    gtcaca_textview_append(g_detail, "  Parameters:");
    /* walk the params string */
    const char *pp = e->params;
    while (*pp) {
      /* find tab */
      const char *tab = strchr(pp, '\t');
      const char *lf  = strchr(pp, '\n');
      if (!tab || (lf && lf < tab)) {
        /* malformed entry, skip to next \n */
        if (lf) pp = lf + 1; else break;
        continue;
      }
      char pname[64] = {0};
      int  pname_len = (int)(tab - pp);
      if (pname_len >= (int)sizeof(pname)) pname_len = (int)sizeof(pname) - 1;
      strncpy(pname, pp, pname_len);

      const char *desc_start = tab + 1;
      int desc_len = lf ? (int)(lf - desc_start) : (int)strlen(desc_start);
      char desc_buf[256] = {0};
      if (desc_len >= (int)sizeof(desc_buf)) desc_len = (int)sizeof(desc_buf) - 1;
      strncpy(desc_buf, desc_start, desc_len);

      char param_line[320];
      snprintf(param_line, sizeof(param_line), "    %-12s  %s", pname, desc_buf);
      gtcaca_textview_append(g_detail, param_line);

      pp = lf ? lf + 1 : desc_start + desc_len;
    }
    gtcaca_textview_append(g_detail, "");
  }

  gtcaca_textview_append(g_detail, "  Returns:");
  _append_wrapped(g_detail, "    ", e->returns, detail_width);
  gtcaca_textview_append(g_detail, "");
}

/* ── Rebuild the function list applying filter ───────────────────────────── */

static void _rebuild_list(const char *filter)
{
  int i;
  char lower_filter[256];
  int fi = 0;

  /* lower-case the filter */
  for (i = 0; filter[i] && i < (int)sizeof(lower_filter) - 1; i++)
    lower_filter[i] = (char)tolower((unsigned char)filter[i]);
  lower_filter[i] = '\0';

  gtcaca_textlist_clear(g_funclist);
  g_filtered_count = 0;

  for (i = 0; i < gtcaca_api_count; i++) {
    const char *name = gtcaca_api[i].name;
    /* case-insensitive contains check */
    int match = 1;
    if (lower_filter[0]) {
      char lower_name[128];
      int j;
      for (j = 0; name[j] && j < (int)sizeof(lower_name) - 1; j++)
        lower_name[j] = (char)tolower((unsigned char)name[j]);
      lower_name[j] = '\0';
      match = (strstr(lower_name, lower_filter) != NULL);
    }
    if (match) {
      gtcaca_textlist_append(g_funclist, (char *)name);
      if (g_filtered_count < 1024) {
        strncpy(g_filtered_names[g_filtered_count], name,
                sizeof(g_filtered_names[0]) - 1);
        g_filtered_names[g_filtered_count][sizeof(g_filtered_names[0]) - 1] = '\0';
        g_filtered_count++;
      }
      fi++;
    }
  }
}

/* ── Callbacks ───────────────────────────────────────────────────────────── */

static int on_search_key(gtcaca_entry_widget_t *entry, int key, void *userdata)
{
  (void)key;
  (void)userdata;
  /* Always rebuild after any key — the entry widget has already updated
     its internal text buffer before this callback fires. */
  _rebuild_list(entry->text);
  /* If there are results, show the first one */
  if (g_filtered_count > 0) {
    int i;
    for (i = 0; i < gtcaca_api_count; i++) {
      if (strcmp(gtcaca_api[i].name, g_filtered_names[0]) == 0) {
        _show_entry(&gtcaca_api[i]);
        break;
      }
    }
  } else {
    gtcaca_textview_clear(g_detail);
  }
  return 0;
}

static int on_func_select(gtcaca_textlist_widget_t *tl, int key, void *userdata)
{
  (void)userdata;

  /* Update docs on navigation (live preview) and on explicit Enter.
     Never steal focus away from the left panel — the user stays in
     the search/list and can keep typing without pressing Ctrl+N/P. */
  if (key != CACA_KEY_RETURN &&
      key != CACA_KEY_UP &&
      key != CACA_KEY_DOWN) return 0;

  char **sel = (char **)gtcaca_textlist_get_text_selected(tl);
  if (!sel || !*sel) return 0;

  int i;
  for (i = 0; i < gtcaca_api_count; i++) {
    if (strcmp(gtcaca_api[i].name, *sel) == 0) {
      _show_entry(&gtcaca_api[i]);
      break;
    }
  }
  return 0;
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
  int cw, ch, lw, rw, h;

  gtcaca_init(&argc, &argv);

  cw = caca_get_canvas_width(gmo.cv);
  ch = caca_get_canvas_height(gmo.cv);
  lw = 32;           /* left panel width */
  rw = cw - lw - 1; /* right panel width (1 col gap between panels) */
  h  = ch - 3;       /* panel height: leave 1 row top (app bar) + 1 bottom (statusbar) + 1 extra */

  if (rw < 20) rw = 20;
  if (h  < 8)  h  = 8;

  gtcaca_application_new("gtcaca API Browser");

  /* ── Left panel ── */
  g_left_win = gtcaca_window_new(NULL, "Functions", 0, 1, lw, h);

  gtcaca_label_new(GTCACA_WIDGET(g_left_win), "Search:", 1, 1);
  g_search = gtcaca_entry_new(GTCACA_WIDGET(g_left_win), 9, 1, lw - 11);
  gtcaca_entry_key_cb_register(g_search, on_search_key, NULL);

  gtcaca_separator_new(GTCACA_WIDGET(g_left_win), 1, 2, lw - 2, 0);

  g_funclist = gtcaca_textlist_new(GTCACA_WIDGET(g_left_win), 1, 3);
  gtcaca_textlist_widget_set_view_size(g_funclist, (unsigned int)(h - 5));
  gtcaca_textlist_key_cb_register(g_funclist, on_func_select, NULL);

  /* ── Right panel ── */
  g_right_win = gtcaca_window_new(NULL, "Documentation", lw, 1, rw, h);
  g_detail    = gtcaca_textview_new(GTCACA_WIDGET(g_right_win), 1, 1, rw - 2, h - 2);

  /* ── Status bar ── */
  gtcaca_statusbar_new(
    "Type to search | Up/Down: browse | Ctrl+N/P: switch panel | Q: quit");

  /* ── Populate list and show first entry ── */
  memset(g_filter, 0, sizeof(g_filter));
  _rebuild_list("");

  if (gtcaca_api_count > 0)
    _show_entry(&gtcaca_api[0]);

  /* ── Initial focus: left window, search entry ── */
  gtcaca_window_set_focus(g_left_win);
  gtcaca_window_set_focused_child(g_left_win, GTCACA_WIDGET(g_search));

  gtcaca_main();
  return 0;
}
