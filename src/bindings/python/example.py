#!/usr/bin/env python3
"""A small GTCaca demo in Python — a two-field form with a menu and status bar.

Run after building/installing the bindings:

    python example.py

Controls:
    Tab        move focus to the next widget
    F10        open/close the menu bar
    Enter      activate the focused button
    q / Esc    quit
"""

import gtcaca as gt


def main():
    gt.init()
    gt.application_new("GTCaca Python Demo")

    cw, ch = gt.canvas_width(), gt.canvas_height()

    # --- menu bar -----------------------------------------------------------
    menu = gt.menu_new()
    file_m = menu.add_entry("File")
    menu.add_item(file_m, "Quit", "q", gt.main_quit)
    help_m = menu.add_entry("Help")
    status = None  # forward ref for the closure below

    # --- window -------------------------------------------------------------
    win = gt.window_new(None, "Settings", 0, 1, cw, ch - 2)

    gt.label_new(win, "Name:", 1, 2)
    name = gt.entry_new(win, 8, 2, cw - 12)
    name.set_text("jdoe")

    gt.label_new(win, "Pass:", 1, 4)
    pw = gt.entry_new(win, 8, 4, cw - 12)
    pw.set_secret(True)

    notify = gt.checkbox_new(win, "Email notifications", 1, 6)
    notify.set_checked(True)

    progress = gt.progressbar_new(win, 1, 8, cw - 4)
    progress.set_value(0.42)

    status = gt.statusbar_new("Ready | Tab: next  F10: menu  q: quit")

    def help_about():
        status.set_text("GTCaca Python bindings — pybind11 demo")
        gt.redraw()

    menu.add_item(help_m, "About", "", help_about)

    def on_ok(key):
        if key == gt.KEY_RETURN:
            status.set_text("Hello, %s!" % name.get_text())
            gt.redraw()
        return 0

    ok = gt.button_new(win, "  OK  ", 1, 10)
    ok.on_key(on_ok)

    win.set_focused_child(name)
    win.set_focus()

    gt.main()


if __name__ == "__main__":
    main()
