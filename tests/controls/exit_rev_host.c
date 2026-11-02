// №927: △ на заднем ходу (5–10 м/с) — машина тормозит и Гомер выходит за N с. Игра без экрана (tools/bots/world.c), как exit_host.c.
#include <stdio.h>
#include <math.h>
#include "world.h"
static BotWorld W;
static int run(float v, float *tExit, float *vMin) {
  bwNew(&W); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  if (gameOnFoot(&W.G)) { printf("FAIL: Гомер не за рулём\n"); return 0; }
  carSetSpeed(&W.car, -v); *tExit = -1.f; *vMin = 1e9f;
  for (int f = 0; f < 360; f++) {
    bwStep(&W, (v >= 8.34f ? f >= 2 : f == 2) ? GP_TRIANGLE : 0, 0, 0);   /* сверка g10-flow: выше 30 км/ч △ держат */ float s = fabsf(W.car.v); if (s < *vMin) *vMin = s;
    if (gameOnFoot(&W.G)) { *tExit = f / 60.f; return 1; }
  }
  return 0;
}
int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "exit_rev_host LEVEL.shr HOMER.shr level01.msn N_SEC\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  float lim = (float)atof(argv[4]); int fails = 0; float t, vm; float vs[3] = {5.f, 7.5f, 10.f};
  for (int i = 0; i < 3; i++) {
    int out = run(vs[i], &t, &vm);
    printf("CHECK rev  %s  задний ход %.1f м/с: △ — %s за %.2f с (предел %.1f), мин. скорость %.1f\n", out && t <= lim ? "OK  " : "FAIL", vs[i], out ? "вышел" : "НЕ вышел", t, lim, vm);
    if (!(out && t <= lim)) fails++;
  }
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
