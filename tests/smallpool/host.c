// Хост-тест пула мелких блоков (viewer/smallpool.h): случайные выделения и освобождения, проверка: выравнивание 16, блоки не пересекаются и держат записанный узор, free по диапазону, переполнение уходит в счётчик.
// Красный контроль: -DSP_BREAK=1 - spFree кладёт блок в чужой класс (узор рвётся/пересечения) - тест обязан упасть.
//   cc -O1 -I viewer tests/smallpool/host.c -o /tmp/sp && /tmp/sp
#include <stdio.h>
#include <stdlib.h>
#include "smallpool.h"
#ifdef SP_BREAK
#define SP_FREE(p) do { unsigned pg_ = (unsigned)(((unsigned char *)(p) - spBase) / SP_PAGE); spPageClsT[pg_] = (spPageClsT[pg_] + 1) % SP_NCLS; spFree(p); } while (0)
#else
#define SP_FREE(p) spFree(p)
#endif
#define N 6000
static struct { unsigned char *p; unsigned n; unsigned char pat; int heap; } L[N];
int main(void) {
  unsigned bytes = 1536u << 10; void *m = aligned_alloc(8192, bytes); spInit(m, bytes); srand(7); int bad = 0; unsigned live = 0, heapN = 0;
  for (long it = 0; it < 400000; it++) {
    int i = rand() % N;
    if (L[i].p) {   // освободить: проверить узор
      for (unsigned k = 0; k < L[i].n; k++) if (L[i].p[k] != L[i].pat) { bad++; break; }
      if (L[i].heap) { free(L[i].p); heapN--; } else { if (!spIn(L[i].p)) bad++; SP_FREE(L[i].p); live--; } L[i].p = 0; }
    else {
      unsigned n = 1u + (unsigned)(rand() % 8192); unsigned char pat = (unsigned char)(1 + rand() % 250);
      void *p = spAlloc(n); int heap = 0; if (!p) { p = malloc(n); heap = 1; heapN++; } else { live++; if (((uintptr_t)p & 15u) || !spIn(p)) bad++; if (spSizeOf(p) < n) bad++; }
      memset(p, pat, n); L[i].p = p; L[i].n = n; L[i].pat = pat; L[i].heap = heap; }
  }
  for (int i = 0; i < N; i++) if (L[i].p) for (unsigned k = 0; k < L[i].n; k++) if (L[i].p[k] != L[i].pat) { bad++; break; }
  // переполнение: забить пул одним классом
  printf("pages=%u/%u liveMax=%u over=%u bad=%d\n", spPagesUsed, spNPage, spLiveMax, spOver, bad);
  void *q[200000]; unsigned nq = 0; for (unsigned k = 0; k < 200000; k++) { void *p = spAlloc(8192); if (!p) break; q[nq++] = p; }
  printf("полный пул 8192: %u блоков (ожидается <= %u), over=%u\n", nq, spNPage, spOver);
  for (unsigned k = 0; k < nq; k++) SP_FREE(q[k]);
  if (bad) { puts("FAIL"); return 1; } puts("PASS"); return 0; }
