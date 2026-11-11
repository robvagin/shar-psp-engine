// Коллизия-96: таран тонкой стенки (сверка с PS2: после удара машина стоит на четырёх колёсах, крен ≤ 10°). wallram LEVEL PLAYER x z hdeg kmh [gas=1]
// Машина ставится за 30 м до точки (x z) курсом hdeg (0 = +z, 90 = +x), берёт скорость kmh, газ держится 3 с после; печать: макс. крен, макс. кадров на двух колёсах (определение allrisk), мин. колёс, v перед ударом, v после.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#define RUN (getenv("WR_RUN") ? (float)atof(getenv("WR_RUN")) : 30.f)
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 7 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: wallram LEVEL PLAYER x z hdeg kmh [gas]\n"); return 2; }
  float x = atof(argv[3]), z = atof(argv[4]), hd = atof(argv[5]) * 0.0174533f, kmh = atof(argv[6]); int gas = argc > 7 ? atoi(argv[7]) : 1; const ColWorld *cw = bwCol();
  float fx = sinf(hd), fz = cosf(hd), sx = x - fx * RUN, sz = z - fz * RUN, gy;
  if (getenv("WR_START")) { float hs; sscanf(getenv("WR_START"), "%f %f %f", &sx, &sz, &hs); hd = hs * 0.0174533f; fx = sinf(hd); fz = cosf(hd); }   // WR_START="x z курс°": старт из трассы PS2 (центр машины), скорость kmh
  if (!colGroundMat(cw, sx, sz, 40.f, -50.f, &gy, 0, 0)) { puts("нет земли"); return 2; }
  memset(&W, 0, sizeof W); bwNew(&W); bwPlace(&W, sx, gy + .3f, sz, hd); for (int f = 0; f < 20; f++) bwStep(&W, 0, 0.f, 0.f);
  float v0 = kmh / 3.6f; W.car.vel[0] = fx * v0; W.car.vel[2] = fz * v0; W.car.v = v0;
  int run = 0, runMax = 0, minW = 4, hit = -1; float v24 = -1, rmax = 0, vhit = 0, vend = 0, vprev = kmh;
  for (int f = 0; f < 8 * 60; f++) { float d = (W.car.x - x) * fx + (W.car.z - z) * fz;
    bwStep(&W, (gas == 1 && (hit < 0 || f < hit + 180)) || (gas == 2 && hit >= 0 && f < hit + 180) ? GP_R : 0, getenv("WR_STEER") ? (float)atof(getenv("WR_STEER")) : 0.f, 0.f); const Car *c = &W.car; if (getenv("WR_POS") && f < 40 && f % 4 == 0) printf("  f%2d x %.2f z %.2f h %.0f v %.0f крен %.1f колёс %d\n", f, c->x, c->z, c->h * 57.2958f, c->v * 3.6f, c->roll * 57.2958f, c->wheels); float roll = fabsf(c->roll) * 57.2958f, v = c->v * 3.6f;
    if (hit < 0 && ((v < vprev - 12.f && d > -6.f) || (c->contacts > 0 && f > 20))) { hit = f; vhit = vprev; } if (hit >= 0 && f == hit + 24) v24 = v; vprev = v; if (hit >= 0 && getenv("WR_TRACE") && f < hit + 70 && (f - hit) % 2 == 0) printf("  +%2d roll%6.1f pitch%6.1f w(%5.1f %5.1f %5.1f) v(%5.1f %4.1f %5.1f) колёс %d контактов %d тип %d n(%.2f %.2f) comp %.2f %.2f %.2f %.2f\n", f - hit, c->roll*57.2958f, c->pitch*57.2958f, c->w[0], c->w[1], c->w[2], c->vel[0], c->vel[1], c->vel[2], c->wheels, c->contacts, c->ctype[0], c->cnx[0], c->cnz[0], c->comp[0], c->comp[1], c->comp[2], c->comp[3]);
      if (hit >= 0) { if (roll > rmax) rmax = roll; if (c->wheels < minW) minW = c->wheels;
      int tw = c->grounded && c->wheels > 0 && c->wheels <= 2 && roll > 12.f; run = tw ? run + 1 : 0; if (run > runMax) runMax = run; vend = v; } }
  printf("курс %3.0f° %3.0f км/ч: удар на кадре %d (v до %.0f), крен макс %.1f°, на двух колёсах подряд %d кадров, мин колёс %d, v через 24 кадра %.0f, в конце %.0f\n", atof(argv[5]), kmh, hit, vhit, rmax, runMax, minW, v24, vend);
  return 0; }
