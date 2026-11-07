// Правка Роба 103 (30.09, вылет у школы): учёт памяти для чёрного ящика, счётчика R+SELECT и строк STAT смоука.
// Сборка (viewer/CMakeLists.txt): -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=memalign — каждый отказ выделения
// в любом модуле движка считается здесь (раньше видны были только отказы клеток города, smkAllocFail в main.c)
#include <stdlib.h>
#include <malloc.h>
#include <reent.h>
#ifndef MEMMAP_OLD
#define MEMMAP_OLD 0   // 1 — без учёта живых выделений (viewer/memmap.h), обёртки только считают отказы, как в TEST89; файл memmapold рядом с EBOOT — то же без пересборки
#endif
#if !MEMMAP_OLD && !LEAK696_TRACE
#include <pspintrman.h>
#include "memmap.h"
volatile int memmapOn = 1;
void memmapSetOn(int on) { memmapOn = on; }
// интерфейс считается из потоков загрузчика, звука и главного: короткая секция без прерываний (вставка/удаление — десяток команд)
#define MMA(p) do { if (memmapOn) { size_t us_ = malloc_usable_size((p)); unsigned s_ = sceKernelCpuSuspendIntr(); mmAdd((p), us_, (uint32_t)__builtin_return_address(0)); sceKernelCpuResumeIntr(s_); } } while (0)
#define MMD(p) do { if (memmapOn) { size_t us_ = malloc_usable_size((p)); unsigned s_ = sceKernelCpuSuspendIntr(); mmDel((p), us_); sceKernelCpuResumeIntr(s_); } } while (0)
#define MMDS(p, sz) do { if (memmapOn) { unsigned s_ = sceKernelCpuSuspendIntr(); mmDel((p), (sz)); sceKernelCpuResumeIntr(s_); } } while (0)
#define memmapOnRt() memmapOn
unsigned memmapReport(char *buf, unsigned cap, float t, unsigned freeKB, unsigned blkKB, unsigned costUs, unsigned maxStepUs) { return mmFormat(buf, cap, t, freeKB, blkKB, costUs, maxStepUs, (uint32_t)(uintptr_t)&memmapReport); }
void memmapScan(void) { unsigned s_ = sceKernelCpuSuspendIntr(); mmScan(); sceKernelCpuResumeIntr(s_); }
#else
#define MMA(p) do { } while (0)
#define MMD(p) do { } while (0)
#define MMDS(p, sz) do { } while (0)
#define memmapOnRt() 0
void memmapSetOn(int on) { (void)on; }
unsigned memmapReport(char *buf, unsigned cap, float t, unsigned freeKB, unsigned blkKB, unsigned costUs, unsigned maxStepUs) { (void)buf; (void)cap; (void)t; (void)freeKB; (void)blkKB; (void)costUs; (void)maxStepUs; return 0; }
void memmapScan(void) {}
#endif

void *__real_malloc(size_t n);
void __real_free(void *p);
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
void __wrap_free(void *p) { if (p) memset(p, 0xFF, malloc_usable_size(p)); MMD(p); __real_free(p); }
#define SCRIB(p, n) do { if (p) memset((p), 0xFF, (n)); } while (0)
#else
#define SCRIB(p, n) do { } while (0)
#endif
#if LEAK696_TRACE
// №696 (служебная сборка -DLEAK696_TRACE=ON): учёт живых выделений по адресу вызывающего (ra); lt696Dump пишет, чьи байты выросли с прошлого дампа
#include <stdio.h>
#include <unistd.h>
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
// ТЕСТ90 оптимизация памяти (диагностика, только LEAK696_TRACE): карта кучи по адресам — просветы >= 128 КБ и что стоит между ними («острова»); показывает, кто дробит кучу после выгрузки уровня
typedef struct { unsigned p, n, ra; } LtS;
static LtS ltS[70000];
static int ltCmp(const void *a, const void *b) { unsigned x = ((const LtS *)a)->p, y = ((const LtS *)b)->p; return x < y ? -1 : x > y; }
void lt696Map(void (*w)(const char *), const char *tag, unsigned minGapKB) {
  unsigned n = 0, tot = 0; for (unsigned i = 0; i < LT_N; i++) if (lt[i].p && lt[i].p != (void *)1) { tot++; if (n < 70000) { ltS[n].p = (unsigned)lt[i].p; ltS[n].n = (unsigned)malloc_usable_size(lt[i].p); ltS[n].ra = lt[i].ra; n++; } }
  qsort(ltS, n, sizeof ltS[0], ltCmp);
  char s[200]; unsigned base = (unsigned)&lt696Map; extern unsigned memMaxBlock(void);
  snprintf(s, sizeof s, "HMAP %s begin live=%u kept=%u modbase_fn=%08x sbrk=%08x maxblk=%u\n", tag, tot, n, base, (unsigned)sbrk(0), memMaxBlock()); w(s);
  unsigned gapMin = minGapKB << 10, runN = 0, runB = 0, firstP = 0, nGaps = 0, gapTot = 0, gapMax = 0;
  for (unsigned i = 0; i + 1 < n; i++) {
    unsigned endi = ltS[i].p + ltS[i].n + 8, gap = ltS[i + 1].p > endi ? ltS[i + 1].p - endi : 0;
    if (!runN) firstP = i; runN++; runB += ltS[i].n;
    if (gap >= gapMin) {
      nGaps++; gapTot += gap; if (gap > gapMax) gapMax = gap;
      snprintf(s, sizeof s, "HMAP %s gap %u KB at %08x | island blocks=%u bytes=%u:", tag, gap >> 10, endi, runN, runB); w(s);
      unsigned show = runN <= 6 ? runN : 3;
      for (unsigned k = 0; k < show; k++) { LtS *b = &ltS[firstP + k]; snprintf(s, sizeof s, " [%08x %uB ra=%08x]", b->p, b->n, b->ra); w(s); }
      if (runN > 6) { for (unsigned k = runN - 2; k < runN; k++) { LtS *b = &ltS[firstP + k]; snprintf(s, sizeof s, " ..[%08x %uB ra=%08x]", b->p, b->n, b->ra); w(s); } }
      w("\n"); runN = 0; runB = 0;
    }
  }
  snprintf(s, sizeof s, "HMAP %s end gaps>=%uKB: n=%u total=%uKB max=%uKB\n", tag, minGapKB, nGaps, gapTot >> 10, gapMax >> 10); w(s);
  // группы по вызывающему отдельно для мелких (< 8 КБ), средних (8–256 КБ) и крупных блоков: число и байты (кто сидит между просветами)
  static struct { unsigned ra, cnt, bytes; } g[3][48]; unsigned ng[3] = {0, 0, 0};
  for (unsigned i = 0; i < n; i++) { int c = ltS[i].n < 8192 ? 0 : ltS[i].n < 262144 ? 1 : 2; unsigned k = 0; while (k < ng[c] && g[c][k].ra != ltS[i].ra) k++;
    if (k == ng[c]) { if (ng[c] >= 48) continue; g[c][k].ra = ltS[i].ra; g[c][k].cnt = 0; g[c][k].bytes = 0; ng[c]++; }
    g[c][k].cnt++; g[c][k].bytes += ltS[i].n; }
  for (int c = 0; c < 3; c++) { for (unsigned a = 0; a < ng[c]; a++) for (unsigned b = a + 1; b < ng[c]; b++) if ((c == 0 ? g[c][b].cnt > g[c][a].cnt : g[c][b].bytes > g[c][a].bytes)) { unsigned t0 = g[c][a].ra, t1 = g[c][a].cnt, t2 = g[c][a].bytes; g[c][a] = g[c][b]; g[c][b].ra = t0; g[c][b].cnt = t1; g[c][b].bytes = t2; }
    snprintf(s, sizeof s, "HMAPG %s %s:", tag, c == 0 ? "small<8K" : c == 1 ? "mid8-256K" : "big>=256K"); w(s);
    for (unsigned a = 0; a < ng[c] && a < 14; a++) { snprintf(s, sizeof s, " %08x:%u/%uB", g[c][a].ra, g[c][a].cnt, g[c][a].bytes); w(s); }
    w("\n"); }
}
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
void *__wrap_malloc(size_t n) { void *p = __real_malloc(n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); SCRIB(p, n); if (p) MMA(p); return p; }
void *__wrap_calloc(size_t a, size_t b) { void *p = __real_calloc(a, b); if (!p && a && b) miss(a * b, (unsigned)__builtin_return_address(0)); if (p) MMA(p); return p; }
void *__wrap_realloc(void *q, size_t n) { size_t qs_ = (memmapOnRt() && q) ? malloc_usable_size(q) : 0; void *p = __real_realloc(q, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); if (q && (p || !n)) MMDS(q, qs_); if (p) MMA(p); return p; }
void *__wrap_memalign(size_t al, size_t n) { void *p = __real_memalign(al, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); SCRIB(p, n); if (p) MMA(p); return p; }
#if !MEM_SCRIBBLE
void __real_free(void *p);
void __wrap_free(void *p) { if (p) MMD(p); __real_free(p); }
#endif
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
    if (p) { __real_free(p); lo = m; } else hi = m;
  }
  __malloc_unlock(_REENT);
  return lo;
}
// TEST90 MEMMAP: та же проба по шагам (по iters делений за вызов), чтобы замер не ложился одним кадром; 1 — готово (lo — наибольший кусок с точностью 16 КБ). Проба идёт мимо учёта (__real_malloc/__real_free)
int memMaxBlockStep(unsigned *lo, unsigned *hi, int iters) {
  __malloc_lock(_REENT);
  for (int i = 0; i < iters && *hi - *lo > (16u << 10); i++) { unsigned m = *lo + (*hi - *lo) / 2; void *p = __real_malloc(m); if (p) { __real_free(p); *lo = m; } else *hi = m; }
  __malloc_unlock(_REENT);
  return *hi - *lo <= (16u << 10);
}
