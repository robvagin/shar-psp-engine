// Пул мелких блоков (Build 95 шаг 1, Опора-93, по замеру Fable-аудита 08.11): после выгрузки уровня в середине свободной кучи оставалось ~20 мелких блоков (< 2 КБ), лениво выделенных во время игры;
// они делили 18 МБ свободного на куски 1-6 МБ. Блоки <= SP_MAXB берутся из одного постоянного куска (1,5 МБ, выделяется на старте самым первым, т.е. внизу кучи), а не из общей кучи.
// Страницы по 4 КБ закрепляются за классом размера (32..2048) и больше не возвращаются; свободные слоты - в односвязном списке класса. Кончился пул - вызывающий идёт в обычную кучу, счётчик spOver.
// Чистый C без PSP (хост-тест tests/smallpool/host.c): блокировку задаёт вызывающий (SP_LOCK/SP_UNLOCK). SMALLPOOL_OLD=1 (memwatch.c) - пула нет
#ifndef SMALLPOOL_H
#define SMALLPOOL_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifndef SP_LOCK
#define SP_LOCK() do { } while (0)
#define SP_UNLOCK() do { } while (0)
#endif
#define SP_PAGE 8192u
#define SP_MAXB 8192u   // блоки <= 8 КБ: после Z0-init остаётся режущий блок readAll msn_view.c:59 (3-5 КБ), пересоздаётся на каждом уровне посреди свободной кучи
#define SP_NCLS 9
#define SP_MAXPAGES 512u
static const unsigned spClsSz[SP_NCLS] = {32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 4096u, 8192u};
static unsigned char *spBase; static unsigned spBytes, spNPage, spPagesUsed, spPageClsT[SP_MAXPAGES];
static void *spFreeL[SP_NCLS]; static unsigned spLive, spLiveMax, spOver;
static inline int spCls(size_t n) { int c = 0; while (c < SP_NCLS - 1 && n > spClsSz[c]) c++; return c; }
static inline int spIn(const void *p) { return spBase && (const unsigned char *)p >= spBase && (const unsigned char *)p < spBase + spBytes; }
static inline unsigned spSizeOf(const void *p) { return spClsSz[spPageClsT[((const unsigned char *)p - spBase) / SP_PAGE]]; }   // размер класса (для учёта и realloc)
// mem - постоянный кусок, выровненный на 4096; bytes кратно 4096
static void spInit(void *mem, unsigned bytes) {
  spBase = (unsigned char *)mem; spBytes = bytes; spNPage = bytes / SP_PAGE; if (spNPage > SP_MAXPAGES) { spNPage = SP_MAXPAGES; spBytes = spNPage * SP_PAGE; }
  spPagesUsed = 0; memset(spFreeL, 0, sizeof spFreeL); spLive = spLiveMax = spOver = 0; }
static void *spAlloc(size_t n) {
  if (!spBase || n > SP_MAXB) return 0;
  int c = spCls(n ? n : 1); void *p = 0;
  SP_LOCK();
  if (!spFreeL[c] && spPagesUsed < spNPage) {   // новая страница под класс: слоты в список от высоких к низким - первый выданный низкий
    unsigned pg = spPagesUsed++; spPageClsT[pg] = (unsigned)c; unsigned sz = spClsSz[c], ns = SP_PAGE / sz;
    for (unsigned k = ns; k-- > 0;) { void **s = (void **)(spBase + pg * SP_PAGE + k * sz); *s = spFreeL[c]; spFreeL[c] = s; } }
  if (spFreeL[c]) { p = spFreeL[c]; spFreeL[c] = *(void **)p; if (++spLive > spLiveMax) spLiveMax = spLive; } else spOver++;
  SP_UNLOCK();
  return p; }
static void spFree(void *p) {
  unsigned pg = (unsigned)(((unsigned char *)p - spBase) / SP_PAGE); int c = (int)spPageClsT[pg];
  SP_LOCK(); *(void **)p = spFreeL[c]; spFreeL[c] = p; if (spLive) spLive--; SP_UNLOCK(); }
#endif
