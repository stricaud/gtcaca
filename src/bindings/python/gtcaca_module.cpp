// Python bindings for the GTCaca libcaca TUI toolkit.
//
// GTCaca is a pure-C library: widgets are created by factory functions that
// return heap pointers owned by the global `gmo` widget lists (never freed by
// the caller), and callbacks are plain C function pointers taking a `void *`
// userdata.  These bindings therefore:
//
//   * expose every widget as a non-owning pybind11 class (reference policy —
//     pybind never deletes a widget; the C library owns them for the run),
//   * bridge Python callables to the C callbacks through small trampolines,
//   * release the GIL while the blocking event loop `gtcaca_main()` runs and
//     re-acquire it inside every trampoline.
//
// The C structs all share an identical "preamble" (type/id/x/y/width/... and
// the parent/children/prev/next links) as their first members — the same trick
// the C side uses via the GTCACA_WIDGET() cast.  We rely on that here too:
// `as_widget()` reinterpret-casts any concrete widget pointer to the common
// `gtcaca_widget_t *` so it can be passed where a parent is expected.

#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/label.h>
#include <gtcaca/button.h>
#include <gtcaca/entry.h>
#include <gtcaca/checkbox.h>
#include <gtcaca/radiobutton.h>
#include <gtcaca/combobox.h>
#include <gtcaca/progressbar.h>
#include <gtcaca/textview.h>
#include <gtcaca/statusbar.h>
#include <gtcaca/menu.h>
#include <gtcaca/dialog.h>
#include <gtcaca/barchart.h>
#include <gtcaca/box.h>
#include <gtcaca/calendar.h>
#include <gtcaca/colordialog.h>
#include <gtcaca/editor.h>
#include <gtcaca/expander.h>
#include <gtcaca/filechooser.h>
#include <gtcaca/frame.h>
#include <gtcaca/gauge.h>
#include <gtcaca/hexview.h>
#include <gtcaca/image.h>
#include <gtcaca/linechart.h>
#include <gtcaca/map.h>
#include <gtcaca/mindmap.h>
#include <gtcaca/piechart.h>
#include <gtcaca/rope.h>
#include <gtcaca/scale.h>
#include <gtcaca/scatter.h>
#include <gtcaca/segdisplay.h>
#include <gtcaca/separator.h>
#include <gtcaca/sparkline.h>
#include <gtcaca/spinbutton.h>
#include <gtcaca/spinner.h>
#include <gtcaca/switch.h>
#include <gtcaca/table.h>
#include <gtcaca/tabs.h>
#include <gtcaca/textlist.h>
#include <gtcaca/tree.h>
#include <gtcaca/widget.h>
#include <gtcaca/custom.h>
}

namespace py = pybind11;

// ---------------------------------------------------------------------------
// Callback storage
// ---------------------------------------------------------------------------
//
// Widget key callbacks are keyed by the widget pointer.  The button register
// API has no userdata slot, so a pointer-keyed map is the uniform choice for
// every widget type.  Menu actions *do* carry userdata, so we keep the
// py::function alive in a deque (stable addresses across push_back) and pass a
// pointer to it as the userdata.

// These hold Python callables for the lifetime of the process. They are
// deliberately heap-allocated and never freed: if they had static-storage
// destructors, the C++ runtime would run them at process exit — *after*
// CPython has finalized — and decref-ing a py::function against a dead
// interpreter segfaults. Leaking them is the standard pybind11 remedy (the OS
// reclaims the memory at exit anyway).
static std::unordered_map<const void *, py::function> &g_key_cbs =
    *new std::unordered_map<const void *, py::function>();
static std::deque<py::function> &g_menu_cbs = *new std::deque<py::function>();
// Draw callbacks for custom widgets, keyed by widget pointer.
static std::unordered_map<const void *, py::function> &g_draw_cbs =
    *new std::unordered_map<const void *, py::function>();
// Mouse callbacks for custom widgets, keyed by widget pointer.
static std::unordered_map<const void *, py::function> &g_mouse_cbs =
    *new std::unordered_map<const void *, py::function>();

// Convert a Python parent argument (a widget instance or None) to the common
// gtcaca_widget_t*.  Every concrete widget exposes as_widget(); None -> NULL.
static gtcaca_widget_t *to_parent(py::object o) {
  if (o.is_none())
    return nullptr;
  if (py::hasattr(o, "as_widget"))
    return o.attr("as_widget")().cast<gtcaca_widget_t *>();
  return o.cast<gtcaca_widget_t *>();
}

// Key-callback trampoline shared by every widget type (same C signature).
#define GTCACA_KEY_TRAMPOLINE(NAME, CTYPE)                                     \
  static int NAME(CTYPE *w, int key, void * /*ud*/) {                          \
    py::gil_scoped_acquire gil;                                                \
    auto it = g_key_cbs.find(static_cast<const void *>(w));                    \
    if (it == g_key_cbs.end())                                                 \
      return 0;                                                                \
    try {                                                                      \
      py::object r = it->second(key);                                          \
      if (!r.is_none() && py::isinstance<py::int_>(r))                         \
        return r.cast<int>();                                                  \
    } catch (py::error_already_set &e) {                                       \
      e.discard_as_unraisable(#NAME);                                          \
    }                                                                          \
    return 0;                                                                  \
  }

GTCACA_KEY_TRAMPOLINE(tramp_custom_key, gtcaca_custom_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_button, gtcaca_button_widget_t)

// Draw trampoline for custom widgets: no args, no return value.
static void tramp_custom_draw(gtcaca_custom_widget_t *w, void * /*ud*/) {
  py::gil_scoped_acquire gil;
  auto it = g_draw_cbs.find(static_cast<const void *>(w));
  if (it == g_draw_cbs.end())
    return;
  try {
    it->second();
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("gtcaca_custom_draw");
  }
}
// Mouse trampoline for custom widgets: callback(event, x, y, button) -> int.
static int tramp_custom_mouse(gtcaca_custom_widget_t *w, gtcaca_mouse_event_t ev,
                              int x, int y, int button, void * /*ud*/) {
  py::gil_scoped_acquire gil;
  auto it = g_mouse_cbs.find(static_cast<const void *>(w));
  if (it == g_mouse_cbs.end())
    return 0;
  try {
    py::object r = it->second((int)ev, x, y, button);
    if (!r.is_none() && py::isinstance<py::int_>(r))
      return r.cast<int>();
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("gtcaca_custom_mouse");
  }
  return 0;
}

GTCACA_KEY_TRAMPOLINE(tramp_entry, gtcaca_entry_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_checkbox, gtcaca_checkbox_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_radiobutton, gtcaca_radiobutton_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_combobox, gtcaca_combobox_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_textview, gtcaca_textview_widget_t)

GTCACA_KEY_TRAMPOLINE(tramp_textlist, gtcaca_textlist_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_editor_key, gtcaca_editor_widget_t)

// Value-change callbacks (switch/scale/spinbutton/expander): the C signature is
// always (widget, value, userdata) -> int, so one registry keyed by widget
// pointer serves them all.
static std::unordered_map<const void *, py::function> &g_val_cbs =
    *new std::unordered_map<const void *, py::function>();

#define GTCACA_VALUE_TRAMPOLINE(NAME, CTYPE, VTYPE, CONV)                      \
  static int NAME(CTYPE *w, VTYPE v, void * /*ud*/) {                          \
    py::gil_scoped_acquire gil;                                                \
    auto it = g_val_cbs.find(static_cast<const void *>(w));                    \
    if (it == g_val_cbs.end())                                                 \
      return 0;                                                                \
    try {                                                                      \
      py::object r = it->second(CONV);                                         \
      if (!r.is_none() && py::isinstance<py::int_>(r))                         \
        return r.cast<int>();                                                  \
    } catch (py::error_already_set &e) {                                       \
      e.discard_as_unraisable(#NAME);                                          \
    }                                                                          \
    return 0;                                                                  \
  }

GTCACA_VALUE_TRAMPOLINE(tramp_switch, gtcaca_switch_widget_t, int, v != 0)
GTCACA_VALUE_TRAMPOLINE(tramp_expander, gtcaca_expander_widget_t, int, v != 0)
GTCACA_VALUE_TRAMPOLINE(tramp_scale, gtcaca_scale_widget_t, double, v)
GTCACA_VALUE_TRAMPOLINE(tramp_spinbutton, gtcaca_spinbutton_widget_t, double, v)

// The calendar marker callback carries no widget pointer, so its Python
// callable travels in the userdata, like menu actions do.
static std::deque<py::function> &g_cal_cbs = *new std::deque<py::function>();
static int tramp_calendar(int year, int month, int day, void *ud) {
  py::gil_scoped_acquire gil;
  auto *f = static_cast<py::function *>(ud);
  if (!f)
    return 0;
  try {
    return py::cast<bool>((*f)(year, month, day)) ? 1 : 0;
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("gtcaca_calendar_marker");
  }
  return 0;
}

static void tramp_menu(void *ud) {
  py::gil_scoped_acquire gil;
  auto *f = static_cast<py::function *>(ud);
  if (!f)
    return;
  try {
    (*f)();
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("gtcaca_menu_action");
  }
}

// ---------------------------------------------------------------------------
// Opaque handles
// ---------------------------------------------------------------------------
//
// gtcaca_box_t / gtcaca_rope_t / the editor's langcfg and grammar are opaque in
// the headers, so pybind cannot register them directly (py::class_ needs a
// complete type).  Wrap the pointer in a tiny struct instead.
struct BoxHandle { gtcaca_box_t *p; };
struct RopeHandle { gtcaca_rope_t *p; };
struct LangCfgHandle { gtcaca_editor_langcfg_t *p; };
struct GrammarHandle { gtcaca_editor_grammar_t *p; };

// ---------------------------------------------------------------------------
// Model adapters (table / tree)
// ---------------------------------------------------------------------------
//
// Both widgets pull their content through a struct of C function pointers.  We
// subclass that struct (it is a POD, so this is just a layout extension) and
// keep the Python callables alongside, then hand the widget a pointer to it.
// Models are kept alive in a registry: the C widget stores a bare pointer and
// would dangle if Python collected the model.

static std::deque<py::object> &g_models = *new std::deque<py::object>();

struct PyTableModel : gtcaca_table_model_t {
  py::object f_row_count, f_col_count, f_header, f_cell, f_row_color;
};

static void put_str(char *buf, int buflen, const std::string &s) {
  if (buflen <= 0)
    return;
  int n = (int)s.size();
  if (n > buflen - 1)
    n = buflen - 1;
  memcpy(buf, s.data(), (size_t)n);
  buf[n] = '\0';
}

static long tbl_row_count(gtcaca_table_model_t *m) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTableModel *>(m);
  try {
    return pm->f_row_count().cast<long>();
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("table.row_count");
  }
  return 0;
}
static int tbl_col_count(gtcaca_table_model_t *m) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTableModel *>(m);
  try {
    return pm->f_col_count().cast<int>();
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("table.col_count");
  }
  return 0;
}
static void tbl_header(gtcaca_table_model_t *m, int col, char *buf, int buflen) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTableModel *>(m);
  try {
    put_str(buf, buflen, pm->f_header(col).cast<std::string>());
    return;
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("table.header");
  }
  put_str(buf, buflen, "");
}
static void tbl_cell(gtcaca_table_model_t *m, long row, int col, char *buf, int buflen) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTableModel *>(m);
  try {
    put_str(buf, buflen, pm->f_cell(row, col).cast<std::string>());
    return;
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("table.cell");
  }
  put_str(buf, buflen, "");
}
static int tbl_row_color(gtcaca_table_model_t *m, long row, uint8_t *fg, uint8_t *bg) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTableModel *>(m);
  try {
    py::object r = pm->f_row_color(row);
    if (r.is_none())
      return 0;
    auto t = r.cast<std::pair<int, int>>();
    *fg = (uint8_t)t.first;
    *bg = (uint8_t)t.second;
    return 1;
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("table.row_color");
  }
  return 0;
}

// Tree nodes are opaque `void *` handles on the C side.  Python models deal in
// plain integers instead (the invisible super-root is None <-> NULL), which
// keeps arbitrary Python identity out of a pointer slot.
static py::object node_to_py(void *n) {
  if (!n)
    return py::none();
  return py::int_(reinterpret_cast<intptr_t>(n));
}
static void *py_to_node(const py::object &o) {
  if (o.is_none())
    return nullptr;
  return reinterpret_cast<void *>(o.cast<intptr_t>());
}

struct PyTreeModel : gtcaca_tree_model_t {
  py::object f_child_count, f_child, f_label, f_has_children, f_draw_row;
};

static long tr_child_count(gtcaca_tree_model_t *m, void *node) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTreeModel *>(m);
  try {
    return pm->f_child_count(node_to_py(node)).cast<long>();
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("tree.child_count");
  }
  return 0;
}
static void *tr_child(gtcaca_tree_model_t *m, void *node, long index) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTreeModel *>(m);
  try {
    return py_to_node(pm->f_child(node_to_py(node), index));
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("tree.child");
  }
  return nullptr;
}
static void tr_label(gtcaca_tree_model_t *m, void *node, char *buf, int buflen) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTreeModel *>(m);
  try {
    put_str(buf, buflen, pm->f_label(node_to_py(node)).cast<std::string>());
    return;
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("tree.label");
  }
  put_str(buf, buflen, "");
}
static int tr_has_children(gtcaca_tree_model_t *m, void *node) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTreeModel *>(m);
  try {
    return pm->f_has_children(node_to_py(node)).cast<bool>() ? 1 : 0;
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("tree.has_children");
  }
  return 0;
}
static void tr_draw_row(gtcaca_tree_model_t *m, void *node, int x, int y, int width,
                        int selected) {
  py::gil_scoped_acquire gil;
  auto *pm = static_cast<PyTreeModel *>(m);
  try {
    pm->f_draw_row(node_to_py(node), x, y, width, selected != 0);
  } catch (py::error_already_set &e) {
    e.discard_as_unraisable("tree.draw_row");
  }
}

// ---------------------------------------------------------------------------
// Common widget binding helper
// ---------------------------------------------------------------------------
//
// Adds the shared preamble fields + show/hide/as_widget to every concrete
// widget class.  All structs declare these members, so &T::field is valid for
// each type and resolves to the same offset.
template <class T>
static void bind_common(py::class_<T> &c) {
  c.def_readwrite("x", &T::x);
  c.def_readwrite("y", &T::y);
  c.def_readwrite("width", &T::width);
  c.def_readwrite("height", &T::height);
  c.def_readwrite("has_focus", &T::has_focus);
  c.def_readwrite("is_visible", &T::is_visible);
  c.def_property_readonly("id", [](const T &s) { return s.id; });
  c.def(
      "as_widget",
      [](T *s) { return reinterpret_cast<gtcaca_widget_t *>(s); },
      py::return_value_policy::reference);
  c.def("show",
        [](T *s) { gtcaca_widget_show(reinterpret_cast<gtcaca_widget_t *>(s)); });
  c.def("hide",
        [](T *s) { gtcaca_widget_hide(reinterpret_cast<gtcaca_widget_t *>(s)); });
}

PYBIND11_MODULE(_gtcaca, m) {
  m.doc() = "Python bindings for GTCaca, a libcaca-based terminal UI toolkit.";

  // -- core lifecycle ------------------------------------------------------
  m.def(
      "init",
      [](py::object argv) {
        // We don't thread real argv through; libcaca init only needs a valid
        // (argc, argv) pair.  Pass an empty vector.
        int argc = 0;
        char *args0 = nullptr;
        char **av = &args0;
        return gtcaca_init(&argc, &av);
      },
      py::arg("argv") = py::none(),
      "Initialise the toolkit and the libcaca canvas/display.");

  m.def(
      "main",
      []() {
        py::gil_scoped_release rel; // event loop blocks; let callbacks re-acquire
        gtcaca_main();
      },
      "Run the blocking event loop until quit() is called.");
  m.def("main_quit", &gtcaca_main_quit, "Ask the event loop to exit.");
  m.def("redraw", &gtcaca_redraw, "Redraw all widgets.");
  m.def("present_shutdown", &gtcaca_present_shutdown,
        "Restore the terminal if the truecolour presenter is active.");
  m.def("terminal_supports_blocks", &gtcaca_terminal_supports_blocks,
        "Best-effort guess whether the terminal can render fine block glyphs.");

  m.def("canvas_width",
        []() { return caca_get_canvas_width(gmo.cv); },
        "Current canvas width in cells.");
  m.def("canvas_height",
        []() { return caca_get_canvas_height(gmo.cv); },
        "Current canvas height in cells.");

  // -- low-level canvas drawing (the "escape hatch") -----------------------
  // These operate directly on the global libcaca canvas.  They are intended to
  // be called from a Custom widget's on_draw callback, letting Python paint
  // arbitrary content (hex grids, trees, gauges) with full per-cell control.
  m.def(
      "set_color",
      [](int fg, int bg) { caca_set_color_ansi(gmo.cv, fg, bg); },
      py::arg("fg"), py::arg("bg"),
      "Set the current pen to ANSI colours (use the CACA_* / colour constants).");
  m.def(
      "set_color_rgb",
      [](int fg12, int bg12) {
        // 12-bit 0xRGB per channel-nibble; add an opaque alpha nibble so the
        // truecolour presenter renders the exact colour.
        caca_set_color_argb(gmo.cv, (uint16_t)(0xF000u | (fg12 & 0x0FFFu)),
                            (uint16_t)(0xF000u | (bg12 & 0x0FFFu)));
      },
      py::arg("fg"), py::arg("bg"),
      "Set the pen to 12-bit RGB colours (each argument is 0xRGB, nibble per channel).");
  m.def(
      "put_str",
      [](int x, int y, const std::string &s) {
        return caca_put_str(gmo.cv, x, y, s.c_str());
      },
      py::arg("x"), py::arg("y"), py::arg("text"),
      "Draw a UTF-8 string at (x, y) with the current pen.");
  m.def(
      "put_char",
      [](int x, int y, int ch) { return caca_put_char(gmo.cv, x, y, (uint32_t)ch); },
      py::arg("x"), py::arg("y"), py::arg("ch"),
      "Draw a single character (unicode code point) at (x, y).");
  m.def(
      "fill_box",
      [](int x, int y, int w, int h, int ch) {
        caca_fill_box(gmo.cv, x, y, w, h, (uint32_t)ch);
      },
      py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
      py::arg("ch") = (int)' ', "Fill a rectangle with a character in the current pen.");
  m.def(
      "draw_box",
      [](int x, int y, int w, int h) { caca_draw_cp437_box(gmo.cv, x, y, w, h); },
      py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
      "Draw a CP437 line-drawing box border.");
  m.def(
      "draw_thin_box",
      [](int x, int y, int w, int h) { caca_draw_thin_box(gmo.cv, x, y, w, h); },
      py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
      "Draw a thin ASCII box border.");
  m.def(
      "draw_line",
      [](int x1, int y1, int x2, int y2, int ch) {
        caca_draw_line(gmo.cv, x1, y1, x2, y2, (uint32_t)ch);
      },
      py::arg("x1"), py::arg("y1"), py::arg("x2"), py::arg("y2"), py::arg("ch"),
      "Draw a line of a character between two points.");

  // -- generic widget base (used for parent passing) -----------------------
  py::class_<gtcaca_widget_t>(m, "Widget")
      .def_readwrite("x", &gtcaca_widget_t::x)
      .def_readwrite("y", &gtcaca_widget_t::y)
      .def_readwrite("width", &gtcaca_widget_t::width)
      .def_readwrite("height", &gtcaca_widget_t::height)
      .def_readwrite("has_focus", &gtcaca_widget_t::has_focus)
      .def_readwrite("is_visible", &gtcaca_widget_t::is_visible)
      .def_property_readonly("id",
                             [](const gtcaca_widget_t &s) { return s.id; })
      .def("as_widget", [](gtcaca_widget_t *s) { return s; },
           py::return_value_policy::reference)
      .def("show", &gtcaca_widget_show)
      .def("hide", &gtcaca_widget_hide);

  // -- application ---------------------------------------------------------
  {
    py::class_<gtcaca_application_widget_t> c(m, "Application");
    bind_common(c);
    c.def("draw", &gtcaca_application_draw);
  }
  m.def(
      "application_new",
      [](const std::string &title) {
        return gtcaca_application_new(title.c_str());
      },
      py::arg("title"), py::return_value_policy::reference,
      "Create the top-level application widget.");

  // -- window --------------------------------------------------------------
  {
    py::class_<gtcaca_window_widget_t> c(m, "Window");
    bind_common(c);
    c.def("draw", &gtcaca_window_draw);
    c.def("set_focus", &gtcaca_window_set_focus);
    c.def("focus_next_child", &gtcaca_window_focus_next_child);
    c.def("focus_prev_child", &gtcaca_window_focus_prev_child);
    c.def("close", &gtcaca_window_close);
    c.def(
        "set_focused_child",
        [](gtcaca_window_widget_t *w, py::object child) {
          gtcaca_window_set_focused_child(w, to_parent(child));
        },
        py::arg("child"));
    c.def(
        "set_default",
        [](gtcaca_window_widget_t *w, py::object child) {
          gtcaca_window_set_default(w, to_parent(child));
        },
        py::arg("widget"));
  }
  m.def(
      "window_new",
      [](py::object parent, const std::string &title, int x, int y, int w,
         int h) {
        return gtcaca_window_new(to_parent(parent), title.c_str(), x, y, w, h);
      },
      py::arg("parent"), py::arg("title"), py::arg("x"), py::arg("y"),
      py::arg("width"), py::arg("height"), py::return_value_policy::reference);
  m.def(
      "window_new_centered",
      [](py::object parent, const std::string &title, int w, int h) {
        return gtcaca_window_new_centered(to_parent(parent), title.c_str(), w,
                                          h);
      },
      py::arg("parent"), py::arg("title"), py::arg("width"), py::arg("height"),
      py::return_value_policy::reference);

  // -- label ---------------------------------------------------------------
  {
    py::class_<gtcaca_label_widget_t> c(m, "Label");
    bind_common(c);
    c.def("draw", &gtcaca_label_draw);
  }
  m.def(
      "label_new",
      [](py::object parent, const std::string &text, int x, int y) {
        return gtcaca_label_new(to_parent(parent), text.c_str(), x, y);
      },
      py::arg("parent"), py::arg("text"), py::arg("x"), py::arg("y"),
      py::return_value_policy::reference);

  // -- button --------------------------------------------------------------
  {
    py::class_<gtcaca_button_widget_t> c(m, "Button");
    bind_common(c);
    c.def("draw", &gtcaca_button_draw);
    c.def(
        "on_key",
        [](gtcaca_button_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_button_key_cb_register(w, tramp_button);
        },
        py::arg("callback"),
        "Register a callback(key)->int invoked on key events.");
  }
  m.def(
      "button_new",
      [](py::object parent, const std::string &label, int x, int y) {
        return gtcaca_button_new(to_parent(parent), label.c_str(), x, y);
      },
      py::arg("parent"), py::arg("label"), py::arg("x"), py::arg("y"),
      py::return_value_policy::reference);

  // -- entry ---------------------------------------------------------------
  {
    py::class_<gtcaca_entry_widget_t> c(m, "Entry");
    bind_common(c);
    c.def("draw", &gtcaca_entry_draw);
    c.def("get_text",
          [](gtcaca_entry_widget_t *e) {
            const char *t = gtcaca_entry_get_text(e);
            return std::string(t ? t : "");
          });
    c.def(
        "set_text",
        [](gtcaca_entry_widget_t *e, const std::string &t) {
          gtcaca_entry_set_text(e, t.c_str());
        },
        py::arg("text"));
    c.def(
        "set_secret",
        [](gtcaca_entry_widget_t *e, bool s) {
          gtcaca_entry_set_secret(e, s ? 1 : 0);
        },
        py::arg("secret"));
    c.def(
        "on_key",
        [](gtcaca_entry_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_entry_key_cb_register(w, tramp_entry, nullptr);
        },
        py::arg("callback"));
  }
  m.def(
      "entry_new",
      [](py::object parent, int x, int y, int width) {
        return gtcaca_entry_new(to_parent(parent), x, y, width);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::return_value_policy::reference);

  // -- checkbox ------------------------------------------------------------
  {
    py::class_<gtcaca_checkbox_widget_t> c(m, "Checkbox");
    bind_common(c);
    c.def("draw", &gtcaca_checkbox_draw);
    c.def_property_readonly("label", [](gtcaca_checkbox_widget_t *c) {
      return std::string(c->label ? c->label : "");
    });
    c.def("get_checked",
          [](gtcaca_checkbox_widget_t *c) {
            return gtcaca_checkbox_get_checked(c) != 0;
          });
    c.def(
        "set_checked",
        [](gtcaca_checkbox_widget_t *c, bool v) {
          gtcaca_checkbox_set_checked(c, v ? 1 : 0);
        },
        py::arg("checked"));
    c.def(
        "on_key",
        [](gtcaca_checkbox_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_checkbox_key_cb_register(w, tramp_checkbox, nullptr);
        },
        py::arg("callback"));
  }
  m.def(
      "checkbox_new",
      [](py::object parent, const std::string &label, int x, int y) {
        return gtcaca_checkbox_new(to_parent(parent), label.c_str(), x, y);
      },
      py::arg("parent"), py::arg("label"), py::arg("x"), py::arg("y"),
      py::return_value_policy::reference);

  // -- radiobutton ---------------------------------------------------------
  {
    py::class_<gtcaca_radiobutton_widget_t> c(m, "RadioButton");
    bind_common(c);
    c.def("draw", &gtcaca_radiobutton_draw);
    c.def_property_readonly("label", [](gtcaca_radiobutton_widget_t *r) {
      return std::string(r->label ? r->label : "");
    });
    c.def("get_active",
          [](gtcaca_radiobutton_widget_t *r) {
            return gtcaca_radiobutton_get_active(r) != 0;
          });
    c.def("set_active", &gtcaca_radiobutton_set_active);
    c.def(
        "on_key",
        [](gtcaca_radiobutton_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_radiobutton_key_cb_register(w, tramp_radiobutton, nullptr);
        },
        py::arg("callback"));
  }
  m.def(
      "radiobutton_new",
      [](py::object parent, const std::string &label, int group_id, int x,
         int y) {
        return gtcaca_radiobutton_new(to_parent(parent), label.c_str(),
                                      group_id, x, y);
      },
      py::arg("parent"), py::arg("label"), py::arg("group_id"), py::arg("x"),
      py::arg("y"), py::return_value_policy::reference);

  // -- combobox ------------------------------------------------------------
  {
    py::class_<gtcaca_combobox_widget_t> c(m, "ComboBox");
    bind_common(c);
    c.def("draw", &gtcaca_combobox_draw);
    c.def(
        "append",
        [](gtcaca_combobox_widget_t *c, const std::string &item) {
          gtcaca_combobox_append(c, item.c_str());
        },
        py::arg("item"));
    c.def("get_selected",
          [](gtcaca_combobox_widget_t *c) {
            const char *s = gtcaca_combobox_get_selected(c);
            return std::string(s ? s : "");
          });
    c.def("get_selected_index", &gtcaca_combobox_get_selected_index);
    c.def(
        "on_key",
        [](gtcaca_combobox_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_combobox_key_cb_register(w, tramp_combobox, nullptr);
        },
        py::arg("callback"));
  }
  m.def(
      "combobox_new",
      [](py::object parent, int x, int y, int width) {
        return gtcaca_combobox_new(to_parent(parent), x, y, width);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::return_value_policy::reference);

  // -- progressbar ---------------------------------------------------------
  {
    py::class_<gtcaca_progressbar_widget_t> c(m, "ProgressBar");
    bind_common(c);
    c.def("draw", &gtcaca_progressbar_draw);
    c.def("set_value", &gtcaca_progressbar_set_value, py::arg("value"));
    c.def("get_value", &gtcaca_progressbar_get_value);
  }
  m.def(
      "progressbar_new",
      [](py::object parent, int x, int y, int width) {
        return gtcaca_progressbar_new(to_parent(parent), x, y, width);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::return_value_policy::reference);

  // -- textview ------------------------------------------------------------
  py::enum_<gtcaca_textview_mode_t>(m, "TextViewMode")
      .value("PLAIN", GTCACA_TEXTVIEW_MODE_PLAIN)
      .value("JSON", GTCACA_TEXTVIEW_MODE_JSON);
  {
    py::class_<gtcaca_textview_widget_t> c(m, "TextView");
    bind_common(c);
    c.def("draw", &gtcaca_textview_draw);
    c.def(
        "append",
        [](gtcaca_textview_widget_t *t, const std::string &line) {
          gtcaca_textview_append(t, line.c_str());
        },
        py::arg("line"));
    c.def("clear", &gtcaca_textview_clear);
    c.def("set_mode", &gtcaca_textview_set_mode, py::arg("mode"));
    c.def(
        "on_key",
        [](gtcaca_textview_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_textview_key_cb_register(w, tramp_textview, nullptr);
        },
        py::arg("callback"));
  }
  m.def(
      "textview_new",
      [](py::object parent, int x, int y, int width, int height) {
        return gtcaca_textview_new(to_parent(parent), x, y, width, height);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- statusbar -----------------------------------------------------------
  {
    py::class_<gtcaca_statusbar_widget_t> c(m, "StatusBar");
    bind_common(c);
    c.def("draw", &gtcaca_statusbar_draw);
    c.def(
        "set_text",
        [](gtcaca_statusbar_widget_t *s, const std::string &t) {
          gtcaca_statusbar_set_text(s, t.c_str());
        },
        py::arg("text"));
    c.def("set_rows_from_bottom", &gtcaca_statusbar_set_rows_from_bottom,
          py::arg("n"));
  }
  m.def(
      "statusbar_new",
      [](const std::string &text) { return gtcaca_statusbar_new(text.c_str()); },
      py::arg("text"), py::return_value_policy::reference);

  // -- menu ----------------------------------------------------------------
  {
    py::class_<gtcaca_menu_widget_t> c(m, "Menu");
    bind_common(c);
    c.def("draw", &gtcaca_menu_draw);
    c.def(
        "add_entry",
        [](gtcaca_menu_widget_t *menu, const std::string &title) {
          return gtcaca_menu_add_entry(menu, title.c_str());
        },
        py::arg("title"), "Add a top-level menu entry; returns its index.");
    c.def(
        "add_item",
        [](gtcaca_menu_widget_t *menu, int entry_idx, const std::string &label,
           const std::string &shortcut, py::object action) {
          gtcaca_menu_action_t cfn = nullptr;
          void *ud = nullptr;
          if (!action.is_none()) {
            g_menu_cbs.emplace_back(action.cast<py::function>());
            ud = static_cast<void *>(&g_menu_cbs.back());
            cfn = tramp_menu;
          }
          return gtcaca_menu_add_item(menu, entry_idx, label.c_str(),
                                      shortcut.c_str(), cfn, ud);
        },
        py::arg("entry_idx"), py::arg("label"), py::arg("shortcut") = "",
        py::arg("action") = py::none());
    c.def(
        "add_separator",
        [](gtcaca_menu_widget_t *menu, int entry_idx) {
          return gtcaca_menu_add_separator(menu, entry_idx);
        },
        py::arg("entry_idx"));
    c.def("handle_key", &gtcaca_menu_handle_key, py::arg("key"));
    c.def(
        "set_focus",
        [](gtcaca_menu_widget_t *menu, bool on) {
          gtcaca_menu_set_focus(menu, on ? 1 : 0);
        },
        py::arg("on") = true,
        "Give (or remove) keyboard focus to the menu bar. Lets an application\n"
        "open the menu from its own key handler; removing focus also closes\n"
        "any open dropdown.");
    c.def(
        "is_focused",
        [](gtcaca_menu_widget_t *menu) {
          return gtcaca_menu_is_focused(menu) != 0;
        },
        "Whether the menu bar currently holds keyboard focus.");
    c.def(
        "set_item_enabled",
        [](gtcaca_menu_widget_t *menu, int entry_idx, int item_idx, bool enabled) {
          gtcaca_menu_set_item_enabled(menu, entry_idx, item_idx, enabled ? 1 : 0);
        },
        py::arg("entry_idx"), py::arg("item_idx"), py::arg("enabled"),
        "Enable or disable an item: disabled items are greyed out, skipped by\n"
        "keyboard navigation, and their action never runs.");
  }
  m.def("menu_new", &gtcaca_menu_new, py::return_value_policy::reference);

  // -- modal dialogs -------------------------------------------------------
  // These run their own blocking event loop and return the user's choice, so
  // they can be called straight from a key handler or a menu action.  The GIL
  // is deliberately *held* throughout: the loop calls gtcaca_redraw(), which
  // re-enters Python through any custom-widget draw trampoline.
  m.def(
      "dialog_run",
      [](const std::string &title, const std::string &message,
         const std::vector<std::string> &buttons) {
        std::vector<const char *> ptrs;
        ptrs.reserve(buttons.size());
        for (const auto &b : buttons) ptrs.push_back(b.c_str());
        return gtcaca_dialog_run(title.c_str(), message.c_str(),
                                 ptrs.empty() ? nullptr : ptrs.data(),
                                 static_cast<int>(ptrs.size()));
      },
      py::arg("title"), py::arg("message"), py::arg("buttons"),
      "Modal dialog with a row of buttons; returns the chosen index, or -1 if\n"
      "cancelled with ESC.");
  // The dialog as a plain widget, for callers that drive their own loop instead
  // of using the blocking helpers below.
  {
    py::class_<gtcaca_dialog_widget_t> c(m, "Dialog");
    bind_common(c);
    c.def("draw", &gtcaca_dialog_draw);
    c.def(
        "set",
        [](gtcaca_dialog_widget_t *d, const std::string &title,
           const std::string &message, const std::vector<std::string> &buttons) {
          std::vector<const char *> p;
          p.reserve(buttons.size());
          for (const auto &b : buttons) p.push_back(b.c_str());
          gtcaca_dialog_set(d, title.c_str(), message.c_str(),
                            p.empty() ? nullptr : p.data(), (int)p.size());
        },
        py::arg("title"), py::arg("message"), py::arg("buttons"));
    c.def("key", [](gtcaca_dialog_widget_t *d, int key) {
      return gtcaca_dialog_key(d, key, nullptr);
    }, py::arg("key"));
    c.def("result", &gtcaca_dialog_result,
          "DIALOG_ONGOING while open, else the button index (-1 = cancelled).");
  }
  m.def(
      "dialog_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_dialog_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.attr("DIALOG_ONGOING") = (int)GTCACA_DIALOG_ONGOING;
  m.def(
      "dialog_confirm",
      [](const std::string &title, const std::string &message) {
        return gtcaca_dialog_confirm(title.c_str(), message.c_str()) != 0;
      },
      py::arg("title"), py::arg("message"),
      "Modal OK/Cancel dialog; True if the user chose OK.");
  m.def(
      "dialog_message",
      [](const std::string &title, const std::string &message) {
        gtcaca_dialog_message(title.c_str(), message.c_str());
      },
      py::arg("title"), py::arg("message"),
      "Modal dialog with a single OK button.");

  // ==========================================================================
  // Additional widgets
  // ==========================================================================

  // -- frame ---------------------------------------------------------------
  {
    py::class_<gtcaca_frame_widget_t> c(m, "Frame");
    bind_common(c);
    c.def("draw", &gtcaca_frame_draw);
  }
  m.def(
      "frame_new",
      [](py::object parent, const std::string &label, int x, int y, int w, int h) {
        return gtcaca_frame_new(to_parent(parent), label.c_str(), x, y, w, h);
      },
      py::arg("parent"), py::arg("label"), py::arg("x"), py::arg("y"),
      py::arg("width"), py::arg("height"), py::return_value_policy::reference,
      "A labelled box drawn around a region.");

  // -- separator -----------------------------------------------------------
  {
    py::class_<gtcaca_separator_widget_t> c(m, "Separator");
    bind_common(c);
    c.def("draw", &gtcaca_separator_draw);
  }
  m.def(
      "separator_new",
      [](py::object parent, int x, int y, int length, bool vertical) {
        return gtcaca_separator_new(to_parent(parent), x, y, length, vertical ? 1 : 0);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("length"),
      py::arg("vertical") = false, py::return_value_policy::reference);

  // -- gauge ---------------------------------------------------------------
  {
    py::class_<gtcaca_gauge_widget_t> c(m, "Gauge");
    bind_common(c);
    c.def("draw", &gtcaca_gauge_draw);
    c.def("set_value", &gtcaca_gauge_set_value, py::arg("value"), "0..1");
    c.def("set_percent", &gtcaca_gauge_set_percent, py::arg("percent"), "0..100");
    c.def("get_value", &gtcaca_gauge_get_value);
    c.def(
        "set_label",
        [](gtcaca_gauge_widget_t *g, py::object label) {
          gtcaca_gauge_set_label(g, label.is_none() ? nullptr
                                                    : label.cast<std::string>().c_str());
        },
        py::arg("label"), "None shows the percentage instead.");
    c.def("set_colors", &gtcaca_gauge_set_colors, py::arg("filled_bg"), py::arg("empty_bg"));
  }
  m.def(
      "gauge_new",
      [](py::object parent, int x, int y, int width) {
        return gtcaca_gauge_new(to_parent(parent), x, y, width);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::return_value_policy::reference);

  // -- switch --------------------------------------------------------------
  {
    py::class_<gtcaca_switch_widget_t> c(m, "Switch");
    bind_common(c);
    c.def("draw", &gtcaca_switch_draw);
    c.def("set_active", [](gtcaca_switch_widget_t *s, bool on) {
      gtcaca_switch_set_active(s, on ? 1 : 0);
    }, py::arg("active"));
    c.def("get_active", [](gtcaca_switch_widget_t *s) {
      return gtcaca_switch_get_active(s) != 0;
    });
    c.def(
        "on_toggle",
        [](gtcaca_switch_widget_t *s, py::function cb) {
          g_val_cbs[static_cast<const void *>(s)] = std::move(cb);
          gtcaca_switch_cb_register(s, tramp_switch, nullptr);
        },
        py::arg("callback"), "callback(active: bool)");
  }
  m.def(
      "switch_new",
      [](py::object parent, int x, int y) {
        return gtcaca_switch_new(to_parent(parent), x, y);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"),
      py::return_value_policy::reference);

  // -- spinner -------------------------------------------------------------
  {
    py::class_<gtcaca_spinner_widget_t> c(m, "Spinner");
    bind_common(c);
    c.def("draw", &gtcaca_spinner_draw);
    c.def("set_spinning", [](gtcaca_spinner_widget_t *s, bool on) {
      gtcaca_spinner_set_spinning(s, on ? 1 : 0);
    }, py::arg("spinning"));
    c.def("step", &gtcaca_spinner_step, "Advance one animation frame.");
  }
  m.def(
      "spinner_new",
      [](py::object parent, int x, int y) {
        return gtcaca_spinner_new(to_parent(parent), x, y);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"),
      py::return_value_policy::reference);

  // -- spinbutton ----------------------------------------------------------
  {
    py::class_<gtcaca_spinbutton_widget_t> c(m, "SpinButton");
    bind_common(c);
    c.def("draw", &gtcaca_spinbutton_draw);
    c.def("handle_key", &gtcaca_spinbutton_handle_key, py::arg("key"));
    c.def("set_value", &gtcaca_spinbutton_set_value, py::arg("value"));
    c.def("get_value", &gtcaca_spinbutton_get_value);
    c.def(
        "on_change",
        [](gtcaca_spinbutton_widget_t *sb, py::function cb) {
          g_val_cbs[static_cast<const void *>(sb)] = std::move(cb);
          gtcaca_spinbutton_cb_register(sb, tramp_spinbutton, nullptr);
        },
        py::arg("callback"), "callback(value: float)");
  }
  m.def(
      "spinbutton_new",
      [](py::object parent, int x, int y, double min, double max, double step) {
        return gtcaca_spinbutton_new(to_parent(parent), x, y, min, max, step);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("min"),
      py::arg("max"), py::arg("step") = 1.0, py::return_value_policy::reference);

  // -- scale ---------------------------------------------------------------
  {
    py::class_<gtcaca_scale_widget_t> c(m, "Scale");
    bind_common(c);
    c.def("draw", &gtcaca_scale_draw);
    c.def("handle_key", &gtcaca_scale_handle_key, py::arg("key"));
    c.def("set_value", &gtcaca_scale_set_value, py::arg("value"));
    c.def("get_value", &gtcaca_scale_get_value);
    c.def(
        "on_change",
        [](gtcaca_scale_widget_t *s, py::function cb) {
          g_val_cbs[static_cast<const void *>(s)] = std::move(cb);
          gtcaca_scale_cb_register(s, tramp_scale, nullptr);
        },
        py::arg("callback"), "callback(value: float)");
  }
  m.def(
      "scale_new",
      [](py::object parent, int x, int y, int width, double min, double max, double step) {
        return gtcaca_scale_new(to_parent(parent), x, y, width, min, max, step);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("min"), py::arg("max"), py::arg("step") = 1.0,
      py::return_value_policy::reference);

  // -- expander ------------------------------------------------------------
  {
    py::class_<gtcaca_expander_widget_t> c(m, "Expander");
    bind_common(c);
    c.def("draw", &gtcaca_expander_draw);
    c.def("handle_key", &gtcaca_expander_handle_key, py::arg("key"));
    c.def("add_managed", [](gtcaca_expander_widget_t *e, py::object w) {
      gtcaca_expander_add_managed(e, to_parent(w));
    }, py::arg("widget"), "Show/hide `widget` with the expander.");
    c.def("set_expanded", [](gtcaca_expander_widget_t *e, bool on) {
      gtcaca_expander_set_expanded(e, on ? 1 : 0);
    }, py::arg("expanded"));
    c.def("get_expanded", [](gtcaca_expander_widget_t *e) {
      return gtcaca_expander_get_expanded(e) != 0;
    });
    c.def(
        "on_toggle",
        [](gtcaca_expander_widget_t *e, py::function cb) {
          g_val_cbs[static_cast<const void *>(e)] = std::move(cb);
          gtcaca_expander_cb_register(e, tramp_expander, nullptr);
        },
        py::arg("callback"), "callback(expanded: bool)");
  }
  m.def(
      "expander_new",
      [](py::object parent, const std::string &label, int x, int y, int width) {
        return gtcaca_expander_new(to_parent(parent), label.c_str(), x, y, width);
      },
      py::arg("parent"), py::arg("label"), py::arg("x"), py::arg("y"),
      py::arg("width"), py::return_value_policy::reference);

  // -- segdisplay ----------------------------------------------------------
  {
    py::class_<gtcaca_segdisplay_widget_t> c(m, "SegDisplay");
    bind_common(c);
    c.def("draw", &gtcaca_segdisplay_draw);
    c.def("set_text", [](gtcaca_segdisplay_widget_t *s, const std::string &t) {
      gtcaca_segdisplay_set_text(s, t.c_str());
    }, py::arg("text"));
    c.def("set_colour", &gtcaca_segdisplay_set_colour, py::arg("colour"));
    c.def("set_title", [](gtcaca_segdisplay_widget_t *s, const std::string &t) {
      gtcaca_segdisplay_set_title(s, t.c_str());
    }, py::arg("title"));
    c.def("set_box", [](gtcaca_segdisplay_widget_t *s, bool on) {
      gtcaca_segdisplay_set_box(s, on ? 1 : 0);
    }, py::arg("on"));
  }
  m.def(
      "segdisplay_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_segdisplay_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference,
      "Seven-segment style numeric/text display.");

  // -- sparkline -----------------------------------------------------------
  {
    py::class_<gtcaca_sparkline_widget_t> c(m, "Sparkline");
    bind_common(c);
    c.def("draw", &gtcaca_sparkline_draw);
    c.def(
        "set_data",
        [](gtcaca_sparkline_widget_t *s, const std::vector<float> &v) {
          gtcaca_sparkline_set_data(s, v.empty() ? nullptr : v.data(), (int)v.size());
        },
        py::arg("values"));
    c.def("push", &gtcaca_sparkline_push, py::arg("value"));
    c.def("set_max", &gtcaca_sparkline_set_max, py::arg("maxval"), "0 = auto-scale");
    c.def("set_color", &gtcaca_sparkline_set_color, py::arg("colour"));
    c.def("set_style", &gtcaca_sparkline_set_style, py::arg("style"));
    c.def("set_style_auto", &gtcaca_sparkline_set_style_auto);
    c.def("set_title", [](gtcaca_sparkline_widget_t *s, const std::string &t) {
      gtcaca_sparkline_set_title(s, t.c_str());
    }, py::arg("title"));
  }
  m.def(
      "sparkline_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_sparkline_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.attr("SPARKLINE_BLOCKS") = (int)GTCACA_SPARKLINE_BLOCKS;
  m.attr("SPARKLINE_AREA") = (int)GTCACA_SPARKLINE_AREA;

  // -- hexview -------------------------------------------------------------
  {
    py::class_<gtcaca_hexview_widget_t> c(m, "HexView");
    bind_common(c);
    c.def("draw", &gtcaca_hexview_draw);
    c.def(
        "set_data",
        [](gtcaca_hexview_widget_t *h, py::buffer buf) {
          py::buffer_info info = buf.request();
          gtcaca_hexview_set_data(h, static_cast<const uint8_t *>(info.ptr),
                                  (int)(info.size * info.itemsize));
        },
        py::arg("data"),
        "Show `data` (bytes / any buffer). The buffer is NOT copied by the C\n"
        "widget, so keep a reference to it alive for as long as it is shown.",
        py::keep_alive<1, 2>());
    c.def("set_highlight", &gtcaca_hexview_set_highlight, py::arg("off"), py::arg("len"));
    c.def("cursor", &gtcaca_hexview_cursor);
    c.def("set_title", [](gtcaca_hexview_widget_t *h, const std::string &t) {
      gtcaca_hexview_set_title(h, t.c_str());
    }, py::arg("title"));
    c.def("key", [](gtcaca_hexview_widget_t *h, int key) {
      return gtcaca_hexview_key(h, key, nullptr);
    }, py::arg("key"));
  }
  m.def(
      "hexview_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_hexview_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- image ---------------------------------------------------------------
  {
    py::class_<gtcaca_image_widget_t> c(m, "Image");
    bind_common(c);
    c.def("draw", &gtcaca_image_draw);
    c.def(
        "load",
        [](gtcaca_image_widget_t *i, const std::string &path) {
          return gtcaca_image_load(i, path.c_str()) == 0;   // 0 = success, -1 = error
        },
        py::arg("path"), "Load from a file; True on success.");
    c.def(
        "load_memory",
        [](gtcaca_image_widget_t *i, py::buffer buf) {
          py::buffer_info info = buf.request();
          return gtcaca_image_load_memory(i, info.ptr,
                                          (size_t)(info.size * info.itemsize)) == 0;
        },
        py::arg("data"), "Load an encoded image from memory; True on success.");
  }
  m.def(
      "image_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_image_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- tabs ----------------------------------------------------------------
  {
    py::class_<gtcaca_tabs_widget_t> c(m, "Tabs");
    bind_common(c);
    c.def("draw", &gtcaca_tabs_draw);
    c.def(
        "set_titles",
        [](gtcaca_tabs_widget_t *t, const std::vector<std::string> &titles) {
          std::vector<const char *> p;
          p.reserve(titles.size());
          for (const auto &s : titles) p.push_back(s.c_str());
          gtcaca_tabs_set_titles(t, p.empty() ? nullptr : p.data(), (int)p.size());
        },
        py::arg("titles"));
    c.def("set_title", [](gtcaca_tabs_widget_t *t, const std::string &s) {
      gtcaca_tabs_set_title(t, s.c_str());
    }, py::arg("title"), "The surrounding box title.");
    c.def("set_selected", &gtcaca_tabs_set_selected, py::arg("idx"));
    c.def("selected", &gtcaca_tabs_selected);
    c.def("key", [](gtcaca_tabs_widget_t *t, int key) {
      return gtcaca_tabs_key(t, key, nullptr);
    }, py::arg("key"));
  }
  m.def(
      "tabs_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_tabs_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- textlist ------------------------------------------------------------
  {
    py::class_<gtcaca_textlist_widget_t> c(m, "TextList");
    bind_common(c);
    c.def("draw", &gtcaca_textlist_draw);
    c.def(
        "append",
        [](gtcaca_textlist_widget_t *t, const std::string &item) {
          // the C side copies the string, but takes a non-const char *
          std::vector<char> tmp(item.begin(), item.end());
          tmp.push_back('\0');
          gtcaca_textlist_append(t, tmp.data());
        },
        py::arg("item"));
    c.def("clear", &gtcaca_textlist_clear);
    c.def("selection_up", &gtcaca_textlist_selection_up);
    c.def("selection_down", &gtcaca_textlist_selection_down);
    c.def(
        "selected_text",
        [](gtcaca_textlist_widget_t *t) -> py::object {
          const char *s = gtcaca_textlist_get_text_selected(t);
          return s ? py::cast(std::string(s)) : py::none();
        });
    c.def("set_view_size", &gtcaca_textlist_widget_set_view_size, py::arg("view_size"));
    c.def("set_search_enabled", [](gtcaca_textlist_widget_t *t, bool on) {
      gtcaca_textlist_set_search_enabled(t, on ? 1 : 0);
    }, py::arg("enabled"));
    c.def("is_searching", [](gtcaca_textlist_widget_t *t) {
      return gtcaca_textlist_is_searching(t) != 0;
    });
    c.def(
        "on_key",
        [](gtcaca_textlist_widget_t *t, py::function cb) {
          g_key_cbs[static_cast<const void *>(t)] = std::move(cb);
          gtcaca_textlist_key_cb_register(t, tramp_textlist, nullptr);
        },
        py::arg("callback"), "callback(key: int) -> int");
  }
  m.def(
      "textlist_new",
      [](py::object parent, int x, int y) {
        return gtcaca_textlist_new(to_parent(parent), x, y);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"),
      py::return_value_policy::reference);

  // -- calendar ------------------------------------------------------------
  {
    py::class_<gtcaca_calendar_widget_t> c(m, "Calendar");
    bind_common(c);
    c.def("draw", &gtcaca_calendar_draw);
    c.def("set_date", &gtcaca_calendar_set_date, py::arg("year"), py::arg("month"),
          py::arg("day"));
    c.def("get_date", [](gtcaca_calendar_widget_t *c_) {
      int y = 0, mo = 0, d = 0;
      gtcaca_calendar_get_date(c_, &y, &mo, &d);
      return py::make_tuple(y, mo, d);
    }, "Returns (year, month, day).");
    c.def("set_title", [](gtcaca_calendar_widget_t *c_, const std::string &t) {
      gtcaca_calendar_set_title(c_, t.c_str());
    }, py::arg("title"));
    c.def("key", [](gtcaca_calendar_widget_t *c_, int key) {
      return gtcaca_calendar_key(c_, key, nullptr);
    }, py::arg("key"));
    c.def(
        "set_marker",
        [](gtcaca_calendar_widget_t *c_, py::function cb) {
          g_cal_cbs.emplace_back(std::move(cb));
          gtcaca_calendar_set_marker(c_, tramp_calendar,
                                     static_cast<void *>(&g_cal_cbs.back()));
        },
        py::arg("callback"),
        "callback(year, month, day) -> bool: mark that day.");
  }
  m.def(
      "calendar_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_calendar_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.def("calendar_days_in_month", &gtcaca_calendar_days_in_month, py::arg("year"),
        py::arg("month"));
  m.def("calendar_day_of_week", &gtcaca_calendar_day_of_week, py::arg("year"),
        py::arg("month"), py::arg("day"), "0=Sunday .. 6=Saturday");

  // -- file chooser --------------------------------------------------------
  {
    py::class_<gtcaca_filechooser_widget_t> c(m, "FileChooser");
    bind_common(c);
    c.def("draw", &gtcaca_filechooser_draw);
    c.def("set_dir", [](gtcaca_filechooser_widget_t *f, const std::string &d) {
      gtcaca_filechooser_set_dir(f, d.c_str());
    }, py::arg("dir"));
    c.def("key", [](gtcaca_filechooser_widget_t *f, int key) {
      return gtcaca_filechooser_key(f, key, nullptr);
    }, py::arg("key"));
  }
  m.def(
      "filechooser_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_filechooser_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.def(
      "filechooser_run",
      [](const std::string &start_dir, bool save_mode) -> py::object {
        char out[4096] = {0};
        if (!gtcaca_filechooser_run(start_dir.c_str(), out, (int)sizeof(out),
                                    save_mode ? 1 : 0))
          return py::none();
        return py::cast(std::string(out));
      },
      py::arg("start_dir") = ".", py::arg("save_mode") = false,
      "Modal file chooser; returns the chosen path, or None if cancelled.");
  m.def(
      "filechooser_run_named",
      [](const std::string &start_dir, const std::string &default_name) -> py::object {
        char out[4096] = {0};
        if (!gtcaca_filechooser_run_named(start_dir.c_str(), default_name.c_str(),
                                          out, (int)sizeof(out)))
          return py::none();
        return py::cast(std::string(out));
      },
      py::arg("start_dir"), py::arg("default_name"),
      "Modal save dialog with the name pre-filled; path or None.");
  m.def(
      "filechooser_run_opts",
      [](const std::string &start_dir, bool save_mode,
         const std::vector<std::pair<std::string, bool>> &options) {
        std::vector<gtcaca_fc_option_t> opts;
        opts.reserve(options.size());
        for (const auto &o : options)
          opts.push_back({o.first.c_str(), o.second ? 1 : 0});
        std::vector<int> states(options.size(), 0);
        char out[4096] = {0};
        int rc = gtcaca_filechooser_run_opts(
            start_dir.c_str(), out, (int)sizeof(out), save_mode ? 1 : 0,
            opts.empty() ? nullptr : opts.data(), (int)opts.size(),
            states.empty() ? nullptr : states.data());
        py::list flags;
        for (int s : states) flags.append(s != 0);
        return py::make_tuple(rc ? py::cast(std::string(out)) : py::none(), flags);
      },
      py::arg("start_dir"), py::arg("save_mode"), py::arg("options"),
      "Chooser with extra checkboxes; returns (path_or_None, [states]).");

  // -- colour dialog -------------------------------------------------------
  {
    py::class_<gtcaca_colordialog_widget_t> c(m, "ColorDialog");
    bind_common(c);
    c.def("draw", &gtcaca_colordialog_draw);
    c.def("set_title", [](gtcaca_colordialog_widget_t *d, const std::string &t) {
      gtcaca_colordialog_set_title(d, t.c_str());
    }, py::arg("title"));
    c.def("key", [](gtcaca_colordialog_widget_t *d, int key) {
      return gtcaca_colordialog_key(d, key, nullptr);
    }, py::arg("key"));
    c.def("result", &gtcaca_colordialog_result);
  }
  m.def(
      "colordialog_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_colordialog_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.def("colordialog_run", &gtcaca_colordialog_run, py::arg("title"),
        py::arg("initial") = 0,
        "Modal colour picker; returns the chosen ANSI colour index, or -1.");
  m.def("color_name", &gtcaca_color_name, py::arg("idx"));

  // -- barchart ------------------------------------------------------------
  {
    py::class_<gtcaca_barchart_widget_t> c(m, "BarChart");
    bind_common(c);
    c.def("draw", &gtcaca_barchart_draw);
    c.def(
        "set_data",
        [](gtcaca_barchart_widget_t *b, const std::vector<float> &values,
           const std::vector<std::string> &labels) {
          std::vector<const char *> lp;
          lp.reserve(labels.size());
          for (const auto &s : labels) lp.push_back(s.c_str());
          gtcaca_barchart_set_data(b, values.empty() ? nullptr : values.data(),
                                   lp.empty() ? nullptr : lp.data(),
                                   (int)values.size());
        },
        py::arg("values"), py::arg("labels") = std::vector<std::string>{});
    c.def("set_max", &gtcaca_barchart_set_max, py::arg("maxval"), "0 = auto");
    c.def("set_bar_width", &gtcaca_barchart_set_bar_width, py::arg("bar_width"),
          py::arg("gap"));
    c.def("set_color", &gtcaca_barchart_set_color, py::arg("colour"));
    c.def("set_show_values", [](gtcaca_barchart_widget_t *b, bool on) {
      gtcaca_barchart_set_show_values(b, on ? 1 : 0);
    }, py::arg("on"));
  }
  m.def(
      "barchart_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_barchart_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- piechart ------------------------------------------------------------
  {
    py::class_<gtcaca_piechart_widget_t> c(m, "PieChart");
    bind_common(c);
    c.def("draw", &gtcaca_piechart_draw);
    c.def(
        "set_data",
        [](gtcaca_piechart_widget_t *p, const std::vector<float> &values,
           const std::vector<std::string> &labels) {
          std::vector<const char *> lp;
          lp.reserve(labels.size());
          for (const auto &s : labels) lp.push_back(s.c_str());
          gtcaca_piechart_set_data(p, values.empty() ? nullptr : values.data(),
                                   lp.empty() ? nullptr : lp.data(),
                                   (int)values.size());
        },
        py::arg("values"), py::arg("labels") = std::vector<std::string>{});
    c.def(
        "set_colors",
        [](gtcaca_piechart_widget_t *p, const std::vector<uint8_t> &cols) {
          gtcaca_piechart_set_colors(p, cols.empty() ? nullptr : cols.data(),
                                     (int)cols.size());
        },
        py::arg("colours"));
    c.def("set_donut", [](gtcaca_piechart_widget_t *p, bool on) {
      gtcaca_piechart_set_donut(p, on ? 1 : 0);
    }, py::arg("on"));
    c.def("set_show_legend", [](gtcaca_piechart_widget_t *p, bool on) {
      gtcaca_piechart_set_show_legend(p, on ? 1 : 0);
    }, py::arg("on"));
    c.def("set_title", [](gtcaca_piechart_widget_t *p, const std::string &t) {
      gtcaca_piechart_set_title(p, t.c_str());
    }, py::arg("title"));
  }
  m.def(
      "piechart_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_piechart_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.def("piechart_palette", &gtcaca_piechart_palette, py::arg("i"),
        "The i-th colour of the default pie palette.");

  // -- linechart -----------------------------------------------------------
  {
    py::class_<gtcaca_linechart_widget_t> c(m, "LineChart");
    bind_common(c);
    c.def("draw", &gtcaca_linechart_draw);
    c.def(
        "add_series",
        [](gtcaca_linechart_widget_t *c_, const std::vector<double> &y, uint8_t colour) {
          return gtcaca_linechart_add_series(c_, y.empty() ? nullptr : y.data(),
                                             (int)y.size(), colour);
        },
        py::arg("y"), py::arg("colour"));
    c.def("clear", &gtcaca_linechart_clear);
    c.def("set_range", &gtcaca_linechart_set_range, py::arg("ymin"), py::arg("ymax"));
    c.def(
        "set_xspan",
        [](gtcaca_linechart_widget_t *c_, double xspan, const std::string &unit) {
          gtcaca_linechart_set_xspan(c_, xspan, unit.c_str());
        },
        py::arg("xspan"), py::arg("unit") = std::string(""));
    c.def("set_log_y", [](gtcaca_linechart_widget_t *c_, bool on) {
      gtcaca_linechart_set_log_y(c_, on ? 1 : 0);
    }, py::arg("on"));
    c.def("set_title", [](gtcaca_linechart_widget_t *c_, const std::string &t) {
      gtcaca_linechart_set_title(c_, t.c_str());
    }, py::arg("title"));
  }
  m.def(
      "linechart_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_linechart_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- scatter -------------------------------------------------------------
  {
    py::class_<gtcaca_scatter_widget_t> c(m, "Scatter");
    bind_common(c);
    c.def("draw", &gtcaca_scatter_draw);
    c.def("add_point", &gtcaca_scatter_add_point, py::arg("x"), py::arg("y"),
          py::arg("colour"));
    c.def(
        "add_series",
        [](gtcaca_scatter_widget_t *s, const std::vector<double> &xs,
           const std::vector<double> &ys, uint8_t colour) {
          int n = (int)std::min(xs.size(), ys.size());
          gtcaca_scatter_add_series(s, xs.data(), ys.data(), n, colour);
        },
        py::arg("xs"), py::arg("ys"), py::arg("colour"));
    c.def(
        "set_data",
        [](gtcaca_scatter_widget_t *s, const std::vector<double> &xs,
           const std::vector<double> &ys, uint8_t colour) {
          int n = (int)std::min(xs.size(), ys.size());
          gtcaca_scatter_set_data(s, xs.data(), ys.data(), n, colour);
        },
        py::arg("xs"), py::arg("ys"), py::arg("colour"));
    c.def("clear", &gtcaca_scatter_clear);
    c.def("set_bounds", &gtcaca_scatter_set_bounds, py::arg("xmin"), py::arg("xmax"),
          py::arg("ymin"), py::arg("ymax"));
    c.def("set_autoscale", [](gtcaca_scatter_widget_t *s, bool on) {
      gtcaca_scatter_set_autoscale(s, on ? 1 : 0);
    }, py::arg("on"));
    c.def("set_glyph", &gtcaca_scatter_set_glyph, py::arg("glyph"));
    c.def("set_title", [](gtcaca_scatter_widget_t *s, const std::string &t) {
      gtcaca_scatter_set_title(s, t.c_str());
    }, py::arg("title"));
  }
  m.def(
      "scatter_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_scatter_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- map -----------------------------------------------------------------
  {
    py::class_<gtcaca_map_widget_t> c(m, "Map");
    bind_common(c);
    c.def("draw", &gtcaca_map_draw);
    c.def("set_bounds", &gtcaca_map_set_bounds, py::arg("lon_min"), py::arg("lon_max"),
          py::arg("lat_min"), py::arg("lat_max"));
    c.def(
        "add_point",
        [](gtcaca_map_widget_t *mp, double lat, double lon, uint32_t glyph,
           uint8_t colour, py::object label) {
          std::string l;
          if (!label.is_none()) l = label.cast<std::string>();
          gtcaca_map_add_point(mp, lat, lon, glyph, colour,
                               label.is_none() ? nullptr : l.c_str());
        },
        py::arg("lat"), py::arg("lon"), py::arg("glyph") = 0,
        py::arg("colour") = (uint8_t)CACA_WHITE, py::arg("label") = py::none());
    c.def(
        "add_city",
        [](gtcaca_map_widget_t *mp, const std::string &name, uint32_t glyph,
           uint8_t colour) {
          return gtcaca_map_add_city(mp, name.c_str(), glyph, colour) != 0;
        },
        py::arg("name"), py::arg("glyph") = 0,
        py::arg("colour") = (uint8_t)CACA_WHITE,
        "Plot a known city by name; False if the name is unknown.");
    c.def(
        "add_polyline",
        [](gtcaca_map_widget_t *mp, const std::vector<double> &lonlat, uint8_t colour) {
          gtcaca_map_add_polyline(mp, lonlat.empty() ? nullptr : lonlat.data(),
                                  (int)(lonlat.size() / 2), colour);
        },
        py::arg("lonlat"), py::arg("colour"),
        "Flat [lon0, lat0, lon1, lat1, ...] sequence.");
    c.def("add_world", &gtcaca_map_add_world, py::arg("colour"));
    c.def("clear", &gtcaca_map_clear);
    c.def("set_graticule", [](gtcaca_map_widget_t *mp, bool on) {
      gtcaca_map_set_graticule(mp, on ? 1 : 0);
    }, py::arg("on"));
    c.def("set_title", [](gtcaca_map_widget_t *mp, const std::string &t) {
      gtcaca_map_set_title(mp, t.c_str());
    }, py::arg("title"));
    c.def(
        "project",
        [](gtcaca_map_widget_t *mp, double lon, double lat) -> py::object {
          int sx = 0, sy = 0;
          if (!gtcaca_map_project(mp, lon, lat, &sx, &sy))
            return py::none();
          return py::make_tuple(sx, sy);
        },
        py::arg("lon"), py::arg("lat"), "Screen (x, y), or None if off-map.");
    c.def("key", [](gtcaca_map_widget_t *mp, int key) {
      return gtcaca_map_key(mp, key, nullptr);
    }, py::arg("key"));
    c.def("selected", &gtcaca_map_selected, "Selected marker index, or -1.");
  }
  m.def(
      "map_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_map_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);
  m.def(
      "map_cities",
      []() {
        int n = 0;
        const gtcaca_map_city_t *cities = gtcaca_map_cities(&n);
        py::list out;
        for (int i = 0; i < n; i++)
          out.append(py::make_tuple(std::string(cities[i].name), cities[i].lat,
                                    cities[i].lon));
        return out;
      },
      "The built-in city table as [(name, lat, lon), ...].");
  m.def(
      "map_find_city",
      [](const std::string &name) -> py::object {
        const gtcaca_map_city_t *c = gtcaca_map_find_city(name.c_str());
        if (!c)
          return py::none();
        return py::make_tuple(std::string(c->name), c->lat, c->lon);
      },
      py::arg("name"), "(name, lat, lon) or None.");

  // -- mindmap -------------------------------------------------------------
  {
    py::class_<gtcaca_mm_node_t> n(m, "MindMapNode");
    n.def("set_text", [](gtcaca_mm_node_t *node, const std::string &t) {
      gtcaca_mindmap_set_text(node, t.c_str());
    }, py::arg("text"));
    n.def("toggle_fold", [](gtcaca_mm_node_t *node) {
      gtcaca_mindmap_toggle_fold(node);
    });

    py::class_<gtcaca_mindmap_widget_t> c(m, "MindMap");
    bind_common(c);
    c.def("draw", &gtcaca_mindmap_draw);
    c.def("root", &gtcaca_mindmap_root, py::return_value_policy::reference);
    c.def("add_child", &gtcaca_mindmap_add_child, py::arg("parent"), py::arg("text"),
          py::return_value_policy::reference);
    c.def("add_sibling", &gtcaca_mindmap_add_sibling, py::arg("node"), py::arg("text"),
          py::return_value_policy::reference);
    c.def("delete", &gtcaca_mindmap_delete, py::arg("node"));
    c.def("selected", &gtcaca_mindmap_selected, py::return_value_policy::reference);
    c.def("select", &gtcaca_mindmap_select, py::arg("node"));
    c.def("set_title", [](gtcaca_mindmap_widget_t *mm, const std::string &t) {
      gtcaca_mindmap_set_title(mm, t.c_str());
    }, py::arg("title"));
    c.def("clear", [](gtcaca_mindmap_widget_t *mm, const std::string &root_text) {
      gtcaca_mindmap_clear(mm, root_text.c_str());
    }, py::arg("root_text"));
    c.def("key", [](gtcaca_mindmap_widget_t *mm, int key) {
      return gtcaca_mindmap_key(mm, key, nullptr);
    }, py::arg("key"));
  }
  m.def(
      "mindmap_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_mindmap_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- box layout ----------------------------------------------------------
  {
    py::class_<BoxHandle> c(m, "Box");
    c.def("add", [](BoxHandle &b, py::object w) {
      gtcaca_box_add(b.p, to_parent(w));
    }, py::arg("widget"), "Add at its natural size.");
    c.def("add_expand", [](BoxHandle &b, py::object w) {
      gtcaca_box_add_expand(b.p, to_parent(w));
    }, py::arg("widget"), "Add, taking any leftover space.");
    c.def("add_box", [](BoxHandle &b, BoxHandle &child) {
      gtcaca_box_add_box(b.p, child.p);
    }, py::arg("child"), py::keep_alive<1, 2>());
    c.def("add_stretch", [](BoxHandle &b) { gtcaca_box_add_stretch(b.p); });
    c.def("add_spacing", [](BoxHandle &b, int size) {
      gtcaca_box_add_spacing(b.p, size);
    }, py::arg("size"));
    c.def("set_spacing", [](BoxHandle &b, int s) { gtcaca_box_set_spacing(b.p, s); },
          py::arg("spacing"));
    c.def("set_margin", [](BoxHandle &b, int mg) { gtcaca_box_set_margin(b.p, mg); },
          py::arg("margin"));
    c.def("set_align", [](BoxHandle &b, int a) {
      gtcaca_box_set_align(b.p, (gtcaca_align_t)a);
    }, py::arg("align"));
    c.def("preferred_width", [](BoxHandle &b) { return gtcaca_box_preferred_width(b.p); });
    c.def("preferred_height", [](BoxHandle &b) { return gtcaca_box_preferred_height(b.p); });
    c.def("apply", [](BoxHandle &b, int x, int y, int w, int h) {
      gtcaca_box_apply(b.p, x, y, w, h);
    }, py::arg("x"), py::arg("y"), py::arg("width"), py::arg("height"),
       "Lay the children out inside this rectangle.");
    c.def("apply_window", [](BoxHandle &b, gtcaca_window_widget_t *win) {
      gtcaca_box_apply_window(b.p, win);
    }, py::arg("window"), "Lay out inside a window's content area.");
  }
  m.def("vbox_new", []() { return BoxHandle{gtcaca_vbox_new()}; },
        "A vertical box layout (positions widgets; draws nothing itself).");
  m.def("hbox_new", []() { return BoxHandle{gtcaca_hbox_new()}; },
        "A horizontal box layout.");
  m.def("box_new", [](int orientation) {
    return BoxHandle{gtcaca_box_new((gtcaca_box_orientation_t)orientation)};
  }, py::arg("orientation"), "BOX_VERTICAL or BOX_HORIZONTAL.");
  m.attr("BOX_VERTICAL") = (int)GTCACA_BOX_VERTICAL;
  m.attr("BOX_HORIZONTAL") = (int)GTCACA_BOX_HORIZONTAL;
  m.attr("ALIGN_START") = (int)GTCACA_ALIGN_START;
  m.attr("ALIGN_CENTER") = (int)GTCACA_ALIGN_CENTER;
  m.attr("ALIGN_END") = (int)GTCACA_ALIGN_END;

  // -- rope ----------------------------------------------------------------
  {
    py::class_<RopeHandle> c(m, "Rope");
    c.def("__len__", [](RopeHandle &r) { return gtcaca_rope_len(r.p); });
    c.def("lines", [](RopeHandle &r) { return gtcaca_rope_lines(r.p); });
    c.def("insert", [](RopeHandle &r, int pos, const std::string &s) {
      gtcaca_rope_insert(r.p, pos, s.data(), (int)s.size());
    }, py::arg("pos"), py::arg("text"));
    c.def("delete", [](RopeHandle &r, int pos, int len) {
      gtcaca_rope_delete(r.p, pos, len);
    }, py::arg("pos"), py::arg("len"));
    c.def("clear", [](RopeHandle &r) { gtcaca_rope_clear(r.p); });
    c.def("at", [](RopeHandle &r, int pos) {
      return std::string(1, gtcaca_rope_at(r.p, pos));
    }, py::arg("pos"));
    c.def("__str__", [](RopeHandle &r) {
      char *s = gtcaca_rope_str(r.p);
      std::string out(s ? s : "");
      free(s);                       // gtcaca_rope_str returns a malloc'd copy
      return out;
    });
    c.def("copy", [](RopeHandle &r, int pos, int len) {
      std::vector<char> buf((size_t)len + 1, '\0');
      int n = gtcaca_rope_copy(r.p, pos, len, buf.data());
      return std::string(buf.data(), (size_t)(n < 0 ? 0 : n));
    }, py::arg("pos"), py::arg("len"));
    c.def("line_of", [](RopeHandle &r, int pos) { return gtcaca_rope_line_of(r.p, pos); },
          py::arg("pos"));
    c.def("line_start", [](RopeHandle &r, int line) {
      return gtcaca_rope_line_start(r.p, line);
    }, py::arg("line"));
    c.def("find", [](RopeHandle &r, int from, const std::string &b) {
      return gtcaca_rope_find(r.p, from, b.empty() ? '\0' : b[0]);
    }, py::arg("from_pos"), py::arg("byte"));
    c.def("rfind", [](RopeHandle &r, int from, const std::string &b) {
      return gtcaca_rope_rfind(r.p, from, b.empty() ? '\0' : b[0]);
    }, py::arg("from_pos"), py::arg("byte"));
  }
  m.def("rope_new", []() { return RopeHandle{gtcaca_rope_new()}; },
        "A rope: a text buffer with cheap insert/delete at any offset.");
  m.def("rope_from", [](const std::string &s) {
    return RopeHandle{gtcaca_rope_from(s.data(), (int)s.size())};
  }, py::arg("text"));

  // -- table ---------------------------------------------------------------
  {
    py::class_<gtcaca_table_widget_t> c(m, "Table");
    bind_common(c);
    c.def("draw", &gtcaca_table_draw);
    c.def(
        "set_model",
        [](gtcaca_table_widget_t *t, py::object model) {
          auto *pm = new PyTableModel();
          pm->row_count = tbl_row_count;
          pm->col_count = tbl_col_count;
          pm->header = tbl_header;
          pm->cell = tbl_cell;
          pm->userdata = nullptr;
          pm->f_row_count = model.attr("row_count");
          pm->f_col_count = model.attr("col_count");
          pm->f_header = model.attr("header");
          pm->f_cell = model.attr("cell");
          if (py::hasattr(model, "row_color")) {
            pm->f_row_color = model.attr("row_color");
            pm->row_color = tbl_row_color;
          } else {
            pm->row_color = nullptr;
          }
          g_models.push_back(model);          // keep the Python model alive
          gtcaca_table_set_model(t, pm);
        },
        py::arg("model"),
        "Attach a model object exposing row_count(), col_count(), header(col),\n"
        "cell(row, col) and optionally row_color(row) -> (fg, bg) | None.");
    c.def(
        "set_column_widths",
        [](gtcaca_table_widget_t *t, const std::vector<int> &w) {
          gtcaca_table_set_column_widths(t, w.empty() ? nullptr : w.data(), (int)w.size());
        },
        py::arg("widths"));
    c.def("key", [](gtcaca_table_widget_t *t, int key) {
      return gtcaca_table_key(t, key, nullptr);
    }, py::arg("key"));
    c.def("selected_row", &gtcaca_table_selected_row);
    c.def("current_row", &gtcaca_table_current_row);
    c.def("current_col", &gtcaca_table_current_col);
    c.def("set_current", &gtcaca_table_set_current, py::arg("row"), py::arg("col"));
    c.def("set_title", [](gtcaca_table_widget_t *t, const std::string &s) {
      gtcaca_table_set_title(t, s.c_str());
    }, py::arg("title"));
  }
  m.def(
      "table_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_table_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- tree ----------------------------------------------------------------
  {
    py::class_<gtcaca_tree_widget_t> c(m, "Tree");
    bind_common(c);
    c.def("draw", &gtcaca_tree_draw);
    c.def(
        "set_model",
        [](gtcaca_tree_widget_t *t, py::object model) {
          auto *pm = new PyTreeModel();
          pm->child_count = tr_child_count;
          pm->child = tr_child;
          pm->label = tr_label;
          pm->has_children = tr_has_children;
          pm->userdata = nullptr;
          pm->f_child_count = model.attr("child_count");
          pm->f_child = model.attr("child");
          pm->f_label = model.attr("label");
          pm->f_has_children = model.attr("has_children");
          if (py::hasattr(model, "draw_row")) {
            pm->f_draw_row = model.attr("draw_row");
            pm->draw_row = tr_draw_row;
          } else {
            pm->draw_row = nullptr;
          }
          g_models.push_back(model);
          gtcaca_tree_set_model(t, pm);
        },
        py::arg("model"),
        "Attach a model exposing child_count(node), child(node, i), label(node)\n"
        "and has_children(node); nodes are ints, the root level is None.  An\n"
        "optional draw_row(node, x, y, width, selected) paints rows itself.");
    c.def("key", [](gtcaca_tree_widget_t *t, int key) {
      return gtcaca_tree_key(t, key, nullptr);
    }, py::arg("key"));
    c.def("visible_count", &gtcaca_tree_visible_count);
    c.def("selected_node", [](gtcaca_tree_widget_t *t) {
      return node_to_py(gtcaca_tree_selected_node(t));
    });
    c.def("select", [](gtcaca_tree_widget_t *t, py::object node) {
      gtcaca_tree_select(t, py_to_node(node));
    }, py::arg("node"));
    c.def("set_title", [](gtcaca_tree_widget_t *t, const std::string &s) {
      gtcaca_tree_set_title(t, s.c_str());
    }, py::arg("title"));
  }
  m.def(
      "tree_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_tree_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference);

  // -- editor --------------------------------------------------------------
  {
    py::class_<LangCfgHandle> lc(m, "EditorLangCfg");
    lc.def("set_line_comment", [](LangCfgHandle &c, const std::string &p) {
      gtcaca_editor_langcfg_set_line_comment(c.p, p.c_str());
    }, py::arg("prefix"));
    lc.def("set_block_comment", [](LangCfgHandle &c, const std::string &o,
                                   const std::string &cl) {
      gtcaca_editor_langcfg_set_block_comment(c.p, o.c_str(), cl.c_str());
    }, py::arg("open"), py::arg("close"));
    lc.def("add_bracket", [](LangCfgHandle &c, const std::string &o,
                             const std::string &cl) {
      gtcaca_editor_langcfg_add_bracket(c.p, o.c_str(), cl.c_str());
    }, py::arg("open"), py::arg("close"));
    lc.def("add_string_delimiter", [](LangCfgHandle &c, const std::string &d) {
      gtcaca_editor_langcfg_add_string_delimiter(c.p, d.c_str());
    }, py::arg("delim"));
    lc.def("set_keywords", [](LangCfgHandle &c, const std::vector<std::string> &words) {
      std::vector<const char *> p;
      p.reserve(words.size());
      for (const auto &w : words) p.push_back(w.c_str());
      gtcaca_editor_langcfg_set_keywords(c.p, p.empty() ? nullptr : p.data(),
                                         (int)p.size());
    }, py::arg("words"));
    lc.def("load_json", [](LangCfgHandle &c, const std::string &path) {
      return gtcaca_editor_langcfg_load_json(c.p, path.c_str());
    }, py::arg("path"));
    lc.def("line_comment", [](LangCfgHandle &c) {
      const char *s = gtcaca_editor_langcfg_get_line_comment(c.p);
      return s ? std::string(s) : std::string();
    });

    py::class_<GrammarHandle>(m, "EditorGrammar");

    py::class_<gtcaca_editor_widget_t> c(m, "Editor");
    bind_common(c);
    c.def("draw", &gtcaca_editor_draw);
    // text
    c.def("set_text", [](gtcaca_editor_widget_t *w, const std::string &t) {
      gtcaca_editor_set_text(w, t.c_str());
    }, py::arg("text"));
    c.def("text", [](gtcaca_editor_widget_t *w) {
      const char *s = gtcaca_editor_text(w);
      return s ? std::string(s) : std::string();
    });
    c.def("__len__", &gtcaca_editor_get_length);
    c.def("append_text", [](gtcaca_editor_widget_t *w, const std::string &s) {
      gtcaca_editor_append_text(w, s.data(), (int)s.size());
    }, py::arg("text"));
    c.def("insert_text", [](gtcaca_editor_widget_t *w, int pos, const std::string &s) {
      gtcaca_editor_insert_text(w, pos, s.c_str());
    }, py::arg("pos"), py::arg("text"));
    c.def("delete_range", &gtcaca_editor_delete_range, py::arg("pos"), py::arg("len"));
    c.def("clear_all", &gtcaca_editor_clear_all);
    c.def("char_at", [](gtcaca_editor_widget_t *w, int pos) {
      return std::string(1, gtcaca_editor_get_char_at(w, pos));
    }, py::arg("pos"));
    c.def("text_range", [](gtcaca_editor_widget_t *w, int start, int end) {
      int n = end - start;
      if (n < 0) n = 0;
      std::vector<char> buf((size_t)n + 1, '\0');
      int got = gtcaca_editor_get_text_range(w, start, end, buf.data(), n + 1);
      return std::string(buf.data(), (size_t)(got < 0 ? 0 : got));
    }, py::arg("start"), py::arg("end"));
    // caret / selection
    c.def("get_current_pos", &gtcaca_editor_get_current_pos);
    c.def("set_current_pos", &gtcaca_editor_set_current_pos, py::arg("pos"));
    c.def("get_anchor", &gtcaca_editor_get_anchor);
    c.def("set_anchor", &gtcaca_editor_set_anchor, py::arg("pos"));
    c.def("set_selection", &gtcaca_editor_set_selection, py::arg("caret"), py::arg("anchor"));
    c.def("set_empty_selection", &gtcaca_editor_set_empty_selection, py::arg("pos"));
    c.def("selection_start", &gtcaca_editor_get_selection_start);
    c.def("selection_end", &gtcaca_editor_get_selection_end);
    c.def("selected_text", [](gtcaca_editor_widget_t *w) {
      int n = gtcaca_editor_get_selection_end(w) - gtcaca_editor_get_selection_start(w);
      if (n < 0) n = 0;
      std::vector<char> buf((size_t)n + 1, '\0');
      int got = gtcaca_editor_get_selected_text(w, buf.data(), n + 1);
      return std::string(buf.data(), (size_t)(got < 0 ? 0 : got));
    });
    c.def("goto_pos", &gtcaca_editor_goto_pos, py::arg("pos"));
    c.def("goto_line", &gtcaca_editor_goto_line, py::arg("line"));
    // lines
    c.def("line_count", &gtcaca_editor_get_line_count);
    c.def("line_from_position", &gtcaca_editor_line_from_position, py::arg("pos"));
    c.def("position_from_line", &gtcaca_editor_position_from_line, py::arg("line"));
    c.def("line_end_position", &gtcaca_editor_get_line_end_position, py::arg("line"));
    c.def("column", &gtcaca_editor_get_column, py::arg("pos"));
    c.def("current_line", &gtcaca_editor_get_current_line);
    c.def("find_column", &gtcaca_editor_find_column, py::arg("line"), py::arg("column"));
    c.def("position_from_point", &gtcaca_editor_position_from_point, py::arg("sx"), py::arg("sy"));
    // movement
    c.def("char_left", &gtcaca_editor_char_left);
    c.def("char_left_extend", &gtcaca_editor_char_left_extend);
    c.def("char_right", &gtcaca_editor_char_right);
    c.def("char_right_extend", &gtcaca_editor_char_right_extend);
    c.def("line_up", &gtcaca_editor_line_up);
    c.def("line_up_extend", &gtcaca_editor_line_up_extend);
    c.def("line_down", &gtcaca_editor_line_down);
    c.def("line_down_extend", &gtcaca_editor_line_down_extend);
    c.def("home", &gtcaca_editor_home);
    c.def("home_extend", &gtcaca_editor_home_extend);
    c.def("line_end", &gtcaca_editor_line_end);
    c.def("line_end_extend", &gtcaca_editor_line_end_extend);
    c.def("document_start", &gtcaca_editor_document_start);
    c.def("document_start_extend", &gtcaca_editor_document_start_extend);
    c.def("document_end", &gtcaca_editor_document_end);
    c.def("document_end_extend", &gtcaca_editor_document_end_extend);
    c.def("page_up", &gtcaca_editor_page_up);
    c.def("page_up_extend", &gtcaca_editor_page_up_extend);
    c.def("page_down", &gtcaca_editor_page_down);
    c.def("page_down_extend", &gtcaca_editor_page_down_extend);
    c.def("word_left", &gtcaca_editor_word_left);
    c.def("word_left_extend", &gtcaca_editor_word_left_extend);
    c.def("word_right", &gtcaca_editor_word_right);
    c.def("word_right_extend", &gtcaca_editor_word_right_extend);
    c.def("word_left_position", &gtcaca_editor_word_left_position, py::arg("pos"));
    c.def("word_right_position", &gtcaca_editor_word_right_position, py::arg("pos"));
    c.def("del_word_left", &gtcaca_editor_del_word_left);
    c.def("del_word_right", &gtcaca_editor_del_word_right);
    // editing
    c.def("add_char", [](gtcaca_editor_widget_t *w, const std::string &ch) {
      if (!ch.empty()) gtcaca_editor_add_char(w, ch[0]);
    }, py::arg("ch"));
    c.def("add_char_utf32", &gtcaca_editor_add_char_utf32, py::arg("codepoint"));
    c.def("new_line", &gtcaca_editor_new_line);
    c.def("delete_back", &gtcaca_editor_delete_back);
    c.def("clear", &gtcaca_editor_clear, "Delete forward, or the selection.");
    c.def("line_duplicate", &gtcaca_editor_line_duplicate);
    c.def("line_transpose", &gtcaca_editor_line_transpose);
    c.def("line_delete", &gtcaca_editor_line_delete);
    c.def("selection_duplicate", &gtcaca_editor_selection_duplicate);
    c.def("upper_case", &gtcaca_editor_upper_case);
    c.def("lower_case", &gtcaca_editor_lower_case);
    // rectangular selection
    c.def("set_rectangular_selection", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_rectangular_selection(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_rectangular_selection", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_rectangular_selection(w) != 0;
    });
    c.def("rect_string_insert", [](gtcaca_editor_widget_t *w, const std::string &s) {
      gtcaca_editor_rect_string_insert(w, s.c_str());
    }, py::arg("text"));
    c.def("rect_kill", &gtcaca_editor_rect_kill);
    c.def("rect_delete", &gtcaca_editor_rect_delete);
    c.def("rect_clear", &gtcaca_editor_rect_clear);
    c.def("rect_copy", &gtcaca_editor_rect_copy);
    c.def("rect_yank", &gtcaca_editor_rect_yank);
    // undo
    c.def("undo", &gtcaca_editor_undo);
    c.def("redo", &gtcaca_editor_redo);
    c.def("can_undo", [](gtcaca_editor_widget_t *w) { return gtcaca_editor_can_undo(w) != 0; });
    c.def("can_redo", [](gtcaca_editor_widget_t *w) { return gtcaca_editor_can_redo(w) != 0; });
    c.def("empty_undo_buffer", &gtcaca_editor_empty_undo_buffer);
    c.def("begin_undo_action", &gtcaca_editor_begin_undo_action);
    c.def("end_undo_action", &gtcaca_editor_end_undo_action);
    c.def("set_save_point", &gtcaca_editor_set_save_point);
    c.def("get_modify", [](gtcaca_editor_widget_t *w) { return gtcaca_editor_get_modify(w) != 0; });
    // view options
    c.def("set_line_numbers", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_line_numbers(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_line_numbers", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_line_numbers(w) != 0;
    });
    c.def("set_tab_width", &gtcaca_editor_set_tab_width, py::arg("n"));
    c.def("get_tab_width", &gtcaca_editor_get_tab_width);
    c.def("set_wrap", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_wrap(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_wrap", [](gtcaca_editor_widget_t *w) { return gtcaca_editor_get_wrap(w) != 0; });
    c.def("set_read_only", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_read_only(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_read_only", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_read_only(w) != 0;
    });
    c.def("set_overtype", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_overtype(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_overtype", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_overtype(w) != 0;
    });
    c.def("set_view_whitespace", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_view_whitespace(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_view_whitespace", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_view_whitespace(w) != 0;
    });
    c.def("set_caret_line_visible", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_caret_line_visible(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("set_caret_line_back", &gtcaca_editor_set_caret_line_back, py::arg("colour"));
    c.def("set_edge_column", &gtcaca_editor_set_edge_column, py::arg("column"));
    c.def("get_edge_column", &gtcaca_editor_get_edge_column);
    c.def("set_edge_colour", &gtcaca_editor_set_edge_colour, py::arg("colour"));
    // styling
    c.def("style_set_fore", &gtcaca_editor_style_set_fore, py::arg("style"), py::arg("color"));
    c.def("style_set_back", &gtcaca_editor_style_set_back, py::arg("style"), py::arg("color"));
    c.def("style_clear_back", &gtcaca_editor_style_clear_back, py::arg("style"));
    c.def("set_selection_colors", &gtcaca_editor_set_selection_colors, py::arg("fore"),
          py::arg("back"));
    c.def("set_bg_rgb12", &gtcaca_editor_set_bg_rgb12, py::arg("rgb12"));
    c.def("set_fg_rgb12", &gtcaca_editor_set_fg_rgb12, py::arg("rgb12"));
    c.def("style_set_fore_rgb12", &gtcaca_editor_style_set_fore_rgb12, py::arg("style"),
          py::arg("rgb12"));
    c.def("style_set_bold", [](gtcaca_editor_widget_t *w, int s, bool on) {
      gtcaca_editor_style_set_bold(w, s, on ? 1 : 0);
    }, py::arg("style"), py::arg("on"));
    c.def("style_set_italic", [](gtcaca_editor_widget_t *w, int s, bool on) {
      gtcaca_editor_style_set_italic(w, s, on ? 1 : 0);
    }, py::arg("style"), py::arg("on"));
    c.def("style_set_underline", [](gtcaca_editor_widget_t *w, int s, bool on) {
      gtcaca_editor_style_set_underline(w, s, on ? 1 : 0);
    }, py::arg("style"), py::arg("on"));
    c.def("style_set_visible", [](gtcaca_editor_widget_t *w, int s, bool on) {
      gtcaca_editor_style_set_visible(w, s, on ? 1 : 0);
    }, py::arg("style"), py::arg("on"));
    c.def("style_reset_default", &gtcaca_editor_style_reset_default);
    c.def("style_clear_all", &gtcaca_editor_style_clear_all);
    c.def("colourize", &gtcaca_editor_colourize);
    // folding
    c.def("set_fold_margin", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_fold_margin(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_fold_margin", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_fold_margin(w) != 0;
    });
    c.def("set_fold_level", &gtcaca_editor_set_fold_level, py::arg("line"), py::arg("level"));
    c.def("get_fold_level", &gtcaca_editor_get_fold_level, py::arg("line"));
    c.def("set_fold_expanded", [](gtcaca_editor_widget_t *w, int line, bool on) {
      gtcaca_editor_set_fold_expanded(w, line, on ? 1 : 0);
    }, py::arg("line"), py::arg("expanded"));
    c.def("get_fold_expanded", [](gtcaca_editor_widget_t *w, int line) {
      return gtcaca_editor_get_fold_expanded(w, line) != 0;
    }, py::arg("line"));
    c.def("toggle_fold", &gtcaca_editor_toggle_fold, py::arg("line"));
    c.def("get_line_visible", [](gtcaca_editor_widget_t *w, int line) {
      return gtcaca_editor_get_line_visible(w, line) != 0;
    }, py::arg("line"));
    c.def("fold_by_indentation", &gtcaca_editor_fold_by_indentation);
    c.def("fold_all", [](gtcaca_editor_widget_t *w, bool expanded) {
      gtcaca_editor_fold_all(w, expanded ? 1 : 0);
    }, py::arg("expanded"));
    // annotations
    c.def("annotation_set_text", [](gtcaca_editor_widget_t *w, int line,
                                    const std::string &t) {
      gtcaca_editor_annotation_set_text(w, line, t.c_str());
    }, py::arg("line"), py::arg("text"));
    c.def("annotation_text", [](gtcaca_editor_widget_t *w, int line) {
      std::vector<char> buf(4096, '\0');
      int n = gtcaca_editor_annotation_get_text(w, line, buf.data(), (int)buf.size());
      return std::string(buf.data(), (size_t)(n < 0 ? 0 : n));
    }, py::arg("line"));
    c.def("annotation_set_style", &gtcaca_editor_annotation_set_style, py::arg("line"),
          py::arg("style"));
    c.def("annotation_get_style", &gtcaca_editor_annotation_get_style, py::arg("line"));
    c.def("annotation_clear_all", &gtcaca_editor_annotation_clear_all);
    c.def("annotation_set_visible", &gtcaca_editor_annotation_set_visible, py::arg("mode"));
    c.def("annotation_get_visible", &gtcaca_editor_annotation_get_visible);
    c.def("annotation_get_lines", &gtcaca_editor_annotation_get_lines, py::arg("line"));
    // search
    c.def("set_search_flags", &gtcaca_editor_set_search_flags, py::arg("flags"));
    c.def("get_search_flags", &gtcaca_editor_get_search_flags);
    c.def("set_target_start", &gtcaca_editor_set_target_start, py::arg("pos"));
    c.def("get_target_start", &gtcaca_editor_get_target_start);
    c.def("set_target_end", &gtcaca_editor_set_target_end, py::arg("pos"));
    c.def("get_target_end", &gtcaca_editor_get_target_end);
    c.def("set_target_range", &gtcaca_editor_set_target_range, py::arg("start"), py::arg("end"));
    c.def("target_whole_document", &gtcaca_editor_target_whole_document);
    c.def("target_from_selection", &gtcaca_editor_target_from_selection);
    c.def("search_in_target", [](gtcaca_editor_widget_t *w, const std::string &t) {
      return gtcaca_editor_search_in_target(w, t.c_str());
    }, py::arg("text"));
    c.def("replace_target", [](gtcaca_editor_widget_t *w, const std::string &t) {
      return gtcaca_editor_replace_target(w, t.c_str());
    }, py::arg("text"));
    c.def(
        "find_text",
        [](gtcaca_editor_widget_t *w, int flags, const std::string &text, int min_pos,
           int max_pos) -> py::object {
          int end = 0;
          int start = gtcaca_editor_find_text(w, flags, text.c_str(), min_pos, max_pos, &end);
          if (start < 0)
            return py::none();
          return py::make_tuple(start, end);
        },
        py::arg("flags"), py::arg("text"), py::arg("min_pos"), py::arg("max_pos"),
        "(start, end) of the match, or None.");
    c.def("search_anchor", &gtcaca_editor_search_anchor);
    c.def("search_next", [](gtcaca_editor_widget_t *w, int flags, const std::string &t) {
      return gtcaca_editor_search_next(w, flags, t.c_str());
    }, py::arg("flags"), py::arg("text"));
    c.def("search_prev", [](gtcaca_editor_widget_t *w, int flags, const std::string &t) {
      return gtcaca_editor_search_prev(w, flags, t.c_str());
    }, py::arg("flags"), py::arg("text"));
    // braces
    c.def("brace_match", &gtcaca_editor_brace_match, py::arg("pos"));
    c.def("brace_highlight", &gtcaca_editor_brace_highlight, py::arg("pos1"), py::arg("pos2"));
    c.def("brace_badlight", &gtcaca_editor_brace_badlight, py::arg("pos"));
    // json / language
    c.def("set_json_mode", [](gtcaca_editor_widget_t *w, bool on) {
      gtcaca_editor_set_json_mode(w, on ? 1 : 0);
    }, py::arg("on"));
    c.def("get_json_mode", [](gtcaca_editor_widget_t *w) {
      return gtcaca_editor_get_json_mode(w) != 0;
    });
    c.def("fold_json", &gtcaca_editor_fold_json);
    c.def("set_langcfg", [](gtcaca_editor_widget_t *w, LangCfgHandle &cfg) {
      gtcaca_editor_set_langcfg(w, cfg.p);
    }, py::arg("cfg"), py::keep_alive<1, 2>());
    c.def("set_grammar", [](gtcaca_editor_widget_t *w, GrammarHandle &g) {
      gtcaca_editor_set_grammar(w, g.p);
    }, py::arg("grammar"), py::keep_alive<1, 2>());
    // callbacks
    c.def(
        "on_key",
        [](gtcaca_editor_widget_t *w, py::function cb) {
          g_key_cbs[static_cast<const void *>(w)] = std::move(cb);
          gtcaca_editor_key_cb_register(w, tramp_editor_key, nullptr);
        },
        py::arg("callback"), "callback(key: int) -> int");
  }
  m.def(
      "editor_new",
      [](py::object parent, int x, int y, int w, int h) {
        return gtcaca_editor_new(to_parent(parent), x, y, w, h);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference,
      "A multi-line text editor (Scintilla-ish API).");
  m.def("editor_langcfg_new", []() {
    return LangCfgHandle{gtcaca_editor_langcfg_new()};
  }, "Syntax configuration: comments, brackets, string delimiters, keywords.");
  m.def("editor_grammar_load", [](const std::string &path) {
    return GrammarHandle{gtcaca_editor_grammar_load(path.c_str())};
  }, py::arg("path"));

  // -- custom widget (canvas escape hatch) ---------------------------------
  {
    py::class_<gtcaca_custom_widget_t> c(m, "Custom");
    bind_common(c);
    c.def("draw", &gtcaca_custom_draw);
    c.def(
        "on_draw",
        [](gtcaca_custom_widget_t *w, py::function f) {
          g_draw_cbs[w] = std::move(f);
          gtcaca_custom_set_draw_cb(w, tramp_custom_draw, nullptr);
        },
        py::arg("callback"),
        "Register a callback() invoked to paint the widget each frame.");
    c.def(
        "on_key",
        [](gtcaca_custom_widget_t *w, py::function f) {
          g_key_cbs[w] = std::move(f);
          gtcaca_custom_set_key_cb(w, tramp_custom_key, nullptr);
        },
        py::arg("callback"),
        "Register a callback(key)->int invoked on key events when focused. "
        "Returning non-zero also claims the keys the main loop would otherwise "
        "take for itself: q and Esc (quit), Tab (cycle focus), Ctrl+N/Ctrl+P.");
    c.def(
        "on_mouse",
        [](gtcaca_custom_widget_t *w, py::function f) {
          g_mouse_cbs[w] = std::move(f);
          gtcaca_custom_set_mouse_cb(w, tramp_custom_mouse, nullptr);
        },
        py::arg("callback"),
        "Register a callback(event, x, y, button)->int for mouse events: "
        "event is MOUSE_PRESS / MOUSE_MOTION / MOUSE_RELEASE / MOUSE_WHEEL / "
        "MOUSE_DOUBLE (a second press on the same cell, delivered just after "
        "the press), "
        "x/y are canvas coordinates, button is 1 left, 2 middle, 3 right, "
        "4 wheel down, 5 wheel up. Motion arrives while a button is held.");
    c.def(
        "set_focusable",
        [](gtcaca_custom_widget_t *w, bool v) {
          gtcaca_custom_set_focusable(w, v ? 1 : 0);
        },
        py::arg("focusable"));
    c.def("set_focus",
          [](gtcaca_custom_widget_t *w) { w->has_focus = 1; });
  }
  m.def(
      "custom_new",
      [](py::object parent, int x, int y, int width, int height) {
        return gtcaca_custom_new(to_parent(parent), x, y, width, height);
      },
      py::arg("parent"), py::arg("x"), py::arg("y"), py::arg("width"),
      py::arg("height"), py::return_value_policy::reference,
      "Create a custom widget: a rectangle you paint via on_draw / handle via on_key.");

  // -- colour constants (ANSI 16) ------------------------------------------
  m.attr("BLACK") = (int)CACA_BLACK;
  m.attr("BLUE") = (int)CACA_BLUE;
  m.attr("GREEN") = (int)CACA_GREEN;
  m.attr("CYAN") = (int)CACA_CYAN;
  m.attr("RED") = (int)CACA_RED;
  m.attr("MAGENTA") = (int)CACA_MAGENTA;
  m.attr("BROWN") = (int)CACA_BROWN;
  m.attr("LIGHTGRAY") = (int)CACA_LIGHTGRAY;
  m.attr("DARKGRAY") = (int)CACA_DARKGRAY;
  m.attr("LIGHTBLUE") = (int)CACA_LIGHTBLUE;
  m.attr("LIGHTGREEN") = (int)CACA_LIGHTGREEN;
  m.attr("LIGHTCYAN") = (int)CACA_LIGHTCYAN;
  m.attr("LIGHTRED") = (int)CACA_LIGHTRED;
  m.attr("LIGHTMAGENTA") = (int)CACA_LIGHTMAGENTA;
  m.attr("YELLOW") = (int)CACA_YELLOW;
  m.attr("WHITE") = (int)CACA_WHITE;

  m.def("set_double_click_time", &gtcaca_set_double_click_time, py::arg("ms"),
        "How close together two presses must be to raise MOUSE_DOUBLE (default 400 ms).");
  m.def("get_double_click_time", &gtcaca_get_double_click_time);

  // -- mouse event constants (Custom.on_mouse) -----------------------------
  m.attr("MOUSE_PRESS") = (int)GTCACA_MOUSE_PRESS;
  m.attr("MOUSE_MOTION") = (int)GTCACA_MOUSE_MOTION;
  m.attr("MOUSE_RELEASE") = (int)GTCACA_MOUSE_RELEASE;
  m.attr("MOUSE_WHEEL") = (int)GTCACA_MOUSE_WHEEL;
  m.attr("MOUSE_DOUBLE") = (int)GTCACA_MOUSE_DOUBLE;

  // -- key constants -------------------------------------------------------
  m.attr("KEY_RETURN") = (int)CACA_KEY_RETURN;
  m.attr("KEY_ESCAPE") = (int)CACA_KEY_ESCAPE;
  m.attr("KEY_TAB") = (int)CACA_KEY_TAB;
  m.attr("KEY_BACKSPACE") = (int)CACA_KEY_BACKSPACE;
  m.attr("KEY_DELETE") = (int)CACA_KEY_DELETE;
  m.attr("KEY_UP") = (int)CACA_KEY_UP;
  m.attr("KEY_DOWN") = (int)CACA_KEY_DOWN;
  m.attr("KEY_LEFT") = (int)CACA_KEY_LEFT;
  m.attr("KEY_RIGHT") = (int)CACA_KEY_RIGHT;
  m.attr("KEY_HOME") = (int)CACA_KEY_HOME;
  m.attr("KEY_END") = (int)CACA_KEY_END;
  m.attr("KEY_PAGEUP") = (int)CACA_KEY_PAGEUP;
  m.attr("KEY_PAGEDOWN") = (int)CACA_KEY_PAGEDOWN;
  m.attr("KEY_F1") = (int)CACA_KEY_F1;
  m.attr("KEY_F2") = (int)CACA_KEY_F2;
  m.attr("KEY_F3") = (int)CACA_KEY_F3;
  m.attr("KEY_F4") = (int)CACA_KEY_F4;
  m.attr("KEY_F5") = (int)CACA_KEY_F5;
  m.attr("KEY_F6") = (int)CACA_KEY_F6;
  m.attr("KEY_F7") = (int)CACA_KEY_F7;
  m.attr("KEY_F8") = (int)CACA_KEY_F8;
  m.attr("KEY_F9") = (int)CACA_KEY_F9;
  m.attr("KEY_F10") = (int)CACA_KEY_F10;
}
