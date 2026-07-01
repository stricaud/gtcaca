"""GTCaca — Python bindings for the libcaca-based terminal UI toolkit.

The compiled extension ``_gtcaca`` exposes the C API with a thin, Pythonic
surface: factory functions (``window_new``, ``button_new``, …) return widget
objects whose methods mirror the C ``gtcaca_<widget>_<verb>`` calls.

Typical use::

    import gtcaca as gt

    gt.init()
    app = gt.application_new("Hello")
    win = gt.window_new(None, "Demo", 0, 1, 40, 10)
    gt.label_new(win, "Press q to quit", 2, 2)
    btn = gt.button_new(win, "  OK  ", 2, 4)

    def on_key(key):
        if key == gt.KEY_RETURN:
            gt.main_quit()
        return 0
    btn.on_key(on_key)

    win.set_focus()
    gt.main()

Callbacks are plain Python callables. Key callbacks receive the integer key and
may return an int (0 = not consumed, the default). Menu actions take no
arguments. Exceptions raised inside a callback are reported via
``sys.unraisablehook`` and do not abort the event loop.
"""

from ._gtcaca import *  # noqa: F401,F403
from . import _gtcaca as _ext

__all__ = [name for name in dir(_ext) if not name.startswith("_")]
