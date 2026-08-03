# GTCaca Python bindings

[pybind11](https://pybind11.readthedocs.io/) bindings for **GTCaca**, a
libcaca-based terminal UI toolkit: windows, widgets, charts, an editor, and a
raw canvas escape hatch — all drawn as text cells.

The bindings compile the GTCaca C sources directly into the extension module, so
the only external runtime dependency is **libcaca** itself. Every widget the
toolkit ships is exposed.

```sh
pip install gtcaca
```

---

## Contents

- [Requirements](#requirements) · [Install](#install)
- [Quick start](#quick-start)
- [Core concepts](#core-concepts) — [lifecycle](#lifecycle), [parents](#widgets-and-parents), [focus](#focus), [callbacks](#callbacks), [colours](#colours)
- **Widgets**
  - [Windows and layout](#windows-and-layout)
  - [Buttons and input](#buttons-and-input)
  - [Text](#text)
  - [Data views: table, tree, hexview](#data-views)
  - [Indicators](#indicators)
  - [Charts and maps](#charts-and-maps)
  - [Menus](#menus)
  - [Dialogs and choosers](#dialogs-and-choosers)
  - [Custom widgets: drawing yourself](#custom-widgets)
- [Utilities: Rope](#utilities)
- [Threading and the GIL](#threading-and-the-gil)
- [API index](#api-index)

---

## Requirements

- A C/C++ toolchain and CMake ≥ 3.20
- `libcaca` with development headers, discoverable via `pkg-config`
  (`pkg-config --exists caca` should succeed)
- Python ≥ 3.9

On macOS: `brew install libcaca`, and if `pkg-config` cannot find it,
`export PKG_CONFIG_PATH="$(brew --prefix libcaca)/lib/pkgconfig"`.
On Debian/Ubuntu: `apt install libcaca-dev`.

## Install

```sh
pip install gtcaca                 # from PyPI
pip install .                      # from a checkout of src/bindings/python
```

Building in-tree without installing:

```sh
cmake -S . -B build -Dpybind11_DIR="$(python -m pybind11 --cmakedir)"
cmake --build build -j
```

---

## Quick start

```python
import gtcaca as gt

gt.init()
gt.application_new("Hello")
win = gt.window_new(None, "Demo", 0, 1, 40, 10)
gt.label_new(win, "Press Enter on the button", 2, 2)

btn = gt.button_new(win, "  OK  ", 2, 4)

def on_key(key):
    if key == gt.KEY_RETURN:
        gt.main_quit()
    return 0            # 0 = not consumed; let others see the key

btn.on_key(on_key)
win.set_focus()
gt.main()               # blocking event loop
gt.present_shutdown()
```

See [`example.py`](example.py) for a fuller program.

---

## Core concepts

### Lifecycle

| Call | Purpose |
| --- | --- |
| `gt.init()` | open the display; must precede any widget creation |
| `gt.main()` | run the blocking event loop |
| `gt.main_quit()` | ask the loop to exit (call from a callback) |
| `gt.redraw()` | repaint now — call after changing state from a callback |
| `gt.present_shutdown()` | tear the display down |
| `gt.canvas_width()`, `gt.canvas_height()` | canvas size in character cells |

The display driver is chosen by libcaca; `CACA_DRIVER=null` runs headless, which
is what makes the widgets testable without a terminal.

### Widgets and parents

Every widget comes from a `*_new()` factory whose first argument is the parent —
another widget, or `None` for a top-level/canvas-anchored widget:

```python
win  = gt.window_new(None, "Title", 0, 0, 40, 12)
name = gt.entry_new(win, 2, 3, 20)          # lives inside the window
```

Widgets are owned by the C library and live for the process; they are never
freed by Python. Each exposes the shared preamble `x`, `y`, `width`, `height`,
`has_focus`, `is_visible`, `id`, plus `show()`, `hide()` and `as_widget()`.

### Focus

Keys go to focused widgets. `widget.set_focus()` (windows/custom) or assigning
`widget.has_focus = True` moves it; `window.focus_next_child()` cycles.

### Callbacks

- **Key callbacks** — `widget.on_key(cb)`, `cb(key: int) -> int`. Return non-zero
  to consume the key; returning `None` means not consumed.
- **Value callbacks** — `on_toggle(cb)` / `on_change(cb)` on `Switch`, `Expander`,
  `Scale`, `SpinButton`. Receive the new value.
- **Menu actions** take no arguments.
- **Mouse** — `Custom.on_mouse(cb)`, `cb(event, x, y, button)`.

Exceptions inside a callback are reported through `sys.unraisablehook` and never
abort the event loop.

### Colours

ANSI colour constants are module attributes: `gt.BLACK`, `gt.RED`, `gt.GREEN`,
`gt.BROWN`, `gt.BLUE`, `gt.MAGENTA`, `gt.CYAN`, `gt.LIGHTGRAY`, `gt.DARKGRAY`,
`gt.LIGHTRED`, `gt.LIGHTGREEN`, `gt.YELLOW`, `gt.LIGHTBLUE`, `gt.LIGHTMAGENTA`,
`gt.LIGHTCYAN`, `gt.WHITE`. `gt.color_name(idx)` gives the display name.

Drawing primitives: `gt.set_color(fg, bg)`, `gt.set_color_rgb(fg12, bg12)` (12-bit
`0xRGB`), `gt.put_str(x, y, s)`, `gt.put_char(x, y, codepoint)`,
`gt.fill_box(x, y, w, h, ch)`, `gt.draw_box(...)`, `gt.draw_thin_box(...)`.

---

## Windows and layout

```python
app = gt.application_new("My app")
win = gt.window_new(None, "Settings", 0, 1, 50, 16)
win2 = gt.window_new_centered(None, "Centered", 30, 8)

gt.frame_new(win, "Network", 2, 2, 44, 6)      # labelled box
gt.separator_new(win, 2, 9, 44)                # horizontal rule
```

### Box layouts

A `Box` positions widgets; it draws nothing itself.

```python
box = gt.vbox_new()                 # or gt.hbox_new() / gt.box_new(gt.BOX_HORIZONTAL)
box.set_spacing(1)
box.set_margin(1)
box.set_align(gt.ALIGN_CENTER)      # ALIGN_START / ALIGN_CENTER / ALIGN_END

box.add(gt.label_new(None, "Name", 0, 0))
box.add_expand(gt.entry_new(None, 0, 0, 20))   # takes leftover space
box.add_spacing(2)
box.add_stretch()

box.apply(0, 0, 40, 12)             # lay out into this rectangle
box.apply_window(win)               # ...or into a window's content area
print(box.preferred_width(), box.preferred_height())
```

### Tabs and expanders

```python
tabs = gt.tabs_new(win, 1, 1, 40, 12)
tabs.set_titles(["General", "Advanced"])
tabs.set_selected(1)
print(tabs.selected())              # 1
tabs.key(gt.KEY_RIGHT)              # feed it keys yourself

exp = gt.expander_new(win, "Details", 2, 14, 30)
exp.add_managed(some_widget)        # shown/hidden with the expander
exp.set_expanded(True)
exp.on_toggle(lambda expanded: print("open" if expanded else "closed"))
```

---

## Buttons and input

```python
btn = gt.button_new(win, "  Apply  ", 2, 2)
btn.on_key(lambda k: gt.main_quit() if k == gt.KEY_RETURN else 0)

chk = gt.checkbox_new(win, "Enable", 2, 4)
chk.set_checked(True); chk.get_checked()

r1 = gt.radiobutton_new(win, "Fast", 1, 2, 6)   # group id 1
r2 = gt.radiobutton_new(win, "Small", 1, 2, 7)
r1.set_active()                  # takes no argument: selects r1, clears the group
r2.get_active()                  # -> False

cb = gt.combobox_new(win, 2, 9, 20)
cb.append("gzip"); cb.append("zstd")
cb.get_selected(); cb.get_selected_index()

sw = gt.switch_new(win, 2, 11)
sw.set_active(True); sw.get_active()
sw.on_toggle(lambda active: print("switch:", active))

sb = gt.spinbutton_new(win, 2, 13, 0, 100, 5)   # min, max, step
sb.set_value(20); sb.get_value()
sb.on_change(lambda v: print("spin:", v))
sb.handle_key(gt.KEY_UP)

sc = gt.scale_new(win, 2, 15, 30, 0.0, 1.0, 0.05)   # a slider
sc.set_value(0.4); sc.get_value()
sc.on_change(lambda v: print("scale:", v))
```

---

## Text

### Label, Entry, TextView

```python
gt.label_new(win, "Read only text", 2, 2)

e = gt.entry_new(win, 2, 4, 24)
e.set_text("hello"); e.get_text()
e.set_secret(True)                 # password field

tv = gt.textview_new(win, 2, 6, 40, 8)
tv.append("a log line")
tv.clear()
```

### TextList — a scrollable, searchable list

```python
tl = gt.textlist_new(win, 2, 2)
tl.set_view_size(10)
for name in ("alpha", "beta", "gamma"):
    tl.append(name)
tl.set_search_enabled(True)
tl.selection_down()
print(tl.selected_text(), tl.is_searching())
tl.on_key(lambda k: 0)
```

### Editor — a multi-line text editor

A Scintilla-shaped API: positions are byte offsets, lines are 0-based.

```python
ed = gt.editor_new(win, 0, 1, 80, 20)
ed.set_text("def f():\n    return 1\n")
ed.set_line_numbers(True)
ed.set_tab_width(4)
ed.set_caret_line_visible(True)

# navigation and selection
ed.goto_line(1); ed.line_end_extend()
print(ed.selected_text(), ed.current_line(), ed.line_count())

# editing with grouped undo
ed.begin_undo_action()
ed.insert_text(0, "# header\n")
ed.end_undo_action()
ed.undo(); ed.redo()
print(ed.can_undo(), ed.get_modify())

# search
hit = ed.find_text(0, "return", 0, len(ed))     # -> (start, end) or None
ed.set_target_range(0, len(ed))
if ed.search_in_target("return") >= 0:
    ed.replace_target("yield")

# syntax configuration
cfg = gt.editor_langcfg_new()
cfg.set_line_comment("#")
cfg.set_block_comment('"""', '"""')
cfg.add_bracket("(", ")")
cfg.set_keywords(["def", "class", "return"])
ed.set_langcfg(cfg)
ed.colourize()

# folding and annotations
ed.fold_by_indentation()
ed.toggle_fold(0)
ed.annotation_set_text(1, "returns an int")
```

Also available: rectangular selection (`set_rectangular_selection`, `rect_copy`,
`rect_yank`, …), word motion (`word_left`, `del_word_right`, …), styling
(`style_set_fore`, `style_set_bold`, `set_bg_rgb12`, …), `set_read_only`,
`set_overtype`, `set_wrap`, `set_view_whitespace`, `brace_match`, and JSON mode
(`set_json_mode`, `fold_json`).

---

## Data views

### Table — a lazy model/view

Rows live behind a *model*: any object with these methods. Only visible rows are
queried, so the row count may be huge.

```python
class FileModel:
    def __init__(self, rows): self.rows = rows
    def row_count(self):      return len(self.rows)
    def col_count(self):      return 2
    def header(self, col):    return ("Name", "Size")[col]
    def cell(self, row, col): return str(self.rows[row][col])
    # optional — Wireshark-style row colouring; None = theme default
    def row_color(self, row):
        return (gt.BLACK, gt.YELLOW) if self.rows[row][1] > 1_000_000 else None

t = gt.table_new(win, 0, 1, 60, 20)
t.set_model(FileModel([("a.txt", 12), ("big.iso", 4_000_000)]))
t.set_column_widths([30, 12])
t.set_title("Files")
t.key(gt.KEY_DOWN)
print(t.current_row(), t.current_col(), t.selected_row())
```

### Tree — a lazy hierarchical view

Nodes are **integers you choose**; the invisible super-root is `None`.

```python
NODES = {1: ("root", [2, 3]), 2: ("child A", []), 3: ("child B", [4]), 4: ("leaf", [])}

class TreeModel:
    def child_count(self, node):  return len(NODES[node][1]) if node else 1
    def child(self, node, i):     return NODES[node][1][i] if node else 1
    def label(self, node):        return NODES[node][0]
    def has_children(self, node): return bool(NODES[node][1]) if node else True
    # optional: paint the row yourself instead of using label()
    # def draw_row(self, node, x, y, width, selected): ...

tr = gt.tree_new(win, 0, 1, 40, 20)
tr.set_model(TreeModel())
tr.select(3)
print(tr.selected_node(), tr.visible_count())
```

### HexView

```python
hv = gt.hexview_new(win, 0, 1, 70, 20)
data = open("file.bin", "rb").read()
hv.set_data(data)            # keep `data` alive: the widget does not copy it
hv.set_highlight(16, 8)
hv.set_title("file.bin")
hv.key(gt.KEY_DOWN)
print(hv.cursor())
```

---

## Indicators

```python
pb = gt.progressbar_new(win, 2, 2, 30); pb.set_value(0.5)

g = gt.gauge_new(win, 2, 4, 30)
g.set_percent(80)                        # or set_value(0.8)
g.set_label("CPU")                       # None shows the percentage
g.set_colors(gt.GREEN, gt.DARKGRAY)

sp = gt.spinner_new(win, 2, 6)
sp.set_spinning(True); sp.step()         # advance one frame

sd = gt.segdisplay_new(win, 2, 8, 30, 6) # seven-segment display
sd.set_text("42.7"); sd.set_colour(gt.LIGHTRED); sd.set_box(True)

sl = gt.sparkline_new(win, 2, 15, 30, 4)
sl.set_data([1.0, 4.0, 2.0, 8.0])
sl.push(5.0)                             # rolling append
sl.set_style(gt.SPARKLINE_AREA)          # or SPARKLINE_BLOCKS / set_style_auto()

bar = gt.statusbar_new(" ready ")
bar.set_text(" saved ")
```

---

## Charts and maps

```python
bc = gt.barchart_new(win, 0, 1, 40, 12)
bc.set_data([3.0, 7.0, 5.0], ["red", "green", "blue"])
bc.set_show_values(True); bc.set_bar_width(3, 1); bc.set_max(0)   # 0 = auto

pc = gt.piechart_new(win, 0, 1, 40, 14)
pc.set_data([30.0, 50.0, 20.0], ["a", "b", "c"])
pc.set_colors([gt.RED, gt.GREEN, gt.BLUE])
pc.set_donut(True); pc.set_show_legend(True)
gt.piechart_palette(0)                      # default palette entry

lc = gt.linechart_new(win, 0, 1, 60, 16)
lc.add_series([1.0, 3.0, 2.0, 5.0], gt.LIGHTGREEN)
lc.set_range(0, 6); lc.set_xspan(10, "s"); lc.set_log_y(False)

sp = gt.scatter_new(win, 0, 1, 60, 16)
sp.add_point(1.0, 2.0, gt.CYAN)
sp.set_data([1.0, 2.0, 3.0], [2.0, 4.0, 8.0], gt.YELLOW)
sp.set_autoscale(True)                      # or set_bounds(xmin, xmax, ymin, ymax)
```

### Map

```python
m = gt.map_new(win, 0, 1, 80, 24)
m.add_world(gt.BLUE)
m.set_graticule(True)
m.add_city("Paris", colour=gt.LIGHTRED)     # built-in city table
m.add_point(35.68, 139.69, label="Tokyo", colour=gt.YELLOW)
m.add_polyline([2.35, 48.85, 139.69, 35.68], gt.GREEN)   # flat lon,lat,lon,lat
print(m.project(2.35, 48.85))               # -> (x, y) or None if off-map
print(gt.map_find_city("Tokyo"))            # ('Tokyo', 35.68, 139.69)
print(len(gt.map_cities()))
```

### MindMap

```python
mm = gt.mindmap_new(win, 0, 1, 60, 20)
mm.clear("Project")
root = mm.root()
a = mm.add_child(root, "design")
mm.add_sibling(a, "build")
a.set_text("design docs")
a.toggle_fold()
mm.select(a)
print(mm.selected())
```

---

## Menus

The library binds **F10** to toggle menu focus and gives the menu exclusive key
dispatch while it is open; it closes a dropdown *before* running the action, so
an action may safely open a modal dialog.

```python
menu = gt.menu_new()

f = menu.add_entry("File")
menu.add_item(f, "Save", "Ctrl-S", lambda: save())
menu.add_item(f, "Save As...", "", lambda: save_as())
menu.add_separator(f)
menu.add_item(f, "Quit", "Ctrl-X", gt.main_quit)

h = menu.add_entry("Help")
menu.add_item(h, "About", "", lambda: gt.dialog_message("About", "v1.0"))

menu.set_focus(True)             # open it from your own key handler
menu.is_focused()
menu.set_item_enabled(f, 0, False)   # grey out File▸Save
```

Limits: 8 top-level entries, 20 items each, 31-character labels, 11-character
shortcut strings.

---

## Dialogs and choosers

The blocking helpers run their own event loop and return the answer, so they can
be called straight from a key handler or a menu action.

```python
if gt.dialog_confirm("Unsaved changes", "Quit anyway?"):
    gt.main_quit()

gt.dialog_message("Done", "Export finished.")

idx = gt.dialog_run("Conflict", "The file changed on disk.",
                    ["Reload", "Overwrite", "Cancel"])   # index, or -1 if ESC
```

For a non-blocking dialog driven by your own loop, use the widget form:
`gt.dialog_new(...)`, `.set(title, message, buttons)`, `.draw()`, `.key(k)`,
`.result()` (which is `gt.DIALOG_ONGOING` until a button is chosen).

### File chooser

```python
path = gt.filechooser_run(".", save_mode=False)      # -> path or None
path = gt.filechooser_run_named(".", "untitled.txt") # save dialog, name pre-filled

path, flags = gt.filechooser_run_opts(".", True, [("compress", True), ("backup", False)])
```

Or embed it: `gt.filechooser_new(parent, x, y, w, h)`, `.set_dir()`, `.draw()`, `.key()`.

### Colour picker and calendar

```python
colour = gt.colordialog_run("Pick a colour", gt.RED)   # ANSI index, or -1

cal = gt.calendar_new(win, 0, 1, 24, 10)
cal.set_date(2026, 8, 3)
cal.set_marker(lambda y, m, d: d in (1, 15))           # mark those days
print(cal.get_date())
gt.calendar_days_in_month(2026, 2)    # 28
gt.calendar_day_of_week(2026, 8, 3)   # 0=Sunday .. 6=Saturday
```

### Image

```python
img = gt.image_new(win, 0, 1, 40, 20)
if img.load("photo.png"):             # also load_memory(bytes)
    img.draw()
```

---

## Custom widgets

When no stock widget fits, take the canvas. This is how you get per-cell colour.

```python
def draw():
    W, H = gt.canvas_width(), gt.canvas_height()
    gt.set_color(gt.WHITE, gt.BLUE)
    gt.fill_box(0, 0, W, 1, ord(" "))
    gt.put_str(1, 0, "custom widget")
    gt.set_color_rgb(0x9CF, 0x000)       # 12-bit RGB
    gt.put_str(1, 2, "coloured text")

def key(k):
    if k == 24:                          # Ctrl-X
        gt.main_quit()
    return 1

def mouse(event, x, y, button):
    if event == gt.MOUSE_PRESS:
        print("click", x, y, button)
    return 0

w = gt.custom_new(None, 0, 0, gt.canvas_width(), gt.canvas_height())
w.on_draw(draw)
w.on_key(key)
w.on_mouse(mouse)
w.set_focusable(True)
w.set_focus()
```

Mouse events are `MOUSE_PRESS`, `MOUSE_RELEASE`, `MOUSE_MOTION`, `MOUSE_WHEEL`,
`MOUSE_DOUBLE`; buttons are 1 left, 2 middle, 3 right, 4/5 wheel.
`gt.set_double_click_time(ms)` tunes double-click detection. Motion only arrives
while a button is held, and the press that starts a drag owns the whole gesture.

---

## Utilities

### Rope — a text buffer with cheap edits anywhere

```python
r = gt.rope_from("hello world")
r.insert(5, ",")
r.delete(0, 1)
print(str(r), len(r), r.lines())
print(r.copy(0, 4), r.at(0))
print(r.line_of(3), r.line_start(0))
print(r.find(0, "w"), r.rfind(len(r), "o"))
```

---

## Threading and the GIL

The GIL is released while `gt.main()` blocks and re-acquired inside every
callback, so callbacks can safely touch Python state. The modal dialog helpers
(`dialog_run`, `dialog_confirm`, `filechooser_run`, `colordialog_run`) hold the
GIL for their duration because their internal redraws call back into Python.

---

## API index

**Lifecycle & canvas** — `init`, `main`, `main_quit`, `redraw`,
`present_shutdown`, `canvas_width`, `canvas_height`, `set_double_click_time`

**Drawing** — `set_color`, `set_color_rgb`, `put_str`, `put_char`, `fill_box`,
`draw_box`, `draw_thin_box`, `color_name`

**Containers** — `application_new`, `window_new`, `window_new_centered`,
`frame_new`, `separator_new`, `vbox_new`, `hbox_new`, `box_new`, `tabs_new`,
`expander_new`

**Input** — `button_new`, `entry_new`, `checkbox_new`, `radiobutton_new`,
`combobox_new`, `switch_new`, `spinbutton_new`, `scale_new`

**Text** — `label_new`, `textview_new`, `textlist_new`, `editor_new`,
`editor_langcfg_new`, `editor_grammar_load`, `rope_new`, `rope_from`

**Data views** — `table_new`, `tree_new`, `hexview_new`

**Indicators** — `progressbar_new`, `gauge_new`, `spinner_new`, `segdisplay_new`,
`sparkline_new`, `statusbar_new`

**Charts** — `barchart_new`, `piechart_new`, `piechart_palette`, `linechart_new`,
`scatter_new`, `map_new`, `map_cities`, `map_find_city`, `mindmap_new`

**Menus & dialogs** — `menu_new`, `dialog_new`, `dialog_run`, `dialog_confirm`,
`dialog_message`, `filechooser_new`, `filechooser_run`, `filechooser_run_named`,
`filechooser_run_opts`, `colordialog_new`, `colordialog_run`, `calendar_new`,
`calendar_days_in_month`, `calendar_day_of_week`, `image_new`

**Escape hatch** — `custom_new`

**Constants** — colours (`BLACK` … `WHITE`), keys (`KEY_RETURN`, `KEY_ESCAPE`,
`KEY_TAB`, `KEY_UP`/`DOWN`/`LEFT`/`RIGHT`, `KEY_HOME`, `KEY_END`, `KEY_PAGEUP`,
`KEY_PAGEDOWN`, `KEY_BACKSPACE`, `KEY_DELETE`, `KEY_F1`–`KEY_F10`), mouse
(`MOUSE_PRESS`, `MOUSE_RELEASE`, `MOUSE_MOTION`, `MOUSE_WHEEL`, `MOUSE_DOUBLE`),
layout (`BOX_VERTICAL`, `BOX_HORIZONTAL`, `ALIGN_START`, `ALIGN_CENTER`,
`ALIGN_END`), `SPARKLINE_BLOCKS`, `SPARKLINE_AREA`, `DIALOG_ONGOING`

Every binding lives in [`gtcaca_module.cpp`](gtcaca_module.cpp); `help(gtcaca)`
and `help(gtcaca.Editor)` work at the REPL for per-method signatures.

## Licence

Public domain, like GTCaca itself.
