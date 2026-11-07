// Что держит машину в точке (ночные боты 05.11: «12 с без приближения к цели»): из 8 направлений за 14 м машина уровня идёт на газу к точке целым кадром игры (tools/bots/world.h), 5 с.
// Печать по заезду: старт, где остановилась, пройдено, скорость, статическая коллизия в точке остановки (глубина, примитив COL1 t/b/c/s + номер) и ближайшие динамические: стейтпроп (номер, тип, класс, ATC, расстояние)
// и проп PRP1 (номер, тип, расстояние); итог — сводка по причинам.   placestuck X Z [РАДИУС=14] (cwd с build/level.shr, build/homer.shr)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
#include "stateprops.h"
int main(int argc, char **argv) {
  if (argc < 3) return 2; float px = atof(argv[1]), pz = atof(argv[2]), R = argc > 3 ? atof(argv[3]) : 14.f; setenv("BW_NO_TRAFFIC", "1", 1);
  if (bwData("build/level.shr", "build/homer.shr", 0)) return 2; static BotWorld W; static BotSnap S; if (bwNew(&W)) return 2; bwSaveAlloc(&W, &S); bwSave(&W, &S); const ColWorld *col = bwCol();
  float gp; float nn[3]; int hg = colGround(col, px, pz, 60.f, -1e6f, &gp, nn); printf("точка %.1f %.1f: опора %s y=%.2f; машина %s\n", px, pz, hg ? "есть" : "НЕТ", hg ? gp : 0.f, bwCarName());
  int nstuck = 0, ncol = 0, ndyn = 0, ntot = 0;
  for (int k = 0; k < 8; k++) { float a = k * 3.14159265f / 4.f, sx = px - sinf(a) * R, sz = pz - cosf(a) * R, g;
    if (!colGround(col, sx, sz, gp + 4.f, gp - 6.f, &g, nn)) continue; bwLoad(&W, &S); Car c; carInit(&c, NULL, col, sx, g, sz, a); if (fabsf(c.y - g) > 1.f) continue; CarBox b0; carBox(&c, &b0); colProbeCar(col, &b0); if (b0.maxDepth > .05f) continue;
    W.car = c; ntot++; for (int f = 0; f < 300; f++) bwStep(&W, GP_R, 0.f, 0.f);
    float mx = W.car.x - sx, mz = W.car.z - sz, d = hypotf(mx, mz), dp = hypotf(W.car.x - px, W.car.z - pz);
    CarBox bf; carBox(&W.car, &bf); colProbeCar(col, &bf); int q = 0; for (int j = 1; j < bf.n && j < COL_MAX_CONTACTS; j++) if (bf.depth[j] > bf.depth[q]) q = j;
    int si = -1; float sd = 1e30f; for (unsigned i = 0; i < W.sp.ninst; i++) { float ex = W.car.x - W.sp.run[i].x, ez = W.car.z - W.sp.run[i].z, e = ex * ex + ez * ez; if (e < sd) { sd = e; si = (int)i; } }
    int pi = -1; float pd = 1e30f; for (unsigned i = 0; i < W.props.ninst; i++) { float ex = W.car.x - W.props.st[i].x, ez = W.car.z - W.props.st[i].z, e = ex * ex + ez * ez; if (e < pd) { pd = e; pi = (int)i; } }
    int stuck = dp > 3.f && d < R - 4.f && W.car.v < 1.f; if (stuck) nstuck++; if (stuck && bf.maxDepth > .02f) ncol++; if (stuck && bf.maxDepth <= .02f) ndyn++;
    printf("  с %4.0f° старт %.1f %.1f -> стоп %.1f %.1f (прошла %.1f м, до точки %.1f, v %.1f) %s | COL %.2f %c%u", a * 57.2958f, sx, sz, W.car.x, W.car.z, d, dp, W.car.v, stuck ? "ЗАСТРЯЛА" : "прошла", bf.maxDepth, bf.n ? "tbcs"[bf.type[q] & 3] : '-', bf.n ? (unsigned)bf.index[q] : 0u);
    if (si >= 0) printf(" | sp%d t%u cls%u atc%u %.1f м", si, (unsigned)W.sp.inst[si].type, (unsigned)W.sp.type[W.sp.inst[si].type].cls, (unsigned)W.sp.type[W.sp.inst[si].type].atc, sqrtf(sd));
    if (pi >= 0) printf(" | pr%d T%u %.1f м", pi, W.props.def[pi].type, sqrtf(pd)); printf("\n"); }
  printf("итог: заездов %d, застряло %d (статическая коллизия %d, динамика %d)\n", ntot, nstuck, ncol, ndyn); return 0;
}
