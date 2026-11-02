#ifndef SHAR_CARCOINS_H
#define SHAR_CARCOINS_H
#include "car.h"
#include "coins.h"
#include "sverka_shared.h"
#ifndef CAR_BOOM_COINS_OLD
#define CAR_BOOM_COINS_OLD 0
#endif
// Сверка g11b (02.11.2026), P03-27: потеря монет при гибели машины игрока — min(20, кошелёк), монеты разлетаются без автосбора (было: до 10). CAR_BOOM_COINS20_OLD=1 — потолок 10, как в TEST81.
// Момент потери (гибель или взрыв через 3 с) отчёт сверять велит по записи PS2; оставлен момент взрыва.
#ifndef CAR_BOOM_COINS20_OLD
#define CAR_BOOM_COINS20_OLD 0
#endif
// Сверка g11b, P03-01 (особое в наряде): машину трафика или ИИ миссии (не полиция) разбил игрок — 10 монет из центра машины обычным выбросом, пол — земля под ней; игрок в машине:
// монеты после первого касания земли сами летят к нему (coins->dropAuto, как у любых выбитых). Событие — svBoom из sverka_shared.h (его заполняет car.c окна g01-cars и увеличивает seq).
// CAR_BOOM_ENEMY_COINS_OLD=1 — монет за взрыв чужой машины нет, как в TEST81.
#ifndef CAR_BOOM_ENEMY_COINS_OLD
#define CAR_BOOM_ENEMY_COINS_OLD 0
#endif
#define CAR_BOOM_ENEMY_COINS 10
// PS2 a355_boom: wallet 4 -> 0 at boom, loose coins can be collected again.
// Cap 10 is Rob's explicit fallback rule; direction/speed/lifetime use CoinManager.
// Arm only before the boom, so loading an existing husk is not another penalty.
static inline void carBoomCoins(const Car *car, Coins *coins, const ColWorld *world, int *armed) {
  // монеты за чужой взрыв (P03-01): реагируем на рост счётчика svBoom.seq; первый вызов только запоминает значение (взрыв до загрузки не считается)
  static unsigned boomSeen; static int boomInit;
  if (!boomInit) { boomInit = 1; boomSeen = svBoom.seq; }
  if (svBoom.seq != boomSeen) {
    boomSeen = svBoom.seq;
    if (!CAR_BOOM_ENEMY_COINS_OLD && coins && svBoom.byPlayer && (svBoom.kind == 1 || svBoom.kind == 2)) {
      float fl = svBoom.y;
      if (world) colGround(world, svBoom.x, svBoom.z, svBoom.y + 1.f, svBoom.y - 30.f, &fl, 0);
      coinsDrop(coins, CAR_BOOM_ENEMY_COINS, svBoom.x, svBoom.y + 1.f, svBoom.z, fl);
    }
  }
  if (!carBoomed(car)) { *armed = 1; return; }
  if (!*armed) return;
  *armed = 0;
  if (CAR_BOOM_COINS_OLD || !coins || coins->collected <= 0) return;
  int cap = CAR_BOOM_COINS20_OLD ? 10 : 20;
  int n = coins->collected < cap ? coins->collected : cap;
  float floor = car->ry;
  if (world) colGround(world, car->x, car->z, car->y + 1.f, car->y - 30.f, &floor, 0);
  coins->collected -= n;
  int automatic = coins->dropAuto;
  coins->dropAuto = 0; // explosion coins scatter for manual pickup, even with no-eject
  coinsDrop(coins, n, car->x, car->y + 1.f, car->z, floor);
  coins->dropAuto = automatic;
}
#endif
