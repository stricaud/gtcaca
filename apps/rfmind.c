/*
 * rfmind — a tiny FreeMind-compatible mind-map editor built on the gtcaca
 * `mindmap` widget. Loads and saves FreeMind .mm files (XML).
 *
 * Usage: rfmind [file.mm]
 *
 * Keys:
 *   Arrows      navigate (Left folds / climbs, Right unfolds / descends)
 *   Tab         add a child to the current node (then edit it)
 *   Enter       add a sibling after the current node (then edit it)
 *   e / F2      edit the current node's text
 *   Space       fold / unfold
 *   Delete      delete the current node (and its subtree)
 *   C-s         save        C-x C-c (or q) quit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <caca.h>
#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/mindmap.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define KEY_CTRL_S 0x13

static gtcaca_mindmap_widget_t  *g_mm;
static gtcaca_statusbar_widget_t *g_bar;
static char g_path[PATH_MAX] = "";
static int  g_dirty = 0;

/* ── XML entities ───────────────────────────────────────────────────────────── */
/* decode &amp; &lt; &gt; &quot; &apos; and numeric &#NN; / &#xHH; in place-ish */
static char *xml_decode(const char *s)
{
  char *out = malloc(strlen(s) + 1), *o = out;
  if (!out) return NULL;
  while (*s) {
    if (*s == '&') {
      if      (!strncmp(s, "&amp;", 5))  { *o++ = '&';  s += 5; }
      else if (!strncmp(s, "&lt;", 4))   { *o++ = '<';  s += 4; }
      else if (!strncmp(s, "&gt;", 4))   { *o++ = '>';  s += 4; }
      else if (!strncmp(s, "&quot;", 6)) { *o++ = '"';  s += 6; }
      else if (!strncmp(s, "&apos;", 6)) { *o++ = '\''; s += 6; }
      else if (s[1] == '#') {
        int base = (s[2] == 'x' || s[2] == 'X') ? 16 : 10;
        const char *p = s + (base == 16 ? 3 : 2);
        long v = strtol(p, (char **)&p, base);
        if (*p == ';') p++;
        *o++ = (char)v; s = p;
      } else *o++ = *s++;
    } else *o++ = *s++;
  }
  *o = '\0';
  return out;
}

/* write `s` to `f` with XML attribute escaping */
static void xml_write_escaped(FILE *f, const char *s)
{
  for (; *s; s++) {
    switch (*s) {
    case '&':  fputs("&amp;", f);  break;
    case '<':  fputs("&lt;", f);   break;
    case '>':  fputs("&gt;", f);   break;
    case '"':  fputs("&quot;", f); break;
    case '\n': fputs("&#xa;", f);  break;
    case '\t': fputs("&#x9;", f);  break;
    default:   fputc(*s, f);
    }
  }
}

/* ── save ───────────────────────────────────────────────────────────────────── */
static void save_node(FILE *f, gtcaca_mm_node_t *n, int depth)
{
  int i;
  for (i = 0; i < depth; i++) fputc(' ', f);
  fputs("<node TEXT=\"", f);
  xml_write_escaped(f, n->text);
  fputc('"', f);
  if (n->folded && n->nkids) fputs(" FOLDED=\"true\"", f);
  if (n->nkids == 0) { fputs("/>\n", f); return; }
  fputs(">\n", f);
  for (i = 0; i < n->nkids; i++) save_node(f, n->kids[i], depth + 1);
  for (i = 0; i < depth; i++) fputc(' ', f);
  fputs("</node>\n", f);
}

static int save_mm(const char *path)
{
  FILE *f = fopen(path, "w");
  if (!f) return 0;
  fputs("<map version=\"1.0.1\">\n", f);
  save_node(f, gtcaca_mindmap_root(g_mm), 0);
  fputs("</map>\n", f);
  fclose(f);
  return 1;
}

/* ── load ───────────────────────────────────────────────────────────────────── */
/* read an attribute value: finds NAME=" and copies up to the next " */
static int attr_get(const char *tag, const char *name, char *out, int outlen)
{
  char needle[32];
  const char *p, *q;
  snprintf(needle, sizeof needle, "%s=\"", name);
  p = strstr(tag, needle);
  if (!p) return 0;
  p += strlen(needle);
  q = strchr(p, '"');
  if (!q) return 0;
  { int n = (int)(q - p); if (n > outlen - 1) n = outlen - 1; memcpy(out, p, n); out[n] = '\0'; }
  return 1;
}

/* Parse a FreeMind .mm: walk the text, tracking <node ...> open / </node> close
   and self-closing <node .../>. Other tags are ignored. */
static int load_mm(const char *path)
{
  FILE *f = fopen(path, "rb");
  long sz; char *buf, *p;
  gtcaca_mm_node_t *stack[256]; int sp = 0;
  int got_root = 0;
  if (!f) return 0;
  fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
  buf = malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return 0; }
  sz = (long)fread(buf, 1, (size_t)sz, f); buf[sz] = '\0';
  fclose(f);

  gtcaca_mindmap_clear(g_mm, "");
  p = buf;
  while ((p = strchr(p, '<')) != NULL) {
    if (!strncmp(p, "<!--", 4)) { char *e = strstr(p, "-->"); p = e ? e + 3 : p + 4; continue; }
    if (!strncmp(p, "</node>", 7)) { if (sp > 0) sp--; p += 7; continue; }
    if (!strncmp(p, "<node", 5) && (p[5] == ' ' || p[5] == '\t' || p[5] == '\n' || p[5] == '>')) {
      char *gt = strchr(p, '>');
      char tag[4096], textraw[2048], folded[16];
      int selfclose;
      gtcaca_mm_node_t *node;
      if (!gt) break;
      selfclose = (gt > p && gt[-1] == '/');
      { int n = (int)(gt - p); if (n > (int)sizeof tag - 1) n = (int)sizeof tag - 1; memcpy(tag, p, n); tag[n] = '\0'; }
      textraw[0] = '\0';
      attr_get(tag, "TEXT", textraw, sizeof textraw);
      { char *dec = xml_decode(textraw);
        if (!got_root) { node = gtcaca_mindmap_root(g_mm); gtcaca_mindmap_set_text(node, dec); got_root = 1; }
        else           { node = gtcaca_mindmap_add_child(g_mm, sp > 0 ? stack[sp - 1] : gtcaca_mindmap_root(g_mm), dec); }
        free(dec); }
      if (attr_get(tag, "FOLDED", folded, sizeof folded) && !strcmp(folded, "true") && node)
        node->folded = 1;
      if (!selfclose && node && sp < 256) stack[sp++] = node;
      p = gt + 1;
      continue;
    }
    p++;   /* some other tag — skip past '<' */
  }
  free(buf);
  gtcaca_mindmap_select(g_mm, gtcaca_mindmap_root(g_mm));
  return got_root;
}

/* ── minibuffer prompt ──────────────────────────────────────────────────────── */
static int prompt(const char *label, char *out, int outlen, const char *initial)
{
  int W = caca_get_canvas_width(gmo.cv), H = caca_get_canvas_height(gmo.cv), len = 0;
  caca_event_t ev;
  out[0] = '\0';
  if (initial) { snprintf(out, (size_t)outlen, "%s", initial); len = (int)strlen(out); }
  for (;;) {
    int x;
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_WHITE); caca_set_attr(gmo.cv, 0);
    for (x = 0; x < W; x++) caca_put_char(gmo.cv, x, H - 1, ' ');
    caca_printf(gmo.cv, 0, H - 1, "%s%s", label, out);
    caca_refresh_display(gmo.dp);
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    {
      int k = caca_get_event_key_ch(&ev);
      if (k == CACA_KEY_RETURN || k == 10) return 1;
      if (k == CACA_KEY_ESCAPE) return 0;
      if ((k == CACA_KEY_BACKSPACE || k == CACA_KEY_DELETE) && len > 0) out[--len] = '\0';
      else if (k >= 32 && k < 127 && len < outlen - 1) { out[len++] = (char)k; out[len] = '\0'; }
    }
  }
}

static void edit_node(gtcaca_mm_node_t *n)
{
  char buf[1024];
  if (!n) return;
  if (prompt("Node text: ", buf, sizeof buf, n->text)) { gtcaca_mindmap_set_text(n, buf); g_dirty = 1; }
}

static void refresh_bar(void)
{
  char t[256];
  snprintf(t, sizeof t, " rfmind  %s%s   Tab:child Enter:sibling e:edit +/-:fold Del:remove C-s:save q:quit",
           g_path[0] ? g_path : "(unsaved)", g_dirty ? " *" : "");
  gtcaca_statusbar_set_text(g_bar, t);
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t *win;
  caca_event_t ev;
  int quit = 0, ctrl_x = 0, ch, cw;

  gtcaca_init(&argc, &argv);
  gmo.theme.textviewfocus.bg = CACA_BLACK; gmo.theme.textviewfocus.fg = CACA_LIGHTGRAY;
  gmo.theme.textview.bg = CACA_BLACK;      gmo.theme.textview.fg = CACA_LIGHTGRAY;
  app = gtcaca_application_new("rfmind");
  cw = caca_get_canvas_width(gmo.cv); ch = caca_get_canvas_height(gmo.cv);
  win = gtcaca_window_new((gtcaca_widget_t *)app, NULL, 0, 0, cw, ch - 1);

  g_mm = gtcaca_mindmap_new(GTCACA_WIDGET(win), 0, 0, win->width, win->height);
  gtcaca_mindmap_set_title(g_mm, "rfmind");
  g_bar = gtcaca_statusbar_new("");

  if (argc > 1) {
    snprintf(g_path, sizeof g_path, "%s", argv[1]);
    if (!load_mm(g_path)) gtcaca_mindmap_set_text(gtcaca_mindmap_root(g_mm), "New Mindmap");
  } else {
    gtcaca_mindmap_set_text(gtcaca_mindmap_root(g_mm), "New Mindmap");
  }
  gtcaca_window_set_focused_child(win, GTCACA_WIDGET(g_mm));

  while (!quit) {
    int k;
    refresh_bar();
    gtcaca_redraw();
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    k = caca_get_event_key_ch(&ev);

    if (ctrl_x) {
      ctrl_x = 0;
      if (k == CACA_KEY_CTRL_C) quit = 1;
      continue;
    }
    switch (k) {
    case CACA_KEY_CTRL_X: ctrl_x = 1; break;
    case KEY_CTRL_S:
      if (!g_path[0]) { char b[PATH_MAX]; if (!prompt("Save as: ", b, sizeof b, "") || !b[0]) break;
                        snprintf(g_path, sizeof g_path, "%s", b); }
      gtcaca_statusbar_set_text(g_bar, save_mm(g_path) ? " Saved" : " Save failed");
      if (1) g_dirty = 0;
      gtcaca_redraw();
      caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, 700000);
      break;
    case 'q': case 'Q': quit = 1; break;
    case CACA_KEY_TAB: {                      /* add a child (unfold the parent) */
      gtcaca_mm_node_t *sel = gtcaca_mindmap_selected(g_mm), *n;
      if (sel) sel->folded = 0;
      n = gtcaca_mindmap_add_child(g_mm, sel, "");
      if (n) { gtcaca_mindmap_select(g_mm, n); g_dirty = 1; edit_node(n); }
      break;
    }
    case CACA_KEY_RETURN: case 10: {          /* add a sibling */
      gtcaca_mm_node_t *n = gtcaca_mindmap_add_sibling(g_mm, gtcaca_mindmap_selected(g_mm), "");
      if (n) { gtcaca_mindmap_select(g_mm, n); g_dirty = 1; edit_node(n); }
      break;
    }
    case 'e': case 'E':                       /* edit text */
      edit_node(gtcaca_mindmap_selected(g_mm));
      break;
    case CACA_KEY_DELETE: case CACA_KEY_BACKSPACE: {
      gtcaca_mm_node_t *n = gtcaca_mindmap_selected(g_mm);
      if (n && n != gtcaca_mindmap_root(g_mm)) { gtcaca_mindmap_delete(g_mm, n); g_dirty = 1; }
      break;
    }
    default:                                  /* arrows + Space handled by the widget */
      gtcaca_mindmap_key(g_mm, k, NULL);
      break;
    }
  }
  caca_set_mouse(gmo.dp, 0);
  caca_free_display(gmo.dp);
  return 0;
}
