#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtcaca/json.h>

typedef struct { const char *p; int ok; } jctx;

static gtcaca_json_value *parse_value(jctx *c);

static gtcaca_json_value *new_val(gtcaca_json_type t)
{
  gtcaca_json_value *v = calloc(1, sizeof(*v));
  if (v) v->type = t;
  return v;
}

/* skip whitespace and JSONC comments */
static void skip_ws(jctx *c)
{
  const char *p = c->p;
  for (;;) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p[0] == '/' && p[1] == '/') {
      p += 2;
      while (*p && *p != '\n') p++;
    } else if (p[0] == '/' && p[1] == '*') {
      p += 2;
      while (*p && !(p[0] == '*' && p[1] == '/')) p++;
      if (*p) p += 2;
    } else {
      break;
    }
  }
  c->p = p;
}

/* parse a JSON string (assumes current char is '"'); returns malloc'd UTF-8ish */
static char *parse_string_raw(jctx *c)
{
  const char *p = c->p;
  char *out, *o;
  int cap;

  if (*p != '"') { c->ok = 0; return NULL; }
  p++;
  cap = 16;
  out = malloc(cap);
  if (!out) { c->ok = 0; return NULL; }
  o = out;

  while (*p && *p != '"') {
    int need = (int)(o - out) + 4;
    if (need > cap) { cap *= 2; char *n = realloc(out, cap); if (!n) { free(out); c->ok = 0; return NULL; } o = n + (o - out); out = n; }
    if (*p == '\\' && p[1]) {
      p++;
      switch (*p) {
      case 'n': *o++ = '\n'; break;
      case 't': *o++ = '\t'; break;
      case 'r': *o++ = '\r'; break;
      case 'b': *o++ = '\b'; break;
      case 'f': *o++ = '\f'; break;
      case '/': *o++ = '/';  break;
      case '\\': *o++ = '\\'; break;
      case '"': *o++ = '"';  break;
      case 'u': {
        /* minimal: decode BMP code point, emit ASCII if < 128 else '?' */
        int i, cp = 0, ok = 1;
        for (i = 1; i <= 4; i++) {
          char h = p[i];
          cp <<= 4;
          if (h >= '0' && h <= '9') cp |= h - '0';
          else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
          else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10;
          else { ok = 0; break; }
        }
        if (ok) { *o++ = (cp < 128) ? (char)cp : '?'; p += 4; }
        else      *o++ = '?';
        break;
      }
      default: *o++ = *p; break;
      }
      p++;
    } else {
      *o++ = *p++;
    }
  }
  if (*p != '"') { free(out); c->ok = 0; return NULL; }
  p++;
  *o = '\0';
  c->p = p;
  return out;
}

static gtcaca_json_value *parse_string(jctx *c)
{
  char *s = parse_string_raw(c);
  gtcaca_json_value *v;
  if (!s) return NULL;
  v = new_val(GTCACA_JSON_STRING);
  if (!v) { free(s); c->ok = 0; return NULL; }
  v->u.string = s;
  return v;
}

static gtcaca_json_value *parse_array(jctx *c)
{
  gtcaca_json_value *v = new_val(GTCACA_JSON_ARRAY);
  int cap = 0;
  if (!v) { c->ok = 0; return NULL; }
  c->p++;  /* '[' */
  skip_ws(c);
  if (*c->p == ']') { c->p++; return v; }
  for (;;) {
    gtcaca_json_value *item;
    skip_ws(c);
    if (*c->p == ']') { c->p++; break; }   /* trailing comma */
    item = parse_value(c);
    if (!c->ok) { gtcaca_json_free(v); return NULL; }
    if (v->u.array.count == cap) {
      cap = cap ? cap * 2 : 4;
      gtcaca_json_value **n = realloc(v->u.array.items, cap * sizeof(*n));
      if (!n) { gtcaca_json_free(item); gtcaca_json_free(v); c->ok = 0; return NULL; }
      v->u.array.items = n;
    }
    v->u.array.items[v->u.array.count++] = item;
    skip_ws(c);
    if (*c->p == ',') { c->p++; continue; }
    if (*c->p == ']') { c->p++; break; }
    c->ok = 0; gtcaca_json_free(v); return NULL;
  }
  return v;
}

static gtcaca_json_value *parse_object(jctx *c)
{
  gtcaca_json_value *v = new_val(GTCACA_JSON_OBJECT);
  int cap = 0;
  if (!v) { c->ok = 0; return NULL; }
  c->p++;  /* '{' */
  skip_ws(c);
  if (*c->p == '}') { c->p++; return v; }
  for (;;) {
    char *key;
    gtcaca_json_value *val;
    skip_ws(c);
    if (*c->p == '}') { c->p++; break; }   /* trailing comma */
    key = parse_string_raw(c);
    if (!c->ok) { gtcaca_json_free(v); return NULL; }
    skip_ws(c);
    if (*c->p != ':') { free(key); c->ok = 0; gtcaca_json_free(v); return NULL; }
    c->p++;
    val = parse_value(c);
    if (!c->ok) { free(key); gtcaca_json_free(v); return NULL; }
    if (v->u.object.count == cap) {
      cap = cap ? cap * 2 : 8;
      char **nk = realloc(v->u.object.keys, cap * sizeof(*nk));
      gtcaca_json_value **nv = realloc(v->u.object.vals, cap * sizeof(*nv));
      if (!nk || !nv) { free(nk); free(nv); free(key); gtcaca_json_free(val); gtcaca_json_free(v); c->ok = 0; return NULL; }
      v->u.object.keys = nk; v->u.object.vals = nv;
    }
    v->u.object.keys[v->u.object.count] = key;
    v->u.object.vals[v->u.object.count] = val;
    v->u.object.count++;
    skip_ws(c);
    if (*c->p == ',') { c->p++; continue; }
    if (*c->p == '}') { c->p++; break; }
    c->ok = 0; gtcaca_json_free(v); return NULL;
  }
  return v;
}

static gtcaca_json_value *parse_value(jctx *c)
{
  skip_ws(c);
  switch (*c->p) {
  case '"': return parse_string(c);
  case '[': return parse_array(c);
  case '{': return parse_object(c);
  case 't':
    if (strncmp(c->p, "true", 4) == 0) { c->p += 4; gtcaca_json_value *v = new_val(GTCACA_JSON_BOOL); if (v) v->u.boolean = 1; else c->ok = 0; return v; }
    break;
  case 'f':
    if (strncmp(c->p, "false", 5) == 0) { c->p += 5; gtcaca_json_value *v = new_val(GTCACA_JSON_BOOL); if (v) v->u.boolean = 0; else c->ok = 0; return v; }
    break;
  case 'n':
    if (strncmp(c->p, "null", 4) == 0) { c->p += 4; return new_val(GTCACA_JSON_NULL); }
    break;
  default:
    if (*c->p == '-' || (*c->p >= '0' && *c->p <= '9')) {
      char *end;
      double d = strtod(c->p, &end);
      if (end == c->p) break;
      c->p = end;
      gtcaca_json_value *v = new_val(GTCACA_JSON_NUMBER);
      if (v) v->u.number = d; else c->ok = 0;
      return v;
    }
  }
  c->ok = 0;
  return NULL;
}

gtcaca_json_value *gtcaca_json_parse(const char *text)
{
  jctx c;
  gtcaca_json_value *v;
  if (!text) return NULL;
  c.p = text; c.ok = 1;
  v = parse_value(&c);
  if (!c.ok) { gtcaca_json_free(v); return NULL; }
  return v;
}

gtcaca_json_value *gtcaca_json_parse_file(const char *path)
{
  FILE *f = fopen(path, "r");
  long n;
  char *buf;
  gtcaca_json_value *v;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) { fclose(f); return NULL; }
  buf = malloc((size_t)n + 1);
  if (!buf) { fclose(f); return NULL; }
  { size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
  fclose(f);
  v = gtcaca_json_parse(buf);
  free(buf);
  return v;
}

void gtcaca_json_free(gtcaca_json_value *v)
{
  int i;
  if (!v) return;
  switch (v->type) {
  case GTCACA_JSON_STRING: free(v->u.string); break;
  case GTCACA_JSON_ARRAY:
    for (i = 0; i < v->u.array.count; i++) gtcaca_json_free(v->u.array.items[i]);
    free(v->u.array.items);
    break;
  case GTCACA_JSON_OBJECT:
    for (i = 0; i < v->u.object.count; i++) { free(v->u.object.keys[i]); gtcaca_json_free(v->u.object.vals[i]); }
    free(v->u.object.keys);
    free(v->u.object.vals);
    break;
  default: break;
  }
  free(v);
}

/* ── serialization ─────────────────────────────────────────────────────────── */

typedef struct { char *buf; int len, cap; } sbuf;

static void sb_put(sbuf *s, const char *t, int n)
{
  if (s->len + n + 1 > s->cap) {
    int nc = s->cap ? s->cap * 2 : 256;
    while (nc < s->len + n + 1) nc *= 2;
    char *p = realloc(s->buf, nc);
    if (!p) return;
    s->buf = p; s->cap = nc;
  }
  memcpy(s->buf + s->len, t, (size_t)n);
  s->len += n;
  s->buf[s->len] = '\0';
}
static void sb_str(sbuf *s, const char *t) { sb_put(s, t, (int)strlen(t)); }
static void sb_indent(sbuf *s, int n) { while (n-- > 0) sb_put(s, " ", 1); }

static void sb_json_string(sbuf *s, const char *t)
{
  sb_put(s, "\"", 1);
  for (; *t; t++) {
    switch (*t) {
    case '"':  sb_str(s, "\\\""); break;
    case '\\': sb_str(s, "\\\\"); break;
    case '\n': sb_str(s, "\\n");  break;
    case '\t': sb_str(s, "\\t");  break;
    case '\r': sb_str(s, "\\r");  break;
    case '\b': sb_str(s, "\\b");  break;
    case '\f': sb_str(s, "\\f");  break;
    default:   sb_put(s, t, 1);   break;
    }
  }
  sb_put(s, "\"", 1);
}

static void sb_write(sbuf *s, const gtcaca_json_value *v, int indent, int level)
{
  int i;
  char num[40];
  const char *nl = indent > 0 ? "\n" : "";

  if (!v) { sb_str(s, "null"); return; }
  switch (v->type) {
  case GTCACA_JSON_NULL:   sb_str(s, "null"); break;
  case GTCACA_JSON_BOOL:   sb_str(s, v->u.boolean ? "true" : "false"); break;
  case GTCACA_JSON_NUMBER:
    snprintf(num, sizeof num, "%.15g", v->u.number);
    sb_str(s, num);
    break;
  case GTCACA_JSON_STRING: sb_json_string(s, v->u.string); break;
  case GTCACA_JSON_ARRAY:
    if (v->u.array.count == 0) { sb_str(s, "[]"); break; }
    sb_put(s, "[", 1); sb_str(s, nl);
    for (i = 0; i < v->u.array.count; i++) {
      sb_indent(s, indent * (level + 1));
      sb_write(s, v->u.array.items[i], indent, level + 1);
      if (i + 1 < v->u.array.count) sb_put(s, ",", 1);
      sb_str(s, nl);
    }
    sb_indent(s, indent * level); sb_put(s, "]", 1);
    break;
  case GTCACA_JSON_OBJECT:
    if (v->u.object.count == 0) { sb_str(s, "{}"); break; }
    sb_put(s, "{", 1); sb_str(s, nl);
    for (i = 0; i < v->u.object.count; i++) {
      sb_indent(s, indent * (level + 1));
      sb_json_string(s, v->u.object.keys[i]);
      sb_str(s, indent > 0 ? ": " : ":");
      sb_write(s, v->u.object.vals[i], indent, level + 1);
      if (i + 1 < v->u.object.count) sb_put(s, ",", 1);
      sb_str(s, nl);
    }
    sb_indent(s, indent * level); sb_put(s, "}", 1);
    break;
  }
}

char *gtcaca_json_stringify(const gtcaca_json_value *v, int indent)
{
  sbuf s = { NULL, 0, 0 };
  sb_write(&s, v, indent, 0);
  if (!s.buf) s.buf = strdup("");
  return s.buf;
}

const char *gtcaca_json_string(const gtcaca_json_value *v)
{
  return (v && v->type == GTCACA_JSON_STRING) ? v->u.string : NULL;
}

gtcaca_json_value *gtcaca_json_object_get(const gtcaca_json_value *v, const char *key)
{
  int i;
  if (!v || v->type != GTCACA_JSON_OBJECT || !key) return NULL;
  for (i = 0; i < v->u.object.count; i++)
    if (strcmp(v->u.object.keys[i], key) == 0) return v->u.object.vals[i];
  return NULL;
}

int gtcaca_json_array_size(const gtcaca_json_value *v)
{
  return (v && v->type == GTCACA_JSON_ARRAY) ? v->u.array.count : 0;
}

gtcaca_json_value *gtcaca_json_array_get(const gtcaca_json_value *v, int i)
{
  if (!v || v->type != GTCACA_JSON_ARRAY || i < 0 || i >= v->u.array.count) return NULL;
  return v->u.array.items[i];
}
