// TEST91 зоны памяти 1А, шаг 1 (docs/handoff/АРХИТЕКТУРА_ПАМЯТЬ_ЗАГРУЗКА_2026-11-04.md §1.4): постоянные слоты под данные «уровень+1» (пакет интерфейса ui.shr 1,04 МБ,
// пакет газеты load.shr 1,05 МБ). Слот выделяется один раз при первом запросе (при запуске игры куча цельная), дальше пакет следующего уровня читается в тот же адрес:
// острова в середине кучи после смены уровня не возникают. Внутри слота — подряд, по 64 Б (пакет ui.shr читается и несколькими кусками); слот освобождается, когда отданы все его куски.
// Пакет больше места, слот занят или не выделился — вызывающий берёт кучу, как раньше (zsAlloc возвращает 0). Чистая арифметика, без PSP: тест tests/zoneslot.
#ifndef ZONESLOT_H
#define ZONESLOT_H
#include <stdlib.h>
#if defined(__APPLE__) || defined(ZS_HOST)   // хост-тест на Маке: нет malloc.h / memalign
static inline void *zsMemalign(unsigned a, unsigned n) { void *p = 0; return posix_memalign(&p, a, n) ? 0 : p; }
#else
#include <malloc.h>
static inline void *zsMemalign(unsigned a, unsigned n) { return memalign(a, n); }
#endif
typedef struct { unsigned char *base; unsigned cap, used, live; int tried; } ZSlot;
static inline void *zsAlloc(ZSlot *z, unsigned n) {
  if (!z->cap) return 0;
  if (!z->base && !z->tried) { z->tried = 1; z->base = (unsigned char *)zsMemalign(64, z->cap); z->used = z->live = 0; }   // один раз: не вышло — больше не пробуем (куча как раньше)
  if (!z->base) return 0;
  unsigned off = (z->used + 63u) & ~63u;
  if (off > z->cap || n > z->cap - off) return 0;
  z->used = off + n; z->live++; return z->base + off;
}
static inline int zsOwns(const ZSlot *z, const void *p) { return z->base && (const unsigned char *)p >= z->base && (const unsigned char *)p < z->base + z->cap; }
// 1 — кусок из слота (место возвращено, когда отданы все куски слота), 0 — не из слота
static inline int zsFree(ZSlot *z, void *p) {
  if (!zsOwns(z, p)) return 0;
#ifdef ZS_NOREUSE   // ТОЛЬКО красный контроль теста: слот не освобождается, следующее чтение ложится дальше или в кучу
  return 1;
#endif
  if (z->live) z->live--;
  if (!z->live) z->used = 0;
  return 1;
}
#endif
