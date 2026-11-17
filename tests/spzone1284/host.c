// 1284: сбитый стейтпроп (ящик) лежит, пока игрок не дальше SP_ZONE_R (400 м). Хост: первый обычный стейтпроп (не высокий, не ломаемый обломком, с состоянием после удара) переводится в состояние 1 (удар), игрок в 100 м и 300 м вне вида 60 с — лежит; в 450 м — возвращается домой.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
extern int spZone1284Old;
static Props props; static ColWorld world; static StateProps S;
static int never(void *u, float x, float y, float z, float r) { (void)u; (void)x; (void)y; (void)z; (void)r; return 0; }
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = aligned_alloc(16, (size + 15) & ~15); if (fread(d, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, d + co, ce - co); propsInit(&props, d + po, *(unsigned *)(d + po + 40)); spInit(&S, props.spr, props.sprSize);
  int chosen = -1, st100 = -1, st300 = -1, st450 = -1;
  for (unsigned i = 0; i < S.ninst && chosen < 0; i++) {
    const SpType *T = &S.type[S.inst[i].type]; if (T->kind != 0 || T->cls == 2 || T->cls == 10 || !T->nCol || S.inst[i].tag || T->nState < 2) continue;
    // кандидат: вернулся бы домой по прежнему правилу (60 м) — значит, подпадает под общий путь spRespawnStep
    int keep = spZone1284Old; spZone1284Old = 1;
    spFree(&S); spInit(&S, props.spr, props.sprSize); spEnter(&S, i, 1); unsigned st0 = S.run[i].state; float px = S.inst[i].x, pz = S.inst[i].z;
    if (st0 != 1 || !S.run[i].alive) { spZone1284Old = keep; continue; }
    for (int fr = 0; fr < 60 * 30; fr++) spRespawnStep(&S, px + 100.f, pz, 1.f / 30.f, never, NULL);
    int returnedOld = S.run[i].state != st0; spZone1284Old = keep;
    if (!returnedOld) continue;
    spFree(&S); spInit(&S, props.spr, props.sprSize); spEnter(&S, i, 1);
    const float ex[3] = {100.f, 300.f, 450.f}; int res[3];
    for (int q = 0; q < 3; q++) { for (int fr = 0; fr < 60 * 30; fr++) spRespawnStep(&S, px + ex[q], pz, 1.f / 30.f, never, NULL); res[q] = S.run[i].state == st0; }
    st100 = res[0]; st300 = res[1]; st450 = res[2]; chosen = (int)i;
  }
  printf("chosen %d\nlies_100 %d\nlies_300 %d\nlies_450 %d\n", chosen, st100, st300, st450);
  return 0;
}
