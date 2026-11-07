// 1Б: проверка пула клеток на Маке (viewer/cellpool.h без PSP). Куча подменена учебной: свой первый-подходящий аллокатор с границами блоков в одном
// буфере 64 МБ, поэтому видно, какой наибольший цельный кусок остаётся. Две части:
//  1) верность: случайные клетки (p50 12 КБ, p90 55 КБ, до 235 КБ) и их хвосты, живые блоки заполнены узором и проверяются при освобождении; ячейки не перекрываются,
//     счётчики сходятся, после выгрузки всех клеток плиты и куски вернулись в кучу (утечек нет).
//  2) дробление: тот же поток «клетки приходят и уходят» вперемешку с долгоживущими предметами сцены 0,1–3 МБ; печатается наибольший свободный кусок
//     и свободное всего для: old (клетки прямо в кучу), a (классы 16…192), b (мельче). Это модель, не железо: проверка на эмуляторе и PSP отдельно.
// cc -O1 -DCP_HOST -o host tests/cellpool1b/host.c && ./host [seed]   -> строки «ok:» / «FAIL:», в конце PASS или FAIL. Сборка: -lm.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>
struct _reent;
#define ARENA (64u << 20)
typedef struct Bk { unsigned off, size; int used; struct Bk *next, *prev; } Bk;
static unsigned char *arena; static Bk *head; static int arenaInit;
static void *H_alloc(size_t sz, size_t al) {
  if (!arenaInit) { arena = aligned_alloc(4096, ARENA); head = calloc(1, sizeof(Bk)); head->size = ARENA; arenaInit = 1; }
  sz = (sz + 63u) & ~63u; if (!sz) sz = 64;
  for (Bk *b = head; b; b = b->next) {
    if (b->used) continue; unsigned a = (b->off + (unsigned)al - 1u) & ~((unsigned)al - 1u), pad = a - b->off;
    if (b->size < pad + sz) continue;
    if (pad) { Bk *n = calloc(1, sizeof(Bk)); n->off = b->off; n->size = pad; n->next = b; n->prev = b->prev; if (b->prev) b->prev->next = n; else head = n; b->prev = n; b->off = a; b->size -= pad; }
    if (b->size > sz) { Bk *n = calloc(1, sizeof(Bk)); n->off = b->off + sz; n->size = b->size - sz; n->next = b->next; n->prev = b; if (b->next) b->next->prev = n; b->next = n; b->size = sz; }
    b->used = 1; return arena + b->off; }
  return 0;
}
static void H_free(void *p) {
  if (!p) return; unsigned off = (unsigned)((unsigned char *)p - arena); Bk *b = head; while (b && !(b->used && b->off == off)) b = b->next; if (!b) { fprintf(stderr, "двойное или чужое free\n"); exit(2); }
  b->used = 0;
  if (b->next && !b->next->used) { Bk *n = b->next; b->size += n->size; b->next = n->next; if (n->next) n->next->prev = b; free(n); }
  if (b->prev && !b->prev->used) { Bk *q = b->prev; q->size += b->size; q->next = b->next; if (b->next) b->next->prev = q; free(b); }
}
static void H_stat(unsigned *freeTot, unsigned *maxBlk) { *freeTot = *maxBlk = 0; for (Bk *b = head; b; b = b->next) if (!b->used) { *freeTot += b->size; if (b->size > *maxBlk) *maxBlk = b->size; } }
#define memalign(al, sz) H_alloc((sz), (al))
#define free(p) H_free(p)
#ifndef CP_HEADER
#define CP_HEADER "../../viewer/cellpool.h"
#endif
#include CP_HEADER
#undef memalign
#undef free

static uint32_t rs = 1; static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static double urand(void) { return (rnd() & 0xFFFFFF) / 16777216.0; }
static unsigned cellSize(void) { double u = urand() + 1e-9, z = sqrt(-2 * log(u)) * cos(6.2831853 * urand()); double kb = 12.5 * exp(0.72 * z); if (kb < 1) kb = 1; if (kb > 235) kb = 235; return (unsigned)(kb * 1024); }
typedef struct { unsigned char *p; unsigned sz; unsigned char tag; int cell; } Blk;
static void fillPat(Blk *b) { for (unsigned i = 0; i < b->sz; i += 61) b->p[i] = (unsigned char)(b->tag + i); }
static int chkPat(const Blk *b) { for (unsigned i = 0; i < b->sz; i += 61) if (b->p[i] != (unsigned char)(b->tag + i)) return 0; return 1; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)

typedef struct { Blk cell; Blk tail[7]; int nt; } Cell;
static Cell *mk(int cellId) {
  Cell *c = calloc(1, sizeof *c); c->cell.sz = cellSize(); c->cell.p = cpAllocCell(c->cell.sz); c->cell.tag = (unsigned char)(rnd()); c->cell.cell = cellId;
  if (!c->cell.p) { free(c); return 0; }
  fillPat(&c->cell); c->nt = 7;
  for (int i = 0; i < 7; i++) { c->tail[i].sz = 200 + rnd() % 6000; c->tail[i].p = cpAlloc(c->tail[i].sz); c->tail[i].tag = (unsigned char)rnd(); if (c->tail[i].p) fillPat(&c->tail[i]); }
  return c;
}
static void rm(Cell *c) {
  CHECK(chkPat(&c->cell), "узор клетки испорчен (размер %u)", c->cell.sz); cpFree(c->cell.p);
  for (int i = 0; i < c->nt; i++) if (c->tail[i].p) { CHECK(chkPat(&c->tail[i]), "узор хвоста испорчен"); cpFree(c->tail[i].p); }
  free(c);
}
static void reset(void) { memset(cpCh, 0, sizeof cpCh); cpN = 0; memset(cpSl, 0, sizeof cpSl); cpSlN = 0; cpAllocs = cpDirect = cpChunksMade = cpChunksFreed = cpChunkFail = cpLiveBytes = 0;
  cpCellAllocs = cpCellDirect = cpSlabsMade = cpSlabsFreed = cpSlabFail = cpCellsLive = cpCellReq = cpSlabHeld = cpHeapFreed = cpCellPeakHeld = 0; }

// часть 1: верность
static void part1(const char *tbl) {
  reset(); cpOld = 0; cpSetTable(tbl[0]); Cell *live[400]; int n = 0; unsigned long made = 0;
  for (int step = 0; step < 6000; step++) {
    if (n < 400 && (n < 20 || rnd() % 100 < 52)) { Cell *c = mk(step); CHECK(c, "нет памяти на клетку"); if (c) { live[n++] = c; made++; } }
    else if (n) { int k = rnd() % n; rm(live[k]); live[k] = live[--n]; }
    if (step % 500 == 0) { unsigned lc = 0; for (int i = 0; i < n; i++) if (live[i]->cell.sz <= cpClsKB[cpNCls - 1] * 1024u) lc++; CHECK(cpCellsLive >= lc - 0 && cpCellsLive <= n, "cpCellsLive %u, живых клеток в классах %u из %d", cpCellsLive, lc, n); }
  }
  for (int i = 0; i < n; i++) { rm(live[i]); }
  unsigned ft, mb; H_stat(&ft, &mb);
  CHECK(cpCellsLive == 0 && cpSlabHeld == 0 && cpLiveBytes == 0, "после выгрузки всего: клеток %u, плит %u КБ, хвостов %u Б", cpCellsLive, cpSlabHeld >> 10, cpLiveBytes);
  CHECK(ft == ARENA && mb == ARENA, "куча не вернулась целиком: свободно %u из %u, кусок %u", ft, ARENA, mb);
  if (!fails) printf("ok: верность, таблица %s: %lu клеток, плит создано %u и возвращено %u, куски хвостов %u/%u, куча вернулась целиком\n", tbl, made, cpSlabsMade, cpSlabsFreed, cpChunksMade, cpChunksFreed);
}
// часть 2: дробление. mode: 0 old, 1 a, 2 b
static void part2(int mode, unsigned seed, unsigned *outFree, unsigned *outBlk, unsigned *outHeld) {
  reset(); rs = seed; cpOld = mode == 0; if (mode) cpSetTable(mode == 2 ? 'b' : 'a');
  enum { NL = 220, NP = 40 }; Cell *live[NL]; int n = 0; struct { unsigned char *p; int dies; } pin[NP]; memset(pin, 0, sizeof pin); unsigned pinned = 0;
  // стартовая куча занята предметами уровня (как после загрузки): 40 МБ арены → оставить ~22 МБ для сцены
  void *base[8]; for (int i = 0; i < 8; i++) base[i] = H_alloc(((i + 1) * 1000u + 500) << 10 >> 2, 64);
  for (int step = 0; step < 9000; step++) {
    if (n < NL) { Cell *c = mk(step); if (c) live[n++] = c; }
    else { int k = rnd() % n; if (urand() < .7) k = step % n; rm(live[k]); Cell *c = mk(step); if (c) live[k] = c; else { live[k] = live[--n]; } }   // пришла клетка — ушла клетка
    if (step % 60 == 0) {   // предмет сцены: art миссии, NPC, комната
      int s = rnd() % NP; if (pin[s].p && pin[s].dies <= step) { H_free(pin[s].p); pin[s].p = 0; pinned--; }
      if (!pin[s].p) { unsigned kb = 120 + rnd() % 2900; pin[s].p = H_alloc((size_t)kb << 10, 16); if (pin[s].p) { pin[s].dies = step + 300 + rnd() % 3000; pinned++; } }
    }
  }
  unsigned ft, mb; H_stat(&ft, &mb); *outFree = ft; *outBlk = mb; *outHeld = cpSlabHeld + 0;
  for (int i = 0; i < n; i++) rm(live[i]);
  for (int i = 0; i < NP; i++) if (pin[i].p) H_free(pin[i].p);
  for (int i = 0; i < 8; i++) H_free(base[i]);
}
int main(int argc, char **argv) {
  unsigned seed = argc > 1 ? (unsigned)atoi(argv[1]) : 7; rs = seed;
  part1("a"); part1("b");
  if (argc > 2 && !strcmp(argv[2], "quick")) { printf(fails ? "FAIL\n" : "PASS\n"); return fails ? 1 : 0; }   // быстрый вариант для гейта: без модели дробления
  const char *nm[] = { "old", "a", "b" }; double sf[3] = { 0 }, sb[3] = { 0 }; int S = 8;
  for (unsigned s = 1; s <= (unsigned)S; s++) for (int m = 0; m < 3; m++) { unsigned f, b, h; part2(m, seed * 977u + s, &f, &b, &h); sf[m] += f; sb[m] += b; }
  printf("дробление (модель, %d прогонов, куча 64 МБ, ~220 клеток + предметы сцены 0,1–3 МБ), среднее:\n", S);
  for (int m = 0; m < 3; m++) printf("  %-3s свободно %.1f МБ, наибольший цельный кусок %.1f МБ\n", nm[m], sf[m] / S / 1048576., sb[m] / S / 1048576.);
  printf(fails ? "FAIL\n" : "PASS\n"); return fails ? 1 : 0;
}
