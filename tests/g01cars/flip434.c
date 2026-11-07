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


// Правка 434 (Роб 06.11, L4: «машина Мардж постоянно переворачивается, ездить невозможно»). Серия заездов на каждой машине флота (семейство высоких машин, п.2 канона):
// манёвры М1 полный руль на 0,45/0,7/0,9 от TopSpeed; М2 заезд на бордюр 0,15 и 0,3 м под 25° на 0,7 TopSpeed; М3 слалом ±1 каждые 0,7 с на 0,7; М4 рывок руля туда-обратно на 0,8;
// М5 полный руль и тормоз на 0,7. Переворот = ось вверх тела ниже 0,3 (более 72°) хотя бы в одном кадре, либо машину пришлось ставить на колёса (rights).
// Печать: по машинам число переворотов из числа заездов; итог флота. V=1 — подробно. cc -std=c99 -O2 -w -I viewer -DSV_FORCE_PLAYER=1 -DSV_AIRWHEEL_OLD=0 tests/g01cars/flip434.c viewer/col.c viewer/car.c -lm
static float TH = .3f;
typedef struct { const char *name; int n, flips; float worstUp; } Res;
static int runOne(const CarParams *p, int man, float K, int sg, float curbH, float *worst) {
  float dt = 1.f / 30.f; world(man == 2 ? 3.f : 1000.f, curbH);
  float V = K * p->topSpeed; Car c; carInit(&c, p, &W, 0, 0, 0, man == 2 ? .43f * sg : 0.f); CarInput in = {1.f, 0.f, 0.f, 0.f};
  for (int f = 0; f < 240; f++) carUpdate(&c, &W, &in, 1.f / 60.f);
  carSetSpeed(&c, V); int tip = 0; float up_min = 1.f; int N = 90;
  for (int f = 0; f < N; f++) {
    float t = f * dt; in.throttle = c.v < V ? 1.f : 0.f; in.brake = 0.f;
    if (man == 1) in.steer = (float)sg;
    else if (man == 3) in.steer = ((int)(t / .7f) & 1) ? -1.f * sg : 1.f * sg;
    else if (man == 4) in.steer = t < .5f ? (float)sg : t < 1.7f ? -(float)sg : 0.f;
    else if (man == 5) { in.steer = (float)sg; if (t > .5f) { in.throttle = 0.f; in.brake = 1.f; } }
    else in.steer = 0.f;
    carUpdate(&c, &W, &in, dt);
    float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]); if (up < up_min) up_min = up; if (up < TH) tip = 1;
  }
  if (c.rights) tip = 1; if (up_min < *worst) *worst = up_min; return tip;
}
int main(int argc, char **argv) {
  TH = getenv("TH") ? (float)atof(getenv("TH")) : .3f; int verbose = getenv("V") != 0, totRuns = 0, totTip = 0, badCars = 0;
  static const float K1[] = {.45f, .7f, .9f};
  for (int ai = 1; ai < argc; ai++) {
    CarParams p; if (!carParamsFor(argv[ai], &p)) continue; int runs = 0, tips = 0; float worst = 1.f; char what[160]; what[0] = 0;
    for (int sg = -1; sg <= 1; sg += 2) {
      for (int k = 0; k < 3; k++) { int r = runOne(&p, 1, K1[k], sg, .15f, &worst); runs++; tips += r; if (r) { char t[16]; snprintf(t, sizeof t, " М1/%.0f", 100 * K1[k]); strncat(what, t, sizeof what - strlen(what) - 1); } }
      for (int h = 0; h < 2; h++) { int r = runOne(&p, 2, .7f, sg, h ? .3f : .15f, &worst); runs++; tips += r; if (r) strncat(what, h ? " М2/.3" : " М2/.15", sizeof what - strlen(what) - 1); }
      for (int m = 3; m <= 5; m++) { int r = runOne(&p, m, m == 4 ? .8f : .7f, sg, .15f, &worst); runs++; tips += r; if (r) { char t[8]; snprintf(t, sizeof t, " М%d", m); strncat(what, t, sizeof what - strlen(what) - 1); } }
    }
    totRuns += runs; totTip += tips; badCars += tips > 0;
    if (verbose || tips) printf("  %-14s перевернулась %2d из %2d  (низшая ось вверх %.2f)%s\n", argv[ai], tips, runs, worst, what);
  }
  printf("ПЕРЕВОРОТОВ: %d из %d заездов, машин с переворотом %d\n", totTip, totRuns, badCars); return 0;
}
