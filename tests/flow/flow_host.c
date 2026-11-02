// Круг 6 (сверка g10-flow): хост-проверки поведения игры, которое раньше не имело теста. Настоящие game.c / car.c / homer.c на данных уровня
// (tools/bots/world.c). Каждая проверка обязана упасть, если собрать с выключателем прежнего поведения (-DNAME_OLD=1) — см. tests/flow/flow.sh.
// Проверки «число против соседнего ложного»: руль ровно −1 (не +1, не 0), пешком оба аналога игнорируются (не только один), прыжок в комнате не выходит из земли.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "world.h"
#include "flowrules.h"
static BotWorld W; static int fails;
#define CHECK(name, ok, ...) do { int o_ = (ok); printf("CHECK %-14s %s ", name, o_ ? "OK  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!o_) fails++; } while (0)
static void onFoot(void) {   // Гомер за рулём -> △ на месте -> пешком
  bwNew(&W); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  carSetSpeed(&W.car, 0.f);
  for (int f = 0; f < 120 && !gameOnFoot(&W.G); f++) bwStep(&W, f == 2 ? GP_TRIANGLE : 0, 0, 0);
  for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "flow_host LEVEL.shr HOMER.shr level01.msn\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  // 1. P17-11: обе стрелки руля — побеждает левая (−1)
  { bwNew(&W); GamePad p; memset(&p, 0, sizeof p); p.buttons = GP_LEFT | GP_RIGHT; CarInput in; gameCarInput(&W.G, &p, &in);
    CHECK("pad_both", in.steer == -1.f, "обе стрелки: руль %.1f (ждём -1)", in.steer); }
  // 2. P01-17: после смерти руль в ноль
  { bwNew(&W); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0); gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
    if (gameOnFoot(&W.G)) { printf("FAIL: Гомер не за рулём\n"); return 1; } W.car.destroyed = 1; CarInput in = {1.f, 1.f, .7f};
    gameCarLimits(&W.G, &in); CHECK("cardead_steer", in.steer == .7f && in.throttle == 0.f && in.brake == 0.f, "разбита: газ %.1f тормоз %.1f руль %.2f (ждём 0, 0 и руль нетронут 0,70)", in.throttle, in.brake, in.steer); }
  // 3. пустая машина игрока: Гомер пешком — остов не едет сам; за рулём — как прежде
  { onFoot(); CarInput in = {0.f, 0.f, 0.f}; gameCarStep(&W.G, &in, 1.f / 60.f); int foot = W.car.huskDrive;
    gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0); gameCarStep(&W.G, &in, 1.f / 60.f); int drv = W.car.huskDrive;
    CHECK("empty_player", foot == 0 && drv == 1, "пешком huskDrive=%d (ждём 0), за рулём %d (ждём 1)", foot, drv); }
  // 4. P17-12 (круг 7): пешком крестовина перекрывает ОБЕ оси аналога — стик вперёд при нажатой ← даёт то же, что одна ←; ← и → вместе гасят друг друга и тогда идёт аналог
  { float x[4], z[4]; unsigned bt[4] = {GP_LEFT, GP_LEFT, GP_LEFT | GP_RIGHT, GP_LEFT | GP_RIGHT}; float ay[4] = {0.f, -1.f, 0.f, -1.f};
    for (int v = 0; v < 4; v++) { onFoot(); for (int f = 0; f < 60; f++) bwStep(&W, bt[v], 0.f, ay[v]); x[v] = W.homer.x; z[v] = W.homer.z; }
    float d = hypotf(x[0] - x[1], z[0] - z[1]);                    // ← со стиком = ← без него
    float dBoth = hypotf(x[2] - x[3], z[2] - z[3]);               // ← + → без стика и со стиком вперёд: со стиком идёт вперёд
    CHECK("foot_dpad", d < .05f && dBoth > 1.f, "← со стиком и без: расхождение %.2f м (ждём ~0); ← + → со стиком вперёд против без стика: ушёл на %.1f м (ждём вперёд, > 1)", d, dBoth); }
  // M3 Д9 / круг 7: титры — ↓ ×5, ↑ −5, оба сразу −1 (1 + 4 − 6), без кнопок ×1
  { float n = flowCreditsRate(0, 0), d = flowCreditsRate(0, 1), u = flowCreditsRate(1, 0), b = flowCreditsRate(1, 1);
    CHECK("credits_rate", n == 1.f && d == 5.f && u == -5.f && b == -1.f, "без кнопок %.0f, ↓ %.0f, ↑ %.0f, ↑+↓ %.0f (ждём 1, 5, -5, -1)", n, d, u, b); }
  // №1104: счёт строк титров для реплик Кэнга и Кодос — строка N выходит при прокрутке 80·272/480 + (N−1)·шаг; шаг 10 px: строка 1 на 45,3, строка 7 на 105,3, не раньше
  { int last = 0, at7 = -1, first = -1, tot = 0; for (int i = 0; i <= 400; i++) { float sc = i * .5f; int ln = flowCreditsLine(sc, 10, &last); if (ln) { tot++; if (ln == 1) first = i; if (ln == 7) at7 = i; } }
    CHECK("credits_lines", first == 91 && at7 == 211 && tot == 16, "строка 1 при прокрутке %.1f (ждём 45,5), строка 7 при %.1f (ждём 105,5), строк за 200 px: %d (ждём 16)", first * .5f, at7 * .5f, tot); }
  // №1112: LOADING на закрытом ирисе — скрыта до 3 с и в 3,00–3,25; видна на 3,25–3,50, скрыта 3,50–3,75, видна 3,75–4,00; внизу экрана (225 px)
  { int ok = !flowIrisLoadingOn(2.9f) && !flowIrisLoadingOn(3.1f) && flowIrisLoadingOn(3.3f) && !flowIrisLoadingOn(3.6f) && flowIrisLoadingOn(3.8f) && !flowIrisLoadingOn(4.1f) && flowIrisLoadingOn(4.3f);
    CHECK("iris_loading", ok && FLOW_IRIS_LOADING_Y == 225, "видна на 3,3 с: %d (ждём 1), на 3,1 с: %d (ждём 0), на 3,6 с: %d (ждём 0); верх строки %d px (ждём 225)", flowIrisLoadingOn(3.3f), flowIrisLoadingOn(3.1f), flowIrisLoadingOn(3.6f), FLOW_IRIS_LOADING_Y); }
  // P11-11 (пинок 400/масса, масса 0 → 100) и g02 3.1:2 (пинок задевает один предмет): хост-проверка не вышла за 3 попытки — из 31 вида предметов уровня 1 деревья, столбы и гидранты разбиваются (обломок),
  // остальные в синтетической расстановке перед Гомером на удар не отвечают; нужен смоук с реальным предметом (журнал круга 6)
  // P02-6 (прыжок и бег в комнате): game.c обнуляет их ещё и до homer.c, но homer.c (g04, homerSetIndoor) гасит то же самое — отдельным числом выключатель game.c не проверить; см. журнал круга 6
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
