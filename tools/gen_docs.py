#!/usr/bin/env python3
"""
gen_docs.py — Generate gtcaca_api.c and gtcaca_api.h from the gtcaca header files.
Run from the project root:
    python3 tools/gen_docs.py
"""

import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
HEADERS_DIR = os.path.join(PROJECT_ROOT, "src", "include", "gtcaca")
DOCS_DIR = os.path.join(PROJECT_ROOT, "docs")

SKIP_FILES = {"utlist.h", "utarray.h", "stb_image.h", "iniparse.h", "log.h"}

# ─── brief generator ────────────────────────────────────────────────────────

def _make_brief(fname):
    """Derive a short English description from a gtcaca function name."""
    # Strip gtcaca_ prefix
    rest = re.sub(r'^gtcaca_', '', fname)
    parts = rest.split('_')

    # special suffixes first
    if len(parts) >= 2 and parts[-1] == 'new':
        widget = ' '.join(parts[:-1])
        return f"Create a new {widget} widget"
    if len(parts) >= 2 and parts[-1] == 'draw':
        widget = ' '.join(parts[:-1])
        return f"Draw the {widget} widget"
    if len(parts) >= 2 and parts[-1] == 'free':
        widget = ' '.join(parts[:-1])
        return f"Free the {widget} widget and its resources"
    if len(parts) >= 2 and parts[-1] == 'init':
        widget = ' '.join(parts[:-1])
        return f"Initialise the {widget} subsystem"
    if len(parts) >= 2 and parts[-1] == 'clear':
        widget = ' '.join(parts[:-1])
        return f"Clear all items from the {widget}"
    if len(parts) >= 3 and parts[-2] == 'cb' and parts[-1] == 'register':
        widget = ' '.join(parts[:-2])
        return f"Register a callback for {widget} events"
    if len(parts) >= 2 and parts[-1] == 'destroy':
        widget = ' '.join(parts[:-1])
        return f"Destroy the {widget} widget"
    if len(parts) >= 3 and parts[-2] == 'set':
        prop = parts[-1]
        widget = ' '.join(parts[:-2])
        return f"Set the {prop} property of {widget}"
    if len(parts) >= 2 and parts[-2] == 'set':
        prop = parts[-1]
        widget = ' '.join(parts[:-2])
        return f"Set the {prop} of {widget}"
    if len(parts) >= 3 and parts[-2] == 'get':
        prop = parts[-1]
        widget = ' '.join(parts[:-2])
        return f"Get the {prop} property of {widget}"
    if len(parts) >= 2 and parts[-2] == 'get':
        prop = parts[-1]
        widget = ' '.join(parts[:-2])
        return f"Get the {prop} of {widget}"
    if len(parts) >= 2 and parts[-1] == 'append':
        widget = ' '.join(parts[:-1])
        return f"Append an item to the {widget}"
    if len(parts) >= 2 and parts[-1] == 'show':
        widget = ' '.join(parts[:-1])
        return f"Show the {widget} widget"
    if len(parts) >= 2 and parts[-1] == 'hide':
        widget = ' '.join(parts[:-1])
        return f"Hide the {widget} widget"
    if len(parts) >= 2 and parts[-1] == 'focus':
        widget = ' '.join(parts[:-1])
        return f"Set focus to the {widget} widget"
    # fallback
    return f"Perform {rest.replace('_', ' ')} operation"


# ─── param description generator ───────────────────────────────────────────

def _param_desc(pname, fname):
    """Generate a short description for a parameter based on common patterns."""
    p = pname.lower()
    if p in ('parent',):
        return "Parent widget"
    if p in ('x',):
        return "X position relative to parent"
    if p in ('y',):
        return "Y position relative to parent"
    if p in ('width',):
        return "Width in characters"
    if p in ('height',):
        return "Height in characters"
    if p in ('label', 'text', 'title'):
        return "Text label to display"
    if p in ('key',):
        return "Key code that was pressed"
    if p in ('userdata',):
        return "User-supplied pointer passed back in callbacks"
    if p in ('cb', 'key_cb', 'callback'):
        return "Callback function pointer"
    if p in ('value',):
        return "New value to set"
    if p in ('item',):
        return "Item string to add"
    if p in ('length',):
        return "Length in characters"
    if p in ('vertical',):
        return "Non-zero for vertical orientation, 0 for horizontal"
    if p in ('min',):
        return "Minimum allowed value"
    if p in ('max',):
        return "Maximum allowed value"
    if p in ('step',):
        return "Step increment"
    if p in ('checked',):
        return "Non-zero to check, 0 to uncheck"
    if p in ('active',):
        return "Non-zero to activate"
    if p in ('spinning',):
        return "Non-zero to start spinning, 0 to stop"
    if p in ('secret',):
        return "Non-zero to hide text (password mode)"
    if p in ('expanded',):
        return "Non-zero to expand, 0 to collapse"
    if p in ('filepath',):
        return "Path to the image file"
    if p in ('argc',):
        return "Pointer to argument count"
    if p in ('argv',):
        return "Pointer to argument vector"
    if p in ('fmt',):
        return "Printf-style format string"
    if p in ('application_title', 'window_title'):
        return "Title string displayed in the header"
    if p in ('group_id',):
        return "Radio button group identifier"
    if p in ('view_size',):
        return "Number of visible rows in the list"
    if p in ('scroll_offset',):
        return "Scroll offset in characters"
    if p in ('theme',):
        return "Theme name or path"
    if p in ('line',):
        return "Text line to append"
    if p in ('widget',):
        return "Target widget"
    if p in ('entry_idx',):
        return "Menu entry index"
    if p in ('shortcut',):
        return "Keyboard shortcut string"
    if p in ('menu',):
        return "Menu widget"
    if p in ('button_label',):
        return "Text displayed on the button"
    # generic fallback
    return f"The {pname} argument"


# ─── description generator ──────────────────────────────────────────────────

def _make_description(fname, module, params):
    """Build a multi-sentence description from the function name."""
    brief = _make_brief(fname)
    rest = re.sub(r'^gtcaca_', '', fname)
    parts = rest.split('_')

    if parts[-1] == 'new':
        widget = ' '.join(parts[:-1])
        param_str = ', '.join(p for p in params if p not in ('parent',))
        if param_str:
            return (
                f"Allocates and initialises a new {widget} widget.\n"
                f"The widget is added to the global widget list and will be drawn\n"
                f"on the next redraw cycle. Provide {param_str} to configure it.\n"
                f"Returns NULL if memory allocation fails."
            )
        return (
            f"Allocates and initialises a new {widget} widget.\n"
            f"The widget is added to the global widget list and will be drawn\n"
            f"on the next redraw cycle. Returns NULL if allocation fails."
        )
    if parts[-1] == 'draw':
        widget = ' '.join(parts[:-1])
        return (
            f"Renders the {widget} widget onto the caca canvas.\n"
            f"Called automatically by the main loop; you normally do not need\n"
            f"to call this directly unless you perform a manual redraw."
        )
    if parts[-1] == 'clear':
        widget = ' '.join(parts[:-1])
        return (
            f"Removes all entries from the {widget} and resets its selection\n"
            f"index to zero. The widget will be blank after the next draw cycle."
        )
    if len(parts) >= 2 and parts[-2] == 'cb' and parts[-1] == 'register':
        widget = ' '.join(parts[:-2])
        return (
            f"Registers a callback function that is invoked whenever a key event\n"
            f"reaches the {widget} widget. The callback receives the widget pointer,\n"
            f"the key code, and the userdata pointer supplied here."
        )
    if len(parts) >= 2 and parts[-2] == 'set':
        prop = parts[-1]
        widget = ' '.join(parts[:-2])
        return f"Sets the {prop} attribute of the {widget} widget to the given value."
    if len(parts) >= 2 and parts[-2] == 'get':
        prop = parts[-1]
        widget = ' '.join(parts[:-2])
        return f"Returns the current {prop} attribute of the {widget} widget."
    if parts[-1] == 'append':
        widget = ' '.join(parts[:-1])
        return f"Appends a new entry to the {widget}. The string is copied internally."
    # fallback
    return f"{brief}."


# ─── returns generator ───────────────────────────────────────────────────────

def _make_returns(ret_type, fname):
    if 'void' in ret_type and '*' not in ret_type:
        return "Nothing."
    if '*' in ret_type and '_widget_t' in ret_type:
        return f"Pointer to the newly allocated widget, or NULL on allocation failure."
    if ret_type.strip() in ('int',):
        return "0 on success, non-zero on error."
    if 'char *' in ret_type or 'const char *' in ret_type:
        return "Pointer to the string, or NULL if not available."
    if ret_type.strip() in ('float', 'double'):
        return "The current value."
    return "Result value."


# ─── C-string escaper ────────────────────────────────────────────────────────

def c_str(s):
    """Escape a Python string for use inside a C string literal."""
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    s = s.replace('\n', '\\n"\n    "')
    return s


# ─── signature formatter ─────────────────────────────────────────────────────

def _clean_sig(sig):
    """Normalise whitespace in a signature."""
    sig = re.sub(r'\s+', ' ', sig.strip())
    return sig


# ─── main scanner ────────────────────────────────────────────────────────────

FUNC_RE = re.compile(
    r'^((?:void|int|char|float|double|unsigned\s+\w*|gtcaca_\w+\s*\*)\s*\*?)'  # return type
    r'\s*(gtcaca_\w+)\s*\('  # function name
    r'([^;{]*)\)',            # params (no body / semicolon)
    re.MULTILINE
)

def scan_headers():
    entries = []
    for fname in sorted(os.listdir(HEADERS_DIR)):
        if not fname.endswith('.h'):
            continue
        if fname in SKIP_FILES:
            continue

        module = fname[:-2]  # strip .h
        path = os.path.join(HEADERS_DIR, fname)
        with open(path) as fh:
            content = fh.read()

        # collapse multi-line declarations onto one line for easier parsing
        # join lines that don't end with ; { } or are continuation lines
        collapsed = re.sub(r'\n\s+', ' ', content)

        for m in FUNC_RE.finditer(collapsed):
            ret_type = m.group(1).strip()
            name = m.group(2).strip()
            params_raw = m.group(3).strip()

            # parse param names
            param_names = []
            param_descs_str = ""
            if params_raw and params_raw != 'void' and params_raw != '':
                raw_params = [p.strip() for p in params_raw.split(',')]
                for rp in raw_params:
                    rp = rp.strip()
                    if not rp or rp == '...':
                        continue
                    # last token is the param name (strip pointer stars)
                    toks = rp.split()
                    pname = toks[-1].lstrip('*').strip() if toks else ''
                    if pname:
                        param_names.append(pname)
                        pdesc = _param_desc(pname, name)
                        param_descs_str += f"{pname}\\t{pdesc}\\n"

            sig = _clean_sig(f"{ret_type} {name}({params_raw})")
            brief = _make_brief(name)
            desc = _make_description(name, module, param_names)
            returns = _make_returns(ret_type, name)

            entries.append({
                'name': name,
                'module': module,
                'brief': brief,
                'signature': sig,
                'description': desc,
                'params': param_descs_str,
                'returns': returns,
            })

    return entries


# ─── writer ──────────────────────────────────────────────────────────────────

HEADER_TEMPLATE = """\
/* gtcaca_api.h — GENERATED by tools/gen_docs.py — do not edit */
#ifndef GTCACA_API_H
#define GTCACA_API_H

typedef struct {
  const char *name;
  const char *module;
  const char *brief;
  const char *signature;
  const char *description;
  const char *params;   /* "param_name\\tdescription\\n" per param */
  const char *returns;
} gtcaca_api_entry_t;

extern const gtcaca_api_entry_t gtcaca_api[];
extern const int gtcaca_api_count;

#endif /* GTCACA_API_H */
"""


def write_header(path):
    with open(path, 'w') as fh:
        fh.write(HEADER_TEMPLATE)
    print(f"Wrote {path}")


def write_source(path, entries):
    lines = []
    lines.append('/* gtcaca_api.c — GENERATED by tools/gen_docs.py — do not edit */')
    lines.append('#include "gtcaca_api.h"')
    lines.append('')
    lines.append('const gtcaca_api_entry_t gtcaca_api[] = {')

    for e in entries:
        lines.append('  {')
        lines.append(f'    /* {e["name"]} */')
        lines.append(f'    "{c_str(e["name"])}",')
        lines.append(f'    "{c_str(e["module"])}",')
        lines.append(f'    "{c_str(e["brief"])}",')
        lines.append(f'    "{c_str(e["signature"])}",')
        # description — multiline string
        desc_escaped = c_str(e['description'])
        lines.append(f'    "{desc_escaped}",')
        # params
        params_escaped = c_str(e['params']) if e['params'] else ''
        lines.append(f'    "{params_escaped}",')
        # returns
        lines.append(f'    "{c_str(e["returns"])}"')
        lines.append('  },')

    lines.append('};')
    lines.append('')
    lines.append('const int gtcaca_api_count = (int)(sizeof(gtcaca_api) / sizeof(gtcaca_api[0]));')
    lines.append('')

    with open(path, 'w') as fh:
        fh.write('\n'.join(lines))
    print(f"Wrote {path}  ({len(entries)} entries)")


def main():
    os.makedirs(DOCS_DIR, exist_ok=True)
    entries = scan_headers()
    if not entries:
        print("No functions found — check HEADERS_DIR path.", file=sys.stderr)
        sys.exit(1)
    write_header(os.path.join(DOCS_DIR, "gtcaca_api.h"))
    write_source(os.path.join(DOCS_DIR, "gtcaca_api.c"), entries)


if __name__ == '__main__':
    main()
