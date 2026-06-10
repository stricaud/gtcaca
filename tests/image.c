/*
 * image test — shows an image widget.
 *
 * Usage:  ./tests/image [path/to/image.png]
 *
 * If no path is given a small RGB gradient is generated to /tmp/gtcaca_test.ppm
 * and loaded automatically.  Press Q or Esc to quit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/main.h>
#include <gtcaca/application.h>
#include <gtcaca/window.h>
#include <gtcaca/label.h>
#include <gtcaca/image.h>
#include <gtcaca/statusbar.h>

#define TEST_PPM "/tmp/gtcaca_test.ppm"
#define IMG_W 128
#define IMG_H 96

/* Write a simple RGB gradient as a binary PPM (P6) so we always have a test
   image without depending on any external file. */
static int write_test_ppm(const char *path)
{
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  fprintf(f, "P6\n%d %d\n255\n", IMG_W, IMG_H);
  for (int y = 0; y < IMG_H; y++) {
    for (int x = 0; x < IMG_W; x++) {
      unsigned char r = (unsigned char)(x * 255 / (IMG_W - 1));
      unsigned char g = (unsigned char)(y * 255 / (IMG_H - 1));
      unsigned char b = (unsigned char)(128 + (x + y) % 128);
      fwrite(&r, 1, 1, f);
      fwrite(&g, 1, 1, f);
      fwrite(&b, 1, 1, f);
    }
  }
  fclose(f);
  return 0;
}

int main(int argc, char **argv)
{
  gtcaca_application_widget_t *app;
  gtcaca_window_widget_t      *win;
  gtcaca_image_widget_t       *img;
  gtcaca_statusbar_widget_t   *sb;
  const char *imgpath;
  int canvas_w, canvas_h;

  gtcaca_init(&argc, &argv);

  canvas_w = caca_get_canvas_width(gmo.cv);
  canvas_h = caca_get_canvas_height(gmo.cv);

  app = gtcaca_application_new("Image Test");

  if (argc > 1) {
    imgpath = argv[1];
  } else {
    write_test_ppm(TEST_PPM);
    imgpath = TEST_PPM;
  }

  win = gtcaca_window_new(NULL, "Image Viewer", 0, 1, canvas_w, canvas_h - 2);

  img = gtcaca_image_new(GTCACA_WIDGET(win), 1, 1, canvas_w - 2, canvas_h - 4);
  gtcaca_image_load(img, imgpath);

  sb = gtcaca_statusbar_new("Q / Esc: quit");

  gtcaca_main();
  return 0;
}
