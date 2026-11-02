// Окно g07-missions, сверка с исходником 02.11.2026: проверка рантайма миссий (viewer/mission) на настоящих данных диска, без игры и эмулятора.
// Каждая проверка прыгает на нужный этап (контрольная точка + msn_retry), крутит рантайм с придуманным MsnWorld и смотрит события и состояние.
// Красный контроль: сборка с -D<ИМЯ>_OLD=1 (все выключатели) обязана упасть на проверках своей правки (tests/g07_sverka/rt_check.sh).
//   rt_check build/missions/level01.msn
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "msn.h"

static int fails, nchecks;
static int skips;
static void skip(const char *name, const char *d) { skips++; printf("CHECK %-9s SKIP  %s\n", name, d); }
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-9s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }

// ---- хост: события считаем, сущности по таблице
#define MAXEV 4096
static int evType[MAXEV], evIdx[MAXEV], nev; static float evVal[MAXEV];
static char lastText[96], lastName[64];
static int ownsAll; static int entOn; static MsnEntity entE;
static void onEvent(void *u, const MsnEvent *e) {
  (void)u;
  if (nev < MAXEV) { evType[nev] = e->type; evIdx[nev] = e->index; evVal[nev] = e->value; nev++; }
  if (e->type == MSN_EV_STAGE_START) snprintf(lastText, sizeof lastText, "%s", e->text ? e->text : "");
  if (e->type == MSN_EV_LOCKED_HINT) snprintf(lastName, sizeof lastName, "%s", e->name ? e->name : "");
}
static void onCommand(void *u, const MsnCall *c) { (void)u; (void)c; }
static int onEntity(void *u, const char *name, MsnEntity *out) { (void)u; (void)name; if (!entOn) return 0; *out = entE; return 1; }
static int onOwns(void *u, int kind, const char *name) { (void)u; (void)kind; (void)name; return ownsAll; }
static int countEv(int type) { int n = 0; for (int i = 0; i < nev; i++) if (evType[i] == type) n++; return n; }

static MsnFile F; static MsnRuntime R;
static void openRt(void) {
  MsnHost h; memset(&h, 0, sizeof h); h.event = onEvent; h.command = onCommand; h.entity = onEntity; h.owns = onOwns;
  msn_init(&R, &F, &h); nev = 0; ownsAll = 0; entOn = 0; lastText[0] = lastName[0] = 0;
}
static MsnWorld W;
static void world(int onFoot, const char *veh, const char *interior) {
  memset(&W, 0, sizeof W); W.on_foot = onFoot; W.vehicle = veh; W.vehicle_health = 1.f; W.interior = interior; W.player.x = W.player.z = -5000.f; W.coins = 1000;
}
static void tick(float dt, int n) { for (int i = 0; i < n; i++) msn_tick(&R, dt, &W); }
// Прыжок на (миссия, фаза, этап)
static void jump(int mission, int phase, int stage) {
  openRt(); R.completed = 0;
  msn_start_mission(&R, mission, 0);
  R.cp_phase = phase; R.cp_stage = stage; R.cp_timer = 0; R.cp_remaining = 0;
  msn_retry(&R, 0);
}
static int firstStage(int (*pred)(const MsnStage *, int), int *mi, int *ph, int *si, int skip) {
  for (uint32_t m = 0; m < F.nmissions; m++) {
    const MsnMission *M = &F.missions[m];
    for (int p = 0; p < 2; p++) {
      unsigned idx = p == 0 ? M->intro : M->main; if (idx == MSN_NONE16) continue;
      const MsnPhase *P = &F.phases[idx];
      for (unsigned s = 0; s < P->stage_count; s++) {
        const MsnStage *S = &F.stages[P->first_stage + s];
        if (pred(S, (int)(P->first_stage + s))) { if (skip-- > 0) continue; *mi = (int)m; *ph = p; *si = (int)s; return 1; }
      }
    }
  }
  return 0;
}
static int hasCond(const MsnStage *S, int type) { for (int k = 0; k < S->ncond; k++) if (F.conds[S->cond_first + k].type == type) return 1; return 0; }
static int pLocked(const MsnStage *S, int i) { (void)i; return (S->flags & MSN_SF_LOCKED) && S->lock_kind == MSN_OWN_CAR; }
static int pOov(const MsnStage *S, int i) { (void)i; return hasCond(S, MSN_COND_OUTOFVEHICLE) && S->obj != MSN_OBJ_DIALOGUE && S->obj != MSN_OBJ_FMV && S->obj != MSN_OBJ_TIMER; }
static int pGamble(const MsnStage *S, int i) { (void)i; return (S->flags & MSN_SF_ELAPSED) && msn_streq(msn_str(&F, S->obj_arg), "gamble"); }
static int pLose(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_LOSETAIL; }
static int pFollow(const MsnStage *S, int i) { (void)i; return hasCond(S, MSN_COND_FOLLOWDISTANCE); }
static int pOutside(const MsnStage *S, int i) {   // gooutside, у которого локатор цели разрешён в пакете (иначе — запасной путь «вышел наружу»)
  (void)i; if (S->obj != MSN_OBJ_GOOUTSIDE) return 0;
  for (unsigned k = 0; k < S->cmd_count; k++) { const MsnCmd *c = &F.cmds[S->cmd_first + k]; if (c->op == MSN_OP_SETDESTINATION) return msn_arg_loc(&F, c, 0) != NULL; }
  return 0;
}
static int pTimeout(const MsnStage *S, int i) { (void)i; return hasCond(S, MSN_COND_TIMEOUT) && S->obj != MSN_OBJ_DIALOGUE; }
static int pBuyCar(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_BUYCAR; }
static int pAbortCmd(const MsnStage *S, int i) { (void)i; for (unsigned k = 0; k < S->cmd_count; k++) if (F.cmds[S->cmd_first + k].op == MSN_OP_ALLOWMISSIONABORT) return 1; return 0; }

static void runToRunning(void) { for (int i = 0; i < 400 && R.status == MSN_COUNTDOWN; i++) tick(0.05f, 1); }

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: rt_check level.msn\n"); return 2; }
  FILE *fp = fopen(argv[1], "rb"); if (!fp) { perror(argv[1]); return 2; }
  fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
  void *buf = malloc((size_t)n + 16); if (fread(buf, 1, (size_t)n, fp) != (size_t)n) return 2; fclose(fp);
  char err[200];
  if (msn_open(&F, buf, (size_t)n, err, sizeof err)) { fprintf(stderr, "msn_open: %s\n", err); return 2; }
  int mi, ph, si; char d[200];

  // ---- 1. задержка между этапами (P06-17, 5.4, P07-27)
  openRt(); snprintf(d, sizeof d, "delay_stage=%.2f flourish=%.2f", R.delay_stage, R.delay_stage_flourish);
  check("delay", R.delay_stage == 0.f && R.delay_stage_flourish == 0.f, d);

  // ---- 2. запертый этап (P07-2, P06-8)
  if (firstStage(pLocked, &mi, &ph, &si, 0)) {
    const MsnStage *S = &F.stages[F.phases[ph == 0 ? F.missions[mi].intro : F.missions[mi].main].first_stage + si];
    const char *lock = msn_str(&F, S->lock_name);
    jump(mi, ph, si); ownsAll = 1;                                  // куплено, но в машине — другая
    snprintf(d, sizeof d, "m%d.%d.%d lock=%s: куплено — этап с разговором всё равно играет (obj=%d stage=%d)", mi, ph, si, lock, R.obj, R.stage);
    check("lockplay", R.obj == MSN_OBJ_DIALOGUE && R.stage == si, d);
    world(1, "other_v", NULL); runToRunning(); msn_notify(&R, MSN_IN_DIALOGUE_DONE, NULL, 0); tick(0.05f, 3);
    snprintf(d, sizeof d, "в машине other_v, замок %s: подсказка выдана %d раз, имя %s", lock, countEv(MSN_EV_LOCKED_HINT), lastName);
    check("lockhint", countEv(MSN_EV_LOCKED_HINT) == 1 && !strcmp(lastName, lock), d);
    snprintf(d, sizeof d, "стартовое сообщение следующего этапа убрано: «%s»", lastText);
    check("locksupp", lastText[0] == 0, d);
    jump(mi, ph, si); world(1, lock, NULL); runToRunning(); msn_notify(&R, MSN_IN_DIALOGUE_DONE, NULL, 0); tick(0.05f, 3);
    snprintf(d, sizeof d, "в нужной машине %s: подсказок %d", lock, countEv(MSN_EV_LOCKED_HINT));
    check("lockok", countEv(MSN_EV_LOCKED_HINT) == 0, d);
  } else skip("lockplay", "в этом уровне нет запертого этапа car");

  // ---- 3. «не выходи из машины» (8.1, P06 Д3, P07-12, P07 Д1)
  if (firstStage(pOov, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); tick(0.05f, 20);
    snprintf(d, sizeof d, "m%d.%d.%d: в машине — условие выключено (oov_on=%d), status=%d", mi, ph, si, R.oov_on, R.status);
    check("oov_in", !R.oov_on && R.status == MSN_RUNNING, d);
    world(1, "a_v", NULL); tick(0.05f, 1);
    float win = R.oov_win; snprintf(d, sizeof d, "вышел — включилось (oov_on=%d), окно %.2f с = SetCondTime + 1", R.oov_on, win);
    check("oov_on", R.oov_on && win > 1.5f, d);
    tick(0.05f, (int)((win - 0.5f) / 0.05f)); snprintf(d, sizeof d, "пешком %.1f с из %.1f — ещё не провал (status=%d)", R.out_time, win, R.status);
    check("oov_wait", R.status == MSN_RUNNING, d);
    tick(0.05f, 30); snprintf(d, sizeof d, "пешком дольше окна — провал outofvehicle (status=%d fail=%d)", R.status, R.fail);
    check("oov_fail", R.status == MSN_FAILED && R.fail == MSN_FAIL_OUTOFVEHICLE, d);
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); tick(0.05f, 5); world(1, "a_v", NULL); tick(0.05f, 40); world(0, "b_v", NULL); tick(0.05f, 2);
    snprintf(d, sizeof d, "вышел из a_v, сел в b_v — провал сразу (status=%d fail=%d)", R.status, R.fail);
    check("oov_other", R.status == MSN_FAILED && R.fail == MSN_FAIL_OUTOFVEHICLE, d);
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); tick(0.05f, 5); world(1, "a_v", NULL); tick(0.05f, 100); world(0, "a_v", NULL); tick(0.05f, 3);
    snprintf(d, sizeof d, "вышел и сел обратно в a_v — выключено, окно заново (oov_on=%d out_time=%.2f status=%d)", R.oov_on, R.out_time, R.status);
    check("oov_back", !R.oov_on && R.out_time == 0.f && R.status == MSN_RUNNING, d);
    jump(mi, ph, si); world(1, "a_v", NULL); runToRunning(); tick(0.05f, 400);
    snprintf(d, sizeof d, "этап начат пешком без выхода — 20 с пешком не провал по outofvehicle (status=%d fail=%d)", R.status, R.fail);
    check("oov_nostart", !(R.status == MSN_FAILED && R.fail == MSN_FAIL_OUTOFVEHICLE), d);
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); tick(0.05f, 5); world(1, "a_v", NULL); tick(0.05f, 20); world(1, "a_v", "KwikEMart"); tick(0.05f, 2);
    snprintf(d, sizeof d, "вышел и зашёл в помещение — провал (status=%d fail=%d)", R.status, R.fail);
    check("oov_inter", R.status == MSN_FAILED && R.fail == MSN_FAIL_OUTOFVEHICLE, d);
  } else skip("oov_in", "в этом уровне нет outofvehicle");

  // ---- 4. пар-время ставки (P07-4, P06-4)
  if (firstStage(pGamble, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); float par = R.par;
    snprintf(d, sizeof d, "m%d.%d.%d: пар %.0f с, таймер вверх=%d, старт %.3f с", mi, ph, si, par, R.timer_up, R.remaining);
    check("par_init", par > 30.f && R.timer_up && R.remaining > 0.f && R.remaining < 0.01f, d);
    while (R.remaining < par - 1.f && R.status == MSN_RUNNING) tick(1.f, 1);
    tick(1.f, 3);    // 3 с после пара
    snprintf(d, sizeof d, "пар %.0f, прошло %.0f: гонка идёт (status=%d), предупреждений %d", par, R.remaining, R.status, countEv(MSN_EV_PAR_LATE));
    check("par_late", R.status == MSN_RUNNING && countEv(MSN_EV_PAR_LATE) >= 1, d);
    while (R.remaining < par + 14.f && R.status == MSN_RUNNING) tick(1.f, 1);
    snprintf(d, sizeof d, "пар + 14 с (%.0f): ещё идёт (status=%d)", R.remaining, R.status);
    check("par_14", R.status == MSN_RUNNING, d);
    tick(1.f, 3);
    snprintf(d, sizeof d, "больше пар + 15 с (%.0f): провал partime (status=%d fail=%d)", R.remaining, R.status, R.fail);
    check("par_fail", R.status == MSN_FAILED && R.fail == MSN_FAIL_PARTIME, d);
  } else skip("par_init", "в этом уровне нет ставочной гонки");

  // ---- 5. losetail (P07-30), followdistance (P07-29), gooutside (P07-31)
  if (firstStage(pLose, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); float od = R.objdist;
    R.timer_on = 0; W.player.x = 0; W.player.z = 0; W.player.y = 0; entOn = 1; memset(&entE, 0, sizeof entE); entE.health = 1.f; entE.waypoint = -1; entE.pos.x = od - 3.f;
    tick(0.05f, 2); snprintf(d, sizeof d, "objdist %.0f: цель на %.0f м — ещё не закрыта (status=%d fail=%d)", od, od - 3.f, R.status, R.fail);
    check("lose_near", R.status == MSN_RUNNING, d);
    entE.pos.x = od + 2.f; tick(0.05f, 1);
    snprintf(d, sizeof d, "цель на %.0f м (> %.0f) — закрыта в этот же кадр (status=%d)", od + 2.f, od, R.status);
    check("lose_far", (R.stage != si || R.status != MSN_RUNNING) && R.status != MSN_FAILED, d);
  } else skip("lose_near", "в этом уровне нет losetail");
  if (firstStage(pFollow, &mi, &ph, &si, 0)) {
    const MsnStage *S = &F.stages[F.phases[ph == 0 ? F.missions[mi].intro : F.missions[mi].main].first_stage + si];
    float fmax = 150.f;
    for (int k = 0; k < S->ncond; k++) { const MsnCond *c = &F.conds[S->cond_first + k]; if (c->type == MSN_COND_FOLLOWDISTANCE)
      for (uint32_t j = 0; j < c->cmd_count; j++) { const MsnCmd *cm = &F.cmds[c->cmd_first + j]; if (cm->op == MSN_OP_SETFOLLOWDISTANCES) fmax = msn_arg_f(&F, cm, 1); } }
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
    W.player.x = 0; W.player.z = 0; entOn = 1; memset(&entE, 0, sizeof entE); entE.health = 1.f; entE.waypoint = -1; entE.finished = 0; entE.pos.x = fmax - 5.f;
    tick(0.05f, 10); snprintf(d, sizeof d, "m%d.%d.%d: максимум %.0f м, цель на %.0f м — не провал (status=%d), шкал близости %d", mi, ph, si, fmax, fmax - 5.f, R.status, countEv(MSN_EV_FOLLOW_NEAR));
    check("fol_near", R.status == MSN_RUNNING && countEv(MSN_EV_FOLLOW_NEAR) > 0, d);
    entE.pos.x = fmax + 3.f; tick(0.05f, 3);
    snprintf(d, sizeof d, "цель на %.0f м (> максимума) — провал в ближайшую проверку 100 мс, не через секунду (status=%d fail=%d)", fmax + 3.f, R.status, R.fail);
    check("fol_fail", R.status == MSN_FAILED && R.fail == MSN_FAIL_FOLLOWDISTANCE, d);
  } else skip("fol_near", "в этом уровне нет followdistance");
  if (firstStage(pOutside, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); tick(0.05f, 3);
    snprintf(d, sizeof d, "m%d.%d.%d: игрок далеко от локатора снаружи — gooutside не закрыт (status=%d)", mi, ph, si, R.status);
    check("outside", R.status == MSN_RUNNING, d);
    const MsnLoc *l = R.c_dest ? msn_arg_loc(&F, R.c_dest, 0) : NULL;
    if (l) { if (l->ntrig) { const MsnTrig *t0 = &F.trigs[l->first_trig]; W.player.x = t0->center[0]; W.player.y = t0->center[1]; W.player.z = t0->center[2]; }
      else { W.player.x = l->pos[0]; W.player.y = l->pos[1]; W.player.z = l->pos[2]; } tick(0.05f, 2);
      snprintf(d, sizeof d, "игрок в триггере локатора — закрыт (status=%d contains=%d obj=%d interior=%s)", R.status, msn_loc_contains(&F, l, W.player, 6.5f), R.obj, W.interior ? W.interior : "-"); check("outside_in", R.stage != si || R.status != MSN_RUNNING, d); }
  }

  // ---- 6. таймер стоит на переходе (P06-11, P07-26)
  if (firstStage(pTimeout, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
    if (R.timer_on && !R.timer_up) {
      float t0 = R.remaining; W.transition = 1; tick(0.05f, 100);
      snprintf(d, sizeof d, "m%d.%d.%d: 5 с перехода — таймер %.2f → %.2f (стоит), status=%d", mi, ph, si, t0, R.remaining, R.status);
      check("pause", fabsf(R.remaining - t0) < 0.001f && R.status == MSN_RUNNING, d);
      W.transition = 0; tick(0.05f, 20); snprintf(d, sizeof d, "после перехода пошёл: %.2f", R.remaining); check("pause_go", R.remaining < t0 - 0.5f, d);
      R.remaining = 30.f; W.arrested = 1; tick(0.05f, 100);
      snprintf(d, sizeof d, "арест 5 с: таймер ИДЁТ (30 → %.2f), провала по времени нет (status=%d)", R.remaining, R.status);
      check("arrest", R.remaining < 26.f && R.status == MSN_RUNNING, d);
      W.arrested = 0; R.remaining = 1.2f; tick(0.05f, 6);
      snprintf(d, sizeof d, "остаток меньше 1 с (целые секунды) — провал по времени (status=%d fail=%d)", R.status, R.fail);
      check("timeout1", R.status == MSN_FAILED && R.fail == MSN_FAIL_TIMEOUT, d);
    } else check("pause", 0, "у этапа нет обратного таймера");
  } else skip("pause", "нет этапа с timeout");

  // ---- 7. AllowMissionAbort (P06-20, P07-23)
  if (firstStage(pAbortCmd, &mi, &ph, &si, 0)) {
    jump(mi, ph, si);
    snprintf(d, sizeof d, "m%d.%d.%d: AllowMissionAbort — отказаться нельзя (allowed=%d)", mi, ph, si, msn_abort_allowed(&R));
    check("abort", !msn_abort_allowed(&R), d);
    msn_start_mission(&R, mi, 0); snprintf(d, sizeof d, "новый старт миссии — снова можно (allowed=%d)", msn_abort_allowed(&R));
    check("abort_re", msn_abort_allowed(&R) == 1, d);
  } else skip("abort", "нет AllowMissionAbort");

  // ---- 8. buycar по текущей машине (P06-24, P07-15)
  if (firstStage(pBuyCar, &mi, &ph, &si, 0)) {
    const MsnStage *S = &F.stages[F.phases[ph == 0 ? F.missions[mi].intro : F.missions[mi].main].first_stage + si];
    const char *want = msn_str(&F, S->obj_arg);
    jump(mi, ph, si); ownsAll = 1; world(0, "other_v", NULL); runToRunning(); tick(0.05f, 3);
    snprintf(d, sizeof d, "m%d.%d.%d: куплено %s, но в машине other_v — этап не закрыт (stage=%d status=%d)", mi, ph, si, want, R.stage, R.status);
    check("buy_not", R.stage == si && R.status == MSN_RUNNING, d);
    world(0, want, NULL); tick(0.05f, 3);
    snprintf(d, sizeof d, "села машина %s — этап закрыт (stage=%d)", want, R.stage);
    check("buy_cur", R.stage != si || R.status != MSN_RUNNING, d);
  } else skip("buy_not", "нет buycar в уровне");

  printf("ИТОГ %d проверок, %d красных, %d пропущено (нет в этом уровне)\n", nchecks, fails, skips);
  return fails ? 1 : 0;
}
