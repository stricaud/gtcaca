#ifndef _GTCACA_EDITOR_H_
#define _GTCACA_EDITOR_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/*
 * editor — a multi-line text-editing widget. Its design follows the same
 * document model used by mature editing components, so the API reads in a
 * familiar way:
 *
 *   - The document is a flat byte buffer; positions are byte offsets in
 *     [0 .. length].  (ASCII / byte-oriented; no UTF-8 column handling.)
 *   - The selection is the range between the *anchor* and the *current
 *     position* (the caret). An empty selection means anchor == current.
 *   - Lines are delimited by '\n'; line numbers are 0-based.
 *   - Movement "key commands" come in plain and *_extend variants: the plain
 *     form collapses the selection, the extend form keeps the anchor so the
 *     selection grows (think unshifted vs. shifted arrow keys).
 *   - A linear undo/redo history records every insertion and deletion.
 *
 * The widget ships sensible default key bindings (arrows, Home/End, PageUp/
 * Down, printable insertion, Backspace, Enter). A container can intercept
 * keys first through the registered key callback (return non-zero to mark a
 * key handled and suppress the default) — that is how apps/cacamacs.c layers
 * Emacs bindings on top.
 */

typedef struct _gtcaca_editor_widget_t gtcaca_editor_widget_t;

/* Return non-zero to mark the key handled (default processing is skipped). */
typedef int  (*gtcaca_editor_key_cb_t)(gtcaca_editor_widget_t *w, int key, void *userdata);
/* Called once after every key the widget processes (use it to refresh UI). */
typedef void (*gtcaca_editor_update_cb_t)(gtcaca_editor_widget_t *w, void *userdata);
/* Called when an autocompletion entry is chosen, with the inserted text. */
typedef void (*gtcaca_editor_autoc_cb_t)(gtcaca_editor_widget_t *w, const char *text, void *userdata);

/* One recorded edit, for undo/redo. */
typedef struct {
  int   is_insert;   /* 1: text was inserted; 0: text was deleted */
  int   pos;
  char *text;        /* copy of the affected text */
  int   len;
  int   group;       /* transaction id (0 = standalone); same id = one undo step */
} gtcaca_editor_action_t;

/*
 * Colourization styles — the "elements" a character can be painted as, in the
 * spirit of Scintilla's styling (each style has a settable fore/back colour,
 * cf. SCI_STYLESETFORE / SCI_STYLESETBACK). The built-in lexer classifies text
 * into these from an attached language configuration.
 */
enum {
  GTCACA_EDITOR_STYLE_DEFAULT = 0,
  GTCACA_EDITOR_STYLE_COMMENT,
  GTCACA_EDITOR_STYLE_STRING,
  GTCACA_EDITOR_STYLE_NUMBER,
  GTCACA_EDITOR_STYLE_KEYWORD,
  GTCACA_EDITOR_STYLE_BRACKET,
  GTCACA_EDITOR_STYLE_OPERATOR,
  GTCACA_EDITOR_STYLE_COUNT
};

typedef struct {
  uint8_t fore;
  uint8_t back;
  int     back_set;   /* 0: inherit the widget's background colour */
  int      fore12_set; /* 1: paint with the 12-bit (4096-colour) foreground below */
  uint16_t fore12;     /* 0x0RGB, 4 bits/channel — used by the truecolour presenter */
  int     bold;
  int     italic;
  int     underline;
  int     visible;    /* 0: text in this style is not drawn (hidden) */
} gtcaca_editor_style_t;

/* Annotation display modes (cf. Scintilla ANNOTATION_*). */
enum {
  GTCACA_EDITOR_ANNOTATION_HIDDEN = 0,
  GTCACA_EDITOR_ANNOTATION_STANDARD,
  GTCACA_EDITOR_ANNOTATION_BOXED
};

/* Fold level encoding (cf. Scintilla SC_FOLDLEVEL*). A level is a small number
   OR'd with flags; the number is the nesting depth. */
#define GTCACA_EDITOR_FOLDLEVELBASE        0x400
#define GTCACA_EDITOR_FOLDLEVELNUMBERMASK  0x0FFF
#define GTCACA_EDITOR_FOLDLEVELHEADERFLAG  0x2000

/* Per-line metadata: folding state and an optional annotation. */
typedef struct {
  int   fold_level;        /* base + flags */
  int   fold_expanded;     /* 1: this header's children are shown */
  int   visible;           /* computed: is the line shown at all? */
  char *annotation;        /* annotation text (may contain '\n'), or NULL */
  int   annotation_style;  /* style id used to draw the annotation */
} gtcaca_editor_line_t;

/*
 * A subset of a VSCode language-configuration.json
 * (https://code.visualstudio.com/api/language-extensions/language-configuration-guide):
 * the comment delimiters, brackets and auto-closing quote pairs that the lexer
 * needs, plus an optional keyword list. Opaque; built with the langcfg_* API.
 */
typedef struct _gtcaca_editor_langcfg_t gtcaca_editor_langcfg_t;

/*
 * A TextMate grammar loaded from a .tmLanguage.json (as shipped by VSCode
 * extensions). Drives full scope-based colourization. Opaque; requires the
 * Oniguruma regex engine at build time (gtcaca_editor_grammar_load returns NULL
 * otherwise).
 */
typedef struct _gtcaca_editor_grammar_t gtcaca_editor_grammar_t;

/*
 * Autocompletion — a pop-up list of candidate words shown next to the caret,
 * modelled on Scintilla's autocompletion (SCI_AUTOC*). The container fills the
 * list (gtcaca_editor_autoc_show); while it is up the widget intercepts keys:
 * typing/Backspace filters it, Up/Down move the selection, Enter/Tab complete,
 * Esc cancels.
 */
typedef struct {
  int    active;
  int    pos_start;        /* where the word being completed starts */
  char **items;            /* full candidate list (as supplied)     */
  int    n_items;
  int   *filtered;         /* indices of items matching the prefix   */
  int    n_filtered;
  int    sel;              /* selected entry, index into `filtered`  */
  int    top;              /* first visible entry (scroll)           */
  char   separator;        /* item separator in the list string      */
  int    ignore_case;
  int    auto_hide;        /* cancel when nothing matches            */
  int    cancel_at_start;  /* Backspacing past the start cancels      */
  int    max_height;       /* max visible rows                       */
  char  *fillups;          /* chars that accept + insert themselves  */
  char  *stops;            /* chars that cancel the list             */
} gtcaca_editor_autoc_t;

struct _gtcaca_editor_widget_t {
  /* Shared widget preamble */
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  uint8_t color_focus_fg;
  uint8_t color_focus_bg;
  uint8_t color_nonfocus_fg;
  uint8_t color_nonfocus_bg;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children;
  struct _gtcaca_editor_widget_t *prev;
  struct _gtcaca_editor_widget_t *next;

  /* Callbacks */
  gtcaca_editor_key_cb_t    private_key_cb;
  gtcaca_editor_key_cb_t    key_cb;
  void                        *key_cb_userdata;
  gtcaca_editor_update_cb_t update_cb;
  void                        *update_cb_userdata;

  /* Document */
  char *text;
  int   length;
  int   cap;

  /* Caret / selection */
  int current_pos;
  int anchor;
  int goal_col;          /* preferred column for line up/down (-1 = recompute) */
  int rect_select;       /* selection is a column box (anchor/caret = corners) */

  /* View */
  int first_visible_line;
  int x_offset;
  int view_lines;        /* rows of text visible (updated on draw) */
  int view_cols;         /* columns of text visible (updated on draw) */
  int tab_width;
  int show_line_numbers;
  int wrap;              /* soft-wrap long lines onto continuation rows */

  /* State */
  int modified;

  /* Undo / redo history */
  gtcaca_editor_action_t *undo;
  int undo_count, undo_cap;
  gtcaca_editor_action_t *redo;
  int redo_count, redo_cap;
  int undo_group_depth;   /* >0 while a begin/end_undo_action transaction is open */
  int undo_group_id;      /* id stamped on edits within the current transaction   */
  int undo_seq;           /* monotonic source of transaction ids                  */

  /* Colourization */
  gtcaca_editor_langcfg_t *langcfg;       /* language config driving the lexer */
  gtcaca_editor_grammar_t *grammar;       /* TextMate grammar (takes priority)  */
  int            colorize_enabled;
  int            json_mode;               /* use the built-in JSON lexer/folder */
  int            colorize_dirty;          /* styles need recomputing */
  unsigned char *styles;                  /* one style id per buffer byte */
  int            styles_cap;
  gtcaca_editor_style_t style_table[GTCACA_EDITOR_STYLE_COUNT];
  int            sel_set;                  /* selection element colour overridden? */
  uint8_t        sel_fore, sel_back;

  /* Autocompletion */
  gtcaca_editor_autoc_t    autoc;
  gtcaca_editor_autoc_cb_t autoc_cb;
  void                    *autoc_cb_userdata;

  /* Search/replace target (cf. Scintilla) */
  int                   target_start;
  int                   target_end;
  int                   search_flags;

  /* Modes & display options */
  int                   read_only;
  int                   overtype;
  int                   view_ws;              /* show whitespace */
  int                   caret_line_visible;
  int                   caret_line_back_set;
  uint8_t               caret_line_back;
  /* 12-bit (4096-colour) surface, used by the truecolour presenter when set */
  int                   bg12_set;  uint16_t bg12;   /* editor background */
  int                   fg12_set;  uint16_t fg12;   /* default foreground */
  int                   edge_column;          /* 0 = off */
  uint8_t               edge_colour;
  /* Brace highlight positions (-1 = none) */
  int                   brace_hl1, brace_hl2, brace_bad;

  /* Margins / folding / annotations */
  int                   show_fold_margin;
  int                   annotation_visible;   /* an ANNOTATION_* mode */
  gtcaca_editor_line_t *lines_meta;
  int                   lines_meta_cap;
  int                   lines_meta_len;
  int                   fold_dirty;           /* line visibility needs recompute */
};

/* ── Construction ──────────────────────────────────────────────────────────── */
gtcaca_editor_widget_t *gtcaca_editor_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_editor_free(gtcaca_editor_widget_t *w);
void gtcaca_editor_draw(gtcaca_editor_widget_t *w);
int  gtcaca_editor_key_cb_register(gtcaca_editor_widget_t *w, gtcaca_editor_key_cb_t cb, void *userdata);
void gtcaca_editor_set_update_cb(gtcaca_editor_widget_t *w, gtcaca_editor_update_cb_t cb, void *userdata);

/* ── Text content ───────────────────────────── */
void gtcaca_editor_set_text(gtcaca_editor_widget_t *w, const char *text);
int  gtcaca_editor_get_text(gtcaca_editor_widget_t *w, char *buf, int len);
int  gtcaca_editor_get_length(gtcaca_editor_widget_t *w);
void gtcaca_editor_append_text(gtcaca_editor_widget_t *w, const char *s, int len);
void gtcaca_editor_insert_text(gtcaca_editor_widget_t *w, int pos, const char *s);
void gtcaca_editor_delete_range(gtcaca_editor_widget_t *w, int pos, int len);
void gtcaca_editor_clear_all(gtcaca_editor_widget_t *w);
char gtcaca_editor_get_char_at(gtcaca_editor_widget_t *w, int pos);
int  gtcaca_editor_get_text_range(gtcaca_editor_widget_t *w, int start, int end, char *buf, int len);

/* ── Caret & selection ──────────────────── */
int  gtcaca_editor_get_current_pos(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_current_pos(gtcaca_editor_widget_t *w, int pos);
int  gtcaca_editor_get_anchor(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_anchor(gtcaca_editor_widget_t *w, int pos);
void gtcaca_editor_set_selection(gtcaca_editor_widget_t *w, int caret, int anchor);
void gtcaca_editor_set_empty_selection(gtcaca_editor_widget_t *w, int pos);
int  gtcaca_editor_get_selection_start(gtcaca_editor_widget_t *w);
int  gtcaca_editor_get_selection_end(gtcaca_editor_widget_t *w);
int  gtcaca_editor_get_selected_text(gtcaca_editor_widget_t *w, char *buf, int len);
void gtcaca_editor_goto_pos(gtcaca_editor_widget_t *w, int pos);
void gtcaca_editor_goto_line(gtcaca_editor_widget_t *w, int line);

/* ── Lines & positions ──────────────────────────── */
int gtcaca_editor_get_line_count(gtcaca_editor_widget_t *w);
int gtcaca_editor_line_from_position(gtcaca_editor_widget_t *w, int pos);
int gtcaca_editor_position_from_line(gtcaca_editor_widget_t *w, int line);
int gtcaca_editor_get_line_end_position(gtcaca_editor_widget_t *w, int line);
int gtcaca_editor_get_column(gtcaca_editor_widget_t *w, int pos);
int gtcaca_editor_get_current_line(gtcaca_editor_widget_t *w);
/* byte position on `line` at visual column `column` (tabs expanded); clamps to
   the line end when the line is shorter than `column` (cf. SCI_FINDCOLUMN). */
int gtcaca_editor_find_column(gtcaca_editor_widget_t *w, int line, int column);

/* document position under screen cell (sx,sy): accounts for the border, the
   line-number/fold margins, vertical scroll, soft wrap and horizontal scroll.
   Clicks outside the text area clamp into it. Used for mouse click-to-position. */
int gtcaca_editor_position_from_point(gtcaca_editor_widget_t *w, int sx, int sy);

/* ── Rectangular (column-box) selection ──────────────────────────────────────
   The box has the caret and anchor as opposite corners. Turn the mode on, move
   the caret (extending) to size the box, then apply one of the ops below. */
void gtcaca_editor_set_rectangular_selection(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_rectangular_selection(gtcaca_editor_widget_t *w);
void gtcaca_editor_rect_string_insert(gtcaca_editor_widget_t *w, const char *s); /* C-x r t */
void gtcaca_editor_rect_kill(gtcaca_editor_widget_t *w);                         /* C-x r k */
void gtcaca_editor_rect_delete(gtcaca_editor_widget_t *w);                       /* C-x r d */
void gtcaca_editor_rect_clear(gtcaca_editor_widget_t *w);                        /* C-x r c */
void gtcaca_editor_rect_copy(gtcaca_editor_widget_t *w);   /* copy box, no delete (M-w / C-x r M-w) */
void gtcaca_editor_rect_yank(gtcaca_editor_widget_t *w);                         /* C-x r y */

/* ── Movement key commands (plain collapses, _extend grows the selection) ──── */
void gtcaca_editor_char_left(gtcaca_editor_widget_t *w);
void gtcaca_editor_char_left_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_char_right(gtcaca_editor_widget_t *w);
void gtcaca_editor_char_right_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_up(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_up_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_down(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_down_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_home(gtcaca_editor_widget_t *w);
void gtcaca_editor_home_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_end(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_end_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_document_start(gtcaca_editor_widget_t *w);
void gtcaca_editor_document_start_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_document_end(gtcaca_editor_widget_t *w);
void gtcaca_editor_document_end_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_page_up(gtcaca_editor_widget_t *w);
void gtcaca_editor_page_up_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_page_down(gtcaca_editor_widget_t *w);
void gtcaca_editor_page_down_extend(gtcaca_editor_widget_t *w);

/* ── Editing key commands ─────────────────────────────────────────────────── */
void gtcaca_editor_add_char(gtcaca_editor_widget_t *w, char c); /* replace sel, insert, advance */
void gtcaca_editor_new_line(gtcaca_editor_widget_t *w);
void gtcaca_editor_delete_back(gtcaca_editor_widget_t *w);
void gtcaca_editor_clear(gtcaca_editor_widget_t *w);  /* delete forward, or the selection */

/* ── Undo / redo ──────────────────────────────────────────────────────────── */
void gtcaca_editor_undo(gtcaca_editor_widget_t *w);
void gtcaca_editor_redo(gtcaca_editor_widget_t *w);
int  gtcaca_editor_can_undo(gtcaca_editor_widget_t *w);
int  gtcaca_editor_can_redo(gtcaca_editor_widget_t *w);
void gtcaca_editor_empty_undo_buffer(gtcaca_editor_widget_t *w);
/* Group a run of edits so one undo/redo reverts them all (cf. SCI_BEGIN/ENDUNDOACTION).
   Calls nest; the outermost begin/end pair defines the transaction. */
void gtcaca_editor_begin_undo_action(gtcaca_editor_widget_t *w);
void gtcaca_editor_end_undo_action(gtcaca_editor_widget_t *w);

/* ── State & display options ──────────────────────────────────────────────── */
void gtcaca_editor_set_save_point(gtcaca_editor_widget_t *w); /* mark as unmodified */
int  gtcaca_editor_get_modify(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_line_numbers(gtcaca_editor_widget_t *w, int on);
void gtcaca_editor_set_tab_width(gtcaca_editor_widget_t *w, int n);
int  gtcaca_editor_get_tab_width(gtcaca_editor_widget_t *w);
/* Soft-wrap: long lines continue on the next screen row instead of scrolling
   horizontally (cf. SCI_SETWRAPMODE). Off by default. */
void gtcaca_editor_set_wrap(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_wrap(gtcaca_editor_widget_t *w);

/* ── Colourization: element/style colours (cf. Scintilla element colours) ──── */
/* Set the foreground/background colour of a syntax style (a caca colour). */
void gtcaca_editor_style_set_fore(gtcaca_editor_widget_t *w, int style, uint8_t color);
void gtcaca_editor_style_set_back(gtcaca_editor_widget_t *w, int style, uint8_t color);
/* Revert a style's background to "inherit the widget background". */
void gtcaca_editor_style_clear_back(gtcaca_editor_widget_t *w, int style);
/* Override the selection element colours (defaults to inverting the text). */
void gtcaca_editor_set_selection_colors(gtcaca_editor_widget_t *w, uint8_t fore, uint8_t back);

/* 12-bit (0x0RGB, 4 bits/channel = 4096 colours) overrides for the truecolour
   presenter. They fall back gracefully on a 16-colour libcaca terminal. */
void gtcaca_editor_set_bg_rgb12(gtcaca_editor_widget_t *w, uint16_t rgb12);
void gtcaca_editor_set_fg_rgb12(gtcaca_editor_widget_t *w, uint16_t rgb12);
void gtcaca_editor_style_set_fore_rgb12(gtcaca_editor_widget_t *w, int style, uint16_t rgb12);
/* Force a re-lex on the next draw (e.g. after changing styles or config). */
void gtcaca_editor_colourize(gtcaca_editor_widget_t *w);

/* ── Style definition (cf. Scintilla StyleDefinition) ─────────────────────── */
void gtcaca_editor_style_set_bold(gtcaca_editor_widget_t *w, int style, int on);
void gtcaca_editor_style_set_italic(gtcaca_editor_widget_t *w, int style, int on);
void gtcaca_editor_style_set_underline(gtcaca_editor_widget_t *w, int style, int on);
void gtcaca_editor_style_set_visible(gtcaca_editor_widget_t *w, int style, int on);
void gtcaca_editor_style_reset_default(gtcaca_editor_widget_t *w);  /* reset DEFAULT */
void gtcaca_editor_style_clear_all(gtcaca_editor_widget_t *w);      /* all styles := DEFAULT */

/* ── Margins ──────────────────────────────────────────────────────────────── */
/* (gtcaca_editor_set_line_numbers is declared below in display options) */
int  gtcaca_editor_get_line_numbers(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_fold_margin(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_fold_margin(gtcaca_editor_widget_t *w);

/* ── Colourization: language configuration ────────────────────────────────── */
gtcaca_editor_langcfg_t *gtcaca_editor_langcfg_new(void);
void gtcaca_editor_langcfg_free(gtcaca_editor_langcfg_t *cfg);
void gtcaca_editor_langcfg_set_line_comment(gtcaca_editor_langcfg_t *cfg, const char *prefix);
void gtcaca_editor_langcfg_set_block_comment(gtcaca_editor_langcfg_t *cfg, const char *open, const char *close);
void gtcaca_editor_langcfg_add_bracket(gtcaca_editor_langcfg_t *cfg, const char *open, const char *close);
void gtcaca_editor_langcfg_add_string_delimiter(gtcaca_editor_langcfg_t *cfg, const char *delim);
void gtcaca_editor_langcfg_set_keywords(gtcaca_editor_langcfg_t *cfg, const char *const *words, int count);
/* Comment delimiters (empty string if unset) — for comment/uncomment commands. */
const char *gtcaca_editor_langcfg_get_line_comment(gtcaca_editor_langcfg_t *cfg);
const char *gtcaca_editor_langcfg_get_block_comment_open(gtcaca_editor_langcfg_t *cfg);
const char *gtcaca_editor_langcfg_get_block_comment_close(gtcaca_editor_langcfg_t *cfg);
/* Load a VSCode language-configuration.json (JSONC tolerated). Returns 0 on success. */
int  gtcaca_editor_langcfg_load_json(gtcaca_editor_langcfg_t *cfg, const char *path);
/* Attach (or detach with NULL) a config; attaching enables colourization. */
void gtcaca_editor_set_langcfg(gtcaca_editor_widget_t *w, gtcaca_editor_langcfg_t *cfg);

/* Internal: (re)compute the per-byte style array. Called by draw when dirty. */
void _gtcaca_editor_colorize(gtcaca_editor_widget_t *w);

/* ── Folding (cf. Scintilla Folding) ──────────────────────────────────────── */
void gtcaca_editor_set_fold_level(gtcaca_editor_widget_t *w, int line, int level);
int  gtcaca_editor_get_fold_level(gtcaca_editor_widget_t *w, int line);
void gtcaca_editor_set_fold_expanded(gtcaca_editor_widget_t *w, int line, int expanded);
int  gtcaca_editor_get_fold_expanded(gtcaca_editor_widget_t *w, int line);
void gtcaca_editor_toggle_fold(gtcaca_editor_widget_t *w, int line);
int  gtcaca_editor_get_line_visible(gtcaca_editor_widget_t *w, int line);
/* Convenience folder: assign fold levels from leading-whitespace indentation. */
void gtcaca_editor_fold_by_indentation(gtcaca_editor_widget_t *w);
/* Expand (expanded=1) or collapse (0) every fold header. */
void gtcaca_editor_fold_all(gtcaca_editor_widget_t *w, int expanded);

/* ── Annotations (cf. Scintilla Annotations) ──────────────────────────────── */
void gtcaca_editor_annotation_set_text(gtcaca_editor_widget_t *w, int line, const char *text);
int  gtcaca_editor_annotation_get_text(gtcaca_editor_widget_t *w, int line, char *buf, int len);
void gtcaca_editor_annotation_set_style(gtcaca_editor_widget_t *w, int line, int style);
int  gtcaca_editor_annotation_get_style(gtcaca_editor_widget_t *w, int line);
void gtcaca_editor_annotation_clear_all(gtcaca_editor_widget_t *w);
void gtcaca_editor_annotation_set_visible(gtcaca_editor_widget_t *w, int mode);
int  gtcaca_editor_annotation_get_visible(gtcaca_editor_widget_t *w);
int  gtcaca_editor_annotation_get_lines(gtcaca_editor_widget_t *w, int line);

/* Internal: ensure per-line metadata is sized and visibility is current. */
void _gtcaca_editor_ensure_meta(gtcaca_editor_widget_t *w);

/* ── JSON mode ────────────────────────────────────────────────────────────── */
/* Colour and fold the buffer as JSON (keys, strings, numbers, punctuation;
   structural folding by brace/bracket depth). */
void gtcaca_editor_set_json_mode(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_json_mode(gtcaca_editor_widget_t *w);
void gtcaca_editor_fold_json(gtcaca_editor_widget_t *w);   /* derive fold levels */
/* Internal: the JSON colouriser, called by draw when json_mode is on. */
void _gtcaca_editor_colorize_json(gtcaca_editor_widget_t *w);

/* ── Searching & replacing (cf. Scintilla Searching) ──────────────────────── */
/* Search flag bits (same values as Scintilla SCFIND_*). */
#define GTCACA_EDITOR_FIND_MATCHCASE  0x00000004
#define GTCACA_EDITOR_FIND_WHOLEWORD  0x00000002
#define GTCACA_EDITOR_FIND_WORDSTART  0x00100000
#define GTCACA_EDITOR_FIND_REGEXP     0x00200000   /* (Oniguruma if built with it) */

void gtcaca_editor_set_search_flags(gtcaca_editor_widget_t *w, int flags);
int  gtcaca_editor_get_search_flags(gtcaca_editor_widget_t *w);

/* The target is the range search/replace operate on. */
void gtcaca_editor_set_target_start(gtcaca_editor_widget_t *w, int pos);
int  gtcaca_editor_get_target_start(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_target_end(gtcaca_editor_widget_t *w, int pos);
int  gtcaca_editor_get_target_end(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_target_range(gtcaca_editor_widget_t *w, int start, int end);
void gtcaca_editor_target_whole_document(gtcaca_editor_widget_t *w);
void gtcaca_editor_target_from_selection(gtcaca_editor_widget_t *w);

/* Search for `text` inside the target using the search flags. On success the
   target is set to the match and its start is returned; -1 on failure. */
int  gtcaca_editor_search_in_target(gtcaca_editor_widget_t *w, const char *text);
/* Replace the target with `text`; the target becomes the replacement. Returns
   the replacement length. */
int  gtcaca_editor_replace_target(gtcaca_editor_widget_t *w, const char *text);

/* Find `text` in [min_pos, max_pos) (search backward when min_pos > max_pos).
   Returns the match start (and *match_end if non-NULL), or -1. */
int  gtcaca_editor_find_text(gtcaca_editor_widget_t *w, int flags, const char *text,
                             int min_pos, int max_pos, int *match_end);

/* Anchored search used for incremental find: set the anchor, then repeatedly
   search forward/backward; a found match is selected. Returns pos or -1. */
void gtcaca_editor_search_anchor(gtcaca_editor_widget_t *w);
int  gtcaca_editor_search_next(gtcaca_editor_widget_t *w, int flags, const char *text);
int  gtcaca_editor_search_prev(gtcaca_editor_widget_t *w, int flags, const char *text);

/* ── Word operations (cf. Scintilla word commands) ────────────────────────── */
int  gtcaca_editor_word_left_position(gtcaca_editor_widget_t *w, int pos);
int  gtcaca_editor_word_right_position(gtcaca_editor_widget_t *w, int pos);
void gtcaca_editor_word_left(gtcaca_editor_widget_t *w);
void gtcaca_editor_word_left_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_word_right(gtcaca_editor_widget_t *w);
void gtcaca_editor_word_right_extend(gtcaca_editor_widget_t *w);
void gtcaca_editor_del_word_left(gtcaca_editor_widget_t *w);
void gtcaca_editor_del_word_right(gtcaca_editor_widget_t *w);

/* ── Brace matching (cf. Scintilla SCI_BRACEMATCH / SCI_BRACEHIGHLIGHT) ────── */
/* Returns the position of the brace matching the one at `pos`, or -1. */
int  gtcaca_editor_brace_match(gtcaca_editor_widget_t *w, int pos);
/* Highlight a matched brace pair (pos2 = -1 to clear); badlight an unmatched. */
void gtcaca_editor_brace_highlight(gtcaca_editor_widget_t *w, int pos1, int pos2);
void gtcaca_editor_brace_badlight(gtcaca_editor_widget_t *w, int pos);

/* ── Line & case commands ─────────────────────────────────────────────────── */
void gtcaca_editor_line_duplicate(gtcaca_editor_widget_t *w);
void gtcaca_editor_line_transpose(gtcaca_editor_widget_t *w);   /* swap with previous line */
void gtcaca_editor_line_delete(gtcaca_editor_widget_t *w);
void gtcaca_editor_selection_duplicate(gtcaca_editor_widget_t *w);
void gtcaca_editor_upper_case(gtcaca_editor_widget_t *w);       /* the selection */
void gtcaca_editor_lower_case(gtcaca_editor_widget_t *w);

/* ── Modes ────────────────────────────────────────────────────────────────── */
void gtcaca_editor_set_read_only(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_read_only(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_overtype(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_overtype(gtcaca_editor_widget_t *w);

/* ── Display options ──────────────────────────────────────────────────────── */
void gtcaca_editor_set_view_whitespace(gtcaca_editor_widget_t *w, int on);
int  gtcaca_editor_get_view_whitespace(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_caret_line_visible(gtcaca_editor_widget_t *w, int on);
void gtcaca_editor_set_caret_line_back(gtcaca_editor_widget_t *w, uint8_t colour);
void gtcaca_editor_set_edge_column(gtcaca_editor_widget_t *w, int column);
int  gtcaca_editor_get_edge_column(gtcaca_editor_widget_t *w);
void gtcaca_editor_set_edge_colour(gtcaca_editor_widget_t *w, uint8_t colour);

/* ── TextMate grammars (.tmLanguage.json) ─────────────────────────────────── */
/* Load a grammar from a .tmLanguage.json. Returns NULL on error, or if the
   library was built without Oniguruma. */
gtcaca_editor_grammar_t *gtcaca_editor_grammar_load(const char *path);
void gtcaca_editor_grammar_free(gtcaca_editor_grammar_t *g);
/* Attach (or detach with NULL) a grammar; attaching enables colourization. */
void gtcaca_editor_set_grammar(gtcaca_editor_widget_t *w, gtcaca_editor_grammar_t *g);
/* Internal: the grammar tokeniser, called by draw when a grammar is attached. */
void _gtcaca_editor_colorize_tm(gtcaca_editor_widget_t *w);

/* ── Autocompletion (cf. Scintilla SCI_AUTOC*) ────────────────────────────── */
/* Show the list. `len_entered` = characters already typed before the caret that
   form the prefix; `item_list` = items joined by the separator (default ' '). */
void gtcaca_editor_autoc_show(gtcaca_editor_widget_t *w, int len_entered, const char *item_list);
void gtcaca_editor_autoc_cancel(gtcaca_editor_widget_t *w);
int  gtcaca_editor_autoc_active(gtcaca_editor_widget_t *w);
int  gtcaca_editor_autoc_pos_start(gtcaca_editor_widget_t *w);
void gtcaca_editor_autoc_complete(gtcaca_editor_widget_t *w);   /* accept current entry */
void gtcaca_editor_autoc_select(gtcaca_editor_widget_t *w, const char *prefix);
int  gtcaca_editor_autoc_get_current(gtcaca_editor_widget_t *w); /* index into the full list, -1 if none */
int  gtcaca_editor_autoc_get_current_text(gtcaca_editor_widget_t *w, char *buf, int len);
void gtcaca_editor_autoc_set_separator(gtcaca_editor_widget_t *w, char sep);
char gtcaca_editor_autoc_get_separator(gtcaca_editor_widget_t *w);
void gtcaca_editor_autoc_set_ignore_case(gtcaca_editor_widget_t *w, int on);
void gtcaca_editor_autoc_set_auto_hide(gtcaca_editor_widget_t *w, int on);
void gtcaca_editor_autoc_set_cancel_at_start(gtcaca_editor_widget_t *w, int on);
void gtcaca_editor_autoc_set_max_height(gtcaca_editor_widget_t *w, int rows);
void gtcaca_editor_autoc_set_fillups(gtcaca_editor_widget_t *w, const char *chars);
void gtcaca_editor_autoc_stops(gtcaca_editor_widget_t *w, const char *chars);
void gtcaca_editor_set_autoc_cb(gtcaca_editor_widget_t *w, gtcaca_editor_autoc_cb_t cb, void *userdata);

/* Internal: handle a key while the list is up (returns 1 if consumed); and the
   pop-up renderer, invoked by gtcaca_editor_draw. */
int  _gtcaca_editor_autoc_key(gtcaca_editor_widget_t *w, int key);
void _gtcaca_editor_draw_autoc(gtcaca_editor_widget_t *w);

#endif /* _GTCACA_EDITOR_H_ */
