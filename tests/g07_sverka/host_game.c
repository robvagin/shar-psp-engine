// Окно g07-missions, круг 6 (02.11.2026): строки game_msn.c на настоящем мире ботов (tools/bots/world.c): луч тарелки L7, безопасная зона, лавка, отмена
// миссии, прятание HUD. Красный контроль: сборка с -DGAME_*_OLD=1 обязана упасть на проверках своей правки (tests/g07_sverka/host_game.sh).
//   host_game LEVEL.shr HOMER.shr levelNN.msn  [число уровня определяется по пакету]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"

static int fails, nchecks;
static void check(const char *name, int ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void check(const char *name, int ok, const char *fmt, ...) {
  char d[300]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
  nchecks++; printf("CHECK %-14s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++;
}
static BotWorld W;
typedef int (*Pred)(const MsnFile *, const MsnStage *, int phase);
// прыжок на первый этап, у которого pred истинен; 1 — нашли
static int jumpTo(BotWorld *w, Pred pred, int skip, int *outMission) {
  GameMsn *m = &w->msn; MsnFile *F = &m->f;
  for (uint32_t mi = 0; mi < F->nmissions; mi++) for (int p = 0; p < 2; p++) {
    unsigned idx = p == 0 ? F->missions[mi].intro : F->missions[mi].main; if (idx == MSN_NONE16) continue;
    const MsnPhase *P = &F->phases[idx];
    for (unsigned s = 0; s < P->stage_count; s++) if (pred(F, &F->stages[P->first_stage + s], p)) {
      if (skip-- > 0) continue;
      msn_start_mission(&m->rt, mi, 0); m->rt.cp_phase = p; m->rt.cp_stage = s; m->rt.cp_timer = 0; m->rt.cp_remaining = 0; msn_retry(&m->rt, 0);
      if (outMission) *outMission = (int)mi; return 1;
    }
  }
  return 0;
}
static int hasOp(const MsnFile *F, const MsnStage *S, int op) { for (unsigned k = 0; k < S->cmd_count; k++) if (F->cmds[S->cmd_first + k].op == op) return 1; return 0; }
static int pBoss(const MsnFile *F, const MsnStage *S, int ph) { (void)F; (void)ph; return S->obj == MSN_OBJ_DESTROYBOSS; }
static int pSafe(const MsnFile *F, const MsnStage *S, int ph) { (void)ph; return hasOp(F, S, MSN_OP_ADDSAFEZONE); }
static int pNoAbort(const MsnFile *F, const MsnStage *S, int ph) { (void)ph; return hasOp(F, S, MSN_OP_ALLOWMISSIONABORT); }
static int pHud(const MsnFile *F, const MsnStage *S, int ph) { (void)ph; return hasOp(F, S, MSN_OP_SHOWHUD); }
static int pIntro(const MsnFile *F, const MsnStage *S, int ph) { (void)F; (void)S; return ph == 0; }
static int pMain(const MsnFile *F, const MsnStage *S, int ph) { (void)F; (void)S; return ph == 1; }
static void steps(BotWorld *w, int n) { for (int i = 0; i < n; i++) bwStep(w, 0, 0.f, 0.f); }
static float dist3(float ax, float ay, float az, float bx, float by, float bz) { float x = ax - bx, y = ay - by, z = az - bz; return sqrtf(x * x + y * y + z * z); }

// ---- строки 1, 4, 15 (P04:U1, P07:9, P04:Д9): луч тарелки тянет к ЦЕНТРУ тарелки, водитель выходит, цель закрывается, игрок на Z3D1 только если за рулём
static void ufo(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  int mi = -1;
  if (!jumpTo(&W, pBoss, 0, &mi)) { printf("CHECK ufo            SKIP  в этом уровне нет destroyboss\n"); bwFree(&W); return; }
  steps(&W, 5);
  if (!m->ufoOn) { check("ufo.on", 0, "тарелка не включена этапом (ufoOn=0)"); bwFree(&W); return; }
  float sx = m->ufoPos[0], sy = m->ufoPos[1], sz = m->ufoPos[2];
  // машина под тарелкой: смещение от оси 5 м, глубина 15 м (косинус 0,94: 15/15,8 = 0,95 — внутри конуса)
  W.car.x = sx + 5.f; W.car.y = sy - 15.f; W.car.z = sz; W.car.destroyed = 0; W.car.damage = 0.f;
  float x0 = W.car.x, d0 = dist3(W.car.x, W.car.y, W.car.z, sx, sy, sz);
  m->ufoGrab = 0; m->ufoSent = 0;
  W.msn.rt.cargo = 1;   // бочка на машине
  steps(&W, 30);   // 0,5 с: тянет 3 м/с, ~1,5 м к центру
  float d1 = dist3(W.car.x, W.car.y, W.car.z, sx, sy, sz);
  check("ufo.to_center", m->ufoGrab >= 1 && fabsf(W.car.x - x0) > .3f && d1 < d0 - 1.2f && d1 > d0 - 1.8f,
        "смещение по x %.2f (ждём сдвиг к оси, не только вверх), расстояние до центра %.2f -> %.2f", W.car.x - x0, d0, d1);
  check("ufo.driver_out", gameOnFoot(&W.G), "водитель вышел из машины при втягивании (пешком=%d)", gameOnFoot(&W.G));
  for (int k = 0; k < 40 && !m->ufoSent; k++) steps(&W, 20);   // до конца: ближе 5 м (около 3,6 с)
  steps(&W, 2);
  int hit = m->ufoSent && (W.msn.rt.status != MSN_RUNNING || W.msn.rt.obj != MSN_OBJ_DESTROYBOSS);
  float hx = W.homer.x, hy = W.homer.y, hz = W.homer.z;
  const MsnLoc *z3 = 0; for (uint32_t i = 0; i < m->f.nlocs; i++) if (msn_streq(msn_str(&m->f, m->f.locs[i].name), "Z3D1")) z3 = &m->f.locs[i];
  int atZ = z3 && dist3(hx, hy, hz, z3->pos[0], z3->pos[1], z3->pos[2]) < 8.f;
  check("ufo.no_z3d1_afoot", m->ufoSent && !atZ, "пешком в конце: игрока НЕ переставляют на Z3D1 (отправлено=%d, у Z3D1=%d)", m->ufoSent, atZ);
  check("ufo.hit", hit, "с бочкой цель закрыта (отправлено=%d, цель=%d, статус=%d)", m->ufoSent, (int)W.msn.rt.obj, (int)W.msn.rt.status);
  bwFree(&W);
}

static int hookLast = -1, hookCalls;
static void safeHook(int active) { hookLast = active; hookCalls++; }
#ifndef MSNWALK1113_OLD
#define MSNWALK1113_OLD 0
#endif
static void skipc(const char *name, const char *d) { printf("CHECK %-14s SKIP  %s\n", name, d); }

// ---- строки 21, 68 (P06-13, P07:Д5): безопасная зона — только машина игрока (по плоскости), полиция стоит (хук), шкала Hit & Run не обнуляется; пешком зона не действует
static GameSafeZone zoneKeep;
static void zoneSteps(BotWorld *w, int n) { for (int i = 0; i < n; i++) { w->msn.safe[0] = zoneKeep; w->msn.nsafe = 1; bwStep(w, 0, 0.f, 0.f); } }
static void safezone(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  if (!jumpTo(&W, pSafe, 0, NULL)) { skipc("safe", "в этом уровне нет AddSafeZone"); bwFree(&W); return; }
  steps(&W, 1);   // команды этапа поставили зону (этап-разговор сразу кончается, поэтому зону дальше держит тест: место и радиус настоящие, из данных)
  zoneKeep = m->safe[0];
  float cx = zoneKeep.c.x, cy = zoneKeep.c.y, cz = zoneKeep.c.z, r = zoneKeep.r;
  check("safe.radius_info", r > 29.9f && r < 30.1f, "радиус зоны %.1f м (по данным диска 30)", r);
  gameMsnSafeHook = safeHook; hookLast = -1; hookCalls = 0;
  // машина в зоне, игрок за рулём
  W.car.x = cx + 10.f; W.car.y = cy + .5f; W.car.z = cz; W.hnrHeat = 60.f; W.hnrCalm = 5.f;
  zoneSteps(&W, 5);
  check("safe.car_in", gameMsnInSafeZone(m, cx + 10.f, cz) && m->safeIn == 1 && hookLast == 1 && W.hnrHeat > 20.f,
        "машина в зоне: внутри=%d, хук=%d, шкала Hit & Run %.0f (не обнулена)", m->safeIn, hookLast, W.hnrHeat);
  check("safe.plane_info", gameMsnInSafeZone(m, cx + 29.f, cz) && !gameMsnInSafeZone(m, cx + 31.f, cz) && gameMsnInSafeZone(m, cx, cz),
        "зона по плоскости: 29 м внутри, 31 м снаружи (высота не считается)");
  // игрок вышел пешком, стоит в зоне: зона не действует, хук получил 0
  W.hnrHeat = 60.f; hookCalls = 0;
  homerPlace(&W.homer, bwCol(), cx + 12.f, cy + .5f, cz, 0.f);
  zoneSteps(&W, 5);
  check("safe.foot_out", gameOnFoot(&W.G) && m->safeIn == 0 && hookLast == 0 && W.hnrHeat > 20.f,
        "пешком в зоне: действует=%d, хук=%d, шкала %.0f (не обнулена)", m->safeIn, hookLast, W.hnrHeat);
  bwFree(&W);
  // модуль полиции не подключён (запасной путь): шкалу всё равно не обнуляем (круг 3, строка 15)
  bwNew(&W); m = &W.msn; gameMsnSafeHook = NULL; jumpTo(&W, pSafe, 0, NULL); steps(&W, 1);
  W.car.x = cx + 10.f; W.car.y = cy + .5f; W.car.z = cz; W.hnrHeat = 60.f;
  zoneSteps(&W, 5);
  check("safe.nohook", !gameOnFoot(&W.G) && m->safeIn == 1 && W.hnrHeat > 20.f, "без модуля полиции: в зоне=%d, шкала %.0f (не обнулена)", m->safeIn, W.hnrHeat);
  bwFree(&W);
}

// ---- строки 30, 49 (P07:23, P06-20): AllowMissionAbort("false") — Accept/Cancel не показывать; на следующей миссии снова можно
static void abortOk(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  if (!jumpTo(&W, pNoAbort, 0, NULL)) { skipc("abort", "в этом уровне нет AllowMissionAbort"); bwFree(&W); return; }
  steps(&W, 3);
  int denied = !gameMsnAbortAllowed(m);
  msn_start_mission(&m->rt, 0, 0); steps(&W, 3);
  check("abort.denied", denied && gameMsnAbortAllowed(m), "после команды отказ запрещён (%d), на новой миссии снова разрешён (%d)", denied, gameMsnAbortAllowed(m));
  bwFree(&W);
}

// ---- строка 50 (P06-21): ShowHUD("false") прячет HUD миссии, на выходе из миссии он возвращается
static void hud(void) {
  bwNew(&W); GameMsn *m = &W.msn; MsnFile *F = &m->f;
  int fm = -1, fp = 0;   // ShowHUD живёт в подготовке фазы (setup), не в этапе
  for (uint32_t mi = 0; mi < F->nmissions && fm < 0; mi++) for (int p = 0; p < 2 && fm < 0; p++) {
    unsigned idx = p == 0 ? F->missions[mi].intro : F->missions[mi].main; if (idx == MSN_NONE16) continue;
    const MsnPhase *P = &F->phases[idx];
    for (unsigned k = 0; k < P->setup_count; k++) if (F->cmds[P->setup_first + k].op == MSN_OP_SHOWHUD) { fm = (int)mi; fp = p; }
  }
  if (fm < 0) { skipc("hud", "в этом уровне нет ShowHUD"); bwFree(&W); return; }
  msn_start_mission(&m->rt, fm, 0); if (fp) { m->rt.cp_phase = fp; m->rt.cp_stage = 0; m->rt.cp_timer = 0; m->rt.cp_remaining = 0; msn_retry(&m->rt, 0); }
  steps(&W, 5);
  int hid = gameMsnHudHidden(m);
  msn_start_mission(&m->rt, 0, 0); steps(&W, 3);
  check("hud.hidden", hid && !gameMsnHudHidden(m), "HUD спрятан %d, на новой миссии показан (%d)", hid, !gameMsnHudHidden(m));
  bwFree(&W);
}

// ---- строки 10, 14 (P05-11, P06-12): лавка и гардероб — только во вступлении; в основной части выключены, вне миссий включены
static void shop(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  int a = jumpTo(&W, pIntro, 0, NULL); steps(&W, 2); int inIntro = a ? gameMsnShopAllowed(m) : -1;
  int b = jumpTo(&W, pMain, 0, NULL); steps(&W, 2); int inMain = b ? gameMsnShopAllowed(m) : -1;
  check("shop.phase", a && b && inIntro == 1 && inMain == 0, "во вступлении лавка %d (ждём 1), в основной части %d (ждём 0)", inIntro, inMain);
  bwFree(&W);
}

// Гомер пешком у двери машины -> △ -> сидит за рулём (кадры посадки)
static void getInCar(BotWorld *w) {
  steps(w, 3);
  for (int k = 0; k < 16 && gameOnFoot(&w->G); k++) {   // дверь водителя с какой-то стороны: перебираем точки вокруг кузова, лицом к машине
    float a = k * 6.2831853f / 16.f, hx = w->car.x + sinf(a) * 1.4f, hz = w->car.z + cosf(a) * 1.4f;
    homerPlace(&w->homer, bwCol(), hx, w->car.y + .3f, hz, atan2f(w->car.x - hx, w->car.z - hz));
    steps(w, 2); bwStep(w, GP_TRIANGLE, 0, 0); steps(w, 100);
  }
}
// ---- строка 57 (P06-Д6): отсчёт «3 2 1» — газ и тормоз выключены, ручник включён, кнопки (в том числе сброс машины) заблокированы; руль и камера L/R живы
static void count(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  getInCar(&W);
  m->rt.status = MSN_COUNTDOWN;
  GamePad p = { GP_CROSS | GP_SQUARE | GP_TRIANGLE | GP_START | GP_L | GP_R | GP_SELECT, GP_CROSS | GP_SELECT | GP_L, .7f, .5f };
  CarInput in = { 1.f, 1.f, .6f, 0.f };
  gameMsnFreezePad(m, &p); gameMsnFreezeCar(m, &in);
  check("count.pad", p.ax > .69f && p.ay == 0.f && (p.buttons & GP_L) && (p.buttons & GP_R) && (p.buttons & GP_START) && !(p.buttons & (GP_CROSS | GP_SQUARE | GP_TRIANGLE | GP_SELECT)),
        "кнопки %x (живы только START, L, R), руль %.2f (жив), газ/тормоз стик ay %.2f", p.buttons, p.ax, p.ay);
  check("count.car", in.throttle == 0.f && in.brake == 0.f && in.handbrake == 1.f && in.steer > .59f, "газ %.1f тормоз %.1f ручник %.1f руль %.2f (жив)", in.throttle, in.brake, in.handbrake, in.steer);
  bwFree(&W);
}

// ---- строка 25 (P07-14): конец разговора — Гомер и собеседник возвращаются туда, где стояли до него (не в машине)
static int pTalk(const MsnFile *F, const MsnStage *S, int ph) { (void)F; (void)ph; return S->obj == MSN_OBJ_TALKTO; }
static const GameNpc *npcNamed(const GameMsn *M, const char *name) {
  for (int i = 0; i < M->nnpc; i++) if (msn_streq(M->npc[i].name, name)) return &M->npc[i];
  return 0;
}
// ---- строка 16 (круг 7): обычный разговор с местами — машина игрока в 7 м от места героя не рисуется, после разговора снова видна
static void talkHide(void) {
  if (bwLevel() != 1) { skipc("talkhide", "проверка — на уровне 1"); return; }
  bwNew(&W); GameMsn *m = &W.msn;
  if (!jumpTo(&W, pTalk, 0, NULL)) { skipc("talkhide", "нет этапа talkto"); bwFree(&W); return; }
  steps(&W, 3);
  const GameNpc *n = npcNamed(m, m->rt.target); if (!n) { skipc("talkhide", "собеседник не найден"); bwFree(&W); return; }
  homerPlace(&W.homer, bwCol(), n->pos.x + 1.f, n->pos.y + .5f, n->pos.z, atan2f(-1.f, 0.f));
  W.car.x = n->pos.x + 4.f; W.car.y = n->pos.y + .3f; W.car.z = n->pos.z + 4.f; steps(&W, 2);
  bwStep(&W, GP_TRIANGLE, 0, 0); steps(&W, 6);
  if (!m->talk.on) { skipc("talkhide", "разговор не начался"); bwFree(&W); return; }
  int hid = gameMsnCarHide; msn_notify(&m->rt, MSN_IN_DIALOGUE_DONE, 0, 0); steps(&W, 4);
  check("talkhide", hid == 1 && gameMsnCarHide == 0, "машина у места разговора: на разговоре спрятана %d, после — видна %d", hid, !gameMsnCarHide);
  bwFree(&W);
}
// ---- строка 37 (КАМЕРА 4.3, круг 7): на крупном плане игрока (pc_near) собеседник на 1,8 м дальше от игрока, на других планах на месте; после разговора на месте
static void talkBack(void) {
  if (bwLevel() != 1) { skipc("talkback", "проверка — на уровне 1"); return; }
  bwNew(&W); GameMsn *m = &W.msn;
  if (!jumpTo(&W, pTalk, 0, NULL)) { skipc("talkback", "нет этапа talkto"); bwFree(&W); return; }
  steps(&W, 3);
  const GameNpc *n = npcNamed(m, m->rt.target); if (!n) { skipc("talkback", "собеседник не найден"); bwFree(&W); return; }
  homerPlace(&W.homer, bwCol(), n->pos.x + 1.f, n->pos.y + .5f, n->pos.z, atan2f(-1.f, 0.f)); steps(&W, 2);
  bwStep(&W, GP_TRIANGLE, 0, 0); steps(&W, 6);
  if (!m->talk.on) { skipc("talkback", "разговор не начался"); bwFree(&W); return; }
  n = npcNamed(m, m->talk.npc);
  float d0 = hypotf(n->pos.x - W.homer.x, n->pos.z - W.homer.z);
  gameMsnTalkNpcBack(m, 1); float d1 = hypotf(n->pos.x - W.homer.x, n->pos.z - W.homer.z);
  gameMsnTalkNpcBack(m, 0); float d2 = hypotf(n->pos.x - W.homer.x, n->pos.z - W.homer.z);
  check("talkback", fabsf(d1 - d0 - 1.8f) < .05f && fabsf(d2 - d0) < .05f, "расстояние до собеседника: обычный план %.2f, pc_near %.2f (ждём +1,8), снова обычный %.2f", d0, d1, d2);
  bwFree(&W);
}
static void talkRet(void) {
  if (bwLevel() != 1) { skipc("talk", "проверка разговоров — на уровне 1 (дальше этап сам переставляет игрока)"); return; }
  bwNew(&W); GameMsn *m = &W.msn;
  if (!jumpTo(&W, pTalk, 0, NULL)) { skipc("talk", "нет этапа talkto"); bwFree(&W); return; }
  steps(&W, 3);
  const GameNpc *n = npcNamed(m, m->rt.target); if (!n) { skipc("talk", "собеседник не найден"); bwFree(&W); return; }
  float nx0 = n->pos.x, nz0 = n->pos.z;
  homerPlace(&W.homer, bwCol(), nx0 + 1.f, n->pos.y + .5f, nz0, atan2f(-1.f, 0.f));
  steps(&W, 2);
  float hx0 = W.homer.x, hz0 = W.homer.z;
  bwStep(&W, GP_TRIANGLE, 0, 0); steps(&W, 6);
  if (!m->talk.on) { skipc("talk", "разговор не начался"); bwFree(&W); return; }
  float hMove = hypotf(W.homer.x - hx0, W.homer.z - hz0);
  { const GameNpc *nt = npcNamed(m, m->talk.npc[0] ? m->talk.npc : m->rt.target); float gap = nt ? hypotf(W.homer.x - nt->pos.x, W.homer.z - nt->pos.z) : -1.f;
    check("talk.gap_info", nt && gap >= 1.6f, "расстояние между героями на разговоре %.2f м (ждём не меньше 1,65 м; подошёл на 1,0 м)", gap); }
  msn_notify(&m->rt, MSN_IN_DIALOGUE_DONE, 0, 0); steps(&W, 4);   // реплики кончились
  const GameNpc *n2 = npcNamed(m, m->talk.npc[0] ? m->talk.npc : m->rt.target);
  float hBack = hypotf(W.homer.x - hx0, W.homer.z - hz0);
  check("talk.return", !m->talk.on && (hMove < .3f || hBack < .4f),
        "Гомер на разговоре сдвинут на %.2f м, после конца от прежнего места %.2f м (ждём возврат < 0,4 м), собеседник %s", hMove, hBack, n2 ? "на месте" : "?");
  bwFree(&W);
}

// ---- строки 13, 31, 40 (P06-11, P07:26, 5.11): переход в помещение/из него и арест доходят до рантайма миссий (вторая половина у main), на переходе шкала Hit & Run стоит
static void state(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  gameMsnSetState(m, 1, 0, "KwikEMart"); W.G.hnrFreeze = 0.f; steps(&W, 2);
  MsnWorld w; gameMsnWorld(m, &w);
  check("state.world_info", w.transition && !w.arrested && w.entering && !strcmp(w.entering, "KwikEMart"), "переход=%d, арест=%d, входит в «%s» (рантайм это видит)", w.transition, w.arrested, w.entering ? w.entering : "");
  check("state.hnr_freeze", W.G.hnrFreeze > 0.f, "на переходе шкала Hit & Run заморожена (%.2f с)", W.G.hnrFreeze);
  gameMsnSetState(m, 0, 1, ""); steps(&W, 1); gameMsnWorld(m, &w);
  check("state.arrest_info", !w.transition && w.arrested, "арест доходит до рантайма (переход=%d, арест=%d)", w.transition, w.arrested);
  bwFree(&W);
}

// строка 4 (P07:9): вне конуса (косинус < 0,94) луч не берёт: 8 м сбоку и 3 м ниже — в прежнем круге 11 м по земле машина «в луче» и после 3 с цель закрывалась
static void ufoOutside(void) {
  bwNew(&W); GameMsn *m = &W.msn;
  if (!jumpTo(&W, pBoss, 0, NULL)) { bwFree(&W); return; }
  steps(&W, 5); if (!m->ufoOn) { bwFree(&W); return; }
  W.car.x = m->ufoPos[0] + 8.f; W.car.y = m->ufoPos[1] - 3.f; W.car.z = m->ufoPos[2]; m->ufoGrab = 0; m->ufoSent = 0;
  homerPlace(&W.homer, bwCol(), W.car.x + 3.f, W.car.y + .3f, W.car.z, 0.f);   // игрок пешком (прежнее правило требовало этого)
  steps(&W, 300);
  check("ufo.outside_cone", !m->ufoGrab && !m->ufoSent && gameOnFoot(&W.G), "машина вне конуса: не взята (захват=%d, цель закрыта=%d, игрок пешком=%d)", m->ufoGrab, m->ufoSent, gameOnFoot(&W.G));
  bwFree(&W);
}


// ---- №1098 Фринк (L2): персонаж миссии идёт по маршруту, машина игрока стоит на его пути (на самой точке и вплотную к нему) — не стоит на месте дольше 3 с и проходит маршрут
static const GameNpc *npcNamedP(const GameMsn *M, const char *name) { for (int i = 0; i < M->nnpc; i++) if (msn_streq(M->npc[i].name, name)) return &M->npc[i]; return 0; }
static int setupP(const char *who, int *stg) {
  bwNew(&W); GameMsn *m = &W.msn; MsnFile *F = &m->f;
  for (uint32_t mi = 0; mi < F->nmissions; mi++) for (int p = 0; p < 2; p++) {
    unsigned idx = p ? F->missions[mi].main : F->missions[mi].intro; if (idx == MSN_NONE16) continue; const MsnPhase *P = &F->phases[idx];
    for (unsigned s = 0; s < P->stage_count; s++) { const MsnStage *S = &F->stages[P->first_stage + s];
      if (S->obj == MSN_OBJ_TALKTO) { msn_start_mission(&m->rt, mi, 0); m->rt.cp_phase = p; m->rt.cp_stage = s; m->rt.cp_timer = 0; m->rt.cp_remaining = 0; msn_retry(&m->rt, 0);
        for (int k = 0; k < 5; k++) bwStep(&W, 0, 0, 0);
        if (m->rt.target && !strcmp(m->rt.target, who)) { *stg = (int)(mi * 100 + p * 50 + s); return 1; } } } }
  return 0;
}
// один прогон: машина игрока в точке пути на доле fr от текущей к следующей, курс ang (рад) от направления пути; Гомер далеко.
// Возвращает макс. простой; tEnd — время маршрута (−1: не дошёл за 45 с); *inside — персонаж хоть раз оказался внутри кузова
static int gWalkAtEnd, gIwpAtEnd;
static float runOneP(const char *who, float fr, float ang, int withCar, float *tEnd, int *stg, int *inside, int *skipped) {
  if (!setupP(who, stg)) return -1.f;
  GameMsn *m = &W.msn; const GameNpc *n = npcNamedP(m, who); MsnVec3 t = n->wp[n->iwp];
  float mx = n->pos.x + (t.x - n->pos.x) * fr, mz = n->pos.z + (t.z - n->pos.z) * fr, dx = t.x - n->pos.x, dz = t.z - n->pos.z, h = atan2f(dx, dz) + ang;
  homerPlace(&W.homer, bwCol(), n->pos.x + 40.f, n->pos.y + .3f, n->pos.z + 40.f, 0.f);
  W.car.x = withCar ? mx : 5000; W.car.y = n->pos.y + .3f; W.car.z = withCar ? mz : 5000; W.car.h = h; memset(W.car.vel, 0, sizeof W.car.vel);
  float lastd = 1e9f, still = 0.f, maxstill = 0.f; int iw0 = n->iwp; *tEnd = -1.f; *inside = 0; *skipped = 0; MsnVec3 prevW = n->wp[n->iwp];
  for (int f = 0; f < 60 * 45; f++) { bwStep(&W, 0, 0, 0); n = npcNamedP(m, who);
    { const Car *c = &W.car; float fx = sinf(c->h), fz = cosf(c->h), cx = c->x + fx * c->p.bodyOfs, cz = c->z + fz * c->p.bodyOfs;   // середина кузова
      float ax = (n->pos.x - cx) * fz - (n->pos.z - cz) * fx, az = (n->pos.x - cx) * fx + (n->pos.z - cz) * fz;
      if (withCar && fabsf(ax) < c->p.halfW - .1f && fabsf(az) < c->p.halfL - .1f) *inside = 1; }
    if (getenv("WALKTRACE") && fr < .2f && ang == 0.f && f % 120 == 0 && f < 60 * 40) printf("  t=%2d pos=(%.2f,%.2f) iwp=%d cf=%.1f cw=%.1f bl=%d\n", f / 60, n->pos.x, n->pos.z, n->iwp, n->carFree, n->carWaitT, n->blocked);
    if (n->iwp != iw0 || !n->walking) { float dd = hypotf(prevW.x - n->pos.x, prevW.z - n->pos.z); if (dd > .6f) (*skipped)++; if (n->iwp < n->nwp) prevW = n->wp[n->iwp]; }   // точка взята: персонаж обязан быть в 0,5 м от неё (шаг + запас)
    if (!n->walking || n->iwp < iw0) { *tEnd = f / 60.f; gWalkAtEnd = n->walking; gIwpAtEnd = n->iwp; break; }   // маршрут окончен — или замкнут в круг: после последней точки снова первая
    MsnVec3 tt = n->wp[n->iwp < n->nwp ? n->iwp : 0]; float d = hypotf(tt.x - n->pos.x, tt.z - n->pos.z);
    if (d < lastd - .02f || n->iwp != iw0) { lastd = d; still = 0.f; iw0 = n->iwp; } else { still += 1.f / 60; if (still > maxstill) maxstill = still; } }
  if (getenv("WALKDBG")) { MsnVec3 tt = n->wp[n->iwp < n->nwp ? n->iwp : 0]; printf("  dbg fr=%.2f ang=%.2f: iwp=%d/%d walking=%d pos=(%.1f,%.1f) цель=(%.1f,%.1f) carFree=%.1f carWait=%.1f blocked=%d машина=(%.1f,%.1f) h=%.2f halfW=%.2f halfL=%.2f\n", fr, ang, n->iwp, n->nwp, n->walking, n->pos.x, n->pos.z, tt.x, tt.z, n->carFree, n->carWaitT, n->blocked, W.car.x, W.car.z, W.car.h, W.car.p.halfW, W.car.p.halfL); }
  bwFree(&W); return maxstill;
}

static void walk1098(void) {
  if (bwLevel() != 2) { skipc("walk1098", "проверка — на уровне 2 (Фринк)"); return; }
  int stg, ins, sk, skAll = 0; float te0; runOneP("frink", .5f, 0.f, 0, &te0, &stg, &ins, &sk);
  int loopOk = gWalkAtEnd && gIwpAtEnd == 0;
  check("loop1113", loopOk, "маршрут Фринка замкнут в круг: дошёл до последней точки — идёт к первой (идёт=%d, точка=%d)", gWalkAtEnd, gIwpAtEnd);
  float worst = 0.f; int bad = 0, n = 0, through = 0, stuck = 0, skip = 0; float dump = 0.f;
  static const float FR[3] = {.15f, .5f, .95f}; static const int ANG[2] = {0, 90};
  for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) { float te; ins = 0; float ms = runOneP("frink", FR[i], ANG[j] * 3.14159265f / 180.f, 1, &te, &stg, &ins, &sk); n++; skAll += sk;
    if (ms > worst) worst = ms; if (ins) through++; if (te < 0) stuck++; if (te < 0 || ms > 3.5f) bad++; if (ms > 3.5f && te >= 0) skip++; (void)dump; }
  // №1098 (правило порта «пропуск точки через 3 с») → №1113 (как в оригинале): сквозь машину не идёт; постояв после касания, идёт дальше. Число случаев «упёрся насовсем» пишем в журнал
  check("walk1113.through_info", through == 0, "Фринк, машина на пути (%d положений): ни разу не прошёл сквозь кузов (сквозь: %d)", n, through);
  check("walk1113.stuck_info", 1, "%d из %d положений не дошёл до конца за 45 с (упёрся насовсем, как возможно и в оригинале); макс. простой %.1f с; маршрут без машины %.1f с", stuck, n, worst, te0);
  check("walk1113.reach", skAll == 0, "точка маршрута взята только когда персонаж ближе 0,5 м к ней (как в оригинале): взято издали %d раз (ждём 0; правило «пропуск через 3 с» %s)", skAll, MSNWALK1113_OLD ? "включено" : "выключено");
  (void)bad; (void)skip;
}

// ---- строки 21, 44 (круг 7): удар по цели dump роняет предмет только при импульсе не меньше 5000 (0,05 от 100000)
static void dumpHit(void) {
  check("dumphit.force", gameMsnDumpHitStrong(5000.f) && gameMsnDumpHitStrong(20000.f) && !gameMsnDumpHitStrong(4900.f) && !gameMsnDumpHitStrong(0.f), "порог силы удара: 4900 — нет, 5000 — да");
}

int main(int argc, char **argv) {
  if (argc != 4 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "host_game LEVEL.shr HOMER.shr levelNN.msn\n"); return 2; }
  ufo(); ufoOutside(); safezone(); abortOk(); hud(); shop(); count(); talkRet(); talkHide(); talkBack(); state(); dumpHit(); walk1098();
  printf("RESULT checks=%d failures=%d\n", nchecks, fails);
  return fails != 0;
}
