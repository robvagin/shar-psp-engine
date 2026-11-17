// Правка 1265 (окно Стена, 107): что стоит вокруг старта L1 m7 — трафик/машины миссии рядом с (47,-281). Стенд: тот же мир, что у ботов.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 5 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "usage: start_dump LEVEL HOMER MSN MISSION [secs]\n"); return 2; }
  int mi = atoi(argv[4]); float secs = argc > 5 ? (float)atof(argv[5]) : 20.f;
  bwNew(&W); GameMsn *M = &W.msn; if (getenv("MAIN")) { M->rt.cp_phase = 1; M->rt.cp_stage = 0; msn_start_mission(&M->rt, mi, 1); } else msn_start_mission(&M->rt, mi, 0);
  for (int f = 0; f < (int)(secs * 60); f++) {
    bwStep(&W, 0, 0, 0);
    if (f % 120 == 0) {
      float hx = W.homer.x, hz = W.homer.z; Car *cs[32]; int n = bwTraffic() ? trafficCarsNear((Traffic *)bwTraffic(), 47.5f, -281.f, 40.f, cs, 32) : 0;
      printf("t=%.1f homer %.1f %.1f %.1f foot=%d car %.1f %.1f %.1f traffic_near=%d\n", f / 60.f, hx, W.homer.y, hz, gameOnFoot(&W.G), W.car.x, W.car.y, W.car.z, n);
      if (bwTraffic()) { const Traffic *T = bwTraffic(); for (int i = 0; i < T->npark; i++) { const TrParked *k = &T->park[i]; if (hypotf(k->car.x - 47.5f, k->car.z + 281.f) > 45.f) continue;
        printf("   park %d: %.1f %.1f %.1f h=%.2f %s state=%d sel=%d gone=%d off=%d model=%d\n", i, k->car.x, k->car.y, k->car.z, k->car.h, T->parkd ? T->parkd[i].name : "?", k->state, k->sel, k->gone, k->off, T->parkd ? T->parkd[i].model : -1); } }
      for (int i = 0; i < n; i++) printf("   tr %d: %.1f %.1f %.1f h=%.2f\n", i, cs[i]->x, cs[i]->y, cs[i]->z, cs[i]->h);
    }
  }
  return 0; }
