// Правка 168 (Роб 01.10: «еду, жму △ — должен притормозить и выйти, а сейчас на ходу ничего не работает»): игра без экрана
// (tools/bots/world.c) с настоящими game.c / car.c / homer.c на данных уровня. Машина на старте разогнана до V м/с прямо по
// дороге, один нажим △ — за 6 с машина сама тормозит и Гомер выходит (пешком). Повторный △ до остановки — отмена: едет дальше.
#include <stdio.h>
#include <math.h>
#include "world.h"
static BotWorld W;
// сверка g10-flow (02.11.2026, СВЕРКА_ГОМЕР 5.4): выше 30 км/ч △ держат (mode 0: держит до выхода), ниже — достаточно нажать; отмена: mode 1 — отпустил △ на f=20,
// mode 2 — газ (R) с f=20. Нажатие после старта на f=2
static int run(float v, int mode, float *tExit, float *vMin) {
  bwNew(&W); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);   // Гомер за рулём (как RESET CAR)
  if (gameOnFoot(&W.G)) { printf("FAIL: Гомер не за рулём\n"); return 0; }
  carSetSpeed(&W.car, v); *tExit = -1.f; *vMin = 1e9f;
  for (int f = 0; f < 360; f++) {
    unsigned b = 0;
    if (v >= 8.34f) { if (f >= 2 && !(mode == 1 && f >= 20)) b |= GP_TRIANGLE; }   // быстрее 30 км/ч — держать
    else if (f == 2) b |= GP_TRIANGLE;
    if (mode == 2 && f >= 20) b |= GP_R;
    bwStep(&W, b, 0, 0); float s = fabsf(W.car.v); if (s < *vMin) *vMin = s;
    if (gameOnFoot(&W.G)) { *tExit = f / 60.f; return 1; }
  }
  return 0;
}
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "exit_host LEVEL.shr HOMER.shr level01.msn\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  int fails = 0; float t, vm;
  float vs[4] = {5.f, 10.f, 20.f, 30.f};
  for (int i = 0; i < 4; i++) {
    int out = run(vs[i], 0, &t, &vm);
    printf("CHECK exit  %s  %.0f м/с: △ на ходу — %s за %.2f с (мин. скорость %.1f)\n", out && t < 8.f ? "OK  " : "FAIL", vs[i], out ? "вышел" : "НЕ вышел", t, vm);
    if (!(out && t < 8.f)) fails++;
  }
  { int out = run(20.f, 1, &t, &vm);
    printf("CHECK cancel %s отпустил △ выше 30 км/ч — %s\n", !out ? "OK  " : "FAIL", out ? "всё равно вышел" : "отмена, едет дальше");
    if (out) fails++; }
  { int out = run(5.f, 2, &t, &vm);
    printf("CHECK gas    %s газ после △ — %s\n", !out ? "OK  " : "FAIL", out ? "всё равно вышел" : "выход отменён газом");
    if (out) fails++; }
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
