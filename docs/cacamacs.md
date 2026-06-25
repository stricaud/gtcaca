# cacamacs

A small terminal text editor built on the gtcaca [`editor` widget](editor.md),
driven with Emacs key bindings. It demonstrates the widget's colourization,
folding, annotations, autocompletion, search/replace and the VSCode-style
language extension model.

```sh
cacamacs [file | directory] [language-configuration.json]
```

- A **file** opens in the editor.
- A **directory** opens the file browser (also `C-x d`). The browser is a
  read-only editor showing one entry per line, so it works like the editor:
  arrows/`C-n`/`C-p` move, `C-s` searches the listing, Enter descends into a
  folder (`../` goes up) or opens a file, and `q`/`Esc` closes it.
- An optional explicit `language-configuration.json` forces that config.

## Key bindings

```
Motion   C-f C-b C-n C-p  C-a C-e  C-v   arrows / Home / End / PageUp / PageDown
         M-< / M->  beginning / end of buffer
         (C-n / C-p step through wrapped rows when line wrap is on)
Help     M-x help  (a scrollable key-binding window; q or Esc closes it)
         M-x runs a command by name (currently: help)
Edit     C-d delete-fwd   C-k kill-line  Backspace   Tab indent / complete
Words    M-f / M-b move   M-d / M-DEL kill   M-u / M-l upcase / downcase
Region   C-Space mark     C-w kill   M-w (Esc w) copy   C-y yank   C-g cancel
Rect     C-x SPC rectangle mark, move to the opposite corner, then:
         C-x r t insert a string on each line   C-x r k kill   C-x r y yank
         C-x r d delete   C-x r c blank   (C-w / M-w also act on the box)
Comment  M-; comment / uncomment the current line or region
Lines    C-x C-t transpose lines   C-t transpose chars
Modes    C-x C-q read-only   C-x w show whitespace   Insert overtype
Search   C-s isearch fwd  C-r isearch back   (Enter accept, C-g cancel)
Replace  M-% (Esc %)   (prompts: Search: … then Replace with: …)
Windows  C-x 2 split the focused window below   C-x 3 split it right
         C-x 1 one window   C-x 0 close window   C-x o other   C-x b buffer
         (windows nest: split one pane without disturbing the others)
Macros   C-x ( start recording   C-x ) finish   C-x e replay
         (C-u N C-x e replays N times, e.g. C-u 100 C-x e)
Repeat   C-u N <command> runs the next command N times (bare C-u = 4):
         C-u 40 - draws 40 dashes, C-u 5 C-f moves five chars, …
Meta     Esc is the Meta prefix — press Esc, then a key, for M-<key>
         (Alt+<key> works too on terminals that send it as Esc-prefixed)
Undo     C-/   (also C-x u)
Files    C-x C-f find file (Tab completes; a directory opens the browser)
         C-x C-s save (opens the save dialog if the buffer has no name)
         C-x C-w write to a different file (save dialog)
         C-x C-c quit   C-x d browser
         (save dialog: type the name, Enter saves via the default OK button;
          Tab to the buttons, Cancel or C-g aborts)
View     C-x l line numbers   C-x f folding   C-x t toggle fold   C-x a annotate
         C-x C-l line wrap (on by default; turn off to scroll long lines)
JSON     C-x p pretty-print (.json/.jsonl open in JSON mode automatically)
Complete Tab (after a word)  — Up/Down choose, Enter/Tab accept, Esc cancel
```

## Configuration — `~/.cacamacs/config.json`

Controls indentation, with optional per-extension overrides:

```json
{
  "tabSize": 4,
  "insertSpaces": true,
  "indentSize": 4,
  "languages": {
    ".py": { "tabSize": 4, "insertSpaces": true, "indentSize": 4 },
    ".c":  { "insertSpaces": false, "tabSize": 8 }
  }
}
```

- `tabSize` — display width of a tab.
- `insertSpaces` — Tab inserts spaces (true) or a real tab (false).
- `indentSize` — number of spaces inserted when `insertSpaces` is true.
- `languages` — overrides keyed by file extension (merged over the globals).

A ready-to-copy sample is in
[examples/cacamacs-config.json](../examples/cacamacs-config.json). (Indentation
is a user preference, so it lives here rather than inside an installed
extension.)

## Languages — VSCode-style extensions

cacamacs resolves a file's language the way VSCode does, from extension folders
containing a `package.json` that declares `contributes.languages` (a
`language-configuration.json` → comments/brackets/strings) and optionally
`contributes.grammars` (a `.tmLanguage.json` → full syntax colouring).

It scans these roots, in order, so **installed VSCode extensions work directly**:

```
~/.cacamacs/extensions      (cacamacs' own)
~/.vscode/extensions        (VSCode)
~/.vscode-oss/extensions    (VSCodium)
~/.cursor/extensions        (Cursor)
```

For a file it picks the language whose `extensions` include the file's suffix,
loads its grammar (preferred for colour) and/or language-configuration, and
shows the language id in the modeline (`(c)`, `(python)`, …). If nothing
matches, the modeline says so. Two sample extensions ship in
[examples/cacamacs-extensions/](../examples/cacamacs-extensions/) (C and Python);
copy one into a root to try it:

```sh
mkdir -p ~/.cacamacs/extensions
cp -r examples/cacamacs-extensions/python-0.0.1 ~/.cacamacs/extensions/
./build/apps/ccm somefile.py
```

> Colour fidelity depends on the grammar. cacamacs maps TextMate scopes onto a
> small fixed palette (comment / string / number / keyword / bracket /
> operator), so a real VSCode grammar colours correctly but with fewer distinct
> hues than VSCode. See [editor.md](editor.md) for the grammar engine's limits.

## JSON mode

Files ending in `.json` / `.jsonl` open in JSON mode: JSON-aware colouring and
brace-depth folding (fold margin on). `C-x p` pretty-prints — the selection, the
whole buffer if it is one JSON value, or the current line (handy for a JSONL
record).
