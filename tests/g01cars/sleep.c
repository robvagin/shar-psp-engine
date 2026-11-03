// сверка g01-cars №1159: чужая машина (трафик, ИИ) после удара и при медленной езде — то же поведение дешевле (сон стоянки, крупный шаг у медленных)
//   сборка и прогон: tests/g01cars/sleep.sh ; печатает по сценариям: путь до остановки, время остановки, где встала, крен/тангаж, подшагов за кадр
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "car.h"
#include "carparams.h"
#include "costnames.h"
unsigned long cntQ[64], cntSub, cntCalls;
static struct { ColHdr h; ColTri t[2]; unsigned start[2], refs[2]; } F;
static ColWorld W;
static void tri(int i, float ax, float az, float bx, float bz, float cx, float cz) { F.t[i] = (ColTri){{ax, 0, az}, {bx, 0, bz}, {cx, 0, cz}, {0, 1, 0}}; }
int main(int argc, char **argv) {
  const char *name = argc > 1 ? argv[1] : "famil_v"; float dt = argc > 2 ? 1.f / (float)atoi(argv[2]) : 1.f / 30.f;
  memset(&F, 0, sizeof F); memcpy(F.h.magic, "COL1", 4); F.h.ox = F.h.oz = -2000; F.h.cell = 4000; F.h.W = F.h.H = 1; F.h.ntri = F.h.nref = 2;
  tri(0, -2000, -2000, 2000, 2000, 2000, -2000); tri(1, -2000, -2000, -2000, 2000, 2000, 2000);
  F.start[1] = 2; F.refs[0] = 0; F.refs[1] = 1;
  if (colInit(&W, &F, sizeof F)) return 2;
  CarParams p; carParamsFor(name, &p);
  // сценарии: v0 м/с, газ, ручник, секунд
  static const struct { const char *n; float v0, th, hb, secs; } S[] = {
    {"после удара 8 м/с, ручник (трафик)", 8.f, 0.f, 1.f, 20.f}, {"после удара 3 м/с, ручник", 3.f, 0.f, 1.f, 20.f},
    {"катится 3 м/с, без ручника", 3.f, 0.f, 0.f, 30.f}, {"с места газ 0,5 (ИИ в очереди)", 0.f, .5f, 0.f, 8.f}, {"стоит с ручником", 0.f, 0.f, 1.f, 10.f}};
  printf("%s, кадр %.0f Гц\n", name, 1.f / dt);
  for (int s = 0; s < 5; s++) {
    Car c; carInit(&c, &p, &W, 0, 0, 0, 0.f);
    CarInput z = {0, 0, 0, 0}; for (int f = 0; f < 120; f++) carUpdate(&c, &W, &z, 1.f / 60.f);
    float x0 = c.x, z0 = c.z; carSetSpeed(&c, S[s].v0);
    CarInput in = {S[s].th, 0, 0, S[s].hb}; int N = (int)(S[s].secs / dt), stopF = -1; float vmax = 0, rollMax = 0, pitchMax = 0; unsigned long sub0 = cntSub;
    for (int f = 0; f < N; f++) {
      carUpdate(&c, &W, &in, dt); float v = fabsf(c.v);
      if (v > vmax) vmax = v; if (fabsf(c.roll) > rollMax) rollMax = fabsf(c.roll); if (fabsf(c.pitch) > pitchMax) pitchMax = fabsf(c.pitch);
      if (S[s].v0 > 0 && stopF < 0 && v < .05f) stopF = f;
    }
    printf("  %-38s путь %.3f м, остановка %s%.2f с, конец v=%.3f, крен/тангаж макс %.2f/%.2f°, подшагов за кадр %.3f (всего %lu)\n", S[s].n, hypotf(c.x - x0, c.z - z0),
           stopF < 0 ? "нет " : "", stopF < 0 ? 0.f : stopF * dt, fabsf(c.v), rollMax * 57.29578f, pitchMax * 57.29578f, (cntSub - sub0) / (double)N, cntSub - sub0);
  }
  return 0;
}
