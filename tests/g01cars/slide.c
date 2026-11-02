// сверка g01-cars №1126 (Гомер L7): занос на скорости 25–35 м/с, с ручником и без, с газом и без, на ровном и у бордюра (0,15 м, по бокам на расстоянии 10 и 20 м)
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
int main(int argc, char **argv) {
  const char *name = argc > 1 ? argv[1] : "famil_v";
  static const float V[] = {25.f, 30.f, 35.f}, DD[] = {1000.f, 8.f, 4.f};
  printf("%s: занос с руля ±1, 4 с; перевороты / наибольший наклон кузова / доля кадров на двух колёсах и меньше\n", name);
  printf("%-5s %-8s %-6s %-6s | %-34s | %s\n", "м/с", "бордюр", "ручник", "газ", "перевороты (из 2 сторон), наклон макс°, 2 колеса %", "");
  int totalFlips = 0, totalRuns = 0;
  for (int di = 0; di < 3; di++) for (int vi = 0; vi < 3; vi++) for (int hb = 0; hb < 2; hb++) for (int gas = 0; gas < 2; gas++) {
    world(DD[di], getenv("CURBH") ? (float)atof(getenv("CURBH")) : .15f); int fl = 0; float tm = 0.f, tw = 0.f, mx = 0.f;
    for (int sg = -1; sg <= 1; sg += 2) {
      CarParams p; if (!carParamsFor(name, &p)) { puts("нет машины"); return 2; }
      Car c; carInit(&c, &p, &W, 0, 0, 0, 0); CarInput in = {1.f, 0.f, 0.f, 0.f};
      for (int f = 0; f < 60 * 3; f++) carUpdate(&c, &W, &in, 1.f / 60.f);
      carSetSpeed(&c, V[vi]); in.steer = (float)sg; in.handbrake = hb ? 1.f : 0.f; in.throttle = gas ? 1.f : 0.f;
      int n = 0, two = 0, flip = 0;
      for (int f = 0; f < 60 * 4; f++) {
        carUpdate(&c, &W, &in, 1.f / 30.f > 0.f ? 1.f / 60.f : 0.f); n++;
        float up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]); float tilt = acosf(fmaxf(-1.f, fminf(1.f, up))) * 57.2958f;
        if (tilt > tm) tm = tilt; if (fabsf(c.x) > mx) mx = fabsf(c.x); if (up < 0.f || c.rights) flip = 1; two += c.wheels <= 2;
      }
      fl += flip; tw += 100.f * two / n;
    }
    totalFlips += fl; totalRuns += 2;
    printf("%-5.0f %-8s %-6s %-6s | перевороты %d из 2, наклон макс %5.1f°, на 2 колёсах %4.1f%%, боком ушла до %.0f м\n", V[vi], DD[di] > 100 ? "нет" : DD[di] > 6.f ? "8 м" : "4 м", hb ? "да" : "нет", gas ? "да" : "нет", fl, tm, tw / 2, mx);
  }
  printf("ИТОГ %s: переворотов %d из %d заездов\n", name, totalFlips, totalRuns);
  return 0;
}
