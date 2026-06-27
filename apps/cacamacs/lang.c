#include "cacamacs.h"
/* ── modeline ──────────────────────────────────────────────────────────────── */

void refresh_modeline(gtcaca_editor_widget_t *ed, void *ud)
{
  char text[256];
  int line = gtcaca_editor_get_current_line(ed) + 1;
  int col  = gtcaca_editor_get_column(ed, gtcaca_editor_get_current_pos(ed)) + 1;
  const char *name = g_filename ? g_filename : "*scratch*";
  const char *mod  = gtcaca_editor_get_modify(ed) ? "**" : "--";

  (void)ud;
  show_paren(ed);
  /* keep fold structure in step with edits */
  if (gtcaca_editor_get_json_mode(ed)) gtcaca_editor_fold_json(ed);
  else if (g_folding)                  gtcaca_editor_fold_by_indentation(ed);
  if (g_message[0])
    snprintf(text, sizeof(text), " %s %s  (%s)  L%d C%d   %s", mod, name, g_langname, line, col, g_message);
  else
    snprintf(text, sizeof(text), " %s %s  (%s)  L%d C%d   C-x C-s save  C-x C-c quit  C-g cancel",
             mod, name, g_langname, line, col);

  gtcaca_statusbar_set_text(g_modeline, text);
}

/* ── file loading ──────────────────────────────────────────────────────────── */

char *read_file(const char *path)
{
  FILE *f = fopen(path, "r");
  long n;
  char *buf;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n < 0) n = 0;
  buf = malloc((size_t)n + 1);
  if (buf) { size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; }
  fclose(f);
  return buf;
}

/* ── configuration (~/.ccm/cacamacs-config.json) ───────────────────────────────── */

int _json_int(gtcaca_json_value *o, const char *key, int dflt)
{
  gtcaca_json_value *v = gtcaca_json_object_get(o, key);
  return (v && v->type == GTCACA_JSON_NUMBER) ? (int)v->u.number : dflt;
}
int _json_bool(gtcaca_json_value *o, const char *key, int dflt)
{
  gtcaca_json_value *v = gtcaca_json_object_get(o, key);
  return (v && v->type == GTCACA_JSON_BOOL) ? v->u.boolean : dflt;
}

void load_config(void)
{
  const char *home = getenv("HOME");
  char path[1024];
  gtcaca_json_value *root, *langs;
  if (!home) return;
  snprintf(path, sizeof path, "%s/.ccm/cacamacs-config.json", home);
  root = gtcaca_json_parse_file(path);
  if (!root) {   /* fall back to the old location */
    snprintf(path, sizeof path, "%s/.cacamacs/config.json", home);
    root = gtcaca_json_parse_file(path);
  }
  if (!root) return;   /* keep built-in defaults */

  g_cfg_tab    = _json_int(root, "tabSize", g_cfg_tab);
  g_cfg_spaces = _json_bool(root, "insertSpaces", g_cfg_spaces);
  g_cfg_indent = _json_int(root, "indentSize", g_cfg_indent);
  g_cfg_edge   = _json_int(root, "edgeColumn", g_cfg_edge);

  /* per-language overrides keyed by file extension, e.g. ".py": { ... } */
  langs = gtcaca_json_object_get(root, "languages");
  if (langs && langs->type == GTCACA_JSON_OBJECT) {
    int i;
    for (i = 0; i < langs->u.object.count && g_noverrides < 32; i++) {
      gtcaca_json_value *o = langs->u.object.vals[i];
      lang_override_t *ov = &g_overrides[g_noverrides];
      if (!o || o->type != GTCACA_JSON_OBJECT) continue;
      strncpy(ov->ext, langs->u.object.keys[i], sizeof ov->ext - 1);
      ov->ext[sizeof ov->ext - 1] = '\0';
      ov->has_ts = gtcaca_json_object_get(o, "tabSize") != NULL;     ov->tab_size = _json_int(o, "tabSize", g_cfg_tab);
      ov->has_is = gtcaca_json_object_get(o, "insertSpaces") != NULL; ov->insert_spaces = _json_bool(o, "insertSpaces", g_cfg_spaces);
      ov->has_id = gtcaca_json_object_get(o, "indentSize") != NULL;  ov->indent_size = _json_int(o, "indentSize", g_cfg_indent);
      g_noverrides++;
    }
  }
  gtcaca_json_free(root);
}

/* Compute the effective indentation for a file extension and apply tab width. */
void apply_indent_settings(const char *ext)
{
  int i;
  g_tab_size = g_cfg_tab; g_insert_spaces = g_cfg_spaces; g_indent_size = g_cfg_indent;
  if (ext) {
    for (i = 0; i < g_noverrides; i++) {
      if (!strcmp(g_overrides[i].ext, ext)) {
        if (g_overrides[i].has_ts) g_tab_size      = g_overrides[i].tab_size;
        if (g_overrides[i].has_is) g_insert_spaces = g_overrides[i].insert_spaces;
        if (g_overrides[i].has_id) g_indent_size   = g_overrides[i].indent_size;
        break;
      }
    }
  }
  if (g_ed) gtcaca_editor_set_tab_width(g_ed, g_tab_size > 0 ? g_tab_size : 8);
}

/* ── language detection (VSCode-style extensions) ──────────────────────────── */

/* Built-in keyword lists, keyed by VSCode language id. Comments/brackets/
   strings come from the language-configuration.json; keywords are not part of
   that file (they live in TextMate grammars), so we ship a few common sets. */
void apply_keywords(gtcaca_editor_langcfg_t *cfg, const char *langid)
{
  static const char *kw_c[] = {
    "auto","break","case","char","const","continue","default","do","double",
    "else","enum","extern","float","for","goto","if","inline","int","long",
    "register","restrict","return","short","signed","sizeof","static","struct",
    "switch","typedef","union","unsigned","void","volatile","while","bool" };
  static const char *kw_js[] = {
    "async","await","break","case","catch","class","const","continue","debugger",
    "default","delete","do","else","export","extends","false","finally","for",
    "function","if","import","in","instanceof","let","new","null","return","super",
    "switch","this","throw","true","try","typeof","var","void","while","with","yield" };
  static const char *kw_py[] = {
    "and","as","assert","async","await","break","class","continue","def","del",
    "elif","else","except","False","finally","for","from","global","if","import",
    "in","is","lambda","None","nonlocal","not","or","pass","raise","return","True",
    "try","while","with","yield" };

  const char **chosen = NULL;
  int n = 0;

  if (!langid) return;
  if (!strcmp(langid, "c") || !strcmp(langid, "cpp") || !strcmp(langid, "c++")) {
    chosen = kw_c; n = (int)(sizeof kw_c / sizeof *kw_c);
  } else if (!strcmp(langid, "javascript") || !strcmp(langid, "typescript") ||
             !strcmp(langid, "javascriptreact") || !strcmp(langid, "typescriptreact")) {
    chosen = kw_js; n = (int)(sizeof kw_js / sizeof *kw_js);
  } else if (!strcmp(langid, "python")) {
    chosen = kw_py; n = (int)(sizeof kw_py / sizeof *kw_py);
  }

  if (chosen) {
    gtcaca_editor_langcfg_set_keywords(cfg, chosen, n);   /* for colourization */
    g_keywords = chosen; g_n_keywords = n;                /* and for completion */
  }
}

/* ── completion: gather candidates from the document + language keywords ───── */

int _cmp_str(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

void _cand_add(char ***arr, int *n, int *cap, const char *s, int slen)
{
  int i;
  for (i = 0; i < *n; i++) if (strncmp((*arr)[i], s, (size_t)slen) == 0 && (*arr)[i][slen] == '\0') return; /* dup */
  if (*n == *cap) {
    int nc = *cap ? *cap * 2 : 32;
    char **na = realloc(*arr, (size_t)nc * sizeof(char *));
    if (!na) return;
    *arr = na; *cap = nc;
  }
  (*arr)[*n] = malloc((size_t)slen + 1);
  if ((*arr)[*n]) { memcpy((*arr)[*n], s, (size_t)slen); (*arr)[*n][slen] = '\0'; (*n)++; }
}

void complete_at_point(gtcaca_editor_widget_t *ed)
{
  int pos = gtcaca_editor_get_current_pos(ed);
  int start = pos, plen, len, i;
  char prefix[128], *doc;
  char **cand = NULL; int ncand = 0, cap = 0;

  while (start > 0 && is_word_char(gtcaca_editor_get_char_at(ed, start - 1))) start--;
  plen = pos - start;
  if (plen <= 0) { snprintf(g_message, sizeof g_message, "Type a prefix to complete"); return; }
  gtcaca_editor_get_text_range(ed, start, pos, prefix, sizeof prefix);

  /* candidates from identifiers already in the document */
  len = gtcaca_editor_get_length(ed);
  doc = malloc((size_t)len + 1);
  if (doc) {
    gtcaca_editor_get_text(ed, doc, len + 1);
    i = 0;
    while (i < len) {
      if (isalpha((unsigned char)doc[i]) || doc[i] == '_' || doc[i] == '$') {
        int s = i;
        while (i < len && is_word_char(doc[i])) i++;
        if (i - s > plen && strncmp(doc + s, prefix, (size_t)plen) == 0)
          _cand_add(&cand, &ncand, &cap, doc + s, i - s);
      } else i++;
    }
    free(doc);
  }

  /* candidates from the language's keyword list */
  for (i = 0; i < g_n_keywords; i++) {
    int kl = (int)strlen(g_keywords[i]);
    if (kl > plen && strncmp(g_keywords[i], prefix, (size_t)plen) == 0)
      _cand_add(&cand, &ncand, &cap, g_keywords[i], kl);
  }

  if (ncand == 0) { snprintf(g_message, sizeof g_message, "No completions for '%s'", prefix); }
  else {
    /* join (sorted) into a single space-separated list and show it */
    int total = 0, off = 0;
    char *list;
    qsort(cand, (size_t)ncand, sizeof(char *), _cmp_str);
    for (i = 0; i < ncand; i++) total += (int)strlen(cand[i]) + 1;
    list = malloc((size_t)total + 1);
    if (list) {
      for (i = 0; i < ncand; i++) { int l = (int)strlen(cand[i]); if (i) list[off++] = ' '; memcpy(list + off, cand[i], (size_t)l); off += l; }
      list[off] = '\0';
      gtcaca_editor_autoc_show(ed, plen, list);
      free(list);
      snprintf(g_message, sizeof g_message, "%d completion%s", ncand, ncand == 1 ? "" : "s");
    }
  }
  for (i = 0; i < ncand; i++) free(cand[i]);
  free(cand);
}

/* Guess a language id from a file extension (incl. dot), for keyword selection
   when none was reported by the extension manifest. */
const char *langid_from_ext(const char *ext)
{
  if (!ext) return NULL;
  if (!strcmp(ext, ".c") || !strcmp(ext, ".h")) return "c";
  if (!strcmp(ext, ".cpp") || !strcmp(ext, ".cc") || !strcmp(ext, ".hpp")) return "cpp";
  if (!strcmp(ext, ".js") || !strcmp(ext, ".mjs")) return "javascript";
  if (!strcmp(ext, ".ts")) return "typescript";
  if (!strcmp(ext, ".py")) return "python";
  return NULL;
}

/*
 * Look through ~/.ccm/extensions for a contributed language whose
 * `extensions` include the opened file's suffix, then load its `configuration`
 * (the language-configuration.json). Mirrors how VSCode resolves a language
 * from installed extensions.
 */

/* Extension roots, scanned in order. The first is cacamacs' own; the rest are
   real editor extension folders so installed VSCode extensions work directly. */
static const char *g_ext_roots[] = {
  "/.ccm/extensions",
  "/.cacamacs/extensions",
  "/.vscode/extensions",
  "/.vscode-oss/extensions",
  "/.cursor/extensions",
};
#define N_EXT_ROOTS ((int)(sizeof g_ext_roots / sizeof *g_ext_roots))

/* Built-in extension folders shipped inside the editor app itself: the stock
   grammars (Python's MagicPython, C, JS, …) live here, NOT in the per-user
   extensions dir, so installing e.g. ms-python doesn't bring the grammar. These
   are absolute paths, scanned as-is. Override/add one with $CCM_BUILTIN_EXTENSIONS. */
static const char *g_ext_roots_abs[] = {
  "/Applications/VSCodium.app/Contents/Resources/app/extensions",
  "/Applications/Visual Studio Code.app/Contents/Resources/app/extensions",
  "/Applications/Cursor.app/Contents/Resources/app/extensions",
  "/usr/share/codium/resources/app/extensions",
  "/usr/share/code/resources/app/extensions",
  "/usr/lib/code/extensions",
  "/opt/visual-studio-code/resources/app/extensions",
};
#define N_EXT_ROOTS_ABS ((int)(sizeof g_ext_roots_abs / sizeof *g_ext_roots_abs))

/* Does a contributed language match this file? VSCode associates a language
   either by file extension (`extensions`: ".cmake") or by exact base name
   (`filenames`: "CMakeLists.txt", "Makefile", "Dockerfile", …). */
static int lang_matches(gtcaca_json_value *L, const char *ext, const char *base)
{
  gtcaca_json_value *a;
  int j, n;
  a = gtcaca_json_object_get(L, "extensions"); n = gtcaca_json_array_size(a);
  for (j = 0; j < n; j++) {
    const char *s = gtcaca_json_string(gtcaca_json_array_get(a, j));
    if (s && ext && strcmp(s, ext) == 0) return 1;
  }
  /* filenames are matched case-insensitively, like VSCode (and to tolerate
     manifest typos such as twxs.cmake's "CMakelists.txt"). */
  a = gtcaca_json_object_get(L, "filenames"); n = gtcaca_json_array_size(a);
  for (j = 0; j < n; j++) {
    const char *s = gtcaca_json_string(gtcaca_json_array_get(a, j));
    if (s && base && strcasecmp(s, base) == 0) return 1;
  }
  return 0;
}

/* Try one extension directory: read <dir>/package.json and, if one of its
   contributed languages matches `ext`/`base`, load its configuration. */
gtcaca_editor_langcfg_t *try_extension(const char *dir, const char *ext, const char *base,
                                              char *out_id, int idsz)
{
  char pkgpath[1024];
  gtcaca_json_value *pkg, *langs;
  gtcaca_editor_langcfg_t *result = NULL;
  int i, nlang;

  snprintf(pkgpath, sizeof pkgpath, "%s/package.json", dir);
  pkg = gtcaca_json_parse_file(pkgpath);
  if (!pkg) return NULL;

  langs = gtcaca_json_object_get(gtcaca_json_object_get(pkg, "contributes"), "languages");
  nlang = gtcaca_json_array_size(langs);
  for (i = 0; i < nlang && !result; i++) {
    gtcaca_json_value *L = gtcaca_json_array_get(langs, i);
    const char *cfg, *id;

    if (!lang_matches(L, ext, base)) continue;

    cfg = gtcaca_json_string(gtcaca_json_object_get(L, "configuration"));
    id  = gtcaca_json_string(gtcaca_json_object_get(L, "id"));
    if (cfg) {
      char cfgpath[1024];
      const char *rel = cfg;
      gtcaca_editor_langcfg_t *lc;
      if (rel[0] == '.' && rel[1] == '/') rel += 2;
      snprintf(cfgpath, sizeof cfgpath, "%s/%s", dir, rel);
      lc = gtcaca_editor_langcfg_new();
      if (lc && gtcaca_editor_langcfg_load_json(lc, cfgpath) == 0) {
        result = lc;
        if (id && out_id) { strncpy(out_id, id, idsz - 1); out_id[idsz - 1] = '\0'; }
      } else {
        gtcaca_editor_langcfg_free(lc);
      }
    }
  }
  gtcaca_json_free(pkg);
  return result;
}

/* Scan one root dir: a package.json dropped straight in, then one folder per
   extension (the normal VSCode layout). */
gtcaca_editor_langcfg_t *scan_root_langcfg(const char *root, const char *ext, const char *base,
                                                  char *out_id, int idsz)
{
  DIR *d;
  struct dirent *de;
  gtcaca_editor_langcfg_t *result = try_extension(root, ext, base, out_id, idsz);
  if (result) return result;
  d = opendir(root);
  if (!d) return NULL;
  while ((de = readdir(d)) != NULL && !result) {
    char sub[1024];
    if (de->d_name[0] == '.') continue;
    snprintf(sub, sizeof sub, "%s/%s", root, de->d_name);
    result = try_extension(sub, ext, base, out_id, idsz);
  }
  closedir(d);
  return result;
}

gtcaca_editor_langcfg_t *discover_langcfg(const char *filename,
                                                 char *out_id, int idsz)
{
  const char *home = getenv("HOME");
  const char *ext  = filename ? strrchr(filename, '.') : NULL;
  const char *base = filename ? (strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename) : NULL;
  int r;
  if (!home || !filename) return NULL;
  for (r = 0; r < N_EXT_ROOTS; r++) {
    char extdir[1024];
    gtcaca_editor_langcfg_t *res;
    snprintf(extdir, sizeof extdir, "%s%s", home, g_ext_roots[r]);
    res = scan_root_langcfg(extdir, ext, base, out_id, idsz);
    if (res) return res;
  }
  { const char *e = getenv("CCM_BUILTIN_EXTENSIONS");
    if (e && *e) { gtcaca_editor_langcfg_t *res = scan_root_langcfg(e, ext, base, out_id, idsz); if (res) return res; } }
  for (r = 0; r < N_EXT_ROOTS_ABS; r++) {
    gtcaca_editor_langcfg_t *res = scan_root_langcfg(g_ext_roots_abs[r], ext, base, out_id, idsz);
    if (res) return res;
  }
  return NULL;
}

/* Find a TextMate grammar (contributes.grammars[].path) in <dir> whose language
   matches the file's extension. Fills out_path / out_id; returns 1 if found. */
int try_grammar(const char *dir, const char *ext, const char *base, char *out_path, int psz, char *out_id, int idsz)
{
  char pkgpath[1024];
  gtcaca_json_value *pkg, *langs, *grammars;
  char id[64] = "";
  int i, nlang, ng, found = 0;

  snprintf(pkgpath, sizeof pkgpath, "%s/package.json", dir);
  pkg = gtcaca_json_parse_file(pkgpath);
  if (!pkg) return 0;

  /* which language id owns this extension / filename? */
  langs = gtcaca_json_object_get(gtcaca_json_object_get(pkg, "contributes"), "languages");
  nlang = gtcaca_json_array_size(langs);
  for (i = 0; i < nlang && !id[0]; i++) {
    gtcaca_json_value *L = gtcaca_json_array_get(langs, i);
    if (lang_matches(L, ext, base)) {
      const char *lid = gtcaca_json_string(gtcaca_json_object_get(L, "id"));
      if (lid) { strncpy(id, lid, sizeof id - 1); id[sizeof id - 1] = '\0'; }
    }
  }
  if (!id[0]) { gtcaca_json_free(pkg); return 0; }

  /* a grammar contributed for that language id */
  grammars = gtcaca_json_object_get(gtcaca_json_object_get(pkg, "contributes"), "grammars");
  ng = gtcaca_json_array_size(grammars);
  for (i = 0; i < ng && !found; i++) {
    gtcaca_json_value *G = gtcaca_json_array_get(grammars, i);
    const char *glang = gtcaca_json_string(gtcaca_json_object_get(G, "language"));
    const char *gpath = gtcaca_json_string(gtcaca_json_object_get(G, "path"));
    if (glang && gpath && !strcmp(glang, id)) {
      const char *rel = gpath;
      if (rel[0] == '.' && rel[1] == '/') rel += 2;
      snprintf(out_path, psz, "%s/%s", dir, rel);
      if (out_id) { strncpy(out_id, id, idsz - 1); out_id[idsz - 1] = '\0'; }
      found = 1;
    }
  }
  gtcaca_json_free(pkg);
  return found;
}

int scan_root_grammar(const char *root, const char *ext, const char *base, char *out_path, int psz, char *out_id, int idsz)
{
  DIR *d;
  struct dirent *de;
  int found;
  if (try_grammar(root, ext, base, out_path, psz, out_id, idsz)) return 1;
  d = opendir(root);
  if (!d) return 0;
  found = 0;
  while ((de = readdir(d)) != NULL && !found) {
    char sub[1024];
    if (de->d_name[0] == '.') continue;
    snprintf(sub, sizeof sub, "%s/%s", root, de->d_name);
    found = try_grammar(sub, ext, base, out_path, psz, out_id, idsz);
  }
  closedir(d);
  return found;
}

int discover_grammar(const char *filename, char *out_path, int psz, char *out_id, int idsz)
{
  const char *home = getenv("HOME");
  const char *ext  = filename ? strrchr(filename, '.') : NULL;
  const char *base = filename ? (strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename) : NULL;
  int r;
  if (!home || !filename) return 0;
  for (r = 0; r < N_EXT_ROOTS; r++) {
    char extdir[1024];
    snprintf(extdir, sizeof extdir, "%s%s", home, g_ext_roots[r]);
    if (scan_root_grammar(extdir, ext, base, out_path, psz, out_id, idsz)) return 1;
  }
  { const char *e = getenv("CCM_BUILTIN_EXTENSIONS");
    if (e && *e && scan_root_grammar(e, ext, base, out_path, psz, out_id, idsz)) return 1; }
  for (r = 0; r < N_EXT_ROOTS_ABS; r++)
    if (scan_root_grammar(g_ext_roots_abs[r], ext, base, out_path, psz, out_id, idsz)) return 1;
  return 0;
}

/* Attach colourization to the editor for `filename`, optionally forcing a
   specific language-configuration.json (explicit_cfg). */
void setup_language(gtcaca_editor_widget_t *ed, const char *filename,
                           const char *explicit_cfg)
{
  const char *ext = filename ? strrchr(filename, '.') : NULL;
  char langid[64] = "", gid[64] = "", gpath[1024] = "";

  /* indentation (tab/space + sizes) for this file's extension */
  apply_indent_settings(ext);

  /* reset modes left over from a previously-open file */
  gtcaca_editor_set_json_mode(ed, 0);
  gtcaca_editor_set_grammar(ed, NULL);
  if (g_grammar) { gtcaca_editor_grammar_free(g_grammar); g_grammar = NULL; }

  /* built-in JSON mode: colour + structural folding, no extension needed */
  if (ext && (!strcmp(ext, ".json") || !strcmp(ext, ".jsonl"))) {
    gtcaca_editor_set_json_mode(ed, 1);
    gtcaca_editor_fold_json(ed);
    gtcaca_editor_set_fold_margin(ed, 1);
    g_folding = 1;
    strncpy(g_langname, !strcmp(ext, ".jsonl") ? "jsonl" : "json", sizeof g_langname - 1);
    g_langname[sizeof g_langname - 1] = '\0';
    return;
  }

  /* Best colourization: a TextMate grammar from an installed extension. */
  if (!explicit_cfg && discover_grammar(filename, gpath, sizeof gpath, gid, sizeof gid)) {
    g_grammar = gtcaca_editor_grammar_load(gpath);
    if (g_grammar) {
      gtcaca_editor_set_grammar(ed, g_grammar);
      gtcaca_editor_style_set_bold(ed, GTCACA_EDITOR_STYLE_KEYWORD, 1);
    }
  }

  /* Language configuration: brackets/comments/strings (fallback colour when no
     grammar) and keywords for completion. */
  if (explicit_cfg) {
    g_langcfg = gtcaca_editor_langcfg_new();
    if (g_langcfg && gtcaca_editor_langcfg_load_json(g_langcfg, explicit_cfg) != 0) {
      gtcaca_editor_langcfg_free(g_langcfg);
      g_langcfg = NULL;
    }
  } else {
    g_langcfg = discover_langcfg(filename, langid, sizeof langid);
  }

  if (g_langcfg) {
    if (!langid[0] && gid[0]) { strncpy(langid, gid, sizeof langid - 1); langid[sizeof langid - 1] = '\0'; }
    if (!langid[0]) { const char *guess = langid_from_ext(ext); if (guess) strncpy(langid, guess, sizeof langid - 1); }
    apply_keywords(g_langcfg, langid);
    gtcaca_editor_set_langcfg(ed, g_langcfg);
    if (!g_grammar) gtcaca_editor_style_set_bold(ed, GTCACA_EDITOR_STYLE_KEYWORD, 1);
  }

  /* language name in the modeline */
  if (gid[0])        { strncpy(g_langname, gid, sizeof g_langname - 1); g_langname[sizeof g_langname - 1] = '\0'; }
  else if (langid[0]){ strncpy(g_langname, langid, sizeof g_langname - 1); g_langname[sizeof g_langname - 1] = '\0'; }

  if (!g_grammar && !g_langcfg && ext)
    snprintf(g_message, sizeof g_message,
             "No language for '%s' in ~/.ccm/extensions or the editor's built-ins — colourization off", ext);
}

