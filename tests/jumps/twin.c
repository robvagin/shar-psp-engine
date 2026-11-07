// Правка Роба 342 (05.10): трамплины как у оригинала. Порт против записанных прыжков оригинала (близнец tools/twins/jumpref.py,
// свой PCSX2 без окна, PS2 PAL 50 к/с): для каждого заезда оригинала порт ставится в ту же точку разгона (место, курс, скорость
// на линии трамплина), едет тем же правилом (газ, пока скорость ниже цели бота; руль на линию до кромки, дальше прямо), и
// сравниваются четыре величины:
//   кромка  — скорость на кромке (у оригинала зона «jump boost» разгоняет машину на въезде, правило car.c jumpZoneDrive);
//   подъём  — вершина полёта над землёй точки синхронизации (у порта низ колёс; у оригинала начало модели минус ORIG_LIFT);
//   удар    — где по линии от кромки машина села после отрыва (земля, стена дома; касание вскользь — не посадка);
//   итог    — где машина через 3 с после удара (перелетела дом / встала у стены / уехала дальше; если обе уехали дальше 20 м
//             от удара — совпало: куда именно, решает руль после кромки, а не трамплин).
// Допуск: кромка ±7 % (не меньше 0,7 м/с), подъём ±15 % (не меньше 0,4 м),
// удар ±15 % (не меньше 2 м: где коснулась — нос или хвост, полкорпуса), итог ±15 % (не меньше 4 м).
// Те же модули, что у движка (car.c, col.c, props.c, stateprops.c: стёкла трамплинов бьются, как в кадре main.c).
//   tests/jumps/twin.sh [level.shr]            — сравнение с текущей физикой; код выхода = число провалов
//   tests/jumps/twin.sh --gate [level.shr]     — гейт: как есть PASS, прежнее правило 47 (подброс до верха зоны) обязано FAIL
// Известные расхождения (общая подвеска, не правило зоны) — tests/jumps/ramps_known.tsv: допуск вдвое шире, печать причины.
//   ключ --boost: прежнее правило подброса (car.c carJumpBoost = 1); --no-drive: без разгона зоны (carJumpZoneDriveOff) — контроли
// Metrics are sampled at 25 Hz on both sides. Comparing original 40 ms velocity
// changes with a port's 16.7 ms changes falsely misses a damped landing.
#ifndef JUMP_METRIC_HZ_OLD
#define JUMP_METRIC_HZ_OLD 0
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"

extern int carJumpBoost, carJumpZoneDriveOff;
unsigned homerSolidFrame = 0;   // заглушка: props.c тянет его из homer.c, в хосте Гомера нет (как tests/walls/props_break_host.c)
static Props props; static StateProps sp; static ColWorld W; static Car car; static PropState *st0;
static int onKick(void *u, unsigned i, float dx, float dz) {
  (void)u; (void)dx; (void)dz;
  int b = spBreakOf(&sp, props.def[i].type); if (b < 0) return 0;
  float pos[3] = {props.def[i].x, props.def[i].y, props.def[i].z};
  return spBreak(&sp, (unsigned)b, pos, props.def[i].q);
}
// начало модели машины оригинала над низом колёс: медиана по 14 заездам (у точки синхронизации, земля порта), 0,67–0,76 м
#define ORIG_LIFT 0.70f
static float wrapf(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

typedef struct { float rise, land, tland, end, vlip; int ok; } Res;
// один заезд порта из точки синхронизации; trace != NULL — построчная трасса (25 Гц, как у оригинала)
static Res runPort(float lx, float ly, float lz, float dx, float dz, float x0, float y0, float z0, float h0, float v0, float vt, FILE *trace) {
  (void)ly; Res r = {0, 0, 0, 0, -1, 0};
  memcpy(props.st, st0, sizeof(PropState) * props.ninst); props.havePreviousCar = 0; spFree(&sp); spInit(&sp, props.spr, props.sprSize);
  carInit(&car, NULL, &W, x0, y0 + 1.f, z0, h0);
  for (int k = 0; k < 10; k++) { CarInput in = {0}; carUpdate(&car, &W, &in, 1 / 60.f); }
  carSetSpeed(&car, v0);
  enum { NMAX = 14 * 60 };
  static float T[NMAX], A[NMAX], Y[NMAX], VY[NMAX];static int Ground[NMAX];
  const float dt = 1 / 60.f; float tLip = -1.f; int n = 0, crossed = 0;
  for (int fr = 0; fr < NMAX; fr++) {
    float along = (car.x - lx) * dx + (car.z - lz) * dz;
    CarInput in = {0}; float v = hypotf(car.vel[0], car.vel[2]);
    in.throttle = v < vt ? 1.f : 0.f;
    if (along < 0.f) {   // руль на точку линии в 15 м впереди (как бот оригинала)
      float a = fminf(along, 0.f) + 15.f, tx = lx + dx * a, tz = lz + dz * a;
      in.steer = fmaxf(-1.f, fminf(1.f, -2.f * wrapf(atan2f(tx - car.x, tz - car.z) - car.h)));
    }
    carUpdate(&car, &W, &in, dt); propsUpdate(&props, &W, &car, dt); spUpdate(&sp, &W, &car, NULL, dt);
    float t = (fr + 1) * dt; along = (car.x - lx) * dx + (car.z - lz) * dz; v = hypotf(car.vel[0], car.vel[2]);
    if (!crossed && along >= 0.f) { crossed = 1; tLip = t; r.vlip = v; }
    int sample=(int)((fr+1)*25/60)!=(int)(fr*25/60);
    if (sample || JUMP_METRIC_HZ_OLD) { T[n] = t; A[n] = along; Y[n] = car.y; VY[n] = car.vel[1]; Ground[n]=car.grounded;n++; }
    if (trace && sample)   // трасса по 0.04 с, как у оригинала
      fprintf(trace, "%.2f\t%.3f\t%.3f\t%.3f\t%.2f\t%.3f\t%.3f\t%d\t%.2f\t%.2f\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", t, car.x, car.y, car.z, along, v, car.vel[1], car.grounded,
              car.pitch * 57.2958f, car.roll * 57.2958f, car.wheels, car.comp[0], car.comp[1], car.comp[2], car.comp[3]);   // №1200: сжатие подвески колёс (0,1 — передние порта)   // №406: тангаж/крен (сверка с pitch_ps2.tsv), колёс на земле
  }
  // те же правила, что tools/twins/jumpref.py: вершина — до первого удара; посадка — удар (скачок vy вверх больше 2 м/с при
  // падении быстрее 1 м/с), кроме касания вскользь (подскочила выше места удара на 0,5 м и улетела дальше 5 м ниже него на 1,5 м)
  float apex = -1e9f, tImp = -1.f; int first = -1, kImp = -1;
  for (int k = 1; k < n; k++) {
    if (A[k] <= -8.f) continue;
    if (tLip >= 0.f && T[k] - tLip > 3.f && first < 0) break;
    if (first < 0 && Y[k] > apex) apex = Y[k];
    if (tLip >= 0.f && T[k] >= tLip && VY[k] - VY[k - 1] > 2.f && VY[k - 1] < -1.f) {
      if (first < 0) first = k;
      float mx = Y[k]; int down = 0,airAgain=0;
      for (int j = k; j < n && T[j] <= T[k] + 3.f; j++) {
        if (T[j] <= T[k] + 1.f) { if(Y[j]>mx)mx=Y[j];if(j>k&&!Ground[j])airAgain=1; }
        if (Y[j] < Y[k] - 1.5f && A[j] > A[k] + 5.f) down = 1;
      }
      // Following a rolling road while remaining supported is a real landing,
      // even if the road subsequently rises and drops. A roof skim flies again.
      if (mx - Y[k] > .5f && down && (airAgain || JUMP_METRIC_HZ_OLD)) continue;
      tImp = T[k]; kImp = k; break;
    }
  }
  r.end = A[n - 1];
  if (kImp >= 0) { r.land = A[kImp]; r.tland = tImp - tLip; for (int j = kImp; j < n; j++) if (T[j] >= tImp + 3.f) { r.end = A[j]; break; } }
  float g0 = car.y; colGround(&W, x0, z0, y0 + 1.f, -1e6f, &g0, 0);
  r.rise = apex - g0; r.ok = kImp >= 0;
  return r;
}

static int within(float port, float orig, float rel, float floor) { return fabsf(port - orig) <= fmaxf(rel * fabsf(orig), floor); }

int main(int argc, char **argv) {
  const char *lvl = "build/level.shr", *ref = "tests/jumps/ramps_ps2.tsv", *known = "tests/jumps/ramps_known.tsv", *traceDir = NULL; int boost = 0, noDrive = 0, pos = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--boost")) boost = 1;
    else if (!strcmp(argv[i], "--no-drive")) noDrive = 1;
    else if (!strcmp(argv[i], "--ref") && i + 1 < argc) ref = argv[++i];
    else if (!strcmp(argv[i], "--known") && i + 1 < argc) known = argv[++i];
    else if (!strcmp(argv[i], "--trace") && i + 1 < argc) traceDir = argv[++i];
    else if (pos++ == 0) lvl = argv[i];
  }
  carJumpBoost = boost; carJumpZoneDriveOff = noDrive;
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("нет %s\n", lvl); return 99; }
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15L); if (fread(data, 1, size, f) != (size_t)size) return 99; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || spInit(&sp, props.spr, props.sprSize)) {
    printf("уровень не читается\n"); return 99; }
  props.onKick = onKick;
  st0 = malloc(sizeof(PropState) * props.ninst); memcpy(st0, props.st, sizeof(PropState) * props.ninst);
  FILE *rf = fopen(ref, "r"); if (!rf) { printf("нет %s\n", ref); return 99; }
  printf("twin: %s, эталон %s%s%s\n", lvl, ref, boost ? " [--boost: прежнее правило 47]" : "", noDrive ? " [--no-drive: без разгона зоны]" : "");
  printf("%-12s %-11s | %-15s | %-17s | %-17s | %-11s | %-17s\n", "заезд", "трамплин", "кромка км/ч", "подъём ор/порт м", "удар ор/порт м", "до удара с", "итог ор/порт м");
  char line[1024]; int n = 0, fails = 0, nKnown = 0;
  while (fgets(line, sizeof line, rf)) {
    if (line[0] == '#' || line[0] == '\n') continue;
    char id[64], ramp[32]; float lx, ly, lz, dx, dz, x0, y0, z0, h0, v0, vt, oVlip, oApex, oLand, oTland, oEnd;
    if (sscanf(line, "%63s %31s %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", id, ramp, &lx, &ly, &lz, &dx, &dz, &x0, &y0, &z0, &h0, &v0, &vt,
               &oVlip, &oApex, &oLand, &oTland, &oEnd) != 18) continue;
    float g0 = y0; colGround(&W, x0, z0, y0 + 1.f, -1e6f, &g0, 0); float oRise = oApex - ORIG_LIFT - g0;
    FILE *tr = NULL;
    if (traceDir) { char p[512]; snprintf(p, sizeof p, "%s/%s%s.tsv", traceDir, id, boost ? "_boost" : ""); tr = fopen(p, "w"); }
    Res r = runPort(lx, ly, lz, dx, dz, x0, y0, z0, h0, v0, vt, tr);
    if (tr) fclose(tr);
    char kw[16] = "", why[256] = "";   // известное расхождение: допуск вдвое шире по указанным величинам
    FILE *kf = fopen(known, "r"); char kl[512];
    while (kf && fgets(kl, sizeof kl, kf)) { char kid[64], km[16]; int nn = 0;
      if (kl[0] != '#' && sscanf(kl, "%63s %15s %n", kid, km, &nn) == 2 && !strcmp(kid, id)) { strcpy(kw, km); snprintf(why, sizeof why, "%s", kl + nn); why[strcspn(why, "\n")] = 0; } }
    if (kf) fclose(kf);
    float kV = strchr(kw, 'v') ? 2.f : 1.f, kR = strchr(kw, 'r') ? 2.f : 1.f, kL = strchr(kw, 'l') ? 2.f : 1.f, kE = strchr(kw, 'e') ? 2.f : 1.f;
    int okV = within(r.vlip, oVlip, .07f * kV, .7f * kV), okR = within(r.rise, oRise, .15f * kR, .4f * kR),
        okL = r.ok && within(r.land, oLand, .15f * kL, 2.f * kL),
        okE = (oEnd - oLand > 20.f && r.end - r.land > 20.f) || within(r.end, oEnd, .15f * kE, 4.f * kE);   // обе уехали дальше — не сравнивать, куда
    nKnown += kw[0] != 0;
    int ok = okV && okR && okL && okE; fails += !ok; n++;
    printf("%-12s %-11s | %5.1f %5.1f %-3s | %6.2f %6.2f %-3s | %6.1f %6.1f %-3s | %5.2f %5.2f | %6.1f %6.1f %-3s %s\n", id, ramp,
           oVlip * 3.6f, r.vlip * 3.6f, okV ? "ok" : "НЕТ", oRise, r.rise, okR ? "ok" : "НЕТ", oLand, r.land, okL ? "ok" : "НЕТ", oTland, r.tland,
           oEnd, r.end, okE ? "ok" : "НЕТ", ok ? (kw[0] ? "~ известно" : "") : "<- FAIL");
    if (kw[0]) printf("             известное расхождение (%s, допуск x2): %s\n", kw, why);
  }
  fclose(rf);
  printf("twin: %s (%d заездов, %d вне допуска; известных расхождений %d — tests/jumps/ramps_known.tsv)\n", n && !fails ? "PASS" : "FAIL", n, fails, nKnown);
  return n ? fails : 99;
}
