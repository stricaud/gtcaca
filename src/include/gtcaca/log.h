#ifndef _GTCACA_LOG_H_
#define _GTCACA_LOG_H_

#include <stdio.h>
#include <stdarg.h>

struct _gtcaca_log_t {
  char *filename;
  FILE *fp;
};
typedef struct _gtcaca_log_t gtcaca_log_t;

gtcaca_log_t *gtcaca_log_init(void);
void gtcaca_log_write(char *fmt, ...);
void gtcaca_log_close(void);


#endif // _GTCACA_LOG_H_
