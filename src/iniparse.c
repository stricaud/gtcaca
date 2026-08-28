#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <gtcaca/win_compat.h>   /* asprintf */
#endif

#include <gtcaca/iniparse.h>

static long _get_file_size(char *filename)
{
  FILE *fp;
  long size;

  fp = fopen(filename, "r");
  if (!fp) {
    return -1;
  }
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
  return 0;
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
  typedef enum {UNKNOWN, READ_SECTION, READ_KEY, READ_VALUE, READ_COMMENT} reader_t;
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
      case '#':
      case ';':
	reader = READ_COMMENT;   /* comment line, to end of line */
	break;
      case '[':
	reader = READ_SECTION;
	break;
      default:
	/* Re-examine this character as the first of a key. `pos` must step back
	   with `buf`, or the two drift apart by one for every key in the file and
	   the `pos < size` loop stops that many bytes short of the end — silently
	   dropping whatever is at the tail. */
	buf--; pos--;
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
      case '\n':                 /* no '=' on this line: not a pair, drop it */
	tmppos = 0;
	reader = UNKNOWN;
	break;
      case '=':
	keybuf[tmppos] = '\0';
	tmppos = 0;
	//	printf("path:[%s]\n", keybuf);
	//	printf("++=[%c]\n", *(s+1));
	if (pos + 1 < size && *(buf + 1) == ' ') {
	  /* Skip the leading space after '='. `pos` steps with `buf` or the two
	     drift apart by one for every pair in the file, and the `pos < size`
	     loop then reads that many bytes past the end of the buffer — the
	     mirror of the miscount noted in the UNKNOWN case above. */
	  buf++; pos++;
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
      /* A '#' or ';' *after whitespace* ends the value: "blue   # the sky".
         Preceded by whitespace, so a value may still start with '#' — an app
         whose colours are "#rrggbb" keeps working. */
      if (*buf == '\n' ||
	  ((*buf == '#' || *buf == ';') && tmppos > 0 &&
	   (valuebuf[tmppos-1] == ' ' || valuebuf[tmppos-1] == '\t'))) {
	while (tmppos > 0 && (valuebuf[tmppos-1] == ' ' ||
			      valuebuf[tmppos-1] == '\t' ||
			      valuebuf[tmppos-1] == '\r')) {
	  tmppos--;                /* trailing blanks would break every lookup */
	}
	valuebuf[tmppos] = '\0';
	_add_section_key_value(ini, sectionbuf, keybuf, valuebuf);
	tmppos = 0;
	reader = (*buf == '\n') ? UNKNOWN : READ_COMMENT;
      } else {
	valuebuf[tmppos] = *buf;
	tmppos++;
      }
      break;
    case READ_COMMENT:
      if (*buf == '\n') { reader = UNKNOWN; }
      break;
    }

    buf++;
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
  if (size < 0) { return NULL; }

  fp = fopen(filename, "r");
  if (!fp) { return NULL; }
  buffer = malloc(size + 1);
  if (!buffer) {
    fprintf(stderr, "Error allocating our buffer!\n");
    fclose(fp);
    return NULL;
  }
  /* Read what is really there rather than trusting the stat: a short read left
     the tail of the buffer uninitialised, and the parser reads one past its
     last byte when it looks for the space after an '='. */
  ret = fread(buffer, 1, (size_t)size, fp);
  size = (long)ret;
  buffer[size] = '\0';
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
