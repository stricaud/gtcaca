#ifndef _GTCACA_IMAGE_H_
#define _GTCACA_IMAGE_H_

#include <gtcaca/main.h>
#include <gtcaca/widget.h>

struct _gtcaca_image_widget_t {

  /* Properties shared across all widgets */
  gtcaca_widget_type_t type;
  unsigned int id;
  int has_focus;
  int is_visible;
  int x;
  int y;
  int width;
  int height;
  uint8_t color_focus_fg;
  uint8_t color_focus_bg;
  uint8_t color_nonfocus_fg;
  uint8_t color_nonfocus_bg;
  gtcaca_widget_t *parent;
  gtcaca_widget_t *children;
  struct _gtcaca_image_widget_t *prev;
  struct _gtcaca_image_widget_t *next;

  /* Custom widget properties */
  char    *filepath;
  void    *pixels;     /* raw ARGB32 pixel data, NULL if no image loaded */
  int      img_width;
  int      img_height;
};
typedef struct _gtcaca_image_widget_t gtcaca_image_widget_t;

gtcaca_image_widget_t *gtcaca_image_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
int  gtcaca_image_load(gtcaca_image_widget_t *img, const char *filepath);
void gtcaca_image_draw(gtcaca_image_widget_t *img);
void gtcaca_image_free(gtcaca_image_widget_t *img);

#endif /* _GTCACA_IMAGE_H_ */
