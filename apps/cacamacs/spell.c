#include "cacamacs.h"
/* ── spell check (M-$), à la Emacs ispell-word ──────────────────────────────── */

/* The system word list, loaded once and kept lowercased + sorted for bsearch. */
static char  *g_dict_buf = NULL;
static char **g_dict = NULL;
static int    g_dict_n = 0;
static int    g_dict_tried = 0;

static int cmp_pstr(const void *a, const void *b)
{ return strcmp(*(const char *const *)a, *(const char *const *)b); }

static void dict_load(void)
{
  FILE *f;
  long sz;
  char *p, *line;
  char **arr;
  int cap, n;

  if (g_dict_tried) return;            /* try once; cache the result */
  g_dict_tried = 1;

  f = fopen("/usr/share/dict/words", "r");
  if (!f) return;
  fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
  if (sz <= 0) { fclose(f); return; }
  g_dict_buf = malloc((size_t)sz + 1);
  if (!g_dict_buf) { fclose(f); return; }
  sz = (long)fread(g_dict_buf, 1, (size_t)sz, f);
  g_dict_buf[sz] = '\0';
  fclose(f);

  cap = 1 << 18; n = 0;
  arr = malloc((size_t)cap * sizeof *arr);
  if (!arr) { free(g_dict_buf); g_dict_buf = NULL; return; }

  /* split on newlines in place, lowercase each word, collect pointers */
  line = g_dict_buf;
  for (p = g_dict_buf; ; p++) {
    if (*p == '\n' || *p == '\0') {
      int last = (*p == '\0');
      *p = '\0';
      if (line[0]) {
        char *q;
        for (q = line; *q; q++) *q = (char)tolower((unsigned char)*q);
        if (n >= cap) {
          char **t; cap *= 2;
          t = realloc(arr, (size_t)cap * sizeof *arr);
          if (!t) { free(arr); free(g_dict_buf); g_dict_buf = NULL; return; }
          arr = t;
        }
        arr[n++] = line;
      }
      if (last) break;
      line = p + 1;
    }
  }
  qsort(arr, (size_t)n, sizeof *arr, cmp_pstr);
  g_dict = arr; g_dict_n = n;
}

static void lower_copy(const char *in, char *out, size_t outsz)
{
  size_t i;
  for (i = 0; in[i] && i + 1 < outsz; i++) out[i] = (char)tolower((unsigned char)in[i]);
  out[i] = '\0';
}

static int dict_has(const char *w)
{
  char low[80];
  const char *key = low;
  if (!g_dict || !w[0]) return 0;
  lower_copy(w, low, sizeof low);
  return bsearch(&key, g_dict, (size_t)g_dict_n, sizeof *g_dict, cmp_pstr) != NULL;
}

/* web2 is a base-word list: it lacks most regular inflections (tracking, parsing,
   used, quickly, cities…) even when the stem is present. So treat a word as
   correct if it is in the dict, or is a plausible inflection of one that is. */
static int stem_ok(const char *base)
{
  char t[80];
  int n = (int)strlen(base);
  if (n < 2) return 0;
  if (dict_has(base)) return 1;                        /* track(+ing/ed/s) */
  snprintf(t, sizeof t, "%se", base);                  /* parse->parsing, use->used */
  if (dict_has(t)) return 1;
  if (base[n - 1] == base[n - 2]) {                    /* run->running, plan->planned */
    memcpy(t, base, (size_t)n - 1); t[n - 1] = '\0';
    if (dict_has(t)) return 1;
  }
  return 0;
}

static int ends_with(const char *s, int n, const char *suf)
{
  int m = (int)strlen(suf);
  return n >= m && !strcmp(s + n - m, suf);
}

static int is_correct(const char *word)
{
  static const char *suf[] = { "ings", "ing", "edly", "ed", "es", "s",
                               "ly", "ers", "er", "est", "ness", "ment", "ful", NULL };
  char low[80], base[80];
  int n, i, bl;

  lower_copy(word, low, sizeof low);
  n = (int)strlen(low);
  if (n == 0) return 0;
  if (dict_has(low)) return 1;

  if (n > 2 && low[n - 2] == '\'' && low[n - 1] == 's') {     /* possessive: dog's */
    memcpy(base, low, (size_t)n - 2); base[n - 2] = '\0';
    if (dict_has(base)) return 1;
  }
  if (n > 4 && ends_with(low, n, "ies")) {                    /* cities->city, tries->try */
    memcpy(base, low, (size_t)n - 3); base[n - 3] = 'y'; base[n - 2] = '\0';
    if (dict_has(base)) return 1;
  }
  for (i = 0; suf[i]; i++) {
    int sl = (int)strlen(suf[i]);
    if (!ends_with(low, n, suf[i])) continue;
    bl = n - sl;
    if (bl < 2) continue;                                     /* keep a real stem */
    memcpy(base, low, (size_t)bl); base[bl] = '\0';
    if (stem_ok(base)) return 1;
  }
  return 0;
}

/* ── modal suggestion picker ───────────────────────────────────────────────── */

int  g_spell_active = 0;
static char g_spell_sugg[10][64];
static int  g_spell_nsugg = 0;
static int  g_spell_wstart = 0, g_spell_wend = 0;
static int  g_spell_upper = 0;       /* original word started with an uppercase letter */

static void spell_prompt(const char *word)
{
  char line[256];
  int i, off;
  off = snprintf(line, sizeof line, "Misspelled \"%s\":", word);
  for (i = 0; i < g_spell_nsugg && off < (int)sizeof line - 1; i++)
    off += snprintf(line + off, sizeof line - (size_t)off, " %d %s", i, g_spell_sugg[i]);
  snprintf(g_message, sizeof g_message, "%s  (0-%d pick, SPC skip)", line, g_spell_nsugg - 1);
}

static void sugg_add(const char *w)
{
  int i;
  if (g_spell_nsugg >= 10) return;
  for (i = 0; i < g_spell_nsugg; i++) if (!strcmp(g_spell_sugg[i], w)) return;   /* dedupe */
  strncpy(g_spell_sugg[g_spell_nsugg], w, sizeof g_spell_sugg[0] - 1);
  g_spell_sugg[g_spell_nsugg][sizeof g_spell_sugg[0] - 1] = '\0';
  g_spell_nsugg++;
}

/* Collect dictionary words within edit distance 1 of `low` (delete / transpose /
   substitute / insert), up to 10. Mirrors Norvig's edits1. */
static void gen_suggestions(const char *low)
{
  int L = (int)strlen(low), i;
  char buf[80], c;
  if (L <= 0 || L >= 60) return;

  /* length-preserving edits first (they read as closer corrections), so e.g.
     "teh" -> "the" (a transposition) ranks ahead of deletions like "eh". */
  for (i = 0; i + 1 < L && g_spell_nsugg < 10; i++) {             /* transpositions */
    strcpy(buf, low);
    buf[i] = low[i + 1]; buf[i + 1] = low[i];
    if (dict_has(buf)) sugg_add(buf);
  }
  for (i = 0; i < L && g_spell_nsugg < 10; i++) {                 /* substitutions */
    strcpy(buf, low);
    for (c = 'a'; c <= 'z' && g_spell_nsugg < 10; c++) {
      if (c == low[i]) continue;
      buf[i] = c;
      if (dict_has(buf)) sugg_add(buf);
    }
  }
  for (i = 0; i < L && g_spell_nsugg < 10; i++) {                 /* deletions */
    memcpy(buf, low, (size_t)i);
    strcpy(buf + i, low + i + 1);
    if (dict_has(buf)) sugg_add(buf);
  }
  for (i = 0; i <= L && g_spell_nsugg < 10; i++) {                /* insertions */
    for (c = 'a'; c <= 'z' && g_spell_nsugg < 10; c++) {
      memcpy(buf, low, (size_t)i);
      buf[i] = c;
      strcpy(buf + i + 1, low + i);
      if (dict_has(buf)) sugg_add(buf);
    }
  }
}

/* the alphabetic word around the caret; returns its length (0 if none) */
static int is_wordc(char c) { return isalpha((unsigned char)c) || c == '\''; }

static int word_at_point(gtcaca_editor_widget_t *ed, int *ws, int *we, char *buf, int bufsz)
{
  int len = gtcaca_editor_get_length(ed);
  int pos = gtcaca_editor_get_current_pos(ed);
  int s, e, i, n;

  if (pos > 0 && (pos >= len || !is_wordc(gtcaca_editor_get_char_at(ed, pos)))
      && is_wordc(gtcaca_editor_get_char_at(ed, pos - 1)))
    pos--;                                           /* caret sitting just past the word */
  if (pos >= len || !is_wordc(gtcaca_editor_get_char_at(ed, pos))) { *ws = *we = pos; return 0; }

  s = pos; while (s > 0   && is_wordc(gtcaca_editor_get_char_at(ed, s - 1))) s--;
  e = pos; while (e < len && is_wordc(gtcaca_editor_get_char_at(ed, e)))     e++;
  n = e - s; if (n >= bufsz) n = bufsz - 1;
  for (i = 0; i < n; i++) buf[i] = gtcaca_editor_get_char_at(ed, s + i);
  buf[n] = '\0';
  *ws = s; *we = e;
  return n;
}

void spell_word(gtcaca_editor_widget_t *ed)
{
  char word[80], low[80];
  int ws, we, n;

  dict_load();
  if (!g_dict) { snprintf(g_message, sizeof g_message, "No dictionary at /usr/share/dict/words"); return; }

  n = word_at_point(ed, &ws, &we, word, sizeof word);
  if (n == 0) { snprintf(g_message, sizeof g_message, "No word at point"); return; }

  if (is_correct(word)) { snprintf(g_message, sizeof g_message, "\"%s\" is correct", word); return; }

  lower_copy(word, low, sizeof low);
  g_spell_nsugg = 0;
  gen_suggestions(low);
  if (g_spell_nsugg == 0) {
    snprintf(g_message, sizeof g_message, "\"%s\" is misspelled (no suggestions)", word);
    return;
  }
  g_spell_wstart = ws; g_spell_wend = we;
  g_spell_upper  = isupper((unsigned char)word[0]) ? 1 : 0;
  g_spell_active = 1;
  spell_prompt(word);
}

/* 0-9 pick a suggestion (replaces the word, matching its capitalisation);
   SPC/n/q/RET/ESC leave it unchanged. */
int spell_key(gtcaca_editor_widget_t *ed, int key)
{
  if (key >= '0' && key <= '9') {
    int idx = key - '0';
    if (idx < g_spell_nsugg) {
      char rep[64];
      strncpy(rep, g_spell_sugg[idx], sizeof rep - 1); rep[sizeof rep - 1] = '\0';
      if (g_spell_upper && rep[0]) rep[0] = (char)toupper((unsigned char)rep[0]);
      gtcaca_editor_set_target_range(ed, g_spell_wstart, g_spell_wend);
      gtcaca_editor_replace_target(ed, rep);
      gtcaca_editor_goto_pos(ed, g_spell_wstart + (int)strlen(rep));
      snprintf(g_message, sizeof g_message, "Replaced with \"%s\"", rep);
    }
    g_spell_active = 0;
    return 1;
  }
  switch (key) {
  case ' ': case 'n': case 'q': case 10: case CACA_KEY_RETURN: case CACA_KEY_ESCAPE:
    g_spell_active = 0;
    snprintf(g_message, sizeof g_message, "Spelling left unchanged");
    return 1;
  default:
    return 1;   /* swallow other keys while the picker is up */
  }
}
