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
  const char *name = argc > 1 ? argv[1] : "famil_v"; float dt = argc > 2 ? 1.f / (float)atoi(argv[2]) : 1.f / 30.f;
  world(1000.f, .15f);
  static const float V[] = {12.f, 18.f, 25.f};
  static const struct { const char *n; float steer, thr, brk; } S[] = {{"газ", 1.f, 1.f, 0.f}, {"накат", 1.f, 0.f, 0.f}, {"тормоз", 1.f, 0.f, 1.f}, {"пол-руля", .5f, 1.f, 0.f}};
  for (int si = 0; si < 4; si++) {
    double bm = 0, ba = 0, wa = 0, hd = 0, air = 0, vyM = 0, cmp = 0; long nfr = 0; int fl = 0, nr = 0;
    for (int vi = 0; vi < 3; vi++) for (int sg = -1; sg <= 1; sg += 2) {
      CarParams p; if (!carParamsFor(name, &p)) { puts("нет машины"); return 2; }
      Car c; carInit(&c, &p, &W, 0, 0, 0, 0); CarInput in = {1.f, 0.f, 0.f, 0.f};
      for (int f = 0; f < 180; f++) carUpdate(&c, &W, &in, 1.f / 60.f);
      carSetSpeed(&c, V[vi]); float h0 = c.h; int nf = (int)(1.5f / dt + .5f), nr2 = (int)(1.0f / dt + .5f);
      float bmax = 0; int flip = 0;
      for (int f = 0; f < nf + nr2 + (int)(1.f / dt); f++) {
        int steerPh = f < nf; in.steer = steerPh ? sg * S[si].steer : 0.f; in.throttle = steerPh ? S[si].thr : 1.f; in.brake = steerPh ? S[si].brk : 0.f;
        carUpdate(&c, &W, &in, dt);
        float be = fabsf(atan2f(c.vl, fmaxf(.5f, fabsf(c.v)))) * 57.2958f; if (steerPh && be > bmax) bmax = be;
        float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]); if (up < 0.f || c.rights) flip = 1;
        air += c.wheels < 4; nfr++; if (fabsf(c.vel[1]) > vyM) vyM = fabsf(c.vel[1]); cmp += (c.comp[0] + c.comp[1] + c.comp[2] + c.comp[3]) * .25f;
        if (f == nf + nr2 - 1) { ba += be; wa += fabsf(c.w[1]) * 57.2958f; }
      }
      bm += bmax; fl += flip; nr++; hd += fabsf(c.h - h0) * 57.2958f;
    }
    printf("%s: бета_макс %.1f° бета_через_1с_после_руля %.1f° рыскание_через_1с %.0f°/с поворот %.0f° перев %d, колёс<4 %.1f%% vy_макс %.2f сжатие %.3f | ", S[si].n, bm / nr, ba / nr, wa / nr, hd / nr, fl, 100. * air / nfr, vyM, cmp / nfr);
  }
  printf("\n"); return 0;
}
