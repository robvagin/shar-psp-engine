// Сторож игры (канон Роба 06.11, CLAUDE.md п.12; ядро — watchdog.c). Виды: nofloor, belowworld, stuck (герой и машина); spawn пишет homer.c (постановка без опоры).
// Чинит всегда через проверку опоры (п.11): надёжное место (последняя точка на опоре), иначе машина, иначе старт текущей миссии/комнаты (крючок gameWdStartPoint (1 — дал точку)).
// Пороги — правило порта (сверка с оригиналом: docs/handoff/СТОРОЖ_ПОРОГИ.md). WDG_*_OLD / wdEnable(0) — выключатель красного контроля.
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "watchdog.h"
#ifndef WDG_NOFLOOR_T
#define WDG_NOFLOOR_T 1.5f     // с: колонка без опоры мира под телом (не зона DEATH) дольше этого — возврат
#endif
#ifndef WDG_CAR_NOFLOOR_T
#define WDG_CAR_NOFLOOR_T 2.5f // с: то же для машины (внутренний возврат car.c срабатывает на 1,5 с; сторож — вторая линия)
#endif
#ifndef WDG_BELOW_M
#define WDG_BELOW_M 30.f       // м ниже самой низкой точки коллизии мира — ниже дна
#endif
#ifndef WDG_STUCK_T
#define WDG_STUCK_T 1.5f       // с: ступни глубже поверхности под центром тела на WDG_SUNK_M
#endif
#ifndef WDG_SUNK_M
#define WDG_SUNK_M 0.6f
#endif
#ifndef WDG_CAR_STUCK_T
#define WDG_CAR_STUCK_T 4.0f   // с: машина глубоко в геометрии (внутренний возврат — 0,5 с)
#endif
int (*gameWdStartPoint)(int hero, float out[4]);   // крючок миссий: старт текущей фазы/комнаты (x, y, z, курс); 0 — не задан. hero=1 герой, 0 машина
// Служебное «сломать вид» (приёмка сторожа: каждый вид искусственно ломается, сторож обязан починить и написать строку): wdBreakMask — биты WDB_*;
// поломка ставится перед самой проверкой каждый кадр, дальше обычное обнаружение и починка. Игра: сценарий смоука «flag wdbreak N»
int wdBreakMask;
static float worldFloor(const ColWorld *w);
static void breakHero(Game *g, const ColWorld *w, int bit) {
  Homer *H = g->pHomer; if (!(wdBreakMask & bit) || !*g->pHaveHomer || !homerOnFoot(H) || homerSpawnPending(H)) return;
  if (bit == WDB_HERO_NOFLOOR) { H->x += 5000.f; H->z += 5000.f; H->grounded = 0; H->vy = 0.f; H->airFrom = H->y; }   // в воздух над точкой, где в мире ничего нет
  else if (bit == WDB_HERO_BELOW) { H->y = worldFloor(w) - 100.f; H->grounded = 0; H->vy = 0.f; H->airFrom = H->y; }
  else if (bit == WDB_HERO_STUCK) { float gg; if (colGround(w, H->x, H->z, H->y + 2.f, H->y - 2.f, &gg, 0)) { H->y = gg - .9f; H->grounded = 1; } }
}
#ifndef WDG_FLOORCACHE_OLD
#define WDG_FLOORCACHE_OLD 0   // 1 — прежний кеш на один мир: герой в комнате и машина в городе сбрасывают его каждый кадр (полный проход по треугольникам дважды за кадр)
#endif
typedef struct { const ColWorld *w; const ColTri *tri; const ColBox *box; uint32_t ntri, nbox; float y; } WdgFloor;
typedef struct { Game *g; float hNo, hBelow, hStuck, cNo, cBelow; const ColWorld *fw; float floorY; WdgFloor fc[2]; int fcNext; } WdG;
static WdG W;
unsigned wdgFloorPasses;   // полных проходов по миру за процесс (гейт tests/spawnground/cost_host.c: герой в комнате, машина в городе)
// Дно мира: самая низкая точка треугольников и ящиков. Считается один раз на мир (ключ: адрес мира и его содержимое), кеш на два мира — город и комната; в кадре только чтение
static float worldFloor(const ColWorld *w) {
#if !WDG_FLOORCACHE_OLD
  for (int i = 0; i < 2; i++) { const WdgFloor *e = &W.fc[i]; if (w && e->w == w && e->tri == w->tri && e->box == w->box && e->ntri == w->ntri && e->nbox == w->nbox) return e->y; }
#else
  if (W.fw == w && w) return W.floorY;
#endif
  wdgFloorPasses++;
  float m = 1e9f;
  { float ty = colMinTriY(w); if (ty < m) m = ty; }   // Коллизия-96: в кольце — из заголовка файла, без полного прохода
  for (uint32_t i = 0; i < w->nbox; i++) { float y = w->box[i].c[1] - w->box[i].h[1]; if (y < m) m = y; }
  if (m > 1e8f) m = -1e4f;
  W.fw = w; W.floorY = m;
  { WdgFloor *e = &W.fc[W.fcNext]; W.fcNext ^= 1; e->w = w; e->tri = w->tri; e->box = w->box; e->ntri = w->ntri; e->nbox = w->nbox; e->y = m; }
  return m;
}
static int deathHere(const ColWorld *w, float x, float y, float z) { return colDeathAt(w, x, y, z, 1.f) >= 0 || colDeathBelow(w, x, y, z, 60.f) >= 0; }
static int heroRescue(Game *g, const ColWorld *w, char *info, int cap) {
  Homer *H = g->pHomer; float g_, p[4];
  const char *how = "safe";
  if (H->safeOk && colGround(w, H->safe[0], H->safe[2], H->safe[1] + 2.f, H->safe[1] - 3.f, &g_, 0)) homerPlace(H, w, H->safe[0], H->safe[1], H->safe[2], H->h);
  else if (!g->pHomerWorld && g->pCar && colGround(w, g->pCar->x, g->pCar->z, g->pCar->y + 2.f, g->pCar->y - 3.f, &g_, 0)) { how = "car"; homerPlace(H, w, g->pCar->x, g->pCar->y, g->pCar->z, H->h); }
  else if (gameWdStartPoint && gameWdStartPoint(1, p)) { how = "start"; homerPlace(H, w, p[0], p[1], p[2], p[3]); }
  else how = "none";
  snprintf(info, cap, "fix=%s ok=%d to=%.2f,%.2f,%.2f", how, !homerSpawnPending(H), H->x, H->y, H->z);
  return 1;
}
static int carRescue(Game *g, const ColWorld *w, char *info, int cap) {
  Car *c = g->pCar; float g_;
  carResetNearby(c, w);
  const char *how = "safe";
  if (!colGround(w, c->x, c->z, c->y + 2.f, c->y - 5.f, &g_, 0)) { how = "home"; carPlace(c, w, c->homeX, c->homeY, c->homeZ, c->homeH); }
  snprintf(info, cap, "fix=%s ok=%d to=%.2f,%.2f,%.2f", how, colGround(w, c->x, c->z, c->y + 2.f, c->y - 5.f, &g_, 0) ? 1 : 0, c->x, c->y, c->z);
  return 1;
}
static int chkNoFloor(void *u, float dt, char *info, int cap) {
  Game *g = W.g; (void)u; char t[200]; float g_;
  if (!g || !*g->pHaveCol) return 0;
  const ColWorld *wc = g->pWorld;
  if (*g->pHaveHomer && g->pHomer->spawnLost) {   // постановка без опоры: ни рядом, ни надёжного места (homer.c spawnResolve) — от машины / старта фазы, через проверку опоры
    Homer *H = g->pHomer; H->spawnLost = 0; heroRescue(g, g->pHomerWorld ? g->pHomerWorld : wc, t, sizeof t); wdFire("spawn", "hero", t);
  }
  breakHero(g, g->pHomerWorld ? g->pHomerWorld : wc, WDB_HERO_NOFLOOR);
  if (wdBreakMask & WDB_CAR_NOFLOOR) { g->pCar->x += 5000.f; g->pCar->z += 5000.f; g->pCar->wheels = 0; }   // A3: перенесённая машина колёсами ни на что не опирается
  if (*g->pHaveHomer && homerOnFoot(g->pHomer) && !homerSpawnPending(g->pHomer)) {
    const ColWorld *w = g->pHomerWorld ? g->pHomerWorld : wc; Homer *H = g->pHomer;
    // стоящий над пустотой на «полу над пустотой» (homer.c voidFloor, правило порта до 6 м от опоры) не падает — не нарушение; считается только воздух
    if (!H->grounded && !colGround(w, H->x, H->z, H->y + 1.f, -1e6f, &g_, 0) && !deathHere(w, H->x, H->y, H->z)) W.hNo += dt; else W.hNo = 0.f;
    if (W.hNo > WDG_NOFLOOR_T) { char p[64]; snprintf(p, sizeof p, "pos=%.2f,%.2f,%.2f noground_s=%.2f ", H->x, H->y, H->z, W.hNo); W.hNo = 0.f; heroRescue(g, w, t, sizeof t); snprintf(info, cap, "%s%s", p, t); wdFire("nofloor", "hero", info); }
  } else W.hNo = 0.f;
  if (g->pCar) {
    Car *c = g->pCar;
    // A3 Опора-93: машина с колёсами на опоре (c->wheels > 0, как «any» у car.c noGroundT) не висит над пустотой, даже если под центром кузова нет проходимой опоры:
    // L3 sr1 (-239.4, -51.3, -260.7) — полоса шириной 1 м из крутых треугольников (n.y 0,49 < COL_WALK_NY) между двумя уровнями дороги (-50,6 / -52,6), колёса по обе стороны
#ifndef WDG_CARWHEELS_OLD
#define WDG_CARWHEELS_OLD 0
#endif
    if (!colGround(wc, c->x, c->z, c->y + 1.f, -1e6f, &g_, 0) && !deathHere(wc, c->x, c->y, c->z) && (WDG_CARWHEELS_OLD || c->wheels <= 0)) W.cNo += dt; else W.cNo = 0.f;
    if (W.cNo > WDG_CAR_NOFLOOR_T) { char p[64]; snprintf(p, sizeof p, "pos=%.2f,%.2f,%.2f noground_s=%.2f ", c->x, c->y, c->z, W.cNo); W.cNo = 0.f; carRescue(g, wc, t, sizeof t); snprintf(info, cap, "%s%s", p, t); wdFire("nofloor", "car", info); }
  }
  return 0;
}
static int chkBelow(void *u, float dt, char *info, int cap) {
  Game *g = W.g; (void)u; (void)dt; char t[200];
  if (!g || !*g->pHaveCol) return 0;
  const ColWorld *wc = g->pWorld;
  breakHero(g, g->pHomerWorld ? g->pHomerWorld : wc, WDB_HERO_BELOW);
  if (wdBreakMask & WDB_CAR_BELOW) g->pCar->y = worldFloor(wc) - 100.f;
  if (*g->pHaveHomer && homerOnFoot(g->pHomer) && !homerSpawnPending(g->pHomer)) {
    const ColWorld *w = g->pHomerWorld ? g->pHomerWorld : wc; Homer *H = g->pHomer; float fl = worldFloor(w);
    if (H->y < fl - WDG_BELOW_M) { char p[64]; snprintf(p, sizeof p, "pos=%.2f,%.2f,%.2f floor=%.1f ", H->x, H->y, H->z, fl); heroRescue(g, w, t, sizeof t); snprintf(info, cap, "%s%s", p, t); wdFire("belowworld", "hero", info); }
  }
  if (g->pCar && g->pCar->y < worldFloor(wc) - WDG_BELOW_M) { Car *c = g->pCar; char p[64]; snprintf(p, sizeof p, "pos=%.2f,%.2f,%.2f floor=%.1f ", c->x, c->y, c->z, worldFloor(wc)); carRescue(g, wc, t, sizeof t); snprintf(info, cap, "%s%s", p, t); wdFire("belowworld", "car", info); }
  return 0;
}
static int chkStuck(void *u, float dt, char *info, int cap) {
  Game *g = W.g; (void)u; char t[200]; float g_;
  if (!g || !*g->pHaveCol) return 0;
  breakHero(g, g->pHomerWorld ? g->pHomerWorld : g->pWorld, WDB_HERO_STUCK);
  if (wdBreakMask & WDB_CAR_STUCK) g->pCar->embedT = WDG_CAR_STUCK_T + 1.f;
  if (*g->pHaveHomer && homerOnFoot(g->pHomer) && !homerSpawnPending(g->pHomer)) {
    const ColWorld *w = g->pHomerWorld ? g->pHomerWorld : g->pWorld; Homer *H = g->pHomer;
    // ступни глубже верхней поверхности под центром тела (поверхность в пределах 1 м над ступнями): тело внутри предмета
    if (H->grounded && colGround(w, H->x, H->z, H->y + 1.f, H->y + WDG_SUNK_M, &g_, 0)) W.hStuck += dt; else W.hStuck = 0.f;
    if (W.hStuck > WDG_STUCK_T) {
      char p[64]; snprintf(p, sizeof p, "pos=%.2f,%.2f,%.2f sunk=%.2f ", H->x, H->y, H->z, g_ - H->y); W.hStuck = 0.f;
      homerPlace(H, w, H->x, g_, H->z, H->h); snprintf(t, sizeof t, "fix=lift ok=%d", !homerSpawnPending(H)); snprintf(info, cap, "%s%s", p, t); wdFire("stuck", "hero", info);
    }
  } else W.hStuck = 0.f;
  if (g->pCar && g->pCar->embedT > WDG_CAR_STUCK_T) { Car *c = g->pCar; char p[64]; snprintf(p, sizeof p, "pos=%.2f,%.2f,%.2f embed_s=%.2f ", c->x, c->y, c->z, c->embedT); carRescue(g, g->pWorld, t, sizeof t); snprintf(info, cap, "%s%s", p, t); wdFire("stuck", "car", info); }
  return 0;
}
void gameWatchdogStep(Game *g, float dt) {
  if (W.g != g) { memset(&W, 0, sizeof W); W.g = g; }
  if (wdRegister("nofloor", "game", chkNoFloor, 0) || wdRegister("belowworld", "game", chkBelow, 0) || wdRegister("stuck", "game", chkStuck, 0)) return;
  wdStep(dt);
}
