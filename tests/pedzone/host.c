// №1132 (сверка g02-traffic): группа моделей пешеходов по зонам. Данные диска: ворота групп (traffic.shr), группы (leveli.mfk), UsePedGroup миссий (level0N.msn).
// Проверяется: вход в ворота включает группу ворот и она остаётся после выхода; рождённые пешеходы — только из модели текущей группы
// (у АЭС L1 E4-8 — только работники, в городе — ни одного); UsePedGroup миссии включает группу один раз, ворота после неё снова главные, конец миссии группу не возвращает.
//   tests/pedzone/host.c LEVEL HOMER MSN
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
static int bad, nchk;
static void check(const char *name, int good, int lvl, int a, int b) { nchk++; if (!good) { printf("FAIL L%d %s (%d %d)\n", lvl, name, a, b); bad++; } }
static void step(Traffic *t, const float *pos) { float eye[3] = {pos[0], pos[1] + 3, pos[2] - 7}, dir[3] = {0, 0, 1}; trafficUpdate(t, W.G.pWorld, &W.car, 0, pos, eye, dir, 1.f / 30); }
static int inGate(const Traffic *t, const float *p) {
  for (unsigned i = 0; i < t->hd->nZone; i++) { const TrZone *Z = &t->zone[i]; float d[3] = {p[0] - Z->c[0], p[1] - Z->c[1], p[2] - Z->c[2]};
    if (fabsf(d[0] * Z->ax[0] + d[1] * Z->ax[1] + d[2] * Z->ax[2]) < Z->h[0] && fabsf(d[1]) < Z->h[1] + 2.f && fabsf(d[0] * Z->az[0] + d[1] * Z->az[1] + d[2] * Z->az[2]) < Z->h[2]) return 1; }
  return 0;
}
static int outsidePoint(const Traffic *t, const TrZone *Z, float *out) {   // точка рядом с воротами, которая не в воротах (игрок вышел)
  for (int k = 0; k < 40; k++) { float a = k * .7854f, r = 40.f + 25.f * (k / 8);
    float p[3] = {Z->c[0] + sinf(a) * r, Z->c[1], Z->c[2] + cosf(a) * r}; if (!inGate(t, p)) { memcpy(out, p, sizeof p); return 1; } }
  return 0;
}
static int inGroup(const Traffic *t, int g, int m) { for (int j = 0; j < t->grp[g].n; j++) if (t->grp[g].model[j] == m) return 1; return 0; }
// родить пешеходов у середины троп, игрок сначала входит в ворота zi, потом идёт к тропе; -> число рождённых, bad если чужая модель
static int spawnCheck(Traffic *t, int lvl, int zi, int g, const char *what) {
  int spawned = 0, foreign = 0; t->maxPeds = TR_MAXPED;
  for (unsigned p = 0; p < t->hd->nPath && spawned < 3; p++) {
    trafficReset(t, W.G.pWorld); step(t, t->zone[zi].c);
    const TrPath *path = &t->path[p]; float pos[3] = {path->cx, t->zone[zi].c[1], path->cz};
    if (inGate(t, pos)) continue;
    t->fresh = 1; for (int fr = 0; fr < 3; fr++) step(t, pos);
    for (int k = 0; k < TR_MAXPED; k++) if (t->ped[k].mode != TRP_OFF) { spawned++; if (!inGroup(t, g, t->ped[k].model)) foreign++; }
  }
  t->maxPeds = 0; trafficReset(t, W.G.pWorld);
  check(what, spawned > 0 && foreign == 0, lvl, spawned, foreign);
  return spawned;
}
int main(int argc, char **argv) {
  if (argc != 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  if (bwNew(&W)) return 2;
  Traffic *t = (Traffic *)bwTraffic(); if (!t || !t->hd) return 2; t->maxCars = t->maxPeds = 0;
  int lvl = W.msn.f.hdr->level;
  int nz = (int)t->hd->nZone, ng = (int)t->hd->nGroup;
  // 1. вход в ворота включает их группу; после выхода группа остаётся; повторный кадр внутри ничего не меняет
  for (int i = 0; i < nz; i++) {
    const TrZone *Z = &t->zone[i]; trafficReset(t, W.G.pWorld);
    step(t, Z->c); check("вход: группа ворот", t->group == (int)Z->group, lvl, t->group, (int)Z->group);
    float out[3]; if (outsidePoint(t, Z, out)) { step(t, out); check("вышел: группа остаётся", t->group == (int)Z->group, lvl, t->group, (int)Z->group); }
  }
  // 2. рождённые пешеходы — только из группы ворот (и АЭС отдельно: у ворот работников — только работники, у остальных — ни одного)
  int seenG = 0, spawnTot = 0;
  for (int i = 0; i < nz; i++) { int g = (int)t->zone[i].group; if (g >= ng || !t->grp[g].n || (seenG & (1 << g))) continue;
    seenG |= 1 << g; spawnTot += spawnCheck(t, lvl, i, g, "рождение только из группы ворот"); }
  check("рождение наблюдалось", spawnTot > 0, lvl, spawnTot, 0);
  // 3. UsePedGroup миссии: включает группу один раз; ворота после неё снова меняют группу; конец миссии группу не возвращает
  int cmds = 0;
  for (unsigned i = 0; i < W.msn.f.ncmds; i++) {
    const MsnCmd *cmd = &W.msn.f.cmds[i]; if (cmd->op != MSN_OP_USEPEDGROUP) continue;
    int d = msn_arg_i(&W.msn.f, cmd, 0); if (d < 0 || d >= ng) continue; cmds++;
    for (int zi = 0; zi < nz; zi++) { int c = (int)t->zone[zi].group; if (c == d || c >= ng) continue;
      trafficReset(t, W.G.pWorld);
      MsnCall call = {&W.msn.f, cmd, MSN_AT_SETUP, -1, -1}; W.msn.rt.host.command(W.msn.rt.host.user, &call);
      check("миссия: группа включена", t->group == d, lvl, t->group, d);
      float away[3]; if (outsidePoint(t, &t->zone[zi], away)) { step(t, away); check("миссия: вне ворот группа держится", t->group == d, lvl, t->group, d); }
      step(t, t->zone[zi].c); check("миссия: ворота после неё главные", t->group == c, lvl, t->group, c);
      MsnEvent ev = {0}; ev.type = MSN_EV_CLEANUP; W.msn.rt.host.event(W.msn.rt.host.user, &ev);
      check("конец миссии: группа остаётся", t->group == c, lvl, t->group, c);
    }
  }
  printf("L%d: ворот %d, групп %d, рождено %d, UsePedGroup %d, проверок %d, провалов %d\n", lvl, nz, ng, spawnTot, cmds, nchk, bad);
  return bad ? 1 : 0;
}
