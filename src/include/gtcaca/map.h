#ifndef _GTCACA_MAP_H_
#define _GTCACA_MAP_H_

#include <stdint.h>
#include <gtcaca/main.h>
#include <gtcaca/widget.h>

/* ── Geo map / "geoview" widget (inspired by tui-rs' Map/Canvas) ─────────────
 *
 * An equirectangular projection: longitude/latitude map linearly to the
 * widget's cells. Draw geometry by adding polylines of (lon,lat) vertices
 * (e.g. coastlines) and plot places as labelled markers. A built-in low-res
 * world outline is available via gtcaca_map_add_world(). All glyphs are plain
 * so it renders in any terminal. */

typedef struct {
  double  lat, lon;
  uint32_t glyph;
  uint8_t colour;
  char   *label;       /* optional text drawn beside the marker */
} gtcaca_map_point_t;

typedef struct {
  double *lonlat;      /* flat lon,lat,lon,lat,… */
  int     n;           /* number of vertices */
  uint8_t colour;
} gtcaca_map_poly_t;

typedef struct _gtcaca_map_widget_t gtcaca_map_widget_t;
struct _gtcaca_map_widget_t {
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
  struct _gtcaca_map_widget_t *prev;
  struct _gtcaca_map_widget_t *next;

  double lon_min, lon_max, lat_min, lat_max;   /* visible window               */
  gtcaca_map_point_t *points;
  int    npoints, cappoints;
  gtcaca_map_poly_t  *polys;
  int    npolys, cappolys;
  int    graticule;                            /* draw meridians/parallels     */
  int    sel;                                  /* selected marker index (-1 none) */
  char  *title;
};

/* ── built-in gazetteer ──────────────────────────────────────────────────────
 * A table of world capitals and major cities, so callers can plot well-known
 * places by name without looking up coordinates. Names are plain ASCII
 * (e.g. "Sao Paulo", "Rio de Janeiro"); lookup is case-insensitive. */
typedef struct { const char *name; double lat, lon; } gtcaca_map_city_t;

/* the full table; *count is set to its length */
const gtcaca_map_city_t *gtcaca_map_cities(int *count);
/* find a city by name (case-insensitive, surrounding spaces ignored); NULL if unknown */
const gtcaca_map_city_t *gtcaca_map_find_city(const char *name);

gtcaca_map_widget_t *gtcaca_map_new(gtcaca_widget_t *parent, int x, int y, int width, int height);
void gtcaca_map_draw(gtcaca_map_widget_t *m);
void gtcaca_map_set_bounds(gtcaca_map_widget_t *m, double lon_min, double lon_max,
                           double lat_min, double lat_max);
/* plot a marker at (lat,lon); glyph 0 => default dot; label may be NULL */
void gtcaca_map_add_point(gtcaca_map_widget_t *m, double lat, double lon,
                          uint32_t glyph, uint8_t colour, const char *label);
/* plot a named city from the built-in gazetteer (label = the city name);
   returns 1 if the city is known and was added, 0 otherwise. glyph 0 => dot. */
int  gtcaca_map_add_city(gtcaca_map_widget_t *m, const char *name,
                         uint32_t glyph, uint8_t colour);
/* add a polyline of `n` (lon,lat) vertices, e.g. a coastline */
void gtcaca_map_add_polyline(gtcaca_map_widget_t *m, const double *lonlat, int n, uint8_t colour);
/* add a built-in low-resolution world outline (continents) */
void gtcaca_map_add_world(gtcaca_map_widget_t *m, uint8_t colour);
void gtcaca_map_clear(gtcaca_map_widget_t *m);
void gtcaca_map_set_graticule(gtcaca_map_widget_t *m, int on);
void gtcaca_map_set_title(gtcaca_map_widget_t *m, const char *title);
/* project (lon,lat) to a cell; returns 1 and fills *sx,*sy if inside the view */
int  gtcaca_map_project(gtcaca_map_widget_t *m, double lon, double lat, int *sx, int *sy);
/* navigate between markers: arrows pick the nearest marker in that direction,
   Tab cycles. Returns 1 if the key was consumed. */
int  gtcaca_map_key(gtcaca_map_widget_t *m, int key, void *userdata);
int  gtcaca_map_selected(gtcaca_map_widget_t *m);   /* selected marker index, or -1 */
void gtcaca_map_free(gtcaca_map_widget_t *m);

#endif /* _GTCACA_MAP_H_ */
