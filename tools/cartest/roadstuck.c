// Где машина застревает на дорогах (правка Роба 1191): по каждому отрезку roads-l1.bin каждые ШАГ м, оба направления вдоль дороги, газ СЕК секунд целым кадром игры (tools/bots/world.h,
// без трафика). Строка на застрявший заезд: отрезок, старт x y z, пройдено м, скорость, ближайший стейтпроп к точке остановки (номер, тип, класс, расстояние).
//   roadstuck X Z РАДИУС ШАГ [СЕК]   (cwd с build/level.shr, build/homer.shr, build/sandbox/roads-l1.bin)
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "world.h"
#include "navgraph.h"
#include "stateprops.h"
int main(int argc, char **argv) {
  float cx = argc > 1 ? atof(argv[1]) : 220.f, cz = argc > 2 ? atof(argv[2]) : 172.f, rad = argc > 3 ? atof(argv[3]) : 200.f, st = argc > 4 ? atof(argv[4]) : 4.f, secs = argc > 5 ? atof(argv[5]) : 3.f;
  setenv("BW_NO_TRAFFIC", "1", 1);
  if (bwData("build/level.shr", "build/homer.shr", 0)) { fprintf(stderr, "roadstuck: нет данных уровня\n"); return 2; }
  navLoad("build/sandbox/roads-l1.bin");
  static BotWorld W; static BotSnap S; if (bwNew(&W)) return 2; bwSaveAlloc(&W, &S); bwSave(&W, &S);
  const ColWorld *col = bwCol(); int n = 0, stuck = 0, frames = (int)(secs * 60.f);
  for (int i = 0; i < nroad; i++) { const Road *r = &roads[i]; float dx = r->bx - r->ax, dz = r->bz - r->az, L = hypotf(dx, dz); if (L < 1.f) continue;
    for (float t = 0; t < L; t += st) for (int dir = 0; dir < 2; dir++) {
      float x = r->ax + dx * t / L, y = r->ay + (r->by - r->ay) * t / L, z = r->az + dz * t / L; if (hypotf(x - cx, z - cz) > rad) continue;
      float h = atan2f(dir ? -dx : dx, dir ? -dz : dz);
      bwLoad(&W, &S); Car c; carInit(&c, NULL, col, x, y, z, h); if (fabsf(c.y - y) > 2.f) continue; CarBox b0; carBox(&c, &b0); colProbeCar(col, &b0); if (b0.maxDepth > .05f) continue;
      W.car = c; n++;
      for (int f = 0; f < frames; f++) bwStep(&W, GP_R, 0.f, 0.f);
      float mx = W.car.x - x, mz = W.car.z - z, d = sqrtf(mx * mx + mz * mz);
      if (d < 4.f + 2.f * secs) { stuck++; int bi = -1; float bd = 1e30f; const StateProps *sp_ = &W.sp;
        for (unsigned k = 0; k < sp_->ninst; k++) { float ex = W.car.x - sp_->run[k].x, ez = W.car.z - sp_->run[k].z, e = ex * ex + ez * ez; if (e < bd) { bd = e; bi = (int)k; } }
        CarBox bf; carBox(&W.car, &bf); colProbeCar(col, &bf); { int q = 0; for (int j = 1; j < bf.n && j < COL_MAX_CONTACTS; j++) if (bf.depth[j] > bf.depth[q]) q = j; printf("COL%.2f/%c%u\t", bf.maxDepth, bf.n ? "tbcs"[bf.type[q] & 3] : '-', bf.n ? (unsigned)bf.index[q] : 0u); }
        int pi = -1; float pd = 1e30f; for (unsigned k = 0; k < W.props.ninst; k++) { float ex = W.car.x - W.props.st[k].x, ez = W.car.z - W.props.st[k].z, e = ex * ex + ez * ez; if (e < pd) { pd = e; pi = (int)k; } }
        printf("%d\t%.1f\t%.1f\t%.1f\t%d\t%.1f\t%.1f\t", i, x, y, z, dir, d, W.car.v);
        if (pi >= 0) printf("pr%d\tPT%u\t%.1f\t", pi, W.props.def[pi].type, sqrtf(pd));
        if (bi >= 0) printf("sp%d\tt%u\tcls%u\t%.1f\t%.1f\t%.1f\n", bi, (unsigned)sp_->inst[bi].type, (unsigned)sp_->type[sp_->inst[bi].type].cls, sqrtf(bd), W.car.x, W.car.z); else printf("-\n"); } } }
  fprintf(stderr, "заездов %d, застряло %d\n", n, stuck); return 0;
}
