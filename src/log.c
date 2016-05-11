#include <stdio.h>
#include <stdlib.h>

#include <gtcaca/main.h>
#include <gtcaca/log.h>

gtcaca_log_t *gtcaca_log_init(void)
{
  gtcaca_log_t *log;

  log = malloc(sizeof(gtcaca_log_t));
  if (!log) {
    fprintf(stderr, "Cannot initialize logging mechanism!\n");
    return NULL;
  }

  log->filename = "/tmp/gtcaca.log";
  log->fp = fopen(log->filename, "a");

  return log;
}

void gtcaca_log_write(char *fmt, ...)
{
  va_list ap;
  
  va_start(ap, fmt);
  vfprintf(gmo.log->fp, fmt, ap);
  va_end(ap);

  fflush(gmo.log->fp);
}

void gtcaca_log_close(void)
{
  fclose(gmo.log->fp);
}
