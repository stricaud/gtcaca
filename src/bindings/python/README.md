# GTCaca Python bindings

[pybind11](https://pybind11.readthedocs.io/) bindings for **GTCaca**, the
libcaca-based terminal UI toolkit in this repository.

The bindings compile the GTCaca C sources directly into the extension module,
so the only external runtime dependency is **libcaca** itself — no separately
installed `libgtcaca` is required.

## Requirements

- A C/C++ toolchain and CMake ≥ 3.20
- `libcaca` with its development headers, discoverable via `pkg-config`
  (`pkg-config --exists caca` should succeed)
- Python ≥ 3.8 and `pybind11`
- *(optional)* `oniguruma` — not needed; only the editor widget uses it and it
  is not exposed here

## Install

From this directory (`src/bindings/python`):

```sh
pip install .
```

This uses [scikit-build-core](https://scikit-build-core.readthedocs.io/) to drive
CMake and produces an importable `gtcaca` package.

### Building in-tree without installing

```sh
cmake -S . -B build \
  -Dpybind11_DIR="$(python -m pybind11 --cmakedir)"
cmake --build build -j
# the compiled _gtcaca*.so lands in build/; add it to PYTHONPATH or copy it
# next to the gtcaca/ package directory.
```

## Usage

```python
import gtcaca as gt

gt.init()
gt.application_new("Hello")
win = gt.window_new(None, "Demo", 0, 1, 40, 10)
gt.label_new(win, "Press q to quit", 2, 2)

btn = gt.button_new(win, "  OK  ", 2, 4)
def on_key(key):
    if key == gt.KEY_RETURN:
        gt.main_quit()
    return 0          # 0 = event not consumed
btn.on_key(on_key)

win.set_focus()
gt.main()             # blocking event loop
```

See [`example.py`](example.py) for a fuller form with a menu, entries,
checkbox, progress bar and status bar.

## API overview

Factory functions create widgets and return widget objects; the first argument
is the parent (a widget object, or `None` for top-level / canvas-anchored
widgets):

| Factory | Returns | Key methods |
| --- | --- | --- |
| `init()`, `main()`, `main_quit()`, `redraw()` | — | lifecycle |
| `canvas_width()`, `canvas_height()` | `int` | canvas size in cells |
| `application_new(title)` | `Application` | `draw()` |
| `window_new(parent, title, x, y, w, h)` | `Window` | `set_focus`, `set_focused_child`, `set_default`, `focus_next_child`, `close` |
| `window_new_centered(parent, title, w, h)` | `Window` | — |
| `label_new(parent, text, x, y)` | `Label` | — |
| `button_new(parent, label, x, y)` | `Button` | `on_key(cb)` |
| `entry_new(parent, x, y, w)` | `Entry` | `get_text`, `set_text`, `set_secret`, `on_key` |
| `checkbox_new(parent, label, x, y)` | `Checkbox` | `get_checked`, `set_checked`, `on_key` |
| `radiobutton_new(parent, label, group_id, x, y)` | `RadioButton` | `get_active`, `set_active`, `on_key` |
| `combobox_new(parent, x, y, w)` | `ComboBox` | `append`, `get_selected`, `get_selected_index`, `on_key` |
| `progressbar_new(parent, x, y, w)` | `ProgressBar` | `set_value`, `get_value` |
| `textview_new(parent, x, y, w, h)` | `TextView` | `append`, `clear`, `set_mode`, `on_key` |
| `statusbar_new(text)` | `StatusBar` | `set_text`, `set_rows_from_bottom` |
| `menu_new()` | `Menu` | `add_entry`, `add_item`, `add_separator`, `handle_key` |

Every widget exposes the shared preamble (`x`, `y`, `width`, `height`,
`has_focus`, `is_visible`, `id`) plus `show()`, `hide()`, and `as_widget()`
(used internally for parent passing).

### Callbacks

- **Key callbacks** (`on_key`) receive the integer key and may return an `int`
  (`0` = not consumed, the default if you return `None`). Key constants are
  exposed as module attributes: `KEY_RETURN`, `KEY_ESCAPE`, `KEY_TAB`,
  `KEY_UP`/`KEY_DOWN`/`KEY_LEFT`/`KEY_RIGHT`, `KEY_F1`, `KEY_F10`, etc.
- **Menu actions** (the last argument to `menu.add_item`) take no arguments.
- Exceptions raised inside a callback are reported via `sys.unraisablehook`
  and do not abort the event loop.

The GIL is released while `gt.main()` runs and re-acquired inside every
callback, so callbacks can safely touch other Python state.

## Scope

The toolkit ships ~40 widgets; these bindings currently cover the core
lifecycle plus the widgets used in the C demo (`demo/demo.c`). Additional
widgets (tree, table, tabs, calendar, charts, editor, hexview, …) follow the
exact same pattern in [`gtcaca_module.cpp`](gtcaca_module.cpp) — bind the struct
with `bind_common`, add a `*_new` factory with a `return_value_policy::reference`,
and wire any callback through the shared trampoline.
