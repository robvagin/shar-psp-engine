// Круг 6 (g11b-collect): тест монет и потери монет при взрыве. Каждая проверка падает на своём выключателе _OLD=1 и на «соседнем ложном» значении.
//   P03-02 COIN_AGE_OLD     возраст выпавшей монеты при рождении случайный 0..3,14 с (ложное: всегда 0)
//   P03-03 COIN_EXPIRE_OLD  без затухания размера; вспышка при исчезновении (ложное: плавная прозрачность 11..16 с, вспышки нет)
//   P03-04 COIN_LAUNCH_OLD  скорость вылета БЕЗ второй нормировки (14,1..28 м/с); за секунду остаётся 10 % горизонтальной скорости (ложное: 9,5 м/с и 82 %)
//   P03-29 COIN_BOUNCE_OLD  отскок без трения; покой при вертикальной скорости меньше 0,01 м/с (ложное: горизонталь x0,5, покой при 0,1)
//   P03-27 CAR_BOOM_COINS20_OLD  при гибели машины игрока теряется min(20, кошелёк) (ложное: 10)
//   P03-01 CAR_BOOM_ENEMY_COINS_OLD  игрок разбил трафик или ИИ миссии: 10 монет; чужой взрыв без игрока, полиция и стоящая машина: ничего
// Сборка и красные контроли — tests/collect/sverka_g11b.sh. Код выхода 0 = всё прошло.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "coins.h"
#include "carcoins.h"
static int fail;
#define CHECK(ok, ...) do { int o_ = (ok); printf("%s ", o_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fail += !o_; } while (0)
static void mk(Coins *cs) { memset(cs, 0, sizeof *cs); float p[3] = {1000.f, 0.f, 1000.f}; coinsInit(cs, p, 1); }   // одна монета мира далеко, пул 200
static int firstPool(const Coins *cs) { for (unsigned i = cs->nworld; i < cs->n; i++) if (cs->c[i].active) return (int)i; return -1; }

int main(void) {
  Coins cs;
  // ---- P03-02: возраст при рождении 0..π (случайный), не всегда 0
  mk(&cs); coinsDrop(&cs, 100, 0, 5, 0, 0);
  float amax = 0, asum = 0; unsigned na = 0;
  for (unsigned i = cs.nworld; i < cs.n; i++) if (cs.c[i].active) { na++; asum += cs.c[i].age; if (cs.c[i].age > amax) amax = cs.c[i].age; }
  CHECK(na == 100 && amax > 2.5f && amax <= 3.1416f && asum / na > 1.f && asum / na < 2.2f, "P03-02 возраст при рождении: макс %.2f с, среднее %.2f с (ждём до 3,14 и около 1,57)", amax, asum / na);
  coinsFree(&cs);

  // ---- P03-04: скорость вылета
  mk(&cs); coinsDrop(&cs, 150, 0, 5, 0, 0);
  float smin = 1e9f, smax = 0, vymin = 1e9f;
  for (unsigned i = cs.nworld; i < cs.n; i++) if (cs.c[i].active) {
    const Coin *c = &cs.c[i]; float s = sqrtf(c->vx * c->vx + c->vy * c->vy + c->vz * c->vz);
    if (s < smin) smin = s; if (s > smax) smax = s; if (c->vy < vymin) vymin = c->vy; }
  CHECK(smin > 13.5f && smax < 28.5f && vymin > 10.4f, "P03-04 скорость вылета %.1f..%.1f м/с, вертикаль не меньше %.1f (ждём 14,1..28 и 10,5)", smin, smax, vymin);
  coinsFree(&cs);
  // сопротивление: монета без земли под собой (пол ниже на 1 км), горизонтальная скорость через 1 с = 10 % от начальной
  mk(&cs); coinsDrop(&cs, 1, 0, 5, 0, -1000.f);
  int k = firstPool(&cs); float vx0 = cs.c[k].vx, vz0 = cs.c[k].vz, h0 = sqrtf(vx0 * vx0 + vz0 * vz0);
  for (int f = 0; f < 60; f++) coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f);
  float h1 = sqrtf(cs.c[k].vx * cs.c[k].vx + cs.c[k].vz * cs.c[k].vz);
  CHECK(h0 > 1.f && fabsf(h1 / h0 - 0.1f) < 0.01f, "P03-04 через 1 с остаётся %.3f горизонтальной скорости (ждём 0,100 = 0,1 в степени dt; прежнее 0,819)", h1 / h0);
  coinsFree(&cs);

  // ---- P03-29: отскок без трения и порог покоя 0,01
  mk(&cs); coinsDrop(&cs, 1, 0, 5, 0, 0); k = firstPool(&cs);
  { Coin *c = &cs.c[k]; c->x = c->z = 0; c->y = 0.001f; c->floor = 0; c->vx = 4.f; c->vz = 0; c->vy = -3.f; c->age = 5.f; c->airborne = 1; }
  coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f);
  CHECK(cs.c[k].vx > 3.5f && cs.c[k].vy > 1.7f, "P03-29 после отскока горизонталь %.2f м/с (ждём 3,9 без трения; прежнее 1,9), вертикаль %.2f", cs.c[k].vx, cs.c[k].vy);
  { Coin *c = &cs.c[k]; c->x = 0; c->y = -0.01f; c->floor = 0; c->vx = 4.f; c->vz = 0; c->vy = 0.25f; c->airborne = 1; }   // перед ударом о землю вертикаль после сопротивления и тяжести = -0,109, после отскока 0,066: между старым (0,1) и новым (0,01) порогом
  coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f);
  CHECK(cs.c[k].vx > 3.5f, "P03-29 вертикаль после отскока около 0,066 м/с выше порога 0,01: монета ещё катится (vx %.2f; прежнее: стоит)", cs.c[k].vx);
  { Coin *c = &cs.c[k]; c->x = 0; c->y = -0.01f; c->floor = 0; c->vx = 4.f; c->vz = 0; c->vy = 0.353f; c->airborne = 1; }   // после тяжести -0,01, после отскока 0,006 < 0,01: стоит
  coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f);
  CHECK(cs.c[k].vx == 0.f && cs.c[k].vy == 0.f, "P03-29 вертикаль после отскока 0,006 м/с ниже порога: монета встала (vx %.2f)", cs.c[k].vx);
  coinsFree(&cs);

  // ---- P03-03: без затухания размера (1,0 на возрасте 14 с), вспышка при исчезновении, раскрутка в последние 5 с
  mk(&cs); coinsDrop(&cs, 1, 0, 5, 0, 0); k = firstPool(&cs);
  cs.c[k].vx = cs.c[k].vy = cs.c[k].vz = 0; cs.c[k].airborne = 0; cs.c[k].age = 14.f;
  CHECK(coinAlpha(&cs.c[k]) == 1.f, "P03-03 размер/прозрачность на возрасте 14 с = %.2f (ждём 1,00; прежнее 0,40)", coinAlpha(&cs.c[k]));
  float spin0 = cs.c[k].spin; cs.c[k].age = 5.f; coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f); float d1 = cs.c[k].spin - spin0;   // обычное вращение, 1 подшаг
  spin0 = cs.c[k].spin; cs.c[k].age = 15.f; coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f); float d2 = cs.c[k].spin - spin0;       // возраст 15: добавка к вращению
  CHECK(d2 > d1 * 3.f, "P03-03 раскрутка: за шаг %.4f рад на возрасте 15 с против %.4f обычных (ждём заметно больше)", d2, d1);
  cs.c[k].age = 15.99f; unsigned fmax = 0;
  for (int f = 0; f < 120; f++) { coinsUpdate(&cs, 5000, 0, 5000, 0, 1.f / 60.f); if (cs.nflash > fmax) fmax = cs.nflash; }
  CHECK(!cs.c[k].active && fmax == 1, "P03-03 на 16 с монета исчезла со вспышкой (вспышек %u), через 0,5 с вспышка погасла (%u)", fmax, cs.nflash);
  coinsFree(&cs);

  // ---- P03-27: потеря монет при взрыве машины игрока
  { Car car; memset(&car, 0, sizeof car); car.x = 10; car.y = 1; car.z = 10; car.destroyed = 1; car.deadT = 99.f;
    mk(&cs); cs.collected = 35; int armed = 1;
    carBoomCoins(&car, &cs, NULL, &armed);
    CHECK(cs.collected == 15 && cs.poolActive == 20, "P03-27 кошелёк 35 при гибели: потеряно %d (ждём 20; прежнее 10), в воздухе %u", 35 - cs.collected, cs.poolActive);
    cs.collected = 7; armed = 1; unsigned before = cs.poolActive; carBoomCoins(&car, &cs, NULL, &armed);
    CHECK(cs.collected == 0 && cs.poolActive == before + 7, "P03-27 кошелёк 7: теряется всё, что есть (осталось %d, в воздух %u)", cs.collected, cs.poolActive - before);
    coinsFree(&cs);
    // ---- P03-01: монеты за чужой взрыв
    mk(&cs); Car live; memset(&live, 0, sizeof live); armed = 0;
    carBoomCoins(&live, &cs, NULL, &armed);                    // первый вызов запоминает счётчик
    svBoom.x = 3; svBoom.y = 1; svBoom.z = 3;
    svBoom.byPlayer = 1; svBoom.kind = 1; svBoom.seq++; carBoomCoins(&live, &cs, NULL, &armed);
    CHECK(cs.poolActive == 10, "P03-01 игрок разбил трафик: в воздух %u монет (ждём 10)", cs.poolActive);
    unsigned b0 = cs.poolActive; svBoom.byPlayer = 0; svBoom.kind = 1; svBoom.seq++; carBoomCoins(&live, &cs, NULL, &armed);
    CHECK(cs.poolActive == b0, "P03-01 трафик взорвался сам (без игрока): монет не добавилось");
    svBoom.byPlayer = 1; svBoom.kind = 3; svBoom.seq++; carBoomCoins(&live, &cs, NULL, &armed);
    CHECK(cs.poolActive == b0, "P03-01 игрок разбил полицию: монет нет");
    svBoom.byPlayer = 1; svBoom.kind = 2; svBoom.seq++; carBoomCoins(&live, &cs, NULL, &armed);
    CHECK(cs.poolActive == b0 + 10, "P03-01 игрок разбил ИИ миссии: ещё 10 монет (в воздухе %u)", cs.poolActive);
    coinsFree(&cs); }
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return !!fail;
}
