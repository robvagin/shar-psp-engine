// №1191: машина проезжает через подвешенное кольцо/арку типа 78 L1 (156,7; 5,2; 159,9) на разных скоростях; run: pass LEVEL.shr — печатает по скорости: дошла ли за объект.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
int main(void) {
  setenv("BW_NO_TRAFFIC", "1", 1);
  if (bwData("build/level.shr", "build/homer.shr", 0)) { fprintf(stderr, "нет данных уровня\n"); return 2; }
  static BotWorld W; static BotSnap S; if (bwNew(&W)) return 2; bwSaveAlloc(&W, &S); bwSave(&W, &S);
  const ColWorld *col = bwCol(); int passed = 0, n = 0;
  float v0[5] = {0.f, 3.f, 6.f, 10.f, 15.f};
  for (int k = 0; k < 5; k++) for (int dir = 0; dir < 2; dir++) {   // с запада на восток и обратно, на уровне объекта
    float sx = dir ? 172.f : 143.f, z = 160.2f, h = dir ? -1.5707963f : 1.5707963f, g; float nn[3];
    if (!colGround(col, sx, z, 60.f, -1e6f, &g, nn)) continue;
    bwLoad(&W, &S); Car c; carInit(&c, NULL, col, sx, g, z, h); W.car = c; W.car.v = v0[k];
    float vmin = 1e9f; for (int f = 0; f < 60 * 5; f++) { bwStep(&W, GP_R, 0.f, 0.f); float xx = W.car.x; if (dir ? (xx < 164.f && xx > 150.f) : (xx > 150.f && xx < 164.f)) if (W.car.v < vmin) vmin = W.car.v; }
    int ok = dir ? W.car.x < 142.f : W.car.x > 172.f; passed += ok; n++;
    printf("v0 %.0f dir %d конец x %.1f мин.скорость у объекта %.1f %s\n", v0[k], dir, W.car.x, vmin > 1e8f ? -1.f : vmin, ok ? "прошла" : "ЗАСТРЯЛА"); }
  printf("прошло %d из %d\n", passed, n); return 0;
}
