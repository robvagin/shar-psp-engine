// №903 гейт: деревья и столбы L1 сбиваются только касанием ствола. Машина (настоящие car.c/props.c/col.c, данные level.shr) едет
// мимо каждого экземпляра тонких типов с зазором GAP м между бортом и стволом — проп должен остаться спящим; удар в лоб — проснуться.
// Прежнее (-DPROP_TRUNK_SLICE_OLD=1): у деревьев без вершин в полосе бампера (L1 тип 8) удар по радиусу кроны 3,5 м — «мимо» будит
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }   // 107б: заглушка сторожа
static Props props; static StateProps sp; static ColWorld world; static Car car;
static int kickAny(void *u, unsigned i, float dx, float dz) { (void)u; (void)i; (void)dx; (void)dz; return 0; }
static int run(unsigned i, float lat) {   // 1 — проп задет (проснулся/спрятан), 2 — задет, но машину до этого развернуло
  for (unsigned k = 0; k < props.ninst; k++) { props.st[k].asleep = 1; props.st[k].hidden = 0; props.st[k].hitCool = 0.f; }
  PropState *s = &props.st[i]; float h0[3] = {s->x, s->y, s->z}, hx = s->x, hz = s->z;
  const float *tk = props.trunk + props.def[i].type * 3;
  if (tk[2] > 0.f) { float m[16]; propPieceMatrix(s, tk[0], s->cy, tk[1], 1.f, m); hx = m[12]; hz = m[14]; }   // ствол в мире (у кроны набок он не в центре коробки)
  float gy = s->y; colGround(&world, hx, hz, s->y + 3.f, -1e6f, &gy, 0);
  carInit(&car, 0, &world, hx + lat, gy + 1.f, hz - 8.f, 0.f); carSetSpeed(&car, 12.f);
  CarInput in = {1.f, 0, 0, 0}; int hit = 0;
  for (int st = 0; st < 45; st++) {
    car.x = hx + lat; car.vel[0] = 0.f;   // держим курс: только проезд мимо на заданном удалении
    carUpdate(&car, &world, &in, 1.f / 30.f); propsUpdate(&props, &world, &car, 1.f / 30.f);
    if (!s->asleep || s->hidden || fabsf(s->x - h0[0]) + fabsf(s->z - h0[2]) > .05f) { CarBox b0; carBox(&car, &b0);
      int other = 0; for (unsigned k = 0; k < props.ninst; k++) other |= k != i && !props.st[k].asleep;   // соседа сбили раньше — он и толкнул
      hit = other || fabsf(atan2f(b0.fx, b0.fz)) > .15f || fabsf(b0.x - hx) < fabsf(lat) - .3f ? 2 : 1;   // 2 — машину развернуло или прижало к стволу стеной/бордюром до встречи: не проезд мимо
      if (getenv("DBG903")) { CarBox b; carBox(&car, &b); float dx = hx - b.x, dz = hz - b.z; printf("   inst %u lat %.2f: удар на шаге %d, кузов x %.2f z %.2f курс %.2f hw %.2f hl %.2f, ствол %.2f %.2f (центр коробки %.2f %.2f), dx %.2f dz %.2f asleep %d hidden %d\n", i, lat, st, b.x, b.z, atan2f(b.fx, b.fz), b.hw, b.hl, hx, hz, h0[0], h0[2], dx, dz, s->asleep, s->hidden); }
      break; }
  }
  return hit;
}
int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "level.shr"; float gap = argc > 2 ? atof(argv[2]) : 1.0f;
  FILE *f = fopen(path, "rb"); if (!f) { printf("нет %s\n", path); return 2; }
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f); unsigned char *data = malloc((size + 15) & ~15); fread(data, 1, size, f); fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40));
  if (props.spr) spInit(&sp, props.spr, props.sprSize); props.onKick = kickAny;
  CarBox cb; carInit(&car, 0, &world, 0, 3, 0, 0); carBox(&car, &cb);
  int fails = 0;
  for (unsigned t = 0; t < props.ntype; t++) {
    if (!props.trunk || props.types[t].radius < 1.5f || 2.f * props.types[t].halfH < 4.f) continue;   // высокие с широкой кроной: деревья
    int n = 0, nearHit = 0, headHit = 0, tested = 0;
    for (unsigned i = 0; i < props.ninst && tested < 12; i++) {
      if (props.def[i].type != t) continue; n++;
      float lat = cb.hw + gap + (props.trunk[t * 3 + 2] > 0.f ? props.trunk[t * 3 + 2] : 0.3f);
      int a = run(i, lat), b = run(i, -lat), c = run(i, 0.f); tested++;
      nearHit += a == 1 || b == 1; headHit += c != 0;
    }
    if (!tested) continue;
    int bad = props.trunk[t * 3 + 2] > 0.f && nearHit > 0;   // со стволом: проезд мимо не будит (в лоб — справочно: склоны и стены у места)
    printf("тип %2u радиус %.2f ствол %.2f: мимо в %.1f м задето %d из %d, в лоб %d из %d -> %s\n", t, props.types[t].radius,
           props.trunk[t * 3 + 2], gap, nearHit, tested, headHit, tested, bad ? "FAIL" : props.trunk[t * 3 + 2] > 0.f ? "PASS" : "без ствола (широкое у земли)");
    fails += bad;
  }
  printf(fails ? "ИТОГ FAIL %d\n" : "ИТОГ PASS\n", fails); return fails ? 1 : 0;
}
