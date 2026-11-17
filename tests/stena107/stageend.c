// Правка A2.2 (Стена 107): L1 m5, конец этапа follow: машина cVan не называется ни следующим, ни последующими этапами → на конце этапа ИИ выключен (стоит) и она уходит из мира, когда игрок далеко (боты: > 110 м).
// Печатает режим и наличие cVan: после конца этапа, потом после того как игрок уехал. Красный: -DMC_STAGEEND_OLD=1 (остаётся в режиме target и стоит у финиша до уборки миссии).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); W.coins.collected = 250; GameMsn *M = &W.msn; M->rt.cp_phase = 1; M->rt.cp_stage = 1; msn_start_mission(&M->rt, 5, 1);
  bwPlace(&W, 222.7f, 4.0f, 165.f, 0.f);
  int sFollow = -1;
  for (int f = 0; f < 60 * 20; f++) { bwStep(&W, 0, 0, 0); if (M->rt.obj == MSN_OBJ_FOLLOW) { sFollow = M->rt.stage; break; } }
  McCar *c = mcFind(&M->cars, "cVan"); if (sFollow < 0 || !c) { printf("RESULT no_follow stage=%d van=%d\n", M->rt.stage, c != 0); return 1; }
  for (int f = 0; f < 90; f++) bwStep(&W, 0, 0, 0);
  { float gy; colGround(bwCol(), -190.f, 96.f, 50.f, -50.f, &gy, 0); carPlace(&c->car, (ColWorld *)bwCol(), -190.f, gy + .5f, 96.f, 3.1f); c->wp = c->passed = 2; c->passed = 1; c->nroute = 0; c->routeWp = -1; colGround(bwCol(), -190.f, 112.f, 50.f, -50.f, &gy, 0); bwPlace(&W, -190.f, gy + .5f, 112.f, 3.14f); }   // у последней точки пути (m5_van_path6)
  int st0 = M->rt.stage;
  for (int f = 0; f < 60 * 30 && M->rt.stage == st0; f++) { bwStep(&W, 0, 0, 0); if (f % 300 == 0) printf("  f=%d van wp=%d fin=%d at %.1f %.1f mode=%d\n", f, c->wp, c->finished, c->car.x, c->car.z, c->mode); }
  c = mcFind(&M->cars, "cVan");
  printf("after_follow stage %d.%d van=%d mode=%d leaveQ=%d\n", M->rt.phase, M->rt.stage, c != 0, c ? c->mode : -1, c ? c->leaveQ : -1);
  { float gy; colGround(bwCol(), 222.f, 170.f, 50.f, -50.f, &gy, 0); bwPlace(&W, 222.f, gy + .5f, 170.f, 0.f); }   // игрок далеко (> 110 м)
  for (int f = 0; f < 120; f++) bwStep(&W, 0, 0, 0);
  c = mcFind(&M->cars, "cVan");
  printf("later van=%d mode=%d\n", c != 0, c ? c->mode : -1);
  printf("RESULT van_gone=%d\n", c == 0);
  return 0; }
