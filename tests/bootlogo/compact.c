#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bootseq.h"

static unsigned char *read_all(const char *path, int *len) {
  FILE *f = fopen(path, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  unsigned char *p = (unsigned char *)malloc((size_t)n); if (!p || fread(p, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(p); return 0; }
  fclose(f); *len = (int)n; return p;
}

int main(int argc, char **argv) {
  if (argc != 3) return 2;
  int an = 0, bn = 0; unsigned char *a = read_all(argv[1], &an), *b = read_all(argv[2], &bn);
  if (!a || !b) return 2;
  BootMarks x, y; int nx = bootMarksParse(a, an, &x), ny = bootMarksParse(b, bn, &y);
  // 24 знака; №1010: +1 тег El Barto L6 (elbarto6) рядом с прежним
  if (nx < 24 || nx > 25 || ny != nx || x.v != 2 || y.v != 3) { printf("FAIL parse old=%d/v%d new=%d/v%d\n", nx, x.v, ny, y.v); return 1; }
  int mismatch = 0, codex = 0, claude = 0;
  for (int k = 0; k < nx; k++) {
    BootMark *u = &x.m[k], *v = &y.m[k];
    if (strcmp(u->name, v->name) || u->w != v->w || u->h != v->h || u->mode != v->mode) mismatch++;
    if (!strcmp(v->name, "codex")) codex++;
    if (!strcmp(v->name, "claude")) claude++;
    for (int py = 0; py < u->h && py < v->h; py++) for (int px = 0; px < u->w && px < v->w; px++)
      mismatch += u->pal[bmkPix(u, px, py)] != v->pal[bmkPix(v, px, py)];
  }
  if (codex != 12 || claude != 6 || mismatch) { printf("FAIL exact pixel map mismatches=%d Codex=%d Claude=%d\n", mismatch, codex, claude); return 1; }
  printf("PASS BMK2→BMK3 exact pixel maps: %d records, Codex %d, Claude %d\n", nx, codex, claude);
  return 0;
}
