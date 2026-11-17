// Правка 1265 (окно Стена, 107): что стоит и ездит вокруг точки (X Z) — стоящие машины трафика и ездящие; машина игрока ставится туда же и стоит.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 6 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "usage: area_dump LEVEL HOMER MSN X Z [secs] [heading]\n"); return 2; }
  float X = (float)atof(argv[4]), Z = (float)atof(argv[5]), secs = argc > 6 ? (float)atof(argv[6]) : 20.f, hd = argc > 7 ? (float)atof(argv[7]) : 0.f;
  bwNew(&W); { float y; if (!colGround(bwCol(), X, Z, 50.f, -50.f, &y, 0)) y = 0; bwPlace(&W, X, y + .5f, Z, hd); }
  for (int f = 0; f < (int)(secs * 60); f++) {
    bwStep(&W, 0, 0, 0);
    if (f % 120 == 0) {
      Car *cs[48]; int n = bwTraffic() ? trafficCarsNear((Traffic *)bwTraffic(), X, Z, 50.f, cs, 48) : 0;
      printf("t=%.1f car %.1f %.1f %.1f moving_near=%d\n", f / 60.f, W.car.x, W.car.y, W.car.z, n);
      for (int i = 0; i < n; i++) printf("   tr: %.1f %.1f %.1f h=%.2f\n", cs[i]->x, cs[i]->y, cs[i]->z, cs[i]->h);
      if (bwTraffic()) { const Traffic *T = bwTraffic(); for (int i = 0; i < T->npark; i++) { const TrParked *k = &T->park[i]; if (hypotf(k->car.x - X, k->car.z - Z) > 60.f) continue;
        printf("   park %d: %.1f %.1f %.1f h=%.2f %s state=%d sel=%d gone=%d off=%d model=%d\n", i, k->car.x, k->car.y, k->car.z, k->car.h, T->parkd ? T->parkd[i].name : "?", k->state, k->sel, k->gone, k->off, T->parkd ? T->parkd[i].model : -1); } }
    }
  }
  return 0; }
