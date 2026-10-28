// №723 (Глюки): трасса полицейской машины порта к стоящему игроку против записи PS2 b3_moving (игрок стоит вне дорог у (122; 214), полиция с 100 м):
// police723 LEVEL.shr HOMER.shr level01.msn X Z — игрок ставится в (X, Z), полиция по правилу main.c hnrPursuit; раз в 25 кадров строка: кадр, полиция (x z), до игрока, от оси дороги (без дорог-срезов), скорость
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "world.h"
#include "msncar.h"
static BotWorld W;
static const McCar *police(void) { for (int i = 0; i < W.msn.cars.n; i++) if (W.msn.cars.c[i].on && !strcmp(W.msn.cars.c[i].name, "cPolice")) return &W.msn.cars.c[i]; return 0; }
static float roadDist(const Traffic *t, float x, float z, float *wOut) {   // до ближайшего отрезка полосы обычной дороги
  float best = 1e9f, bw = 0.f;
  for (unsigned r = 0; r < t->hd->nRoad; r++) { const TrRoad *R = &t->road[r]; if (R->flags & 1) continue;
    for (int l = 0; l < (R->lanes ? R->lanes : 1); l++) { const TrPt *P = t->pt + R->firstPt + l * R->nPt;
      for (int k = 0; k + 1 < R->nPt; k++) { float ax = P[k].x, az = P[k].z, dx = P[k + 1].x - ax, dz = P[k + 1].z - az, L = dx * dx + dz * dz, u = L < 1e-9f ? 0.f : ((x - ax) * dx + (z - az) * dz) / L;
        u = u < 0.f ? 0.f : u > 1.f ? 1.f : u; float d = hypotf(x - ax - u * dx, z - az - u * dz); if (d < best) { best = d; bw = R->width; } } } }
  if (wOut) *wOut = bw; return best;
}
int main(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "police723 LEVEL.shr HOMER.shr level01.msn X Z [секунд]\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  const Traffic *t = (const Traffic *)bwTraffic(); float X = atof(argv[4]), Z = atof(argv[5]), secs = argc > 6 ? atof(argv[6]) : 30.f;
  bwNew(&W); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  float gy = W.car.y; bwPlace(&W, X, gy + 3.f, Z, 0.f); for (int f = 0; f < 90; f++) bwStep(&W, 0, 0, 0);
  printf("игрок (%.1f, %.1f), от оси дороги %.1f м\n", W.car.x, W.car.z, roadDist(t, W.car.x, W.car.z, 0));
  gameMsnPolice(&W.msn, 1, "cPolice", "Pursuit\\L1cop.con", 1);
  int respawns = 0; float spawnT = 1.f, idleT = 0.f; int offFrames = 0, frames = 0; float maxOff = 0.f, firstLeaveD = -1.f;
  for (int f = 0; f < (int)(secs * 60); f++) {
    bwStep(&W, 0, 0, 0); const McCar *c = police(); float dd = c ? hypotf(c->car.x - W.car.x, c->car.z - W.car.z) : 1e9f;
    idleT = c && dd > 10.f && fabsf(W.car.v) < 30.f / 3.6f ? idleT + BW_DT : 0.f;
    if (((!c || dd > 110.f) && (spawnT -= BW_DT) <= 0.f) || idleT > 10.f) { spawnT = 1.f; idleT = 0.f; if (c) gameMsnPolice(&W.msn, 0, "cPolice", "", 1); gameMsnPolice(&W.msn, 1, "cPolice", "Pursuit\\L1cop.con", 1); respawns++; c = police(); }
    if (!c) continue; float w, d = roadDist(t, c->car.x, c->car.z, &w); frames++;
    if (d > (w > 6.f ? w : 6.f) / 2.f + 2.f) { offFrames++; if (firstLeaveD < 0.f) firstLeaveD = dd; } if (d > maxOff) maxOff = d;
    if (f % 25 == 0) printf("%4d  (%.0f,%.0f)  до игрока %.0f  от оси %.1f  v %.1f  mode %d\n", f, c->car.x, c->car.z, dd, d, fabsf(c->car.v), c->mode);
  }
  printf("ИТОГ кадров полиции %d, вне дороги %d (%.0f %%), максимум от оси %.1f м, первый выезд с дороги на расстоянии до игрока %.0f м, пересадок %d\n", frames, offFrames, frames ? 100.f * offFrames / frames : 0.f, maxOff, firstLeaveD, respawns);
  return 0;
}
