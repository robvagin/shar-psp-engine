// Запекание MCPROBE (загрузка 2, пункт 1): mcProbeRoads на Маке движковым кодом -> mcpr.shr (секция MCP1, viewer/msncar.c).
//   mcp_bake LEVEL.shr TRAFFIC.shr OUT.shr        пишет OUT.shr и строку "MCPBAKE roads=N blocked=B"
//   mcp_bake LEVEL.shr TRAFFIC.shr --check FILE   считает заново и сверяет с FILE: 0 — то же, 1 — иначе (гейт tests/load2/mcp.sh)
// Хеш COL1 — по тем же границам, что в levelLoad (viewer/main_levels.h): COL1 до ближайшей из секций PVS1/PRP, иначе до конца файла.
// Сборка: tools/mcp_bake.sh (cc -ffp-contract=off: умножение со сложением как у PSP, без fma)
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
  if (argc < 4) { fprintf(stderr, "usage: mcp_bake LEVEL.shr TRAFFIC.shr OUT.shr | --check FILE\n"); return 2; }
  unsigned n; unsigned char *lvl = readFile(argv[1], &n);
  if (!lvl || n < 104 || (memcmp(lvl, "SHR7", 4) && memcmp(lvl, "SHR8", 4))) { fprintf(stderr, "mcp_bake: нет level.shr\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(lvl + 88), colOff = *(unsigned *)(lvl + 92), propOff = *(unsigned *)(lvl + 96), end = 0;
  if (pvsOff && (!end || pvsOff < end)) end = pvsOff;
  if (propOff && (!end || propOff < end)) end = propOff;
  if (!end) end = n;
  if (!colOff || end <= colOff) { fprintf(stderr, "mcp_bake: нет COL1\n"); return 2; }
  unsigned colHash = mcProbeHash(lvl + colOff, end - colOff);
  static ColWorld w; static Traffic T;
  if (colInit(&w, lvl + colOff, end - colOff)) { fprintf(stderr, "mcp_bake: COL1 не читается\n"); return 2; }
  if (trafficLoad(&T, argv[2])) { fprintf(stderr, "mcp_bake: traffic.shr не читается\n"); return 2; }
  int bad = mcProbeRoads(&T, &w);
  static unsigned char out[MCP_FILE_BYTES]; unsigned sz = mcProbeBakeBuild(&T, colHash, out);
  printf("MCPBAKE roads=%u blocked=%d colhash=%08x\n", T.hd->nRoad, bad, colHash);
  if (!strcmp(argv[3], "--check")) {
    unsigned m; unsigned char *old = argc > 4 ? readFile(argv[4], &m) : 0;
    int same = old && m == sz && !memcmp(old, out, sz); printf("MCPCHECK %s\n", same ? "same" : "DIFFERENT"); return same ? 0 : 1;
  }
  FILE *f = fopen(argv[3], "wb"); if (!f || fwrite(out, 1, sz, f) != sz) { fprintf(stderr, "mcp_bake: не записать %s\n", argv[3]); return 2; }
  fclose(f); return 0;
}

// 107b (Гомер П7/П8): homer.c ссылается на правила объёмов предметов из stateprops.c; запекание их не вызывает (homerSolidSprops только в игре),
// заглушки не тянут stateprops.c, а с ним props и монеты
int spNoSolid(const void *sp, unsigned i) { (void)sp; (void)i; return 0; }
int spVolumeShift(void *sp, unsigned i, float offOld[3], float offW[3]) { (void)sp; (void)i; (void)offOld; (void)offW; return 0; }
