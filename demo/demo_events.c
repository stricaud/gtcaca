/*
 * demo_events — raw input-event inspector. Shows exactly what libcaca reports
 * for every event (keys, mouse press/release, wheel, motion, resize) so you can
 * read off the real values your terminal delivers. Quit with Ctrl-C.
 *
 * Mouse wheel, if your terminal/libcaca reports it, shows up as a MOUSE press
 * with button 4 (up) / 5 (down) — that's what to look for.
 */
#include <stdio.h>
#include <string.h>
#include <caca.h>

#define NLOG 36

int main(int argc, char **argv)
{
  caca_display_t *dp = caca_create_display(NULL);
  caca_canvas_t *cv;
  caca_event_t ev;
  char log[NLOG][128];
  int nlog = 0, running = 1;
  long motions = 0;
  /* "sgr" mode: turn OFF libcaca's (broken) mouse decoder and enable the xterm
     SGR mouse protocol ourselves, so we can see whether the raw ESC[<b;x;y;M
     reports arrive as plain key events (which we could then parse like k9s). */
  int sgr = (argc > 1 && strcmp(argv[1], "sgr") == 0);

  if (!dp) { fprintf(stderr, "cannot create display\n"); return 1; }
  cv = caca_get_canvas(dp);
  caca_set_display_title(dp, "gtcaca event inspector");
  if (sgr) { caca_set_mouse(dp, 0); fputs("\033[?1000h\033[?1006h", stdout); fflush(stdout); }
  else       caca_set_mouse(dp, 1);

  while (running) {
    int H = caca_get_canvas_height(cv), i, t;
    char line[128] = "";

    caca_set_color_ansi(cv, CACA_WHITE, CACA_BLACK); caca_clear_canvas(cv);
    caca_set_color_ansi(cv, CACA_BLACK, CACA_CYAN);
    caca_printf(cv, 0, 0, " Input event inspector   press keys / click / scroll the wheel   (Ctrl-C quits) ");
    caca_set_color_ansi(cv, CACA_YELLOW, CACA_BLACK);
    caca_printf(cv, 0, 1, " motion events: %ld     tracked mouse pos: (%d, %d)  <- move the mouse, does this update?",
                motions, caca_get_mouse_x(dp), caca_get_mouse_y(dp));
    caca_set_color_ansi(cv, CACA_LIGHTGRAY, CACA_BLACK);
    for (i = 0; i < nlog && 3 + i < H; i++) caca_printf(cv, 0, 3 + i, "%s", log[i]);
    caca_refresh_display(dp);

    if (!caca_get_event(dp, CACA_EVENT_ANY, &ev, -1)) continue;
    t = caca_get_event_type(&ev);

    if (t & CACA_EVENT_KEY_PRESS) {
      int ch = caca_get_event_key_ch(&ev);
      unsigned long u = (unsigned long)caca_get_event_key_utf32(&ev);
      char cbuf[8] = "";
      if (ch >= 32 && ch < 127) snprintf(cbuf, sizeof cbuf, "'%c'", ch);
      snprintf(line, sizeof line, "KEY    press    ch=%-4d 0x%02x  utf32=0x%lx  %s", ch, ch & 0xff, u, cbuf);
      if (ch == 3 /* Ctrl-C */) running = 0;
    } else if (t & CACA_EVENT_KEY_RELEASE) {
      snprintf(line, sizeof line, "KEY    release  ch=%d", caca_get_event_key_ch(&ev));
    } else if (t & CACA_EVENT_MOUSE_PRESS) {
      snprintf(line, sizeof line, "MOUSE  press    x=%-3d y=%-3d button=%d",
               caca_get_event_mouse_x(&ev), caca_get_event_mouse_y(&ev), caca_get_event_mouse_button(&ev));
    } else if (t & CACA_EVENT_MOUSE_RELEASE) {
      snprintf(line, sizeof line, "MOUSE  release  x=%-3d y=%-3d button=%d",
               caca_get_event_mouse_x(&ev), caca_get_event_mouse_y(&ev), caca_get_event_mouse_button(&ev));
    } else if (t & CACA_EVENT_MOUSE_MOTION) {
      motions++;
      continue;                         /* counted above; don't flood the log */
    } else if (t & CACA_EVENT_RESIZE) {
      snprintf(line, sizeof line, "RESIZE w=%d h=%d",
               caca_get_event_resize_width(&ev), caca_get_event_resize_height(&ev));
    } else {
      snprintf(line, sizeof line, "OTHER  type=0x%x", t);
    }

    if (nlog < NLOG) strcpy(log[nlog++], line);
    else { for (i = 1; i < NLOG; i++) strcpy(log[i - 1], log[i]); strcpy(log[NLOG - 1], line); }
  }
  if (sgr) { fputs("\033[?1000l\033[?1006l", stdout); fflush(stdout); }
  caca_free_display(dp);
  return 0;
}
