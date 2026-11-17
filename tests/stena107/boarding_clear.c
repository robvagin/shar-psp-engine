// Правка A2.11 (Стена 107): L1 m5, старт основной части: на m5_carstart (215.2, 3.4, 170.5) стоит машина трафика; оригинал снимает трафик в 10 м от машины игрока. Красный: -DMSN_SPAWNCLEAR_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); Traffic *T = (Traffic *)bwTraffic(); if (!T) { printf("нет трафика\n"); return 1; }
  const float X = 215.2f, Z = 170.5f; float gy; colGround(bwCol(), X, Z, 50.f, -50.f, &gy, 0);
  TrCar *c = &T->car[0]; memset(c, 0, sizeof *c); c->model = T->slot[0].model; c->junc = -1; c->mode = TRM_DRIVE; c->v = 0.f; carInit(&c->car, &T->params[c->model], bwCol(), X + 6.f, gy + .5f, Z, 0.f);
  GameMsn *M = &W.msn; M->rt.cp_phase = 1; M->rt.cp_stage = 0; msn_start_mission(&M->rt, 5, 1); 
  int n = 0; for (int i = 0; i < TR_MAXCAR; i++) { const TrCar *k = &T->car[i]; if (k->mode == TRM_OFF) continue; if (hypotf(k->car.x - X, k->car.z - Z) < 10.f) n++; }
  printf("RESULT traffic_within_10m=%d player_car %.1f %.1f\n", n, W.car.x, W.car.z); return 0; }
