#ifndef _GTCACA_APPLICATION_H_
#define _GTCACA_APPLICATION_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

typedef struct _gtcaca_application_widget_t gtcaca_application_widget_t;

/*
 * Callback definitions
 */
typedef int (*gtcaca_application_key_cb_t)(gtcaca_application_widget_t *widget, int key, void *userdata);

struct _gtcaca_application_widget_t {

  /* Properties shared accross all widgets */
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children; // Any widget can be a children
  struct _gtcaca_application_widget_t *prev;
  struct _gtcaca_application_widget_t *next;

  /* Widget callbacks */
  gtcaca_application_key_cb_t private_key_cb; /* Things the widget must do */
  
  /* Now starts custom widget properties */
  char *application_title;
};
typedef struct _gtcaca_application_widget_t gtcaca_application_widget_t;

gtcaca_application_widget_t *gtcaca_application_new(char *application_title);
void gtcaca_application_draw(gtcaca_application_widget_t *application);

#endif // _GTCACA_APPLICATION_H_
