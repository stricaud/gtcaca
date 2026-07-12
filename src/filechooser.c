#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <gtcaca/win_compat.h>
#else
#include <dirent.h>
#endif
#include <sys/stat.h>

#include <caca.h>

#include <gtcaca/filechooser.h>
#include <gtcaca/main.h>

static void clear_entries(gtcaca_filechooser_widget_t *fc)
{
  int i;
  for (i = 0; i < fc->nentries; i++) free(fc->entries[i].name);
  fc->nentries = 0;
}

static void add_entry(gtcaca_filechooser_widget_t *fc, const char *name, int isdir)
{
  if (fc->nentries == fc->cap) {
    fc->cap = fc->cap ? fc->cap * 2 : 32;
    fc->entries = realloc(fc->entries, (size_t)fc->cap * sizeof *fc->entries);
  }
  fc->entries[fc->nentries].name = strdup(name);
  fc->entries[fc->nentries].isdir = isdir;
  fc->nentries++;
}

static int cmp_entry(const void *a, const void *b)
{
  const gtcaca_fc_entry_t *x = a, *y = b;
  if (x->isdir != y->isdir) return y->isdir - x->isdir;   /* dirs first */
  return strcasecmp(x->name, y->name);
}

/* case-insensitive substring test */
static int ci_sub(const char *hay, const char *needle)
{
  size_t i;
  if (!needle[0]) return 1;
  for (i = 0; hay[i]; i++) {
    size_t j = 0;
    while (needle[j] && hay[i + j] &&
           tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j])) j++;
    if (!needle[j]) return 1;
  }
  return 0;
}

/* Rebuild filt[] = indices of entries matching the search ("" → all). sel/top
   index into filt[]. ".." stays visible while searching so you can still go up. */
static void rebuild_filter(gtcaca_filechooser_widget_t *fc)
{
  int i;
  if (fc->filt_cap < fc->nentries) {
    fc->filt_cap = fc->nentries + 8;
    fc->filt = realloc(fc->filt, (size_t)fc->filt_cap * sizeof *fc->filt);
  }
  fc->nfilt = 0;
  for (i = 0; i < fc->nentries; i++) {
    if (!fc->searching || !fc->search[0] ||
        strcmp(fc->entries[i].name, "..") == 0 || ci_sub(fc->entries[i].name, fc->search))
      fc->filt[fc->nfilt++] = i;
  }
  if (fc->sel >= fc->nfilt) fc->sel = fc->nfilt ? fc->nfilt - 1 : 0;
  if (fc->sel < 0) fc->sel = 0;
  if (fc->top > fc->sel) fc->top = fc->sel;
}

static void read_dir(gtcaca_filechooser_widget_t *fc)
{
  DIR *dp = opendir(fc->curdir);
  struct dirent *de;
  clear_entries(fc);
  add_entry(fc, "..", 1);
  if (dp) {
    while ((de = readdir(dp)) != NULL) {
      char full[PATH_MAX]; struct stat st; int isdir;
      if (de->d_name[0] == '.' ) continue;                /* skip dotfiles + . / .. */
      snprintf(full, sizeof full, "%s/%s", fc->curdir, de->d_name);
      isdir = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
      add_entry(fc, de->d_name, isdir);
    }
    closedir(dp);
  }
  qsort(fc->entries + 1, (size_t)(fc->nentries - 1), sizeof *fc->entries, cmp_entry);
  fc->sel = 0; fc->top = 0;
  fc->searching = 0; fc->search[0] = '\0';
  rebuild_filter(fc);
}

void gtcaca_filechooser_set_dir(gtcaca_filechooser_widget_t *fc, const char *dir)
{
  char resolved[PATH_MAX];
  if (dir && realpath(dir, resolved)) snprintf(fc->curdir, sizeof fc->curdir, "%s", resolved);
  else if (!realpath(".", fc->curdir)) snprintf(fc->curdir, sizeof fc->curdir, "/");
  read_dir(fc);
}

gtcaca_filechooser_widget_t *gtcaca_filechooser_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_filechooser_widget_t *fc = calloc(1, sizeof(*fc));
  if (!fc) { fprintf(stderr, "Error: Cannot allocate filechooser\n"); return NULL; }
  fc->id = gtcaca_get_newid();
  fc->has_focus = 1; fc->is_visible = 1; fc->type = GTCACA_WIDGET_FILECHOOSERDIALOG;
  fc->parent = parent;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(fc), x, y);
  fc->width = width; fc->height = height;
  fc->color_focus_fg = fc->color_nonfocus_fg = gmo.theme.textview.fg;
  fc->color_focus_bg = fc->color_nonfocus_bg = gmo.theme.textview.bg;
  fc->mode = GTCACA_FILECHOOSER_OPEN; fc->result = -100;
  fc->title = NULL;
  fc->nopt = 0; fc->opt_sel = -1;
  if (!realpath(".", fc->curdir)) snprintf(fc->curdir, sizeof fc->curdir, "/");
  read_dir(fc);
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(fc));
  return fc;
}

void gtcaca_filechooser_set_options(gtcaca_filechooser_widget_t *fc,
                                    const gtcaca_fc_option_t *opts, int nopts)
{
  int i;
  if (!fc) return;
  if (nopts < 0) nopts = 0;
  if (nopts > GTCACA_FC_MAX_OPTS) nopts = GTCACA_FC_MAX_OPTS;
  for (i = 0; i < nopts; i++) {
    snprintf(fc->opt[i].label, sizeof fc->opt[i].label, "%s",
             opts[i].label ? opts[i].label : "");
    fc->opt[i].state = opts[i].initial ? 1 : 0;
  }
  fc->nopt = nopts;
  fc->opt_sel = -1;
}

void gtcaca_filechooser_draw(gtcaca_filechooser_widget_t *fc)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = fc->x + 1, inner_y = fc->y + 1, inner_w = fc->width - 2, inner_h = fc->height - 2;
  int listy = inner_y, rows, i;
  const char *t = fc->title ? fc->title : (fc->mode == GTCACA_FILECHOOSER_SAVE ? "Save File" : "Open File");

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, fc->x, fc->y, fc->width, fc->height, ' ');
  caca_draw_cp437_box(gmo.cv, fc->x, fc->y, fc->width, fc->height);
  caca_set_color_ansi(gmo.cv, CACA_YELLOW, bg); caca_set_attr(gmo.cv, CACA_BOLD);
  caca_printf(gmo.cv, fc->x + 2, fc->y, "| %s |", t);
  caca_set_attr(gmo.cv, 0);

  /* current directory */
  caca_set_color_ansi(gmo.cv, CACA_LIGHTCYAN, bg);
  caca_printf(gmo.cv, inner_x, listy, "%-.*s", inner_w, fc->curdir);
  listy++;
  if (fc->mode == GTCACA_FILECHOOSER_SAVE) {
    caca_set_color_ansi(gmo.cv, fg, bg);
    caca_printf(gmo.cv, inner_x, listy, "Name: ");
    caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLUE);
    caca_printf(gmo.cv, inner_x + 6, listy, "%-.*s", inner_w - 6, fc->name);
    listy++;
  }
  /* search prompt (type-to-filter), or a key hint when the dialog has options */
  if (fc->searching) {
    caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_YELLOW);
    caca_printf(gmo.cv, inner_x, listy, "/%-.*s", inner_w - 1, fc->search);
    caca_set_color_ansi(gmo.cv, fg, bg);
  } else if (fc->mode == GTCACA_FILECHOOSER_SAVE && fc->nopt > 0) {
    caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, bg);
    caca_printf(gmo.cv, inner_x, listy, "%-.*s", inner_w,
                "Tab: options   Space: toggle   Enter: save");
    caca_set_color_ansi(gmo.cv, fg, bg);
  }
  listy++;   /* separator / search / hint row */

  /* reserve the bottom rows for the option checkboxes (save mode) */
  {
    int optrows = (fc->mode == GTCACA_FILECHOOSER_SAVE) ? fc->nopt : 0;
    rows = inner_y + inner_h - listy - optrows;
    if (rows < 1) rows = 1;
    if (fc->sel < fc->top) fc->top = fc->sel;
    if (fc->sel >= fc->top + rows) fc->top = fc->sel - rows + 1;
    for (i = 0; i < rows && fc->top + i < fc->nfilt; i++) {
      gtcaca_fc_entry_t *e = &fc->entries[fc->filt[fc->top + i]];
      int sel = (fc->top + i == fc->sel && fc->opt_sel < 0);
      char buf[PATH_MAX];
      snprintf(buf, sizeof buf, "%s%s", e->name, e->isdir ? "/" : "");
      if (sel) { caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN); caca_set_attr(gmo.cv, CACA_BOLD); }
      else { caca_set_color_ansi(gmo.cv, e->isdir ? CACA_LIGHTBLUE : fg, bg); caca_set_attr(gmo.cv, 0); }
      caca_printf(gmo.cv, inner_x, listy + i, "%-.*s", inner_w, buf);
    }
    caca_set_attr(gmo.cv, 0);

    if (optrows > 0) {
      int oy = inner_y + inner_h - optrows, k;
      for (k = 0; k < fc->nopt; k++) {
        int foc = (fc->opt_sel == k);
        if (foc) { caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_CYAN); caca_set_attr(gmo.cv, CACA_BOLD); }
        else     { caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0); }
        caca_printf(gmo.cv, inner_x, oy + k, "[%c] %-.*s",
                    fc->opt[k].state ? 'x' : ' ', inner_w - 4, fc->opt[k].label);
      }
      caca_set_attr(gmo.cv, 0);
    }
  }
}

static void enter_dir(gtcaca_filechooser_widget_t *fc, const char *name)
{
  char joined[PATH_MAX], resolved[PATH_MAX];
  snprintf(joined, sizeof joined, "%s/%s", fc->curdir, name);
  if (realpath(joined, resolved)) { snprintf(fc->curdir, sizeof fc->curdir, "%s", resolved); read_dir(fc); }
}

int gtcaca_filechooser_key(gtcaca_filechooser_widget_t *fc, int key, void *userdata)
{
  gtcaca_fc_entry_t *e;
  (void)userdata;
  /* current entry resolves through the filter map */
  e = (fc->sel >= 0 && fc->sel < fc->nfilt) ? &fc->entries[fc->filt[fc->sel]] : NULL;

  /* Tab cycles focus between the file area (-1) and each option checkbox. */
  if (key == '\t' && fc->nopt > 0) {
    fc->opt_sel = (fc->opt_sel + 1 >= fc->nopt) ? -1 : fc->opt_sel + 1;
    return 1;
  }
  /* While a checkbox is focused, only Space (toggle), Enter (commit) and Esc
     act; everything else is swallowed so it can't disturb the name/list. */
  if (fc->opt_sel >= 0) {
    if (key == ' ') { fc->opt[fc->opt_sel].state = !fc->opt[fc->opt_sel].state; return 1; }
    if (key == CACA_KEY_RETURN || key == 10) {
      if (fc->name[0]) { snprintf(fc->chosen, sizeof fc->chosen, "%s/%s", fc->curdir, fc->name); fc->result = 1; }
      return 1;
    }
    if (key == CACA_KEY_ESCAPE) { fc->result = 0; return 1; }
    return 1;
  }

  /* '/' starts type-to-search in open mode (in save mode it's a path char). */
  if (key == '/' && fc->mode != GTCACA_FILECHOOSER_SAVE && !fc->searching) {
    fc->searching = 1; fc->search[0] = '\0';
    rebuild_filter(fc);
    return 1;
  }

  switch (key) {
  case CACA_KEY_UP:   if (fc->sel > 0) fc->sel--; return 1;
  case CACA_KEY_DOWN: if (fc->sel < fc->nfilt - 1) fc->sel++; return 1;
  case CACA_KEY_PAGEUP:   fc->sel -= 10; if (fc->sel < 0) fc->sel = 0; return 1;
  case CACA_KEY_PAGEDOWN: fc->sel += 10; if (fc->sel > fc->nfilt - 1) fc->sel = fc->nfilt - 1; return 1;
  case CACA_KEY_ESCAPE:
    if (fc->searching) {             /* first Esc exits search, not the dialog */
      fc->searching = 0; fc->search[0] = '\0'; rebuild_filter(fc);
    } else {
      fc->result = 0;
    }
    return 1;
  case CACA_KEY_RETURN: case 10:
    if (fc->mode == GTCACA_FILECHOOSER_SAVE) {
      /* A typed name commits the save (possibly a brand-new file); with an
         empty name field, Enter navigates into a highlighted directory so the
         user can pick where to save. */
      if (fc->name[0]) { snprintf(fc->chosen, sizeof fc->chosen, "%s/%s", fc->curdir, fc->name); fc->result = 1; }
      else if (e && e->isdir) { enter_dir(fc, e->name); }
      return 1;
    }
    if (e && e->isdir) { enter_dir(fc, e->name); return 1; }   /* read_dir clears search */
    if (e) { snprintf(fc->chosen, sizeof fc->chosen, "%s/%s", fc->curdir, e->name); fc->result = 1; }
    return 1;
  default:
    if (fc->searching) {              /* edit the search query */
      int n = (int)strlen(fc->search);
      if ((key == CACA_KEY_BACKSPACE || key == CACA_KEY_DELETE)) {
        if (n > 0) fc->search[--n] = '\0';
        else { fc->searching = 0; }   /* backspace on empty query leaves search */
        rebuild_filter(fc);
        return 1;
      }
      if (key >= 32 && key < 127 && n < (int)sizeof fc->search - 1) {
        fc->search[n] = (char)key; fc->search[n + 1] = '\0';
        rebuild_filter(fc);
        return 1;
      }
      return 1;
    }
    if (fc->mode == GTCACA_FILECHOOSER_SAVE) {        /* edit the name field */
      int n = (int)strlen(fc->name);
      if ((key == CACA_KEY_BACKSPACE || key == CACA_KEY_DELETE) && n > 0) { fc->name[--n] = '\0'; return 1; }
      if (key >= 32 && key < 127 && n < (int)sizeof fc->name - 1) { fc->name[n] = (char)key; fc->name[n + 1] = '\0'; return 1; }
    }
    return 0;
  }
}

void gtcaca_filechooser_free(gtcaca_filechooser_widget_t *fc)
{
  if (!fc) return;
  CDL_DELETE(gmo.widgets_list, GTCACA_WIDGET(fc));
  clear_entries(fc); free(fc->entries); free(fc->filt); free(fc->title); free(fc);
}

int gtcaca_filechooser_run_opts(const char *start_dir, char *out_path, int outlen,
                                int save_mode, const gtcaca_fc_option_t *opts,
                                int nopts, int *states_out)
{
  int cw = caca_get_canvas_width(gmo.cv), chh = caca_get_canvas_height(gmo.cv);
  int w = cw * 3 / 4, h = chh * 3 / 4, res, i;
  gtcaca_filechooser_widget_t *fc;
  caca_event_t ev;
  if (w < 30) w = cw - 2; if (h < 8) h = chh - 2;

  fc = gtcaca_filechooser_new(NULL, (cw - w) / 2, (chh - h) / 2, w, h);
  if (!fc) return 0;
  fc->mode = save_mode ? GTCACA_FILECHOOSER_SAVE : GTCACA_FILECHOOSER_OPEN;
  if (save_mode && opts && nopts > 0) gtcaca_filechooser_set_options(fc, opts, nopts);
  gtcaca_filechooser_set_dir(fc, start_dir);

  while (fc->result == -100) {
    int k;
    gtcaca_redraw();
    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    k = caca_get_event_key_ch(&ev);
    /* auto-fill the save name when highlighting a file (file area focused) */
    if (save_mode && fc->opt_sel < 0 && (k == CACA_KEY_UP || k == CACA_KEY_DOWN)) {
      gtcaca_filechooser_key(fc, k, NULL);
      if (fc->sel < fc->nfilt && !fc->entries[fc->filt[fc->sel]].isdir)
        snprintf(fc->name, sizeof fc->name, "%s", fc->entries[fc->filt[fc->sel]].name);
      continue;
    }
    gtcaca_filechooser_key(fc, k, NULL);
  }
  res = fc->result;
  if (res == 1 && out_path) snprintf(out_path, (size_t)outlen, "%s", fc->chosen);
  if (res == 1 && states_out)
    for (i = 0; i < fc->nopt; i++) states_out[i] = fc->opt[i].state;
  gtcaca_filechooser_free(fc);
  gtcaca_redraw();
  return res;
}

int gtcaca_filechooser_run(const char *start_dir, char *out_path, int outlen, int save_mode)
{
  return gtcaca_filechooser_run_opts(start_dir, out_path, outlen, save_mode, NULL, 0, NULL);
}
