// сверка g01-cars Д5/4.4: удар машины о высокую стену на 30 / 60 / 90 км/ч, в лоб и под 30°, против записи PS2 (седан, забор f008 L1; ps2-twin-wheel-fence-hits)
//   cc -std=c11 -O2 -I viewer -DSV_FORCE_PLAYER=1 tests/g01cars/fence.c viewer/car.c viewer/col.c -lm -o fence && ./fence
// PS2 (такт 40 мс): скорость после первого такта в лоб 2,2/3,3/4,7 м/с при входе 11,8/16,8/24,3; через 10 тактов 0,9/0,3/2,4; отдача назад −0,63/−0,89/−1,48;
// подброс vy до +1,5/+2,4/+4,1 (до удара +2,0…2,4, склон), подъём ЦМ 0,00/0,17/0,80 м, up.y не ниже 0,980/0,955/0,850.
// Под 30°: вход 11,4/14,8/22,4: первый такт 2,1/3,3/5,5, через 10 тактов 0,8/1,8/2,9, отдача −1,45/−2,68/−2,41, vy до +1,5/+2,0/+3,4, подъём ЦМ 0,02/0,04/0,17
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
static struct { ColHdr h; ColTri t[4]; unsigned start[2], refs[4]; } F;
static ColWorld W;
static void tri(int i, float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, float nx, float ny, float nz) { F.t[i] = (ColTri){{ax, ay, az}, {bx, by, bz}, {cx, cy, cz}, {nx, ny, nz}}; }
int main(int argc, char **argv) {
  const char *name = argc > 1 ? argv[1] : "famil_v"; const float ZW = 40.f;
  memset(&F, 0, sizeof F); memcpy(F.h.magic, "COL1", 4); F.h.ox = F.h.oz = -2000; F.h.cell = 4000; F.h.W = F.h.H = 1; F.h.ntri = F.h.nref = 4;
  tri(0, -2000, 0, -2000, 2000, 0, 2000, 2000, 0, -2000, 0, 1, 0); tri(1, -2000, 0, -2000, -2000, 0, 2000, 2000, 0, 2000, 0, 1, 0);
  tri(2, -500, 0, ZW, 500, 4, ZW, 500, 0, ZW, 0, 0, -1); tri(3, -500, 0, ZW, -500, 4, ZW, 500, 4, ZW, 0, 0, -1);
  F.start[1] = 4; for (int i = 0; i < 4; i++) F.refs[i] = i;
  if (colInit(&W, &F, sizeof F)) return 2;
  static const float V[] = {11.8f, 16.8f, 24.3f}, V30[] = {11.4f, 14.8f, 22.4f};
  printf("%s: удар о стену; такт 40 мс; скорость вдоль входа после 1-го такта удара / через 10 тактов / отдача / подброс vy / подъём ЦМ / наименьший up.y\n", name);
  for (int ang = 0; ang < 2; ang++) for (int vi = 0; vi < 3; vi++) {
    float v0 = ang ? V30[vi] : V[vi];
    CarParams p; carParamsFor(name, &p); Car c;
    float hbest = 0.f;
    for (int k = 0; k < 4; k++) { float h = k * 1.5708f; carInit(&c, &p, &W, 0, 0, 0, h); for (int f = 0; f < 120; f++) { CarInput z = {0, 0, 0, 0}; carUpdate(&c, &W, &z, 1.f / 60.f); } carSetSpeed(&c, 5.f); if (c.vel[2] > 4.5f) { hbest = h; break; } }
    float h = hbest + (ang ? 30.f * 0.017453f : 0.f);
    carInit(&c, &p, &W, ang ? -(ZW - 12.f) * tanf(30.f * 0.017453f) : 0.f, 0, ZW - 12.f - 10.f, h);   // 12 м до стены
    CarInput in = {0, 0, 0, 0};
    for (int f = 0; f < 60; f++) carUpdate(&c, &W, &in, 1.f / 60.f);
    carSetSpeed(&c, v0);
    float yIn = c.y; float dirx = c.vel[0] / hypotf(c.vel[0], c.vel[2]), dirz = c.vel[2] / hypotf(c.vel[0], c.vel[2]);
    float vy0 = c.vel[1]; int hit = -1; float vFirst = -1, v10 = -1, rebound = 0, vyMax = -9, riseMax = -9, upMin = 1;
    for (int t = 0; t < 40; t++) {   // такты 40 мс
      carUpdate(&c, &W, &in, 0.04f);
      float vn = c.vel[0] * dirx + c.vel[2] * dirz, up = 1.f - 2.f * (c.q[1] * c.q[1] + c.q[3] * c.q[3]);
      if (hit < 0 && vn < .7f * v0) { hit = t; vFirst = vn; }
      if (hit >= 0) { if (t == hit + 10) v10 = vn; if (vn < rebound) rebound = vn; if (c.vel[1] - vy0 > vyMax) vyMax = c.vel[1] - vy0; if (c.y - yIn > riseMax) riseMax = c.y - yIn; if (up < upMin) upMin = up; }
    }
    printf("%s %2.0f км/ч (вход %4.1f): 1-й такт %4.1f, через 10 тактов %4.1f, отдача %5.2f, подброс %+4.1f, подъём ЦМ %4.2f, up.y мин %.3f\n", ang ? "30° " : "лоб ", v0 * 3.6f, v0, vFirst, v10, rebound, vyMax, riseMax, upMin);
  }
  return 0;
}
