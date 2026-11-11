// Миссии поверх логики кадра (см. game_msn.h).
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef __PSP__
#include <pspkernel.h>
#endif
#include <strings.h>
#include <math.h>
#include "phonebusy.h"
#include "game_msn.h"
#include "race_onroad.h"
#include "flowrules.h"   // подпись △: имя собеседника как в тексте задания (flowTalkName)

#define GM ((GameMsn *)u)
#define G_ (GM->g)
// ---- сверка с исходником оригинала, окно g07-missions (02.11.2026, правки Роба 1077–1081). Выключатель = 1 — поведение прежнее (TEST81).
// Документ и номер строки — в комментарии у каждого; журнал: docs/handoff/СВЕРКА_ВНЕДРЕНИЕ/g07-missions_журнал.md
#ifndef GAME_CARNEAR51_OLD
#define GAME_CARNEAR51_OLD 0   // СВЕРКА_С_ИСХОДНИКОМ 5.1: машина — ровно на точку данных миссии, правила «машину к Гомеру» нет (было: после RESUME/LOAD к Гомеру, №1067/1073)
#endif
#ifndef GAME_UFOBEAM_OLD
#define GAME_UFOBEAM_OLD 0     // P04 U1/Д9, P07-9: луч тарелки L7 тянет машину игрока (конус 0,94, 30 м, 3 м/с, у днища 5 м); было: стоять 3 с в круге 11 м (№967)
#endif
#ifndef GAME_HNRAI_OLD
#define GAME_HNRAI_OLD 0       // P03 Д1: удар/взрыв машины погони H&R не двигают; остальные машины ИИ: +5, взрыв +25 и 10 монет; было — наоборот (правка 121)
#endif
#ifndef GAME_SAFE13_OLD
#define GAME_SAFE13_OLD 0      // P06-13, P07 Д5: безопасная зона — только машина игрока, полиция стоит, шкала не обнуляется
#endif
#ifndef GAME_SHOPPHASE_OLD
#define GAME_SHOPPHASE_OLD 0   // P05-11, P06-12: лавка и гардероб — только пока идёт вступление (воскресная езда) и вне миссий
#endif
#ifndef GAME_TRAFFICFOOT_OLD
#define GAME_TRAFFICFOOT_OLD 0 // P03 Д5: лимит трафика этапа действует, пока игрок в машине; пешком трафик набирается до полного
#endif
#ifndef GAME_TALKRET_OLD
#define GAME_TALKRET_OLD 0     // P07-14, P06-16: после разговора Гомер и собеседник возвращаются на прежние места (кроме SetDialoguePositions с 4-м аргументом 1)
#endif
#ifndef GAME_TALKGAP_OLD
#define GAME_TALKGAP_OLD 0     // СВЕРКА_КАМЕРА 4.4: между героями не ближе 1,65 м, ближе — раздвигаются симметрично
#endif
#ifndef GAME_TALKCAR_OLD
#define GAME_TALKCAR_OLD 0     // P15-13: разговор начинается и при игроке в машине — он выходит на землю у машины
#endif
#ifndef GAME_TALKPAIR_OLD
#define GAME_TALKPAIR_OLD 0    // P15-14: Пэтти и Сельма — обе лицом к игроку
#endif
#ifndef GAME_COUNT57_OLD
#define GAME_COUNT57_OLD 0     // P06 Д6: на отсчёте «3 2 1» руль и камера живы, газ/тормоз/сброс выключены, ручник включён
#endif
#ifndef GAME_RESUME33_OLD
#define GAME_RESUME33_OLD 0    // P14-16: после загрузки/RESUME текущая миссия начинается заново с её начала (запись хранит только уровень и миссию)
#endif
#ifndef GAME_GIVENCAR52_OLD
#define GAME_GIVENCAR52_OLD 0  // СВЕРКА 5.2: после миссии с выданной машиной Гомера у её точки возвращают туда, где он начинал
#endif
#ifndef GAME_HIDECHAR_OLD
#define GAME_HIDECHAR_OLD 0    // P06-21: SetCharacterToHide — на выходе из этапа персонаж убирается; ShowHUD("false") прячет HUD миссии
#endif
#ifndef GAME_ABORT_OLD
#define GAME_ABORT_OLD 0       // P06-20, P07-23: AllowMissionAbort("false") — Cancel вступления не действует
#endif
#ifndef GAME_TALKGEST_OLD
#define GAME_TALKGEST_OLD 0    // круг 2, P15-15
#endif
#ifndef GAME_C5_OLD
#define GAME_C5_OLD 0          // круг 5: buycar по текущей машине, заморозка шкалы на переходе, будка/разговор не при аресте и провале, скрытие машины на диалоге бонус-миссии
#endif
#ifndef GAME_UFOCENTER_OLD
#define GAME_UFOCENTER_OLD 0   // круг 3, проверка строк 1/4: луч тянет к центру тарелки; игрока на Z3D1 — только если ещё за рулём; 1 — прежнее (вверх, Z3D1 всегда)
#endif
#ifndef GAME_C3_SAFE_OLD
#define GAME_C3_SAFE_OLD 0     // круг 3, строка 15: безопасная зона замораживает полицию (McEnv.freeze), шкала Hit & Run не обнуляется ни при каком запасном пути
#endif
#ifndef GAME_C3_GIVEN_OLD
#define GAME_C3_GIVEN_OLD 0    // круг 3, строка 41: правило выданной машины — при завершении миссии (без подмены на свою)
#endif
#ifndef HNR_IMMUNE_OLD
#define HNR_IMMUNE_OLD 0      // круг 7 (g10-flow, P03-30): 1 — взрыв цели destroy / dump двигает шкалу, как было
#endif
#ifndef GAME_C2_DMG_OLD
#define GAME_C2_DMG_OLD 0      // круг 2, СВЕРКА_МАШИНЫ:7.2, P07 Д6: цель «разбить» получает урон только от машины игрока; follow/dump — цель неуязвима; dump с несколькими — и машина игрока
#endif
#ifndef GAME_C2_CELEB_OLD
#ifndef GATEDFANFARE_OLD
#define GATEDFANFARE_OLD 0    // ТЕСТ90 (МУЗЫКА_ПО_МЕСТУ п.2): запертый этап не пройден (нет нужной машины или костюма) — вставка Gated_Mission (Horn_Blast), как у оригинала при отказе смены этапа; 1 — молча
#endif
#define GAME_C2_CELEB_OLD 0    // круг 2, g04: Гомер празднует (homerCelebrate): этап со вспышкой — малый клип, миссия — большой
#endif
#ifndef GAME_C2_CAMGOAL_OLD
#define GAME_C2_CAMGOAL_OLD 0  // круг 2, g04 P08 Д1: камера пешком притягивается к рисуемому предмету цели (homerCamGoal)
#endif
#ifndef GAME_C2_SOUND_OLD
#define GAME_C2_SOUND_OLD 0    // круг 2, g08: атмосфера старта миссии, звук подбора, реплика Arrive, вставка Win_3Street_Races
#endif
#ifndef GAME_C2_STREETRACE_OLD
#define GAME_C2_STREETRACE_OLD 0 // круг 2, g02 P03-13: на уличной гонке пешеходов нет (trafficStreetRace)
#endif
#ifndef GAME_C2_NAV_OLD
#define GAME_C2_NAV_OLD 0      // круг 2, g02 P16-06: путь навигации по дорогам строится один раз при старте цели (trafficNavBuild)
#endif
#ifndef GAME_C2_UFOH_OLD
#define GAME_C2_UFOH_OLD 0     // круг 2, g07b P04:U2: тарелка не ниже Y = 22, ниже — поднимается на 1 м/с
#endif
#ifndef GAME_C2_BONUS_OLD
#define GAME_C2_BONUS_OLD 0    // круг 2, g07b P06-2: на новом входе в уровень персонаж bm1 не создаётся, если бонус-миссия уже пройдена
#endif
#ifndef GAME_C2_ODDS_OLD
#define GAME_C2_ODDS_OLD 0     // круг 2, g07b P06-3: выплата ставки = mvWagerPay(вход)
#endif
#ifdef __PSP__
extern int mvWagerPay(int fee);   // вид (msn_view.c); в ботах вида нет
#endif
#ifdef __PSP__
#include "sound.h"   // круг 2, g08: атмосфера миссии, звук подбора, реплика Arrive, музыкальные вставки (в ботах звука нет)
#endif
int gameMsnCarHide;   // круг 5, P06-15: 1 — машина игрока не рисуется (диалог бонус-миссии); читает main_loop.h
int gameMsnBoothBlocked(const GameMsn *m) { return !GAME_C5_OLD && (m->rt.status == MSN_FAILED || m->arrested); }   // круг 5, P05-25: △ у будки игнорируется при проваленной миссии и аресте
// g07b-msnview, ps2-twin P05:25: будки выключены, пока идёт миссия с принудительной машиной (phonebusy.h); PHONEBUSY_OLD=1 — всегда включены
int gameMsnBoothsOff(const GameMsn *m) {
  const MsnRuntime *rt = &m->rt;
  return rt->cur >= 0 && pbMissionOff(rt->cur, (m->f.missions[rt->cur].flags & MSN_MF_FORCEDCAR) != 0, rt->status == MSN_RUNNING || rt->status == MSN_COUNTDOWN || rt->status == MSN_BETWEEN);
}
void (*gameMsnSafeHook)(int active);   // P06-13: модуль полиции ставит функцию — «безопасная зона: погоню остановить (1) / продолжить (0)»; NULL — запасной путь: шкала гасится
static const char *(*skinNameFn)(void);   // P06-8: имя надетого костюма, как в замке этапа (ставит main)
void gameMsnSetSkinNameFn(const char *(*fn)(void)) { skinNameFn = fn; }
int gameMsnShopAllowed(const GameMsn *m);
// вид (msn_view.c) получает user = GameMsn* во всех вызовах: и в переходниках ниже, и в группах, что идут ему напрямую
#define VIEW(grp, fn, ...) do { if (GM->view.grp.fn) GM->view.grp.fn(u, __VA_ARGS__); } while (0)
static void inCar(Game *g) {   // Гомер за рулём
  if (!*g->pHaveHomer) return;
  HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, g->pCar, *g->pLift); homerSetInCar(g->pHomer, &hc);
  if (g->hk.door) g->hk.door(g->hk.u, 0.f);
}
static void putCar(Game *g, const MsnPlace *at) {
  if (!at || !*g->pHaveCol) return;
  carPlace(g->pCar, g->pWorld, at->pos.x, at->pos.y, at->pos.z, at->has_heading ? mcCarHeading(at->heading) : g->pCar->h);   // машина: +π (msncar.h)
  g->pProps->havePreviousCar = 0;
}
#define TELEPORTED() (GM->teleports++, GM->scam.placed |= GM->scam.arm)   // правка 40: перенос на старте фазы — к её камере
static float wdPut[4]; static int wdPutOk;   // сторож (watchdog_game.c): точка последней постановки героя фазой миссии — запасное «надёжное место»
static int wdStartPoint(int hero, float out[4]) { if (!hero || !wdPutOk) return 0; memcpy(out, wdPut, sizeof wdPut); return 1; }
static void putHomer(Game *g, const MsnPlace *at) {
  if (!at || !*g->pHaveHomer) return;
  wdPut[0] = at->pos.x; wdPut[1] = at->pos.y; wdPut[2] = at->pos.z; wdPut[3] = at->has_heading ? at->heading : g->pHomer->h; wdPutOk = 1; gameWdStartPoint = wdStartPoint;
  homerPlace(g->pHomer, g->pWorld, at->pos.x, at->pos.y, at->pos.z, at->has_heading ? at->heading : g->pHomer->h);
}
// круг 2–3: игрок сразу ставится на землю у машины (выход без анимации): сначала сбоку по одну сторону, потом по другую, иначе в центре машины
static void stepOutOfCar(Game *g) {
  Car *c = g->pCar; ColWorld *cw = g->pWorld; int ok = 0;
  for (int side = 1; side >= -1 && !ok; side -= 2) {
    float ox = c->x + (float)side * cosf(c->h) * (c->p.halfW + .9f), oz = c->z - (float)side * sinf(c->h) * (c->p.halfW + .9f), gy;
    if (*g->pHaveCol && colGround(cw, ox, oz, c->y + 1.5f, c->y - 1.f, &gy, 0) && fabsf(gy - c->y) < .8f) { homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : cw, ox, gy, oz, c->h); ok = 1; }
  }
  if (!ok) homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : cw, c->x, c->y, c->z, c->h);
}
// SetMissionResetPlayerInCar / OutCar / AddStageCharacter: пешком у at (машина у car), или в машине у car.
// RESUME GAME на старте миссии пешком (м4 L1, гараж АЭС, видео Роба IMG_7867): машина игрока по данным локатора (m4_carstart) стоит за колонной в 6,9 м и со старта не видна; у оригинала (PS2, внешнее видео) машина рядом с Гомером,
// он выходит у её переднего колеса. Общее правило: первый сброс «пешком у точки, машина у точки» после RESUME/LOAD, если машина дальше 3,5 м, ставит её рядом (3,4–5 м), свободное место (земля в пределах 0,8 м
// от ног Гомера, кузов без контактов с миром): сначала впереди (до 60° от курса, камера за спиной видит), иначе где угодно, ближе к прежнему месту. Курс машины прежний. Нет места — остаётся где была.
// RESUME_CAR_NEAR_OLD=1 — как было
#ifndef RESUME_CAR_NEAR_OLD
#define RESUME_CAR_NEAR_OLD 0
#endif
static int gmResumeStart;
void gameMsnResumeStart(int on) { gmResumeStart = on; }
int gameMsnDeferStart;   // №943: 1 — msn_level_start (resumeAt) не стартует первую сюжетную: ждёт выбранную строку MISSION SELECT (main_levels.h menuStartGame)
// №1073 (Роб 31.10, повтор №1067: «опять машина ставится туда, где она не может выехать… у Барта на шестом уровне… площадка просмотровая и вот лестница»;
// «машина должна ставиться в начале миссии, чаще всего возле телефона и будки… не может быть правило, что она ставится возле именно персонажа»). Причина: правило
// выше переносило машину к герою с любого расстояния. L6 m4 через MISSION SELECT: машина у будки K3-4 в 36 м и на 3,9 м ниже уезжала наверх, на смотровую
// площадку у обсерватории, откуда выезда нет. Правило по месту машины в данных (замер эмулятора, вход MISSION SELECT, tests/flow1070): ближе 10 м на том же ярусе —
// к герою, как раньше (гараж АЭС L1 m4: 6,9 м за колонной); другой ярус (выше/ниже больше 1,5 м: смотровая, комната дома L7 m1 — 23 м по высоте) или 10–60 м
// (своё место миссии, будка L6 m4 — 32 м) — машина остаётся на локаторе данных; дальше 60 м на том же ярусе (L5 m7 — 297 м: место машины в другом конце города)
// — к герою, как раньше. CAR_NEAR1073_OLD=1 / flag carnearold 1 — как до правки
#ifndef CAR_NEAR1073_OLD
#define CAR_NEAR1073_OLD 0
#endif
int gameMsnCarNearOld = CAR_NEAR1073_OLD;
void (*gameMsnNote)(const char *line);
static void carNearHomer(Game *g) {
  Car *c = g->pCar; Homer *H = g->pHomer; ColWorld *w = g->pWorld;
  float ox = c->x, oy = c->y, oz = c->z, oh = c->h, dx = ox - H->x, dz = oz - H->z;
  if (dx * dx + dz * dz <= 3.5f * 3.5f) return;
  if (!gameMsnCarNearOld && (fabsf(oy - H->y) > 1.5f || (dx * dx + dz * dz > 10.f * 10.f && dx * dx + dz * dz <= 60.f * 60.f))) {
    if (gameMsnNote) { char t[96]; snprintf(t, sizeof t, "CARNEAR1073 keep d=%.1f dy=%.1f car=%.1f,%.1f,%.1f", sqrtf(dx * dx + dz * dz), oy - H->y, ox, oy, oz); gameMsnNote(t); }
    return;
  }
  static const float R[3] = {3.4f, 4.2f, 5.0f}; static const float A[8] = {0.f, .6f, -.6f, 1.05f, -1.05f, 1.6f, -1.6f, 3.1416f};
  float bx = 0.f, bz = 0.f, by = 0.f, bs = 1e30f; int have = 0;
  for (int pass = 0; pass < 2 && !have; pass++)
    for (int ir = 0; ir < 3; ir++) for (int ia = 0; ia < 8; ia++) {
      float a = A[ia]; if (pass == 0 && fabsf(a) > 1.05f) continue;
      float x = H->x + R[ir] * sinf(H->h + a), z = H->z + R[ir] * cosf(H->h + a), gy;
      if (!colGround(w, x, z, H->y + 1.5f, H->y - 1.f, &gy, 0) || fabsf(gy - H->y) > .8f) continue;
      carPlace(c, w, x, gy, z, oh); CarBox b; carBox(c, &b); if (colProbeCar(w, &b)) continue;
      float sc = pass == 0 ? R[ir] * 10.f + fabsf(a) : (x - ox) * (x - ox) + (z - oz) * (z - oz);
      if (sc < bs) { bs = sc; bx = x; by = gy; bz = z; have = 1; }
    }
  if (have) carPlace(c, w, bx, by, bz, oh); else carPlace(c, w, ox, oy, oz, oh);
  g->pProps->havePreviousCar = 0;
}
// В окне подготовки фазы (SetMissionReset*) — только при старте/повторе миссии; AddStageCharacter (этап) — всегда
static void plReset(void *u, const MsnPlace *at, const MsnPlace *car, const char *name) {
  (void)name;
  if (GM->keepPlayer) { GM->skipped++; return; }
  if (GM->inSetup && !GM->startArmed) { GM->skipped++; return; }
  GM->resets++;
  if (car) putCar(G_, car);
  if (at) putHomer(G_, at); else inCar(G_);
  if (gmResumeStart) { gmResumeStart = 0; if (GAME_CARNEAR51_OLD && !RESUME_CAR_NEAR_OLD && car && at && *G_->pHaveCol && *G_->pHaveHomer) carNearHomer(G_); }   // 5.1: в оригинале загрузка = перезапуск миссии, машина на точке данных
  TELEPORTED();
}
static Traffic *roadsT;   // дороги и трафик уровня (main.c: traffic.shr; боты: bwTraffic); NULL — без дорог
static int gameMsnResetRoad(const Car *c, float out[4]) { return mcResetPoint(roadsT, c->x, c->y, c->z, c->h, c->vel[0], c->vel[2], out); }
static int gameMsnResetRoadAt(const Car *c, float off, float out[4]) { return mcResetPointOff(roadsT, c->x, c->y, c->z, c->h, c->vel[0], c->vel[2], off, out); }
void gameMsnSetRoads(Traffic *t) { roadsT = t; gameResetRoad = t ? gameMsnResetRoad : 0; gameResetRoadAt = t ? gameMsnResetRoadAt : 0; }
// ---- машина миссии у игрока: InitLevelPlayerVehicle(имя, локатор, "OTHER") + SetForcedCar (уровень 1: m6 — машина Мардж marge_v).
// Машина игрока становится этой машиной: ходовые и кузов, сиденье, дверь, камера — блок машины CAR1 v2 из таблицы пакета msncars
// (MCR1 v2, tools/msncar_export.py; то же правило, что у машины уровня: у marge_v — числа машины уровня 4); нет блока — .con машины
// и кузов модели пакета. Стоящая машина миссии того же имени (m6sd: marge_v у магазина) уходит — в неё сел игрок. Машина уровня
// возвращается SwapInDefaultCar (там, где стоит игрок, или на SetSwapDefaultCarLocator; машина миссии остаётся стоять на
// SetSwapForcedCarLocator) и при конце / провале / повторе миссии (cleanup). Рисует main.c — моделью пакета (mvForcedCar)
static const char *const MC_PARK = "NULL";
static void forcedOn(GameMsn *m, const char *car, const MsnPlace *at) {
  Game *g = m->g; if (!*g->pHaveCol) return;
  const McGeo *G = mcGeoFind(car); CarParams P;
  int ok = G && G->hasInfo && carParamsFromInfo(G->info, &P) == 1;
  if (!ok) {   // запасной путь: .con машины, кузов модели
    char lc[16]; int k = 0; for (const char *q = car; *q && k < 15; q++) lc[k++] = (*q >= 'A' && *q <= 'Z') ? (char)(*q + 32) : *q; lc[k] = 0;
    carParamsFor(lc, &P);
    if (G) { P.halfW = G->halfW; P.halfL = G->halfL; P.bodyOfs = G->bodyOfs; P.wheelX = G->wheelX; P.wheelF = G->wheelF; P.wheelR = G->wheelR;
             P.wheelRad = G->wheelRad; P.bodyLo = G->bodyLo; P.bodyHi = G->bodyHi; }
  }
  if (!m->forced && !m->jacked) { m->defCar = g->pCar->p; m->defLift = *g->pLift; }
  if (!GAME_GIVENCAR52_OLD && !m->forced) {   // 5.2: где начинал Гомер (пешком — он сам, в машине — машина)
    int foot_ = *g->pHaveHomer && gameOnFoot(g);
    m->forcedStart[0] = foot_ ? g->pHomer->x : g->pCar->x; m->forcedStart[1] = foot_ ? g->pHomer->y : g->pCar->y; m->forcedStart[2] = foot_ ? g->pHomer->z : g->pCar->z;
    m->forcedStartOn = foot_;
    m->forcedPt[0] = at ? at->pos.x : g->pCar->x; m->forcedPt[1] = at ? at->pos.y : g->pCar->y; m->forcedPt[2] = at ? at->pos.z : g->pCar->z;
  }
  m->jacked = 0; m->jackedName[0] = 0;   // машина миссии сменяет угнанную
  mcRemove(&m->cars, car);
  float x = g->pCar->x, y = g->pCar->y, z = g->pCar->z, h = g->pCar->h;
  if (at && at->loc) { x = at->pos.x; y = at->pos.y; z = at->pos.z; if (at->has_heading) h = mcCarHeading(at->heading); }
  carInit(g->pCar, &P, g->pWorld, x, y + .5f, z, h); g->pProps->havePreviousCar = 0;
  if (G) *(float *)g->pLift = G->lift;   // посадка Гомера по модели (pLift — CarVis.lift у EBOOT, BotWorld.lift у ботов)
  m->forced = 1; strncpy(m->forcedName, car, sizeof m->forcedName - 1); m->forcedName[sizeof m->forcedName - 1] = 0;
}
static void forcedOff(GameMsn *m, const MsnPlace *defAt, const MsnPlace *forcedAt) {
  Game *g = m->g; if (!m->forced || !*g->pHaveCol) return;
  float x = g->pCar->x, y = g->pCar->y, z = g->pCar->z, h = g->pCar->h;
  if (forcedAt && forcedAt->loc) {   // машина миссии остаётся стоять на своём локаторе
    float p[3] = {forcedAt->pos.x, forcedAt->pos.y, forcedAt->pos.z};
    mcSpawn(&m->cars, g->pWorld, roadsT, m->forcedName, p, mcCarHeading(forcedAt->heading), forcedAt->has_heading, MC_PARK, "", "");
  }
  if (defAt && defAt->loc) { x = defAt->pos.x; y = defAt->pos.y; z = defAt->pos.z; if (defAt->has_heading) h = mcCarHeading(defAt->heading); }
  carInit(g->pCar, &m->defCar, g->pWorld, x, y + .5f, z, h); g->pProps->havePreviousCar = 0;
  *(float *)g->pLift = m->defLift;
  m->forced = 0; m->forcedName[0] = 0;
}
// Правка Роба 63 (запись PS2 33:40: Гомер подходит к машине трафика и уезжает на ней): машина игрока становится угнанной — ходовые
// из .con модели (main.c: carParamsFor + размеры модели трафика), посадка — lift модели; машина уровня запомнена (defCar), её
// возвращает машина миссии (SetForcedCar) или будка. Рисует main.c моделью трафика (mcrDrawPlayer по имени)
void gameMsnCarjack(GameMsn *m, const char *model, const CarParams *P, float lift, float x, float y, float z, float h) {
  Game *g = m->g; if (!*g->pHaveCol || m->forced) return;
  if (!m->jacked) { m->defCar = g->pCar->p; m->defLift = *g->pLift; }
  carInit(g->pCar, P, g->pWorld, x, y + .5f, z, h); g->pProps->havePreviousCar = 0;
  *(float *)g->pLift = lift;
  m->jacked = 1; strncpy(m->jackedName, model, sizeof m->jackedName - 1); m->jackedName[sizeof m->jackedName - 1] = 0;
}
// Level-authored chase model, .con, population and idle decay (owned disc leveli.mfk).
GameHnrConfig gameMsnHnrConfig(const MsnFile *f) {
  GameHnrConfig h = {"cPolice", "Pursuit\\L1cop.con", 1, 3.f};
  if (!HNR_LEVEL_C3_OLD && f && f->level) {
    for (unsigned k=0; k<2; k++) {
      unsigned first=k?f->level->init_first:f->level->lvl_first, n=k?f->level->init_count:f->level->lvl_count;
      for (unsigned i=0;i<n;i++) {
        const MsnCmd *c=&f->cmds[first+i];
        if(c->op==MSN_OP_CREATECHASEMANAGER && msn_argc(c)>=2) {h.car=msn_arg_s(f,c,0);h.con=msn_arg_s(f,c,1);}
        if(c->op==MSN_OP_SETNUMCHASECARS) h.count=msn_arg_i(f,c,0);
        if(c->op==MSN_OP_SETHITANDRUNDECAY) h.decay=msn_arg_f(f,c,0);
      }
    }
  }
  return h;
}
void gameMsnHnrPolice(GameMsn *m,int on) { gameMsnPolice(m,on,m->hnr.car,m->hnr.con,m->hnr.count); }
// №627 (запись PS2 ps2-police-after-busted.yaml): после ареста полицейская стоит на месте (скорость 0,04 м/с) 1,5 с и исчезает вместе с карточкой: стоп, а не уход
// сверка g09b-menu круг 5 (P03-08): погоня ставится и снимается по одной машине. Проход раз в 0,7 с ставит одну (если положенных ещё нет), раз в 0,5 с каждая дальше 110 м
// снимается отдельно. Группа целиком — только в старом режиме (HNRONE_OLD, main_frontend.h)
void gameMsnHnrPoliceAdd(GameMsn *m) { gameMsnPolice(m, 1, m->hnr.car, m->hnr.con, 1); }
int gameMsnHnrPoliceLimit(const GameMsn *m) { return m->hnr.count; }
void gameMsnHnrPoliceFar(GameMsn *m, float px, float pz, float R) {
  for (int i = 0; i < m->cars.n; i++) { McCar *c = &m->cars.c[i];
    if (c->on && !strcasecmp(c->name, m->hnr.car) && hypotf(c->car.x - px, c->car.z - pz) > R) memset(c, 0, sizeof *c); }
}
// сверка g09b-menu круг 7 (P03 Д2): снять одну машину погони по номеру в списке машин миссии
void gameMsnHnrPoliceDrop(GameMsn *m, int i) {
  if (i >= 0 && i < m->cars.n && m->cars.c[i].on && !strcasecmp(m->cars.c[i].name, m->hnr.car)) memset(&m->cars.c[i], 0, sizeof m->cars.c[i]);
}
// правка 433 (Роб 06.11, L4): аналог EnterLimbo оригинала (DisableAllActiveVehicleAIs в ChaseManager): помеченный на удаление коп бросает погоню и тормозит (режим «стоит»: ручник, тормоз при движении);
// on = 0 — вернуть погоню (шкала снова поднялась и защёлка встала до того, как коп ушёл из кадра). Исчезают они не здесь, а правилом «вне кадра 5 с / дальше 110 м» (main_frontend.h)
void gameMsnHnrPoliceLimbo(GameMsn *m, int on) {
  for (int i = 0; i < m->cars.n; i++) { McCar *c = &m->cars.c[i]; if (!c->on || strcasecmp(c->name, m->hnr.car)) continue;
    if (on && c->mode == MC_CHASE) { c->mode = MC_PARKED; c->asleep = 0; c->still = 0.f; }
    else if (!on && c->mode == MC_PARKED) { c->mode = MC_CHASE; c->asleep = 0; c->still = 0.f; c->chaseRoad = -2; c->chaseRoadT = 0.f; } }
}
void gameMsnHnrPoliceHold(GameMsn *m) {
  for (int i = 0; i < m->cars.n; i++) { McCar *c = &m->cars.c[i]; if (c->on && !strcasecmp(c->name, m->hnr.car)) { c->mode = MC_PARKED; c->asleep = 0; c->still = 0.f; } }
}
// Правка Роба 64/72: погоня Hit & Run — машина CreateChaseManager уровня (L1: cPolice, Pursuit\L1cop.con, SetNumChaseCars 1) в
// режиме погони машин миссии (MC_CHASE: едет к игроку по полосам, таран); ставится на дорогу за спиной игрока
#define POLICE_ROUTE_MAX 1.5f   // правка 197: путь по дорогам до игрока не длиннее 1,5 прямой (правило порта)
#define POLICE_PLANS 8          // правка 197: не больше 8 прокладок пути на одно место (каждая ~2 млн шагов на PSP)
// Правка Роба 478 (10.10, голосом): при полной шкале полиция появляется ПО ХОДУ движения, спереди и навстречу, недалеко — в пределах радиуса радара (правый
// нижний угол), потом разворачивается и гонится (путь до игрока по дорогам и развороты делает водитель chase). Было: 100 м сзади (данные pursuitSpawns) — игрок
// полицию не видел. ПРАВИЛО ПОРТА: впереди по курсу игрока (поиск по кругу через 22,5° вокруг направления вперёд), 60–80 м; радиус радара в метрах в данных не
// найден — 70 м взято как «в пределах радара» (проверить глазом). POLICE_SPAWN_BEHIND_OLD=1 — прежнее (100 м сзади)
#ifndef POLICE_SPAWN_BEHIND_OLD
#define POLICE_SPAWN_BEHIND_OLD 0
#endif
// сверка g03-aicars круг 2 (СВЕРКА_С_ИСХОДНИКОМ:2.9, слово Роба 01.11 «делать всё как в оригинале»): полиция появляется на круге 100 м вокруг игрока, на дороге, которая
// ведёт к месту, где игрок будет через секунду (поиск начинается с направления его движения), носом к игроку. POLICE_SPAWN100_OLD = 1 — прежнее (60–80 м впереди по курсу, правка 478).
// Не сделано здесь: «по одной» с паузой 0,7 с (mvPolice(1) ставит все машины разом — main_frontend.h, g09b/g10) и снятие после 5 с вне кадра (нужна видимость машины из камеры)
#ifndef POLICE_SPAWN100_OLD
#define POLICE_SPAWN100_OLD 0
#endif
#define POLICE_AHEAD_D 70.f
#define POLICE_SP_D (POLICE_SPAWN_BEHIND_OLD ? 100.f : POLICE_SPAWN100_OLD ? POLICE_AHEAD_D : 100.f)
#define POLICE_SP_BASE (POLICE_SPAWN_BEHIND_OLD ? 3.14159265f : 0.f)
#define POLICE_SP_DMIN (POLICE_SPAWN_BEHIND_OLD || !POLICE_SPAWN100_OLD ? 90.f : 60.f)
#define POLICE_SP_DMAX (POLICE_SPAWN_BEHIND_OLD || !POLICE_SPAWN100_OLD ? 105.f : 80.f)
int gamePoliceDmgOld;   // правка 280: служебное — полный урон полицейской (прежнее)
// сверка g09b-menu круг 7 (P03-07): на время разговора и перехода в помещение машины погони уходят из мира и стоят «в запасе» (в оригинале они остаются в счёте погони и возвращаются на то же место);
// у нас — запоминаем позу и урон, убираем, потом ставим обратно в ту же точку. HNRSUSP_OLD=1 — как было (убрать совсем и поставить заново в 100 м)
#ifndef HNRSUSP_OLD
#define HNRSUSP_OLD 0
#endif
static struct { int n; float p[8][3], h[8], dmg[8]; int preset; float pp[3], ph; } polSaved;
void gameMsnPolice(GameMsn *m, int on, const char *car, const char *con, int n);
int gameMsnHnrPoliceSuspend(GameMsn *m) {
  polSaved.n = 0;
  for (int i = 0; i < m->cars.n && polSaved.n < 8; i++) { McCar *c = &m->cars.c[i];
    if (c->on && !strcasecmp(c->name, m->hnr.car)) { int k = polSaved.n++; polSaved.p[k][0] = c->car.x; polSaved.p[k][1] = c->car.y; polSaved.p[k][2] = c->car.z; polSaved.h[k] = c->car.h; polSaved.dmg[k] = c->car.damage; memset(c, 0, sizeof *c); } }
  return polSaved.n;
}
void gameMsnHnrPoliceResume(GameMsn *m) {
  int n = polSaved.n; float P[8][3], H[8], D[8]; memcpy(P, polSaved.p, sizeof P); memcpy(H, polSaved.h, sizeof H); memcpy(D, polSaved.dmg, sizeof D); polSaved.n = 0;
  for (int k = 0; k < n; k++) {
    polSaved.preset = 1; memcpy(polSaved.pp, P[k], sizeof polSaved.pp); polSaved.ph = H[k];
    gameMsnPolice(m, 1, m->hnr.car, m->hnr.con, 1);
    polSaved.preset = 0;
    for (int i = 0; i < m->cars.n; i++) { McCar *c = &m->cars.c[i];
      if (c->on && !strcasecmp(c->name, m->hnr.car) && fabsf(c->car.x - P[k][0]) < .5f && fabsf(c->car.z - P[k][2]) < .5f) { c->car.damage = D[k]; break; } }
  }
}
void gameMsnPolice(GameMsn *m, int on, const char *car, const char *con, int n) {
  Game *g = m->g; if (!*g->pHaveCol) return;
  if (!on) { do { mcRemove(&m->cars, car); } while(!HNR_POOL_C3_OLD && mcFind(&m->cars, car)); polSaved.n = 0; return; }   // круг 7: снятие погони забывает и отложенные машины
  // правка 121: как в данных погони (pursuit.ts pursuitSpawns: точки дорог на окружности 100 м вокруг игрока, не ближе 8 м к другой
  // машине) — точка полосы в 100 м (90–105), сначала сзади, дальше по кругу через 22.5°; курс по полосе в сторону игрока.
  // Раньше — точка в 100 м сзади как есть: во дворе или на крыше, полицейская застревала в 70 м и не доезжала (эмулятор 30.09)
  int foot = gameOnFoot(g);
  float px = foot ? g->pHomer->x : g->pCar->x, pz = foot ? g->pHomer->z : g->pCar->z, ph = g->pCar->h;
#if !POLICE_SPAWN100_OLD
  if (!foot && !POLICE_SPAWN_BEHIND_OLD && hypotf(g->pCar->vel[0], g->pCar->vel[2]) > 2.f) ph = atan2f(g->pCar->vel[0], g->pCar->vel[2]);   // куда игрок едет (место «через секунду»)
#endif
  float goal[3] = {px, foot ? g->pHomer->y : g->pCar->y, pz};
  for (int k = 0; k < n; k++) {
    float p[3], h = ph, bp[3], bh = 0.f, bestLen = 1e30f; int got = 0, plans = 0;
    if (polSaved.preset) { memcpy(p, polSaved.pp, sizeof p); h = polSaved.ph; got = 1; }   // круг 7: возврат на прежнее место
    for (int a = 0; a < 16 && !got; a++) {
      float ang = ph + POLICE_SP_BASE + (a & 1 ? 1.f : -1.f) * (float)((a + 1) / 2) * .39269908f, lh;
      if (!trafficLaneSpot(roadsT, px + sinf(ang) * POLICE_SP_D, pz + cosf(ang) * POLICE_SP_D, 20.f, p, &lh)) continue;
      float dx = px - p[0], dz = pz - p[2], d = sqrtf(dx * dx + dz * dz); if (d < POLICE_SP_DMIN || d > POLICE_SP_DMAX) continue;   // не у самой границы удаления 110 м — иначе заново каждую секунду
      int busy = 0;
      for (int i = 0; i < m->cars.n; i++) { const McCar *o = &m->cars.c[i];
        if (o->on && (o->car.x - p[0]) * (o->car.x - p[0]) + (o->car.z - p[2]) * (o->car.z - p[2]) < 64.f) busy = 1; }
      if (busy) continue;
      h = (sinf(lh) * dx + cosf(lh) * dz) >= 0.f ? lh : lh + 3.14159265f;
#if defined(POLICE_SPAWN_STRAIGHT) && POLICE_SPAWN_STRAIGHT   // красный tests/hnr/police.sh: прежнее — первая точка полосы на окружности
      got = 1;
#else
      // правка Роба 197 (03.10): место — то, откуда путь по дорогам до игрока короткий. По прямой 100 м, а по дорогам бывает 441 м
      // (от дома Симпсонов — через x ≈ 41): полицейская уезжала дальше 110 м, её снимали и ставили заново, до игрока она не
      // доезжала никогда. Первое место с путём не длиннее POLICE_ROUTE_MAX × прямой — сразу; иначе самое короткое из проверенных
      float route[MC_ROUTE][3]; int off = 0, nr = mcPlan(roadsT, p[0], p[1], p[2], h, goal, 0, route, MC_ROUTE, &off), plan = 1;   // без срезов, как едет полиция (ниже)
      float len = hypotf(route[0][0] - p[0], route[0][2] - p[2]);
      for (int i = 1; i < nr; i++) len += hypotf(route[i][0] - route[i - 1][0], route[i][2] - route[i - 1][2]);
      if (nr < 2 || off) plan = 0;
      if (plan && len < bestLen) { bestLen = len; memcpy(bp, p, sizeof bp); bh = h; }
      if (plan && len <= POLICE_ROUTE_MAX * d) got = 1;
      if (++plans >= POLICE_PLANS && bestLen < 1e29f) { memcpy(p, bp, sizeof p); h = bh; got = 1; }
#endif
    }
    if (!got && bestLen < 1e29f) { memcpy(p, bp, sizeof p); h = bh; got = 1; }
#ifndef POLICE_ZONE_OLD
#define POLICE_ZONE_OLD 0
#endif
    if (!got && !POLICE_ZONE_OLD && roadsT) {
      // №590: полицию не ставить туда, откуда не выехать (крыши L3, тупики):
      // ищем ближайшую валидную полосу дороги в радиусе до 120 м
      float lp[3], lh;
      if (trafficLaneSpot(roadsT, px, pz, 120.f, lp, &lh)) {
        memcpy(p, lp, sizeof p); h = lh; got = 1;
      }
    }
    if (!got) {   // дорог рядом нет — прежнее правило: 100 м сзади
      h = ph + (k & 1 ? .6f : -.6f); p[0] = px - sinf(h) * 100.f; p[1] = g->pCar->y; p[2] = pz - cosf(h) * 100.f;   // нет дорог рядом — как было (сзади)
    }
    // mcSpawn identifies a mission vehicle by model name and replaces it. A chase pool may
    // contain two copies of that model; hide only their lookup keys during this synchronous call.
    int hidden[MC_MAX], nh=0;
    if(!HNR_POOL_C3_OLD) for(int i=0;i<m->cars.n;i++) if(m->cars.c[i].on && !strcasecmp(m->cars.c[i].name,car)) {
      hidden[nh++]=i; m->cars.c[i].name[0]='\1';
    }
    McCar *pc = mcSpawn(&m->cars, g->pWorld, roadsT, car, p, h, 1, "chase", con, "");
    for(int i=0;i<nh;i++) m->cars.c[hidden[i]].name[0]=car[0];
    if (pc) pc->police = HNR_ORACLE_POLICE;
    // Правка Роба 280 (04.10: «полицейская от трёх ударов взрывается»): у машины погони L1 по диску SetHitPoints 0.5 (Pursuit\L1cop.con),
    // а машины миссий получают полный урон (цель тарана m3); полиция — машина ИИ, как трафик: урон машина-машина × CAR_HIT_DMG_K,
    // множитель снят с замера оригинала (близнец В14: минивэн трафика цел после 14 таранов). Сколько таранов у оригинала — В47.
    // «flag policedmgold 1» — прежнее (полный урон)
    if (pc && !gamePoliceDmgOld) pc->car.hitDmgK = 0.f;   // 0 — CAR_HIT_DMG_K
#if !defined(POLICE_SPAWN_STRAIGHT) || !POLICE_SPAWN_STRAIGHT
    // правка 197: полиция — по дорогам, без срезов дворами (срезы — у гонщиков с SetVehicleAIParams; у полиции параметров нет, и она
    // шла срезом 44 у J9-4 сквозь клумбу common_gens51Shape — твёрдую по данным диска — и вставала)
    if (pc) { pc->hasSk = 1; pc->skLo = pc->skHi = 0; }
#endif   // правка 121: поведение полиции оригинала (msncar.c POLICE_STANDOFF), за флагом (msncar.h)
  }
}
// Правка Роба 62: телефонная будка — своя машина (машина уровня; угнанная — назад на машину уровня). Машину миссии не трогает (0)
int gameMsnSummon(GameMsn *m) {
  Game *g = m->g; if (!*g->pHaveCol || m->forced) return 0;
  if (m->jacked) { carInit(g->pCar, &m->defCar, g->pWorld, g->pCar->x, g->pCar->y + .5f, g->pCar->z, g->pCar->h); *(float *)g->pLift = m->defLift;
    m->jacked = 0; m->jackedName[0] = 0; }
  return 1;
}
// правка 94: машина из галереи телефонной будки. Машина уровня (defaultcar) — назад на неё (gameMsnSummon); иначе модель и
// ходовые из пакета msncars (как SetForcedCar), состояние «угнанная» (не машина миссии): -> 1 — машина игрока сменилась
int gameMsnPhoneCar(GameMsn *m, const char *car) {
  Game *g = m->g; if (!*g->pHaveCol || m->forced || !car) return 0;
  const McGeo *G = mcGeoFind(car); CarParams P;
  if (!G) return 0;
  if (!(G->hasInfo && carParamsFromInfo(G->info, &P) == 1)) {
    char lc[16]; int k = 0; for (const char *q = car; *q && k < 15; q++) lc[k++] = (*q >= 'A' && *q <= 'Z') ? (char)(*q + 32) : *q; lc[k] = 0;
    carParamsFor(lc, &P);
    P.halfW = G->halfW; P.halfL = G->halfL; P.bodyOfs = G->bodyOfs; P.wheelX = G->wheelX; P.wheelF = G->wheelF; P.wheelR = G->wheelR;
    P.wheelRad = G->wheelRad; P.bodyLo = G->bodyLo; P.bodyHi = G->bodyHi;
  }
  if (!m->jacked) { m->defCar = g->pCar->p; m->defLift = *g->pLift; }
  carInit(g->pCar, &P, g->pWorld, g->pCar->x, g->pCar->y + .5f, g->pCar->z, g->pCar->h); g->pProps->havePreviousCar = 0;
  *(float *)g->pLift = G->lift;
  m->jacked = 1; strncpy(m->jackedName, car, sizeof m->jackedName - 1); m->jackedName[sizeof m->jackedName - 1] = 0;
  return 1;
}
// правка Роба 53 (30.09): машины, которые стоят в мире сами (локатор CarStart уровня, не миссии) — ракетная машина Гомера
// rocke_v у локатора «rocke» (l1r3.p3d, запись PS2 32:35). Стоит как машина миссии (MC_PARK), после каждой миссии
// ставится заново; △ рядом — игрок пересаживается в неё (gameMsnTakeWorld), своя остаётся стоять (main.c oldCar)
static int hnrOff;   // перепроверка 96: DisableHitAndRun (sr1i/sr2i/sr3i уровня 1 — уличные гонки) — Hit & Run выключен до конца миссии
#ifndef WORLD_CAR_FAR_R
#define WORLD_CAR_FAR_R 200.f      // те же числа, что в worldcars.h (сверка g10b-loop круг 5): сборки ботов не включают список машин
#define WORLD_CAR_MISSION_R 10.f
#endif
#define WORLD_CARS 4
static struct { char name[16]; float p[3], h; int on; } wcar[WORLD_CARS]; static int nwcar;
static void worldCarsSpawn(GameMsn *m) {
  Game *g = m->g; if (!*g->pHaveCol) return;
  for (int i = 0; i < nwcar; i++) if (wcar[i].on && !mcFind(&m->cars, wcar[i].name)) {
#ifndef WORLD_CAR_SPOT_OLD
    // №418: опора — под точкой локатора, а не до 2.5 м над ней (mcSpawn +0.5, carPlace ищет землю с y + 2): L6 hype_v вставала на
    // карниз гаража над стоянкой (-57.13 вместо -59.86) и сползала на 4.7 м. Правило порта: поиск с 0.5 м над локатором
    // (локатор CarStart — центр машины над землёй: L1 rocke -0.77, земля -0.81)
    float p[3] = {wcar[i].p[0], wcar[i].p[1] - 2.f, wcar[i].p[2]};
#else
    const float *p = wcar[i].p;
#endif
    mcSpawn(&m->cars, g->pWorld, roadsT, wcar[i].name, p, wcar[i].h, 1, MC_PARK, "", "");
  }
}
void gameMsnWorldCar(GameMsn *m, const char *car, const float p[3], float h) {
  for (int i = 0; i < nwcar; i++) if (!strcasecmp(wcar[i].name, car)) { worldCarsSpawn(m); return; }   // уже в списке (NEW GAME снова, flag msn)
  if (nwcar >= WORLD_CARS) return;
  snprintf(wcar[nwcar].name, sizeof wcar[nwcar].name, "%s", car); memcpy(wcar[nwcar].p, p, sizeof wcar[nwcar].p);
  wcar[nwcar].h = h; wcar[nwcar].on = 1; nwcar++; worldCarsSpawn(m);
}
void gameMsnWorldCarsReset(void) { nwcar = 0; }
// сверка g10b-loop круг 5 (P03-20): «свободная машина» уходит с выгрузкой своей зоны — игрок дальше WORLD_CAR_FAR_R (200 м) от её места, — и уходит, когда миссия ставит
// свою машину ближе WORLD_CAR_MISSION_R (10 м) от этого места; возвращается, когда игрок снова ближе (запас 20 м) и рядом нет машины миссии. Взятая игроком (on == 0) не трогается.
// WCAR_ZONE_OLD=1 — стоит весь уровень (прежнее)
#include "g10b_calc.h"   // WCAR_ZONE_OLD, WCAR_STEP_COST_OLD, правило ухода/возврата
void gameMsnWorldCarsStep(GameMsn *m, float px, float pz) {
  if (WCAR_ZONE_OLD) return;
  static int tick; if (!g10bEvery(&tick, 10)) return;   // круг 6 (цена): раз в 10 кадров
  static unsigned char away[WORLD_CARS];
  for (int i = 0; i < nwcar; i++) {
    if (!wcar[i].on) { away[i] = 0; continue; }
    float dx = wcar[i].p[0] - px, dz = wcar[i].p[2] - pz, d2 = dx * dx + dz * dz;
    int mis = 0;
    for (int k = 0; k < m->cars.n && !mis; k++) {
      const McCar *c = &m->cars.c[k]; if (!c->on || !strcasecmp(c->name, wcar[i].name)) continue;
      float ex = c->car.x - wcar[i].p[0], ez = c->car.z - wcar[i].p[2]; if (ex * ex + ez * ez < WORLD_CAR_MISSION_R * WORLD_CAR_MISSION_R) mis = 1;
    }
    int na = g10bWorldCarAway(away[i], d2, mis, WORLD_CAR_FAR_R);
    if (na && !away[i]) { if (mcFind(&m->cars, wcar[i].name)) mcRemove(&m->cars, wcar[i].name); }
    else if (!na && away[i]) worldCarsSpawn(m);
    away[i] = (unsigned char)na;
  }
}
int gameMsnWorldCarNear(GameMsn *m, float x, float z, float r, float out[4]) {
  for (int i = 0; i < nwcar; i++) {
    if (!wcar[i].on) continue;
    const McCar *c = mcFind(&m->cars, wcar[i].name); if (!c || !c->on) continue;
    float dx = c->car.x - x, dz = c->car.z - z, rr = r + c->car.p.halfL;
    if (dx * dx + dz * dz < rr * rr) { out[0] = c->car.x; out[1] = c->car.y; out[2] = c->car.z; out[3] = c->car.h; return i; }
  }
  return -1;
}
int gameMsnTakeWorld(GameMsn *m, int i) {
  if (i < 0 || i >= nwcar || !wcar[i].on) return 0;
  if (!gameMsnPhoneCar(m, wcar[i].name)) return 0;
  mcRemove(&m->cars, wcar[i].name); wcar[i].on = 0; return 1;
}
const char *gameMsnForcedCar(const GameMsn *m) { return m->forced ? m->forcedName : m->jacked ? m->jackedName : 0; }
static void plVehicle(void *u, const char *car, int forced, const MsnPlace *at) {
  if (GM->keepPlayer) { GM->skipped++; return; }
  GM->places++;
  if (forced && car && *car) { forcedOn(GM, car, at); inCar(G_); TELEPORTED(); return; }
  putCar(G_, at); inCar(G_); TELEPORTED();
}
static void plSwap(void *u, const MsnPlace *player, const MsnPlace *defCar, const MsnPlace *forcedCar) {   // SwapInDefaultCar
  if (GM->keepPlayer) { GM->skipped++; return; }
  if (!GM->forced) return;
  float hx_ = G_->pHomer->x, hz_ = G_->pHomer->z; int foot_ = *G_->pHaveHomer && gameOnFoot(G_);
  forcedOff(GM, defCar, forcedCar);
  if (player && player->loc) { putHomer(G_, player); TELEPORTED(); } else inCar(G_);
  if (!GAME_GIVENCAR52_OLD && GAME_C3_GIVEN_OLD && GM->forcedStartOn && foot_ && forcedCar && forcedCar->loc) {
    // 5.2: Гомер стоял у точки выданной машины (ближе 2 м) и не дальше 20 м от места, где начинал, — его возвращают туда, где он начинал; своя машина игрока на месте
    float dx = hx_ - forcedCar->pos.x, dz = hz_ - forcedCar->pos.z, ex = hx_ - GM->forcedStart[0], ez = hz_ - GM->forcedStart[2];
    if (dx * dx + dz * dz < 4.f && ex * ex + ez * ez <= 400.f) {
      putHomer(G_, &(MsnPlace){ .pos = { GM->forcedStart[0], GM->forcedStart[1], GM->forcedStart[2] }, .has_heading = 0, .locator = "", .loc = 0 }); TELEPORTED();
    }
  }
  if (GAME_C3_GIVEN_OLD) GM->forcedStartOn = 0;   // круг 3: правило выданной машины перенесено в конец миссии (cleanup); при подмене на свою (SwapInDefaultCar) оно не действует
}
static void plPlaceCar(void *u, const char *car, const MsnPlace *at) { (void)car; if (GM->keepPlayer) { GM->skipped++; return; } GM->places++; putCar(G_, at); TELEPORTED(); }
static void plEnter(void *u) { if (!GM->keepPlayer) inCar(G_); }
static void plWalk(void *u, const MsnPlace *to) {   // SetInitialWalk: оригинал ведёт Гомера сам (из двери дома); ставим в конец пути
  if (GM->keepPlayer) { GM->skipped++; return; }
  if (GM->inSetup && !GM->startArmed) { GM->skipped++; return; }
  GM->places++; putHomer(G_, to); TELEPORTED();
}
static void propsRaceC2(void *u, const char *name, int load) { trafficStreetRace(roadsT, load); VIEW(props, race_props, name, load); }   // StreetRacePropsLoad/Unload: на уличной гонке пешеходов нет
static void navMarker(void *u, const MsnMarker *m) {
  GM->markerOn = m != 0; if (m) GM->marker = *m;
  if (!GAME_C2_NAV_OLD && m && !m->target && roadsT && *GM->g->pHaveCol && !gameOnFoot(GM->g) && (!GM->navOn || fabsf(GM->navTx - m->pos.x) > 1.f || fabsf(GM->navTz - m->pos.z) > 1.f)) {
    // круг 2, g02 P16-06: путь по дорогам до цели строится один раз при старте цели (дальше стрелки рисует вид по gameMsnNavArrows)
    const Car *c = GM->g->pCar; trafficNavBuild(roadsT, c->x, c->z, c->h, m->pos.x, m->pos.z); GM->navOn = 1; GM->navTx = m->pos.x; GM->navTz = m->pos.z;
  }
  if (!m) GM->navOn = 0;
  VIEW(nav, marker, m);
}
int gameMsnNavArrows(GameMsn *m, TrNavArrow *out, int max) { if (!roadsT || !m->navOn || gameOnFoot(m->g)) return 0; return trafficNavArrows(roadsT, m->g->pCar->x, m->g->pCar->z, out, max); }
static void uiFlash(void *u, int what, const char *text) { GM->flashes++; VIEW(ui, flash, what, text); }
static void uiFailed(void *u, int reason, const char *text, const char *hint) { GM->fails++; VIEW(ui, failed, reason, text, hint); }

// ------------------------------------------------------------------ персонажи
// Правка 180 (близнец О4 03.10, кадр f03600: Ральф гонки sr3 стоит на sr3_ralph_sd под флагом; Б2: оригинал читает louie_m у
// школы): персонаж бонус-миссии и фоновый с тем же именем — разные люди (Ральф уровней 1, 4, 5: маршрут ambient_ralph и место
// гонки sr3). Поиск по имени — сначала персонаж миссии, потом бонусный, потом остальные
static GameNpc *npcFind(GameMsn *m, const char *name) {
  GameNpc *best = 0; int bp = -1;
  for (int i = 0; i < m->nnpc; i++) {
    GameNpc *n = &m->npc[i]; if (!msn_streq(n->name, name)) continue;
    int p = n->kind == MSN_NPC_MISSION ? 2 : n->bonus ? 1 : 0;
    if (p > bp) { bp = p; best = n; }
  }
  return best;
}
static GameNpc *npcFindSeller(GameMsn *m, const char *name) {   // персонаж-продавец: то же имя, но не бонус-персонаж (SELLER96_OLD=1 — npcFind, как было)
  if (!gameSeller96) return npcFind(m, name);
  for (int i = 0; i < m->nnpc; i++) { GameNpc *n = &m->npc[i]; if (msn_streq(n->name, name) && !n->bonus && n->kind == MSN_NPC_PLACE) return n; }   // сначала «место» (продавец сам): у гонки sr3 свой zmale1 этапа миссии, продавец — не он
  for (int i = 0; i < m->nnpc; i++) { GameNpc *n = &m->npc[i]; if (msn_streq(n->name, name) && !n->bonus) return n; }               // продавца взяла себе миссия (L1 m2: AddNPC("barney", "m2_barney"), правка 140): место разговора идёт за ним
  return 0;
}
// №984 (боты езды L3–L7, цикл по персонажам: значки продавцов и собеседника mvDrawMarkers звали npcFind по имени КАЖДЫЙ КАДР на каждого продавца — перебор до 24 персонажей со сравнением строк,
// 0,3–0,48 мс PSP на L3, L4, L6, L7). Теперь ответ npcFind запоминается по указателю имени (16 записей): запись годна, пока не менялся список персонажей (npcGen растёт при появлении, снятии,
// очистке миссии, новой миссии), указатель, сама строка имени и GameMsn те же — результат побитно тот же (тот же GameNpc*). GAMEMSNNPC984_OLD=1 / flag msnnpc984old 1 — как было; flag msnnpcchk 1 — каждый ответ
// сверяется с npcFind (служебно, строка MSNNPC984)
#ifndef GAMEMSNNPC984_OLD
#define GAMEMSNNPC984_OLD 0
#endif
int gameMsnNpc984Old = GAMEMSNNPC984_OLD, gameMsnNpcChk, gameMsnNpcTime; unsigned gameMsnNpcCalls, gameMsnNpcMiss, gameMsnNpcBad, gameMsnNpcUs;
static unsigned npcGen = 1;
typedef struct { const char *key; const GameMsn *m; unsigned gen; int idx; char name[48]; } NpcCache;
static NpcCache npcCache[16];
static const GameNpc *gameMsnNpcFast(const GameMsn *m, const char *name) {
  if (gameMsnNpc984Old || !name || strlen(name) >= sizeof npcCache[0].name) return npcFind((GameMsn *)m, name);
  NpcCache *c = &npcCache[((uintptr_t)name >> 3) & 15]; const GameNpc *r; gameMsnNpcCalls++;
  if (c->key == name && c->m == m && c->gen == npcGen && !strcmp(c->name, name)) r = c->idx >= 0 ? &m->npc[c->idx] : 0;
  else { GameNpc *f = npcFind((GameMsn *)m, name); c->key = name; c->m = m; c->gen = npcGen; c->idx = f ? (int)(f - m->npc) : -1; strcpy(c->name, name); r = f; gameMsnNpcMiss++; }
  if (gameMsnNpcChk && r != npcFind((GameMsn *)m, name)) gameMsnNpcBad++;
  return r;
}
const GameNpc *gameMsnNpc(const GameMsn *m, const char *name) {
#ifndef __PSP__
  return gameMsnNpcFast(m, name);
#else
  if (!gameMsnNpcTime) return gameMsnNpcFast(m, name);
  unsigned t0 = sceKernelGetSystemTimeLow(); const GameNpc *r = gameMsnNpcFast(m, name); gameMsnNpcUs += sceKernelGetSystemTimeLow() - t0; return r;   // служебно (flag msnnpctime 1): мкс на поиск
#endif
}
// кого заменяет новый: бонусный — себя же (та же миссия); персонаж миссии — бонусного только в его же миссии (вступление sr3:
// AddNPC("ralph", "sr3_ralph_sd")), иначе не бонусного; фоновые и продавцы — не бонусного
static GameNpc *npcSlot(GameMsn *m, const char *name, int kind, int bonus) {
  for (int pass = 0; pass < 2; pass++)
    for (int i = 0; i < m->nnpc; i++) {
      GameNpc *c = &m->npc[i]; if (!msn_streq(c->name, name)) continue;
      int want = bonus ? bonus : kind == MSN_NPC_MISSION && pass == 0 ? m->rt.cur + 1 : 0;
      if (want > 0 ? c->bonus == want : !c->bonus) return c;
    }
  return 0;
}
static void npcSpawnB(void *u, const char *name, const MsnPlace *at, int kind, float radius, const char *interior, int bonus) {
  npcGen++;
  GameNpc *n = npcSlot(GM, name, kind, bonus);
  if (!n) { if (GM->nnpc >= GAME_NPC_MAX) return; n = &GM->npc[GM->nnpc++]; }
  memset(n, 0, sizeof *n);
  strncpy(n->name, name, sizeof n->name - 1); strncpy(n->interior, interior ? interior : "", sizeof n->interior - 1);
  n->pos = at->pos; n->h = at->has_heading ? at->heading : 0.f; n->kind = kind; n->radius = radius; n->bonus = bonus;
  GM->lastNpc = (int)(n - GM->npc);
  VIEW(npc, spawn, name, at, kind, radius, interior);
}
static void npcSpawn(void *u, const char *name, const MsnPlace *at, int kind, float radius, const char *interior) {
  npcSpawnB(u, name, at, kind, radius, interior, 0);
}
int gameMsnAmbWantRange(const GameMsn *m, float x, float y, float z, float range, int max, int *want) {
  if (max <= 0 || !want) return 0;
  int n = 0; float d[GAME_NPC_MAX];
  for (int i = 0; i < m->nnpc && n < GAME_NPC_MAX; i++) {
    const GameNpc *c = &m->npc[i];
    if (c->kind == MSN_NPC_MISSION) continue;
#ifdef GAME_AMB_SCRIPT_ORDER
    if (n < max) want[n++] = i;   // прежнее правило: модели первым по порядку скрипта, навсегда
#else
#ifdef GAME_AMB_TAG_ONLY   // прежнее (правка 140): свой мир по имени интерьера — красный гейта tests/npc/rooms.sh
    if ((c->interior[0] != 0) != (m->interior != 0) || (m->interior && !msn_streq(c->interior, m->interior))) continue;   // свой мир
    float e = hypotf(c->pos.x - x, c->pos.z - z); if (e > range) continue;
#else
    // правка 171 (Роб 01.10: «нет Лизы в коридоре, нет Апу, ходил в пустоту»): AddAmbientCharacter не называет комнату (Апу —
    // Kwik-E-Mart, Лиза — школа, leveli.mfk), поэтому свой мир — по месту: расстояние 3D (комнаты стоят под городом, y < −10);
    // имя комнаты отсекает, только если персонаж им помечен
    if (c->interior[0] && (!m->interior || !msn_streq(c->interior, m->interior))) continue;
    float dy = c->pos.y - y, e = sqrtf((c->pos.x - x) * (c->pos.x - x) + dy * dy + (c->pos.z - z) * (c->pos.z - z));
    if (e > range) continue;
#endif
    int k = n < max ? n++ : max;   // вставка по расстоянию: ближние первыми, лишний дальний выпадает
    if (k == max) { if (e >= d[max - 1]) continue; k = max - 1; }
    while (k > 0 && d[k - 1] > e) { d[k] = d[k - 1]; want[k] = want[k - 1]; k--; }
    d[k] = e; want[k] = i;
#endif
  }
  return n;
}
int gameMsnAmbWant(const GameMsn *m, float x, float y, float z, int max, int *want) {
  return gameMsnAmbWantRange(m, x, y, z, GAME_AMB_NEAR, max, want);
}
static void npcWaypoint(void *u, const char *name, const MsnPlace *at) {   // Add*NPCWaypoint: идёт по точкам по порядку
  // правка 180: маршрут — тому, кого сценарий только что поставил (у двух Ральфов уровня 1 свои маршруты)
  GameNpc *n = GM->lastNpc < GM->nnpc && msn_streq(GM->npc[GM->lastNpc].name, name) ? &GM->npc[GM->lastNpc] : npcFind(GM, name);
  if (n && n->nwp < GAME_NPC_WP && at->loc) { n->wp[n->nwp++] = at->pos; n->walking = 1; }
  VIEW(npc, waypoint, name, at);
}
static void npcRemove(void *u, const char *name) {
  npcGen++;
  GameNpc *n = npcFind(GM, name);
  if (n) { *n = GM->npc[--GM->nnpc]; }
  VIEW(npc, remove, name);
}
// ------------------------------------------------------------------ машины миссии с ИИ (viewer/msncar.c)
static void placeXYZ(const MsnPlace *at, float p[3]) { p[0] = at->pos.x; p[1] = at->pos.y; p[2] = at->pos.z; }
static void carLoad(void *u, const char *name, const char *file, const char *slot) { VIEW(cars, load, name, file, slot); }
static void carSpawn(void *u, const MsnCarSpawn *s) {   // AddStageVehicle: машина на локаторе (нет локатора в данных — не ставится)
  if (s->at.loc && *G_->pHaveCol) {
    float p[3]; placeXYZ(&s->at, p);
    if (!mcSpawn(&GM->cars, G_->pWorld, roadsT, s->name, p, mcCarHeading(s->at.heading), s->at.has_heading, s->ai, s->con, s->driver)) return;
  }
  VIEW(cars, spawn, s);
}
static void carWaypoint(void *u, int index, const MsnPlace *at) {   // AddStageWaypoint: круги — SetRaceLaps этапа (рантайм прочёл до команд)
  if (at->loc) { float p[3]; placeXYZ(at, p); mcWaypoint(&GM->cars, index, p, GM->rt.laps); }
  VIEW(cars, waypoint, index, at);
}
static void carActivate(void *u, const char *name, const MsnPlace *at, const char *ai) {
  float p[3]; if (at) placeXYZ(at, p);
  if (*G_->pHaveCol) mcActivate(&GM->cars, G_->pWorld, name, at ? p : 0, at ? mcCarHeading(at->heading) : 0.f, at ? at->has_heading : 0, ai);
  VIEW(cars, activate, name, at, ai);
}
static void carParamsCb(void *u, const char *name, int a, int b) { mcParams(&GM->cars, name, a, b); VIEW(cars, ai_params, name, a, b); }
// сверка g03-aicars круг 2 (g11 P11-02): предметы реагируют на машины ИИ рядом с игроком, не только на машину игрока
static void svCarTouch(void *u, Car *c) { GameMsn *m = (GameMsn *)u; propsHitCar(m->g->pProps, m->g->pWorld, c, 0); }
static void carCatchup(void *u, const char *name, int kind, const float *p, int n) { mcCatchup(&GM->cars, name, kind, p, n); VIEW(cars, catchup, name, kind, p, n); }
// SetMaxTraffic(n) / NoTrafficForStage (0) этапа; -1 — этап молчит (как на уровне: столько, сколько мест трафика в данных).
// Лишние машины трафика уходят сразу (этап гонки начинается отсчётом / затемнением); у ботов — тот же трафик (снимок мира)
static void trafficSet(GameMsn *gm, int n) {
  if (!roadsT || !roadsT->hd) return;
  int def = roadsT->hd->nSlot < TR_MAXCAR ? (int)roadsT->hd->nSlot : TR_MAXCAR, m = n < 0 || n > def ? def : n;
  roadsT->maxCars = m;
  for (int i = m; i < TR_MAXCAR; i++) roadsT->car[i].mode = TRM_OFF;
  gm->trafficMax = m;
}
static void trafficMax(void *u, int n) { GM->trafficStage = n; trafficSet(GM, n); }
// P06-25: ClearTrafficForStage / NoTrafficForStage — машины трафика в радиусе r вокруг игрока исчезают (NoTrafficForStage потом держит лимит 0 — msn_game.c)
static void trafficClear(void *u, float r) {
  if (!roadsT || !roadsT->hd) return;
  Game *g = GM->g; int foot = *g->pHaveHomer && gameOnFoot(g);
  float px = foot ? g->pHomer->x : g->pCar->x, pz = foot ? g->pHomer->z : g->pCar->z;
  for (int i = 0; i < TR_MAXCAR; i++) { if (roadsT->car[i].mode == TRM_OFF) continue; float dx = roadsT->car[i].car.x - px, dz = roadsT->car[i].car.z - pz; if (dx * dx + dz * dz < r * r) roadsT->car[i].mode = TRM_OFF; }
}
#ifndef HNR_HIT_MODEL_C3_OLD
#define HNR_HIT_MODEL_C3_OLD 0
#endif
#ifndef MSN_DUMPHIT_OLD
#define MSN_DUMPHIT_OLD 0      // круг 7, g07 строки 21, 44 (P07-10, 5.9; сверено с оригиналом): удар по цели dump роняет предмет, только если сила удара не меньше 0,05 от максимальной, то есть импульс не меньше 5000 (у нас нормированный импульс машин = импульс / 100000, car.c hitImp). Было: любой удар быстрее 1 м/с. 1 — как было
#endif
#define DUMPHIT_J 5000.f
int gameMsnDumpHitStrong(float J) { return MSN_DUMPHIT_OLD || J >= DUMPHIT_J; }
static void carHit(void *u, const char *name) {   // dump: таран -> предмет
  GameMsn *m = u; if (gameMsnDumpHitStrong(m->g->pCar->hitImp)) msn_notify(&m->rt, MSN_IN_HIT_VEHICLE, name, 0);
  if (m->rt.target && !strcasecmp(name, m->rt.target)) m->hitTgtT = 0.5f;   // круг 2: удар игрока по цели — урон цели в ближайшие 0,5 с засчитывается
  // правка 121: удар по полицейской — нарушение Hit & Run, как в данных погони (pursuit.ts: удар 5, разбита 25 — один раз)
  McCar *c = (McCar *)mcFind(&m->cars, name);
  if (!GAME_HNRAI_OLD && !HNR_HIT_MODEL_C3_OLD) {
    // P03 Д1: машина погони (модель из списка погони уровня) H&R не двигает ни ударом, ни взрывом; остальные машины ИИ миссий (соперники, попутчики) — удар +5, взрыв +25 и 10 монет,
    // кроме иммунных (цель destroy этапа). Раз в 1,5 с и паузу в погоне держит gameHnrOffense
    if (!c || (m->hnr.car && !strcasecmp(name, m->hnr.car))) return;
    if (m->rt.status == MSN_RUNNING && m->rt.target && !strcasecmp(name, m->rt.target) && (m->rt.obj == MSN_OBJ_DESTROY || (m->rt.obj == MSN_OBJ_DUMP && m->rt.nbind == 0))) return;   // иммунны цель «разбить» и цель «собрать выпавшее» в режиме столкновений (круг 3, строка 8)
    int wreck = c->car.destroyed && !c->hnrWreck; if (wreck) c->hnrWreck = 1;
    if (gameHnrOffense(m->g, wreck ? HNR_CAR_WRECK : HNR_CAR_HIT) && wreck) { shopCoinsAdd(&m->shop, 10, 0); }
    return;
  }
  if (c && !strcasecmp(name, HNR_HIT_MODEL_C3_OLD ? "cPolice" : m->hnr.car)) {
    int wreck = c->car.destroyed && !c->hnrWreck; if (wreck) c->hnrWreck = 1;
    gameHnrOffense(m->g, wreck ? HNR_CAR_WRECK : HNR_CAR_HIT);
  }
}
// место игрока в гонке с соперниками (правка 300, с правки 326 — красный контроль racepos300): остаток пути до финиша в метрах — у игрока по его точкам гонки (AddCollectible,
// круги SetRaceLaps), у соперника по его точкам этапа; 1 — первый; 0 — не гонка. Прежние доли по числу точек — mcRacePosOld.
// Ниже — прежняя заметка правки 178 (первый отрезок от места старта этапа): для прежней меры и догонялки (gameMsnRaceFracLen)
// правка 178 (Роб 01.10: «обгоняю Скиннера срезками — игра не понимает, что я первый»; близнец В9, ps2-race-progress.yaml):
// у первой точки прежней не было — отрезок = расстояние до неё, доля игрока 0 до самой точки (в S-M-R-T точка одна — финиш:
// игрок всегда «2-й»). Первый отрезок — от места игрока на старте этапа гонки (запоминается в начале этапа и при повторе)
static void raceStart(const MsnRuntime *rt, const MsnWorld *w, float *sx, float *sz) {
  static int rkey = -1; static float rsx, rsz, rt0 = 1e9f;
  int key = (rt->cur << 16) ^ (rt->phase << 8) ^ rt->stage;
  if (key != rkey || rt->t_stage < rt0) { rkey = key; rsx = w->player.x; rsz = w->player.z; }
  rt0 = rt->t_stage; *sx = rsx; *sz = rsz;
}
// правка 326 (Роб 04.10: «гонка со Скиннером всё равно не считает, кто первый: ехал рядом — не понимает в реальном времени»;
// указание Роба: «кто ближе к точке — тот и первый»): место считается каждый кадр в gameMsnTick (raceTrack -> msncar.c
// mcRacePlaceRob) по точкам гонки ИГРОКА для всех: больше пройдено точек — впереди, поровну — кто ближе по прямой к той же
// следующей точке (гистерезис 1 м). Здесь — только отдать место этого кадра. Прежние меры — за флагами (красные контроли):
// mcRacePos300 (flag racepos300 1) — остаток метров по своим точкам (правка 300), mcRacePosOld (flag raceposold 1) — правка 178
static int raceInPt(void *u, int k, const float p[3]) {   // соперник внутри точки гонки k — так же, как игрок (рантайм: radius_car)
  const GameMsn *m = u; const MsnRuntime *rt = &m->rt;
  const MsnLoc *l = rt->nitems > 0 ? msn_arg_loc(&m->f, rt->items[k % rt->nitems], 0) : 0; if (!l) return 0;
  MsnVec3 v = {p[0], p[1], p[2]};
  return msn_loc_contains(&m->f, l, v, rt->radius_car) || hypotf(l->pos[0] - p[0], l->pos[2] - p[2]) < 10.f;   // 10 м — WP_R (msncar.c)
}
static void raceTrack(GameMsn *m) {
  const MsnRuntime *rt = &m->rt; m->racePlace = m->raceRacers = 0; m->raceDp = 0.f; m->raceOffRoad = 0;
  if (rt->status != MSN_RUNNING || rt->obj != MSN_OBJ_RACE || !m->cars.nwp || rt->nitems < 1) return;
  MsnWorld w; gameMsnWorld(m, &w);
  int n = rt->nitems, L = rt->laps > 0 ? rt->laps : 1, pk = rt->lap * n + rt->next_item;
  float pts[MSN_MAX_ITEMS][3], p[3] = {w.player.x, w.player.y, w.player.z};
  for (int k = 0; k < n; k++) { const MsnLoc *b = msn_arg_loc(&m->f, rt->items[k], 0);
    const float *q = b ? b->pos : k ? pts[k - 1] : p; pts[k][0] = q[0]; pts[k][1] = q[1]; pts[k][2] = q[2]; }
  m->raceOffRoad = !raceOnRoad(roadsT, p[0], p[1], p[2]);
  m->racePlace = mcRacePlaceRob(&m->cars, (const float (*)[3])pts, n, L, pk, p, raceInPt, m, &m->raceDp, &m->raceRacers);
}
int gameMsnRacePos(const GameMsn *m, int *racers) {
  const MsnRuntime *rt = &m->rt; if (racers) *racers = 0;
  if (rt->status != MSN_RUNNING || rt->obj != MSN_OBJ_RACE || !m->cars.nwp || rt->nitems < 1) return 0;
  if (!mcRacePosOld && !mcRacePos300) { if (racers) *racers = m->raceRacers; return m->racePlace; }
  MsnWorld w; gameMsnWorld(m, &w);
  int n = rt->nitems, L = rt->laps > 0 ? rt->laps : 1, next = rt->lap * n + rt->next_item;
  float rsx, rsz; raceStart(rt, &w, &rsx, &rsz);
  float pts[MSN_MAX_ITEMS][3], p[3] = {w.player.x, w.player.y, w.player.z};
  for (int k = 0; k < n; k++) { const MsnLoc *b = msn_arg_loc(&m->f, rt->items[k], 0);   // точки гонки игрока (AddCollectible)
    const float *q = b ? b->pos : k ? pts[k - 1] : p; pts[k][0] = q[0]; pts[k][1] = q[1]; pts[k][2] = q[2]; }
  // правка 300: остаток пути до финиша в метрах (msncar.c mcRacePlace); mcRacePosOld — прежние доли по числу точек (красный)
  if (mcRacePosOld) {
#ifdef GAME_RACE_FIRSTLEG_OLD   // прежнее до правки 178 (красный): первый отрезок — от места игрока сейчас (доля 0 до точки)
    rsx = w.player.x; rsz = w.player.z;
#endif
    return mcRacePlaceOld(&m->cars, mcPathFracCount((const float (*)[3])pts, n, L, next, rsx, rsz, p), racers);
  }
  return mcRacePlace(&m->cars, mcPathRemaining((const float (*)[3])pts, n, L, next, p), racers);
}
// 03.10 (m7, догонялка соперников): доля пройденного пути гонки игрока ПО ДЛИНЕ — отрезки от места старта этапа через его точки
// (AddCollectible этапа race, круги SetRaceLaps): пройденные целиком + доля текущего; < 0 — не гонка. Догонялка (msncar.c
// playerDoneLen) по ней выбирает, где игрок на пути соперника, когда путь проходит рядом сам с собой (m7: финиш у старта)
float gameMsnRaceFracLen(const GameMsn *m) {
  const MsnRuntime *rt = &m->rt;
  if (rt->status != MSN_RUNNING || rt->obj != MSN_OBJ_RACE || !m->cars.nwp || rt->nitems < 1) return -1.f;
  MsnWorld w; gameMsnWorld(m, &w);
  int n = rt->nitems, L = rt->laps > 0 ? rt->laps : 1, next = rt->lap * n + rt->next_item, T = n * L;
  float ax, az; raceStart(rt, &w, &ax, &az);
  if (next >= T) return 1.f;
  float done = 0.f, tot = 0.f;
  for (int k = 0; k < T; k++) { const MsnLoc *b = msn_arg_loc(&m->f, rt->items[k % n], 0); if (!b) continue;
    float leg = hypotf(b->pos[0] - ax, b->pos[2] - az);
    if (k < next) done += leg;
    else if (k == next && leg > 1.f) done += leg * (1.f - fminf(1.f, hypotf(b->pos[0] - w.player.x, b->pos[2] - w.player.z) / leg));
    tot += leg; ax = b->pos[0]; az = b->pos[2]; }
  return tot > 1.f ? done / tot : 0.f;
}
const McCar *gameMsnTargetCar(const GameMsn *m) {
  if (!m->rt.target || m->rt.status == MSN_IDLE) return 0;
  return mcFind((MsnCars *)&m->cars, m->rt.target);
}
#ifndef GAME_NPC_SOLID
#define GAME_NPC_SOLID 1   // отрицательный контроль гейта tests/missions/npc_block.sh: 0 — персонажи проходят сквозь Гомера
#endif
static int npcBlocksHomer(const GameNpc *n, const Game *g, float x, float z) {   // Гомер пешком на пути персонажа
  if (!GAME_NPC_SOLID || !*g->pHaveHomer || !gameOnFoot(g)) return 0;
  const Homer *H = g->pHomer; float dx = x - H->x, dz = z - H->z, rad = GAME_NPC_RADIUS + HOMER_RADIUS;
  return fabsf(n->pos.y - H->y) < HOMER_HEIGHT && dx * dx + dz * dz < rad * rad;
}
// В85: собеседник цели talkto не подходит к Гомеру пешком ближе stop (game_msn.h GAME_NPC_TALK_STOP): шаг в (x, z) запрещён, если он
// ближе stop и ближе, чем сейчас (от Гомера отойти можно — не застревает, когда Гомер сзади). stop 0 — правило выключено
static int npcStopsAtHomer(const GameNpc *n, const Game *g, float x, float z, float stop) {
  if (stop <= 0.f || !*g->pHaveHomer || !gameOnFoot(g)) return 0;
  const Homer *H = g->pHomer;
  if (fabsf(n->pos.y - H->y) >= HOMER_HEIGHT) return 0;
  float d1 = hypotf(x - H->x, z - H->z), d0 = hypotf(n->pos.x - H->x, n->pos.z - H->z);
  return d1 < stop && d1 < d0;
}
// В85: где на PS2 записано расстояние покоя собеседника talkto — только там правило остановки (game_msn.h GAME_NPC_TALK_STOP).
// Одна строка — одна запись PS2: фаза (вступление миссии, имя из пакета), собеседник цели talkto, расстояние. Фаза m0sd есть только
// в пакете уровня 1. Всех остальных персонажей, в том числе других собеседников talkto, держит прежняя граница столкновения 0.6 м
static const struct { const char *phase, *npc; float stop; } npcTalkStops[] = {
  {"m0sd", "marge", GAME_NPC_TALK_STOP},   // ps2_v85_marge_rtti_bound (факт ps2-marge-distance-cancel-window)
};
static float npcTalkStop(const GameMsn *m, const GameNpc *n) {   // 0 — правило не действует
  const MsnRuntime *rt = &m->rt;
  if (GAME_NPC_TALK_STOP_OLD || n->kind != MSN_NPC_MISSION || n->bonus || rt->status != MSN_RUNNING || !rt->st ||
      rt->obj != MSN_OBJ_TALKTO || !rt->target || !msn_streq(rt->target, n->name) || rt->cur < 0 || (unsigned)rt->cur >= m->f.nmissions)
    return 0.f;
  const MsnMission *mm = &m->f.missions[rt->cur];
  unsigned ph = rt->phase == 0 ? mm->intro : mm->main;
  if (ph == MSN_NONE16 || ph >= m->f.nphases) return 0.f;
  const char *pn = msn_str(&m->f, m->f.phases[ph].name);
  for (unsigned i = 0; i < sizeof npcTalkStops / sizeof *npcTalkStops; i++)
    if (msn_streq(pn, npcTalkStops[i].phase) && msn_streq(n->name, npcTalkStops[i].npc)) return npcTalkStops[i].stop;
  return 0.f;
}
// Правка Роба 217 (03.10: «Мардж не замечает машину — проходит сквозь неё и только потом начинает обходить»): машина игрока —
// препятствие на пути персонажа миссии. Прямой путь к точке маршрута режет кузов (с запасом радиуса персонажа) — персонаж идёт к
// углу кузова, через который путь короче (как Гомер обходит к двери, homer.c HS_TO_DOOR), с угла — дальше к точке. Правило порта:
// в данных маршрута обхода нет; GAME_NPC_CAR_AVOID=0 — прежнее (красный контроль tests/npc/car_host.sh)
#ifndef GAME_NPC_CAR_AVOID
#define GAME_NPC_CAR_AVOID 1
#endif
static int segHitsBox(float ax, float az, float bx, float bz, float hw, float hl) {   // отрезок a->b режет |x|<hw, |z|<hl (Лян–Барски)
  float t0 = 0.f, t1 = 1.f, dx = bx - ax, dz = bz - az;
  float P[4] = {-dx, dx, -dz, dz}, Q[4] = {ax + hw, hw - ax, az + hl, hl - az};
  for (int i = 0; i < 4; i++) {
    if (fabsf(P[i]) < 1e-6f) { if (Q[i] < 0.f) return 0; continue; }
    float r = Q[i] / P[i];
    if (P[i] < 0.f) { if (r > t1) return 0; if (r > t0) t0 = r; } else { if (r < t0) return 0; if (r < t1) t1 = r; }
  }
  return t0 < t1;
}
static int npcAroundCarOf(const Game *g, const Car *c, const GameNpc *n, float *gx, float *gz) {   // сверка g02-traffic: обход угла кузова любой машины (было — только машины игрока)
  if (!GAME_NPC_CAR_AVOID || !*g->pHaveCol || !c) return 0;
  if (fabsf(c->y - n->pos.y) > 2.5f) return 0;   // другой этаж / комната под городом
  float fx = sinf(c->h), fz = cosf(c->h), rx = fz, rz = -fx;
  float cx = c->x + fx * c->p.bodyOfs, cz = c->z + fz * c->p.bodyOfs;
  float mg = GAME_NPC_RADIUS + .2f, hw = c->p.halfW + mg, hl = c->p.halfL + mg;
  float ax = (n->pos.x - cx) * rx + (n->pos.z - cz) * rz, az = (n->pos.x - cx) * fx + (n->pos.z - cz) * fz;
  float bx = (*gx - cx) * rx + (*gz - cz) * rz, bz = (*gx - cx) * fx + (*gz - cz) * fz;
  if (fabsf(ax) < c->p.halfW && fabsf(az) < c->p.halfL) return 0;   // стоит в самом кузове (поставили сценарием) — идёт как шёл
  if (!segHitsBox(ax, az, bx, bz, hw * .98f, hl * .98f)) return 0;
  // кратчайший обход прямоугольника: от места к видимому углу, по углам вдоль борта (по или против часовой), пока точка не видна.
  // Ближайшая цель — первый угол пути; если персонаж уже на нём (ближе 0,2 м) — следующий угол или сама точка
  const float K[4][2] = {{-hw * 1.05f, -hl * 1.05f}, {hw * 1.05f, -hl * 1.05f}, {hw * 1.05f, hl * 1.05f}, {-hw * 1.05f, hl * 1.05f}};
  const float sw = hw * .98f, sl = hl * .98f;
  float best = 1e9f, qx = 0.f, qz = 0.f;
  for (int i = 0; i < 4; i++) {
    if (segHitsBox(ax, az, K[i][0], K[i][1], sw, sl)) continue;
    for (int dir = -1; dir <= 1; dir += 2) {
      float L = hypotf(K[i][0] - ax, K[i][1] - az); int j = i, second = -1, ok = 0;
      for (int step = 0; step < 4; step++) {
        if (!segHitsBox(K[j][0], K[j][1], bx, bz, sw, sl)) { L += hypotf(bx - K[j][0], bz - K[j][1]); ok = 1; break; }
        int nj = (j + dir + 4) % 4; L += hypotf(K[nj][0] - K[j][0], K[nj][1] - K[j][1]); if (second < 0) second = nj; j = nj;
      }
      if (!ok || L >= best) continue;
      best = L;
      if (hypotf(K[i][0] - ax, K[i][1] - az) < .2f) { if (second >= 0) { qx = K[second][0]; qz = K[second][1]; } else { qx = bx; qz = bz; } }
      else { qx = K[i][0]; qz = K[i][1]; }
    }
  }
  if (best > 1e8f) return 0;
  *gx = cx + rx * qx + fx * qz; *gz = cz + rz * qx + fz * qz;
  return 1;
}
static int npcAroundCar(const Game *g, const GameNpc *n, float *gx, float *gz) { return npcAroundCarOf(g, g->pCar, n, gx, gz); }
// №510 (Роб: «Милхаус находит на Барта, стоит и дёргается; у NPC нет логики обходить людей»): путь перекрыт игроком или другим персонажем —
// не стоять в упор с походкой на месте, а обойти сбоку: шаг по направлению, повёрнутому на 50° и 90° в сторону, где свободно (сторона держится
// AVOID_HOLD с — не прыгает кадр от кадра), без обрыва под ногами; нельзя ни туда, ни туда — стоит (клип покоя). Поворот корпуса плавный.
// -DGAME_NPC_AVOID_OLD=1 — прежнее (стоит с клипом ходьбы, пока путь занят)
#ifndef GAME_NPC_AVOID_OLD
#define GAME_NPC_AVOID_OLD 0
#endif
#define AVOID_HOLD 1.5f
static const GameMsn *npcCtx;   // мир персонажей на время npcStep (gameMsnStep)
static int npcBlocksNpc(const GameNpc *n, float x, float z) {   // другой персонаж (не сбитый, тот же этаж) на пути
  if (GAME_NPC_AVOID_OLD || !npcCtx) return 0;
  for (int i = 0; i < npcCtx->nnpc; i++) { const GameNpc *o = &npcCtx->npc[i]; if (o == n || o->knock == TRP_FLY || o->knock == TRP_DOWN) continue;
    if (fabsf(o->pos.y - n->pos.y) >= HOMER_HEIGHT) continue;
    float dx = x - o->pos.x, dz = z - o->pos.z, rad = 2.f * GAME_NPC_RADIUS;
    if (dx * dx + dz * dz < rad * rad && hypotf(n->pos.x - o->pos.x, n->pos.z - o->pos.z) >= hypotf(dx, dz)) return 1; }   // уже вплотную и отходит — не держим
  return 0;
}
static int npcSidestep(GameNpc *n, const Game *g, float ux, float uz, float s, float *nx, float *nz) {
  float bx = 0.f, bz = 0.f; int have = 0;
  if (*g->pHaveHomer && gameOnFoot(g)) { bx = g->pHomer->x - n->pos.x; bz = g->pHomer->z - n->pos.z; have = 1; }
  if (!have && npcCtx) for (int i = 0; i < npcCtx->nnpc; i++) { const GameNpc *o = &npcCtx->npc[i]; if (o == n) continue;
    if (hypotf(o->pos.x - n->pos.x, o->pos.z - n->pos.z) < 2.5f * GAME_NPC_RADIUS) { bx = o->pos.x - n->pos.x; bz = o->pos.z - n->pos.z; have = 1; break; } }
  if (!n->avoidSide) n->avoidSide = (ux * bz - uz * bx) > 0.f ? -1 : 1;   // сторона, противоположная той, где стоит преграда
  n->avoidT = AVOID_HOLD;
  static const float ang[4] = {0.87f, 1.57f, -0.87f, -1.57f};   // 50°, 90°; потом в другую сторону
  for (int k = 0; k < 4; k++) {
    float a = ang[k] * (float)n->avoidSide * (k < 2 ? 1.f : 1.f); if (k >= 2) a = -ang[k - 2] * (float)n->avoidSide;
    float c = cosf(a), sn = sinf(a), vx = ux * c - uz * sn, vz = ux * sn + uz * c, px = n->pos.x + vx * s, pz = n->pos.z + vz * s;
    if (npcBlocksHomer(n, g, px, pz) || npcBlocksNpc(n, px, pz)) continue;
    const ColWorld *w = n->interior[0] && g->pHomerWorld ? g->pHomerWorld : g->pWorld; float gy;
    if (w && colCovered(w, px, pz) && !colGround(w, px, pz, n->pos.y + .6f, n->pos.y - .6f, &gy, 0)) continue;   // обрыв/пустота под ногами (вне плана кольца не проверяется, Коллизия-96)
    float want = atan2f(vx, vz), d = want - n->h; while (d > 3.14159265f) d -= 6.2831853f; while (d < -3.14159265f) d += 6.2831853f;
    n->h += d > 0.f ? fminf(d, .25f) : fmaxf(d, -.25f);   // плавно (≈15° за шаг), не рывком
    *nx = px; *nz = pz; n->blocked = 0; if (k >= 2) n->avoidSide = -n->avoidSide; return 1; }
  return 0;
}
// №813 (Роб 18.10, L2: «Милхаус обходит Барта и начинает бесконечно ходить по кругу; должен идти дальше и стоять на своём месте, по своей
// траектории, а не возле Барта кружить»): точка маршрута под игроком (или под другим персонажем) недостижима — обход №510 вёл персонажа
// по кругу вокруг занявшего без конца. Теперь: путь перекрыт, точка занята, до неё ближе NPC_ORBIT_REACH — точка пройдена (дальше по
// маршруту; последняя — стоит здесь, лицом к ней). Страховка на любой другой круг: обход без продвижения к точке NPC_ORBIT_T с — обход
// брошен: рядом с точкой — пройдена, далеко — стоит (клип покоя), пока путь не освободится. GAME_NPC_ORBIT_OLD=1 — прежнее
#ifndef GAME_NPC_ORBIT_OLD
#define GAME_NPC_ORBIT_OLD 0
#endif
#define NPC_ORBIT_REACH 2.f    // м: «рядом с точкой» (у занятой точки упирается не дальше 2 радиусов тела и шага ≈ 1,5 м)
#define NPC_ORBIT_PAD .25f     // м: запас к кругу занявшего — точка на краю его тела тоже занята
#define NPC_ORBIT_T 2.5f       // с без продвижения к точке при обходе (честный обход тела 0,3 м идёт с продвижением не позже ~1 с)
#define NPC_ORBIT_GAIN .2f     // м: продвижение, которое сбрасывает счёт
#ifndef NPC_PC_WAIT_OLD
#define NPC_PC_WAIT_OLD 0
#endif
#define NPC_PC_WAIT_T 2.f      // №831: с — стоит перед игроком у своей точки, потом точка пройдена
// сверка g07 (Дирижёр 02.11, «маршруты по кругу»): в оригинале маршрут ЛЮБОГО персонажа (и персонажа задания тоже) замкнут: дошёл до последней точки — идёт к первой,
// и так по кругу, пока не остановят (разговор, удар, игрок на пути). Одиночная точка — стоит. У нас персонаж задания шёл один проход (правка 831: «не кружит
// вокруг игрока, не смотрит на него» — это остаётся: игрок на пути, персонаж стоит лицом по пути). MSNLOOP1113_OLD=1 — персонаж задания один проход, как было
#ifndef MSNLOOP1113_OLD
#define MSNLOOP1113_OLD 0
#endif
#ifndef NPC_PATROL_OLD
#define NPC_PATROL_OLD 0
#endif
static int npcSpotTaken(const GameNpc *n, const Game *g, float x, float z) {
  if (*g->pHaveHomer && gameOnFoot(g)) { const Homer *H = g->pHomer; float r = GAME_NPC_RADIUS + HOMER_RADIUS + NPC_ORBIT_PAD;
    if (fabsf(n->pos.y - H->y) < HOMER_HEIGHT && (x - H->x) * (x - H->x) + (z - H->z) * (z - H->z) < r * r) return 1; }
  if (npcCtx) for (int i = 0; i < npcCtx->nnpc; i++) { const GameNpc *o = &npcCtx->npc[i]; if (o == n || o->knock == TRP_FLY || o->knock == TRP_DOWN) continue;
    float r = 2.f * GAME_NPC_RADIUS + NPC_ORBIT_PAD;
    if (fabsf(o->pos.y - n->pos.y) < HOMER_HEIGHT && (x - o->pos.x) * (x - o->pos.x) + (z - o->pos.z) * (z - o->pos.z) < r * r) return 1; }
  return 0;
}
// шаг к точке t, лицом по ходу; Гомер на пути — ждёт; 1 — дошёл (правка 240: общий для маршрута и возврата на место после удара);
// −1 — В85: собеседник остановился перед Гомером (stop > 0)
#define NPC_CAR_WAIT 3.f   // с стоит после касания машины (оригинал: 3,0 с до возврата к пути)
#define NPC_CAR_FREE 8.f   // с обходит по углам кузова после этого (число наше)
static int npcWalkTo(GameNpc *n, const Game *g, MsnVec3 t, float dt, float stop) {
  float dx = t.x - n->pos.x, dy = t.y - n->pos.y, dz = t.z - n->pos.z, d = sqrtf(dx * dx + dz * dz), s = GAME_NPC_WALK * dt, dT = d;
  if (d <= s || d < 1e-3f) { if (npcStopsAtHomer(n, g, t.x, t.z, stop)) return -1;
    if (!npcBlocksHomer(n, g, t.x, t.z)) {
#if !defined(GAME_NPC_WP_Y) || !GAME_NPC_WP_Y
      t.y = n->pos.y;   // правка 219: дошёл до точки — высота прежняя (пол), не высота локатора
#endif
      n->pos = t; return 1; }
    if (!GAME_NPC_ORBIT_OLD && !GAME_NPC_AVOID_OLD) { n->blocked = 0; return 1; }   // №813: у самой точки, а на ней игрок — пройдена, без шага в него
    return 0; }
  if (!GAME_NPC_ORBIT_OLD && !GAME_NPC_AVOID_OLD) {   // №813: продвижение к точке, пока идёт обход
    if (n->avoidT <= 0.f || t.x != n->avoidTx || t.z != n->avoidTz) { n->avoidTx = t.x; n->avoidTz = t.z; n->avoidBest = dT; n->avoidStall = 0.f; }
    else if (dT < n->avoidBest - NPC_ORBIT_GAIN) { n->avoidBest = dT; n->avoidStall = 0.f; }
    else if ((n->avoidStall += dt) > NPC_ORBIT_T) n->avoidGive = 1;
  }
  { float gx = t.x, gz = t.z; int around = 0;   // правка 217: машина на пути — к углу кузова
    if (!PEDCAR_ALL_OLD && roadsT && n->carFree > 0.f) {   // сверка g02-traffic: коснулся чужой машины (трафик, стоящая, миссии) и отстоял 3,0 с — обходит её по углам кузова
      n->carFree -= dt; if (trafficSolidCarAt(roadsT, n->pos.x, n->pos.z, n->pos.y, GAME_NPC_RADIUS + 1.5f, NULL)) n->carFree = NPC_CAR_FREE;   // пока рядом чужая машина — обход не кончается (иначе у самого борта встал бы ещё на 3 с)
      const Car *cs[6]; int nc = trafficCarsSolid(roadsT, n->pos.x, n->pos.z, 12.f, cs, 6);
      for (int k = 0; k < nc; k++) if (npcAroundCarOf(g, cs[k], n, &gx, &gz)) { around = 1; break; } }
    if (npcAroundCar(g, n, &gx, &gz)) around = 1;
    if (around) { dx = gx - n->pos.x; dz = gz - n->pos.z; d = sqrtf(dx * dx + dz * dz); if (d < 1e-3f) return 0; if (s > d) s = d; } }
  float nx = n->pos.x + dx / d * s, nz = n->pos.z + dz / d * s, h0_ = n->h;
  n->h = atan2f(dx, dz);
  if (!PEDCAR_ALL_OLD && roadsT && n->carFree <= 0.f) {   // сверка g02-traffic (слой персонаж ↔ машина): в оригинале любая машина толкает физика, касание = стоит 3,0 с и идёт дальше; здесь шаг, входящий в кузов чужой машины, не делается
    if (trafficSolidCarAt(roadsT, nx, nz, n->pos.y, GAME_NPC_RADIUS + .35f, NULL)) {   // запас 0,35 м: остановка снаружи полосы обхода кузова (npcAroundCarOf: радиус + 0,2), иначе от угла не выйти
      n->h = h0_; n->blocked = 1;
      if ((n->carWaitT += dt) >= NPC_CAR_WAIT) { n->carWaitT = 0.f; n->carFree = NPC_CAR_FREE; }
      return 0; }
    n->carWaitT = 0.f; }
  if (npcStopsAtHomer(n, g, nx, nz, stop)) return -1;
  // №831 (Роб 18.10, после TEST35: «Милхаус всё равно ходит вокруг меня по моей оси и смотрит ровно в центр — как зомби, жутко; поставь
  // обычное поведение NPC»): игрок на пути — персонаж НЕ обходит его (обход вокруг игрока смотрелся кружением со взглядом на игрока),
  // а спокойно стоит (клип покоя), лицом по своему пути, а не на игрока; игрок отошёл — идёт дальше. Простоял так NPC_PC_WAIT_T с у своей
  // точки (ближе NPC_ORBIT_REACH) — точка пройдена. Обход других персонажей (№510) — как был. NPC_PC_WAIT_OLD=1 — обход игрока, как TEST35
  if (!NPC_PC_WAIT_OLD && !GAME_NPC_AVOID_OLD && npcBlocksHomer(n, g, nx, nz)) {
    n->h = h0_; n->blocked = 1;
    if ((n->pcWaitT += dt) > NPC_PC_WAIT_T && dT < NPC_ORBIT_REACH) { n->pcWaitT = 0.f; n->blocked = 0; return 1; }
    return 0; }
  n->pcWaitT = 0.f;
  if (npcBlocksHomer(n, g, nx, nz) || npcBlocksNpc(n, nx, nz)) {
    if (GAME_NPC_AVOID_OLD) return 0;
    if (!GAME_NPC_ORBIT_OLD) {   // №813: занятая точка рядом (или обход без продвижения) — пройдена; обход брошен далеко от точки — стоит
      if (dT < NPC_ORBIT_REACH && (n->avoidGive || npcSpotTaken(n, g, t.x, t.z))) { n->blocked = 0; n->avoidGive = 0; n->avoidStall = 0.f; return 1; }
      if (n->avoidGive) { n->blocked = 1; return 0; }
    }
    if (!npcSidestep(n, g, dx / d, dz / d, s, &nx, &nz)) { n->blocked = 1; return 0; }   // обойти нельзя — стоит (клип покоя), без дёрганья на месте
  } else if (!GAME_NPC_AVOID_OLD) { n->blocked = 0; n->avoidGive = 0; if ((n->avoidT -= dt) < 0.f) { n->avoidT = 0.f; n->avoidSide = 0; } }
#if !defined(GAME_NPC_WP_Y) || !GAME_NPC_WP_Y
  // правка Роба 219 (03.10: «Лиза в школе ходит в потолке, миссию не сдать»): точки маршрута на диске стоят выше пола (m1 lisa_walk_1/2 —
  // y −16,5 при поле школы −20 и месте m1_lisa_place −20), оригинал ведёт персонажа по полу. Было: высота тянулась к точке маршрута
  // (+3,5 м за проход). Теперь высота — пол своего мира под ногами (комната — её коллизия, когда Гомер в ней; иначе город), в окне
  // ±1,5 м от прежней; пола нет — высота прежняя
  { const ColWorld *w = n->interior[0] && g->pHomerWorld ? g->pHomerWorld : g->pWorld; float gy;
    n->pos.x = nx; n->pos.z = nz;
#if !COLRING_OLD
    if (w && !colCovered(w, nx, nz)) { n->pos.y += dy * (s / d); n->farHold = 1; }   // Коллизия-96: вне плана кольца земля не спрашивается (каждый шаг был бы чтением карты), высота тянется к маршруту
    else {
      if (w && colGround(w, nx, nz, n->pos.y + 1.5f, n->pos.y - 1.5f, &gy, 0)) { n->pos.y = gy; n->farHold = 0; }
      else if (w && n->farHold && colGround(w, nx, nz, n->pos.y + 8.f, n->pos.y - 12.f, &gy, 0)) { n->pos.y = gy; n->farHold = 0; }   // вернулся в план после дальней прогулки: опору ищем широким окном, затем окно прежнее
    }
#else
    if (w && colGround(w, nx, nz, n->pos.y + 1.5f, n->pos.y - 1.5f, &gy, 0)) n->pos.y = gy;
#endif
  }
  (void)dy;
#else
  n->pos.x = nx; n->pos.z = nz; n->pos.y += dy * (s / d);   // отрицательный контроль: высота к точке маршрута
#endif
  return 0;
}
int gameTalkCarOld;   // правка 305: служебное — машина на место разговора из данных (прежнее)
// Правка (Механик/Опора-93, L7 m2 «Long Black Probes»): продавец зомби-машины и ведущий гонки sr3 — два разных персонажа с одним именем zmale1: продавец стоит на zmale1_loc (около кладбища) и
// ходит по zmale1_walk1, ведущий гонки — на sr3_ralph_sd (у школы). npcFind по имени отдаёт ведущего (бонус старше), и место разговора продавца переезжало на ведущего: △ у «лавки» запускал уличную
// гонку (бонус-персонаж в радиусе проверялся раньше продавца), машину купить нельзя, миссия по кругу. Теперь (1) место разговора продавца следует за ЕГО персонажем (не бонусным), нет такого — остаётся на локаторе;
// (2) пока этап — покупка машины/костюма, продавец в радиусе важнее бонус-персонажа. В оригинале это два отдельных персонажа, у каждого свой участок. SELLER96_OLD=1 / файл seller96old / flag seller96 0 — как было
#ifndef SELLER96_OLD
#define SELLER96_OLD 0
#endif
int gameSeller96 = !SELLER96_OLD;
int gameShopFirstOld;   // правка 306: служебное — лавка перехватывает △ у собеседника этапа (прежнее)
int gameBoss967Old = BOSS967_OLD;
int gameSellerAtLoc;   // правка 288: служебное — место разговора продавца на локаторе, как было
// сверка g07 №1098 (Роб 02.11: «Фринк на уровне 2 упирается в машину и не может её обойти; в оригинале обходит»). Оригинал (сверено, круг 7): персонаж миссии
// идёт по прямым отрезкам между точками, обхода машин по точке нет; коснулась машина — стоит 3,0 с, потом идёт дальше (3,0 с «стоит» совпадает с нашим
// NPC_STALL_T); отсутствие обхода физика кузова смягчает скольжением вдоль борта, а сорванного с пути (дальше 1 м) возвращают на отрезок: сразу, если его не
// видно ни одной камере, иначе он сам идёт к пути. У нас обход угла кузова (правка 217) только для машины игрока и только когда точка вне кузова; точка ПОД
// кузовом или Фринк вплотную к кузову оставляли его на месте навсегда. ПРАВИЛО ПОРТА (в оригинале однозначного «перескока» нет): простоял у машины NPC_STALL_T с
// без продвижения к точке — точка пройдена (идёт к следующей; последняя — маршрут окончен). MSNWALK1098_OLD=1 — как было
#ifndef MSNWALK1098_OLD
#define MSNWALK1098_OLD 0
#endif
// сверка g07 №1113 (Роб 02.11: «Фринк и пешие у машины как в оригинале»): пропуск точки через 3 с — выдумка порта, в оригинале его нет: после касания стоит 3,0 с и
// идёт дальше по своему пути, машину огибает физика (слой персонаж-машина g02: касание = стоит 3,0 с, затем обход по углам кузова). Теперь правило 1098 выключено;
// MSNWALK1113_OLD=1 — вернуть пропуск точки через 3 с (поведение №1098)
#ifndef MSNWALK1113_OLD
#define MSNWALK1113_OLD 0
#endif
#define NPC_STALL_T 3.0f
#define NPC_REACH_R 0.5f   // м: «дошёл до точки маршрута» (число оригинала)
static int npcLoops(const GameNpc *n) { return !NPC_PATROL_OLD && n->nwp >= 2 && (n->kind != MSN_NPC_MISSION || !MSNLOOP1113_OLD); }   // маршрут замкнут в круг
static int npcNearCar(const GameNpc *n, const Game *g, float r) {   // персонаж в r м от кузова машины игрока или машины миссии
  const Car *cs[1 + MC_MAX]; int nc = 0;
  if (*g->pHaveCol && g->pCar) cs[nc++] = g->pCar;
  if (npcCtx) for (int i = 0; i < npcCtx->cars.n; i++) if (npcCtx->cars.c[i].on) cs[nc++] = &npcCtx->cars.c[i].car;
  for (int k = 0; k < nc; k++) { const Car *c = cs[k]; if (fabsf(c->y - n->pos.y) > 2.5f) continue;
    float fx = sinf(c->h), fz = cosf(c->h), rx = fz, rz = -fx, cx = c->x + fx * c->p.bodyOfs, cz = c->z + fz * c->p.bodyOfs;
    float ax = (n->pos.x - cx) * rx + (n->pos.z - cz) * rz, az = (n->pos.x - cx) * fx + (n->pos.z - cz) * fz;
    if (fabsf(ax) < c->p.halfW + r && fabsf(az) < c->p.halfL + r) return 1; }
  return 0;
}
static void npcStep(GameNpc *n, const Game *g, float dt, float stop) {   // шаг к следующей точке маршрута (stop — В85, только маршрут)
  if (n->knock) return;   // правка 240: сбит — летит, лежит, встаёт (npcKnock)
  if (n->homing) {        // правка 240: встал — назад, где стоял (новая точка маршрута от сценария — важнее)
    if (n->walking) n->homing = 0;
    else {
      if (n->homing == 2) { if ((n->homeT += dt) > 3.f) { n->homing = 1; n->homeP = n->pos; n->homeT = 0.f; } return; }   // ждёт
      if (npcWalkTo(n, g, n->home, dt, 0.f) > 0) n->homing = 0;
      else if (hypotf(n->pos.x - n->homeP.x, n->pos.z - n->homeP.z) > .1f) { n->homeP = n->pos; n->homeT = 0.f; }
      else if ((n->homeT += dt) > 1.f) { n->homing = 2; n->homeT = 0.f; }   // 1 с ни с места: машина на месте, Гомер на пути
      return;
    }
  }
  if (!n->walking) return;
  if (n->iwp >= n->nwp) { n->walking = 0; return; }
  int r;
  // №1113: оригинал считает точку маршрута пройденной, когда персонаж ближе 0,5 м к ней (по земле); у нас было «в пределах шага» (≈1,5 см), и точка у самого борта
  // или под кузовом не бралась никогда. Промежуточные точки — по 0,5 м всегда; последняя (маршрут окончен, место разговора) — по 0,5 м только у машины, иначе точно
  { const MsnVec3 w = n->wp[n->iwp]; int last = n->iwp + 1 >= n->nwp && !npcLoops(n);
    if (!MSNWALK1113_OLD && hypotf(w.x - n->pos.x, w.z - n->pos.z) < NPC_REACH_R && (!last || npcNearCar(n, g, GAME_NPC_RADIUS + .6f))) r = 1;
    else r = npcWalkTo(n, g, n->wp[n->iwp], dt, stop); }
  if (!MSNWALK1098_OLD && MSNWALK1113_OLD && r == 0) {   // №1113: только при MSNWALK1113_OLD=1; у машины без продвижения к точке NPC_STALL_T с — точка пройдена (правило порта, см. выше)
    float dT = hypotf(n->wp[n->iwp].x - n->pos.x, n->wp[n->iwp].z - n->pos.z);
    if (!npcNearCar(n, g, GAME_NPC_RADIUS + .6f)) { n->carStallT = 0.f; n->carStallD = dT; }
    else if (dT < n->carStallD - .2f || n->carStallD <= 0.f) { n->carStallD = dT; n->carStallT = 0.f; }
    else if ((n->carStallT += dt) >= NPC_STALL_T) { n->carStallT = 0.f; n->carStallD = 0.f; r = 1; }
  } else if (r > 0) { n->carStallT = 0.f; n->carStallD = 0.f; }
  // №831 (Роб 18.10: «сделаем как в оригинале — чтобы он просто ходил по своему месту, как ему назначено»): фоновые персонажи и продавцы
  // (AddAmbientNPCWaypoint, AddBonusMissionNPCWaypoint, AddPurchaseCarNPCWaypoint — не персонажи задания) ходят по своим точкам по кругу,
  // постоянно; персонаж задания (AddObjectiveNPCWaypoint) — один проход, как было. NPC_PATROL_OLD=1 — один проход у всех, как TEST35
  if (r > 0) { n->iwp++; if (n->iwp >= n->nwp) { if (npcLoops(n)) n->iwp = 0; else n->walking = 0; } }
  // В85: остановился перед Гомером — маршрут окончен, ждёт разговора на месте. ПРАВИЛО ПОРТА: PS2 записан только в покое (10 с без
  // ввода, Гомер рядом — Мардж на месте) и после карточки (140 кадров без движения); пойдёт ли она дальше, если Гомер отойдёт, — не
  // записано (в атласе m0d у Мардж нет колонок, снимков между кадрами 10 и 103 нет)
  else if (r < 0) { n->walking = 0; n->iwp = n->nwp; }
}
// Правка 240 (Роб 03.10 14:51: «не сбиваются люди вообще никакие»): персонаж миссии (фоновый, продавец, бонусный, персонаж
// миссии вне разговора) сбивается машиной игрока и сбитыми машинами трафика — ТЕМ ЖЕ механизмом, что прохожий трафика
// (traffic.c: trafficCarHitsBody — удар и отлёт, trafficBodyFly — полёт, TR_PED_DOWN — лежит, trafficGetUpLen — встаёт),
// событие TR_EV_PED_HIT — в очередь трафика (main.c / боты: Hit & Run +10, удар по телу, голос по имени). Оригинал: у всех
// персонажей уровня на диске есть реплики «сбит машиной» (<имя>/p_hitbyc_*.rsd, dialog.spt P_HitByC_<код>) и клипы рига
// flail / get_up (art/chars/npd.cho; свои — marge/lisa/apu/bart.cho). Встал: шёл по маршруту — идёт дальше, стоял — идёт туда,
// где стоял (правило порта: как у оригинала — не сверено, вопрос близнецу); улетел за край мира — на месте (убрать, как
// прохожего, нельзя). Собеседник разговора не сбивается (игрок стоит, машина на месте)
static void npcKnock(GameMsn *m, float dt) {
  Game *g = m->g; if (!*g->pHaveCol || !g->pCar) return;
  for (int i = 0; i < m->nnpc; i++) {
    GameNpc *n = &m->npc[i];
    if (n->knock == TRP_FLY) {
      const ColWorld *w = n->interior[0] && g->pHomerWorld ? g->pHomerWorld : g->pWorld;
      float p[3] = {n->pos.x, n->pos.y, n->pos.z}; int r = trafficBodyFly(w, p, n->kv, &n->kt, dt);
      n->pos.x = p[0]; n->pos.y = p[1]; n->pos.z = p[2];
      if (r > 0) { n->knock = TRP_DOWN; n->kt = 0.f; }
      else if (r < 0) { n->pos = n->home; n->knock = 0; n->homing = 0; }
      continue;
    }
    if (n->knock == TRP_DOWN) { if ((n->kt += dt) > TR_PED_DOWN) { n->knock = TRP_GETUP; n->kt = 0.f; } }
    else if (n->knock == TRP_GETUP && (n->kt += dt) >= trafficGetUpLen(roadsT)) {   // встал: по маршруту дальше или на место
      n->knock = 0; n->homing = !n->walking; n->homeP = n->pos; n->homeT = 0.f; }
    // стоящий, идущий, лежащий и встающий — машина сбивает или толкает (как прохожих: traffic.c pedHitBy)
    if (!GAME_NPC_KNOCK || (m->talk.on && msn_streq(n->name, m->talk.npc))) continue;
    int down = n->knock == TRP_DOWN;   // лежащего — подбросить снова, без нового нарушения
    const Car *cars[1 + TR_MAXCAR]; int nc = 0;   // как у прохожих (trafficUpdate): машина игрока и сбитые машины трафика
    cars[nc++] = g->pCar;
    if (roadsT && roadsT->hd) for (int j = 0; j < roadsT->maxCars; j++)
      if (roadsT->car[j].mode == TRM_PHYS || roadsT->car[j].mode == TRM_WRECK) cars[nc++] = &roadsT->car[j].car;
    for (int k = 0; k < nc; k++) {
      MsnVec3 was = n->pos; float v[3]; int r = trafficCarHitsBody(cars[k], &n->pos.x, n->pos.y, &n->pos.z, TR_PED_R, v);
      if (r == 1) trafficCarHitsBody(cars[k], &n->pos.x, n->pos.y, &n->pos.z, GAME_NPC_RADIUS + .25f, v);   // медленно: вытолкнут —
      if (r != 2) continue;   // за полосу обхода кузова (npcAroundCar: радиус + 0,2), иначе к углу машины ему не выйти
      if (!n->homing && !n->knock) n->home = was;   // где стоял (шёл на место / лежал / вставал — место прежнее)
      n->homing = 0; n->knock = TRP_FLY; n->kt = 0.f; n->kv[0] = v[0]; n->kv[1] = v[1]; n->kv[2] = v[2];
      if (down) break;
      if (roadsT && roadsT->hd) trafficPushEvent(roadsT, TR_EV_PED_HIT, -1 - i, n->pos.x, n->pos.y, n->pos.z, TR_HIT_PED, n->name);
      else gameHnrOffense(g, HNR_PED_HIT);   // без трафика (нет traffic.shr) — только шкала
      break;
    }
  }
}
// №843 (Роб 19.10, TEST38: «не могу бить Милхауса — персонажей миссий; бить, как прохожих»): пинок Гомера (HOMER_EV_HIT) сбивает персонажа
// миссии тем же механизмом, что прохожего трафика (traffic.c trafficKick: отлёт 5 м/с по направлению удара и 3 м/с вверх, событие TR_EV_PED_KICK —
// шкала Hit & Run и голос по имени); дальше — полёт, лежит, встаёт (npcKnock, правка 240). Собеседник разговора не сбивается. GAME_NPC_KICK843_OLD=1 — как было
#ifndef GAME_NPC_KICK843_OLD
#define GAME_NPC_KICK843_OLD 0
#endif
int gameMsnKick(GameMsn *m, float x, float y, float z, float r, float dx, float dz) {
  if (GAME_NPC_KICK843_OLD || !GAME_NPC_KNOCK || !m) return 0;
  int k = 0;
  for (int i = 0; i < m->nnpc; i++) { GameNpc *n = &m->npc[i];
    if (n->knock == TRP_FLY || (m->talk.on && msn_streq(n->name, m->talk.npc))) continue;
    float ex = n->pos.x - x, ey = n->pos.y + .9f - y, ez = n->pos.z - z, R = r + TR_PED_R + .3f;
    if (ex * ex + ey * ey + ez * ez > R * R) continue;
    if (!n->homing && !n->knock) n->home = n->pos;   // где стоял — туда вернётся, встав
    n->homing = 0; n->knock = TRP_FLY; n->kt = 0.f; n->kv[0] = dx * 5.f; n->kv[1] = 3.f; n->kv[2] = dz * 5.f;
    if (roadsT && roadsT->hd) trafficPushEvent(roadsT, TR_EV_PED_KICK, -1 - i, n->pos.x, n->pos.y, n->pos.z, .13f, n->name);   // .13 — HIT_KICK traffic.c
    k++;
  }
  return k;
}
#ifndef GAME_MSN_FREEZE
#define GAME_MSN_FREEZE 1   // отрицательный контроль гейта tests/missions/countdown.sh: 0 — газ проходит во время отсчёта
#endif
int gameMsnFrozen(const GameMsn *m) { return GAME_MSN_FREEZE && (m->rt.status == MSN_COUNTDOWN || m->talk.on || m->shop.open); }   // отсчёт, разговор, лавка
// P06 Д6: на отсчёте «3 2 1» газ и тормоз машины игрока выключены, ручник включён, кнопка сброса машины заблокирована; руль и камера остаются живыми
static int count57(const GameMsn *m) { return !GAME_COUNT57_OLD && m->rt.status == MSN_COUNTDOWN && !m->talk.on && !m->shop.open && *m->g->pHaveHomer && !gameOnFoot(m->g); }
void gameMsnFreezePad(const GameMsn *m, GamePad *p) {
  if (!gameMsnFrozen(m)) return;
  if (count57(m)) { p->buttons &= GP_START | GP_L | GP_R; p->hit &= GP_START | GP_L | GP_R; p->ay = 0.f; return; }   // руль (ax) и камера (L/R) живы
  p->buttons &= GP_START; p->hit &= GP_START; p->ax = p->ay = 0.f;
}
void gameMsnFreezeCar(const GameMsn *m, CarInput *in) {
  if (!gameMsnFrozen(m)) return;
  in->throttle = in->brake = 0.f; if (!count57(m)) in->steer = 0.f; in->handbrake = 1.f;   // тормоз стоя = задний ход (game.c), поэтому ручник
}
int gameMsnSolid(void *u, float *x, float *z, float y, float r, float *top, float *onx, float *onz) {
  GameMsn *m = u; int pushed = 0;
  if (m->prevSolid) pushed = m->prevSolid(m->prevSolidUser, x, z, y, r, top, onx, onz);
  pushed |= mcSolid(&m->cars, x, z, y, r, top, onx, onz);   // машины миссии: кузов твёрдый, крыша — опора
  if (!GAME_NPC_SOLID) return pushed;
  for (int i = 0; i < m->nnpc; i++) {   // капсула персонажа: вытолкнуть Гомера по xz, скорость вдоль нормали гасит homer.c
    const GameNpc *n = &m->npc[i];
    if (n->knock == TRP_FLY || n->knock == TRP_DOWN) continue;   // правка 240: летит / лежит — не твёрдый (как прохожий трафика)
    if (fabsf(n->pos.y - y) >= HOMER_HEIGHT) continue;   // другой этаж / комната под городом
    float dx = *x - n->pos.x, dz = *z - n->pos.z, rad = r + GAME_NPC_RADIUS, d2 = dx * dx + dz * dz;
    if (d2 >= rad * rad) continue;
    float d = sqrtf(d2), nx, nz; if (d < 1e-4f) { nx = sinf(n->h); nz = cosf(n->h); } else { nx = dx / d; nz = dz / d; }
    *x = n->pos.x + nx * rad; *z = n->pos.z + nz * rad; *onx = nx; *onz = nz; pushed = 1;
  }
  return pushed;
}
static int entity(void *u, const char *name, MsnEntity *out);
#if !defined(GAME_BONUS_NPC) || GAME_BONUS_NPC
static int bonusEntity(void *u, int mission, const char *name, MsnEntity *out) {   // правка 180: бонусный — тот, что на месте своей миссии
  (void)name;
  for (int i = 0; i < GM->nnpc; i++) {
    const GameNpc *n = &GM->npc[i]; if (n->bonus != mission + 1) continue;
    out->pos = n->pos; out->health = 1.f; out->finished = !n->walking; out->waypoint = n->iwp - 1; return 1;
  }
  return 0;
}
#endif
static int entity(void *u, const char *name, MsnEntity *out) {   // живые машины миссии и персонажи
  const McCar *c = mcFind(&GM->cars, name);
  if (c) { out->pos.x = c->car.x; out->pos.y = c->car.y; out->pos.z = c->car.z; out->health = 1.f - c->car.damage;
           out->finished = c->finished; out->waypoint = c->passed; return 1; }
  GameNpc *n = npcFind(GM, name);
  if (n) { out->pos = n->pos; out->health = 1.f; out->finished = !n->walking; out->waypoint = n->iwp - 1; return 1; }
  return GM->view.entity ? GM->view.entity(u, name, out) : 0;
}
static void cleanup(void *u) {   // миссия закончилась / повтор: её персонажи уходят, фоновые остаются
  npcGen++;
  for (int i = 0; i < GM->nnpc;) if (GM->npc[i].kind == MSN_NPC_MISSION) GM->npc[i] = GM->npc[--GM->nnpc]; else i++;
#if !defined(GAME_PLACE_NPC) || GAME_PLACE_NPC
  // правка 140: продавец, которого миссия брала себе (m2i: AddNPC("barney", "m2_barney")), возвращается к лавке (правило порта)
  for (int k = 0; k < GM->shop.nseller; k++) {
    const ShopSeller *L = &GM->shop.seller[k];
    if (!L->npc[0] || npcFind(GM, L->npc)) continue;
    MsnPlace at; memset(&at, 0, sizeof at); at.pos.x = L->x; at.pos.y = L->y; at.pos.z = L->z;
    npcSpawn(u, L->npc, &at, MSN_NPC_PLACE, L->radius, "");
  }
#endif
#if !defined(GAME_BONUS_NPC) || GAME_BONUS_NPC
  // правка 180: персонаж бонус-миссии, которого его миссия брала себе (вступление sr3: AddNPC("ralph", "sr3_ralph_sd")), — снова
  // на своём месте (правило порта, как у продавцов: оригинал после гонки — не сверен)
  for (int k = 0; k < GM->nbgiver; k++) {
    int have = 0; for (int i = 0; i < GM->nnpc; i++) if (GM->npc[i].bonus == GM->bgiver[k].mission + 1) have = 1;
    if (!have) npcSpawnB(u, GM->bgiver[k].name, &GM->bgiver[k].at, MSN_NPC_PLACE, 0.f, "", GM->bgiver[k].mission + 1);
  }
#endif
  GM->markerOn = 0;
  mcClear(&GM->cars);   // машины миссии уходят вместе с ней (повтор ставит их заново сценарием)
  worldCarsSpawn(GM);   // правка 53: машины мира (ракетная) остаются на местах
  hnrOff = 0;   // перепроверка 96: миссия кончилась — Hit & Run снова работает
  trafficMax(u, -1);    // трафик — как на уровне
  trafficPedGroup(roadsT, -1);
  if (GM->forced) {
    int foot = gameOnFoot(G_);
    if (!GAME_GIVENCAR52_OLD && !GAME_C3_GIVEN_OLD && GM->missionDone && !foot && GM->forcedStartOn && *G_->pHaveHomer) {
      // круг 3, строка 41: миссия завершена, игрок в выданной машине и подмены на свою не было — он выходит на землю; если после этого ближе 2 м к точке выданной машины
      // и ближе 20 м к месту, где стоял при выдаче, — его ставят на то место
      stepOutOfCar(G_); foot = 1;
      float dx = G_->pHomer->x - GM->forcedPt[0], dz = G_->pHomer->z - GM->forcedPt[2], ex = G_->pHomer->x - GM->forcedStart[0], ez = G_->pHomer->z - GM->forcedStart[2];
      if (dx * dx + dz * dz < 4.f && ex * ex + ez * ez < 400.f) putHomer(G_, &(MsnPlace){ .pos = { GM->forcedStart[0], GM->forcedStart[1], GM->forcedStart[2] }, .has_heading = 0, .locator = "", .loc = 0 });
    }
    GM->forcedStartOn = 0; GM->missionDone = 0;
    forcedOff(GM, 0, 0); if (!foot) inCar(G_);   // машина уровня — игроку (на месте)
  }
  if (GM->view.cleanup) GM->view.cleanup(u);
}
// ------------------------------------------------------------------ разговор: места, лицом друг к другу, жесты (правки 26, 38)
static const char *const pcCodes[][2] = {{"homer", "hom"}, {"bart", "brt"}, {"lisa", "lis"}, {"marge", "mrg"}, {"apu", "apu"}};
static const char *pcCode(const char *name) {   // код говорящего в именах реплик (c_*_hom_l1.rsd) — у персонажей игрока
  for (unsigned i = 0; i < sizeof pcCodes / sizeof *pcCodes; i++) if (msn_streq(pcCodes[i][0], name)) return pcCodes[i][1];
  return "";
}
static MsnPlace placeOf(const MsnFile *f, const MsnCmd *c, int i) {
  MsnPlace p; memset(&p, 0, sizeof p); const MsnLoc *l = msn_arg_loc(f, c, i);
  p.locator = msn_arg_s(f, c, i); p.loc = l;
  if (l) { p.pos.x = l->pos[0]; p.pos.y = l->pos[1]; p.pos.z = l->pos[2]; p.has_heading = (l->flags & 1) != 0; p.heading = p.has_heading ? l->heading : 0.f; }
  return p;
}
static void talkFeature(GameMsn *m, const MsnCall *c) {   // команды разговора этапа (данные диска) — до начала разговора
  const MsnFile *f = c->f; const MsnCmd *k = c->cmd; GameTalk *t = &m->talk;
  if (m->animStale && (k->op == MSN_OP_ADDAMBIENTPCANIMATION || k->op == MSN_OP_ADDAMBIENTNPCANIMATION || k->op == MSN_OP_CLEARAMBIENTANIMATIONS)) { t->nAnim[0] = t->nAnim[1] = 0; m->animStale = 0; }   // этап задаёт свои жесты
  switch (k->op) {
    case MSN_OP_SETDIALOGUEPOSITIONS:
      t->at[0] = placeOf(f, k, 0); t->at[1] = placeOf(f, k, 1); t->hasPos = t->at[0].loc && t->at[1].loc;
      t->car = placeOf(f, k, 2); t->hasCar = t->car.loc != 0;
      t->noReturn = msn_argc(k) > 3 && msn_arg_i(f, k, 3) == 1;   // P06-16: 14 разговоров диска (4-й аргумент 1) — после реплик на прежние места не возвращать
      break;
    case MSN_OP_ADDAMBIENTPCANIMATION: case MSN_OP_ADDAMBIENTNPCANIMATION: {
      int w = k->op == MSN_OP_ADDAMBIENTNPCANIMATION;
      if (t->nAnim[w] < GAME_TALK_ANIMS) t->anim[w][t->nAnim[w]++] = msn_arg_s(f, k, 0);
      break; }
    case MSN_OP_AMBIENTANIMATIONRANDOMIZE: { int who = msn_arg_i(f, k, 0); if (who == 0 || who == 1) t->rnd[who] = msn_arg_i(f, k, 1) != 0; break; }
    case MSN_OP_CLEARAMBIENTANIMATIONS: t->nAnim[0] = t->nAnim[1] = 0; break;
    default: break;
  }
}
static void camFeature(GameMsn *m, const MsnCall *c) {   // правка 40: камеры фазы и разговора (данные диска)
  const MsnFile *f = c->f; const MsnCmd *k = c->cmd; GameTalk *t = &m->talk;
  switch (k->op) {
    case MSN_OP_SETMISSIONSTARTCAMERANAME:   // камера старта фазы (mNsdi); SetAnimatedCameraName — если своей нет (гонки)
      if (c->when == MSN_AT_SETUP) { m->scam.cam = msn_arg_s(f, k, 0); m->scam.req = 0; } break;
    case MSN_OP_SETANIMATEDCAMERANAME:
      if (c->when == MSN_AT_SETUP && !m->scam.cam) m->scam.cam = msn_arg_s(f, k, 0); break;
    case MSN_OP_SETCONVERSATIONCAM: {        // план на реплику: SetConversationCam(номер, "npc_far" ...)
      int line = msn_arg_i(f, k, 0);
      if (c->when == MSN_AT_STAGE && line >= 0 && line < GAME_TALK_CAMS) t->cam[line] = msn_arg_s(f, k, 1);
      break; }
    case MSN_OP_SETCAMBESTSIDE: {            // лучшая сторона камеры разговора: локатор
      const MsnLoc *l = c->when == MSN_AT_STAGE ? msn_arg_loc(f, k, 0) : 0;
      if (l) { t->hasBest = 1; t->best.x = l->pos[0]; t->best.y = l->pos[1]; t->best.z = l->pos[2]; }
      break; }
    default: break;
  }
}
int gameMsnHnrOff(void) { return hnrOff; }
// правка 122: высота земли под точкой (заборы трассы гонки ставятся от земли) -> 1 — есть
int gameMsnGroundY(GameMsn *m, float x, float z, float *y) { Game *g = m->g; return *g->pHaveCol && colGround(g->pWorld, x, z, 200.f, -200.f, y, 0); }
// правка 122: стрелка навигации к цели tgt по дорогам трафика (traffic.c trafficRouteArrow): только за рулём; перекрёсток ближе 70 м
int gameMsnNavBuild(GameMsn *m, float tx, float tz) {   // сверка g07b-msnview круг 2
  Game *g = m->g; if (!roadsT || gameOnFoot(g)) return 0;
  return trafficNavBuild(roadsT, g->pCar->x, g->pCar->z, g->pCar->h, tx, tz);
}
// gameMsnNavArrows — одна, выше у navMarker (сведение дирижёра: g07-missions и g07b-msnview сделали её дважды)
int gameMsnRouteArrow(GameMsn *m, const float tgt[3], float p[3], float *h) {
  Game *g = m->g; if (!roadsT || !tgt || gameOnFoot(g)) return 0;
  return trafficRouteArrow(roadsT, g->pCar->x, g->pCar->z, g->pCar->h, tgt[0], tgt[2], 70.f, p, h);
}
#ifndef GAME_SAFE_ON
#define GAME_SAFE_ON 1   // отрицательный контроль гейта tests/levelmech/safezone.sh: 0 — зоны не действуют
#endif
static void safeAdd(GameMsn *m, const MsnCall *c) {   // AddSafeZone(локатор, радиус) — см. game_msn.h
  const MsnLoc *l = msn_arg_loc(c->f, c->cmd, 0);
  if (!GAME_SAFE_ON || !l || m->nsafe >= GAME_SAFE_MAX) return;
  GameSafeZone *z = &m->safe[m->nsafe++];
  z->c.x = l->pos[0]; z->c.y = l->pos[1]; z->c.z = l->pos[2]; z->r = (float)msn_arg_i(c->f, c->cmd, 1);
}
int gameMsnInSafeZone(const GameMsn *m, float x, float z) {
  for (int i = 0; i < m->nsafe; i++) { float dx = x - m->safe[i].c.x, dz = z - m->safe[i].c.z; if (dx * dx + dz * dz < m->safe[i].r * m->safe[i].r) return 1; }
  return 0;
}
static void feat(void *u, int feature, const MsnCall *c) {   // команды без своего модуля: разговор — здесь, остальное — виду
  if (!gameBoss967Old && c && c->cmd && c->cmd->op == MSN_OP_ADDFLYINGACTORBYLOCATOR) {   // №967: тарелка L7 (имя и модель — аргументы 0 и 1, место — локатор)
    const MsnLoc *l = msn_arg_loc(c->f, c->cmd, 2);
    if (l) { GM->ufoPos[0] = l->pos[0]; GM->ufoPos[1] = l->pos[1]; GM->ufoPos[2] = l->pos[2]; GM->ufoOn = 1; GM->ufoCharge = 0.f; GM->ufoSent = 0; } }
  if (c && c->cmd && c->cmd->op == MSN_OP_DISABLEHITANDRUN) hnrOff = 1;
  if (!GAME_HIDECHAR_OLD && c && c->cmd && c->cmd->op == MSN_OP_SHOWHUD) GM->hudHidden = msn_streq(msn_arg_s(c->f, c->cmd, 0), "false");
  if (!GAME_HIDECHAR_OLD && c && c->cmd && c->cmd->op == MSN_OP_SETCHARACTERTOHIDE && c->when == MSN_AT_STAGE) snprintf(GM->hideName, sizeof GM->hideName, "%s", msn_arg_s(c->f, c->cmd, 0));
  if (c && c->cmd && c->cmd->op == MSN_OP_ADDSAFEZONE) safeAdd(GM, c);
  if (c && c->cmd && c->cmd->op == MSN_OP_USEPEDGROUP) trafficPedGroup(roadsT, msn_arg_i(c->f,c->cmd,0));
  if (feature == MSN_FEAT_DIALOGUE && c && c->cmd) talkFeature(GM, c);
  if (feature == MSN_FEAT_CAMERA && c && c->cmd) camFeature(GM, c);
  if (GM->view.feature) GM->view.feature(u, feature, c);
}
static void talkBegin(GameMsn *m, const char *a, const char *b) {   // MSN_EV_DIALOGUE: SetDialogueInfo(a, b, ...)
  GameTalk *t = &m->talk; Game *g = m->g;
  m->preTalk.on = 0;   // В85: места до разговора — только этого разговора
  if (t->off || !a || !b || !*g->pHaveHomer) return;
  int startedInCar = 0;
  if (!gameOnFoot(g)) {
    if (GAME_TALKCAR_OLD) return;
    // P15-13: разговор при игроке в машине — игрок сразу ставится на землю у машины (как выход без анимации), дальше обычный разговор
    startedInCar = 1; stepOutOfCar(g);
    if (!gameOnFoot(g)) return;
  }
  if (0) {
    Car *c = g->pCar; ColWorld *cw = g->pWorld; int ok = 0;
    for (int side = 1; side >= -1 && !ok; side -= 2) {
      float ox = c->x + (float)side * cosf(c->h) * (c->p.halfW + .9f), oz = c->z - (float)side * sinf(c->h) * (c->p.halfW + .9f), gy;
      if (*g->pHaveCol && colGround(cw, ox, oz, c->y + 1.5f, c->y - 1.f, &gy, 0) && fabsf(gy - c->y) < .8f) { homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : cw, ox, gy, oz, c->h); ok = 1; }
    }
    if (!ok) homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : cw, c->x, c->y, c->z, c->h);
    if (!gameOnFoot(g)) return;
  }
  int aNpc = npcFind(m, a) != 0, bNpc = npcFind(m, b) != 0;
  t->pcFirst = !aNpc && (*pcCode(a) || bNpc);            // игрок — тот из двоих, кого нет среди персонажей миссии
  const char *nn = t->pcFirst ? b : a;
  GameNpc *n = npcFind(m, nn);
  strncpy(t->npc, n ? nn : "", sizeof t->npc - 1); t->npc[sizeof t->npc - 1] = 0;
  t->npc2[0] = 0;
  if (!GAME_TALKPAIR_OLD && n) {   // P15-14: Пэтти и Сельма (zmale1/2/3 — Сельма на L7 стоит как zmale1): вторая из пары тоже разворачивается к игроку
    static const char *const sel[] = {"zmale1", "zmale2", "zmale3"};
    if (!strcasecmp(nn, "patty")) { for (int i = 0; i < 3; i++) if (npcFind(m, sel[i])) { snprintf(t->npc2, sizeof t->npc2, "%s", sel[i]); break; } }
    else for (int i = 0; i < 3; i++) if (!strcasecmp(nn, sel[i]) && npcFind(m, "patty")) { snprintf(t->npc2, sizeof t->npc2, "patty"); break; }
  }
  Homer *H = g->pHomer; const ColWorld *hw = g->pHomerWorld ? g->pHomerWorld : g->pWorld;
  MsnVec3 P = {H->x, H->y, H->z}, N;
  if (t->hasPos) { P = t->at[t->pcFirst ? 0 : 1].pos; N = t->at[t->pcFirst ? 1 : 0].pos; }   // места из данных
  else if (n) N = n->pos;   // мест нет — оба стоят, где стояли, лицом друг к другу: как в оригинале (слово Роба 30.09 «пусть будет
                            // всё, как в оригинале»; запись PS2 m4 18:26–18:40 — Гомер у пульта, где нажал действие). Прежнее правило
                            // порта «игрок на линии в 2.45 м» ставило Гомера в пустоту за стеной пультовой АЭС (m4, Карл за стеной)
  else return;                                           // собеседника нет в мире — ставить некого
  if (!GAME_TALKGAP_OLD) {   // 4.4: между героями не ближе 1,65 м; ближе — оба расходятся симметрично вдоль линии между ними (по xz)
    float gx = N.x - P.x, gz = N.z - P.z, gd = hypotf(gx, gz);
    if (gd < 1.65f) {
      if (gd < 1e-3f) { gx = sinf(H->h); gz = cosf(H->h); gd = 1.f; }
      float push = (1.65f - hypotf(N.x - P.x, N.z - P.z)) * .5f, ux = gx / gd, uz = gz / gd;
      P.x -= ux * push; P.z -= uz * push; N.x += ux * push; N.z += uz * push;
    }
  }
  { Homer *H0 = g->pHomer;   // В85: где стояли до разговора — туда вернёт Cancel (gameMsnTalkCancel)
    m->preTalk.on = n != 0; m->preTalk.hCar = startedInCar; snprintf(m->preTalk.npc, sizeof m->preTalk.npc, "%s", n ? nn : "");
    m->preTalk.hp.x = H0->x; m->preTalk.hp.y = H0->y; m->preTalk.hp.z = H0->z; m->preTalk.hh = H0->h;
    if (n) { m->preTalk.np = n->pos; m->preTalk.nh = n->h; } }
  if (n) { n->pos = N; n->walking = 0; n->iwp = n->nwp; n->h = atan2f(P.x - N.x, P.z - N.z); t->baseN = N; t->shifted = 0; }   // маршрут окончен, лицом к игроку
  homerPlace(H, hw, P.x, P.y, P.z, atan2f(N.x - P.x, N.z - P.z));                                  // игрок лицом к собеседнику
  // машина — на своё место разговора (если не там), если это место у разговора: третий аргумент SetDialoguePositions на диске
  // либо рядом (7–40 м: m0/m1/m2/m3/m6 sd, финиши гонок), либо за 290–800 м (level1_carstart у бонус-миссий на другом конце
  // карты, m7sd — место из m6). ПРАВИЛО ПОРТА: дальше GAME_TALK_CAR_NEAR машина остаётся, где была (иначе m7 начинает гонку
  // у электростанции, а машина — у магазина за 800 м)
  // Правка Роба 305 (04.10: «во время разговоров машину переносит куда-то в другое место — должна стоять там, где стояла»): своя
  // машина на разговоре не переставляется (слово Роба; оригинал — вопрос В29). «flag talkcarold 1» — прежнее (место из данных)
  float cdx = t->car.pos.x - P.x, cdz = t->car.pos.z - P.z;
  if (gameTalkCarOld && t->hasPos && t->hasCar && !g->pHomerWorld && cdx * cdx + cdz * cdz < GAME_TALK_CAR_NEAR * GAME_TALK_CAR_NEAR) {
    float dx = g->pCar->x - t->car.pos.x, dz = g->pCar->z - t->car.pos.z;
    if (dx * dx + dz * dz > 1.f) putCar(g, &t->car);
  }
  t->dist = hypotf(H->x - N.x, H->z - N.z); t->on = 1; t->starts++;
#ifndef GAME_C7_TALKHIDE_OLD
#define GAME_C7_TALKHIDE_OLD 0   // круг 7 (сверено с оригиналом), g07 строка 16 (P06-15) и обычные разговоры с местами: машина игрока в 7 м от места любого из героев прячется на время разговора (3D, квадрат расстояния меньше 49), трафик в 50 м вокруг места первого героя вычищается; после разговора машина видна. Было: только в диалоге бонус-миссии (там 100 м, без изменений). 1 — как было
#endif
  if (!GAME_C5_OLD && !GAME_C7_TALKHIDE_OLD && t->hasPos && !(m->rt.cur >= 0 && (m->f.missions[m->rt.cur].flags & MSN_MF_BONUS))) {   // обычный разговор с местами (круг 7)
    float d1 = sqrtf((g->pCar->x - P.x) * (g->pCar->x - P.x) + (g->pCar->y - P.y) * (g->pCar->y - P.y) + (g->pCar->z - P.z) * (g->pCar->z - P.z));
    float d2 = sqrtf((g->pCar->x - N.x) * (g->pCar->x - N.x) + (g->pCar->y - N.y) * (g->pCar->y - N.y) + (g->pCar->z - N.z) * (g->pCar->z - N.z));
    if (d1 < 7.f || d2 < 7.f) { gameMsnCarHide = 1; m->carHidden = 1; }
    { MsnVec3 C1 = t->pcFirst ? P : N;
      if (roadsT && roadsT->hd) for (int i = 0; i < TR_MAXCAR; i++) { if (roadsT->car[i].mode == TRM_OFF) continue; float cx = roadsT->car[i].car.x - C1.x, cz = roadsT->car[i].car.z - C1.z; if (cx * cx + cz * cz < 50.f * 50.f) roadsT->car[i].mode = TRM_OFF; } }
  }
  if (!GAME_C5_OLD && t->hasPos && m->rt.cur >= 0 && (m->f.missions[m->rt.cur].flags & MSN_MF_BONUS)) {
    // круг 5, P06-15: диалог бонус-миссии — машина игрока ближе 7 м к местам героев прячется на время диалога; трафик в 100 м вокруг Гомера вычищается; после диалога всё возвращается
    float d1 = hypotf(g->pCar->x - P.x, g->pCar->z - P.z), d2 = hypotf(g->pCar->x - N.x, g->pCar->z - N.z);
    if (d1 < 7.f || d2 < 7.f) { gameMsnCarHide = 1; m->carHidden = 1; }
    if (roadsT && roadsT->hd) for (int i = 0; i < TR_MAXCAR; i++) { if (roadsT->car[i].mode == TRM_OFF) continue; float cx = roadsT->car[i].car.x - P.x, cz = roadsT->car[i].car.z - P.z; if (cx * cx + cz * cz < 100.f * 100.f) roadsT->car[i].mode = TRM_OFF; }
  }
}
#ifndef GAME_C7_NPCBACK_OLD
#define GAME_C7_NPCBACK_OLD 0   // круг 7, СВЕРКА_ВНЕДРЕНИЕ/g07-missions.md строка 37 (КАМЕРА 4.3; сверено с оригиналом): на плане pc_near (крупный план игрока) собеседник отодвигается на 1,8 м назад по оси от игрока и на 0,21 м вбок; у остальных планов сдвига нет. Боковые 0,21 м не делаю (сторона зависит от зеркала мира, на глаз не видно). Камера считает по месту без сдвига. Было: собеседник стоял на месте на всех планах. 1 — как было
#endif
#define NPC_BACK_M 1.8f
void gameMsnTalkNpcBack(GameMsn *m, int on) {
  GameTalk *t = &m->talk; if (GAME_C7_NPCBACK_OLD || !t->on || !t->npc[0] || !*m->g->pHaveHomer) return;
  GameNpc *n = npcFind(m, t->npc); if (!n || n->knock) return;
  if (!!on == t->shifted) return;
  const Homer *H = m->g->pHomer; float ax = t->baseN.x - H->x, az = t->baseN.z - H->z, d = hypotf(ax, az); if (d < 1e-3f) return;
  n->pos.x = t->baseN.x + (on ? ax / d * NPC_BACK_M : 0.f); n->pos.z = t->baseN.z + (on ? az / d * NPC_BACK_M : 0.f); t->shifted = !!on;
}
const float *gameMsnTalkNpcBase(const GameMsn *m) { return (!GAME_C7_NPCBACK_OLD && m->talk.on && m->talk.shifted) ? &m->talk.baseN.x : 0; }
static void talkEnd(GameMsn *m) {
  if (m->talk.shifted) { GameNpc *n0 = npcFind(m, m->talk.npc); if (n0) { n0->pos.x = m->talk.baseN.x; n0->pos.z = m->talk.baseN.z; } m->talk.shifted = 0; }
  if (!GAME_TALKRET_OLD && m->talk.on && !m->talk.noReturn && (m->talk.hasPos || GAME_C3_GIVEN_OLD) && m->preTalk.on) {   // круг 3: только если для разговора заданы места
    // P07-14 / P06-16: по концу разговора Гомер и собеседник возвращаются на места, где стояли до него (они были не в машине — preTalk пишется только пешком);
    // в 14 разговорах (SetDialoguePositions 4-й аргумент 1) остаются на местах разговора
    Game *g = m->g; GameNpc *n = npcFind(m, m->preTalk.npc);
    if (n) { n->pos = m->preTalk.np; n->h = m->preTalk.nh; n->walking = 0; n->iwp = n->nwp; n->homing = 0; }
    if (*g->pHaveHomer && gameOnFoot(g) && !m->preTalk.hCar) homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : g->pWorld, m->preTalk.hp.x, m->preTalk.hp.y, m->preTalk.hp.z, m->preTalk.hh);   // круг 3: начавший разговор за рулём остаётся на месте разговора
    m->preTalk.on = 0;
  }
  m->talk.on = 0;
  if (m->carHidden) { m->carHidden = 0; gameMsnCarHide = 0; }   // круг 5, P06-15: машина снова видна
}
static void talkReset(GameMsn *m) {   // новый этап: команды разговора — заново из его данных
  GameTalk *t = &m->talk; unsigned st = t->starts; int off = t->off; GameTalk keep = *t;
  memset(t, 0, sizeof *t); t->starts = st; t->off = off;
  if (!GAME_TALKGEST_OLD) {   // круг 2, P15-15 (g04): играющий жест не рвётся, если новый этап жестов не задаёт — список жестов остаётся, пока этап не задаст свои
    memcpy(t->anim, keep.anim, sizeof t->anim); memcpy(t->nAnim, keep.nAnim, sizeof t->nAnim); memcpy(t->rnd, keep.rnd, sizeof t->rnd); m->animStale = keep.nAnim[0] + keep.nAnim[1] > 0;
  }
}
int gameMsnTalkLine(const GameMsn *m, int *pcSpeaks) {
  const MsnAudio *a = &m->audio;
  if (!m->talk.on) return -1;
  if (!a->b) { if (pcSpeaks) *pcSpeaks = 0; return 0; }   // без звука (боты): одна реплика
  if (!a->active || !a->blocking || a->line < 0 || a->line >= a->nlines) return -1;
  if (pcSpeaks) { const char *ac = a->actor[a->line]; *pcSpeaks = 0;
    for (unsigned i = 0; ac && i < sizeof pcCodes / sizeof *pcCodes; i++) if (!strcmp(ac, pcCodes[i][1]) && !npcFind((GameMsn *)m, pcCodes[i][0])) *pcSpeaks = 1; }
  return a->line;
}
const char *gameMsnTalkAnim(const GameMsn *m, int who, int line) {
  const GameTalk *t = &m->talk;
  if (who < 0 || who > 1 || line < 0 || !t->nAnim[who]) return 0;
  int i = t->rnd[who] ? (int)(((unsigned)line * 2654435761u + (unsigned)who * 40503u + t->starts * 97u) >> 16) % t->nAnim[who] : line;   // случайный жест — детерминированно по номеру
  if (i >= t->nAnim[who]) return 0;
  const char *s = t->anim[who][i];
  return (!s || !*s || msn_streq(s, "none")) ? 0 : s;
}

// ------------------------------------------------------------------ звук миссий (msn_audio.c): голос разговоров и музыка
static const MsnAudioBackend *audioB;
void gameMsnSetAudio(const MsnAudioBackend *b) { audioB = b; }
static void sndDialogue(void *u, const MsnDialogue *d) { msnAudioDialogue(&GM->audio, d, 1); VIEW(sound, dialogue, d); }
static void sndOneliner(void *u, const MsnDialogue *d) { msnAudioDialogue(&GM->audio, d, 0); VIEW(sound, oneliner, d); }
static void sndMusicEv(void *u, const char *ev) { msnAudioEvent(&GM->audio, ev); VIEW(sound, music_event, ev); }
static void sndMusicChange(void *u) { msnAudioSuffix(&GM->audio, "_drama"); if (GM->view.sound.music_change) GM->view.sound.music_change(u); }
static void sndAlwaysOn(void *u) { msnAudioAlwaysOn(&GM->audio); if (GM->view.sound.music_always_on) GM->view.sound.music_always_on(u); }
static void sndState(void *u, const char *state, const char *value) { msnAudioState(&GM->audio, state, value); VIEW(sound, music_state, state, value); }
static void audioEvent(GameMsn *m, const MsnEvent *e) {
  if (!m->audio.b) return;
  const MsnRuntime *rt = &m->rt;
  switch (e->type) {
    case MSN_EV_PHASE_START:   // основная часть миссии (после брифинга; при повторе с чекпоинта — тоже): <код>_start
      if (e->index == 1 && rt->cur >= 0) msnAudioMissionStart(&m->audio, msn_str(&m->f, m->f.missions[rt->cur].id), (m->f.missions[rt->cur].flags & MSN_MF_BONUS) != 0);
      break;
    case MSN_EV_STAGE_START: msnAudioStage(&m->audio); break;
    case MSN_EV_MISSION_COMPLETE: msnAudioSuffix(&m->audio, "_win"); m->audio.code[0] = 0; break;
    case MSN_EV_MISSION_FAILED: msnAudioSuffix(&m->audio, "_lose"); m->audio.code[0] = 0; break;
    case MSN_EV_CLEANUP: if (m->audio.blocking) msnAudioStopVoice(&m->audio); msnAudioMissionEnd(&m->audio); break;
    default: break;
  }
}
// правка 40: окно старта фазы — её подготовка и команды первого этапа (гонки ставят машину PlacePlayerCar в этапе 0); всё это
// приходит одним вызовом рантайма, после него — оценка: камера фазы есть и игрок поставлен сценарием -> заявка виду
static void scamEval(GameMsn *m) {
  if (!m->scam.arm || m->scam.stages < 1) return;
  if (m->scam.placed && m->scam.cam) { m->scam.req = 1; m->scam.count++; }
  m->scam.arm = 0;
}
void gameMsnAbort(GameMsn *m) {
  // [правило Роба №369] Перезапуск вступления нужен для следующего разговора,
  // но его Reset/Place/Walk не должны возвращать игрока к началу миссии
  Car *c = m->g->pCar;
  float x = c->x, y = c->y, z = c->z, h = c->h;
  m->keepPlayer = 1; m->scam.quiet = 1;
  msn_abort(&m->rt);
  // Cleanup машины миссии может заменить её машиной уровня; место сохраняется
  c->x = x; c->y = y; c->z = z; c->h = h;
  m->keepPlayer = 0; m->scam.quiet = 0;
}
void gameMsnTalkCancel(GameMsn *m) {   // В85 (game_msn.h): Cancel карточки разговора
  if (!gameMsnAbortAllowed(m)) return;   // P06-20 / P07-23: AllowMissionAbort("false") — отказаться нельзя
  GamePreTalk pre = m->preTalk;
  gameMsnAbort(m);
  if (GAME_TALK_CANCEL_OLD) return;
  m->preTalk.on = 0;
  Game *g = m->g; GameNpc *n = pre.on ? npcFind(m, pre.npc) : 0;
  if (n && *g->pHaveHomer && gameOnFoot(g)) {
    n->pos = pre.np; n->h = pre.nh; n->walking = 0; n->iwp = n->nwp; n->homing = 0;   // стоит, где стоял; маршрут не продолжает
    homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : g->pWorld, pre.hp.x, pre.hp.y, pre.hp.z, pre.hh);
  }
  m->retalkT = GAME_TALK_RETALK;
}
int gameMsnStartCam(GameMsn *m, const char **phase, const char **cam) {
  scamEval(m);
  if (!m->scam.req) return 0;
  m->scam.req = 0; if (phase) *phase = m->scam.phase; if (cam) *cam = m->scam.cam;
  return 1;
}
// круг 2: события миссии → празднование Гомера (g04), звуки (g08)
static void c2Event(GameMsn *m, const MsnEvent *e) {
  Game *g = m->g; const MsnRuntime *rt = &m->rt;
  if (!GAME_C2_CELEB_OLD && *g->pHaveHomer) {   // g04 P05:1, P07 Д4: этап со вспышкой — малый клип, успех миссии — большой (в машине клипов нет — вернёт 0)
    if (e->type == MSN_EV_STAGE_COMPLETE && e->index) homerCelebrate(g->pHomer, 0);
    if (e->type == MSN_EV_MISSION_COMPLETE) homerCelebrate(g->pHomer, 1);
  }
#ifdef __PSP__
  if (!GAME_C2_SOUND_OLD) {
    static int sv8Mis = -1;   // сверка g08 круг 6: номер текущей миссии (для звука подбора: L2 m6 — свой клип)
    if (e->type == MSN_EV_MISSION_START && e->name && e->name[0] == 'm' && e->name[1] >= '0' && e->name[1] <= '9' && m->f.hdr) {
      sv8Mis = e->name[1] - '0';
      sndMissionAmbience((int)m->f.hdr->level, e->name[1] - '0', *g->pHaveHomer && gameOnFoot(g));   // g08 P09:22: атмосфера старта миссии
    }
    if (e->type == MSN_EV_COLLECT && m->f.hdr && m->f.hdr->level >= 1 && m->f.hdr->level <= 7) {   // g08 P09:23: звук подбора цели «собрать»
      // круг 6 (g08, P09:23): клип подбора по уровню и миссии — sound.c sndPickupClip (SV_PICKUP_OLD=1 — прежнее: общий клип уровня L1-L6, L7 молчит)
      const char *nm = sndPickupClip((int)m->f.hdr->level, sv8Mis); int id = *nm ? sndId(nm) : -1; if (id >= 0) sndPlay2D(id, 1.f);
    }
    if (e->type == MSN_EV_STAGE_COMPLETE && rt->st && rt->obj == MSN_OBJ_GOTO && !(rt->st->flags & MSN_SF_GOTODIALOGOFF)) sndSay(SND_SAY_ARRIVE, 0);   // g08 P07:33: цель достигнута (шанс 20 % внутри)
    if (e->type == MSN_EV_MISSION_COMPLETE && e->index == 2) { const char *tr; sndMusicEvent("Win_3Street_Races", &tr); }   // g08 P09:Д4: все три уличные гонки
    if (!GATEDFANFARE_OLD && e->type == MSN_EV_LOCKED_HINT) { const char *tr; sndMusicEvent("Gated_Mission", &tr); }   // ТЕСТ90: запертый этап кончился, нужной машины/костюма нет — фанфара закрытого этапа
  }
#else
  (void)rt;
#endif
}
int gameMsnResetAlways;   // отрицательный контроль правки 61: перенос на точку сброса и после пройденной миссии (как раньше)
// Правка Роба 1205 (05.11: «в уличных гонках проверить соперничество»). Уличная гонка (sr1, sr2, sr3) — отдельная миссия после вступления (разговор «sr3sd»): у оригинала на загрузке
// любой такой миссии зовётся сброс игрока при загрузке миссии — машина игрока ставится на SetMissionResetPlayerInCar, а он сам садится в неё (посадка в машину). Данные уровней 1–4, 6, 7 это
// дублируют в этапе (PlacePlayerCar + PutMFPlayerInCar), данные УРОВНЯ 5 (sr1, sr2, sr3) — нет: у нас SetMissionResetPlayerInCar в окне подготовки основной фазы пропускался (plReset: не
// startArmed), Гомер оставался пешком там, где кончился разговор, а вернее — на месте до разговора, машина стояла на старте. Игрок пешком не гоняет: соперники уезжали, а ждущая гонка
// (sr3: дальше 300 м от игрока соперник встаёт на ручник) не двигалась вовсе (L5 sr3 bart_v «не едет», L5 sr1–sr3 у ботов «застрял»). RACE_RESET_OLD=1 / «flag raceresetold 1» — как было
#ifndef RACE_RESET_OLD
#define RACE_RESET_OLD 0
#endif
int gameMsnRaceResetOld = RACE_RESET_OLD;
static void onEvent(void *u, const MsnEvent *e) {   // окно подготовки фазы и старт/повтор (см. plReset)
  switch (e->type) {
    // Правка Роба 61 (29.09: «вышли от Апу — сразу перенесло к дому; должен быть путь домой своим ходом»): SetMissionReset* —
    // точка повтора/загрузки миссии. У m1sd первый этап — «доехать до дома» (goto m2_simpsonhouse_sd), значит после пройденной
    // миссии игрока не переносят. Ставим игрока только на старте уровня, повторе, загрузке, MISSION SELECT
    case MSN_EV_MISSION_START: GM->startArmed = gameMsnResetAlways || !(e->name2 && !strcmp(e->name2, "continue")); break;
    case MSN_EV_CLEANUP: GM->startArmed = 1; break;
    case MSN_EV_PHASE_START: GM->inSetup = 1;   // правка 40: окно старта фазы — до второго этапа (гонки ставят игрока в этапе 0)
      if (!gameMsnRaceResetOld && e->index == 1 && GM->rt.cur >= 0 && (unsigned)GM->rt.cur < GM->f.nmissions && (GM->f.missions[GM->rt.cur].flags & MSN_MF_STREETRACE)) GM->startArmed = 1;   // правка 1205: основная фаза уличной гонки — ResetPlayer оригинала
      GM->scam.cam = 0; GM->scam.phase = e->name; GM->scam.arm = !GM->scam.quiet; GM->scam.quiet = 0; GM->scam.placed = 0;
      GM->scam.stages = 0; GM->scam.req = 0; break;
    case MSN_EV_STAGE_START: GM->inSetup = 0; GM->startArmed = 0; talkReset(GM); GM->nsafe = 0;   // команды этапа придут следом
      if (GM->scam.stages++ >= 1) { scamEval(GM); GM->scam.arm = 0; } break;
    case MSN_EV_DIALOGUE: talkBegin(GM, e->name, e->name2); break;                        // разговор: места и лицом друг к другу
    default: break;
  }
  c2Event(GM, e);
  if (e->type == MSN_EV_MISSION_COMPLETE) GM->missionDone = 1; else if (e->type == MSN_EV_MISSION_START) GM->missionDone = 0;
  if (e->type == MSN_EV_MISSION_START) { GM->missionClock = 0.f; gameMsnRecord(GM, 0); } else if (e->type == MSN_EV_MISSION_COMPLETE) gameMsnRecord(GM, 1);   // №1114: попытка и лучшее время
  if (e->type == MSN_EV_CLEANUP || e->type == MSN_EV_MISSION_FAILED || e->type == MSN_EV_STAGE_COMPLETE) talkEnd(GM);
  if (!GAME_HIDECHAR_OLD && e->type == MSN_EV_STAGE_COMPLETE && GM->hideName[0]) { npcRemove(GM, GM->hideName); GM->hideName[0] = 0; }   // P06-21: на выходе из этапа персонаж убирается
  if (!GAME_HIDECHAR_OLD && (e->type == MSN_EV_CLEANUP || e->type == MSN_EV_MISSION_START)) { GM->hideName[0] = 0; GM->hudHidden = 0; }
  if (e->type == MSN_EV_CLEANUP) GM->nsafe = 0;
  audioEvent(GM, e);
  if (GM->view.on_event) GM->view.on_event(u, e);
}

// ------------------------------------------------------------------ кошелёк и продавцы (viewer/shop.h)
static void shCoins(void *u, int delta, int why) { shopCoinsAdd(&GM->shop, delta, why); if (GM->view.shop.coins_add) GM->view.shop.coins_add(u, delta, why); }
static int shOwns(void *u, int kind, const char *name) { return shopOwns(&GM->shop, kind, name); }
static const char *shCurSkin(void *u) { (void)u; return skinNameFn ? skinNameFn() : 0; }   // P06-8: надетый костюм; без функции main — NULL («неизвестно»)
// метка этапа buyskin (msn_run.c nav): функцию цели ставит main (комнаты и места из skins.txt) или бот; нет функции — метки нет
static int (*skinTargetFn)(float px, float py, float pz, float out[3]);
void gameMsnSetSkinTarget(int (*fn)(float, float, float, float *)) { skinTargetFn = fn; }
static int shSkinSpot(void *u, MsnVec3 *out) {
  (void)u; if (!skinTargetFn) return 0;
  MsnWorld w; gameMsnWorld(GM, &w); float t[3];
  if (!skinTargetFn(w.player.x, w.player.y, w.player.z, t)) return 0;
  out->x = t[0]; out->y = t[1]; out->z = t[2]; return 1;
}
static void shUnlock(void *u, const MsnReward *r) { if (r && r->name) shopGive(&GM->shop, r->name); if (GM->view.shop.unlock) GM->view.shop.unlock(u, r); }
// П1 (L1_ФИНИШ 30.09): предметы миссии «сломать» — delivery без рисунка (msn_run.c item_breakable; L1 m4: щитки PP_powerbox1..9).
// Рантайм зовёт props.activate_tag на показе предмета: стейтпроп с тегом N (tools/prp2_export.py: экземпляр powerboxN) — в s1
// (виден, коллизия), и после прошлой попытки (s3) снова целый; конец этапа (items_clear) — несломанные назад в s0 (спят до миссии).
// OBJECT_DESTROYED стейтпропа (stateprops.c destroyedTags) -> msn_notify(MSN_IN_DESTROYED, "powerboxN") один раз (gameMsnTick).
// Данные диска (tools/stateprop_dump.py l1z6_powerbox): s1 HIT->s2, s2 HIT->s3, STOMP->s3; s3 OBJECT_DESTROYED, SPAWN_1_COIN.
// До 30.09 связи не было: m4 не проходилась ни на PSP, ни ботом (щитки спали в s0, цель не засчитывалась).
// GAME_MSN_BREAKABLES=0 — отрицательный контроль гейта tests/missions/powerbox.sh (как было: щитки не включаются)
#ifndef GAME_MSN_BREAKABLES
#define GAME_MSN_BREAKABLES 1
#endif
static int propsActivateTag(void *u, int tag) {
  GameMsn *m = GM; Game *g = m->g; int n = 0;
  if (*g->pHaveSP && tag > 0 && tag < 32) {
    StateProps *sp = g->pSp;
    for (unsigned i = 0; i < sp->ninst; i++) if (sp->inst[i].tag == tag && sp->ntype && sp->type[sp->inst[i].type].nState > 1) { spEnter(sp, i, 1); n++; }
    sp->destroyedTags &= ~(1u << tag); m->tagsSent &= ~(1u << tag); if (n) m->tagsOn |= 1u << tag;
  }
  if (m->view.props.activate_tag) m->view.props.activate_tag(u, tag);
  return n;
}
static void propsItemsClear(void *u) {
  GameMsn *m = GM; Game *g = m->g;
  if (*g->pHaveSP && m->tagsOn) {
    StateProps *sp = g->pSp; uint32_t rest = m->tagsOn & ~sp->destroyedTags;
    for (unsigned i = 0; i < sp->ninst; i++) if (sp->inst[i].tag && sp->inst[i].tag < 32 && (rest >> sp->inst[i].tag & 1u)) spEnter(sp, i, 0);
  }
  m->tagsOn = 0;
  if (m->view.props.items_clear) m->view.props.items_clear(u);
}
static void tagsReport(GameMsn *m) {   // сломанные включённые щитки -> рантайму (один раз на включение)
  Game *g = m->g; if (!*g->pHaveSP) return;
  uint32_t fresh = g->pSp->destroyedTags & m->tagsOn & ~m->tagsSent;
  for (int k = 1; k < 32 && fresh; k++) if (fresh >> k & 1u) {
    char nm[16]; snprintf(nm, sizeof nm, "powerbox%d", k); m->tagsSent |= 1u << k; fresh &= ~(1u << k);
    msn_notify(&m->rt, MSN_IN_DESTROYED, nm, 0);
  }
}
static void npcGiver(void *u, const MsnGiver *gv) {   // AddPurchaseCarReward: продавец, его место и лавка
  if (gv && gv->kind == MSN_GIVER_SELLER && gv->mission) {
    float car[3] = {gv->car_at.pos.x, gv->car_at.pos.y, gv->car_at.pos.z};
    shopAddSeller(&GM->shop, gv->mission, gv->name, gv->at.pos.x, gv->at.pos.y, gv->at.pos.z, gv->radius, gv->car_at.loc ? car : 0);
    // правка 140: продавец стоит на своём локаторе (данные: AddPurchaseCarReward, маршрут — AddPurchaseCarNPCWaypoint;
    // запись PS2 13:51 — Барни у заправки). В списке персонажей, как фоновый: модель по близости (gameMsnAmbWant)
#if !defined(GAME_PLACE_NPC) || GAME_PLACE_NPC   // -DGAME_PLACE_NPC=0 — прежнее поведение (продавца нет в списке), красный гейта
    if (gv->name && gv->at.loc) npcSpawn(u, gv->name, &gv->at, MSN_NPC_PLACE, gv->radius, "");
#endif
  }
#if !defined(GAME_BONUS_NPC) || GAME_BONUS_NPC   // -DGAME_BONUS_NPC=0 — прежнее (бонусных в мире нет), красный гейта tests/npc/bonus_twins.py
  // Правка 180 (близнец О4 03.10: у оригинала Ральф гонки sr3 стоит на sr3_ralph_sd под клетчатым флагом, Луи — у школы под
  // кубиками; Б2: louie_m, nelson_m, cletus_m читаются за ~150 м от их мест): AddNPCCharacterBonusMission ставит персонажа на его
  // локатор с начала уровня — к нему подходят пешком (△ — разговор и миссия, msn_bonus_near). Значок над ним рисует вид
  else if (gv && gv->name && gv->at.loc && gv->mission_index >= 0 &&
           (GAME_C2_BONUS_OLD || !((GM->f.missions[gv->mission_index].flags & MSN_MF_BONUSMISSION) && (GM->rt.completed >> gv->mission_index & 1) && !GM->rt.dev_bonus_replay))) {   // круг 2, P06-2: пройденный bm1 на новом входе не создаётся
    npcSpawnB(u, gv->name, &gv->at, MSN_NPC_PLACE, 0.f, "", 1 + gv->mission_index);
    if (GM->nbgiver < GAME_BGIVER_MAX) {
      int k = GM->nbgiver++; strncpy(GM->bgiver[k].name, gv->name, sizeof GM->bgiver[k].name - 1);
      GM->bgiver[k].at = gv->at; GM->bgiver[k].mission = gv->mission_index;
    }
  }
#endif
  VIEW(npc, giver, gv);
}
int gameShopOpen(const GameMsn *m) { return m->shop.open ? m->shop.at : -1; }
void gameShopInput(GameMsn *m, const GamePad *p) {   // лавка открыта: ◄ ► X ○ △ (см. game_msn.h)
  if (!m->shop.open) return;
  Shop *s = &m->shop;
  if (p->hit & (GP_CIRCLE | GP_TRIANGLE)) { s->open = 0; return; }
  if (p->hit & GP_LEFT) shopStep(s, -1);
  if (p->hit & GP_RIGHT) shopStep(s, 1);
  if (p->hit & GP_CROSS) { int r = shopBuy(s, shopSellerItem(s, s->at, s->sel)); if (r != SHOP_OK) m->shopDenied++; }
}

int gameMsnOpen(GameMsn *m, Game *g, const void *pack, size_t size, const MsnGameApi *view, char *err, int errlen) {
  return gameMsnOpenAt(m, g, pack, size, view, NULL, err, errlen);
}
// Загрузка сохранения (viewer/savegame.h): пройденные миссии из записи; текущая сюжетная миссия — с её чекпоинта (msn_resume),
// если она не пройдена и запись в границах пакета; иначе старт уровня с этими пройденными (следующая непройденная сюжетная)
#ifndef GAME_MSN_DEV
#define GAME_MSN_DEV 1   // красный контроль tests/savegame/devsave/host.sh: 0 — загрузка DEV SAVE как прежде (новая игра уровня)
#endif
int gameLvProgOld;   // 1: прежний прогресс только текущего уровня (контроль lvprogold)
uint32_t gameMsnDevMask(const MsnFile *f) {   // правка 335: сюжетные (не бонус, не переход, не движковые), кроме последней
  uint32_t m = 0; int last = -1;
  for (uint32_t i = 0; i < f->nmissions && i < 32; i++) {
    if (f->missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION)) continue;
    m |= 1u << i; last = (int)i;
  }
  return last >= 0 ? m & ~(1u << last) : 0;
}
static void resumeAt(MsnRuntime *rt, const MsnFile *f, const MsnProgress *p) {
  if (GAME_MSN_DEV && p->story == GAME_MSN_STORY_DEV && p->level == f->hdr->level) {   // правка 335: DEV SAVE — следующая
    rt->completed = gameMsnDevMask(f);
#ifndef GAME_MSN_DEV_BONUS_OLD
    // Rob 350: DEV means full bonus/race progress. Final story remains open to
    // prevent automatic level completion; replay access is transient DEV state.
    for (uint32_t i = 0; i < f->nmissions && i < 32; i++)
      if ((f->missions[i].flags & MSN_MF_BONUS) && !(f->missions[i].flags & (MSN_MF_NOSCRIPT | MSN_MF_TRANSITION))) rt->completed |= 1u << i;
    rt->dev_bonus_replay = 1;
#endif
    msn_level_start(rt); return; }                  // непройденная сюжетная = последняя
  int ok = p->level == f->hdr->level && p->story < f->nmissions && !(p->completed >> p->story & 1) && p->phase <= 1;
  if (ok) {
    const MsnMission *mm = &f->missions[p->story];
    unsigned ph = p->phase == 0 ? mm->intro : mm->main;
    ok = !(mm->flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT)) && ph != MSN_NONE16 && ph < f->nphases && p->stage < f->phases[ph].stage_count;
  }
  if (ok) { if (GAME_RESUME33_OLD) msn_resume(rt, p); else msn_resume_restart(rt, p); return; }   // P14-16: RESUME/загрузка — текущая миссия с начала
  rt->completed = p->level == f->hdr->level ? p->completed : 0;
  // облако 05.10 (карьера): возврат на пройденный уровень (MISSION SELECT обычной игры, main.c levelAdvance — маска из записи
  // прогресса всех уровней). Маска 0xFFFFFFFF — уровень пройден по записи до 05.10 (какие бонусы — неизвестно): как DEV, сюжетные
  // кроме последней пройдены. Пройден весь сюжет — последняя сюжетная снова текущая: у порта нет «уровень пройден, свободная езда»,
  // а старт уровня без непройденных сразу завершил бы его заново (LEVEL COMPLETE -> следующий уровень). Правило порта.
  // gameLvProgOld = 1 — прежнее (flag lvprogold 1)
  if (!gameLvProgOld && p->level == f->hdr->level && p->story == 0xff) {
    uint32_t dm = gameMsnDevMask(f), all = 0;
    for (uint32_t i = 0; i < f->nmissions && i < 32; i++) if (!(f->missions[i].flags & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION))) all |= 1u << i;
    if (rt->completed == 0xFFFFFFFFu) rt->completed = dm;
    else if (all && (rt->completed & all) == all) rt->completed &= ~(all & ~dm);
  }
  msn_level_start(rt);
}
int gameMsnOpenAt(GameMsn *m, Game *g, const void *pack, size_t size, const MsnGameApi *view, const MsnProgress *resume,
                  char *err, int errlen) {
  HomerSolidFn ps = 0; void *pu = 0;   // твёрдые тела Гомера до миссий; повторное открытие не оборачивает само себя
  if (*g->pHaveHomer) {
    const Homer *H = g->pHomer;
    if (H->solid == gameMsnSolid) { const GameMsn *o = H->solidUser; ps = o->prevSolid; pu = o->prevSolidUser; }
    else { ps = H->solid; pu = H->solidUser; }
  }
  gameMsnCarHide = 0; npcGen++; memset(m, 0, sizeof *m); m->g = g; m->vehicle = "famil_v"; m->prevSolid = ps; m->prevSolidUser = pu;
  if (view) m->view = *view;
  if (msn_open(&m->f, pack, size, err, errlen) != 0) return -1;
  m->hnr=gameMsnHnrConfig(&m->f);
  trafficPedGroup(roadsT, -1);
  shopInit(&m->shop, *g->pHaveCoins ? g->pCoins : 0, m->f.hdr->level);   // каталог — BindReward уровня (rewards.mfk)
  for (int k = 0, n = msn_reward_count(&m->f); k < n; k++) {
    MsnReward r; if (msn_reward_get(&m->f, k, &r) == 1) shopAddItem(&m->shop, r.name, r.kind, r.quest, r.price, r.seller);
  }
#ifndef GAME_SHOP_ATTR
#define GAME_SHOP_ATTR 1   // отрицательный контроль tests/shop: 0 — без SetCarAttributes (звёзд на экране покупки нет)
#endif
  for (uint32_t i = 0; GAME_SHOP_ATTR && m->f.level && i < m->f.level->rew_count; i++) {   // звёзды экрана покупки: SetCarAttributes (rewards.mfk)
    const MsnCmd *c = &m->f.cmds[m->f.level->rew_first + i];
    if (c->op != MSN_OP_SETCARATTRIBUTES || msn_argc(c) < 5) continue;
    float a[4]; for (int k = 0; k < 4; k++) a[k] = msn_arg_f(&m->f, c, 1 + k);
    shopSetAttr(&m->shop, msn_arg_s(&m->f, c, 0), a);
  }
  MsnGameApi api = m->view; api.user = m;   // что не наше (интерфейс, звук, ролики, пропы, машины) — прямо виду
  api.player.reset = plReset; api.player.vehicle = plVehicle; api.player.place_car = plPlaceCar; api.player.swap_default_car = plSwap;
  api.player.enter_car = plEnter; api.player.walk_to = plWalk;
  if (!GAME_C2_STREETRACE_OLD) api.props.race_props = propsRaceC2;   // круг 2, g02 P03-13
#ifdef __PSP__
  msnWagerPayFn = GAME_C2_ODDS_OLD ? 0 : mvWagerPay;   // круг 2, g07b P06-3: выплата ставки по коэффициенту машины
#endif
  api.nav.marker = navMarker; api.ui.flash = uiFlash; api.ui.failed = uiFailed;
  api.npc.spawn = npcSpawn; api.npc.waypoint = npcWaypoint; api.npc.remove = npcRemove;
  api.entity = entity; api.cleanup = cleanup; api.on_event = onEvent; api.feature = feat;
#if !defined(GAME_BONUS_NPC) || GAME_BONUS_NPC
  api.bonus_entity = bonusEntity;   // правка 180
#endif
#ifndef GAME_SHOP_API
#define GAME_SHOP_API 1   // отрицательный контроль tests/shop: 0 — рантайм миссий без кошелька (ставки, «нужен Plow King»)
#endif
  if (GAME_SHOP_API) { api.shop.coins_add = shCoins; api.shop.owns = shOwns; api.shop.cur_skin = shCurSkin; api.shop.unlock = shUnlock; api.shop.skin_spot = shSkinSpot; }
  api.npc.giver = npcGiver;
  if (GAME_MSN_BREAKABLES) { api.props.activate_tag = propsActivateTag; api.props.items_clear = propsItemsClear; }   // m4: щитки
  m->tagsOn = m->tagsSent = 0;
  api.cars.load = carLoad; api.cars.spawn = carSpawn; api.cars.waypoint = carWaypoint; api.cars.activate = carActivate;
  api.cars.ai_params = carParamsCb; api.cars.catchup = carCatchup; api.traffic.max_cars = trafficMax; api.traffic.clear = trafficClear;
  mcInit(&m->cars);
  nwcar = 0;   // правка 53: машины мира ставит уровень заново (main.c после msnStart)
  msnAudioInit(&m->audio, audioB);
  if (audioB) {   // голос и музыка миссий — здесь (общие с ботами); вид получает те же вызовы после
    api.sound.dialogue = sndDialogue; api.sound.oneliner = sndOneliner; api.sound.music_event = sndMusicEv;
    api.sound.music_change = sndMusicChange; api.sound.music_always_on = sndAlwaysOn;
    api.sound.music_state = sndState;
  }
  msn_game_init(&m->mg, &m->rt, &m->f, &api);
  if (*g->pHaveHomer) { g->pHomer->solid = gameMsnSolid; g->pHomer->solidUser = m; }   // персонажи миссий твёрдые для Гомера (поверх стейтпропов)
  m->rt.defer_first = gameMsnDeferStart && resume;   // №943: выбранная строка MISSION SELECT стартует сама — стартовая цель уровня не ставится
  if (resume) resumeAt(&m->rt, &m->f, resume);   // загрузка: игрока ставит сценарий, как при старте миссии (MSN_EV_MISSION_START)
  else msn_level_start(&m->rt);
  m->rt.defer_first = 0;
  return 0;
}
void gameMsnWorld(const GameMsn *m, MsnWorld *w) {
  const Game *g = m->g; memset(w, 0, sizeof *w);
  int foot = gameOnFoot(g);
  w->player.x = foot ? g->pHomer->x : g->pCar->x; w->player.y = foot ? g->pHomer->y : g->pCar->y; w->player.z = foot ? g->pHomer->z : g->pCar->z;
  w->on_foot = foot; w->vehicle = (!GAME_C5_OLD && m->jacked && m->jackedName[0]) ? m->jackedName : g->pCar->p.name[0] ? g->pCar->p.name : m->vehicle;   // круг 5: угнанная/вызванная машина — по её имени   // машина уровня (CAR1 v2), иначе famil_v
  w->vehicle_health = 1.f - g->pCar->damage; w->interior = m->interior;
  w->coins = shopCoins(&m->shop);   // кошелёк (Coins.collected; без монет мира — кошелёк лавки)
  w->transition = m->transition; w->arrested = m->arrested; w->entering = m->entering[0] ? m->entering : 0;   // сверка g07: переход в интерьер, арест
  w->vehicle_boom = g->pCar->destroyed ? (carBoomed(g->pCar) ? 2 : 1) : 0;   // P07-13: взрыв машины игрока идёт (1) / доигран (2)
}
// сверка g07 (02.11.2026): main сообщает состояние мира, которого миссии не видно. transition — идёт вход в помещение или выход (от начала перехода до конца);
// arrested — полиция ловит игрока; entering — имя интерьера, в который игрок входит (в момент начала перехода; NULL/"" — нет). Без вызова всё 0 (прежнее поведение)
void gameMsnSetState(GameMsn *m, int transition, int arrested, const char *entering) {
  m->transition = transition; m->arrested = arrested;
  snprintf(m->entering, sizeof m->entering, "%s", entering ? entering : "");
}
// P05-11 / P06-12: лавка и гардероб — только пока идёт вступление миссии («воскресная езда», фаза 0) и вне миссий; в основной части и в бонус-миссиях выключены
int gameMsnShopAllowed(const GameMsn *m) {
  if (GAME_SHOPPHASE_OLD) return 1;
  const MsnRuntime *rt = &m->rt;
  if (rt->cur < 0 || rt->status == MSN_IDLE || rt->status == MSN_LEVEL_DONE) return 1;
  return rt->phase == 0 && !(m->f.missions[rt->cur].flags & MSN_MF_BONUS);
}
// P06-20 / P07-23: 0 — AllowMissionAbort("false") (L1 m3sd, L2 m6sd, L6 m1sd): отказаться от начала миссии нельзя; Accept/Cancel в разговоре показывать не надо
int gameMsnAbortAllowed(const GameMsn *m) { return GAME_ABORT_OLD ? 1 : msn_abort_allowed(&m->rt); }
int gameMsnTalking(const GameMsn *m) { return m->talk.on; }   // P (5.11): идёт разговор — прибавка шкалы Hit & Run и счёта должна стоять (модуль game.c / stateprops)
int gameMsnHudHidden(const GameMsn *m) { return m->hudHidden; }   // P06-21: ShowHUD("false") — HUD миссии спрятан
static int msnWants(GameMsn *m, const MsnWorld *wp);
static int msnWantsX(GameMsn *m, const MsnWorld *wp, int withBonus);
static int sellerFirst96(const GameMsn *m);
int gameMsnWantsMissionAction(GameMsn *m) { MsnWorld w; gameMsnWorld(m, &w); return w.on_foot && msnWants(m, &w); }
// Правка Роба 206: подпись к △ миссии — что делает действие (правило порта, у HUD оригинала подписи нет): разговор — «Talk To <Имя>»
// (имя собеседника из данных SetTalkToTarget, без префикса m3_ и т.п.), бонус-персонаж — «Talk», точка с действием — «Use»,
// продавец — «Shop», лавка открыта — «Cancel»
const char *gameMsnActionLabel(GameMsn *m, char *buf, int n) {
  MsnWorld w; gameMsnWorld(m, &w); MsnRuntime *rt = &m->rt;
  if (m->shop.open) return "Cancel";
  if (rt->auto_bonus && msn_bonus_near(rt, &w) >= 0 && !(sellerFirst96(m) && shopSellerNear(&m->shop, w.player.x, w.player.y, w.player.z) >= 0 && gameMsnShopAllowed(m))) return "Talk";
  // правка 288: подпись △ — от того, что △ сделает здесь: у продавца, вне радиуса разговора миссии, — «Shop», а не «Talk To Marge»
  { int sv = shopSellerNear(&m->shop, w.player.x, w.player.y, w.player.z);   // правка 306: собеседник этапа — не «Shop»
    if (sv >= 0 && rt->status == MSN_RUNNING && rt->st && rt->obj == MSN_OBJ_TALKTO && rt->target && msn_streq(rt->target, m->shop.seller[sv].npc)) sv = -1;
    if (!gameSellerAtLoc && !msnWantsX(m, &w, !sellerFirst96(m)) && sv >= 0 && gameMsnShopAllowed(m)) return "Shop"; }
  if (rt->status == MSN_RUNNING && rt->st && rt->obj == MSN_OBJ_TALKTO && rt->target) {
    char nb[40]; const char *nm = flowTalkName(rt->target, nb, sizeof nb);   // имя как в тексте задания (viewer/flowrules.h), не код данных
    snprintf(buf, (size_t)n, "Talk To %s", nm);
    return buf;
  }
  if (rt->status == MSN_RUNNING && rt->st && rt->obj == MSN_OBJ_GOTO) return "Use";
  if (shopSellerNear(&m->shop, w.player.x, w.player.y, w.player.z) >= 0 && gameMsnShopAllowed(m)) return "Shop";
  return "";
}
int gameMsnWantsAction(GameMsn *m) {
  MsnWorld w; gameMsnWorld(m, &w);
  if (!w.on_foot) return 0;
  if (m->shop.open) return 1;                                               // лавка открыта: △ — Cancel
  if (msnWants(m, &w)) return 1;
  return !m->talk.on && shopSellerNear(&m->shop, w.player.x, w.player.y, w.player.z) >= 0;   // продавец: лавка
}
static int msnWantsX(GameMsn *m, const MsnWorld *wp, int withBonus) {   // △ нужен самой миссии: бонус-персонаж, разговор в радиусе, точка с действием
  MsnRuntime *rt = &m->rt; MsnWorld w = *wp;
  if (withBonus && rt->auto_bonus && msn_bonus_near(rt, &w) >= 0) return 1;              // бонус-персонаж рядом: разговор
  if (rt->status != MSN_RUNNING || !rt->st) return 0;
  if (rt->obj == MSN_OBJ_TALKTO && rt->target) {                             // тот же радиус, что у рантайма (msn_run.c, правка 345)
    MsnEntity e; memset(&e, 0, sizeof e);
    if (!entity(m, rt->target, &e)) return 0;
    float dx = w.player.x - e.pos.x, dy = w.player.y - e.pos.y, dz = w.player.z - e.pos.z;
    return sqrtf(dx * dx + dy * dy + dz * dz) < msn_talk_reach_w(rt, rt->c_talk, &w);   // в комнате не меньше 2,6 м
  }
  if (rt->obj == MSN_OBJ_GOTO && (rt->st->flags & MSN_SF_MUSTACTION) && rt->c_dest) {   // точка «нажми действие»
    const MsnLoc *l = msn_arg_loc(&m->f, rt->c_dest, 0);
    return l && msn_loc_contains(&m->f, l, w.player, rt->radius_foot);
  }
  return 0;
}
static int sellerFirst96(const GameMsn *m) { return gameSeller96 && m->rt.status == MSN_RUNNING && m->rt.st && (m->rt.obj == MSN_OBJ_BUYCAR || m->rt.obj == MSN_OBJ_BUYSKIN); }   // этап покупки: продавец важнее бонус-персонажа
static int msnWants(GameMsn *m, const MsnWorld *wp) { return msnWantsX(m, wp, 1); }
// ---- P04 U1 / Д9, P07-9: луч тарелки L7 (UFO_BEAM_ALWAYS_ON). Числа: сфера 30 м вокруг тарелки, конус вниз — косинус угла к вертикали
// не меньше 0,94 (около 20°; на глубине 20 м радиус около 7 м), тянет к центру тарелки 3 м/с; ближе 5 м к центру машина убирается. Водитель при втягивании принудительно выходит
// на землю. Машина игрока с бочкой (rt->cargo, цель pickupitem) — тарелка ранена, цель destroyboss закрыта (MSN_IN_BOSS_HIT); без бочки машина взрывается, цель не
// закрывается. Игрока ставят на локатор Z3D1, только если на конце он ещё за рулём (круг 3). Машин трафика и миссий в луче порт не тянет — журнал g07, «нельзя».
// Правило порта: машину с бочкой в конце возвращаем на её место до луча (оригинал её убирает), чтобы игрок не остался без машины без нового сброса.
#define UFOB_R 30.f
#define UFOB_COS .94f
#define UFOB_LIFT 3.f
#define UFOB_END 5.f
static const MsnLoc *locByName(const GameMsn *m, const char *name) {
  for (uint32_t i = 0; i < m->f.nlocs; i++) if (msn_streq(msn_str(&m->f, m->f.locs[i].name), name)) return &m->f.locs[i];
  return 0;
}
static void ufoBeam(GameMsn *m, const MsnWorld *w, float dt) {
  void *u = m; Game *g = m->g; MsnRuntime *rt = &m->rt; Car *c = g->pCar;
  int active = rt->status == MSN_RUNNING && rt->obj == MSN_OBJ_DESTROYBOSS && !m->ufoSent && *g->pHaveCol;
  if (!active) { if (m->ufoGrab == 1) m->ufoGrab = 0; return; }
  float dx = c->x - m->ufoPos[0], dy = c->y - m->ufoPos[1], dz = c->z - m->ufoPos[2], d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (!m->ufoGrab) {
    if (d > UFOB_R || dy >= 0.f || -dy < UFOB_COS * d || c->destroyed) return;   // вне шара или конуса, разбитую не берёт
    m->ufoGrab = 1; m->ufoHome[0] = c->x; m->ufoHome[1] = c->y; m->ufoHome[2] = c->z; m->ufoHome[3] = c->h;
  }
  if (!w->on_foot && *g->pHaveHomer) {   // водитель принудительно выходит (на землю под машиной)
    float gy, ox = c->x, oz = c->z; ColWorld *cw = g->pWorld;
    if (!colGround(cw, ox, oz, c->y + 1.5f, c->y - 80.f, &gy, 0)) gy = c->y;
    homerPlace(g->pHomer, g->pHomerWorld ? g->pHomerWorld : cw, ox, gy, oz, c->h);
  }
  if (GAME_UFOCENTER_OLD) { c->y += UFOB_LIFT * dt; dy = c->y - m->ufoPos[1]; }
  else {   // круг 3 (проверка по исходнику): тянет по прямой К ЦЕНТРУ тарелки, а не вверх вдоль оси; конец — трёхмерное расстояние до центра меньше 5 м
    float k = d > 1e-3f ? UFOB_LIFT * dt / d : 0.f; if (k > 1.f) k = 1.f;
    c->x -= dx * k; c->y -= dy * k; c->z -= dz * k; dx = c->x - m->ufoPos[0]; dy = c->y - m->ufoPos[1]; dz = c->z - m->ufoPos[2];
  }
  c->vel[0] = c->vel[1] = c->vel[2] = 0.f;   // скорости обнуляются каждый кадр
  d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (d >= UFOB_END) return;
  m->ufoGrab = 2; m->ufoSent = 1;
  int barrel = rt->cargo != 0;
  const MsnLoc *z = locByName(m, "Z3D1");
  if (barrel) msn_notify(rt, MSN_IN_BOSS_HIT, NULL, 0);
  else { c->damage = 1.f; c->destroyed = 1; c->deadT = 0.f; }   // машина получает смертельный урон, цель не закрыта
  if (barrel && !GAME_UFOCENTER_OLD) { putCar(g, &(MsnPlace){ .pos = { m->ufoHome[0], m->ufoHome[1], m->ufoHome[2] }, .has_heading = 1, .heading = m->ufoHome[3] }); }   // правило порта: машина с бочкой «убрана» — стоит на прежнем месте
  if (z && *g->pHaveHomer && (GAME_UFOCENTER_OLD || !gameOnFoot(g))) {   // игрок «похищен» и ставится на локатор Z3D1 — только если на конце ещё за рулём
    MsnPlace pz; memset(&pz, 0, sizeof pz); pz.pos.x = z->pos[0]; pz.pos.y = z->pos[1]; pz.pos.z = z->pos[2]; pz.loc = z;
    if (GAME_UFOCENTER_OLD && barrel) { putCar(g, &(MsnPlace){ .pos = { z->pos[0] + 4.f, z->pos[1], z->pos[2] }, .has_heading = 0, .loc = z }); }
    putHomer(g, &pz); TELEPORTED();
  }
}
unsigned (*gameMsnClockUs)(void);   // служебное: часы микросекунд (main_loop.h, только в смоуке); NULL — подфазы не считаются
#define NOWUS_() (gameMsnClockUs ? gameMsnClockUs() : 0u)
float gameMsnPartMs[5];   // служебное: подфазы gameMsnTick (мс, сумма с прошлого сброса): 0 NPC, 1 доля гонки, 2 машины миссии, 3 этап (msn_game_tick, гонка, камера), 4 всё
// №1114: слот записи о миссии (savegame.h SV_MREC_N) и вызовы хука
void (*gameMsnRecordHook)(int level, int slot, int kind, int sec);
#ifndef SAVEV3_1114_OLD
#define SAVEV3_1114_OLD 0
#endif
int gameMsnBestSlot(const GameMsn *m, int cur) {
  if (cur < 0 || (unsigned)cur >= m->f.nmissions) return -1;
  unsigned fl = m->f.missions[cur].flags;
  if (fl & MSN_MF_GAMBLE) return 12;
  if (fl & MSN_MF_BONUSMISSION) return 11;
  if (fl & MSN_MF_STREETRACE) { int k = 0; for (int i = 0; i < cur; i++) if (m->f.missions[i].flags & MSN_MF_STREETRACE) k++; return 8 + (k < 2 ? k : 2); }
  if (fl & (MSN_MF_BONUS | MSN_MF_NOSCRIPT | MSN_MF_TRANSITION | MSN_MF_LEVELOVER | MSN_MF_GAMEOVER)) return -1;
  return cur < 8 ? cur : -1;
}
void gameMsnRecord(GameMsn *m, int kind) {
  if (SAVEV3_1114_OLD || !gameMsnRecordHook || !m->f.hdr) return;
  int slot = gameMsnBestSlot(m, m->rt.cur), lv = (int)m->f.hdr->level; if (slot < 0 || lv < 1 || lv > 7) return;
  gameMsnRecordHook(lv, slot, kind, kind ? (int)m->missionClock : 0);
}
void gameMsnTick(GameMsn *m, const GamePad *p, float dt) {
  if (!SAVEV3_1114_OLD && m->rt.status == MSN_RUNNING && m->rt.phase == 1 && !m->rt.paused && dt > 0.f) m->missionClock += dt;   // №1114: время основной части миссии
  unsigned pt0_ = NOWUS_(), pt1_ = pt0_, pt2_ = pt0_, pt3_ = pt0_;
  if (m->talk.on && (m->rt.status != MSN_RUNNING || m->rt.obj != MSN_OBJ_DIALOGUE)) talkEnd(m);
  if (m->talk.on && m->talk.npc[0] && *m->g->pHaveHomer) {   // собеседник смотрит на игрока весь разговор
    GameNpc *n = npcFind(m, m->talk.npc); const Homer *H = m->g->pHomer;
    if (n && n->knock) {   // правка 240: разговор начался, пока собеседник сбит, — стоит на земле там, где лежал
      float gy; if (*m->g->pHaveCol && colGround(m->g->pWorld, n->pos.x, n->pos.z, n->pos.y + 1.5f, n->pos.y - 4.f, &gy, 0)) n->pos.y = gy;
      n->knock = 0; n->homing = 0; }
    if (n) n->h = atan2f(H->x - n->pos.x, H->z - n->pos.z);
    if (m->talk.npc2[0]) { GameNpc *n2 = npcFind(m, m->talk.npc2); if (n2 && !n2->knock) n2->h = atan2f(H->x - n2->pos.x, H->z - n2->pos.z); }   // P15-14
  }
  MsnWorld w; gameMsnWorld(m, &w);
  if (!GAME_C5_OLD) {
    const MsnRuntime *r5 = &m->rt;
    // круг 5, P06-24 / P07-15: у оригинала второе нажатие в магазине «вызывает купленную машину» — этап buycar закрывается по текущей машине. У порта выбор машины — будка;
    // купленная машина ставится игроку здесь, один раз на этап; не вышло (машина миссии у игрока) — этап закрывается по владению, чтобы не повиснуть
    if (r5->status == MSN_RUNNING && r5->obj == MSN_OBJ_BUYCAR && r5->st) {
      const char *want = msn_str(&m->f, r5->st->obj_arg);
      if (shopOwns(&m->shop, SHOP_CAR, want) && !(w.vehicle && msn_streq(w.vehicle, want)) && !m->buyTried) { m->buyTried = 1; if (!gameMsnPhoneCar(m, want)) m->rt.buy_fallback = 1; }
    } else if (r5->obj != MSN_OBJ_BUYCAR) m->buyTried = 0;
    if (m->transition) gameHnrFreeze(m->g, .1f);   // СВЕРКА_С_ИСХОДНИКОМ 5.11: на входе и выходе из помещения шкала Hit & Run стоит (в разговоре — main_loop.h mvTalkActive)
  }
  if (!GAME_C2_UFOH_OLD && m->ufoOn && m->ufoPos[1] < 22.f) m->ufoPos[1] = fminf(22.f, m->ufoPos[1] + dt);   // круг 2, g07b P04:U2: тарелка не ниже Y = 22, ниже — поднимается на 1 м/с
  if (!GAME_C2_DMG_OLD) {   // круг 2, g07b Д6: dump с несколькими предметами — машина игрока неуязвима
    Car *pc_ = m->g->pCar; const MsnRuntime *r_ = &m->rt;
    if (r_->status == MSN_RUNNING && r_->obj == MSN_OBJ_DUMP && r_->nitems > 1) {
      if (m->dmgHoldOn && pc_->damage > m->dmgHold) { pc_->damage = m->dmgHold; if (pc_->damage < 1.f) pc_->destroyed = 0; }
      m->dmgHold = pc_->damage; m->dmgHoldOn = 1;
    } else m->dmgHoldOn = 0;
  }
  if (!HOMER_ROCKOUT_OLD && *m->g->pHaveHomer) {   // сверка g04-homer, AllowRockOut (P06:Д5): у идущего этапа есть команда «кайф» — Гомер за рулём после 3 с играет ndr_in_car_victory_small
    const MsnRuntime *rk_ = &m->rt; const MsnStage *sk_ = rk_->st; int on_ = 0;
    if (sk_ && rk_->status == MSN_RUNNING) for (uint32_t i_ = 0; i_ < sk_->cmd_count; i_++) if (m->f.cmds[sk_->cmd_first + i_].op == MSN_OP_ALLOWROCKOUT) { on_ = 1; break; }
    homerRockOut(m->g->pHomer, on_);
  }
  if (!GAME_C2_CAMGOAL_OLD && *m->g->pHaveHomer) {   // круг 2, g04 P08-Д1: этап «дойти» с рисуемым предметом — камера пешком притягивается к нему
    const MsnRuntime *r_ = &m->rt; const MsnLoc *l_ = (r_->status == MSN_RUNNING && r_->obj == MSN_OBJ_GOTO && r_->c_dest && w.on_foot && *msn_arg_s(&m->f, r_->c_dest, 1)) ? msn_arg_loc(&m->f, r_->c_dest, 0) : 0;
    if (l_) homerCamGoal(1, l_->pos[0], l_->pos[1], l_->pos[2]); else homerCamGoal(0, 0.f, 0.f, 0.f);
  }
  if (!gameBoss967Old && m->ufoOn) {   // №967: машина игрока (стоящая, если он вышел) в луче тарелки BOSS967_CHARGE с, игрок пешком -> цель destroyboss (MSN_IN_BOSS_HIT)
    int key_ = m->rt.cur * 256 + m->rt.stage + 1; if (m->rt.obj != MSN_OBJ_DESTROYBOSS || key_ != m->ufoKey) { m->ufoCharge = 0.f; m->ufoSent = 0; m->ufoGrab = 0; m->ufoKey = m->rt.obj == MSN_OBJ_DESTROYBOSS ? key_ : 0; }   // новый этап — заряд с нуля
    if (!GAME_UFOBEAM_OLD) ufoBeam(m, &w, dt);
    else if (m->rt.status == MSN_RUNNING && m->rt.obj == MSN_OBJ_DESTROYBOSS && !m->ufoSent && *m->g->pHaveCol) {
      float dx = m->g->pCar->x - m->ufoPos[0], dz = m->g->pCar->z - m->ufoPos[2];
      if (dx * dx + dz * dz < BOSS967_R * BOSS967_R) m->ufoCharge += dt; else m->ufoCharge = 0.f;
      if (m->ufoCharge >= BOSS967_CHARGE && w.on_foot) { m->ufoSent = 1; msn_notify(&m->rt, MSN_IN_BOSS_HIT, NULL, 0); }
    }
  }
  if (!GAME_TRAFFICFOOT_OLD && roadsT && roadsT->hd && m->trafficStage > 0) {   // P03 Д5: лимит этапа действует, пока игрок в машине; пешком набирается до полного (если предел не 0)
    int want = w.on_foot ? -1 : m->trafficStage; int def_ = roadsT->hd->nSlot < TR_MAXCAR ? (int)roadsT->hd->nSlot : TR_MAXCAR;
    if (roadsT->maxCars != (want < 0 ? def_ : want)) trafficSet(m, want);
  }
  npcCtx = m;
  for (int i = 0; i < m->nnpc; i++) npcStep(&m->npc[i], m->g, dt, npcTalkStop(m, &m->npc[i]));
  npcCtx = 0;   // В85: stop — только записанные случаи
  // Правка Роба 288 (04.10: «Барни ходит, но не он то, с чем взаимодействуешь, а место на асфальте — △ появляется, когда идёшь по
  // асфальту»): продавец идёт по маршруту AddPurchaseCarNPCWaypoint (barney_loc -> barney_walk1), а место разговора стояло на
  // локаторе — теперь место разговора там, где сам продавец (тот же радиус данных 1,3 м + SHOP_REACH). Правило порта.
  // «flag selleratloc 1» (gameSellerAtLoc) — прежнее (красный tests/npc/seller_follow.py)
  if (!gameSellerAtLoc) for (int i = 0; i < m->shop.nseller; i++) { const GameNpc *n = npcFindSeller(m, m->shop.seller[i].npc);
    if (n && !n->knock) { m->shop.seller[i].x = n->pos.x; m->shop.seller[i].y = n->pos.y; m->shop.seller[i].z = n->pos.z; } }
  npcKnock(m, dt);   // правка 240: удар машиной, полёт, лежит, встаёт
  pt1_ = NOWUS_();
  mcTrafficSees(m->cars.n ? &m->cars : 0, roadsT);   // трафик тормозит перед машинами миссии (нет машин — список пуст)
  if (*m->g->pHaveCol && m->cars.n) {   // машины миссии: после машины игрока и Гомера, до трафика (как main.c / боты)
    McEnv e; memset(&e, 0, sizeof e); e.w = m->g->pWorld; e.tr = roadsT; e.player = m->g->pCar; e.driving = !w.on_foot;
    e.ppos[0] = w.player.x; e.ppos[1] = w.player.y; e.ppos[2] = w.player.z; e.hold = m->rt.status == MSN_COUNTDOWN;
    // 03.10 (m7): догонялка соперников гонки — по доле пройденного пути гонки игрока (по длине, его точки); только уровень 1 (слово
    // Роба 03.10: уровни 2–7 потом), у остальных — прежняя мера по прямой
    if (m->f.hdr && m->f.hdr->level == 1) { float fr = gameMsnRaceFracLen(m); if (fr >= 0.f) { e.haveFrac = 1; e.pfrac = fr; } }
    {   // g03-aicars A10 (круг 2: поля msncar.h уже в общем коде): уличные гонки 1 и 2 не ждут игрока
      const MsnMission *mm_ = m->rt.cur >= 0 ? &m->f.missions[m->rt.cur] : 0;   // уличные гонки 1 и 2 не ждут игрока
      e.noRaceWait = mm_ && (mm_->flags & MSN_MF_STREETRACE) && (!strcasecmp(msn_str(&m->f, mm_->id), "sr1") || !strcasecmp(msn_str(&m->f, mm_->id), "sr2")); }
    for (int i_ = 0; i_ < m->cars.n; i_++) m->cars.c[i_].noPlace = m->rt.target && m->rt.obj == MSN_OBJ_DESTROY && !strcasecmp(m->cars.c[i_].name, m->rt.target);   // сверка g03-aicars круг 3: цель уничтожения не ставится на дорогу при застревании
    e.freeze = m->g->hnrFreeze > 0.f || (!GAME_C3_SAFE_OLD && m->safeIn);   // круг 2 (g03): полиция стоит в разговоре (g10 2.8); круг 3 (g07): машина игрока в безопасной зоне — полиция стоит
    e.carTouch = svCarTouch; e.touchU = m;   // сверка g03-aicars круг 2: машины ИИ задевают предметы (g11 P11-02); сведение: в ветке g07 круг 3 строка попала в комментарий, возвращена
    pt2_ = NOWUS_();
    McCar *dmgT_ = 0; float dmgB_ = 0.f; const MsnRuntime *rr_ = &m->rt;
    if (!GAME_C2_DMG_OLD && rr_->status == MSN_RUNNING && rr_->target && (rr_->obj == MSN_OBJ_DESTROY || rr_->obj == MSN_OBJ_FOLLOW || rr_->obj == MSN_OBJ_DUMP)) {
      dmgT_ = (McCar *)mcFind(&m->cars, rr_->target); if (dmgT_) dmgB_ = dmgT_->car.damage;
    }
    if (m->hitTgtT > 0.f) m->hitTgtT -= dt;
    if (!HNR_IMMUNE_OLD) for (int i_ = 0; i_ < m->cars.n; i_++) {   // круг 7 (g10-flow, P03-30; против оригинала): взрыв машины-цели destroy или dump без привязанных предметов (бамперные машины) не двигает шкалу Hit & Run и не даёт монет
      Car *k_ = &m->cars.c[i_].car; int imm_ = rr_->status == MSN_RUNNING && rr_->target && (rr_->obj == MSN_OBJ_DESTROY || (rr_->obj == MSN_OBJ_DUMP && rr_->nbind == 0)) && !strcasecmp(m->cars.c[i_].name, rr_->target);
      if (imm_ && k_->svKind == 2) k_->svKind = 5; else if (!imm_ && k_->svKind == 5) k_->svKind = 2; }
    mcUpdate(&m->cars, &e, dt, carHit, m);
    if (dmgT_ && dmgT_->car.damage > dmgB_) {   // круг 2 (g01 7.2, g07b Д6): цель «разбить» страдает только от машины игрока (окно 0,5 с после её удара); у follow и dump с несколькими предметами цель неуязвима
      int keep = rr_->obj == MSN_OBJ_FOLLOW || (rr_->obj == MSN_OBJ_DUMP && rr_->nitems > 1) || (rr_->obj == MSN_OBJ_DESTROY && m->hitTgtT <= 0.f);
      if (keep) { dmgT_->car.damage = dmgB_; if (dmgB_ < 1.f) dmgT_->car.destroyed = 0; }
    }
    for (int i_ = 0; i_ < m->cars.n; i_++) if (m->cars.c[i_].waitEvt > m->aiWaitSeen[i_ < 16 ? i_ : 15]) { m->aiArrived++; m->aiWaitSeen[i_ < 16 ? i_ : 15] = m->cars.c[i_].waitEvt; }   // «ИИ добрался до цели»: waitEvt растёт раз в 20 с ожидания
    pt3_ = NOWUS_();
  }
  w.action = (p->hit & GP_TRIANGLE) != 0;
  if (!GAME_C5_OLD) {   // круг 5, P05-25: кнопки разговора миссии не срабатывают, пока идёт арест; разговор с персонажем, занятым другим разговором, закрыт
    if (m->arrested) w.action = 0;
    if (m->rt.obj == MSN_OBJ_TALKTO && m->talk.on && m->rt.target && m->talk.npc[0] && !msn_streq(m->talk.npc, m->rt.target)) w.action = 0;
  }
  if (m->retalkT > 0.f) {   // В85: после Cancel разговора △ миссии не принимается 26 кадров PAL (game_msn.h GAME_TALK_RETALK)
    m->retalkT -= dt;
    if (m->retalkT > 1e-4f) w.action = 0; else m->retalkT = 0.f;
  }
  if (m->shop.open) { gameShopInput(m, p); w.action = 0; if (!w.on_foot) m->shop.open = 0; }   // лавка: кнопки ей, миссия ждёт
  else if (w.action && w.on_foot && !m->talk.on && gameMsnShopAllowed(m)) {   // △ у продавца (если миссии это нажатие не нужно для разговора/точки)
    int sv = shopSellerNear(&m->shop, w.player.x, w.player.y, w.player.z), other = 0;
    // правка Роба 306 (04.10: «в m2 Барни и лавка, и собеседник миссии — подхожу поговорить, а он только лавка»): пока этап миссии —
    // разговор с этим самым продавцом (TALK TO BARNEY), △ у него — разговору, лавка не перехватывает. «flag shopfirstold 1» — прежнее
    if (sv >= 0 && !gameShopFirstOld && m->rt.status == MSN_RUNNING && m->rt.st && m->rt.obj == MSN_OBJ_TALKTO && m->rt.target &&
        msn_streq(m->rt.target, m->shop.seller[sv].npc)) sv = -1;
    if (sv >= 0) other = msnWantsX(m, &w, !sellerFirst96(m));   // этап покупки: бонус-персонаж в радиусе не перехватывает △ у продавца
    if (sv >= 0 && !other) { m->shop.open = 1; m->shop.at = sv; m->shop.sel = 0; m->shopOpens++; w.action = 0;
      for (int k = 0, n = shopSellerCount(&m->shop, sv); k < n; k++)   // сразу на то, что нужно миссии (этап «купи Plow King»)
        if (m->rt.status == MSN_RUNNING && m->rt.st && (m->rt.obj == MSN_OBJ_BUYCAR || m->rt.obj == MSN_OBJ_BUYSKIN) &&
            msn_streq(m->shop.item[shopSellerItem(&m->shop, sv, k)].name, msn_str(&m->f, m->rt.st->obj_arg))) m->shop.sel = k; }
  }
  if (m->audio.b) {   // разговор доигран до конца последней реплики -> цель dialogue засчитана
    const MsnRuntime *rt = &m->rt;
    float left = rt->status == MSN_RUNNING && rt->timer_on && !rt->timer_up ? rt->remaining : -1.f;
    { // сверка g08-sound круг 3 (проверка P09:20, P06 Д4): реплика «осталось 10 секунд» — только на последнем этапе основной части миссии
      int lastSt = 0;
      if (rt->phase == 1 && rt->cur >= 0 && (uint32_t)rt->cur < m->f.nmissions) { const MsnMission *ms = &m->f.missions[rt->cur];
        if (ms->main != MSN_NONE16 && ms->main < m->f.nphases) lastSt = rt->stage + 1 >= m->f.phases[ms->main].stage_count; }
      if (!lastSt) left = -1.f; }
    if (msnAudioTick(&m->audio, dt, w.on_foot, left)) msn_notify(&m->rt, MSN_IN_DIALOGUE_DONE, 0, 0);
  }
  if (GAME_MSN_BREAKABLES) tagsReport(m);   // щиток сломан в этом кадре (стейтпропы игры) -> цель миссии
  msn_game_tick(&m->mg, dt, &w);
  raceTrack(m);   // правка 326: место в гонке этого кадра (после сбора точки игроком в этом кадре)
  scamEval(m);   // правка 40: фаза стартовала в этом кадре (этап 0 уже поставил игрока) — заявка камеры старта
  if (GAME_SAFE13_OLD) {
    if (m->nsafe && m->g->pHnrHeat && gameMsnInSafeZone(m, w.player.x, w.player.z)) {   // AddSafeZone: шкала Hit & Run гаснет
      *m->g->pHnrHeat = 0.f; if (m->g->pHnrCalm) *m->g->pHnrCalm = 0.f; m->safeFrames++;
    }
  } else {
    // P06-13 / P07 Д5: зона действует, только пока игрок в МАШИНЕ и машина (по плоскости) в зоне; вход останавливает ИИ погони, выход включает; шкала не обнуляется.
    // Модуль полиции ставит gameMsnSafeHook; пока его нет — запасной путь прежний (шкала гасится), но уже только для машины игрока
    int in = m->nsafe && !w.on_foot && gameMsnInSafeZone(m, m->g->pCar->x, m->g->pCar->z);
    if (in != m->safeIn) { m->safeIn = in; if (gameMsnSafeHook) gameMsnSafeHook(in); }
    if (in) m->safeFrames++;
    // круг 3, строка 15: шкалу не обнуляем ни при каком пути — полицию в зоне держит McEnv.freeze (см. mcUpdate выше); GAME_C3_SAFE_OLD=1 — прежний запасной путь
    if (GAME_C3_SAFE_OLD && in && !gameMsnSafeHook && m->g->pHnrHeat) { *m->g->pHnrHeat = 0.f; if (m->g->pHnrCalm) *m->g->pHnrCalm = 0.f; }
  }
  { unsigned pe_ = NOWUS_(); if (pt3_ == pt0_) pt3_ = pt2_ = pe_ - 0; gameMsnPartMs[0] += (pt1_ - pt0_) / 1000.f; gameMsnPartMs[1] += (pt2_ - pt1_) / 1000.f; gameMsnPartMs[2] += (pt3_ - pt2_) / 1000.f; gameMsnPartMs[3] += (pe_ - pt3_) / 1000.f; gameMsnPartMs[4] += (pe_ - pt0_) / 1000.f; }
}
