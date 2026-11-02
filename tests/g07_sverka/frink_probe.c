// проба: персонаж-собеседник L2 идёт по маршруту, поперёк стоит машина. frink_probe LEVEL HOMER MSN [имя] [угол_градусов]
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "world.h"
static BotWorld W;
static const GameNpc *npcNamed(const GameMsn *M, const char *name) { for (int i = 0; i < M->nnpc; i++) if (msn_streq(M->npc[i].name, name)) return &M->npc[i]; return 0; }
static int setup(const char *who, int *stg) {
  bwNew(&W); GameMsn *m = &W.msn; MsnFile *F = &m->f;
  for (uint32_t mi = 0; mi < F->nmissions; mi++) for (int p = 0; p < 2; p++) {
    unsigned idx = p ? F->missions[mi].main : F->missions[mi].intro; if (idx == MSN_NONE16) continue; const MsnPhase *P = &F->phases[idx];
    for (unsigned s = 0; s < P->stage_count; s++) { const MsnStage *S = &F->stages[P->first_stage + s];
      if (S->obj == MSN_OBJ_TALKTO) { msn_start_mission(&m->rt, mi, 0); m->rt.cp_phase = p; m->rt.cp_stage = s; m->rt.cp_timer = 0; m->rt.cp_remaining = 0; msn_retry(&m->rt, 0);
        for (int k = 0; k < 5; k++) bwStep(&W, 0, 0, 0);
        if (m->rt.target && !strcmp(m->rt.target, who)) { *stg = (int)(mi * 100 + p * 50 + s); return 1; } } } }
  return 0;
}
// один прогон: машина игрока в точке пути на доле fr от текущей к следующей, курс ang (рад) от направления пути; Гомер далеко. Возвращает макс. простой, время маршрута
static float runOne(const char *who, float fr, float ang, int withCar, float *tEnd, int *stg) {
  if (!setup(who, stg)) return -1.f;
  GameMsn *m = &W.msn; const GameNpc *n = npcNamed(m, who); MsnVec3 t = n->wp[n->iwp];
  float mx = n->pos.x + (t.x - n->pos.x) * fr, mz = n->pos.z + (t.z - n->pos.z) * fr, dx = t.x - n->pos.x, dz = t.z - n->pos.z, h = atan2f(dx, dz) + ang;
  homerPlace(&W.homer, bwCol(), n->pos.x + 40.f, n->pos.y + .3f, n->pos.z + 40.f, 0.f);
  W.car.x = withCar ? mx : 5000; W.car.y = n->pos.y + .3f; W.car.z = withCar ? mz : 5000; W.car.h = h; memset(W.car.vel, 0, sizeof W.car.vel);
  float lastd = 1e9f, still = 0.f, maxstill = 0.f; int iw0 = n->iwp; *tEnd = -1.f;
  for (int f = 0; f < 60 * 45; f++) { bwStep(&W, 0, 0, 0); n = npcNamed(m, who);
    if (!n->walking) { *tEnd = f / 60.f; break; }
    MsnVec3 tt = n->wp[n->iwp < n->nwp ? n->iwp : 0]; float d = hypotf(tt.x - n->pos.x, tt.z - n->pos.z);
    if (d < lastd - .02f || n->iwp != iw0) { lastd = d; still = 0.f; iw0 = n->iwp; } else { still += 1.f / 60; if (still > maxstill) maxstill = still; } }
  bwFree(&W); return maxstill;
}
int main(int argc, char **argv) {
  if (bwData(argv[1], argv[2], argv[3])) return 2;
  const char *who = argc > 4 ? argv[4] : "frink"; int stg, bad = 0, total = 0; float te0; float base; runOne(who, 0.5f, 0.f, 0, &te0, &stg); base = te0;
  printf("%s: без машины маршрут %.1f с\n", who, base);
  for (float fr = 0.15f; fr < 1.f; fr += 0.2f) for (int a = 0; a < 180; a += 30) {
    float te; float ms = runOne(who, fr, a * 3.14159265f / 180.f, 1, &te, &stg); total++;
    if (te < 0 || ms > 3.f) { bad++; printf("  доля %.2f угол %3d: простой %.1f с, маршрут %.1f с\n", fr, a, ms, te); } }
  printf("прогонов %d, застревает (простой больше 3 с или не прошёл): %d\n", total, bad);
  return 0;
}
