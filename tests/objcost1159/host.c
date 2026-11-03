// №1159 (g11, регресс скорости TEST83→85): счётчики обходов предметов на данных уровней. (а) снаряд осы (boltnear1159.h): тысячи отрезков через центры настоящих предметов и стейтпропов и мимо; сколько записей
// дошло до дорогой проверки и тот же ли результат (сумма и число попаданий) при отсеве и при полном обходе (сборка с -DBOLTNEAR1159_OLD=1); (б) список будок (propbooth1159.h): совпадает с полным перебором;
// (в) spHitCar: сколько записей дошло до carContact и до дорогой части, с телами кустов (№1137) и без (-DBUSHFALL1137_OLD=1), машина в тысячах случайных мест рядом со стейтпропами
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
#include "boltnear1159.h"
#include "propbooth1159.h"
unsigned homerSolidFrame = 0; void uiFlash(void) {}
static Props props; static ColWorld world; static StateProps sp; static Car car; static Coins coins;
static unsigned rs = 12345u; static float rnd(void) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) * (1.f / 16777216.f); }
int main(int argc, char **argv) {
  for (int a = 1; a < argc; a++) {
    FILE *f = fopen(argv[a], "rb"); if (!f) continue; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f); unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) continue; fclose(f);
    unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size; if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
    spFree(&sp); memset(&props, 0, sizeof props); colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&sp, props.spr, props.sprSize);
    // (а) снаряд
    unsigned st[4] = {0, 0, 0, 0}; double sum = 0; unsigned hits = 0, segs = 0; rs = 12345u + (unsigned)a;
    for (int n = 0; n < 4000; n++) {
      float c[3] = {0, 0, 0}; int pick = (int)(rnd() * 3.f);
      if (pick == 0 && sp.ninst) { unsigned i = (unsigned)(rnd() * sp.ninst) % sp.ninst; float r_; spBounds(&sp, i, c, &r_); }
      else if (pick == 1 && props.ninst) { unsigned i = (unsigned)(rnd() * props.ninst) % props.ninst; c[0] = props.st[i].x; c[1] = props.st[i].y; c[2] = props.st[i].z; }
      else { c[0] = (rnd() - .5f) * 600.f; c[1] = rnd() * 20.f; c[2] = (rnd() - .5f) * 600.f; }
      float j = pick < 2 ? .9f : 0.f, d[3] = {rnd() - .5f, (rnd() - .5f) * .3f, rnd() - .5f}, dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + 1e-6f;
      float m[3] = {c[0] + (rnd() - .5f) * 2.f * j, c[1] + (rnd() - .5f) * 2.f * j, c[2] + (rnd() - .5f) * 2.f * j}, L = 1.f + rnd() * 3.f;
      float A[3] = {m[0] - d[0] / dl * L, m[1] - d[1] / dl * L, m[2] - d[2] / dl * L}, B[3] = {m[0] + d[0] / dl * L, m[1] + d[1] / dl * L, m[2] + d[2] / dl * L}, dir[3] = {d[0] / dl, d[1] / dl, d[2] / dl};
      float t = boltScan(&sp, &props, A, B, dir, 0, st); segs++; if (t <= 1.f) { hits++; sum += t; }
    }
    printf("L%d bolt segs %u hits %u sum %.6f sp_seen %u sp_deep %u pr_seen %u pr_deep %u\n", a, segs, hits, sum, st[0], st[1], st[2], st[3]);
    // (б) будки
    unsigned nb = ptBoothEnsure(&sp), brute = 0, same = 1; for (unsigned i = 0; i < sp.ninst; i++) { int b = ptIsBooth(&sp, &sp.type[sp.inst[i].type]); brute += (unsigned)b; if (b && (nb <= brute - 1 || ptBoothL[brute - 1] != i)) same = 0; }
    printf("L%d booth list %u brute %u same %u ninst %u\n", a, nb, brute, same && nb == brute, sp.ninst);
    // (в) spHitCar: машина в случайных местах рядом со стейтпропами
    memset(spHitStat, 0, sizeof spHitStat); unsigned calls = 0; rs = 777u + (unsigned)a;
    for (int n = 0; n < 3000 && sp.ninst; n++) {
      unsigned i = (unsigned)(rnd() * sp.ninst) % sp.ninst; float x = sp.run[i].x + (rnd() - .5f) * 30.f, z = sp.run[i].z + (rnd() - .5f) * 30.f, g = 0;
      if (!colGround(&world, x, z, sp.run[i].y + 6.f, -50.f, &g, 0)) continue;
      carInit(&car, 0, &world, x, g + .6f, z, rnd() * 6.28f); spHitCar(&sp, &world, &car, &coins); calls++;
    }
    printf("L%d hitcar calls %u seen/call %.1f entered/call %.2f deep/call %.3f\n", a, calls, calls ? (double)spHitStat[0] / calls : 0., calls ? (double)spHitStat[1] / calls : 0., calls ? (double)spHitStat[2] / calls : 0.);
  }
  return 0; }
