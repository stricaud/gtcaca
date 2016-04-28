#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtcaca/iniparse.h>

static long _get_file_size(char *filename)
{
  FILE *fp;
  long size;

  fp = fopen(filename, "r");
  fseek(fp, 0L, SEEK_END);
  size = ftell(fp);
  fseek(fp, 0L, SEEK_SET);
  fclose(fp);

  return size;
}

int get_keyvals_size(ini_t *ini) 
{
  int count;

  if (!ini->keyvals) return 0;
  for (count = 0; ini->keyvals[count] != NULL; count++) {}
  if (count == 0) return 0;
  return count / 2;
}

int get_keyvals_pos(ini_t *ini) 
{
  int count;

  if (!ini->keyvals) return 0;
  for (count = 0; ini->keyvals[count] != NULL; count++) {}
  return count;
}

int _add_section_key_value(ini_t *ini, char *section, char *key, char *value)
{
  char *gkey;
  int pos;

  asprintf(&gkey, "%s.%s", section, key);

  //  pos = get_keyvals_pos(ini);
  pos = ini->n_items;

  ini->keyvals_size = (sizeof(char *) * 2) + ini->keyvals_size;
  ini->keyvals = realloc(ini->keyvals, ini->keyvals_size);

  ini->keyvals[pos] = strdup(gkey);
  ini->keyvals[pos+1] = strdup(value);

  ini->n_items += 2;
  
  free(gkey);
}

void ini_free(ini_t *ini)
{
  int count;

  for (count = 0; count < ini->n_items; count += 2) {
    free(ini->keyvals[count]);
    free(ini->keyvals[count+1]);
  }

  free(ini->keyvals);
  free(ini);

}

ini_t *ini_parse_buffer(char *buf, long size)
{
  //  char *s = NULL;
  long pos;
  char *sectionbuf;
  char *keybuf;
  char *valuebuf;
  long tmppos;
  typedef enum {UNKNOWN, READ_SECTION, READ_KEY, READ_VALUE} reader_t;
  reader_t reader = UNKNOWN;
  ini_t *ini;

  ini = malloc(sizeof(ini_t));
  if (!ini) {
    fprintf(stderr, "Cannot allocate our ini file holder!\n");
    return NULL;
  }
  ini->keyvals = NULL;
  ini->keyvals_size = 0;
  ini->n_items = 0;

  sectionbuf = malloc(size);
  if (!sectionbuf) {
    fprintf(stderr, "Error allocating the section buffer\n");
    return NULL;
  }
  keybuf = malloc(size);
  if (!keybuf) {
    fprintf(stderr, "Error allocating the key buffer\n");
    return NULL;
  }
  valuebuf = malloc(size);
  if (!valuebuf) {
    fprintf(stderr, "Error allocating the value buffer\n");
    return NULL;
  }

  tmppos = 0;
  pos = 0;

  while (pos < size) {
    switch(reader) {
    case UNKNOWN:
      switch(*buf) {
      case '\n':
      case ' ':
      case '\r':
      case '\t':
	break;
      case '[':
	reader = READ_SECTION;
	break;
      default:
	*buf--;
	reader = READ_KEY;
	break;
      }
      break;
    case READ_SECTION:
      if (*buf == '\n') {
	//	sectionbuf[tmppos] = '.';
	//	tmppos++;
	sectionbuf[tmppos] = '\0';
	tmppos = 0;
	//	printf("section:[%s]\n", sectionbuf);
	reader = READ_KEY;
	break;
      }
      if (*buf != ']') {
	sectionbuf[tmppos] = *buf;
	tmppos++;
      }
      break;
    case READ_KEY:
      switch(*buf) {
      case '=':
	keybuf[tmppos] = '\0';
	tmppos = 0;
	//	printf("path:[%s]\n", keybuf);
	//	printf("++=[%c]\n", *(s+1));
	if (*(buf+1) == ' ') {
	  *buf++; // Just to skip the first space normal peope usually write!
	}
	reader = READ_VALUE;
	break;
      case ' ':
      case '\t':
	break;
      default:
	keybuf[tmppos] = *buf;
	tmppos++;
	break;
      }
      break;
    case READ_VALUE:
      if (*buf == '\n') {
	reader = UNKNOWN;
	valuebuf[tmppos] = '\0';
	//	printf("value:[%s]\n", valuebuf);
	_add_section_key_value(ini, sectionbuf, keybuf, valuebuf);
	tmppos = 0;
      } else {
	valuebuf[tmppos] = *buf;
	tmppos++;
      }
      break;
    }

    *buf++;
    pos++;
  }

  free(sectionbuf);
  free(keybuf);
  free(valuebuf);

  return ini;
}

ini_t *ini_parse_file(char *filename)
{
  char *buffer = NULL;
  FILE *fp;
  long size;
  size_t ret;
  ini_t *ini;

  size = _get_file_size(filename);

  fp = fopen(filename, "r");
  buffer = malloc(size + 1);
  if (!buffer) {
    fprintf(stderr, "Error allocating our buffer!\n");
    return NULL;
  }
  ret = fread(buffer, (size_t)size, 1, fp);
  //  printf("buffer:%s\n", buffer);  
  fclose(fp);

  ini = ini_parse_buffer(buffer, size);

  free(buffer);
  
  return ini;
}

char *ini_get_value(ini_t *ini, char *key)
{
  int count;

  for (count = 0; count < ini->n_items; count += 2) {
    char *k = ini->keyvals[count];

    if (!strcmp(k, key)) {
      return ini->keyvals[count + 1];
    }
  } 

  return NULL;
}


#ifdef SELF_TEST
int main(int argc, char **argv)
{
  int count;
  int retval;
  ini_t *ini;

  if (argc < 2) {
    printf("%s file.ini\n", argv[0]);
    return 1;
  }

  ini = ini_parse_file(argv[1]);
  
  for (count = 0; count < ini->n_items; count += 2) {
    char *k = ini->keyvals[count];
    char *v = ini->keyvals[count+1];

    printf("keys=[%s] vals=[%s]\n", k, v);
  }

  printf("Value that we get:%s\n", ini_get_value(ini, "foo.bar"));

  ini_free(ini);

  return 0;
}
#endif // SELF_TEST
