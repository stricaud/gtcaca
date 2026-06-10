#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

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

gmo_t gmo;

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
  case GTCACA_WIDGET_CALENDAR:
  case GTCACA_WIDGET_DIALOG:
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
    break;
  }

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

  caca_refresh_display(gmo.dp);
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
  case GTCACA_WIDGET_PROGRESSBAR:
  case GTCACA_WIDGET_STATUSBAR:
  case GTCACA_WIDGET_LABEL:
  case GTCACA_WIDGET_CALENDAR:
  case GTCACA_WIDGET_DIALOG:
  case GTCACA_WIDGET_FILECHOOSERDIALOG:
  case GTCACA_WIDGET_IMAGE:
    break;
  }

  return 0;
}

int gtcaca_widgets_handle_key_press(int key)
{
  gtcaca_widget_t *widget = NULL;

  /* TAB: cycle focus to next focusable child in focused window */
  if (key == '\t') {
    gtcaca_window_widget_t *win = gtcaca_window_get_current_focus();
    if (win) gtcaca_window_focus_next_child(win);
    return 0;
  }

  /* Ctrl+N / Ctrl+P: switch between windows */
  if (key == '\x0e' || key == '\x10') {
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
                      type != GTCACA_WIDGET_COMBOBOX);
        } else {
          /* Left/Right: only Entry uses these for cursor movement. */
          navigate = (type != GTCACA_WIDGET_ENTRY);
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
    CDL_FOREACH(gmo.widgets_list, widget) {
      if (menu_active && widget->type != GTCACA_WIDGET_MENU) continue;
      _gtcaca_widget_handle_key_press(widget, key);
    }
  }

  return 0;
}

void gtcaca_main(void)
{
  caca_event_t ev;
  int quit = 0;
  int key;
  gtcaca_widget_t *widget = NULL;
  int _dbg = open("/tmp/gtcacaX.log", O_WRONLY|O_CREAT|O_TRUNC, 0666);
  if (_dbg >= 0) write(_dbg, "start\n", 6);

  gtcaca_redraw();

  while (!quit) {
    while (caca_get_event(gmo.dp, CACA_EVENT_ANY, &ev, 0)) {
      int evtype = caca_get_event_type(&ev);
      if (_dbg >= 0) {
        char buf[48];
        int n;
        if (evtype & CACA_EVENT_KEY_PRESS)
          n = snprintf(buf, sizeof(buf), "KEY ch=0x%x\n", caca_get_event_key_ch(&ev));
        else
          n = snprintf(buf, sizeof(buf), "EVT 0x%x\n", evtype);
        write(_dbg, buf, n);
      }
      if (!(evtype & CACA_EVENT_KEY_PRESS)) {
        if (evtype & CACA_EVENT_RESIZE) gtcaca_redraw();
        continue;
      }

      key = caca_get_event_key_ch(&ev);

      switch (key) {
      case 'q':
      case 'Q': {
        int entry_focused = 0;
        CDL_FOREACH(gmo.widgets_list, widget) {
          if (widget->has_focus && widget->type == GTCACA_WIDGET_ENTRY) {
            entry_focused = 1;
            break;
          }
        }
        if (entry_focused)
          gtcaca_widgets_handle_key_press(key);
        else
          quit = 1;
        break;
      }
      case CACA_KEY_ESCAPE: {
        int handled = 0;
        CDL_FOREACH(gmo.widgets_list, widget) {
          if (widget->type == GTCACA_WIDGET_MENU && widget->has_focus) {
            gtcaca_menu_widget_t *m = (gtcaca_menu_widget_t *)widget;
            m->is_open = 0;
            m->has_focus = 0;
            handled = 1;
            break;
          }
          if (widget->type == GTCACA_WIDGET_COMBOBOX) {
            gtcaca_combobox_widget_t *cb = (gtcaca_combobox_widget_t *)widget;
            if (cb->is_open) {
              cb->is_open = 0;
              handled = 1;
              break;
            }
          }
        }
        if (!handled) quit = 1;
        break;
      }
      default:
        gtcaca_widgets_handle_key_press(key);
        break;
      }

      gtcaca_redraw();
    }
    usleep(10000);
  }

  if (_dbg >= 0) close(_dbg);
  caca_free_display(gmo.dp);
}

unsigned int gtcaca_get_newid(void)
{
  return ++gmo.id;
}
