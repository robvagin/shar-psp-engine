// Логика кадра игры без рисования (см. game.h). Перенесено из viewer/main.c без изменений чисел и порядка:
// тела блоков те же, имена состояния — через макросы ниже (main.c держит свои static, боты — свою структуру мира).
#include <math.h>
#include <string.h>
#include "game.h"
#include "xsub.h"
#if !SHR_REL_SMK
unsigned xsubU[XS_N], xsubC[XS_N], xsubW[XS_N]; int xsubOn, xsubSkin, xsubCar, xsubFlag;
#endif
#include "carcoins.h"
#include "boltnear1159.h"
#include "sverka_shared.h"   // сверка g10-flow: svBoom (взрыв машины), общая точка с car.c

#define world (*g->pWorld)
#define car (*g->pCar)
#define props (*g->pProps)
#define sprops (*g->pSp)
#define coins (*g->pCoins)
#define homer (*g->pHomer)
#define haveCol (*g->pHaveCol)
#define haveProps (*g->pHaveProps)
#define haveSP (*g->pHaveSP)
#define haveCoins (*g->pHaveCoins)
#define haveHomer (*g->pHaveHomer)
#define hnrHeat (*g->pHnrHeat)
#define hnrCalm (*g->pHnrCalm)
#define wasDestroyed (*g->pWasDestroyed)
#define spSt (g->pSpSt)
#define spStInit (*g->pSpStInit)
#define LIFT (*g->pLift)

int gameOnFoot(const Game *g) { return haveHomer && !homerDriving(&homer); }

// сбитый DynaPhys с обломком BreakableObject: проиграть обломок в его исходной точке; 1 — у обломка есть меши, проп спрятать
// №923 (Роб 24.10: «бью вперёд, а разбивается влево»): осколки (частицы и куски обломка) летят по скорости машины в кадре удара. props.c даёт направление по скорости
// ПОСЛЕ шага: стена мира (стекло витрины, будка в COL1) к этому месту уже погасила или отбросила машину — направление вбок или назад. Берём скорость в начале шага
// (car.velPre); пешим ударом (Гомер, машина стоит) остаётся направление удара. BREAK_DIR923_OLD=1 — как было
#ifndef BREAK_DIR923_OLD
#define BREAK_DIR923_OLD 0
#endif
__attribute__((weak)) int sndIsCrate(int set, unsigned i) { (void)set; (void)i; return 0; }   // слабое определение: настоящее в sound.c, в ботах без звука — «не ящик»
static int gameIsCrate(void *u, unsigned i) { (void)u; return sndIsCrate(1 /* SND_SET_SPROPS */, i); }   // круг 3 (g11): ящик — по звуку удара предмета
static void gameRadiate(void *u, float x, float y, float z, float r, float force) { Game *g = u; if (haveProps) propsRadiate(&props, x, y, z, r, force); }   // сверка g11 (P16-21)
static int touchSeen(const Game *g, unsigned key) {
  int n = g->touchN < 16 ? g->touchN : 16;
  for (int k = 0; k < n; k++) if (g->touchKey[k] == key) return 1;
  return 0;
}
static void touchAdd(Game *g, unsigned key) { g->touchKey[g->touchHead] = key; g->touchHead = (g->touchHead + 1) & 15; if (g->touchN < 16) g->touchN++; }
int gameTouchNew(Game *g, unsigned key) { if (HNR_TOUCH16_OLD) return 1; if (touchSeen(g, key)) return 0; touchAdd(g, key); return 1; }
int gamePropKick(void *u, unsigned i, float dx, float dz) {
  Game *g = u;
  float pos[3] = {props.def[i].x, props.def[i].y, props.def[i].z};
  float v2[2] = {0.f, 0.f}, *pv = NULL;
  if (!BREAK_DIR923_OLD) {
    float l = hypotf(car.velPre[0], car.velPre[2]);
    if (l > 1.5f) { dx = car.velPre[0] / l; dz = car.velPre[2] / l; v2[0] = car.velPre[0]; v2[1] = car.velPre[2]; pv = v2; }
    else if (dx * dx + dz * dz > .25f) { v2[0] = dx; v2[1] = dz; pv = v2; }
  }
  if (propsKickOther) {   // круг 3 (g11): предмет сбила машина трафика или ИИ — звук и частицы по направлению удара, шкала и монета игроку не идут
    if (g->hk.propKick) g->hk.propKick(g->hk.u, i, pos, 8.f, dx, dz);
    if (!haveSP) return 0;
    int b2 = spBreakOf(&sprops, props.def[i].type); if (b2 < 0) return 0;
    float v2o[2] = {dx, dz};
    return spBreakDir(&sprops, (unsigned)b2, pos, props.def[i].q, dx * dx + dz * dz > .25f ? v2o : NULL);
  }
  if (g->hk.propKick) g->hk.propKick(g->hk.u, i, pos, fabsf(car.v), dx, dz);   // родной звук предмета, частицы ATC
  // P03-11 (сверка g10-flow): +7,5 и 1 монета только за новый предмет (не из последних 16 тронутых); живые изгороди при ударе машиной и стейтпропы
  // (нужен индекс удара в stateprops.c — передано g11) этим правилом не охвачены
  if (HNR_TOUCH16_OLD) gameHnrOffense(g, HNR_PROP);   // Hit & Run: сбитый предмет
  else if (touchSeen(g, i + 1u)) { /* повтор — ни шкалы, ни монеты */ }
  else if (gameHnrOffense(g, HNR_PROP)) { touchAdd(g, i + 1u); if (haveCoins) { coins.collected++; if (g->hk.coin) g->hk.coin(g->hk.u); } }   // записываем, только если засчитано
  if (!haveSP) return 0;
  int b = spBreakOf(&sprops, props.def[i].type); if (b < 0) return 0;
  int r = spBreakDir(&sprops, (unsigned)b, pos, props.def[i].q, pv);
#ifndef BREAK_FX_OLD
  // №419 (Роб 09.10: «стеклянная труба L5 F3-5 должна разбиться, а летает, как металлическая»): обломок без мешей, чьи частицы — сами
  // осколки предмета (данные BreakableObject 0x3001000: 18 «l02z4_glasshatter» — стеклянная игла l2z4/l5z4, 30/31/32
  // «pumpkin_bits1_ps*» — тыквы l7), заменяет предмет так же, как обломок с мешами: предмет прячется.
  if (!r) {
    unsigned bi = sprops.type[b].bindex;
#ifndef HYDRANT_SMASH_OLD
    // №588 (#10, №434): гидрант (bi == 3, вода гидранта) при ударе заменяется эффектом воды и прячется
    if (bi == 3) r = 1;
#endif
    if (bi == 18 || bi == 30 || bi == 31 || bi == 32) r = 1;
  }
#endif
  return r;
}

#define GAME_EXIT_V 5.f   // м/с: медленнее — Гомер выходит сразу (как было)
#ifndef GAME_EXIT_ASSIST
#define GAME_EXIT_ASSIST 1   // правка 168; 0 — прежнее: на ходу △ ничего не делает (красный tests/controls/exit.sh)
#endif
// Сверка g10-flow (02.11.2026, СВЕРКА_ГОМЕР 5.4 и P05 21): выход из машины как в оригинале. △ в машине: стоит (< 3 км/ч, 0,83 м/с) — Гомер выходит
// сразу; медленнее 30 км/ч (8,33 м/с) — выход «зафиксирован» (exitReq 1): машина сама тормозит РУЧНИКОМ (газ обнуляется каждый кадр, ножной
// тормоз игрока не трогается), Гомер выходит, когда скорость упала ниже 3 км/ч; быстрее 30 км/ч — △ надо ДЕРЖАТЬ (exitReq 2): тормозит
// ручником, как только скорость ниже 30 км/ч и машина на колёсах — выход зафиксирован; отпустил — отмена. Газ игрока отменяет выход в любой момент.
// Было (правка 168 + №927): выше 5 м/с △ включала тормоз ножным тормозом, повторное △ отменяло, выход при 5 м/с (18 км/ч).
// Не сверено (в отчёте нет числа): «во время ожидания сброса Hit & Run △ игнорируется» — условия сброса в отчёте не описаны.
// Страховка порта: если ручник не остановил машину за GAME_EXIT_BRAKE_T секунд, добавляется ножной тормоз (иначе игрок мог бы катиться бесконечно).
// GAME_EXIT_PS2_OLD=1 — как было
#ifndef GAME_EXIT_PS2_OLD
#define GAME_EXIT_PS2_OLD 0
#endif
#define GAME_EXIT_HOLD_V (30.f / 3.6f)   // выше — △ держать
#define GAME_EXIT_STOP_V (3.f / 3.6f)    // ниже — Гомер выходит
#define GAME_EXIT_BRAKE_T 2.5f           // правило порта: через столько секунд ручника добавляется ножной тормоз
static float lastDt = 1.f / 60.f;      // шаг прошлого кадра машины: gameExitAssist стоит до gameCarStep
static float exitT;                      // сколько секунд длится заказанный выход
static int triHeld;                      // △ зажата в этом кадре (gameCarInput записывает; режим «держать выше 30 км/ч»)
void gameTriangle(Game *g) {   // △ — выйти / сесть, в том числе с зажатым тормозом
  HomerCarCb cb = {g->hk.door, 0, g->hk.u};
  HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &car, LIFT);   // нули: неиспользуемые контакты кузова не несут мусор стека (сверка побайтно)
  // выходит на малой скорости; правка 168 (Роб 01.10: «еду, жму △ — должен притормозить и выйти, а ничего не работает»):
  // на ходу △ заказывает выход — машина тормозит сама (gameExitAssist), на малой скорости Гомер выходит; △ ещё раз — отмена
  if (homerDriving(&homer)) {
    if (GAME_EXIT_PS2_OLD) { if (fabsf(car.v) < GAME_EXIT_V) { g->exitReq = 0; homerExitCar(&homer, &hc, &cb); } else if (GAME_EXIT_ASSIST) g->exitReq = !g->exitReq; }
    else {
      float av = fabsf(car.v);
      if (av < GAME_EXIT_STOP_V) { g->exitReq = 0; homerExitCar(&homer, &hc, &cb); }
      else { if (!g->exitReq) exitT = 0.f; g->exitReq = av < GAME_EXIT_HOLD_V ? (g->exitReq == 2 ? 2 : 1) : (g->exitReq == 1 ? 1 : 2); if (g->exitReq == 2) triHeld = 1; }   // △ в этом кадре нажата
    }
  }
  else if (homerCanEnter(&homer, &hc)) homerEnterCar(&homer, &hc, &cb);
}

#ifndef RESET_ROAD_OLD
#define RESET_ROAD_OLD 0   // 1: RESET CAR возвращает в последнее безопасное место (карман), как до 11.10
#endif
int gameResetRoadOld = RESET_ROAD_OLD;
int (*gameResetRoad)(const Car *c, float out[4]);   // game_msn.c: ближняя полоса по ходу (mcResetPoint); NULL — дорог нет
void gameReset(Game *g) {   // RESET CAR (пауза) / START без ui.shr
  float rp[4];
  if (!gameResetRoadOld && gameResetRoad && gameResetRoad(&car, rp)) { carPlace(&car, &world, rp[0], rp[1] + .5f, rp[2], rp[3]); car.safeT = car.t; car.nsafe = 0; }   // RESET CAR по дорогам (свод ботов п. 5)
  else carResetNearby(&car, &world);
  // RESET CAR остов не чинит (29.09, пункт 4): в оригинале разбитую машину чинят гаечный ключ или телефонная будка — так
  // говорит сообщение библии «VEHICLE DESTROYED! FIND A WRENCH, OR GO TO A PHONEBOOTH, TO REPAIR YOUR VEHICLE»; до ключей
  // здесь стояла временная починка. Без ключей в данных уровня (старый level.shr) — прежняя временная починка
  if (car.destroyed && !world.nwrench) { car.damage = 0.f; car.destroyed = 0; wasDestroyed = 0; }
  if (haveHomer) {
    int wasOnFoot = gameOnFoot(g); HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &car, LIFT);   // нули: неиспользуемые контакты кузова не несут мусор стека (сверка побайтно)
    homerSetInCar(&homer, &hc); if (g->hk.door) g->hk.door(g->hk.u, 0.f);
    if (g->hk.reset) g->hk.reset(g->hk.u, wasOnFoot);
  }
}

// Правка Роба 86: зоны DEATH оригинала (Event 4: овраг, канал под мостом r7, горящие покрышки у свалки — R6D01): попал машиной или
// пешком — вернуть в точку локатора (у дороги), машина целой по состоянию (урон как был), скорость 0. Перенесено из main.c
// deathCheck (01.10, Г4): та же проверка в мире ботов (tools/bots/world.c) — без неё ночь считала провалом то, что в игре
// возвращает зона (мост l1r7, 76 тыс. случаев за ночь; оракул PS2 f01/f19: «падает, зона DEATH R7D04/R7D06 ставит в …»)
// Правка Роба 349 (05.10, L1 K2-8: «вся территория за забором рядом с горящими покрышками — зона, куда попасть нельзя»): у
// локатора DEATH бывает несколько объёмов — R6D01 двумя ящиками накрывает весь двор покрышек (x 262…312, z −543…−488), до 349
// брался только первый (x 284…312): во двор у K2-8 можно было въехать и стоять. [PS2] 05.10 (pcsx2-twins, R6D01 оба ящика, Z6D1,
// Гомер пешком): машина или Гомер в любом объёме — мир стоит, экран гаснет, перенос в точку локатора, курс машины — курс
// локатора (R6D01 2,672 → 0,000, Z6D1 0,852 → 0,785). Здесь — правило данных (все объёмы, курс точки); показ (затемнение) —
// main.c deathCheck. DEATH_OLD=1 / gameDeathOld(1) — как до 349: только первый объём локатора, курс прежний (красный контроль)
#ifndef DEATH_OLD
#define DEATH_OLD 0
#endif
static int deathOld = DEATH_OLD;
void gameDeathOld(int on) { deathOld = on; }
int gameDeathIsOld(void) { return deathOld; }
#ifndef DEATH_R6_VOLUME_OLD
#define DEATH_R6_VOLUME_OLD 0
#endif
#ifndef DEATH_R6_RAMP_EXEMPT_OLD
#define DEATH_R6_RAMP_EXEMPT_OLD 0
#endif
// №432: ramp018 uses the authored L1 jumpzone9. Only a flight that actually
// departed this zone keeps its chance; placement and any ground contact end it.
static int r6RampFlight(Game *g) {
  int j = car.flightZone;
  return !car.grounded && !car.bodyHit && j >= 0 && (unsigned)j < world.nzone &&
    !strcmp(world.zone[j].name, "jumpzone9") &&
    !(car.contacts && fabsf(car.v) < 1.f && fabsf(car.vy) < 1.f);
}
static int r6RampClimb(Game *g) {
  if (!car.grounded || !car.wheels || car.vy <= .5f || fabsf(car.v) <= 3.f) return 0;
  for (unsigned j = 0; j < world.nzone; j++)
    if (!strcmp(world.zone[j].name, "jumpzone9") &&
        colZoneContains(world.zone + j, car.x, car.y + .5f, car.z, car.p.halfL)) return 1;
  return 0;
}
// Rob 09.10: the air over the tyre yard and its fence is DEATH too.
// The disc's two tilted R6D01 boxes are intact. Extrude their XZ projection
// upward; include a car body's reach outside the thin western fence, above
// that fence's actual top plane. Walking and the authored return stay intact.
static int r6AirDeath(Game *g) {
  float reach = 2.f * (car.p.halfL + car.p.halfW), roof = car.y + car.p.bodyHi + car.modelRaise;
  for (unsigned i = 0; i < world.ndeath; i++) {
    const ColPad *d = world.death + i;
    if (strcmp(d->name, "R6D01") || !d->shape) continue;
    const float *a[3] = {d->ax,d->ay,d->az}; float ex=0,ez=0;
    for(int k=0;k<3;k++){ex+=fabsf(a[k][0])*d->h[k];ez+=fabsf(a[k][2])*d->h[k];}
    if(fabsf(car.z-d->c[2])>ez+.5f || car.x>d->c[0]+ex+.5f || car.x<d->c[0]-ex-reach-.5f)continue;
    if(car.x>=d->c[0]-ex-.5f && roof>=d->c[1])return (int)i;
    const ColBox *fence=0;float best=reach+.5f;
    for(unsigned j=0;j<world.nbox;j++){
      const ColBox *b=world.box+j;const float *ax[3]={b->ax,b->ay,b->az};float bx=0,bz=0;
      for(int k=0;k<3;k++){bx+=fabsf(ax[k][0])*b->h[k];bz+=fabsf(ax[k][2])*b->h[k];}
      if(bx>car.p.halfW || bz<ez*.5f || fabsf(b->c[2]-d->c[2])>ez)continue;
      float dist=fabsf(b->c[0]-(d->c[0]-ex));if(dist<best){best=dist;fence=b;}
    }
    if(!fence || car.x<fence->c[0]-reach-.5f)continue;
    const float *ax[3]={fence->ax,fence->ay,fence->az};int up=0;
    for(int k=1;k<3;k++)if(fabsf(ax[k][1])>fabsf(ax[up][1]))up=k;
    if(fabsf(ax[up][1])<COL_WALK_NY)continue;
    float top=fence->c[1]+fence->h[up]/fabsf(ax[up][1])
      -(ax[up][0]*(car.x-fence->c[0])+ax[up][2]*(car.z-fence->c[2]))/ax[up][1];
    if(roof>=top)return (int)i;
  }
  return -1;
}
int gameDeathAt(Game *g) {
  if (!haveCol || !world.ndeath) return -1;
  int foot = gameOnFoot(g); float x = foot ? homer.x : car.x, y = (foot ? homer.y : car.y) + .5f, z = foot ? homer.z : car.z;
  int k=colDeathAtEx(&world, x, y, z, .5f, deathOld);
  // The fence/body extrusion can already overlap the rising ramp before
  // takeoff. Keep that approach, but never waive an authored box on the ground.
  if(k<0 && !foot && !deathOld && !DEATH_R6_VOLUME_OLD &&
      (DEATH_R6_RAMP_EXEMPT_OLD || !r6RampClimb(g)))k=r6AirDeath(g);
  if(k>=0 && !foot && !deathOld && !DEATH_R6_RAMP_EXEMPT_OLD &&
      !strcmp(world.death[k].name, "R6D01") && r6RampFlight(g))k=-1;
  return k;
}
// ДОБИВКА 18 (правка 349, [PS2] В86 — knowledge/facts/ps2-r6-foot-death.yaml, запись ps2-w build/twins/o4/
// ps2_v86_r6_foot_death_physics): Гомер пешком в R6D01 — перенос на кадре 606 в точку локатора, курс после переноса 0° (лицом по
// +Z диска = −Z порта; до переноса 73°) — это курс локатора (LocatorMatrix единичная → курс движка π, как у машины); активной
// остаётся WalkerCam, глаз переставлен сразу (кадр 608) с лица Гомера — homer.c homerCamDeathPose. Было: курс прежний, камера
// за спиной по прежнему курсу. DEATH_FOOT_OLD=1 / gameDeathFootOld(1) — как было (красный контроль tests/death/r6_host.c)
#ifndef DEATH_FOOT_OLD
#define DEATH_FOOT_OLD 0
#endif
static int deathFootOld = DEATH_FOOT_OLD;
void gameDeathFootOld(int on) { deathFootOld = on; }
void gameDeathPut(Game *g, int k) {
  if (k < 0 || (uint32_t)k >= world.ndeath) return;
  const float *A = world.death[k].apex; float h = car.h;
  if (gameOnFoot(g)) {
    float hh = homer.h; if (!deathFootOld) colDeathHeading(&world, (uint32_t)k, &hh);   // свой переключатель (не DEATH_OLD)
    homerPlace(&homer, &world, A[0], A[1], A[2], hh);
    if (!deathFootOld) homerCamDeathPose(&homer);
  }
  else { if (!deathOld) colDeathHeading(&world, (uint32_t)k, &h); carPlace(&car, &world, A[0], A[1], A[2], h); }
}
int gameDeath(Game *g) {   // мир ботов: проверка и перенос сразу (затемнения там нет)
  int k = gameDeathAt(g); if (k >= 0) gameDeathPut(g, k);
  return k;
}

#ifndef PAD_BOTH_OLD
#define PAD_BOTH_OLD 0
#endif
void gameCarInput(const Game *g, const GamePad *p, CarInput *in) {
  (void)g;
  float st = p->ax;
  // P17-11 (сверка g10-flow): цифровая стрелка даёт ровно ±1 и перекрывает аналог; при обеих стрелках побеждает ЛЕВАЯ (значения равны,
  // оригинал берёт первую). Было: побеждала правая. PAD_BOTH_OLD=1 — как было
  if (PAD_BOTH_OLD) { if (p->buttons & GP_LEFT) st = -1; if (p->buttons & GP_RIGHT) st = 1; }
  else { if (p->buttons & GP_RIGHT) st = 1; if (p->buttons & GP_LEFT) st = -1; }
  // раскладка оригинала (frontend.p3d ControllerPS2.pag, в машине): X ACCELERATE, ○ BRAKE / REVERSE, □ E-BRAKE,
  // △ GET OUT OF CAR, SELECT RESET CAR (main.c). У PSP нет правого стика и L2/R2: R — газ, L — тормоз (Роб 27.09)
  triHeld = (p->buttons & GP_TRIANGLE) != 0;   // сверка g10-flow: режим «держать △ выше 30 км/ч» (gameExitAssist)
  CarInput i0 = { (p->buttons & (GP_R | GP_CROSS)) ? 1.f : 0.f,   // Роб 27.09: R — газ (X тоже)
                  (p->buttons & (GP_L | GP_CIRCLE)) ? 1.f : 0.f, st };   // L и ○ — тормоз / назад (правка Роба 42: ○ как в оригинале)
  i0.handbrake = (p->buttons & GP_SQUARE) ? 1.f : 0.f;              // □ — ручник (занос)
  *in = i0;
}

#ifndef CARDEAD_STEER_OLD
#define CARDEAD_STEER_OLD 0
#endif
void gameCarLimits(const Game *g, CarInput *in) {
  // P01-17 (сверка g10-flow, круг 7 против оригинала): после смерти в ноль только газ и тормоз (газ и тормоз уничтоженной машины принимаются как ноль); руль живой (в круге 1 по ошибке обнулялся). Ручник 1 — пустой водитель, водителя выбрасывает.
  // CARDEAD_STEER_OLD=1 — руль тоже в ноль (прежняя ошибочная правка круга 1)
  if (car.destroyed && !carHuskDrives(&car)) { in->throttle = 0.f; in->brake = 0.f; in->handbrake = 1.f; if (CARDEAD_STEER_OLD) in->steer = 0.f; }   // разбита и горит — не едет; остов после взрыва едет медленно (правка 355); тормоз стоя = задний ход, поэтому ручник
  if (gameOnFoot(g)) { in->throttle = in->brake = in->steer = 0.f; in->handbrake = 1.f; }   // этап 4: водителя нет — стоит на ручнике
}

// №927 (Роб 24.10: «когда сдаю назад и жму △, нет правила, что машина тормозит, чтобы выйти»): тормоз гасит только ход вперёд (car.c: brake при движении назад — это газ назад,
// «brk > 0 && along <= .15» — тяга назад), поэтому на заднем ходу выход ждал самого наката, пока скорость не упадёт ниже GAME_EXIT_V. Задний ход гасится газом вперёд
// (тяга против хода; выход по |v| < GAME_EXIT_V — раньше, чем скорость сменит знак). GAME_EXIT_REV_OLD=1 — как было
#ifndef GAME_EXIT_REV_OLD
#define GAME_EXIT_REV_OLD 0
#endif
void gameExitAssist(Game *g, CarInput *in) {
  if (!g->exitReq) return;
  if (!homerDriving(&homer) || (car.destroyed && !carHuskDrives(&car))) { g->exitReq = 0; return; }
  if (!GAME_EXIT_PS2_OLD) {   // сверка g10-flow: ручник, газ в ноль каждый кадр, ножной тормоз игрока не трогаем; газ игрока — отмена
    if (in->throttle > .1f) { g->exitReq = 0; return; }
    float av = fabsf(car.v);
    if (g->exitReq == 2) {
      if (!triHeld) { g->exitReq = 0; return; }   // отпустил △ — отмена
      if (av < GAME_EXIT_HOLD_V && car.grounded) g->exitReq = 1;   // замедлилась и на колёсах — выход зафиксирован
    }
    exitT += lastDt;   // шаг прошлого кадра (gameCarStep)
    in->throttle = 0.f; in->handbrake = 1.f;
    if (exitT > GAME_EXIT_BRAKE_T) in->brake = 1.f;
    if (g->exitReq == 1 && av < GAME_EXIT_STOP_V) { exitT = 0.f; gameTriangle(g); g->exitReq = 0; }
    return;
  }
  in->throttle = (!GAME_EXIT_REV_OLD && car.v < -.5f) ? 1.f : 0.f; in->brake = car.v > .5f ? 1.f : 0.f; in->handbrake = fabsf(car.v) < 3.f ? 1.f : 0.f;   // стоя тормоз = назад, поэтому ручник
  if (fabsf(car.v) < GAME_EXIT_V) gameTriangle(g);
}

static CarInput drvIn; static float drvDmg;   // правки 220/231: вход машины этого кадра — для жестов водителя (homerDriveInput)
// круг 2 (g01 P01 Д4): у машины игрока, из которой вышел водитель, допуски покоя «пустой» (скорость < 0,5 м/с, вращение < 3 рад/с): car.c считает её пустой, пока huskDrive = 0
// (ручник 1, газ и руль 0 ставит gameCarLimits). SV_EMPTYPLAYER_OLD=1 — как было (всегда 1)
#ifndef SV_EMPTYPLAYER_OLD
#define SV_EMPTYPLAYER_OLD 0
#endif
void gameCarStep(Game *g, const CarInput *in, float dt) { if (dt > 0.f) lastDt = dt; drvIn = *in; car.huskDrive = (SV_EMPTYPLAYER_OLD || !gameOnFoot(g)) ? 1 : 0; carUpdate(&car, &world, in, dt); }   // правка 355: остов игрока едет

static int coinGround(void *u, float x, float z, float yTop, float *gy) {   // правка 81: опора под монетой (до 6 м ниже)
  return colGround((const ColWorld *)u, x, z, yTop, yTop - 6.f, gy, 0);
}
#ifndef WASP5_OLD
#define WASP5_OLD 0
#endif
#ifndef KICK_FIRST_OLD
#define KICK_FIRST_OLD 0       // круг 2 (g02 3.1:2): 1 — пинок задевает все предметы PRP1 в сфере
#endif
#ifndef ROOM_NOJUMP_OLD
#define ROOM_NOJUMP_OLD 0
#endif
#ifndef FOOT_DPAD_FULL_OLD
#define FOOT_DPAD_FULL_OLD 0   // P17-12 / P02-5: 1 — крестовина перекрывает аналог по каждой оси отдельно, как было
#endif
#ifndef SP_FOOT_EV_OLD
#define SP_FOOT_EV_OLD 0       // круг 5 (P11-04, P16-20): 1 — стомп слал только STOMP
#endif
#ifndef SP_HIT_COL_OLD
#define SP_HIT_COL_OLD 0       // P11-26: 1 — стейтпроп принимает удар и без включённой коллизии
#endif
#ifndef PROP_KICK400_OLD
#define PROP_KICK400_OLD 0     // P11-11: 1 — прежняя сила пинка 6·100/(100+масса) вбок
#endif
#ifndef KICKWAVE_AIR_OLD
#define KICKWAVE_AIR_OLD 0     // P02-19: 1 — волна удара и с земли
#endif
// сверка g11b-collect круг 7 (P04 W5, удар снаряда в оригинале): снаряд осы бьёт подвижные предметы — стейтпропы (кроме ящиков и торгоматов, у них объём едет за суставом) толчком силой 400
// (событие HIT и spImpulse, как пинок) и пропы PRP1 (скорость 400 / масса под 45° по полёту). По отрезку полёта a -> b: -> доля пути до первого (больше 1 — нет); apply = 1 — удар. Игрока и ос не касается
unsigned gameBoltStat[4];   // №1159: счётчики обхода снаряда за прогон (см. boltnear1159.h)
float gameBoltProps(Game *g, const float a[3], const float b[3], const float dir[3], int apply) {
  (void)g;
  return boltScan(haveSP ? &sprops : 0, haveProps ? &props : 0, a, b, dir, apply, gameBoltStat);   // №1159: тело и отсев — в boltnear1159.h (BOLTNEAR1159_OLD=1 — полный обход)
}
void gameAfterCar(Game *g, const GamePad *p, float dt) {
  unsigned xt_ = XS_T(), xw_ = XS_W();
  carBoomCoins(&car, haveCoins ? &coins : NULL, haveCol ? &world : NULL, &g->coinBoomArmed); XS_LAP(4, xt_, xw_);
  if (haveHomer) {   // этап 4: Гомер — после carVisUpdate (дверь поверх шарниров), до пропов/монет
    HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &car, LIFT);   // нули: неиспользуемые контакты кузова не несут мусор стека (сверка побайтно)
    // разбита: Гомер вылетает (homerEject, правка 147: через ~0,75 с, до взрыва). Правка 355: в остов после взрыва сесть можно —
    // выброс только до взрыва (прежде — каждый кадр, пока разбита: севшего в остов выбрасывало сразу)
    if (car.destroyed && !g->noEject && (CAR_HUSK_OLD || !carBoomed(&car)))
      homerEject(&homer, g->pHomerWorld ? g->pHomerWorld : &world, &hc, car.x, car.z, HOMER_EJECT_POWER);
    HomerInput hi; memset(&hi, 0, sizeof hi); hi.camYaw = homerCtrlOn ? homerCtrlYaw : homer.camYaw;
    if (gameOnFoot(g)) {
      hi.mx = p->ax; hi.my = -p->ay;
      // P17-12 / СВЕРКА_ГОМЕР 1.7 (сверка g10-flow): пешком крестовина имеет ПОЛНЫЙ приоритет — нажата любая её сторона: обе оси берутся с
      // крестовины (аналог не учитывается совсем; сравнения «чей модуль больше» в оригинале на PS2 нет). Было: приоритет по каждой оси отдельно
      // круг 7 (против оригинала): крестовина берётся, только если её сумма не нулевая: ← и → вместе гасят друг друга (x = 0), ↑ и ↓ тоже; когда обе оси крестовины в нуле — идёт аналог.
      // Было (круг 1): любая сторона крестовины забирала обе оси, ← + → давали →. FOOT_DPAD_FULL_OLD=1 — так
      { int dx_ = ((p->buttons & GP_RIGHT) != 0) - ((p->buttons & GP_LEFT) != 0), dz_ = ((p->buttons & GP_UP) != 0) - ((p->buttons & GP_DOWN) != 0);
        if (FOOT_DPAD_FULL_OLD) {
          if (p->buttons & GP_LEFT) hi.mx = -1.f; if (p->buttons & GP_RIGHT) hi.mx = 1.f;
          if (p->buttons & GP_UP) hi.my = 1.f; if (p->buttons & GP_DOWN) hi.my = -1.f;
        } else if (dx_ || dz_) { hi.mx = (float)dx_; hi.my = (float)dz_; } }
      // раскладка оригинала пешком (frontend.p3d ControllerPS2.pag): X JUMP, □ ATTACK, ○ DASH / SPRINT (зажат — бег;
      // srr2: «PRESS AND HOLD ○ TO RUN»), △ GET IN CAR / TALK TO CHARACTER (main.c). Стомп — X X □ (homer.c)
      hi.jump = (p->hit & GP_CROSS) != 0; hi.attack = (p->hit & GP_SQUARE) != 0; hi.dash = (p->buttons & GP_CIRCLE) != 0;
      // СВЕРКА_ГОМЕР 1.6 / P02-6 (сверка g10-flow): внутри интерьеров бег и прыжок отключены (○ и ✕ ничего не делают), удар играет клип, но по
      // предметам не бьёт. Комната = pHomerWorld (коллизия комнаты). ROOM_NOJUMP_OLD=1 — как было
      if (!ROOM_NOJUMP_OLD && g->pHomerWorld) { hi.jump = 0; hi.dash = 0; }
      if (g->footPadOld) { hi.dash = 1; hi.stomp = (p->hit & GP_CIRCLE) != 0; }   // отрицательный контроль: прежняя раскладка порта
    }
    if (homerDriving(&homer)) homerDriveInput(&homer, drvIn.steer, drvIn.throttle, drvIn.brake, car.v, car.damage - drvDmg > .02f);
    drvDmg = car.damage; memset(&drvIn, 0, sizeof drvIn);
    XS_LAP(13, xt_, xw_);
    homerSetIndoor(&homer, g->pHomerWorld != NULL);   // сверка g04-homer P02-6: в комнате (мир интерьера) бег и прыжок отключены, удар не бьёт по предметам
    homerUpdate(&homer, g->pHomerWorld ? g->pHomerWorld : &world, &hc, &hi, dt); XS_LAP(5, xt_, xw_);
    if (g->hk.afterHomer) g->hk.afterHomer(g->hk.u, &hc); XS_LAP(6, xt_, xw_);
    HomerEvent e;
    while (homerPopEvent(&homer, &e)) {
      if ((e.type == HOMER_EV_HIT || e.type == HOMER_EV_STOMP) && (ROOM_NOJUMP_OLD || !g->pHomerWorld)) {   // в комнате удар по предметам не бьёт (P02-6)
        if (haveSP) for (unsigned i = 0; i < sprops.ninst; i++) {          // стейтпропы: ящик/автомат/будка/куст...
          if (!sprops.run[i].alive) continue;
          if (!SP_HIT_COL_OLD && !sprops.run[i].colOn && !sprops.run[i].col2On) continue;   // P11-26: без включённой коллизии удар не принимается
          float c[3], r; spBounds(&sprops, i, c, &r);
          if (homerHitTest(&e, c[0], c[1], c[2], fminf(r, 1.f))) {
#if !SP_FOOT_EV_OLD
            // сверка g11b-collect круг 5 (P11-04, P16-20): сила 400 (пинок) = только HIT; сила 800 (стомп) = подряд STOMP, DESTROYED, HIT
            // (каждое событие идёт в состояние, в которое предмет попал от предыдущего). Шкала Hit & Run растёт один раз за удар
            if (e.type == HOMER_EV_HIT) spEvent(&sprops, i, SP_EV_HIT);
            else {
              unsigned h0 = sprops.hnrHits, hn0 = sprops.hitInstN; int any = spEvent(&sprops, i, SP_EV_STOMP);
              any |= spEvent(&sprops, i, SP_EV_DESTROYED); any |= spEvent(&sprops, i, SP_EV_HIT);
              if (any && sprops.hnrHits > h0 + 1) { sprops.hnrHits = h0 + 1; if (sprops.hitInstN > hn0 + 1) sprops.hitInstN = hn0 + 1; }
            }
#else
            spEvent(&sprops, i, e.type == HOMER_EV_HIT ? SP_EV_HIT : SP_EV_STOMP);
#endif
            spImpulse(&sprops, i, e.type == HOMER_EV_HIT ? e.dx : c[0] - e.x, e.type == HOMER_EV_HIT ? e.dz : c[2] - e.z, e.type == HOMER_EV_HIT ? 400.f : 800.f);   // сверка g11 P11-Д1: пинок 400, стомп 800
          }
        }
        int kickOnly = -1; float kickD = 1e30f;
        if (!KICK_FIRST_OLD && e.type == HOMER_EV_HIT && haveProps) for (unsigned i = 0; i < props.ninst; i++) {   // круг 2 (g02 3.1:2): пинок задевает ОДИН предмет — ближайший к удару
          PropState *st = &props.st[i]; PropType *ty = &props.types[props.def[i].type];
          if (st->hidden || (PROP_KICK400_OLD && ty->mass <= 0.f)) continue;   // круг 3: масса 0 — по умолчанию 100 (4 м/с), предмет пинается
          float point[3] = {e.x, e.y, e.z}, closest[3]; propsClosestPoint(&props, i, point, closest);
          if (!homerHitTest(&e, closest[0], closest[1], closest[2], 0.f)) continue;
          float d2 = (closest[0] - e.x) * (closest[0] - e.x) + (closest[1] - e.y) * (closest[1] - e.y) + (closest[2] - e.z) * (closest[2] - e.z);
          if (d2 < kickD) { kickD = d2; kickOnly = (int)i; }
        }
        if (haveProps) for (unsigned i = 0; i < props.ninst; i++) {        // сбиваемые пропы PRP1: пинок как лёгкий удар машиной
          PropState *st = &props.st[i]; PropType *ty = &props.types[props.def[i].type];
          if (st->hidden || (PROP_KICK400_OLD && ty->mass <= 0.f)) continue;   // круг 3: масса 0 — по умолчанию 100 (4 м/с), предмет пинается
          if (!KICK_FIRST_OLD && e.type == HOMER_EV_HIT && (int)i != kickOnly) continue;
          // №389: удар у основания высокого дерева/столба должен попадать в его объём.
          // Прежняя сфера у центра кроны не достигалась с земли; коробка уже есть у PropState.
          float point[3] = {e.x, e.y, e.z}, closest[3]; propsClosestPoint(&props, i, point, closest);
          if (!homerHitTest(&e, closest[0], closest[1], closest[2], 0.f)) continue;
          float dx = e.dx, dz = e.dz; if (e.type == HOMER_EV_STOMP) { dx = st->x - e.x; dz = st->z - e.z; float l = sqrtf(dx * dx + dz * dz) + 1e-3f; dx /= l; dz /= l; }
          if (gamePropKick(g, i, dx, dz)) { st->hidden = 1; st->asleep = 1; continue; }   // обломок (дерево, знак, гидрант)
          float k = 6.f * 100.f / (100.f + ty->mass);                         // лёгкое летит, тяжёлое едва сдвигается
          if (!PROP_KICK400_OLD) {   // P11-11 (сверка g10-flow): скорость = сила 400 / масса (кг) по направлению пинка, 45° вверх: 100 кг — 4 м/с, 1000 кг — 0,4, 20 кг — 20
            float dl = sqrtf(dx * dx + dz * dz); if (dl > 1e-4f) { dx /= dl; dz /= dl; }
            float v = 400.f / (ty->mass > 0.f ? ty->mass : 100.f), h45 = v * .70710678f;
            st->vel[0] = dx * h45; st->vel[2] = dz * h45; st->vel[1] = h45; k = h45;
          } else {
          st->vel[0] = dx * k; st->vel[2] = dz * k; st->vel[1] = fminf(k * .4f, 3.f); }
          st->w[0] = dz * k; st->w[2] = -dx * k; st->asleep = 0; st->sleepT = 0.f; st->fresh = 1;
        }
      }
      if (g->pColl) {
        collectHomerHit(g->pColl, &e);
#ifndef FOOT_FX_OLD
        if (e.type == HOMER_EV_HIT && (KICKWAVE_AIR_OLD || !homer.grounded)) collectFootKick(g->pColl, &homer);   // P02-19: волна — только на ударе в прыжке
#endif
      }                          // В5: пинок/стомп по осе
      if (g->hk.homerEvent) g->hk.homerEvent(g->hk.u, &e);                 // звук, музыка, пыль
      if (e.type == HOMER_EV_HURT && haveCoins) {
        // круг 2 (g04 8.6, g11b P04:W5, P09 Д18/Д5): лазер осы (в collect.c он зовёт homerHurt с силой 0,05) отнимает 5 монет и оглушает 0,5 с (homerStun),
        // со звуком потери монет; удар током (сила 1) — как было, до 10. WASP5_OLD=1 — прежнее
        int wasp = !WASP5_OLD && e.r < .1f, cap = wasp ? 5 : 10;
        int n = coins.collected < cap ? coins.collected : cap;   // удар током: монеты разлетаются
        if (wasp) homerStun(&homer, .5f);
        coins.collected -= n; coinsDrop(&coins, n, e.x, e.y, e.z, homer.groundY);
        if (wasp && n > 0 && g->hk.coinLose) { float lp[3] = {e.x, e.y, e.z}; g->hk.coinLose(g->hk.u, lp); } }
    }
  }
  XS_LAP(7, xt_, xw_);
  if (haveProps) propsUpdate(&props, &world, &car, dt); XS_LAP(8, xt_, xw_);
  if (haveSP) {   // стейтпропы: машина состояний, удары машины, монеты; частицы — пресеты в fx.c
    sprops.nearP[1][0] = haveHomer ? homer.x : car.x; sprops.nearP[1][1] = haveHomer ? homer.z : car.z;   // №1000
    sprops.hnrOff = g->hnrOff || g->hnrRoom || gameHnrFrozen(g);   // круг 2: и в разговоре (P03-07)   // сверка g11 (P11-06): Hit & Run выключен — вызовы «N монет» монет не дают (stateprops.c)
    sprops.radiate = gameRadiate; sprops.radUser = g; sprops.crateLike = gameIsCrate;   // сверка g11 (P16-21): вызов 8 RADIATE_FORCE (статуи Иедедайи) — толчок вверх предметам
    spUpdate(&sprops, &world, &car, haveCoins ? &coins : NULL, dt);
    // правка Роба 135.1: звук — на событие удара (машина, пинок/стомп Гомера: очередь spSndPop), не на любую смену состояния:
    // авто-переход (куст-шар встаёт сам через 1.3 с), возврат на место, обломок — без звука. spSt — как было (NEW GAME)
    { unsigned i; while (spSndPop(&sprops, &i)) if (spStInit && g->hk.spChange) g->hk.spChange(g->hk.u, i, fmaxf(fabsf(car.v), 4.f)); }
    for (unsigned i = 0; i < sprops.ninst && i < 512; i++) spSt[i] = sprops.run[i].state;
    spStInit = 1;
    // правка Роба 112 (30.09: «Hit & Run сразу, как только что-то разбиваю»): шкала +7.5% (данные: проп, docs/gameplay.md §6) — только за
    // удар игрока, сменивший состояние (машина, пинок, стомп, разрушение машиной). Раньше — за ЛЮБУЮ смену состояния: автопереходы по
    // таймеру (ящик исчезает через 1.4 с), возврат на место, теги миссий, обломок той же поломки — один ящик давал 3–4 прибавки
    // сверка g11 круг 2 (P03-11 от g10-flow): стейтпроп тоже даёт +7,5 и 1 монету только как новый (не из последних 16 тронутых); ключ — 0x4000 + номер экземпляра. STATEHIT16_SV_OLD=1 — как было
#ifndef STATEHIT16_SV_OLD
#define STATEHIT16_SV_OLD 0
#endif
    if (STATEHIT16_SV_OLD || HNR_TOUCH16_OLD) { for (; sprops.hnrHits; sprops.hnrHits--) gameHnrOffense(g, HNR_PROP); }
    else {
      for (unsigned q_ = 0; q_ < sprops.hnrHits; q_++) {
        unsigned key_ = q_ < sprops.hitInstN ? 0x4000u + sprops.hitInst[q_] + 1u : 0u;
        if (key_ && touchSeen(g, key_)) continue;
        if (gameHnrOffense(g, HNR_PROP) && key_) { touchAdd(g, key_); if (haveCoins) { coins.collected++; if (g->hk.coin) g->hk.coin(g->hk.u); } }
      }
      sprops.hnrHits = 0;
    }
    sprops.hitInstN = 0;
    SpFxOut o;
    while (spFxPop(&sprops, &o)) if (g->hk.spFx) g->hk.spFx(g->hk.u, &o);
  }
  XS_LAP(9, xt_, xw_);
  gameHnrBoom(g); gameCarHealthTrack(g);   // сверка g10-flow: взрыв машины ИИ миссии игроком (+25) и прочность машин — каждый кадр, без правки main_loop.h
  gameWrenchStep(g); XS_LAP(10, xt_, xw_);   // пункт 4: гаечные ключи (машина чинится, в том числе остов)
  if (haveCoins) {   // этап 4: монеты подбирает Гомер пешком (inCar = 0, досягаемость ×1) или машина (×2.25)
    float cp[3] = {car.x, car.y + 0.8f, car.z}; int drv = !haveHomer || homerDriving(&homer);
    if (haveHomer) homerCoinPoint(&homer, cp);
    int c0 = coins.collected;
    coins.ground = coinGround; coins.groundU = g->pWorld;   // правка 81: монеты не уходят в землю
    coins.dropAuto = drv; coinsUpdate(&coins, cp[0], cp[1], cp[2], drv, dt);
    if (coins.collected > c0 && g->hk.coin) g->hk.coin(g->hk.u);
  }
  XS_LAP(11, xt_, xw_);
  if (g->pColl) {   // В5: карточки, осы, гэги — после монет (монеты гэгов и ос вылетают в этом же кадре)
    int foot = haveHomer && homerOnFoot(&homer);
    CollPlayer p = {foot ? homer.x : car.x, foot ? homer.y : car.y, foot ? homer.z : car.z, foot, g->collRoom - 1, g->collAction};
    collectStep(g->pColl, &p, haveHomer ? &homer : 0, &car, &world, haveCoins ? &coins : 0, dt);
    g->collAction = 0;
  }
  XS_LAP(12, xt_, xw_);
}

#define WRENCH_REACH_FOOT .3f   // правило порта: запас к радиусу ключа (сфера ActionEvent 2.5 м) — пешком и в машине
#define WRENCH_REACH_CAR 1.f
static void wrenchMap(Game *g) {   // экземпляр модели для каждого ключа: тип SPR1 «насквозь» (cls 2) в той же точке
  g->wrenchMapped = haveSP ? 2 : 1;   // №417: 2 — сопоставлено с SPR1 (1 — без пакета: сопоставить, когда он появится)
  for (unsigned k = 0; k < 32; k++) {
    g->wrenchInst[k] = -1;
    if (k >= world.nwrench) continue;
    if (!haveSP) continue;
    const ColWrench *W = &world.wrench[k];
    for (unsigned i = 0; i < sprops.ninst; i++) {
      const SpInst *I = &sprops.inst[i];
      if (sprops.type[I->type].cls != 2) continue;
      float dx = I->x - W->p[0], dy = I->y - W->p[1], dz = I->z - W->p[2];
      if (dx * dx + dy * dy + dz * dz < .01f) { g->wrenchInst[k] = (short)i; break; }
    }
  }
}
// №417 (Роб 09.10: «ключ при подборе по-прежнему не исчезает»): Game живёт всю сессию, а SPR1 уровня пересоздаётся — смена уровня
// (LEVELUP, меню: levelUnload/levelLoad), NEW GAME / LOAD (newGame: spInit). Прежде сопоставление ключ -> экземпляр SPR1 делалось
// один раз: на уровне 2+ после смены прятался экземпляр с номером ключа ПРОШЛОГО уровня (чужой проп), модель ключа оставалась.
// После spInit модели снова живые, а биты «взят» прошлой игры держали ключ видимым и неподбираемым. Теперь: экземпляр проверяется
// по точке каждый кадр (не совпал — пересопоставить, все ключи на месте); взятый ключ, чья модель снова жива (новая игра), — на месте
static int wrenchStale(Game *g) {
  if (!g->wrenchMapped || (haveSP && g->wrenchMapped != 2)) return 1;
  if (!haveSP) return 0;
  for (unsigned k = 0; k < world.nwrench && k < 32; k++) {
    int i = g->wrenchInst[k]; if (i < 0) continue;
    if ((unsigned)i >= sprops.ninst) return 1;
    const SpInst *I = &sprops.inst[i]; const ColWrench *W = &world.wrench[k];
    float dx = I->x - W->p[0], dy = I->y - W->p[1], dz = I->z - W->p[2];
    if (dx * dx + dy * dy + dz * dz >= .01f) return 1;
  }
  return 0;
}
int gameWrenchStep(Game *g) {
  if (!world.nwrench) return 0;
#ifndef WRENCH_REMAP_OLD
  if (wrenchStale(g)) { wrenchMap(g); g->wrenchTaken = 0; memset(g->wrenchAway, 0, sizeof g->wrenchAway); }
  for (unsigned k = 0; k < world.nwrench && k < 32; k++)
    if ((g->wrenchTaken >> k & 1u) && g->wrenchInst[k] >= 0 && sprops.run[g->wrenchInst[k]].alive) { g->wrenchTaken &= ~(1u << k); g->wrenchAway[k] = 0.f; }
#else
  if (!g->wrenchMapped) wrenchMap(g);
#endif
  int foot = haveHomer && homerOnFoot(&homer), got = 0;
  float px = foot ? homer.x : car.x, py = (foot ? homer.y : car.y) + .8f, pz = foot ? homer.z : car.z;
  float reach = foot ? WRENCH_REACH_FOOT : WRENCH_REACH_CAR;
  for (unsigned k = 0; k < world.nwrench && k < 32; k++) {
    if (g->wrenchTaken >> k & 1u) continue;
    const ColWrench *W = &world.wrench[k]; float dx = px - W->p[0], dy = py - W->p[1], dz = pz - W->p[2], r = W->r + reach;
    if (dx * dx + dy * dy + dz * dz > r * r) continue;
    int wreck = car.destroyed || car.damage > 0.f;
    g->wrenchTaken |= 1u << k; g->wrenchAway[k] = 0.f; g->wrenches++; got++;
#ifndef WASP_WRENCH_AGGR_OLD
#define WASP_WRENCH_AGGR_OLD 0   // сверка g11b-collect круг 7 (P04 Д12): подбор ключа переводит ос в агрессию, как карточка (в оригинале событие подбора вещи одно на все). 1 — прежнее
#endif
    if (!WASP_WRENCH_AGGR_OLD && g->pColl) collectWatchersAggro(g->pColl);
    if (g->wrenchInst[k] >= 0) { SpRun *R = &sprops.run[g->wrenchInst[k]]; R->alive = 0; R->colOn = R->col2On = 0; }
    if (wreck) { car.damage = 0.f; car.destroyed = 0; wasDestroyed = 0; g->wrenchRepairs++; }   // вся машина, и остов
    if (g->hk.wrench) g->hk.wrench(g->hk.u, W->p, wreck);
  }
  return got;
}

// Правка 359 (Роб: «ключ появляется сразу»): возврат ключа — параметры. TODO близнецу: замерить у оригинала (SLES RespawnEntity /
// команда времени возврата; tools/twins, патч agent355_twin/a355_twin.py) — сколько секунд и на каком расстоянии; пока прежние 5 с / 60 м
// №417 (09.10): правило порта — числа задержки в данных диска нет (скрипты level*/*.mfk без Respawn*, в SLES_518.97 только имена
// классов RespawnEntity / RespawnCollectible / WrenchIcon), запись PS2 L1 (4:15) возврата не показывает; гейт tests/wrench/levels.sh меряет 5.00 с
#ifndef WRENCH_RESPAWN_T
#define WRENCH_RESPAWN_T 5.f
#endif
#ifndef WRENCH_RESPAWN_R
#define WRENCH_RESPAWN_R 60.f
#endif
// Сверка g10-flow (02.11.2026, P07 №7, P05 №10): ключ ремонта возвращается ровно через 30 с после подбора (значение по умолчанию; команды
// времени возврата в данных уровня нет), по таймеру собранного предмета, независимо от расстояния и вида камеры. Было: 5 с вне вида и дальше 60 м.
// WRENCH30_OLD=1 — как было. Предметы и стейтпропы возвращаются прежним правилом (их правит окно g11)
#ifndef WRENCH30_OLD
#define WRENCH30_OLD 0
#endif
#define WRENCH_RESPAWN_PS2 30.f
void gameRespawn(Game *g, float ex, float ez, float dt, int (*seen)(void *, float, float, float, float), void *u) {
  if (!WRENCH30_OLD) {
    for (unsigned k = 0; k < world.nwrench && k < 32 && dt > 0.f; k++) {
      if (!(g->wrenchTaken >> k & 1u)) continue;
      if ((g->wrenchAway[k] += fminf(dt, .25f)) < WRENCH_RESPAWN_PS2) continue;
      g->wrenchTaken &= ~(1u << k); g->wrenchAway[k] = 0.f;
      if (g->wrenchInst[k] >= 0) { SpRun *R = &sprops.run[g->wrenchInst[k]]; R->alive = 1; }
    }
  }
  for (unsigned k = 0; k < world.nwrench && k < 32 && dt > 0.f && seen && WRENCH30_OLD; k++) {   // ключ возвращается, как предметы: 5 с вне вида, дальше 60 м
    if (!(g->wrenchTaken >> k & 1u)) continue;
    const ColWrench *W = &world.wrench[k]; float dx = W->p[0] - ex, dz = W->p[2] - ez;
    if (dx * dx + dz * dz < WRENCH_RESPAWN_R * WRENCH_RESPAWN_R || seen(u, W->p[0], W->p[1], W->p[2], W->r)) { g->wrenchAway[k] = 0.f; continue; }
    if ((g->wrenchAway[k] += fminf(dt, .05f)) < WRENCH_RESPAWN_T) continue;
    g->wrenchTaken &= ~(1u << k);
    if (g->wrenchInst[k] >= 0) { SpRun *R = &sprops.run[g->wrenchInst[k]]; R->alive = 1; }
  }
  if (haveProps) propsRespawnStep(&props, ex, ez, dt, seen, u);
  if (haveSP) spRespawnStep(&sprops, ex, ez, dt, seen, u);
}

static int hnrChasesN;   // погонь на этом уровне (P03 Д4)
#ifndef HNR_DECAY_DELAY
#define HNR_DECAY_DELAY 3.f   // с без наездов до начала спада (запись PS2: последний наезд 25:59 -> спад с 26:02)
#endif
#define HNR_SCALE 100.f       // ед. на полной шкале (docs/gameplay.md §6, числа исполняемого файла PAL: удар машины +5, проп +7.5 …)
// №723 (Роб: «как PS2 целиком»): у PS2 в погоне шкала спадает 6,0 ед./с БЕЗ паузы (knowledge/facts/ps2-hnr-meter.yaml: «запись 100 сразу включает погоню; в погоне спад 6,0/с без паузы»; tw_v8_stand: 99,78 → 0 за 17,6 с,
// b3_chase и v111 естественная: 15,9 с), пауза 3 с после нарушения — только вне погони. Порт: нарушение, добившее шкалу до 100 ДО защёлки погони (main_frontend.h hnrPursuit), обнуляло hnrCalm, и первые 3 с погони шкала
// стояла на месте (погоня до нуля 19–20 с вместо 16–17). Теперь в защёлке погони пауза не ждётся. HNR_CHASE_PAUSE_OLD=1 / gameHnrChasePauseOld = 1 / файл hnrpause723old / flag hnrpause723 0 — как было
#ifndef HNR_CHASE_PAUSE_OLD
#define HNR_CHASE_PAUSE_OLD 0
#endif
int gameHnrChasePauseOld = HNR_CHASE_PAUSE_OLD;
void gameHnrRate(Game *g, float dt, float rate) {   // спад rate ед./с через HNR_DECAY_DELAY с без наездов
  if (!HNR_FREEZE_OLD && g->hnrFreeze > 0.f) { g->hnrFreeze -= dt; return; }   // P03-07: разговор/переход — шкала замерла
  hnrCalm += dt; if (hnrCalm > HNR_DECAY_DELAY || (g->hnrLatched && !gameHnrChasePauseOld)) hnrHeat = fmaxf(0.f, hnrHeat - rate / HNR_SCALE * dt);
}
// Правка Роба 112 (30.09, «сверяйся с оригиналом напрямую»): обычный спад — SetHitAndRunDecay уровня (L1 3.0) на шкале 100: полная
// шкала гаснет за ~33 с. Замер записи PS2 (tools/ps2ref/hnr_track.py, жёлтый указатель на ободе радара, ход ~300°): спад 5–9.5°/с,
// медиана 8°/с ≈ 2.7 ед./с. Прежние «50 ед., 16.7 с» сняты с красной шкалы ПОГОНИ (26:02–26:18) — это спад в погоне 6/с (main.c)
void gameHnr(Game *g, float dt) { gameHnrRate(g, dt, 3.f); }
// P03-07 / СВЕРКА_С_ИСХОДНИКОМ 2.8: пока идёт разговор или вход/выход из помещения, Hit & Run выключен — события не считаются, спад стоит
// (в помещении при включённом режиме спад идёт 10/с, это уже в main_loop.h). Вызывающий держит заморозку, пока переход идёт
void gameHnrFreeze(Game *g, float sec) { if (sec > g->hnrFreeze) g->hnrFreeze = sec; }
int gameHnrFrozen(const Game *g) { return !HNR_FREEZE_OLD && g->hnrFreeze > 0.f; }
// 2.3: взрыв машины ИИ миссии, разбитой игроком, — +25 (монеты 10 делает окно g11b). Трафик (kind 1) считает main_loop.h по событию
// трафика TR_EV_CAR_WRECK, чтобы не прибавлять дважды; полиция (kind 3) и машина игрока (kind 0) не считаются (P03 Д1: у оригинала
// машины погони Hit & Run не двигают). Иммунные машины сценария (P03-30): отметки в данных миссий нет — см. журнал
#ifndef HNR_BOOM_OLD
#define HNR_BOOM_OLD 0
#endif
int gameHnrBoom(Game *g) {
  if (HNR_BOOM_OLD || svBoom.seq == g->boomSeen) return 0;
  g->boomSeen = svBoom.seq;
  if (!svBoom.byPlayer || svBoom.kind != 2) return 0;
  return gameHnrOffense(g, HNR_CAR_WRECK);
}
void gameHnrNewLevel(Game *g) { (void)g; hnrChasesN = 0; }
// №430 (ДОБИВКА 19), арест по 7 арестам PS2: build/ps2ref/answers/b3_bust, b3_bust80, b3_fine30, b3_fine200, b3_coinauth,
// b3_coins30, b3_coins80 (trace.tsv: флаги погони 0x6121b4, счётчик близости 0x6121dc, таймер ареста 0x6121ec; разбор
// knowledge/twins_library/разбор/ps2ref.md, раздел 1; колонка coins там — 0x9e79c0, не монеты, не используется):
//  - полиция: отсчёты записей идут через кадр, событие лежит в 1 кадре до своего отсчёта, задержка известна с точностью ±1 кадр.
//    От флага погони 0x100 до позиции полиции 0xa257e0: 35–37 кадров PAL в 7 записях (b3_bust, bust80, coinauth, coins80, fine30,
//    fine80, moving), 33–35 в двух (b3_fine200, b3_coins30: флаг на 152, позиция на 186, на 184 её ещё нет); общее для всех 9 —
//    только 35 кадров = 0,70 с. До флага 0x101: 33–39, общего значения нет (b3_bust80, b3_chase 37–39; fine200, coins30 33–35).
//    0,72 с (36 кадров) — ПРАВИЛО ПОРТА в диапазоне факта ps2-hnr-police «0,72–0,76 с»: в 7 записях внутри, в fine200 и coins30
//    порт позже PS2 на 1–3 кадра (гейт delay сравнивает с отсчётами и проходит там на краю допуска 2). Было 0,6 с (ps2-hnr-chase,
//    ступень 4) — раньше всех записей
//  - таймер ареста 0x6121ec: игра ставит 1500 мс (ps2-hnr-police, tools/ps2ref/addrs.tsv) и вычитает время кадра (в записях
//    ~10–32 мс за кадр, не ровно 20). По отсчётам от первого с таймером до первого с нулём 74 rel во всех 7 записях, но это сетка через кадр: по мс
//    кадр ареста и кадр нуля дают ~73–75 кадров (b3_coins80 ~75: счётчик 741 на rel 414, арест на 415, 24 мс на 488, −17 на 490).
//    1,48 с — ПРАВИЛО ПОРТА: 1500 мс, из которых первый шаг ~20 мс сделан в кадре ареста (в 4 записях первый отсчёт ~1480);
//    при 50 Гц 74 кадра. Было 1,5 с (76 кадров при 50 Гц из-за остатка float)
//  - близость (10 м, 30 км/ч, 0,75 с; факт ps2-hnr-police) не меняется. Сбросы счётчика 0x6121dc в записях: на 9,934–10,063 м
//    в плоскости x/z при скорости 0 (b3_fine30 rel 274; fine200 268, 366; coinauth 270; coins30 268, 364; coins80 270, 366) и
//    один необъяснённый в b3_bust rel 870 (8,63 м, 8,29 км/ч). Сброса по скорости в записях нет: порог 30 км/ч — из факта
//    (число 31,6 км/ч там — отсчёт b3_bust rel 876, когда счётчик уже 0 с rel 870). Счётчик растёт при 25,2 км/ч (b3_bust
//    rel 1030). Арест при 3,7–9,5 м и 0–19,8 км/ч; счётчик в отсчёте перед арестом 716–770 мс (b3_bust 770) — у порта 0,75 с
// ARREST_PS2_OLD=1 — прежнее (0,6 с и 1,5 с; красный контроль tests/hnr/arrest_ps2.sh)
#ifndef ARREST_PS2_OLD
#define ARREST_PS2_OLD 0
#endif
#define HNR_POLICE_DELAY_PS2 .72f   // правило порта: 36 кадров PAL, в факте ps2-hnr-police 0,72–0,76 с; записи b3_* 33–37 (±1)
#define HNR_BUST_T_PS2 1.48f        // правило порта: 1500 мс 0x6121ec минус шаг кадра ареста; записи 73–75 кадров (±1)
#define HNR_TIMER_EPS 1e-4f         // правило порта: остаток float после N шагов dt — ноль (иначе 1,48 при 50 Гц длится 75 кадров)
// кадр защёлки сам делает первый шаг таймера (main.c: защёлка, потом проверка полиции) — его dt прошёл до погони: полиция в
// первом кадре, когда с защёлки прошло 0,72 с (50 Гц — кадр 36, у PS2 по позиции 33–37; 60 Гц — кадр 44). Прежнее: 0,6 с вместе с этим шагом
// P03 Д4 (сверка g10-flow): задержку 0,72 с даёт счётчик добавления, стартующий с нуля, — только первая погоня уровня; вторая и следующие в том
// же уровне: счётчик копился всё время, полиция ставится в первом же кадре после 100 %. HNR_CHASE_FIRST_OLD=1 — 0,72 с при каждой погоне.
// Счётчик погонь обнуляет gameHnrNewLevel (загрузка уровня, новая игра)
#ifndef HNR_CHASE_FIRST_OLD
#define HNR_CHASE_FIRST_OLD 0
#endif
void gameHnrChaseStart(float *spawnT, float dt) {
  int later = !HNR_CHASE_FIRST_OLD && !ARREST_PS2_OLD && hnrChasesN > 0; hnrChasesN++;
  *spawnT = ARREST_PS2_OLD ? .6f : later ? dt : HNR_POLICE_DELAY_PS2 + dt;
}
int gameHnrSpawnDue(float *t, float dt) {
  *t -= dt;
  return ARREST_PS2_OLD ? *t <= 0.f : *t <= HNR_TIMER_EPS;
}
int gameHnrCatch(float *nearT, float dt, int armed, float dist, float speed, int onFoot) {
  int can = armed && dist < HNR_BUST_R && (onFoot || speed < HNR_BUST_V);
  *nearT = can ? *nearT + dt : 0.f;
  if (*nearT + 1e-6f < (onFoot ? HNR_BUST_FOOT : HNR_BUST_CAR)) return 0;
  *nearT = 0.f; return 1;
}
float gameHnrBustTime(void) { return ARREST_PS2_OLD ? 1.5f : HNR_BUST_T_PS2; }
int gameHnrBustTick(Game *g, float dt) {
  if (g->hnrBusted <= 0.f) return 0;
  g->hnrBusted -= dt;
  if (g->hnrBusted <= (ARREST_PS2_OLD ? 0.f : HNR_TIMER_EPS)) { g->hnrBusted = 0.f; return 1; }
  return 0;
}
// Правка 121 (30.09, «сверяйся с оригиналом напрямую»): правила шкалы — исполняемый файл PAL SLES_518.97 (разбор
// hit-and-run-web/src/hit-and-run.ts, MIT: конструктор 0x250d68, события 0x251328, обновление 0x251c78); прибавки сверены с
// записью PS2 (tools/ps2ref/hnr_track.py). Нарушение не считается: сценарий выключил шкалу, игрок в комнате, идёт арест. Удар
// машины вне погони — не раньше, чем через 1.5 с после прошлого нарушения (половина паузы спада); в погоне — всегда. Любое
// нарушение ставит паузу спада 3 с, в погоне — без паузы. hnrCalm — секунды с прошлого нарушения (пауза = 3 − hnrCalm)
#ifndef HNR_CAR_COOL   // переопределяется только отрицательным контролем гейта tests/hnr/run.sh
#define HNR_CAR_COOL 1.5f
#endif
// Аудит 09.10 п. 25 / В35: сбитый машиной пешеход у PS2 +13, не +10 (запись tw_v14_multi30, кадр 15099: 5 -> 18 при 17,4 м/с, на
// снимке rel 450 сбитый ребёнок лежит у машины; tw_v14_rig30: +13 кадры 15541, 16219 и +26 — двое, b2_ped2 +26). +10 не
// встречается ни в одной записи. HNR_PED10_OLD=1 — прежнее +10 (красный tests/hnr/run.sh)
#ifndef HNR_PED10_OLD
#define HNR_PED10_OLD 0
#endif
// СВЕРКА_ГОМЕР 3.4 / СВЕРКА_С_ИСХОДНИКОМ 2.4 (сверка g10-flow): пинок пешехода — +10 (сбил машиной — +13, подтверждено записью PS2; пинок отдельно
// не мерили, число из оригинала). HNR_KICK10_OLD=1 — прежнее +13
#ifndef HNR_KICK10_OLD
#define HNR_KICK10_OLD 0
#endif
static const float hnrAmt[] = {5.f, 25.f, 7.5f, HNR_PED10_OLD ? 10.f : 13.f, HNR_KICK10_OLD ? 13.f : 10.f};   // HNR_CAR_HIT … HNR_PED_KICK, ед. шкалы 100
int gameHnrOffense(Game *g, int type) {
  if (g->hnrOff || g->hnrRoom || g->hnrBusted > 0.f || type < HNR_CAR_HIT || type > HNR_PED_KICK) return 0;
  if (!HNR_FREEZE_OLD && g->hnrFreeze > 0.f) return 0;   // P03-07: разговор/переход — события не считаются
  float pause = fmaxf(0.f, HNR_DECAY_DELAY - hnrCalm);
  if (type == HNR_CAR_HIT && pause > (g->hnrLatched ? 0.f : HNR_CAR_COOL)) return 0;
  hnrCalm = g->hnrLatched ? HNR_DECAY_DELAY : 0.f;
  hnrHeat = fminf(1.f, hnrHeat + hnrAmt[type] / HNR_SCALE);
  return 1;
}
// сел в машину (данные: «вошёл в машину» — шкала −100, если это не та же машина, что в прошлый раз; во время ареста — нет)
void gameHnrVehicle(Game *g, const char *name) {
  if (!name) name = "";
  if (g->hnrCarSet && !strncmp(g->hnrCar, name, sizeof g->hnrCar - 1)) return;
  g->hnrCarSet = 1; strncpy(g->hnrCar, name, sizeof g->hnrCar - 1); g->hnrCar[sizeof g->hnrCar - 1] = 0;
  if (g->hnrOff || g->hnrBusted > 0.f) return;
  hnrHeat = 0.f; hnrCalm = g->hnrLatched ? HNR_DECAY_DELAY : 0.f;
}

// ---- P01 Д1 (сверка g10-flow): прочность личной машины игрока хранится по имени машины долей 0..1 и переживает смену миссии, уровня, вызов из будки.
// Таблица общая на игру (8 машин); пишется каждый кадр по машине последней посадки (gameHnrVehicle) и в запись игры (savegame ext, main_save.h).
// Постановку машины с применением доли делают места, где машина создаётся (загрузка уровня, будка: main_levels.h, main_frontend.h) —
// они зовут gameCarHealthApply. CAR_HEALTH_OLD=1 — таблица не ведётся, урон обнуляется при каждой постановке, как было
static uint16_t chHashOf(const char *n) {   // FNV-1a, 16 бит; ключ таблицы и записи игры (имён в записи нет)
  uint32_t h = 2166136261u; if (!n) n = "";
  for (; *n; n++) { h ^= (unsigned char)*n; h *= 16777619u; }
  return (uint16_t)(h ^ (h >> 16));
}
static uint16_t chKey(const Game *g, const char *n) {   // №1171: машина уровня (пустое имя) — своя запись у каждого уровня
  if (CAR_HEALTH_LVL1171_OLD || (n && *n)) return chHashOf(n);
  char t[4] = {'~', (char)('A' + (g->chLevel & 15)), 0, 0}; return chHashOf(t);   // «~A»… по номеру уровня
}
static int chFind(const Game *g, const char *n) {
  uint16_t k = chKey(g, n);
  for (int i = 0; i < g->chN; i++) if (g->chHash[i] == k) return i;
  return -1;
}
void gameCarHealthNote(Game *g, const char *name, float frac) {
  if (CAR_HEALTH_OLD) return;
  if (frac < 0.f) frac = 0.f; if (frac > 1.f) frac = 1.f;
  int i = chFind(g, name);
  if (i < 0) {
    if (g->chN < 8) i = g->chN++;
    else { for (int k = 1; k < 8; k++) { g->chHash[k - 1] = g->chHash[k]; g->chFrac[k - 1] = g->chFrac[k]; } i = 7; }   // самая старая уходит
    g->chHash[i] = chKey(g, name);
  }
  g->chFrac[i] = frac;
}
float gameCarHealthGet(const Game *g, const char *name) { int i = chFind(g, name); return i < 0 ? -1.f : g->chFrac[i]; }
// доля -> урон: HitPoints×доля; видимый урон выставляется сразу (место удара — капот, как в оригинале); доля <= 0 — машина создаётся остовом
void gameCarHealthApply(Game *g, const char *name, Car *c) {
  if (CAR_HEALTH_OLD) return;
  float f = gameCarHealthGet(g, name); if (f < 0.f) return;
  if (f <= 0.f) { c->damage = 1.f; c->destroyed = 1; c->deadT = CAR_BOOM_DELAY + 1.f; }   // остов: уже взорвалась, Гомера не выбрасывает
  else if (f < 1.f) { c->damage = 1.f - f; c->destroyed = 0; }
}
// будка (P01 Д1): ремонт — только остову (плата — phone_view.c, цена там уже есть) и ключ; побитая, но целая машина из будки приезжает побитой.
// Остов после вызова из будки — цел (доля 1). CAR_HEALTH_OLD=1 — как было (урон в ноль при каждой постановке)
void gameCarSummon(Game *g, const char *name, Car *c) {
  if (CAR_HEALTH_OLD) { c->damage = 0.f; c->destroyed = 0; return; }
  float f = gameCarHealthGet(g, name);
  if (f < 0.f || f <= 0.f) { c->damage = 0.f; c->destroyed = 0; gameCarHealthNote(g, name, 1.f); }
  else { c->damage = f < 1.f ? 1.f - f : 0.f; c->destroyed = 0; }
}
void gameCarHealthTrack(Game *g) {
  if (CAR_HEALTH_OLD) return;
  gameCarHealthNote(g, g->hnrCarSet ? g->hnrCar : "", car.destroyed ? 0.f : 1.f - car.damage);
}
void gameCarHealthClear(Game *g) { g->chN = 0; }
int gameCarHealthDump(const Game *g, uint16_t *hash, float *frac) { for (int i = 0; i < g->chN; i++) { hash[i] = g->chHash[i]; frac[i] = g->chFrac[i]; } return g->chN; }
void gameCarHealthLoad(Game *g, int n, const uint16_t *hash, const float *frac) {
  g->chN = 0; for (int i = 0; i < n && i < 8; i++) { g->chHash[g->chN] = hash[i]; g->chFrac[g->chN++] = frac[i]; }
}

// ---- стик (P17-9; СВЕРКА_МАШИНЫ 1.4): мёртвая зона 0,25 по каждой оси отдельно, остаток пересчитан в 0..1 (плавный старт), значение оси
// меняется, только если новое отличается от прежнего больше чем на 0,05. Возврат в ноль — сразу (как в оригинале: значение внутри зоны становится нулём без проверки на 0,05). Допуск оси 0,01 и колесо-руль в отчёте без применения на PSP
static float stickPrev[2];
static float stickAxis(float raw, float *prev) {
  float a = fabsf(raw), v = 0.f;
  if (GAME_STICK_OLD) { v = a < .2f ? 0.f : raw; *prev = v; return v; }
  if (a > .25f) { v = (a - .25f) / .75f; if (v > 1.f) v = 1.f; if (raw < 0.f) v = -v; }
  if (v == 0.f || fabsf(v - *prev) > .05f) *prev = v;
  return *prev;
}
void gameStick(float rawx, float rawy, float *ax, float *ay) { *ax = stickAxis(rawx, &stickPrev[0]); *ay = stickAxis(rawy, &stickPrev[1]); }
// №1104 (коды оригинала): таблица включённых кодов живёт здесь, ввод — viewer/cheats.h; меню зовут gameCheatsFrame, остальные модули спрашивают gameCheatOn
#include "cheats.h"
static CheatSys gameCheats;
static void gameCheatsSync(void) {   // флаги машины игрока (car.c) от таблицы кодов
  carCheatNoDrag = cheatOn(&gameCheats, CH_NOTOP); carCheatAccel = cheatOn(&gameCheats, CH_ACCEL);
  carCheatInvincible = cheatOn(&gameCheats, CH_INVINC); carCheatOneTap = cheatOn(&gameCheats, CH_ONETAP); carCheatHornJump = cheatOn(&gameCheats, CH_HORNJUMP);
}
int gameCheatOn(int id) { return cheatOn(&gameCheats, id); }
void gameCheatsReset(void) { cheatsReset(&gameCheats); gameCheatsSync(); }
int gameCheatsFrame(unsigned held, unsigned hit, int allStory, int *id) { int ev = cheatsFrame(&gameCheats, held, hit, allStory, id); gameCheatsSync(); return ev; }
int gameCheatsActive(void) { return cheatsActive(&gameCheats); }
int gameCheatsPending(void) { return cheatsActive(&gameCheats) && gameCheats.idx == 3; }   // следующее нажатие завершит четвёрку (тогда нужно знать, пройдена ли игра)
void gameCheatsSet(unsigned mask) { gameCheats.on = mask; gameCheatsSync(); }   // служебно (хост-тест): включить коды без ввода
