// Гейт правки Роба 1241 (L2: игрок в машине NPC проехал сквозь оставленную свою): синтетический плоский мир, без данных игры.
//  A — игрок на полном газу едет в оставленную (стоит): удар был, центры не ближе 1,5 м, ОБЕ машины отреагировали: оставленная сдвинулась (> 1 м) и получила урон,
//      машина игрока потеряла скорость; после остановки оставленная засыпает (физика не вечная);
//  B — боком (под 90°): то же;
//  C — оставленная ниже/выше (разница высот > 2,5 м) не мешает: проезд как раньше (мост над улицей).
// Красный: -DOLDCARHIT1241_OLD=1 обязан упасть на A и B.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "car.h"
#include "traffic.h"
static int fails;
static void check(const char *n, int ok) { printf("%s %s\n", ok ? "ok" : "FAIL", n); fails += !ok; }
static uint32_t colBuf[1024];
static int flatWorld(ColWorld *W) {
  unsigned char *b = (unsigned char *)colBuf; memset(colBuf, 0, sizeof colBuf);
  ColHdr h; memset(&h, 0, sizeof h); memcpy(h.magic, "COL1", 4); h.ox = -200.f; h.oz = -200.f; h.cell = 400.f; h.W = h.H = 1; h.ntri = 2; h.nref = 2;
  memcpy(b, &h, 48);
  ColTri t[2] = {{{-200, 0, -200}, {-200, 0, 200}, {200, 0, 200}, {0, 1, 0}}, {{-200, 0, -200}, {200, 0, 200}, {200, 0, -200}, {0, 1, 0}}};
  memcpy(b + 48, t, sizeof t);
  uint32_t cs[2] = {0, 2}, refs[2] = {0, 1};
  memcpy(b + 48 + 96, cs, 8); memcpy(b + 48 + 96 + 8, refs, 8);
  return colInit(W, b, 48 + 96 + 16);
}
static void step(Car *c, const ColWorld *W, float thr) { CarInput in = {thr, 0, 0, 0}; carUpdate(c, W, &in, 1 / 60.f); }
static float gOwnMoved, gOwnDmg, gPlayerV, gPlayerVmax, gVhit, gVafter; static int gAsleep;
static int ram(const ColWorld *W, float h0, float oy, float *minD, int *hits) {   // игрок с (0,0) на курсе h0 едет к стоящей в 20 м впереди
  Car own; float ox = 20.f * sinf(h0), oz = 20.f * cosf(h0);
  carInit(&own, NULL, W, ox, 0.f, oz, h0 + 1.5707963f); for (int k = 0; k < 30; k++) step(&own, W, 0.f);
  own.y += oy;   // другой этаж: поднята после посадки на пол
  Car p; carInit(&p, NULL, W, 0.f, 0.f, 0.f, h0); p.huskDrive = 1; for (int k = 0; k < 30; k++) step(&p, W, 0.f);
  float ex = own.x, ez = own.z; *minD = 1e9f; *hits = 0; int awake = 0; float still = 0.f; gPlayerVmax = 0.f, gVhit = 0.f, gVafter = 1e9f; int hk = -1;
  for (int k = 0; k < 60 * 6; k++) { step(&p, W, 1.f); if (fabsf(p.v) > gPlayerVmax) gPlayerVmax = fabsf(p.v);
    { float vb = fabsf(p.v); int h_ = trafficHitPlayerStill(W, &p, &own, &awake, &still, 1 / 60.f); *hits += h_; if (h_ && hk < 0) { hk = k; gVhit = vb; } }
    if (hk >= 0 && k <= hk + 30 && fabsf(p.v) < gVafter) gVafter = fabsf(p.v);
    float d = hypotf(p.x - own.x, p.z - own.z); if (d < *minD) *minD = d; }
  gPlayerV = fabsf(p.v);
  for (int k = 0; k < 60 * 12; k++) { step(&p, W, 0.f); trafficHitPlayerStill(W, &p, &own, &awake, &still, 1 / 60.f); }   // игрок отпустил газ: оставленная доживает и засыпает
  gOwnMoved = hypotf(own.x - ex, own.z - ez); gOwnDmg = own.damage; gAsleep = !awake; return 0;
}
int main(void) {
  static ColWorld W; if (flatWorld(&W)) { printf("FAIL мир\n"); return 2; }
  float md; int hits;
  ram(&W, 0.f, 0.f, &md, &hits);
  printf("   A: ударов %d, ближе всего %.2f м, оставленная сдвинута на %.2f м, урон %.3f, игрок в момент удара %.1f, потом не выше %.1f м/с, уснула %d\n", hits, md, gOwnMoved, gOwnDmg, gVhit, gVafter, gAsleep);
  check("A: удар был, центры не ближе 1,5 м", hits > 0 && md > 1.5f);
  check("A2: оставленная сдвинулась (> 1 м) и получила урон", gOwnMoved > 1.f && gOwnDmg > 0.f);
  check("A3: машина игрока потеряла скорость от удара (за 0,5 с ниже 90% скорости удара)", gVafter < .9f * gVhit);
  check("A4: оставленная успокоилась и уснула", gAsleep);
  ram(&W, 1.5707963f, 0.f, &md, &hits);
  printf("   B: ударов %d, ближе всего %.2f м, сдвинута на %.2f м\n", hits, md, gOwnMoved);
  check("B: то же под 90° (удар, сдвиг)", hits > 0 && md > 1.5f && gOwnMoved > 1.f);
  ram(&W, 0.f, 4.f, &md, &hits);
  printf("   C: ударов %d, ближе всего %.2f м\n", hits, md);
  check("C: оставленная на 4 м выше (другой этаж) не мешает", hits == 0 || OLDCARHIT1241_OLD);
  printf("RESULT %s (%d провалов)\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
