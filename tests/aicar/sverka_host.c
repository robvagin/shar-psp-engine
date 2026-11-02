// Сверка с исходником, окно g03-aicars (02.11.2026): проверки ИИ машин миссий на ровной площадке с одной стеной (CPU Мака, без эмулятора).
// Зелёный — сборка по умолчанию; красный — та же сборка с -DMC_SVERKA_OLD=1 -DPOLICE_STAND27_OLD=1 (прежнее поведение): каждая проверка
// обязана упасть. Что проверяется: желаемая скорость по типу этапа (P04 A11 Д3 Д4), полиция всегда едет без остановки в 27 м (2.10),
// застревание 0,7 с (A6), ожидание игрока дальше 300 м (A10), объезд 3 м × cos (A3).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "car.h"
#include "msncar.h"
static int fails;
static void check(const char *n, int ok, const char *d) { printf("CHECK %-6s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static struct { ColHdr h; ColTri t[4]; unsigned start[2], refs[4]; } fx;
static ColWorld W;
static const float DT = 1.f / 60.f;
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static void world(void) {
  memcpy(fx.h.magic, "COL1", 4); fx.h.ox = fx.h.oz = -800; fx.h.cell = 1600; fx.h.W = fx.h.H = 1; fx.h.ntri = 4; fx.h.nref = 4;
  fx.t[0] = (ColTri){{-800, 0, -800}, {800, 0, 800}, {800, 0, -800}, {0, 1, 0}};
  fx.t[1] = (ColTri){{-800, 0, -800}, {-800, 0, 800}, {800, 0, 800}, {0, 1, 0}};
  fx.t[2] = (ColTri){{-800, 0, 400}, {800, 0, 400}, {800, 10, 400}, {0, 0, -1}};   // стена поперёк, z = 400
  fx.t[3] = (ColTri){{-800, 0, 400}, {800, 10, 400}, {-800, 10, 400}, {0, 0, -1}};
  fx.start[1] = 4; fx.refs[0] = 0; fx.refs[1] = 1; fx.refs[2] = 2; fx.refs[3] = 3;
}
static void env(McEnv *e, Car *pl, int driving, float px, float pz) {
  memset(e, 0, sizeof *e); e->w = &W; e->player = pl; e->driving = driving; e->ppos[0] = px; e->ppos[2] = pz;
}
static McCar *mk(MsnCars *M, const char *ai, float x, float z, float h) {
  mcInit(M); float p[3] = {x, 0.f, z};
  McCar *c = mcSpawn(M, &W, 0, "smith_v", p, h, 1, ai, "Missions\\level01\\M7race.con", "x");
  float w[3] = {0.f, 0.f, 600.f}; mcWaypoint(M, 0, w, 1);
  return c;
}
// c->vdes после первого пересчёта: игрок за рулём на (px, pz) со скоростью pv
static float vdesAt(const char *ai, int driving, float px, float pz, float pv, float *top, float *ptop) {
  static MsnCars M; McCar *c = mk(&M, ai, 0.f, 0.f, 0.f); Car pl; carInit(&pl, 0, &W, px, 0.f, pz, 0.f); pl.v = pv;
  McEnv e; env(&e, &pl, driving, px, pz);
  float v0 = c->vdes; mcUpdate(&M, &e, DT, 0, 0);
  if (fabsf(v0 - .7f * c->baseTop) > .01f) printf("  (первые 2 с желаемая %.2f, ожидалось 0,7 × TopSpeed)\n", v0);
  for (int f = 0; f < 125; f++) { c->car.x = 0.f; c->car.z = 0.f; mcUpdate(&M, &e, DT, 0, 0); }   // первые 2 с — 0,7 своей, потом считается по типу этапа
  *top = c->baseTop; *ptop = pl.p.topSpeed; return c->vdes;
}
int main(void) {
  world();
  if (colInit(&W, &fx, sizeof fx)) { printf("FAIL: мир\n"); return 1; }
  char d[260]; float top, ptop, v;
  // ---- желаемая скорость
  v = vdesAt("race", 1, 12.f, 0.f, 20.f, &top, &ptop);
  snprintf(d, sizeof d, "гонка, игрок ближе 25 м, 20 м/с: желаемая %.2f (1,1 × скорость игрока = 22; зажим %.1f…%.1f)", v, .5f * ptop, 1.7f * ptop);
  check("race25", fabsf(v - clampf(22.f, .5f * ptop, 1.7f * ptop)) < .05f, d);
  v = vdesAt("waypoint", 1, 12.f, 0.f, 20.f, &top, &ptop);
  snprintf(d, sizeof d, "этап без догонялки: желаемая %.2f = 0,7 × TopSpeed %.2f", v, .7f * top);
  check("wp70", fabsf(v - .7f * top) < .05f, d);
  v = vdesAt("race", 0, 12.f, 0.f, 0.f, &top, &ptop);
  snprintf(d, sizeof d, "гонка, игрок пешком: желаемая %.2f = 0,7 × TopSpeed %.2f", v, .7f * top);
  check("foot70", fabsf(v - .7f * top) < .05f, d);
  { float a = vdesAt("target", 1, 10.f, 0.f, 5.f, &top, &ptop), b = vdesAt("target", 1, 0.f, 100.f, 5.f, &top, &ptop);
    snprintf(d, sizeof d, "цель: игрок в 10 м — %.2f (TopSpeed игрока %.2f), в 100 м — %.2f (0,2 × = %.2f)", a, ptop, b, .2f * ptop);
    check("target", fabsf(a - ptop) < .05f && fabsf(b - .2f * ptop) < .05f, d); }
  { float a = vdesAt("evade", 1, 10.f, 0.f, 5.f, &top, &ptop), b = vdesAt("evade", 1, 0.f, 100.f, 5.f, &top, &ptop);
    snprintf(d, sizeof d, "уход: игрок в 10 м — %.2f (свой TopSpeed %.2f), в 100 м — %.2f (0,7 × = %.2f)", a, top, b, .7f * top);
    check("evade", fabsf(a - top) < .05f && fabsf(b - .7f * top) < .05f, d); }
  // ---- полиция: всегда едет, стоп в 27 м от стоящего игрока убран
  {
    static MsnCars M; mcInit(&M); float p[3] = {0.f, 0.f, -150.f};
    McCar *c = mcSpawn(&M, &W, 0, "cPolice", p, 0.f, 1, "chase", "Pursuit\\L1cop.con", ""); c->police = 1;
    Car pl; carInit(&pl, 0, &W, 0.f, 0.f, 0.f, 0.f); CarInput none = {0, 0, 0, 1}; float dmin = 1e9f;
    for (int f = 0; f < 60 * 50; f++) {
      carUpdate(&pl, &W, &none, DT); McEnv e; env(&e, &pl, 1, pl.x, pl.z); e.ppos[1] = pl.y;
      mcUpdate(&M, &e, DT, 0, 0);
      float dx = c->car.x - pl.x, dz = c->car.z - pl.z, dd = sqrtf(dx * dx + dz * dz); if (dd < dmin) dmin = dd;
    }
    snprintf(d, sizeof d, "полиция к стоящему игроку, 50 с: ближе всего %.1f м (в исходнике подъезжает вплотную; прежнее — стоп в 27 м)", dmin);
    check("police", dmin < 12.f, d);
  }
  // ---- застревание: стена на пути; от остановки до начала заднего хода
  {
    static MsnCars M; McCar *c = mk(&M, "race", 0.f, 330.f, 0.f);   // стена z = 400, путь за ней
    Car pl; carInit(&pl, 0, &W, 25.f, 0.f, 330.f, 0.f); float tStop = -1.f, tBack = -1.f, peak = 0.f; int maxN = 0;
    for (int f = 0; f < 60 * 25; f++) {
      McEnv e; env(&e, &pl, 1, 25.f, 330.f); mcUpdate(&M, &e, DT, 0, 0); float t = f * DT, v_ = c->car.v;
      if (v_ > peak) peak = v_;
      if (peak > 5.f && tStop < 0.f && fabsf(v_) <= 1.f && c->car.z > 390.f - 6.f) tStop = t;
      if (tStop >= 0.f && tBack < 0.f && c->stuckN > 0) tBack = t;   // сработало «застряла» (задний ход начинается тут же)
      if (c->stuckN > maxN) maxN = c->stuckN;
      if (getenv("SV_TRACE") && f % 30 == 0) printf("  t %.1f z %.1f v %.2f stuckN %d backT %.2f stkOn %d stopHold %d\n", t, c->car.z, v_, c->stuckN, c->backT, c->stkOn, c->stopHold);
    }
    snprintf(d, sizeof d, "у стены: остановка на %.2f с, «застряла» сработало на %.2f с — через %.2f с (не позже 0,7 с, на повторном замедлении сразу; прежнее 1,5 с)", tStop, tBack, tBack - tStop);
    check("stuck", tStop > 0.f && tBack > 0.f && tBack - tStop < .9f, d);
  }
  // переворот (P04 Д2): после слияния кругов car.c (g01, svFlipNow) сам ставит чужие машины на колёса за секунду — проверка там (tests g01); у msncar.c страховка 75°/1 с остаётся
  // ---- ожидание игрока дальше 300 м
  {
    static MsnCars M; McCar *c = mk(&M, "race", 0.f, 0.f, 0.f); Car pl; carInit(&pl, 0, &W, 0.f, 0.f, -380.f, 0.f);
    float run = 0.f;
    int evts = 0;
    for (int f = 0; f < 60 * 45; f++) { McEnv e; env(&e, &pl, 1, 0.f, -380.f); mcUpdate(&M, &e, DT, 0, 0); run = fmaxf(run, hypotf(c->car.x, c->car.z)); evts = c->waitEvt; }
    snprintf(d, sizeof d, "игрок в 380 м позади: за 45 с отъехала %.1f м, событий ожидания %d (стоит; событие каждые 20 с, счётчик растёт: за 45 с не меньше двух)", run, evts);
    check("wait", run < 15.f && evts >= 2, d);
  }
  // ---- объезд: стоящая машина в 30 м прямо по курсу — сдвиг точки руля ~3 м без сглаживания
  {
    static MsnCars M; McCar *c = mk(&M, "race", 0.f, 0.f, 0.f); float q[3] = {.4f, 0.f, 30.f};
    mcSpawn(&M, &W, 0, "park", q, 0.f, 1, "NULL", "Missions\\level01\\M7race.con", "x");
    Car pl; carInit(&pl, 0, &W, 300.f, 0.f, 0.f, 0.f); float sh = 0.f;
    for (int f = 0; f < 10; f++) { McEnv e; env(&e, &pl, 1, 300.f, 0.f); mcUpdate(&M, &e, DT, 0, 0); sh = c->shiftF; }
    snprintf(d, sizeof d, "стоящая машина в 30 м по курсу: сдвиг точки руля %.2f м (3 м × cos ≈ 3)", sh);
    check("avoid", fabsf(sh) > 2.5f, d);
  }
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
