// Коллизия-96: крен на вираже по записанной траектории. curve_roll LEVEL PLAYER PATH.txt [км/ч=100]: PATH — строки «x z» (кадры ботов перед событием «два колеса»);
// машина ставится на начало, едет по ломаной (преследование точки на 8 м вперёд), держит заданную скорость; печать: макс. крен, кадров «на двух колёсах» (определение allrisk), мин. колёс, макс. скорость.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W; static float PX[4096], PZ[4096];
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: curve_roll LEVEL PLAYER PATH [kmh]\n"); return 2; }
  float tv = (argc > 4 ? (float)atof(argv[4]) : 100.f) / 3.6f; FILE *f = fopen(argv[3], "r"); int n = 0; while (n < 4000 && fscanf(f, "%f %f", &PX[n], &PZ[n]) == 2) n++; fclose(f);
  if (n < 5) return 2; const ColWorld *cw = bwCol(); float gy = -1e9f; if (!colGroundMat(cw, PX[0], PZ[0], 30.f, -50.f, &gy, 0, 0)) { puts("нет земли в начале"); return 2; }
  memset(&W, 0, sizeof W); bwNew(&W); float hd = atan2f(PX[3] - PX[0], PZ[3] - PZ[0]); bwPlace(&W, PX[0], gy + .3f, PZ[0], hd); for (int i = 0; i < 20; i++) bwStep(&W, 0, 0.f, 0.f);
  W.car.vel[0] = sinf(hd) * tv * .8f; W.car.vel[2] = cosf(hd) * tv * .8f; W.car.v = tv * .8f;
  FILE *dump = getenv("CR_DUMP") ? fopen(getenv("CR_DUMP"), "w") : 0;
  int cur = 0, run = 0, runMax = 0, two = 0, minW = 4; float rmax = 0, vmax = 0, rAtMax = 0;
  for (int fr = 0; fr < 20 * 60 && cur < n - 3; fr++) {
    float bd = 1e18f; for (int i = cur; i < n && i < cur + 12; i++) { float dx = PX[i] - W.car.x, dz = PZ[i] - W.car.z, d = dx * dx + dz * dz; if (d < bd) { bd = d; cur = i; } }
    int ti = cur + 4 >= n ? n - 1 : cur + 4; float ang = atan2f(PX[ti] - W.car.x, PZ[ti] - W.car.z), err = ang - W.car.h; while (err > 3.14159f) err -= 6.28318f; while (err < -3.14159f) err += 6.28318f;
    static float gain = -1.f; if (gain < 0.f) gain = getenv("CR_GAIN") ? (float)atof(getenv("CR_GAIN")) : 1.5f; float st = err * gain; if (st > 1.f) st = 1.f; if (st < -1.f) st = -1.f; float vh = hypotf(W.car.vel[0], W.car.vel[2]);
    unsigned btn = vh < tv ? GP_R : 0;
    if (getenv("CR_PS2")) {   // водитель PS2-наблюдателя (run_drive.py): ориентир на 3 точки вперёд, руль 2,5×ошибка, целевая скорость v·max(0,35; cos ошибки), тормоз при v > цель + 5 м/с
      int tj = cur + 3 >= n ? n - 1 : cur + 3; float a2 = atan2f(PX[tj] - W.car.x, PZ[tj] - W.car.z), e2 = a2 - W.car.h; while (e2 > 3.14159f) e2 -= 6.28318f; while (e2 < -3.14159f) e2 += 6.28318f;
      st = e2 * 2.5f; if (st > 1.f) st = 1.f; if (st < -1.f) st = -1.f; float vt = tv * fmaxf(.35f, cosf(e2)); btn = vh < vt ? GP_R : (vh > vt + 5.f ? GP_L : 0); }
    bwStep(&W, btn, st, 0.f); const Car *c = &W.car; float roll = fabsf(c->roll) * 57.2958f, v = c->v * 3.6f;
    if (dump) fprintf(dump, "%d %.3f %.3f %.3f %.3f %.3f %d\n", fr, c->x, c->z, c->v, c->roll * 57.2958f, c->h, c->wheels);
    if (roll > rmax) { rmax = roll; rAtMax = v; } if (v > vmax) vmax = v; if (c->wheels < minW) minW = c->wheels;
    int tw = c->grounded && c->wheels > 0 && c->wheels <= 2 && roll > 12.f; run = tw ? run + 1 : 0; if (run > runMax) runMax = run; two += tw; }
  printf("крен макс %.1f° (на %.0f км/ч), кадров на двух колёсах %d (подряд %d), мин колёс %d, v макс %.0f км/ч, прошла точек %d из %d\n", rmax, rAtMax, two, runMax, minW, vmax, cur, n); return 0; }
