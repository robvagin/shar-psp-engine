// Гейт правки 431 (канон «никто не появляется раньше сцены»): постановка Гомера в точку без опоры (воздух над пустотой, вне карты, карман комнат
// в мире города) не даёт ни падения, ни вечного ожидания. Мак, CPU, без эмулятора, повтор по seed.
//   spawn_host LEVEL HOMER TRIALS SEED   (данные уровня как у tools/bots/fallhunt)
// Каждая проба: опорная точка P мира, из неё Q на d м (3, 8, 30, 80, 300) в случайную сторону; Q без опоры (в окне homerPlace) — проба.
// homerPlace(Q), 5 с игры без ввода. Нарушение: тело не ждёт опору, а под ним нет поверхности ближе 2 м (висит/падает). Итог:
//   S<TAB>проб<TAB>нарушений<TAB>ждут_в_конце<TAB>кольцо<TAB>safe<TAB>строк_сторожа
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "watchdog.h"
static BotWorld W; static BotSnap S0; static uint32_t rs = 1;
static uint32_t xr(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static void logLine(const char *l) { if (getenv("SPAWN_VERBOSE")) printf("L\t%s\n", l); }
int main(int argc, char **argv) {
  if (argc < 5) { fprintf(stderr, "spawn_host LEVEL HOMER TRIALS SEED\n"); return 2; }
  if (bwData(argv[1], argv[2], 0)) { fprintf(stderr, "spawn_host: нет данных\n"); return 2; }
  int trials = atoi(argv[3]); rs = (uint32_t)atoi(argv[4]) | 1;
  const ColWorld *cw = bwCol();
  bwNew(&W); bwSaveAlloc(&W, &S0); bwSave(&W, &S0); wdSetLog(logLine);
  static const float D[] = {3.f, 8.f, 30.f, 80.f, 300.f};
  int done = 0, cviol = 0, viol = 0, waiting = 0, rings = 0, safes = 0; unsigned fires = 0;
  for (int t = 0; t < trials * 40 && done < trials; t++) {
    if (!cw->ntri) break;
    const ColTri *T = &cw->tri[xr() % cw->ntri]; if (T->n[1] < .7f) continue;
    float px = (T->a[0] + T->b[0] + T->c[0]) / 3.f, pz = (T->a[2] + T->b[2] + T->c[2]) / 3.f, py = (T->a[1] + T->b[1] + T->c[1]) / 3.f;
    float d = D[xr() % 5], a = (xr() % 6283) / 1000.f, qx = px + d * sinf(a), qz = pz + d * cosf(a), y0 = py + .5f, g;
    if (colGround(cw, qx, qz, y0 + 2.f, y0 - 100.f, &g, 0)) continue;   // с опорой — не проба
    bwLoad(&W, &S0); wdReset();
    homerPlace(&W.homer, bwCol(), px, py, pz, 0.f);           // надёжное место: опорная точка (как прежняя законная постановка)
    for (int f = 0; f < 40; f++) bwStep(&W, 0, 0, 0);
    if (!W.homer.grounded || fabsf(W.homer.y - py) > 2.f) continue;   // опорная точка не держит стоящего (куст, тонкая плёнка): не годится в надёжное место
    homerPlace(&W.homer, bwCol(), qx, y0, qz, 0.f);           // постановка без опоры
    int cbad = 0;
    { bwLoad(&W, &S0); carPlace(&W.car, cw, px, py + .3f, pz, 0.f); for (int f = 0; f < 40; f++) bwStep(&W, 0, 0, 0);   // машина стоит на опорной точке; потом постановка без опоры
      carPlace(&W.car, cw, qx, y0, qz, 0.f); for (int f = 0; f < 300; f++) bwStep(&W, 0, 0, 0);
      float gc, gx_; int near_ = 0; { static const float OX[4] = {2.f, -2.f, 0.f, 0.f}, OZ[4] = {0.f, 0.f, 2.f, -2.f};
        for (int k = 0; k < 4; k++) if (W.car.grounded && colGround(cw, W.car.x + OX[k], W.car.z + OZ[k], W.car.y + 2.f, W.car.y - 2.f, &gx_, 0)) near_ = 1; }   // на краю уступа центр над пустотой, колёса на опоре — не падение
      if (!near_ && !colGround(cw, W.car.x, W.car.z, W.car.y + 2.f, W.car.y - 2.f, &gc, 0)) { cbad = 1; if (getenv("SPAWN_VERBOSE")) printf("C\td=%.0f want=%.1f,%.1f,%.1f car=%.2f,%.2f,%.2f gr=%d rings=%d keeps=%d\n", d, qx, y0, qz, W.car.x, W.car.y, W.car.z, W.car.grounded, W.car.spawnRings, W.car.spawnKeeps); }
      bwLoad(&W, &S0); homerPlace(&W.homer, bwCol(), px, py, pz, 0.f); for (int f = 0; f < 40; f++) bwStep(&W, 0, 0, 0); homerPlace(&W.homer, bwCol(), qx, y0, qz, 0.f); }
    int bad = 0;
    for (int f = 0; f < 480; f++) {
      bwStep(&W, 0, 0, 0);
      const Homer *H = &W.homer; float gg;
      if (getenv("SPAWN_TRACE") && f % 15 == 0) printf("T\t%d\t%.2f %.2f %.2f gr=%d wait=%d st=%d\n", f, H->x, H->y, H->z, H->grounded, H->spawnWait, H->state);
      if (!homerSpawnPending(H) && !H->grounded && !colGround(cw, H->x, H->z, H->y + 2.f, H->y - 2.f, &gg, 0)) { bad = 1; }
    }
    if (bad) printf("V\td=%.0f q=%.1f,%.1f,%.1f now=%.2f,%.2f,%.2f state=%d gr=%d wait=%d fixes=%d rings=%d safes=%d\n", d, qx, y0, qz, W.homer.x, W.homer.y, W.homer.z, W.homer.state, W.homer.grounded, W.homer.spawnWait, W.homer.spawnFixes, W.homer.spawnRings, W.homer.spawnSafes);
    if (bad) { float a1=-9,a2=-9,a3=-9,n3[3]={0}; int r1=colGround(cw,W.homer.x,W.homer.z,W.homer.y+2.f,W.homer.y-2.f,&a1,n3), r2=colGround(cw,W.homer.x,W.homer.z,W.homer.y+3.f,W.homer.y-100.f,&a2,0), r3=colGround(cw,W.homer.x,W.homer.z,W.homer.y+.01f,-1e6f,&a3,0); printf("D\tnear=%d %.3f ring=%d %.3f low=%d %.3f\n",r1,a1,r2,a2,r3,a3); }
    cviol += cbad; viol += bad; waiting += homerSpawnPending(&W.homer); rings += W.homer.spawnRings; safes += W.homer.spawnSafes; fires += wdCount("spawn") + wdCount("nofloor") + wdCount("belowworld");
    done++;
  }
  printf("S\t%d\t%d\t%d\t%d\t%d\t%u\t%d\n", done, viol, waiting, rings, safes, fires, cviol);
  return 0;
}
