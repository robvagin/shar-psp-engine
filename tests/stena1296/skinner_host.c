// 1309 (Роб: «Скиннер очень тупо едет, врезается в стены»): L2 m1, этап 2 «оторваться» (losetail skinn_v). Игра без экрана, 60 с: игрок стоит в своей машине, Скиннер едет к нему. Считаем кадры, когда Скиннер почти стоит (< 1 м/с), а до игрока дальше 12 м (упёрся, не доехал), и самую долгую такую остановку, с.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
extern int mcChaseCurve1309Old, mcChaseHead1309Old, mcSlipDecay1309Old;
static float vh[16];
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "skinner_host: нет данных\n"); return 2; }
  float secs = argc > 4 ? (float)atof(argv[4]) : 60.f;
  if (getenv("SK_OLD")) mcChaseCurve1309Old = mcChaseHead1309Old = 1;
  if (getenv("SK_NOHEAD")) mcChaseHead1309Old = 1;
  if (getenv("SK_NOSLIP")) mcSlipDecay1309Old = 1;
  bwNew(&W); for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  bwPlace(&W, -22.f, 3.f, 8.f, 0.f); for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);   // как сценарий emulator: игрок у Скиннера, потом этап 2 и перенос на дорогу (92; -20)
  for (int g = 0; g < 40 && !(W.msn.rt.phase == 1 && W.msn.rt.stage >= 2); g++) { msn_service_pass(&W.msn.rt); for (int f = 0; f < 6 && !(W.msn.rt.phase == 1 && W.msn.rt.stage >= 2); f++) bwStep(&W, 0, 0, 0); }
  { float tx = argc > 6 ? (float)atof(argv[5]) : 92.f, tz = argc > 6 ? (float)atof(argv[6]) : -20.f, gy = 1e9f; int nl; char ob[16]; for (float y = -20.f; y < 120.f; y += .25f) if (bwWhere(tx, y, tz, 2.f, &nl, ob) >= 0) { gy = y; break; }
    if (gy > 1e8f) { printf("SK1309 цель %.0f,%.0f без опоры\n", tx, tz); printf("RESULT SKIP\n"); return 3; }
    bwPlace(&W, tx, gy, tz, 0.f); }
  printf("SK1309 phase=%d stage=%d obj=%d\n", W.msn.rt.phase, W.msn.rt.stage, W.msn.rt.obj);
  for (int i = 0; i < W.msn.cars.n; i++) printf("SK1309 car[%d] %s on=%d mode=%d pos=%.1f,%.1f\n", i, W.msn.cars.c[i].name, W.msn.cars.c[i].on, W.msn.cars.c[i].mode, W.msn.cars.c[i].car.x, W.msn.cars.c[i].car.z);
  McCar *sk = mcFind(&W.msn.cars, "skinn_v"); if (!sk) { printf("RESULT FAIL нет skinn_v\n"); return 1; }
  float arr = -1.f; int far = 0, imp = 0, stall = 0, run = 0, maxRun = 0, n = (int)(secs * 60), sec = 0; float minD = 1e9f;
  for (int f = 0; f < n; f++) {
    bwStep(&W, 0, 0, 0); if (getenv("SK_DBG") && (f < 40 || (f < 600 && f % 30 == 0))) printf("DBG %s f=%d phase=%d stage=%d obj=%d cars=%d c0.on=%d mode=%d pos=%.1f,%.1f\n", W.msn.cars.c[0].name, f, W.msn.rt.phase, W.msn.rt.stage, W.msn.rt.obj, W.msn.cars.n, W.msn.cars.c[0].on, W.msn.cars.c[0].mode, W.msn.cars.c[0].car.x, W.msn.cars.c[0].car.z);
    sk = mcFind(&W.msn.cars, "skinn_v"); if (!sk) { printf("SK1309 skinn_v исчез t=%d\n", f / 60); break; }
    float v = hypotf(sk->car.vel[0], sk->car.vel[2]), d = hypotf(sk->car.x - W.car.x, sk->car.z - W.car.z); if (d < minD) minD = d;
    if (getenv("SK_TR") && f >= atoi(getenv("SK_TR")) && f < atoi(getenv("SK_TR")) + 90 && f % 3 == 0) printf("TR f=%d pos=%.1f,%.1f h=%.2f v=%.1f vt=%.1f turning=%d direct=%d near=%.1f,%.1f far=%.1f,%.1f slip=%.2f ramT=%.2f backT=%.2f stopHold=%d stkOn=%d stkT=%.2f\n", f, sk->car.x, sk->car.z, sk->car.h, hypotf(sk->car.vel[0], sk->car.vel[2]), sk->vt, sk->turning, sk->direct, sk->dbgNear[0], sk->dbgNear[2], sk->dbgFar[0], sk->dbgFar[2], sk->slipT, sk->ramT, sk->backT, sk->stopHold, sk->stkOn, sk->stkT);
    if (sk->car.wallV >= 12.f && hypotf(sk->car.x - W.car.x, sk->car.z - W.car.z) > 30.f) { far++; printf("SK1309 стена-далеко t=%.2f wallV=%.1f at=%.1f,%.1f\n", f / 60.f, sk->car.wallV, sk->car.x, sk->car.z); }
    if (sk->car.wallV > 6.f) printf("SK1309 стена t=%.2f wallV=%.1f тип=%d номер=%u мат=%d нормаль=%.2f,%.2f точка=%.1f,%.1f,%.1f v=%.1f\n", f / 60.f, sk->car.wallV, sk->car.wallType, sk->car.wallIdx, sk->car.wallMat, sk->car.wallN[0], sk->car.wallN[1], sk->car.wallP[0], sk->car.wallP[1], sk->car.wallP[2], hypotf(sk->car.vel[0], sk->car.vel[2]));
    if (sk->car.carHitV > 3.f || sk->car.hardV > 6.f || sk->car.foliV > 3.f) printf("SK1309 контакт t=%.2f carHitV=%.1f hardV=%.1f foliV=%.1f at=%.1f,%.1f\n", f / 60.f, sk->car.carHitV, sk->car.hardV, sk->car.foliV, sk->car.x, sk->car.z);
    { float v0 = vh[f & 15]; vh[f & 15] = v; static int cool; if (cool > 0) cool--; else if (f > 12 && v0 > 12.f && v < v0 - 7.f) { imp++; cool = 60; { int nl; char ob[16] = "-"; float px = sk->car.x + sinf(sk->car.h) * 2.5f, pz = sk->car.z + cosf(sk->car.h) * 2.5f; bwWhere(px, sk->car.y, pz, 1.2f, &nl, ob); printf("SK1309 удар t=%.1f pos=%.1f,%.1f v %.1f -> %.1f direct=%d turning=%d перед=%s\n", f / 60.f, sk->car.x, sk->car.z, v0, v, sk->direct, sk->turning, ob); } } }
    if (arr < 0.f && d < 10.f) arr = f / 60.f;
    if (v < 1.f && d > 12.f) { stall++; run++; if (run > maxRun) maxRun = run; } else run = 0;
    if (f / 60 != sec) { sec = f / 60; if (sec % 5 == 0 || sec < 8) printf("SK1309 t=%2d nroute=%d pos=%.1f,%.1f v=%.1f d=%.1f mode=%d stuckN=%d resets=%u replans=%u\n", sec, sk->nroute, sk->car.x, sk->car.z, v, d, sk->mode, sk->stuckN, sk->resets, sk->replans); }
  }
  printf("SK1309 stall_frames=%d longest=%.2f с minD=%.1f ударов=%d стен_далеко=%d прибыл=%.1f\n", stall, maxRun / 60.f, minD, imp, far, arr);
  printf("RESULT %s\n", maxRun / 60.f <= 1.f ? "OK" : "FAIL");
  return maxRun / 60.f <= 1.f ? 0 : 1;
}
