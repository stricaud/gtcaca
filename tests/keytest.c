/*
 * keytest — diagnose whether caca_get_event delivers keyboard events.
 *
 * Run it, press a few keys (arrows, letters, Enter, Esc), wait 5 seconds.
 * Then cat /tmp/keytest.log to see what was recorded.
 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <caca.h>

int main(void)
{
    caca_display_t *dp;
    caca_canvas_t  *cv;
    caca_event_t    ev;
    FILE           *log;
    time_t          deadline;
    int             count = 0;

    cv = caca_create_canvas(80, 24);
    dp = caca_create_display(cv);
    if (!dp) {
        fprintf(stderr, "caca_create_display failed\n");
        return 1;
    }

    log = fopen("/tmp/keytest.log", "w");
    if (log) {
        fprintf(log, "driver: %s\n", caca_get_display_driver(dp));
        fflush(log);
    }

    caca_set_color_ansi(cv, CACA_WHITE, CACA_BLACK);
    caca_printf(cv, 0, 0, "Press keys for 5 seconds. Results -> /tmp/keytest.log");
    caca_refresh_display(dp);

    deadline = time(NULL) + 5;
    while (time(NULL) < deadline) {
        while (caca_get_event(dp, CACA_EVENT_ANY, &ev, 0)) {
            int t = caca_get_event_type(&ev);
            if (log) {
                if (t & CACA_EVENT_KEY_PRESS) {
                    fprintf(log, "KEY_PRESS ch=0x%x (%d)\n",
                            caca_get_event_key_ch(&ev),
                            caca_get_event_key_ch(&ev));
                } else if (t & CACA_EVENT_KEY_RELEASE) {
                    fprintf(log, "KEY_RELEASE ch=0x%x\n",
                            caca_get_event_key_ch(&ev));
                } else if (t & CACA_EVENT_RESIZE) {
                    fprintf(log, "RESIZE\n");
                } else {
                    fprintf(log, "OTHER type=0x%x\n", t);
                }
                fflush(log);
            }
            count++;
        }
        usleep(10000);
    }

    if (log) {
        fprintf(log, "total events: %d\n", count);
        fclose(log);
    }

    caca_free_display(dp);
    caca_free_canvas(cv);
    return 0;
}
