// Правка A2.13 (Стена 107): все 6 мест машин миссии заняты (3 полицейских Hit & Run + 3 лишних в очереди/стоящих); машина этапа должна появиться.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); MsnCars *m = &W.msn.cars; float p[3] = {220.f, 3.5f, 172.f}; char nm[16];
  for (int i = 0; i < MC_MAX; i++) { snprintf(nm, sizeof nm, "fill%d", i); McCar *c = mcSpawn(m, bwCol(), 0, nm, p, 0.f, 1, "NULL", "", ""); if (c && i < 3) c->police = 1; }
  McCar *s = mcSpawn(m, bwCol(), 0, "smith_v", p, 0.f, 1, "race", "", "");
  printf("RESULT spawned=%d n=%d\n", s != 0, m->n); return 0; }
