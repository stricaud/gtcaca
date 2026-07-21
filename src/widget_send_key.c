/* gtcaca_widget_send_key — route a key press to any widget by type.
 *
 * Widgets normally receive keys from gtcaca_main(), which dispatches to the
 * focused child. Applications that run their own event loop (polling keys with
 * gtcaca_poll_key) need to feed keys to a widget themselves; this does it
 * uniformly so callers don't switch on the widget type or reach into private
 * callback fields. Returns 1 if the key was consumed. */
#include <caca.h>

#include <gtcaca/widget.h>
#include <gtcaca/entry.h>
#include <gtcaca/button.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/radiobutton.h>
#include <gtcaca/combobox.h>
#include <gtcaca/textlist.h>
#include <gtcaca/textview.h>
#include <gtcaca/editor.h>
#include <gtcaca/menu.h>
#include <gtcaca/switch.h>
#include <gtcaca/scale.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/expander.h>
#include <gtcaca/tree.h>
#include <gtcaca/table.h>
#include <gtcaca/hexview.h>
#include <gtcaca/map.h>
#include <gtcaca/tabs.h>
#include <gtcaca/calendar.h>
#include <gtcaca/mindmap.h>
#include <gtcaca/dialog.h>
#include <gtcaca/filechooser.h>

#define PRIV(T, w) do { \
    T *o = (T *)(w); \
    if (o->private_key_cb) return o->private_key_cb(o, key, NULL); \
    return 0; \
  } while (0)

int gtcaca_widget_send_key(gtcaca_widget_t *w, int key)
{
  if (!w) return 0;
  switch (w->type) {
  case GTCACA_WIDGET_ENTRY:       PRIV(gtcaca_entry_widget_t, w);
  case GTCACA_WIDGET_BUTTON:      PRIV(gtcaca_button_widget_t, w);
  case GTCACA_WIDGET_CHECKBOX:    PRIV(gtcaca_checkbox_widget_t, w);
  case GTCACA_WIDGET_RADIOBUTTON: PRIV(gtcaca_radiobutton_widget_t, w);
  case GTCACA_WIDGET_COMBOBOX:    PRIV(gtcaca_combobox_widget_t, w);
  case GTCACA_WIDGET_TEXTLIST:    PRIV(gtcaca_textlist_widget_t, w);
  case GTCACA_WIDGET_TEXTVIEW:    PRIV(gtcaca_textview_widget_t, w);
  case GTCACA_WIDGET_EDITOR:      PRIV(gtcaca_editor_widget_t, w);
  case GTCACA_WIDGET_MENU:        gtcaca_menu_handle_key((gtcaca_menu_widget_t *)w, key); return 1;
  case GTCACA_WIDGET_SWITCH:
    if (key == ' ' || key == CACA_KEY_RETURN || key == 10) {
      gtcaca_switch_widget_t *s = (gtcaca_switch_widget_t *)w;
      gtcaca_switch_set_active(s, !gtcaca_switch_get_active(s));
      return 1;
    }
    return 0;
  case GTCACA_WIDGET_SCALE:       gtcaca_scale_handle_key((gtcaca_scale_widget_t *)w, key); return 1;
  case GTCACA_WIDGET_SPINBUTTON:  gtcaca_spinbutton_handle_key((gtcaca_spinbutton_widget_t *)w, key); return 1;
  case GTCACA_WIDGET_EXPANDER:    gtcaca_expander_handle_key((gtcaca_expander_widget_t *)w, key); return 1;
  case GTCACA_WIDGET_TREE:        return gtcaca_tree_key((gtcaca_tree_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_TABLE:       return gtcaca_table_key((gtcaca_table_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_HEXVIEW:     return gtcaca_hexview_key((gtcaca_hexview_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_MAP:         return gtcaca_map_key((gtcaca_map_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_TABS:        return gtcaca_tabs_key((gtcaca_tabs_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_CALENDAR:    return gtcaca_calendar_key((gtcaca_calendar_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_MINDMAP:     return gtcaca_mindmap_key((gtcaca_mindmap_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_DIALOG:      return gtcaca_dialog_key((gtcaca_dialog_widget_t *)w, key, NULL);
  case GTCACA_WIDGET_FILECHOOSERDIALOG: return gtcaca_filechooser_key((gtcaca_filechooser_widget_t *)w, key, NULL);
  default: return 0;
  }
}
