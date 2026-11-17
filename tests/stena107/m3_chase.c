// Правка 1265 (Стена 107): L1 m3, финал «destroy smith_v»: куда едет машина Смитерса, рисуется ли и твёрда ли она, где стоит трафик. Игрок ставится на chase_start и стоит.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  float secs = argc > 4 ? (float)atof(argv[4]) : 60.f; int stg = argc > 5 ? atoi(argv[5]) : 2;
  bwNew(&W); W.coins.collected = 250; GameMsn *M = &W.msn; M->rt.cp_phase = 1; M->rt.cp_stage = stg; int mi = getenv("MI") ? atoi(getenv("MI")) : 3; msn_start_mission(&M->rt, mi, 1);
  if (getenv("PX")) bwPlace(&W, (float)atof(getenv("PX")), (float)atof(getenv("PY")), (float)atof(getenv("PZ")), 0.f); else bwPlace(&W, 214.2f, 3.9f, 280.1f, 0.f);
  int lastSt = -9;
  for (int f = 0; f < (int)(secs * 60); f++) {
    bwStep(&W, 0, 0, 0);
    if (getenv("FOLLOW") && M->cars.n && f % 30 == 0) { const McCar *c = &M->cars.c[0]; if (hypotf(c->car.x - W.car.x, c->car.z - W.car.z) > 35.f) {
      float hh = c->car.h; bwPlace(&W, c->car.x - sinf(hh) * 18.f * -1.f, c->car.y + .6f, c->car.z - cosf(hh) * 18.f * -1.f, hh); } }
    if (M->rt.stage != lastSt) { lastSt = M->rt.stage; printf("t=%.1f stage %d.%d obj=%s\n", f / 60.f, M->rt.phase, M->rt.stage, msn_objective_name[M->rt.obj]); }
    if (f % 60 == 0) for (int i = 0; i < M->cars.n; i++) { const McCar *c = &M->cars.c[i];
      if (f % 120 == 0 || getenv("EVERY")) printf("t=%.0f car %s on=%d mode=%d fin=%d at %.1f %.1f %.1f v=%.1f stuck=%.1f still=%.1f wp=%d player %.1f %.1f\n", f / 60.f, c->name, c->on, c->mode, c->finished, c->car.x, c->car.y, c->car.z, hypotf(c->car.vel[0], c->car.vel[2]), c->stuckT, c->still, c->wp, W.car.x, W.car.z); }
  }
  return 0; }
