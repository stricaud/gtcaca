#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

/* Implement stb_image in this translation unit */
#define STB_IMAGE_IMPLEMENTATION
#include <gtcaca/stb_image.h>

#include <gtcaca/image.h>
#include <gtcaca/main.h>

gtcaca_image_widget_t *gtcaca_image_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_image_widget_t *img;

  img = malloc(sizeof(gtcaca_image_widget_t));
  if (!img) {
    fprintf(stderr, "Error: Cannot allocate image widget\n");
    return NULL;
  }

  img->id = gtcaca_get_newid();
  img->has_focus = 0;
  img->is_visible = 1;
  img->type = GTCACA_WIDGET_IMAGE;
  img->parent = parent;
  img->children = NULL;

  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(img), x, y);
  img->width = width;
  img->height = height;

  img->color_focus_fg = gmo.theme.text.fg;
  img->color_focus_bg = gmo.theme.text.bg;
  img->color_nonfocus_fg = gmo.theme.text.fg;
  img->color_nonfocus_bg = gmo.theme.text.bg;

  img->filepath = NULL;
  img->pixels = NULL;
  img->img_width = 0;
  img->img_height = 0;

  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(img));

  return img;
}

int gtcaca_image_load(gtcaca_image_widget_t *img, const char *filepath)
{
  int channels;

  if (!img || !filepath) return -1;

  stbi_image_free(img->pixels);
  img->pixels = NULL;
  free(img->filepath);
  img->filepath = NULL;

  /* Load as RGBA (4 bytes per pixel: R, G, B, A in memory order) */
  img->pixels = stbi_load(filepath,
                          &img->img_width, &img->img_height,
                          &channels, 4);
  if (!img->pixels) {
    fprintf(stderr, "gtcaca_image_load: cannot load '%s': %s\n",
            filepath, stbi_failure_reason());
    return -1;
  }

  img->filepath = strdup(filepath);
  return 0;
}

void gtcaca_image_draw(gtcaca_image_widget_t *img)
{
  caca_dither_t *dither;

  if (!img->pixels) {
    /* Placeholder box when no image is loaded */
    caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_DARKGRAY);
    caca_fill_box(gmo.cv, img->x, img->y, img->width, img->height, ' ');
    caca_draw_cp437_box(gmo.cv, img->x, img->y, img->width, img->height);
    if (img->width > 10 && img->height > 0)
      caca_printf(gmo.cv, img->x + 1, img->y + img->height / 2, "[no image]");
    return;
  }

  /* stb_image RGBA: bytes in memory are [R][G][B][A].
     As a 32-bit little-endian value: R | (G<<8) | (B<<16) | (A<<24). */
  dither = caca_create_dither(
    32,
    img->img_width, img->img_height,
    img->img_width * 4,  /* pitch: bytes per row */
    0x000000FF,          /* red   mask */
    0x0000FF00,          /* green mask */
    0x00FF0000,          /* blue  mask */
    0xFF000000           /* alpha mask */
  );
  if (!dither) return;

  /* Render with half/quarter-block glyphs rather than libcaca's default ASCII
     charset: each cell then carries colour subpixels instead of a letter, which
     (with the truecolour presenter) looks like an actual photo instead of noise.
     Fall back to shaded glyphs where the font can't do fine blocks. Floyd–
     Steinberg + prefilter antialiasing smooth the down-sample. */
  caca_set_dither_charset(dither, gtcaca_terminal_supports_blocks() ? "blocks" : "shades");
  caca_set_dither_color(dither, "full16");
  caca_set_dither_antialias(dither, "prefilter");
  caca_set_dither_algorithm(dither, "fstein");

  caca_dither_bitmap(gmo.cv,
    img->x, img->y, img->width, img->height,
    dither, img->pixels);

  caca_free_dither(dither);
}

void gtcaca_image_free(gtcaca_image_widget_t *img)
{
  if (!img) return;
  stbi_image_free(img->pixels);
  free(img->filepath);
  free(img);
}
