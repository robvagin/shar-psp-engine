// 1330: свёрстка «лежит»: 300 случайных кувырков над плоской плитой (скорость 8–25 м/с, вращение до ±7 рад/с по трём осям, высота 1–3 м); для каждого — когда машина первый раз легла
// (ось вверх < 0,5 при скорости < 3 м/с), когда сработал сброс (rights/recoveries), и если нет — где осталась (ось вверх, колёс, скорость, вращение) через 12 с.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
#define NT 2
static struct { ColHdr h; ColTri t[NT]; unsigned start[2], refs[NT]; } fd; static ColWorld W;
static float rnd(float a, float b) { return a + (b - a) * (rand() / (float)RAND_MAX); }
int main(int argc, char **argv) {
  const char *nm = argc > 1 ? argv[1] : "famil_v"; int N = argc > 2 ? atoi(argv[2]) : 300; float dt = 1.f / 30.f; srand(1330);
  memset(&fd, 0, sizeof fd); memcpy(fd.h.magic, "COL1", 4); fd.h.ox = fd.h.oz = -2000; fd.h.cell = 4000; fd.h.W = fd.h.H = 1; fd.h.ntri = fd.h.nref = NT;
  { float x0 = -2000, x1 = 2000, z0 = -2000, z1 = 2000; fd.t[0] = (ColTri){{x0, 0, z0}, {x1, 0, z1}, {x1, 0, z0}, {0, 1, 0}}; fd.t[1] = (ColTri){{x0, 0, z0}, {x0, 0, z1}, {x1, 0, z1}, {0, 1, 0}}; }
  fd.start[1] = 2; fd.h.ntri = fd.h.nref = 2; fd.refs[0] = 0; fd.refs[1] = 1; if (colInit(&W, &fd, sizeof fd)) return 2;
  CarParams p; carParamsFor(nm, &p); int dumpN = 3; int lay = 0, fired = 0, never = 0; float sumT = 0, maxT = 0; int hist[8] = {0};
  for (int n = 0; n < N; n++) {
    Car c; carInit(&c, &p, &W, 0, 0, 0, 0); c.huskDrive = 1; for (int f = 0; f < 10; f++) { CarInput in; memset(&in, 0, sizeof in); carUpdate(&c, &W, &in, dt); }
    c.y = rnd(1, 3); float sp = rnd(8, 35); c.vel[0] = 0; c.vel[2] = sp; c.vel[1] = rnd(-2, 6); c.w[0] = rnd(-14, 14); c.w[1] = rnd(-5, 5); c.w[2] = rnd(-14, 14);
    float tLay = -1, tReset = -1; unsigned r0 = c.rights + c.recoveries; float lastUp = 1, lastAn = 0, lastSp = 0; int lastW = 0;
    for (int f = 0; f < 30 * 12; f++) {
      CarInput in; memset(&in, 0, sizeof in); float t = (f + 1) * dt; unsigned rb = c.rights + c.recoveries; carUpdate(&c, &W, &in, dt);
      float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]), s = sqrtf(c.vel[0] * c.vel[0] + c.vel[1] * c.vel[1] + c.vel[2] * c.vel[2]), an = sqrtf(c.w[0] * c.w[0] + c.w[1] * c.w[1] + c.w[2] * c.w[2]);
      if (tLay < 0 && c.y < 1.8f && up < .7f && s < 3.f) tLay = t;
      if (c.rights + c.recoveries > rb) { tReset = t; break; }
      lastUp = up; lastAn = an; lastSp = s; lastW = c.wheels;
    }
    (void)r0;
    if (getenv("DUMP1330") && tReset < 0 && lastUp < .6f) { printf("  состояние n=%d: y=%.2f up=%.2f roll=%.1f° wheels=%d wcon=%x comp=(%.2f %.2f %.2f %.2f) xs=(%.2f %.2f %.2f %.2f) contacts=%d pen=%.3f grounded=%d vel=(%.2f %.2f %.2f) w=(%.2f %.2f %.2f) halfW=%.2f wheelX=%.2f wheelRad=%.2f bodyLo=%.2f cmY=%.2f\n", n, c.y, lastUp, c.roll * 57.2958f, c.wheels, c.wcon, c.comp[0], c.comp[1], c.comp[2], c.comp[3], c.xs[0], c.xs[1], c.xs[2], c.xs[3], c.contacts, c.pen, c.grounded, c.vel[0], c.vel[1], c.vel[2], c.w[0], c.w[1], c.w[2], c.p.halfW, c.p.wheelX, c.p.wheelRad, c.p.bodyLo, c.p.cmY); if (--dumpN <= 0) break; }
    if (tLay >= 0) { lay++; if (tReset >= 0) { fired++; float d = tReset - tLay; sumT += d; if (d > maxT) maxT = d; int b = d < 0 ? 0 : d < .5f ? 1 : d < 1 ? 2 : d < 2 ? 3 : d < 4 ? 4 : 5; hist[b]++; } else { never++; printf("  не сработал: n=%d легла %.2f с; в конце up=%.2f колёс=%d v=%.2f w=%.2f\n", n, tLay, lastUp, lastW, lastSp, lastAn); } }
  }
  printf("%s: кувырков %d, легли %d, сброс сработал %d (в среднем %.2f с после «легла», худший %.2f), НЕ сработал за 12 с: %d; распределение (до 0,5 / 1 / 2 / 4 / дольше): %d %d %d %d %d\n", nm, N, lay, fired, fired ? sumT / fired : 0, maxT, never, hist[1] + hist[0], hist[2], hist[3], hist[4], hist[5]);
  return 0;
}
