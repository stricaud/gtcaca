#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caca.h>

#include <gtcaca/map.h>
#include <gtcaca/main.h>

gtcaca_map_widget_t *gtcaca_map_new(gtcaca_widget_t *parent, int x, int y, int width, int height)
{
  gtcaca_map_widget_t *m = malloc(sizeof(*m));
  if (!m) { fprintf(stderr, "Error: Cannot allocate map\n"); return NULL; }
  m->id = gtcaca_get_newid();
  m->has_focus = 0;
  m->is_visible = 1;
  m->type = GTCACA_WIDGET_MAP;
  m->parent = parent;
  m->children = NULL;
  gtcaca_widget_position_size_parent(parent, GTCACA_WIDGET(m), x, y);
  m->width = width; m->height = height;
  m->color_focus_fg = m->color_nonfocus_fg = gmo.theme.textview.fg;
  m->color_focus_bg = m->color_nonfocus_bg = gmo.theme.textview.bg;
  m->lon_min = -180.0; m->lon_max = 180.0;
  m->lat_min = -90.0;  m->lat_max = 90.0;
  m->points = NULL; m->npoints = 0; m->cappoints = 0;
  m->polys = NULL; m->npolys = 0; m->cappolys = 0;
  m->graticule = 1;
  m->sel = -1;
  m->title = NULL;
  CDL_APPEND(gmo.widgets_list, GTCACA_WIDGET(m));
  return m;
}

void gtcaca_map_clear(gtcaca_map_widget_t *m)
{
  int i;
  for (i = 0; i < m->npoints; i++) free(m->points[i].label);
  for (i = 0; i < m->npolys; i++) free(m->polys[i].lonlat);
  m->npoints = 0; m->npolys = 0;
}

void gtcaca_map_free(gtcaca_map_widget_t *m)
{
  if (!m) return;
  gtcaca_map_clear(m);
  free(m->points); free(m->polys); free(m->title); free(m);
}

void gtcaca_map_set_bounds(gtcaca_map_widget_t *m, double lon_min, double lon_max,
                           double lat_min, double lat_max)
{ m->lon_min = lon_min; m->lon_max = lon_max; m->lat_min = lat_min; m->lat_max = lat_max; }

void gtcaca_map_add_point(gtcaca_map_widget_t *m, double lat, double lon,
                          uint32_t glyph, uint8_t colour, const char *label)
{
  gtcaca_map_point_t *p;
  if (m->npoints == m->cappoints) {
    int nc = m->cappoints ? m->cappoints * 2 : 32;
    gtcaca_map_point_t *q = realloc(m->points, (size_t)nc * sizeof(*q));
    if (!q) return;
    m->points = q; m->cappoints = nc;
  }
  p = &m->points[m->npoints++];
  p->lat = lat; p->lon = lon; p->glyph = glyph ? glyph : '*'; p->colour = colour;
  p->label = label ? strdup(label) : NULL;
}

void gtcaca_map_add_polyline(gtcaca_map_widget_t *m, const double *lonlat, int n, uint8_t colour)
{
  gtcaca_map_poly_t *p;
  if (n < 2) return;
  if (m->npolys == m->cappolys) {
    int nc = m->cappolys ? m->cappolys * 2 : 16;
    gtcaca_map_poly_t *q = realloc(m->polys, (size_t)nc * sizeof(*q));
    if (!q) return;
    m->polys = q; m->cappolys = nc;
  }
  p = &m->polys[m->npolys++];
  p->lonlat = malloc((size_t)n * 2 * sizeof(double));
  if (!p->lonlat) { m->npolys--; return; }
  memcpy(p->lonlat, lonlat, (size_t)n * 2 * sizeof(double));
  p->n = n; p->colour = colour;
}

void gtcaca_map_set_graticule(gtcaca_map_widget_t *m, int on) { m->graticule = on ? 1 : 0; }
void gtcaca_map_set_title(gtcaca_map_widget_t *m, const char *title)
{ free(m->title); m->title = title ? strdup(title) : NULL; }

int gtcaca_map_project(gtcaca_map_widget_t *m, double lon, double lat, int *sx, int *sy)
{
  int inner_x = m->x + 1, inner_y = m->y + 1, inner_w = m->width - 2, inner_h = m->height - 2;
  double fx, fy;
  if (inner_w <= 0 || inner_h <= 0) return 0;
  if (m->lon_max == m->lon_min || m->lat_max == m->lat_min) return 0;
  fx = (lon - m->lon_min) / (m->lon_max - m->lon_min);
  fy = (m->lat_max - lat) / (m->lat_max - m->lat_min);     /* latitude up */
  if (fx < 0.0 || fx > 1.0 || fy < 0.0 || fy > 1.0) return 0;
  *sx = inner_x + (int)(fx * (inner_w - 1) + 0.5);
  *sy = inner_y + (int)(fy * (inner_h - 1) + 0.5);
  return 1;
}

/* ── built-in low-resolution world outline (hand-simplified continents) ─────── */
static const double _WORLD_NA[] = {
  -168,65,-165,60,-153,58,-135,57,-130,52,-123,48,-124,40,-117,32,-110,23,-105,20,
  -97,16,-90,15,-83,9,-81,18,-80,26,-75,35,-70,42,-67,45,-60,47,-56,52,-64,60,
  -78,62,-95,69,-125,70,-156,71,-168,65 };
static const double _WORLD_SA[] = {
  -78,8,-70,11,-60,10,-50,0,-44,-3,-35,-6,-39,-13,-48,-25,-54,-34,-62,-40,-66,-45,
  -71,-52,-75,-50,-73,-42,-71,-30,-76,-16,-81,-6,-80,2,-78,8 };
static const double _WORLD_AF[] = {
  -16,15,-16,21,-10,28,0,32,10,34,20,33,32,31,35,24,43,12,51,12,42,-2,40,-12,
  35,-22,25,-34,18,-34,12,-18,9,-2,5,5,-8,5,-16,12,-16,15 };
static const double _WORLD_EU[] = {
  -10,36,-9,43,-2,48,2,51,4,58,10,63,25,71,30,66,40,60,40,50,28,45,18,42,8,44,0,40,-10,36 };
static const double _WORLD_AS[] = {
  26,40,30,47,50,50,60,45,70,55,90,52,110,52,130,53,142,50,140,42,130,35,122,30,
  121,22,109,21,105,10,100,7,98,16,90,22,80,8,77,20,70,25,60,25,52,28,45,37,35,38,26,40 };
static const double _WORLD_AU[] = {
  114,-22,122,-18,130,-12,137,-12,142,-11,146,-18,150,-25,150,-37,143,-39,135,-35,
  129,-32,123,-34,115,-35,113,-26,114,-22 };

void gtcaca_map_add_world(gtcaca_map_widget_t *m, uint8_t colour)
{
  gtcaca_map_add_polyline(m, _WORLD_NA, (int)(sizeof _WORLD_NA / sizeof(double) / 2), colour);
  gtcaca_map_add_polyline(m, _WORLD_SA, (int)(sizeof _WORLD_SA / sizeof(double) / 2), colour);
  gtcaca_map_add_polyline(m, _WORLD_AF, (int)(sizeof _WORLD_AF / sizeof(double) / 2), colour);
  gtcaca_map_add_polyline(m, _WORLD_EU, (int)(sizeof _WORLD_EU / sizeof(double) / 2), colour);
  gtcaca_map_add_polyline(m, _WORLD_AS, (int)(sizeof _WORLD_AS / sizeof(double) / 2), colour);
  gtcaca_map_add_polyline(m, _WORLD_AU, (int)(sizeof _WORLD_AU / sizeof(double) / 2), colour);
}

void gtcaca_map_draw(gtcaca_map_widget_t *m)
{
  uint8_t fg = gmo.theme.textview.fg, bg = gmo.theme.textview.bg;
  int inner_x = m->x + 1, inner_y = m->y + 1, inner_w = m->width - 2, inner_h = m->height - 2;
  int i, j, sx, sy;

  caca_set_color_ansi(gmo.cv, fg, bg); caca_set_attr(gmo.cv, 0);
  caca_fill_box(gmo.cv, m->x, m->y, m->width, m->height, ' ');
  caca_draw_cp437_box(gmo.cv, m->x, m->y, m->width, m->height);
  if (m->title) caca_printf(gmo.cv, m->x + 2, m->y, "| %s |", m->title);
  if (inner_w <= 0 || inner_h <= 0) return;

  /* faint graticule every 30 degrees */
  if (m->graticule) {
    double lon, lat;
    caca_set_color_ansi(gmo.cv, CACA_DARKGRAY, bg); caca_set_attr(gmo.cv, 0);
    for (lon = -150.0; lon <= 150.0 + 0.01; lon += 30.0)
      for (lat = m->lat_min; lat <= m->lat_max; lat += (m->lat_max - m->lat_min) / (inner_h * 2))
        if (gtcaca_map_project(m, lon, lat, &sx, &sy)) caca_put_char(gmo.cv, sx, sy, ':');
    for (lat = -60.0; lat <= 60.0 + 0.01; lat += 30.0)
      for (lon = m->lon_min; lon <= m->lon_max; lon += (m->lon_max - m->lon_min) / (inner_w * 2))
        if (gtcaca_map_project(m, lon, lat, &sx, &sy)) caca_put_char(gmo.cv, sx, sy, '.');
  }

  /* polylines (continents/coastlines) drawn with connected segments */
  for (i = 0; i < m->npolys; i++) {
    gtcaca_map_poly_t *p = &m->polys[i];
    int px = 0, py = 0, have = 0;
    caca_set_color_ansi(gmo.cv, p->colour, bg); caca_set_attr(gmo.cv, 0);
    for (j = 0; j < p->n; j++) {
      if (gtcaca_map_project(m, p->lonlat[j * 2], p->lonlat[j * 2 + 1], &sx, &sy)) {
        if (have) caca_draw_line(gmo.cv, px, py, sx, sy, 0x2588);  /* █ segment */
        px = sx; py = sy; have = 1;
      } else have = 0;
    }
  }

  /* markers + labels on top; the selected one is highlighted */
  for (i = 0; i < m->npoints; i++)
    if (gtcaca_map_project(m, m->points[i].lon, m->points[i].lat, &sx, &sy)) {
      int selected = (i == m->sel);
      if (selected) { caca_set_color_ansi(gmo.cv, CACA_BLACK, m->points[i].colour); }
      else          { caca_set_color_ansi(gmo.cv, m->points[i].colour, bg); }
      caca_set_attr(gmo.cv, CACA_BOLD);
      caca_put_char(gmo.cv, sx, sy, m->points[i].glyph);
      if (m->points[i].label) {
        caca_set_color_ansi(gmo.cv, selected ? CACA_WHITE : m->points[i].colour, bg);
        caca_set_attr(gmo.cv, selected ? CACA_BOLD : 0);
        caca_printf(gmo.cv, sx + 1, sy, "%-.*s",
                    inner_x + inner_w - (sx + 1) > 0 ? inner_x + inner_w - (sx + 1) : 0,
                    m->points[i].label);
      }
    }
  caca_set_attr(gmo.cv, 0);
}

int gtcaca_map_selected(gtcaca_map_widget_t *m) { return m->sel; }

int gtcaca_map_key(gtcaca_map_widget_t *m, int key, void *userdata)
{
  int dirx = 0, diry = 0, i, best = -1, csx, csy;
  long bestd = 0;
  (void)userdata;
  if (m->npoints == 0) return 0;
  if (m->sel < 0) m->sel = 0;

  switch (key) {
  case CACA_KEY_TAB:   m->sel = (m->sel + 1) % m->npoints; return 1;
  case CACA_KEY_UP:    diry = -1; break;
  case CACA_KEY_DOWN:  diry = 1;  break;
  case CACA_KEY_LEFT:  dirx = -1; break;
  case CACA_KEY_RIGHT: dirx = 1;  break;
  default: return 0;
  }
  if (!gtcaca_map_project(m, m->points[m->sel].lon, m->points[m->sel].lat, &csx, &csy)) return 1;
  /* pick the nearest marker that lies in the pressed direction */
  for (i = 0; i < m->npoints; i++) {
    int sx, sy; long d;
    if (i == m->sel) continue;
    if (!gtcaca_map_project(m, m->points[i].lon, m->points[i].lat, &sx, &sy)) continue;
    if (dirx && (sx - csx) * dirx <= 0) continue;
    if (diry && (sy - csy) * diry <= 0) continue;
    d = (long)(sx - csx) * (sx - csx) + (long)(sy - csy) * (sy - csy);
    if (best < 0 || d < bestd) { bestd = d; best = i; }
  }
  if (best >= 0) m->sel = best;
  return 1;
}
