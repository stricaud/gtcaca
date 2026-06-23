#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <caca.h>

#include <gtcaca/map.h>
#include <gtcaca/main.h>

/* ── built-in gazetteer: world capitals + major cities (lat N+, lon E+) ─────── */
static const gtcaca_map_city_t g_cities[] = {
  /* North America */
  { "Washington",       38.90,  -77.04 }, { "New York",        40.71,  -74.01 },
  { "San Francisco",    37.77, -122.42 }, { "Los Angeles",     34.05, -118.24 },
  { "Chicago",          41.88,  -87.63 }, { "Seattle",         47.61, -122.33 },
  { "Boston",           42.36,  -71.06 }, { "Houston",         29.76,  -95.37 },
  { "Miami",            25.76,  -80.19 }, { "Toronto",         43.65,  -79.38 },
  { "Ottawa",           45.42,  -75.70 }, { "Montreal",        45.50,  -73.57 },
  { "Vancouver",        49.28, -123.12 }, { "Mexico City",     19.43,  -99.13 },
  { "Havana",           23.11,  -82.37 }, { "Guatemala City",  14.63,  -90.51 },
  /* South America */
  { "Brasilia",        -15.79,  -47.88 }, { "Rio de Janeiro", -22.91,  -43.17 },
  { "Sao Paulo",       -23.55,  -46.63 }, { "Buenos Aires",   -34.60,  -58.38 },
  { "Lima",            -12.05,  -77.04 }, { "Bogota",           4.71,  -74.07 },
  { "Santiago",        -33.45,  -70.67 }, { "Caracas",         10.48,  -66.90 },
  { "Quito",            -0.18,  -78.47 }, { "La Paz",         -16.50,  -68.15 },
  { "Montevideo",      -34.90,  -56.16 }, { "Asuncion",       -25.30,  -57.64 },
  /* Europe */
  { "London",           51.51,   -0.13 }, { "Paris",           48.85,    2.35 },
  { "Berlin",           52.52,   13.40 }, { "Madrid",          40.42,   -3.70 },
  { "Rome",             41.90,   12.50 }, { "Lisbon",          38.72,   -9.14 },
  { "Amsterdam",        52.37,    4.90 }, { "Brussels",        50.85,    4.35 },
  { "Vienna",           48.21,   16.37 }, { "Bern",            46.95,    7.45 },
  { "Zurich",           47.37,    8.54 }, { "Barcelona",       41.39,    2.17 },
  { "Munich",           48.14,   11.58 }, { "Milan",           45.46,    9.19 },
  { "Dublin",           53.35,   -6.26 }, { "Copenhagen",      55.68,   12.57 },
  { "Stockholm",        59.33,   18.07 }, { "Oslo",            59.91,   10.75 },
  { "Helsinki",         60.17,   24.94 }, { "Warsaw",          52.23,   21.01 },
  { "Prague",           50.09,   14.42 }, { "Budapest",        47.50,   19.04 },
  { "Athens",           37.98,   23.73 }, { "Moscow",          55.76,   37.62 },
  { "Saint Petersburg", 59.93,   30.34 }, { "Kyiv",            50.45,   30.52 },
  { "Bucharest",        44.43,   26.10 }, { "Belgrade",        44.79,   20.45 },
  { "Sofia",            42.70,   23.32 }, { "Reykjavik",       64.15,  -21.94 },
  /* Middle East + Turkey */
  { "Istanbul",         41.01,   28.98 }, { "Ankara",          39.93,   32.86 },
  { "Jerusalem",        31.78,   35.22 }, { "Tel Aviv",        32.07,   34.78 },
  { "Riyadh",           24.71,   46.68 }, { "Dubai",           25.20,   55.27 },
  { "Abu Dhabi",        24.45,   54.38 }, { "Doha",            25.29,   51.53 },
  { "Kuwait City",      29.38,   47.99 }, { "Baghdad",         33.31,   44.36 },
  { "Tehran",           35.69,   51.39 }, { "Amman",           31.95,   35.93 },
  { "Beirut",           33.89,   35.50 }, { "Damascus",        33.51,   36.29 },
  /* Africa */
  { "Cairo",            30.04,   31.24 }, { "Lagos",            6.52,    3.38 },
  { "Nairobi",          -1.29,   36.82 }, { "Cape Town",      -33.92,   18.42 },
  { "Johannesburg",    -26.20,   28.05 }, { "Pretoria",       -25.75,   28.19 },
  { "Casablanca",       33.57,   -7.59 }, { "Rabat",           34.02,   -6.83 },
  { "Accra",             5.60,   -0.19 }, { "Addis Ababa",      9.03,   38.74 },
  { "Algiers",          36.75,    3.06 }, { "Tunis",           36.81,   10.18 },
  { "Dakar",            14.69,  -17.45 }, { "Khartoum",        15.50,   32.56 },
  { "Kinshasa",         -4.32,   15.31 }, { "Luanda",          -8.84,   13.23 },
  { "Harare",          -17.83,   31.05 }, { "Dar es Salaam",   -6.79,   39.21 },
  /* Asia */
  { "Tokyo",            35.68,  139.69 }, { "Osaka",           34.69,  135.50 },
  { "Beijing",          39.90,  116.41 }, { "Shanghai",        31.23,  121.47 },
  { "Hong Kong",        22.32,  114.17 }, { "Seoul",           37.57,  126.98 },
  { "Taipei",           25.03,  121.57 }, { "Delhi",           28.61,   77.21 },
  { "Mumbai",           19.08,   72.88 }, { "Bangalore",       12.97,   77.59 },
  { "Kolkata",          22.57,   88.36 }, { "Chennai",         13.08,   80.27 },
  { "Bangkok",          13.76,  100.50 }, { "Singapore",        1.35,  103.82 },
  { "Jakarta",          -6.21,  106.85 }, { "Kuala Lumpur",     3.14,  101.69 },
  { "Manila",           14.60,  120.98 }, { "Hanoi",           21.03,  105.85 },
  { "Ho Chi Minh City", 10.82,  106.63 }, { "Kathmandu",       27.72,   85.32 },
  { "Dhaka",            23.81,   90.41 }, { "Islamabad",       33.69,   73.06 },
  { "Karachi",          24.86,   67.00 }, { "Colombo",          6.93,   79.86 },
  { "Ulaanbaatar",      47.89,  106.91 }, { "Almaty",          43.24,   76.95 },
  { "Tashkent",         41.30,   69.24 },
  /* Oceania */
  { "Sydney",          -33.87,  151.21 }, { "Melbourne",      -37.81,  144.96 },
  { "Canberra",        -35.28,  149.13 }, { "Brisbane",       -27.47,  153.03 },
  { "Perth",           -31.95,  115.86 }, { "Auckland",       -36.85,  174.76 },
  { "Wellington",      -41.29,  174.78 },
};

/* case-insensitive compare that ignores leading/trailing blanks */
static int city_name_eq(const char *a, const char *b)
{
  while (*a == ' ' || *a == '\t') a++;
  while (*b == ' ' || *b == '\t') b++;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    a++; b++;
  }
  while (*a == ' ' || *a == '\t') a++;
  while (*b == ' ' || *b == '\t') b++;
  return *a == '\0' && *b == '\0';
}

const gtcaca_map_city_t *gtcaca_map_cities(int *count)
{
  if (count) *count = (int)(sizeof g_cities / sizeof g_cities[0]);
  return g_cities;
}

const gtcaca_map_city_t *gtcaca_map_find_city(const char *name)
{
  int i, n = (int)(sizeof g_cities / sizeof g_cities[0]);
  if (!name) return NULL;
  for (i = 0; i < n; i++)
    if (city_name_eq(name, g_cities[i].name)) return &g_cities[i];
  return NULL;
}

int gtcaca_map_add_city(gtcaca_map_widget_t *m, const char *name, uint32_t glyph, uint8_t colour)
{
  const gtcaca_map_city_t *c = gtcaca_map_find_city(name);
  if (!c) return 0;
  gtcaca_map_add_point(m, c->lat, c->lon, glyph, colour, c->name);
  return 1;
}

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
