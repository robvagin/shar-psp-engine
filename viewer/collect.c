// Карточки, осы-камеры и гэги (см. collect.h: данные, правила порта). Чистый C99, без malloc в кадре.
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "collect.h"
#include "bolt_math.h"
#include "ticket.h"
#include "sverka_shared.h"   // круг 7: svBoltWorld — снаряд осы и мир
static int gmDirty = 1;   // №637: таблица экземпляр->гэг (collectGagOf) строится заново после collectInit/collectFree

_Static_assert(sizeof(CollCard) == 32, "CollCard 32");
_Static_assert(sizeof(CollWasp) == 80, "CollWasp 80");
_Static_assert(sizeof(CollGag) == 128, "CollGag 128");

#define ALIGN16(x) (((x) + 15u) & ~15u)
#define CARD_REACH_FOOT .6f      // правило порта: досягаемость карточки сверх радиуса триггера
#define CARD_REACH_CAR 1.4f
#define WASP_DESPAWN 15.f        // оса исчезает дальше дистанции появления + 15 м
#define WASP_HEAD_FOOT 2.f       // высота полёта к игроку над его ступнями: пешком — в досягаемости прыжка с пинком
#define WASP_HEAD_CAR 1.f        // в машине — в слое кузова (0.35–1.6 м), машина может сбить
#define WASP_ZAP_ARC 15.f        // выстрел попадает в дуге ±15° и дальности x1.2
#define WASP_HIT_R 1.f           // досягаемость пинка по осе (как у стейтпропов в game.c)
// щит осы (правило порта, см. collect.h): ударов до пролома, погасание после пролома, радиус шара удара машиной —
// половина билборда shield_bbShape в состоянии «держит» (2.97 м, l05_fx.p3d BQG кадр 40)
#ifndef COLL_SHIELD_HP
#define COLL_SHIELD_HP 2
#endif
#ifndef COLL_SHIELD_ON
#define COLL_SHIELD_ON 1   // отрицательный контроль гейта tests/levelmech/shield.sh: 0 — щит не держит удар
#endif
#define COLL_SHIELD_GONE_S 1.f
#define COLL_SHIELD_R 1.5f
enum { WS_APPEAR = 0, WS_FLY = 1, WS_CHARGE = 2, WS_CHARGED = 3, WS_FIRE = 4, WS_DEAD = 5 };   // состояния beecamera в данных

// ---- сверка с исходником, окно g11b (02.11.2026). Каждый переключатель: 1 — прежнее поведение TEST81; рядом — документ сверки (docs/handoff/СВЕРКА_ПОЛНАЯ/P04_ии_машин_и_летающих.md, СВЕРКА_ГОМЕР.md)
#ifndef WASP_BOLT_KMH_OLD
#define WASP_BOLT_KMH_OLD 0    // W1: первое число SetProjectileStats — км/ч (было: м/с), второе — размер пула снарядов (было: время жизни, с); снаряд гаснет дальше 65 м от игрока
#endif
#ifndef WASP_SPAWN_ORIG_OLD
#define WASP_SPAWN_ORIG_OLD 0  // W2: оса появляется в сфере радиуса «пятого» аргумента (roam), исчезает дальше 65 м от игрока, без поводка от точки (было: шестой аргумент + 15 м, поводок roam)
#endif
#ifndef WASP_AGGR_OLD
#define WASP_AGGR_OLD 0        // W3, W8, W10, Д7, 8.2, 8.3: наблюдение -> агрессия при игроке ближе минимума ATTRACTION; заряд сразу; перелёты в позицию атаки по интервалу (было: атака с дистанции стрельбы, перелётов нет)
#endif
#ifndef WASP_EVADE_OLD
#define WASP_EVADE_OLD 0       // W9, 8.4: уход (EVADE_PLAYER) при игроке ближе 4 м (было: уклонения нет)
#endif
#ifndef WASP_AIM_OLD
#define WASP_AIM_OLD 0         // W4, 8.1, 8.5: выстрел по оси осы, половина дуги, дальность без x1,2, оса в кадре, цель +1,2 м без прыжка (было: точно в игрока +1 м, полная дуга, x1,2)
#endif
#ifndef WASP_FLIGHT_OLD
#define WASP_FLIGHT_OLD 0      // W6, Д8, 8.7: минимум 2 м над землёй/крышей, обход статики лучом COL1 (было: 1 м, напрямик)
#endif
#ifndef WASP_FENCEBOX_OLD
#define WASP_FENCEBOX_OLD 0    // круг 5 (P11-Д5, P11-25): заборы в луче перелёта осы перекрывают путь рамкой по двум концам отрезка (было: только по самому отрезку)
#endif
#ifndef WASP_BOLT_HIT_OLD
#define WASP_BOLT_HIT_OLD 0    // W5, Д10: стена проверяется один раз на 25 м при выстреле, взрыв на 1,75 м за поверхностью; прыгающего игрока снаряд пропускает; другую осу убивает; удар током без отлёта
#endif
#ifndef GAG_CAR15_OLD
#define GAG_CAR15_OLD 0        // круг 7: триггер-гэг касанием: из машины запас 1,5 м (оригинал, как у карточки). 1 — без запаса
#endif
#ifndef CARD_CAR15_OLD
#define CARD_CAR15_OLD 0       // круг 7 (P05:9): из машины карточка берётся, когда сфера 1,5 м вокруг машины касается её сферы (оригинал); круг 2 убрал запас совсем — ошибка. 1 — без запаса
#endif
#ifndef WASPBOLTWORLD_OLD
#define WASPBOLTWORLD_OLD 0    // круг 7 (P04 W5): снаряд бьёт и подвижные предметы, пешеходов, трафик (крюк svBoltWorld). 1 — только игрока и ос, как было
#endif
#ifndef WASP_BOLT_BEHIND75_OLD
#define WASP_BOLT_BEHIND75_OLD 0   // круг 7 (P04 Д10): точка взрыва у стены 0,75 м за поверхностью (оригинал), было 1,75. 1 — прежнее
#endif
#ifndef WASP_SHIELD_OLD
#define WASP_SHIELD_OLD 0      // W7: щит ломается первым сильным ударом (было: вторым), порог машины 0,85 м/с (было: 2 м/с)
#endif
#ifndef WASP_CARD_AGGR_OLD
#define WASP_CARD_AGGR_OLD 0   // Д12: подбор карточки в режиме наблюдения переводит всех таких ос в агрессию (карточки ли это — по данным сверить не удалось, принято: да)
#endif
#ifndef GAG_SOUNDWAIT_OLD
#define GAG_SOUNDWAIT_OLD 0    // P16-14: гэг проигран, когда кончились и анимация, и звук (было: по длине анимации)
#endif
#ifndef GAG_COINS_AUTO_OLD
#define GAG_COINS_AUTO_OLD 0   // P16 Д5: монеты гэга на улице после первого касания земли сами летят к игроку (было: лежат до подбора)
#endif
#ifndef GAG_MISSION_OLD
#define GAG_MISSION_OLD 0      // P16 Д7: гэги с роликом (GagPlayFMV) и проверочные срабатывают только вне миссии
#endif
#ifndef WASP_ZAP_COINS_OLD
#define WASP_ZAP_COINS_OLD 0   // круг 2, P09:Д18: удар осой отнимает 5 монет (было: 10 у любого HOMER_EV_HURT в game.c); при HOMER_STUN_OLD=1 остаётся путь game.c
#endif
#ifndef CARD_EXACT_OLD
#define CARD_EXACT_OLD 0       // круг 2, P05:9 (g09b): карточка берётся точкой строго внутри сферы локатора, из машины — центр и четыре угла кузова, без запаса 0,6/1,4 м (было: запас)
#endif
#define WASP_SV_FAR 65.f         // W1/W2: дальше от игрока — снаряд и оса гаснут
#define WASP_MIN_ALT 2.f         // Д8: не ниже 2 м над землёй или крышей
#define WASP_GOAL_R .4f          // 8.7: путевая точка пройдена в 0,4 м
#define WASP_MOVE_KMH 20.f       // W8: скорость перелёта в позицию атаки и отлёта
#define WASP_EVADE_NEAR 4.f      // W9: игрок ближе 4 м — уход
#define WASP_BACKOFF_NEAR 2.f    // 8.3: игрок ближе 2 м — отлёт на 5 м
#define WASP_BACKOFF_D 5.f
// числа EVADE_PLAYER из leveli.mfk (min, max по горизонтали, км/ч); команда не попадает в пакет CLT1 (запись осы без поля) — таблица по уровню; 0 в min/max — ухода на уровне нет
static const float WASP_EVADE[8][3] = {{0, 0, 0}, {0, 0, 0}, {2.5f, 8.f, 5.f}, {1.f, 2.f, 10.f}, {1.f, 5.f, 5.f}, {1.f, 5.f, 10.f}, {0, 0, 0}, {1.f, 3.5f, 30.f}};

static const char *ROOMN[] = {"SpringfieldElementary", "KwikEMart", "SimpsonsHouse", "DMV", "Moes", "AndroidsDungeon",
                              "Observatory", "BartRoom"};   // как NAMES в tools/interior_export.py

const char *collectError(int code) {
  switch (code) {
    case -1: return "short or unaligned pack"; case -2: return "not CLT1"; case -3: return "table out of file";
    case -4: return "SPR1 rejected"; case -5: return "bad instance index"; default: return "?";
  }
}

int collectRoomId(const char *s) {
  if (!s) return -1;
  for (int i = 0; i < (int)(sizeof ROOMN / sizeof *ROOMN); i++) {
    const char *a = ROOMN[i], *b = s;
    while (*a && *b && ((*a | 32) == (*b | 32))) { a++; b++; }
    if (!*a && !*b) return i;
  }
  return -1;
}

static unsigned rnd(CollState *s) { s->rng = s->rng * 1664525u + 1013904223u; return s->rng >> 8; }

static void push(Collect *c, int type, int idx, float x, float y, float z) {
  CollState *s = &c->st;
  if (s->nev >= COLL_EVQ) { s->evDropped++; return; }
  CollEvent *e = &s->evq[s->nev++]; e->type = type; e->idx = idx; e->x = x; e->y = y; e->z = z;
}

int collectPop(Collect *c, CollEvent *e) {
  CollState *s = &c->st;
  if (!s->nev) return 0;
  *e = s->evq[0]; memmove(s->evq, s->evq + 1, --s->nev * sizeof *e);   // по порядку появления
  return 1;
}

int collectInitResident(Collect *c, const void *pak, unsigned size, int detachedKeys) {
  gmDirty = 1;
  memset(c, 0, sizeof *c); c->kickFx = c->shockFx = c->bolt0 = -1;
  const unsigned char *h = pak;
  if (!pak || size < 64 || ((uintptr_t)pak & 15)) return -1;
  if (memcmp(h, "CLT1", 4)) return -2;
  const uint32_t *u = (const uint32_t *)(h + 4);
  unsigned level = u[1], ntex = u[2], texOff = u[3], sprOff = u[4], sprSize = u[5], ncard = u[6], cardOff = u[7],
           nwasp = u[8], waspOff = u[9], ngag = u[10], gagOff = u[11], alive = u[12];
  if ((uint64_t)sprOff + sprSize > size || (uint64_t)cardOff + ncard * 32ull > size || (uint64_t)waspOff + nwasp * 80ull > size ||
      (uint64_t)gagOff + ngag * 128ull > size || texOff > size || ncard > COLL_MAXCARD || nwasp > COLL_MAXWASP || ngag > COLL_MAXGAG ||
      (sprOff & 15) || (cardOff & 3) || (waspOff & 3) || (gagOff & 3)) return -3;
  c->level = (int)level; c->maxAlive = alive ? alive : 3; c->pak = h; c->size = size;
  c->card = (const CollCard *)(h + cardOff); c->ncard = ncard;
  c->wasp = (const CollWasp *)(h + waspOff); c->nwasp = nwasp;
  c->gag = (const CollGag *)(h + gagOff); c->ngag = ngag;
  c->tex = h + texOff; c->ntex = ntex;
  if (sprSize) {
    if (spInitResident(&c->sp, h + sprOff, sprSize, detachedKeys)) return -4;
    c->haveSp = 1;
  }
  for (unsigned i = 0; i < ncard; i++) if (c->haveSp && c->card[i].inst >= c->sp.ninst) { collectFree(c); return -5; }
  for (unsigned i = 0; i < nwasp; i++) if (c->haveSp && c->wasp[i].inst >= c->sp.ninst) { collectFree(c); return -5; }
  for (unsigned i = 0; i < ngag; i++) if (c->gag[i].inst >= 0 && (!c->haveSp || (unsigned)c->gag[i].inst >= c->sp.ninst)) { collectFree(c); return -5; }
#ifndef FOOT_FX_OLD
  if (u[0] >= 2) {
    if (size < 80) { collectFree(c); return -3; }
    c->kickFx = (int)((const uint32_t *)h)[16]; c->shockFx = (int)((const uint32_t *)h)[17];
    if ((c->kickFx >= 0 && (!c->haveSp || (unsigned)c->kickFx >= c->sp.ninst)) ||
        (c->shockFx >= 0 && (!c->haveSp || (unsigned)c->shockFx >= c->sp.ninst))) { collectFree(c); return -5; }
  }
#endif
#ifndef WASP_BOLT_OLD
  if (u[0] >= 3) {
    if (size < 80) { collectFree(c); return -3; }
    c->bolt0 = (int)((const uint32_t *)h)[18]; c->nbolt = ((const uint32_t *)h)[19];
    if (c->nbolt > COLL_MAXBOLT || (c->nbolt && (c->bolt0 < 0 || !c->haveSp || (unsigned)c->bolt0 > c->sp.ninst ||
        c->nbolt > c->sp.ninst - (unsigned)c->bolt0))) { collectFree(c); return -5; }
    for(unsigned i=0;i<c->nwasp;i++) if(c->nbolt && (!(c->wasp[i].boltSpeed>0) || !(c->wasp[i].boltLife>0) ||
      !isfinite(c->wasp[i].boltSpeed) || !isfinite(c->wasp[i].boltLife))) { collectFree(c); return -3; }
  }
#endif
  collectNewGame(c);
  return 0;
}

int collectInit(Collect *c, const void *pak, unsigned size) { return collectInitResident(c, pak, size, 0); }

void collectFree(Collect *c) { gmDirty = 1; if (c->haveSp) spFree(&c->sp); c->haveSp = 0; }

static void hide(Collect *c, int inst) { if (c->haveSp && inst >= 0 && (unsigned)inst < c->sp.ninst) { c->sp.run[inst].alive = 0; c->sp.run[inst].colOn = c->sp.run[inst].col2On = 0; } }

void collectNewGame(Collect *c) {
  CollState *s = &c->st;
  memset(s, 0, sizeof *s);
  s->lastRoom = -2; s->rng = 0x5eed1u + (unsigned)c->level; s->pRoom = -1;
  hide(c, c->kickFx); hide(c, c->shockFx);
  for(unsigned i=0;i<c->nbolt;i++) hide(c,c->bolt0+i);
  for (unsigned i = 0; i < c->nwasp; i++) { hide(c, c->wasp[i].inst); if (c->wasp[i].flags & COLL_WASP_SHIELD) hide(c, c->wasp[i].shield); }   // осы появляются по близости
  for (unsigned i = 0; i < c->ncard; i++) if (c->haveSp) spEnter(&c->sp, c->card[i].inst, 0);
  for (unsigned g = 0; g < c->ngag; g++) if (c->gag[g].inst >= 0) spEnter(&c->sp, (unsigned)c->gag[g].inst, 0);
}

int collectInstRoom(const Collect *c, unsigned inst) {
  if ((int)inst == c->kickFx || (int)inst == c->shockFx) return c->st.pRoom;
  int g = collectGagOf(c, inst);
  return g < 0 ? -1 : c->gag[g].room;
}
#ifndef COLL_GAGMAP_OLD
#define COLL_GAGMAP_OLD 0
#endif
// №637: обратная таблица «экземпляр -> гэг» (collDraw спрашивал на каждый экземпляр дважды за кадр, линейный поиск по всем гэгам: 126 × 33 × 2 × 2 циклов); gag[] лежит в пакете и после загрузки не меняется
static const CollGag *gmKeyG; static unsigned gmKeyN, gmKeyI; static signed char *gmTab;   // tab[inst] = индекс гэга или -1 (гэгов < 128); gmDirty — после collectInit/collectFree
int collectGagOf(const Collect *c, unsigned inst) {
#if !COLL_GAGMAP_OLD
#define keyG gmKeyG
#define keyN gmKeyN
#define keyI gmKeyI
#define tab gmTab
  if (c->haveSp && c->ngag < 127 && (gmDirty || c->gag != keyG || c->ngag != keyN || c->sp.ninst != keyI || !tab)) {
    free(tab); tab = (signed char *)malloc(c->sp.ninst ? c->sp.ninst : 1);
    if (tab) { memset(tab, -1, c->sp.ninst ? c->sp.ninst : 1); for (unsigned g = c->ngag; g-- > 0;) if (c->gag[g].inst >= 0 && (unsigned)c->gag[g].inst < c->sp.ninst) tab[c->gag[g].inst] = (signed char)g; }
    keyG = c->gag; keyN = c->ngag; keyI = c->sp.ninst; gmDirty = 0;
  }
  if (tab && c->haveSp && c->ngag < 127 && c->gag == keyG && c->ngag == keyN && c->sp.ninst == keyI) return inst < keyI ? tab[inst] : -1;
#undef keyG
#undef keyN
#undef keyI
#undef tab
#endif
  for (unsigned g = 0; g < c->ngag; g++) if (c->gag[g].inst == (int)inst) return (int)g;
  return -1;
}
int collectSparkle(const Collect *c, unsigned g) { return g < c->ngag && c->gag[g].sparkle && !c->st.gagFound[g]; }

// ---------------------------------------------------------------- прогресс
unsigned collectCards(const Collect *c, int level) { return level > 0 && level < COLL_LEVELS ? c->st.cards[level] : 0; }
uint32_t collectWasps(const Collect *c, int level) { return level > 0 && level < COLL_LEVELS ? c->st.wasps[level] : 0; }
uint64_t collectGags(const Collect *c, int level) { return level > 0 && level < COLL_LEVELS ? c->st.gags[level] : 0; }
static int pop64(uint64_t v) { int n = 0; while (v) { v &= v - 1; n++; } return n; }
int collectCount(const Collect *c, int level, int what, int *total) {
  int t = 0, n = 0;
  if (level == c->level) {
    if (what == COLL_CARDS) t = (int)c->ncard;
    else if (what == COLL_WASPS) t = (int)c->nwasp;
    else for (unsigned g = 0; g < c->ngag; g++) t += c->gag[g].coins > 0;
  }
  if (level > 0 && level < COLL_LEVELS)
    n = what == COLL_CARDS ? pop64(c->st.cards[level]) : what == COLL_WASPS ? pop64(c->st.wasps[level]) : pop64(c->st.gags[level]);
  if (total) *total = t;
  return n;
}
void collectSetProgress(Collect *c, int level, unsigned cards, uint32_t wasps, uint64_t gags) {
  if (level <= 0 || level >= COLL_LEVELS) return;
  CollState *s = &c->st;
  s->cards[level] = (uint8_t)cards; s->wasps[level] = wasps; s->gags[level] = gags;
  if (level != c->level) return;
  for (unsigned i = 0; i < c->ncard; i++) {   // собранные карточки не висят в мире
    unsigned k = c->card[i].id % 10; s->cardGot[i] = k >= 1 && (cards >> (k - 1) & 1);
    if (s->cardGot[i]) hide(c, c->card[i].inst); else if (c->haveSp) spEnter(&c->sp, c->card[i].inst, 0);
  }
  for (unsigned i = 0; i < c->nwasp; i++) { s->waspSt[i] = (wasps >> i & 1) ? 2 : 0; hide(c, c->wasp[i].inst); }
  for (unsigned g = 0; g < c->ngag; g++) {
    s->gagFound[g] = (uint8_t)(gags >> g & 1); s->gagCoin[g] = s->gagFound[g];
    if (s->gagFound[g] && c->gag[g].persist && c->gag[g].cycle == COLL_SINGLE && !collectGagReset96) {   // сыгранный single остаётся на последнем кадре (Build 96: нет — гэг строится заново при входе, см. collectGagReset96)
      s->gagSt[g] = 2; if (c->gag[g].inst >= 0) spEnter(&c->sp, (unsigned)c->gag[g].inst, 2);
    }
  }
}

// ---------------------------------------------------------------- карточки
// Все осы в наблюдении (ATTRACTION) переходят в агрессию: в оригинале событие «подобрана вещь» (карточка, ключ, еда, нитро) снимает у наблюдающей осы исключительность
// поведения наблюдения, дальше работает атака; расстояние до осы не проверяется (круг 7, P04 Д12 теперь по оригиналу: ключ тоже). Звонит cardsStep и game.c при подборе ключа
void collectWatchersAggro(Collect *c) {
  CollState *s = &c->st;
  for (unsigned q = 0; q < c->nwasp; q++) if (s->waspSt[q] == 1 && !s->waspAggr[q]) { s->waspAggr[q] = 1; s->waspLastFly[q] = -1e9f; }
}
static void cardsStep(Collect *c, const CollPlayer *p, const Car *car) {
  CollState *s = &c->st;
  if (p->room >= 0) return;
  for (unsigned i = 0; i < c->ncard; i++) {
    if (s->cardGot[i]) continue;
    const CollCard *k = &c->card[i];
    float dx = p->x - k->x, dy = p->y + .9f - k->y, dz = p->z - k->z, r = k->r + (CARD_EXACT_OLD ? (p->onFoot ? CARD_REACH_FOOT : CARD_REACH_CAR) : (!p->onFoot && !CARD_CAR15_OLD ? 1.5f : 0.f));   // центр игрока: ступни + 0.9 м; круг 7: из машины сфера 1,5 м вокруг центра машины касается сферы карточки (как в оригинале: сфера 1,5 м), пешком — точка строго внутри
    int in = dx * dx + dy * dy + dz * dz <= r * r;
    if (!in && !CARD_EXACT_OLD && !p->onFoot && car && car->p.halfL > .1f) {   // круг 2, P05:9: из машины ещё четыре угла кузова — два задних нижних и два передних верхних (полуразмеры из Car)
      float fx = sinf(car->h), fz = cosf(car->h), rx = fz, rz = -fx, hl = car->p.halfL, hw = car->p.halfW > .1f ? car->p.halfW : 1.f;
      for (int q = 0; q < 4 && !in; q++) {
        float sd = (q & 1) ? hw : -hw, lg = (q & 2) ? hl : -hl, hy = (q & 2) ? 1.5f : .2f;
        float qx = p->x + fx * lg + rx * sd - k->x, qy = p->y + hy - k->y, qz = p->z + fz * lg + rz * sd - k->z;
        in = qx * qx + qy * qy + qz * qz <= r * r;
      }
    }
    if (!in) continue;
    s->cardGot[i] = 1; hide(c, k->inst);
    if (!WASP_CARD_AGGR_OLD) collectWatchersAggro(c);   // P04 Д12: на любом расстоянии
    unsigned lv = k->id / 10, n = k->id % 10;
    if (lv > 0 && lv < COLL_LEVELS && n >= 1 && n <= 8) s->cards[lv] |= (uint8_t)(1u << (n - 1));
    push(c, CE_CARD, (int)i, k->x, k->y, k->z);
    int tot, got = collectCount(c, c->level, COLL_CARDS, &tot);
    if (got == tot && tot) push(c, CE_CARDS_DONE, (int)i, k->x, k->y, k->z);
  }
}

// ---------------------------------------------------------------- осы
static float angDiff(float a, float b) { float d = fmodf(a - b + 3.14159265f, 6.2831853f); if (d < 0) d += 6.2831853f; return d - 3.14159265f; }
static void setYaw(SpRun *r, float yaw) {   // поворот вокруг Y: R по столбцам, модель смотрит в +Z
  float cs = cosf(yaw), sn = sinf(yaw);
  r->R[0] = cs; r->R[1] = 0; r->R[2] = -sn; r->R[3] = 0; r->R[4] = 1; r->R[5] = 0; r->R[6] = sn; r->R[7] = 0; r->R[8] = cs;
  r->poseValid = 0;
}

static void shieldFollow(Collect *c, unsigned i) {   // щит — на осе: то же место и поворот
  const CollWasp *w = &c->wasp[i]; const SpRun *r = &c->sp.run[w->inst]; SpRun *q = &c->sp.run[w->shield];
  q->x = r->x; q->y = r->y; q->z = r->z; memcpy(q->R, r->R, sizeof q->R); q->poseValid = 0;
}
static void boltImpact(Collect *c, unsigned k, int hit, const CollPlayer *p, Homer *h, Coins *coins) {
  unsigned owner=c->st.boltOwner[k]; SpRun *r=c->sp.run+c->bolt0+k;
  c->st.boltOn[k]=2;  // source FEATHER_TOUCH -> explosion, KILL SPEED, REMOVE_FROM_WORLD
  if(!spEvent(&c->sp,c->bolt0+k,9)) hide(c,c->bolt0+k);
  if(hit==2) return;   // сверка g11b, P04 W5: попал в другую осу — взрыв без «мимо» и без удара током (оса убита событием DESTROYED у вызывающего)
  if(hit) {
    c->st.zaps++; push(c,CE_WASP_ZAP,owner,p->x,p->y,p->z);
    // сверка g11b, P04 W5: удар током оглушает без отлёта (было: отлёт спиной силой 1, как у наезда). Сила 0,05 — отлёт меньше 0,3 м/с; потеря монет и длительность оглушения (5 монет, 0,5 с) — game.c и homer.c, передано
    if(h) { if(!HOMER_STUN_OLD) homerStun(h,.5f); else homerHurt(h,r->x,r->z,WASP_BOLT_HIT_OLD ? 1.f : .05f); }   // круг 2, ГОМЕР 8.6 (g04): оглушение 0,5 с с мерцанием, без отлёта (homerStun)
    if(coins && !HOMER_STUN_OLD && !WASP_ZAP_COINS_OLD) {   // круг 2, P09:Д18: 5 монет (game.c на оглушение не реагирует — десятки там остались для наезда)
      int n=coins->collected<5?coins->collected:5; coins->collected-=n; coinsDrop(coins,n,p->x,p->y+.9f,p->z,p->y);
    }
#ifndef FOOT_FX_OLD
    if(c->shockFx>=0 && h) {spEnter(&c->sp,c->shockFx,1);c->sp.run[c->shockFx].alive=1;c->st.shockOn=1;}
#endif
  } else {c->st.boltMissed++;push(c,CE_WASP_MISS,owner,r->x,r->y,r->z);}
}
// расстояние вдоль отрезка a-b до ближайшей точки к q: доля 0..1 и расстояние
static float segNear(const float *a, const float *b, const float *q, float *dist) {
  float ab[3] = {b[0]-a[0], b[1]-a[1], b[2]-a[2]}, aq[3] = {q[0]-a[0], q[1]-a[1], q[2]-a[2]};
  float l2 = ab[0]*ab[0] + ab[1]*ab[1] + ab[2]*ab[2], t = l2 > 1e-9f ? (aq[0]*ab[0] + aq[1]*ab[1] + aq[2]*ab[2]) / l2 : 0.f;
  t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
  float d[3] = {a[0]+ab[0]*t-q[0], a[1]+ab[1]*t-q[1], a[2]+ab[2]*t-q[2]};
  *dist = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
  return t;
}
static void boltsStep(Collect *c,const CollPlayer *p,Homer *h,const ColWorld *world,Coins *coins,float dt) {
#ifndef WASP_BOLT_OLD
  for(unsigned k=0;k<c->nbolt;k++) {
    if(!c->st.boltOn[k]) continue;
    SpRun *r=c->sp.run+c->bolt0+k;
    if(!r->alive) {c->st.boltOn[k]=0;continue;}
    if(c->st.boltOn[k]==2) continue;
    const CollWasp *w=c->wasp+c->st.boltOwner[k];
    c->st.boltT[k]+=dt;
    int gone = p->room>=0;
    if(WASP_BOLT_KMH_OLD) gone = gone || c->st.boltT[k]>=w->boltLife;
    else {   // сверка g11b, P04 W1: времени жизни у снаряда нет — гаснет дальше 65 м от игрока
      float ex=r->x-p->x, ey=r->y-p->y, ez=r->z-p->z; gone = gone || ex*ex+ey*ey+ez*ez>WASP_SV_FAR*WASP_SV_FAR;
    }
    if(gone) {hide(c,c->bolt0+k);c->st.boltOn[k]=0;c->st.boltMissed++;continue;}
    const SpType *t=c->sp.type+c->sp.inst[c->bolt0+k].type;
    const SpCol *shape=t->nCol ? c->sp.col+t->firstCol : 0;
    float off[3]={0,0,0};
    if(shape) for(int j=0;j<3;j++) off[j]=r->R[j]*shape->c[0]+r->R[j+3]*shape->c[1]+r->R[j+6]*shape->c[2];
    float a[3]={r->x+off[0],r->y+off[1],r->z+off[2]}, b[3];
    for(int j=0;j<3;j++) b[j]=a[j]+c->st.boltV[k][j]*dt;
    float old[3]={c->st.px,c->st.py,c->st.pz}, now[3]={p->x,p->y,p->z};
    float radius=shape && shape->kind==1 ? shape->h[0] : 0;
    // сверка g11b, P04 W5: прыгающего игрока снаряд пропускает (было: бьёт в прыжке тоже)
    float hit=(p->onFoot && (WASP_BOLT_HIT_OLD || c->st.pJump<.2f)) ? boltCapsule(a,b,old,now,HOMER_RADIUS,HOMER_HEIGHT,radius) : 2;
    float wall=1.f;
    float travel=sqrtf(c->st.boltV[k][0]*c->st.boltV[k][0]+c->st.boltV[k][1]*c->st.boltV[k][1]+c->st.boltV[k][2]*c->st.boltV[k][2])*dt;
    if(WASP_BOLT_HIT_OLD) wall=world ? colRayFrac(world,a,b,0) : 1;
    else if(c->st.boltEnd[k]>0.f && c->st.boltRun[k]+travel>=c->st.boltEnd[k])   // сверка g11b, P04 Д10: стена проверена при выстреле на 25 м; точка взрыва на 1,75 м за поверхностью
      wall=travel>1e-6f ? fmaxf(0.f,(c->st.boltEnd[k]-c->st.boltRun[k])/travel) : 0.f;
    int hw=-1; float hwT=2.f;
    if(!WASP_BOLT_HIT_OLD) for(unsigned j=0;j<c->nwasp;j++) {   // другая оса на пути: сила 400 убивает (P04 W5)
      if(j==c->st.boltOwner[k] || c->st.waspSt[j]!=1) continue;
      const SpRun *q=&c->sp.run[c->wasp[j].inst]; if(!q->alive || q->state==WS_DEAD) continue;
      float qq[3]={q->x,q->y,q->z},dd; float tt=segNear(a,b,qq,&dd);
      if(dd<1.f && tt<hwT) {hwT=tt;hw=(int)j;}
    }
    float at=hit<=wall && hit<=1 ? hit : wall;
    if(hw>=0 && hwT<at) at=hwT;
    int whit=0; float dv[3]={c->st.boltV[k][0],c->st.boltV[k][1],c->st.boltV[k][2]}, dvl=sqrtf(dv[0]*dv[0]+dv[1]*dv[1]+dv[2]*dv[2]);   // круг 7: подвижные предметы, пешеходы, трафик — по крюку мира
    if(!WASPBOLTWORLD_OLD && !WASP_BOLT_HIT_OLD && svBoltWorld && dvl>1e-6f) { dv[0]/=dvl; dv[1]/=dvl; dv[2]/=dvl;
      float wt=svBoltWorld(a,b,dv,0); if(wt<=1.f && wt<at) { at=wt; whit=1; } }
    r->x+=c->st.boltV[k][0]*dt*at;r->y+=c->st.boltV[k][1]*dt*at;r->z+=c->st.boltV[k][2]*dt*at;
    c->st.boltRun[k]+=travel*at;
    if(whit) { svBoltWorld(a,b,dv,1); boltImpact(c,k,2,p,h,coins); }   // круг 7: взрыв у объекта мира, удар применён хозяином мира
    else if(hw>=0 && hwT<=hit && hwT<=wall) {
      spEvent(&c->sp,c->wasp[hw].inst,SP_EV_DESTROYED); boltImpact(c,k,2,p,h,coins);
    }
    else if(hit<=wall && hit<=1) boltImpact(c,k,1,p,h,coins);
    else if(wall<1) boltImpact(c,k,0,p,h,coins);
  }
#endif
}
// to — точка, в которую целится оса (для наклона оси); направление выстрела — вдоль курса осы (P04 W4: «куда оса смотрит в момент выстрела»)
static void boltFire(Collect *c,unsigned owner,const CollPlayer *p,const ColWorld *world,float toY) {
  const CollWasp *w=c->wasp+owner; const SpRun *from=c->sp.run+w->inst;
  unsigned pool=c->nbolt;
  if(!WASP_BOLT_KMH_OLD) {   // W1: второе число команды — размер пула снарядов (не больше 19); все в полёте — выстрела нет
    unsigned n=(unsigned)(w->boltLife+.5f); if(n<1) n=1; if(n>19) n=19; if(n<pool) pool=n;
    unsigned busy=0; for(unsigned k=0;k<c->nbolt;k++) busy+=c->st.boltOn[k]!=0;
    if(busy>=pool) {c->st.boltPoolFull++;return;}
  }
  for(unsigned k=0;k<c->nbolt;k++) if(!c->st.boltOn[k]) {
    SpRun *r=c->sp.run+c->bolt0+k;spEnter(&c->sp,c->bolt0+k,0);r->alive=1;r->colOn=r->col2On=0;
    r->x=from->x;r->y=from->y;r->z=from->z;memcpy(r->R,from->R,sizeof r->R);
    float aim[3]={p->x-r->x,p->y+1.f-r->y,p->z-r->z};
    if(!WASP_AIM_OLD) {   // ось выстрела: курс осы по горизонтали (R[6],R[8] — её «вперёд»), наклон — к цели (оса наводится по обеим осям; курс отстаёт на rot °/с)
      float hx=from->R[6],hz=from->R[8],hl=sqrtf(hx*hx+hz*hz); if(hl<1e-6f){hx=0;hz=1;hl=1;} hx/=hl;hz/=hl;
      float dxp=p->x-r->x,dzp=p->z-r->z,dh=sqrtf(dxp*dxp+dzp*dzp),pit=atan2f(toY-r->y,dh>.1f?dh:.1f),cp=cosf(pit);
      aim[0]=hx*cp;aim[1]=sinf(pit);aim[2]=hz*cp;
    }
    float len=sqrtf(aim[0]*aim[0]+aim[1]*aim[1]+aim[2]*aim[2]);
    if(len<1e-6f) {hide(c,c->bolt0+k);return;}
    float spd=WASP_BOLT_KMH_OLD ? w->boltSpeed : w->boltSpeed/3.6f;   // W1: км/ч -> м/с
    for(int j=0;j<3;j++) c->st.boltV[k][j]=aim[j]/len*spd;
    c->st.boltRun[k]=0.f;c->st.boltEnd[k]=0.f;
    if(!WASP_BOLT_HIT_OLD && world) {   // Д10: стена один раз на 25 м вперёд; взрыв на 1,75 м за поверхностью
      float a[3]={r->x,r->y,r->z},b[3]={a[0]+aim[0]/len*25.f,a[1]+aim[1]/len*25.f,a[2]+aim[2]/len*25.f};
      float f=colRayFrac(world,a,b,0); if(f<1.f) c->st.boltEnd[k]=f*25.f+(WASP_BOLT_BEHIND75_OLD?1.75f:.75f);   // круг 7: у оригинала снаряд встаёт на 0,75 м за точкой пересечения по ходу полёта (было 1,75)
    }
    c->st.boltOn[k]=1;c->st.boltOwner[k]=owner;c->st.boltT[k]=0;c->st.boltFired++;return;
  }
  c->st.boltFull++;
}
static void shieldHit(Collect *c, unsigned i) {   // удар достался щиту: s2 (держит) или s3 (пролом) — машина состояний рисунка
  CollState *s = &c->st; const CollWasp *w = &c->wasp[i];
  if (!s->shieldHp[i]) return;
  s->shieldHits++;
  if (--s->shieldHp[i]) { spEnter(&c->sp, w->shield, 2); return; }
  spEnter(&c->sp, w->shield, 3); s->shieldBreaks++; s->shieldT[i] = 0.f;
  SpRun *r = &c->sp.run[w->inst]; r->colOn = r->col2On = 1;   // оса снова бьётся машиной
}
static void shieldStep(Collect *c, unsigned i, const Car *car, float dt) {
  CollState *s = &c->st; const CollWasp *w = &c->wasp[i]; SpRun *r = &c->sp.run[w->inst];
  if (w->shield >= c->sp.ninst) return;
  shieldFollow(c, i);
  if (!s->shieldHp[i]) {   // пробит: вспышка пролома и погасание
    if (c->sp.run[w->shield].alive && (s->shieldT[i] += dt) >= COLL_SHIELD_GONE_S) hide(c, w->shield);
    return;
  }
  r->colOn = r->col2On = 0;   // пока щит цел, машина бьёт щит, не осу (spEnter состояний осы включает объём — гасим каждый кадр)
  if (s->shieldT[i] > 0.f) { s->shieldT[i] -= dt; return; }
  if (!car) return;
  float dx = r->x - car->x, dz = r->z - car->z, dy = r->y - (car->y + .8f), d = sqrtf(dx * dx + dz * dz), rr = COLL_SHIELD_R + 1.5f;
  if (d >= rr || fabsf(dy) >= COLL_SHIELD_R + 1.f || d < 1e-3f) return;
  // сверка g11b, P04 W7: порог удара машины по щиту — импульс около 1265 (масса x скорость), для машины 1500 кг сближение около 0,85 м/с (единицы импульса физики не подтверждены вторым читателем); WASP_SHIELD_OLD=1 — 2 м/с
  if ((car->vel[0] * dx + car->vel[2] * dz) / d < (WASP_SHIELD_OLD ? 2.f : .85f)) return;   // сближение (как удар стейтпропа, SP_HIT_SPEED)
  shieldHit(c, i); s->shieldT[i] = .3f;
}

static float waspSpawnDistance(const CollWasp *w) {
#ifndef COLLECT_SPAWN_OLD
  // L2/L5 disc scripts use -1 for this parameter. Squaring the sentinel made these
  // wasps appear only inside 1 m. Port rule until PS2 measurement: use the disc's
  // positive roaming radius for proximity activation, never treat -1 as metres.
  if (w->dist <= 0.f) return fmaxf(w->roam, 0.f);
#endif
  return w->dist;
}

static void waspsStepOld(Collect *c, const CollPlayer *p, Homer *homer, const Car *car, const ColWorld *world, float dt) {   // TEST81 (WASP_SV_OLD=1)
  CollState *s = &c->st;
  if (!c->haveSp) return;
  unsigned alive = 0;
  for (unsigned i = 0; i < c->nwasp; i++) if (s->waspSt[i] == 1) alive++;
  // появление: ближайшая спящая в дистанции, пока живых меньше предела (в комнате — нет: осы в городе)
  while (alive < c->maxAlive && p->room < 0) {
    int best = -1; float bd = 1e30f;
    for (unsigned i = 0; i < c->nwasp; i++) {
      if (s->waspSt[i]) continue;
      const CollWasp *w = &c->wasp[i];
      float dx = p->x - w->x, dz = p->z - w->z, d = dx * dx + dz * dz;
      if (d < waspSpawnDistance(w) * waspSpawnDistance(w) && d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) break;
    const CollWasp *w = &c->wasp[best]; SpRun *r = &c->sp.run[w->inst];
    spEnter(&c->sp, w->inst, WS_APPEAR); r->x = w->x; r->y = w->y; r->z = w->z;
    s->waspYaw[best] = atan2f(p->x - w->x, p->z - w->z); setYaw(r, s->waspYaw[best]);
    s->waspSt[best] = 1; s->waspShot[best] = 0; alive++;
    s->shieldHp[best] = 0; s->shieldT[best] = 0.f;
    if (COLL_SHIELD_ON && (w->flags & COLL_WASP_SHIELD) && w->shield < c->sp.ninst) {   // AddShield: щит с появления, на осе
      s->shieldHp[best] = WASP_SHIELD_OLD ? COLL_SHIELD_HP : 1;   // сверка g11b, P04 W7: первый же сильный удар ломает щит, следующий убивает осу (было: щит держал 2 удара)
      spEnter(&c->sp, w->shield, 0); shieldFollow(c, (unsigned)best);
    }
    push(c, CE_WASP_SPAWN, best, w->x, w->y, w->z);
  }
  for (unsigned i = 0; i < c->nwasp; i++) {
    if (s->waspSt[i] != 1) continue;
    const CollWasp *w = &c->wasp[i]; SpRun *r = &c->sp.run[w->inst];
    if (r->state == WS_DEAD) {   // машина или пинок: засчитана; монеты и исчезновение — вызовы состояния из данных
      s->waspSt[i] = 2; s->kills++; if (c->level > 0 && c->level < COLL_LEVELS) s->wasps[c->level] |= 1u << i;
      if (w->flags & COLL_WASP_SHIELD) hide(c, w->shield);
      push(c, CE_WASP_DEAD, (int)i, r->x, r->y + .5f, r->z);
      int tot, got = collectCount(c, c->level, COLL_WASPS, &tot);
      if (got == tot && tot) push(c, CE_WASPS_DONE, (int)i, r->x, r->y, r->z);
      continue;
    }
    float dx = p->x - r->x, dz = p->z - r->z, dh = sqrtf(dx * dx + dz * dz);
    float sx = p->x - w->x, sz = p->z - w->z;
    if (p->room >= 0 || sx * sx + sz * sz > (waspSpawnDistance(w) + WASP_DESPAWN) * (waspSpawnDistance(w) + WASP_DESPAWN)) {   // ушёл далеко: уснула
      s->waspSt[i] = 0; hide(c, w->inst); if (w->flags & COLL_WASP_SHIELD) hide(c, w->shield); continue;
    }
    // поворот к игроку
    float want = atan2f(dx, dz), turn = w->rot * 0.01745329f * dt, da = angDiff(want, s->waspYaw[i]);
    s->waspYaw[i] += da > turn ? turn : da < -turn ? -turn : da; setYaw(r, s->waspYaw[i]);
    // ATTRACTION
    if (!(w->flags & 1) && w->attrSpeed > 0.f && r->state != WS_APPEAR) {
      float ty = p->y + (p->onFoot ? WASP_HEAD_FOOT : WASP_HEAD_CAR);
      float d3 = sqrtf(dx * dx + (ty - r->y) * (ty - r->y) + dz * dz);
      if (d3 > w->attrMax && dh > 1e-3f) {
        float v = w->attrSpeed / 3.6f * dt, k = v / d3;
        float nx = r->x + dx * k, nz = r->z + dz * k, ny = r->y + (ty - r->y) * k;
        float ox = nx - w->x, oz = nz - w->z, od = sqrtf(ox * ox + oz * oz);
        if (od > w->roam) { nx = w->x + ox / od * w->roam; nz = w->z + oz / od * w->roam; }   // не дальше радиуса полёта
        float g;
        if (world && colGround(world, nx, nz, ny + 2.f, ny - 30.f, &g, 0) && ny < g + 1.f) ny = g + 1.f;
        r->x = nx; r->y = ny; r->z = nz;
      }
    }
    // ATTACK_PLAYER: пешком, в дальности и в дуге
    float d3 = sqrtf(dx * dx + (p->y + 1.f - r->y) * (p->y + 1.f - r->y) + dz * dz);
    float off = fabsf(angDiff(atan2f(dx, dz), s->waspYaw[i])) * 57.29578f;
    int inArc = off <= fmaxf(w->atkArc, 1.f), inRange = d3 <= w->atkRange;
    if (r->state == WS_FLY && p->onFoot && inRange && inArc) { spEnter(&c->sp, w->inst, WS_CHARGE); push(c, CE_WASP_CHARGE, (int)i, r->x, r->y, r->z); }
    else if (r->state == WS_CHARGED) {
      if (p->onFoot && d3 <= w->atkRange * 1.2f) { spEnter(&c->sp, w->inst, WS_FIRE); s->waspShot[i] = 0; }
      else spEnter(&c->sp, w->inst, WS_FLY);
    } else if (r->state == WS_FIRE && !s->waspShot[i]) {
      // момент выстрела — вызов FIRE_ENERGY_BOLT состояния в данных (op SP_OP_BOLT, время в секундах)
      const SpType *T = &c->sp.type[c->sp.inst[w->inst].type]; const SpState *S = &c->sp.state[T->firstState + r->state];
      float at = 1.f;
      for (unsigned k = 0; k < S->nCall; k++) if (c->sp.call[S->firstCall + k].op == SP_OP_BOLT) at = c->sp.call[S->firstCall + k].time;
      if (r->t >= at) {
        s->waspShot[i] = 1; push(c, CE_WASP_FIRE, (int)i, r->x, r->y, r->z);
#ifndef WASP_BOLT_OLD
        if(c->nbolt) boltFire(c,i,p,world,p->y+1.f);
        else
#endif
        if (p->onFoot && d3 <= w->atkRange * 1.2f && off <= WASP_ZAP_ARC) {
          s->zaps++; push(c, CE_WASP_ZAP, (int)i, p->x, p->y, p->z);
          if (homer) homerHurt(homer, r->x, r->z, 1.f);
#ifndef FOOT_FX_OLD
          if (c->shockFx >= 0 && homer) { spEnter(&c->sp, c->shockFx, 1); c->sp.run[c->shockFx].alive = 1; s->shockOn = 1; }
#endif
        }
      }
    }
    if (w->flags & COLL_WASP_SHIELD) shieldStep(c, i, car, dt);
  }
}

// ---- сверка g11b: новая логика ос (P04 W2–W10, Д7, Д8, СВЕРКА_ГОМЕР 8.x). Свой код по описанию поведения из отчётов; числа — из данных диска и отчётов.
#ifndef WASP_SV_OLD
#define WASP_SV_OLD 0   // 1 — весь шаг ос как в TEST81 (waspsStepOld)
#endif
// круг 7 (сверка с оригиналом, поведение агрессивной осы): (а) пока идёт любой гэг (событие начала гэга — до конца), оса в агрессии ничего нового не делает: не стартует перелёт, не заряжается и не стреляет
// (уже начатый перелёт и выстрел доигрываются); (б) заряженная оса, до игрока которой дальше дальности атаки, летит в позицию атаки сразу, не дожидаясь интервала. WASP_GAGPAUSE_OLD=1 / WASP_RANGEFLY_OLD=1 — как было
#ifndef WASP_GAGPAUSE_OLD
#define WASP_GAGPAUSE_OLD 0
#endif
#ifndef WASP_RANGEFLY_OLD
#define WASP_RANGEFLY_OLD 0
#endif
#define WASP_HFOV_HALF .70f   // 8.1: «оса в кадре» — полуугол по горизонтали от курса камеры, рад (кадр 16:9, ≈77° у машины; запас)
static float urand(CollState *s) { return (float)(rnd(s) & 0xFFFFu) * (1.f / 65536.f); }
static int groundUnder(const ColWorld *world, float x, float y, float z, float *g) { return world && colGround(world, x, z, y + 2.f, y - 30.f, g, 0); }
static int waspOnScreen(const SpRun *r, const CollPlayer *p, const Homer *h) {   // 8.1: стрельба только пока сама оса в кадре камеры; без Гомера (боты) — всегда
  if (!h) return 1;
  float cy = h->camYaw, ex = p->x - sinf(cy) * 4.f, ez = p->z - cosf(cy) * 4.f, dx = r->x - ex, dz = r->z - ez;   // глаз ≈ 4 м позади игрока по курсу камеры
  if (dx * dx + dz * dz < 1e-4f) return 1;
  return fabsf(angDiff(atan2f(dx, dz), cy)) < WASP_HFOV_HALF;
}
// старт перелёта: kind 1 позиция атаки, 2 уход, 3 отлёт. Путь проверяется лучом COL1 один раз при старте; закрыт — обходы вправо и влево на 2, 4,5, 7 м (6 попыток), нет — стоит (W6, Д8)
static int waspFlightStart(Collect *c, unsigned i, int kind, float tx, float ty, float tz, const ColWorld *world) {
  CollState *s = &c->st; const SpRun *r = &c->sp.run[c->wasp[i].inst];
  float a[3] = {r->x, r->y, r->z}, b[3] = {tx, ty, tz}, g;
  if (!WASP_FLIGHT_OLD && groundUnder(world, tx, ty, tz, &g) && ty < g + WASP_MIN_ALT) b[1] = ty = g + WASP_MIN_ALT;
  colRayFenceBox = !WASP_FENCEBOX_OLD;   // круг 5 (P11-Д5): заборы перекрывают путь рамкой по двум концам
  int ok = WASP_FLIGHT_OLD || !world || colRayFrac(world, a, b, 0) >= 1.f;
  if (!ok) {
    static const float side[6] = {2.f, -2.f, 4.5f, -4.5f, 7.f, -7.f};
    float dx = tx - a[0], dz = tz - a[2], dl = sqrtf(dx * dx + dz * dz); if (dl < 1e-3f) { dx = 0.f; dz = 1.f; dl = 1.f; } dx /= dl; dz /= dl;
    for (int k = 0; k < 6 && !ok; k++) {
      float q[3] = {tx - dz * side[k], ty, tz + dx * side[k]};
      if (groundUnder(world, q[0], q[1], q[2], &g)) q[1] = fmaxf(q[1], g + WASP_MIN_ALT + urand(s) * 2.f);   // точка обхода: 2 м плюс случайные 0..2 над землёй
      if (colRayFrac(world, a, q, 0) >= 1.f) { b[0] = q[0]; b[1] = q[1]; b[2] = q[2]; ok = 1; }
    }
  }
  colRayFenceBox = 0;
  if (!ok) { s->waspLastFly[i] = s->wclock; return 0; }   // путь закрыт: стоит; интервал считается заново
  s->waspTo[i][0] = b[0]; s->waspTo[i][1] = b[1]; s->waspTo[i][2] = b[2]; s->waspFly[i] = (uint8_t)kind;
  return 1;
}
static void waspFlightStep(Collect *c, unsigned i, float dt) {
  CollState *s = &c->st; SpRun *r = &c->sp.run[c->wasp[i].inst];
  float kmh = s->waspFly[i] == 2 && c->level > 0 && c->level < COLL_LEVELS ? WASP_EVADE[c->level][2] : WASP_MOVE_KMH, v = kmh / 3.6f * dt;
  float dx = s->waspTo[i][0] - r->x, dy = s->waspTo[i][1] - r->y, dz = s->waspTo[i][2] - r->z, d = sqrtf(dx * dx + dy * dy + dz * dz);
  if (d <= WASP_GOAL_R || v >= d) { r->x = s->waspTo[i][0]; r->y = s->waspTo[i][1]; r->z = s->waspTo[i][2]; s->waspFly[i] = 0; s->waspLastFly[i] = s->wclock; return; }
  r->x += dx / d * v; r->y += dy / d * v; r->z += dz / d * v;
}

static void waspsStepSv(Collect *c, const CollPlayer *p, Homer *homer, const Car *car, const ColWorld *world, float dt) {
  CollState *s = &c->st;
  if (!c->haveSp) return;
  s->wclock += dt;
  unsigned alive = 0;
  for (unsigned i = 0; i < c->nwasp; i++) if (s->waspSt[i] == 1) alive++;
  const float pcy = p->y + .9f;   // центр игрока для сферы-триггера появления
  // появление (W2): игрок вошёл в сферу радиуса «пятого аргумента» вокруг локатора; живых не больше PreallocateActors; в комнате осы не появляются
  while (alive < c->maxAlive && p->room < 0) {
    int best = -1; float bd = 1e30f;
    for (unsigned i = 0; i < c->nwasp; i++) {
      if (s->waspSt[i]) continue;
      const CollWasp *w = &c->wasp[i];
      float rad = WASP_SPAWN_ORIG_OLD ? waspSpawnDistance(w) : fmaxf(w->roam, 0.f);
      float dx = p->x - w->x, dy = WASP_SPAWN_ORIG_OLD ? 0.f : pcy - w->y, dz = p->z - w->z, d = dx * dx + dy * dy + dz * dz;
      if (d < rad * rad && d < bd) { bd = d; best = (int)i; }
    }
    if (best < 0) break;
    const CollWasp *w = &c->wasp[best]; SpRun *r = &c->sp.run[w->inst];
    spEnter(&c->sp, w->inst, WS_APPEAR); r->x = w->x; r->y = w->y; r->z = w->z;   // в центре сферы (в точке локатора)
    s->waspYaw[best] = atan2f(p->x - w->x, p->z - w->z); setYaw(r, s->waspYaw[best]);
    s->waspSt[best] = 1; s->waspShot[best] = 0; alive++;
    s->waspBob[best][0] = s->waspBob[best][1] = s->waspBob[best][2] = 0.f; s->waspAggr[best] = 0; s->waspFly[best] = 0; s->waspLastFly[best] = -1e9f; s->waspNear[best] = 0;
    s->shieldHp[best] = 0; s->shieldT[best] = 0.f;
    if (COLL_SHIELD_ON && (w->flags & COLL_WASP_SHIELD) && w->shield < c->sp.ninst) {
      s->shieldHp[best] = WASP_SHIELD_OLD ? COLL_SHIELD_HP : 1;
      spEnter(&c->sp, w->shield, 0); shieldFollow(c, (unsigned)best);
    }
    push(c, CE_WASP_SPAWN, best, w->x, w->y, w->z);
  }
  const float jump = s->pJump;
  const float ty = p->y - jump + 1.2f;   // 8.5: цель по высоте — игрок +1,2 м без высоты прыжка
  int gagBusy = 0; if (!WASP_GAGPAUSE_OLD) for (unsigned g_ = 0; g_ < c->ngag; g_++) if (s->gagSt[g_] == 1) { gagBusy = 1; break; }   // круг 7: идёт гэг — агрессия ждёт
  for (unsigned i = 0; i < c->nwasp; i++) {
    if (s->waspSt[i] != 1) continue;
    const CollWasp *w = &c->wasp[i]; SpRun *r = &c->sp.run[w->inst];
    if (r->state == WS_DEAD) {   // машина или пинок: засчитана
      s->waspSt[i] = 2; s->kills++; if (c->level > 0 && c->level < COLL_LEVELS) s->wasps[c->level] |= 1u << i;
      if (w->flags & COLL_WASP_SHIELD) hide(c, w->shield);
      push(c, CE_WASP_DEAD, (int)i, r->x, r->y + .5f, r->z);
      int tot, got = collectCount(c, c->level, COLL_WASPS, &tot);
      if (got == tot && tot) push(c, CE_WASPS_DONE, (int)i, r->x, r->y, r->z);
      continue;
    }
    if (!WASP_BOB_OLD) { r->x -= s->waspBob[i][0]; r->y -= s->waspBob[i][1]; r->z -= s->waspBob[i][2]; }   // снять прошлое качание: логика работает с опорной точкой
    float dx = p->x - r->x, dz = p->z - r->z, dh = sqrtf(dx * dx + dz * dz);
    float px3 = dx * dx + (pcy - r->y) * (pcy - r->y) + dz * dz;
    int far = WASP_SPAWN_ORIG_OLD ? ((p->x - w->x) * (p->x - w->x) + (p->z - w->z) * (p->z - w->z) > (waspSpawnDistance(w) + WASP_DESPAWN) * (waspSpawnDistance(w) + WASP_DESPAWN))
                                  : px3 > WASP_SV_FAR * WASP_SV_FAR;   // W2: сама оса дальше 65 м от игрока
    if (p->room >= 0 || far) { s->waspSt[i] = 0; s->waspFly[i] = 0; hide(c, w->inst); if (w->flags & COLL_WASP_SHIELD) hide(c, w->shield); continue; }
    // P15-07, P04 W12: событие «оса приблизилась» — расстояние до игрока меньше 5 м и оса ещё в режиме наблюдения (после начала агрессии не посылается); один раз за приближение, чтобы не забивать очередь (оригинал — каждый кадр)
    if (px3 >= 5.f * 5.f || s->waspAggr[i]) s->waspNear[i] = 0;
    else if (!WASP_NEAR_OLD && !s->waspNear[i]) { s->waspNear[i] = 1; push(c, CE_WASP_NEAR, (int)i, r->x, r->y, r->z); }
    // поворот к игроку
    float want = atan2f(dx, dz), turn = w->rot * 0.01745329f * dt, da = angDiff(want, s->waspYaw[i]);
    s->waspYaw[i] += da > turn ? turn : da < -turn ? -turn : da; setYaw(r, s->waspYaw[i]);
    float d3 = sqrtf(dx * dx + (ty - r->y) * (ty - r->y) + dz * dz);   // до цели по высоте +1,2 м
    int st = w->flags & 1;   // стоящая (крыша): скорость ATTRACTION -1 — не летает, только поворачивается
    // ATTRACTION (наблюдение): дальше максимума — подлетает, ближе минимума — агрессия до конца (W3)
    if (!s->waspAggr[i] && r->state != WS_APPEAR) {
      if (!WASP_AGGR_OLD && d3 < w->attrMin) { s->waspAggr[i] = 1; s->waspLastFly[i] = -1e9f; }
      if (!(s->waspAggr[i]) && !st && w->attrSpeed > 0.f && d3 > w->attrMax && dh > 1e-3f) {
        float hy = p->y + (p->onFoot ? WASP_HEAD_FOOT : WASP_HEAD_CAR);
        float d3b = sqrtf(dx * dx + (hy - r->y) * (hy - r->y) + dz * dz);
        float v = w->attrSpeed / 3.6f * dt, k = v / d3b;
        float nx = r->x + dx * k, nz = r->z + dz * k, ny = r->y + (hy - r->y) * k;
        if (WASP_SPAWN_ORIG_OLD) { float ox = nx - w->x, oz = nz - w->z, od = sqrtf(ox * ox + oz * oz); if (od > w->roam) { nx = w->x + ox / od * w->roam; nz = w->z + oz / od * w->roam; } }   // поводок убран (W2)
        float g;
        if (groundUnder(world, nx, ny, nz, &g) && ny < g + (WASP_FLIGHT_OLD ? 1.f : WASP_MIN_ALT)) ny = g + (WASP_FLIGHT_OLD ? 1.f : WASP_MIN_ALT);
        r->x = nx; r->y = ny; r->z = nz;
      }
    }
    // агрессия: заряд сразу (W10); перелёты, уход и выстрел (W8, W9, Д7, 8.2–8.4)
    if (s->waspAggr[i] && !WASP_AGGR_OLD) {
      if (s->waspFly[i]) waspFlightStep(c, i, dt);
      else if (r->state != WS_FIRE && !gagBusy) {
        const float *ev = c->level > 0 && c->level < COLL_LEVELS ? WASP_EVADE[c->level] : WASP_EVADE[0];
        int hasEv = !WASP_EVADE_OLD && !st && ev[1] > 0.f;
        float ndx = r->x - p->x, ndz = r->z - p->z, nd = sqrtf(ndx * ndx + ndz * ndz);
        if (hasEv && (rnd(s) & 0x100u) && nd < WASP_EVADE_NEAR) {   // бросок 50 на 50 каждый кадр; уход — только если игрок ближе 4 м
          float ang = urand(s) * 6.2831853f, dist = ev[0] + urand(s) * (ev[1] - ev[0]);
          waspFlightStart(c, i, 2, r->x + sinf(ang) * dist, r->y, r->z + cosf(ang) * dist, world);
        } else if (!st && w->atkEvade >= 0.f) {
          if (nd < WASP_BACKOFF_NEAR && jump < .2f && nd > 1e-3f)   // 8.3: ближе 2 м — отлёт на 5 м прочь
            waspFlightStart(c, i, 3, r->x + ndx / nd * WASP_BACKOFF_D, r->y, r->z + ndz / nd * WASP_BACKOFF_D, world);
          else if (s->wclock - s->waspLastFly[i] >= w->atkEvade) {   // Д7: интервал от конца прошлого перелёта; первый — сразу
            float cy = homer ? homer->camYaw : atan2f(-dx, -dz), ang = cy + (urand(s) * 2.f - 1.f) * .7f, half = w->atkRange * .5f;
            waspFlightStart(c, i, 1, p->x + sinf(ang) * half, p->y - jump + 1.2f + 1.5f + urand(s) * .5f, p->z + cosf(ang) * half, world);   // 8.2: впереди игрока по взгляду камеры на половине дальности
          } else if (!WASP_RANGEFLY_OLD && r->state == WS_CHARGED && d3 >= w->atkRange && p->onFoot) {   // круг 7: заряжена, но игрок дальше дальности атаки — в позицию атаки сразу, без интервала
            float cy = homer ? homer->camYaw : atan2f(-dx, -dz), ang = cy + (urand(s) * 2.f - 1.f) * .7f, half = w->atkRange * .5f;
            waspFlightStart(c, i, 1, p->x + sinf(ang) * half, p->y - jump + 1.2f + 1.5f + urand(s) * .5f, p->z + cosf(ang) * half, world);
          }
        }
      }
      if (r->state == WS_FLY && !gagBusy) { spEnter(&c->sp, w->inst, WS_CHARGE); push(c, CE_WASP_CHARGE, (int)i, r->x, r->y, r->z); }   // W10: заряд сразу, как включена атака, без условий дальности и без игрока пешком
    }
    float off = fabsf(angDiff(atan2f(dx, dz), s->waspYaw[i])) * 57.29578f;
    int inArc, inRange;
    if (WASP_AIM_OLD) { inArc = off <= fmaxf(w->atkArc, 1.f); inRange = d3 <= w->atkRange; }
    else {   // W4: половина дуги, симметрично (косинус по модулю); дальность строго меньше; 8.1: оса в кадре
      float eff = off > 90.f ? 180.f - off : off;
      inArc = eff <= w->atkArc * .5f; inRange = d3 < w->atkRange && waspOnScreen(r, p, homer);
    }
    if (WASP_AGGR_OLD) {
      if (r->state == WS_FLY && p->onFoot && inRange && inArc) { spEnter(&c->sp, w->inst, WS_CHARGE); push(c, CE_WASP_CHARGE, (int)i, r->x, r->y, r->z); }
    }
    if (r->state == WS_CHARGED) {
      if (WASP_AGGR_OLD) {
        if (p->onFoot && d3 <= w->atkRange * 1.2f) { spEnter(&c->sp, w->inst, WS_FIRE); s->waspShot[i] = 0; }
        else spEnter(&c->sp, w->inst, WS_FLY);
      } else if (s->waspAggr[i] && !s->waspFly[i] && !gagBusy && p->onFoot && inRange && inArc) { spEnter(&c->sp, w->inst, WS_FIRE); s->waspShot[i] = 0; }   // выстрел — только при условиях W4; иначе остаётся заряженной
    } else if (r->state == WS_FIRE && !s->waspShot[i]) {
      const SpType *T = &c->sp.type[c->sp.inst[w->inst].type]; const SpState *S = &c->sp.state[T->firstState + r->state];
      float at = 1.f;
      for (unsigned k = 0; k < S->nCall; k++) if (c->sp.call[S->firstCall + k].op == SP_OP_BOLT) at = c->sp.call[S->firstCall + k].time;
      if (r->t >= at) {
        s->waspShot[i] = 1; push(c, CE_WASP_FIRE, (int)i, r->x, r->y, r->z);
#ifndef WASP_BOLT_OLD
        if (c->nbolt) boltFire(c, i, p, world, ty);
        else
#endif
        if (p->onFoot && d3 <= w->atkRange * 1.2f && off <= WASP_ZAP_ARC) {
          s->zaps++; push(c, CE_WASP_ZAP, (int)i, p->x, p->y, p->z);
          if (homer) { if (!HOMER_STUN_OLD) homerStun(homer, .5f); else homerHurt(homer, r->x, r->z, 1.f); }
#ifndef FOOT_FX_OLD
          if (c->shockFx >= 0 && homer) { spEnter(&c->sp, c->shockFx, 1); c->sp.run[c->shockFx].alive = 1; s->shockOn = 1; }
#endif
        }
      }
    }
    // Д8: не ниже 2 м над землёй или крышей (стоя на месте поднимается)
    if (!WASP_FLIGHT_OLD && !s->waspFly[i]) { float g; if (groundUnder(world, r->x, r->y, r->z, &g) && r->y < g + WASP_MIN_ALT) r->y = fminf(g + WASP_MIN_ALT, r->y + 3.f * dt); }
    if (!WASP_BOB_OLD) {   // P04 W11: качание по вертикали синусом ±0,5 м около 1 Гц и боковой дрейф ±0,5 м по правой оси осы, период 1,99 с (крылья 7,5 Гц — анимация рисунка, не процедурная)
      float bv = .5f * sinf(s->wclock * 6.2831853f * 1.f + (float)i), bd = .5f * sinf(s->wclock * 6.2831853f / 1.99f + (float)i * 1.7f);
      float rx = cosf(s->waspYaw[i]), rz = -sinf(s->waspYaw[i]);   // правая ось осы (столбец 0 матрицы поворота)
      s->waspBob[i][0] = rx * bd; s->waspBob[i][1] = bv; s->waspBob[i][2] = rz * bd;
      r->x += s->waspBob[i][0]; r->y += s->waspBob[i][1]; r->z += s->waspBob[i][2];
    }
    if (w->flags & COLL_WASP_SHIELD) shieldStep(c, i, car, dt);
  }
}
static void waspsStep(Collect *c, const CollPlayer *p, Homer *homer, const Car *car, const ColWorld *world, float dt) {
  if (WASP_SV_OLD) waspsStepOld(c, p, homer, car, world, dt); else waspsStepSv(c, p, homer, car, world, dt);
}

void collectHomerHit(Collect *c, const HomerEvent *e) {
  if (!c->haveSp || (e->type != HOMER_EV_HIT && e->type != HOMER_EV_STOMP)) return;
  for (unsigned i = 0; i < c->nwasp; i++) {
    if (c->st.waspSt[i] != 1) continue;
    unsigned in = c->wasp[i].inst; SpRun *r = &c->sp.run[in];
    if (!r->alive || r->state == WS_DEAD) continue;
    float cc[3], rr; spBounds(&c->sp, in, cc, &rr);
    if (homerHitTest(e, cc[0], cc[1], cc[2], fminf(rr, WASP_HIT_R))) {
      if (COLL_SHIELD_ON && c->st.shieldHp[i]) { shieldHit(c, i); continue; }   // щит цел — удар ему
      if (!spEvent(&c->sp, in, e->type == HOMER_EV_HIT ? SP_EV_HIT : SP_EV_STOMP)) spEvent(&c->sp, in, SP_EV_DESTROYED);
    }
  }
}

// ---------------------------------------------------------------- гэги
// №873 (Роб 23.10: «подхожу прямо к проходу с калиткой — дед стоит»): гэг деда gag_s5 (L1, дом Симпсонов) — «touch» со сферой 6 м, а вход в дом
// внутри неё. Правило порта «стоял в триггере при входе — не срабатывает, пока не выйдет» держало его до выхода из сферы, а к деду идёшь, не выходя.
// Теперь вход внутри сферы — отметка 2: сработает, как только Гомер отойдёт от места входа на GAG_ENTRY_MOVE (сам вход, загрузка — не запуск).
// GAG_ENTRY873_OLD=1 — как было
#ifndef GAG_ENTRY873_OLD
#define GAG_ENTRY873_OLD 0
#endif
#define GAG_ENTRY_MOVE 1.f
static float gagEntX, gagEntZ;
// Build 96 (Роб, правка 1226 и 08.11: автомат Squishee в Kwik «сразу включён сам»): в оригинале гэги комнаты строятся заново при КАЖДОМ входе в комнату и снимаются при выходе (менеджер интерьеров снимает гэги при выходе
// и строит заново при входе), так что на входе любой гэг стоит в начальном кадре (автомат цел, брызг нет), а запомненным остаётся только «найден» (монеты за повтор не даются). Порт же держал сыгранный single-гэг с флагом persist
// на последнем кадре между входами и при загрузке прогресса (Squishee: разбитый автомат и розовые брызги с входа в комнату). Теперь на входе в комнату сбрасываются ВСЕ гэги комнаты (в том числе persist), а при загрузке прогресса
// сыгранные гэги не остаются на последнем кадре; «найден» и монеты не меняются. GAG_RESET96_OLD=1 / flag gagreset96 0 / файл gagreset96old — как было
#ifndef GAG_RESET96_OLD
#define GAG_RESET96_OLD 0
#endif
int collectGagReset96 = !GAG_RESET96_OLD;
static int gagHere(const Collect *c, unsigned g, const CollPlayer *p) {
  const CollGag *G = &c->gag[g];
  if (G->room != p->room) return 0;
  if (G->random && !c->st.gagPick[g]) return 0;
  return 1;
}
static int gagInside(const CollGag *G, float x, float y, float z) {
  float dx = x - G->tx, dy = y - G->ty, dz = z - G->tz; return dx * dx + dy * dy + dz * dz <= G->tr * G->tr;
}
static int gagFree(const Collect *c, unsigned g) {   // можно запустить сейчас
  const CollState *s = &c->st;
  if (!GAG_MISSION_OLD && s->inMission && (c->gag[g].fmv[0] || c->gag[g].pad)) return 0;   // P16 Д7: ролик и проверка (карточки, фильм) — только в свободной езде
  if (s->gagSt[g] == 1) return 0;
  if (s->gagSt[g] == 2 && c->gag[g].cycle == COLL_SINGLE) return 0;
  if (c->gagBlock[g]) return 0;   // COLLSPLIT: гэг не появляется на глазах и не отзывается, пока не готов (канон п.11)
  return 1;
}

static int actionGag(const Collect *c, const CollPlayer *p) {   // ближайший action-гэг, в чьей сфере стоит Гомер пешком
  if (!p->onFoot) return -1;
  int best = -1; float bd = 1e30f;
  for (unsigned g = 0; g < c->ngag; g++) {
    const CollGag *G = &c->gag[g];
    if (!TICKET_CONTENT_C3_OLD && G->pad==1 && (c->st.ticketFlags&TICKET_OWNED)) continue;
    if (G->trig != COLL_ACTION || !gagHere(c, g, p) || !gagFree(c, g) || !gagInside(G, p->x, p->y, p->z)) continue;
    float dx = p->x - G->tx, dz = p->z - G->tz, d = dx * dx + dz * dz;
    if (d < bd) { bd = d; best = (int)g; }
  }
  return best;
}
int collectWantsAction(const Collect *c) {
  const CollState *s = &c->st;
  CollPlayer p = {s->px, s->py, s->pz, s->pOnFoot, s->pRoom, 0};
  return actionGag(c, &p);
}

static void gagCoins(Collect *c, unsigned g, Coins *coins, const ColWorld *world) {
  const CollGag *G = &c->gag[g];
  if (!G->coins || !coins) return;
  if (G->room >= 0) { coins->collected += G->coins; return; }   // в комнате монеты не рисуются: сразу в кошелёк (правило порта)
  float fl = G->y, gy;
  if (world && colGround(world, G->x, G->z, G->y + 2.f, G->y - 6.f, &gy, 0)) fl = gy;
  int autoPrev = coins->dropAuto;
  if (!GAG_COINS_AUTO_OLD) coins->dropAuto = 1;   // P16 Д5: «на сбор сразу» — после первого касания земли летят к игроку и засчитываются
  coinsDrop(coins, G->coins, G->x, G->y + .6f, G->z, fl);
  coins->dropAuto = autoPrev;
}

// круг 7 (P15:Д2 и P15-19, сверка с оригиналом): GagSetIntro n — пока гэг ждёт, его первые n кадров идут по кругу; запуск выходит из круга и клип идёт дальше с того же места круга.
// GagSetOutro n — когда клип дошёл до последних n кадров, они крутятся по кругу НАВСЕГДА: проигрыватель никогда не «кончается», значит нет конца гэга (событие, повторный запуск) и нет монет,
// у которых задержка «после конца» (−1): в данных так у плавящегося мороженого L1 и L4 (outro 60, intro 1) и флага L3 и L6 (80 и 80). Найденным гэг засчитывается при запуске, как и было.
// GAG_INTRO_OLD / GAG_OUTRO_OLD = 1 — как было (без кругов, конец по длительности)
#ifndef GAG_INTRO_OLD
#define GAG_INTRO_OLD 0
#endif
#ifndef GAG_OUTRO_OLD
#define GAG_OUTRO_OLD 0
#endif
static void gagStart(Collect *c, unsigned g) {
  CollState *s = &c->st; const CollGag *G = &c->gag[g];
  if(!TICKET_CONTENT_C3_OLD && G->pad){push(c,CE_TICKET,(int)g,G->x,G->y,G->z);return;}
  s->gagSt[g] = 1; s->gagT[g] = 0.f; s->gagRuns++;
  float tIntro_ = 0.f; int intro_ = !GAG_INTRO_OLD && G->inst >= 0 && G->intro > 0.f;
  if (intro_) { const SpType *T_ = &c->sp.type[c->sp.inst[G->inst].type]; float fps_ = T_->fps ? (float)T_->fps : 30.f; tIntro_ = fmodf(c->sp.run[G->inst].t * fps_, G->intro) / fps_; spLoop(&c->sp, (unsigned)G->inst, 0.f, 0.f); }
  if (G->inst >= 0) spEnter(&c->sp, (unsigned)G->inst, 1);
  if (intro_) c->sp.run[G->inst].t = tIntro_;
  push(c, CE_GAG_START, (int)g, G->x, G->y, G->z);
  if (!s->gagFound[g] && G->coins) {   // засчитывается один раз
    s->gagFound[g] = 1;
    if (c->level > 0 && c->level < COLL_LEVELS) s->gags[c->level] |= 1ull << g;
    push(c, CE_GAG_FOUND, (int)g, G->x, G->y, G->z);
    int tot, got = collectCount(c, c->level, COLL_GAGS, &tot);
    if (got == tot && tot) push(c, CE_GAGS_DONE, (int)g, G->x, G->y, G->z);
  }
}
void collectGagPlay(Collect *c, unsigned g) { if (c && g < c->ngag) gagStart(c, g); }   // №869 служебно: сценарий «gagplay N»

static void gagsStep(Collect *c, const CollPlayer *p, Coins *coins, const ColWorld *world, float dt) {
  CollState *s = &c->st;
  if (p->room != s->lastRoom) {   // вход в комнату (или в город): выбор random-гэгов и сброс непостоянных
    if (p->room >= 0) {
      unsigned idx[COLL_MAXGAG], n = 0;
      for (unsigned g = 0; g < c->ngag; g++) if (c->gag[g].room == p->room && c->gag[g].random) { s->gagPick[g] = 0; idx[n++] = g; }
      if (n) s->gagPick[idx[rnd(s) % n]] = 1;
    }
    for (unsigned g = 0; g < c->ngag; g++) {
      const CollGag *G = &c->gag[g];
      if (G->room != p->room || (G->persist && !collectGagReset96)) continue;
      s->gagSt[g] = 0; s->gagT[g] = 0.f;
      if (G->inst >= 0) spEnter(&c->sp, (unsigned)G->inst, 0);
    }
    for (unsigned g = 0; g < c->ngag; g++) s->gagIn[g] = c->gag[g].room == p->room && gagInside(&c->gag[g], p->x, p->y, p->z) ? (GAG_ENTRY873_OLD ? 1 : 2) : 0;   // стоял в триггере при входе — не срабатывает сразу (2 — до шага, №873)
    gagEntX = p->x; gagEntZ = p->z;
    s->lastRoom = p->room;
  }
  int act = p->action ? actionGag(c, p) : -1;
  for (unsigned g = 0; g < c->ngag; g++) {
    const CollGag *G = &c->gag[g];
    if (collectGagReset96 && G->room < 0 && p->room < 0 && s->gagSt[g] != 0) {   // Build 96: уличный гэг в оригинале снимается, когда игрок дальше 150 м (порог выгрузки по умолчанию, скрипты уровней его не меняют), и при возврате ближе 100 м строится заново — в начальном кадре
      float ex_ = p->x - G->x, ey_ = p->y - G->y, ez_ = p->z - G->z;
      if (ex_ * ex_ + ey_ * ey_ + ez_ * ez_ > 150.f * 150.f) { s->gagSt[g] = 0; s->gagT[g] = 0.f; s->gagIn[g] = 0; if (G->inst >= 0) spEnter(&c->sp, (unsigned)G->inst, 0); }
    }
    if (!GAG_INTRO_OLD && G->intro > 0.f && G->inst >= 0 && s->gagSt[g] == 0 && !c->sp.run[G->inst].loopOn) spLoop(&c->sp, (unsigned)G->inst, 0.f, G->intro);   // круг 7: ждущий гэг крутит вступление
    if (s->gagSt[g] == 1) {   // играет: конец по длительности данных (сцена SPR1 сама уходит в «сыграл»/«ждёт»)
      s->gagT[g] += dt;
      int outro_ = !GAG_OUTRO_OLD && G->outro > 0.f && G->inst >= 0;   // круг 7: окончание по кругу навсегда — ни конца, ни монет «после конца»
      if (outro_) { const SpType *T_ = &c->sp.type[c->sp.inst[G->inst].type]; float fps_ = T_->fps ? (float)T_->fps : 30.f, n_ = (float)(T_->frames & 0xFFFFu);
        if (n_ > G->outro && s->gagT[g] * fps_ > n_ - G->outro && !c->sp.run[G->inst].loopOn) spLoop(&c->sp, (unsigned)G->inst, n_ - G->outro, n_); }
      float cd = G->coinDelay < 0.f ? G->len : G->coinDelay;
      if (!s->gagCoin[g] && s->gagFound[g] && G->coins && s->gagT[g] >= cd && !(outro_ && G->coinDelay < 0.f)) { s->gagCoin[g] = 1; gagCoins(c, g, coins, world); }
      if (!outro_ && G->cycle != COLL_CYCLE && s->gagT[g] >= G->len && (GAG_SOUNDWAIT_OLD || s->gagHold != (int)g + 1)) {   // P16-14: пока идёт звук гэга — «играет»
        s->gagSt[g] = G->cycle == COLL_SINGLE ? 2 : 0;
        push(c, CE_GAG_END, (int)g, G->x, G->y, G->z);
      }
    }
    if (!gagHere(c, g, p)) { s->gagIn[g] = 0; continue; }
    int in = gagInside(G, p->x, p->y, p->z);
    if (!in && !p->onFoot && !GAG_CAR15_OLD && G->trig == COLL_TOUCH) { float e_ = G->tr + 1.5f, dx_ = p->x - G->tx, dy_ = p->y - G->ty, dz_ = p->z - G->tz; in = dx_ * dx_ + dy_ * dy_ + dz_ * dz_ <= e_ * e_; }   // круг 7: из машины сфера 1,5 м вокруг центра машины касается сферы триггера (как в оригинале: сфера 1,5 м); пешком — точка
    if (G->trig == COLL_TOUCH) {
      int fresh = in && !s->gagIn[g];
      if (in && s->gagIn[g] == 2 && hypotf(p->x - gagEntX, p->z - gagEntZ) > GAG_ENTRY_MOVE) fresh = 1;   // №873: вошёл в комнату уже в сфере — сработает с первого шага
      if (fresh && gagFree(c, g)) gagStart(c, g);
      s->gagIn[g] = (uint8_t)(in ? (s->gagIn[g] == 2 && !fresh ? 2 : 1) : 0);
    } else { if ((int)g == act) gagStart(c, g); s->gagIn[g] = (uint8_t)in; }
  }
}

// Attachment and visibility while HS_HURT are port rules. The sprite geometry,
// colours and kick's 14 frames / 30 fps are authored on the disc.
static void footFxPlace(Collect *c, int inst, const Homer *h) {
  if (inst < 0 || !h) return;
  SpRun *r = c->sp.run + inst; r->x = h->x; r->y = h->y; r->z = h->z; setYaw(r, h->h);
}
void collectFootKick(Collect *c, const Homer *h) {
#ifndef FOOT_FX_OLD
  if (c && c->haveSp && c->kickFx >= 0) { spEnter(&c->sp, c->kickFx, 1); c->sp.run[c->kickFx].alive = 1; footFxPlace(c, c->kickFx, h); }
#endif
}
static void footFxStep(Collect *c, const Homer *h) {
#ifndef FOOT_FX_OLD
  if (!h) return;
  footFxPlace(c, c->kickFx, h); footFxPlace(c, c->shockFx, h);
  if (c->st.shockOn && h->state != HS_HURT && (HOMER_STUN_OLD || h->stunT <= 0.f)) { c->st.shockOn = 0; hide(c, c->shockFx); }   // круг 2: при оглушении (homerStun) эффект держится, пока идёт stunT
#endif
}
void collectStep(Collect *c, const CollPlayer *p, Homer *homer, Car *car, const ColWorld *world, Coins *coins, float dt) {
  CollState *s = &c->st;
  if (dt <= 0.f) return;
  s->pJump = (homer && p->onFoot && !homer->grounded && p->y - homer->groundY > 0.f && p->y - homer->groundY < 6.f) ? p->y - homer->groundY : 0.f;   // высота прыжка над землёй (сверка g11b, W4/W5/8.5)
  cardsStep(c, p, car);
  boltsStep(c, p, homer, world, coins, dt);
  waspsStep(c, p, homer, car, world, dt);
  gagsStep(c, p, coins, world, dt);
  footFxStep(c, homer);
  if (c->haveSp) spUpdate(&c->sp, world, car, coins, dt);   // карточки крутятся, осы летают/умирают (удар машины — там же), сцены гэгов
  // частицы окон EFX копятся в c->sp (spFxPop): main.c забирает после кадра; без вызывающего очередь просто полна (SP_FXQ)
  s->px = p->x; s->py = p->y; s->pz = p->z; s->pOnFoot = p->onFoot; s->pRoom = p->room;
}
