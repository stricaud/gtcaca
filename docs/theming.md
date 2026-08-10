# Theming

Every gtcaca widget draws from one palette, `gmo.theme` — twenty foreground /
background pairs, one per widget kind (`window`, `button`, `statusbar`,
`textview`, …, each with a `…focus` variant for when the widget holds focus).

`gtcaca_init()` fills it in by calling `gtcaca_theme_parse_ini(NULL)`, which
starts from the built-in palette and then applies every theme file it finds.

## Theme files

An INI file under a `[gtcaca_theme]` section, one `<widget>_bg` / `<widget>_fg`
key per colour:

```ini
[gtcaca_theme]
window_bg    = blue
window_fg    = white
statusbar_bg = cyan
statusbar_fg = black
```

Colour names: `black blue green cyan red magenta brown lightgray darkgray
lightblue lightgreen lightcyan lightred lightmagenta yellow white`, plus
`default` and `transparent`.

Widget names are the field names of `gtcaca_theme_t`: `default`, `window`,
`windowfocus`, `text`, `textfocus`, `button`, `buttonfocus`, `entry`,
`entryfocus`, `checkbox`, `checkboxfocus`, `progressbar`, `statusbar`,
`textview`, `textviewfocus`, `combobox`, `comboboxfocus`, `menu`, `menuitem`,
`menuitemfocus`.

## Files stack

Files are applied lowest priority first, each overriding only the keys it
mentions. A theme that changes one colour is one line long; everything else
keeps the value underneath it.

| | Location | For |
|---|---|---|
| 1 | `GTCACA_DATA_DIR/themes/<name>` | the prefix the library was built for |
| 2 | `<exe dir>/../share/gtcaca/themes/<name>` | what a relocatable bundle shipped |
| 3 | dirs given to `gtcaca_theme_add_search_dir()` | the application's own defaults |
| 4 | `$XDG_CONFIG_HOME/gtcaca/themes/<name>`, else `~/.config/gtcaca/themes/<name>` (`%APPDATA%\gtcaca\...` on Windows) | the person running it |
| 5 | `$GTCACA_THEME_DIR/themes/<name>` | an explicit directory |
| 6 | `$GTCACA_THEME` | one explicit file, whatever it is called |

`<name>` is `default` unless the application passes something else.

Nothing found at any level is a normal outcome, not an error: the built-in
palette stands and gtcaca says nothing about it. An application that wants to
know calls `gtcaca_theme_set_verbose(1)`, which reports every location tried.

## Shipping a theme with your application

Register the directory before `gtcaca_init()` — that is where themes are read:

```c
#ifdef GTCACA_HAVE_THEME_SEARCH
  gtcaca_theme_add_search_dir(my_data_dir());   /* <dir>/themes/default */
#endif
  gtcaca_init(&argc, &argv);
```

Your file lands at level 3, so it overrides gtcaca's own default while the
user's config still overrides yours. `GTCACA_HAVE_THEME_SEARCH` lets the same
source build against an older gtcaca.

To find your data directory in a bundle that can be unpacked anywhere, use
`gtcaca_executable_dir()` rather than a prefix fixed at compile time.

`gtcaca_theme_loaded_path()` returns the last file applied, or `NULL` when the
built-in palette is in use — useful in a `--version`-style diagnostic.
