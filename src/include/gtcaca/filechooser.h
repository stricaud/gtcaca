#ifndef _GTCACA_FILECHOOSER_H_
#define _GTCACA_FILECHOOSER_H_

#include <stdint.h>
#include <limits.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* ── Modal file chooser ──────────────────────────────────────────────────────
 *
 * A directory browser (folders first) used to pick a file to open or a path to
 * save to. Up/Down move, Enter descends into a folder or chooses a file, Esc
 * cancels. In save mode a name field at the top is editable. Typically driven
 * by the blocking gtcaca_filechooser_run(). */

enum { GTCACA_FILECHOOSER_OPEN = 0, GTCACA_FILECHOOSER_SAVE = 1 };

#define GTCACA_FC_MAX_OPTS 6

typedef struct { char *name; int isdir; } gtcaca_fc_entry_t;

/* An optional labelled checkbox shown in the save dialog (e.g. "Only save
   displayed packets"). `initial` is the starting state; after the dialog runs,
   the chosen states are reported back through gtcaca_filechooser_run_opts(). */
typedef struct { const char *label; int initial; } gtcaca_fc_option_t;

typedef struct _gtcaca_filechooser_widget_t gtcaca_filechooser_widget_t;
struct _gtcaca_filechooser_widget_t {
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
  struct _gtcaca_filechooser_widget_t *prev;
  struct _gtcaca_filechooser_widget_t *next;

  char  curdir[PATH_MAX];
  gtcaca_fc_entry_t *entries;
  int    nentries, cap;
  int    sel, top;
  int    mode;                 /* GTCACA_FILECHOOSER_OPEN / _SAVE */
  char   name[256];            /* edited name in save mode        */
  char  *title;
  int    result;               /* -100 ongoing, 1 chosen, 0 cancelled */
  char   chosen[PATH_MAX];

  /* type-to-search ('/' to start): filters the list to matching names.
     sel/top index into filt[] (the matching entries) when active. */
  char   search[128];
  int    searching;
  int   *filt;
  int    nfilt, filt_cap;

  /* optional save-dialog checkboxes (Tab cycles focus, Space toggles). */
  struct { char label[80]; int state; } opt[GTCACA_FC_MAX_OPTS];
  int    nopt;

  /* Which control has keyboard focus. Tab cycles through them:
       0            = file list   (arrows navigate, Enter descends/picks)
       1            = name field  (save mode; type to edit, Enter saves)
       2 .. 2+nopt-1= checkbox    (index = focus - 2; Space toggles)          */
  int    focus;
};

enum { GTCACA_FC_FOCUS_LIST = 0, GTCACA_FC_FOCUS_NAME = 1, GTCACA_FC_FOCUS_OPT = 2 };

gtcaca_filechooser_widget_t *gtcaca_filechooser_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_filechooser_set_dir(gtcaca_filechooser_widget_t *fc, const char *dir);
void gtcaca_filechooser_draw(gtcaca_filechooser_widget_t *fc);
int  gtcaca_filechooser_key(gtcaca_filechooser_widget_t *fc, int key, void *userdata);
void gtcaca_filechooser_free(gtcaca_filechooser_widget_t *fc);

void gtcaca_filechooser_set_options(gtcaca_filechooser_widget_t *fc,
                                    const gtcaca_fc_option_t *opts, int nopts);

/* blocking: returns 1 and fills out_path if a file was chosen, 0 if cancelled */
int  gtcaca_filechooser_run(const char *start_dir, char *out_path, int outlen, int save_mode);

/* Like gtcaca_filechooser_run() but shows `nopts` checkboxes (save mode). On a
   successful choice, states_out[i] (if non-NULL) receives each checkbox state. */
int  gtcaca_filechooser_run_opts(const char *start_dir, char *out_path, int outlen,
                                 int save_mode, const gtcaca_fc_option_t *opts,
                                 int nopts, int *states_out);

/* Save dialog with the name field pre-filled to `default_name`. */
int  gtcaca_filechooser_run_named(const char *start_dir, const char *default_name,
                                  char *out_path, int outlen);

#endif /* _GTCACA_FILECHOOSER_H_ */
