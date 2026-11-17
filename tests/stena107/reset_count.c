// Правка Ф2-11 (Стена 107): L1 sr1 (уличная гонка 1), отсчёт «3 2 1»: поврежденной машине игрока кнопка сброса (START у ботов) не должна чинить её и переставлять. Печатает урон до и после.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); GameMsn *M = &W.msn; msn_start_bonus(&M->rt, 8); int got = 0;
  for (int f = 0; f < 60 * 40; f++) { bwStep(&W, 0, 0, 0); if (M->rt.status == MSN_COUNTDOWN && gameMsnResetBlocked(M) != -1) { got = 1; break; } }
  if (!got) { printf("RESULT no_countdown status=%d stage=%d.%d\n", M->rt.status, M->rt.phase, M->rt.stage); return 1; }
  W.car.damage = .5f; float x0 = W.car.x, z0 = W.car.z; bwStep(&W, 0, 0, 0); W.car.damage = .5f; bwStep(&W, GP_START, 0, 0);
  printf("countdown stage %d.%d damage_before 0.5 damage_after %.2f moved %.2f\n", M->rt.phase, M->rt.stage, W.car.damage, hypotf(W.car.x - x0, W.car.z - z0));
  printf("RESULT moved=%.2f\n", hypotf(W.car.x - x0, W.car.z - z0)); return 0; }
