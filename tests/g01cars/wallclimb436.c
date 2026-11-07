// №1172 (по мотивам slide.c №1126 Гомер L7): занос на скорости 25–35 м/с, с ручником и без, с газом и без, на ровном и у бордюра (0,15 м, по бокам на расстоянии 10 и 20 м)
// (взято из flip434.c)
//   cc -std=c11 -O2 -I viewer -DSV_FORCE_PLAYER=1 tests/g01cars/slide.c viewer/car.c viewer/col.c -lm -o slide && ./slide [машина]
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[4]; unsigned start[2], refs[4]; } F;
static ColWorld W;
static void tri(int i, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float nx, float ny, float nz) {
  F.t[i] = (ColTri){{ax, ay, az}, {bx, by, bz}, {cx, cy, cz}, {nx, ny, nz}};
}
static void world(float WZ) {   // пол + вертикальная стена на z = WZ (нормаль к -z, высота 8 м)
  memset(&F, 0, sizeof F); memcpy(F.h.magic, "COL1", 4);
  F.h.ox = F.h.oz = -2000; F.h.cell = 4000; F.h.W = F.h.H = 1;
  float L = 2000.f; int n = 0;
  tri(n++, -L, 0, -L, L, 0, L, L, 0, -L, 0, 1, 0); tri(n++, -L, 0, -L, -L, 0, L, L, 0, L, 0, 1, 0);
  tri(n++, -L, 0, WZ, L, 8, WZ, L, 0, WZ, 0, 0, -1); tri(n++, -L, 0, WZ, -L, 8, WZ, L, 8, WZ, 0, 0, -1);
  F.h.ntri = F.h.nref = n; F.start[0] = 0; F.start[1] = n; for (int i = 0; i < n; i++) F.refs[i] = i;
  int rc = colInit(&W, &F, sizeof F); if (rc) { printf("colInit %d\n", rc); exit(2); }
}
// Правка 436 (находка Кадров 07.11: L2, rocke_v на автопилоте «газ в пол» кувыркается). Машина едет на стену под курсом YAW (0 — в лоб) на скорости K·TopSpeed с газом в пол и полным рулём в сторону стены или без руля
// и стоит у стены ещё 6 с с газом. Мерим: наибольший наклон кузова (°), наибольший подъём центра над землёй (м). Заход «в лоб газ в пол» не должен ставить машину на нос/корму или лезть вверх по стене.
// cc -std=c99 -O2 -w -I viewer -DSV_FORCE_PLAYER=1 tests/g01cars/wallclimb436.c viewer/col.c viewer/car.c -lm
static void run(const CarParams *p, float K, float yaw, float steer, float h0, float *tilt, float *rise) {
  float dt = 1.f / 30.f; float V0 = K * p->topSpeed; world(60.f + (h0 > 0.f ? V0 * sqrtf(2.f * h0 / 9.8f) * cosf(yaw) : 0.f)); Car c; carInit(&c, p, &W, 0, 0, 0, yaw); CarInput in = {1.f, 0.f, 0.f, 0.f};   // курс 0 — по +z к стене
  for (int f = 0; f < 240; f++) { CarInput z = {0, 0, 0, 0}; carUpdate(&c, &W, &z, 1.f / 60.f); }
  float V = K * p->topSpeed; carSetSpeed(&c, V); float y0 = c.y; c.y += h0; *tilt = 0.f; *rise = 0.f;
  for (int f = 0; f < 30 * 14; f++) {
    in.throttle = 1.f; in.brake = 0.f; in.steer = steer; carUpdate(&c, &W, &in, dt);
    float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]); float tl = acosf(fmaxf(-1.f, fminf(1.f, up))) * 57.29578f;
    if (tl > *tilt) *tilt = tl; if (c.y - y0 > *rise) *rise = c.y - y0;
    if (getenv("V") && f % 3 == 0 && f < 200) printf("   f=%d v %.1f y %.2f тангаж %.0f крен %.0f колёс %d нзем %d z %.2f конт %d wedge %.2f/%d perch %.2f st32 %d\n", f, c.v, c.y - y0, c.pitch * 57.29578f, c.roll * 57.29578f, c.wheels, c.grounded, c.z, c.contacts, c.wedgeT, c.wedgeTried, c.perchT, c.stuck32);
  }
}
int main(int argc, char **argv) {
  if (getenv("ONE")) { char nm[32]; float K, yw, st, h0, tl, rs; if (sscanf(getenv("ONE"), "%31[^,],%f,%f,%f,%f", nm, &K, &yw, &st, &h0) == 5) { CarParams p; if (carParamsFor(nm, &p)) { if (getenv("NOWHEELIE")) p.wheelieRange = 0.f; setenv("V", "1", 1); run(&p, K, yw, st, h0, &tl, &rs); printf("наклон %.1f подъём %.2f\n", tl, rs); } } return 0; }
  int bad = 0, runs = 0; float lim = getenv("LIM") ? (float)atof(getenv("LIM")) : 45.f;
  for (int ai = 1; ai < argc; ai++) {
    CarParams p; if (!carParamsFor(argv[ai], &p)) continue; if (getenv("NOWHEELIE")) p.wheelieRange = 0.f; float wt = 0.f, wr = 0.f; char what[200]; what[0] = 0;
    for (int hi = 0; hi < 4; hi++) for (int ki = 0; ki < 3; ki++) for (int yi = 0; yi < 3; yi++) for (int si = 0; si < 3; si++) {
      float K = (float[]){.3f, .6f, 1.f}[ki], yaw = (float[]){0.f, .35f, .8f}[yi], st = (float[]){0.f, -1.f, 1.f}[si], h0 = (float[]){0.f, .4f, 1.f, 2.f}[hi], tl, rs;
      run(&p, K, yaw, st, h0, &tl, &rs); runs++; if (tl > wt) wt = tl; if (rs > wr) wr = rs;
      if (tl > lim) { bad++; char t[32]; snprintf(t, sizeof t, " K%.1f/yaw%.2f/st%.0f/h%.1f", K, yaw, st, h0); if (strlen(what) < 120) strncat(what, t, 60); }
    }
    printf("  %-12s макс наклон %5.1f°, макс подъём центра %5.2f м%s\n", argv[ai], wt, wr, wt > lim ? "  <- ПЛОХО" : "");
    if (wt > lim && getenv("SHOW")) printf("      %s\n", what);
  }
  printf("ЗАХОДОВ с наклоном > %.0f°: %d из %d\n", lim, bad, runs); return 0;
}
