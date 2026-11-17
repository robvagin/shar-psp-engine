// Правка A2.5 (Стена 107): перезапуск основной части с точки: машины прежних этапов ставятся стоящими без ИИ и уходят в очередь (если их не называет этап точки и позже).
// L2 m4 (точка искусственно на этапе 5): pizza/target из этапа 0 (старое: едет, режим target). Параметры: level.shr homer.shr msn id точка
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 6 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); GameMsn *M = &W.msn; int mi = -1; for (uint32_t i = 0; i < M->f.nmissions; i++) if (!strcmp(msn_str(&M->f, M->f.missions[i].id), argv[4])) mi = (int)i;
  if (mi < 0) { printf("RESULT no_mission\n"); return 1; }
  M->rt.cp_phase = 1; M->rt.cp_stage = atoi(argv[5]); msn_start_mission(&M->rt, mi, 1);
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  int moving = 0, parkedQ = 0, n = 0; for (int i = 0; i < M->cars.n; i++) { const McCar *c = &M->cars.c[i]; if (!c->on) continue; n++; printf("  car %s mode=%d leaveQ=%d\n", c->name, c->mode, c->leaveQ); if (strcasecmp(c->name, "snake_v")) moving++; if (c->leaveQ) parkedQ++; }
  printf("stage %d.%d cars=%d RESULT stale_cars=%d queued=%d\n", M->rt.phase, M->rt.stage, n, moving, parkedQ); return 0; }
