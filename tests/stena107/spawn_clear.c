// Правка Ф2-6 (Стена 107): L1 m3, вход в этап 1.2: на m4_smithers_carstart (231.3, 5.2, 307.1) заранее стоит машина трафика; оригинал убирает трафик в 5 м от точки машины этапа.
// Печатает, сколько машин трафика осталось в 5 м от точки после появления smith_v. Красный: -DMSN_SPAWNCLEAR_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); W.coins.collected = 250; Traffic *T = (Traffic *)bwTraffic(); if (!T) { printf("нет трафика\n"); return 1; }
  const float X = 231.3f, Y = 5.2f, Z = 307.1f;
  float gy; colGround(bwCol(), X, Z, 50.f, -50.f, &gy, 0);
  TrCar *c = &T->car[0]; memset(c, 0, sizeof *c); c->model = T->slot[0].model; c->junc = -1; c->mode = TRM_DRIVE; c->v = 0.f;
  carInit(&c->car, &T->params[c->model], bwCol(), X + 1.5f, gy + .5f, Z, 0.f);
  GameMsn *M = &W.msn; M->rt.cp_phase = 1; M->rt.cp_stage = 2; msn_start_mission(&M->rt, 3, 1);
  bwStep(&W, 0, 0, 0);
  McCar *sv = mcFind(&M->cars, "smith_v"); int n = 0;
  for (int i = 0; i < TR_MAXCAR; i++) { const TrCar *k = &T->car[i]; if (k->mode == TRM_OFF) continue; if (hypotf(k->car.x - X, k->car.z - Z) < 5.f) n++; }
  printf("smith_v=%d at %.1f %.1f traffic_within_5m=%d\n", sv != 0, sv ? sv->car.x : 0.f, sv ? sv->car.z : 0.f, n);
  printf("RESULT overlap=%d\n", n); return 0; }
