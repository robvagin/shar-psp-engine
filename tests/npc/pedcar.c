// Слой «персонаж ↔ машина» (сверка g02-traffic, по просьбе g07): персонаж миссии идёт по двум точкам, на отрезке стоит чужая машина (стоящая машина трафика).
// Прежнее (PEDCAR_ALL_OLD=1): проходит сквозь кузов. Новое: останавливается у кузова на 3,0 с, потом обходит по углам и доходит; кузов не пересекает.
//   tests/npc/pedcar.c LEVEL HOMER MSN
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); unsigned nmis = W.msn.f.nmissions; bwFree(&W);
  for (unsigned mi = 0; mi < nmis; mi++) {
    bwNew(&W); GameMsn *m = &W.msn; msn_start_mission(&m->rt, mi, 1); bwStep(&W, 0, 0.f, 0.f);
    Traffic *tr = (Traffic *)bwTraffic(); if (!tr || !tr->hd || !tr->npark || m->nnpc < 1) { bwFree(&W); continue; }
    GameNpc *n = &m->npc[0];
    // две точки: от места персонажа на 14 м вперёд по курсу, где есть земля
    float ux = 0, uz = 0, gy = 0; int ok = 0;
    for (int k = 0; k < 8 && !ok; k++) { float a = n->h + k * .785398f; ux = sinf(a); uz = cosf(a);
      for (int s = 2; s <= 14; s += 2) { float x = n->pos.x + ux * s, z = n->pos.z + uz * s; if (!colGround(bwCol(), x, z, n->pos.y + 1.f, n->pos.y - 1.f, &gy, 0)) break; if (s == 14) ok = 1; } }
    if (!ok) { bwFree(&W); continue; }
    n->nwp = 2; n->wp[0] = n->pos; n->wp[1].x = n->pos.x + ux * 14.f; n->wp[1].y = n->pos.y; n->wp[1].z = n->pos.z + uz * 14.f; n->iwp = 1; n->walking = 1; n->knock = 0; n->homing = 0; n->carWaitT = n->carFree = 0.f;
    float mx = n->pos.x + ux * 7.f, mz = n->pos.z + uz * 7.f, hh = atan2f(ux, uz) + 1.5707963f;   // машина поперёк пути, середина отрезка
    float px = n->pos.x, pz = n->pos.z, stood = 0.f, minClr = 99.f; int inside = 0, reached = 0; float t = 0.f;
    for (int f = 0; f < 60 * 40; f++) {
      TrParked *P = &tr->park[0]; carInit(&P->car, &tr->params[tr->parkd[0].model], bwCol(), mx, n->pos.y, mz, hh); P->gone = 0; P->off = 0; P->state = 0;   // стоящая машина на месте
      bwStep(&W, 0, 0.f, 0.f); t += 1.f / 60.f;
      static Car obs; carInit(&obs, &tr->params[tr->parkd[0].model], bwCol(), mx, n->pos.y, mz, hh); CarBox b; carBox(&obs, &b); float dx = n->pos.x - b.x, dz = n->pos.z - b.z;   // место машины к моменту шага персонажа (трафик после шага возвращает её на свой участок) float dx = n->pos.x - b.x, dz = n->pos.z - b.z;
      { float ca = fabsf(dx * b.fx + dz * b.fz) - b.hl, cb = fabsf(-dx * b.fz + dz * b.fx) - b.hw, c = ca > cb ? ca : cb; if (c < minClr) minClr = c; }
      if (fabsf(dx * b.fx + dz * b.fz) < b.hl + GAME_NPC_RADIUS - .05f && fabsf(-dx * b.fz + dz * b.fx) < b.hw + GAME_NPC_RADIUS - .05f) inside = 1;
      if (hypotf(n->pos.x - px, n->pos.z - pz) < 1e-5f && n->walking) stood += 1.f / 60.f; px = n->pos.x; pz = n->pos.z;
      if (hypotf(n->pos.x - n->wp[1].x, n->pos.z - n->wp[1].z) < .7f) { reached = 1; break; }   // дошёл до дальней точки (фоновые ходят по кругу — флаг walking не снимается)
    }
    printf("(наименьший просвет до кузова %.2f м) ", minClr); printf("mission %u %s: сквозь кузов прошёл=%d, стоял на месте %.1f с, дошёл=%d за %.1f с\n", mi, n->name, inside, stood, reached, t);
    int bad = inside || !reached || stood < 2.9f || stood > 3.5f;   // ровно одна остановка на 3,0 с
    printf("%s\n", bad ? "FAIL" : "PASS"); bwFree(&W); return bad;
  }
  printf("нет подходящей миссии\n"); return 2;
}
