// П5 Зоны-95: проверка viewer/zone3.h на Маке (без PSP). cc -O1 -DZ3_HOST -o host tests/zone3/host.c -lm && ./host [seed] [old]
// Часть 1, верность: случайные выдачи и возвраты в обеих областях, узоры в данных, отметка/сброс до отметки, стирание, цепочка блоков цела, ничего не перекрывается.
// Часть 2, дробление: учебная куча 24 МБ (первый подходящий, видно наибольший кусок); долгоживущие данные уровня, поток клеток «пришли и ушли» вперемешку с блоками миссий,
//   входы («комната 0,8 МБ + пакет будки 2 МБ», «ролик 2,5 МБ»). «old» — клиенты берут прямо из кучи (ZONE3_OLD): отказы входов > 0; новое — области, отказов и проливов 0.
// Строки «ok:» / «FAIL:», последняя PASS или FAIL (для old — ожидается FAIL: красный контроль).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stddef.h>
struct _reent;
static unsigned ARENA = 22u << 20;   // куча как у PSP в тесной полосе L4: после данных уровня (80% от 14,5 МБ) и клеток остаётся 3–4 МБ, наибольший кусок < 1 МБ
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
#ifndef Z3_HEADER
#define Z3_HEADER "../../viewer/zone3.h"
#endif
static char logBuf[4096]; static int logN;
#define Z3_LOGF(s) (logN++, snprintf(logBuf, sizeof logBuf, "%s", (s)))
#include Z3_HEADER
#undef memalign
#undef free

static uint32_t rs = 1; static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static double urand(void) { return (rnd() & 0xFFFFFF) / 16777216.0; }
static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } } while (0)
typedef struct { unsigned char *p; unsigned sz; unsigned char tag; int area; unsigned seq; } Blk;
static void fillPat(Blk *b) { for (unsigned i = 0; i + 1 < b->sz; i += 61) b->p[i] = (unsigned char)(b->tag + i); if (b->sz) b->p[b->sz - 1] = b->tag; }
static int chkPat(const Blk *b) { for (unsigned i = 0; i + 1 < b->sz; i += 61) if (b->p[i] != (unsigned char)(b->tag + i)) return 0; return !b->sz || b->p[b->sz - 1] == b->tag; }

// ---- часть 1 ----
static void part1(void) {
  z3Scribble = 1;
  CHECK(zone3Init(4u << 20, 3u << 20, 0, 0xFFFFFFFFu), "области не взялись");
  enum { N = 400 }; static Blk bl[N]; memset(bl, 0, sizeof bl);
  for (int it = 0; it < 60000; it++) {
    int i = rnd() % N; Blk *b = &bl[i];
    if (b->p) { CHECK(chkPat(b), "узор испорчен блок %d область %d", i, b->area); zone3Free(b->p); b->p = 0; }
    else { b->area = rnd() & 1; b->sz = 64 + rnd() % (b->area ? 300000 : 160000); b->tag = (unsigned char)rnd();
      b->p = zone3Alloc(b->area, "t", b->sz, 16); if (b->p) fillPat(b); }
    if (it % 5000 == 0) { for (int a = 0; a < 2; a++) CHECK(!z3AreaVerify(&z3A[a]), "цепочка области %d испорчена", a); }
  }
  for (int i = 0; i < N; i++) if (bl[i].p) { CHECK(chkPat(&bl[i]), "узор испорчен (финал)"); zone3Free(bl[i].p); bl[i].p = 0; }
  for (int a = 0; a < 2; a++) { unsigned mf, ft, nl; z3AreaStat(&z3A[a], &mf, &ft, &nl);
    CHECK(nl == 0 && z3A[a].usedB == 0 && !z3AreaVerify(&z3A[a]), "область %d не пуста после возврата всего: live=%u used=%u", a, nl, z3A[a].usedB);
    CHECK(mf + Z3_HDR == z3A[a].cap, "область %d не склеилась в один блок: %u из %u", a, mf + Z3_HDR, z3A[a].cap); }
  // отметка/сброс до отметки: старые блоки живы, новые сняты, повторное освобождение снятого безопасно
  void *keep = zone3Alloc(Z3_PLACE, "keep", 100000, 64); memset(keep, 0x5A, 100000);
  unsigned m = zone3Mark(Z3_PLACE);
  void *x1 = zone3Alloc(Z3_PLACE, "room", 800000, 16), *x2 = zone3Alloc(Z3_PLACE, "pack", 1000000, 16); memset(x1, 1, 800000); memset(x2, 2, 1000000);
  int n = zone3Release(Z3_PLACE, m);
  CHECK(n == 2, "сброс до отметки снял %d из 2", n);
  CHECK(((unsigned char *)keep)[99999] == 0x5A && ((unsigned char *)keep)[0] == 0x5A, "старый блок задет сбросом");
  CHECK(((unsigned char *)x1)[1000] == 0xFF, "служебная заливка 0xFF не видна (x1[1000]=%02X)", ((unsigned char *)x1)[1000]);
  zone3Free(x1); zone3Free(x2);   // после сброса — безвредно
  CHECK(!z3AreaVerify(&z3A[Z3_PLACE]), "цепочка после повторного возврата");
  zone3Free(keep);
  // чужой указатель идёт в free()
  void *h = H_alloc(1000, 16); zone3Free(h);
  // стирание области: утечки считаются
  void *l1 = zone3Alloc(Z3_MISSION, "leak", 5000, 16); (void)l1; void *l2 = zone3Alloc(Z3_MISSION, "leak", 6000, 16); (void)l2;
  CHECK(z3AreaReset(&z3A[Z3_MISSION]) == 2, "стирание: утечек не 2");
  CHECK(z3A[Z3_MISSION].usedB == 0 && !z3AreaVerify(&z3A[Z3_MISSION]), "стирание не вернуло область в один блок");
  // выравнивание данных
  for (int i = 0; i < 50; i++) { void *q = zone3Alloc(i & 1, "al", 1 + rnd() % 5000, 64); CHECK(((uintptr_t)q & 63u) == 0, "выравнивание 64 нарушено"); zone3Free(q); }
  printf("ok: часть 1 (верность) fails=%d\n", fails);
}

// ---- часть 2 ----
typedef struct { void *p; unsigned sz; unsigned life; } Cell;
static unsigned cellSize(void) { double u = urand() + 1e-9, z = sqrt(-2 * log(u)) * cos(6.2831853 * urand()); double kb = 25 * exp(1.0 * z); if (kb < 1) kb = 1; if (kb > 235) kb = 235; return (unsigned)(kb * 1024); }
static int entryFail, entryTry; static unsigned minRoomBlk = ~0u;
static void *heapAlloc(unsigned sz) { return H_alloc(sz, 64); }
static void part2(int old) {
  // старт: куча чистая. Новое — области берутся первыми (как zone0Init); старое — их нет.
  if (!old) { rs = rs; CHECK(zone3Init(4u << 20, 3u << 20, 0, 0xFFFFFFFFu), "области не взялись"); }
  // данные уровня: 10 блоков (резидент, COL1, PRP, PVS …) ≈ 14,5 МБ, лежат всё время
  void *lev[10]; unsigned lsz[10] = { 7600000, 3800000, 1100000, 560000, 380000, 256000, 256000, 256000, 140000, 100000 }; unsigned lscale = getenv("LEVPCT") ? (unsigned)atoi(getenv("LEVPCT")) : 80;
  for (int i = 0; i < 10; i++) { lev[i] = heapAlloc((unsigned)((unsigned long long)lsz[i] * lscale / 100)); CHECK(lev[i] != 0, "данные уровня %d не легли", i); }
  Cell cells[130]; memset(cells, 0, sizeof cells);
  Blk ms[16]; memset(ms, 0, sizeof ms);   // блоки миссий
  unsigned t = 0; unsigned okEntries = 0;
  for (int step = 0; step < 60000; step++, t++) {
    // клетки: каждую итерацию одна пара «ушла/пришла»
    int ci = rnd() % 130; if (cells[ci].p) H_free(cells[ci].p); cells[ci].sz = cellSize(); cells[ci].p = heapAlloc(cells[ci].sz);
    if (!cells[ci].p) { // стрим: нет места — выкинуть самую дальнюю (случайную) и повторить, как CELLRETRY; тут просто пропуск
      cells[ci].sz = 0; }
    // блоки миссии: раз в 400 шагов ротация (модели 0,2–0,5 МБ, картинки 0,15 МБ), в старом — из кучи
    if (step % 400 == 0) { if (!old) { for (int i = 0; i < 16; i++) ms[i].p = 0; int lk = z3AreaReset(&z3A[Z3_MISSION]); CHECK(lk <= 16, "утечек при стирании миссии %d", lk); }
      for (int i = 0; i < 16; i++) { if (ms[i].p) { H_free(ms[i].p); ms[i].p = 0; }
        if (urand() < 0.8) { ms[i].sz = (i < 6 ? 200000u + rnd() % 250000u : 100000u + rnd() % 60000u); ms[i].p = old ? heapAlloc(ms[i].sz) : zone3Alloc(Z3_MISSION, "msn", ms[i].sz, 64); } } }
    // вход: каждые 9000 шагов — комната + пакет будки (в 2 приёма по 1 МБ), на 4-м — ролик 2,5 МБ
    if (step > 4000 && step % 9000 == 4001) {
      int kind = (step / 9000) % 4 == 3; entryTry++;
      unsigned mark = old ? 0 : zone3Mark(Z3_PLACE);
      void *a[3] = {0, 0, 0}; unsigned sz[3]; int n;
      if (kind) { sz[0] = 2500000; n = 1; } else { sz[0] = 794816; sz[1] = 1003648; sz[2] = 1003648; n = 3; }
      int ok = 1;
      for (int i = 0; i < n; i++) { a[i] = old ? heapAlloc(sz[i]) : zone3Alloc(Z3_PLACE, "entry", sz[i], 64); if (!a[i]) { ok = 0; break; } }
      if (!ok) entryFail++; else okEntries++;
      unsigned ft, mx; H_stat(&ft, &mx); if (mx < minRoomBlk) minRoomBlk = mx;
      for (int i = 0; i < n; i++) if (a[i]) { if (old) H_free(a[i]); else zone3Free(a[i]); }
      if (!old) zone3Release(Z3_PLACE, mark);
    }
  }
  unsigned ft, mx; H_stat(&ft, &mx);
  char s[400]; zone3StatLine(s, sizeof s, (float)t, "end"); if (!old) printf("%s", s);
  printf("%s: входов %d, отказов входа %d, проливов %u, наибольший кусок кучи в момент входа min=%.2f МБ, свободно кучи %.2f МБ в конце\n", old ? "old" : "new", entryTry, entryFail, z3SpillTotal, minRoomBlk / 1048576.0, ft / 1048576.0);
  if (!old) { CHECK(!z3AreaVerify(&z3A[0]) && !z3AreaVerify(&z3A[1]), "цепочка областей"); }
  CHECK(entryFail == 0, "отказов входа: %d из %d", entryFail, entryTry);
  CHECK(z3SpillTotal == 0, "проливов в кучу: %u", z3SpillTotal);
}
int main(int argc, char **argv) {
  if (getenv("ARENAMB")) ARENA = (unsigned)atoi(getenv("ARENAMB")) << 20;
  rs = argc > 1 ? (uint32_t)atoi(argv[1]) : 1; if (!rs) rs = 1;
  const char *mode = argc > 2 ? argv[2] : "new";   // p1 — верность; new — дробление с областями; old — дробление без них (красный контроль)
  if (!strcmp(mode, "p1")) part1(); else part2(!strcmp(mode, "old"));
  printf(fails ? "FAIL\n" : "PASS\n"); return fails ? 1 : 0;
}
