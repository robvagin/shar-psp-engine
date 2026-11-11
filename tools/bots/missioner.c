// Миссионер (пункт Б2, docs/БОТЫ-ИГРОКИ_2026-09-28.md §3.2): проходит миссии уровня настоящей игрой без экрана.
// Миссия запускается как пункт MISSION SELECT меню паузы оригинала (msn_start_mission), дальше только ввод игрока:
// к маркеру цели — по графу дорог водителем навигатора; цель «пешком» (дверь, разговор) — выйти (△), дойти, нажать действие (△).
// Цели засчитывает рантайм миссий по настоящему положению (триггеры локаторов). fake_engine и телепорта нет.
// Между целями — снимок мира: при провале перебирается только последний отрезок (другие числа водителя), до 3 попыток.
// Итог по миссии: пройдена / провал / не покрыто (чего нет в движке: интерьеры, ИИ-машины) / застрял; время, запас таймера, урон.
//   missioner LEVEL HOMER ROADS MSN [--mission N | --all] [--secs 900] [--impossible-timer] [--coins N] [--out DIR]
//   --impossible-timer: отрицательный контроль гейта — любой таймер стадии урезается до 1 с, миссия обязана провалиться
// Т1.5 (docs/handoff/ДВА_ТЕСТИРОВЩИКА_2026-09-28.md §3): к каждой новой машинной цели миссионер сначала едет навигатором
// (эволюция отрезка, как Б1); «застрял» проверяет второй независимый водитель (другой seed, со старта стадии, объезд места
// первого). Класс итога: игра (оба не доехали) / водитель (второй доехал) / нет данных (не покрыто, время) / намеренно
// (провал правилом миссии: таймер, урон — игра сработала как задумано) / — (пройдена).
//   missioner LEVEL HOMER ROADS MSN --leg X Y Z [--secs S]: только отрезок от спавна до точки двумя водителями -> класс (гейт Т1.5)
//   MISSIONER_WEAK1=1 — первый водитель нарочно слабый (руль наоборот); MISSIONER_NO_SECOND=1 — без второго водителя (как до Т1.5);
//   сборка -DMSN_LOOSE_ARRIVE=1 — приезд без проверки высоты (контроль гейта: непроходимое «достигается»)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "navgraph.h"
#include "footpath.h"
#include "footnav.h"   // 05.11: пешая навигация g04-homer (626ca49f): длинный план по дорогам + пешеходы A/B, прыжок через тонкую преграду. MISSIONER_NO_FOOTNAV=1 — без неё (красный контроль)   // 03.10: путь пешком по сетке по настоящей коллизии (m0 своя машина у двери, m1 крыльцо школы)

static BotWorld W; static BotSnap stageSnap; static int impossibleTimer, verbose, startCoins = -1, fromMain, brokenGoal;   // startCoins: --coins N — кошелёк на старте миссии (покупки: m3 «купи Plow King»)
static BotSnap cpSnap; static int cpRec, cpKey, cpTries;   // 30.09: снимок стадии RESET_TO_HERE — чекпоинт, с которого игра повторяет миссию
typedef struct { unsigned b; float ax, ay; } In;
static In *rec; static int nrec, recCap = 60 * 60 * 30;
static int traceMission = -1, tracePhase = -99, traceStage = -99, traceEnabled;
static void traceCurrentStage(void) {
#ifndef ALLRISK_STAGE_TRACE_OLD
  const MsnRuntime *rt = &W.msn.rt;
  if (!traceEnabled || (tracePhase == rt->phase && traceStage == rt->stage)) return;
  if (rt->status != MSN_RUNNING) return;
  tracePhase = rt->phase; traceStage = rt->stage;
  float x = gameOnFoot(&W.G) ? W.homer.x : W.car.x;
  float y = gameOnFoot(&W.G) ? W.homer.y : W.car.y;
  float z = gameOnFoot(&W.G) ? W.homer.z : W.car.z;
  printf("STAGE\t%d\t%d\t%d\t%s\t%.3f\t%.3f\t%.3f\t%u\n", traceMission, rt->phase, rt->stage,
         msn_objective_name[rt->obj], x, y, z, W.st.frames);
  fflush(stdout);
#endif
}
// ночь 04.11 (наряд «боты»): быстрый водитель и лестница без вечного стояния
//   MISSIONER_FAST=1 — всегда быстрый (не careful: толкает трафик, едет без оглядки на урон), =0 — никогда; без переменной — быстрый
//   с третьей попытки стадии на гонке и таймере. MISSIONER_SEED=N — вариант: другие числа водителя (PAR0) и другой поток эволюции.
//   MISSIONER_NO_TELEPORT=1 — без переноса (красный контроль лестницы). --broken-goal — красный контроль: цель на 60 м над маркером
static float fastK = 0.f;   // 0 — по умолчанию: гонка 3.0 (L2 sr1 проходится на первой попытке за 96.7 с при 1.35 и 2.0 не успевает на 3–6 %), прочий таймер 1.8;   // MISSIONER_FASTK: во сколько раз можно превысить желаемую скорость водителя полос до торможения (быстрый водитель)
static int fastDrive; static float parBase[NPAR]; static uint32_t seedSet; static char waitWhy[160];
#define TP_MAX 8   // переносов на миссию; больше — честное «застрял»
// Механики (05.11, «главное, чтобы механика работала»): счётчики событий мира за прогон (монеты, удары по предметам, ломка стейтпропов, входы в комнаты, удары трафика)
// с местом первого события, и ход гонки раз в 5 с (прогресс игрока и соперников). Счётчики считают всё, что видели, включая отброшенные повторы. Строки MECH в stdout
enum { MK_COIN, MK_KICK, MK_SP, MK_ROOM, MK_TRAF, MK_N };
static unsigned mPrev[MK_N], mCnt[MK_N]; static int mHave[MK_N]; static float mPos[MK_N][3];
static void mechReset(void) { memset(mPrev, 0, sizeof mPrev); memset(mCnt, 0, sizeof mCnt); memset(mHave, 0, sizeof mHave); }
static void mechTrack(void) {
  unsigned cur[MK_N] = {W.st.coins, W.st.propKicks, W.st.spChanges, W.st.rooms, W.st.trEvents};
  for (int k = 0; k < MK_N; k++) {
    if (cur[k] > mPrev[k]) { mCnt[k] += cur[k] - mPrev[k]; if (!mHave[k]) { mHave[k] = 1; int ft = gameOnFoot(&W.G);
      mPos[k][0] = ft ? W.homer.x : W.car.x; mPos[k][1] = ft ? W.homer.y : W.car.y; mPos[k][2] = ft ? W.homer.z : W.car.z; } }
    mPrev[k] = cur[k];
  }
  const MsnRuntime *rt = &W.msn.rt;
  if (rt->status == MSN_RUNNING && rt->obj == MSN_OBJ_RACE && W.st.frames % 300 == 0) {
    printf("MECH\trace\t%u\t%d/%d\t%d/%d", W.st.frames / 60, rt->lap, rt->laps, rt->next_item, rt->nitems);
    for (int i = 0; i < W.msn.cars.n; i++) { const McCar *c = &W.msn.cars.c[i]; if (c->on) printf("\t%s:%d:%d", c->name, c->wp, c->finished); }
    printf("\n");
  }
}
static void mechSummary(int mi) {
  static const char *nm[MK_N] = {"coins", "kicks", "sp", "rooms", "traffic"};
  printf("MECH\tsum\t%d", mi);
  for (int k = 0; k < MK_N; k++) printf("\t%s:%u:%.1f:%.1f:%.1f", nm[k], mCnt[k], mHave[k] ? mPos[k][0] : 0.f, mHave[k] ? mPos[k][1] : 0.f, mHave[k] ? mPos[k][2] : 0.f);
  printf("\n");
}
static void step(unsigned b, float ax, float ay) {
  if (nrec < recCap) rec[nrec++] = (In){b, ax, ay};
  bwStep(&W, b, ax, ay); traceCurrentStage(); mechTrack();
}

// эволюция отрезка, как навигатор Б1: от снимка cur к точке p — варианты чисел водителя и сдвигов опорных точек вбок,
// лучшие скрещиваются и мутируют; цель — настоящий приезд (6 м по плоскости, ±2.5 м по высоте). -> кадров до цели или -1;
// out — ввод лучшего варианта (его потом играет основной прогон с записью)
static BotSnap curSnap; static In *leg, *legBest; static uint32_t lrs = 1; static int legWeak;   // legWeak: руль наоборот (MISSIONER_WEAK1)
static float legEndX, legEndZ;   // где остался лучший вариант (не доехал) — второй водитель объезжает это место
#ifndef MSN_LOOSE_ARRIVE
#define MSN_LOOSE_ARRIVE 0
#endif
static float lfr(void) { lrs ^= lrs << 13; lrs ^= lrs >> 17; lrs ^= lrs << 5; return (lrs >> 8) * (1.f / 16777216.f); }
static float lgauss(void) { float u = lfr() + 1e-7f, v = lfr(); return sqrtf(-2.f * logf(u)) * cosf(6.2831853f * v); }
static int legRun(const P3 *q0, int n, const float *par, const float *off, MsnVec3 p, int frames, In *buf, float *fit) {
  bwLoad(&W, &curSnap);
  P3 q[MAXWP]; for (int i = 0; i < n; i++) {
    int a = i > 0 ? i - 1 : 0, b = i + 1 < n ? i + 1 : n - 1; float dx = q0[b].x - q0[a].x, dz = q0[b].z - q0[a].z, l = hypotf(dx, dz) + 1e-3f;
    float o = (i == 0 || i == n - 1) ? 0.f : off[i]; q[i] = (P3){q0[i].x + dz / l * o, q0[i].y, q0[i].z - dx / l * o}; }
  int seg = 0; float best = 1e9f;
  for (int f = 0; f < frames; f++) {
    unsigned b; float ax; navSteer(q, n, &seg, &W.car, par, &b, &ax); if (legWeak) ax = -ax; buf[f] = (In){b, ax, 0}; bwStep(&W, b, ax, 0);
    float d = hypotf(W.car.x - p.x, W.car.z - p.z); if (d < best) best = d;
    if (d < 6.f && (MSN_LOOSE_ARRIVE || fabsf(W.car.y - p.y) < 2.5f)) { *fit = f / 60.f; return f + 1; }
  }
  *fit = frames / 60.f + best / 5.f + 2.f * fabsf(W.car.y - p.y); return -1;
}
static int evolveLeg(MsnVec3 p, float secs, int *nout) {
  bwSave(&W, &curSnap);
  P3 q0[MAXWP]; int n = navPlan(W.car.x, W.car.y, W.car.z, p.x, p.y, p.z, q0, MAXWP), frames = (int)(secs * 60), bestN = -1; float bestFit = 1e9f;
#if defined(ALLRISK_MISSION_LIBRARY) && !defined(ALLRISK_SEARCH_OLD)
  // Tool budget: three generations of eight candidate inputs. This does not
  // alter game logic; the independent second driver still gets three seeds.
  enum { POP = 8, GENS = 3, ELITE = 2 };
#else
  enum { POP = 24, GENS = 16, ELITE = 4 };
#endif
  static float par[POP][NPAR], off[POP][MAXWP], fit[POP]; int idx[POP];
  for (int g = 0; g < GENS; g++) {
    for (int i = 0; i < POP; i++) {
      if (g == 0) { for (int k = 0; k < NPAR; k++) par[i][k] = i == 0 ? PAR0[k] : fmaxf(PARLO[k], fminf(PARHI[k], PAR0[k] * (1.f + .3f * lgauss())));
                    for (int k = 0; k < n; k++) off[i][k] = i == 0 ? 0.f : 4.f * lgauss() * (lfr() < .4f); }
      else if (i >= ELITE) { int a = idx[(int)(lfr() * ELITE)], b = idx[(int)(lfr() * ELITE)];
        for (int k = 0; k < NPAR; k++) { par[i][k] = lfr() < .5f ? par[a][k] : par[b][k]; if (lfr() < .3f) par[i][k] = fmaxf(PARLO[k], fminf(PARHI[k], par[i][k] * (1.f + .15f * lgauss()))); }
        for (int k = 0; k < n; k++) { off[i][k] = lfr() < .5f ? off[a][k] : off[b][k]; if (lfr() < .2f) off[i][k] = fmaxf(-14.f, fminf(14.f, off[i][k] + 3.f * lgauss())); } }
      else continue;
      int r = legRun(q0, n, par[i], off[i], p, frames, leg, &fit[i]);
      if (fit[i] < bestFit) { bestFit = fit[i]; bestN = r; memcpy(legBest, leg, sizeof(In) * (size_t)(r > 0 ? r : frames)); legEndX = W.car.x; legEndZ = W.car.z; }
    }
    for (int i = 0; i < POP; i++) idx[i] = i;
    for (int i = 0; i < POP; i++) for (int j = i + 1; j < POP; j++) if (fit[idx[j]] < fit[idx[i]]) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    if (g >= 0) { float tmp[POP][NPAR], tmo[POP][MAXWP], tf[POP];   // элита — в начало
      for (int i = 0; i < ELITE; i++) { memcpy(tmp[i], par[idx[i]], sizeof tmp[i]); memcpy(tmo[i], off[idx[i]], sizeof tmo[i]); tf[i] = fit[idx[i]]; }
      for (int i = 0; i < ELITE; i++) { memcpy(par[i], tmp[i], sizeof tmp[i]); memcpy(off[i], tmo[i], sizeof tmo[i]); fit[i] = tf[i]; idx[i] = i; } }
  }
  bwLoad(&W, &curSnap); *nout = bestN; return bestN > 0;
}

// 30.09 (m1, дверь школы): прямая пешком свободна — забор не пересекается; земля через каждые 0.5 м: подъём не выше ступеньки
// Гомера (HOMER_STEP), спуск до 3 м; на высоте 0.5–1.5 м над землёй стены нет (столбик 0.15 м, как bwReachable)
static int footClear(float ax, float ay, float az, float bx, float bz) {
  const ColWorld *cw = bwCol(); float d = hypotf(bx - ax, bz - az), py = ay; int n = (int)(d / .5f) + 1;
  if (colCrossFence(cw, ax, az, bx, bz, 0, 0) || colCrossFence(cw, bx, bz, ax, az, 0, 0)) return 0;
  for (int k = 1; k <= n; k++) { float t = (float)k / n, x = ax + (bx - ax) * t, z = az + (bz - az) * t, g;
    if (!colGround(cw, x, z, py + HOMER_STEP, py - 3.f, &g, 0)) return 0;
    CarBox q; memset(&q, 0, sizeof q); q.x = x; q.z = z; q.fz = 1.f; q.hw = q.hl = .15f; q.y = g; q.lo = .5f; q.hi = 1.5f;
    colProbeCar(cw, &q); if (q.maxDepth > .05f) return 0;
    py = g; }
  return 1;
}
static int interiorPos(const MsnVec3 *p) { return p->y < -12.f; }   // комнаты оригинала лежат под городом (y ≈ -20): их нет в движке
static int footTarget(const GameMsn *m) {
  const MsnRuntime *rt = &m->rt; const char *d = m->marker.drawable ? m->marker.drawable : "";
  return rt->c_talk != 0 || strstr(d, "doorstar") != 0 || rt->obj == MSN_OBJ_BUYCAR || rt->obj == MSN_OBJ_BUYSKIN ||
         (!getenv("ALLRISK_INTERIOR_OLD") && (rt->obj == MSN_OBJ_INTERIOR || rt->obj == MSN_OBJ_GOOUTSIDE)) ||
         (rt->status == MSN_RUNNING && rt->obj == MSN_OBJ_COINS);   // ставка у Луи — пешком и △   // купить — пешком у продавца
}
// П1 (L1_ФИНИШ 30.09, m4): маркер — предмет «сломать» (delivery без рисунка, msn_run.c item_breakable: щитки PP_powerbox1..9).
// Как игрок: пешком вплотную и пинок □ (данные диска l1z6_powerbox: s1 HIT->s2 HIT->s3, машина их не ломает — VEHICLE_HIT нет).
// MISSIONER_NO_KICK=1 — без правила (контроль гейта: m4 не проходится)
static int breakTarget(const GameMsn *m) {
  static int off = -1; if (off < 0) off = getenv("MISSIONER_NO_KICK") != 0;
  return !off && m->rt.status == MSN_RUNNING && m->rt.obj == MSN_OBJ_DELIVERY && m->markerOn && !(m->marker.drawable && m->marker.drawable[0]);
}

// ------------------------------------------------------------------ правка 116: подъёмник (общее правило бота, по данным ANM1)
// Цель выше, чем Гомер достаёт с земли двумя прыжками (HOMER_JUMP_H + HOMER_JUMP2_H), а рядом объект уровня с действием по △
// (PlayAnim / ToggleOnOff), чей живой объём COL1 клип поднимает до досягаемости цели, — к его выключателю, △, на этот объём, ждать
// верх, двойной прыжок к цели. Какой объём поднимается, куда и за сколько — прогон клипа тем же aoStep на снимке мира (не
// координаты грузовика). Запись PS2 10:04–10:09 (m2, ингалятор на крыше Duff): △ у кнопки на борту, Гомер запрыгивает на ящики
// платформы, едет вверх, двойным прыжком — на крышу. MISSIONER_NO_LIFT=1 — без правила (контроль: m2 застревает на ингаляторе)
#ifdef BW_HAVE_AO
#define LIFT_REACH (HOMER_JUMP_H + HOMER_JUMP2_H)   // сколько Гомер набирает над опорой двумя прыжками
typedef struct { int obj, nv, vol[16]; float top0[16], top[16], xt[16], zt[16], tTop; } Lift;   // объёмы объекта: верх в кадре 0 / наибольший
static Lift lifts[8]; static int nlift = -1;
static float volTop(const AoVol *V, float *x, float *z) {   // верх живого объёма в мире (ящик — по проекциям полуосей на вертикаль)
  const ColWorld *cw = bwCol();
  if (V->kind == 0) { const ColBox *b = &cw->box[V->col]; *x = b->c[0]; *z = b->c[2];
    return b->c[1] + fabsf(b->ax[1]) * b->h[0] + fabsf(b->ay[1]) * b->h[1] + fabsf(b->az[1]) * b->h[2]; }
  const ColCyl *c = &cw->cyl[V->col]; *x = c->c[0]; *z = c->c[2];
  return c->c[1] + fabsf(c->axis[1]) * c->halfLen + c->r * sqrtf(fmaxf(0.f, 1.f - c->axis[1] * c->axis[1]));
}
static float volReach(const AoVol *V) {   // полуразмер верха по плоскости (стоит ли Гомер на нём)
  const ColWorld *cw = bwCol();
  if (V->kind == 0) { const ColBox *b = &cw->box[V->col]; return fmaxf(b->h[0], fmaxf(b->h[1], b->h[2])) + .2f; }
  return cw->cyl[V->col].r + .2f;
}
static void liftScan(void) {   // один раз на процесс, на мире новой игры: △ каждому выключателю на снимке, клип до конца, снимок назад
  nlift = 0; if (!W.haveAo || getenv("MISSIONER_NO_LIFT")) return;
  if (getenv("MISSIONER_AO_LIST")) for (unsigned k = 0; k < W.ao.nobj; k++) { const AoObj *o = &W.ao.obj[k];   // служебно: все объекты с действием (вертикальный доступ, №519)
    printf("AO %u action %d trig %d nfr %u fps %.1f nvol %u at %.1f %.1f %.1f h %.1f %.1f %.1f\n", k, o->action, o->trig, o->nfr, o->fps, o->nVol, o->c[0], o->c[1], o->c[2], o->h[0], o->h[1], o->h[2]); }
  for (unsigned k = 0; k < W.ao.nobj && nlift < 8; k++) {
    const AoObj *o = &W.ao.obj[k]; if ((o->action != AO_PLAYANIM && o->action != AO_TOGGLE) || !o->nVol || o->nVol > 16) continue;
    bwSave(&W, &curSnap);
    const SpRun *r = &W.sp.run[o->firstInst]; float px = r->x, py = r->y, pz = r->z, fps = o->fps > 0.f ? o->fps : 30.f, x, z;
    Lift L; memset(&L, 0, sizeof L); L.obj = (int)k; L.nv = o->nVol;
    for (int v = 0; v < L.nv; v++) { L.vol[v] = o->firstVol + v; L.top[v] = L.top0[v] = volTop(&W.ao.vol[L.vol[v]], &L.xt[v], &L.zt[v]); }
    W.ao.want = (int)k; aoStep(&W.ao, &W.sp, (ColWorld *)bwCol(), px, py, pz, 1, 1, 0.f);   // △ этому объекту (как игрок в его объёме)
    for (float t = 0.f; t < (o->nfr > 1 ? (o->nfr - 1) / fps : 0.f) + .5f; t += 1.f / 30.f) {
      aoStep(&W.ao, &W.sp, (ColWorld *)bwCol(), px, py, pz, 1, 0, 1.f / 30.f);
      for (int v = 0; v < L.nv; v++) { float y = volTop(&W.ao.vol[L.vol[v]], &x, &z);
        if (y > L.top[v] + .01f) { L.top[v] = y; L.xt[v] = x; L.zt[v] = z; if (L.top[v] - L.top0[v] > 1.f) L.tTop = fmaxf(L.tTop, t); }
        if (getenv("MISSIONER_LIFT_TRACE") && v == 0 && ((int)(t * 30.f + .5f)) % 6 == 0) printf("    клип %u t=%.1f объём %d верх %.2f центр %.2f %.2f\n", k, t, L.vol[v], y, x, z); }
    }
    bwLoad(&W, &curSnap);
    int rise = 0; for (int v = 0; v < L.nv; v++) rise |= L.top[v] - L.top0[v] > 1.f;   // клип поднимает объём больше чем на 1 м
    if (rise) lifts[nlift++] = L;
    if (verbose) for (int v = 0; v < L.nv; v++) printf("  подъёмник? объект %u объём %d: верх %.2f -> %.2f, центр %.1f %.1f%s\n", k, L.vol[v], L.top0[v], L.top[v], L.xt[v], L.zt[v],
                                                       rise ? "  (поднимает)" : "");
  }
}
// подъёмник для цели p: Гомер с земли не достаёт (p.y выше ступней больше LIFT_REACH), а с верха одного из объёмов в верхней
// точке — да, и тот верх не дальше 8 м от цели по плоскости; Гомер ближе 40 м. -> номер в lifts или -1
static int liftRideOk(const Lift *L, int v, const MsnVec3 *p) { return p->y - L->top[v] <= LIFT_REACH - .3f && L->top[v] - p->y < 3.f && hypotf(L->xt[v] - p->x, L->zt[v] - p->z) < 8.f; }
static int liftFor(const MsnVec3 *p) {
  if (nlift <= 0 || p->y - W.homer.y <= LIFT_REACH || hypotf(p->x - W.homer.x, p->z - W.homer.z) > 40.f) return -1;
  for (int i = 0; i < nlift; i++) for (int v = 0; v < lifts[i].nv; v++) if (liftRideOk(&lifts[i], v, p)) return i;
  return -1;
}
// ход правила: 0 к выключателю и △ · 1 наверх по объёмам (ступенями: на самый высокий в одном прыжке, второй прыжок у верха) ·
// 2 едет, пока клип не довёл до верхней точки · 3 двойной прыжок к цели · 4 наверху: дальше обычный шаг к цели. Сорвался / клип
// кончился — снова с выключателя (не больше 4 кругов). Упёрся (1 с без сдвига к точке на 0.3 м) — прыжок к ней, как игрок
static struct { int key, li, phase, cycles; float t, gx, gz, gd, stuckT; } LS = {-1, -1, 0, 0, 0.f, 0.f, 0.f, 0.f, 0.f};
static void stickTo(float x, float z, unsigned b) {   // стик к точке относительно камеры (как шаг пешком ниже); упёрся — прыжок
  float d = hypotf(x - W.homer.x, z - W.homer.z);
  if (fabsf(x - LS.gx) + fabsf(z - LS.gz) > .5f || d < LS.gd - .3f) { LS.gx = x; LS.gz = z; LS.gd = d; LS.stuckT = 0.f; }
  else if ((LS.stuckT += BW_DT) > 1.f && W.homer.grounded && d > .6f) { b |= GP_CROSS; LS.stuckT = 0.f; LS.gd = d; }
  float r = atan2f(x - W.homer.x, z - W.homer.z) - W.homer.camYaw; step(b, -sinf(r), -cosf(r));
}
static int onVol(const Lift *L, int v) {   // Гомер стоит на верхе объёма v
  float x, z, top = volTop(&W.ao.vol[L->vol[v]], &x, &z);
  return W.homer.grounded && fabsf(W.homer.y - top) < .35f && hypotf(x - W.homer.x, z - W.homer.z) < volReach(&W.ao.vol[L->vol[v]]);
}
static int liftRule(const MsnVec3 *p, int key) {
  if (LS.key != key) { LS.key = key; LS.li = -1; LS.phase = 0; LS.cycles = 0; }
  if (LS.li < 0) { if (LS.cycles >= 4 || (LS.li = liftFor(p)) < 0) return 0; LS.phase = 0; LS.t = 0.f; }
  const Lift *L = &lifts[LS.li]; const AoObj *o = &W.ao.obj[L->obj]; const AoRun *run = &W.ao.run[L->obj];
  float fps = o->fps > 0.f ? o->fps : 30.f;
  LS.t += BW_DT;
  if (verbose && W.st.frames % 30 == 0) printf("    подъёмник ход %d t=%.1f Гомер %.2f %.2f %.2f земля %d кадр %.0f играет %d хочет %d\n",
                                               LS.phase, LS.t, W.homer.x, W.homer.y, W.homer.z, W.homer.grounded, run->f, run->playing, aoWantsAction(&W.ao));
  int on = -1; for (int v = 0; v < L->nv; v++) if (onVol(L, v)) on = v;
  switch (LS.phase) {
    case 0: {   // к выключателю (центр его объёма); в объёме — △, как игрок по подсказке
      if (run->playing && run->f / fps < L->tTop - 1.f) { LS.phase = 1; LS.t = 0.f; break; }   // уже едет (прошлый △) — сразу наверх
      if (aoWantsAction(&W.ao) == L->obj && !run->playing) { step(GP_TRIANGLE, 0, 0); LS.phase = 1; LS.t = 0.f; return 1; }
      float d = hypotf(o->c[0] - W.homer.x, o->c[2] - W.homer.z);
      if (d < .5f) { step(W.homer.grounded && LS.t > 1.f ? GP_CROSS : 0, 0, 0); if (LS.t > 1.f) LS.t = 0.f; return 1; }   // выключатель выше рук — прыжок
      stickTo(o->c[0], o->c[2], d > 3.f ? GP_CIRCLE : 0); return 1; }
    case 1: {   // наверх ступенями: объём, с верха которого цель достаётся, — конец; иначе самый высокий в одном прыжке
      if (!run->playing || LS.t > 8.f) { LS.phase = 0; LS.t = 0.f; LS.cycles++; break; }
      if (on >= 0 && liftRideOk(L, on, p)) { LS.phase = 2; LS.t = 0.f; break; }
      int nx = -1; float ny = -1e9f, cy = on >= 0 ? volTop(&W.ao.vol[L->vol[on]], &(float){0}, &(float){0}) : W.homer.y;
      for (int v = 0; v < L->nv; v++) { float x, z, y = volTop(&W.ao.vol[L->vol[v]], &x, &z);
        if (v != on && y > cy + .25f && y - cy <= HOMER_JUMP_H - .3f && y > ny) { ny = y; nx = v; } }
      if (nx < 0) for (int v = 0; v < L->nv; v++) { float x, z, y = volTop(&W.ao.vol[L->vol[v]], &x, &z);   // ниже нет — ближний выше (двойным)
        if (v != on && y > cy + .25f && (nx < 0 || y < ny)) { ny = y; nx = v; } }
      if (nx < 0) { step(0, 0, 0); return 1; }
      float vx, vz; volTop(&W.ao.vol[L->vol[nx]], &vx, &vz); float dv = hypotf(vx - W.homer.x, vz - W.homer.z);
      unsigned b = 0;
      if (W.homer.grounded && dv < 1.6f) b = GP_CROSS;                                                                  // прыжок на него
      else if (!W.homer.grounded && W.homer.jumps == 1 && W.homer.vy < .5f && W.homer.y < ny + .2f) b = GP_CROSS;       // второй у верха
      stickTo(vx, vz, b); return 1; }
    case 2: {   // едет: держаться середины верха, пока клип не довёл до верхней точки
      if (on < 0 && W.homer.grounded) { LS.phase = 1; LS.t = 0.f; break; }   // соскользнул — снова наверх
      if (!run->playing && LS.t > .5f) { LS.phase = 0; LS.t = 0.f; LS.cycles++; break; }
      if (run->f / fps >= L->tTop - .05f) { LS.phase = 3; LS.t = 0.f; break; }
      float vx, vz; if (on >= 0) volTop(&W.ao.vol[L->vol[on]], &vx, &vz); else { vx = W.homer.x; vz = W.homer.z; }
      if (hypotf(vx - W.homer.x, vz - W.homer.z) > .3f) stickTo(vx, vz, 0); else step(0, 0, 0);
      return 1; }
    case 3: {   // двойной прыжок к цели: первый с верха, второй в высшей точке первого
      if (LS.t > .1f && W.homer.grounded) { LS.phase = W.homer.y > p->y - LIFT_REACH ? 4 : 0; if (!LS.phase) LS.cycles++; LS.t = 0.f; break; }
      unsigned b = (LS.t < BW_DT * 1.5f) ? GP_CROSS : (!W.homer.grounded && W.homer.jumps == 1 && W.homer.vy < .3f) ? GP_CROSS : 0;
      float r = atan2f(p->x - W.homer.x, p->z - W.homer.z) - W.homer.camYaw; step(b, -sinf(r), -cosf(r)); return 1; }
    default:    // наверху: сорвался вниз — снова с выключателя; иначе обычный шаг к цели
      if (W.homer.grounded && W.homer.y < p->y - LIFT_REACH) { LS.phase = 0; LS.t = 0.f; LS.cycles++; break; }
      return 0;
  }
  static int depth; if (depth > 4) { step(0, 0, 0); return 1; }   // ход сменился в этом кадре — сразу следующий (не глубже 4)
  depth++; int r = liftRule(p, key); depth--; return r;
}
#endif

// 29.09: водитель по полосам (viewer/msncar.c mcBotInput — тот же, что ведёт машины миссий): к цели машиной по дорогам уровня
// (traffic.shr), таран машины-цели (destroy, dump до выпадения груза), уход от погони (losetail) — точка по дорогам в 250–450 м
// от игрока, дальняя от преследователя. MISSIONER_OLD_NAV=1 — прежний навигатор (граф roads-l<N>.bin и эволюция отрезка)
#define RACE_NAV_GAIN 1.15f   // облако 05.10 (sr3): навигатор, если путь по полосам длиннее его на 15 % и больше
static MsnVec3 rcP = {1e9f, 0.f, 0.f}; static int rcPick = -1;   // выбор водителя точки гонки (сброс — с каждым сбросом маршрута)
static void rcReset(void) { rcP.x = 1e9f; rcPick = -1; }
static float navPathLen(float ax, float ay, float az, float bx, float by, float bz) {   // по всем узлам A* (без прореживания); < 0 — пути нет
  int s = nearestNode(ax, ay, az), g = nearestNode(bx, by, bz), path[1024], n = astar(s, g, path, 1024);
  if (n <= 0) return -1.f;
  float L = hypotf(node[path[0]].x - ax, node[path[0]].z - az) + hypotf(bx - node[path[n - 1]].x, bz - node[path[n - 1]].z);
  for (int i = 1; i < n; i++) L += hypotf(node[path[i]].x - node[path[i - 1]].x, node[path[i]].z - node[path[i - 1]].z);
  return L;
}
static McCar botDrv; static float thrAcc, brkAcc; static int laneBot = -1;
// след машины-цели (follow, dump/destroy издали): бот едет её же дорогой — точки через 6 м, по очереди (ближе 12 м — следующая);
// иначе путь к месту, где она сейчас, перекладывается каждые 5 м и уводит в другие кварталы (m5: фургон уходит за 250 м)
static float trail[512][3]; static int trailN, trailHead;
static void trailReset(void) { trailN = trailHead = 0; }
static void trailPush(const Car *k) {
  if (trailN) { const float *l = trail[(trailHead + trailN - 1) % 512]; if (hypotf(l[0] - k->x, l[2] - k->z) < 6.f) return; }
  if (trailN == 512) { trailHead = (trailHead + 1) % 512; trailN--; }
  float *q = trail[(trailHead + trailN) % 512]; q[0] = k->x; q[1] = k->y; q[2] = k->z; trailN++;
}
static void trailTrim(float px, float pz) {   // пройденные точки следа (ближе 12 м и ближайшая) — прочь
  int best = -1; float bd = 1e30f;
  for (int i = 0; i < trailN && i < 40; i++) { const float *q = trail[(trailHead + i) % 512]; float d = hypotf(q[0] - px, q[2] - pz); if (d < bd) { bd = d; best = i; } }
  if (best > 0 && bd < 20.f) { trailHead = (trailHead + best) % 512; trailN -= best; }
  while (trailN > 1) { const float *q = trail[trailHead]; if (hypotf(q[0] - px, q[2] - pz) >= 8.f) break; trailHead = (trailHead + 1) % 512; trailN--; }
}
static int trailPath(float (*out)[3], int cap) {   // след вперёд (до cap точек)
  int n = trailN < cap ? trailN : cap;
  for (int i = 0; i < n; i++) { const float *q = trail[(trailHead + i) % 512]; out[i][0] = q[0]; out[i][1] = q[1]; out[i][2] = q[2]; }
  return n;
}
static int escapePoint(float px, float pz, float cx, float cz, MsnVec3 *out) {
  const Traffic *t = bwTraffic(); if (!t || !t->hd) return 0;
  float best = -1.f;
  for (unsigned r = 0; r < t->hd->nRoad; r++) {
    if (t->road[r].flags & 1) continue;
    const TrPt *P = t->pt + t->road[r].firstPt;
    for (int k = 0; k < t->road[r].nPt; k++) {
      float d = hypotf(P[k].x - px, P[k].z - pz); if (d < 250.f || d > 450.f) continue;
      float e = hypotf(P[k].x - cx, P[k].z - cz); if (e > best) { best = e; out->x = P[k].x; out->y = P[k].y; out->z = P[k].z; }
    }
  }
  return best > 0.f;
}
static void laneStep(const MsnVec3 *p, int ram, const char *target, int follow) {
  McEnv e; memset(&e, 0, sizeof e); e.w = bwCol(); e.tr = (Traffic *)bwTraffic(); e.driving = 1; e.ppos[0] = e.ppos[2] = 1e6f;
  botDrv.car = W.car; botDrv.mode = ram ? MC_CHASE : MC_RACE; botDrv.hasSk = 0;   // игрок едет и по срезам (дороги диска с флагом shortcut: у гонки sr1 — западная сторона круга)
  // 30.09 (часть race): машина игрока не толкает трафик и стоящие, как соперник гонки (им стены не бьют, игроку — любой удар):
  // урон gr1 0.38 ещё до старта гонки (толкался у Луи), провалы «damage». Таран — толкать; MISSIONER_PUSHY=1 — прежний водитель
  botDrv.careful = !ram && !getenv("MISSIONER_PUSHY") && !fastDrive;
  snprintf(botDrv.ignore, sizeof botDrv.ignore, "%s", ram && target ? target : "");
  float g[3] = {p->x, p->y, p->z}; CarInput in;
  static float tp[60][3]; int ntp = 0;
  // follow == 2 — слежка: следом, но не ближе FOLLOW_GAP к ней (не таранить: своя машина разобьётся о фургон); 1 — таран издали
  enum { FOLLOW_GAP = 15 };
  if (follow) { trailTrim(W.car.x, W.car.z); ntp = trailPath(tp, 59);
    if (follow == 2) while (ntp > 0 && hypotf(tp[ntp - 1][0] - g[0], tp[ntp - 1][2] - g[2]) < FOLLOW_GAP) ntp--;
    else if (ntp) { memcpy(tp[ntp], g, sizeof g); ntp++; } }
  if (follow == 2 && hypotf(W.car.x - g[0], W.car.z - g[2]) < FOLLOW_GAP) { memset(&in, 0, sizeof in); in.brake = fabsf(W.car.v) > .5f ? 1.f : 0.f; }
  else if (ntp >= 2) { mcBotInputPath(&W.msn.cars, &botDrv, &e, (const float (*)[3])tp, ntp, BW_DT, &in); botDrv.nroute = 0; }   // её следом
  else mcBotInput(&W.msn.cars, &botDrv, &e, g, BW_DT, &in);
  // правка 347: таран толчками, как игрок. Удар засчитывается от 1 м/с сближения (mcUpdate: hitSpeed > 1, предмет — раз в 1.5 с);
  // бот в упоре за фургоном на газу толкал его 5–10 с со сближением 0.1–0.7 м/с (замер m6 05.10: зазор кузовов 0.1 м, ни одного
  // предмета). Упор дольше 0.4 с без удара — отпустить газ на 0.5 с (отстать на 1–2 м) и снова в него. MISSIONER_RAM_OLD=1 — прежнее
  static float pushT, backT;
  if (ram && target && !mcBotRamOld) {
    const McCar *tc = 0; for (int i = 0; i < W.msn.cars.n; i++) if (W.msn.cars.c[i].on && !strcmp(W.msn.cars.c[i].name, target)) tc = &W.msn.cars.c[i];
    int touch = 0;
    if (tc) { CarBox A, Bx; carBox(&W.car, &A); carBox(&tc->car, &Bx); float dx = Bx.x - A.x, dz = Bx.z - A.z, a = dx * A.fx + dz * A.fz, l = -dx * A.fz + dz * A.fx;
      touch = a > 0.f && a - A.hl - Bx.hl < .4f && fabsf(l) < A.hw + Bx.hw; }
    if (W.car.hitSpeed > 1.f || !touch) pushT = 0.f; else pushT += BW_DT;
    if (pushT > .4f) { backT = .5f; pushT = 0.f; }
    if (backT > 0.f) { backT -= BW_DT; in.throttle = 0.f; in.brake = .3f; }
  } else pushT = backT = 0.f;
  // быстрый водитель (04.11): водитель полос берёт скорость поворотов консервативно (vt 7–20 м/с при предельных 35: sr1 L2 вдвое медленнее времени). Не тормозить,
  // пока перебор над желаемой скоростью меньше 35 % + 2 м/с, газ не слабее 0.6; ручник в крутых поворотах уже даёт mcBotInput. MISSIONER_FAST=0 — отключить
  if (fastDrive && !ram && !follow && in.brake > 0.f && W.car.v < botDrv.vt * (fastK > 0.f ? fastK : W.msn.rt.obj == MSN_OBJ_RACE ? 3.0f : 1.8f) + 2.f) { in.brake = 0.f; in.throttle = fmaxf(in.throttle, .6f); }
  unsigned b = 0; thrAcc += in.throttle; if (thrAcc >= 1.f) { b |= GP_R; thrAcc -= 1.f; }   // газ/тормоз пульта — нажатия: доля кадров
  brkAcc += in.brake; if (brkAcc >= 1.f) { b |= GP_L; brkAcc -= 1.f; }
  if (in.handbrake > .5f) b |= GP_SQUARE;
  if (verbose && W.st.frames % 60 == 0) {
    printf("    полосы t=%.0f %.1f %.1f %.1f v %.1f vt %.1f -> %.1f %.1f  путь %d/%d вне %.1f %s", W.st.frames / 60.f, W.car.x, W.car.y, W.car.z, W.car.v, botDrv.vt, p->x, p->z,
           botDrv.seg, botDrv.nroute, botDrv.offLane, ram ? "таран " : "");
    printf(" k%d g%.1f dmg %.2f", botDrv.dbgKind, botDrv.dbgGap < 1e8f ? botDrv.dbgGap : -1.f, W.car.damage);
    for (int i = 0; i < W.msn.cars.n; i++) { const McCar *c = &W.msn.cars.c[i]; if (c->on) printf(" | %s %s %.0f %.0f v %.1f wp %d fin %d dmg %.2f k%d g%.1f vt %.1f seg %d/%d", c->name, mcModeName[c->mode], c->car.x, c->car.z, c->car.v, c->wp, c->finished, c->car.damage, c->dbgKind, c->dbgGap < 1e8f ? c->dbgGap : -1.f, c->vt, c->seg, c->nroute); }
    if (verbose > 1) {   // стоящие машины рядом с машинами миссии (кто их держит, k9)
      const Traffic *T = (const Traffic *)bwTraffic();
      for (int i = 0; T && i < W.msn.cars.n; i++) { const McCar *c = &W.msn.cars.c[i]; if (!c->on) continue;
        for (int j = 0; j < T->npark; j++) { const Car *pk = &T->park[j].car; float dx = pk->x - c->car.x, dz = pk->z - c->car.z;
          if (dx * dx + dz * dz < 15.f * 15.f) printf(" [%s: стоящая %d %.1f %.1f курс %.2f сост %d]", c->name, j, pk->x, pk->z, pk->h, T->park[j].state); } }
    }
    printf("\n");
  }
  step(b, in.steer, 0);
}
typedef struct { const char *result, *why; float t, margin, damage; int stages, attempts, frames; float sx, sz; int wall; const char *cls; int nav, second, raceNavPick, tp, waitjump; } Out;
// попыток стадии после первой: 3; у гонки с соперниками — 7 (игрок тоже переигрывает гонку; водителей по попыткам чередуем)
static int maxTries(const MsnRuntime *rt) {
#ifdef ALLRISK_MISSION_LIBRARY
  (void)rt; return 2;  // Three runs including first, per Codex-3 handoff.
#else
  return rt->obj == MSN_OBJ_RACE ? 7 : 3;
#endif
}
// второй независимый водитель (Т1.5): со снимка s, до трёх попыток с другими seed, объезд места, где остался первый; -> доехал ли до p
static int secondDriver(const BotSnap *s, MsnVec3 p, float secs, float avoidX, float avoidZ) {
  if (getenv("MISSIONER_NO_SECOND")) return 0;
  bwLoad(&W, s); int keepW = legWeak, keepA = navAvoidN; uint32_t keepR = lrs; legWeak = 0; lrs = 0x9e3779b9u;
  float sx = gameOnFoot(&W.G) ? W.homer.x : W.car.x, sz = gameOnFoot(&W.G) ? W.homer.z : W.car.z;
  if (hypotf(avoidX - sx, avoidZ - sz) > 20.f) navAvoid(avoidX, avoidZ);   // объезд места первого, если он уехал от старта
  static const uint32_t seeds[3] = {0x9e3779b9u, 0x85ebca6bu, 0xc2b2ae35u}; int nb = 0, ok = 0;   // три независимых попытки
  for (int k = 0; k < 3 && !ok; k++) { bwLoad(&W, s); lrs = seeds[k]; ok = evolveLeg(p, secs, &nb); }
  legWeak = keepW; lrs = keepR; navAvoidN = keepA; bwLoad(&W, s); return ok;
}
// 03.10 (разбор ночи 02.10, bm1 This Old Shanty): бонус-миссию игрок начинает у её персонажа, приехав туда на своей машине; бот
// начинает с новой игры у дома Симпсонов. У миссии со вступлением (разговор с Клетусом) старт машину не переносит (game_msn.h:
// SetMissionReset* — при старте основной части без вступления и при повторе) — машина оставалась у дома, в 545 м от Гомера: этап
// «сядь в машину» бот шёл пешком 96 с. Правило стенда, не игры: до старта такой миссии машина (без Гомера) — на локаторе
// SetMissionResetPlayerInCar основной части (там её ставит повтор миссии). MISSIONER_BONUS_CAR_OLD=1 — как раньше (контроль)
static void bonusCarToStart(int mi) {
  if (getenv("MISSIONER_BONUS_CAR_OLD")) return;
  const MsnFile *F = &W.msn.f; const MsnMission *ms = &F->missions[mi];
  if (ms->intro == MSN_NONE16 || ms->main == MSN_NONE16) return;
  const MsnPhase *P = &F->phases[ms->main];
  for (unsigned c = 0; c < P->setup_count; c++) { const MsnCmd *C = &F->cmds[P->setup_first + c]; const MsnLoc *l;
    if (C->op != MSN_OP_SETMISSIONRESETPLAYERINCAR || !(l = msn_arg_loc(F, C, 0))) continue;
    float h = (l->flags & 1) ? mcCarHeading(l->heading) : W.car.h;
    carPlace(&W.car, (ColWorld *)bwCol(), l->pos[0], l->pos[1], l->pos[2], h); W.props.havePreviousCar = 0;
    if (verbose) printf("  бонус-миссия: машина у её старта %s %.1f %.1f %.1f (Гомер %.1f %.1f)\n", msn_str(F, l->name), l->pos[0], l->pos[1], l->pos[2], W.homer.x, W.homer.z);
    return; }
}
// Лестница, последняя ступень (проект ботов 04.11 §3): перенос на узел дорог не ближе 30 м от места застревания, по пути к цели; < 0 — переносить некуда.
// Всегда с пометкой: строка TP с местом (код места считает отчёт), итог миссии «пройдена с переносом», класс «игра/бот: разобрать».
static int roomGridOn = -1;   // 05.11: сетка пути (footpath) и в комнатах; MISSIONER_ROOMGRID=1 — включить (не доказано: fpPlan в Moe's возвращает 0 точек)
static float stillT, sideT;   // пешком: время без сдвига и шаг вбок (общие с блоком footnav)
static float tpFrom[TP_MAX][2]; static int tpN;   // откуда уже переносили: не возвращать туда же (L2 m2: пешеход мотался H10-2 <-> H8-7)
// 06.11 (Роб: «обучи ботов… чтобы правки не были как снег на голову»): в гонке оба водителя застревали у бордюров и домов по пути к точке (21 заезд TEST91 — «игра»,
// в эмуляторе на тех же местах машина уезжает: открытые перекрёстки) — теперь и в гонке последняя ступень — перенос по пути навигатора к точке (итог «пройдена с переносом»,
// место — в списке переносов), а «игра» остаётся за настоящими тупиками. MISSIONER_RACE_NO_TP=1 — как было (в гонке переноса нет)
static int teleportAhead(int mi, MsnVec3 goal, int foot, float t, const MsnRuntime *rt) {
  float fx = foot ? W.homer.x : W.car.x, fy = foot ? W.homer.y : W.car.y, fz = foot ? W.homer.z : W.car.z;
  if (foot && W.room >= 0) {   // Гомер в комнате (y около −20 — комнаты лежат под городом, это не провал сквозь пол): перенос на локатор выхода комнаты
    float t3[3]; bwRoomExit(W.room, t3);
    for (int i = 0; i < tpN; i++) if (hypotf(t3[0] - tpFrom[i][0], t3[2] - tpFrom[i][1]) < 3.f) return -1;
    if (tpN < TP_MAX) { tpFrom[tpN][0] = fx; tpFrom[tpN][1] = fz; tpN++; }
    printf("TP\t%d\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\t%.1f\t%d.%d\n", mi, fx, fy, fz, t3[0], t3[1], t3[2], foot, t, rt->phase, rt->stage);
    homerPlace(&W.homer, bwCol(), t3[0], t3[1] + .5f, t3[2], 0.f);
    for (int k = 0; k < 10; k++) bwStep(&W, 0, 0, 0);
    return 0;
  }
  P3 q[MAXWP]; int nq = navPlan(fx, fy, fz, goal.x, goal.y, goal.z, q, MAXWP), pick = -1;
  for (int i = 0; i < nq; i++) if (hypotf(q[i].x - fx, q[i].z - fz) >= 30.f) { pick = i; break; }
  if (pick < 0) return -1;
  float tx = q[pick].x, ty = q[pick].y, tz = q[pick].z, nx = pick + 1 < nq ? q[pick + 1].x : goal.x, nz = pick + 1 < nq ? q[pick + 1].z : goal.z;
  for (int i = 0; i < tpN; i++) if (hypotf(tx - tpFrom[i][0], tz - tpFrom[i][1]) < 20.f) return -1;   // перенос привёл бы в место прежнего застревания: честное «застрял»
  if (tpN < TP_MAX) { tpFrom[tpN][0] = fx; tpFrom[tpN][1] = fz; tpN++; }
  float h = atan2f(nx - tx, nz - tz);
  printf("TP\t%d\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%.1f\t%d\t%.1f\t%d.%d\n", mi, fx, fy, fz, tx, ty, tz, foot, t, rt->phase, rt->stage);
  if (foot) homerPlace(&W.homer, bwCol(), tx, ty + .5f, tz, h);
  else { carPlace(&W.car, (ColWorld *)bwCol(), tx, ty, tz, h); W.props.havePreviousCar = 0; }
  for (int k = 0; k < 10; k++) bwStep(&W, 0, 0, 0);   // осесть (кадры вне записи ввода: повтор mN.in пометки переноса не воспроизводит)
  return pick;
}
static Out playMission(int mi, float secs) {
  traceEnabled = 0; traceMission = mi; tracePhase = traceStage = -99;
  rcReset(); mechReset();
  Out o = {"застрял", "", 0, -1, 0, 0, 1, 0, 0, 0, -1, "игра", 0, 0};
  bwFree(&W); bwNew(&W); nrec = 0;
#ifdef BW_HAVE_AO
  if (nlift < 0) liftScan();   // правка 116: какие объекты с △ поднимают свой объём (мир новой игры, снимок назад)
#endif
  GameMsn *M = &W.msn; MsnRuntime *rt = &M->rt;
  // 29.09: кошелёк бота на старте миссии — --coins N (гейт tests/shop: m3 «купи Plow King у Барни» с 0 монет обязан застрять),
  // иначе MISSIONER_COINS (по умолчанию 250): игрок к этим миссиям уровня 1 набирает монеты по городу, бот их не собирает;
  // нужны ставке gr1 (20) и покупке Plow King (150) — допущение стенда, не игры
  if (W.haveCoins) W.coins.collected = startCoins >= 0 ? startCoins : getenv("MISSIONER_COINS") ? atoi(getenv("MISSIONER_COINS")) : 250;
#if !defined(ALLRISK_WALLET_OLD)   // 04.11: и для обычного миссионера (L2–L7: покупки дороже кошелька L1 в 250); MISSIONER_COINS/--coins дают точное число
  // Starting fixture, not free in-game coins: later-level purchase stages cost
  // more than the legacy L1 wallet. Derive required budget from packet BindReward.
  // Explicit --coins/MISSIONER_COINS keep their exact value for negative controls.
  if (W.haveCoins && startCoins < 0 && !getenv("MISSIONER_COINS")) {
    int need = 0;
    const MsnMission *mission = &M->f.missions[mi];
    for (int ph = 0; ph < 2; ph++) {
      unsigned pi = ph ? mission->main : mission->intro;
      if (pi == MSN_NONE16) continue;
      const MsnPhase *phase = &M->f.phases[pi];
      for (unsigned k = 0; k < phase->stage_count; k++) {
        const MsnStage *st = &M->f.stages[phase->first_stage + k];
        if (st->obj == MSN_OBJ_BUYCAR || st->obj == MSN_OBJ_BUYSKIN) {
          int it = shopFind(&M->shop, msn_str(&M->f, st->obj_arg));
          if (it >= 0) need += M->shop.item[it].price;
        }
      }
    }
    if (W.coins.collected < need) W.coins.collected = need;
  }
#endif
#ifdef ALLRISK_MISSION_LIBRARY
  printf("WALLET\t%d\t%d\tstarting_fixture\n", mi, W.coins.collected); fflush(stdout);
#endif
  if (fromMain && M->f.missions[mi].main != MSN_NONE16) { rt->cp_phase = 1; rt->cp_stage = 0; msn_start_mission(rt, mi, 1); }   // с чекпоинта основной части
  else if (M->f.missions[mi].flags & (MSN_MF_BONUS | MSN_MF_STREETRACE | MSN_MF_GAMBLE | MSN_MF_BONUSMISSION)) { bonusCarToStart(mi); msn_start_bonus(rt, mi); }
  else msn_start_mission(rt, mi, 0);
  P3 q[MAXWP]; int nq = 0, seg = 0; MsnVec3 aim = {1e9f, 0, 1e9f};
  float par[NPAR]; memcpy(parBase, PAR0, sizeof parBase);
  if (seedSet) for (int k = 0; k < NPAR; k++) parBase[k] = fmaxf(PARLO[k], fminf(PARHI[k], PAR0[k] * (1.f + .12f * lgauss())));   // вариант: другие числа водителя (seed — ось ночи)
  memcpy(par, parBase, sizeof par);
  float stageD0 = -1.f, stageMin = 1e9f;   // расстояние до цели стадии на старте и лучшее за стадию (доля пути до провала)
  int tp = 0; tpN = 0; float waitT = 0.f; int waitJump = 0;   // лестница: переносы и ожидание события
  { const char *fe = getenv("MISSIONER_FAST"); fastDrive = fe && *fe == '1'; }
  { const char *fk = getenv("MISSIONER_FASTK"); if (fk) fastK = (float)atof(fk); }
  int lastStage = -99, frames = (int)(secs * 60), tries = 0; float best = 1e9f, bestT = 0;
  float resetsT = 0, shift = 0, slowT = 0, backT = 0, backAx = 0; int resets = 0, evolved = 0; navAvoidN = 0;
  float escT = 0.f; MsnVec3 esc = {0, 0, 0}; memset(&botDrv, 0, sizeof botDrv); thrAcc = brkAcc = 0.f;
  if (laneBot < 0) laneBot = !getenv("MISSIONER_OLD_NAV") && bwTraffic();
  mcBotRamOld = getenv("MISSIONER_RAM_OLD") != 0;     // правка 347: прежний бот-таран (красный гейта tests/missions/start_shift.sh)
  mcBotPassOld = getenv("MISSIONER_PASS_OLD") != 0;   // правка 347: прежний бот — не обгоняет едущих (красный того же гейта)
  cpKey = -1; cpTries = 0;
  // правка 347: MISSIONER_DELAY=S — бот стоит S с до первого ввода (трафик и мир идут): миссия стартует позже относительно трафика,
  // как при другом радиусе разговора (345 сдвинул старт погони m6 на 0,5 с). Гейт tests/missions/start_shift.sh: исход стадий с
  // машинами миссий не должен зависеть от этого сдвига
  { const char *ds = getenv("MISSIONER_DELAY"); int n = ds ? (int)(atof(ds) * 60.f + .5f) : 0;
    for (int k = 0; k < n; k++) step(0, 0, 0); if (n && verbose) printf("  старт позже на %.2f с (MISSIONER_DELAY)\n", n / 60.f); }
  traceEnabled = 1; traceCurrentStage();
  bwSave(&W, &stageSnap); int stageRec = nrec;   // запись ввода откатывается вместе с миром: повтор mN.in = ровно сыгранное
  for (int f = 0; f < frames; f++) {
    if ((rt->completed >> mi) & 1u) { o.result = tp ? "пройдена с переносом" : "пройдена"; o.cls = tp ? "игра/бот: разобрать (перенос)" : "—"; o.tp = tp; o.waitjump = waitJump; o.t = f / 60.f; o.damage = W.car.damage; o.frames = f;
      if (verbose) printf("  пройдена t=%.1f: таймер %.1f%s, монет %d\n", W.st.frames / 60.f, rt->remaining, rt->timer_up ? " (вверх)" : "", W.coins.collected);   // gr1: выигрыш ставки
      return o; }
    int stageKey = rt->phase * 1000 + rt->stage;
    if (stageKey != lastStage && rt->phase == rt->cp_phase && rt->stage == rt->cp_stage && stageKey != cpKey) { bwSave(&W, &cpSnap); cpRec = nrec; cpKey = stageKey; }
    if (stageKey != lastStage) { stageD0 = -1.f; stageMin = 1e9f; lastStage = stageKey; o.stages++; bwSave(&W, &stageSnap); stageRec = nrec; tries = 0; best = 1e9f; bestT = f / 60.f; aim.x = 1e9f; evolved = 0; botDrv.nroute = 0; rcReset(); escT = 0.f; trailReset();
      traceCurrentStage();
      if (verbose) printf("  стадия %d.%d t=%.1f: dest %d talk %d dialog %d target %s objdist %.1f; маркер %d %s %.1f %.1f %.1f loc %d; машина %.1f %.1f %.1f пешком %d; монет %d; урон %.2f\n",
                          rt->phase, rt->stage, f / 60.f, rt->c_dest != 0, rt->c_talk != 0, rt->c_dialog != 0, rt->target ? rt->target : "-", rt->objdist,
                          M->markerOn, M->marker.drawable ? M->marker.drawable : "", M->marker.pos.x, M->marker.pos.y, M->marker.pos.z, M->marker.loc != 0,
                          W.car.x, W.car.y, W.car.z, gameOnFoot(&W.G), W.coins.collected, W.car.damage); }
    if (impossibleTimer && rt->timer_on && !rt->timer_up && rt->remaining > 1.f) rt->remaining = 1.f;
    if (rt->timer_on && !rt->timer_up) o.margin = rt->remaining;
    if (rt->status == MSN_FAILED) {                      // провал отрезка: снова с начала стадии другими числами водителя
      o.attempts++;
      // 05.11: строка FAIL всегда (отчёт «провалы по правилу миссии»): стадия, причина, остаток таймера, урон, круг/точка, доля пути до цели (1 − лучшее расстояние / стартовое)
      printf("FAIL\t%d\t%d.%d\t%s\t%s\t%.1f\t%.2f\t%d/%d\t%d/%d\t%.2f\t%.1f\t%.1f\n", mi, rt->phase, rt->stage, msn_objective_name[rt->obj], msn_fail_name[rt->fail], rt->remaining, W.car.damage,
             rt->lap, rt->laps, rt->next_item, rt->nitems, stageD0 > 1.f ? 1.f - stageMin / stageD0 : -1.f, W.car.x, W.car.z);
      // 30.09 (часть race): причина провала в журнал -v — «игра или бот» решается по ней (таймер, урон, соперник первым, пар-время)
      if (verbose) { printf("  провал стадии %d.%d t=%.1f: %s; таймер %.1f%s, урон %.2f, круг %d/%d, точка %d/%d, собрано %d, монет %d", rt->phase, rt->stage, W.st.frames / 60.f,
                            msn_fail_name[rt->fail], rt->remaining, rt->timer_up ? " (вверх)" : "", W.car.damage, rt->lap, rt->laps, rt->next_item, rt->nitems, rt->ncollected, W.coins.collected);   // правка 347: собрано — у dump/delivery «точка» не считает
                     for (int i = 0; i < M->cars.n; i++) { const McCar *c = &M->cars.c[i]; if (c->on) printf(" | %s wp %d fin %d", c->name, c->wp, c->finished); }
                     printf("\n"); }
      // 30.09 (часть race): таймер кончился на стадии без своего времени (AddStageTime 0: bm1 — поле томатов) — повтор, как в игре,
      // с чекпоинта RESET_TO_HERE (там таймер полный), не со снимка этой стадии, где время уже 0 (прежде 4 мгновенных провала подряд)
      if (rt->fail == MSN_FAIL_TIMEOUT && cpKey >= 0 && stageKey != cpKey) {
        if (++cpTries > 2) { o.result = "провал"; o.cls = "намеренно"; o.why = "таймер: 3 захода с чекпоинта (RESET_TO_HERE) не успели"; o.t = f / 60.f; o.tp = tp; o.frames = f; return o; }
        bwLoad(&W, &cpSnap); nrec = cpRec; botDrv.nroute = 0; rcReset(); escT = 0.f; trailReset(); aim.x = 1e9f; continue;
      }
      if (++tries > maxTries(rt)) { o.result = "провал"; o.cls = "намеренно"; o.why = "3 попытки стадии не прошли (правило миссии: таймер, урон)"; o.t = f / 60.f; o.tp = tp; o.frames = f; return o; }
      bwLoad(&W, &stageSnap); nrec = stageRec; botDrv.nroute = 0; rcReset(); escT = 0.f; trailReset();   // снимок стадии уже содержит сценарий в её начале — msn_retry не нужен
      for (int k = 0; k < NPAR; k++) par[k] = fmaxf(PARLO[k], fminf(PARHI[k], parBase[k] * (1.f + .15f * (tries & 1 ? 1 : -1) * (k + 1) / NPAR)));
      aim.x = 1e9f; continue;
    }
    // «сядь в машину» (getin, current): маркера нет — пешком к своей машине и △ у двери, как игрок (после разговора
    // с Мардж игрок стоит на месте: SetMissionResetPlayerInCar переносит только при старте/повторе, game_msn.h)
    int getin = rt->status == MSN_RUNNING && rt->obj == MSN_OBJ_GETIN && gameOnFoot(&W.G);
    if (!getenv("MISSIONER_FAST")) fastDrive = !(tries & 1) && (rt->obj == MSN_OBJ_RACE || (rt->timer_on && !rt->timer_up));   // быстрый водитель на гонке и таймере: с первой попытки, чётные попытки быстрым, нечётные аккуратным (дирижёр 05.11)
    if (!M->markerOn && !getin) {      // диалог, пауза между стадиями: ждать — но не вечно (лестница: 20 с кнопки, 60 с прыжок на чекпоинт или честное «застрял»)
      if (rt->status == MSN_RUNNING) waitT += BW_DT; else waitT = 0.f;
      if (waitT > 20.f) step(((int)(waitT * 2.f) & 7) == 0 ? ((int)waitT & 1 ? GP_CROSS : GP_TRIANGLE) : 0, 0, 0); else step(0, 0, 0);
      if (waitT > 60.f) {
        if (rt->phase == 0 && M->f.missions[mi].main != MSN_NONE16 && !waitJump && !getenv("MISSIONER_NO_TELEPORT")) {
          waitJump = 1; waitT = 0.f; printf("WAIT\t%d\t%d.%d\tпрыжок на чекпоинт основной части\t%.1f\n", mi, rt->phase, rt->stage, f / 60.f);
          rt->cp_phase = 1; rt->cp_stage = 0; msn_start_mission(rt, mi, 1); lastStage = -99; continue; }
        o.result = "застрял"; o.cls = "игра"; o.waitjump = waitJump; o.tp = tp; o.t = f / 60.f; o.frames = f;
        snprintf(waitWhy, sizeof waitWhy, "ждёт события: стадия %d.%d %s, маркера нет 60 с", rt->phase, rt->stage, msn_objective_name[rt->obj]); o.why = waitWhy;
        o.sx = gameOnFoot(&W.G) ? W.homer.x : W.car.x; o.sz = gameOnFoot(&W.G) ? W.homer.z : W.car.z; return o; }
      continue; }
    waitT = 0.f;
    MsnVec3 p = M->marker.pos; if (brokenGoal) p.y += 60.f;   // красный контроль: цель в воздухе, бот обязан не «пройти» и не ждать вечно
    if (getin) { p.x = W.car.x; p.y = W.car.y; p.z = W.car.z; }
    int ram = 0, followTrail = 0;
    if (laneBot && rt->status == MSN_RUNNING && !gameOnFoot(&W.G) && rt->target) {
      const McCar *tc = gameMsnTargetCar(M);
      if (tc && rt->obj == MSN_OBJ_LOSETAIL) {   // уйти от погони: точка по дорогам подальше от преследователя, раз в 5 с заново
        if ((escT -= BW_DT) <= 0.f && escapePoint(W.car.x, W.car.z, tc->car.x, tc->car.z, &esc)) escT = 5.f;
        if (escT > 0.f) p = esc;
      }
      ram = tc && (rt->obj == MSN_OBJ_DESTROY || (rt->obj == MSN_OBJ_DUMP && hypotf(p.x - tc->car.x, p.z - tc->car.z) < 3.f));
      if (tc && (rt->obj == MSN_OBJ_FOLLOW || ram)) {   // её дорогой: след; таран рядом (ближе 35 м) — прямо на неё
        trailPush(&tc->car); p.x = tc->car.x; p.y = tc->car.y; p.z = tc->car.z;
        followTrail = rt->obj == MSN_OBJ_FOLLOW ? 2 : hypotf(W.car.x - tc->car.x, W.car.z - tc->car.z) > 35.f;
      }
    }
    if (!bwRooms()) {   // без пакета комнат (build/interiors-l1.bin) стадии в комнатах не покрыты
      if (interiorPos(&p)) { o.result = "не покрыто"; o.cls = "нет данных"; o.why = "цель в комнате (интерьеры не перенесены)"; o.t = f / 60.f; o.frames = f; return o; }
      if (M->marker.drawable && strstr(M->marker.drawable, "doorstar")) { o.result = "не покрыто"; o.cls = "нет данных"; o.why = "вход в комнату (интерьеры не перенесены)"; o.t = f / 60.f; o.frames = f; return o; }
    }
    int foot = gameOnFoot(&W.G), wantFoot = getin ? 0 : footTarget(M);
    {   // предмет-цель рядом (не сфера для машины): пешком, как игрок во дворе
      const char *dr = M->marker.drawable ? M->marker.drawable : ""; float hx = foot ? W.homer.x : W.car.x, hz = foot ? W.homer.z : W.car.z;
      if (rt->c_dest && dr[0] && !strstr(dr, "carsphere") && hypotf(p.x - hx, p.z - hz) < 25.f) wantFoot = 1;
      // 30.09 (m2): пешком, а машина дальше — к предмету пешком, как игрок: бег ~7 м/с, машина ~20 м/с и ~5 с на посадку ->
      // пешком выгоднее, пока до предмета меньше 1.5 × до машины + 50 м (иначе бот уходил от цели за машиной за 150 м)
      if (foot && rt->c_dest && dr[0] && !strstr(dr, "carsphere") && hypotf(p.x - hx, p.z - hz) < 1.5f * hypotf(W.car.x - hx, W.car.z - hz) + 50.f) wantFoot = 1;
      if (breakTarget(M) && hypotf(p.x - hx, p.z - hz) < 60.f) wantFoot = 1;   // П1 (m4): щиток ломают пинком — пешком
    }
    {   // комнаты: цель в комнате, а Гомер снаружи -> её дверь; Гомер в комнате, цель снаружи -> выход
      int want = bwRoomAt(p.x, p.y, p.z); float t[3];
      if (want >= 0 && W.room != want && bwRoomDoor(want, t)) { p.x = t[0]; p.y = t[1]; p.z = t[2]; wantFoot = 1; }
      else if (W.room >= 0 && want != W.room) { bwRoomExit(W.room, t); p.x = t[0]; p.y = t[1]; p.z = t[2]; wantFoot = 1; }
      else if (W.room >= 0) wantFoot = 1;
    }
    float px = foot ? W.homer.x : W.car.x, pz = foot ? W.homer.z : W.car.z, py = foot ? W.homer.y : W.car.y;
    float dist = hypotf(p.x - px, p.z - pz); if (stageD0 < 0.f) stageD0 = dist; if (dist < stageMin) stageMin = dist;
    // цель едет (слежка, таран): ход — и когда бот сам проехал 15 м (расстояние до неё может не падать)
    { static float mx0, mz0; if (followTrail || ram) { if (hypotf(px - mx0, pz - mz0) > 15.f) { mx0 = px; mz0 = pz; best = 1e9f; } } else { mx0 = px; mz0 = pz; } }
    if (dist < best - 1.f) { best = dist; bestT = f / 60.f; }
    else if (f / 60.f - bestT > 12.f && !foot && evolved < 2 && !wantFoot) {   // застряла в машине: эволюция отрезка до цели из этого места
      evolved++; int nb = 0;
      if (evolveLeg(p, fminf(90.f, 20.f + dist / 5.f), &nb)) {
        for (int k = 0; k < nb; k++) step(legBest[k].b, legBest[k].ax, 0);   // лучший ввод — в основной прогон с записью
        f += nb; best = 1e9f; bestT = f / 60.f; aim.x = 1e9f; continue;
      }
      bestT = f / 60.f;   // не нашлось — дальше обычный возврат
    }
    else if (f / 60.f - bestT > 12.f) {                  // 12 с без приближения: RESET CAR и новый путь; 3 раза — застрял
      if (++resets > 3) {
        if (++tries <= maxTries(rt)) {   // как при провале: отрезок стадии заново с её начала другими числами водителя (езда хаотична)
          o.attempts++; bwLoad(&W, &stageSnap); nrec = stageRec; resets = 0; navAvoidN = 0; shift = 0; evolved = 0; botDrv.nroute = 0; rcReset(); escT = 0.f; trailReset();
          for (int k = 0; k < NPAR; k++) par[k] = fmaxf(PARLO[k], fminf(PARHI[k], parBase[k] * (1.f + .15f * (tries & 1 ? 1 : -1) * (k + 1) / NPAR)));
          bestT = f / 60.f; best = 1e9f; aim.x = 1e9f; continue;
        }
        if (tp < TP_MAX && !getenv("MISSIONER_NO_TELEPORT") && (rt->obj != MSN_OBJ_RACE || !getenv("MISSIONER_RACE_NO_TP")) && rt->obj != MSN_OBJ_FOLLOW && rt->obj != MSN_OBJ_LOSETAIL && rt->obj != MSN_OBJ_DESTROYBOSS && teleportAhead(mi, p, foot, f / 60.f, rt) >= 0) {   // последняя ступень: перенос с пометкой, задание проверяется дальше
          tp++; resets = 0; tries = 0; navAvoidN = 0; shift = 0; evolved = 0; botDrv.nroute = 0; rcReset(); escT = 0.f; trailReset(); aim.x = 1e9f; bestT = f / 60.f; best = 1e9f;
          bwSave(&W, &stageSnap); stageRec = nrec; continue; }   // новый снимок стадии = место после переноса
        o.result = "застрял"; o.why = "12 с без приближения к цели, 3 возврата и объезда"; o.tp = tp; o.t = f / 60.f; o.frames = f;
        o.sx = foot ? W.homer.x : W.car.x; o.sz = foot ? W.homer.z : W.car.z; o.wall = W.car.wallType * 100000 + (int)W.car.wallIdx;
        if (!foot && !wantFoot) {   // Т1.5: второй независимый водитель со старта стадии; доехал — виноват водитель, нет — подозрение на игру
          o.second = 1; int ok = secondDriver(&stageSnap, p, fminf(120.f, 30.f + dist / 4.f), o.sx, o.sz);
          o.cls = ok ? "водитель" : "игра"; if (ok) o.why = "первый водитель застрял, второй доехал до цели стадии";
        } else o.cls = "нет данных", o.why = "застрял пешком (второй водитель — только для машины)";
        return o; }
      if (foot) step(GP_TRIANGLE, 0, 0); else step(GP_START, 0, 0);
      navAvoid(foot ? W.homer.x : W.car.x, foot ? W.homer.z : W.car.z);   // следующий путь объезжает это место
      shift = (resets & 1 ? 1.f : -1.f) * 4.f * resets;   // объезд: опорные точки пути сдвигаются вбок
      bestT = f / 60.f; best = 1e9f; aim.x = 1e9f; continue;
    }
    (void)resetsT; (void)py;
    if (verbose > 1 && f % 60 == 0) printf("    кадр t=%.0f пешком %d к цели пешком %d dist %.1f (%.1f %.1f %.1f) машина %.1f %.1f %.1f урон %.2f разбита %d сбросов %d\n", f / 60.f, foot, wantFoot, dist,
                                           p.x, p.y, p.z, W.car.x, W.car.y, W.car.z, W.car.damage, W.car.destroyed, resets);
    if (!foot) {
      if (wantFoot && dist < 14.f) {                    // у двери/собеседника: встать и выйти
        if (fabsf(W.car.v) > 4.f) step(GP_L, 0, 0); else step(GP_TRIANGLE, 0, 0);
        continue;
      }
      // гонка (race): попытки стадии чередуют водителей — чётная по полосам (объезд перекрытых дорог и машин: m1, m7), нечётная —
      // навигатором (эволюция отрезка по настоящей физике, Т1.5: sr2, sr3); MISSIONER_RACE_LANES=1 — всегда по полосам
      // Правка 347-хвост (облако 05.10, sr3): на длинном отрезке гонки водитель по полосам ехал в объезд (путь по полосам длиннее
      // прямого графа дорог) и проигрывал до того, как попытки доходили до навигатора. Теперь на каждую новую точку гонки: после
      // первого кадра по полосам (маршрут botDrv спланирован) — сравнить длину пути по полосам с путём навигатора (navPlan);
      // навигатор короче на RACE_NAV_GAIN — эта точка навигатором сразу, не дожидаясь застревания. Повторные попытки чередуют
      // водителей, как прежде. MISSIONER_RACE_ALT=1 — прежнее (только чередование по попыткам)
      if (rt->obj == MSN_OBJ_RACE && laneBot && !getenv("MISSIONER_RACE_ALT") && !getenv("MISSIONER_RACE_LANES")) {
        if (hypotf(p.x - rcP.x, p.z - rcP.z) > 3.f) { rcP = p; rcPick = -1; }
        else if (rcPick < 0 && botDrv.nroute > 0) {
          float ll = hypotf(botDrv.route[0][0] - W.car.x, botDrv.route[0][2] - W.car.z), ln = 0.f;
          for (int i = 1; i < botDrv.nroute; i++) ll += hypotf(botDrv.route[i][0] - botDrv.route[i - 1][0], botDrv.route[i][2] - botDrv.route[i - 1][2]);
          ll += hypotf(p.x - botDrv.route[botDrv.nroute - 1][0], p.z - botDrv.route[botDrv.nroute - 1][2]);
          ln = navPathLen(W.car.x, W.car.y, W.car.z, p.x, p.y, p.z);   // нет пути по графу — навигатор не выбираем
          rcPick = !botDrv.routeCut && ln > 30.f && ln * RACE_NAV_GAIN < ll;
          if (verbose) printf("    гонка: путь по полосам %.0f м%s, навигатором %.0f м -> %s\n", ll, botDrv.routeCut ? " (обрезан)" : "", ln, rcPick ? "навигатор" : "по полосам");
          if (rcPick) o.raceNavPick++;
        }
      }
      // выбран навигатор — он на первой попытке, дальше чередование (по полосам, навигатор, …); иначе — как прежде
      int raceNav = rt->obj == MSN_OBJ_RACE && !getenv("MISSIONER_RACE_LANES") && (rcPick == 1 ? !(tries & 1) : (tries & 1));
      if (laneBot && !raceNav) { laneStep(&p, ram, rt->target, followTrail); continue; }   // 29.09: водитель по полосам (цель, таран, уход от погони)
      if (hypotf(p.x - aim.x, p.z - aim.z) > 3.f && !getenv("MISSIONER_NO_NAV") && dist > 30.f) {   // Т1.5: новая цель — сначала навигатором (эволюция отрезка, Б1)
        int nb = 0; aim = p;
        if (evolveLeg(p, fminf(90.f, 20.f + dist / 5.f), &nb)) { for (int k = 0; k < nb; k++) step(legBest[k].b, legBest[k].ax, 0); f += nb; o.nav++; best = 1e9f; bestT = f / 60.f; continue; }
        aim.x = 1e9f;   // не доехал за бюджет — дальше обычным водителем
      }
      if (hypotf(p.x - aim.x, p.z - aim.z) > 3.f) {      // новая цель: путь по дорогам
        aim = p; nq = navPlan(W.car.x, W.car.y, W.car.z, p.x, p.y, p.z, q, MAXWP); seg = 0;
        for (int i = 1; i + 1 < nq; i++) { float dx = q[i + 1].x - q[i - 1].x, dz = q[i + 1].z - q[i - 1].z, l = hypotf(dx, dz) + 1e-3f;
          q[i].x += dz / l * shift; q[i].z -= dx / l * shift; }
      }
      unsigned b; float ax; navSteer(q, nq, &seg, &W.car, par, &b, &ax);
      if (verbose && f % 60 == 0) printf("    машина t=%.0f %.1f %.1f %.1f v %.1f seg %d/%d dist %.1f кнопки %x руль %.2f стена %d:%u колёс %d земля %d RESET %u безопасных %d\n",
                                          f / 60.f, W.car.x, W.car.y, W.car.z, W.car.v, seg, nq, dist, b, ax, W.car.wallType, W.car.wallIdx, W.car.wheels,
                                          W.car.grounded, W.st.resets, W.car.nsafe);
      if (dist < 20.f && W.car.v > 10.f) b = (b & ~GP_R) | GP_L;   // подъезд к сфере не на полном ходу
      if (backT > 0) { b = GP_L; ax = backAx; backT -= BW_DT; }   // сдать назад с обратным рулём, как игрок
      else if ((b & GP_R) && fabsf(W.car.v) < 1.f) { if ((slowT += BW_DT) > 1.5f) { backT = 1.2f; backAx = -ax; slowT = 0; } } else slowT = 0;
      step(b, ax, 0);
    } else {
      int toWrench = 0;
      if (!wantFoot && W.car.destroyed) {   // машина разбита
        // 30.09 (m2, ингалятор): RESET CAR остов не чинит (viewer/game.c, библия «VEHICLE DESTROYED! FIND A WRENCH, OR GO TO A
        // PHONEBOOTH») — бот жал START вечно. Как игрок: предмет-цель не для машины (не carsphere) — к нему пешком; иначе к
        // ближнему свободному гаечному ключу (подбор пешком чинит машину, gameWrenchStep); ключей нет — START, как было
        const char *dr = M->marker.drawable ? M->marker.drawable : ""; const ColWorld *cw = bwCol(); float bd = 1e9f;
        if (rt->c_dest && dr[0] && !strstr(dr, "carsphere")) wantFoot = 1;
        else for (unsigned k = 0; k < cw->nwrench && k < 32; k++) { if (W.G.wrenchTaken >> k & 1u) continue;
          float d = hypotf(cw->wrench[k].p[0] - W.homer.x, cw->wrench[k].p[2] - W.homer.z);
          if (d < bd) { bd = d; p.x = cw->wrench[k].p[0]; p.y = cw->wrench[k].p[1]; p.z = cw->wrench[k].p[2]; toWrench = 1; } }
        if (!wantFoot && !toWrench) { step((f & 15) == 0 ? GP_START : 0, 0, 0); continue; }
        dist = hypotf(p.x - W.homer.x, p.z - W.homer.z); bestT = f / 60.f;   // к ключу — не «застрял» (от цели уходит нарочно)
        if (verbose && f % 60 == 0 && toWrench) printf("    машина разбита: к ключу %.1f %.1f %.1f, %.1f м\n", p.x, p.y, p.z, dist);
      }
      int toCar = 0;
      if (!wantFoot && !toWrench) {   // цель машинная: дойти до машины и сесть (△ работает только у двери)
        float dc = hypotf(W.car.x - W.homer.x, W.car.z - W.homer.z);
        HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &W.car, W.lift);   // △ садит только у двери (homerCanEnter)
        if (homerCanEnter(&W.homer, &hc) || W.room >= 0) { step((f & 1) ? GP_TRIANGLE : 0, 0, 0); continue; }
        p.x = W.car.x; p.y = W.car.y; p.z = W.car.z; dist = dc; toCar = 1;
      }
#ifdef BW_HAVE_AO
      if (liftRule(&p, lastStage * 16 + tries)) { bestT = f / 60.f; continue; }   // правка 116: цель выше досягаемого — подъёмник
#endif
      // 03.10 (разбор ночи 02.10, приёмка L1): m0 — своя машина встала у двери Kwik-E-Mart, Гомер 220 с упирался в неё; m1 — к двери
      // школы прямая упирается в боковую стенку крыльца (z3_schoolShape199), вход — по ступеням с востока. Как игрок: упёрся пешком
      // (1.5 с без сдвига, см. шаг вбок ниже), цель ближе 35 м, а прямая к ней не свободна (footpath.h fpClear: опора, тело, заборы,
      // своя машина) — путь A* по сетке 0.5 м по той же коллизии (fpPlan, до 2 м от цели, дальше прямо); идти к самой дальней точке
      // пути, до которой прямая свободна (до 12 м); путь заново раз в 2 с или когда цель сдвинулась. Пока не упёрся — ход прежний
      // (миссии, которые проходились, идут тем же путём). Правило бота, не игры. MISSIONER_NO_FOOTGRID=1 — без правила (красный
      // контроль tests/bots/footgrid.sh)
      if (roomGridOn < 0) roomGridOn = getenv("MISSIONER_ROOMGRID") != 0;
      static int gOff = -1, gArm, gArmKey = -1; static float gArmX, gArmZ;
      if (gOff < 0) gOff = getenv("MISSIONER_NO_FOOTGRID") != 0;
      int gKey = lastStage * 16 + tries, grid = 0; MsnVec3 p0 = p;
      if (gArm && (gArmKey != gKey || hypotf(p.x - gArmX, p.z - gArmZ) > 3.f || dist <= 1.f || toCar || (W.room >= 0 && !roomGridOn))) gArm = 0;
      { static FpPt path[256]; static int np, haveW; static float ptx = 1e9f, ptz = 1e9f, replanT, chkT, wx, wz;
        if (gArm && dist < 35.f) {
          if ((chkT -= BW_DT) <= 0.f || hypotf(p.x - ptx, p.z - ptz) > 1.f) {
            const ColWorld *cw = bwCol(); float ey; chkT = .1f; haveW = 0;
            if (!fpClear(cw, &W.car, W.homer.x, W.homer.y, W.homer.z, p.x, p.z, &ey)) {
              if (hypotf(p.x - ptx, p.z - ptz) > 1.f || (replanT -= .1f) <= 0.f) {   // пути нет — заново через 1 с (поиск ~50 мс)
                np = fpPlan(cw, &W.car, W.homer.x, W.homer.y, W.homer.z, p.x, p.y, p.z, 2.f, path, 256); ptx = p.x; ptz = p.z; replanT = np ? 2.f : 1.f;
                if (verbose) printf("    путь пешком t=%.1f: %d точек к %.1f %.1f %.1f от %.2f %.2f %.2f\n", f / 60.f, np, p.x, p.y, p.z, W.homer.x, W.homer.y, W.homer.z);
              }
              for (int k = np - 1; k >= 0 && !haveW; k--)
                if (hypotf(path[k].x - W.homer.x, path[k].z - W.homer.z) < 12.f && fpClear(cw, &W.car, W.homer.x, W.homer.y, W.homer.z, path[k].x, path[k].z, &ey)) {
                  wx = path[k].x; wz = path[k].z; haveW = 1;
                  if (k == np - 1 && hypotf(wx - W.homer.x, wz - W.homer.z) < .7f) haveW = 0; }   // конец пути (до 2 м от цели) — дальше прямо
              if (!haveW && np && hypotf(path[np - 1].x - W.homer.x, path[np - 1].z - W.homer.z) >= .7f) { wx = path[0].x; wz = path[0].z; haveW = 1; }
            } else np = 0;
          }
          if (haveW) { p.x = wx; p.z = wz; grid = 1; }
        } else { np = 0; haveW = 0; chkT = 0.f; ptx = ptz = 1e9f; }
      }
      // 05.11 (наряд ночи, «застрял пешком»): цель дальше 40 м — сетка footpath не берёт (область ±40 м). Крупный путь по графу дорог режется на прыжки до 55 м, каждый
      // строят пешеходы A и B (footnav.h, настоящая коллизия, прыжок через низкую преграду); идём по точкам с запасом 1,3 м, берём точку по ходу не ближе 2 м.
      // Упёрся дважды подряд — пересчёт вторым, независимым пешеходом (только B) с текущей точки. Не вышло — как прежде (граф дорог), затем лестница и перенос
      static int fnOff = -1; static FpPt fnPath[600]; static int fnN, fnSeg, fnFails, fnBOnly; static float fnGx = 1e9f, fnGz, fnAge;
      if (fnOff < 0) fnOff = getenv("MISSIONER_NO_FOOTNAV") != 0;
      static float fnFailT = -99.f;
      if (!fnOff && !grid && dist > 40.f && W.room < 0 && W.homer.y > -8.f && !toCar && !(fnN == 0 && f / 60.f - fnFailT < 10.f)) {   // не удалось — десять секунд не пересчитывать (дорогой поиск)
        if (hypotf(p0.x - fnGx, p0.z - fnGz) > 3.f || fnAge > 12.f) {   // новая цель или давно строили: путь заново
          P3 cq[MAXWP]; int ncq = navPlan(W.homer.x, W.homer.y, W.homer.z, p0.x, p0.y, p0.z, cq, MAXWP); FpPt co[MAXWP]; for (int i = 0; i < ncq; i++) co[i] = (FpPt){cq[i].x, cq[i].y, cq[i].z};
          FnResult fr; fnN = fnLongPlan(bwCol(), &W.car, bwLevel(), W.homer.x, W.homer.y, W.homer.z, p0.x, p0.y, p0.z, 1.5f, co, ncq, fnBOnly, fnPath, 600, &fr);
          fnSeg = 0; fnGx = p0.x; fnGz = p0.z; fnAge = 0.f; if (fnN) fnFails = 0; else { fnFails++; fnFailT = f / 60.f; }
          if (verbose) printf("    footnav t=%.1f: %d точек к %.1f %.1f (%s%s) %s\n", f / 60.f, fnN, p0.x, p0.z, fnN ? "путь " : "нет пути ", fnN ? (fr.who ? (fr.who == 'B' ? "B" : "A") : "") : fnWhyName(fr.why), fr.place);
        }
        fnAge += BW_DT;
        if (fnN) {
          while (fnSeg < fnN - 1 && hypotf(fnPath[fnSeg].x - W.homer.x, fnPath[fnSeg].z - W.homer.z) < 1.3f) fnSeg++;
          int k = fnSeg; while (k < fnN - 1 && hypotf(fnPath[k].x - W.homer.x, fnPath[k].z - W.homer.z) < 2.f) k++;   // точка по ходу не ближе 2 м: на бегу круг поворота около 2 м
          p.x = fnPath[k].x; p.z = fnPath[k].z; grid = 1;
        }
      } else { fnGx = 1e9f; fnBOnly = 0; }
      if (!grid && dist > 12.f && W.room < 0) {   // далеко пешком: по графу дорог (обход заборов между дворами), точка — первая дальше 3 м
        static float pullT;
        if (hypotf(p.x - aim.x, p.z - aim.z) > 3.f || !nq) { aim = p; nq = navPlan(W.homer.x, W.homer.y, W.homer.z, p.x, p.y, p.z, q, MAXWP); seg = 0; pullT = 0.f; }
        while (seg < nq - 1 && hypotf(q[seg + 1].x - W.homer.x, q[seg + 1].z - W.homer.z) < 3.f) seg++;
        // 30.09 (m1, дверь школы): граф — дороги машин, его ближний к Гомеру узел бывает позади, и бот уходил от цели (dist 110 -> 145).
        // Как игрок: к самой дальней точке пути (цель — первой), до которой прямая пешком свободна (footClear); раз в 0.5 с, до 80 м.
        // MISSIONER_NO_PULL=1 — как раньше (контроль)
        static int noPull = -1; if (noPull < 0) noPull = getenv("MISSIONER_NO_PULL") != 0;
        if (!noPull && (pullT -= BW_DT) <= 0.f) { pullT = .5f;
          for (int j = nq - 1; j > seg + 1; j--) if (hypotf(q[j].x - W.homer.x, q[j].z - W.homer.z) < 80.f && footClear(W.homer.x, W.homer.y, W.homer.z, q[j].x, q[j].z)) { seg = j - 1; break; } }
        if (seg < nq - 1) { p.x = q[seg + 1].x; p.z = q[seg + 1].z; }
      }
      // П1 (m4): у предмета «сломать» — пинок □ раз в 0.7 с, стик на него (лицом к щитку); шаг вбок не нужен — стоит у цели нарочно
      int kick = breakTarget(M) && dist < 2.2f;
      // 29.09: упёрся пешком (машина миссии, забор, угол дома) — 1.5 с без сдвига на 0.5 м: шаг вбок 0.8 с, стороны по очереди
      { static float fx0, fz0; static int sideN;
        if (kick) { stillT = 0.f; sideT = 0.f; }
        if (hypotf(W.homer.x - fx0, W.homer.z - fz0) > .5f) { fx0 = W.homer.x; fz0 = W.homer.z; stillT = 0.f; } else if (dist > 1.5f && !kick) stillT += BW_DT;
        if (stillT > 1.5f && sideT <= 0.f) {
          if (!gOff && !gArm && !toCar && (W.room < 0 || roomGridOn) && dist < 35.f) { gArm = 1; gArmKey = gKey; gArmX = p0.x; gArmZ = p0.z; stillT = 0.f; }   // сначала путь по сетке
          else { sideT = .8f; sideN++; stillT = 0.f; fnGx = 1e9f; fnBOnly = (sideN & 2) != 0; } }   // footnav: каждый второй упор — путь вторым пешеходом (B)
        if (sideT > 0.f) { sideT -= BW_DT; float a = atan2f(p.x - W.homer.x, p.z - W.homer.z) + (sideN & 1 ? 1.5708f : -1.5708f);
          p.x = W.homer.x + sinf(a) * 5.f; p.z = W.homer.z + cosf(a) * 5.f; } }
      static int sjOff = -1; if (sjOff < 0) sjOff = getenv("MISSIONER_SHORTJUMP_OLD") != 0;
      float yaw = W.homer.camYaw, r = atan2f(p.x - W.homer.x, p.z - W.homer.z) - yaw;
      if (verbose && f % 60 == 0) printf("    пешком t=%.0f Гомер %.2f %.2f %.2f -> цель %.1f %.1f  dist %.2f camYaw %.2f state %d\n", f / 60.f, W.homer.x, W.homer.y, W.homer.z, p.x, p.z, dist, yaw, W.homer.state);
      if (verbose && f % 300 == 0 && (rt->obj == MSN_OBJ_BUYCAR || rt->obj == MSN_OBJ_BUYSKIN)) for (int si = 0; si < M->shop.nseller; si++) printf("    продавец %s %.2f %.2f %.2f до Гомера %.2f радиус %.2f\n", M->shop.seller[si].npc, M->shop.seller[si].x, M->shop.seller[si].y, M->shop.seller[si].z, hypotf(M->shop.seller[si].x - W.homer.x, M->shop.seller[si].z - W.homer.z), M->shop.seller[si].radius);
      // у цели — действие: как игрок по подсказке △ (миссия его ждёт: разговор в радиусе, точка с действием) или вплотную
      int buy = rt->obj == MSN_OBJ_BUYCAR || rt->obj == MSN_OBJ_BUYSKIN;   // лавка продавца нужна только этапу «купи»
      unsigned b = (f % 30) == 0 && (dist < 1.5f || gameMsnWantsMissionAction(&W.msn) || (buy && gameMsnWantsAction(&W.msn))) ? GP_TRIANGLE : 0;
      if (gameShopOpen(&W.msn) >= 0 && (f % 30) == 0) b = buy ? GP_CROSS : GP_CIRCLE;   // лавка: Purchase (X) для этапа «купи», иначе отказ (○)
      if (dist > 3.f) b |= GP_CIRCLE;   // далеко — бегом, как игрок (○ DASH / SPRINT, ControllerPS2.pag); у цели — шагом
      if (((grid && fnN) || (!sjOff && dist < 40.f)) && sideT <= 0.f && (f % 45) == 0 && W.homer.grounded && stillT > .6f) b |= GP_CROSS;   // упор на пути footnav: прыжок ✕ через низкую преграду (забор 1,5 м)
      // 07.11 (Данные-93, разбор L1 m4): тот же прыжок ✕ и на коротком пешем пути (цель ближе 40 м, footnav не берёт): Гомер упирался в низкую преграду в «кармане» рабочего места 200 с. MISSIONER_SHORTJUMP_OLD=1 — как было (красный контроль: L1 m4 застревает)
      if (kick) { b = W.homer.grounded && (f % 42) == 0 ? GP_SQUARE : 0; bestT = f / 60.f; }   // бьёт — не «застрял»
      step(b, -sinf(r), -cosf(r));   // стик относительно камеры: вправо камеры = −x при курсе 0 (ось z отзеркалена)
    }
  }
  o.sx = gameOnFoot(&W.G) ? W.homer.x : W.car.x; o.sz = gameOnFoot(&W.G) ? W.homer.z : W.car.z;
  o.t = secs; o.frames = frames; o.tp = tp; o.waitjump = waitJump; o.why = "время вышло"; o.cls = "нет данных"; return o;
}

// ------------------------------------------------------------------ П1 (L1_ФИНИШ 30.09): теоретическое время блока с таймером
// Урок L1 (КАНОН_УРОВНЯ §8 п.13): миссия «время вышло» — сперва скорость машины против таймера, потом бот. Блок — стадии одного
// таймера (SetStageTime > 1 открывает, AddStageTime продолжает, стадия без времени закрывает, как msn_run.c; гонка на ставку —
// SetParTime). Путь — по графу дорог ботов (roads-l<N>.bin, A*; концы — прямыми до ближайшего узла) от точки начала блока через
// цели: goto — точка, гонка — точки по порядку и круги, delivery — лучший порядок (перебор подмножеств до 16 точек). Теория =
// путь / TopSpeed машины игрока (.con, carparams.h) — нижняя граница: разгон мгновенный, повороты без торможения. Доля от
// таймера < 0.8 — таймер машине посилен, чинить бота; иначе — вопрос оракулу (скорость машины в оригинале) и строка Робу
// «пройди на PSP». Цель машины миссии (таран, слежка, соперник) движется — такой блок теорией не считается («цель едет»).
//   missioner LEVEL HOMER ROADS MSN --theory [--mission N]   -> строки T (поля — в printf ниже), для всех уровней
//   сборка -DTHEORY_BREAK=1 — отрицательный контроль гейта (tests/missions/theory_gate.sh): путь берётся по прямой / 4 —
//   короче прямой, гейт «путь по дорогам не короче прямой» обязан упасть
#ifndef THEORY_BREAK
#define THEORY_BREAK 0
#endif
static int thPath[8192];
static float roadLen(float ax, float ay, float az, float bx, float by, float bz, int *ok) {   // по графу дорог; нет пути — прямая x1.4
  float straight = hypotf(bx - ax, bz - az);
  if (THEORY_BREAK) { *ok = 1; return straight * .25f; }
  int s = nearestNode(ax, ay, az), g = nearestNode(bx, by, bz), n = s >= 0 && g >= 0 ? astar(s, g, thPath, 8192) : 0;
  if (!n) { *ok = 0; return straight * 1.4f; }
  float L = hypotf(node[s].x - ax, node[s].z - az) + hypotf(node[g].x - bx, node[g].z - bz);
  for (int i = 1; i < n; i++) L += hypotf(node[thPath[i]].x - node[thPath[i - 1]].x, node[thPath[i]].z - node[thPath[i - 1]].z);
  *ok = 1; return fmaxf(L, straight);   // концы прямыми: путь не короче прямой (узел за спиной у обоих концов)
}
#define TH_MAXP 17
static float thD[TH_MAXP][TH_MAXP], thDP[1 << (TH_MAXP - 1)][TH_MAXP - 1];
// лучший путь из точки 0 через точки 1..n-1 (любой порядок, конец любой) -> длина; *last — какой точкой кончился
static float tspOpen(const float (*p)[3], int n, int *last, int *noRoad) {
  for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) { int ok = 1;
    thD[i][j] = i == j ? 0.f : roadLen(p[i][0], p[i][1], p[i][2], p[j][0], p[j][1], p[j][2], &ok); if (!ok) (*noRoad)++; }
  int m = n - 1, full = (1 << m) - 1; if (m <= 0) { *last = 0; return 0.f; }
  for (int s = 0; s <= full; s++) for (int k = 0; k < m; k++) thDP[s][k] = 1e30f;
  for (int k = 0; k < m; k++) thDP[1 << k][k] = thD[0][k + 1];
  for (int s = 1; s <= full; s++) for (int k = 0; k < m; k++) { float v = thDP[s][k]; if (!(s >> k & 1) || v >= 1e29f) continue;
    for (int j = 0; j < m; j++) if (!(s >> j & 1)) { float w = v + thD[k + 1][j + 1]; if (w < thDP[s | 1 << j][j]) thDP[s | 1 << j][j] = w; } }
  float best = 1e30f; for (int k = 0; k < m; k++) if (thDP[full][k] < best) { best = thDP[full][k]; *last = k + 1; }
  return best;
}
static void locXYZ(const MsnLoc *l, float *q) { q[0] = l->pos[0]; q[1] = l->pos[1]; q[2] = l->pos[2]; }
static void theoryMission(int mi) {
  const MsnFile *F = &W.msn.f; const MsnMission *ms = &F->missions[mi];
  // машина игрока: SetForcedCar миссии, иначе купленная в её вступлении (buycar), иначе машина уровня (CAR1)
  char car[32]; snprintf(car, sizeof car, "%s", bwCarName()); const char *carWhy = "машина уровня";
  for (int ph = 0; ph < 2; ph++) { unsigned pi = ph == 0 ? ms->intro : ms->main; if (pi == MSN_NONE16) continue; const MsnPhase *P = &F->phases[pi];
    for (unsigned s = 0; s < P->stage_count; s++) { const MsnStage *st = &F->stages[P->first_stage + s];
      if (st->obj == MSN_OBJ_BUYCAR && st->obj_arg) { snprintf(car, sizeof car, "%s", msn_str(F, st->obj_arg)); carWhy = "куплена во вступлении"; }
      for (unsigned c = 0; c < st->cmd_count; c++) { const MsnCmd *C = &F->cmds[st->cmd_first + c];
        if (C->op == MSN_OP_SETFORCEDCAR && msn_argc(C) > 0) { snprintf(car, sizeof car, "%s", msn_arg_s(F, C, 0)); carWhy = "SetForcedCar"; } } }
    for (unsigned c = 0; c < P->setup_count; c++) { const MsnCmd *C = &F->cmds[P->setup_first + c];
      if (C->op == MSN_OP_SETFORCEDCAR && msn_argc(C) > 0) { snprintf(car, sizeof car, "%s", msn_arg_s(F, C, 0)); carWhy = "SetForcedCar"; } } }
  CarParams cp; memset(&cp, 0, sizeof cp); { char lc[32]; int k = 0; for (const char *q = car; *q && k < 31; q++) lc[k++] = (char)((*q >= 'A' && *q <= 'Z') ? *q + 32 : *q); lc[k] = 0; carParamsFor(lc, &cp); }
  float top = cp.topSpeed > 1.f ? cp.topSpeed : 33.3f;
  unsigned pi = ms->main != MSN_NONE16 ? ms->main : ms->intro; if (pi == MSN_NONE16) return;
  const MsnPhase *P = &F->phases[pi];
  float pos[3] = {0, 0, 0}; int havePos = 0;
  for (unsigned c = 0; c < P->setup_count; c++) { const MsnCmd *C = &F->cmds[P->setup_first + c]; const MsnLoc *l = 0;
    if (C->op == MSN_OP_SETMISSIONRESETPLAYERINCAR) l = msn_arg_loc(F, C, 0);
    else if (C->op == MSN_OP_SETMISSIONRESETPLAYEROUTCAR) l = msn_arg_loc(F, C, 1);
    if (l) { locXYZ(l, pos); havePos = 1; } }
  int open = 0, a = 0, dyn = 0, noRoad = 0, gamble = 0; float budget = 0.f, len = 0.f, straight = 0.f;
  #define TH_FLUSH(bEnd) do { if (open) { \
      float th = len / top; \
      printf("T\t%d\t%s\t%d-%d\t%.0f\t%s\t%s\t%.0f\t%.0f\t%.1f\t%.2f\t%.0f\t%s\n", mi, msn_str(F, ms->id), a, (bEnd), budget, gamble ? "ставка" : "таймер", car, \
             top * 3.6f, len, dyn ? -1.f : th, dyn ? -1.f : th / fmaxf(budget, 1.f), straight, \
             dyn ? "цель едет: не считается" : noRoad ? "часть пути без графа дорог (прямая x1.4)" : carWhy); open = 0; } } while (0)
  for (unsigned s = 0; s < P->stage_count; s++) {
    const MsnStage *st = &F->stages[P->first_stage + s];
    float setT = -1.f, addT = -1.f, par = 0.f; int laps = 1, nit = 0; float it[48][3]; const MsnLoc *dest = 0, *npc = 0;
    for (unsigned c = 0; c < st->cmd_count; c++) { const MsnCmd *C = &F->cmds[st->cmd_first + c]; const MsnLoc *l;
      switch (C->op) {
        case MSN_OP_SETSTAGETIME: setT = msn_arg_f(F, C, 0); break;
        case MSN_OP_ADDSTAGETIME: addT = fmaxf(0.f, addT) + msn_arg_f(F, C, 0); break;
        case MSN_OP_SETPARTIME: par = msn_arg_f(F, C, 0); break;
        case MSN_OP_SETRACELAPS: laps = msn_arg_i(F, C, 0) > 0 ? msn_arg_i(F, C, 0) : 1; break;
        case MSN_OP_PLACEPLAYERCAR: if ((l = msn_arg_loc(F, C, 1))) { locXYZ(l, pos); havePos = 1; } break;
        case MSN_OP_ADDCOLLECTIBLE: if (nit < 48 && (l = msn_arg_loc(F, C, 0))) { locXYZ(l, it[nit]); nit++; } break;
        case MSN_OP_SETDESTINATION: dest = msn_arg_loc(F, C, 0); break;
        case MSN_OP_ADDNPC: if (!npc) npc = msn_arg_loc(F, C, 1); break;
        default: break; } }
    int timed = setT > 1.f || addT >= 0.f || par > 0.f;
    if (!timed || setT > 1.f || par > 0.f) TH_FLUSH((int)s - 1);   // стадия без времени закрывает блок; новое время — новый блок
    if (timed && !open) { open = 1; a = (int)s; dyn = 0; noRoad = 0; len = straight = 0.f; gamble = par > 0.f;
      budget = par > 0.f ? par : setT > 1.f ? setT : fmaxf(0.f, addT); }
    else if (timed && addT > 0.f) budget += addT;
    // цели стадии -> путь (и по блоку, и без таймера: точка начала следующего блока)
    int ok = 1;
    if (st->obj == MSN_OBJ_RACE && nit) {
      for (int L = 0; L < laps; L++) for (int k = 0; k < nit; k++) {
        if (havePos) { float l = roadLen(pos[0], pos[1], pos[2], it[k][0], it[k][1], it[k][2], &ok); len += l; straight += hypotf(it[k][0] - pos[0], it[k][2] - pos[2]); if (!ok) noRoad++;
          if (verbose) printf("  гонка %d: %.0f %.0f -> %.0f %.0f  дорогой %.0f м, прямая %.0f м%s\n", k, pos[0], pos[2], it[k][0], it[k][2], l, hypotf(it[k][0] - pos[0], it[k][2] - pos[2]), ok ? "" : " (нет пути по графу)"); }
        memcpy(pos, it[k], sizeof pos); havePos = 1; } }
    else if ((st->obj == MSN_OBJ_DELIVERY || st->obj == MSN_OBJ_PICKUPITEM) && nit) {
      if (havePos && nit < TH_MAXP) { float p[TH_MAXP][3]; memcpy(p[0], pos, sizeof p[0]); for (int k = 0; k < nit; k++) memcpy(p[k + 1], it[k], sizeof p[0]);
        int last = 0; float L = tspOpen((const float (*)[3])p, nit + 1, &last, &noRoad); len += L;
        if (verbose) printf("  стадия %u: %d предметов от %.0f %.0f, лучший порядок %.0f м, конец у %.0f %.0f\n", s, nit, pos[0], pos[2], L, p[last][0], p[last][2]);
        float sb = 0.f; for (int k = 0; k < nit; k++) sb = fmaxf(sb, hypotf(it[k][0] - pos[0], it[k][2] - pos[2])); straight += sb;   // прямая: хотя бы до дальней
        memcpy(pos, p[last], sizeof pos); }
      else { dyn |= open; if (nit) memcpy(pos, it[nit - 1], sizeof pos); } }
    else if (dest && (st->obj == MSN_OBJ_GOTO || st->obj == MSN_OBJ_INTERIOR || st->obj == MSN_OBJ_GOOUTSIDE)) {
      float q[3]; locXYZ(dest, q);
      if (havePos) { float l = roadLen(pos[0], pos[1], pos[2], q[0], q[1], q[2], &ok); len += l; straight += hypotf(q[0] - pos[0], q[2] - pos[2]); if (!ok) noRoad++;
        if (verbose) printf("  стадия %u: %.0f %.0f -> %s %.0f %.0f  дорогой %.0f м, прямая %.0f м\n", s, pos[0], pos[2], msn_str(F, dest->name), q[0], q[2], l, hypotf(q[0] - pos[0], q[2] - pos[2])); }
      memcpy(pos, q, sizeof pos); havePos = 1; }
    else if (st->obj == MSN_OBJ_TALKTO && npc) { locXYZ(npc, pos); havePos = 1; }
    else if (st->obj == MSN_OBJ_DESTROY || st->obj == MSN_OBJ_FOLLOW || st->obj == MSN_OBJ_LOSETAIL || st->obj == MSN_OBJ_DUMP ||
             st->obj == MSN_OBJ_DESTROYBOSS) dyn |= open;
  }
  TH_FLUSH((int)P->stage_count - 1);
  #undef TH_FLUSH
}

static void writeIn(const char *out, int mi, const Out *o) {
  char fn[512]; snprintf(fn, sizeof fn, "%s/m%d.in", out, mi); FILE *f = fopen(fn, "w"); if (!f) return;
  fprintf(f, "# missioner: миссия %d, итог %s; повтор: game_headless LEVEL HOMER --msn MSN --mission %d --input этот_файл\n", mi, o->result, mi);
  for (int i = 0; i < nrec; ) { int j = i; while (j < nrec && rec[j].b == rec[i].b && rec[j].ax == rec[i].ax && rec[j].ay == rec[i].ay) j++;
    fprintf(f, "%d %x %.9g %.9g\n", j - i, rec[i].b, rec[i].ax, rec[i].ay); i = j; }
  fclose(f);
}
int missionerRun(int argc, char **argv) {
  if (argc < 6) { fprintf(stderr, "usage: missioner LEVEL HOMER ROADS MSN [--mission N | --all] [--secs S] [--impossible-timer] [--out DIR]\n"); return 2; }
  if (bwData(argv[1], argv[2], argv[4])) { fprintf(stderr, "missioner: нет данных\n"); return 2; }
  navLoad(argv[3]);
  int one = -1, all = 0, legMode = 0, theory = 0, packs = 0, catalog = 0; float secs = 900, lp[3] = {0}; const char *out = 0;
  for (int i = 5; i < argc; i++) {
    if (!strcmp(argv[i], "--mission") && i + 1 < argc) one = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--all")) all = 1;
    else if (!strcmp(argv[i], "--catalog")) catalog = 1;
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--impossible-timer")) impossibleTimer = 1;
    else if (!strcmp(argv[i], "--coins") && i + 1 < argc) startCoins = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--broken-goal")) brokenGoal = 1;
    else if (!strcmp(argv[i], "--from-main")) fromMain = 1;   // 29.09: основная часть миссии с её чекпоинта (как повтор после провала)
    else if (!strcmp(argv[i], "--leg") && i + 3 < argc) { legMode = 1; for (int k = 0; k < 3; k++) lp[k] = (float)atof(argv[i + 1 + k]); i += 3; }
    else if (!strcmp(argv[i], "--theory")) theory = 1;   // П1: теоретическое время блоков с таймером (theoryMission)
    else if (!strcmp(argv[i], "--packs")) packs = 1;     // 03.10: какие спутники игры видит мир миссий (ночь сверяет, что он как у EBOOT)
    else if (!strcmp(argv[i], "-v")) verbose = 1;
    else if (!strcmp(argv[i], "-vv")) verbose = 2;   // и строка каждой секунды в любой ветке (где стоит бот, почему не едет)
    else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
  }
  if (getenv("MISSIONER_SEED")) { lrs = (uint32_t)strtoul(getenv("MISSIONER_SEED"), 0, 0) * 2654435761u | 1; seedSet = lrs; }   // вариант прохождения (ось ночи)
  rec = malloc(sizeof(In) * (size_t)recCap);
  bwNew(&W); bwSaveAlloc(&W, &stageSnap); bwSaveAlloc(&W, &curSnap); bwSaveAlloc(&W, &cpSnap);
  leg = malloc(sizeof(In) * 60 * 120); legBest = malloc(sizeof(In) * 60 * 120);
  if (legMode) {   // гейт Т1.5: отрезок от спавна двумя водителями -> класс
    legWeak = getenv("MISSIONER_WEAK1") != 0; if (getenv("MISSIONER_SEED")) lrs = (uint32_t)strtoul(getenv("MISSIONER_SEED"), 0, 0) | 1; MsnVec3 p = {lp[0], lp[1], lp[2]}; float s0 = secs < 900 ? secs : 60.f;
    { float x, y, z, h; bwSpawn(&x, &y, &z, &h); bwPlace(&W, x, y, z, h); }   // Гомер за рулём на спавне (с миссиями игра начинается пешком)
    for (int k = 0; k < 30; k++) bwStep(&W, 0, 0, 0);
    bwSave(&W, &stageSnap); int nb = 0, ok1 = evolveLeg(p, s0, &nb), ok2 = 0; float ex = legEndX, ez = legEndZ;
    if (!ok1) { legWeak = 0; ok2 = secondDriver(&stageSnap, p, s0, ex, ez); }
    printf("L\t%s\t%s\t%.1f\t%.1f\n", ok1 ? "первый доехал" : ok2 ? "первый нет, второй доехал" : "оба не доехали", ok1 ? "—" : ok2 ? "водитель" : "игра", ex, ez);
    return 0;
  }
  if (!W.haveMsn) { fprintf(stderr, "missioner: миссии не открылись\n"); return 2; }
  // 03.10 (разбор ночи 02.10): мир миссий бота = мир EBOOT — кузова машин миссий и SetForcedCar (msncars-l<N>.shr), длины реплик и
  // события музыки (audio/), комнаты, трафик. Строка "P\tкузовов\tзвук\tкомнат\tтрафик": 0 — спутника рядом с level.shr нет
  if (packs) { int cg = 0, au = 0; bwMissionPacks(&cg, &au); printf("P\t%d\t%d\t%d\t%d\n", cg, au, bwRooms(), W.haveTraffic); return 0; }
  if (catalog) {
    const MsnFile *F = &W.msn.f;
    for (unsigned mi = 0; mi < F->nmissions; mi++) {
      const MsnMission *ms = &F->missions[mi];
      printf("CAT\t%u\t%s\t%u\n", mi, msn_str(F, ms->id), ms->flags);
      for (int ph = 0; ph < 2; ph++) {
        unsigned pi = ph ? ms->main : ms->intro;
        if (pi == MSN_NONE16) continue;
        const MsnPhase *P = &F->phases[pi];
        for (unsigned k = 0; k < P->stage_count; k++) {
          const MsnStage *st = &F->stages[P->first_stage + k];
          printf("PLAN\t%u\t%s\t%d\t%u\t%s\n", mi, msn_str(F, ms->id), ph, k, msn_objective_name[st->obj]);
          printf("SOURCE\t%u\t%d\t%u\t%s\n", mi, ph, k, msn_str(F, P->file));
        }
      }
    }
    return 0;
  }
  if (theory) { for (int mi = one >= 0 ? one : 0; mi < (one >= 0 ? one + 1 : (int)W.msn.f.nmissions); mi++) theoryMission(mi); return 0; }
  int n = (int)W.msn.f.nmissions, a = one >= 0 ? one : 0, b = one >= 0 ? one + 1 : (all ? n : 1);
  for (int mi = a; mi < b && mi < n; mi++) {
    const MsnMission *ms = &W.msn.f.missions[mi];
    if (one < 0 && (ms->flags & (MSN_MF_BONUS | MSN_MF_STREETRACE | MSN_MF_GAMBLE | MSN_MF_BONUSMISSION))) continue;   // --all: сюжет m0..m7
    Out o = playMission(mi, secs); mechSummary(mi);
    printf("M\t%d\t%s\t%s\t%s\t%s\t%.1f\t%.1f\t%.2f\t%d\t%d\t%d\t%.1f\t%.1f\t%d\t%s\t%d\t%d\t%d\t%d\n", mi, msn_str(&W.msn.f, ms->id), msn_str(&W.msn.f, ms->title), o.result, o.why, o.t,
           o.margin, o.damage, o.stages, o.attempts, o.frames, o.sx, o.sz, o.wall, o.cls, o.nav, o.second, o.tp, o.waitjump);
    if (out) writeIn(out, mi, &o);
    fflush(stdout);
  }
  return 0;
}

#ifndef ALLRISK_MISSION_LIBRARY
int main(int argc, char **argv) { return missionerRun(argc, argv); }
#endif
