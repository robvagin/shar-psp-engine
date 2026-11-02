// Сверка, круг 6, окно g03-aicars: тест на каждую строку (CPU Мака, без эмулятора и без данных диска). Синтетические дороги и ровная площадка.
// Красный — та же сборка с -DMC_SVERKA_OLD=1 -DPOLICE_STAND27_OLD=1: проверки обязаны падать (tests/aicar/sverka6.sh).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "car.h"
#include "msncar.h"
static int fails, nchecks;
static void check(const char *n, int ok, const char *d) { nchecks++; printf("CHECK %-8s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static struct { ColHdr h; ColTri t[4]; unsigned start[2], refs[4]; } fx;
static ColWorld W;
static const float DT = 1.f / 60.f;
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static void world(void) {
  memcpy(fx.h.magic, "COL1", 4); fx.h.ox = fx.h.oz = -800; fx.h.cell = 1600; fx.h.W = fx.h.H = 1; fx.h.ntri = 4; fx.h.nref = 4;
  fx.t[0] = (ColTri){{-800, 0, -800}, {800, 0, 800}, {800, 0, -800}, {0, 1, 0}};
  fx.t[1] = (ColTri){{-800, 0, -800}, {-800, 0, 800}, {800, 0, 800}, {0, 1, 0}};
  fx.t[2] = (ColTri){{-800, 0, 400}, {800, 0, 400}, {800, 10, 400}, {0, 0, -1}};
  fx.t[3] = (ColTri){{-800, 0, 400}, {800, 10, 400}, {-800, 10, 400}, {0, 0, -1}};
  fx.start[1] = 4; fx.refs[0] = 0; fx.refs[1] = 1; fx.refs[2] = 2; fx.refs[3] = 3;
}
static void env(McEnv *e, Car *pl, int driving, float px, float pz) { memset(e, 0, sizeof *e); e->w = &W; e->player = pl; e->driving = driving; e->ppos[0] = px; e->ppos[2] = pz; }
static const char *CON = "Missions\\level01\\M7race.con";
static McCar *mk(MsnCars *M, const char *ai, float x, float z, float h) {
  mcInit(M); float p[3] = {x, 0.f, z};
  McCar *c = mcSpawn(M, &W, 0, "smith_v", p, h, 1, ai, CON, "x");
  float w[3] = {0.f, 0.f, 600.f}; mcWaypoint(M, 0, w, 1);
  return c;
}
// ---------------------------------------------------------------- синтетические дороги TRF1
static TrHeader NH[6]; static TrJunc NJ[6][8]; static TrRoad NR[6][8]; static TrPt NP[6][512]; static Traffic NT[6]; static int nRoads[6], nPts[6];
static void netBegin(int n) { memset(&NT[n], 0, sizeof NT[n]); memset(&NH[n], 0, sizeof NH[n]); nRoads[n] = nPts[n] = 0; NH[n].nJunc = 8; NT[n].hd = &NH[n]; NT[n].junc = NJ[n]; NT[n].road = NR[n]; NT[n].pt = NP[n]; }
// lanes полос с шагом gap по x; точки базовой полосы p[0..n); length — длина для цены (может не совпадать с геометрией)
static int netRoad(int n, int sj, int ej, int lanes, float gap, int flags, int pad8, float length, int np, const float (*p)[3]) {
  TrRoad *R = &NR[n][nRoads[n]]; memset(R, 0, sizeof *R);
  R->startJ = (uint16_t)sj; R->endJ = (uint16_t)ej; R->rev = 0xFFFF; R->lanes = (uint16_t)lanes; R->flags = (uint8_t)flags; R->pad8 = (uint8_t)pad8; R->maxCars = 4; R->speed = 50;
  R->firstPt = (uint32_t)nPts[n]; R->nPt = (uint16_t)np; R->length = length; R->width = 8.f;
  for (int l = 0; l < lanes; l++) { float s = 0.f; for (int i = 0; i < np; i++) {
    if (i) s += hypotf(p[i][0] - p[i - 1][0], p[i][2] - p[i - 1][2]);
    TrPt *q = &NP[n][nPts[n]++]; q->x = p[i][0] + ((float)l - (float)(lanes - 1) * .5f) * gap; q->y = p[i][1]; q->z = p[i][2]; q->s = s; } }
  NH[n].nRoad = (uint32_t)++nRoads[n]; return nRoads[n] - 1;
}
// машина "waypoint"/mode на дороге сети n; цель g; один кадр — маршрут в c->route
static McCar *planOn(MsnCars *M, int net, const char *ai, float x, float z, float h, const float g[3], Car *pl, McEnv *e) {
  mcInit(M); float p[3] = {x, 0.f, z};
  McCar *c = mcSpawn(M, &W, &NT[net], "smith_v", p, h, 1, ai, CON, "x");
  mcWaypoint(M, 0, g, 1);
  env(e, pl, 1, x + 20.f, z); e->tr = &NT[net];
  mcUpdate(M, e, DT, 0, 0);
  return c;
}
static int routeHas(const McCar *c, float x, float z, float tolx, float tolz, int from) {
  for (int i = from; i < c->nroute; i++) if (fabsf(c->route[i][0] - x) < tolx && fabsf(c->route[i][2] - z) < tolz) return 1;
  return 0;
}
// ---------------------------------------------------------------- машина с готовым маршрутом (без перепланирования)
static McCar *withRoute(MsnCars *M, const char *ai, int n, const float (*r)[3], float v) {
  McCar *c = mk(M, ai, 0.f, 0.f, 0.f);
  for (int i = 0; i < n; i++) memcpy(c->route[i], r[i], 12);
  c->nroute = n; c->routeWp = c->wp; c->routeCut = 0; c->seg = 0; c->replanT = 5.f; carSetSpeed(&c->car, v);
  return c;
}
static void touchCount(void *u, Car *c) { (void)c; (*(int *)u)++; }
static int bonusHook(void *u, const McCar *c, const float g[3], float (*out)[3], int cap) { (void)u; (void)c; (void)g; (void)out; (void)cap; return 0; }
static int lineHook(void *u, const McCar *c, const float g[3], float (*out)[3], int cap) { (void)u; (void)c; (void)g; (void)cap; out[0][0] = 0; out[0][1] = 0; out[0][2] = 0; out[1][0] = g[0]; out[1][1] = g[1]; out[1][2] = g[2]; return 2; }
static void step1(MsnCars *M, Car *pl, McCar *c, float px, float pz) { McEnv e; env(&e, pl, 1, px, pz); (void)c; mcUpdate(M, &e, DT, 0, 0); }

static void bench(void) {
  static MsnCars B; mcInit(&B); Car pb; carInit(&pb, 0, &W, 25.f, 0.f, 0.f, 0.f); static Car trc[8]; netBegin(5); NT[5].maxCars = 8;
  for (int i = 0; i < 8; i++) { carInit(&NT[5].car[i].car, 0, &W, 10.f * (float)i - 30.f, 0.f, 40.f + 7.f * (float)i, 0.f); NT[5].car[i].mode = TRM_DRIVE; } (void)trc;
  for (int i = 0; i < 6; i++) { char nm[16]; snprintf(nm, sizeof nm, "c%d", i); mcSpawn(&B, &W, 0, nm, (float[3]){(float)(i * 6), 0.f, 0.f}, 0.f, 1, i < 3 ? "race" : "chase", CON, "x"); }
  float w[3] = {0.f, 0.f, 380.f}; mcWaypoint(&B, 0, w, 1);
  struct timespec a, b; double tot = 0.0; int n = 0;
  for (int f = 0; f < 1200; f++) { McEnv e; env(&e, &pb, 1, 25.f, 0.f); e.tr = &NT[5]; clock_gettime(CLOCK_MONOTONIC, &a); mcUpdate(&B, &e, DT, 0, 0); clock_gettime(CLOCK_MONOTONIC, &b);
    tot += (double)(b.tv_sec - a.tv_sec) * 1e6 + (double)(b.tv_nsec - a.tv_nsec) / 1e3; n++; }
  printf("BENCH 6 машин миссии + 8 машин трафика: %.1f мкс на mcUpdate (Мак; на PSP примерно ×5,8)\n", tot / n);
}
int main(int argc, char **argv) {
  world(); if (colInit(&W, &fx, sizeof fx)) { printf("FAIL: мир\n"); return 1; }
  if (argc > 1 && !strcmp(argv[1], "-b")) { bench(); return 0; }
  char d[300]; static MsnCars M; static Car pl; carInit(&pl, 0, &W, 25.f, 0.f, 0.f, 0.f);

  int chordOk = 0, backOk = 0;
  // ================= P04:A1 путь по середине дороги, хорда 40 %, P16-05 множитель 1,30
  { netBegin(0); const float A[3][3] = {{0, 0, 0}, {0, 0, 100}, {0, 0, 200}};
    int ra = netRoad(0, 0, 1, 2, 4.f, 0, 0, 200.f, 3, A); (void)ra;
    float g[3] = {0.f, 0.f, 180.f}; McEnv e; McCar *c = planOn(&M, 0, "waypoint", 2.f, 10.f, 0.f, g, &pl, &e);
    int mid = c->nroute > 1; for (int i = 1; i < c->nroute; i++) if (fabsf(c->route[i][0]) > .5f) mid = 0;
    snprintf(d, sizeof d, "двухполосная дорога (полосы ±2 м): точки пути на оси x = 0 (точек %d)", c->nroute); check("A1mid", mid, d); }
  { netBegin(1); const float A[3][3] = {{0, 0, 0}, {0, 0, 50}, {0, 0, 100}}, B[3][3] = {{4, 0, 110}, {4, 0, 160}, {4, 0, 210}};
    netRoad(1, 0, 1, 1, 0.f, 0, 0, 100.f, 3, A); netRoad(1, 1, 2, 1, 0.f, 0, 0, 100.f, 3, B);
    float g[3] = {4.f, 0.f, 200.f}; McEnv e; McCar *c = planOn(&M, 1, "waypoint", 0.f, 10.f, 0.f, g, &pl, &e);
    int chord = routeHas(c, 0.f, 104.3f, 1.f, 2.5f, 1);
    snprintf(d, sizeof d, "проезд прямо через перекрёсток (выезд в 10,8 м): есть точка хорды у (0; 104,3), точек %d", c->nroute); chordOk = chord; }
  { netBegin(2); const float A[3][3] = {{0, 0, 0}, {0, 0, 50}, {0, 0, 100}}, B[3][3] = {{10, 0, 100}, {60, 0, 100}, {110, 0, 100}};
    netRoad(2, 0, 1, 1, 0.f, 0, 0, 100.f, 3, A); netRoad(2, 1, 2, 1, 0.f, 0, 0, 100.f, 3, B);
    float g[3] = {100.f, 0.f, 100.f}; McEnv e; McCar *c = planOn(&M, 2, "waypoint", 0.f, 10.f, 0.f, g, &pl, &e);
    int none = !routeHas(c, 0.f, 104.f, 1.f, 2.5f, 1);
    snprintf(d, sizeof d, "хорда 40 %% есть при проезде прямо (%d) и нет на повороте 90° (%d; соседний ложный: хорда везде)", chordOk, none); check("A1chord", chordOk && none, d); }
  { netBegin(3); const float R0[2][3] = {{0, 0, 0}, {0, 0, 200}}, Q[4][3] = {{0, 0, 200}, {30, 0, 200}, {30, 0, 0}, {0, 0, 0}};
    netRoad(3, 0, 1, 1, 0.f, 0, 0, 200.f, 2, R0); netRoad(3, 1, 0, 1, 0.f, 0, 0, 70.f, 4, Q);
    float g[3] = {0.f, 0.f, 5.f}; McEnv e; McCar *c = planOn(&M, 3, "waypoint", 0.f, 100.f, 0.f, g, &pl, &e);
    int loop = routeHas(c, 30.f, 100.f, 5.f, 120.f, 1);
    snprintf(d, sizeof d, "обратный ход ×1,30: объезд петлёй (цена 175) дешевле разворота (183,5); при 1,15 разворот (169) — петля %s", loop ? "выбрана" : "нет"); backOk = loop;
    mcInit(&M); { float pp[3] = {0.f, 0.f, 100.f}; c = mcSpawn(&M, &W, &NT[3], "smith_v", pp, 0.f, 1, "chase", CON, "x"); }
    { Car pc2; carInit(&pc2, 0, &W, 0.f, 0.f, 5.f, 0.f); env(&e, &pc2, 1, 0.f, 5.f); e.tr = &NT[3]; mcUpdate(&M, &e, DT, 0, 0); }
    int back = !routeHas(c, 30.f, 100.f, 5.f, 120.f, 1);
    snprintf(d, sizeof d, "множитель против хода: 1,30 у гонщика (петля %d), 1,0 у погони (разворот %d)", backOk, back); check("P16-05", backOk && back, d); }

  // ================= P04:A12 / 1.9 срезы: бросок против сложности pad8
  { netBegin(4); const float P[2][3] = {{0, 0, -30}, {0, 0, 0}}, Mn[2][3] = {{-50, 0, 0}, {-50, 0, 400}}, S[2][3] = {{50, 0, 0}, {50, 0, 100}}, G[2][3] = {{0, 0, 410}, {0, 0, 440}};
    netRoad(4, 3, 0, 1, 0.f, 0, 0, 30.f, 2, P); netRoad(4, 0, 1, 1, 0.f, 0, 0, 400.f, 2, Mn); netRoad(4, 0, 1, 1, 0.f, 1, 22, 100.f, 2, S); netRoad(4, 1, 2, 1, 0.f, 0, 0, 30.f, 2, G);
    float g[3] = {0.f, 0.f, 435.f}; int tookHi = 0, tookLo = 0;
    for (int pass = 0; pass < 2; pass++) {
      mcInit(&M); float p[3] = {0.f, 0.f, -20.f}; McCar *c = mcSpawn(&M, &W, &NT[4], "smith_v", p, 0.f, 1, "waypoint", CON, "x");
      mcWaypoint(&M, 0, g, 1); int v = pass ? 10 : 30; mcParams(&M, "smith_v", v, v);
      McEnv e; env(&e, &pl, 1, 20.f, -20.f); e.tr = &NT[4]; mcUpdate(&M, &e, DT, 0, 0);
      int sc = routeHas(c, 50.f, 50.f, 5.f, 60.f, 0); if (pass) tookLo = sc; else tookHi = sc; }
    // срез, конец которого не лежит на пути без срезов, не берётся даже при высоком броске (прежний граф с срезами брал его: 110 < 400)
    netBegin(4); { const float P[2][3] = {{0, 0, -30}, {0, 0, 0}}, Mn[2][3] = {{-50, 0, 0}, {-50, 0, 400}}, S2[2][3] = {{50, 0, 0}, {50, 0, 50}}, R4[2][3] = {{50, 0, 50}, {50, 0, 110}}, G[2][3] = {{0, 0, 410}, {0, 0, 440}};
      netRoad(4, 3, 0, 1, 0.f, 0, 0, 30.f, 2, P); netRoad(4, 0, 1, 1, 0.f, 0, 0, 400.f, 2, Mn); netRoad(4, 0, 4, 1, 0.f, 1, 22, 50.f, 2, S2); netRoad(4, 4, 1, 1, 0.f, 0, 0, 60.f, 2, R4); netRoad(4, 1, 2, 1, 0.f, 0, 0, 30.f, 2, G);
      mcInit(&M); float p[3] = {0.f, 0.f, -20.f}; McCar *c = mcSpawn(&M, &W, &NT[4], "smith_v", p, 0.f, 1, "waypoint", CON, "x"); mcWaypoint(&M, 0, g, 1); mcParams(&M, "smith_v", 40, 40);
      McEnv e; env(&e, &pl, 1, 20.f, -20.f); e.tr = &NT[4]; mcUpdate(&M, &e, DT, 0, 0);
      int wrong = routeHas(c, 50.f, 25.f, 5.f, 40.f, 0);
      snprintf(d, sizeof d, "срез сложности 22: при броске 30 — срез (%d), при 10 — длинная дорога (срез %d); срез, чей конец не на пути, при броске 40 не берётся (%d)", tookHi, tookLo, wrong);
      check("A12", tookHi && !tookLo && !wrong, d); } }

  // ================= желаемая скорость: первые 2 с, погоня 0,7, ступени гонки
  { McCar *c = mk(&M, "race", 0.f, 0.f, 0.f); Car p2; carInit(&p2, 0, &W, 12.f, 0.f, 0.f, 0.f); p2.v = 0.f;
    McEnv e; env(&e, &p2, 1, 12.f, 0.f); mcUpdate(&M, &e, DT, 0, 0); for (int f = 0; f < 60; f++) { c->car.x = 0; c->car.z = 0; mcUpdate(&M, &e, DT, 0, 0); }
    float early = c->vdes; for (int f = 0; f < 90; f++) { c->car.x = 0; c->car.z = 0; mcUpdate(&M, &e, DT, 0, 0); } float late = c->vdes;
    snprintf(d, sizeof d, "гонка при стоящем игроке: через 1 с желаемая %.2f = 0,7 × %.2f, через 2,5 с %.2f = пол 0,5 × TopSpeed игрока %.2f", early, c->baseTop, late, .5f * p2.p.topSpeed);
    check("first2s", fabsf(early - .7f * c->baseTop) < .05f && fabsf(late - .5f * p2.p.topSpeed) < .05f, d); }
  { McCar *c = mk(&M, "chase", 0.f, 0.f, 0.f); Car p2; carInit(&p2, 0, &W, 80.f, 0.f, 0.f, 0.f); McEnv e; env(&e, &p2, 1, 80.f, 0.f);
    for (int f = 0; f < 150; f++) { c->car.x = 0; c->car.z = 0; mcUpdate(&M, &e, DT, 0, 0); }
    snprintf(d, sizeof d, "погоня и полиция: желаемая %.2f = 0,7 × TopSpeed %.2f (не полная)", c->vdes, c->baseTop); check("chase70", fabsf(c->vdes - .7f * c->baseTop) < .05f, d); }
  { // ступени гонки: круги/точки, потом разница расстояний
    float pts[1][3] = {{0.f, 0.f, 500.f}}; Car p2; carInit(&p2, 0, &W, 40.f, 0.f, 0.f, 0.f); p2.v = 20.f; float res[3];
    for (int k = 0; k < 3; k++) {
      McCar *c = mk(&M, "race", 0.f, 0.f, 0.f); McEnv e; env(&e, &p2, 1, 40.f, 0.f);
      int pk = k == 0 ? 2 : k == 1 ? 0 : 1; c->rk = k == 1 ? 3 : 1; if (k == 2) c->rk = 1;   // 0: игрок впереди (pk 2 > rk 1); 1: соперник впереди (rk 3 > pk 0); 2: одна точка — по расстоянию
      float dp; mcRacePlaceRob(&M, (const float (*)[3])pts, 1, 5, pk, (float[3]){40.f, 0.f, 0.f}, 0, 0, &dp, 0);
      if (k == 2) { c->rDist = dp - 400.f; }   // на одной точке и соперник ближе на 400 м (≥ D): крайнее «медленно»
      c->car.x = 0; c->car.z = 0; c->rk = (k == 2 ? 1 : c->rk); c->catchT = 0.f; mcUpdate(&M, &e, DT, 0, 0);
      for (int f = 0; f < 3; f++) { c->car.x = 0; c->car.z = 0; c->catchT = 0.f; c->rAhead = 1; mcUpdate(&M, &e, DT, 0, 0); }
      res[k] = c->vdes; }
    snprintf(d, sizeof d, "скорость 20 м/с: игрок на точку впереди — 1,7× = %.1f; соперник на точку впереди — пол 0,5× TopSpeed игрока = %.1f", res[0], res[1]);
    check("D4steps", fabsf(res[0] - clampf(1.7f * 20.f, .5f * p2.p.topSpeed, 1.7f * p2.p.topSpeed)) < .5f && fabsf(res[1] - .5f * p2.p.topSpeed) < .5f, d); }

  { // формула гонки: минимум, середина и максимум зажимаются каждый, потом интерполяция по коэффициенту (скорость игрока 25, TopSpeed игрока ~33,3, соперник позади на 40 м → f = −0,5)
    float pts[1][3] = {{0.f, 0.f, 500.f}}; Car p2; carInit(&p2, 0, &W, 40.f, 0.f, 0.f, 0.f); p2.v = 25.f;
    McCar *c = mk(&M, "race", 0.f, 0.f, 0.f); McEnv e; env(&e, &p2, 1, 40.f, 0.f); float dp; c->rk = 0;
    mcRacePlaceRob(&M, (const float (*)[3])pts, 1, 5, 0, (float[3]){40.f, 0.f, 0.f}, 0, 0, &dp, 0); c->rDist = dp - 40.f; c->rAhead = 1; c->catchT = 0.f; mcUpdate(&M, &e, DT, 0, 0);
    float pt = p2.p.topSpeed, mn = clampf(25.f * .5f, pt * .5f, pt * 1.7f), md = clampf(25.f * 1.1f, pt * .5f, pt * 1.7f), want = md - .5f * (md - mn), old = clampf(25.f * (1.1f - .5f * .6f), pt * .5f, pt * 1.7f);
    snprintf(d, sizeof d, "соперник впереди на 40 м (f = −0,5): желаемая %.2f = середина %.2f − 0,5 × (середина − минимум %.2f) = %.2f (прежнее зажатие после интерполяции дало бы %.2f)", c->vdes, md, mn, want, old);
    check("race-f", fabsf(c->vdes - want) < .15f && fabsf(want - old) > 1.f, d); }

  // ================= P04:A4/1.6 две точки впереди
  { const float r[3][3] = {{0, 0, 0}, {0, 0, 100}, {0, 0, 200}}; McCar *c = withRoute(&M, "waypoint", 3, r, 20.f); step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "20 м/с: ближняя точка на %.1f м (0,8 с = 16), дальняя на %.1f м (1,0 с = 20)", c->dbgNear[2], c->dbgFar[2]);
    check("A4look", fabsf(c->dbgNear[2] - 16.f) < 1.f && fabsf(c->dbgFar[2] - 20.f) < 1.f, d);
    c = withRoute(&M, "waypoint", 3, r, 2.f); step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "2 м/с: нижние границы — ближняя %.1f м (5), дальняя %.1f м (10)", c->dbgNear[2], c->dbgFar[2]);
    check("A4min", fabsf(c->dbgNear[2] - 5.f) < 1.f && fabsf(c->dbgFar[2] - 10.f) < 1.f, d); }

  // ================= P04:A5 руль = угол / предельный угол колёс; обратный знак ×1,25 в остановке после заднего хода
  { const float r[2][3] = {{0, 0, 0}, {6.f, 0, 19.f}}; McCar *c = withRoute(&M, "waypoint", 2, r, 0.f); c->vdes = 20.f; c->catchT = 2.f;
    float ang = atan2f(5.f * 6.f / 19.8f, 5.f * 19.f / 19.8f); (void)ang;
    step1(&M, &pl, c, 25.f, 0.f); float st = c->lastIn.steer, mw = c->car.p.maxWheelTurn, e1 = atan2f(c->dbgNear[0], c->dbgNear[2]);
    float want = clampf(-e1 / mw, -1.f, 1.f);
    snprintf(d, sizeof d, "цель под углом %.3f рад: руль %.3f = −угол / %.3f = %.3f (прежний −угол × 2,2 = %.3f)", e1, st, mw, want, -e1 * 2.2f);
    check("A5steer", fabsf(st - want) < .02f && fabsf(want + e1 * 2.2f) > .02f, d);
    c = withRoute(&M, "waypoint", 2, r, 0.f); c->vdes = 20.f; c->catchT = 2.f; c->stopHold = 1; step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "остановка после заднего хода: знак обратный ×1,25 — руль %.3f (вперёд было %.3f)", c->lastIn.steer, st);
    check("A5hold", c->lastIn.steer > 0.f && fabsf(c->lastIn.steer - clampf(-want * 1.25f, -1.f, 1.f)) < .03f, d); }

  // ================= P04:A2/1.4 газ релейный, тормоза перед препятствием нет
  { const float r[2][3] = {{0, 0, 0}, {0, 0, 300}}; McCar *c = withRoute(&M, "waypoint", 2, r, 20.f); c->vdes = 30.f; c->catchT = 100.f;
    netBegin(5); NT[5].maxCars = 1; carInit(&NT[5].car[0].car, 0, &W, 0.f, 0.f, 30.f, 0.f); NT[5].car[0].mode = TRM_DRIVE;   // машина трафика впереди (прежний водитель тормозил перед ней)
    int brakes = 0, full = 0; for (int f = 0; f < 20; f++) { c->vdes = 30.f; McEnv e; env(&e, &pl, 1, 25.f, 0.f); e.tr = &NT[5]; mcUpdate(&M, &e, DT, 0, 0); brakes += c->lastIn.brake > .01f; full += c->lastIn.throttle > .99f; }
    snprintf(d, sizeof d, "20 м/с, машина трафика в 30 м, желаемая 30: торможений %d, кадров с полным газом %d из 20", brakes, full);
    check("A2relay", brakes == 0 && full >= 18, d);
    c = withRoute(&M, "waypoint", 2, r, 36.f); c->vdes = 30.f; step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "36 м/с при желаемой 30 (+1,39): газ ноль %.2f, тормоза нет %.2f", c->lastIn.throttle, c->lastIn.brake); check("A2cut", c->lastIn.throttle < .01f && c->lastIn.brake < .01f, d); }

  // ================= 1.5 / Д1 поворот: занос раньше выхода; выход с полным газом; 50 и 40 м/с
  { const float r[3][3] = {{0, 0, 0}, {5.66f, 0, 5.66f}, {16.f, 0, 24.f}}; McCar *c = withRoute(&M, "waypoint", 3, r, 10.f); c->turning = 1; c->vdes = 30.f; c->catchT = 100.f;
    step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "в повороте нос отвернут от ближней точки на 45° (>32°), звено в 29° (<45°), 10 м/с: ручник %.1f, состояние %d (занос раньше выхода)", c->lastIn.handbrake, c->turning);
    check("D1skid", c->lastIn.handbrake > .5f && c->turning == 1, d); }
  { const float r[3][3] = {{0, 0, 0}, {0, 0, 100}, {0, 0, 200}}; McCar *c = withRoute(&M, "waypoint", 3, r, 10.f); c->turning = 1; c->vdes = 5.f; c->catchT = 100.f; step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "в повороте звено прямо: выход (состояние %d) с полным газом %.2f при желаемой 5 и скорости 10", c->turning, c->lastIn.throttle); check("D1exit", c->turning == 0 && c->lastIn.throttle > .99f, d); }
  { float b55, t55, b45, t45;
    for (int k = 0; k < 2; k++) { float v = k ? 45.f : 55.f, ln = .8f * v, m = .2f * v - 2.f; float r[3][3] = {{0, 0, 0}, {0, 0, ln + 2.f}, {m * .99f * 3.f + 0.f, 0, ln + 2.f + m * .15f * 3.f}};
      McCar *c = withRoute(&M, "waypoint", 3, r, v); c->turning = 1; c->vdes = 20.f; c->catchT = 100.f; step1(&M, &pl, c, 25.f, 0.f);
      if (k) { b45 = c->lastIn.brake; t45 = c->lastIn.throttle; } else { b55 = c->lastIn.brake; t55 = c->lastIn.throttle; } (void)t55; }
    snprintf(d, sizeof d, "в повороте: 55 м/с тормоз %.1f, 45 м/с тормоза нет (%.1f) и газ сброшен (%.1f)", b55, b45, t45);
    check("D1speed", b55 > .5f && b45 < .01f && t45 < .01f, d); }

  // ================= P04:A3/Д5 объезд
  { const float r[2][3] = {{0, 0, 0}, {0, 0, 300}}; McCar *c = withRoute(&M, "waypoint", 2, r, 10.f); float q[3] = {10.f, 0.f, 10.f}; mcSpawn(&M, &W, 0, "park", q, 0.f, 1, "NULL", CON, "x"); step1(&M, &pl, c, 25.f, 0.f);
    float sh1 = c->shiftF;
    c = withRoute(&M, "waypoint", 2, r, 10.f); float q2[3] = {0.f, 0.f, -5.f}; mcSpawn(&M, &W, 0, "park", q2, 0.f, 1, "NULL", CON, "x"); step1(&M, &pl, c, 25.f, 0.f);
    snprintf(d, sizeof d, "машина под 45° в 14 м: сдвиг %.2f (3 м × cos45° = 2,12, в сторону от неё); машина позади на 5 м — сдвиг %.2f (нет)", sh1, c->shiftF);
    check("A3cos", fabsf(fabsf(sh1) - 2.12f) < .15f && sh1 < 0.f && fabsf(c->shiftF) < .01f, d); }

  // ================= P04:A6 застревание (стена z = 400): порог, время заднего хода растёт, седьмой раз на дорогу
  { int pass; float firstBack[8] = {0}; int resets[2] = {0, 0};
    for (pass = 0; pass < 2; pass++) {
      static MsnCars M2; McCar *c = mk(&M2, "race", 0.f, 394.f, 0.f); c->noPlace = pass; Car p3; carInit(&p3, 0, &W, 25.f, 0.f, 394.f, 0.f);
      int last = 0; for (int f = 0; f < 60 * 100; f++) { McEnv e; env(&e, &p3, 1, 25.f, 394.f); c->vdes = 6.f; c->catchT = 100.f; mcUpdate(&M2, &e, DT, 0, 0);   // медленный подъезд: счётчик попыток сбрасывается при скорости вперёд выше 14 м/с
        if (c->stuckN > last && c->stuckN < 8 && !pass && firstBack[c->stuckN] == 0.f) firstBack[c->stuckN] = c->backT; last = c->stuckN;
        if (getenv("SV_TRACE") && !pass && f % 30 == 0 && f < 60 * 40) printf("  t %.1f z %.1f v %.2f N %d backT %.2f backLen %.2f stkOn %d stkT %.2f thr %.2f hold %d\n", f * DT, c->car.z, c->car.v, c->stuckN, c->backT, c->backLen, c->stkOn, c->stkT, c->stkThr, c->stopHold); }
      resets[pass] = (int)c->resets; }
    snprintf(d, sizeof d, "задний ход по срабатываниям: %.2f %.2f %.2f %.2f с (0,5; +0,2 каждый раз)", firstBack[1], firstBack[2], firstBack[3], firstBack[4]);
    check("A6back", fabsf(firstBack[1] - .5f) < .03f && fabsf(firstBack[2] - .7f) < .03f && fabsf(firstBack[3] - .9f) < .03f, d);
    snprintf(d, sizeof d, "седьмое срабатывание ставит на дорогу: постановок %d (обычная), %d (цель уничтожения: не ставится)", resets[0], resets[1]);
    check("A6place", resets[0] >= 1 && resets[1] == 0, d); }

  // ================= P04:A7 потеря управления, A8 пауза и задний ход погони после тарана
  { static MsnCars M3; McCar *c = mk(&M3, "race", 0.f, 0.f, 0.f); Car p4; carInit(&p4, 0, &W, -14.f, 0.f, 0.f, 1.5707963f); carSetSpeed(&p4, 15.f);
    float slipMax = 0.f; CarInput in = {1, 0, 0, 0};
    for (int f = 0; f < 90; f++) { carUpdate(&p4, &W, &in, DT); McEnv e; env(&e, &p4, 1, p4.x, p4.z); e.hold = 1; mcUpdate(&M3, &e, DT, 0, 0); if (c->slipT > slipMax) slipMax = c->slipT; }
    static MsnCars M3b; McCar *cw = mk(&M3b, "race", 0.f, 0.f, 0.f); Car p4b; carInit(&p4b, 0, &W, -5.f, 0.f, 0.f, 1.5707963f); carSetSpeed(&p4b, 1.5f); float slipWeak = 0.f;
    for (int f = 0; f < 120; f++) { carUpdate(&p4b, &W, &in, DT * 0.f + DT * 0.f); p4b.vel[0] = 1.5f; McEnv e; env(&e, &p4b, 1, p4b.x, p4b.z); e.hold = 1; mcUpdate(&M3b, &e, DT, 0, 0); if (cw->slipT > slipWeak) slipWeak = cw->slipT; p4b.x += 1.5f * DT; }
    snprintf(d, sizeof d, "удар игрока по стоящей машине ИИ: потеря управления %.2f с (1,5); слабое касание 1,5 м/с — %.2f (импульс ниже 0,02 × 100000)", slipMax, slipWeak); check("A7slip", slipMax > 1.4f && slipWeak < .01f, d);
    McCar *c2 = withRoute(&M, "waypoint", 2, (const float[2][3]){{0, 0, 0}, {6.f, 0, 19.f}}, 0.f); c2->vdes = 20.f; c2->catchT = 100.f; c2->slipT = 1.5f; step1(&M, &pl, c2, 25.f, 0.f);
    float st2 = c2->lastIn.steer; c2->slipT = 0.f; c2->car.x = c2->car.z = 0.f; step1(&M, &pl, c2, 25.f, 0.f); float st1 = c2->lastIn.steer;
    snprintf(d, sizeof d, "руль под потерей управления %.3f = ×1,5 от обычного %.3f", st2, st1); check("A7steer", fabsf(st2 - clampf(st1 * 1.5f, -1.f, 1.f)) < .03f && fabsf(st1) > .05f, d); }
  { static MsnCars M4; mcInit(&M4); float p[3] = {-14.f, 0.f, 0.f}; McCar *c = mcSpawn(&M4, &W, 0, "cPolice", p, 1.5707963f, 1, "chase", "Pursuit\\L1cop.con", ""); c->police = 1;
    Car p5; carInit(&p5, 0, &W, 0.f, 0.f, 0.f, 0.f); CarInput none = {0, 0, 0, 1}; float ramMax = 0.f, backFirst = -1.f; int prevBack = 0;
    for (int f = 0; f < 60 * 8; f++) { carUpdate(&p5, &W, &none, DT); McEnv e; env(&e, &p5, 1, p5.x, p5.z); mcUpdate(&M4, &e, DT, 0, 0);
      if (c->ramT > ramMax) ramMax = c->ramT; if (!prevBack && c->backT > 0.f && backFirst < 0.f && ramMax > 0.f) backFirst = c->backT; prevBack = c->backT > 0.f; }
    snprintf(d, sizeof d, "полиция тараном ударила стоящего игрока: пауза на ручнике %.2f с, затем задний ход %.2f с (0,3)", ramMax, backFirst);
    check("A8ram", ramMax > .45f && fabsf(backFirst - .3f) < .05f, d); }

  // ================= P04:A9 погоня: пороги 20 м и 60 м, упреждение, пешком
  { static MsnCars M5; mcInit(&M5); float p[3] = {50.f, 0.f, 0.f}; McCar *pc = mcSpawn(&M5, &W, 0, "cPolice", p, 0.f, 1, "chase", "Pursuit\\L1cop.con", ""); pc->police = 1;
    McCar *mc = mcSpawn(&M5, &W, 0, "smith_v", (float[3]){-50.f, 0.f, 0.f}, 0.f, 1, "chase", CON, "x");
    Car p6; carInit(&p6, 0, &W, 0.f, 0.f, 0.f, 0.f); McEnv e; env(&e, &p6, 1, 0.f, 0.f); mcUpdate(&M5, &e, DT, 0, 0);
    int dp = pc->direct, dm = mc->direct; pc->car.x = 15.f; mc->car.x = -15.f; mcUpdate(&M5, &e, DT, 0, 0);
    snprintf(d, sizeof d, "в 50 м: полиция напрямую %d (порог 60), обычная погоня %d (порог 20); в 15 м: обычная %d", dp, dm, mc->direct);
    check("A9thr", dp == 1 && dm == 0 && mc->direct == 1, d); }
  { static MsnCars M6; mcInit(&M6); McCar *c = mcSpawn(&M6, &W, 0, "smith_v", (float[3]){-15.f, 0.f, 0.f}, 1.5707963f, 1, "chase", CON, "x");
    Car p7; carInit(&p7, 0, &W, 0.f, 0.f, 0.f, 0.f); float px = 0.f; float gx = 0.f;
    for (int f = 0; f < 4; f++) { px += 10.f * DT; McEnv e; env(&e, &p7, 0, px, 0.f); mcUpdate(&M6, &e, DT, 0, 0); gx = c->dbgNear[0]; c->car.x = -15.f; carSetSpeed(&c->car, 0.f); }
    snprintf(d, sizeof d, "игрок пешком бежит по +x, преследователь стоит: точка упреждения x = %.0f (смещение 1000 м вперёд по бегу)", gx); check("A9foot", gx > 500.f, d); }

  // ================= P04:A10 ожидание игрока: впереди ждёт, позади нет, уличная гонка не ждёт; 3D
  { float run[3];
    for (int k = 0; k < 3; k++) { McCar *c = mk(&M, "race", 0.f, 0.f, 0.f); Car p8; carInit(&p8, 0, &W, 0.f, 0.f, -380.f, 0.f);
      c->rAhead = k == 1 ? 0 : 1; run[k] = 0.f;
      for (int f = 0; f < 60 * 8; f++) { McEnv e; env(&e, &p8, 1, 0.f, -380.f); e.noRaceWait = k == 2; mcUpdate(&M, &e, DT, 0, 0); c->rAhead = k == 1 ? 0 : 1; run[k] = fmaxf(run[k], hypotf(c->car.x, c->car.z)); } }
    snprintf(d, sizeof d, "игрок в 380 м: впереди — стоит (%.0f м), позади — едет (%.0f м), уличная гонка — едет (%.0f м)", run[0], run[1], run[2]);
    check("A10", run[0] < 10.f && run[1] > 30.f && run[2] > 30.f, d); }

  // ================= P04:A13/B2 точка пути в 3D, радиус 7 м в бонус-игре; A14 32 точки
  { Car p9; carInit(&p9, 0, &W, 25.f, 0.f, 0.f, 0.f);
    McCar *c = mk(&M, "waypoint", 0.f, 0.f, 0.f); float wa[3] = {0.f, 9.5f, 6.f}, wf[3] = {0.f, 0.f, 500.f}; mcWaypoint(&M, 0, wa, 1); mcWaypoint(&M, 1, wf, 1); step1(&M, &p9, c, 25.f, 0.f);
    snprintf(d, sizeof d, "точка в 6 м по земле и 9,5 м вверху (3D 11,2 м > 10): не пройдена, wp %d (в плоскости было бы пройдена)", c->wp); check("A13", c->wp == 0, d);
    c = mk(&M, "waypoint", 0.f, 0.f, 0.f); float wb[3] = {0.f, 0.f, 8.f}; mcWaypoint(&M, 0, wb, 1); mcWaypoint(&M, 1, wf, 1); step1(&M, &p9, c, 25.f, 0.f); int plain = c->wp;
    mcPlanHook = bonusHook; c = mk(&M, "waypoint", 0.f, 0.f, 0.f); mcWaypoint(&M, 0, wb, 1); mcWaypoint(&M, 1, wf, 1); step1(&M, &p9, c, 25.f, 0.f); int bonus = c->wp; mcPlanHook = 0;
    snprintf(d, sizeof d, "точка в 8 м: в миссии пройдена (wp %d, радиус 10), в бонус-игре нет (wp %d, радиус 7)", plain, bonus); check("B2", plain >= 1 && bonus == 0, d); }
  { mcInit(&M); float w[3] = {0.f, 0.f, 1.f}; for (int i = 0; i < 20; i++) mcWaypoint(&M, i, w, 1);
    snprintf(d, sizeof d, "20 точек пути в этапе: принято %d (до 32; прежнее 16)", M.nwp); check("A14", M.nwp == 20, d); }

  // ================= svKind/svRace (g01 7.4), полиция стоит в разговоре (g10 2.8), касание предметов (g11), счётчик событий ожидания
  { static MsnCars M7; mcInit(&M7); McCar *r = mcSpawn(&M7, &W, 0, "smith_v", (float[3]){0.f, 0.f, 0.f}, 0.f, 1, "race", CON, "x");
    McCar *pc = mcSpawn(&M7, &W, 0, "cPolice", (float[3]){0.f, 0.f, 50.f}, 0.f, 1, "chase", "Pursuit\\L1cop.con", ""); pc->police = 1;
    Car p10; carInit(&p10, 0, &W, 25.f, 0.f, 0.f, 0.f); McEnv e; env(&e, &p10, 1, 25.f, 0.f); mcUpdate(&M7, &e, DT, 0, 0);
    snprintf(d, sizeof d, "вид машины для взрыва: ИИ миссии %d (2), полиция %d (3); гонка — взрыв через 3 с (svRace %d, у полиции %d)", r->car.svKind, pc->car.svKind, r->car.svRace, pc->car.svRace);
    check("svKind", r->car.svKind == 2 && pc->car.svKind == 3 && r->car.svRace == 1 && pc->car.svRace == 0, d);
    float x0 = pc->car.x, z0 = pc->car.z; for (int f = 0; f < 60; f++) { McEnv e2; env(&e2, &p10, 1, 25.f, 0.f); e2.freeze = 1; mcUpdate(&M7, &e2, DT, 0, 0); }
    float mv = hypotf(pc->car.x - x0, pc->car.z - z0);
    snprintf(d, sizeof d, "разговор (freeze): полиция за 1 с сдвинулась на %.2f м (стоит); без разговора едет", mv); float x1 = pc->car.x, z1 = pc->car.z;
    for (int f = 0; f < 60; f++) { McEnv e3; env(&e3, &p10, 1, 25.f, 0.f); mcUpdate(&M7, &e3, DT, 0, 0); }
    check("freeze", mv < .3f && hypotf(pc->car.x - x1, pc->car.z - z1) > 1.f, d); }
  { static int touched; static MsnCars M8; mcInit(&M8);
    McCar *a = mcSpawn(&M8, &W, 0, "smith_v", (float[3]){0.f, 0.f, 0.f}, 0.f, 1, "race", CON, "x"); carSetSpeed(&a->car, 5.f);
    McCar *bb = mcSpawn(&M8, &W, 0, "b", (float[3]){0.f, 0.f, 300.f}, 0.f, 1, "race", CON, "x"); carSetSpeed(&bb->car, 5.f);
    mcSpawn(&M8, &W, 0, "c", (float[3]){40.f, 0.f, 0.f}, 0.f, 1, "NULL", CON, "x");
    Car p11; carInit(&p11, 0, &W, 25.f, 0.f, 0.f, 0.f);
    McEnv e; env(&e, &p11, 1, 25.f, 0.f); e.carTouch = touchCount; e.touchU = &touched; touched = 0; mcUpdate(&M8, &e, DT, 0, 0);
    snprintf(d, sizeof d, "касание предметов: из 3 машин (едет в 25 м, стоит в 40 м, едет в 300 м от игрока) вызвано %d — только движущиеся в пределах 80 м (до правки 0: предметы не замечали ИИ; обход всех подряд стоил бы лишнего)", touched);
    check("touch", touched == 1, d); }
  // ================= круг 7: кто ударил (встречный удар — «ударены» обе), петля ИИ после последней точки, одна точка за кадр, обход в 3D
  { static MsnCars Mh; mcInit(&Mh); McCar *a = mcSpawn(&Mh, &W, 0, "a", (float[3]){-30.f, 0.f, 0.f}, 1.5707963f, 1, "chase", CON, "x"), *b = mcSpawn(&Mh, &W, 0, "b", (float[3]){30.f, 0.f, 0.f}, -1.5707963f, 1, "chase", CON, "x");
    carSetSpeed(&a->car, 15.f); carSetSpeed(&b->car, 15.f); Car far_; carInit(&far_, 0, &W, 0.f, 0.f, 300.f, 0.f); float sa = 0.f, sb = 0.f;
    for (int f = 0; f < 120; f++) { McEnv e; env(&e, &far_, 1, 0.f, 0.f); e.hold = 0; e.noRaceWait = 1; mcUpdate(&Mh, &e, DT, 0, 0); if (a->slipT > sa) sa = a->slipT; if (b->slipT > sb) sb = b->slipT;
      if (getenv("SV_TRACE") && f % 10 == 0) printf("  hon f %d a %.1f v %.1f hv %.2f | b %.1f v %.1f hv %.2f slip %.2f %.2f\n", f, a->car.x, a->car.v, a->car.carHitV, b->car.x, b->car.v, b->car.carHitV, a->slipT, b->slipT); }
    snprintf(d, sizeof d, "встречный удар двух машин ИИ на 15 м/с: потеря управления у обеих (%.2f и %.2f)", sa, sb); check("headon", sa > 1.4f && sb > 1.4f, d); }
  { McCar *c = mk(&M, "waypoint", 0.f, 0.f, 0.f); float w0[3] = {0.f, 0.f, 3.f}, w1[3] = {0.f, 0.f, 600.f}; mcWaypoint(&M, 0, w0, 1); mcWaypoint(&M, 1, w1, 1);
    Car p13; carInit(&p13, 0, &W, 25.f, 0.f, 0.f, 0.f); step1(&M, &p13, c, 25.f, 0.f); int one = c->wp;
    mcWaypoint(&M, 0, w0, 1); float w2[3] = {0.f, 0.f, 7.f}; mcWaypoint(&M, 1, w2, 1); c = &M.c[0]; c->wp = 0; c->passed = -1; c->finished = 0;
    for (int f = 0; f < 60 * 3; f++) { McEnv e; env(&e, &p13, 1, 25.f, 0.f); mcUpdate(&M, &e, DT, 0, 0); }
    snprintf(d, sizeof d, "две точки в радиусе подряд: за один кадр пройдена одна (wp %d); после последней точки ИИ не встаёт, а идёт по кругу (finished %d, скорость %.1f, wp %d)", one, c->finished, c->car.v, c->wp);
    check("loop", one == 1 && c->finished && c->car.v > 1.f, d); }
  { const float r[2][3] = {{0, 0, 0}, {0, 0, 300}}; McCar *c = withRoute(&M, "waypoint", 2, r, 10.f); float q[3] = {0.f, 0.f, 10.f}; McCar *pk = mcSpawn(&M, &W, 0, "park", q, 0.f, 1, "NULL", CON, "x"); pk->car.y = 60.f; step1(&M, &pl, c, 25.f, 0.f); float shHigh = c->shiftF;
    McCar *c2 = withRoute(&M, "waypoint", 2, r, 10.f); float q2[3] = {10.f, 0.f, 10.f}; mcSpawn(&M, &W, 0, "park", q2, 0.f, 1, "NULL", CON, "x"); step1(&M, &pl, c2, 25.f, 0.f);
    snprintf(d, sizeof d, "машина в 10 м по земле на 60 м выше (3D 61 м > 50): сдвиг %.2f (нет); на одной высоте под 45° — %.2f (−2,12)", shHigh, c2->shiftF); check("avoid3d", fabsf(shHigh) < .01f && fabsf(c2->shiftF + 2.12f) < .15f, d); }

  // ================= P04:Д13 дальше 10 м от всех звеньев пути дольше 3 с — путь заново
  { unsigned rp[2]; mcPlanHook = lineHook;
    for (int k = 0; k < 2; k++) { McCar *c = mk(&M, "race", k ? 8.f : 12.f, 0.f, 0.f); Car p12; carInit(&p12, 0, &W, 25.f, 0.f, 0.f, 0.f); unsigned r0 = 0;
      for (int f = 0; f < 60 * 4; f++) { c->car.x = k ? 8.f : 12.f; c->car.z = 0.f; c->vdes = 3.f; c->catchT = 100.f; McEnv e; env(&e, &p12, 1, 25.f, 0.f); mcUpdate(&M, &e, DT, 0, 0); if (f == 30) r0 = c->replans; }
      rp[k] = c->replans - r0; }
    mcPlanHook = 0;
    snprintf(d, sizeof d, "в 12 м от пути 3,5 с: перепланирований %u (≥1); в 8 м: %u (0)", rp[0], rp[1]); check("D13", rp[0] >= 1 && rp[1] == 0, d); }

  printf("RESULT %s провалов %d из %d\n", fails ? "FAIL" : "PASS", fails, nchecks);
  return fails ? 1 : 0;
}
