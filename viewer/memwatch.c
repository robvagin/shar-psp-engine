// Правка Роба 103 (30.09, вылет у школы): учёт памяти для чёрного ящика, счётчика R+SELECT и строк STAT смоука.
// Сборка (viewer/CMakeLists.txt): -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=memalign — каждый отказ выделения
// в любом модуле движка считается здесь (раньше видны были только отказы клеток города, smkAllocFail в main.c)
#include <stdlib.h>
#include <malloc.h>
#include <reent.h>
struct _reent;
void *__real__malloc_r(struct _reent *r, size_t n); void *__real__calloc_r(struct _reent *r, size_t a, size_t b); void *__real__realloc_r(struct _reent *r, void *q, size_t n);
void *__real__memalign_r(struct _reent *r, size_t al, size_t n); void __real__free_r(struct _reent *r, void *p);
#ifndef MALLOCR_OLD
#define MALLOCR_OLD 0
#endif
void *__real_malloc(size_t n); void __real_free(void *p);
#if MALLOCR_OLD
#define PROBE_MALLOC(n) __real_malloc(n)
#define PROBE_FREE(p) __real_free(p)
#else
#define PROBE_MALLOC(n) __real__malloc_r(_REENT, (n))   // проба наибольшего куска идёт мимо обёрток (отказы пробы не считаются, в таблицу не попадают)
#define PROBE_FREE(p) __real__free_r(_REENT, (p))
#endif
#ifndef MEMMAP_OLD
#define MEMMAP_OLD 0   // 1 — без учёта живых выделений (viewer/memmap.h), обёртки только считают отказы, как в TEST89; файл memmapold рядом с EBOOT — то же без пересборки
#endif
#if !MEMMAP_OLD && !LEAK696_TRACE
#include <pspintrman.h>
#include <pspthreadman.h>
#include "memmap.h"
volatile int memmapOn = 1;
void memmapSetOn(int on) { memmapOn = on; }
// Опора-93 (запрос Fable-аудита 08.11): живые блоки >= 128 КБ по адресу — какие постоянные блоки стоят после выгрузки уровня и режут кучу. Только для smoke (levelLog)
void *__real_malloc(size_t n); void __real_free(void *p);
// Карта свободной кучи (только smoke): наибольший свободный кусок занимается пробой, адрес и размер пишутся, повтор; в конце всё освобождается. Показывает, где лежат самые крупные куски и что между ними (незаписанное в таблице тоже)
void memmapFreeMap(void (*out)(const char *), const char *tag) {
  void *keep[16]; unsigned sz[16]; unsigned n = 0;
  for (; n < 16; n++) {
    unsigned lo = 0, hi = 64u << 20; while (hi - lo > (16u << 10)) { unsigned m = lo + (hi - lo) / 2; void *p = PROBE_MALLOC(m); if (p) { PROBE_FREE(p); lo = m; } else hi = m; }
    if (lo < (256u << 10)) break; void *q = PROBE_MALLOC(lo); if (!q) break; keep[n] = q; sz[n] = lo; }
  char l[96]; for (unsigned i = 0; i < n; i++) { snprintf(l, sizeof l, "FREEMAP %s a=%08x kb=%u\n", tag, (unsigned)(uintptr_t)keep[i], sz[i] >> 10); out(l); }
  for (unsigned i = 0; i < n; i++) PROBE_FREE(keep[i]);
}
void memmapDump(void (*out)(const char *), const char *tag) {
  static struct { unsigned a, kb, ra; } e[4000]; unsigned n = 0; unsigned s_ = sceKernelCpuSuspendIntr();
  for (unsigned i = 0; i < MM_N && n < 4000; i++) { uint32_t a = mmTab[i].a; if (a == 0 || a == 0xFFFFFFFFu) continue;
    unsigned su = ((a >> 22) & 1023u) | ((mmTab[i].b >> 23) << 10), kb = su * 16u; if (kb < MM_MIN) continue;   /* здесь kb - БАЙТЫ */
    e[n].a = (((a & 0x3FFFFFu) - 1u) << 4) + MM_BASE; e[n].kb = kb; e[n].ra = (((mmTab[i].b & 0x7FFFFFu) << 2) + MM_BASE); n++; }
  sceKernelCpuResumeIntr(s_);
  for (unsigned i = 1; i < n; i++) { unsigned a = e[i].a, k = e[i].kb, r = e[i].ra; unsigned j = i; while (j && e[j - 1].a > a) { e[j] = e[j - 1]; j--; } e[j].a = a; e[j].kb = k; e[j].ra = r; }
  // мелкие блоки (< 2 КБ), по вызывающему: сколько, сколько байт, диапазон адресов - они дробят свободную середину кучи (в таблицу идут при MM_MIN < 2048)
  { struct { unsigned ra, n, b, lo, hi; } g[128]; unsigned ng = 0; unsigned s2_ = sceKernelCpuSuspendIntr();
    for (unsigned i = 0; i < MM_N; i++) { uint32_t a = mmTab[i].a; if (a == 0 || a == 0xFFFFFFFFu) continue;
      unsigned su = ((a >> 22) & 1023u) | ((mmTab[i].b >> 23) << 10), by = su * 16u; if (by >= 2048u) continue;
      unsigned ad = (((a & 0x3FFFFFu) - 1u) << 4) + MM_BASE, ra = (((mmTab[i].b & 0x7FFFFFu) << 2) + MM_BASE); unsigned k = 0; while (k < ng && g[k].ra != ra) k++;
      if (k == ng) { if (ng >= 128) continue; g[ng].ra = ra; g[ng].n = g[ng].b = 0; g[ng].lo = 0xFFFFFFFFu; g[ng].hi = 0; ng++; }
      g[k].n++; g[k].b += by; if (ad < g[k].lo) g[k].lo = ad; if (ad > g[k].hi) g[k].hi = ad; }
    sceKernelCpuResumeIntr(s2_);
    for (unsigned i = 0; i < ng; i++) { char l2[120]; snprintf(l2, sizeof l2, "MEMSMALL %s ra=%08x n=%u b=%u lo=%08x hi=%08x\n", tag, g[i].ra, g[i].n, g[i].b, g[i].lo, g[i].hi); out(l2); } }
  char l[96]; for (unsigned i = 0; i < n; i++) { snprintf(l, sizeof l, "MEMLIVE %s a=%08x b=%u ra=%08x\n", tag, e[i].a, e[i].kb, e[i].ra); out(l); }
}
// интерфейс считается из потоков загрузчика, звука и главного: короткая секция без прерываний (вставка/удаление — десяток команд)
#define MMA(p) do { if (memmapOn) { size_t us_ = malloc_usable_size((p)); unsigned s_ = sceKernelCpuSuspendIntr(); mmAdd((p), us_, (uint32_t)__builtin_return_address(0)); sceKernelCpuResumeIntr(s_); } } while (0)
#define MMD(p) do { if (memmapOn) { size_t us_ = malloc_usable_size((p)); unsigned s_ = sceKernelCpuSuspendIntr(); mmDel((p), us_); sceKernelCpuResumeIntr(s_); } } while (0)
#define MMDS(p, sz) do { if (memmapOn) { unsigned s_ = sceKernelCpuSuspendIntr(); mmDel((p), (sz)); sceKernelCpuResumeIntr(s_); } } while (0)
#define MMA_R(p, ra) do { if (memmapOn) { size_t us_ = malloc_usable_size((p)); unsigned s_ = sceKernelCpuSuspendIntr(); mmAdd((p), us_, (ra)); sceKernelCpuResumeIntr(s_); } } while (0)   // то же MMA с явным адресом вызывающего (_r-обёртки)
#define MMAS_R(p, sz, ra) do { if (memmapOn) { unsigned s_ = sceKernelCpuSuspendIntr(); mmAdd((p), (sz), (ra)); sceKernelCpuResumeIntr(s_); } } while (0)
#define MMAS(p, sz) do { if (memmapOn) { unsigned s_ = sceKernelCpuSuspendIntr(); mmAdd((p), (sz), (uint32_t)__builtin_return_address(0)); sceKernelCpuResumeIntr(s_); } } while (0)   // блок пула: размер класса, не malloc_usable_size
#define memmapOnRt() memmapOn
unsigned memmapReport(char *buf, unsigned cap, float t, unsigned freeKB, unsigned blkKB, unsigned costUs, unsigned maxStepUs) { return mmFormat(buf, cap, t, freeKB, blkKB, costUs, maxStepUs, (uint32_t)(uintptr_t)&memmapReport); }
void memmapScan(void) { unsigned s_ = sceKernelCpuSuspendIntr(); mmScan(); sceKernelCpuResumeIntr(s_); }
#else
#define MMA(p) do { } while (0)
#define MMD(p) do { } while (0)
#define MMDS(p, sz) do { } while (0)
#define MMAS(p, sz) do { } while (0)
#define MMA_R(p, ra) do { } while (0)
#define MMAS_R(p, sz, ra) do { } while (0)
#define memmapOnRt() 0
void memmapSetOn(int on) { (void)on; }
void memmapDump(void (*out)(const char *), const char *tag) { (void)out; (void)tag; }
void memmapFreeMap(void (*out)(const char *), const char *tag) { (void)out; (void)tag; }
unsigned memmapReport(char *buf, unsigned cap, float t, unsigned freeKB, unsigned blkKB, unsigned costUs, unsigned maxStepUs) { (void)buf; (void)cap; (void)t; (void)freeKB; (void)blkKB; (void)costUs; (void)maxStepUs; return 0; }
void memmapScan(void) {}
#endif

int (*lvaFreeXHook)(void *p);   // Build 96 шаг 6: main_stream.h ставит; блок из участка уровня free() не отдаёт в кучу
extern int zone3FreeX(void *p) __attribute__((weak));   // П5 Зоны-95 (zone3.c): блок из области «миссия»/«место» возвращается в область, а не в кучу
void *__real_malloc(size_t n);
void __real_free(void *p);
void *__real_calloc(size_t a, size_t b);
void *__real_realloc(void *p, size_t n);
void *__real_memalign(size_t al, size_t n);

volatile unsigned memFails, memFailLast, memFailBig;   // отказы: сколько всего, размер последнего и самого крупного, байт
// №1065: размер и адрес вызывающего каждого отказа (первые 16) — строка MEMFAILSZ в смоуке (main_frontend.h); адрес по ELF: tools/crash_addr.py
#define MF_RING 16u
static struct { unsigned bytes, ra; } memFailRing[MF_RING];
#ifndef MEMFAILHIST_OLD
#define MEMFAILHIST_OLD 0   // 1 - без последних 16 отказов и гистограммы по вызывающему (TEST93: кольцо хранило только первые 16 из 2225, остальное не разобрать)
#endif
// Опора-93 (TEST94): последние 16 отказов и гистограмма по адресу вызывающего (32 ячейки; не влезло - в «прочие»), в shar_report.txt и в ящике
#define MF_LAST 16u
#define MF_HIST 32u
static struct { unsigned bytes, ra; } mfLast[MF_LAST];
static struct { unsigned ra, n, sum, max, run; } mfHist[MF_HIST]; static unsigned mfHistOther;
// Лестница-96 (Fable шаг 1): повторы одного вызывающего подряд (без чужого отказа между ними) - цикл «отказ - повтор» виден числом. cur - текущая серия, best - самая длинная за прогон
#ifndef MEMFAILRUN_OLD
#define MEMFAILRUN_OLD 0   // 1 - без счёта серий одного ra подряд
#endif
static unsigned mfRunRa, mfRunCur, mfRunBest, mfRunBestRa;
static void miss(size_t n, unsigned ra) {
  if (memFails < MF_RING) { memFailRing[memFails].bytes = (unsigned)n; memFailRing[memFails].ra = ra; }
  if (!MEMFAILHIST_OLD) {
    mfLast[memFails % MF_LAST].bytes = (unsigned)n; mfLast[memFails % MF_LAST].ra = ra;
    if (!MEMFAILRUN_OLD) { if (memFails && ra == mfRunRa) mfRunCur++; else { mfRunRa = ra; mfRunCur = 1; } if (mfRunCur > mfRunBest) { mfRunBest = mfRunCur; mfRunBestRa = ra; } }
    unsigned k = 0; while (k < MF_HIST && mfHist[k].n && mfHist[k].ra != ra) k++;
    if (k < MF_HIST && !MEMFAILRUN_OLD && mfRunCur > mfHist[k].run) mfHist[k].run = mfRunCur;
    if (k < MF_HIST) { if (!mfHist[k].n) mfHist[k].ra = ra; mfHist[k].n++; mfHist[k].sum += (unsigned)n; if ((unsigned)n > mfHist[k].max) mfHist[k].max = (unsigned)n; } else mfHistOther++;
  }
  memFails++; memFailLast = (unsigned)n; if (n > memFailBig) memFailBig = (unsigned)n; }
// Служебно (только смоук, «flag memfailfake N»): N выдуманных отказов - N-3 от ra 0xDEAD0001, 2 от 0xDEAD0002, 3 снова от 0xDEAD0001; проверка печати серий (MEMFAILHIST run, MEMFAILRUN best) без настоящей нехватки памяти
void memFailFake(unsigned n) { for (unsigned i = 0; i < n; i++) miss(4096u, (i >= n - 5u && i < n - 3u) ? 0xDEAD0002u : 0xDEAD0001u); }
// k-й с конца отказ (0 - самый новый): 1 - есть
int memFailLastGet(unsigned k, unsigned *bytes, unsigned *ra) { if (k >= MF_LAST || k >= memFails) return 0; unsigned i = (memFails - 1u - k) % MF_LAST; *bytes = mfLast[i].bytes; *ra = mfLast[i].ra; return 1; }
// k-я по числу отказов запись гистограммы (0 - самая частая): 1 - есть; other - отказов вне таблицы
int memFailHistTopRun(unsigned k, unsigned *ra, unsigned *n, unsigned *sum, unsigned *max, unsigned *run);
int memFailHistTop(unsigned k, unsigned *ra, unsigned *n, unsigned *sum, unsigned *max) { unsigned run_; return memFailHistTopRun(k, ra, n, sum, max, &run_); }
// то же и самая длинная серия этого ra подряд (run)
int memFailHistTopRun(unsigned k, unsigned *ra, unsigned *n, unsigned *sum, unsigned *max, unsigned *run) {
  unsigned used[MF_HIST] = {0}, best = MF_HIST;
  for (unsigned r = 0; r <= k; r++) { best = MF_HIST; for (unsigned i = 0; i < MF_HIST; i++) if (mfHist[i].n && !used[i] && (best == MF_HIST || mfHist[i].n > mfHist[best].n)) best = i; if (best == MF_HIST) return 0; used[best] = 1; }
  *ra = mfHist[best].ra; *n = mfHist[best].n; *sum = mfHist[best].sum; *max = mfHist[best].max; *run = mfHist[best].run; return 1; }
// самая длинная серия одного ra подряд за прогон: 1 - есть отказы
int memFailRunBest(unsigned *ra, unsigned *len) { if (!memFails) return 0; *ra = mfRunBestRa; *len = mfRunBest; return 1; }
unsigned memFailHistOther(void) { return mfHistOther; }
int memFailRingGet(unsigned i, unsigned *bytes, unsigned *ra) { if (i >= memFails || i >= MF_RING) return 0; *bytes = memFailRing[i].bytes; *ra = memFailRing[i].ra; return 1; }

#ifndef DCACHE96_OLD
#define DCACHE96_OLD 0
#endif
int dcache96 = !DCACHE96_OLD, dcAll96On; unsigned dc96Calls, dc96Bytes, dc96PostCalls, dc96PostBytes;   // viewer/dcache96.h
// Правка 252 (03.10, зависание всей PSP у особняка, эмулятор не повторяет): служебная сборка -DMEM_SCRIBBLE=1 (и
// -Wl,--wrap=free) заливает каждое новое и освобождённое выделение байтом 0xFF — как «грязная» память настоящей PSP (эмулятор
// отдаёт кучу нулями). 0xFFFFFFFF — это NaN для float и -1/плохой адрес для целых и указателей: чтение неинициализированного
// или освобождённого сразу видно эмулятору-проверяльщику (tools/twins/ppsspp_hwcheck_build.sh: NaN, плохой адрес).
// В выпуске не собирается
#if MEM_SCRIBBLE
#include <string.h>
void __real_free(void *p);
void __wrap_free(void *p) { if (p && zone3FreeX && zone3FreeX(p)) return; if (p && lvaFreeXHook && lvaFreeXHook(p)) return; if (p) memset(p, 0xFF, malloc_usable_size(p)); MMD(p); __real_free(p); }
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
void __wrap_free(void *p) { if (p && zone3FreeX && zone3FreeX(p)) return; if (p && lvaFreeXHook && lvaFreeXHook(p)) return; ltDel(p); __real_free(p); }
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
void *__wrap__malloc_r(struct _reent *r, size_t n) { return __real__malloc_r(r, n); }   // LEAK696: _r-обёртки только пропускают
void *__wrap__calloc_r(struct _reent *r, size_t a, size_t b) { return __real__calloc_r(r, a, b); }
void *__wrap__realloc_r(struct _reent *r, void *q, size_t n) { return __real__realloc_r(r, q, n); }
void *__wrap__memalign_r(struct _reent *r, size_t al, size_t n) { return __real__memalign_r(r, al, n); }
void __wrap__free_r(struct _reent *r, void *p) { __real__free_r(r, p); }
#else
// Пул мелких блоков (Build 95 шаг 1): блоки <= 2 КБ (выравнивание <= 16) - из постоянного куска, выделенного на старте внизу кучи (smallPoolInit, первым вызовом main); free/realloc узнают пул по диапазону адреса.
// Кончился пул - обычная куча, счётчик spOver (в shar_report и ящик). SMALLPOOL_OLD=1 - пула нет, как было
#ifndef SMALLPOOL_OLD
#define SMALLPOOL_OLD 0
#endif
#define SP_GEN 4096u   // общий порог пула (malloc/calloc/realloc/memalign с выравниванием < 64); 4-8 КБ - только memalign >= 64 (readAll msn_view.c:59); листы GE клеток (5,7 КБ, потоком) идут в кучу
#ifndef SMALLPOOL_KB
#define SMALLPOOL_KB 1536u
#endif
#if !SMALLPOOL_OLD
#define SP_LOCK() unsigned sp_s_ = sceKernelCpuSuspendIntr()
#define SP_UNLOCK() sceKernelCpuResumeIntr(sp_s_)
#include "smallpool.h"
void smallPoolInit(void) { if (spBase) return; void *m = __real_memalign(8192, SMALLPOOL_KB << 10); if (m) spInit(m, SMALLPOOL_KB << 10); }
unsigned smallPoolOver(void) { return spOver; }
unsigned smallPoolPages(void) { return spPagesUsed; }
unsigned smallPoolLive(void) { return spLive; }
unsigned smallPoolLiveMax(void) { return spLiveMax; }
unsigned smallPoolBase(void) { return (unsigned)(uintptr_t)spBase; }
#define SPIN(p) spIn(p)
#else
void smallPoolInit(void) {}
unsigned smallPoolOver(void) { return 0; }
unsigned smallPoolPages(void) { return 0; }
unsigned smallPoolLive(void) { return 0; }
unsigned smallPoolLiveMax(void) { return 0; }
unsigned smallPoolBase(void) { return 0; }
#define SPIN(p) 0
#endif
#ifndef MALLOCR_OLD
#define MALLOCR_OLD 0   // 1 - обёртки _malloc_r и родни только пропускают (выделения внутри libc не видны учёту и пулу), как было
#endif
#if MALLOCR_OLD
void *__wrap_malloc(size_t n) {
#if !SMALLPOOL_OLD
  if (n && n <= SP_MAXB) { void *q = spAlloc(n); if (q) { MMAS(q, spSizeOf(q)); return q; } }
#endif
  void *p = __real_malloc(n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); SCRIB(p, n); if (p) MMA(p); return p; }
void *__wrap_calloc(size_t a, size_t b) {
#if !SMALLPOOL_OLD
  if (a && b && a <= SP_MAXB && b <= SP_MAXB && a * b <= SP_MAXB) { void *q = spAlloc(a * b); if (q) { memset(q, 0, a * b); MMAS(q, spSizeOf(q)); return q; } }
#endif
  void *p = __real_calloc(a, b); if (!p && a && b) miss(a * b, (unsigned)__builtin_return_address(0)); if (p) MMA(p); return p; }
void *__wrap_realloc(void *q, size_t n) {
#if !SMALLPOOL_OLD
  if (q && SPIN(q)) {   // блок пула: влезает в класс - остаётся, иначе новый блок (пул или куча), копия, старый возвращается в класс
    unsigned os = spSizeOf(q);
    if (!n) { MMDS(q, os); spFree(q); return 0; }
    if (n <= os) return q;
    void *np = __wrap_malloc(n); if (!np) return 0;
    memcpy(np, q, os); MMDS(q, os); spFree(q); return np; }
  if (!q && n && n <= SP_MAXB) return __wrap_malloc(n);
#endif
  size_t qs_ = (memmapOnRt() && q) ? malloc_usable_size(q) : 0; void *p = __real_realloc(q, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); if (q && (p || !n)) MMDS(q, qs_); if (p) MMA(p); return p; }
void *__wrap_memalign(size_t al, size_t n) {
#if !SMALLPOOL_OLD
  if (al <= SP_MAXB && !(al & (al - 1)) && n && (n <= SP_GEN || (al >= 64 && n <= SP_MAXB))) { void *q = spAlloc(n < al ? al : n); if (q) { MMAS(q, spSizeOf(q)); return q; } }   // выравнивание степенью двойки <= размер класса: слот класса кратен своему размеру, страница выровнена на 8 КБ
#endif
  void *p = __real_memalign(al, n); if (!p && n) miss(n, (unsigned)__builtin_return_address(0)); SCRIB(p, n); if (p) MMA(p); return p; }
#if !MEM_SCRIBBLE
void __real_free(void *p);
void __wrap_free(void *p) {
  if (p && zone3FreeX && zone3FreeX(p)) return; if (p && lvaFreeXHook && lvaFreeXHook(p)) return;   // П5 Зоны-95: блок области «миссия»/«место»/Z2 отдаёт зона
#if !SMALLPOOL_OLD
  if (p && SPIN(p)) { MMDS(p, spSizeOf(p)); spFree(p); return; }
#endif
  if (p) MMD(p); __real_free(p); }
#endif
void *__wrap__malloc_r(struct _reent *r, size_t n) { return __real__malloc_r(r, n); }
void *__wrap__calloc_r(struct _reent *r, size_t a, size_t b) { return __real__calloc_r(r, a, b); }
void *__wrap__realloc_r(struct _reent *r, void *q, size_t n) { return __real__realloc_r(r, q, n); }
void *__wrap__memalign_r(struct _reent *r, size_t al, size_t n) { return __real__memalign_r(r, al, n); }
void __wrap__free_r(struct _reent *r, void *p) { __real__free_r(r, p); }
#else
// Build 95 (Опора-93): ВСЕ пути выделения (наш код через --wrap=malloc... и выделения ВНУТРИ libc через _malloc_r..., stdio, printf, reent) идут через одни функции wm/wc/wr/wa/wf: учёт MEMMAP, пул мелочи, счётчик отказов.
// Настоящее выделение - всегда __real__*_r (не __real_malloc), поэтому двойного учёта нет. free по адресу вне нашего учёта (до обёртки, из libc) идёт в настоящий free без падения.
// выделения, которые newlib делает САМ внутри _calloc_r/_realloc_r/_memalign_r (через _malloc_r): мимо пула (memalign режет чанк по заголовку, указатель пула испортил бы слоты) и мимо учёта
// (блок учтёт внешняя обёртка по итоговому указателю; иначе двойная запись и «призраки» в таблице MEMLIVE). Размеры функций - psp-nm -S (0xF0, 0x5CC, 0x1AC) с запасом
static inline int inLibcR(unsigned ra) { return (ra - (unsigned)(uintptr_t)__real__calloc_r) < 0x100u || (ra - (unsigned)(uintptr_t)__real__realloc_r) < 0x600u || (ra - (unsigned)(uintptr_t)__real__memalign_r) < 0x1B0u; }
// Build 96 (Опора-93, smallpool over при дальности 250 м на L4, ballast 5: 89 строк за заезд, к концу 727 отказов): списки GE клеток (main_render.h dlAlloc95, dlBuild*, 2-4 КБ) занимали 46 из 192
// страниц пула и растут с числом клеток. Такие выделения идут мимо пула через memalignNoPool: поток помечает себя на время вызова, а обёртки wm/wa его блок в пул не берут (newlib для memalign с
// выравниванием <= 16 хвостовым вызовом идёт в _malloc_r, поэтому проверка нужна и в wm). SPNOPOOL_OLD=1 - как было (всё в пул)
#ifndef SPNOPOOL_OLD
#define SPNOPOOL_OLD 0
#endif
static volatile unsigned spNoPoolTid; static volatile int spNoPoolOn;
static inline int spNoPoolMine(void) { return !SPNOPOOL_OLD && spNoPoolOn && spNoPoolTid == (unsigned)sceKernelGetThreadId(); }
void *memalignNoPool(size_t al, size_t n) {
  unsigned s_ = sceKernelCpuSuspendIntr(); spNoPoolTid = (unsigned)sceKernelGetThreadId(); spNoPoolOn++; sceKernelCpuResumeIntr(s_);
  void *p = memalign(al, n);
  s_ = sceKernelCpuSuspendIntr(); if (spNoPoolOn > 0) spNoPoolOn--; sceKernelCpuResumeIntr(s_);
  return p; }
static void *wm(struct _reent *r, size_t n, unsigned ra) {
  if (inLibcR(ra)) { void *p = __real__malloc_r(r, n); if (!p && n) miss(n, ra); return p; }
#if !SMALLPOOL_OLD
  if (n && n <= SP_GEN && !spNoPoolMine()) { void *q = spAlloc(n); if (q) { MMAS_R(q, spSizeOf(q), ra); return q; } }
#endif
  void *p = __real__malloc_r(r, n); if (!p && n) miss(n, ra); SCRIB(p, n); if (p) MMA_R(p, ra); return p; }
static void *wc(struct _reent *r, size_t a, size_t b, unsigned ra) {
#if !SMALLPOOL_OLD
  if (a && b && a <= SP_GEN && b <= SP_GEN && a * b <= SP_GEN) { void *q = spAlloc(a * b); if (q) { memset(q, 0, a * b); MMAS_R(q, spSizeOf(q), ra); return q; } }
#endif
  void *p = __real__calloc_r(r, a, b); if (!p && a && b) miss(a * b, ra); if (p) MMA_R(p, ra); return p; }
static void *wa(struct _reent *r, size_t al, size_t n, unsigned ra) {
#if !SMALLPOOL_OLD
#ifndef SPMEMAL_OLD
#define SPMEMAL_OLD 0   // 1 - memalign до 8 КБ любого выравнивания идёт в пул (было в Build 95/96: листы GE клеток, дальние листы, пропы 4-8 КБ набивали страницы по 8 КБ, WATCHDOG smallpool over в езде L4)
#endif
  if (!spNoPoolMine() && al <= SP_MAXB && !(al & (al - 1)) && n && (SPMEMAL_OLD ? n <= SP_MAXB : (n <= SP_GEN || (al >= 64 && n <= SP_MAXB)))) { void *q = spAlloc(n < al ? al : n); if (q) { MMAS_R(q, spSizeOf(q), ra); return q; } }   // выравнивание степенью двойки <= размер класса: слот кратен размеру, страница выровнена на 8 КБ
#endif
  void *p = __real__memalign_r(r, al, n); if (!p && n) miss(n, ra); SCRIB(p, n); if (p) MMA_R(p, ra); return p; }
static void wf(struct _reent *r, void *p) {
  if (p && zone3FreeX && zone3FreeX(p)) return; if (p && lvaFreeXHook && lvaFreeXHook(p)) return;   // П5 Зоны-95: блок области «миссия»/«место»/Z2 отдаёт зона (раньше пула мелочи)
#if !SMALLPOOL_OLD
  if (p && SPIN(p)) { MMDS(p, spSizeOf(p)); spFree(p); return; }
#endif
  if (p) MMD(p); __real__free_r(r, p); }
static void *wr(struct _reent *r, void *q, size_t n, unsigned ra) {
#if !SMALLPOOL_OLD
  if (q && SPIN(q)) {   // блок пула: влезает в класс - остаётся, иначе новый блок (пул или куча), копия, старый возвращается в класс
    unsigned os = spSizeOf(q);
    if (!n) { MMDS(q, os); spFree(q); return 0; }
    if (n <= os) return q;
    void *np = wm(r, n, ra); if (!np) return 0;
    memcpy(np, q, os); MMDS(q, os); spFree(q); return np; }
  if (!q && n && n <= SP_GEN) return wm(r, n, ra);
#endif
  size_t qs_ = (memmapOnRt() && q) ? malloc_usable_size(q) : 0; void *p = __real__realloc_r(r, q, n); if (!p && n) miss(n, ra); if (q && (p || !n)) MMDS(q, qs_); if (p) MMA_R(p, ra); return p; }
void *__wrap__malloc_r(struct _reent *r, size_t n) { return wm(r, n, (unsigned)__builtin_return_address(0)); }
void *__wrap__calloc_r(struct _reent *r, size_t a, size_t b) { return wc(r, a, b, (unsigned)__builtin_return_address(0)); }
void *__wrap__realloc_r(struct _reent *r, void *q, size_t n) { return wr(r, q, n, (unsigned)__builtin_return_address(0)); }
void *__wrap__memalign_r(struct _reent *r, size_t al, size_t n) { return wa(r, al, n, (unsigned)__builtin_return_address(0)); }
void __wrap__free_r(struct _reent *r, void *p) { wf(r, p); }
void *__wrap_malloc(size_t n) { return wm(_REENT, n, (unsigned)__builtin_return_address(0)); }
void *__wrap_calloc(size_t a, size_t b) { return wc(_REENT, a, b, (unsigned)__builtin_return_address(0)); }
void *__wrap_realloc(void *q, size_t n) { return wr(_REENT, q, n, (unsigned)__builtin_return_address(0)); }
void *__wrap_memalign(size_t al, size_t n) { return wa(_REENT, al, n, (unsigned)__builtin_return_address(0)); }
#if !MEM_SCRIBBLE
void __wrap_free(void *p) { wf(_REENT, p); }
#endif
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
    unsigned m = lo + (hi - lo) / 2; void *p = PROBE_MALLOC(m);
    if (p) { PROBE_FREE(p); lo = m; } else hi = m;
  }
  __malloc_unlock(_REENT);
  return lo;
}
// TEST90 MEMMAP: та же проба по шагам (по iters делений за вызов), чтобы замер не ложился одним кадром; 1 — готово (lo — наибольший кусок с точностью 16 КБ). Проба идёт мимо учёта (__real_malloc/__real_free)
int memMaxBlockStep(unsigned *lo, unsigned *hi, int iters) {
  __malloc_lock(_REENT);
  for (int i = 0; i < iters && *hi - *lo > (16u << 10); i++) { unsigned m = *lo + (*hi - *lo) / 2; void *p = PROBE_MALLOC(m); if (p) { PROBE_FREE(p); *lo = m; } else *hi = m; }
  __malloc_unlock(_REENT);
  return *hi - *lo <= (16u << 10);
}
