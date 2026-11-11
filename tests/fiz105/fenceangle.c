// fiz105: то же касание под 10/20/30/45° на 40 и 80 км/ч, но об НАСТОЯЩИЙ забор (ColSeg) уровня: первый ровный отрезок длиннее 80 м. fenceangle level.shr [машина] [индекс отрезка]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
#include "car.h"
static ColWorld w;
static int loadCol(const char *path) {
  FILE *f = fopen(path, "rb"); if (!f) return -1; fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *d = aligned_alloc(16, (n + 15) & ~15); if (fread(d, 1, n, f) != (size_t)n) return -1; fclose(f);
  unsigned pvsOff = *(unsigned *)(d + 88), colOff = *(unsigned *)(d + 92), propOff = *(unsigned *)(d + 96);
  unsigned end = n; if (pvsOff && pvsOff < end && pvsOff > colOff) end = pvsOff; if (propOff && propOff < end && propOff > colOff) end = propOff;
  void *cm = aligned_alloc(16, ((end - colOff) + 15) & ~15); memcpy(cm, d + colOff, end - colOff);
  return colInit(&w, cm, end - colOff);
}
static float wrap(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
int main(int argc, char **argv) {
  if (argc < 2 || loadCol(argv[1])) { puts("usage: fenceangle level.shr [car] [skip]"); return 2; }
  const char *car = argc > 2 && strcmp(argv[2], "-") ? argv[2] : NULL; int skip = argc > 3 ? atoi(argv[3]) : 0, seen = 0;
  int pick = -1; float px = 0, pz = 0, tx = 0, tz = 0, nx = 0, nz = 0, gy = 0;
  for (unsigned i = 0; i < w.nseg && pick < 0; i++) {
    const ColSeg *s = &w.seg[i]; float dx = s->b[0] - s->a[0], dz = s->b[1] - s->a[1], L = hypotf(dx, dz); if (L < 60.f) continue;
    float ux = dx / L, uz = dz / L, n0 = s->n[0], n1 = s->n[1]; int ok = 1; float g0 = 0, g;
    for (int k = 0; k <= 8 && ok; k++) for (int side = -1; side <= 1 && ok; side += 2) {   // ровно: земля вдоль забора и на 12 м от него по лицевой стороне
      float x = s->a[0] + ux * L * (0.2f + 0.6f * k / 8.f) + n0 * 12.f * (side > 0), z = s->a[1] + uz * L * (0.2f + 0.6f * k / 8.f) + n1 * 12.f * (side > 0);
      if (!colGround(&w, x, z, 200.f, -200.f, &g, 0)) { ok = 0; break; } if (k == 0 && side == -1) g0 = g; if (fabsf(g - g0) > 0.4f) ok = 0; }
    if (ok && seen++ >= skip) { pick = (int)i; tx = ux; tz = uz; nx = n0; nz = n1; px = s->a[0] + ux * L * 0.5f; pz = s->a[1] + uz * L * 0.5f; gy = g0; }
  }
  if (pick < 0) { puts("нет подходящего забора"); return 2; }
  printf("забор #%d: центр (%.1f, %.1f), нормаль (%.2f, %.2f), y1=%.0f, земля %.2f\n", pick, px, pz, nx, nz, w.seg[pick].y1, gy);
  static const float ang[] = {10, 20, 30, 45}, kmh[] = {40, 80};
  for (int ki = 0; ki < 2; ki++) for (int ai = 0; ai < 4; ai++) {
    float a = ang[ai] * 0.0174533f, v0 = kmh[ki] / 3.6f;
    // курс: вдоль забора (t) с отклонением к забору на угол a (против нормали)
    float hx = tx * cosf(a) - nx * sinf(a), hz = tz * cosf(a) - nz * sinf(a), L = 3.4f / sinf(a);
    float sx = px + nx * 2.5f - hx * L, sz = pz + nz * 2.5f - hz * L, h = atan2f(hx, hz);
    Car c; CarParams p, *pp = NULL; if (car && carParamsFor(car, &p)) pp = &p;
    carInit(&c, pp, &w, sx, gy, sz, h); c.huskDrive = 1;
    for (int k = 0; k < 40; k++) { CarInput in = {0, 0, 0, 0}; carUpdate(&c, &w, &in, 1 / 60.f); }
    c.vel[0] = hx * v0; c.vel[2] = hz * v0; c.v = v0;
    int hit = -1; float vprev = kmh[ki], vh = 0, h0 = 0, v05 = -1, v1 = -1, h1 = 0, rmax = 0, along = 0;
    for (int f = 0; f < 60 * 6; f++) {
      CarInput in = {hit >= 0 ? 1.f : 0.f, 0.f, 0, 0}; carUpdate(&c, &w, &in, 1 / 60.f);
      float sp = hypotf(c.vel[0], c.vel[2]) * 3.6f;
      if (hit < 0 && (c.wallN[0] * c.wallN[0] + c.wallN[1] * c.wallN[1] > .25f || c.fenceT > 0.f)) { hit = f; h0 = c.h; vh = vprev; }
      vprev = sp;
      if (hit >= 0) { if (f == hit + 30) v05 = sp; if (f == hit + 60) { v1 = sp; h1 = wrap(c.h - h0) * 57.2958f; }
        if (f <= hit + 60 && fabsf(c.roll) * 57.2958f > rmax) rmax = fabsf(c.roll) * 57.2958f; }
    }
    printf("%2.0f° %2.0f км/ч: касание %.0f км/ч, через 0.5 с %.0f, через 1 с %.0f, курс %+.0f°, %s, крен макс %.0f°\n", ang[ai], kmh[ki], vh, v05, v1, h1, v1 < 2.f ? "СТОИТ" : "скользит", rmax);
  }
  return 0;
}
