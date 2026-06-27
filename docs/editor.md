# The `editor` widget

`editor` is gtcaca's multi-line text-editing widget. It provides a document
model, caret and selection, undo/redo, scrolling, default key bindings and
optional syntax colourization. [apps/cacamacs/](../apps/cacamacs/) is a full
example: an editor with Emacs key bindings and VSCode-style language support.

- Header: [src/include/gtcaca/editor.h](../src/include/gtcaca/editor.h)
- Sources: [src/editor.c](../src/editor.c), [src/editor_lang.c](../src/editor_lang.c)

## 1. Creating and embedding one

```c
#include <gtcaca/editor.h>

gtcaca_editor_widget_t *ed = gtcaca_editor_new(GTCACA_WIDGET(win), x, y, w, h);
gtcaca_editor_set_line_numbers(ed, 1);
gtcaca_editor_set_tab_width(ed, 4);
```

The constructor takes a width/height like the other widgets, but you can also
let a [box layout](layout.md) size it — pass `0,0,1,1` and add it with
`gtcaca_box_add_expand`:

```c
gtcaca_box_t *box = gtcaca_vbox_new();
gtcaca_box_set_margin(box, 0);
gtcaca_box_add_expand(box, GTCACA_WIDGET(ed));
gtcaca_box_apply_window(box, win);
gtcaca_box_free(box);

gtcaca_window_set_focused_child(win, GTCACA_WIDGET(ed));
```

The widget is a focusable child of a window, drawn and dispatched by the main
loop like any other widget. When it is focused, the toolkit lets Tab, arrows,
Ctrl+N/P, `q` and Esc reach the editor instead of acting as global shortcuts.

## 2. Document model

The document is a flat byte buffer. **Positions are byte offsets** in
`[0 .. length]` (ASCII / byte-oriented — no UTF-8 column handling). Lines are
delimited by `'\n'` and numbered from 0.

```c
gtcaca_editor_set_text(ed, "hello\nworld");   /* replaces all, resets undo */
int len  = gtcaca_editor_get_length(ed);
char buf[256];
gtcaca_editor_get_text(ed, buf, sizeof buf);

gtcaca_editor_insert_text(ed, 5, "!");        /* insert at a position   */
gtcaca_editor_delete_range(ed, 0, 5);         /* delete a [pos,len) span */
gtcaca_editor_append_text(ed, "...", 3);
gtcaca_editor_clear_all(ed);
```

Line/column queries mirror the usual editor-component API:

```c
int n   = gtcaca_editor_get_line_count(ed);
int ln  = gtcaca_editor_line_from_position(ed, pos);
int s   = gtcaca_editor_position_from_line(ed, ln);
int e   = gtcaca_editor_get_line_end_position(ed, ln);
int col = gtcaca_editor_get_column(ed, pos);    /* visual column, tabs expanded */
```

## 3. Caret and selection

The **selection** is the range between the *anchor* and the *current position*
(the caret). An empty selection means `anchor == current`.

```c
gtcaca_editor_goto_pos(ed, 0);                 /* move caret, collapse selection */
gtcaca_editor_goto_line(ed, 10);
gtcaca_editor_set_selection(ed, caret, anchor);
gtcaca_editor_set_empty_selection(ed, pos);

int a = gtcaca_editor_get_selection_start(ed);
int b = gtcaca_editor_get_selection_end(ed);
char sel[256];
gtcaca_editor_get_selected_text(ed, sel, sizeof sel);
```

### Movement key commands

Each movement comes in two forms: the plain form collapses the selection, the
`_extend` form keeps the anchor so the selection grows (think unshifted vs.
shifted arrows). This is how you implement Emacs `C-f` vs. shifted selection.

```
char_left / char_right        line_up / line_down
home / line_end               document_start / document_end
page_up / page_down
```

```c
gtcaca_editor_char_right(ed);          /* move,   collapse selection */
gtcaca_editor_char_right_extend(ed);   /* move,   extend  selection  */
```

### Editing key commands

```c
gtcaca_editor_add_char(ed, 'x');   /* replace selection, insert, advance caret */
gtcaca_editor_new_line(ed);        /* insert '\n'                              */
gtcaca_editor_delete_back(ed);     /* Backspace (or delete selection)          */
gtcaca_editor_clear(ed);           /* delete forward (or delete selection)     */
```

## 4. Undo, modify state, callbacks

A linear undo/redo history records every insertion and deletion.

```c
gtcaca_editor_undo(ed);
gtcaca_editor_redo(ed);
gtcaca_editor_empty_undo_buffer(ed);    /* e.g. right after loading a file */

gtcaca_editor_set_save_point(ed);       /* mark as unmodified (after saving) */
if (gtcaca_editor_get_modify(ed)) { /* buffer has unsaved changes */ }
```

Two callbacks let a container drive the widget:

```c
/* Return non-zero to mark a key handled (suppresses the widget's default). */
static int on_key(gtcaca_editor_widget_t *ed, int key, void *ud) { ... }
gtcaca_editor_key_cb_register(ed, on_key, NULL);

/* Runs once after every key the widget processes — refresh your status line. */
static void on_update(gtcaca_editor_widget_t *ed, void *ud) { ... }
gtcaca_editor_set_update_cb(ed, on_update, NULL);
```

The key callback gets **first refusal**: keys it claims are consumed, everything
else falls through to the widget's built-in editing keys (arrows, Home/End,
PageUp/Down, printable insertion, Backspace, Enter). That split is exactly how
`cacamacs` layers an Emacs keymap on top — see its `on_key`.

## 5. Syntax colourization

Colouring is built from two independent pieces, matching how real editors split
the concerns:

1. **Styles** — the colourable *elements* (default, comment, string, number,
   keyword, bracket, operator), each with a fore/back colour. Same idea as
   Scintilla's element/style colours (`SCI_STYLESETFORE` / `SCI_STYLESETBACK`).
2. **A language configuration** — what text *is* a comment / string / bracket,
   read from a VSCode
   [`language-configuration.json`](https://code.visualstudio.com/api/language-extensions/language-configuration-guide).

A built-in lexer ties them together: given the configuration it classifies each
byte into a style, and the widget paints it with that style's colour. Re-lexing
is automatic on edits.

> A `language-configuration.json` describes comments, brackets and auto-closing
> pairs — **not** a full grammar. So we colour comments, strings (from the
> auto-closing quote pairs), brackets and numbers directly, and take *keywords*
> from a word list you supply (keywords live in TextMate grammars in VSCode,
> which we don't parse).

### Adding a language in code

```c
gtcaca_editor_langcfg_t *cfg = gtcaca_editor_langcfg_new();

/* …either load a VSCode file… */
gtcaca_editor_langcfg_load_json(cfg, "c/language-configuration.json");

/* …or build it by hand: */
gtcaca_editor_langcfg_set_line_comment(cfg, "//");
gtcaca_editor_langcfg_set_block_comment(cfg, "/*", "*/");
gtcaca_editor_langcfg_add_bracket(cfg, "{", "}");
gtcaca_editor_langcfg_add_string_delimiter(cfg, "\"");

const char *kw[] = { "int", "return", "if", "while" };
gtcaca_editor_langcfg_set_keywords(cfg, kw, 4);   /* optional */

gtcaca_editor_set_langcfg(ed, cfg);   /* attach → colourization on */
/* free cfg after the widget is done with it: gtcaca_editor_langcfg_free(cfg) */
```

`gtcaca_editor_langcfg_load_json` reads only the colour-relevant fields:

| Field                                   | Used for                              |
| --------------------------------------- | ------------------------------------- |
| `comments.lineComment`                  | line-comment colouring                |
| `comments.blockComment` `[a, b]`        | block-comment colouring (spans lines) |
| `brackets`                              | bracket colouring                     |
| `autoClosingPairs` / `surroundingPairs` | string delimiters (the quote pairs)   |

The JSON reader tolerates JSONC (`//`, `/* */`, trailing commas) as VSCode
config files use them.

### Customising the colours

```c
gtcaca_editor_style_set_fore(ed, GTCACA_EDITOR_STYLE_COMMENT, CACA_DARKGRAY);
gtcaca_editor_style_set_fore(ed, GTCACA_EDITOR_STYLE_STRING,  CACA_LIGHTGREEN);
gtcaca_editor_style_set_back(ed, GTCACA_EDITOR_STYLE_KEYWORD, CACA_BLUE);
gtcaca_editor_style_clear_back(ed, GTCACA_EDITOR_STYLE_KEYWORD);   /* inherit bg again */
gtcaca_editor_set_selection_colors(ed, CACA_BLACK, CACA_CYAN);     /* selection element */
gtcaca_editor_colourize(ed);   /* force a re-lex after changing styles */
```

Style ids: `GTCACA_EDITOR_STYLE_DEFAULT`, `_COMMENT`, `_STRING`, `_NUMBER`,
`_KEYWORD`, `_BRACKET`, `_OPERATOR`. A style's background defaults to the
widget's background until you set one.

### Loading a language like a VSCode extension

`cacamacs` discovers languages from installed extensions, just like VSCode.
Drop an extension folder under:

```
~/.ccm/extensions/<name>-<version>/
    package.json
    language-configuration.json
```

`package.json` maps file extensions to a configuration file:

```json
{
  "name": "c-lang",
  "version": "0.0.1",
  "contributes": {
    "languages": [
      { "id": "c", "extensions": [".c", ".h"], "configuration": "./language-configuration.json" }
    ]
  }
}
```

Opening a file makes `cacamacs` scan every `~/.ccm/extensions/*/package.json`,
match the file's suffix against each language's `extensions`, load that
language's `configuration` and enable colourization (first match wins; multiple
languages and extensions are supported).

A ready-to-use sample ships in the repo:

```sh
mkdir -p ~/.ccm/extensions
cp -r examples/cacamacs-extensions/c-lang-0.0.1 ~/.ccm/extensions/
./build/apps/ccm src/editor.c          # opens coloured
```

You can also bypass discovery and pass a config explicitly:

```sh
./build/apps/ccm file.c  path/to/language-configuration.json
```

The `JSON` reader used here is also public
([gtcaca/json.h](../src/include/gtcaca/json.h)) if you need to parse config of
your own.

### TextMate grammars (.tmLanguage.json)

For full scope-based colouring, attach a TextMate grammar — the same
`.tmLanguage.json` files VSCode extensions ship. Requires the library to be
built with Oniguruma (the regex engine TextMate uses); `gtcaca_editor_grammar_load`
returns NULL otherwise.

```c
gtcaca_editor_grammar_t *gr = gtcaca_editor_grammar_load("syntaxes/c.tmLanguage.json");
if (gr) gtcaca_editor_set_grammar(ed, gr);     /* takes priority over langcfg */
/* … gtcaca_editor_grammar_free(gr) when done */
```

Supported: `patterns`, `repository`, `include` (`$self` / `#name`), `match` +
`name` + `captures`, and `begin`/`end` + `name`/`contentName` + nested
`patterns` + begin/end captures. TextMate scopes (e.g. `keyword.control`,
`string.quoted`, `comment.line`, `constant.numeric`) are mapped to the editor's
styles. Not supported: external-grammar includes, injections, `while` rules, and
back-references in end patterns.

### JSON mode

A built-in mode (no grammar needed) that colours JSON — property keys, string
values, numbers, `true`/`false`/`null`, punctuation — and folds by brace depth.

```c
gtcaca_editor_set_json_mode(ed, 1);
gtcaca_editor_fold_json(ed);          /* derive fold levels from nesting */
```

Pretty-printing is available via the JSON library:
`gtcaca_json_stringify(gtcaca_json_parse(text), 2)`.

## 6. Autocompletion

A pop-up list of candidate words can be shown next to the caret, modelled on
Scintilla's autocompletion (`SCI_AUTOC*`). The **container decides when to show
it and what goes in it**; while the list is up the widget intercepts keys —
typing/Backspace filters it, Up/Down (or `C-p`/`C-n`) move the selection,
Enter/Tab accept, Esc cancels.

```c
/* Show a list. The second arg is how many characters before the caret are
   already typed (the prefix); items are separated by the separator (default ' '). */
gtcaca_editor_autoc_show(ed, prefix_len, "apple apricot avocado banana");

gtcaca_editor_autoc_active(ed);          /* is the list up? */
gtcaca_editor_autoc_complete(ed);        /* accept the current entry          */
gtcaca_editor_autoc_cancel(ed);          /* hide it                            */
gtcaca_editor_autoc_get_current_text(ed, buf, sizeof buf);
```

Options mirror Scintilla:

```c
gtcaca_editor_autoc_set_separator(ed, ' ');
gtcaca_editor_autoc_set_ignore_case(ed, 1);
gtcaca_editor_autoc_set_auto_hide(ed, 1);          /* close when nothing matches */
gtcaca_editor_autoc_set_cancel_at_start(ed, 1);    /* Backspacing past the start cancels */
gtcaca_editor_autoc_set_max_height(ed, 8);         /* visible rows */
gtcaca_editor_autoc_set_fillups(ed, "(");          /* these chars accept + insert themselves */
gtcaca_editor_autoc_stops(ed, ".");                /* these chars cancel the list */

/* Notified when an entry is chosen (the text is inserted by the widget first). */
gtcaca_editor_set_autoc_cb(ed, on_autoc, NULL);
```

On accept, the widget replaces the typed prefix (`pos_start .. caret`) with the
chosen item. You supply the candidate list however you like — from the document,
a symbol table, language keywords, etc. `cacamacs` binds **Tab** to a
"complete word at point" command that gathers identifiers already in the buffer
plus the language's keywords:

```c
/* sketch of cacamacs' completion */
int start = caret;
while (start > 0 && is_word_char(get_char_at(ed, start - 1))) start--;
/* …collect words in the document + keywords that start with text[start..caret]… */
gtcaca_editor_autoc_show(ed, caret - start, space_separated_sorted_list);
```

## 6b. Searching & replacing

Modelled on Scintilla's search API: a **target** range that search/replace work
on, plus `find_text` and the anchored `search_next`/`search_prev` used for
incremental find. Flags: `MATCHCASE`, `WHOLEWORD`, `WORDSTART`, and `REGEXP`
(Oniguruma, when built with it).

```c
gtcaca_editor_set_search_flags(ed, GTCACA_EDITOR_FIND_MATCHCASE);

/* find within a range (backward if min > max) */
int end, p = gtcaca_editor_find_text(ed, 0, "needle", 0, gtcaca_editor_get_length(ed), &end);

/* target-based replace-all */
gtcaca_editor_set_target_range(ed, 0, gtcaca_editor_get_length(ed));
while (gtcaca_editor_search_in_target(ed, "foo") >= 0) {
  gtcaca_editor_replace_target(ed, "bar");
  gtcaca_editor_set_target_range(ed, gtcaca_editor_get_target_end(ed), gtcaca_editor_get_length(ed));
}

/* incremental search (selects each match) */
gtcaca_editor_search_anchor(ed);
gtcaca_editor_search_next(ed, 0, "needle");   /* or _search_prev */
```

`cacamacs` builds Emacs `C-s`/`C-r` isearch on the anchored calls and `C-x r`
replace-all on the target calls.

## 6c. Editing operations & modes

Word, brace, line, case and mode commands modelled on Scintilla:

```c
/* words */
gtcaca_editor_word_left(ed);  gtcaca_editor_word_right(ed);   /* + _extend */
gtcaca_editor_del_word_left(ed);  gtcaca_editor_del_word_right(ed);
int p = gtcaca_editor_word_right_position(ed, pos);

/* brace matching (skips braces in string/comment styles) */
int m = gtcaca_editor_brace_match(ed, pos);
gtcaca_editor_brace_highlight(ed, pos, m);   /* or _brace_badlight; -1,-1 clears */

/* line commands */
gtcaca_editor_line_duplicate(ed);
gtcaca_editor_line_transpose(ed);    /* swap with previous line */
gtcaca_editor_line_delete(ed);
gtcaca_editor_selection_duplicate(ed);

/* case (the selection) */
gtcaca_editor_upper_case(ed);  gtcaca_editor_lower_case(ed);

/* modes */
gtcaca_editor_set_read_only(ed, 1);   /* blocks user edits */
gtcaca_editor_set_overtype(ed, 1);    /* typing overwrites */

/* display */
gtcaca_editor_set_view_whitespace(ed, 1);            /* show · and » */
gtcaca_editor_set_caret_line_visible(ed, 1);
gtcaca_editor_set_caret_line_back(ed, CACA_BLACK);
gtcaca_editor_set_edge_column(ed, 80);               /* column ruler */
gtcaca_editor_set_edge_colour(ed, CACA_DARKGRAY);
```

In `cacamacs`: `M-f`/`M-b`/`M-d`/`M-DEL` words, `M-u`/`M-l` case, `C-t` /
`C-x C-t` transpose, `C-x C-q` read-only, `Insert` overtype, `C-x w` whitespace;
matching braces glow automatically.

## 7. Style definitions

Each syntax style is a full *style definition* (cf. Scintilla): foreground and
background colour plus bold / italic / underline, and a visibility flag.

```c
gtcaca_editor_style_set_fore(ed, GTCACA_EDITOR_STYLE_KEYWORD, CACA_CYAN);
gtcaca_editor_style_set_back(ed, GTCACA_EDITOR_STYLE_STRING,  CACA_BLUE);
gtcaca_editor_style_set_bold(ed, GTCACA_EDITOR_STYLE_KEYWORD, 1);
gtcaca_editor_style_set_italic(ed, GTCACA_EDITOR_STYLE_COMMENT, 1);
gtcaca_editor_style_set_underline(ed, GTCACA_EDITOR_STYLE_OPERATOR, 1);
gtcaca_editor_style_set_visible(ed, GTCACA_EDITOR_STYLE_COMMENT, 0);  /* hide text in this style */

gtcaca_editor_style_reset_default(ed);   /* restore the DEFAULT style   */
gtcaca_editor_style_clear_all(ed);       /* set every style := DEFAULT   */
```

Bold/italic/underline are rendered via the terminal's text attributes where the
driver supports them.

## 8. Margins

```c
gtcaca_editor_set_line_numbers(ed, 1);   int n = gtcaca_editor_get_line_numbers(ed);
gtcaca_editor_set_fold_margin(ed, 1);    int f = gtcaca_editor_get_fold_margin(ed);
```

The line-number margin shows line numbers; the fold margin shows `+`/`-` markers
on fold headers. `cacamacs` toggles them with `C-x l` and `C-x f`.

## 9. Folding

Folding hides ranges of lines under a *header*. Each line has a **fold level**
(a small nesting number OR'd with `GTCACA_EDITOR_FOLDLEVELHEADERFLAG` for header
lines) and an **expanded** flag — the same model as Scintilla.

```c
/* Assign levels yourself … */
gtcaca_editor_set_fold_level(ed, line, GTCACA_EDITOR_FOLDLEVELBASE + depth);
gtcaca_editor_set_fold_level(ed, hdr, (GTCACA_EDITOR_FOLDLEVELBASE + depth) |
                                       GTCACA_EDITOR_FOLDLEVELHEADERFLAG);
/* …or let the built-in indentation folder do it: */
gtcaca_editor_fold_by_indentation(ed);

gtcaca_editor_toggle_fold(ed, line);          /* collapse/expand a header   */
gtcaca_editor_set_fold_expanded(ed, line, 0);
gtcaca_editor_fold_all(ed, 0);                /* collapse every header      */
int shown = gtcaca_editor_get_line_visible(ed, line);
```

Folded-away lines are skipped by the display and by `line_up`/`line_down`, and
the fold margin shows the markers. Enable the margin (`set_fold_margin`) to see
them. In `cacamacs`: `C-x f` turns folding on, `C-x t` toggles the fold at the
caret.

## 10. Annotations

Annotations are read-only lines shown *beneath* a document line — handy for
inline diagnostics. Each carries text (which may span lines) and a style.

```c
gtcaca_editor_annotation_set_text(ed, line, "error: undefined symbol");
gtcaca_editor_annotation_set_style(ed, line, GTCACA_EDITOR_STYLE_COMMENT);
gtcaca_editor_annotation_get_text(ed, line, buf, sizeof buf);
gtcaca_editor_annotation_get_lines(ed, line);     /* display rows it occupies */

gtcaca_editor_annotation_set_visible(ed, GTCACA_EDITOR_ANNOTATION_STANDARD); /* or _HIDDEN / _BOXED */
gtcaca_editor_annotation_clear_all(ed);
```

`cacamacs` toggles a sample annotation on the current line with `C-x a`.

> Note: annotations and fold levels are attached by line index; structural edits
> that insert/delete lines shift them (re-run `fold_by_indentation`, as cacamacs
> does on each change, to re-derive folds).

## 11. Display behaviour

The view scrolls to keep the caret visible (counting folded-away lines and
annotation rows), expands tabs to the tab width (`gtcaca_editor_set_tab_width`),
and draws the selection, caret, fold markers and annotations.

## See also

- [apps/cacamacs/](../apps/cacamacs/) — Emacs-keybinding editor using all of the above.
- [examples/cacamacs-extensions/](../examples/cacamacs-extensions/) — a sample language extension.
- [layout.md](layout.md) — sizing the editor inside a window with box layouts.
