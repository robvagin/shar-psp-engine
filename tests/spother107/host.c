// 107б П3: стейтпропы бьёт и чужая машина (stateprops.c spHitOther). Хост на настоящих данных уровня: для каждого из первых экземпляров с объёмом (alive, nCol, cls != 2) машина едет на него с 14 м на 10 м/с.
// A (игрок, spHitCar с монетами) — эталон: берём те экземпляры, чьё состояние меняется. B (чужая машина, spHitOther) на тех же: состояние должно сменяться так же, а счётчики игрока (coins, hnrHits, hits, touchInst) — нет.
// Ключи: ref (сколько экземпляров сменилось у игрока), other (у чужой машины), stray (сколько раз у чужой тронулись счётчики игрока). Параметры: LEVEL.shr [N экземпляров]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "coins.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
static Props props; static ColWorld world; static StateProps S; static Coins coins;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); coinsInit(&coins, NULL, 0);
  unsigned nmax = argc > 2 ? (unsigned)atoi(argv[2]) : 60; unsigned tried = 0, ref = 0, other = 0, stray = 0;
  if (spInit(&S, props.spr, props.sprSize)) return 3;
  unsigned n = S.ninst;
  for (unsigned i = 0; i < n && tried < nmax; i++) {
    const SpType *T = &S.type[S.inst[i].type]; if (T->kind != 0 || !T->nCol || T->cls == 2 || S.inst[i].tag) continue;
    float x = S.run[i].x, z = S.run[i].z, y = S.run[i].y, gy = 0.f, ang = 0.9f, fx = sinf(ang), fz = cosf(ang);
    if (!colGround(&world, x - fx * 14.f, z - fz * 14.f, y + 3.f, y - 6.f, &gy, 0)) continue;
    tried++; int st0[3]; int changed[3]; unsigned bh = 0; int bc = 0, bhit = 0;
    for (int who = 2; who >= 0; who--) {   // 2 — основа: чужая машина едет так же, но spHitOther выключен (всё, что меняется само или от самой машины), 0 — игрок (эталон), 1 — чужая машина
      spFree(&S); spInit(&S, props.spr, props.sprSize); Car c; carInit(&c, 0, &world, x - fx * 14.f, gy + 1.f, z - fz * 14.f, ang); carSetSpeed(&c, 10.f);
      st0[who] = S.run[i].state; unsigned h0 = S.hnrHits; int c0 = S.coins, hit0 = S.hits;
      for (int fr = 0; fr < 120; fr++) { CarInput in = {1.f, 0, 0, 0}; carUpdate(&c, &world, &in, 1.f / 30.f); spUpdate(&S, &world, who == 0 ? &c : NULL, who == 0 ? &coins : NULL, 1.f / 30.f); if (who >= 1) { int keep = spOther107Old; if (who == 2) spOther107Old = 1; spHitOther(&S, &world, &c); spOther107Old = keep; } }
      if (who == 2) { bh = S.hnrHits; bc = S.coins; bhit = S.hits; }
      changed[who] = S.run[i].state != st0[who] || !S.run[i].alive;
      if (who == 1 && (S.hnrHits != bh || S.coins != bc || S.hits != bhit || S.touchInst >= 0)) stray++; (void)h0; (void)c0; (void)hit0;
    }
    if (getenv("DBGSP") && changed[1]) printf("  i=%u type=%u cls=%d changed=%d/%d/%d hnr=%u leak=%u\n", i, S.inst[i].type, T->cls, changed[0], changed[1], changed[2], S.hnrHits, stray);
    if (changed[2]) continue;   // самосмена без машины — не считаем
    ref += changed[0]; other += changed[0] && changed[1]; if (!changed[0] && changed[1]) other += 0;
  }
  printf("tried %u\nref %u\nother %u\nstray %u\n", tried, ref, other, stray);
  return 0;
}
