// Правка Ф2-4/A2.3 (Стена 107): L1 m3, этап 2 «goto Квик-и-Март»: smith_v стоит (ИИ NULL). 10 таранов её машиной игрока на 15–20 м/с; печатает урон и взрыв.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); W.coins.collected = 250; GameMsn *M = &W.msn; M->rt.cp_phase = 1; M->rt.cp_stage = 2; msn_start_mission(&M->rt, 3, 1);
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  McCar *c = mcFind(&M->cars, "smith_v"); if (!c) { printf("нет smith_v\n"); return 1; }
  { float gy; colGround(bwCol(), 340.f, 150.f, 50.f, -50.f, &gy, 0); carPlace(&c->car, (ColWorld *)bwCol(), 340.f, gy + .5f, 150.f, 0.f); }   // дальше сферы цели этапа (иначе таран сам кончает этап)
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  float sx = c->car.x, sz = c->car.z, maxd = 0; int boom = 0, rams = 0, hits0 = 0;
  for (int r = 0; r < 10; r++) {
    float y; colGround(bwCol(), sx, sz + 18.f, 50.f, -50.f, &y, 0); bwPlace(&W, sx, y + .5f, sz + 18.f, 3.14159f);
    for (int f = 0; f < 150; f++) { bwStep(&W, GP_CROSS, 0, 0); if (c->car.damage > maxd) maxd = c->car.damage; if (c->car.destroyed) boom = 1; static int ls = -1; if (M->rt.stage != ls) { ls = M->rt.stage; printf("ram %d f %d stage %d.%d player %.1f %.1f smith %.1f %.1f dmg %.3f\n", r, f, M->rt.phase, M->rt.stage, W.car.x, W.car.z, c->car.x, c->car.z, c->car.damage); } }
    rams++; (void)hits0;
  }
  printf("stage %d.%d mode=%d rams %d damage_max %.3f destroyed %d smith at %.1f %.1f\n", M->rt.phase, M->rt.stage, c->mode, rams, maxd, boom, c->car.x, c->car.z);
  return 0; }
