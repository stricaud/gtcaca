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

#include <deque>
#include <unordered_map>

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
GTCACA_KEY_TRAMPOLINE(tramp_entry, gtcaca_entry_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_checkbox, gtcaca_checkbox_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_radiobutton, gtcaca_radiobutton_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_combobox, gtcaca_combobox_widget_t)
GTCACA_KEY_TRAMPOLINE(tramp_textview, gtcaca_textview_widget_t)

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
  }
  m.def("menu_new", &gtcaca_menu_new, py::return_value_policy::reference);

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
        "Register a callback(key)->int invoked on key events when focused.");
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
