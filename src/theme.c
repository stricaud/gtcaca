#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>   /* offsetof — the theme field table */
#ifdef _WIN32
#include <gtcaca/win_compat.h>   /* asprintf */
#include <windows.h>             /* GetModuleFileNameA */
#else
#include <unistd.h>              /* readlink */
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>         /* _NSGetExecutablePath */
#endif

#include <gtcaca/main.h>
#include <gtcaca/iniparse.h>
#include <gtcaca/theme.h>

enum caca_color gtcaca_theme_string_to_caca_color(char *str)
{
  if (!strcmp(str, "black")) {
    return CACA_BLACK;
  }
  if (!strcmp(str, "gray") || !strcmp(str, "grey")) {   /* common alias */
    return CACA_LIGHTGRAY;
  }
  if (!strcmp(str, "blue")) {
    return CACA_BLUE;
  }
  if (!strcmp(str, "green")) {
    return CACA_GREEN;
  }
  if (!strcmp(str, "cyan")) {
    return CACA_CYAN;
  }
  if (!strcmp(str, "red")) {
    return CACA_RED;
  }
  if (!strcmp(str, "magenta")) {
    return CACA_MAGENTA;
  }
  if (!strcmp(str, "brown")) {
    return CACA_BROWN;
  }
  if (!strcmp(str, "lightgray")) {
    return CACA_LIGHTGRAY;
  }
  if (!strcmp(str, "darkgray")) {
    return CACA_DARKGRAY;
  }
  if (!strcmp(str, "lightblue")) {
    return CACA_LIGHTBLUE;
  }
  if (!strcmp(str, "lightgreen")) {
    return CACA_LIGHTGREEN;
  }
  if (!strcmp(str, "lightcyan")) {
    return CACA_LIGHTCYAN;
  }
  if (!strcmp(str, "lightred")) {
    return CACA_LIGHTRED;
  }
  if (!strcmp(str, "lightmagenta")) {
    return CACA_LIGHTMAGENTA;
  }
  if (!strcmp(str, "yellow")) {
    return CACA_YELLOW;
  }
  if (!strcmp(str, "white")) {
    return CACA_WHITE;
  }
  if (!strcmp(str, "default")) {
    return CACA_DEFAULT;
  }
  if (!strcmp(str, "transparent")) {
    return CACA_TRANSPARENT;
  }

  return CACA_DEFAULT;
}

void gtcaca_theme_default(void)
{
  /* A consistent blue-desktop / red-button scheme, in the spirit of Debian's
     `dialog`/whiptail and classic ncurses TUIs: surfaces are blue, the active
     control is red, selections are cyan, text inputs are white. Every pair has
     clear fg/bg contrast so no widget is ever invisible. */
  gmo.theme._default.bg      = CACA_BLUE;       gmo.theme._default.fg      = CACA_WHITE;

  gmo.theme.window.bg        = CACA_BLUE;       gmo.theme.window.fg        = CACA_WHITE;
  gmo.theme.windowfocus.bg   = CACA_BLUE;       gmo.theme.windowfocus.fg   = CACA_YELLOW;

  /* text / label / list / expander */
  gmo.theme.text.bg          = CACA_BLUE;       gmo.theme.text.fg          = CACA_WHITE;
  gmo.theme.textfocus.bg     = CACA_CYAN;       gmo.theme.textfocus.fg     = CACA_BLACK;

  /* buttons: inactive blue, ACTIVE = red (the look) */
  gmo.theme.button.bg        = CACA_BLUE;       gmo.theme.button.fg        = CACA_WHITE;
  gmo.theme.buttonfocus.bg   = CACA_RED;        gmo.theme.buttonfocus.fg   = CACA_WHITE;

  /* text inputs: a dark field, white when focused */
  gmo.theme.entry.bg         = CACA_BLACK;      gmo.theme.entry.fg         = CACA_LIGHTGRAY;
  gmo.theme.entryfocus.bg    = CACA_WHITE;      gmo.theme.entryfocus.fg    = CACA_BLACK;

  /* checkbox / radio / switch */
  gmo.theme.checkbox.bg      = CACA_BLUE;       gmo.theme.checkbox.fg      = CACA_WHITE;
  gmo.theme.checkboxfocus.bg = CACA_CYAN;       gmo.theme.checkboxfocus.fg = CACA_BLACK;

  gmo.theme.combobox.bg      = CACA_BLUE;       gmo.theme.combobox.fg      = CACA_WHITE;
  gmo.theme.comboboxfocus.bg = CACA_CYAN;       gmo.theme.comboboxfocus.fg = CACA_BLACK;

  gmo.theme.progressbar.bg   = CACA_BLUE;       gmo.theme.progressbar.fg   = CACA_LIGHTGREEN;

  gmo.theme.statusbar.bg     = CACA_CYAN;       gmo.theme.statusbar.fg     = CACA_BLACK;

  /* the multi-line text/editor surface stays dark (apps like cacamacs want it) */
  gmo.theme.textview.bg      = CACA_BLACK;      gmo.theme.textview.fg      = CACA_LIGHTGRAY;
  gmo.theme.textviewfocus.bg = CACA_BLACK;      gmo.theme.textviewfocus.fg = CACA_WHITE;

  gmo.theme.menu.bg          = CACA_LIGHTGRAY;  gmo.theme.menu.fg          = CACA_BLACK;
  gmo.theme.menuitem.bg      = CACA_LIGHTGRAY;  gmo.theme.menuitem.fg      = CACA_BLACK;
  gmo.theme.menuitemfocus.bg = CACA_RED;        gmo.theme.menuitemfocus.fg = CACA_WHITE;
}

/* ── finding a theme file ─────────────────────────────────────────────────────
 *
 * A theme used to be resolved against GTCACA_DATA_DIR alone — the install
 * prefix frozen into the library when it was *compiled*. Anything relocatable
 * (a pip wheel, a .pkg, a directory copied to another machine) therefore looked
 * for its theme in a directory that only ever existed on the build host, missed
 * it every single time, and printed a line naming that build directory on every
 * single start-up. Applications had no way to correct the path and no way to
 * quiet the message.
 *
 * So: search the places a deployed program actually keeps a theme, let the
 * application add its own, and say nothing when there is no theme file. Falling
 * back to the built-in palette is the ordinary case, not a fault — and a
 * library that complains about the ordinary case is one every embedder has to
 * work around.
 */

#define THEME_PATH_LEN 1024

static char *g_search[GTCACA_THEME_MAX_SEARCH_DIRS];
static int   g_nsearch = 0;
static int   g_verbose = 0;
static char  g_loaded[THEME_PATH_LEN] = "";

#ifdef _WIN32
#define THEME_SEP "\\"
#else
#define THEME_SEP "/"
#endif

void gtcaca_theme_add_search_dir(const char *dir)
{
  char *copy;
  int i;
  if (!dir || !*dir) { return; }
  if (g_nsearch >= GTCACA_THEME_MAX_SEARCH_DIRS) { return; }
  copy = strdup(dir);
  if (!copy) { return; }
  for (i = g_nsearch; i > 0; i--) { g_search[i] = g_search[i - 1]; }  /* newest first */
  g_search[0] = copy;
  g_nsearch++;
}

void gtcaca_theme_set_verbose(int on) { g_verbose = on ? 1 : 0; }

const char *gtcaca_theme_loaded_path(void) { return g_loaded[0] ? g_loaded : NULL; }

/* Directory holding the running executable, so a bundle can find the data it
   shipped with no matter where it was built or unpacked. NULL if unknown.
   Public because every application that ships data files needs exactly this
   and would otherwise write the same three #ifdefs. */
const char *gtcaca_executable_dir(void)
{
  static char dir[THEME_PATH_LEN];
  static int done = 0;
  char *cut;

  if (done) { return dir[0] ? dir : NULL; }
  done = 1;

#if defined(_WIN32)
  if (!GetModuleFileNameA(NULL, dir, (DWORD)sizeof dir)) { dir[0] = '\0'; }
#elif defined(__APPLE__)
  { uint32_t n = (uint32_t)sizeof dir;
    if (_NSGetExecutablePath(dir, &n) != 0) { dir[0] = '\0'; } }
#else
  { ssize_t n = readlink("/proc/self/exe", dir, sizeof dir - 1);
    if (n > 0) { dir[n] = '\0'; } else { dir[0] = '\0'; } }
#endif
  if (!dir[0]) { return NULL; }

  cut = strrchr(dir, '/');
#ifdef _WIN32
  { char *bs = strrchr(dir, '\\'); if (bs && (!cut || bs > cut)) { cut = bs; } }
#endif
  if (!cut) { dir[0] = '\0'; return NULL; }
  *cut = '\0';
  return dir;
}

/* <exe dir>/../share/gtcaca — where a relocatable bundle keeps its data. */
static const char *gtcaca_bundle_data_dir(void)
{
  static char dir[THEME_PATH_LEN];
  static int done = 0;
  const char *exe;

  if (done) { return dir[0] ? dir : NULL; }
  done = 1;
  exe = gtcaca_executable_dir();
  if (exe) { snprintf(dir, sizeof dir, "%s" THEME_SEP ".." THEME_SEP "share" THEME_SEP "gtcaca", exe); }
  return dir[0] ? dir : NULL;
}

/* The user's own gtcaca directory. */
static const char *gtcaca_user_config_dir(void)
{
  static char dir[THEME_PATH_LEN];
  static int done = 0;
  const char *base;

  if (done) { return dir[0] ? dir : NULL; }
  done = 1;
#ifdef _WIN32
  base = getenv("APPDATA");
  if (!base || !*base) { base = getenv("USERPROFILE"); }
  if (base && *base) { snprintf(dir, sizeof dir, "%s" THEME_SEP "gtcaca", base); }
#else
  base = getenv("XDG_CONFIG_HOME");
  if (base && *base) {
    snprintf(dir, sizeof dir, "%s/gtcaca", base);
  } else {
    base = getenv("HOME");
    if (base && *base) { snprintf(dir, sizeof dir, "%s/.config/gtcaca", base); }
  }
#endif
  return dir[0] ? dir : NULL;
}

/* "<dir>/themes/<name>", with any trailing separator on `dir` collapsed — that
   is what used to produce the "share/gtcaca//themes/default" in the message. */
static void gtcaca_theme_join(char *out, size_t outsz, const char *dir, const char *name)
{
  size_t n = strlen(dir);
  while (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\')) { n--; }
  snprintf(out, outsz, "%.*s" THEME_SEP "themes" THEME_SEP "%s", (int)n, dir, name);
}

/* Every colour pair in gtcaca_theme_t, by the name a theme file uses.  Driven
   from a table rather than a chain of strcmp()s because the chain only ever
   grew to eight of these twenty: a theme file setting windowfocus_fg or
   statusbar_bg — both of which the *shipped* default theme sets — was parsed,
   matched nothing, and was dropped without a word. */
#define TF(name, field) { name, offsetof(gtcaca_theme_t, field) }
static const struct { const char *name; size_t off; } THEME_FIELDS[] = {
  TF("default",       _default),      TF("window",        window),
  TF("windowfocus",   windowfocus),   TF("text",          text),
  TF("textfocus",     textfocus),     TF("button",        button),
  TF("buttonfocus",   buttonfocus),   TF("entry",         entry),
  TF("entryfocus",    entryfocus),    TF("checkbox",      checkbox),
  TF("checkboxfocus", checkboxfocus), TF("progressbar",   progressbar),
  TF("statusbar",     statusbar),     TF("textview",      textview),
  TF("textviewfocus", textviewfocus), TF("combobox",      combobox),
  TF("comboboxfocus", comboboxfocus), TF("menu",          menu),
  TF("menuitem",      menuitem),      TF("menuitemfocus", menuitemfocus)
};
#define N_THEME_FIELDS ((int)(sizeof THEME_FIELDS / sizeof *THEME_FIELDS))

/* Apply one theme file over whatever is already in gmo.theme. Keys it does not
   mention keep their current value, which is what makes the files stack. */
static int gtcaca_theme_apply_file(const char *path)
{
  ini_t *ini = ini_parse_file((char *)path);
  int count, i;

  if (!ini) { return 0; }

  for (count = 0; count < ini->n_items; count += 2) {
    const char *k = ini->keyvals[count];
    const char *v = ini->keyvals[count + 1];
    const char *field, *suffix;
    size_t flen;

    if (strncmp(k, "gtcaca_theme.", 13) != 0) { continue; }
    field = k + 13;
    suffix = strrchr(field, '_');
    if (!suffix) { continue; }
    flen = (size_t)(suffix - field);

    for (i = 0; i < N_THEME_FIELDS; i++) {
      gtcaca_theme_color_t *c;
      if (strlen(THEME_FIELDS[i].name) != flen) { continue; }
      if (strncmp(THEME_FIELDS[i].name, field, flen) != 0) { continue; }
      c = (gtcaca_theme_color_t *)((char *)&gmo.theme + THEME_FIELDS[i].off);
      if      (!strcmp(suffix, "_bg")) { c->bg = gtcaca_theme_string_to_caca_color((char *)v); }
      else if (!strcmp(suffix, "_fg")) { c->fg = gtcaca_theme_string_to_caca_color((char *)v); }
      break;
    }
  }
  ini_free(ini);
  return 1;
}

int gtcaca_theme_parse_ini(char *theme)
{
  const char *dirs[GTCACA_THEME_MAX_SEARCH_DIRS + 3];
  char path[THEME_PATH_LEN];
  const char *env;
  int i, ndirs = 0, applied = 0;

  if (!theme) { theme = "default"; }

  /* Seed every colour with the built-in defaults FIRST, so a theme file that
     only sets some keys (e.g. one with no statusbar lines) doesn't leave the
     rest at 0 = CACA_BLACK — which silently made the status bar black-on-black
     (invisible) whenever a partial theme file was installed. */
  gtcaca_theme_default();
  g_loaded[0] = '\0';

  /* Lowest priority first: each file found is applied over the one before it,
     so a partial file layers instead of replacing. That is what lets an
     application ship a default theme that adjusts gtcaca's without having to
     restate every colour — and lets the user's own file adjust that in turn.

         GTCACA_DATA_DIR            the prefix the library was built for
         <exe>/../share/gtcaca      what a relocatable bundle shipped
         application search dirs    the app's own defaults
         user config dir            the person running it
         $GTCACA_THEME_DIR          an explicit directory
         $GTCACA_THEME              one explicit file                       */
  dirs[ndirs++] = GTCACA_DATA_DIR;
  if ((env = gtcaca_bundle_data_dir()))           { dirs[ndirs++] = env; }
  for (i = g_nsearch - 1; i >= 0; i--)            { dirs[ndirs++] = g_search[i]; }
  if ((env = gtcaca_user_config_dir()))           { dirs[ndirs++] = env; }
  if ((env = getenv("GTCACA_THEME_DIR")) && *env) { dirs[ndirs++] = env; }

  for (i = 0; i < ndirs; i++) {
    if (!dirs[i] || !*dirs[i]) { continue; }
    gtcaca_theme_join(path, sizeof path, dirs[i], theme);
    if (gtcaca_theme_apply_file(path)) {
      applied++;
      snprintf(g_loaded, sizeof g_loaded, "%s", path);
    }
  }

  env = getenv("GTCACA_THEME");
  if (env && *env) {
    if (gtcaca_theme_apply_file(env)) {
      applied++;
      snprintf(g_loaded, sizeof g_loaded, "%s", env);
    } else if (g_verbose) {
      fprintf(stderr, "gtcaca: $GTCACA_THEME is set but %s cannot be read\n", env);
    }
  }

  /* No theme file anywhere is a perfectly good outcome — the built-in palette
     stands. Only an application that asked to hear about it is told. */
  if (!applied && g_verbose) {
    fprintf(stderr, "gtcaca: no \"%s\" theme file found; using the built-in palette. Looked in:\n", theme);
    for (i = 0; i < ndirs; i++) {
      if (!dirs[i] || !*dirs[i]) { continue; }
      gtcaca_theme_join(path, sizeof path, dirs[i], theme);
      fprintf(stderr, "  %s\n", path);
    }
  }

  return 0;
}
