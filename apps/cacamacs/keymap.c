#include "cacamacs.h"
/* ── keyboard macros & numeric prefix ──────────────────────────────────────── */

 int on_key(gtcaca_editor_widget_t *ed, int key, void *ud);

/* Mirror the editor widget's default text handling for keys on_key ignores,
   so replaying a recorded key reproduces exactly what typing it would do. */
void editor_default_key(gtcaca_editor_widget_t *ed, int key)
{
  switch (key) {
  case CACA_KEY_LEFT:     gtcaca_editor_char_left(ed);  break;
  case CACA_KEY_RIGHT:    gtcaca_editor_char_right(ed); break;
  case CACA_KEY_UP:       gtcaca_editor_line_up(ed);    break;
  case CACA_KEY_DOWN:     gtcaca_editor_line_down(ed);  break;
  case CACA_KEY_HOME:     gtcaca_editor_home(ed);       break;
  case CACA_KEY_END:      gtcaca_editor_line_end(ed);   break;
  case CACA_KEY_PAGEUP:   gtcaca_editor_page_up(ed);    break;
  case CACA_KEY_PAGEDOWN: gtcaca_editor_page_down(ed);  break;
  case CACA_KEY_RETURN: case 10:                        gtcaca_editor_new_line(ed);    break;
  case CACA_KEY_BACKSPACE: case CACA_KEY_DELETE:        gtcaca_editor_delete_back(ed); break;
  case CACA_KEY_TAB:      gtcaca_editor_add_char(ed, '\t'); break;
  default: if (key >= 32 && key <= 126) gtcaca_editor_add_char(ed, (char)key); break;
  }
}

/* Feed one key through the same path the main loop uses for the focused editor. */
void macro_feed(gtcaca_editor_widget_t *ed, int key)
{
  if (!on_key(ed, key, NULL)) editor_default_key(ed, key);
}

void macro_execute(gtcaca_editor_widget_t *ed)
{
  int i;
  if (g_macro_len == 0) { snprintf(g_message, sizeof g_message, "No keyboard macro defined"); return; }
  g_macro_executing = 1;
  gtcaca_editor_begin_undo_action(ed);   /* one macro run = one undo step */
  for (i = 0; i < g_macro_len; i++) macro_feed(ed, g_macro[i]);
  gtcaca_editor_end_undo_action(ed);
  g_macro_executing = 0;
}

/* Consume the armed numeric prefix, returning the repeat count (default 1, or 4
   for a bare C-u with no digits). */
int take_prefix(void)
{
  int n = g_prefix_pending ? (g_prefix_have ? (int)g_prefix_arg : 4) : 1;
  g_prefix_pending = 0; g_prefix_have = 0; g_prefix_arg = 0;
  return n < 1 ? 1 : n;
}

/* ── Emacs keymap ──────────────────────────────────────────────────────────── */

int on_key(gtcaca_editor_widget_t *ed, int key, void *ud)
{
  (void)ud;

  /* record into the keyboard macro (the C-x ( / C-x ) control keys are trimmed
     by the handlers below; replayed keys are not re-recorded) */
  if (g_macro_recording && !g_macro_executing && g_macro_len < MACRO_MAX)
    g_macro[g_macro_len++] = key;

  /* if focus landed on a different pane's editor, sync our notion of focus */
  if (ed != g_ed && ed != g_browser_ed && ed != g_help_ed) {
    int bi = buf_index_of(ed);
    if (bi >= 0 && g_buffers[bi].pane >= 0) focus_pane(g_buffers[bi].pane);
  }

  /* incremental search and the minibuffer take keys first */
  if (g_isearch)   return isearch_key(ed, key);
  if (g_mb_active) return minibuffer_key(key);

  /* help viewer: read-only and scrollable — q/Esc close it, C-s searches, and
     arrows / PageUp / PageDown / C-v fall through to the editor for scrolling. */
  if (ed == g_help_ed) {
    if (key == 'q' || key == CACA_KEY_ESCAPE) { hide_help(); return 1; }
    if (key == CACA_KEY_CTRL_X) return 1;   /* swallow C-x */
  }

  /* browser mode: the listing is a read-only editor — Enter opens the entry,
     q/Esc close it, and everything else (arrows, C-s search, …) falls through
     to the normal editor handling (edits are no-ops because it is read-only). */
  if (ed == g_browser_ed) {
    if (key == CACA_KEY_RETURN || key == 10) { browser_open_current(); return 1; }
    if (key == 'q' || key == CACA_KEY_ESCAPE) { hide_browser(); return 1; }
    if (key == CACA_KEY_CTRL_X) return 1;   /* swallow C-x (no save/quit chords here) */
  }

  /* third key of a C-x r rectangle chord */
  if (g_rect_prefix) {
    g_rect_prefix = 0;
    switch (key) {
    case 't': start_string_rectangle();      return 1;  /* C-x r t string-rectangle */
    case 'k': gtcaca_editor_rect_kill(ed);   g_mark_active = 0;
              snprintf(g_message, sizeof g_message, "Killed rectangle");  return 1;  /* C-x r k */
    case 'y': gtcaca_editor_rect_yank(ed);
              snprintf(g_message, sizeof g_message, "Yanked rectangle");  return 1;  /* C-x r y */
    case 'd': gtcaca_editor_rect_delete(ed); g_mark_active = 0;
              snprintf(g_message, sizeof g_message, "Deleted rectangle"); return 1;  /* C-x r d */
    case 'c': gtcaca_editor_rect_clear(ed);  g_mark_active = 0;
              snprintf(g_message, sizeof g_message, "Cleared rectangle"); return 1;  /* C-x r c */
    default:
      snprintf(g_message, sizeof g_message, "C-x r %c is undefined", key >= 32 ? key : '?');
      return 1;
    }
  }

  /* second key of a C-x chord */
  if (g_ctrl_x) {
    int reps = take_prefix();   /* consumed here; only C-x e uses the count */
    g_ctrl_x = 0;
    switch (key) {
    case '(':                                                        /* C-x ( start macro */
      g_macro_len = 0; g_macro_recording = 1;
      snprintf(g_message, sizeof g_message, "Defining keyboard macro...");
      return 1;
    case ')':                                                        /* C-x ) end macro */
      if (g_macro_recording) {
        if (g_macro_len >= 2) g_macro_len -= 2;   /* drop the trailing C-x ) */
        g_macro_recording = 0;
        snprintf(g_message, sizeof g_message, "Keyboard macro defined (%d keys)", g_macro_len);
      }
      return 1;
    case 'e': {                                                      /* C-x e run macro */
      int i; for (i = 0; i < reps; i++) macro_execute(ed);
      return 1;
    }
    case KEY_CTRL_S:        save_file(ed);                return 1;  /* C-x C-s */
    case CACA_KEY_CTRL_W:   start_save_as();              return 1;  /* C-x C-w write-file */
    case CACA_KEY_CTRL_F:   start_find_file();            return 1;  /* C-x C-f */
    case CACA_KEY_CTRL_C:   gtcaca_main_quit();           return 1;  /* C-x C-c */
    case CACA_KEY_CTRL_X:   exchange_point_and_mark(ed);  return 1;  /* C-x C-x */
    case 'u':               gtcaca_editor_undo(ed); g_mark_active = 0; return 1; /* C-x u */
    case 'l':               toggle_line_numbers(ed);      return 1;  /* C-x l */
    case 'f':               toggle_folding(ed);           return 1;  /* C-x f */
    case 't':               toggle_fold_here(ed);         return 1;  /* C-x t */
    case 'a':               toggle_annotation_here(ed);   return 1;  /* C-x a */
    case 'd':               show_browser();               return 1;  /* C-x d (dired) */
    case 'p':               pretty_print_json(ed);        return 1;  /* C-x p */
    case 'r':               g_rect_prefix = 1;                        /* C-x r … rectangle */
                            snprintf(g_message, sizeof g_message, "C-x r-"); return 1;
    case ' ':               set_rectangle_mark(ed);       return 1;  /* C-x SPC rect mark */
    case '2':               pane_split(1);                return 1;  /* C-x 2 split below */
    case '3':               pane_split(0);                return 1;  /* C-x 3 split right */
    case '1':               pane_unsplit_one();           return 1;  /* C-x 1 one window */
    case '0':               pane_delete_current();        return 1;  /* C-x 0 delete window */
    case 'o':               pane_other_window();          return 1;  /* C-x o other window */
    case 'b':               pane_switch_buffer();         return 1;  /* C-x b switch buffer */
    case CACA_KEY_CTRL_T:   gtcaca_editor_line_transpose(ed); return 1;  /* C-x C-t transpose lines */
    case CACA_KEY_CTRL_L:   toggle_wrap(ed);              return 1;  /* C-x C-l toggle line wrap */
    case CACA_KEY_CTRL_Q:
      gtcaca_editor_set_read_only(ed, !gtcaca_editor_get_read_only(ed));
      snprintf(g_message, sizeof g_message, "Read-only %s", gtcaca_editor_get_read_only(ed) ? "on" : "off");
      return 1;
    case 'w':
      gtcaca_editor_set_view_whitespace(ed, !gtcaca_editor_get_view_whitespace(ed));
      snprintf(g_message, sizeof g_message, "Whitespace %s", gtcaca_editor_get_view_whitespace(ed) ? "shown" : "hidden");
      return 1;
    default:
      snprintf(g_message, sizeof(g_message), "C-x %c is undefined", key >= 32 ? key : '?');
      return 1;
    }
  }

  /* second key of a Meta (Esc) chord — Esc then a key acts as M-<key> */
  if (g_meta) {
    (void)take_prefix();   /* consume any armed count (M- commands run once) */
    g_meta = 0;
    switch (key) {
    case '%':               start_replace();   return 1;  /* M-% query replace */
    case 'w': case 'W':     copy_region(ed);   return 1;  /* M-w copy region   */
    case 'f': case 'F':     move(ed, gtcaca_editor_word_right, gtcaca_editor_word_right_extend); return 1; /* M-f */
    case 'b': case 'B':     move(ed, gtcaca_editor_word_left,  gtcaca_editor_word_left_extend);  return 1; /* M-b */
    case 'd': case 'D':     kill_word_right(ed); return 1;  /* M-d  kill word fwd  */
    case CACA_KEY_BACKSPACE:
    case CACA_KEY_DELETE:   kill_word_left(ed);  return 1;  /* M-DEL kill word back */
    case 'u': case 'U':     case_word(ed, 1);    return 1;  /* M-u  upcase word    */
    case 'l': case 'L':     case_word(ed, 0);    return 1;  /* M-l  downcase word  */
    case ';':               comment_dwim(ed);    return 1;  /* M-;  comment-dwim   */
    case 'x': case 'X':     start_mx();          return 1;  /* M-x  run command    */
    case '<':               move(ed, gtcaca_editor_document_start, gtcaca_editor_document_start_extend); return 1; /* M-< */
    case '>':               move(ed, gtcaca_editor_document_end,   gtcaca_editor_document_end_extend);   return 1; /* M-> */
    case CACA_KEY_ESCAPE:   keyboard_quit(ed); return 1;  /* Esc Esc cancels   */
    default:
      snprintf(g_message, sizeof(g_message), "M-%c is undefined", key >= 32 ? key : '?');
      return 1;
    }
  }

  /* numeric prefix argument: C-u [digits] command (e.g. C-u 100 C-x e) */
  if (g_prefix_reading) {
    if (key >= '0' && key <= '9') {
      g_prefix_arg = (g_prefix_have ? g_prefix_arg : 0) * 10 + (key - '0');
      g_prefix_have = 1;
      snprintf(g_message, sizeof g_message, "C-u %ld-", g_prefix_arg);
      return 1;
    }
    if (key == CACA_KEY_CTRL_U) {                 /* C-u C-u … multiplies by 4 */
      g_prefix_arg = (g_prefix_have ? g_prefix_arg : 1) * 4; g_prefix_have = 1;
      snprintf(g_message, sizeof g_message, "C-u %ld-", g_prefix_arg);
      return 1;
    }
    g_prefix_reading = 0; g_prefix_pending = 1;   /* count armed; this key is the command */
  }
  /* a count is armed: repeat this command (chord/meta starters pass through and
     consume it in their own handlers). macro_feed handles every key kind —
     self-insert, motion, edit — exactly as typing it would. */
  if (g_prefix_pending && key != CACA_KEY_CTRL_X && key != CACA_KEY_ESCAPE) {
    int n = take_prefix(), i, save = g_macro_executing;
    g_macro_executing = 1;              /* don't double-record the repeats */
    gtcaca_editor_begin_undo_action(ed); /* C-u N <cmd> = one undo step */
    for (i = 0; i < n; i++) macro_feed(ed, key);
    gtcaca_editor_end_undo_action(ed);
    g_macro_executing = save;
    return 1;
  }

  g_message[0] = '\0';

  switch (key) {
  case CACA_KEY_CTRL_X: g_ctrl_x = 1; return 1;
  case CACA_KEY_ESCAPE: g_meta = 1; snprintf(g_message, sizeof g_message, "ESC-"); return 1;
  case CACA_KEY_CTRL_U:                                  /* C-u universal prefix */
    g_prefix_reading = 1; g_prefix_have = 0; g_prefix_arg = 4;
    snprintf(g_message, sizeof g_message, "C-u-");
    return 1;

  /* search */
  case KEY_CTRL_S:      start_isearch(ed, 1);  return 1;   /* C-s */
  case CACA_KEY_CTRL_R: start_isearch(ed, -1); return 1;   /* C-r */

  /* misc editing */
  case CACA_KEY_CTRL_T: transpose_chars(ed);   return 1;   /* C-t transpose chars */
  case CACA_KEY_INSERT:
    gtcaca_editor_set_overtype(ed, !gtcaca_editor_get_overtype(ed));
    snprintf(g_message, sizeof g_message, "Overtype %s", gtcaca_editor_get_overtype(ed) ? "on" : "off");
    return 1;

  /* motion */
  case CACA_KEY_CTRL_F: move(ed, gtcaca_editor_char_right, gtcaca_editor_char_right_extend); return 1;
  case CACA_KEY_CTRL_B: move(ed, gtcaca_editor_char_left,  gtcaca_editor_char_left_extend);  return 1;
  case CACA_KEY_CTRL_N: move(ed, gtcaca_editor_line_down,  gtcaca_editor_line_down_extend);  return 1;
  case CACA_KEY_CTRL_P: move(ed, gtcaca_editor_line_up,    gtcaca_editor_line_up_extend);    return 1;
  case CACA_KEY_CTRL_A: move(ed, gtcaca_editor_home,       gtcaca_editor_home_extend);       return 1;
  case CACA_KEY_CTRL_E: move(ed, gtcaca_editor_line_end,   gtcaca_editor_line_end_extend);   return 1;
  case CACA_KEY_CTRL_V: move(ed, gtcaca_editor_page_down,  gtcaca_editor_page_down_extend);  return 1;

  /* editing */
  case CACA_KEY_CTRL_D: gtcaca_editor_clear(ed);      return 1;  /* delete forward */
  case CACA_KEY_CTRL_K: kill_line(ed);                return 1;
  case CACA_KEY_CTRL_W: kill_region(ed);              return 1;
  case CACA_KEY_CTRL_Y: yank(ed);                     return 1;

  /* region / misc */
  case KEY_CTRL_SPACE:  set_mark(ed);                 return 1;
  case CACA_KEY_CTRL_G: keyboard_quit(ed);            return 1;
  case CACA_KEY_CTRL_L: recenter(ed);                 return 1;
  case KEY_UNDO:        gtcaca_editor_undo(ed); g_mark_active = 0; return 1;

  /* Tab: complete the word before the caret, otherwise indent (per config). */
  case CACA_KEY_TAB: {
    int pos = gtcaca_editor_get_current_pos(ed);
    if (pos > 0 && is_word_char(gtcaca_editor_get_char_at(ed, pos - 1)))
      complete_at_point(ed);
    else if (g_insert_spaces) { int k; for (k = 0; k < g_indent_size; k++) gtcaca_editor_add_char(ed, ' '); }
    else gtcaca_editor_add_char(ed, '\t');
    return 1;
  }

  /* When a mark is active, the plain arrow/Home/End keys extend the region. */
  case CACA_KEY_LEFT:  if (g_mark_active) { gtcaca_editor_char_left_extend(ed);  return 1; } return 0;
  case CACA_KEY_RIGHT: if (g_mark_active) { gtcaca_editor_char_right_extend(ed); return 1; } return 0;
  case CACA_KEY_UP:    if (g_mark_active) { gtcaca_editor_line_up_extend(ed);    return 1; } return 0;
  case CACA_KEY_DOWN:  if (g_mark_active) { gtcaca_editor_line_down_extend(ed);  return 1; } return 0;
  case CACA_KEY_HOME:  if (g_mark_active) { gtcaca_editor_home_extend(ed);       return 1; } return 0;
  case CACA_KEY_END:   if (g_mark_active) { gtcaca_editor_line_end_extend(ed);   return 1; } return 0;

  default:
    return 0;  /* printable chars, Enter, Backspace: handled by the widget */
  }
}

