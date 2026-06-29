#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <caca.h>

#include <gtcaca/theme.h>
#include <gtcaca/main.h>
#include <gtcaca/textlist.h>
#include <gtcaca/window.h>
#include <gtcaca/button.h>
#include <gtcaca/label.h>
#include <gtcaca/application.h>
#include <gtcaca/entry.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/radiobutton.h>
#include <gtcaca/progressbar.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/textview.h>
#include <gtcaca/combobox.h>
#include <gtcaca/menu.h>
#include <gtcaca/image.h>
#include <gtcaca/spinner.h>
#include <gtcaca/scale.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/switch.h>
#include <gtcaca/frame.h>
#include <gtcaca/separator.h>
#include <gtcaca/expander.h>
#include <gtcaca/editor.h>
#include <gtcaca/sparkline.h>
#include <gtcaca/gauge.h>
#include <gtcaca/barchart.h>
#include <gtcaca/tree.h>
#include <gtcaca/table.h>
#include <gtcaca/map.h>
#include <gtcaca/tabs.h>
#include <gtcaca/mindmap.h>
#include <gtcaca/segdisplay.h>
#include <gtcaca/linechart.h>
#include <gtcaca/hexview.h>
#include <gtcaca/calendar.h>
#include <gtcaca/dialog.h>
#include <gtcaca/filechooser.h>

gmo_t gmo;

static int g_quit_requested = 0;

void gtcaca_main_quit(void)
{
  g_quit_requested = 1;
}

int gtcaca_init(int *argc, char ***argv)
{
  int retval;

  retval = gtcaca_theme_parse_ini(NULL);
  if (retval < 0) {
    return -1;
  }

  gmo.dp = caca_create_display(NULL);
  if (!gmo.dp) {
    fprintf(stderr, "Error, cannot create display!\n");
    return -1;
  }
  gmo.cv = caca_get_canvas(gmo.dp);
  gmo.log = gtcaca_log_init();
  gmo.id = 0;
  gmo.widgets_list = NULL;

  return 0;
}

void gtcaca_clear_screen(void)
{
  caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK);
  caca_fill_box(gmo.cv, 0, 0, caca_get_canvas_width(gmo.cv), caca_get_canvas_height(gmo.cv), ' ');
}

void _gtcaca_widget_redraw(gtcaca_widget_t *widget)
{
  switch (widget->type) {
  case GTCACA_WIDGET_APPLICATION:
    gtcaca_application_draw((gtcaca_application_widget_t *)widget);
    break;
  case GTCACA_WIDGET_WINDOW:
    gtcaca_window_draw((gtcaca_window_widget_t *)widget);
    break;
  case GTCACA_WIDGET_TEXTLIST:
    gtcaca_textlist_draw((gtcaca_textlist_widget_t *)widget);
    break;
  case GTCACA_WIDGET_BUTTON:
    gtcaca_button_draw((gtcaca_button_widget_t *)widget);
    break;
  case GTCACA_WIDGET_ENTRY:
    gtcaca_entry_draw((gtcaca_entry_widget_t *)widget);
    break;
  case GTCACA_WIDGET_CHECKBOX:
    gtcaca_checkbox_draw((gtcaca_checkbox_widget_t *)widget);
    break;
  case GTCACA_WIDGET_RADIOBUTTON:
    gtcaca_radiobutton_draw((gtcaca_radiobutton_widget_t *)widget);
    break;
  case GTCACA_WIDGET_PROGRESSBAR:
    gtcaca_progressbar_draw((gtcaca_progressbar_widget_t *)widget);
    break;
  case GTCACA_WIDGET_STATUSBAR:
    gtcaca_statusbar_draw((gtcaca_statusbar_widget_t *)widget);
    break;
  case GTCACA_WIDGET_TEXTVIEW:
    gtcaca_textview_draw((gtcaca_textview_widget_t *)widget);
    break;
  case GTCACA_WIDGET_COMBOBOX:
    gtcaca_combobox_draw((gtcaca_combobox_widget_t *)widget);
    break;
  case GTCACA_WIDGET_MENU:
    gtcaca_menu_draw((gtcaca_menu_widget_t *)widget);
    break;
  case GTCACA_WIDGET_LABEL:
    gtcaca_label_draw((gtcaca_label_widget_t *)widget);
    break;
  case GTCACA_WIDGET_IMAGE:
    gtcaca_image_draw((gtcaca_image_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SPINNER:
    gtcaca_spinner_draw((gtcaca_spinner_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SCALE:
    gtcaca_scale_draw((gtcaca_scale_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SPINBUTTON:
    gtcaca_spinbutton_draw((gtcaca_spinbutton_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SWITCH:
    gtcaca_switch_draw((gtcaca_switch_widget_t *)widget);
    break;
  case GTCACA_WIDGET_FRAME:
    gtcaca_frame_draw((gtcaca_frame_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SEPARATOR:
    gtcaca_separator_draw((gtcaca_separator_widget_t *)widget);
    break;
  case GTCACA_WIDGET_EXPANDER:
    gtcaca_expander_draw((gtcaca_expander_widget_t *)widget);
    break;
  case GTCACA_WIDGET_EDITOR:
    gtcaca_editor_draw((gtcaca_editor_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SPARKLINE:
    gtcaca_sparkline_draw((gtcaca_sparkline_widget_t *)widget);
    break;
  case GTCACA_WIDGET_GAUGE:
    gtcaca_gauge_draw((gtcaca_gauge_widget_t *)widget);
    break;
  case GTCACA_WIDGET_BARCHART:
    gtcaca_barchart_draw((gtcaca_barchart_widget_t *)widget);
    break;
  case GTCACA_WIDGET_TREE:
    gtcaca_tree_draw((gtcaca_tree_widget_t *)widget);
    break;
  case GTCACA_WIDGET_TABLE:
    gtcaca_table_draw((gtcaca_table_widget_t *)widget);
    break;
  case GTCACA_WIDGET_MAP:
    gtcaca_map_draw((gtcaca_map_widget_t *)widget);
    break;
  case GTCACA_WIDGET_TABS:
    gtcaca_tabs_draw((gtcaca_tabs_widget_t *)widget);
    break;
  case GTCACA_WIDGET_MINDMAP:
    gtcaca_mindmap_draw((gtcaca_mindmap_widget_t *)widget);
    break;
  case GTCACA_WIDGET_SEGDISPLAY:
    gtcaca_segdisplay_draw((gtcaca_segdisplay_widget_t *)widget);
    break;
  case GTCACA_WIDGET_LINECHART:
    gtcaca_linechart_draw((gtcaca_linechart_widget_t *)widget);
    break;
  case GTCACA_WIDGET_CALENDAR:
    gtcaca_calendar_draw((gtcaca_calendar_widget_t *)widget);
    break;
  case GTCACA_WIDGET_DIALOG:
    gtcaca_dialog_draw((gtcaca_dialog_widget_t *)widget);
    break;
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
    gtcaca_filechooser_draw((gtcaca_filechooser_widget_t *)widget);
    break;
  case GTCACA_WIDGET_HEXVIEW:
    gtcaca_hexview_draw((gtcaca_hexview_widget_t *)widget);
    break;
  }

}

/* ── true(ish)colour presenter: bypass libcaca's 16-colour terminal output ─────
 * libcaca only emits the 16 ANSI colours, but it still stores up to 4096 (12-bit)
 * per cell. When stdout is a tty we render the canvas ourselves with 24-bit (or
 * 256-colour) escapes, reading each cell's char + stored colour back from the
 * canvas, diffing against the previous frame so only changed cells repaint.
 * Set GTCACA_NO_TRUECOLOR=1 to fall back to libcaca's own output. */
static int       g_present = -2;     /* -2 uninit, 0 = use libcaca, 1 = our renderer */
static int       g_truecolor = 0;    /* 24-bit available (else 256-colour cube) */
static int       g_mouse_sgr = 0;    /* re-assert SGR mouse on each redraw while the UI loop runs */
static char     *g_ob = NULL;        /* output buffer */
static size_t    g_obcap = 0;

static int _present_enabled(void)
{
  if (g_present == -2) {
    const char *ct = getenv("COLORTERM");
    /* Overlay truecolour on top of libcaca's paint: libcaca keeps managing the
       alt screen / mouse / input, and we just repaint the cells in 24-bit colour
       (terminals without truecolour down-sample it — e.g. Apple Terminal still
       shows the right background). We deliberately do NOT use synchronised-update
       (?2026): Apple Terminal mishandles it, which swallowed the alt-screen + mouse
       setup. Off with GTCACA_NO_TRUECOLOR=1. */
    g_present   = (isatty(1) && !getenv("GTCACA_NO_TRUECOLOR")) ? 1 : 0;
    g_truecolor = ct && (strstr(ct, "truecolor") || strstr(ct, "24bit"));
  }
  return g_present == 1;
}

void gtcaca_present_shutdown(void)   /* restore the terminal on exit (libcaca handles the rest) */
{
  if (g_present == 1) {
    const char *s = "\033[0m\033[?25h";   /* reset colours, show the cursor */
    if (write(1, s, strlen(s)) < 0) { /* ignore */ }
  }
}

/* write the whole buffer, looping over short writes (a tty can accept fewer
   bytes than asked on a big frame — the cause of the bottom of a large screen
   never painting). */
static void _write_all(const char *buf, size_t n)
{
  size_t off = 0;
  while (off < n) {
    ssize_t w = write(1, buf + off, n - off);
    if (w > 0) { off += (size_t)w; continue; }
    if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;   /* retry; tty will drain */
    break;   /* a real error: give up */
  }
}

static int _utf8(char *b, uint32_t c)
{
  if (c < 0x80)    { b[0] = (char)c; return 1; }
  if (c < 0x800)   { b[0] = (char)(0xC0|(c>>6));  b[1] = (char)(0x80|(c&0x3f)); return 2; }
  if (c < 0x10000) { b[0] = (char)(0xE0|(c>>12)); b[1] = (char)(0x80|((c>>6)&0x3f)); b[2] = (char)(0x80|(c&0x3f)); return 3; }
  b[0] = (char)(0xF0|(c>>18)); b[1] = (char)(0x80|((c>>12)&0x3f)); b[2] = (char)(0x80|((c>>6)&0x3f)); b[3] = (char)(0x80|(c&0x3f)); return 4;
}

static int _emit_col(char *b, int rgb12, int fg)   /* 12-bit colour -> 24-bit or 256 escape */
{
  int r = ((rgb12>>8)&0xf)*17, g = ((rgb12>>4)&0xf)*17, bl = (rgb12&0xf)*17;
  if (g_truecolor) return sprintf(b, "\033[%d;2;%d;%d;%dm", fg ? 38 : 48, r, g, bl);
  return sprintf(b, "\033[%d;5;%dm", fg ? 38 : 48, 16 + 36*(r*5/255) + 6*(g*5/255) + (bl*5/255));
}

static void gtcaca_present(void)
{
  int w = caca_get_canvas_width(gmo.cv), h = caca_get_canvas_height(gmo.cv);
  int x, y, cfg = -1, cbg = -1;
  size_t need = (size_t)w * h * 64 + 64, n = 0;

  if (need > g_obcap) { free(g_ob); g_ob = malloc(need); if (!g_ob) { g_obcap = 0; return; } g_obcap = need; }

  /* Repaint every cell, every frame. A diff against the previous frame desyncs
     from the physical screen whenever anything else clears it, so cells looked
     "already painted" yet stayed black. Synchronised-update brackets (2026) make
     the whole frame land atomically on terminals that support them (a no-op on
     the rest), so a full repaint doesn't flicker. */
  n += (size_t)sprintf(g_ob+n, "\033[?25l\033[H");   /* hide cursor (we draw our caret); sync is opened in gtcaca_redraw */
  for (y = 0; y < h; y++) {
    n += (size_t)sprintf(g_ob+n, "\033[%d;1H", y+1);
    for (x = 0; x < w; x++) {
      uint32_t ch = caca_get_char(gmo.cv, x, y);
      uint32_t at = caca_get_attr(gmo.cv, x, y);
      int f  = (int)(uint16_t)caca_attr_to_rgb12_fg(at);
      int bg = (int)(uint16_t)caca_attr_to_rgb12_bg(at);
      if (ch < 0x20 || ch == 0x000fffffu) ch = ' ';      /* control / fullwidth-magic */
      if (f  != cfg) { n += (size_t)_emit_col(g_ob+n, f,  1); cfg = f; }
      if (bg != cbg) { n += (size_t)_emit_col(g_ob+n, bg, 0); cbg = bg; }
      n += (size_t)_utf8(g_ob+n, ch);
    }
  }
  n += (size_t)sprintf(g_ob+n, "\033[0m");
  _write_all(g_ob, n);
}

void gtcaca_redraw(void)
{
  gtcaca_widget_t *widget = NULL;
  gtcaca_widget_t *menu_widget = NULL;

  gtcaca_clear_screen();

  /* Draw all non-menu widgets first */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->is_visible && widget->type != GTCACA_WIDGET_MENU) {
      _gtcaca_widget_redraw(widget);
    }
  }

  /* Draw menus last so dropdowns appear on top */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->is_visible && widget->type == GTCACA_WIDGET_MENU) {
      _gtcaca_widget_redraw(widget);
    }
  }

  /* Let libcaca paint and keep managing the terminal (alt screen, mouse, input),
     then overlay our truecolour on top of the same cells. No synchronised-update
     wrapper — Apple Terminal mishandles it and that broke the alt screen + mouse. */
  caca_refresh_display(gmo.dp);
  if (_present_enabled()) gtcaca_present();

  /* Re-assert SGR mouse reporting AFTER the paint. Entering the alternate screen
     (and some terminals' refresh) resets mouse mode, so an enable done once at
     startup gets wiped when libcaca switches to the alt screen — which is exactly
     why the wheel stopped reaching us. Keeping it on every frame is cheap and
     robust. */
  if (g_mouse_sgr) { const char *s = "\033[?1002h\033[?1006h"; if (write(1, s, strlen(s)) < 0) { /* ignore */ } }
}

int _gtcaca_widget_handle_key_press(gtcaca_widget_t *widget, int key)
{
  gtcaca_textlist_widget_t *textlist;
  gtcaca_button_widget_t *button;
  gtcaca_application_widget_t *application;
  gtcaca_entry_widget_t *entry;
  gtcaca_checkbox_widget_t *checkbox;
  gtcaca_radiobutton_widget_t *rb;
  gtcaca_textview_widget_t *tv;
  gtcaca_combobox_widget_t *cb;
  gtcaca_menu_widget_t *menu;
  gtcaca_scale_widget_t *scale;
  gtcaca_spinbutton_widget_t *spinbutton;
  gtcaca_switch_widget_t *sw;
  gtcaca_expander_widget_t *exp;

  if (!widget->has_focus) return 0;

  switch (widget->type) {
  case GTCACA_WIDGET_APPLICATION:
    application = (gtcaca_application_widget_t *)widget;
    application->private_key_cb(application, key, NULL);
    break;
  case GTCACA_WIDGET_WINDOW:
    break;
  case GTCACA_WIDGET_TEXTLIST:
    textlist = (gtcaca_textlist_widget_t *)widget;
    textlist->private_key_cb(textlist, key, NULL);
    if (textlist->key_cb)
      textlist->key_cb(textlist, key, textlist->key_cb_userdata);
    break;
  case GTCACA_WIDGET_BUTTON:
    button = (gtcaca_button_widget_t *)widget;
    button->private_key_cb(button, key, NULL);
    if (button->key_cb)
      button->key_cb(button, key, NULL);
    break;
  case GTCACA_WIDGET_ENTRY:
    entry = (gtcaca_entry_widget_t *)widget;
    entry->private_key_cb(entry, key, NULL);
    if (entry->key_cb)
      entry->key_cb(entry, key, entry->key_cb_userdata);
    break;
  case GTCACA_WIDGET_CHECKBOX:
    checkbox = (gtcaca_checkbox_widget_t *)widget;
    checkbox->private_key_cb(checkbox, key, NULL);
    if (checkbox->key_cb)
      checkbox->key_cb(checkbox, key, checkbox->key_cb_userdata);
    break;
  case GTCACA_WIDGET_RADIOBUTTON:
    rb = (gtcaca_radiobutton_widget_t *)widget;
    rb->private_key_cb(rb, key, NULL);
    if (rb->key_cb)
      rb->key_cb(rb, key, rb->key_cb_userdata);
    break;
  case GTCACA_WIDGET_TEXTVIEW:
    tv = (gtcaca_textview_widget_t *)widget;
    tv->private_key_cb(tv, key, NULL);
    if (tv->key_cb)
      tv->key_cb(tv, key, tv->key_cb_userdata);
    break;
  case GTCACA_WIDGET_COMBOBOX:
    cb = (gtcaca_combobox_widget_t *)widget;
    cb->private_key_cb(cb, key, NULL);
    if (cb->key_cb)
      cb->key_cb(cb, key, cb->key_cb_userdata);
    break;
  case GTCACA_WIDGET_MENU:
    menu = (gtcaca_menu_widget_t *)widget;
    gtcaca_menu_handle_key(menu, key);
    break;
  case GTCACA_WIDGET_SCALE:
    scale = (gtcaca_scale_widget_t *)widget;
    gtcaca_scale_handle_key(scale, key);
    if (scale->cb) scale->cb(scale, scale->value, scale->cb_userdata);
    break;
  case GTCACA_WIDGET_SPINBUTTON:
    spinbutton = (gtcaca_spinbutton_widget_t *)widget;
    gtcaca_spinbutton_handle_key(spinbutton, key);
    if (spinbutton->cb) spinbutton->cb(spinbutton, spinbutton->value, spinbutton->cb_userdata);
    break;
  case GTCACA_WIDGET_SWITCH:
    sw = (gtcaca_switch_widget_t *)widget;
    if (key == ' ' || key == CACA_KEY_RETURN) {
      sw->active = !sw->active;
      if (sw->cb) sw->cb(sw, sw->active, sw->cb_userdata);
    }
    break;
  case GTCACA_WIDGET_EXPANDER:
    exp = (gtcaca_expander_widget_t *)widget;
    gtcaca_expander_handle_key(exp, key);
    break;
  case GTCACA_WIDGET_EDITOR: {
    gtcaca_editor_widget_t *ed = (gtcaca_editor_widget_t *)widget;
    int handled = 0;
    /* An open autocompletion list intercepts keys first; then the container
       (e.g. the Emacs keymap); whatever neither claims falls through to the
       widget's built-in editing keys. */
    if (gtcaca_editor_autoc_active(ed)) handled = _gtcaca_editor_autoc_key(ed, key);
    if (!handled && ed->key_cb) handled = ed->key_cb(ed, key, ed->key_cb_userdata);
    if (!handled)   ed->private_key_cb(ed, key, NULL);
    if (ed->update_cb) ed->update_cb(ed, ed->update_cb_userdata);
    break;
  }
  case GTCACA_WIDGET_TREE:
    gtcaca_tree_key((gtcaca_tree_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_TABLE:
    gtcaca_table_key((gtcaca_table_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_MAP:
    gtcaca_map_key((gtcaca_map_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_TABS:
    gtcaca_tabs_key((gtcaca_tabs_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_MINDMAP:
    gtcaca_mindmap_key((gtcaca_mindmap_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_PROGRESSBAR:
  case GTCACA_WIDGET_CALENDAR:
    gtcaca_calendar_key((gtcaca_calendar_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_DIALOG:
    gtcaca_dialog_key((gtcaca_dialog_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
    gtcaca_filechooser_key((gtcaca_filechooser_widget_t *)widget, key, NULL);
    break;
  case GTCACA_WIDGET_STATUSBAR:
  case GTCACA_WIDGET_LABEL:
  case GTCACA_WIDGET_IMAGE:
  case GTCACA_WIDGET_SPINNER:
  case GTCACA_WIDGET_FRAME:
  case GTCACA_WIDGET_SEPARATOR:
  case GTCACA_WIDGET_SPARKLINE:
  case GTCACA_WIDGET_GAUGE:
  case GTCACA_WIDGET_BARCHART:
  case GTCACA_WIDGET_SEGDISPLAY:
  case GTCACA_WIDGET_LINECHART:
    break;
  }

  return 0;
}

/* Widgets that act on Enter themselves; for these the focused child handles
   Enter and it should not be redirected to the window's default widget. */
static int _gtcaca_widget_claims_enter(gtcaca_widget_type_t type)
{
  switch (type) {
  case GTCACA_WIDGET_BUTTON:
  case GTCACA_WIDGET_COMBOBOX:
  case GTCACA_WIDGET_RADIOBUTTON:
  case GTCACA_WIDGET_SWITCH:
  case GTCACA_WIDGET_EXPANDER:
  case GTCACA_WIDGET_MENU:
    return 1;
  default:
    return 0;
  }
}

/* Trigger a widget's Enter action regardless of whether it currently holds
   focus (used to activate a window's default widget). */
static void _gtcaca_widget_activate(gtcaca_widget_t *widget)
{
  int saved = widget->has_focus;
  widget->has_focus = 1;
  _gtcaca_widget_handle_key_press(widget, CACA_KEY_RETURN);
  widget->has_focus = saved;
}

/* The editor widget needs Tab, Ctrl+N/P, arrows and Esc for itself; when it is
   focused the global shortcuts that use those keys step aside. */
static int _focused_is_editor(void)
{
  gtcaca_window_widget_t *win = gtcaca_window_get_current_focus();
  return win && win->focused_child &&
         win->focused_child->type == GTCACA_WIDGET_EDITOR;
}

int gtcaca_widgets_handle_key_press(int key)
{
  gtcaca_widget_t *widget = NULL;

  /* Enter: activate the focused window's default widget, unless the focused
     child consumes Enter itself. Lets a form submit from inside a text entry. */
  if (key == CACA_KEY_RETURN) {
    gtcaca_window_widget_t *win = gtcaca_window_get_current_focus();
    if (win && win->default_widget &&
        (!win->focused_child ||
         !_gtcaca_widget_claims_enter(win->focused_child->type))) {
      _gtcaca_widget_activate(win->default_widget);
      return 0;
    }
  }

  /* TAB: cycle focus to next focusable child in focused window */
  if (key == '\t' && !_focused_is_editor()) {
    gtcaca_window_widget_t *win = gtcaca_window_get_current_focus();
    if (win) gtcaca_window_focus_next_child(win);
    return 0;
  }

  /* Ctrl+N / Ctrl+P: switch between windows */
  if ((key == '\x0e' || key == '\x10') && !_focused_is_editor()) {
    gtcaca_window_widget_t *win = gtcaca_window_get_current_focus();
    if (win) {
      gtcaca_window_widget_t *next = (key == '\x0e')
        ? gtcaca_window_get_next(win)
        : gtcaca_window_get_prev(win);
      if (next && next != win) gtcaca_window_set_focus(next);
    }
    return 0;
  }

  /* Arrow keys: navigate between widgets unless the focused widget uses them
     internally. Menu gets priority — skip navigation when menu is active. */
  if (key == CACA_KEY_UP || key == CACA_KEY_DOWN ||
      key == CACA_KEY_LEFT || key == CACA_KEY_RIGHT) {
    int menu_active = 0;
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (widget->type == GTCACA_WIDGET_MENU && widget->has_focus) {
        menu_active = 1; break;
      }
    }
    if (!menu_active) {
      gtcaca_window_widget_t *win = gtcaca_window_get_current_focus();
      if (win && win->focused_child) {
        int type = win->focused_child->type;
        int navigate;
        if (key == CACA_KEY_UP || key == CACA_KEY_DOWN) {
          /* Entry, TextList, TextView, ComboBox consume Up/Down internally.
             RadioButton uses inter-widget nav so Up/Down cycle the group. */
          navigate = (type != GTCACA_WIDGET_ENTRY &&
                      type != GTCACA_WIDGET_TEXTLIST &&
                      type != GTCACA_WIDGET_TEXTVIEW &&
                      type != GTCACA_WIDGET_COMBOBOX &&
                      type != GTCACA_WIDGET_EDITOR &&
                      type != GTCACA_WIDGET_TREE &&
                      type != GTCACA_WIDGET_TABLE &&
                      type != GTCACA_WIDGET_MAP &&
                      type != GTCACA_WIDGET_MINDMAP &&
                      type != GTCACA_WIDGET_CALENDAR);
        } else {
          /* Left/Right: Entry, Scale, SpinButton use these internally; the Tree
             uses them to collapse/expand; the Map navigates between markers. */
          navigate = (type != GTCACA_WIDGET_ENTRY &&
                      type != GTCACA_WIDGET_SCALE &&
                      type != GTCACA_WIDGET_SPINBUTTON &&
                      type != GTCACA_WIDGET_EDITOR &&
                      type != GTCACA_WIDGET_TREE &&
                      type != GTCACA_WIDGET_TABLE &&
                      type != GTCACA_WIDGET_MAP &&
                      type != GTCACA_WIDGET_TABS &&
                      type != GTCACA_WIDGET_MINDMAP &&
                      type != GTCACA_WIDGET_CALENDAR);
        }
        if (navigate) {
          if (key == CACA_KEY_UP || key == CACA_KEY_LEFT)
            gtcaca_window_focus_prev_child(win);
          else
            gtcaca_window_focus_next_child(win);
          return 0;
        }
      }
    }
  }

  /* F10: toggle menu focus */
  if (key == CACA_KEY_F10) {
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (widget->type == GTCACA_WIDGET_MENU) {
        widget->has_focus = !widget->has_focus;
        if (!widget->has_focus) {
          ((gtcaca_menu_widget_t *)widget)->is_open = 0;
        } else {
          ((gtcaca_menu_widget_t *)widget)->active_entry = 0;
          ((gtcaca_menu_widget_t *)widget)->is_open = 0;
        }
        break;
      }
    }
    return 0;
  }

  /* When menu is active it gets exclusive dispatch */
  {
    int menu_active = 0;
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (widget->type == GTCACA_WIDGET_MENU && widget->has_focus) {
        menu_active = 1;
        break;
      }
    }
    /* Snapshot the widgets focused right now and deliver only to them. This
       way a widget appended during dispatch (e.g. a dialog from a button
       callback) — or one that *gains* focus mid-dispatch (e.g. another window
       focused by C-x o / C-x 0) — does not also receive this same key. */
    gtcaca_widget_t *focused[64];
    int nfocused = 0, i;
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (menu_active && widget->type != GTCACA_WIDGET_MENU) continue;
      if (widget->has_focus && nfocused < 64) focused[nfocused++] = widget;
    }
    for (i = 0; i < nfocused; i++)
      _gtcaca_widget_handle_key_press(focused[i], key);
  }

  return 0;
}

/* ---- Mouse hit-testing ---- */

static void _focus_widget_in_window(gtcaca_widget_t *hit)
{
  if (hit->parent && hit->parent->type == GTCACA_WIDGET_WINDOW) {
    gtcaca_window_set_focus((gtcaca_window_widget_t *)hit->parent);
    gtcaca_window_set_focused_child((gtcaca_window_widget_t *)hit->parent, hit);
  }
}

/* While the left button is held down over an editor, motion extends the
   selection from where the press landed. g_drag_editor is non-NULL between
   press and release; g_drag_anchor is the position the press selected. */
static gtcaca_editor_widget_t *g_drag_editor = NULL;
static int g_drag_anchor = 0;

static void _gtcaca_handle_mouse_press(int mx, int my, int button)
{
  gtcaca_widget_t *widget;
  gtcaca_widget_t *hit = NULL;
  int i, j;

  /* Mouse wheel: libcaca mis-reports the wheel coordinates on some terminals
     (e.g. macOS Terminal gives a garbage position), so we scroll the *focused*
     scrollable widget rather than the one under the cursor. Button 4 = down,
     5 = up (matches what those terminals report). */
  if (button == 4 || button == 5) {
    int key = (button == 4) ? CACA_KEY_DOWN : CACA_KEY_UP;
    gtcaca_widget_t *w, *target = NULL;
    (void)mx; (void)my;
    CDL_FOREACH(gmo.widgets_list, w) {
      if (w->has_focus && (w->type == GTCACA_WIDGET_TREE ||
                           w->type == GTCACA_WIDGET_TABLE ||
                           w->type == GTCACA_WIDGET_EDITOR)) { target = w; break; }
    }
    if (target) switch (target->type) {
    case GTCACA_WIDGET_TREE:   gtcaca_tree_key((gtcaca_tree_widget_t *)target, key, NULL); break;
    case GTCACA_WIDGET_TABLE:  gtcaca_table_key((gtcaca_table_widget_t *)target, key, NULL); break;
    case GTCACA_WIDGET_EDITOR: {
      gtcaca_editor_widget_t *e = (gtcaca_editor_widget_t *)target;
      int n;
      for (n = 0; n < 3; n++)        /* a wheel notch scrolls ~3 lines */
        if (key == CACA_KEY_DOWN) gtcaca_editor_line_down(e);
        else                      gtcaca_editor_line_up(e);
      if (e->update_cb) e->update_cb(e, e->update_cb_userdata);  /* refresh modeline/colourize */
      break;
    }
    default: break;
    }
    return;
  }

  if (button != 1) return;  /* left button only */

  /* Menus are drawn on top — check them first. */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (widget->type != GTCACA_WIDGET_MENU) continue;
    gtcaca_menu_widget_t *menu = (gtcaca_menu_widget_t *)widget;

    /* Click on open dropdown */
    if (menu->is_open) {
      gtcaca_menu_entry_t *entry = &menu->entries[menu->active_entry];
      int dropdown_x = menu->x + entry->x_pos;
      int dropdown_y = menu->y + 1;
      int dropdown_w = 22;
      for (j = 0; j < entry->n_items; j++) {
        int sc_len  = (int)strlen(entry->items[j].shortcut);
        int lbl_len = (int)strlen(entry->items[j].label);
        int needed  = lbl_len + (sc_len > 0 ? sc_len + 2 : 0) + 3;
        if (needed > dropdown_w) dropdown_w = needed;
      }
      if (mx >= dropdown_x && mx < dropdown_x + dropdown_w &&
          my >  dropdown_y  && my < dropdown_y + entry->n_items + 1) {
        int item_idx = my - dropdown_y - 1;
        if (item_idx >= 0 && item_idx < entry->n_items &&
            !entry->items[item_idx].is_separator) {
          gtcaca_menu_item_t *item = &entry->items[item_idx];
          menu->is_open    = 0;   /* close before running the action (modal-safe) */
          menu->has_focus  = 0;
          if (item->action) item->action(item->userdata);
        }
        return;
      }
      /* Click outside dropdown — close it */
      menu->is_open   = 0;
      menu->has_focus = 0;
    }

    /* Click on menu bar row */
    if (my == menu->y) {
      int x_pos = 1;
      for (i = 0; i < menu->n_entries; i++) {
        int title_len = (int)strlen(menu->entries[i].title);
        int entry_w   = title_len + 2;  /* " title " */
        if (mx >= x_pos && mx < x_pos + entry_w) {
          if (menu->has_focus && menu->active_entry == i && menu->is_open) {
            menu->is_open = 0;  /* toggle off */
          } else {
            menu->has_focus    = 1;
            menu->active_entry = i;
            menu->is_open      = 1;
            /* Select first non-separator item */
            menu->active_item  = 0;
            gtcaca_menu_entry_t *e = &menu->entries[i];
            while (menu->active_item < e->n_items &&
                   e->items[menu->active_item].is_separator)
              menu->active_item++;
          }
          return;
        }
        x_pos += entry_w;
      }
      /* Clicked menu bar background — close */
      menu->is_open   = 0;
      menu->has_focus = 0;
      return;
    }

    break;  /* only one menu widget expected */
  }

  /* Find the topmost widget under the cursor (last match in draw order wins). */
  CDL_FOREACH(gmo.widgets_list, widget) {
    if (!widget->is_visible)           continue;
    if (widget->type == GTCACA_WIDGET_MENU) continue;
    if (mx >= widget->x && mx < widget->x + widget->width &&
        my >= widget->y && my < widget->y + widget->height)
      hit = widget;
  }

  if (!hit) return;

  switch (hit->type) {
  case GTCACA_WIDGET_WINDOW:
    gtcaca_window_set_focus((gtcaca_window_widget_t *)hit);
    break;
  case GTCACA_WIDGET_BUTTON:
    _focus_widget_in_window(hit);
    _gtcaca_widget_handle_key_press(hit, CACA_KEY_RETURN);
    break;
  case GTCACA_WIDGET_CHECKBOX:
    _focus_widget_in_window(hit);
    _gtcaca_widget_handle_key_press(hit, ' ');
    break;
  case GTCACA_WIDGET_RADIOBUTTON:
    _focus_widget_in_window(hit);
    _gtcaca_widget_handle_key_press(hit, ' ');
    break;
  case GTCACA_WIDGET_COMBOBOX:
    _focus_widget_in_window(hit);
    _gtcaca_widget_handle_key_press(hit, CACA_KEY_RETURN);
    break;
  case GTCACA_WIDGET_SWITCH:
    _focus_widget_in_window(hit);
    _gtcaca_widget_handle_key_press(hit, ' ');
    break;
  case GTCACA_WIDGET_EXPANDER:
    _focus_widget_in_window(hit);
    _gtcaca_widget_handle_key_press(hit, ' ');
    break;
  case GTCACA_WIDGET_SCALE: {
    gtcaca_scale_widget_t *sc = (gtcaca_scale_widget_t *)hit;
    _focus_widget_in_window(hit);
    {
      int track_w = sc->width - 2;
      int click_pos = mx - sc->x - 1;
      double range = sc->max - sc->min;
      if (track_w > 0 && range > 0.0 && click_pos >= 0 && click_pos < track_w) {
        sc->value = sc->min + (double)click_pos / (track_w - 1) * range;
        if (sc->value < sc->min) sc->value = sc->min;
        if (sc->value > sc->max) sc->value = sc->max;
        if (sc->cb) sc->cb(sc, sc->value, sc->cb_userdata);
      }
    }
    break;
  }
  case GTCACA_WIDGET_SPINBUTTON: {
    gtcaca_spinbutton_widget_t *sb = (gtcaca_spinbutton_widget_t *)hit;
    _focus_widget_in_window(hit);
    /* Click on '<' (leftmost char) decrements, click on '>' (rightmost) increments */
    if (mx == sb->x) {
      gtcaca_spinbutton_handle_key(sb, CACA_KEY_LEFT);
      if (sb->cb) sb->cb(sb, sb->value, sb->cb_userdata);
    } else if (mx == sb->x + sb->width - 1) {
      gtcaca_spinbutton_handle_key(sb, CACA_KEY_RIGHT);
      if (sb->cb) sb->cb(sb, sb->value, sb->cb_userdata);
    }
    break;
  }
  case GTCACA_WIDGET_TREE: {
    gtcaca_tree_widget_t *t = (gtcaca_tree_widget_t *)hit;
    long row = t->top + (my - (t->y + 1));
    _focus_widget_in_window(hit);
    if (my > t->y && my < t->y + t->height - 1 && row >= 0 && row < gtcaca_tree_visible_count(t)) {
      t->sel = row;
      gtcaca_tree_key(t, ' ', NULL);   /* toggle expand/collapse if it has children */
    }
    break;
  }
  case GTCACA_WIDGET_TABLE: {
    gtcaca_table_widget_t *t = (gtcaca_table_widget_t *)hit;
    long row = t->top + (my - (t->y + 3));   /* border + header + separator */
    _focus_widget_in_window(hit);
    if (t->model && row >= 0 && row < t->model->row_count(t->model)) t->sel = row;
    break;
  }
  case GTCACA_WIDGET_MAP: {
    gtcaca_map_widget_t *m = (gtcaca_map_widget_t *)hit;
    int best = -1, k; long bd = 0;
    _focus_widget_in_window(hit);
    for (k = 0; k < m->npoints; k++) {
      int sx, sy; long d;
      if (!gtcaca_map_project(m, m->points[k].lon, m->points[k].lat, &sx, &sy)) continue;
      d = (long)(sx - mx) * (sx - mx) + (long)(sy - my) * (sy - my);
      if (best < 0 || d < bd) { bd = d; best = k; }
    }
    if (best >= 0) m->sel = best;
    break;
  }
  case GTCACA_WIDGET_EDITOR: {
    gtcaca_editor_widget_t *e = (gtcaca_editor_widget_t *)hit;
    int pos = gtcaca_editor_position_from_point(e, mx, my);
    _focus_widget_in_window(hit);
    gtcaca_editor_set_empty_selection(e, pos);   /* place the caret, clear selection */
    g_drag_editor = e;                            /* begin a possible click-drag */
    g_drag_anchor = pos;
    break;
  }
  default:
    /* Entry, TextView, TextList, ProgressBar, Label, etc.: just focus. */
    _focus_widget_in_window(hit);
    break;
  }
}

/* Left button held + moved over the editor: extend the selection to the cursor. */
static void _gtcaca_handle_mouse_motion(int mx, int my)
{
  int pos;
  if (!g_drag_editor) return;
  pos = gtcaca_editor_position_from_point(g_drag_editor, mx, my);
  gtcaca_editor_set_selection(g_drag_editor, pos, g_drag_anchor);  /* caret=pos, anchor=press */
}

static void _gtcaca_handle_mouse_release(void) { g_drag_editor = NULL; }

/* One key's worth of the application keymap (the editor/widgets see it). */
static void _dispatch_key(int key, int *quit)
{
  gtcaca_widget_t *widget;
  switch (key) {
  case 'q':
  case 'Q': {
    int text_focused = 0;
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (widget->has_focus &&
          (widget->type == GTCACA_WIDGET_ENTRY || widget->type == GTCACA_WIDGET_EDITOR)) {
        text_focused = 1; break;
      }
    }
    if (text_focused) gtcaca_widgets_handle_key_press(key);
    else *quit = 1;
    break;
  }
  case CACA_KEY_ESCAPE: {
    int handled = 0;
    if (_focused_is_editor()) { gtcaca_widgets_handle_key_press(key); break; }
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (widget->type == GTCACA_WIDGET_MENU && widget->has_focus) {
        ((gtcaca_menu_widget_t *)widget)->is_open = 0;
        ((gtcaca_menu_widget_t *)widget)->has_focus = 0;
        handled = 1; break;
      }
      if (widget->type == GTCACA_WIDGET_COMBOBOX) {
        gtcaca_combobox_widget_t *cb = (gtcaca_combobox_widget_t *)widget;
        if (cb->is_open) { cb->is_open = 0; handled = 1; break; }
      }
    }
    if (!handled) *quit = 1;
    break;
  }
  default:
    gtcaca_widgets_handle_key_press(key);
    break;
  }
}

/* ── mouse via the terminal's SGR protocol (1006), bypassing libcaca ──────────
 * libcaca's own wheel decoder silently drops one scroll direction and reports
 * garbage coordinates (verified on macOS terminals), so we turn its mouse off,
 * enable SGR mouse reporting ourselves, and parse the "ESC [ < b ; x ; y ; M|m"
 * reports out of the key stream. SGR gives exact coordinates and *both* wheel
 * directions. */
static int _next_key(int *k)
{
  caca_event_t e;
  if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &e, 30000)) return 0;  /* 30 ms */
  *k = caca_get_event_key_ch(&e);
  return 1;
}

static void _handle_sgr_mouse(int b, int x, int y, int press)
{
  if (b & 0x40) {                                /* wheel: 64 = up, 65 = down */
    _gtcaca_handle_mouse_press(x, y, (b & 1) ? 4 : 5);   /* down -> 4, up -> 5 */
    gtcaca_redraw();
  } else if (b & 0x20) {                          /* motion while a button is held (drag) */
    if (g_drag_editor) { _gtcaca_handle_mouse_motion(x, y); gtcaca_redraw(); }
  } else if (press) {                             /* button down: 0 left, 1 middle, 2 right */
    _gtcaca_handle_mouse_press(x, y, (b & 3) + 1);
    gtcaca_redraw();
  } else {                                        /* button release */
    _gtcaca_handle_mouse_release();
  }
}

/* We just read an ESC. Peek ahead a few ms: if it begins an SGR mouse report,
   parse and handle it; otherwise dispatch the ESC (and any peeked key) as keys
   so Meta/Escape still work. */
static void _esc_or_sgr_mouse(int *quit)
{
  int k, b = 0, x = 0, y = 0, field = 0, fin = 0, i;
  if (!_next_key(&k)) { _dispatch_key(CACA_KEY_ESCAPE, quit); return; }
  if (k != '[')       { _dispatch_key(CACA_KEY_ESCAPE, quit); _dispatch_key(k, quit); return; }
  if (!_next_key(&k)) { _dispatch_key(CACA_KEY_ESCAPE, quit); _dispatch_key('[', quit); return; }
  if (k != '<')       { _dispatch_key(CACA_KEY_ESCAPE, quit); _dispatch_key('[', quit); _dispatch_key(k, quit); return; }
  for (i = 0; i < 32; i++) {
    if (!_next_key(&k)) return;                   /* truncated report: drop it */
    if      (k >= '0' && k <= '9') { if (field == 0) b = b*10 + (k-'0'); else if (field == 1) x = x*10 + (k-'0'); else y = y*10 + (k-'0'); }
    else if (k == ';')             field++;
    else if (k == 'M' || k == 'm') { fin = k; break; }
    else return;                                 /* malformed: drop it */
  }
  if (fin) _handle_sgr_mouse(b, x - 1, y - 1, fin == 'M');   /* SGR coords are 1-based */
}

void gtcaca_main(void)
{
  caca_event_t ev;
  int quit = 0;
  int key;

  /* Drive the mouse via SGR ourselves; libcaca's decoder is off so the raw
     reports reach our parser (handles both wheel directions). This is exactly
     the setup that worked before the truecolour work. */
  caca_set_mouse(gmo.dp, 0);
  fputs("\033[?1002h\033[?1006h", stdout);   /* button-event tracking + SGR encoding */
  fflush(stdout);
  g_mouse_sgr = 1;   /* keep it asserted after every redraw (alt-screen switch resets it) */

  gtcaca_redraw();

  if (getenv("CCM_EVLOG"))
    fprintf(stderr, "[ccm] present=%d truecolor=%d isatty(1)=%d TERM=%s COLORTERM=%s driver=%s\n",
            g_present, g_truecolor, isatty(1), getenv("TERM") ? getenv("TERM") : "?",
            getenv("COLORTERM") ? getenv("COLORTERM") : "?",
            caca_get_display_driver(gmo.dp) ? caca_get_display_driver(gmo.dp) : "?");

  while (!quit && !g_quit_requested) {
    while (caca_get_event(gmo.dp, CACA_EVENT_ANY, &ev, 0)) {
      int evtype = caca_get_event_type(&ev);
      if (getenv("CCM_EVLOG")) {
        if (evtype & CACA_EVENT_KEY_PRESS)         fprintf(stderr, "[ccm] KEY ch=%d 0x%02x\n", caca_get_event_key_ch(&ev), caca_get_event_key_ch(&ev) & 0xff);
        else if (evtype & CACA_EVENT_MOUSE_PRESS)  fprintf(stderr, "[ccm] MOUSE-PRESS button=%d\n", caca_get_event_mouse_button(&ev));
        else if (evtype & CACA_EVENT_MOUSE_RELEASE)fprintf(stderr, "[ccm] MOUSE-RELEASE\n");
        else if (evtype & CACA_EVENT_MOUSE_MOTION) fprintf(stderr, "[ccm] MOUSE-MOTION\n");
        else                                       fprintf(stderr, "[ccm] EVENT type=0x%x\n", evtype);
      }
      if (!(evtype & CACA_EVENT_KEY_PRESS)) {
        if (evtype & CACA_EVENT_RESIZE) {
          gtcaca_redraw();
        } else if (evtype & CACA_EVENT_MOUSE_PRESS) {
          /* Use the display's *tracked* mouse position rather than the event's
             coordinates: libcaca reports garbage per-event coordinates on some
             terminals (e.g. macOS Terminal), but the tracked position is valid. */
          _gtcaca_handle_mouse_press(
            caca_get_mouse_x(gmo.dp),
            caca_get_mouse_y(gmo.dp),
            caca_get_event_mouse_button(&ev));
          gtcaca_redraw();
        } else if (evtype & CACA_EVENT_MOUSE_MOTION) {
          if (g_drag_editor) {   /* only redraw while a drag-select is in progress */
            _gtcaca_handle_mouse_motion(caca_get_mouse_x(gmo.dp), caca_get_mouse_y(gmo.dp));
            gtcaca_redraw();
          }
        } else if (evtype & CACA_EVENT_MOUSE_RELEASE) {
          _gtcaca_handle_mouse_release();
        }
        continue;
      }

      key = caca_get_event_key_ch(&ev);

      /* An ESC may be a real Esc/Meta or the start of an SGR mouse report. */
      if (key == CACA_KEY_ESCAPE) { _esc_or_sgr_mouse(&quit); gtcaca_redraw(); continue; }

      _dispatch_key(key, &quit);
      gtcaca_redraw();
    }
    usleep(10000);
  }

  g_mouse_sgr = 0;
  fputs("\033[?1002l\033[?1006l", stdout);   /* restore: disable SGR mouse reporting */
  fflush(stdout);
  gtcaca_present_shutdown();                  /* restore cursor if we took over output */
  caca_set_mouse(gmo.dp, 0);
  caca_free_display(gmo.dp);
}

unsigned int gtcaca_get_newid(void)
{
  return ++gmo.id;
}

int gtcaca_terminal_supports_blocks(void)
{
  const char *loc, *tp;
  /* fine glyphs need a UTF-8 locale */
  loc = getenv("LC_ALL");
  if (!loc || !*loc) loc = getenv("LC_CTYPE");
  if (!loc || !*loc) loc = getenv("LANG");
  if (loc && !(strstr(loc, "UTF-8") || strstr(loc, "utf-8") ||
               strstr(loc, "UTF8")  || strstr(loc, "utf8")))
    return 0;
  /* terminals that ship complete fonts */
  if (getenv("KITTY_WINDOW_ID") || getenv("WEZTERM_PANE") || getenv("WT_SESSION") ||
      getenv("ALACRITTY_WINDOW_ID") || getenv("ALACRITTY_SOCKET") ||
      getenv("KONSOLE_VERSION") || getenv("VTE_VERSION") ||
      getenv("GHOSTTY_RESOURCES_DIR") || getenv("FOOT_VERSION"))
    return 1;
  tp = getenv("TERM_PROGRAM");
  if (tp && (!strcmp(tp, "iTerm.app") || !strcmp(tp, "WezTerm") ||
             !strcmp(tp, "vscode") || !strcmp(tp, "ghostty") ||
             !strcmp(tp, "rio") || !strcmp(tp, "Hyper")))
    return 1;
  return 0;   /* unknown (plain xterm, Terminal.app, …): assume ASCII-safe */
}
