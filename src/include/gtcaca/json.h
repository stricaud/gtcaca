#ifndef _GTCACA_JSON_H_
#define _GTCACA_JSON_H_

/*
 * A small, dependency-free JSON reader. It tolerates JSONC niceties found in
 * VSCode config files: // and / * * / comments and trailing commas. It is a
 * read-only DOM — enough to consume package.json and language-configuration.json.
 */

typedef enum {
  GTCACA_JSON_NULL,
  GTCACA_JSON_BOOL,
  GTCACA_JSON_NUMBER,
  GTCACA_JSON_STRING,
  GTCACA_JSON_ARRAY,
  GTCACA_JSON_OBJECT
} gtcaca_json_type;

typedef struct gtcaca_json_value gtcaca_json_value;

struct gtcaca_json_value {
  gtcaca_json_type type;
  union {
    int    boolean;
    double number;
    char  *string;
    struct { gtcaca_json_value **items; int count; } array;
    struct { char **keys; gtcaca_json_value **vals; int count; } object;
  } u;
};

/* Parse NUL-terminated JSON text; returns NULL on error. Free with _free. */
gtcaca_json_value *gtcaca_json_parse(const char *text);
/* Parse the contents of a file; returns NULL if it can't be read or parsed. */
gtcaca_json_value *gtcaca_json_parse_file(const char *path);
void gtcaca_json_free(gtcaca_json_value *v);

/* Serialize back to text. If indent > 0, pretty-print with that many spaces per
   level; if indent == 0, produce a compact single line. malloc'd; caller frees. */
char *gtcaca_json_stringify(const gtcaca_json_value *v, int indent);

/* Accessors — all NULL/zero-safe. */
const char        *gtcaca_json_string(const gtcaca_json_value *v);       /* NULL if not a string */
gtcaca_json_value *gtcaca_json_object_get(const gtcaca_json_value *v, const char *key);
int                gtcaca_json_array_size(const gtcaca_json_value *v);
gtcaca_json_value *gtcaca_json_array_get(const gtcaca_json_value *v, int i);

#endif /* _GTCACA_JSON_H_ */
