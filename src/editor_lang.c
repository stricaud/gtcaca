#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <gtcaca/editor.h>
#include <gtcaca/json.h>

#define LANG_MAX_BRACKETS 32
#define LANG_MAX_DELIMS   8
#define LANG_STR          16

struct _gtcaca_editor_langcfg_t {
  char line_comment[LANG_STR];
  char block_open[LANG_STR];
  char block_close[LANG_STR];
  struct { char open[LANG_STR]; char close[LANG_STR]; } brackets[LANG_MAX_BRACKETS];
  int  n_brackets;
  char string_delims[LANG_MAX_DELIMS];   /* single-char quote delimiters */
  int  n_delims;
  char **keywords;
  int  n_keywords;
};

/* ── construction / configuration ──────────────────────────────────────────── */

gtcaca_editor_langcfg_t *gtcaca_editor_langcfg_new(void)
{
  return calloc(1, sizeof(gtcaca_editor_langcfg_t));
}

void gtcaca_editor_langcfg_free(gtcaca_editor_langcfg_t *cfg)
{
  int i;
  if (!cfg) return;
  for (i = 0; i < cfg->n_keywords; i++) free(cfg->keywords[i]);
  free(cfg->keywords);
  free(cfg);
}

static void _copy(char *dst, const char *src) { strncpy(dst, src ? src : "", LANG_STR - 1); dst[LANG_STR - 1] = '\0'; }

void gtcaca_editor_langcfg_set_line_comment(gtcaca_editor_langcfg_t *cfg, const char *prefix)
{
  if (cfg) _copy(cfg->line_comment, prefix);
}

void gtcaca_editor_langcfg_set_block_comment(gtcaca_editor_langcfg_t *cfg, const char *open, const char *close)
{
  if (!cfg) return;
  _copy(cfg->block_open, open);
  _copy(cfg->block_close, close);
}

void gtcaca_editor_langcfg_add_bracket(gtcaca_editor_langcfg_t *cfg, const char *open, const char *close)
{
  if (!cfg || cfg->n_brackets >= LANG_MAX_BRACKETS || !open || !close) return;
  _copy(cfg->brackets[cfg->n_brackets].open, open);
  _copy(cfg->brackets[cfg->n_brackets].close, close);
  cfg->n_brackets++;
}

void gtcaca_editor_langcfg_add_string_delimiter(gtcaca_editor_langcfg_t *cfg, const char *delim)
{
  int i;
  if (!cfg || !delim || delim[0] == '\0' || delim[1] != '\0') return;  /* single char only */
  for (i = 0; i < cfg->n_delims; i++) if (cfg->string_delims[i] == delim[0]) return;  /* dedup */
  if (cfg->n_delims < LANG_MAX_DELIMS) cfg->string_delims[cfg->n_delims++] = delim[0];
}

const char *gtcaca_editor_langcfg_get_line_comment(gtcaca_editor_langcfg_t *cfg) { return cfg ? cfg->line_comment : ""; }
const char *gtcaca_editor_langcfg_get_block_comment_open(gtcaca_editor_langcfg_t *cfg) { return cfg ? cfg->block_open : ""; }
const char *gtcaca_editor_langcfg_get_block_comment_close(gtcaca_editor_langcfg_t *cfg) { return cfg ? cfg->block_close : ""; }

void gtcaca_editor_langcfg_set_keywords(gtcaca_editor_langcfg_t *cfg, const char *const *words, int count)
{
  int i;
  if (!cfg) return;
  for (i = 0; i < cfg->n_keywords; i++) free(cfg->keywords[i]);
  free(cfg->keywords);
  cfg->keywords = NULL;
  cfg->n_keywords = 0;
  if (count <= 0) return;
  cfg->keywords = calloc((size_t)count, sizeof(char *));
  if (!cfg->keywords) return;
  for (i = 0; i < count; i++) cfg->keywords[cfg->n_keywords++] = strdup(words[i]);
}

/* ── JSON loading ──────────────────────────────────────────────────────────── */

/* Collect any single-char quote delimiters appearing in an auto-closing /
   surrounding pairs array (entries are either ["a","b"] or {open,close}). */
static void _collect_quote_delims(gtcaca_editor_langcfg_t *cfg, gtcaca_json_value *arr)
{
  int i, n = gtcaca_json_array_size(arr);
  for (i = 0; i < n; i++) {
    gtcaca_json_value *e = gtcaca_json_array_get(arr, i);
    const char *open = NULL;
    if (e && e->type == GTCACA_JSON_ARRAY)
      open = gtcaca_json_string(gtcaca_json_array_get(e, 0));
    else if (e && e->type == GTCACA_JSON_OBJECT)
      open = gtcaca_json_string(gtcaca_json_object_get(e, "open"));
    if (open && (open[0] == '"' || open[0] == '\'' || open[0] == '`') && open[1] == '\0')
      gtcaca_editor_langcfg_add_string_delimiter(cfg, open);
  }
}

int gtcaca_editor_langcfg_load_json(gtcaca_editor_langcfg_t *cfg, const char *path)
{
  gtcaca_json_value *root, *comments, *brackets, *v;
  int i, n;

  if (!cfg) return -1;
  root = gtcaca_json_parse_file(path);
  if (!root) return -1;

  comments = gtcaca_json_object_get(root, "comments");
  if (comments) {
    const char *lc = gtcaca_json_string(gtcaca_json_object_get(comments, "lineComment"));
    if (lc) gtcaca_editor_langcfg_set_line_comment(cfg, lc);
    v = gtcaca_json_object_get(comments, "blockComment");
    if (gtcaca_json_array_size(v) >= 2)
      gtcaca_editor_langcfg_set_block_comment(cfg,
        gtcaca_json_string(gtcaca_json_array_get(v, 0)),
        gtcaca_json_string(gtcaca_json_array_get(v, 1)));
  }

  brackets = gtcaca_json_object_get(root, "brackets");
  n = gtcaca_json_array_size(brackets);
  for (i = 0; i < n; i++) {
    gtcaca_json_value *pair = gtcaca_json_array_get(brackets, i);
    const char *o = gtcaca_json_string(gtcaca_json_array_get(pair, 0));
    const char *c = gtcaca_json_string(gtcaca_json_array_get(pair, 1));
    if (o && c) gtcaca_editor_langcfg_add_bracket(cfg, o, c);
  }

  _collect_quote_delims(cfg, gtcaca_json_object_get(root, "autoClosingPairs"));
  _collect_quote_delims(cfg, gtcaca_json_object_get(root, "surroundingPairs"));

  gtcaca_json_free(root);
  return 0;
}

/* ── the lexer ─────────────────────────────────────────────────────────────── */

static int _is_ident_start(int c) { return isalpha(c) || c == '_' || c == '$'; }
static int _is_ident(int c)       { return isalnum(c) || c == '_' || c == '$'; }

static int _matches(const char *text, int len, int pos, const char *tok)
{
  int n = (int)strlen(tok);
  if (n == 0 || pos + n > len) return 0;
  return strncmp(text + pos, tok, (size_t)n) == 0;
}

static int _is_bracket_char(gtcaca_editor_langcfg_t *cfg, char c)
{
  int i;
  for (i = 0; i < cfg->n_brackets; i++) {
    if (cfg->brackets[i].open[0]  == c && cfg->brackets[i].open[1]  == '\0') return 1;
    if (cfg->brackets[i].close[0] == c && cfg->brackets[i].close[1] == '\0') return 1;
  }
  return 0;
}

static int _is_delim(gtcaca_editor_langcfg_t *cfg, char c)
{
  int i;
  for (i = 0; i < cfg->n_delims; i++) if (cfg->string_delims[i] == c) return 1;
  return 0;
}

static int _is_keyword(gtcaca_editor_langcfg_t *cfg, const char *text, int start, int len)
{
  int i;
  for (i = 0; i < cfg->n_keywords; i++) {
    int kl = (int)strlen(cfg->keywords[i]);
    if (kl == len && strncmp(cfg->keywords[i], text + start, (size_t)len) == 0) return 1;
  }
  return 0;
}

void _gtcaca_editor_colorize(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_langcfg_t *cfg = w->langcfg;
  const char *t = w->text;
  int len = w->length;
  int i;

  if (w->styles_cap < len) {
    unsigned char *p = realloc(w->styles, (size_t)len > 0 ? (size_t)len : 1);
    if (!p) return;
    w->styles = p;
    w->styles_cap = len > 0 ? len : 1;
  }
  if (!cfg) { memset(w->styles, GTCACA_EDITOR_STYLE_DEFAULT, (size_t)len); return; }

  i = 0;
  while (i < len) {
    char c = t[i];

    /* line comment → to end of line */
    if (cfg->line_comment[0] && _matches(t, len, i, cfg->line_comment)) {
      while (i < len && t[i] != '\n') w->styles[i++] = GTCACA_EDITOR_STYLE_COMMENT;
      continue;
    }
    /* block comment → to closing delimiter (may span lines) */
    if (cfg->block_open[0] && _matches(t, len, i, cfg->block_open)) {
      int cl = (int)strlen(cfg->block_close);
      int k = (int)strlen(cfg->block_open);
      while (k-- > 0 && i < len) w->styles[i++] = GTCACA_EDITOR_STYLE_COMMENT;
      while (i < len) {
        if (_matches(t, len, i, cfg->block_close)) {
          int m = cl;
          while (m-- > 0 && i < len) w->styles[i++] = GTCACA_EDITOR_STYLE_COMMENT;
          break;
        }
        w->styles[i++] = GTCACA_EDITOR_STYLE_COMMENT;
      }
      continue;
    }
    /* string → to matching delimiter, respecting backslash escapes */
    if (_is_delim(cfg, c)) {
      w->styles[i++] = GTCACA_EDITOR_STYLE_STRING;
      while (i < len) {
        if (t[i] == '\\' && i + 1 < len) { w->styles[i] = w->styles[i + 1] = GTCACA_EDITOR_STYLE_STRING; i += 2; continue; }
        w->styles[i] = GTCACA_EDITOR_STYLE_STRING;
        if (t[i] == c) { i++; break; }
        if (t[i] == '\n') { i++; break; }   /* don't run unterminated strings forever */
        i++;
      }
      continue;
    }
    /* number */
    if (isdigit((unsigned char)c)) {
      while (i < len && (isalnum((unsigned char)t[i]) || t[i] == '.')) w->styles[i++] = GTCACA_EDITOR_STYLE_NUMBER;
      continue;
    }
    /* identifier / keyword */
    if (_is_ident_start((unsigned char)c)) {
      int start = i;
      int style;
      while (i < len && _is_ident((unsigned char)t[i])) i++;
      style = _is_keyword(cfg, t, start, i - start) ? GTCACA_EDITOR_STYLE_KEYWORD
                                                    : GTCACA_EDITOR_STYLE_DEFAULT;
      memset(w->styles + start, style, (size_t)(i - start));
      continue;
    }
    /* bracket */
    if (_is_bracket_char(cfg, c)) { w->styles[i++] = GTCACA_EDITOR_STYLE_BRACKET; continue; }
    /* punctuation / operator */
    if (!isspace((unsigned char)c) && isprint((unsigned char)c))
      w->styles[i++] = GTCACA_EDITOR_STYLE_OPERATOR;
    else
      w->styles[i++] = GTCACA_EDITOR_STYLE_DEFAULT;
  }
}
