// сверка g01-cars №1159: кэш матриц куска в carvis.c — те же числа бит в бит, меньше обходов. Печатает хеш всех матриц и число расчётов матрицы тела и поворота колеса за кадр
//   сборка и прогон: tests/g01cars/cvcache.sh
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "carvis.h"
#include "carparams.h"
unsigned long pmCalls;
void cvPM(const Car *c, float lift, float ox, float oy, float oz, float sc, float m[16]);
int main(void) {
  static CarVis v; Car c; memset(&c, 0, sizeof c);
  v.ok = 1; v.njoint = 6; v.lift = .1f; v.huskLift = .05f; v.maxSteer = .5f; for (int j = 0; j < 6; j++) { v.joint[j].x = j * .1f; v.joint[j].y = .2f; v.joint[j].z = -j * .3f; }
  for (int i = 0; i < 4; i++) { v.wheelJ[i] = i + 1; v.wheelUp[i] = 1e9f; }
  v.nhinge = 1; v.hinge[0].joint = 5; v.hinge[0].axis = 0; v.hinge[0].sign = 1;
  c.eq = .1f; c.lim = .3f; double h = 0; unsigned long pieces = 0; const int FR = 300, PIECES = 36;
  for (int f = 0; f < FR; f++) {
    c.x = f * .3f; c.y = 1.f + .01f * sinf(f); c.z = f * .1f; float a = f * .02f; c.q[0] = cosf(a); c.q[1] = 0; c.q[2] = sinf(a); c.q[3] = 0; c.steer = sinf(f * .1f) * .8f; c.modelRaise = .02f;
    for (int i = 0; i < 4; i++) { c.comp[i] = .1f + .02f * sinf(f + i); v.spin[i] += .3f; }
    for (int k = 0; k < PIECES; k++) {   // как в игре: куски подряд, суставы 0..5, один вариант
      float m[16]; carVisPieceMatrixEx(&v, &c, k % 6, 0, .1f * k, .2f, -.1f * k, 1.f, k % 7 == 6, m); pieces++;
      if (getenv("DUMP") && f < 2) { printf("f%d k%d:", f, k); for (int i = 0; i < 16; i++) printf(" %.5f", m[i]); printf("\n"); }
      for (int i = 0; i < 16; i++) { unsigned u; memcpy(&u, &m[i], 4); h = h * 1.0000003 + u; }
    }
  }
  printf("хеш матриц %.6e; кусков %lu; расчётов матрицы тела %lu (%.2f за кадр)\n", h, pieces, pmCalls, (double)pmCalls / FR);
  return 0;
}
void cvPM(const Car *c, float lift, float ox, float oy, float oz, float sc, float m[16]) { pmCalls++; carPieceMatrix(c, lift, ox, oy, oz, sc, m); }
