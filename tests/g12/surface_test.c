// Проверка viewer/surface.c на Маке (сверка g12-data P18-04): level.shr -> секция COL1 -> colInit -> svSurfaceBind -> svSurfaceAt.
// Вход: level.shr, затем пары x z. Печатает тип поверхности и признак помещения. Красный контроль: level.shr без SRF1 -> везде 0.
// cc -std=c11 -O2 -I viewer tests/g12/surface_test.c viewer/surface.c viewer/col.c -lm -o OUT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "col.h"
int svSurfaceAt(float x, float y, float z, int *indoor);
void svSurfaceBind(const ColWorld *w, const void *section, unsigned size);
int svSurfaceSide(const void *side, unsigned n);   // круг 4: файл surf-l<N>.shr рядом с уровнем (переменная окружения SURF_SIDE)
int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: level.shr x z [x z ...]\n"); return 2; }
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
  uint32_t off; memcpy(&off, b + 92, 4);
  uint32_t end = n;   // секция COL1 тянется до PRP: берём до конца файла, colInit и surface проверяют границы по ColHdr
  void *sec = aligned_alloc(16, ((end - off) + 15) & ~15u); memcpy(sec, b + off, end - off);
  static ColWorld w; int r = colInit(&w, sec, end - off);
  if (r) { fprintf(stderr, "colInit %d (%s)\n", r, colError(r)); return 1; }
  svSurfaceBind(&w, sec, end - off);
  if (getenv("SURF_SIDE")) {
    FILE *g = fopen(getenv("SURF_SIDE"), "rb"); if (!g) return 2;
    fseek(g, 0, SEEK_END); long m = ftell(g); fseek(g, 0, SEEK_SET);
    unsigned char *sb = malloc(m); if (fread(sb, 1, m, g) != (size_t)m) return 2; fclose(g);
    fprintf(stderr, "SURF_SIDE: %s\n", svSurfaceSide(sb, (unsigned)m) ? "принят" : "ОТВЕРГНУТ");
  }
  int hist[8] = {0}, indoorN = 0;
  for (int i = 2; i + 1 < argc; i += 2) {
    float x = atof(argv[i]), z = atof(argv[i + 1]), gy, n3[3]; int ind;
    if (!colGround(&w, x, z, 1000.f, -1000.f, &gy, n3)) { printf("(%.1f,%.1f) нет земли\n", x, z); continue; }
    int t = svSurfaceAt(x, gy, z, &ind);
    printf("(%.1f,%.1f) y=%.2f тип %d помещение %d\n", x, z, gy, t, ind);
    hist[t]++; indoorN += ind;
  }
  printf("ИТОГ дорога %d трава %d песок %d гравий %d вода %d настил %d металл %d грунт %d; помещение %d\n", hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7], indoorN);
  return 0;
}
