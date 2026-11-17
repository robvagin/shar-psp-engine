// Пункт 2 «загрузки 2»: lmap.shr — карта высот и стены level.shr (плотный массив после заголовка) блоком LZ4 (viewer/lmap.h).
//   lmap_pack LEVEL.shr OUT.shr            пишет OUT.shr, строка "LMAP dense=.. comp=.. ratio=.."
//   lmap_pack LEVEL.shr --check FILE       0 — файл совпадает с тем, что получилось бы сейчас; и распаковка даёт те же байты
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LMP_BUILD 1
#include "lmap.h"
#include "lmappage.h"
static unsigned char *readFile(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = malloc((size_t)s + 64); if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (unsigned)s; return b;
}
// простейший жадный кодировщик блока LZ4 (хеш 4 байт, один кандидат): хватает для карт с длинными пустыми прогонами
static unsigned lz4Enc(const unsigned char *s, unsigned n, unsigned char *d) {
  static int tab[1 << 16]; memset(tab, -1, sizeof tab);
  unsigned char *dp = d; unsigned i = 0, anchor = 0, limit = n > 12 ? n - 12 : 0;
  while (i < limit) {
    unsigned w = s[i] | s[i + 1] << 8 | s[i + 2] << 16 | (unsigned)s[i + 3] << 24, h = (w * 2654435761u) >> 16; int c = tab[h]; tab[h] = (int)i;
    if (c < 0 || i - (unsigned)c > 65535 || memcmp(s + c, s + i, 4)) { i++; continue; }
    unsigned ml = 4; while (i + ml < n - 5 && s[c + ml] == s[i + ml]) ml++;
    unsigned ll = i - anchor; unsigned char *tok = dp++;
    *tok = (unsigned char)((ll >= 15 ? 15 : ll) << 4);
    if (ll >= 15) { unsigned r = ll - 15; while (r >= 255) { *dp++ = 255; r -= 255; } *dp++ = (unsigned char)r; }
    memcpy(dp, s + anchor, ll); dp += ll;
    unsigned off = i - (unsigned)c; *dp++ = (unsigned char)off; *dp++ = (unsigned char)(off >> 8);
    unsigned m = ml - 4; *tok |= m >= 15 ? 15 : m;
    if (m >= 15) { unsigned r = m - 15; while (r >= 255) { *dp++ = 255; r -= 255; } *dp++ = (unsigned char)r; }
    i += ml; anchor = i;
  }
  unsigned ll = n - anchor; unsigned char *tok = dp++; *tok = (unsigned char)((ll >= 15 ? 15 : ll) << 4);
  if (ll >= 15) { unsigned r = ll - 15; while (r >= 255) { *dp++ = 255; r -= 255; } *dp++ = (unsigned char)r; }
  memcpy(dp, s + anchor, ll); dp += ll; return (unsigned)(dp - d);
}
// 108 П1: версия 2 — готовое хранилище страниц (viewer/lmappage.h) вместо плотного массива; level.shr не меняется.
//   lmap_pack LEVEL.shr OUT.shr [--v1]     по умолчанию версия 2 (--v1 — прежний плотный блок)
//   lmap_pack LEVEL.shr --check FILE       0 — файл совпадает с тем, что получилось бы сейчас (той же версии)
//   каждый запуск версии 2 сверяет ВСЕ ячейки сетки: читатель страниц (lmpHm / lmpWl) против плотного массива level.shr (строка LMAPGRID, ненулевое расхождение — выход 3)
int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: lmap_pack LEVEL.shr OUT.shr [--v1] | --check FILE\n"); return 2; }
  unsigned n; unsigned char *lvl = readFile(argv[1], &n);
  if (!lvl || n < 104 || memcmp(lvl, "SHR7", 4)) { fprintf(stderr, "lmap_pack: нет level.shr (SHR7)\n"); return 2; }
  const int *hd = (const int *)(lvl + 12); unsigned hw = (unsigned)hd[14], hh = (unsigned)hd[15];
  unsigned dend = lmapDenseEnd(hw, hh); if (!hw || !hh || dend > n) { fprintf(stderr, "lmap_pack: сетка %ux%u не из этого файла\n", hw, hh); return 2; }
  int v1 = 0; for (int i = 2; i < argc; i++) if (!strcmp(argv[i], "--v1")) v1 = 1;
  unsigned dn = dend - 104; unsigned char *src = lvl + 104; unsigned sn = dn; unsigned char *store = 0;
  LmapHead h; memset(&h, 0, sizeof h); memcpy(h.magic, LMAP_MAGIC, 4); h.ver = v1 ? 1 : 2; h.key = lmapKey(lvl, n);
  if (!v1) {
    const short *hm = (const short *)(lvl + 104); unsigned p = 104 + hw * hh * 4 * 2; p = (p + 15) & ~15u; const short *wl = (const short *)(lvl + p);
    store = malloc(dn + 8192); sn = lmpBuild(hm, wl, hw, hh, store); src = store;
    LmPg g; int er = lmpOpen(&g, store, sn, hw, hh); if (er) { fprintf(stderr, "lmap_pack: хранилище не открывается (%d)\n", er); return 2; }
    unsigned long bad = 0, cells = 0, wallbad = 0; short tmp[4];
    for (unsigned z = 0; z < hh; z++) for (unsigned x = 0; x < hw; x++) { cells++;
      const short *a = lmpHm(&g, (int)x, (int)z, tmp), *b = hm + ((size_t)z * hw + x) * 4; if (memcmp(a, b, 8)) bad++;
      const short *wa = lmpWl(&g, (int)x, (int)z), *wb = wl + ((size_t)z * hw + x) * 2; if ((wa[0] > wa[1]) != (wb[0] > wb[1]) || (wb[0] <= wb[1] && memcmp(wa, wb, 4))) wallbad++; }
    printf("LMAPGRID cells=%lu hm_bad=%lu wall_bad=%lu store=%u dense=%u\n", cells, bad, wallbad, sn, dn);
    if (bad || wallbad) return 3;
    h.pad[0] = dn;
  }
  unsigned char *buf = malloc(sn + sn / 255 + 64); unsigned cn = lz4Enc(src, sn, buf);
  unsigned char *chk = malloc(sn); if (lmapUnpack(buf, cn, chk, sn) || memcmp(chk, src, sn)) { fprintf(stderr, "lmap_pack: распаковка не даёт исходное\n"); return 2; }
  h.denseBytes = sn; h.compBytes = cn; h.denseHash = lmapHash(src, sn);
  unsigned total = (unsigned)sizeof h + cn; unsigned char *out = malloc(total); memcpy(out, &h, sizeof h); memcpy(out + sizeof h, buf, cn);
  printf("LMAP v%d dense=%u store=%u comp=%u ratio=%.3f\n", (int)h.ver, dn, sn, cn, (double)cn / sn);
  if (!strcmp(argv[2], "--check")) { unsigned m; unsigned char *old = argc > 3 ? readFile(argv[3], &m) : 0; int same = old && m == total && !memcmp(old, out, total); printf("LMAPCHECK %s\n", same ? "same" : "DIFFERENT"); return same ? 0 : 1; }
  FILE *f = fopen(argv[2], "wb"); if (!f || fwrite(out, 1, total, f) != total) { fprintf(stderr, "lmap_pack: не записать\n"); return 2; } fclose(f); return 0;
}
