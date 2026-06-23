/* games.c — snake and sokoban, split out of cacamacs.c. They are
   self-contained game loops; the only shared symbol is g_message. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <caca.h>
#include <gtcaca/main.h>
#include "cacamacs.h"

/* ── snake / nibbles (M-x snake) ────────────────────────────────────────────
 * A self-contained worm game in the spirit of the classic Nibbles: steer the
 * worm to eat the numbered targets 1..9, growing each time; clear all nine to
 * advance a level (faster, new walls). Hitting a wall or yourself costs a life.
 * Original implementation with its own level layouts. */

#define SNAKE_GRID(g, W, x, y) (g)[(y) * (W) + (x)]
enum { SC_EMPTY = 0, SC_WALL = 1, SC_BODY = 2 };

/* the play field is a fixed 80x25 (classic Nibbles), centred on the canvas; all
   grid coordinates are offset by (snake_ox, snake_oy) when drawn to the screen */
static int snake_ox = 0, snake_oy = 0;

/* paint a solid cell with a full-block glyph █ in `colour`. Using a glyph (not a
   space with a coloured background) guarantees the display repaints the cell —
   some drivers don't repaint space cells whose only change is the background. */
static void snake_cell(int x, int y, uint8_t colour)
{
  caca_set_color_ansi(gmo.cv, colour, CACA_BLACK);
  caca_set_attr(gmo.cv, 0);
  caca_put_char(gmo.cv, snake_ox + x, snake_oy + y, 0x2588);   /* █ */
}

/* fill the whole canvas with solid black blocks (a clean, always-repainted bg) */
static void snake_fill_bg(void)
{
  int x, y, CW = caca_get_canvas_width(gmo.cv), CH = caca_get_canvas_height(gmo.cv);
  caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_BLACK);
  caca_set_attr(gmo.cv, 0);
  for (y = 0; y < CH; y++)
    for (x = 0; x < CW; x++)
      caca_put_char(gmo.cv, x, y, 0x2588);
}

/* lay out the walls for `level` into the grid (border + an original pattern) */
static void snake_build_level(unsigned char *g, int W, int H, int level)
{
  int x, y, ix0 = 1, iy0 = 2, ix1 = W - 2, iy1 = H - 2;
  int cx = W / 2, cy = (iy0 + iy1) / 2;
  memset(g, SC_EMPTY, (size_t)W * H);
  /* outer border (row 0 is the status line; the field starts at row 1) */
  for (x = 0; x < W; x++) { SNAKE_GRID(g, W, x, 1) = SC_WALL; SNAKE_GRID(g, W, x, H - 1) = SC_WALL; }
  for (y = 1; y < H; y++) { SNAKE_GRID(g, W, 0, y) = SC_WALL; SNAKE_GRID(g, W, W - 1, y) = SC_WALL; }

  switch (level % 5) {
  case 0: /* open field */
    break;
  case 1: /* two horizontal bars */
    for (x = ix0 + W / 6; x <= ix1 - W / 6; x++) {
      SNAKE_GRID(g, W, x, iy0 + (iy1 - iy0) / 3) = SC_WALL;
      SNAKE_GRID(g, W, x, iy1 - (iy1 - iy0) / 3) = SC_WALL;
    }
    break;
  case 2: /* a central cross */
    for (x = ix0 + W / 8; x <= ix1 - W / 8; x++) SNAKE_GRID(g, W, x, cy) = SC_WALL;
    for (y = iy0 + 1; y <= iy1 - 1; y++) SNAKE_GRID(g, W, cx, y) = SC_WALL;
    SNAKE_GRID(g, W, cx, cy) = SC_EMPTY;            /* gap at the centre */
    SNAKE_GRID(g, W, cx, cy - 1) = SC_EMPTY;
    SNAKE_GRID(g, W, cx, cy + 1) = SC_EMPTY;
    break;
  case 3: /* four corner blocks */
    for (y = 0; y < (iy1 - iy0) / 3; y++)
      for (x = 0; x < W / 5; x++) {
        SNAKE_GRID(g, W, ix0 + 2 + x, iy0 + 1 + y) = SC_WALL;
        SNAKE_GRID(g, W, ix1 - 2 - x, iy0 + 1 + y) = SC_WALL;
        SNAKE_GRID(g, W, ix0 + 2 + x, iy1 - 1 - y) = SC_WALL;
        SNAKE_GRID(g, W, ix1 - 2 - x, iy1 - 1 - y) = SC_WALL;
      }
    break;
  default: /* zig-zag rails */
    for (y = iy0 + 2; y <= iy1 - 2; y += 4) {
      int x0 = (y % 8 == iy0 + 2 % 8) ? ix0 + 1 : cx;
      for (x = x0; x < x0 + (ix1 - ix0) / 2; x++)
        if (x > ix0 && x < ix1) SNAKE_GRID(g, W, x, y) = SC_WALL;
    }
    break;
  }

  /* Always keep the worm's spawn runway clear. The worm starts at
     (cx-3..cx, H/2) heading right (see snake_reset_worm), so open that row
     around the centre — otherwise a pattern crossing the middle (e.g. the
     central cross) buries the snake the instant the level starts. */
  { int ry = H / 2, rx;
    for (rx = cx - 3; rx <= cx + 6; rx++)
      if (rx > 0 && rx < W - 1) SNAKE_GRID(g, W, rx, ry) = SC_EMPTY; }
}

static void snake_place_food(unsigned char *g, int W, int H, int *fx, int *fy)
{
  int tries;
  for (tries = 0; tries < W * H * 4; tries++) {
    int x = 1 + rand() % (W - 2);
    int y = 2 + rand() % (H - 3);
    if (SNAKE_GRID(g, W, x, y) == SC_EMPTY) { *fx = x; *fy = y; return; }
  }
  *fx = W / 2; *fy = H / 2;
}

/* centre a fresh 4-cell worm heading right; returns via the deque state */
static void snake_reset_worm(unsigned char *g, int W, int H, int *body, int *head, int *tail,
                             int *len, int *dx, int *dy)
{
  int cx = W / 2, cy = H / 2, i;
  /* clear any previous body marks */
  for (i = 0; i < W * H; i++) if (g[i] == SC_BODY) g[i] = SC_EMPTY;
  *head = 0; *tail = 0; *len = 0; *dx = 1; *dy = 0;
  for (i = 0; i < 4; i++) {
    int x = cx - 3 + i, y = cy;
    body[i * 2] = x; body[i * 2 + 1] = y;
    SNAKE_GRID(g, W, x, y) = SC_BODY;
    *head = i; (*len)++;
  }
  *tail = 0;
}

void run_snake(void)
{
  int CW = caca_get_canvas_width(gmo.cv);
  int CH = caca_get_canvas_height(gmo.cv);
  int W = CW < 80 ? CW : 80;        /* fixed 80x25 play field (classic Nibbles), */
  int H = CH < 25 ? CH : 25;        /* shrunk only if the terminal is smaller     */
  unsigned char *g;
  int *body;                       /* ring of (x,y) pairs, cap W*H            */
  int cap = W * H, head = 0, tail = 0, len = 0, dx = 1, dy = 0, ndx = 1, ndy = 0;
  int fx = 0, fy = 0, level = 0, number = 1, score = 0, lives = 5;
  int running = 1, started = 0, x, y;
  caca_event_t ev;

  if (W < 20 || H < 8) { snprintf(g_message, sizeof g_message, "Window too small for snake"); return; }
  snake_ox = (CW - W) / 2;          /* centre the play field on the canvas */
  snake_oy = (CH - H) / 2;
  g = malloc((size_t)cap);
  body = malloc((size_t)cap * 2 * sizeof(int));
  if (!g || !body) { free(g); free(body); return; }
  srand((unsigned)time(NULL));

  /* start screen: choose a starting level (as Nibbles prompts for skill) */
  {
    int chosen = 0, ox = snake_ox, oy = snake_oy;
    snake_fill_bg();
    caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_printf(gmo.cv, ox + W / 2 - 4, oy + H / 2 - 3, "S N A K E");
    caca_set_attr(gmo.cv, 0);
    caca_printf(gmo.cv, ox + W / 2 - 18, oy + H / 2 - 1, "Start at which level (1-9)?  Enter = 1");
    caca_printf(gmo.cv, ox + W / 2 - 18, oy + H / 2 + 1, "Eat 1..9 to clear a level; it speeds up each level.");
    caca_printf(gmo.cv, ox + W / 2 - 18, oy + H / 2 + 2, "Steer with the arrows.  p pauses, q quits.");
    caca_refresh_display(gmo.dp);
    while (!chosen) {
      if (caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) {
        int k = caca_get_event_key_ch(&ev);
        if (k >= '1' && k <= '9') { level = k - '1'; chosen = 1; }
        else if (k == CACA_KEY_RETURN || k == 10 || k == ' ') { level = 0; chosen = 1; }
        else if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) { free(g); free(body); gtcaca_redraw(); return; }
      }
    }
  }

  snake_build_level(g, W, H, level);
  snake_reset_worm(g, W, H, body, &head, &tail, &len, &dx, &dy);
  ndx = dx; ndy = dy;
  snake_place_food(g, W, H, &fx, &fy);

  while (running) {
    int nx, ny, ate, tx, ty, idx;

    /* ── draw the whole board ── */
    snake_fill_bg();                           /* solid black field */
    caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK); caca_set_attr(gmo.cv, 0);
    caca_printf(gmo.cv, snake_ox + 1, snake_oy + 0, "SNAKE  level %d   eat: %d   score: %d   lives: %d   (p pause, q quit)",
                level + 1, number, score, lives);
    for (y = 1; y < H; y++)
      for (x = 0; x < W; x++)
        if (SNAKE_GRID(g, W, x, y) == SC_WALL) snake_cell(x, y, CACA_BLUE);
    for (idx = 0; idx < len; idx++) {
      int bi = (tail + idx) % cap;
      snake_cell(body[bi * 2], body[bi * 2 + 1], bi == head ? CACA_LIGHTGREEN : CACA_GREEN);
    }
    caca_set_color_ansi(gmo.cv, CACA_YELLOW, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
    caca_put_char(gmo.cv, snake_ox + fx, snake_oy + fy, '0' + number);
    if (!started) {                            /* keep clear of the worm's row */
      caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
      caca_printf(gmo.cv, snake_ox + W / 2 - 12, snake_oy + H / 2 - 4, " press an arrow to start ");
    }
    caca_refresh_display(gmo.dp);

    /* ── input (block until the game starts, else tick).  caca_get_event's
       timeout is in MICROSECONDS, so a playable ~8 moves/sec is ~120000us,
       getting a little faster each level. ── */
    int tick_ms = 130 - level * 8; if (tick_ms < 70) tick_ms = 70;
    if (caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, started ? tick_ms * 1000 : -1)) {
      int k = caca_get_event_key_ch(&ev);
      if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) { running = 0; continue; }
      if (k == 'p' || k == 'P') {            /* pause */
        caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
        caca_printf(gmo.cv, snake_ox + W / 2 - 5, snake_oy + H / 2 - 4, " paused ");
        caca_refresh_display(gmo.dp);
        caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1);
        continue;
      }
      /* any arrow starts the game; the direction is taken unless it reverses */
      {
        int dir = 1;
        if      (k == CACA_KEY_UP    || k == CACA_KEY_CTRL_P) { if (dy == 0) { ndx = 0; ndy = -1; } }
        else if (k == CACA_KEY_DOWN  || k == CACA_KEY_CTRL_N) { if (dy == 0) { ndx = 0; ndy = 1; } }
        else if (k == CACA_KEY_LEFT  || k == CACA_KEY_CTRL_B) { if (dx == 0) { ndx = -1; ndy = 0; } }
        else if (k == CACA_KEY_RIGHT || k == CACA_KEY_CTRL_F) { if (dx == 0) { ndx = 1; ndy = 0; } }
        else dir = 0;
        if (dir) started = 1;
      }
    }
    if (!started) continue;

    /* ── advance one step ── */
    dx = ndx; dy = ndy;
    nx = body[head * 2] + dx; ny = body[head * 2 + 1] + dy;
    tx = body[tail * 2]; ty = body[tail * 2 + 1];
    ate = (nx == fx && ny == fy);
    /* moving into the tail cell is fine unless we are about to grow into it */
    if (SNAKE_GRID(g, W, nx, ny) == SC_WALL ||
        (SNAKE_GRID(g, W, nx, ny) == SC_BODY && !(nx == tx && ny == ty && !ate))) {
      if (--lives <= 0) {                    /* game over */
        caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_RED); caca_set_attr(gmo.cv, CACA_BOLD);
        caca_printf(gmo.cv, snake_ox + W / 2 - 14, snake_oy + H / 2, "  GAME OVER  -  score %d  -  r restart, q quit  ", score);
        caca_refresh_display(gmo.dp);
        for (;;) {
          caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1);
          { int k = caca_get_event_key_ch(&ev);
            if (k == 'r' || k == 'R') { level = 0; number = 1; score = 0; lives = 5; started = 0;
              snake_build_level(g, W, H, level); snake_reset_worm(g, W, H, body, &head, &tail, &len, &dx, &dy);
              ndx = dx; ndy = dy; snake_place_food(g, W, H, &fx, &fy); break; }
            if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) { running = 0; break; } }
        }
      } else {                               /* lose a life, keep the level */
        snake_reset_worm(g, W, H, body, &head, &tail, &len, &dx, &dy); ndx = dx; ndy = dy; started = 0;
      }
      continue;
    }

    /* add the new head */
    head = (head + 1) % cap; body[head * 2] = nx; body[head * 2 + 1] = ny;
    SNAKE_GRID(g, W, nx, ny) = SC_BODY; len++;

    if (ate) {
      score += number * (level + 1) * 10;
      number++;
      if (number > 9) {                      /* level complete */
        level++; number = 1; started = 0;
        snake_build_level(g, W, H, level);
        snake_reset_worm(g, W, H, body, &head, &tail, &len, &dx, &dy); ndx = dx; ndy = dy;
      }
      snake_place_food(g, W, H, &fx, &fy);
    } else {                                 /* pop the tail */
      SNAKE_GRID(g, W, tx, ty) = SC_EMPTY;
      tail = (tail + 1) % cap; len--;
    }
  }

  free(g); free(body);
  snprintf(g_message, sizeof g_message, "Snake: final score %d", score);
  gtcaca_redraw();
  caca_refresh_display(gmo.dp);
}

/* ── sokoban (M-x sokoban) ──────────────────────────────────────────────────
 * Levels are loaded from files at runtime (one level per N.txt), so we never
 * embed a level set. Legend: X wall, * player, # box, . goal, $ box-on-goal,
 * % player-on-goal, space floor. Each cell is drawn as a multi-cell sprite. */

#define SOK_MAXW 96
#define SOK_MAXH 48
enum { SOKS_FLOOR = 0, SOKS_WALL, SOKS_GOAL };

typedef struct {
  int w, h, pr, pc;
  unsigned char st[SOK_MAXH][SOK_MAXW];    /* SOKS_* (walls/goals are static) */
  unsigned char box[SOK_MAXH][SOK_MAXW];   /* movable boxes */
} sok_level_t;

typedef struct { signed char dr, dc; unsigned char pushed; } sok_move_t;

/* Optional override: drop your own N.txt levels in ~/.cacamacs/sokoban and the
   game uses those instead of the built-in set below. */
static int sok_find_dir(char *out, size_t n)
{
  const char *home = getenv("HOME");
  struct stat s; char p[PATH_MAX];
  if (!home) return 0;
  snprintf(p, sizeof p, "%s/.cacamacs/sokoban", home);
  if (stat(p, &s) == 0 && S_ISDIR(s.st_mode)) { snprintf(out, n, "%s", p); return 1; }
  return 0;
}

static int sok_load(const char *dir, int idx, sok_level_t *L)
{
  char path[PATH_MAX]; FILE *f; char line[256]; int r = 0, maxw = 0;
  snprintf(path, sizeof path, "%s/%d.txt", dir, idx);
  f = fopen(path, "r");
  if (!f) return 0;
  memset(L, 0, sizeof *L);
  while (fgets(line, sizeof line, f) && r < SOK_MAXH) {
    int len = (int)strlen(line), c;
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = 0;
    for (c = 0; c < len && c < SOK_MAXW; c++) {
      switch (line[c]) {
      case 'X': L->st[r][c] = SOKS_WALL; break;
      case '.': L->st[r][c] = SOKS_GOAL; break;
      case '#': L->box[r][c] = 1; break;
      case '%': L->st[r][c] = SOKS_GOAL; L->box[r][c] = 1; break;  /* box on target */
      case '*': L->pr = r; L->pc = c; break;
      case '$': L->st[r][c] = SOKS_GOAL; L->pr = r; L->pc = c; break;  /* player on target */
      default: break;
      }
    }
    if (len > maxw) maxw = len;
    r++;
  }
  fclose(f);
  L->h = r; L->w = maxw;
  return r > 0;
}

/* Built-in level set, embedded as text (legend: X wall, * player, # box,
   . target, % box-on-target, $ player-on-target). Each level is a
   NULL-terminated list of rows; add more by extending the array. */
static const char *const g_sok_levels[][16] = {
  { "     XXXXX",
    "XXXXXX . X",
    "X   XX#. X",
    "X*###  . X",
    "XX  X  . X",
    " X  XXXXXX",
    " XXXX     ",
    NULL },
  { "XXXXXX",
    "X    X",
    "X### X",
    "X....X",
    "X ##.X",
    "X  * X",
    "XXXXXX",
    NULL },
  { "XXXXXX  ",
    "X *  X  ",
    "X XX#XXX",
    "X  . . X",
    "XXX #  X",
    "  XXX  X",
    "    XXXX",
    NULL },
  { "XXXXX",
    "X * X",
    "X X XXXX",
    "X ...  X",
    "XX ### X",
    " XXX   X",
    "   XXXXX",
    NULL },
  { "XXXXXX",
    "X.   X",
    "X.   X",
    "XX . X",
    "X  XXX",
    "X ## X",
    "X  # X",
    "X  * X",
    "XXXXXX",
    NULL },
  { "XXXXXXXXX",
    "X   X   X",
    "X %. .. X",
    "XX#X X#XX",
    "X #     X",
    "X   X * X",
    "XXXXXXXXX",
    NULL },
  { "     XXXX",
    "     X  X",
    "     X  X",
    "XXXXXX  X",
    "X*   XX X",
    "X ###X .X",
    "X # #  .X",
    "X   X #.X",
    "XXXXX...X",
    "    XXXXX",
    NULL },
  { " XXXXXX ",
    " X    X ",
    " X X##XX",
    " X     X",
    "XX#XXX X",
    "X  X . X",
    "X    . X",
    "XXX  $ X",
    "  XXXXXX",
    NULL },
  { " XXXXXX ",
    " X    X ",
    " X XX X ",
    "XX .#.X ",
    "X  #*#XX",
    "X X.#. X",
    "X X XX X",
    "X      X",
    "XXXXXXXX",
    NULL },
  { "  XXXX ",
    " XX  X ",
    "XX   XX",
    "X .#. X",
    "X X#X X",
    "X  *  X",
    "XXXXXXX",
    NULL },
  { " XXXX  ",
    " X  XX ",
    "XX # X",
    "X .%.XX",
    "X X#  X",
    "X  *  X",
    "XXXXXXX",
    NULL },
  { "XXXXXXXXX",
    "X   ... X",
    "X   XXX X",
    "XXX #   X",
    "X  #X X X",
    "X # X * X",
    "X   XXXXX",
    "XXXXX    ",
    NULL },
  { "XXXXXXXXX",
    "X.....  X",
    "X   . # X",
    "XXXX XXXX",
    "X #     X",
    "X ### # X",
    "X * X   X",
    "XXXXXXXXX",
    NULL },
  { " XXXX     ",
    " X  X     ",
    " X  XXXXX ",
    " X      XX",
    " XX XX # X",
    "XXX X. #*X",
    "X    .X#XX",
    "X   X.  X ",
    "XXXXXXXXX",
    NULL },
  { "  XXXXX ",
    "  X  .XX",
    "XXX.#. X",
    "X  ##X X",
    "X   *  X",
    "XXXXXXXX",
    NULL },
  { "  XXXXX",
    "XXX  .X",
    "X   X X",
    "X*###.X",
    "X   X X",
    "X  XX X",
    "X    .X",
    "XXXXXXX",
    NULL },
  { "XXXXXXXX",
    "X      X",
    "X ##   X",
    "XX*XX  X",
    " X# X#XX",
    " X.... X",
    " X     X",
    " XXXXXXX",
    NULL },
  { "  XXXXXXX",
    " XX     X",
    " X    # X",
    " X  X #XX",
    "XXX#XX  X",
    "X   ..%.X",
    "X   X * X",
    "XXXXXXXXX",
    NULL },
  { " XXXX     ",
    " X  XXXXXX",
    "XX #X  . X",
    "X #*# .%.X",
    "X  # X . X",
    "X   XXXXXX",
    "XXXXX     ",
    NULL },
  { "XXXXXXX",
    "X     X",
    "X % % X",
    "XX. #XX",
    "X % % X",
    "X     X",
    "X *X  X",
    "XXXXXXX",
    NULL },
  { "   XXXXX",
    "   X   X",
    "XXXX#  X",
    "X   # XX",
    "X X X  X",
    "X * . .X",
    "XXXXXXXX",
    NULL },
  { "  XXXXXXX",
    "  X  .  X",
    " XXX#X  X",
    " X  . . X",
    "XXX#X# XX",
    "X  . . .X",
    "X #X#X# X",
    "X  * X  X",
    "XXXXXXXXX",
    NULL },
  { "  XXXXX   ",
    "XXX   XXXX",
    "X  ..##  X",
    "X  ##..  X",
    "XXXX * XXX",
    "   XXXXX  ",
    NULL },
  { "XXXXXXXX",
    "X ...  X",
    "X  XX# X",
    "X  #   X",
    "XX#XX#XX",
    "X   #  X",
    "X #XX  X",
    "X  ...*X",
    "XXXXXXXX",
    NULL },
  { "XXXXXX",
    "X    X",
    "X### X",
    "X....X",
    "X ##.X",
    "X  * X",
    "XXXXXX",
    NULL },
  { "   XXXXX",
    " XXX   X",
    " X   # X",
    "XX#..#XX",
    "X #.. X ",
    "X * XXX ",
    "XXXXX    ",
    NULL },
  { " XXXXXX   ",
    " X    X   ",
    " X XX#XXXX",
    " X X #   X",
    " X X * # X",
    "XX X#XX XX",
    "X ..    X ",
    "X ..X   X ",
    "XXXXXXXXX ",
    NULL },
  { " XXXXXX   ",
    " X    X   ",
    " X XX XXXX",
    " X X ##  X",
    " X X     X",
    "XX X XX#XX",
    "X ....# X ",
    "X   X * X ",
    "XXXXXXXXX ",
    NULL },
  { " XXXXX    ",
    " X   XXXXX",
    " X X ..  X",
    " X X ..  X",
    "XX#X#XX XX",
    "X # #   X ",
    "X * X   X ",
    "XXXXXXXXX ",
    NULL },
  { "XXXXXXXXX",
    "X       X",
    "X # # # X",
    "XX#XX XXX",
    " X X. . X",
    " X  . . X",
    " X * X  X",
    " XXXXXXXX",
    NULL },
  { " XXXXX ",
    " X   X ",
    "XX X XX",
    "X # # X",
    "X.. ..X",
    "X #X# X",
    "XX * XX",
    " XXXXX ",
    NULL },
  { " XXXXX ",
    " X   X ",
    "XX X XX",
    "X # # X",
    "X.....X",
    "X ### X",
    "XX * XX",
    " XXXXX ",
    NULL },
  { "XXXXX ",
    "X   X ",
    "X # X ",
    "X*##XX",
    "XX#..X",
    " X ..X",
    " X   X",
    " XXXXX",
    NULL },
  { "XXXXXXXXXXXXXXXX",
    "X              X",
    "X              X",
    "X              X",
    "X              X",
    "X * #   ..     X",
    "X              X",
    "X              X",
    "X              X",
    "X              X",
    "XXXXXXXXXXXXXXXX",
    NULL },
};
#define SOK_NLEVELS ((int)(sizeof g_sok_levels / sizeof g_sok_levels[0]))

static int sok_load_embedded(int idx, sok_level_t *L)
{
  const char *const *rows;
  int r, c;
  if (idx < 1 || idx > SOK_NLEVELS) return 0;
  rows = g_sok_levels[idx - 1];
  memset(L, 0, sizeof *L);
  for (r = 0; rows[r]; r++) {
    for (c = 0; rows[r][c]; c++) {
      char ch = rows[r][c];
      if (ch == 'X') L->st[r][c] = SOKS_WALL;
      else if (ch == '.') L->st[r][c] = SOKS_GOAL;
      else if (ch == '#') L->box[r][c] = 1;
      else if (ch == '%') { L->st[r][c] = SOKS_GOAL; L->box[r][c] = 1; }   /* box on target */
      else if (ch == '*') { L->pr = r; L->pc = c; }
      else if (ch == '$') { L->st[r][c] = SOKS_GOAL; L->pr = r; L->pc = c; } /* player on target */
    }
    if ((int)strlen(rows[r]) > L->w) L->w = (int)strlen(rows[r]);
  }
  L->h = r;
  return 1;
}

/* load level `idx` from files if a level dir was found, else from the built-in set */
static int sok_get(int have_dir, const char *dir, int idx, sok_level_t *L)
{
  return have_dir ? sok_load(dir, idx, L) : sok_load_embedded(idx, L);
}

static int sok_won(const sok_level_t *L)
{
  int r, c, boxes = 0;
  for (r = 0; r < L->h; r++)              /* solved when every box sits on a target */
    for (c = 0; c < L->w; c++)
      if (L->box[r][c]) { boxes++; if (L->st[r][c] != SOKS_GOAL) return 0; }
  return boxes > 0;
}

/* draw one cell as a sprite of tw x th terminal cells */
static void sok_tile(int sx, int sy, int tw, int th, int kind)
{
  int i, j;
  switch (kind) {
  case 1:                                   /* wall: solid grey blocks */
    caca_set_color_ansi(gmo.cv, CACA_LIGHTGRAY, CACA_BLACK); caca_set_attr(gmo.cv, 0);
    for (j = 0; j < th; j++) for (i = 0; i < tw; i++) caca_put_char(gmo.cv, sx + i, sy + j, 0x2588);
    break;
  case 3: case 4: {                         /* box / box-on-goal */
    uint8_t col = (kind == 4) ? CACA_GREEN : CACA_BROWN;
    caca_set_color_ansi(gmo.cv, col, CACA_BLACK); caca_set_attr(gmo.cv, 0);
    for (j = 0; j < th; j++) for (i = 0; i < tw; i++) caca_put_char(gmo.cv, sx + i, sy + j, 0x2588);
    if (tw >= 4 && th >= 2) {                /* crate cross-braces */
      caca_set_color_ansi(gmo.cv, CACA_BLACK, col); caca_set_attr(gmo.cv, CACA_BOLD);
      caca_put_char(gmo.cv, sx + 1, sy,        'X');
      caca_put_char(gmo.cv, sx + tw - 2, sy,   'X');
    }
    break;
  }
  case 2:                                   /* goal marker */
    caca_set_color_ansi(gmo.cv, CACA_RED, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
    if (tw >= 4 && th >= 2) {
      caca_put_char(gmo.cv, sx + 1, sy, '.');         caca_put_char(gmo.cv, sx + tw - 2, sy, '.');
      caca_put_char(gmo.cv, sx + 1, sy + th - 1, '.'); caca_put_char(gmo.cv, sx + tw - 2, sy + th - 1, '.');
    } else {
      caca_put_char(gmo.cv, sx + tw / 2, sy + th / 2, 'o');
    }
    break;
  case 5: case 6:                           /* player */
    caca_set_color_ansi(gmo.cv, CACA_LIGHTCYAN, CACA_BLACK); caca_set_attr(gmo.cv, CACA_BOLD);
    if (tw >= 4 && th >= 2) {
      caca_put_char(gmo.cv, sx + tw / 2 - 1, sy, 0x2588);   /* head */
      caca_put_char(gmo.cv, sx + tw / 2,     sy, 0x2588);
      for (i = 1; i < tw - 1; i++) caca_put_char(gmo.cv, sx + i, sy + 1, 0x2588); /* body+arms */
    } else {
      for (j = 0; j < th; j++) for (i = 0; i < tw; i++) caca_put_char(gmo.cv, sx + i, sy + j, 0x2588);
    }
    break;
  default: break;                           /* floor: black background */
  }
}

void run_sokoban(void)
{
  int W = caca_get_canvas_width(gmo.cv), H = caca_get_canvas_height(gmo.cv);
  char dir[PATH_MAX];
  int have_dir = sok_find_dir(dir, sizeof dir);
  int idx = 1, moves = 0, running = 1, undo_n = 0;
  sok_level_t L;
  sok_move_t *undo = malloc(sizeof(sok_move_t) * 8192);
  caca_event_t ev;

  if (!undo) return;
  if (!sok_get(have_dir, dir, idx, &L)) { have_dir = 0; sok_get(0, dir, idx, &L); }

  while (running) {
    int tw, th, ox, oy, r, c, k, dr = 0, dc = 0;

    if      (L.w * 4 <= W - 2 && L.h * 2 <= H - 3) { tw = 4; th = 2; }
    else if (L.w * 2 <= W - 2 && L.h     <= H - 3) { tw = 2; th = 1; }
    else                                           { tw = 1; th = 1; }
    ox = (W - L.w * tw) / 2;       if (ox < 0) ox = 0;
    oy = 1 + ((H - 1) - L.h * th) / 2; if (oy < 1) oy = 1;

    snake_fill_bg();                         /* solid black background */
    caca_set_color_ansi(gmo.cv, CACA_WHITE, CACA_BLACK); caca_set_attr(gmo.cv, 0);
    caca_printf(gmo.cv, 1, 0, "SOKOBAN  level %d   moves %d   (arrows move, u undo, r reset, n/p level, q quit)%s",
                idx, moves, have_dir ? "" : "   [built-in]");
    for (r = 0; r < L.h; r++)
      for (c = 0; c < L.w; c++) {
        int on_goal = (L.st[r][c] == SOKS_GOAL), kind;
        if (L.st[r][c] == SOKS_WALL)         kind = 1;
        else if (r == L.pr && c == L.pc)     kind = on_goal ? 6 : 5;
        else if (L.box[r][c])                kind = on_goal ? 4 : 3;
        else if (on_goal)                    kind = 2;
        else                                 kind = 0;
        sok_tile(ox + c * tw, oy + r * th, tw, th, kind);
      }
    if (sok_won(&L)) {
      caca_set_color_ansi(gmo.cv, CACA_BLACK, CACA_GREEN); caca_set_attr(gmo.cv, CACA_BOLD);
      caca_printf(gmo.cv, W / 2 - 24, H - 1, "  SOLVED in %d moves!   press n for the next level  ", moves);
    }
    caca_refresh_display(gmo.dp);

    if (!caca_get_event(gmo.dp, CACA_EVENT_KEY_PRESS, &ev, -1)) continue;
    k = caca_get_event_key_ch(&ev);

    if (k == 'q' || k == 'Q' || k == CACA_KEY_ESCAPE) { running = 0; continue; }
    if (k == 'r' || k == 'R') { sok_get(have_dir, dir, idx, &L); moves = 0; undo_n = 0; continue; }
    if (k == 'u' || k == 'U') {
      if (undo_n > 0) {
        sok_move_t m = undo[--undo_n];
        if (m.pushed) { L.box[L.pr + m.dr][L.pc + m.dc] = 0; L.box[L.pr][L.pc] = 1; }
        L.pr -= m.dr; L.pc -= m.dc; if (moves > 0) moves--;
      }
      continue;
    }
    if (k == 'n' || k == 'N') { sok_level_t T; if (sok_get(have_dir, dir, idx + 1, &T)) { idx++; L = T; moves = 0; undo_n = 0; } continue; }
    if (k == 'p' || k == 'P') { sok_level_t T; if (idx > 1 && sok_get(have_dir, dir, idx - 1, &T)) { idx--; L = T; moves = 0; undo_n = 0; } continue; }

    if      (k == CACA_KEY_UP    || k == CACA_KEY_CTRL_P) dr = -1;
    else if (k == CACA_KEY_DOWN  || k == CACA_KEY_CTRL_N) dr = 1;
    else if (k == CACA_KEY_LEFT  || k == CACA_KEY_CTRL_B) dc = -1;
    else if (k == CACA_KEY_RIGHT || k == CACA_KEY_CTRL_F) dc = 1;
    else continue;

    if (sok_won(&L)) continue;               /* level done: wait for n */
    {
      int nr = L.pr + dr, nc = L.pc + dc;
      if (nr < 0 || nr >= L.h || nc < 0 || nc >= L.w) continue;
      if (L.st[nr][nc] == SOKS_WALL) continue;
      if (L.box[nr][nc]) {                    /* push a box */
        int br = nr + dr, bc = nc + dc;
        if (br < 0 || br >= L.h || bc < 0 || bc >= L.w) continue;
        if (L.st[br][bc] == SOKS_WALL || L.box[br][bc]) continue;
        L.box[nr][nc] = 0; L.box[br][bc] = 1;
        L.pr = nr; L.pc = nc;
        if (undo_n < 8192) { undo[undo_n].dr = (signed char)dr; undo[undo_n].dc = (signed char)dc; undo[undo_n].pushed = 1; undo_n++; }
        moves++;
      } else {                                /* just walk */
        L.pr = nr; L.pc = nc;
        if (undo_n < 8192) { undo[undo_n].dr = (signed char)dr; undo[undo_n].dc = (signed char)dc; undo[undo_n].pushed = 0; undo_n++; }
        moves++;
      }
    }
  }
  free(undo);
  gtcaca_redraw();
  caca_refresh_display(gmo.dp);
  snprintf(g_message, sizeof g_message, "Sokoban: level %d", idx);
}

