// 107б П10 (каркас tests/proploss107): урон машине от удара о предмет по порогу оригинала. Ниже — прежний комментарий каркаса: потеря скорости машины о фонарь 1000 кг, простой предмет (props.c) и стейтпроп (stateprops.c, cls 10): запись PS2 1,0 + 0,28·v (2,5 / 3,75 / 6,4 м/с при 5,3 / 10,3 / 20,2). Хост: машина держит скорость v до 5,5 м
// от фонаря, дальше катится; потеря = наибольшее падение скорости за один кадр (между carUpdate и ударом). Берётся первый фонарь и курс из 8, на котором удар случился на всех трёх скоростях. Параметры: LEVEL_простые.shr LEVEL_стейтпропы.shr
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
static Props props; static ColWorld world; static StateProps S;
static unsigned char *load(const char *p, unsigned *size) { FILE *f = fopen(p, "rb"); if (!f) return NULL; fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); unsigned char *d = aligned_alloc(16, (n + 15) & ~15); if (fread(d, 1, n, f) != (size_t)n) return NULL; fclose(f); *size = (unsigned)n; return d; }
static void init(unsigned char *d, unsigned n) { unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = n; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv; colInit(&world, d + co, ce - co); propsInit(&props, d + po, *(unsigned *)(d + po + 40)); spInit(&S, props.spr, props.sprSize); }
// kind 0 — простой, 1 — стейтпроп; возврат: потеря (м/с), -1 — удара не было
static float vHit, lastDmg;   // скорость машины перед кадром удара
static float runOne(unsigned char *d, unsigned n, int kind, int idx, float ang, float v);
static float dmgDiff;   // урон от предмета = урон с правкой минус урон без неё в том же заезде (стены и бордюры сзади в обоих)
static float run(unsigned char *d, unsigned n, int kind, int idx, float ang, float v) {
  int keep = carDmgProp107Old; carDmgProp107Old = 1; float r0 = runOne(d, n, kind, idx, ang, v); float dm0 = lastDmg; carDmgProp107Old = keep; float r1 = runOne(d, n, kind, idx, ang, v); lastDmg = lastDmg - dm0; (void)r0; return r1;
}
static float runOne(unsigned char *d, unsigned n, int kind, int idx, float ang, float v) {
  if (kind == 0) propsFree(&props); else spFree(&S); init(d, n);
  float x = kind ? S.run[idx].x : props.st[idx].x, y = kind ? S.run[idx].y : props.st[idx].y, z = kind ? S.run[idx].z : props.st[idx].z, g = 0, fx = sinf(ang), fz = cosf(ang);
  if (!colGround(&world, x - fx * 14.f, z - fz * 14.f, y + 4.f, y - 8.f, &g, 0)) return -1.f;
  Car c; carInit(&c, 0, &world, x - fx * 14.f, g + 1.f, z - fz * 14.f, ang); carSetSpeed(&c, v); float best = 0.f; int hit = 0;
  for (int f = 0; f < 150; f++) { CarInput in = {0, 0, 0, 0}; if (hypotf(c.x - x, c.z - z) > 5.5f) carSetSpeed(&c, v); carUpdate(&c, &world, &in, 1.f / 30.f);
    float v0 = hypotf(c.vel[0], c.vel[2]); if (kind) spUpdate(&S, &world, &c, NULL, 1.f / 30.f); else propsUpdate(&props, &world, &c, 1.f / 30.f);
    float v1 = hypotf(c.vel[0], c.vel[2]); if (v0 - v1 > best) { best = v0 - v1; vHit = v0; }
    if (kind ? S.run[idx].state != 0 : props.st[idx].kicked) hit = 1; }
  lastDmg = c.damage; return hit ? best : -1.f;
}
int main(int argc, char **argv) {
  unsigned n1, n2; unsigned char *d1 = load(argv[1], &n1), *d2 = load(argv[2], &n2); if (!d1 || !d2) return 2;
  const float vs[3] = {6.f, 12.f, 16.f}; float dmgL[3] = {-1, -1, -1}, dmgC[3] = {-1, -1, -1}, dmgS[3] = {-1, -1, -1}; int il = -1, ic = -1, is = -1;
  init(d1, n1);
  for (unsigned i = 0; i < props.ninst && il < 0; i++) { const PropType *t = &props.types[props.def[i].type]; if (!(t->mass >= 1000.f && t->halfH >= 2.5f && t->radius < 1.f)) continue;
    for (int a = 0; a < 8 && il < 0; a++) { float ang = a * 0.785f + .2f, l[3], dm[3]; int ok = 1; for (int k = 0; k < 3 && ok; k++) { l[k] = run(d1, n1, 0, (int)i, ang, vs[k]); dm[k] = lastDmg; ok = l[k] >= 0.f; } if (ok) { il = (int)i; memcpy(dmgL, dm, sizeof dm); } } }
  for (unsigned i = 0; i < props.ninst && ic < 0; i++) { const PropType *t = &props.types[props.def[i].type]; if (!(t->mass > 0.f && t->mass <= 150.f && t->halfH < 1.5f && t->radius < 1.f)) continue;
    for (int a = 0; a < 8 && ic < 0; a++) { float ang = a * 0.785f + .2f, l[3], dm[3]; int ok = 1; for (int k = 0; k < 3 && ok; k++) { l[k] = run(d1, n1, 0, (int)i, ang, vs[k]); dm[k] = lastDmg; ok = l[k] >= 0.f; } if (ok) { ic = (int)i; memcpy(dmgC, dm, sizeof dm); } } }
  init(d2, n2);
  for (unsigned i = 0; i < S.ninst && is < 0; i++) { const SpType *t = &S.type[S.inst[i].type]; if (!(t->cls == 10 && t->mass >= 1000.f && t->nCol && !S.inst[i].tag)) continue;
    for (int a = 0; a < 8 && is < 0; a++) { float ang = a * 0.785f + .2f, l[3], dm[3]; int ok = 1; for (int k = 0; k < 3 && ok; k++) { l[k] = run(d2, n2, 1, (int)i, ang, vs[k]); dm[k] = lastDmg; ok = l[k] >= 0.f; } if (ok) { is = (int)i; memcpy(dmgS, dm, sizeof dm); } } }
  printf("lamp_idx %d\ncrate_idx %d\nsp_idx %d\n", il, ic, is);
  for (int k = 0; k < 3; k++) printf("v%d %.1f lamp %.4f crate %.4f sp %.4f\n", k, vs[k], dmgL[k], dmgC[k], dmgS[k]);
  return 0;
}
