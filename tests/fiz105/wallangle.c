// fiz105: касание стены под углом 5/10/20/30/45° на 40 и 80 км/ч, газ держится, руль 0. Плоский мир, длинная вертикальная стена x=10.
// Печать на заезд: скорость через 0,5 с и 1 с после касания (км/ч), поворот курса за 1 с (°), остановилась (v<2 км/ч) через 1 с, макс. крен (°), мин. колёс.
// Собирается с car.c/col.c ЛЮБОЙ сборки (viewer/ этой сборки): cc -DSKIN_HOST -I viewer tests/fiz105/wallangle.c viewer/car.c viewer/col.c viewer/watchdog.c -lm
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
static uint32_t colBuf[2048];
static int world(ColWorld *W) {
  unsigned char *b = (unsigned char *)colBuf; memset(colBuf, 0, sizeof colBuf);
  ColHdr h; memset(&h, 0, sizeof h); memcpy(h.magic, "COL1", 4); h.ox = -300.f; h.oz = -300.f; h.cell = 600.f; h.W = h.H = 1; h.ntri = 4; h.nref = 4;
  memcpy(b, &h, 48);
  ColTri t[4] = {{{-300, 0, -300}, {-300, 0, 300}, {300, 0, 300}, {0, 1, 0}}, {{-300, 0, -300}, {300, 0, 300}, {300, 0, -300}, {0, 1, 0}},
                 {{10, 0, -300}, {10, 10, -300}, {10, 10, 300}, {-1, 0, 0}}, {{10, 0, -300}, {10, 10, 300}, {10, 0, 300}, {-1, 0, 0}}};
  memcpy(b + 48, t, sizeof t);
  uint32_t cs[2] = {0, 4}, refs[4] = {0, 1, 2, 3};
  memcpy(b + 48 + sizeof t, cs, 8); memcpy(b + 48 + sizeof t + 8, refs, 16);
  return colInit(W, b, 48 + sizeof t + 24);
}
static float wrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
int main(int argc, char **argv) {
  const char *car = argc > 1 ? argv[1] : NULL; float steer = argc > 2 ? (float)atof(argv[2]) : 0.f;
  static ColWorld W; if (world(&W)) { puts("мир не собрался"); return 2; }
  static const float ang[] = {5, 10, 20, 30, 45}, kmh[] = {40, 80};
  for (int ki = 0; ki < 2; ki++) for (int ai = 0; ai < 5; ai++) {
    float a = ang[ai] * 0.0174533f, v0 = kmh[ki] / 3.6f, L = 3.4f / sinf(a);
    float fx = sinf(a), fz = cosf(a), sx = 10.f - 2.5f - L * fx, sz = -L * fz;
    Car c; CarParams p, *pp = NULL; if (car && carParamsFor(car, &p)) pp = &p;
    carInit(&c, pp, &W, sx, 0.f, sz, a); c.huskDrive = 1;
    for (int k = 0; k < 40; k++) { CarInput in = {0, 0, 0, 0}; carUpdate(&c, &W, &in, 1 / 60.f); }
    c.vel[0] = fx * v0; c.vel[2] = fz * v0; c.v = v0;
    int hit = -1; float vprev = kmh[ki], vh = 0, h0 = 0, v05 = -1, v1 = -1, h1 = 0, rmax = 0; int minW = 4, vz1 = 0;
    for (int f = 0; f < 60 * 8; f++) {
      CarInput in = {hit >= 0 ? 1.f : 0.f, hit >= 0 ? steer : 0.f, 0, 0}; carUpdate(&c, &W, &in, 1 / 60.f);   // до касания накат, после — газ держится
      float sp = hypotf(c.vel[0], c.vel[2]) * 3.6f;
      if (hit < 0 && (c.wallN[0] * c.wallN[0] + c.wallN[1] * c.wallN[1] > .25f)) { hit = f; h0 = c.h; vh = vprev; }
      vprev = sp;
      if (hit >= 0) { if (f == hit + 30) v05 = sp; if (f == hit + 60) { v1 = sp; h1 = wrap(c.h - h0) * 57.2958f; }
        if (f <= hit + 60) { if (c.roll * 57.2958f > rmax || -c.roll * 57.2958f > rmax) rmax = fabsf(c.roll) * 57.2958f; if (c.wheels < minW) minW = c.wheels; } }
    }
    printf("%2.0f° %2.0f км/ч: касание %.0f км/ч, через 0.5 с %.0f, через 1 с %.0f км/ч, курс %+.0f°, %s, крен макс %.0f°, мин колёс %d\n", ang[ai], kmh[ki], vh, v05, v1, h1, v1 < 2.f ? "СТОИТ" : "скользит", rmax, minW);
  }
  return 0;
}
