// Правка Роба 1200 (тангаж на склонах и трамплинах): трамплин jumpzone5 L1 (F10-8, склон x -172..-158, z 306..271 в осях движка, крыша F10-1
// за ним z 197..222, верх 12,6-14,4 м) против 5 записей оригинала PS2 (docs/handoff/ТРАМПЛИН_JZ5_СВЕРКА_2026-11-05.md,
// build/emu_hub/done/g01-cars_jz5roof/jump_*.tsv): порт ставится в ту же точку, что «толчок» записи, едет тем же правилом (газ, пока скорость ниже
// цели) и сверяется:
//   на кромке (линия z 270,5): скорость км/ч, вертикальная скорость vy м/с, тангаж °;
//   итог: КРЫША — первое касание после отрыва на крыше дома (y >= 11,8 и z 197..224), ФАСАД — удар о стену дома под крышей (y 8..11,8 на z 224..236),
//   УЛИЦА — упала на землю перед домом, ЗА_ДОМОМ — перелетела дом.
// Для ровного разгона с земли (как tests/airwalls/host.sh scan -165 306 180) есть режим "scan" (--kmh a:b:s --lat a:b:s).
//   tests/jumps/jz5.sh [level.shr]            — 5 записей PS2 против порта; код выхода 0 — все метрики в допуске
//   tests/jumps/jz5.sh --scan [level.shr]     — веер скоростей 96..112 км/ч прямо и со сдвигом
// Допуск (--gate): на кромке vy ±1,0 м/с и тангаж ±5° для прямых заездов 1, 3, 4; заезд 1 (100 км/ч) садится на КРЫШУ и съезжает на улицу за домом.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"

unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld W; static Car car; static CarParams carP; static int carOk;
static PropState *st0;
static int onKick(void *u, unsigned i, float dx, float dz) {
  (void)u; (void)dx; (void)dz;
  int b = spBreakOf(&sp, props.def[i].type); if (b < 0) return 0;
  float pos[3] = {props.def[i].x, props.def[i].y, props.def[i].z};
  return spBreak(&sp, (unsigned)b, pos, props.def[i].q);
}
static float wrapf(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

#define LIPZ 270.5f
typedef struct { float vlip, vylip, pitchlip, hlip, xlip, ylip; float ix, iy, iz; int impact; float maxY; const char *what; float ex, ey, ez; float lat; int roof; } Res;

// start: x,y,z, курс в осях движка (град), v0 м/с, vt км/ч (газ, пока ниже), punch: 1 — ставится в воздух над землёй и сразу получает скорость (как толчок записи)
static Res run(float x, float y, float z, float hdeg, float v0, float vt, int punch, FILE *tr, float secs) {
  Res r; memset(&r, 0, sizeof r); r.what = "?";
  memcpy(props.st, st0, sizeof(PropState) * props.ninst); props.havePreviousCar = 0; spFree(&sp); spInit(&sp, props.spr, props.sprSize);
  float h = hdeg * 0.01745329f, g = 0; colGround(&W, x, z, 400.f, -1e6f, &g, 0);
  if (punch) {
    carInit(&car, carOk ? &carP : NULL, &W, x, g, z, h);
    // оригинал: начало модели y над землёй на ~0,7 (ORIG_LIFT); толчок ставит машину в воздух над склоном
    car.y = fmaxf(g, y - 0.70f);
    // толчок записи не меняет поворот машины: на старте она стоит на дороге с носом +3,3° (кадр до толчка: тангаж 3,26°, курс 1,9°)
    { float p = 3.3f * 0.01745329f, ch = cosf(h * .5f), sh = sinf(h * .5f), cp = cosf(p * .5f), sp_ = sinf(-p * .5f);
      car.q[0] = ch * cp; car.q[1] = ch * sp_; car.q[2] = sh * cp; car.q[3] = -sh * sp_; memset(car.w, 0, sizeof car.w); }
  } else {
    carInit(&car, carOk ? &carP : NULL, &W, x, g, z, h);
    for (int k = 0; k < 20; k++) { CarInput in = {0}; carUpdate(&car, &W, &in, 1 / 60.f); }
  }
  carSetSpeed(&car, v0);
  if (punch) car.vel[1] = 0.19f * v0;   // толчок записи: скорость (v, 0,19·v вверх, 0) в осях склона (rj_punch.py: f32 a218b4 = vt*slope)
  const float dt = 1 / 60.f; int crossed = 0, first = 0; float prevVy = 0, vt_ms = vt / 3.6f; float tImp = -1;
  int stopped = 0;
  for (int fr = 0; fr < secs * 60; fr++) {
    CarInput in = {0}; float v = hypotf(car.vel[0], car.vel[2]);
    in.throttle = v < vt_ms ? 1.f : 0.f;
    carUpdate(&car, &W, &in, dt); propsUpdate(&props, &W, &car, dt); spUpdate(&sp, &W, &car, NULL, dt);
    float t = (fr + 1) * dt;
    if (!crossed && car.z <= LIPZ) { crossed = 1; r.vlip = hypotf(car.vel[0], car.vel[2]) * 3.6f; r.vylip = car.vel[1]; r.pitchlip = car.pitch * 57.2958f; r.hlip = car.h * 57.2958f; r.xlip = car.x; r.ylip = car.y; }
    if (crossed && !first && car.vel[1] - prevVy > 2.f && prevVy < -1.f) { first = 1; r.impact = 1; r.ix = car.x; r.iy = car.y; r.iz = car.z; tImp = t; }
    if (crossed && !first && car.y > r.maxY) r.maxY = car.y;
    if (crossed && car.wheels > 0 && car.y >= 11.8f && car.z >= 195.f && car.z <= 225.f) r.roof++;
    prevVy = car.vel[1];
    if (tr && (int)((fr + 1) * 25 / 60) != (int)(fr * 25 / 60))
      fprintf(tr, "%.2f\t%.3f\t%.3f\t%.3f\t%.2f\t%.3f\t%.3f\t%d\t%.2f\t%.2f\t%d\t%.3f\t%.3f\t%.3f\t%.3f\n", t, car.x, car.y, car.z, LIPZ - car.z, v, car.vel[1], car.grounded, car.pitch * 57.2958f, car.roll * 57.2958f, car.wheels, car.comp[0], car.comp[1], car.comp[2], car.comp[3]);
    if (hypotf(car.vel[0], car.vel[2]) < 1.5f && fr > 120 && car.grounded) { if (++stopped > 90) break; } else stopped = 0;
  }
  r.ex = car.x; r.ey = car.y; r.ez = car.z;
  // итог: КРЫША — колёса стояли на крыше дома (y >= 11,8, z 195..225) хоть один кадр; иначе по концу: перед домом (ФАСАД: упала у стены, z > 228) / за домом (ПЕРЕЛЕТ)
  if (!r.impact) r.what = "НЕ_СЕЛА";
  else if (r.roof) r.what = "КРЫША";
  else if (r.ez > 228.f) r.what = "ФАСАД/УЛИЦА";
  else if (r.ez < 195.f) r.what = "ПЕРЕЛЕТ";
  else r.what = "ДОМ_ДРУГОЕ";
  (void)tImp;
  return r;
}

static int load(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15L); if (fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), cr = *(unsigned *)(data + 100), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || spInit(&sp, props.spr, props.sprSize)) return 2;
  props.onKick = onKick;
  carDefaults(&carP);
  if (cr && cr < (unsigned)size) { unsigned ce2 = (unsigned)size; if (pv > cr && pv < ce2) ce2 = pv; carOk = carParamsFromCar1(data + cr, ce2 - cr, &carP) == 1; }
  st0 = malloc(sizeof(PropState) * props.ninst); memcpy(st0, props.st, sizeof(PropState) * props.ninst);
  return 0;
}

static void printRes(const char *id, const Res *r, float ps2v, float ps2vy, float ps2pitch, const char *ps2what) {
  printf("%-22s | кромка %5.1f км/ч vy %5.2f (PS2 %5.2f) тангаж %5.1f° (PS2 %5.1f°) | первое касание x %7.2f y %5.2f z %6.2f %-13s (PS2 %s) | конец %.1f %.1f %.1f\n",
         id, r->vlip, r->vylip, ps2vy, r->pitchlip, ps2pitch, r->ix, r->iy, r->iz, r->what, ps2what, r->ex, r->ey, r->ez);
  (void)ps2v;
}

int main(int argc, char **argv) {
  const char *lvl = "build/level.shr"; int scan = 0, gate = 0, pos = 0; const char *traceDir = NULL;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--scan")) scan = 1;
    else if (!strcmp(argv[i], "--gate")) gate = 1;
    else if (!strcmp(argv[i], "--trace") && i + 1 < argc) traceDir = argv[++i];
    else if (pos++ == 0) lvl = argv[i];
  }
  if (load(lvl)) { printf("нет %s\n", lvl); return 99; }
  printf("jz5: %s, машина %s (CAR1 %s)\n", lvl, carP.name, carOk ? "v2" : "нет");
  if (scan) {
    // ровный разгон с земли (как host scan -165 306 180): прямо и со сдвигом +6 м вбок на курсе 180-8
    float lats[3] = {0.f, 6.f, -6.f};
    for (int li = 0; li < 3; li++) for (float k = 96; k <= 112.1f; k += 4) {
      float lat = lats[li], x = -165.f + lat, z = 306.f;
      Res r = run(x, 0, z, 180.f, k / 3.6f, k, 0, NULL, 10.f);
      char id[40]; snprintf(id, sizeof id, "scan lat %+.0f %3.0f км/ч", lat, k);
      printf("%-22s | кромка %5.1f км/ч vy %5.2f тангаж %5.1f° | касание x %7.2f y %5.2f z %6.2f %-13s | конец %.1f %.1f %.1f\n", id, r.vlip, r.vylip, r.pitchlip, r.ix, r.iy, r.iz, r.what, r.ex, r.ey, r.ez);
    }
    return 0;
  }
  // 5 записей PS2 (толчок): x, y(начала модели), z — оси движка, курс движка = 180 - курс PS2
  struct { const char *id; float x, y, z, h, v0, vt, ps2v, ps2vy, ps2pitch; const char *what; } rec[5] = {
    {"1_100_прямо",      -165.00f,  6.54f, 300.50f, 180.f -  1.9f, 100.f / 3.6f, 100.f, 99.4f, 11.27f, 23.3f, "КРЫША (скользнула по кромке z 230,3 y 13,1, заехала на крышу z 221..197, съехала на улицу z 156)"},
    {"2_100_сдвиг-6_к8", -172.11f, 10.70f, 278.42f, 180.f - 11.3f, 100.f / 3.6f, 100.f, 106.4f, 9.40f, 21.8f, "ФАСАД/УЛИЦА (удар о фасад y 11,7 z 234,3, упала на улицу перед домом)"},
    {"3_70_прямо",       -165.00f,  6.54f, 300.50f, 180.f -  2.0f,  70.f / 3.6f,  70.f, 69.8f, 9.34f, 22.8f, "ФАСАД/УЛИЦА (упала на улицу z 235,9)"},
    {"4_110_прямо",      -165.00f,  6.54f, 300.50f, 180.f -  2.0f, 110.f / 3.6f, 110.f, 108.4f, 12.04f, 17.6f, "КРЫША (западный край, z 221,5..206,8, потом дальше)"},
    {"5_104_сдвиг-6_к8", -172.11f, 10.70f, 278.42f, 180.f - 16.5f, 104.f / 3.6f, 104.f, 101.0f, 8.38f, 12.4f, "КРЫША (z 221,3..206,1, съехала на улицу z 156)"}};
  int bad = 0;
  for (int i = 0; i < 5; i++) {
    FILE *tr = NULL; if (traceDir) { char p[512]; snprintf(p, sizeof p, "%s/jz5_%d.tsv", traceDir, i + 1); tr = fopen(p, "w"); }
    Res r = run(rec[i].x, rec[i].y, rec[i].z, rec[i].h, rec[i].v0, rec[i].vt, 1, tr, 9.f);
    if (tr) fclose(tr);
    printRes(rec[i].id, &r, rec[i].ps2v, rec[i].ps2vy, rec[i].ps2pitch, rec[i].what);
    if (gate && (i == 0 || i == 3)) {
      if (strcmp(r.what, "КРЫША")) { printf("   FAIL: заезд %s должен сесть на КРЫШУ, получилось %s\n", rec[i].id, r.what); bad++; }
    }
  }
  if (gate) printf("jz5 gate: %s\n", bad ? "FAIL" : "PASS");
  return gate ? bad : 0;
}
