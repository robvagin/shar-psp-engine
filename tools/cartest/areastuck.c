// Где машина застревает на ходу (правка Роба 1191: «арка с кольцом, машина застревает, как будто невидимый блок»): сетка стартов вокруг точки, 4 курса, газ 3 с
// целым кадром игры (tools/bots/world.h: carUpdate, пропы, стейтпропы, коллизия), без трафика. Строка на застрявший заезд: старт x z курс, пройдено м, скорость в конце,
// ближайший стейтпроп к точке остановки (номер, тип, класс, расстояние). Сравнивать сборки с -DBUSHFALL1137_OLD=1 и без: застряло только в новой — след моей правки.
//   areastuck X Z РАДИУС ШАГ [СЕК]   (cwd с build/level.shr, build/homer.shr; BW_NO_TRAFFIC=1)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
#include "stateprops.h"
int main(int argc, char **argv) {
  float cx = argc > 1 ? atof(argv[1]) : 220.f, cz = argc > 2 ? atof(argv[2]) : 172.f, rad = argc > 3 ? atof(argv[3]) : 120.f, st = argc > 4 ? atof(argv[4]) : 8.f, secs = argc > 5 ? atof(argv[5]) : 3.f;
  setenv("BW_NO_TRAFFIC", "1", 1);
  if (bwData("build/level.shr", "build/homer.shr", 0)) { fprintf(stderr, "areastuck: нет данных уровня\n"); return 2; }
  static BotWorld W; static BotSnap S; if (bwNew(&W)) return 2; bwSaveAlloc(&W, &S); bwSave(&W, &S);
  const ColWorld *col = bwCol(); int n = 0, stuck = 0, frames = (int)(secs * 60.f);
  for (float z = cz - rad; z <= cz + rad; z += st) for (float x = cx - rad; x <= cx + rad; x += st) {
    float g; float nn[3]; if (!colGround(col, x, z, 60.f, -1e6f, &g, nn)) continue;
    for (int k = 0; k < 4; k++) { float h = k * 1.5707963f;
      bwLoad(&W, &S); Car c; carInit(&c, NULL, col, x, g, z, h); if (fabsf(c.y - g) > 1.f) continue; CarBox b0; carBox(&c, &b0); colProbeCar(col, &b0); if (b0.maxDepth > .05f) continue;   // старт чистый
      W.car = c; n++;
      for (int f = 0; f < frames; f++) bwStep(&W, GP_R, 0.f, 0.f);
      float dx = W.car.x - x, dz = W.car.z - z, d = sqrtf(dx * dx + dz * dz), sp = W.car.v;
      if (d < 4.f + 2.f * secs) {   // за 3 с на газу должно быть заметно дальше 10 м
        stuck++; int bi = -1; float bd = 1e30f; const StateProps *sp_ = &W.sp;
        for (unsigned i = 0; i < sp_->ninst; i++) { float ex = W.car.x - sp_->run[i].x, ez = W.car.z - sp_->run[i].z, e = ex * ex + ez * ez; if (e < bd) { bd = e; bi = (int)i; } }
        printf("%.1f\t%.1f\t%d\t%.1f\t%.1f\t", x, z, k, d, sp);
        if (bi >= 0) printf("sp%d\tt%u\tcls%u\t%.1f\t%.1f,%.1f\n", bi, (unsigned)sp_->inst[bi].type, (unsigned)sp_->type[sp_->inst[bi].type].cls, sqrtf(bd), W.car.x, W.car.z); else printf("-\n"); } } }
  fprintf(stderr, "стартов %d, застряло %d\n", n, stuck); return 0;
}
