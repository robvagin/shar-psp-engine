// Круг 6 (g11b-collect): осы, продолжение sverka_g11b.c. Каждая проверка падает на своём выключателе _OLD=1.
//   W8/8.2/8.3 WASP_AGGR_OLD  перелёт в позицию атаки и отлёт на 5 м от игрока ближе 2 м     (ложное: оса стоит)
//   W9/8.4     WASP_EVADE_OLD уход при игроке ближе 4 м                                         (ложное: ухода нет)
//   W4         WASP_AIM_OLD   выстрел только в дуге ПОЛОВИНЫ атаки (5° → ±2,5°)                 (ложное: ±5°)
//   W5         WASP_BOLT_HIT_OLD прыгающего игрока снаряд пропускает                            (ложное: бьёт)
//   W11        WASP_BOB_OLD   качание осы ±0,5 м около 1 Гц                                     (ложное: висит)
//   W12/P15-07 WASP_NEAR_OLD  событие «оса ближе 5 м» при приближении, один раз на приближение  (ложное: при появлении)
//   Д12        WASP_CARD_AGGR_OLD подбор карточки переводит наблюдающих ос в агрессию           (ложное: нет)
//   W2         WASP_SPAWN_ORIG_OLD сфера появления = пятый аргумент (roam)
//   круг 7     WASP_GAGPAUSE_OLD  пока идёт гэг, оса в агрессии не стартует перелёт и не заряжается   (ложное: работает как обычно)
//   круг 7     WASP_RANGEFLY_OLD  заряженная оса, до игрока которой дальше дальности атаки, летит сразу, без интервала (ложное: ждёт интервал)
//   круг 7     подбор ключа (game.c) = collectWatchersAggro, как карточка
// Без мира и Гомера: collectStep(NULL, ...). Данные: build/collect-l1.shr (уровень 1). Не покрыто (нужна земля COL1/Гомер): Д8/8.7 минимум 2 м над землёй, W5 стена на 25 м, 8.1 «оса в кадре».
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "collect.h"
static Collect c; static unsigned char *blob; static int fail;
#define CHECK(ok, ...) do { int o_ = (ok); printf("%s ", o_ ? "PASS" : "FAIL"); printf(__VA_ARGS__); printf("\n"); fail += !o_; } while (0)
static const char *PATH;
static void fresh(void) {
  static int have; if (have) { collectFree(&c); free(blob); } have = 1;
  FILE *f = fopen(PATH, "rb"); fseek(f, 0, SEEK_END); unsigned n = ftell(f); rewind(f);
  blob = aligned_alloc(16, (n + 15u) & ~15u); if (fread(blob, 1, n, f) != n) exit(2); fclose(f);
  if (collectInit(&c, blob, n)) exit(2);
}
static int byName(const char *nm) { for (unsigned i = 0; i < c.nwasp; i++) if (!strcmp(c.wasp[i].name, nm)) return (int)i; exit(2); }
static CollWasp *mw(int i) { return (CollWasp *)&c.wasp[i]; }   // пакет живёт в нашей памяти: параметры осы для проверки можно менять
static Homer *HM;   // не NULL — Гомер для высоты прыжка (W5): p.y - groundY = высота над землёй
static void step(CollPlayer *p, int n) { for (int f = 0; f < n; f++) { collectStep(&c, p, HM, 0, 0, 0, 1.f / 60.f); } }
static int countEv(int type) { int n = 0; CollEvent e; while (collectPop(&c, &e)) n += e.type == type; return n; }
static SpRun *run(int i) { return &c.sp.run[c.wasp[i].inst]; }

int main(int argc, char **argv) {
  if (argc != 2) return 2; PATH = argv[1];

  // ---- W2: сфера появления — пятый аргумент (roam=50 у w_bonuscar): 45 м да, 55 м нет (общая проверка; на красном WASP_SPAWN_ORIG_OLD радиус другой)
  fresh(); int b = byName("w_bonuscar"); const CollWasp *w = &c.wasp[b];
  CollPlayer p = {w->x + 45.f, w->y - 1.2f, w->z, 1, -1, 0}; step(&p, 2);
  int in45 = c.st.waspSt[b] == 1;
  fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; p = (CollPlayer){w->x + 55.f, w->y - 1.2f, w->z, 1, -1, 0}; step(&p, 2);
  CHECK(in45 && c.st.waspSt[b] != 1, "W2 сфера 50 м: на 45 м оса появилась (%d), на 55 м нет (%d)", in45, c.st.waspSt[b] == 1);

  // ---- W11: качание. Стоящая оса w_simpsons (наблюдает, игрок в 9 м, сфера появления 10 м): высота за 3 с гуляет на ±0,5 м
  fresh(); int s = byName("w_simpsons"); w = &c.wasp[s]; p = (CollPlayer){w->x + 9.f, w->y - 1.2f, w->z, 1, -1, 0};
  step(&p, 2); float ymin = 1e9f, ymax = -1e9f;
  for (int f = 0; f < 180; f++) { step(&p, 1); float y = run(s)->y; if (y < ymin) ymin = y; if (y > ymax) ymax = y; }
  CHECK(ymax - ymin > 0.8f && ymax - ymin < 1.1f, "W11 качание: высота за 3 с гуляет на %.2f м (ждём около 1,0 = ±0,5; прежнее 0)", ymax - ymin);

  // ---- W12: событие «оса ближе 5 м» — не при появлении, а при приближении; один раз на приближение
  fresh(); s = byName("w_simpsons"); w = &c.wasp[s]; p = (CollPlayer){w->x + 9.f, w->y - 1.2f, w->z, 1, -1, 0};
  step(&p, 60); int n0 = countEv(CE_WASP_NEAR);
  p.x = w->x + 4.6f; step(&p, 60); int n1 = countEv(CE_WASP_NEAR);
  step(&p, 60); int n1b = countEv(CE_WASP_NEAR);
  p.x = w->x + 8.f; step(&p, 30); p.x = w->x + 4.6f; step(&p, 30); int n2 = countEv(CE_WASP_NEAR);
  CHECK(n0 == 0 && n1 == 1 && n1b == 0 && n2 == 1, "W12 «ближе 5 м»: на 9 м %d, подошёл %d, стоит рядом %d (ждём 0, 1, 0), отошёл и подошёл снова %d (ждём 1)", n0, n1, n1b, n2);

  // ---- W8/8.2/8.3 и W9/8.4: игрок держится в 3 м от осы (каждый кадр ставим его относительно осы): агрессия, уход, перелёт в позицию атаки
  fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; p = (CollPlayer){w->x + 3.f, w->y - 1.2f, w->z, 1, -1, 0};
  int seen[4] = {0, 0, 0, 0};
  for (int f = 0; f < 60 * 40; f++) {
    const SpRun *r = run(b); p.x = r->x + 3.f; p.z = r->z; p.y = r->y - 1.2f;   // y осы с качанием: держим 3 м по горизонтали
    step(&p, 1); if (c.st.waspFly[b] < 4) seen[c.st.waspFly[b]] = 1;
  }
  CHECK(seen[1], "W8/8.2 оса в агрессии сама перелетает в позицию атаки (перелёт 1 был: %d)", seen[1]);
  fresh(); c.level = 2; b = byName("w_bonuscar"); w = &c.wasp[b]; p = (CollPlayer){w->x + 3.f, w->y - 1.2f, w->z, 1, -1, 0};   // у уровня 1 в данных ухода нет (0/0/0); берём таблицу уровня 2: 2,5..8 м на 5 км/ч
  memset(seen, 0, sizeof seen);
  for (int f = 0; f < 60 * 40; f++) {
    const SpRun *r = run(b); p.x = r->x + 3.f; p.z = r->z; p.y = r->y - 1.2f;
    step(&p, 1); if (c.st.waspFly[b] < 4) seen[c.st.waspFly[b]] = 1;
  }
  CHECK(seen[2], "W9/8.4 игрок ближе 4 м: оса уходит от него (перелёт 2 был: %d)", seen[2]);
  fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; p = (CollPlayer){w->x + 1.5f, w->y - 1.2f, w->z, 1, -1, 0};
  memset(seen, 0, sizeof seen);
  for (int f = 0; f < 60 * 40; f++) {
    const SpRun *r = run(b); p.x = r->x + 1.5f; p.z = r->z; p.y = r->y - 1.2f;
    step(&p, 1); if (c.st.waspFly[b] < 4) seen[c.st.waspFly[b]] = 1;
  }
  CHECK(seen[3], "8.3 игрок ближе 2 м: оса отлетает на 5 м (перелёт 3 был: %d)", seen[3]);

  // ---- W4: дуга выстрела — половина атаки. Стоящая оса (флаг 1), не поворачивается (rot 0), игрок в 3 м под углом к её курсу: 1,5° — стреляет, 4° — нет (ждём ±2,5°)
  for (int k = 0; k < 2; k++) {
    float ang = k ? 4.f : 1.5f;
    fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; mw(b)->flags = 1; mw(b)->rot = 0.f;   // стоит и не поворачивается с самого начала
    p = (CollPlayer){w->x, w->y - 1.2f, w->z + 3.f, 1, -1, 0}; step(&p, 1);   // появилась, смотрит на игрока (курс 0)
    float a = ang * 0.0174533f; p.x = w->x + sinf(a) * 3.f; p.z = w->z + cosf(a) * 3.f;
    unsigned f0 = c.st.boltFired; step(&p, 60 * 30);
    if (k == 0) CHECK(c.st.boltFired > f0, "W4 угол 1,5° от курса (меньше половины дуги 2,5°): оса стреляет (выстрелов %u)", c.st.boltFired - f0);
    else CHECK(c.st.boltFired == f0, "W4 угол 4° от курса (больше половины дуги 2,5°, меньше полной 5°): оса не стреляет (выстрелов %u)", c.st.boltFired - f0);
  }

  // ---- W5: прыгающего игрока снаряд пропускает (pJump 1 м), стоящего на земле бьёт
  for (int k = 0; k < 2; k++) {
    fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; mw(b)->flags = 1; mw(b)->rot = 0.f;
    p = (CollPlayer){w->x, w->y - 1.2f, w->z + 3.f, 1, -1, 0}; step(&p, 1);
    static Homer hm; memset(&hm, 0, sizeof hm); HM = &hm; hm.grounded = k ? 0 : 1; hm.groundY = p.y; if (k) p.y += 1.f;   // прыжок: ступни на 1 м над землёй
    unsigned z0 = c.st.zaps; step(&p, 60 * 30);
    if (!k) CHECK(c.st.boltFired > 0 && c.st.zaps > z0, "W5 игрок на земле: снаряд попадает (выстрелов %u, ударов током %u)", c.st.boltFired, c.st.zaps - z0);
    else CHECK(c.st.boltFired > 0 && c.st.zaps == z0, "W5 игрок в прыжке 1 м: оса стреляет (%u), снаряд пропускает (ударов %u)", c.st.boltFired, c.st.zaps - z0);
  }

  HM = 0;
  // ---- Д12: подбор карточки переводит наблюдающих ос в агрессию (карточка 6 у трейлера w_trailor1, 5 м от неё, дальше минимума 4)
  fresh(); b = byName("w_trailor1"); const CollCard *cd = &c.card[6]; w = &c.wasp[b];
  p = (CollPlayer){cd->x - 8.f, cd->y, cd->z, 1, -1, 0}; step(&p, 60);
  int before = c.st.waspSt[b] == 1 && !c.st.waspAggr[b];
  p = (CollPlayer){cd->x, cd->y, cd->z, 1, -1, 0}; step(&p, 2);
  int got = c.st.cardGot[6];
  CHECK(before && got && c.st.waspAggr[b], "Д12 оса наблюдала (%d), игрок взял карточку (%d): оса агрессивна (%d)", before, got, c.st.waspAggr[b]);
  // ---- круг 7: гэг идёт — агрессивная оса ждёт (оригинал: ход агрессивной осы прерывается, пока счётчик гэгов больше 0). Игрок в 3 м, гэг 0 играет: за 6 с ни заряда, ни перелёта
  for (int k = 0; k < 2; k++) {
    fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; p = (CollPlayer){w->x + 3.f, w->y - 1.2f, w->z, 1, -1, 0};
    step(&p, 2); if (k == 0) c.st.gagSt[0] = 1;   // играет гэг
    int ch0 = 0, fl = 0;
    for (int f = 0; f < 60 * 6; f++) { const SpRun *r = run(b); p.x = r->x + 3.f; p.z = r->z; p.y = r->y - 1.2f; step(&p, 1); if (c.st.waspFly[b]) fl = 1; }
    CollEvent e; while (collectPop(&c, &e)) ch0 += e.type == CE_WASP_CHARGE;
    if (k == 0) CHECK(!fl && ch0 == 0, "круг 7 гэг идёт: за 6 с оса не летит (%d), не заряжается (событий %d)", fl, ch0);
    else CHECK(fl || ch0 > 0, "круг 7 гэга нет: оса летит (%d) и заряжается (событий %d)", fl, ch0);
  }
  // ---- круг 7: заряженная оса, игрок отошёл дальше дальности атаки: летит в позицию атаки, не дожидаясь интервала (10 с у w_bonuscar): за 3 с вылет есть (заряд после перелёта идёт около 2 с)
  fresh(); b = byName("w_bonuscar"); w = &c.wasp[b]; p = (CollPlayer){w->x + 3.f, w->y - 1.2f, w->z, 1, -1, 0};
  int done1 = 0, f1 = -1;
  for (int f = 0; f < 60 * 20 && !done1; f++) { const SpRun *r = run(b); p.x = r->x + 3.f; p.z = r->z; p.y = r->y - 1.2f; step(&p, 1); if (c.st.waspFly[b] == 1) f1 = f; if (f1 >= 0 && !c.st.waspFly[b]) done1 = 1; }
  float rng = c.wasp[b].atkRange; int hs = 0, fr = -1;
  if (done1) {
    for (int f = 0; f < 60 * 3 && !hs; f++) { const SpRun *r = run(b); p.x = r->x + rng + 6.f; p.z = r->z; p.y = r->y - 1.2f; step(&p, 1); if (c.st.waspFly[b] == 1) { hs = 1; fr = f; } }
  }
  CHECK(done1 && hs && fr < 170, "круг 7 игрок дальше дальности атаки %.0f м: заряженная оса летит в позицию атаки на кадре %d (ждём до 170, без интервала; интервал данных %.1f с)", rng, fr, c.wasp[b].atkEvade);
  // ---- круг 7: подбор ключа / карточки — осы в наблюдении агрессивны (collectWatchersAggro; game.c зовёт её при подборе ключа)
  fresh(); b = byName("w_trailor1"); w = &c.wasp[b]; p = (CollPlayer){w->x + 8.f, w->y - 1.2f, w->z, 1, -1, 0}; step(&p, 60);
  int w0 = c.st.waspSt[b] == 1 && !c.st.waspAggr[b]; collectWatchersAggro(&c);
  CHECK(w0 && c.st.waspAggr[b], "круг 7 подбор вещи (collectWatchersAggro): оса наблюдала (%d), стала агрессивной (%d)", w0, c.st.waspAggr[b]);
  { FILE *gf = fopen("viewer/game.c", "r"); char *gb = malloc(1 << 21); size_t gn = gf ? fread(gb, 1, (1 << 21) - 1, gf) : 0; if (gf) fclose(gf); gb[gn] = 0;
    CHECK(gn && strstr(gb, "WASP_WRENCH_AGGR_OLD && g->pColl) collectWatchersAggro(g->pColl)"), "круг 7 game.c: при подборе ключа звонит collectWatchersAggro под WASP_WRENCH_AGGR_OLD"); free(gb); }
  HM = 0; printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return !!fail;
}
