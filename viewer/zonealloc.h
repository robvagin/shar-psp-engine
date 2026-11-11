// П1-95 (Fable-спецификация 08.11, раздел А.3): интерфейс зон памяти. ЭТО ЗАГЛУШКА ПОВЕРХ КУЧИ: клиенты можно переводить уже сейчас, зоны потом встанут под тот же интерфейс.
// Зоны: Z0 постоянная, Z1 уровень, Z2 город, Z3M «миссия», Z3P «место» (стек с отметками).
//   void *zoneAlloc(zone, who, size)  блок из зоны или 0; при отказе один раз зовёт лестницу памяти memLadder(who, size, LAD_TOP) и повторяет; пишет WATCHDOG mem who=... fail; не падает
//   unsigned zoneMark(zone)           отметка (стек «места»: перед входом)
//   void zoneRelease(zone, mark)      освободить все блоки зоны, выделенные ПОСЛЕ отметки (в обратном порядке)
//   void zoneFree(zone)               освободить все блоки зоны (Z1 = lvaReset, Z2 = cellsDropAll, Z3 оба - позже, когда зоны будут настоящими)
//   void zoneFreeBlock(p)             освободить один блок, выданный zoneAlloc (клиент НЕ зовёт free() сам, иначе реестр заглушки разойдётся)
// Реализация в main.c (там memladder.h): перед #include "zonealloc.h" задать #define ZONEALLOC_IMPL. Остальные файлы включают его только ради прототипов.
// ZONEALLOC_OLD=1 — zoneAlloc = memalign без реестра и лестницы (прежнее поведение клиентов)
#ifndef ZONEALLOC_H
#define ZONEALLOC_H
typedef enum { ZONE_Z0 = 0, ZONE_Z1, ZONE_Z2, ZONE_Z3M, ZONE_Z3P, ZONE_N } ZoneId;
void *zoneAlloc(ZoneId zone, const char *who, unsigned size);
unsigned zoneMark(ZoneId zone);
void zoneRelease(ZoneId zone, unsigned mark);
void zoneFree(ZoneId zone);
void zoneFreeBlock(void *p);
unsigned zoneBytes(ZoneId zone);   // сколько байт сейчас занято блоками зоны (для строки ZONE в журнале)
#endif
#ifdef ZONEALLOC_IMPL
#ifndef ZONEALLOC_IMPL_DONE
#define ZONEALLOC_IMPL_DONE
#ifndef ZONEALLOC_OLD
#define ZONEALLOC_OLD 0
#endif
#define ZA_MAX 2048u
static struct { void *p; unsigned sz, seq; unsigned char zone; } zaT[ZA_MAX]; static unsigned zaSeq, zaBytes[ZONE_N], zaFails;
void *zoneAlloc(ZoneId zone, const char *who, unsigned size) {
  if (!size) return 0;
  void *p = memalign(64, size);
  if (!p && !ZONEALLOC_OLD) { memLadder(who, size, LAD_TOP); p = memalign(64, size); if (!p) { zaFails++; if (smkOn) { char l[96]; snprintf(l, sizeof l, "ZONEFAIL zone=%d who=%s size=%u t=%.1f\n", (int)zone, who ? who : "?", size, smkT); smkWrite(l); } } }
  if (p && !ZONEALLOC_OLD && zone < ZONE_N) { for (unsigned i = 0; i < ZA_MAX; i++) if (!zaT[i].p) { zaT[i].p = p; zaT[i].sz = size; zaT[i].seq = ++zaSeq; zaT[i].zone = (unsigned char)zone; zaBytes[zone] += size; break; } }   // реестр полон: блок живёт без учёта (free клиентом работает)
  return p; }
unsigned zoneMark(ZoneId zone) { (void)zone; return zaSeq; }
static void zaDrop(unsigned i) { zaBytes[zaT[i].zone] -= zaT[i].sz; free(zaT[i].p); zaT[i].p = 0; }
void zoneRelease(ZoneId zone, unsigned mark) {
  for (;;) { int best = -1; for (unsigned i = 0; i < ZA_MAX; i++) if (zaT[i].p && zaT[i].zone == zone && zaT[i].seq > mark && (best < 0 || zaT[i].seq > zaT[best].seq)) best = (int)i; if (best < 0) break; zaDrop((unsigned)best); } }
void zoneFree(ZoneId zone) { zoneRelease(zone, 0); }
void zoneFreeBlock(void *p) { if (!p) return; for (unsigned i = 0; i < ZA_MAX; i++) if (zaT[i].p == p) { zaDrop(i); return; } free(p); }
unsigned zoneBytes(ZoneId zone) { return zone < ZONE_N ? zaBytes[zone] : 0; }
#endif
#endif
