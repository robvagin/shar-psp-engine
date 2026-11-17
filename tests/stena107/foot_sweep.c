// Правка 1265 (Стена 107): Гомер пешком по клетке M7-1 (x 405…440, z −100…−62): из каждой точки сетки 8 направлений по 0,75 с; карта «сколько направлений упёрто».
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) return 2;
  float X0 = 405, X1 = 440, Z0 = -100, Z1 = -62, S = 2.5f;
  bwNew(&W); float y; colGround(bwCol(), 425.f, -80.f, 50.f, -50.f, &y, 0); bwPlace(&W, 425.f, y + .5f, -80.f, 0.f);
  for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  for (int f = 0; f < 60 && !gameOnFoot(&W.G); f++) bwStep(&W, f == 5 ? GP_TRIANGLE : 0, 0, 0);
  if (!gameOnFoot(&W.G)) { printf("не вышел из машины\n"); return 1; }
  BotSnap snap; bwSaveAlloc(&W, &snap); bwSave(&W, &snap);
  printf("карта: строка = z от %.0f к %.0f, столбец = x от %.0f к %.0f, шаг %.1f; цифра = направлений упёрто из 8 (. = нет земли)\n", Z0, Z1, X0, X1, S);
  for (float z = Z0; z <= Z1; z += S) { for (float x = X0; x <= X1; x += S) {
      float gy; if (!colGround(bwCol(), x, z, 20.f, -20.f, &gy, 0)) { putchar('.'); continue; }
      int blocked = 0;
      for (int d = 0; d < 8; d++) {
        bwLoad(&W, &snap); homerPlace(&W.homer, bwCol(), x, gy + .3f, z, 0.f); for (int f = 0; f < 20; f++) bwStep(&W, 0, 0, 0);
        float sx = W.homer.x, sz = W.homer.z, a = d * 0.785398f, dx = sinf(a), dz = cosf(a), r = atan2f(dx, dz) - W.homer.camYaw;
        for (int f = 0; f < 45; f++) bwStep(&W, 0, -sinf(r), -cosf(r));
        float mv = hypotf(W.homer.x - sx, W.homer.z - sz); if (mv < 1.2f) blocked++; }
      putchar(blocked > 8 ? '9' : '0' + blocked); } putchar('\n'); }
  return 0; }
