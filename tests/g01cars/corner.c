// сверка g01-cars №1126: поворот на скорости по улицам L5 (позиции берутся с хода автопилота: STREETS_POSES), без эмулятора
//   corner LEVEL PLAYER MSN POSES  → по каждой точке и скорости 12/20/28/36 м/с: руль ±1 две секунды, газ по скорости; печатает перевороты и крен
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
float bwDtNow = 1.f / 60.f;
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 5 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "usage: corner LEVEL PLAYER MSN POSES\n"); return 2; }
  FILE *F = fopen(argv[4], "r"); if (!F) return 2; float px[400], py[400], pz[400], ph[400]; int np = 0;
  while (np < 400 && fscanf(F, "%f %f %f %f", px + np, py + np, pz + np, ph + np) == 4) np++;
  fclose(F); bwNew(&W);
  static const float SP[4] = {12.f, 20.f, 28.f, 36.f}; const int sx = 0; (void)sx;
  int tr3[3], *tr = 0; if (getenv("CT") && sscanf(getenv("CT"), "%d,%d,%d", tr3, tr3 + 1, tr3 + 2) == 3) tr = tr3;
  const int gasAlways = getenv("CGAS") != 0;
  int runs[4] = {0}, flips[4] = {0}; double rollSum[4] = {0}, tiltSum[4] = {0}; float rollMax[4] = {0}, tiltMax[4] = {0}, latMax[4] = {0};
  for (int i = 0; i < np; i++) for (int k = 0; k < 4; k++) for (int sg = -1; sg <= 1; sg += 2) {
    float gy; colGround(bwCol(), px[i], pz[i], py[i] + 3.f, -1e6f, &gy, 0); bwPlace(&W, px[i], gy + .3f, pz[i], ph[i]);
    for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
    carSetSpeed(&W.car, SP[k]);
    float rm = 0.f, tm = 0.f; int fl = 0; float x0 = W.car.x, z0 = W.car.z;
    for (int f = 0; f < 240; f++) {   // 2 с руль, ещё 2 с отпущено (газ держит скорость)
      float st = f < 120 ? (float)sg : 0.f; unsigned b = (gasAlways || W.car.v < SP[k]) ? GP_R : 0;   // CGAS=1 — газ держится весь поворот, как у игрока
      bwStep(&W, b, st, 0.f);
      const Car *c = &W.car; float upy = 1.f - 2.f * (c->q[1] * c->q[1] + c->q[3] * c->q[3]); float tilt = acosf(fmaxf(-1.f, fminf(1.f, upy))) * 57.29578f;
      if (tr && tr[0] == i && tr[1] == k && tr[2] == sg && f % 6 == 0) printf("  f%3d tilt %5.1f roll %6.1f v %5.1f рулевой угол %5.1f колёс %d сжатие %.2f %.2f %.2f %.2f com.y %.2f слип %d\n", f, tilt, c->roll * 57.29578f, c->v, c->wheelSteer * 57.29578f, c->wheels, c->comp[0], c->comp[1], c->comp[2], c->comp[3], c->com[1], c->slip);
      if (tilt > tm) tm = tilt; if (fabsf(c->roll) * 57.29578f > rm) rm = fabsf(c->roll) * 57.29578f; if (tilt > 70.f) fl = 1;
    }
    (void)x0; (void)z0;
    if (fl && getenv("CF")) printf("  перевернулась: поза %d (%.0f,%.0f) скорость %d знак %d, наклон макс %.0f\n", i, px[i], pz[i], k, sg, tm);
    runs[k]++; flips[k] += fl; rollSum[k] += rm; tiltSum[k] += tm; if (rm > rollMax[k]) rollMax[k] = rm; if (tm > tiltMax[k]) tiltMax[k] = tm;
  }
  for (int k = 0; k < 4; k++) printf("%2.0f м/с (%3.0f км/ч): прогонов %d, перевёрнулась %d, наклон средний %.1f° макс %.1f°, крен средний %.1f° макс %.1f°\n", SP[k], SP[k] * 3.6f, runs[k], flips[k], tiltSum[k] / runs[k], tiltMax[k], rollSum[k] / runs[k], rollMax[k]);
  return 0;
}
