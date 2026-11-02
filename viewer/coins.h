// Монеты (этап 3б): лежат в мире (локаторы kind 14), крутятся, подбираются машиной, вылетают с отскоком.
// Правила — как в оригинале (PAL CoinManager, сверено в hit-and-run-web/src/coin-simulation.ts):
// пул выпавших 200, g 21, отскок 0.6, скорость вылета 9.5, прозрачнеют с 11 с, исчезают на 16 с,
// подбор ромбом |dx|,|dz| < 2.25, dx+dz < 3 (в машине ×2.25), |dy| < 2.25, притяжение 0.4 с. Чистый C99.
#ifndef COINS_H
#define COINS_H
#ifdef __cplusplus
extern "C" {
#endif

#define COIN_POOL 200
#define COIN_FLASHES 8   // одновременно живых вспышек исчезновения

typedef struct {
  float x, y, z, ox, oy, oz, vx, vy, vz, floor, age, magnet, spin;
  unsigned char active, collecting, airborne, loose, autoCollect;
} Coin;

typedef struct {
  Coin *c; unsigned n, nworld;   // первые nworld — монеты мира, дальше пул выпавших
  int collected, total;          // собрано (кошелёк) / всего монет мира
  int dropAuto;                  // 1 — игрок в машине: выбитые монеты после первого касания земли сами летят к нему (как в оригинале)
  float acc;
  unsigned rng;                  // разброс выпавших монет (coinsInit: 12345)
  unsigned owned[8];             // купленное/полученное за монеты (viewer/shop.h): уровень 1..7 -> бит = номер BindReward в пакете
                                 // миссий уровня; живёт вместе с кошельком (NEW GAME обнуляет), пишется в сохранение (savegame v2)
  // правка Роба 81: пол под летящей монетой — по её месту (холмы, ямки), не высота точки высыпания; NULL — как раньше
  int (*ground)(void *u, float x, float z, float yTop, float *gy); void *groundU;
  // разбор L3/L5 (TEST55): монеты мира по сетке клеток COIN_GRID_CELL м и счётчики пула / притягиваемых — шаг и рисунок обходят только близкие (coins.c, COINGRID_OLD)
  unsigned poolActive, nCollect, recount; int gw, gh; float gx0, gz0; unsigned short *gstart, *gidx;
  // сверка g11b, P03-03: вспышки исчезновения монет (точка и возраст, с; живёт COIN_FLASH_T = 0,5 с). Рисунок — main_loop.h (размер 0,25), здесь данные
  unsigned nflash; struct { float x, y, z, t; } flash[COIN_FLASHES];
} Coins;

// pos: nworld троек x,y,z. 0 = ок
int coinsInit(Coins *cs, const float *pos, unsigned nworld);
void coinsFree(Coins *cs);
// шаг: игрок (машина) в x,y,z; inCar — множитель досягаемости. Возвращает, сколько монет собрано за шаг
int coinsUpdate(Coins *cs, float px, float py, float pz, int inCar, float dt);
// выбросить n монет из точки (пол floor). Не влезло в пул — сразу в кошелёк (как в оригинале)
void coinsDrop(Coins *cs, int n, float x, float y, float z, float floor);
// то же, но горизонталь вылета — в конусе ±acos(cone) вокруг направления (dx, dz) (монеты «_Z» торгового автомата)
void coinsDropDir(Coins *cs, int n, float x, float y, float z, float floor, float dx, float dz, float cone);
// индексы активных монет в пределах r м от (x, z) по горизонтали (клетки сетки целиком, лишние отсеивает вызывающий) + все активные из пула выпавших; возвращает число (до cap) или 0xFFFFFFFF, если сетки нет (COINGRID_OLD, пустой мир)
unsigned coinsNear(const Coins *cs, float x, float z, float r, unsigned *out, unsigned cap);
// размер/прозрачность монеты 0..1 (до сверки g11b выпавшие гасли с 11 до 16 с; теперь 1 — COIN_EXPIRE_OLD=1 возвращает затухание)
float coinAlpha(const Coin *c);

#ifdef __cplusplus
}
#endif
#endif
