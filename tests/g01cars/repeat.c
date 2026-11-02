// сверка g01-cars №1139: один поворот много раз на одной скорости — разброс результата и зависимость от шага кадра, без эмулятора
//   cc -std=c11 -O2 -I viewer [-DSV_FORCE_PLAYER=1] tests/g01cars/repeat.c viewer/car.c viewer/col.c -lm -o repeat && ./repeat [машина]
// Ровная площадка, тот же carUpdate, что в игре. Заезд: разгон до скорости, затем руль 0,7 три секунды с газом. Мера: угол поворота курса за 3 с, наибольший крен,
// доля кадров на двух колёсах и меньше, переворот. Шаги кадра: ровные 1/60, 1/30, 1/25 (PAL), и 50 заездов с шумом таймера ±4 мс (разброс = нестабильность «то держит, то нет»)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } floorData;
static ColWorld w;
static unsigned rs = 1; static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) / 16777216.f; }
typedef struct { float turn, roll, two, minv; int flip; } Res;
static Res run(const char *name, float v0, float steer, int mode, float dt0) {   // mode 0 ровный dt0, 1 шум ±4 мс вокруг dt0
  CarParams p; carParamsFor(name, &p); Car c; carInit(&c, &p, &w, 0, 0, 0, 0);
  CarInput in = {1.f, 0.f, 0.f, 0.f};
  for (int f = 0; f < 60 * 3; f++) { carUpdate(&c, &w, &in, 1.f / 60.f); }
  carSetSpeed(&c, v0);
  float h0 = c.h, t = 0.f, rollMax = 0.f; int two = 0, n = 0, flip = 0; Res r; r.minv = 1e9f;
  in.steer = steer;
  while (t < 3.f) {
    float dt = mode ? dt0 + (rnd() - .5f) * .008f : dt0; if (dt < .004f) dt = .004f;
    in.throttle = c.v < v0 ? 1.f : .6f;
    carUpdate(&c, &w, &in, dt); t += dt; n++;
    float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]);
    float roll = fabsf(c.roll) * 57.2958f; if (roll > rollMax) rollMax = roll;
    two += c.wheels <= 2; if (up < 0.f) flip = 1; if (c.v < r.minv) r.minv = c.v;
  }
  float dh = c.h - h0; while (dh > 3.14159f) dh -= 6.28318f; while (dh < -3.14159f) dh += 6.28318f;
  r.turn = fabsf(dh) * 57.2958f; r.roll = rollMax; r.two = 100.f * two / n; r.flip = flip; return r;
}
int main(int argc, char **argv) {
  memset(&floorData, 0, sizeof floorData); memcpy(floorData.h.magic, "COL1", 4);
  floorData.h.ox = floorData.h.oz = -2000; floorData.h.cell = 4000; floorData.h.W = floorData.h.H = 1; floorData.h.ntri = floorData.h.nref = 2;
  floorData.t[0] = (ColTri){{-2000, 0, -2000}, {2000, 0, 2000}, {2000, 0, -2000}, {0, 1, 0}};
  floorData.t[1] = (ColTri){{-2000, 0, -2000}, {-2000, 0, 2000}, {2000, 0, 2000}, {0, 1, 0}};
  floorData.start[1] = 2; floorData.refs[0] = 0; floorData.refs[1] = 1;
  if (colInit(&w, &floorData, sizeof floorData)) return 2;
  static const char *CARS[] = {"snake_v", "cpolice", "famil_v", "apu_v", "marge_v", "coffin"}; const int NC = 6;
  static const float KMH[] = {40.f, 70.f, 100.f}; static const float STR[] = {.4f, .7f, 1.f};
  float worstSd = 0.f; int flipsAll = 0;
  printf("%-8s %5s %5s | шаг 1/60  1/30  1/25: поворот° | 50 с шумом: поворот° среднее ± разброс (мин…макс), крен макс, на 2 колёсах %%, переворотов\n", "машина", "км/ч", "руль");
  for (int ci = 0; ci < NC; ci++) {
    if (argc > 1 && strcmp(argv[1], CARS[ci])) continue;
    for (int si = 0; si < 3; si++) for (int ki = 0; ki < 3; ki++) {
      float v = KMH[ki] / 3.6f;
      Res a = run(CARS[ci], v, STR[si], 0, 1.f / 60.f), b = run(CARS[ci], v, STR[si], 0, 1.f / 30.f), c = run(CARS[ci], v, STR[si], 0, 1.f / 25.f);
      double s = 0, s2 = 0; float mn = 1e9f, mx = -1e9f, rm = 0.f, tw = 0.f; int fl = 0;
      rs = 12345u + ci * 7 + si * 3 + ki;
      for (int k = 0; k < 50; k++) { Res r = run(CARS[ci], v, STR[si], 1, 1.f / 30.f); s += r.turn; s2 += (double)r.turn * r.turn; if (r.turn < mn) mn = r.turn; if (r.turn > mx) mx = r.turn; if (r.roll > rm) rm = r.roll; tw += r.two; fl += r.flip; }
      double m = s / 50, sd = sqrt(fmax(0.0, s2 / 50 - m * m)); if (sd > worstSd) worstSd = (float)sd; flipsAll += fl;
      printf("%-8s %5.0f %5.1f | %7.1f %5.1f %5.1f | %7.1f ± %4.2f (%.1f…%.1f), крен %4.1f, 2 колеса %4.1f%%, перевороты %d\n", CARS[ci], KMH[ki], STR[si], a.turn, b.turn, c.turn, m, sd, mn, mx, rm, tw / 50, fl);
    }
  }
  printf("ИТОГ: наибольший разброс поворота %.2f°, переворотов на ровном %d\n", worstSd, flipsAll);
  return 0;
}
