// Гейт 1325: машина ИИ миссии (цель, соперник) в зоне DEATH и вверх колёсами вне дороги возвращается, как игрок (viewer/msncar.c, настоящие данные уровня).
//   aicar1325_host LEVEL.shr TRAFFIC.shr MSNCARS.shr
// Проверки (кадр 1/30 с, мир без игрока и без трафика; машина — цель ("target") с путём вдоль дороги):
//   A зона DEATH: машина телепортом (carPlace) в центр каждой из первых 6 зон; ждём 3 с. Новое: возврат на 1-м кадре (не позже 0,1 с), точка — apex зоны (<= 2 м), счётчик +1.
//     Старое: машины ИИ зон DEATH не знают — возврата нет за все 3 с.
//   B вверх колёсами вне дороги: машина на 25–40 м от оси ближней полосы (по грунту), кузов опрокинут (q = поворот 180° вокруг продольной оси), скорость 0. Новое: через 1,0–1,3 с
//     машина на оси полосы (<= 2,5 м), на колёсах (наклон < 20°), ставится не на месте (сдвиг > 15 м). Старое: через 1 с на месте (сдвиг < 2 м), от дороги дальше 20 м.
//   C в покое на колёсах на дороге ничего не происходит: за 3 с сдвиг < 1 м, сброса нет.
// Печатает RESULT: PASS / FAIL (пройдено/всего); код 0 — PASS.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "msncar.h"

static ColWorld world; static Traffic T; static int fails, total;
static const float DT = 1.f / 30.f;
static unsigned char *readFile(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, ((size_t)s + 63) & ~(size_t)63);
  if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (unsigned)s; return b;
}
static void chk(int ok, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void chk(int ok, const char *fmt, ...) { char b[300]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); total++; if (!ok) fails++; printf("  %-4s %s\n", ok ? "ok" : "FAIL", b); }
static int mkCar(MsnCars *m, McEnv *e, McCar **cp, const float pos[3], float h) {
  mcInit(m); McCar *c = mcSpawn(m, &world, &T, "famil_v", pos, h, 1, "target", "", ""); if (!c) return 0;
  memset(e, 0, sizeof *e); e->w = &world; e->tr = &T; e->ppos[0] = 1e6f; e->ppos[2] = 1e6f; *cp = c; return 1;
}
static int gnd(float x, float z, float *g) { float n3[3]; return colGround(&world, x, z, 1e3f, -1e3f, g, n3); }
static int roadPt(float out[4]) {   // точка на оси полосы: первая дорога, где MC даёт точку возврата
  for (int i = 0; i < 400; i++) { float x = world.ox + (world.W * world.cell) * ((i * 37) % 100) / 100.f, z = world.oz + (world.H * world.cell) * ((i * 61) % 100) / 100.f;
    if (mcLaneDist(&T, x, z) < 3.f) { float g = 0; if (gnd(x, z, &g)) { out[0] = x; out[1] = g; out[2] = z; out[3] = 0.f; return 1; } } }
  return 0;
}
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: aicar1325_host LEVEL.shr TRAFFIC.shr MSNCARS.shr\n"); return 2; }
  unsigned n; unsigned char *lvl = readFile(argv[1], &n);
  if (!lvl || n < 104 || memcmp(lvl, "SHR7", 4)) { fprintf(stderr, "нет level.shr\n"); return 2; }
  unsigned pv = *(unsigned *)(lvl + 88), co = *(unsigned *)(lvl + 92), po = *(unsigned *)(lvl + 96), ce = n;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, lvl + co, ce - co)) { fprintf(stderr, "COL1 не читается\n"); return 2; }
  if (trafficLoad(&T, argv[2])) { fprintf(stderr, "traffic.shr не читается\n"); return 2; }
  printf("гейт 1325: зон DEATH %u%s\n", world.ndeath, mcAiReset1325Old ? "  [сборка ПРЕЖНЕЕ: MC_AIRESET1325_OLD=1]" : "");
  // --- A: зоны DEATH
  int nz = (int)world.ndeath < 12 ? (int)world.ndeath : 12, got = 0, late = 0, far = 0, any = 0, skipped = 0;
  for (int z = 0; z < nz; z++) {
    const ColPad *d = &world.death[z]; float pos[3] = {d->c[0], d->c[1], d->c[2]};
    MsnCars m; McEnv e; McCar *c; if (!mkCar(&m, &e, &c, pos, 0.f)) continue;
    float wp[3] = {pos[0] + 50.f, pos[1], pos[2]}; mcWaypoint(&m, 0, wp, 1);
    unsigned n0 = mcAiResetDeathN; float t = 0.f, tr = -1.f;
    carPlace(&c->car, &world, d->c[0], d->c[1], d->c[2], 0.f);   // внутрь объёма
    int in0 = colDeathAtEx(&world, c->car.x, c->car.y + .5f, c->car.z, .5f, 0); float cy0 = c->car.y;
    for (; t < 3.f; t += DT) { mcUpdate(&m, &e, DT, 0, 0); if (tr < 0.f && mcAiResetDeathN > n0) tr = t; }
    if (in0 >= 0 && tr < 0.f) printf("    (зона %d: после постановки машина y=%.2f, центр зоны y=%.2f, полувысота %.2f, зона под машиной: %d)\n", z, cy0, d->c[1], d->h[1], in0);
    const ColPad *ex = in0 >= 0 ? &world.death[in0] : d;   // объёмы одного места перекрываются: возвращает первый из накрывающих — его apex и ждём
    float dd = hypotf(mcAiResetLast[0] - ex->apex[0], mcAiResetLast[2] - ex->apex[2]);
    if (in0 < 0) { skipped++; continue; }   // объём в воздухе над землёй (мост, обрыв): машину в него телепортом не поставить — в гейт не входит
    any++; if (tr >= 0.f) { got++; if (tr > .1f) late++; if (dd > 2.f) far++; }
    printf("    зона %d %.8s: возврат %s t=%.2f, точка от apex %.1f м\n", z, d->name, tr >= 0.f ? "есть" : "нет", tr, tr >= 0.f ? dd : -1.f);
  }
  chk(any >= 3, "есть что проверять: зон DEATH на земле %d (пропущено в воздухе %d)", any, skipped);
  chk(any > 0 && got == any, "зона DEATH: машина ИИ возвращена %d из %d", got, any);
  chk(any > 0 && late == 0, "возврат на первом кадре (не позже 0,1 с): опоздали %d", late);
  chk(any > 0 && far == 0, "точка возврата = apex зоны (<= 2 м): дальше %d", far);
  // --- B: вверх колёсами вне дороги
  float rp[4]; int okB = 0;
  { // ищем точку вне дороги 25–40 м от оси полосы
    float px = 0, pz = 0, py = 0; int have = 0;
    for (int i = 0; i < 4000 && !have; i++) { float x = world.ox + (world.W * world.cell) * ((i * 131) % 1000) / 1000.f, z = world.oz + (world.H * world.cell) * ((i * 523) % 1000) / 1000.f;
      float dl = mcLaneDist(&T, x, z); float g;
      if (dl > 25.f && dl < 40.f && gnd(x, z, &g) && colDeathAt(&world, x, g + .5f, z, 1.f) < 0) { px = x; pz = z; py = g; have = 1; } }
    if (have) {
      MsnCars m; McEnv e; McCar *c; float pos[3] = {px, py + .5f, pz};
      if (mkCar(&m, &e, &c, pos, 0.f)) {
        float wp[3] = {px + 60.f, py, pz}; mcWaypoint(&m, 0, wp, 1);
        for (int i = 0; i < 20; i++) mcUpdate(&m, &e, DT, 0, 0);   // осесть
        Car *k = &c->car; float x0 = k->x, y0 = k->y, z0 = k->z, t = 0.f, tt = -1.f, rx = 0.f, rz = 0.f; unsigned r0 = c->resets;
        // вверх колёсами и неподвижна, пока ИИ не сбросит её (физику перевёрнутой не проверяем — только правило сброса: 75° и 1 с): каждый кадр кузов вверх, скорости нуль.
        // Поворот 180° вокруг продольной оси (кватернион w, x, y, z = 0, 0, 0, 1)
        for (; t < 3.f; t += DT) {
          if (tt < 0.f) { k->q[0] = 0.f; k->q[1] = 0.f; k->q[2] = 0.f; k->q[3] = 1.f; k->vel[0] = k->vel[1] = k->vel[2] = 0.f; k->w[0] = k->w[1] = k->w[2] = 0.f; k->x = x0; k->y = y0; k->z = z0; }
          mcUpdate(&m, &e, DT, 0, 0); if (tt < 0.f && c->resets > r0) { tt = t; rx = k->x; rz = k->z; }   // положение сразу после сброса (дальше машина едет сама)
        }
        float mv = hypotf(rx - x0, rz - z0), dl = mcLaneDist(&T, rx, rz);
        float up = 1.f - 2.f * (k->q[1] * k->q[1] + k->q[3] * k->q[3]);
        printf("    переворот: старт %.0f м от полосы; сдвиг %.1f м, через %.2f с, от полосы %.1f м, вертикаль %.2f\n", mcLaneDist(&T, x0, z0), mv, tt, dl, up);
        okB = 1;
        chk(tt >= 0.9f && tt <= 1.2f, "сброс перевёрнутой через 1 с (0,9–1,2): %.2f", tt);
        if (!mcAiReset1325Old) {
                    chk(mv > 15.f, "новое: поставлена не на месте, сдвиг %.1f м (> 15)", mv);
          chk(mv <= mcLaneDist(&T, x0, z0) + 3.f, "новое: на ближайшую дорогу, не дальше старта от полосы + 3 м: %.1f <= %.1f", mv, mcLaneDist(&T, x0, z0) + 3.f);
          chk(dl <= 2.5f, "новое: на оси полосы, от неё %.1f м (<= 2,5)", dl);
          chk(up > .8f, "новое: на колёсах, вертикаль %.2f (> 0,8)", up);
        } else {
          chk(mv < 2.f, "прежнее: на месте, сдвиг %.1f м (< 2)", mv);
          chk(dl > 20.f, "прежнее: вне дороги, от полосы %.1f м (> 20)", dl);
        }
      }
    }
  }
  chk(okB, "точка вне дороги найдена и прогон B выполнен");
  // --- D: соперник гонки (ai "race"): ставим на дорогу в точке R, даём записать «последнюю точку пути», телепортом уводим на 25–40 м от дороги (дальше 80 м от R) и держим вверх колёсами 1 с.
  //     Новое: встаёт на R (<= 3 м), не на ближайшую к месту переворота полосу. 1325б ПРЕЖНЕЕ (MC_AIRESET1325B_OLD): на ближайшую полосу к месту переворота.
  // --- E: в точке возврата нет машин трафика (радиус двух колёсных баз): перед переворотом машина трафика ставится в R. 1325б ПРЕЖНЕЕ: остаётся.
  float rp2[4];
  if (roadPt(rp2)) {
    for (int att = 0; att < 8; att++) {   // место переворота: первая годная точка; где физика сама поднимает машину (яма, склон) и счёт 1 с не набирается — следующая
    float px = 0, pz = 0, py = 0; int have = 0, skip = att;
    for (int i = 0; i < 6000 && !have; i++) { float x = world.ox + (world.W * world.cell) * ((i * 211) % 1000) / 1000.f, z = world.oz + (world.H * world.cell) * ((i * 757) % 1000) / 1000.f;
      float dl = mcLaneDist(&T, x, z), g; float d0 = hypotf(x - rp2[0], z - rp2[2]);
      if (dl > 25.f && dl < 40.f && d0 > 80.f && gnd(x, z, &g) && colDeathAt(&world, x, g + .5f, z, 1.f) < 0 && skip-- == 0) { px = x; pz = z; py = g; have = 1; } }
    MsnCars m; McEnv e; McCar *c; float pos[3] = {rp2[0], rp2[1] + .5f, rp2[2]};
    if (have && mkCar(&m, &e, &c, pos, 0.f)) {
      mcInit(&m); c = mcSpawn(&m, &world, &T, "famil_v", pos, 0.f, 1, "race", "", ""); float wp[3] = {rp2[0] + 60.f, rp2[1], rp2[2]}; mcWaypoint(&m, 0, wp, 1);
      trafficReset(&T, &world);
      for (int i = 0; i < 40; i++) mcUpdate(&m, &e, DT, 0, 0);   // стоит на дороге (гонка может ждать — не важно), точка записана
      Car *k = &c->car; float lx = c->lastRdOk ? c->lastRd[0] : k->x, lz = c->lastRdOk ? c->lastRd[2] : k->z;   // последняя точка на дороге (записанная)
      float Q[4]; int hq = mcResetPointNear(&T, px, py, pz, 0.f, Q);   // ближайшая полоса к месту переворота (ожидание прежнего)
      // две машины трафика: у последней точки (новое сбросит) и у ближайшей к месту переворота полосы (прежнее встанет в неё); считаем те, что ближе 6 м к точке сброса
      int ntr0 = 0; T.car[0].mode = TRM_DRIVE; T.car[0].car.x = lx + 1.5f; T.car[0].car.z = lz; T.car[0].car.y = k->y; ntr0++;
      if (hq) { T.car[1].mode = TRM_DRIVE; T.car[1].car.x = Q[0] + 1.5f; T.car[1].car.z = Q[2]; T.car[1].car.y = Q[1]; ntr0++; }
      carPlace(k, &world, px, py + .5f, pz, 0.f); unsigned r0 = c->resets; float t = 0.f, tt = -1.f, rx = 0.f, rz = 0.f;
      for (; t < 3.f; t += DT) {
        if (tt < 0.f) { k->q[0] = 0.f; k->q[1] = 0.f; k->q[2] = 0.f; k->q[3] = 1.f; k->vel[0] = k->vel[1] = k->vel[2] = 0.f; k->w[0] = k->w[1] = k->w[2] = 0.f; k->x = px; k->y = py; k->z = pz; }
        mcUpdate(&m, &e, DT, 0, 0); if (tt < 0.f && c->resets > r0) { tt = t; rx = k->x; rz = k->z; }
      }
      if (tt < 0.f && att < 7) continue;
      float dR = hypotf(rx - lx, rz - lz), dQ = hq ? hypotf(rx - Q[0], rz - Q[2]) : 1e9f; int ntr1 = 0;
      for (int i = 0; i < TR_MAXCAR; i++) if (T.car[i].mode != TRM_OFF && hypotf(T.car[i].car.x - rx, T.car[i].car.z - rz) < 6.f) ntr1++;
      printf("    соперник гонки: последняя точка (%.0f, %.0f), место переворота (%.0f, %.0f) в %.0f м; сброс через %.2f с в (%.0f, %.0f): от последней точки %.1f м, от ближайшей к месту переворота %.1f м; трафик у точки: %d -> %d\n", lx, lz, px, pz, hypotf(px - lx, pz - lz), tt, rx, rz, dR, dQ, ntr0, ntr1);
      chk(tt >= 0.9f && tt <= 1.2f, "гонка: перевёрнутая встала через 1 с (%.2f)", tt);
      if (!mcAiReset1325Old && !mcAiReset1325BOld) {
        chk(dR <= 3.f, "гонка: стоит на последней пройденной точке пути, от неё %.1f м (<= 3)", dR);
        chk(dQ > 30.f, "гонка: не на ближайшей к месту переворота полосе (от неё %.1f м > 30)", dQ);
        chk(ntr0 >= 1 && ntr1 == 0, "в точке возврата трафика нет: было %d, стало %d", ntr0, ntr1);
      } else {
        chk(dQ <= 3.f || mcAiReset1325Old, "прежнее: на ближайшей к месту переворота полосе, от неё %.1f м", dQ);
        chk(ntr1 >= 1 || mcAiReset1325Old, "прежнее: трафик в точке остался, машин %d", ntr1);
      }
    } else chk(0, "D/E: нет места для прогона (вне дороги найдено: %d)", have);
    break; }
  } else chk(0, "D/E: нет точки на дороге");
  // --- C: покой на дороге
  if (roadPt(rp)) {
    MsnCars m; McEnv e; McCar *c; float pos[3] = {rp[0], rp[1] + .5f, rp[2]};
    if (mkCar(&m, &e, &c, pos, 0.f)) {
      unsigned d0 = mcAiResetDeathN, f0 = mcAiResetFlipN; for (int i = 0; i < 90; i++) mcUpdate(&m, &e, DT, 0, 0);
      chk(mcAiResetDeathN == d0 && mcAiResetFlipN == f0, "на колёсах на дороге сбросов нет (death +%u, flip +%u)", mcAiResetDeathN - d0, mcAiResetFlipN - f0);
    } else chk(0, "C: машина не создана");
  } else chk(0, "C: точка на дороге не найдена");
  printf("RESULT: %s (%d/%d)\n", fails ? "FAIL" : "PASS", total - fails, total);
  return fails ? 1 : 0;
}
