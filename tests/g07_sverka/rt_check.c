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
static char lastText[96], lastName[64], lastHint[160]; static MsnVec3 markPos; static int nMark;
static int ownsAll; static int entOn; static MsnEntity entE;
static void onEvent(void *u, const MsnEvent *e) {
  (void)u;
  if (nev < MAXEV) { evType[nev] = e->type; evIdx[nev] = e->index; evVal[nev] = e->value; nev++; }
  if (e->type == MSN_EV_STAGE_START) snprintf(lastText, sizeof lastText, "%s", e->text ? e->text : "");
  if (e->type == MSN_EV_MARKER) { markPos = e->pos; nMark++; }
  if (e->type == MSN_EV_MISSION_FAILED) snprintf(lastHint, sizeof lastHint, "%s", e->name2 ? e->name2 : "");
  if (e->type == MSN_EV_LOCKED_HINT) snprintf(lastName, sizeof lastName, "%s", e->name ? e->name : "");
}
static void onCommand(void *u, const MsnCall *c) { (void)u; (void)c; }
static int onEntity(void *u, const char *name, MsnEntity *out) { (void)u; (void)name; if (!entOn) return 0; *out = entE; return 1; }
static int onOwns(void *u, int kind, const char *name) { (void)u; (void)kind; (void)name; return ownsAll; }
static int countEv(int type) { int n = 0; for (int i = 0; i < nev; i++) if (evType[i] == type) n++; return n; }

static int bonusAt = -1;   // у какой бонус-миссии персонаж стоит в начале координат (у остальных его в мире нет)
static int onBonusEntity(void *u, int mission, const char *name, MsnEntity *out) { (void)u; (void)name; if (mission != bonusAt) return 0; memset(out, 0, sizeof *out); out->health = 1.f; out->waypoint = -1; return 1; }   // бонус-персонаж стоит в начале координат
static MsnFile F; static MsnRuntime R;
static void openRt(void) {
  MsnHost h; memset(&h, 0, sizeof h); h.event = onEvent; h.command = onCommand; h.entity = onEntity; h.owns = onOwns; h.bonus_entity = onBonusEntity;
  msn_init(&R, &F, &h); nev = 0; ownsAll = 0; entOn = 0; lastText[0] = lastName[0] = 0;
}
static MsnWorld W;
static void world(int onFoot, const char *veh, const char *interior) {
  memset(&W, 0, sizeof W); W.on_foot = onFoot; W.vehicle = veh; W.vehicle_health = 1.f; W.interior = interior; W.player.x = W.player.z = -5000.f; W.coins = 1000;
}
static void tick(float dt, int n) { for (int i = 0; i < n; i++) msn_tick(&R, dt, &W); }
// Прыжок на (миссия, фаза, этап)
static int jumpOwns;   // 1 — игрок уже владеет вещью до входа в этап (владение должно быть известно в момент входа, а не после)
static void jump(int mission, int phase, int stage) {
  openRt(); ownsAll = jumpOwns; R.completed = 0;
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


// ---- круг 6 (02.11.2026): добавки к проверкам — ирис, SetFadeOut, SetLevelOver, цель-до-условий, взрыв при условии damage, interior, взнос ставки, случайные подсказки
static int hasOpS(const MsnStage *S, int op) { for (unsigned k = 0; k < S->cmd_count; k++) if (F.cmds[S->cmd_first + k].op == op) return 1; return 0; }
static int pIris(const MsnStage *S, int i) { (void)i; return hasOpS(S, MSN_OP_SETIRISWIPE) && (S->obj == MSN_OBJ_DIALOGUE || S->obj == MSN_OBJ_GOTO || S->obj == MSN_OBJ_TIMER); }
static int pFade(const MsnStage *S, int i) { (void)i; return hasOpS(S, MSN_OP_SETFADEOUT) && (S->obj == MSN_OBJ_DIALOGUE || S->obj == MSN_OBJ_GOTO || S->obj == MSN_OBJ_TIMER); }
static int pLevelOver(const MsnStage *S, int i) { (void)i; return hasOpS(S, MSN_OP_SETLEVELOVER) || hasOpS(S, MSN_OP_SETGAMEOVER); }
static int pGotoTimeout(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_GOTO && hasCond(S, MSN_COND_TIMEOUT); }
static int pDamage(const MsnStage *S, int i) { (void)i; return hasCond(S, MSN_COND_DAMAGE) && S->obj != MSN_OBJ_DIALOGUE; }
static int pInterior(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_INTERIOR; }
static int pCoins(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_COINS; }
// довести цель этапа до выполнения (диалог, ролик, точка, таймер); 1 — сделано
static int finish(void) {
  switch (R.obj) {
    case MSN_OBJ_DIALOGUE: msn_notify(&R, MSN_IN_DIALOGUE_DONE, NULL, 0); return 1;
    case MSN_OBJ_TIMER: R.t_stage = R.duration + 1.f; return 1;
    case MSN_OBJ_GOTO: { const MsnLoc *l = R.c_dest ? msn_arg_loc(&F, R.c_dest, 0) : NULL; if (!l) return 0;
      if (l->ntrig) { const MsnTrig *t0 = &F.trigs[l->first_trig]; W.player.x = t0->center[0]; W.player.y = t0->center[1]; W.player.z = t0->center[2]; }
      else { W.player.x = l->pos[0]; W.player.y = l->pos[1]; W.player.z = l->pos[2]; }
      return 1; }
    default: return 0;
  }
}
static int evIndexOf(int type, int value) { for (int i = 0; i < nev; i++) if (evType[i] == type && evIdx[i] == value) return 1; return 0; }
static int pDump(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_DUMP; }
// цель dump: 0 — без привязок (удар машиной), 1 — с привязками к путевым точкам; найти этап и встать на него с живой целью
static int dumpStage(int wantBind, int *mi, int *ph, int *si) {
  for (int skip = 0; skip < 12; skip++) {
    if (!firstStage(pDump, mi, ph, si, skip)) return 0;
    jump(*mi, *ph, *si); world(0, "a_v", NULL); entOn = 1; memset(&entE, 0, sizeof entE); entE.health = 1.f; entE.waypoint = -1; entE.pos.x = 400.f; W.player.x = 330.f; W.player.z = 0.f; W.player.y = 0.f;   // игрок в 70 м за целью: следование не рвётся, предмет сам не подбирается
    runToRunning();
    if ((R.nbind > 0) == wantBind && R.nitems > 0) return 1;
  }
  return 0;
}
// как dumpStage, но по числу предметов у цели без привязок: 1 или больше одного
static int dumpStageN(int single, int *mi, int *ph, int *si) {
  for (int skip = 0; skip < 12; skip++) {
    if (!firstStage(pDump, mi, ph, si, skip)) return 0;
    jump(*mi, *ph, *si); world(0, "a_v", NULL); entOn = 1; memset(&entE, 0, sizeof entE); entE.health = 1.f; entE.waypoint = -1; entE.pos.x = 400.f; W.player.x = 330.f; W.player.z = 0.f; W.player.y = 0.f;
    runToRunning();
    if (R.nbind == 0 && ((R.nitems == 1) == single) && R.nitems > 0) return 1;
  }
  return 0;
}
static void dumpChecks(void) {
  int mi, ph, si; char d[260];
  // ---- строки 21, 44 (P07-10, 5.9): удар по машине-цели без привязок — предмет выпадает через 1 с после удара; лежит 15 с, мигает с 10-й, потом пропадает
  if (dumpStage(0, &mi, &ph, &si)) {
    msn_notify(&R, MSN_IN_HIT_VEHICLE, R.target, 0); tick(0.05f, 10);
    int early = countEv(MSN_EV_DROP);
    tick(0.05f, 14);
    snprintf(d, sizeof d, "m%d.%d.%d: после удара: 0,5 с — выпало %d, через 1,2 с — выпало %d (ждём 0 и 1)", mi, ph, si, early, countEv(MSN_EV_DROP));
    check("dump_delay", early == 0 && countEv(MSN_EV_DROP) == 1, d);
    tick(0.05f, 160);   // 8 с лежит: до мигания
    int b0 = countEv(MSN_EV_DROP_BLINK); tick(0.05f, 60);   // 11,2 с
    int b1 = countEv(MSN_EV_DROP_BLINK), e1 = countEv(MSN_EV_DROP_EXPIRE); tick(0.05f, 90);   // 15,7 с
    snprintf(d, sizeof d, "лежит: мигание с 10 с (было %d → %d), исчезает к 15 с (было %d → %d)", b0, b1, e1, countEv(MSN_EV_DROP_EXPIRE));
    check("dump_life", b0 == 0 && b1 == 1 && e1 == 0 && countEv(MSN_EV_DROP_EXPIRE) == 1, d);
  } else skip("dump_delay", "в этом уровне нет dump без привязок");
  // ---- строки 21, 44 (круг 7): один предмет — удар не роняет, предмет выпадает, когда цель разбита (через 1 с)
  if (dumpStageN(1, &mi, &ph, &si)) {
    msn_notify(&R, MSN_IN_HIT_VEHICLE, R.target, 0); tick(0.05f, 40);
    int afterHit = countEv(MSN_EV_DROP);
    entE.health = 0.f; tick(0.05f, 10); int early = countEv(MSN_EV_DROP); tick(0.05f, 14);
    snprintf(d, sizeof d, "m%d.%d.%d: один предмет: после удара выпало %d (ждём 0); цель разбита: через 0,5 с %d, через 1,2 с %d (ждём 0 и 1)", mi, ph, si, afterHit, early, countEv(MSN_EV_DROP));
    check("dump_single", afterHit == 0 && early == 0 && countEv(MSN_EV_DROP) == 1, d);
  } else skip("dump_single", "нет dump без привязок с одним предметом");
  // ---- строка 22 (P07-11): с привязкой к путевой точке — не сразу, а через случайную задержку 0..5 с после того, как цель прошла точку
  if (dumpStage(1, &mi, &ph, &si)) {
    int wp = R.bind_wp[0]; entE.waypoint = wp; tick(0.05f, 1); 
    int now = countEv(MSN_EV_DROP); tick(0.05f, 105);   // 5,3 с
    snprintf(d, sizeof d, "m%d.%d.%d: цель прошла точку %d: в тот же кадр выпало %d (ждём 0), через 5,3 с — %d (ждём 1)", mi, ph, si, wp, now, countEv(MSN_EV_DROP));
    check("dump_bind", now == 0 && countEv(MSN_EV_DROP) >= 1, d);
    // ---- строка 21–22 (круг 7): выпавший предмет дальше 200 м от игрока — провал; ближе — нет
    if (R.released[R.bind_item[0]] || countEv(MSN_EV_DROP) >= 1) {
      int bi = -1; for (int j = 0; j < R.nitems; j++) if (R.released[j] && !R.done[j]) { bi = j; break; }
      if (bi >= 0) { MsnVec3 rp = R.relpos[bi]; W.player.x = rp.x + 150.f; W.player.y = rp.y; W.player.z = rp.z; entE.pos.x = rp.x + 140.f; tick(0.05f, 2);
        int ok150 = R.status != MSN_FAILED;
        W.player.x = rp.x + 230.f; entE.pos.x = rp.x + 220.f; tick(0.05f, 2);
        snprintf(d, sizeof d, "предмет %d выпал: игрок в 150 м — идём (%d), в 230 м — провал collect (status=%d fail=%d)", bi, ok150, R.status, R.fail);
        check("dump_far", ok150 && R.status == MSN_FAILED && R.fail == MSN_FAIL_COLLECT, d); } }
  } else skip("dump_bind", "в этом уровне нет dump с привязками");
}
static int pAddTime(const MsnStage *S, int i) { (void)i; return hasOpS(S, MSN_OP_ADDSTAGETIME) && !hasOpS(S, MSN_OP_SETSTAGETIME) && S->obj != MSN_OBJ_DIALOGUE; }
static int pSetTime(const MsnStage *S, int i) { (void)i; return hasOpS(S, MSN_OP_SETSTAGETIME) && !hasOpS(S, MSN_OP_ADDSTAGETIME) && S->obj != MSN_OBJ_DIALOGUE; }
static float stageTimeArg(const MsnStage *S, int op) { for (unsigned k = 0; k < S->cmd_count; k++) if (F.cmds[S->cmd_first + k].op == op) return msn_arg_f(&F, &F.cmds[S->cmd_first + k], 0); return 0.f; }
static int pDelivery2(const MsnStage *S, int i) { (void)i; return S->obj == MSN_OBJ_DELIVERY; }
static void circle6(void) {
  dumpChecks();
  { int mi, ph, si; char d[260];
    // ---- строки 55, 66 (P06-7, P07-35): повтор после провала — таймер от остатка прошлого прогона: SetStageTime заменяет, AddStageTime прибавляется к остатку
    if (firstStage(pAddTime, &mi, &ph, &si, 0)) {
      const MsnStage *S = &F.stages[F.phases[ph == 0 ? F.missions[mi].intro : F.missions[mi].main].first_stage + si]; float add = stageTimeArg(S, MSN_OP_ADDSTAGETIME);
      openRt(); msn_start_mission(&R, mi, 0); R.cp_phase = ph; R.cp_stage = si; R.cp_timer = 0; R.cp_remaining = 0.f; R.timer_on = 1; R.remaining = 7.f; msn_retry(&R, 0);   /* 7 с осталось на момент провала */
      snprintf(d, sizeof d, "m%d.%d.%d: AddStageTime %.0f, остаток на момент провала 7 с — таймер %.1f (ждём %.1f)", mi, ph, si, add, R.remaining, 7.f + add);
      check("retry_add", fabsf(R.remaining - (7.f + add)) < .01f, d);
    } else skip("retry_add", "нет AddStageTime");
    if (firstStage(pSetTime, &mi, &ph, &si, 0)) {
      const MsnStage *S = &F.stages[F.phases[ph == 0 ? F.missions[mi].intro : F.missions[mi].main].first_stage + si]; float set = stageTimeArg(S, MSN_OP_SETSTAGETIME);
      openRt(); msn_start_mission(&R, mi, 0); R.cp_phase = ph; R.cp_stage = si; R.cp_timer = 0; R.cp_remaining = 0.f; R.timer_on = 1; R.remaining = 7.f; msn_retry(&R, 0);   /* 7 с осталось на момент провала */
      snprintf(d, sizeof d, "m%d.%d.%d: SetStageTime %.0f, остаток на момент провала 7 с — таймер %.1f (значение заменяется)", mi, ph, si, set, R.remaining);
      check("retry_set", fabsf(R.remaining - set) < .01f, d);
    } else skip("retry_set", "нет SetStageTime");
    // ---- строка P06-5 (круг 2): старт миссии — точка повтора на первом этапе-чекпоинте вступления
    for (uint32_t m = 0; m < F.nmissions; m++) { unsigned idx = F.missions[m].intro; if (idx == MSN_NONE16) continue; const MsnPhase *P = &F.phases[idx]; int cs = -1;
      for (unsigned k = 0; k < P->stage_count; k++) if (F.stages[P->first_stage + k].flags & MSN_SF_CHECKPOINT) { cs = (int)k; break; }
      if (cs > 0) { openRt(); msn_start_mission(&R, (int)m, 0);
        snprintf(d, sizeof d, "m%u: первый чекпоинт вступления — этап %d, точка повтора после старта: этап %d", m, cs, R.cp_stage);
        check("cp5", R.cp_stage == cs, d); break; } }
  }
  // ---- строки P06-1, P06-2 (круг 2): бонус-миссия доступна на любом этапе вступления; пройденная — повторно, если 7-й аргумент AddBonusMissionNPC не 1
  { int story = R.story, bi = -1, once1 = -1, rep = -1; char d[260]; openRt(); story = -1; for (uint32_t m = 0; m < F.nmissions && story < 0; m++) if (!(F.missions[m].flags & MSN_MF_BONUS)) story = (int)m;   // первая сюжетная миссия
    for (uint32_t m = 0; m < F.nmissions; m++) { const MsnMission *M = &F.missions[m]; if (!(M->flags & MSN_MF_BONUS) || M->bonus_npc == 0xFFFFFFFFu) continue;
      if (bi < 0) bi = (int)m;
      if (msn_arg_i(&F, &F.cmds[M->bonus_npc], 6) == 1) { if (once1 < 0) once1 = (int)m; } else if (rep < 0) rep = (int)m; }
    unsigned nst = story >= 0 && F.missions[story].intro != MSN_NONE16 ? F.phases[F.missions[story].intro].stage_count : 0;
    if (bi >= 0 && nst > 1) {
      jump(story, 0, 1); world(1, NULL, NULL);
      snprintf(d, sizeof d, "вступление сюжетной миссии, этап 2 из %u: бонус-миссия %d доступна (ответ %d)", nst, bi, msn_start_bonus(&R, bi));
      check("bonus_intro", R.cur == bi, d);
    } else skip("bonus_intro", "нет бонус-миссии или вступления из нескольких этапов");
    if (once1 >= 0) { openRt(); msn_start_mission(&R, story, 0); runToRunning(); bonusAt = once1; R.completed = 1u << once1; world(1, NULL, NULL); W.player.x = W.player.y = W.player.z = 0.f;
      snprintf(d, sizeof d, "пройденная бонус-миссия %d с 7-м аргументом 1: повторно не предлагается (рядом: %d)", once1, msn_bonus_near(&R, &W));
      check("bonus_once", msn_bonus_near(&R, &W) != once1, d); } else skip("bonus_once", "нет бонус-миссии с 7-м аргументом 1");
    if (rep >= 0) { openRt(); msn_start_mission(&R, story, 0); runToRunning(); bonusAt = rep; R.completed = 1u << rep; world(1, NULL, NULL); W.player.x = W.player.y = W.player.z = 0.f;
      snprintf(d, sizeof d, "пройденная бонус-миссия %d без 1 в 7-м аргументе: предлагается повторно (рядом: %d)", rep, msn_bonus_near(&R, &W));
      check("bonus_repeat", msn_bonus_near(&R, &W) == rep, d); } else skip("bonus_repeat", "нет повторяемой бонус-миссии"); }
  // ---- строка 27 (P07-16): цель с несколькими предметами — метка к ПЕРВОМУ неподобранному по порядку скрипта, не к ближайшему
  { int mi, ph, si, k = 0; char d[260]; int found = 0;
    for (k = 0; k < 10 && !found; k++) {
      if (!firstStage(pDelivery2, &mi, &ph, &si, k)) break;
      jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
      if (R.nitems >= 2) { const MsnLoc *l0 = msn_arg_loc(&F, R.items[0], 0), *l1 = msn_arg_loc(&F, R.items[1], 0);
        if (l0 && l1 && (fabsf(l0->pos[0] - l1->pos[0]) + fabsf(l0->pos[2] - l1->pos[2])) > 20.f) {
          W.player.x = l1->pos[0]; W.player.y = l1->pos[1]; W.player.z = l1->pos[2] + 1.f; nMark = 0; tick(0.05f, 6);
          snprintf(d, sizeof d, "m%d.%d.%d: игрок у предмета 2, метка идёт к предмету 1 (метка %.0f,%.0f; предмет 1 %.0f,%.0f; предмет 2 %.0f,%.0f)", mi, ph, si, markPos.x, markPos.z, l0->pos[0], l0->pos[2], l1->pos[0], l1->pos[2]);
          check("talkorder", fabsf(markPos.x - l0->pos[0]) < 1.f && fabsf(markPos.z - l0->pos[2]) < 1.f, d); found = 1; } } }
    if (!found) skip("talkorder", "нет цели delivery с двумя предметами"); }
  int mi, ph, si; char d[260];
  // ---- строка 12 (P06-10): SetIrisWipe — по концу этапа ирис закрывается (событие 1), смена этапа ждёт 1 с, следующий этап открывает ирис (событие 0)
  if (firstStage(pIris, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
    if (!finish()) skip("iris", "этап с ирисом: цель не из простых"); else {
      tick(0.05f, 3);
      snprintf(d, sizeof d, "m%d.%d.%d: конец этапа — ирис закрыт (событий закрытия %d), этап держит паузу %.2f с", mi, ph, si, evIndexOf(MSN_EV_IRIS, 1) ? countEv(MSN_EV_IRIS) : 0, R.delay);
      check("iris", evIndexOf(MSN_EV_IRIS, 1) && R.iris_closed && R.delay >= 0.9f, d);
      tick(0.05f, 70);
      unsigned nst = F.phases[ph == 0 ? F.missions[mi].intro : F.missions[mi].main].stage_count;
      if ((unsigned)si + 1 >= nst) skip("iris_open", "этап с ирисом последний в фазе — следующего этапа нет"); else {
      snprintf(d, sizeof d, "следующий этап стартовал под закрытым ирисом и открыл его (событий открытия %d, закрыт=%d)", evIndexOf(MSN_EV_IRIS, 0), R.iris_closed);
      check("iris_open", evIndexOf(MSN_EV_IRIS, 0) && !R.iris_closed, d); } }
  } else skip("iris", "в этом уровне нет SetIrisWipe");
  // ---- SetFadeOut (P06-9, круг 2): число не читается, пауза 2,25 с
  if (firstStage(pFade, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
    if (finish()) { tick(0.05f, 1); snprintf(d, sizeof d, "m%d.%d.%d: SetFadeOut — пауза до следующего этапа %.2f с (ждём 2,25 минус один кадр)", mi, ph, si, R.delay);
      check("fade", R.delay > 2.15f && R.delay < 2.3f, d); } else skip("fade", "этап с SetFadeOut: цель не из простых");
  } else skip("fade", "в этом уровне нет SetFadeOut");
  // ---- строка 51 (P06-22): SetLevelOver / SetGameOver срабатывают на СТАРТЕ этапа
  if (firstStage(pLevelOver, &mi, &ph, &si, 0)) {
    jump(mi, ph, si);
    snprintf(d, sizeof d, "m%d.%d.%d: на старте этапа с SetLevelOver/GameOver событие «уровень завершён» %d раз", mi, ph, si, countEv(MSN_EV_LEVEL_OVER_NOW));
    check("lvlover", countEv(MSN_EV_LEVEL_OVER_NOW) == 1, d);
  } else skip("lvlover", "в этом уровне нет SetLevelOver");
  // ---- строка 60 (P07-28): цель и условие в один кадр — цель побеждает
  if (firstStage(pGotoTimeout, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
    if (R.timer_on && !R.timer_up && finish()) { R.remaining = 0.f; tick(0.05f, 1);
      snprintf(d, sizeof d, "m%d.%d.%d: в одном кадре время вышло и цель достигнута — этап пройден, не провал (status=%d stage=%d fail=%d)", mi, ph, si, R.status, R.stage, R.fail);
      check("order", R.status != MSN_FAILED, d);
    } else skip("order", "нет этапа с целью и обратным таймером");
  } else skip("order", "нет этапа goto + timeout");
  // ---- строки 24, 86 (P07-13): условие damage — провал по КОНЦУ взрыва машины, пока взрыв идёт, миссия не проиграна
  if (firstStage(pDamage, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); tick(0.05f, 2);
    W.vehicle_health = 0.f; W.vehicle_boom = 1; tick(0.05f, 3);
    snprintf(d, sizeof d, "m%d.%d.%d: прочность 0, взрыв идёт — ещё не провал (status=%d)", mi, ph, si, R.status);
    check("dmg_wait", R.status == MSN_RUNNING, d);
    W.vehicle_boom = 2; tick(0.05f, 3);
    snprintf(d, sizeof d, "взрыв доигран — провал damage (status=%d fail=%d)", R.status, R.fail);
    check("dmg_fail_info", R.status == MSN_FAILED && R.fail == MSN_FAIL_DAMAGE, d);
  } else skip("dmg_wait", "в этом уровне нет условия damage");
  // ---- строка 64 (P07-32): interior закрывается в момент НАЧАЛА перехода в нужный интерьер
  if (firstStage(pInterior, &mi, &ph, &si, 0)) {
    const char *want = ""; jump(mi, ph, si); world(0, "a_v", NULL); runToRunning(); if (R.c_dest) want = msn_arg_s(&F, R.c_dest, 0);
    W.entering = want; W.transition = 1; tick(0.05f, 3);
    snprintf(d, sizeof d, "m%d.%d.%d: переход в «%s» начался (игрок ещё снаружи) — этап закрыт (stage=%d status=%d)", mi, ph, si, want, R.stage, R.status);
    check("interior", R.stage != si || R.status != MSN_RUNNING, d);
  } else skip("interior", "в этом уровне нет interior");
  // ---- строка 29 (P07-22): взнос ставки: монет не хватает — отказ и миссия сразу отменена
  if (firstStage(pCoins, &mi, &ph, &si, 0)) {
    jump(mi, ph, si); world(0, "a_v", NULL); runToRunning();
    if (R.fee > 0) { int fee = (int)R.fee; W.coins = fee - 1; W.action = 1; tick(0.05f, 1);
      snprintf(d, sizeof d, "m%d.%d.%d: взнос %d, монет %d — отказ (событий %d), миссия отменена (очистка мира, событий %d)", mi, ph, si, fee, (int)W.coins, countEv(MSN_EV_WAGER_DENIED), countEv(MSN_EV_CLEANUP));
      check("coins_deny", countEv(MSN_EV_WAGER_DENIED) == 1 && countEv(MSN_EV_CLEANUP) >= 1, d);
    } else skip("coins_deny", "взнос 0");
  } else skip("coins_deny", "в этом уровне нет этапа coins");
  // ---- строки 48, 70 (P06-19, P13-14): подсказка провала — случайная из первых N, не по кругу
  if (firstStage(pTimeout, &mi, &ph, &si, 0)) {
    int seq[24], n = 0, same = 0, avail = 0; jump(mi, ph, si); world(0, "a_v", NULL);
    while (avail < 8 && *msn_fail_hint(&F, MSN_FAIL_TIMEOUT, avail)) avail++;
    for (int t = 0; t < 24 && avail > 1; t++) {
      msn_retry(&R, 0); world(0, "a_v", NULL); runToRunning(); R.remaining = .4f; tick(0.05f, 4);
      if (R.status != MSN_FAILED) break;
      int k = -1; for (int j = 0; j < avail; j++) if (!strcmp(lastHint, msn_fail_hint(&F, MSN_FAIL_TIMEOUT, j))) { k = j; break; }
      seq[n++] = k;
    }
    for (int t = 1; t < n; t++) same += seq[t] == (seq[0] + t) % avail;
    snprintf(d, sizeof d, "подсказок %d, провалов подряд %d: идут по кругу в %d из %d шагов (по кругу было бы во всех)", avail, n, same, n - 1);
    check("hint_rand", avail > 1 && n >= 20 && same < n - 1, d);
  } else skip("hint_rand", "нет этапа с timeout");
}

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
    jumpOwns = 1; jump(mi, ph, si); jumpOwns = 0;                   // куплено до входа в этап, но в машине — другая
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

  circle6();

  printf("ИТОГ %d проверок, %d красных, %d пропущено (нет в этом уровне)\n", nchecks, fails, skips);
  return fails ? 1 : 0;
}
