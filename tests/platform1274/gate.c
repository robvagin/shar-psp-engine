// №1274: Гомер на подвижной платформе (animobj.c aoStep: rideOn/rideD/rideYaw). Хост на Маке, настоящие данные уровня. Для каждого объекта с живыми ящиками и действием (Toggle, AutoPlay, PlayAnim):
// точка ступней ставится на верх первого ящика (центр), клип идёт целиком (Toggle по кругу), каждый кадр: aoStep, смещение rideD прибавляется к ступням (как main_loop.h прибавляет к Гомеру), верх ящика —
// высота ступней (опора по вертикали держит прежний код). Платформа «держит», если ступни всё время в пределах ящика по плоскости (+0,3 м, как standsOnLift). Печатает по объекту: ход центра ящика, кадров вне ящика.
// Итог: объектов с горизонтальным ходом ≥ 1,5 м, из них удержало; при aoRide1274Old=1 — ожидаемо падает (красный контроль). Параметры: LEVEL.shr [old]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "animobj.h"
#include "car.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
void uiFlash(void) {}
static Props props; static ColWorld world; static StateProps S; static AnimObjs ao;
static int onBox(const ColBox *b, const float f[3]) {
  float d[3] = {f[0] - b->c[0], 0.f, f[2] - b->c[2]}; const float *ax[3] = {b->ax, b->ay, b->az};
  for (int q = 0; q < 3; q++) { if (fabsf(ax[q][1]) > .7f) continue; float s = d[0] * ax[q][0] + d[2] * ax[q][2]; if (fabsf(s) > b->h[q] + .3f) return 0; }
  return 1;
}
int main(int argc, char **argv) {
  if (argc < 2) return 2; if (argc > 2) aoRide1274Old = atoi(argv[2]);
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co); propsInit(&props, data + po, *(unsigned *)(data + po + 40)); spInit(&S, props.spr, props.sprSize);
  if (aoInit(&ao, data + po, *(unsigned *)(data + po + 40), &S, &world)) { printf("нет ANM1\n"); return 3; }
  unsigned nh = 0, nhold = 0; const float dt = 1.f / 30.f;
  for (unsigned k = 0; k < ao.nobj; k++) {
    const AoObj *o = &ao.obj[k]; if (!o->nVol || !o->nInst || (o->action == AO_DOORBELL || o->action == AO_AUTODOOR)) continue;   // дверям ход не нужен; AO_NONE (шестерни, вентиляторы, платформы без кнопки) — тоже семейство
    int v0 = -1; for (unsigned v = o->firstVol; v < (unsigned)o->firstVol + o->nVol; v++) if (ao.vol[v].kind == 0 && ao.vol[v].col < world.nbox) { v0 = (int)v; break; }
    if (v0 < 0) continue;
    for (unsigned i = o->firstInst; i < (unsigned)o->firstInst + o->nInst; i++) { S.run[i].poseF = 0.f; S.run[i].poseValid = 0; }
    aoPlace(&ao, &S, &world, k);
    const ColBox *b = &world.box[ao.vol[v0].col]; float c00[3] = {b->c[0], b->c[1], b->c[2]};
    float top = b->c[1] + fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2];
    float feet[3] = {b->c[0], top, b->c[2]}; float maxMove = 0.f, yawSum = 0.f, maxTilt = 0.f; unsigned out = 0, frames = 0, rode = 0;
    ao.run[k].playing = 1; ao.run[k].f = 0.f; ao.run[k].pad[1] = 3; if (o->action == AO_NONE) ao.run[k].f = 0.f;
    unsigned n = (unsigned)((o->nfr > 1 ? (float)o->nfr / (o->fps > 0.f ? o->fps : 30.f) : 3.f) / dt * 1.05f);
    for (unsigned s = 0; s < n; s++) {
      aoStep(&ao, &S, &world, feet[0], feet[1], feet[2], 1, 0, dt);
      if (ao.rideOn) { feet[0] += ao.rideD[0]; feet[2] += ao.rideD[2]; rode++; yawSum += fabsf(ao.rideYaw); }
      b = &world.box[ao.vol[v0].col]; feet[1] = b->c[1] + fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2];   // вертикаль держит прежняя опора
      float mv = hypotf(b->c[0] - c00[0], b->c[2] - c00[2]); if (mv > maxMove) maxMove = mv;
      { float uy = fmaxf(fabsf(b->ax[1]), fmaxf(fabsf(b->ay[1]), fabsf(b->az[1]))); float tl = acosf(uy > 1.f ? 1.f : uy); if (tl > maxTilt) maxTilt = tl; }
      frames++; if (!onBox(b, feet)) out++;
      if (getenv("DBG") && k == (unsigned)atoi(getenv("DBG")) && s % 6 == 0) printf("  s=%u f=%.1f c=(%.2f %.2f %.2f) ay=(%.2f %.2f %.2f) ax=(%.2f %.2f %.2f) az=(%.2f %.2f %.2f) h=(%.2f %.2f %.2f) feet=(%.2f %.2f %.2f) rideOn=%d\n", s, ao.run[k].f, b->c[0], b->c[1], b->c[2], b->ay[0], b->ay[1], b->ay[2], b->ax[0], b->ax[1], b->ax[2], b->az[0], b->az[1], b->az[2], b->h[0], b->h[1], b->h[2], feet[0], feet[1], feet[2], ao.rideOn);
    }
    if (maxMove < 1.5f && yawSum < .3f) continue;
    if (maxTilt > .35f) { printf("obj=%u action=%d: створка/люк (наклон %.2f рад), не платформа — не считаем\n", k, o->action, maxTilt); continue; }
    nh++; if (!out) nhold++;
    if (getenv("INFO")) printf("  INFO obj=%u trig=%d c=(%.1f %.1f %.1f) h=(%.1f %.1f %.1f) box0=(%.1f %.1f %.1f) top=%.2f\n", k, o->trig, o->c[0], o->c[1], o->c[2], o->h[0], o->h[1], o->h[2], c00[0], c00[1], c00[2], top);
    printf("obj=%u action=%d inst=%u nfr=%u ход=%.1f м поворот %.2f рад кадров %u вне ящика %u в ящике-носителе кадров %u\n", k, o->action, o->firstInst, o->nfr, maxMove, yawSum, frames, out, rode);
  }
  printf("ИТОГ подвижных платформ (ход ≥ 1,5 м или поворот) %u, удержало %u, old=%d\n", nh, nhold, aoRide1274Old);
  return nh && nhold == nh ? 0 : 1;
}
