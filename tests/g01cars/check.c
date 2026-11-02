// Проверки физики машины по строкам сверки g01-cars (круг 6). Каждая проверка печатает «имя значение»; run.sh собирает программу с
// выключателем _OLD=1 и без него и требует: новое значение в границах, значение с _OLD=1 — вне границ (тест падает на старом).
// Полигон — тот же, что у tools/cartest/feel.c (земля, стена z = 300, трамплин 15°); числа ожидаемого — из поведения оригинала, не из порта.
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "col.h"
#include "car.h"
#define PI 3.14159265f
#define FPS 60
#define DEG (180.f / PI)
static ColWorld W;
static ColTri T[4096]; static int nT;
static ColBox B[64]; static int nB;
static ColWorld W;
static void tri(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz) {
  ColTri *t = &T[nT++]; t->a[0] = ax; t->a[1] = ay; t->a[2] = az; t->b[0] = bx; t->b[1] = by; t->b[2] = bz; t->c[0] = cx; t->c[1] = cy; t->c[2] = cz;
  float ux = bx - ax, uy = by - ay, uz = bz - az, vx = cx - ax, vy = cy - ay, vz = cz - az;
  float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx, l = sqrtf(nx * nx + ny * ny + nz * nz);
  t->n[0] = nx / l; t->n[1] = ny / l; t->n[2] = nz / l;
}
// четырёхугольник земли над [x0,x1] x [z0,z1], высоты по z: y0 у z0, y1 у z1 (нормаль вверх)
static void quad(float x0, float x1, float z0, float z1, float y0, float y1) {
  tri(x0, y0, z0, x0, y1, z1, x1, y0, z0);
  tri(x0, y1, z1, x1, y1, z1, x1, y0, z0);
}
static void box(float cx, float cy, float cz, float hx, float hy, float hz) {
  ColBox *b = &B[nB++]; memset(b, 0, sizeof *b);
  b->c[0] = cx; b->c[1] = cy; b->c[2] = cz; b->ax[0] = 1; b->ay[1] = 1; b->az[2] = 1; b->h[0] = hx; b->h[1] = hy; b->h[2] = hz;
}
static void *colBlob;
static void buildWorld(void) {
  // земля: квадраты 50 м на x [-300, 300], z [-300, 1000]
  for (float x = -300; x < 300; x += 50) for (float z = -300; z < 1000; z += 50) quad(x, x + 50, z, z + 50, 0, 0);
  box(0, 2, 301, 40, 2, 1);            // стена: грань z = 300, x -40..40
  box(80, 2, 300.5f, 0.5f, 2, 0.5f);   // столб 1 x 1 м в (80, 300)
  box(-100, 0.15f, 150, 20, 0.15f, 50);   // бордюр-площадка 0.3 м: z 100..200
  quad(140, 160, 100, 112.44f, 0.f, 3.333f);   // трамплин 15 градусов, 12.4 м, высота 3.33 (торец открыт)
  float ox = -300, oz = -300, cell = 4; int Wd = 150, Hd = 325;
  // клетки: у каждого примитива — все клетки его xz-габарита
  int ncell = Wd * Hd; uint32_t *cnt = calloc(ncell + 1, 4);
  #define RANGE(x0, z0, x1, z1) int gx0 = (int)floorf(((x0) - ox) / cell), gz0 = (int)floorf(((z0) - oz) / cell), \
      gx1 = (int)floorf(((x1) - ox) / cell), gz1 = (int)floorf(((z1) - oz) / cell); \
      if (gx0 < 0) gx0 = 0; if (gz0 < 0) gz0 = 0; if (gx1 >= Wd) gx1 = Wd - 1; if (gz1 >= Hd) gz1 = Hd - 1;
  for (int pass = 0; pass < 2; pass++) {
    uint32_t *refs = 0, *fill = 0;
    if (pass == 1) {
      uint32_t acc = 0; for (int i = 0; i < ncell; i++) { uint32_t k = cnt[i]; cnt[i] = acc; acc += k; } cnt[ncell] = acc;
      size_t sz = 48 + nT * 48 + nB * 64 + (ncell + 1) * 4 + acc * 4 + 16;
      colBlob = calloc(1, sz); char *p = colBlob;
      memcpy(p, "COL1", 4); float *hf = (float *)(p + 4); hf[0] = ox; hf[1] = oz; hf[2] = cell;
      uint32_t *hu = (uint32_t *)(p + 16); hu[0] = Wd; hu[1] = Hd; hu[2] = nT; hu[3] = nB; hu[4] = 0; hu[5] = 0; hu[6] = acc; hu[7] = 0;
      memcpy(p + 48, T, nT * 48); memcpy(p + 48 + nT * 48, B, nB * 64);
      uint32_t *cs = (uint32_t *)(p + 48 + nT * 48 + nB * 64); memcpy(cs, cnt, (ncell + 1) * 4);
      refs = cs + ncell + 1; fill = calloc(ncell, 4); memcpy(fill, cnt, ncell * 4);
    }
    for (int i = 0; i < nT; i++) {
      ColTri *t = &T[i];
      float x0 = fminf(t->a[0], fminf(t->b[0], t->c[0])), x1 = fmaxf(t->a[0], fmaxf(t->b[0], t->c[0]));
      float z0 = fminf(t->a[2], fminf(t->b[2], t->c[2])), z1 = fmaxf(t->a[2], fmaxf(t->b[2], t->c[2]));
      RANGE(x0, z0, x1, z1)
      for (int gz = gz0; gz <= gz1; gz++) for (int gx = gx0; gx <= gx1; gx++) { int c = gz * Wd + gx; if (pass) refs[fill[c]++] = (0u << 29) | i; else cnt[c]++; }
    }
    for (int i = 0; i < nB; i++) {
      ColBox *b = &B[i];
      RANGE(b->c[0] - b->h[0], b->c[2] - b->h[2], b->c[0] + b->h[0], b->c[2] + b->h[2])
      for (int gz = gz0; gz <= gz1; gz++) for (int gx = gx0; gx <= gx1; gx++) { int c = gz * Wd + gx; if (pass) refs[fill[c]++] = (1u << 29) | i; else cnt[c]++; }
    }
    if (pass) free(fill);
  }
  uint32_t *hu = (uint32_t *)((char *)colBlob + 16);
  size_t sz = 48 + nT * 48 + nB * 64 + (ncell + 1) * 4 + hu[6] * 4;
  int r = colInit(&W, colBlob, (unsigned)sz);
  if (r) { fprintf(stderr, "colInit: %s\n", colError(r)); exit(1); }
  free(cnt);
}

// ------------------------------------------------------------------------------------------------ машина
static CarParams P0;
static void place(Car *c, float x, float z, float h) { carParamsFor("famil_v", &P0); carInit(c, &P0, &W, x, 0.f, z, h); }
static void step(Car *c, float thr, float brk, float steer, int hb) {
  CarInput in; memset(&in, 0, sizeof in); in.throttle = thr; in.brake = brk; in.steer = steer; in.handbrake = (float)hb;
  carUpdate(c, &W, &in, 1.f / FPS);
}
static void run(Car *c, int n, float thr, float brk, float steer, int hb) { for (int i = 0; i < n; i++) step(c, thr, brk, steer, hb); }
static void rollBy(Car *c, float degs) {   // крен вокруг оси вперёд: правый борт вверх; левые колёса остаются на земле
  float a = degs / DEG, s = sinf(a * .5f), co = cosf(a * .5f), q[4] = {co, 0, 0, -s}, *p = c->q, r0 = p[0], r1 = p[1], r2 = p[2], r3 = p[3];
  p[0] = r0 * q[0] - r1 * q[1] - r2 * q[2] - r3 * q[3]; p[1] = r0 * q[1] + r1 * q[0] + r2 * q[3] - r3 * q[2];
  p[2] = r0 * q[2] - r1 * q[3] + r2 * q[0] + r3 * q[1]; p[3] = r0 * q[3] + r1 * q[2] - r2 * q[1] + r3 * q[0];
  c->y += sinf(a) * c->p.wheelX + 0.05f;
}
static float spd(const Car *c) { return sqrtf(c->vel[0] * c->vel[0] + c->vel[1] * c->vel[1] + c->vel[2] * c->vel[2]); }
static float fwdSpd(const Car *c) { return c->v; }
#define OUT(name, val) printf("%s %.4f\n", name, (double)(val))
// старт на ровной земле далеко от стены и бордюра: x = -200, z = -250 (курс 0 — вдоль +z)
static void start(Car *c, float v) { carParamsFor("famil_v", &P0); place(c, -200.f, -250.f, 0.f); run(c, 30, 0, 0, 0, 0); carSetSpeed(c, v); }

// 1.1 множитель руля на TopSpeed = Drop (0,27)
static void t_steer_top(void) { Car c; start(&c, P0.topSpeed); carParamsFor("famil_v", &P0); run(&c, 60, 1, 0, 1, 0); OUT("steer_top", fabsf(c.wheelSteer) / c.p.maxWheelTurn); }
// 1.1+1.3 на 3 м/с: (30° + 10°) × (1 − 0,73 × доля)
static void t_steer_low(void) { Car c; start(&c, 3.f); run(&c, 60, 0, 0, 1, 0); OUT("steer_low_deg", fabsf(c.wheelSteer) * DEG); }
// 1.2 руль игрока на месте идёт линейно: за 0,1 с — 0,1 / 0,3
static void t_steer_rate(void) { Car c; start(&c, 0.f); run(&c, 6, 0, 0, 1, 0); OUT("steer_rate", fabsf(c.steer)); }
// 1.6 ручник защёлкивается: через 0,33 с после отпускания ещё держится (в этот момент lastHb = 1)
static void t_hblatch(void) { Car c; start(&c, 10.f); step(&c, 0, 0, 0, 1); run(&c, 20, 0, 0, 0, 0); OUT("hblatch", c.lastHb); }
// 1.5 на 90 % TopSpeed тормоз ограничен 1 − доля = 0,1: замедление = 2·11·0,1 + качение 2 + сопротивление без поправки 0,25 (тормоз > 0): 2·G·0,81 ≈ 11,5 м/с²
static void t_brake_limit(void) { Car c; start(&c, 0.9f * P0.topSpeed); float v0 = fwdSpd(&c); run(&c, 6, 0, 1, 0, 0); OUT("brake_limit_decel", (v0 - fwdSpd(&c)) / 0.1f); }
// 2.4 тормоз на 10 м/с: 2·(BrakeScale + 3)·(1 − доля) + качение 2 + сопротивление (за 0,2 с, м/с²)
static void t_brake_force(void) { Car c; start(&c, 10.f); float v0 = fwdSpd(&c); run(&c, 12, 0, 1, 0, 0); OUT("brake_force", (v0 - fwdSpd(&c)) / 0.2f); }
// 2.5 накат на 28 м/с (выше 50 км/ч, без газа и тормоза): качение 2 + сопротивление 2·G·доля²·0,25 ≈ 3,6 м/с²
static void t_coast(void) { Car c; start(&c, 28.f); float v0 = fwdSpd(&c); run(&c, 12, 0, 0, 0, 0); OUT("coast", (v0 - fwdSpd(&c)) / 0.2f); }
// 2.1 ручник не отключает газ: со стоянки с газом и ручником скорость растёт хотя бы до 0,5 м/с
static void t_hb_throttle(void) { Car c; start(&c, 0.f); run(&c, 30, 1, 0, 0, 1); OUT("hb_throttle_speed", spd(&c)); }
// 2.2 рывок: ручник и газ 3 с, отпуск — тяга ×2 столько секунд, сколько накоплено (темп ×2, не дольше MaxSpeedBurstTime / 2)
static float burstSecs(float thrAtRelease, int holdFrames) {   // газ после отпуска ручника: thrAtRelease первые 0,7 с (защёлка ручника 0,5 с), потом полный
  Car c; start(&c, 0.f); run(&c, holdFrames, 1, 0, 0, 1); int n = 0;
  for (int i = 0; i < 10 * FPS; i++) { step(&c, i < 42 ? thrAtRelease : 1.f, 0, 0, 0); if (c.burstOn) n++; }
  return n / (float)FPS;
}
static void t_burst_cap(void) { carParamsFor("famil_v", &P0); OUT("burst_time", P0.burstTime); OUT("burst_cap_secs", burstSecs(1.f, 3 * FPS)); }
static void t_burst_cancel(void) { OUT("burst_cancel_secs", burstSecs(0.f, 3 * FPS)); }
// 2.3 задний ход без предела 50 км/ч: тормоз 25 с со стоянки, км/ч назад
static void t_reverse(void) { Car c; start(&c, 0.f); c.z = 900.f; run(&c, 25 * FPS, 0, 1, 0, 0); OUT("reverse_kmh", -fwdSpd(&c) * 3.6f); }
// 2.5/2.6 газ до упора на 90 % TopSpeed: ускорение = тяга 2·G − сопротивление 2·G·доля² (качения при газе нет) = 1,71 м/с²
static void t_topspeed(void) { Car c; start(&c, 0.9f * P0.topSpeed); float v0 = fwdSpd(&c); run(&c, 30, 1, 0, 0, 0); OUT("top_accel", (fwdSpd(&c) - v0) / 0.5f); }
// 3.1 разворот задним ходом: назад 8 м/с, руль 1, без газа и ручника — боковое сопротивление почти ноль (latK), доля > 0,15
static void t_jturn_drop(void) { Car c; start(&c, -8.f); run(&c, 16, 0, 0, 1, 0); OUT("jturn_latK", c.latK); }
static void t_jturn_slow(void) { Car c; start(&c, -2.f); run(&c, 16, 0, 0, 1, 0); OUT("jturn_slow_flag", c.jturn); }
// 5.1 в воздухе крутящий момент гасится 7·m·ω (игрок ещё 3·m·ω): ω(t) = ω0·exp(−10·m/I·t); крен 3 рад/с, через 0,3 с — доля от ω0
static void t_airstab(void) {
  Car c; place(&c, -200.f, -250.f, 0.f); c.y = 40.f; c.vel[1] = 0; c.w[2] = 3.f; float k = 10.f * c.p.mass * c.invInertia[5] + c.p.angDamp;
  run(&c, 9, 0, 0, 0, 0); OUT("airstab_k", k); OUT("airstab_ratio", fabsf(c.w[2]) / 3.f / expf(-k * 0.15f));
}
// 5.1 стабилизатор: наклон 40° в воздухе, без вращения — за один кадр угловая скорость = 200·m·(1 − cos 40°)·(1/I) · dt
static void t_airstab_tilt(void) {
  Car c; place(&c, -200.f, -250.f, 0.f); rollBy(&c, 40.f); c.y = 40.f; c.vel[1] = 0; c.w[0] = c.w[1] = c.w[2] = 0;
  float want = 200.f * c.p.mass * (1.f - cosf(40.f / DEG)) * c.invInertia[5] / FPS; step(&c, 0, 0, 0, 0);
  OUT("airstab_tilt_ratio", sqrtf(c.w[0] * c.w[0] + c.w[1] * c.w[1] + c.w[2] * c.w[2]) / want);
}
// 5.1 в воздухе при наклоне до 10° стабилизатора нет: вращение вокруг оси вверх гасит только игрок, 3·m·ω (и общее 0,8/с); с 7·m·ω затухало бы втрое быстрее
static void t_airdamp(void) {
  Car c; start(&c, 10.f); c.y = 40.f; c.vel[1] = 0; c.w[0] = c.w[1] = c.w[2] = 0; c.w[1] = 2.f; float k = 3.f * c.p.mass * c.invInertia[3] + c.p.angDamp;
  run(&c, 9, 0, 0, 0, 0); OUT("airdamp_ratio", fabsf(c.w[1]) / 2.f / expf(-k * 0.15f));
}
// 4.2 неваляшка по диагонали: крен 35° с левыми колёсами на земле — колёса одного борта оторваны, диагональной пары нет, момента нет. Снимок угла через 0,3 с (первых принципов для амплитуды нет)
static void t_weeble_diag(void) { Car c; start(&c, 0.f); rollBy(&c, 35.f); run(&c, 18, 0, 0, 0, 0); OUT("weeble_roll_deg", fabsf(c.roll) * DEG); }
// поверхность под колёсами для проверок: тип задаёт тест (настоящая svSurfaceAt живёт в surface.c окна g12)
extern int gSurf;   // tests/g01cars/surf.c
// боковое замедление: на 27 м/с (доля 0,81 > 0,7, 97 км/ч > 90) с боковой скоростью 4 м/с, за 0,1 с, м/с²; поверхность surf, газ thr, руль st
static float latDecelV(int surf, float thr, float st, float v) {
  gSurf = surf; Car c; start(&c, v); c.vel[0] = 4.f; float v0 = c.vel[0]; run(&c, 40, thr, 0, st, 0);   // руль выдерживаем 0,67 с, чтобы входной руль стал > 0,1
  carSetSpeed(&c, v); c.vel[0] = 4.f; c.w[1] = 0; v0 = c.vel[0]; run(&c, 6, thr, 0, st, 0); gSurf = 0; return (v0 - c.vel[0]) / 0.1f;
}
static float latDecel(int surf, float thr, float st) { return latDecelV(surf, thr, st, 27.f); }
// 3.3 трава держит боковую силу на 0,7 от дороги
static void t_surface_grip(void) { float r = latDecel(0, 0.f, 0.f); float g = latDecel(1, 0.f, 0.f); OUT("surface_grass_ratio", g / r); }
// 3.2 игрок на скорости > 90 км/ч с газом и рулём > 0,1: сцепление ведущих (задних) ×(1 − 0,2·газ)
static void t_gripLoss(void) {   // 24 м/с = 86 км/ч (правила нет) против 26 м/с = 94 км/ч (есть): боковое замедление ∝ 1/скорость, поправка на неё в ожидании
  float lo = latDecelV(0, 1.f, 0.12f, 24.f), hi = latDecelV(0, 1.f, 0.12f, 26.f); (void)lo; OUT("griploss_hi", hi);   // снимок: боковое замедление на 94 км/ч с газом и рулём > 0,1 (чистой формулы нет — поворот корпуса подмешивает тягу в боковую скорость)
}
// 3.6 пончик: НЕ проверяется числом — вклад момента (Donut·m·(1 − доля)) тонет в силах шин: на стоянке угловое ускорение 1,6× формулы и на старом правиле тоже (1,56×), тест не может упасть
static void t_donut(void) {
  Car c; start(&c, 0.f); run(&c, 14, 1, 0, 0.7f, 0); float w0 = c.w[1]; run(&c, 6, 1, 0, 0.7f, 0); float a = fabsf(c.w[1] - w0) / 0.1f;
  OUT("donut_ratio", a / (c.p.donutTorque * c.p.mass * c.invInertia[3]));
}
// 4.3 машина на боку (крен 88°), стоит, руль 1: момент крена 130·m·руль
static void t_flipsteer(void) {
  Car c; start(&c, 0.f); rollBy(&c, 88.f); float mx = 0; for (int i = 0; i < 20; i++) { step(&c, 0, 0, 1, 0); if (c.stabRoll > 0.f && fabsf(c.steer) > .05f) { float r = c.stabRoll / (130.f * c.p.mass * fabsf(c.steer)); if (r > mx) mx = r; } }
  OUT("flipsteer_ratio", mx);
}
// 2.9 покой на склоне 15°: без газа, тормоза и ручника машина на трамплине не скатывается (за 4 с, м)
static void t_rest_slope(void) {
  Car c; carParamsFor("famil_v", &P0); place(&c, 150.f, 105.f, 0.f);
  float x0 = c.x, z0 = c.z; run(&c, 5 * FPS, 0, 0, 0, 0); OUT("rest_slope_move", sqrtf((c.x - x0) * (c.x - x0) + (c.z - z0) * (c.z - z0)));
}
// 7.2 кто ударил: A носом ОТ соседки, но едет боком на неё со скоростью 8 м/с — по скорости она ударившая, урон ×0,5 против урона стоящей B
static void t_hitter(void) {
  Car A, B; carParamsFor("famil_v", &P0); place(&B, -200.f, -200.f, 0.f); place(&A, -206.5f, -200.f, -PI / 2);   // нос A смотрит в −x, от B
  run(&A, 20, 0, 0, 0, 0); run(&B, 20, 0, 0, 0, 0); A.damage = B.damage = 0; A.vel[0] = 8.f;
  for (int i = 0; i < 30; i++) { carUpdate(&A, &W, &(CarInput){0}, 1.f / FPS); carHitCar(&A, &B, &W); }
  OUT("hitter_dmgA", A.damage); OUT("hitter_dmgB", B.damage); OUT("hitter_ratio", B.damage > 1e-5f ? A.damage / B.damage : -1.f);
}
// P01-18 автосброс: машина вверх колёсами и медленно вертится (вращение 1 рад/с, больше порога покоя 0,3), скорость < 1 м/с — по правилу «ось вверх < 0,2 со знаком дольше 1 с» ставится на колёса; считаем постановки за 2,5 с
static void t_autoreset(void) { Car c; start(&c, 0.f); rollBy(&c, 180.f); c.y += 0.8f; c.w[1] = 1.0f; int f1 = -1; for (int i = 0; i < 150; i++) { step(&c, 0, 0, 0, 0); if (f1 < 0 && c.rights > 0) f1 = i; } OUT("autoreset_first_s", f1 < 0 ? 9.f : f1 / (float)FPS); }
// круг 7, 7.3: прочность +1 только у машин списка оригинала (atv_v в списке нет, HitPoints 0,5): урон стене на 60 км/ч = D / HitPoints (а не D / (HitPoints + 1))
static void t_beef(void) {
  Car c; place(&c, 0.f, 300.f - 12.f, 0.f); c.p.hitPoints = 0.5f; strcpy(c.p.name, "dune_v");   // физика famil_v, имя вне списка, HitPoints 0,5
  carSetSpeed(&c, 16.7f); float hs = 0;
  for (int i = 0; i < 60; i++) { step(&c, 0, 0, 0, 0); if (c.hitSpeed > hs) hs = c.hitSpeed; }
  float D = c.p.bodyMass * hs * 1.2f / 100000.f; OUT("beef_ratio", c.damage * (c.p.hitPoints > .1f ? c.p.hitPoints : .1f) / D);
}
// круг 7, P01-14: взрыв рядом отнимает 0,5 HitPoints и у машины игрока: famil_v (3) — 0,167
static void t_boomplayer(void) {
  Car c; place(&c, -200.f, -250.f, 0.f); SvBoom b; memset(&b, 0, sizeof b); b.x = -200.f; b.y = 0.f; b.z = -240.f; b.radius = 20.f; b.force = 20000.f;
  carBoomHit(&c, &b, 1); OUT("boom_player_damage", c.damage);
}
// круг 7, P01-15: прерывание решения по импульсу > 0,8 действует и на остов huska: удар на 100 м/с массой 1500 (импульс 0,84 > 0,8) — скорость не меняется
static void t_huskabort(void) {
  Car A, B; place(&B, -200.f, -200.f, 0.f); carParamsFor("huska", &P0); carInit(&A, &P0, &W, -203.f, 0.f, -200.f, PI / 2); carParamsFor("famil_v", &P0);
  run(&A, 20, 0, 0, 0, 0); run(&B, 20, 0, 0, 0, 0); A.p.mass = A.p.bodyMass = 1500.f; A.vel[0] = 100.f; carHitCar(&A, &B, &W); OUT("huskabort_vel", A.vel[0]);
}
// 5.3 предел скорости 200 м/с
static void t_speedcap(void) { Car c; start(&c, 0.f); c.vel[1] = 250.f; run(&c, 1, 0, 0, 0, 0); OUT("speedcap", spd(&c)); }
// 7.1/7.4 лобовой удар в стену на 100 км/ч: не добит за один удар (0,999), второй добивает
static void t_noonehit(void) {
  Car c; place(&c, 0.f, 300.f - 20.f, 0.f); c.damage = 0.9f; carSetSpeed(&c, 27.8f); run(&c, 90, 0, 0, 0, 0);
  OUT("noonehit_destroyed", c.destroyed); OUT("noonehit_damage", c.damage);
}
// 7.1 урон на 60 км/ч в стену = нормированный импульс m·v·(1 + 0,2) / 100000, делённый на прочность
static void t_damage(void) {
  Car c; place(&c, 0.f, 300.f - 12.f, 0.f); carSetSpeed(&c, 16.7f); float hs = 0; for (int i = 0; i < 60; i++) { step(&c, 0, 0, 0, 0); if (c.hitSpeed > hs) hs = c.hitSpeed; } OUT("damage_hitspeed", hs); OUT("damage_hp", c.p.hitPoints); OUT("damage_val", c.damage);
  OUT("damage_ratio", c.damage / (c.p.bodyMass * hs * 1.2f / 100000.f / (c.p.hitPoints > 1.f ? c.p.hitPoints : 1.f)));
}
// P01-02 силы шин у оторванных колёс: крен 35°, полный газ 0,3 с — скорость вперёд
static void t_airwheel(void) { Car c; start(&c, 0.f); rollBy(&c, 35.f); run(&c, 18, 1, 0, 0, 0); OUT("airwheel_speed", fwdSpd(&c)); }
// 5.2 + P01-07 зона прыжка: ящик в (-200, 1, -100) 20 × 6 × 30 м, курс зоны +z или −z (файл JZD1)
static ColZone Z;
static void zoneSetup(float dirz) {
  memset(&Z, 0, sizeof Z); Z.c[0] = -200; Z.c[1] = 1; Z.c[2] = -100; Z.ax[0] = 1; Z.ay[1] = 1; Z.az[2] = 1; Z.h[0] = 10; Z.h[1] = 3; Z.h[2] = 15; Z.shape = 1; strcpy(Z.name, "tzone");
  W.zone = &Z; W.nzone = 1;
  unsigned char b[48]; memset(b, 0, sizeof b); memcpy(b, "JZD1", 4); uint32_t v[3] = {1, 1, 0}; memcpy(b + 4, v, 12);
  strcpy((char *)b + 16, "tzone"); float d[2] = {0.f, dirz}; memcpy(b + 32, d, 8); carJumpDirLoad(b, 48);
}
static void t_zone_boost(void) {   // ускорение внутри зоны на 15 м/с: тяга ×1,49 без сопротивления и качения (≈13,4 м/с²)
  zoneSetup(1.f); Car c; place(&c, -200.f, -108.f, 0.f); run(&c, 30, 0, 0, 0, 0); carSetSpeed(&c, 15.f); float v0 = fwdSpd(&c);
  run(&c, 12, 1, 0, 0, 0); OUT("zone_accel", (fwdSpd(&c) - v0) / 0.2f);
}
static void t_zone_dir(void) {     // въезд против курса зоны: разгона нет (подшагов разгона 0); по курсу — есть
  zoneSetup(-1.f); Car c; place(&c, -200.f, -125.f, 0.f); run(&c, 30, 0, 0, 0, 0); carSetSpeed(&c, 15.f);
  for (int i = 0; i < 60; i++) step(&c, 1, 0, 0, 0);
  OUT("zone_against_boosts", c.boosts);
  zoneSetup(1.f); place(&c, -200.f, -125.f, 0.f); run(&c, 30, 0, 0, 0, 0); carSetSpeed(&c, 15.f);
  for (int i = 0; i < 180; i++) step(&c, 1, 0, 0, 0);
  OUT("zone_along_boosts", c.boosts > 0); OUT("zone_along_bigair", c.bigAirSeq);
}

typedef struct { const char *name; void (*fn)(void); } Test;
static const Test TESTS[] = {
  {"steer_top", t_steer_top}, {"steer_low", t_steer_low}, {"steer_rate", t_steer_rate}, {"hblatch", t_hblatch}, {"brake_limit", t_brake_limit},
  {"brake_force", t_brake_force}, {"coast", t_coast}, {"hb_throttle", t_hb_throttle}, {"burst_cap", t_burst_cap}, {"burst_cancel", t_burst_cancel},
  {"reverse", t_reverse}, {"topspeed", t_topspeed}, {"jturn_drop", t_jturn_drop}, {"jturn_slow", t_jturn_slow}, {"airstab", t_airstab}, {"airstab_tilt", t_airstab_tilt}, {"airdamp", t_airdamp}, {"surface_grip", t_surface_grip}, {"hitter", t_hitter}, {"autoreset", t_autoreset}, {"beef", t_beef}, {"boomplayer", t_boomplayer}, {"huskabort", t_huskabort}, {"gripLoss", t_gripLoss}, {"donut", t_donut}, {"flipsteer", t_flipsteer}, {"rest_slope", t_rest_slope}, {"weeble_diag", t_weeble_diag}, {"speedcap", t_speedcap},
  {"noonehit", t_noonehit}, {"damage", t_damage}, {"airwheel", t_airwheel}, {"zone_boost", t_zone_boost}, {"zone_dir", t_zone_dir},
};
int main(int argc, char **argv) {
  carParamsFor("famil_v", &P0); buildWorld();
  for (unsigned i = 0; i < sizeof TESTS / sizeof *TESTS; i++) if (argc < 2 || !strcmp(argv[1], TESTS[i].name)) TESTS[i].fn();
  return 0;
}
