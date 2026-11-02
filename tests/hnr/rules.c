// Гейт правки 121 (Hit & Run): правила шкалы viewer/game.c против правил исполняемого файла PAL SLES_518.97
// (разбор hit-and-run-web/src/hit-and-run.ts, MIT). Каждый случай — ожидание из этих правил, без знания нашей реализации.
// Выход: строки CHECK <имя> OK|FAIL и RESULT <ок>/<всего>; код 1 — есть FAIL. Сборка и отрицательные контроли — tests/hnr/run.sh
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "sverka_shared.h"

static float heat, calm; static Game G; static int ok, all;
static void check(const char *name, int pass, float got, float want) {
  all++; ok += pass;
  printf("CHECK %-34s %s  получено %.3f, ждём %.3f\n", name, pass ? "OK" : "FAIL", got, want);
}
static void fresh(void) { memset(&G, 0, sizeof G); G.pHnrHeat = &heat; G.pHnrCalm = &calm; heat = 0.f; calm = 100.f; }
static void run(float sec, float rate) { for (int i = 0; i < (int)(sec * 60.f + .5f); i++) gameHnrRate(&G, 1.f / 60.f, rate); }
static int near(float a, float b) { return fabsf(a - b) < .002f; }

int main(void) {
  // прибавки: удар машины 5, разбитая 25, предмет 7.5, пешеход 13 (аудит 09.10: запись PS2 tw_v14_multi30 кадр 15099 +13,
  // сбитый ребёнок на снимке; у правил hit-and-run-web было 10), пинок 13 (шкала 100)
  const float amt[] = {5.f, 25.f, 7.5f, 13.f, 10.f};   // сверка g10-flow 02.11 (СВЕРКА_ГОМЕР 3.4): пинок пешехода +10 (сбил машиной +13)
  const char *nm[] = {"прибавка: удар машины 5", "прибавка: разбитая машина 25", "прибавка: предмет 7.5", "прибавка: пешеход 13 (PS2)",
                      "прибавка: пинок 10 (исходник)"};
  for (int t = HNR_CAR_HIT; t <= HNR_PED_KICK; t++) { fresh(); gameHnrOffense(&G, t); check(nm[t], near(heat, amt[t] / 100.f), heat * 100.f, amt[t]); }
  // удар машины вне погони: не раньше 1.5 с после прошлого нарушения
  fresh(); gameHnrOffense(&G, HNR_CAR_HIT); run(1.f, 3.f); gameHnrOffense(&G, HNR_CAR_HIT);
  check("второй удар машины через 1 с — нет", near(heat, .05f), heat * 100.f, 5.f);
  run(.6f, 3.f); gameHnrOffense(&G, HNR_CAR_HIT);
  check("удар машины через 1.6 с — да", near(heat, .10f), heat * 100.f, 10.f);
  fresh(); gameHnrOffense(&G, HNR_PROP); run(.5f, 3.f); gameHnrOffense(&G, HNR_CAR_HIT);
  check("удар машины через 0.5 с после предмета — нет", near(heat, .075f), heat * 100.f, 7.5f);
  fresh(); gameHnrOffense(&G, HNR_PROP); run(.2f, 3.f); gameHnrOffense(&G, HNR_PROP);
  check("предмет за предметом — оба", near(heat, .15f), heat * 100.f, 15.f);
  // спад: 3 с паузы, потом SetHitAndRunDecay (L1 3/с)
  fresh(); gameHnrOffense(&G, HNR_CAR_WRECK); run(2.9f, 3.f);
  check("пауза спада 3 с: через 2.9 с полная", near(heat, .25f), heat * 100.f, 25.f);
  run(5.1f, 3.f);   // 8 с от нарушения: 5 с спада по 3/с
  check("спад 3/с: через 8 с 25 - 15 = 10", fabsf(heat - .10f) < .006f, heat * 100.f, 10.f);
  // погоня: нарушение без паузы спада, удары машины не реже
  fresh(); G.hnrLatched = 1; heat = .5f; gameHnrOffense(&G, HNR_CAR_HIT); run(.2f, 6.f); gameHnrOffense(&G, HNR_CAR_HIT);
  check("погоня: удары машины подряд — оба", fabsf(heat - (.6f - .012f)) < .004f, heat * 100.f, 58.8f);
  run(1.f, 6.f);
  check("погоня: спад 6/с сразу, без паузы", fabsf(heat - (.588f - .06f)) < .004f, heat * 100.f, 52.8f);
  // №723: вход в погоню — нарушение (не защёлкнута) добило шкалу, потом защёлка: у PS2 спад 6/с идёт сразу (tw_v8_stand: 99,78 → 0 за 17,6 с), пауза 3 с только вне погони
  fresh(); heat = .95f; gameHnrOffense(&G, HNR_CAR_WRECK); G.hnrLatched = 1; run(1.f, 6.f);
  check("вход в погоню: спад 6/с сразу, без паузы 3 с", fabsf(heat - .94f) < .004f, heat * 100.f, 94.f);
  fresh(); heat = .95f; gameHnrOffense(&G, HNR_CAR_WRECK); run(1.f, 3.f);
  check("вне погони: пауза 3 с цела", near(heat, 1.f), heat * 100.f, 100.f);
  // не считается: комната, арест, выключено сценарием
  fresh(); G.hnrRoom = 1; gameHnrOffense(&G, HNR_PED_HIT); check("в комнате — нет", near(heat, 0.f), heat * 100.f, 0.f);
  fresh(); G.hnrBusted = 1.f; gameHnrOffense(&G, HNR_PED_HIT); check("во время ареста — нет", near(heat, 0.f), heat * 100.f, 0.f);
  fresh(); G.hnrOff = 1; gameHnrOffense(&G, HNR_PED_HIT); check("DisableHitAndRun — нет", near(heat, 0.f), heat * 100.f, 0.f);
  // потолок
  fresh(); for (int i = 0; i < 6; i++) gameHnrOffense(&G, HNR_CAR_WRECK); check("потолок 100", near(heat, 1.f), heat * 100.f, 100.f);
  // смена машины: та же — шкала остаётся, другая — в ноль, во время ареста — нет
  fresh(); gameHnrVehicle(&G, NULL); gameHnrOffense(&G, HNR_CAR_WRECK); gameHnrVehicle(&G, NULL);
  check("села в ту же машину — шкала та же", near(heat, .25f), heat * 100.f, 25.f);
  gameHnrVehicle(&G, "cletu_v"); check("села в другую — шкала 0", near(heat, 0.f), heat * 100.f, 0.f);
  gameHnrOffense(&G, HNR_CAR_WRECK); G.hnrBusted = 1.f; gameHnrVehicle(&G, "famil_v");
  check("другая машина во время ареста — шкала та же", near(heat, .25f), heat * 100.f, 25.f);
  // сверка g10-flow (02.11.2026): заморозка в разговоре (P03-07), взрыв машины ИИ миссии +25, первая погоня уровня (Д4)
  fresh(); gameHnrOffense(&G, HNR_CAR_WRECK); gameHnrFreeze(&G, 5.f); run(2.f, 100.f);
  check("разговор: шкала замерла", near(heat, .25f), heat * 100.f, 25.f);
  fresh(); gameHnrFreeze(&G, 5.f); gameHnrOffense(&G, HNR_CAR_WRECK); check("разговор: нарушения не считаются", near(heat, 0.f), heat * 100.f, 0.f);
  fresh(); svBoom.seq = 5; G.boomSeen = 5; svBoom.byPlayer = 1; svBoom.kind = 2; svBoom.seq++; gameHnrBoom(&G);
  check("взрыв машины ИИ миссии игроком: +25", near(heat, .25f), heat * 100.f, 25.f);
  gameHnrBoom(&G); check("тот же взрыв второй раз — нет", near(heat, .25f), heat * 100.f, 25.f);
  fresh(); G.boomSeen = svBoom.seq; svBoom.kind = 3; svBoom.seq++; gameHnrBoom(&G); check("взрыв полицейской: не двигает шкалу", near(heat, 0.f), heat * 100.f, 0.f);
  fresh(); G.boomSeen = svBoom.seq; svBoom.kind = 5; svBoom.byPlayer = 1; svBoom.seq++; gameHnrBoom(&G); check("взрыв цели destroy / dump (иммунитет): не двигает шкалу", near(heat, 0.f), heat * 100.f, 0.f);
  { float t1, t2; gameHnrNewLevel(&G); gameHnrChaseStart(&t1, 1.f / 60.f); gameHnrChaseStart(&t2, 1.f / 60.f);
    check("первая погоня уровня: 0,72 с", near(t1, .72f + 1.f / 60.f), t1, .72f + 1.f / 60.f);
    check("вторая погоня уровня: в ближайшем кадре", near(t2, 1.f / 60.f), t2, 1.f / 60.f); }
  { fresh(); int a1 = gameTouchNew(&G, 1), a2 = gameTouchNew(&G, 1); int ok16 = 1; for (unsigned k = 2; k < 18; k++) gameTouchNew(&G, k);   // 16 других вытесняют предмет 1
    int a3 = gameTouchNew(&G, 1);
    check("касание предмета: новый да, повтор нет, после 16 других снова новый", a1 == 1 && a2 == 0 && ok16 && a3 == 1, (float)(a1 * 100 + a2 * 10 + a3), 101.f); }
  { gameCarHealthNote(&G, "famil_v", .4f); gameCarHealthNote(&G, "cletu_v", .9f); gameCarHealthNote(&G, "famil_v", .3f);
    check("прочность по имени машины", near(gameCarHealthGet(&G, "famil_v"), .3f) && near(gameCarHealthGet(&G, "cletu_v"), .9f) && gameCarHealthGet(&G, "x") < 0.f, gameCarHealthGet(&G, "famil_v"), .3f); }
  { float ax, ay; gameStick(.2f, 0.f, &ax, &ay); check("стик .2: зона .25 — ноль", near(ax, 0.f), ax, 0.f);
    gameStick(.26f, 0.f, &ax, &ay); check("стик .26: плавный старт, не .26", ax < .03f && ax > 0.f ? 1 : near(ax, 0.f), ax, .013f);
    gameStick(1.f, 0.f, &ax, &ay); check("стик 1.0: полный", near(ax, 1.f), ax, 1.f); }
  printf("RESULT %d/%d\n", ok, all);
  return ok == all ? 0 : 1;
}
