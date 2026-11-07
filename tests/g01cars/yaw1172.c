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
int main(int argc, char **argv) {
  const char *name = argc > 1 ? argv[1] : "famil_v"; float dt = 1.f / 30.f;
  world(1000.f, .15f);
  static const float V[] = {8.f, 11.f, 14.3f, 18.f, 22.f, 26.f, 30.f, 36.f};
  printf("%s: полный руль, газ держит скорость; рыскание °/с на 1,0 с и 2,0 с, бета° на 1,0 с\n", name);
  for (int vi = 0; vi < 8; vi++) {
    CarParams p; if (!carParamsFor(name, &p)) return 2;
    Car c; carInit(&c, &p, &W, 0, 0, 0, 0); CarInput in = {1.f, 0.f, 0.f, 0.f};
    for (int f = 0; f < 180; f++) carUpdate(&c, &W, &in, 1.f / 60.f);
    carSetSpeed(&c, V[vi]); float w1 = 0, w2 = 0, b1 = 0, vend = 0; in.steer = 1.f;
    for (int f = 0; f < 60; f++) { in.throttle = c.v < V[vi] ? 1.f : 0.f; carUpdate(&c, &W, &in, dt); if (f == 29) { w1 = fabsf(c.w[1]) * 57.2958f; b1 = fabsf(atan2f(c.vl, fmaxf(.5f, fabsf(c.v)))) * 57.2958f; } }
    w2 = fabsf(c.w[1]) * 57.2958f; vend = c.v;
    printf("  %4.1f м/с: рыскание 1с %5.1f 2с %5.1f, бета %4.1f°, скорость к 2с %.1f\n", V[vi], w1, w2, b1, vend);
  }
  return 0;
}
