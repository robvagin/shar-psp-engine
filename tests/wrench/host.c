// Гаечные ключи (пункт 4 ведущего 29.09) на Маке: игра без экрана (tools/bots/world.c), тот же viewer/game.c, что EBOOT.
// Всё из данных уровня: ключи — JMP1 COL1 (ActionEvent Wrench сцен уровня 1, tools/jumps_export.py), модель — тип wrench SPR1.
//   tests/wrench/host.sh [--gate]    (--gate: как есть PASS; уровень без ключей в данных — FAIL)
// Проверки:
//   data     — ключей 14, у каждого есть экземпляр модели в той же точке (SPR1)
//   reset    — RESET CAR остов не чинит (сообщение библии: «FIND A WRENCH, OR GO TO A PHONEBOOTH»)
//   foot     — машина взорвана, Гомер пешком у ключа: ключ исчез (модель спрятана), машина целая (урон 0, не остов), можно сесть
//   car      — битая машина проезжает через ключ: урон 0
//   respawn  — взятый ключ возвращается, когда игрок далеко и 5 с не видит его; рядом — не возвращается
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"

static int fails;
static void check(const char *name, int ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void check(const char *name, int ok, const char *fmt, ...) {
  char d[300]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
  printf("CHECK %-10s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++;
}
static BotWorld W;
static void steps(int n) { for (int i = 0; i < n; i++) bwStep(&W, 0, 0, 0); }

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "host LEVEL.shr HOMER.shr level01.msn\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  bwNew(&W); steps(30);
  const ColWorld *C = bwCol(); Game *G = &W.G;
  int mapped = 0; gameWrenchStep(G);
  for (unsigned k = 0; k < C->nwrench && k < 32; k++) mapped += G->wrenchInst[k] >= 0;
  check("data", C->nwrench == 14 && mapped == (int)C->nwrench, "ключей в данных %u (уровень 1: 14), с моделью в той же точке %d", C->nwrench, mapped);
  if (!C->nwrench) { printf("RESULT FAIL %d\n", fails); return 1; }
  // ключ 0 и точка рядом с ним на земле
  const ColWrench *K = &C->wrench[0]; float gy = K->p[1];
  { float g; if (colGround(C, K->p[0] + 6.f, K->p[2], K->p[1] + 3.f, K->p[1] - 10.f, &g, 0)) gy = g; }
  // ---- reset: остов RESET CAR не чинит
  bwPlace(&W, K->p[0] + 6.f, gy, K->p[2], 0.f); steps(20);
  W.car.damage = 1.f; W.car.destroyed = 1; steps(90);   // взрыв: Гомер выброшен (homerEject)
  bwStep(&W, GP_START, 0, 0); steps(5);
  check("reset", W.car.destroyed == 1, "после RESET CAR машина остов: %d (урон %.2f)", W.car.destroyed, W.car.damage);
  // ---- пешком к ключу
  for (int i = 0; i < 300 && !gameOnFoot(G); i++) steps(1);
  unsigned took0 = G->wrenches;
  homerPlace(&W.homer, C, K->p[0] + 4.f, gy + 1.f, K->p[2], 0.f); steps(5);
  int before = (G->wrenchTaken & 1u) == 0 && W.car.destroyed;
  homerPlace(&W.homer, C, K->p[0] + .5f, gy + 1.f, K->p[2], 0.f); steps(3);
  int hid = G->wrenchInst[0] >= 0 && !W.sp.run[G->wrenchInst[0]].alive;
  HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &W.car, W.lift);
  check("foot", before && (G->wrenchTaken & 1u) && G->wrenches == took0 + 1 && !W.car.destroyed && W.car.damage == 0.f && hid && !hc.dead,
        "в 4 м ключ лежит %d; у ключа взят %d, машина: остов %d урон %.2f, модель спрятана %d, сесть можно %d", before, (int)(G->wrenchTaken & 1u),
        W.car.destroyed, W.car.damage, hid, !hc.dead);
  // ---- машиной через другой ключ
  bwNew(&W); steps(20); G = &W.G; C = bwCol();
  const ColWrench *K2 = &C->wrench[1]; float g2 = K2->p[1]; { float g; if (colGround(C, K2->p[0], K2->p[2], K2->p[1] + 3.f, K2->p[1] - 10.f, &g, 0)) g2 = g; }
  W.car.damage = .6f; bwPlace(&W, K2->p[0], g2, K2->p[2], 0.f); W.car.damage = .6f; steps(3);
  check("car", (G->wrenchTaken >> 1 & 1u) && W.car.damage == 0.f, "битая машина (0.6) у ключа 1: взят %d, урон %.2f", (int)(G->wrenchTaken >> 1 & 1u), W.car.damage);
  // ---- возврат (сверка g10-flow 02.11, P07 №7): ровно через 30 с после подбора по таймеру, независимо от расстояния и вида: 8 с рядом, потом игрок далеко —
  // до 30 с ключ лежит взятым (старое правило вернуло бы его через 5 с вне вида), после 30 с вернулся
  steps(60 * 8); int stay = (G->wrenchTaken >> 1 & 1u) != 0;
  const ColWrench *Kf = &C->wrench[0]; float best = 0; for (unsigned k = 0; k < C->nwrench; k++) { float d = hypotf(C->wrench[k].p[0] - K2->p[0], C->wrench[k].p[2] - K2->p[2]); if (d > best) { best = d; Kf = &C->wrench[k]; } }
  { float g; float fy = Kf->p[1]; if (colGround(C, Kf->p[0] + 8.f, Kf->p[2], Kf->p[1] + 3.f, Kf->p[1] - 10.f, &g, 0)) fy = g; bwPlace(&W, Kf->p[0] + 8.f, fy, Kf->p[2], 0.f); }
  steps(60 * 15); int early = (G->wrenchTaken >> 1 & 1u) != 0;   // 23 с после подбора
  steps(60 * 10); int back = (G->wrenchTaken >> 1 & 1u) == 0 && (G->wrenchInst[1] < 0 || W.sp.run[G->wrenchInst[1]].alive);   // 33 с
  check("respawn", stay && early && back, "8 с рядом — лежит взятым %d; игрок за %.0f м, 23 с — лежит %d; 33 с — вернулся %d", stay, best, early, back);
  printf("RESULT %s %d\n", fails ? "FAIL" : "OK", fails);
  return fails ? 1 : 0;
}
