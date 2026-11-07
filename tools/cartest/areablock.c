// Где машина стоит «в коллизии» на открытом месте (правка Роба 1191: «арка с кольцом, машина застревает, как будто невидимый блок»): сетка вокруг точки,
// 4 курса, кузов как у игрока; глубже 0,3 м — строка с примитивом COL1 (b коробка, c цилиндр, s сфера, t треугольник и номер).
//   areablock X Z РАДИУС ШАГ  (cwd с build/level.shr, build/homer.shr; сборка как tests/roadblock/run.sh)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
int main(int argc, char **argv) {
  float cx = argc > 1 ? atof(argv[1]) : 220.f, cz = argc > 2 ? atof(argv[2]) : 172.f, rad = argc > 3 ? atof(argv[3]) : 120.f, st = argc > 4 ? atof(argv[4]) : 2.f;
  if (bwData("build/level.shr", "build/homer.shr", 0)) { fprintf(stderr, "areablock: нет данных уровня\n"); return 2; }
  const ColWorld *w = bwCol(); int n = 0, bad = 0;
  for (float z = cz - rad; z <= cz + rad; z += st) for (float x = cx - rad; x <= cx + rad; x += st) {
    float g; float nn[3]; if (!colGround(w, x, z, 60.f, -1e6f, &g, nn)) continue;   // есть опора сверху
    for (int k = 0; k < 4; k++) { float h = k * 1.5707963f;
      Car c; carInit(&c, NULL, w, x, g, z, h); if (fabsf(c.y - g) > 1.f) continue;
      CarBox b; carBox(&c, &b); colProbeCar(w, &b); n++;
      if (b.maxDepth > .3f) { int q = 0; for (int j = 1; j < b.n && j < COL_MAX_CONTACTS; j++) if (b.depth[j] > b.depth[q]) q = j; bad++;
        printf("%.1f\t%.1f\t%.2f\t%d\t%.2f\t%c%u\n", x, z, g, k, b.maxDepth, "tbcs"[b.type[q] & 3], (unsigned)b.index[q]); } } }
  fprintf(stderr, "проб %d, в коллизии %d\n", n, bad); return 0;
}
