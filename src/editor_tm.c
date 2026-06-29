/*
 * editor_tm.c — TextMate grammar (.tmLanguage.json) colourization.
 *
 * A practical line-oriented tokenizer modelled on the TextMate / vscode-textmate
 * algorithm: a stack of begin/end contexts, leftmost-match selection among the
 * active context's patterns (and its end pattern), with includes ($self / #repo)
 * and captures. Regexes are compiled with Oniguruma (the same engine VSCode
 * uses), so grammar patterns run as written.
 *
 * Supported: patterns, repository, include ($self, #name), match + name +
 *   captures, begin/end + name/contentName + beginCaptures/endCaptures + nested
 *   patterns, scope→style mapping.
 * Not supported: external-grammar includes, injections, `while` rules, and
 *   back-references in end patterns (\1 …).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gtcaca/editor.h>
#include <gtcaca/main.h>
#include <gtcaca/json.h>

#ifdef GTCACA_HAVE_ONIG
#include <oniguruma.h>

#define TM_MAX_GROUPS 32
#define TM_MAX_STACK  64

enum { RULE_MATCH, RULE_BEGINEND, RULE_INCLUDE };

typedef struct { int group; int style; } tm_cap;

typedef struct {
  int       kind;
  regex_t  *re_match;     /* MATCH match, or BEGINEND begin */
  regex_t  *re_end;       /* BEGINEND end (used when it has no \N back-refs) */
  char     *end_src;      /* BEGINEND end source, for rebuilding \1.. back-refs */
  int       line_bounded; /* end can match a newline ($/\n) -> close it at EOL */
  int       style;        /* mapped from `name`        */
  int       content_style;/* mapped from `contentName`, or -1 */
  tm_cap   *caps;    int ncaps;     /* captures / beginCaptures */
  tm_cap   *endcaps; int nendcaps;  /* endCaptures */
  int      *patterns; int npatterns;/* child rule indices */
  char     *include;      /* INCLUDE target */
} tm_rule;

struct _gtcaca_editor_grammar_t {
  tm_rule *rules; int nrules, cap;
  int     *root;  int nroot;
  char   **repo_names;  int **repo_lists; int *repo_counts; int nrepo;
};

/* ── scope → style mapping ─────────────────────────────────────────────────── */

static int _scope_style(const char *scope)
{
  if (!scope) return -1;
  /* substring so the '#'/quotes (punctuation.definition.comment / .string) take
     the comment / string colour too, not the generic punctuation colour */
  if (strstr(scope, "comment"))                 return GTCACA_EDITOR_STYLE_COMMENT;
  /* `string.unquoted.*` is what shell grammars tag *bare words* (command names,
     arguments) with — it is not a visual string. Colouring it green is what made
     shell scripts "all green", so keep it the default text colour. */
  if (strstr(scope, "string.unquoted"))         return GTCACA_EDITOR_STYLE_DEFAULT;
  if (strstr(scope, "string"))                  return GTCACA_EDITOR_STYLE_STRING;
  if (!strncmp(scope, "constant.numeric", 16))  return GTCACA_EDITOR_STYLE_NUMBER;
  if (!strncmp(scope, "constant.language", 17)) return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "constant", 8))           return GTCACA_EDITOR_STYLE_NUMBER;
  if (!strncmp(scope, "keyword.operator", 16))  return GTCACA_EDITOR_STYLE_OPERATOR;
  if (!strncmp(scope, "keyword", 7))            return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "storage", 7))            return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "support.type", 12))      return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "support.function", 16))  return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "entity.name.function", 20)) return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "entity.name.tag", 15))   return GTCACA_EDITOR_STYLE_KEYWORD;
  if (!strncmp(scope, "punctuation", 11))       return GTCACA_EDITOR_STYLE_OPERATOR;
  return -1;   /* leave as the inherited style */
}

/* ── loading ───────────────────────────────────────────────────────────────── */

static regex_t *_compile(const char *pat)
{
  regex_t *re;
  OnigErrorInfo einfo;
  int rc;
  if (!pat) return NULL;
  rc = onig_new(&re, (const OnigUChar *)pat, (const OnigUChar *)pat + strlen(pat),
                ONIG_OPTION_CAPTURE_GROUP, ONIG_ENCODING_UTF8, ONIG_SYNTAX_ONIGURUMA, &einfo);
  if (rc != ONIG_NORMAL) return NULL;
  return re;
}

/* Build a concrete end regex from an end pattern that uses \1.. back-references,
   substituting each \N with the (regex-escaped) text the begin rule captured in
   group N. Returns NULL if the pattern has no back-refs (use the rule's static
   re_end then) or on failure. `base` is the begin-search base (line start);
   `reg` holds the begin match's groups. Fixes e.g. Python strings whose end is
   `\1` (close with the same quote that opened). */
static regex_t *_build_dynamic_end(const char *src, OnigRegion *reg, const char *base)
{
  char out[2048];
  size_t o = 0;
  const char *p;
  int has = 0;

  if (!src || !reg) return NULL;
  for (p = src; p[0]; p++) if (p[0] == '\\' && p[1] >= '1' && p[1] <= '9') { has = 1; break; }
  if (!has) return NULL;

  for (p = src; *p && o < sizeof out - 8; ) {
    if (p[0] == '\\' && p[1] >= '1' && p[1] <= '9') {
      int gn = p[1] - '0';
      if (gn < reg->num_regs && reg->beg[gn] >= 0) {
        const char *cap = base + reg->beg[gn];
        int caplen = reg->end[gn] - reg->beg[gn], k;
        for (k = 0; k < caplen && o < sizeof out - 4; k++) {
          if (strchr(".^$*+?()[]{}|\\/-", cap[k])) out[o++] = '\\';   /* escape metachars */
          out[o++] = cap[k];
        }
      }
      p += 2;
    } else {
      out[o++] = *p++;
    }
  }
  out[o] = '\0';
  return _compile(out);
}

static int _grammar_add(gtcaca_editor_grammar_t *g, gtcaca_json_value *pat);

static void _parse_caps(gtcaca_json_value *capsobj, tm_cap **out, int *nout)
{
  int i, n;
  *out = NULL; *nout = 0;
  if (!capsobj || capsobj->type != GTCACA_JSON_OBJECT) return;
  n = capsobj->u.object.count;
  *out = calloc((size_t)n, sizeof(tm_cap));
  for (i = 0; i < n; i++) {
    const char *key = capsobj->u.object.keys[i];
    const char *name = gtcaca_json_string(gtcaca_json_object_get(capsobj->u.object.vals[i], "name"));
    int style = _scope_style(name);
    if (style < 0) continue;
    (*out)[*nout].group = atoi(key);
    (*out)[*nout].style = style;
    (*nout)++;
  }
}

static int *_parse_patterns(gtcaca_editor_grammar_t *g, gtcaca_json_value *arr, int *count)
{
  int i, n = gtcaca_json_array_size(arr), m = 0;
  int *list;
  *count = 0;
  if (n == 0) return NULL;
  list = calloc((size_t)n, sizeof(int));
  for (i = 0; i < n; i++) {
    int idx = _grammar_add(g, gtcaca_json_array_get(arr, i));
    if (idx >= 0) list[m++] = idx;
  }
  *count = m;
  return list;
}

/* Add one pattern object, returns its rule index (or -1). */
static int _grammar_add(gtcaca_editor_grammar_t *g, gtcaca_json_value *pat)
{
  tm_rule r;
  int idx;
  const char *inc, *match, *begin, *end, *name, *content;

  if (!pat || pat->type != GTCACA_JSON_OBJECT) return -1;
  memset(&r, 0, sizeof r);
  r.content_style = -1;

  inc   = gtcaca_json_string(gtcaca_json_object_get(pat, "include"));
  match = gtcaca_json_string(gtcaca_json_object_get(pat, "match"));
  begin = gtcaca_json_string(gtcaca_json_object_get(pat, "begin"));
  end   = gtcaca_json_string(gtcaca_json_object_get(pat, "end"));
  name  = gtcaca_json_string(gtcaca_json_object_get(pat, "name"));
  content = gtcaca_json_string(gtcaca_json_object_get(pat, "contentName"));

  if (inc) {
    r.kind = RULE_INCLUDE;
    r.include = strdup(inc);
  } else if (begin && end) {
    r.kind = RULE_BEGINEND;
    r.re_match = _compile(begin);
    r.re_end   = _compile(end);
    r.end_src  = strdup(end);   /* kept so \1.. back-refs can be substituted per match */
    /* a rule whose end anchors on a newline (\n) or line end ($) is meant to be
       line-bounded; we use that to stop a stuck context (e.g. a regex char-class
       our simplified engine can't fully close) from leaking past its line. */
    r.line_bounded = (strstr(end, "\\n") != NULL) || (strchr(end, '$') != NULL);
    r.style = _scope_style(name);
    r.content_style = _scope_style(content);
    _parse_caps(gtcaca_json_object_get(pat, "beginCaptures"), &r.caps, &r.ncaps);
    _parse_caps(gtcaca_json_object_get(pat, "endCaptures"),   &r.endcaps, &r.nendcaps);
    if (!r.ncaps) _parse_caps(gtcaca_json_object_get(pat, "captures"), &r.caps, &r.ncaps);
    r.patterns = _parse_patterns(g, gtcaca_json_object_get(pat, "patterns"), &r.npatterns);
    if (!r.re_match || !r.re_end) { /* keep rule but it just won't match */ }
  } else if (match) {
    r.kind = RULE_MATCH;
    r.re_match = _compile(match);
    r.style = _scope_style(name);
    _parse_caps(gtcaca_json_object_get(pat, "captures"), &r.caps, &r.ncaps);
  } else if (gtcaca_json_object_get(pat, "patterns")) {
    /* a grouping pattern with only nested patterns: model as an include-all */
    r.kind = RULE_BEGINEND;       /* reuse: no begin/end regex, just children */
    r.re_match = NULL; r.re_end = NULL;
    r.patterns = _parse_patterns(g, gtcaca_json_object_get(pat, "patterns"), &r.npatterns);
  } else {
    return -1;
  }

  if (g->nrules == g->cap) {
    int nc = g->cap ? g->cap * 2 : 64;
    tm_rule *nr = realloc(g->rules, (size_t)nc * sizeof(tm_rule));
    if (!nr) return -1;
    g->rules = nr; g->cap = nc;
  }
  idx = g->nrules++;
  g->rules[idx] = r;
  return idx;
}

/* ── XML-plist grammars ──────────────────────────────────────────────────────
 * Many TextMate/VSCode grammars ship as Apple XML property lists (.tmLanguage)
 * rather than JSON (.tmLanguage.json). They carry the same structure, so we
 * parse the plist into the same gtcaca_json_value DOM the loader consumes.
 * Only the elements grammars use are handled: dict, array, string, integer/
 * real, true/false. */
static gtcaca_json_value *pl_new(gtcaca_json_type t)
{
  gtcaca_json_value *v = calloc(1, sizeof *v);
  if (v) v->type = t;
  return v;
}
static const char *pl_skip(const char *p)
{
  for (;;) {
    while (*p && (unsigned char)*p <= ' ') p++;
    if (p[0] == '<' && p[1] == '?') { const char *e = strstr(p, "?>"); p = e ? e + 2 : p + strlen(p); continue; }
    if (p[0] == '<' && p[1] == '!') {
      if (!strncmp(p, "<!--", 4)) { const char *e = strstr(p, "-->"); p = e ? e + 3 : p + strlen(p); }
      else { const char *e = strchr(p, '>'); p = e ? e + 1 : p + strlen(p); }
      continue;
    }
    break;
  }
  return p;
}
static char *pl_text(const char *s, const char *e)   /* decode XML entities */
{
  char *out = malloc((size_t)(e - s) + 1), *o;
  if (!out) return NULL;
  o = out;
  while (s < e) {
    if (*s == '&') {
      if      (!strncmp(s, "&lt;", 4))   { *o++ = '<';  s += 4; }
      else if (!strncmp(s, "&gt;", 4))   { *o++ = '>';  s += 4; }
      else if (!strncmp(s, "&amp;", 5))  { *o++ = '&';  s += 5; }
      else if (!strncmp(s, "&quot;", 6)) { *o++ = '"';  s += 6; }
      else if (!strncmp(s, "&apos;", 6)) { *o++ = '\''; s += 6; }
      else if (s[1] == '#') {
        int base = (s[2] == 'x' || s[2] == 'X') ? 16 : 10;
        const char *q = s + (base == 16 ? 3 : 2);
        long v = strtol(q, (char **)&q, base);
        if (*q == ';') q++;
        if (v < 0x80) *o++ = (char)v;
        else if (v < 0x800) { *o++ = (char)(0xC0 | (v >> 6)); *o++ = (char)(0x80 | (v & 0x3F)); }
        else { *o++ = (char)(0xE0 | (v >> 12)); *o++ = (char)(0x80 | ((v >> 6) & 0x3F)); *o++ = (char)(0x80 | (v & 0x3F)); }
        s = q;
      } else *o++ = *s++;
    } else *o++ = *s++;
  }
  *o = '\0';
  return out;
}
static gtcaca_json_value *pl_value(const char **pp)
{
  const char *p = pl_skip(*pp), *t, *te, *gt, *body;
  size_t nlen;
  if (*p != '<') { *pp = p; return NULL; }
  t = p + 1; te = t;
  while (*te && *te != '>' && *te != ' ' && *te != '/' && *te != '\t' && *te != '\n') te++;
  nlen = (size_t)(te - t);
  gt = strchr(p, '>'); if (!gt) { *pp = p + strlen(p); return NULL; }
  body = gt + 1;
#define TAG(s) (nlen == strlen(s) && !strncmp(t, s, nlen))
  if (TAG("plist")) { const char *q = body; gtcaca_json_value *v = pl_value(&q); *pp = q; return v; }
  if (TAG("dict")) {
    gtcaca_json_value *v = pl_new(GTCACA_JSON_OBJECT);
    const char *q = body;
    for (;;) {
      const char *ks, *ke; char *key, **nk; gtcaca_json_value *val, **nv;
      q = pl_skip(q);
      if (!strncmp(q, "</dict>", 7)) { q += 7; break; }
      if (strncmp(q, "<key>", 5)) break;
      ks = q + 5; ke = strstr(ks, "</key>"); if (!ke) break;
      key = pl_text(ks, ke); q = ke + 6;
      val = pl_value(&q);
      nk = realloc(v->u.object.keys, (size_t)(v->u.object.count + 1) * sizeof(char *));
      nv = realloc(v->u.object.vals, (size_t)(v->u.object.count + 1) * sizeof(void *));
      if (!nk || !nv) { free(key); break; }
      v->u.object.keys = nk; v->u.object.vals = nv;
      v->u.object.keys[v->u.object.count] = key;
      v->u.object.vals[v->u.object.count] = val;
      v->u.object.count++;
    }
    *pp = q; return v;
  }
  if (TAG("array")) {
    gtcaca_json_value *v = pl_new(GTCACA_JSON_ARRAY);
    const char *q = body;
    for (;;) {
      const char *before; gtcaca_json_value *item, **ni;
      q = pl_skip(q);
      if (!strncmp(q, "</array>", 8)) { q += 8; break; }
      if (*q != '<') break;
      before = q; item = pl_value(&q);
      if (q == before) break;
      ni = realloc(v->u.array.items, (size_t)(v->u.array.count + 1) * sizeof(void *));
      if (!ni) break;
      v->u.array.items = ni; v->u.array.items[v->u.array.count++] = item;
    }
    *pp = q; return v;
  }
  if (gt[-1] == '/') {                       /* self-closing */
    gtcaca_json_value *v;
    if (TAG("true"))       { v = pl_new(GTCACA_JSON_BOOL); v->u.boolean = 1; }
    else if (TAG("false")) { v = pl_new(GTCACA_JSON_BOOL); v->u.boolean = 0; }
    else                   { v = pl_new(GTCACA_JSON_STRING); v->u.string = calloc(1, 1); }
    *pp = body; return v;
  }
  if (TAG("string")) {
    const char *e = strstr(body, "</string>"); gtcaca_json_value *v = pl_new(GTCACA_JSON_STRING);
    if (!e) { v->u.string = calloc(1, 1); *pp = body; return v; }
    v->u.string = pl_text(body, e); *pp = e + 9; return v;
  }
  if (TAG("integer") || TAG("real")) {
    gtcaca_json_value *v = pl_new(GTCACA_JSON_NUMBER); const char *e = strchr(body, '<');
    v->u.number = strtod(body, NULL); *pp = e ? e : body; return v;
  }
  if (TAG("true"))  { const char *e = strstr(body, "</true>");  gtcaca_json_value *v = pl_new(GTCACA_JSON_BOOL); v->u.boolean = 1; *pp = e ? e + 7 : body; return v; }
  if (TAG("false")) { const char *e = strstr(body, "</false>"); gtcaca_json_value *v = pl_new(GTCACA_JSON_BOOL); v->u.boolean = 0; *pp = e ? e + 8 : body; return v; }
  *pp = body; return pl_new(GTCACA_JSON_NULL);   /* data/date/unknown: ignore */
#undef TAG
}
static gtcaca_json_value *pl_parse_file(const char *path)
{
  FILE *f = fopen(path, "rb"); long n; char *buf; gtcaca_json_value *root; const char *p;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n < 0) { fclose(f); return NULL; }
  buf = malloc((size_t)n + 1);
  if (!buf) { fclose(f); return NULL; }
  n = (long)fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
  p = buf; root = pl_value(&p);
  free(buf);
  return root;
}
/* read a grammar DOM from JSON or XML-plist, sniffing the first non-blank byte */
static gtcaca_json_value *load_grammar_dom(const char *path)
{
  FILE *f = fopen(path, "rb"); int c;
  if (!f) return NULL;
  do { c = fgetc(f); } while (c == ' ' || c == '\t' || c == '\n' || c == '\r');
  fclose(f);
  if (c == '<') return pl_parse_file(path);
  return gtcaca_json_parse_file(path);
}

gtcaca_editor_grammar_t *gtcaca_editor_grammar_load(const char *path)
{
  gtcaca_json_value *root, *repo;
  gtcaca_editor_grammar_t *g;
  int i;
  static int onig_inited = 0;

  if (!onig_inited) {
    OnigEncoding encs[1]; encs[0] = ONIG_ENCODING_UTF8;
    onig_initialize(encs, 1);
    onig_inited = 1;
  }

  root = load_grammar_dom(path);
  if (!root) return NULL;

  g = calloc(1, sizeof(*g));

  /* repository first, so includes can resolve by name */
  repo = gtcaca_json_object_get(root, "repository");
  if (repo && repo->type == GTCACA_JSON_OBJECT) {
    g->nrepo = repo->u.object.count;
    g->repo_names  = calloc((size_t)g->nrepo, sizeof(char *));
    g->repo_lists  = calloc((size_t)g->nrepo, sizeof(int *));
    g->repo_counts = calloc((size_t)g->nrepo, sizeof(int));
    for (i = 0; i < g->nrepo; i++) {
      gtcaca_json_value *entry = repo->u.object.vals[i];
      g->repo_names[i] = strdup(repo->u.object.keys[i]);
      /* an entry is itself a pattern (may have its own `patterns`) */
      if (gtcaca_json_object_get(entry, "patterns") &&
          !gtcaca_json_object_get(entry, "match") &&
          !gtcaca_json_object_get(entry, "begin")) {
        g->repo_lists[i] = _parse_patterns(g, gtcaca_json_object_get(entry, "patterns"), &g->repo_counts[i]);
      } else {
        int idx = _grammar_add(g, entry);
        if (idx >= 0) { g->repo_lists[i] = malloc(sizeof(int)); g->repo_lists[i][0] = idx; g->repo_counts[i] = 1; }
      }
    }
  }

  g->root = _parse_patterns(g, gtcaca_json_object_get(root, "patterns"), &g->nroot);
  gtcaca_json_free(root);
  return g;
}

void gtcaca_editor_grammar_free(gtcaca_editor_grammar_t *g)
{
  int i;
  if (!g) return;
  for (i = 0; i < g->nrules; i++) {
    if (g->rules[i].re_match) onig_free(g->rules[i].re_match);
    if (g->rules[i].re_end)   onig_free(g->rules[i].re_end);
    free(g->rules[i].end_src);
    free(g->rules[i].caps);
    free(g->rules[i].endcaps);
    free(g->rules[i].patterns);
    free(g->rules[i].include);
  }
  free(g->rules);
  free(g->root);
  for (i = 0; i < g->nrepo; i++) { free(g->repo_names[i]); free(g->repo_lists[i]); }
  free(g->repo_names); free(g->repo_lists); free(g->repo_counts);
  free(g);
}

/* ── tokenizer ─────────────────────────────────────────────────────────────── */

/* Resolve a context's pattern list to concrete matchable rule indices,
   expanding includes ($self / #name). Bounded by `out_cap`. */
static void _expand(gtcaca_editor_grammar_t *g, int *list, int n, int *out, int *nout, int out_cap, int depth)
{
  int i;
  if (depth > 8) return;
  for (i = 0; i < n && *nout < out_cap; i++) {
    tm_rule *r = &g->rules[list[i]];
    if (r->kind == RULE_INCLUDE) {
      if (r->include && r->include[0] == '$') {                 /* $self */
        _expand(g, g->root, g->nroot, out, nout, out_cap, depth + 1);
      } else if (r->include && r->include[0] == '#') {          /* #repository */
        int k;
        for (k = 0; k < g->nrepo; k++)
          if (!strcmp(g->repo_names[k], r->include + 1)) {
            _expand(g, g->repo_lists[k], g->repo_counts[k], out, nout, out_cap, depth + 1);
            break;
          }
      }
    } else if (r->kind == RULE_BEGINEND && !r->re_match && r->patterns) {
      _expand(g, r->patterns, r->npatterns, out, nout, out_cap, depth + 1);  /* grouping */
    } else {
      out[(*nout)++] = list[i];
    }
  }
}

static void _fill(gtcaca_editor_widget_t *w, int a, int b, int style)
{
  int i;
  if (style < 0) return;
  for (i = a; i < b && i < w->length; i++) w->styles[i] = (unsigned char)style;
}

static void _apply_caps(gtcaca_editor_widget_t *w, OnigRegion *region, int base, tm_cap *caps, int ncaps)
{
  int i;
  for (i = 0; i < ncaps; i++) {
    int gnum = caps[i].group;
    if (gnum < region->num_regs && region->beg[gnum] >= 0)
      _fill(w, base + region->beg[gnum], base + region->end[gnum], caps[i].style);
  }
}

void _gtcaca_editor_colorize_tm(gtcaca_editor_widget_t *w)
{
  gtcaca_editor_grammar_t *g = w->grammar;
  const char *t = w->text;
  int len = w->length, line, line_count;
  /* context stack: rule index of the active begin/end (-1 = root) + its style */
  int stack[TM_MAX_STACK]; int sp = 0;
  int style_stack[TM_MAX_STACK];
  regex_t *end_stack[TM_MAX_STACK];   /* per-context end regex when it has back-refs (else NULL) */
  OnigRegion *region = onig_region_new();

  if (!g) return;
  if (w->styles_cap < len) {
    unsigned char *p = realloc(w->styles, (size_t)(len > 0 ? len : 1));
    if (!p) { onig_region_free(region, 1); return; }
    w->styles = p; w->styles_cap = len > 0 ? len : 1;
  }
  memset(w->styles, GTCACA_EDITOR_STYLE_DEFAULT, (size_t)len);

  stack[0] = -1; style_stack[0] = GTCACA_EDITOR_STYLE_DEFAULT; end_stack[0] = NULL;  /* root */

  /* Walk lines incrementally. gtcaca_editor_position_from_line() /
     get_line_end_position() each rescan the whole buffer from the start, so
     calling them per line made colourising O(n²) — seconds on an 800-line file.
     Tracking the line start as we go makes it O(n). */
  line_count = gtcaca_editor_get_line_count(w);
  {
  int ls = 0;
  for (line = 0; line < line_count; line++) {
    int le = ls;
    while (le < len && t[le] != '\n') le++;
    int pos = ls;
    int prev_pos = -1, stall = 0;   /* break any zero-width match loop */

    while (pos <= le) {
      int active_style = style_stack[sp];
      int cand[128], ncand = 0, ci;
      int best_start = le + 1, best_end = -1, best_rule = -2; /* -2 none, -1 end */
      int best_is_end = 0;
      OnigRegion *best_region = NULL;

      /* candidate child patterns of the active context */
      if (stack[sp] == -1)
        _expand(g, g->root, g->nroot, cand, &ncand, 128, 0);
      else
        _expand(g, g->rules[stack[sp]].patterns, g->rules[stack[sp]].npatterns, cand, &ncand, 128, 0);

      /* search each candidate; keep the leftmost match */
      for (ci = 0; ci < ncand; ci++) {
        tm_rule *r = &g->rules[cand[ci]];
        int rc;
        if (!r->re_match) continue;
        rc = onig_search(r->re_match, (const OnigUChar *)(t + ls), (const OnigUChar *)(t + le),
                         (const OnigUChar *)(t + pos), (const OnigUChar *)(t + le), region, ONIG_OPTION_NONE);
        if (rc >= 0) {
          int s = ls + region->beg[0];
          if (s < best_start) {
            best_start = s; best_end = ls + region->end[0]; best_rule = cand[ci]; best_is_end = 0;
            if (!best_region) best_region = onig_region_new();
            onig_region_copy(best_region, region);
          }
        }
      }
      /* the active context's end pattern (lower priority on ties); a per-context
         regex (with \N back-refs filled in) takes precedence over the static one */
      regex_t *endre = (stack[sp] != -1)
                         ? (end_stack[sp] ? end_stack[sp] : g->rules[stack[sp]].re_end) : NULL;
      if (endre) {
        int rc = onig_search(endre, (const OnigUChar *)(t + ls), (const OnigUChar *)(t + le),
                             (const OnigUChar *)(t + pos), (const OnigUChar *)(t + le), region, ONIG_OPTION_NONE);
        if (rc >= 0) {
          int s = ls + region->beg[0];
          if (s < best_start) {
            best_start = s; best_end = ls + region->end[0]; best_is_end = 1; best_rule = stack[sp];
            if (!best_region) best_region = onig_region_new();
            onig_region_copy(best_region, region);
          }
        }
      }

      if (best_rule == -2) { _fill(w, pos, le, active_style); break; }  /* nothing more on this line */

      _fill(w, pos, best_start, active_style);   /* text before the match */

      int pushed = 0;
      if (best_is_end) {
        tm_rule *r = &g->rules[stack[sp]];
        _fill(w, best_start, best_end, r->style >= 0 ? r->style : active_style);
        _apply_caps(w, best_region, ls, r->endcaps, r->nendcaps);
        if (sp > 0) { if (end_stack[sp]) { onig_free(end_stack[sp]); end_stack[sp] = NULL; } sp--; }
      } else {
        tm_rule *r = &g->rules[best_rule];
        _fill(w, best_start, best_end, r->style >= 0 ? r->style : active_style);
        _apply_caps(w, best_region, ls, r->caps, r->ncaps);
        if (r->kind == RULE_BEGINEND && sp + 1 < TM_MAX_STACK) {
          pushed = 1;
          sp++;
          stack[sp] = best_rule;
          style_stack[sp] = r->content_style >= 0 ? r->content_style
                          : (r->style >= 0 ? r->style : active_style);
          /* if this rule's end uses \N back-refs, bind them to what begin captured */
          end_stack[sp] = _build_dynamic_end(r->end_src, best_region, t + ls);
        }
      }

      if (best_region) onig_region_free(best_region, 1);
      /* Advance. A zero-width END (a lookahead like `(?=\))`) must NOT step past
         its anchor char, or an enclosing context that ends on that same char
         (e.g. a function call's `)`) never matches and leaks forever. Likewise a
         zero-width BEGIN that pushed a context (e.g. shell's `(?:\G(?<="))`
         command-name-in-quotes rule) must NOT step on, or it skips the first
         content char and that char keeps the parent's colour (the white first
         letter inside a "string"). Both make progress (the stack changed), so the
         stall guard below still breaks any true loop. A zero-width plain match
         (no push/pop) must advance to avoid stalling. */
      if (best_end > best_start)         pos = best_end;
      else if (best_is_end)              pos = best_end;        /* lookahead end: let the parent match here too */
      else if (pushed && style_stack[sp] == GTCACA_EDITOR_STYLE_STRING)
                                         pos = best_end;        /* zero-width string begin: don't skip the first content char */
      else                               pos = best_start + 1;  /* zero-width plain match: step on */
      /* hard stall guard: if we sit on one position too long, force progress */
      if (pos <= prev_pos) { if (++stall > 4 * TM_MAX_STACK) { pos = prev_pos + 1; stall = 0; } }
      else                 { stall = 0; prev_pos = pos; }
    }
    /* Close line-bounded contexts still open at end of line so a construct our
       engine couldn't fully close can't leak its colour onto every following
       line. A line-bounded context (its end anchors on $/\n) MUST end at EOL —
       and so must everything nested inside it. The old code only popped while the
       *top* was line-bounded, so a stuck non-line-bounded child (e.g. shell's
       command-name word context, or an unterminated quote) on top of a
       line-bounded statement blocked the cleanup and the whole stack bled green.
       Instead, find the OUTERMOST open line-bounded context and pop from there up.
       Genuine multi-line constructs (Python triple-quoted strings, block
       comments) aren't nested under a line-bounded context, so they're kept. */
    {
      int cut = -1, q;
      for (q = 1; q <= sp; q++) if (g->rules[stack[q]].line_bounded) { cut = q; break; }
      if (cut >= 1)
        while (sp >= cut) { if (end_stack[sp]) { onig_free(end_stack[sp]); end_stack[sp] = NULL; } sp--; }
    }
    ls = le + 1;   /* next line starts just past this line's newline */
  }
  }
  while (sp > 0) { if (end_stack[sp]) onig_free(end_stack[sp]); sp--; }   /* free open contexts' end regexes */
  onig_region_free(region, 1);
}

void gtcaca_editor_set_grammar(gtcaca_editor_widget_t *w, gtcaca_editor_grammar_t *g)
{
  w->grammar = g;
  w->colorize_dirty = 1;
}

#else  /* no Oniguruma: stubs */

gtcaca_editor_grammar_t *gtcaca_editor_grammar_load(const char *path) { (void)path; return NULL; }
void gtcaca_editor_grammar_free(gtcaca_editor_grammar_t *g) { (void)g; }
void gtcaca_editor_set_grammar(gtcaca_editor_widget_t *w, gtcaca_editor_grammar_t *g) { (void)w; (void)g; }
void _gtcaca_editor_colorize_tm(gtcaca_editor_widget_t *w) { (void)w; }

#endif
