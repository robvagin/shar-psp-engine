// №1050: проверка «нет проскальзывания сквозь тонкие стенки»: для тонких вертикальных коробок уровня (толщина ≤ 0,5 м, длина ≥ 2,5 м, высота ≥ 0,8 м) кузов 2,1×4,4 м идёт на стенку (шаг 0,3 м, colPushCar после каждого шага)
// под углами 0°, ±25°, ±30° к нормали и вдоль оси (10°…35° к оси) у торцов; считаем, сколько раз центр кузова оказался по другую сторону плоскости стенки в пределах её длины. walls_sim LEVEL.shr MODE(1 новое/0 старое) N_СТЕН
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
static ColWorld W;
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *d = malloc((size + 15) & ~15); fread(d, 1, size, f); fclose(f);
  unsigned co = *(unsigned *)(d + 92), po = *(unsigned *)(d + 96), pv = *(unsigned *)(d + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&W, d + co, ce - co)) return 3;
  colRamp1050 = 0; colRampRail1050 = atoi(argv[2]); int want = atoi(argv[3]);
  int nw = 0, cross = 0, runs = 0, pushedEnd = 0; unsigned stepw = 1;
  for (unsigned bi = 0; bi < W.nbox && nw < want; bi += stepw) {
    const ColBox *B = &W.box[bi]; const float *A[3] = {B->ax, B->ay, B->az}; int v = -1;
    for (int j = 0; j < 3; j++) if (fabsf(A[j][1]) > .9f) v = j; if (v < 0 || 2.f * B->h[v] < .8f || B->pad == 1.f) continue;
    int j1 = (v + 1) % 3, j2 = (v + 2) % 3; float h1 = B->h[j1], h2 = B->h[j2];
    int ti = h1 < h2 ? j1 : j2, li = h1 < h2 ? j2 : j1; float T = 2 * B->h[ti], Ln = 2 * B->h[li];
    if (T > .5f || Ln < 2.5f) continue;
    float dxw = A[li][0], dzw = A[li][2], dl = hypotf(dxw, dzw); dxw /= dl; dzw /= dl; float nxw = -dzw, nzw = dxw;
    float gy; if (!colGround(&W, B->c[0], B->c[2], B->c[1] + 3.f, B->c[1] - 6.f, &gy, 0)) gy = B->c[1] - B->h[v];
    nw++;
    float angs[] = {0, 25, -25, 30, -30}; float offs[] = {0, Ln / 2 - .3f, -(Ln / 2 - .3f), Ln / 2 + .5f, -(Ln / 2 + .5f)};
    for (int side = -1; side <= 1; side += 2) for (int ai = 0; ai < 5; ai++) for (int oi = 0; oi < 5; oi++) {
      float th = angs[ai] * 3.14159265f / 180.f; // угол от нормали стенки
      float hx = side * (nxw * cosf(th) + dxw * sinf(th)), hz = side * (nzw * cosf(th) + dzw * sinf(th));  // курс на стенку (side: с какой стороны)
      float tx = B->c[0] + dxw * offs[oi], tz = B->c[2] + dzw * offs[oi];  // точка цели
      float x = tx - hx * 7.f, z = tz - hz * 7.f; float s0 = (x - B->c[0]) * nxw + (z - B->c[2]) * nzw; int crossed = 0;
      for (int st = 0; st < 50; st++) {
        CarBox b; memset(&b, 0, sizeof b); b.x = x; b.z = z; b.fx = hx; b.fz = hz; b.hw = 1.05f; b.hl = 2.2f; b.y = gy; b.lo = .35f; b.hi = 1.6f; b.fsupOn = 1; b.fsup = gy;
        float mx, mz, nx, nz; colPushCar(&W, &b, &mx, &mz, &nx, &nz); x += mx; z += mz;
        x += hx * .3f; z += hz * .3f;
        float s1 = (x - B->c[0]) * nxw + (z - B->c[2]) * nzw, along = (x - B->c[0]) * dxw + (z - B->c[2]) * dzw;
        if ((s1 > 0) != (s0 > 0) && fabsf(along) < Ln / 2 + .2f) { crossed = 1; break; } }
      if (getenv("DUMP")) printf("w%u a side%d ang%d off%d: %d\n", bi, side, ai, oi, crossed); runs++; cross += crossed; }
    // вдоль оси у торца: курс 10…35° к оси, смещение по нормали 0…1,3 м
    for (int end = -1; end <= 1; end += 2) for (int ai = 0; ai < 4; ai++) for (int lo_ = 0; lo_ < 4; lo_++) {
      float th = (10.f + ai * 8.f) * 3.14159265f / 180.f, lat = .3f + lo_ * .4f;
      float hx = -end * (dxw * cosf(th)) + nxw * sinf(th), hz = -end * (dzw * cosf(th)) + nzw * sinf(th);
      float x = B->c[0] + dxw * end * (Ln / 2 + 6.f) + nxw * lat, z = B->c[2] + dzw * end * (Ln / 2 + 6.f) + nzw * lat; int crossed = 0; float s0 = lat;
      for (int st = 0; st < 50; st++) {
        CarBox b; memset(&b, 0, sizeof b); b.x = x; b.z = z; b.fx = hx; b.fz = hz; b.hw = 1.05f; b.hl = 2.2f; b.y = gy; b.lo = .35f; b.hi = 1.6f; b.fsupOn = 1; b.fsup = gy;
        float mx, mz, nx, nz; colPushCar(&W, &b, &mx, &mz, &nx, &nz); x += mx; z += mz; x += hx * .3f; z += hz * .3f;
        float s1 = (x - B->c[0]) * nxw + (z - B->c[2]) * nzw, along = (x - B->c[0]) * dxw + (z - B->c[2]) * dzw;
        if (getenv("TRACE") && (int)bi == atoi(getenv("TRACE")) && end == 1 && ai == 2 && lo_ == 0) printf("  st%d lat %.2f along %.2f mtv(%.2f %.2f) n%d\n", st, s1, along, mx, mz, 0);
        if ((s1 > 0) != (s0 > 0) && fabsf(along) < Ln / 2 - .2f) { crossed = 1; break; } }
      if (getenv("DUMP")) printf("w%u e end%d ang%d lat%d: %d\n", bi, end, ai, lo_, crossed); runs++; cross += crossed; }
  }
  printf("walls %d, прогонов %d, проскочило сквозь %d\n", nw, runs, cross); return 0; }
