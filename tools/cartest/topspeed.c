// Разгон и максималка машины уровня на самой длинной прямой дороге уровня целым кадром игры (tools/bots/world.h): газ, руль прямо, скорость в 2/5/10/20/30 с, пройденный путь и наибольшая скорость, км/ч,
// против параметра TopSpeed из CAR1. Нужна для разбора гонок на время (теория ботов считает путь / TopSpeed).   topspeed (cwd с build/level.shr, build/homer.shr, build/sandbox/roads-lN.bin как первый аргумент)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
#include "navgraph.h"
int main(int argc, char **argv) {
  setenv("BW_NO_TRAFFIC", "1", 1); if (argc < 2) return 2;
  if (bwData("build/level.shr", "build/homer.shr", 0)) return 2; navLoad(argv[1]);
  static BotWorld W; if (bwNew(&W)) return 2; const ColWorld *col = bwCol();
  int bi = -1; float bl = 0; for (int i = 0; i < nroad; i++) { const Road *r = &roads[i]; float L = hypotf(r->bx - r->ax, r->bz - r->az); if (L > bl) { bl = L; bi = i; } }
  const Road *r = &roads[bi]; float dx = r->bx - r->ax, dz = r->bz - r->az, h = atan2f(dx, dz);
  Car c; carInit(&c, NULL, col, r->ax, r->ay, r->az, h); W.car = c; float x0 = c.x, z0 = c.z, vmax = 0; float at[5] = {2, 5, 10, 20, 30}; int ai = 0;
  printf("машина %s TopSpeed %.1f км/ч; самая длинная прямая: отрезок %d, %.0f м\n", bwCarName(), W.car.p.topSpeed * 3.6f, bi, bl);
  for (int f = 1; f <= 60 * 30; f++) { bwStep(&W, GP_R, 0.f, 0.f); float v = W.car.v * 3.6f; if (v > vmax) vmax = v;
    if (ai < 5 && f == (int)(at[ai] * 60)) { printf("  %2.0f с: %.0f км/ч, путь %.0f м\n", at[ai], v, hypotf(W.car.x - x0, W.car.z - z0)); ai++; } }
  printf("  наибольшая %.0f км/ч (%.0f%% TopSpeed)\n", vmax, vmax / (W.car.p.topSpeed * 3.6f) * 100.f); return 0;
}
