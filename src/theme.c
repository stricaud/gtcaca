#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <gtcaca/iniparse.h>
#include <gtcaca/theme.h>

int gtcaca_theme_parse_ini(char *theme)
{
  char *theme_fullpath;
  ini_t *ini;
  int count;
  
  if (!theme) { theme = "default"; }

  asprintf(&theme_fullpath, "%s/themes/%s", theme);

  ini = ini_parse_file(theme_fullpath);

  for (count = 0; count < ini->n_items; count += 2) {
    char *k = ini->keyvals[count];
    char *v = ini->keyvals[count+1];   
    
    printf("keys=[%s] vals=[%s]\n", k, v);
  }

  
  free(theme_fullpath);
  ini_free(ini);
  return 0;
}
