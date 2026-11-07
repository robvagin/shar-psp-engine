// №bake-3Б: запекание разбора COL1 (viewer/col.c colBakeDump) на Маке тем же кодом colInit, и сверка.
//   colbake LEVEL.shr OUT.shr          живой colInit, запись OUT (colbake.shr); печатает "COLBAKE bytes=N key=..."
//   colbake LEVEL.shr --check FILE     0 — одинаково: (1) живой мир == файл побайтно (colBakeVerify), (2) colInitBake из файла даёт мир, чей дамп == файл,
//                                      (3) красные контроли: байт файла испорчен (хеш записи) — colInitBake откатывается на живой расчёт и мир снова == файл; ключ другого входа — то же
// Хеш входа — mcProbeHash по границам COL1 как в levelLoad (то же, что mcp_bake). Сборка: tools/colbake.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msncar.h"
static unsigned char *readFile(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, ((size_t)s + 63) & ~(size_t)63);
  if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (unsigned)s; return b;
}
int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: colbake LEVEL.shr OUT.shr | --check FILE\n"); return 2; }
  unsigned n; unsigned char *lvl = readFile(argv[1], &n);
  if (!lvl || n < 104 || (memcmp(lvl, "SHR7", 4) && memcmp(lvl, "SHR8", 4))) { fprintf(stderr, "colbake: нет level.shr\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(lvl + 88), colOff = *(unsigned *)(lvl + 92), propOff = *(unsigned *)(lvl + 96), end = 0;
  if (pvsOff && (!end || pvsOff < end)) end = pvsOff;
  if (propOff && (!end || propOff < end)) end = propOff;
  if (!end) end = n;
  if (!colOff || end <= colOff) { fprintf(stderr, "colbake: нет COL1\n"); return 2; }
  unsigned key = mcProbeHash(lvl + colOff, end - colOff);
  static ColWorld w, w2;
  if (colInit(&w, lvl + colOff, end - colOff)) { fprintf(stderr, "colbake: COL1 не читается\n"); return 2; }
  unsigned cap = colBakeSizeMax(&w); unsigned char *buf = malloc(cap); unsigned sz = colBakeDump(&w, key, buf, cap);
  if (!sz) { fprintf(stderr, "colbake: не влезло\n"); return 2; }
  if (strcmp(argv[2], "--check")) {
    FILE *f = fopen(argv[2], "wb"); if (!f || fwrite(buf, 1, sz, f) != sz) { fprintf(stderr, "colbake: не записать %s\n", argv[2]); return 2; }
    fclose(f); printf("COLBAKE bytes=%u key=%08x ntri=%u nbox=%u ncyl=%u nseg=%u cells=%dx%d\n", sz, key, w.ntri, w.nbox, w.ncyl, w.nseg, w.W, w.H); return 0;
  }
  if (argc < 4) return 2;
  unsigned fn; unsigned char *file = readFile(argv[3], &fn); int bad = 0;
  if (!file) { fprintf(stderr, "colbake: нет %s\n", argv[3]); return 2; }
  int v = colBakeVerify(&w, key, file, fn); printf("живое против файла: %s\n", v ? "РАЗНО" : "одинаково"); bad |= v != 0;
  if (colInitBake(&w2, lvl + colOff, end - colOff, file, fn, key)) { printf("colInitBake: ошибка\n"); return 1; }
  int v2 = colBakeVerify(&w2, key, file, fn); printf("мир из файла, дамп против файла: %s\n", v2 ? "РАЗНО" : "одинаково"); bad |= v2 != 0; colFree(&w2);
  unsigned char *bad1 = malloc(fn); memcpy(bad1, file, fn); bad1[fn / 2] ^= 0x5a;   // красный контроль 1: порча байта — хеш записи отвергает, мир считается заново
  if (colInitBake(&w2, lvl + colOff, end - colOff, bad1, fn, key)) { printf("контроль 1: colInitBake ошибка\n"); return 1; }
  int v3 = colBakeVerify(&w2, key, file, fn); printf("контроль 1 (порча байта): откат на живое %s\n", v3 ? "НЕ СРАБОТАЛ" : "OK"); bad |= v3 != 0; colFree(&w2);
  if (colInitBake(&w2, lvl + colOff, end - colOff, file, fn, key ^ 1u)) { printf("контроль 2: ошибка\n"); return 1; }
  int v4 = colBakeVerify(&w2, key, file, fn); printf("контроль 2 (чужой ключ входа): откат на живое %s\n", v4 ? "НЕ СРАБОТАЛ" : "OK"); bad |= v4 != 0; colFree(&w2);
  /* красный контроль гейта: испорченный файл с пересчитанным хешем записи не отвергается загрузкой, но живое против него обязано дать РАЗНО */
  unsigned char *bad2 = malloc(fn); memcpy(bad2, file, fn); int vb = colBakeVerify(&w, key, bad1, fn); printf("контроль 3 (сверка видит испорченный файл): %s\n", vb ? "OK (РАЗНО)" : "НЕ СРАБОТАЛ"); bad |= vb == 0; (void)bad2;
  printf("COLBAKE_CHECK %s\n", bad ? "FAIL" : "PASS"); return bad;
}
