// Поиск участков рельефа (COL_TRI) с наклоном 33–50° и длиной подъёма ≥ 6 м на реальных уровнях: печатает начало (низ), курс подъёма, набор высоты. Для гейта В1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "col.h"
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static ColWorld world;
static int g(float x, float z, float *y, float n[3], int *type) { int mat; uint32_t ref; if (!colGroundMatId(&world, x, z, 400.f, -400.f, y, n, &mat, &ref)) return 0; *type = (int)(ref >> 29); return 1; }
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f), end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); if (!cm || colInit(&world, cm, end - colOff)) return 2;
  float cx = argc > 2 ? atof(argv[2]) : 0, cz = argc > 3 ? atof(argv[3]) : 0, R = argc > 4 ? atof(argv[4]) : 400; int found = 0;
  for (float x = cx - R; x < cx + R && found < 12; x += 2.f) for (float z = cz - R; z < cz + R && found < 12; z += 2.f) {
    float y, n[3]; int t; if (!g(x, z, &y, n, &t) || t != 0 || n[1] < .64f || n[1] > .85f) continue;
    float hl = hypotf(n[0], n[2]); if (hl < 1e-3f) continue; float ux = -n[0] / hl, uz = -n[2] / hl;   // вверх по склону
    // вниз до начала склона
    float bx = x, bz = z, by = y; for (int s = 0; s < 40; s++) { float y2, n2[3]; int t2; float px = bx - ux * 1.f, pz = bz - uz * 1.f; if (!g(px, pz, &y2, n2, &t2) || t2 != 0 || n2[1] < .64f || n2[1] > .85f || y2 > by) break; bx = px; bz = pz; by = y2; }
    int ok = 1; float top = by, lastY = by; int len = 0;
    for (int s = 1; s <= 12; s++) { float y2, n2[3]; int t2; float px = bx + ux * s, pz = bz + uz * s; if (!g(px, pz, &y2, n2, &t2) || t2 != 0 || n2[1] < .64f || n2[1] > .85f || y2 < lastY + .2f) { ok = s >= 7; break; } lastY = y2; len = s; top = y2; }
    if (!ok || len < 7) continue;
    printf("slope from %.1f %.1f %.2f dir %.3f %.3f len %d rise %.2f ny %.2f\n", bx, bz, by, ux, uz, len, top - by, n[1]); found++; x += 20.f;
  }
  return 0;
}
