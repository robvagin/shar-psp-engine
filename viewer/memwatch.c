// Правка Роба 103 (30.09, вылет у школы): учёт памяти для чёрного ящика, счётчика R+SELECT и строк STAT смоука.
// Сборка (viewer/CMakeLists.txt): -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=memalign — каждый отказ выделения
// в любом модуле движка считается здесь (раньше видны были только отказы клеток города, smkAllocFail в main.c)
#include <stdlib.h>
#include <malloc.h>
#include <reent.h>

void *__real_malloc(size_t n);
void *__real_calloc(size_t a, size_t b);
void *__real_realloc(void *p, size_t n);
void *__real_memalign(size_t al, size_t n);

volatile unsigned memFails, memFailLast, memFailBig;   // отказы: сколько всего, размер последнего и самого крупного, байт
// №1065: размер и адрес вызывающего каждого отказа (первые 16) — строка MEMFAILSZ в смоуке (main_frontend.h); адрес по ELF: tools/crash_addr.py
#define MF_RING 16u
static struct { unsigned bytes, ra; } memFailRing[MF_RING];
static void miss(size_t n, unsigned ra) { if (memFails < MF_RING) { memFailRing[memFails].bytes = (unsigned)n; memFailRing[memFails].ra = ra; } memFails++; memFailLast = (unsigned)n; if (n > memFailBig) memFailBig = (unsigned)n; }
int memFailRingGet(unsigned i, unsigned *bytes, unsigned *ra) { if (i >= memFails || i >= MF_RING) return 0; *bytes = memFailRing[i].bytes; *ra = memFailRing[i].ra; return 1; }

// Правка 252 (03.10, зависание всей PSP у особняка, эмулятор не повторяет): служебная сборка -DMEM_SCRIBBLE=1 (и
// -Wl,--wrap=free) заливает каждое новое и освобождённое выделение байтом 0xFF — как «грязная» память настоящей PSP (эмулятор
// отдаёт кучу нулями). 0xFFFFFFFF — это NaN для float и -1/плохой адрес для целых и указателей: чтение неинициализированного
// или освобождённого сразу видно эмулятору-проверяльщику (tools/twins/ppsspp_hwcheck_build.sh: NaN, плохой адрес).
// В выпуске не собирается
#if MEM_SCRIBBLE
#include <string.h>
void __real_free(void *p);
void __wrap_free(void *p) { if (p) memset(p, 0xFF, malloc_usable_size(p)); __real_free(p); }
#define SCRIB(p, n) do { if (p) memset((p), 0xFF, (n)); } while (0)
#else
#define SCRIB(p, n) do { } while (0)
#endif
#if LEAK696_TRACE
// №696 (служебная сборка -DLEAK696_TRACE=ON): учёт живых выделений по адресу вызывающего (ra); lt696Dump пишет, чьи байты выросли с прошлого дампа
#include <stdio.h>
void __real_free(void *p);
#define LT_N 131072u
static struct { void *p; unsigned n, ra; } lt[LT_N];
static void ltAdd(void *p, size_t n, unsigned ra) { if (!p) return; unsigned h = ((unsigned)p >> 4) * 2654435761u >> 15; for (unsigned k = 0; k < LT_N; k++) { unsigned i = (h + k) & (LT_N - 1); if (!lt[i].p || lt[i].p == (void *)1) { lt[i].p = p; lt[i].n = (unsigned)n; lt[i].ra = ra; return; } } }
static void ltDel(void *p) { if (!p) return; unsigned h = ((unsigned)p >> 4) * 2654435761u >> 15; for (unsigned k = 0; k < LT_N; k++) { unsigned i = (h + k) & (LT_N - 1); if (lt[i].p == p) { lt[i].p = (void *)1; return; } if (!lt[i].p) return; } }
void __wrap_free(void *p) { ltDel(p); __real_free(p); }
void *__wrap_malloc(size_t n) { void *p = __real_malloc(n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); ltAdd(p, n, (unsigned)__builtin_return_address(0)); return p; }
void *__wrap_calloc(size_t a, size_t b) { void *p = __real_calloc(a, b); if (!p && a && b) miss(a * b, (unsigned)__builtin_return_address(0)); ltAdd(p, a * b, (unsigned)__builtin_return_address(0)); return p; }
void *__wrap_realloc(void *q, size_t n) { ltDel(q); void *p = __real_realloc(q, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); ltAdd(p, n, (unsigned)__builtin_return_address(0)); return p; }
void *__wrap_memalign(size_t al, size_t n) { void *p = __real_memalign(al, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); ltAdd(p, n, (unsigned)__builtin_return_address(0)); return p; }
#define LT_R 4096u
static int getenv_lt = 1;
static struct { unsigned ra, bytes, cnt; } ltPrev[LT_R], ltCur[LT_R];
int lt696Pending(void *p);
void lt696Dump(void (*w)(const char *), const char *tag) {
  unsigned nr = 0; for (unsigned i = 0; i < LT_N; i++) if (lt[i].p && lt[i].p != (void *)1) {
    unsigned k = 0; while (k < nr && ltCur[k].ra != lt[i].ra) k++;
    if (k == nr && nr < LT_R) { ltCur[nr].ra = lt[i].ra; ltCur[nr].bytes = 0; ltCur[nr].cnt = 0; nr++; }
    if (k < nr) { ltCur[k].bytes += lt[i].n; ltCur[k].cnt++; } }
  static unsigned np; char s[120];
  for (unsigned k = 0; k < nr; k++) { unsigned pb = 0, pc = 0; for (unsigned j = 0; j < np; j++) if (ltPrev[j].ra == ltCur[k].ra) { pb = ltPrev[j].bytes; pc = ltPrev[j].cnt; break; }
#ifdef LT_ABS
    pb = 0; pc = 0;   // №877: служебно — все группы целиком (не только прирост)
#endif
    if (ltCur[k].bytes > pb + 4096u) { snprintf(s, sizeof s, "LT696 %s ra=%08x bytes=%u (+%u) cnt=%u (+%u)\n", tag, ltCur[k].ra, ltCur[k].bytes, ltCur[k].bytes - pb, ltCur[k].cnt, ltCur[k].cnt - pc); w(s); } }
  { unsigned bk = 0; for (unsigned k = 0; k < nr; k++) if (ltCur[k].bytes > ltCur[bk].bytes + 0) { /* крупнейшая группа */ if (ltCur[k].ra >= 0x08900000u && ltCur[k].ra < 0x08920000u) bk = k; }
    for (unsigned i = 0; i < LT_N; i++) if (lt[i].p && lt[i].p != (void *)1 && lt[i].ra == ltCur[bk].ra) { snprintf(s, sizeof s, "LT696p %s %p %u q=%d\n", tag, lt[i].p, lt[i].n, lt696Pending(lt[i].p)); w(s); } }
  for (unsigned k = 0; k < nr; k++) ltPrev[k] = ltCur[k]; np = nr;
}
#else
void *__wrap_malloc(size_t n) { void *p = __real_malloc(n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); SCRIB(p, n); return p; }
void *__wrap_calloc(size_t a, size_t b) { void *p = __real_calloc(a, b); if (!p && a && b) miss(a * b, (unsigned)__builtin_return_address(0)); return p; }
void *__wrap_realloc(void *q, size_t n) { void *p = __real_realloc(q, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); return p; }
void *__wrap_memalign(size_t al, size_t n) { void *p = __real_memalign(al, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); SCRIB(p, n); return p; }
#endif

// Наибольший кусок, который куча отдаст одним выделением, байт (с точностью 16 КБ): проба __real_malloc — в счёт отказов не
// идёт. Под замком malloc (рекурсивный), чтобы загрузчик клеток не вклинился между пробой и возвратом
extern void __malloc_lock(struct _reent *);
extern void __malloc_unlock(struct _reent *);
unsigned memMaxBlock(void) {
  __malloc_lock(_REENT);
  unsigned lo = 0, hi = 64u << 20;
  while (hi - lo > (16u << 10)) {
    unsigned m = lo + (hi - lo) / 2; void *p = __real_malloc(m);
    if (p) { free(p); lo = m; } else hi = m;
  }
  __malloc_unlock(_REENT);
  return lo;
}
