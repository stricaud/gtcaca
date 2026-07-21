/* bindgen entry point: pull in the entire GTCaca public API so every
 * gtcaca_* function and type is emitted into the raw FFI module.
 *
 * main.h brings in caca.h, application/log/theme/widget/utlist; the rest of
 * the widget headers are listed explicitly. Keep this in sync when a new
 * public header is added under src/include/gtcaca/. */
#include <gtcaca/main.h>

#include <gtcaca/application.h>
#include <gtcaca/barchart.h>
#include <gtcaca/box.h>
#include <gtcaca/button.h>
#include <gtcaca/calendar.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/colordialog.h>
#include <gtcaca/combobox.h>
#include <gtcaca/custom.h>
#include <gtcaca/dialog.h>
#include <gtcaca/editor.h>
#include <gtcaca/entry.h>
#include <gtcaca/expander.h>
#include <gtcaca/filechooser.h>
#include <gtcaca/frame.h>
#include <gtcaca/gauge.h>
#include <gtcaca/hexview.h>
#include <gtcaca/image.h>
#include <gtcaca/iniparse.h>
#include <gtcaca/json.h>
#include <gtcaca/label.h>
#include <gtcaca/linechart.h>
#include <gtcaca/log.h>
#include <gtcaca/map.h>
#include <gtcaca/menu.h>
#include <gtcaca/mindmap.h>
#include <gtcaca/piechart.h>
#include <gtcaca/progressbar.h>
#include <gtcaca/scatter.h>
#include <gtcaca/radiobutton.h>
#include <gtcaca/scale.h>
#include <gtcaca/segdisplay.h>
#include <gtcaca/separator.h>
#include <gtcaca/sparkline.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/spinner.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/switch.h>
#include <gtcaca/table.h>
#include <gtcaca/tabs.h>
#include <gtcaca/textlist.h>
#include <gtcaca/textview.h>
#include <gtcaca/theme.h>
#include <gtcaca/tree.h>
#include <gtcaca/widget.h>
#include <gtcaca/window.h>
