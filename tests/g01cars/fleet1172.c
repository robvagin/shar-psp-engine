// №1172 (по мотивам slide.c №1126 Гомер L7): занос на скорости 25–35 м/с, с ручником и без, с газом и без, на ровном и у бордюра (0,15 м, по бокам на расстоянии 10 и 20 м)
//   cc -std=c11 -O2 -I viewer -DSV_FORCE_PLAYER=1 tests/g01cars/slide.c viewer/car.c viewer/col.c -lm -o slide && ./slide [машина]
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[10]; unsigned start[2], refs[10]; } F;
static ColWorld W;
static void tri(int i, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float nx, float ny, float nz) {
  F.t[i] = (ColTri){{ax, ay, az}, {bx, by, bz}, {cx, cy, cz}, {nx, ny, nz}};
}
static void world(float D, float H) {   // D — расстояние до бордюра по бокам, H — высота
  memset(&F, 0, sizeof F); memcpy(F.h.magic, "COL1", 4);
  F.h.ox = F.h.oz = -2000; F.h.cell = 4000; F.h.W = F.h.H = 1;
  float L = 2000.f; int n = 0;
  tri(n++, -D, 0, -L, D, 0, L, D, 0, -L, 0, 1, 0); tri(n++, -D, 0, -L, -D, 0, L, D, 0, L, 0, 1, 0);          // дорога
  tri(n++, D, H, -L, L, H, L, L, H, -L, 0, 1, 0); tri(n++, D, H, -L, D, H, L, L, H, L, 0, 1, 0);              // тротуар справа
  tri(n++, -L, H, -L, -D, H, L, -D, H, -L, 0, 1, 0); tri(n++, -L, H, -L, -L, H, L, -D, H, L, 0, 1, 0);        // тротуар слева
  tri(n++, D, 0, -L, D, H, L, D, H, -L, -1, 0, 0); tri(n++, D, 0, -L, D, 0, L, D, H, L, -1, 0, 0);            // бордюр справа
  tri(n++, -D, 0, -L, -D, H, -L, -D, H, L, 1, 0, 0); tri(n++, -D, 0, -L, -D, H, L, -D, 0, L, 1, 0, 0);       // бордюр слева
  F.h.ntri = F.h.nref = n; F.start[0] = 0; F.start[1] = n; for (int i = 0; i < n; i++) F.refs[i] = i;
  int rc = colInit(&W, &F, sizeof F); if (rc) { printf("colInit %d\n", rc); exit(2); }
}

// №1172: занос на ровной дороге: поворот на скорости, отпускание руля, торможение в повороте. Печатает одну строку итога
// №1172: срыв в боковой занос на скорости. Для каждой машины (имена — аргументы): скорость 0,45 / 0,7 / 0,9 от TopSpeed, полный руль 2 с, газ держит скорость.
// «Срыв»: скорость за 2 с упала больше чем на 25 % (машину развернуло боком и она гасит ход) или угол между ходом и носом больше 20°. Печать: по машинам с срывом и итог
int main(int argc, char **argv) {
  float dt = 1.f / 30.f; world(1000.f, .15f); static const float K[] = {.45f, .7f, .9f}; int bad = 0, runs = 0, badCars = 0; int verbose = getenv("V") != 0;
  for (int ai = 1; ai < argc; ai++) {
    CarParams p; if (!carParamsFor(argv[ai], &p)) continue; int carBad = 0; char line[256]; line[0] = 0;
    for (int ki = 0; ki < 3; ki++) for (int sg = -1; sg <= 1; sg += 2) {
      float V = K[ki] * p.topSpeed; Car c; carInit(&c, &p, &W, 0, 0, 0, 0); CarInput in = {1.f, 0.f, 0.f, 0.f};
      for (int f = 0; f < 240; f++) carUpdate(&c, &W, &in, 1.f / 60.f);
      carSetSpeed(&c, V); in.steer = (float)sg; float bmax = 0; int flip = 0;
      for (int f = 0; f < 60; f++) { in.throttle = c.v < V ? 1.f : 0.f; carUpdate(&c, &W, &in, dt); float be = fabsf(atan2f(c.vl, fmaxf(.5f, fabsf(c.v)))) * 57.2958f; if (f > 10 && be > bmax) bmax = be;
        float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]); if (up < 0.f || c.rights) flip = 1; }
      int sp = c.v < .75f * V || bmax > 20.f || flip; runs++; if (sp) { bad++; carBad = 1; char t[48]; snprintf(t, sizeof t, " [%.0f%% скор=%.0f%% бета=%.0f°%s]", 100.f * K[ki], 100.f * c.v / V, bmax, flip ? " перев" : ""); strncat(line, t, sizeof line - strlen(line) - 1); }
    }
    badCars += carBad; if (verbose && carBad) printf("  %s%s\n", argv[ai], line);
  }
  printf("срыв в занос: %d из %d заездов, машин со срывом %d\n", bad, runs, badCars); return 0;
}
