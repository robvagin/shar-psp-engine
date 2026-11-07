// ТЕСТ90 (g01-cars): общая часть матриц кусков машины считается один раз на сустав и масштаб в кадре (carVisMtxScope), числа те же бит в бит.
// Печатает: расхождений матриц с расчётом заново (должно быть 0) и сколько раз общая часть считалась с нуля за кадр. Сборка и прогон: tests/g01cars/carmtx.sh
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "carvis.h"
#include "carparams.h"
int main(void) {
  static CarVis v; Car c; memset(&c, 0, sizeof c);
  v.ok = 1; v.njoint = 6; v.lift = .1f; v.huskLift = .05f; v.maxSteer = .5f; for (int j = 0; j < 6; j++) { v.joint[j].x = j * .1f; v.joint[j].y = .2f; v.joint[j].z = -j * .3f; }
  for (int i = 0; i < 4; i++) { v.wheelJ[i] = i + 1; v.wheelUp[i] = 1e9f; }
  v.nhinge = 1; v.hinge[0].joint = 5; v.hinge[0].axis = 0; v.hinge[0].sign = 1; v.th[0] = .3f;
  c.eq = .1f; c.lim = .3f; const int FR = 300, PIECES = 36; unsigned long diff = 0, pieces = 0, core = 0;
  for (int f = 0; f < FR; f++) {
    c.x = f * .3f; c.y = 1.f + .01f * sinf(f); c.z = f * .1f; float a = f * .02f; c.q[0] = cosf(a); c.q[1] = 0; c.q[2] = sinf(a); c.q[3] = 0; c.steer = sinf(f * .1f) * .8f; c.modelRaise = .02f;
    for (int i = 0; i < 4; i++) { c.comp[i] = .1f + .02f * sinf(f + i); v.spin[i] += .3f; }
    v.th[0] = .3f + .01f * f;
    float ref[PIECES][16];
    carVisMtxScope(0, 0, 0);   // без кэша — эталон
    for (int k = 0; k < PIECES; k++) { int var = k % 11 == 10; carVisPieceMatrixEx(&v, &c, var ? 0 : k % 6, var, .1f * k, .2f, -.1f * k, 1.f + .01f * k, k % 7 == 6, ref[k]); }
    unsigned long c0 = carMtxCoreN;
    carVisMtxScope(&v, &c, 1);
    for (int k = 0; k < PIECES; k++) {
      float m[16]; int var = k % 11 == 10; carVisPieceMatrixEx(&v, &c, var ? 0 : k % 6, var, .1f * k, .2f, -.1f * k, 1.f + .01f * k, k % 7 == 6, m); pieces++;
      if (memcmp(m, ref[k], sizeof m)) diff++;
    }
    carVisMtxScope(0, 0, 0); core += carMtxCoreN - c0;
  }
  printf("кусков %lu; матриц, отличных от расчёта заново: %lu; общая часть с нуля %.2f раз за кадр из %d кусков\n", pieces, diff, (double)core / FR, PIECES);
  return diff == 0 && (double)core / FR < PIECES * .5 ? 0 : 1;
}
