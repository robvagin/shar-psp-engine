// Зона 3 «эпизод»: обёртка над zone3.h для всех единиц сборки (ядро в заголовке — оно же проверяется на Маке в tests/zone3/host.c).
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <pspkernel.h>
#include "zone3_api.h"
static void (*z3LogFn)(const char *);
#define Z3_LOGF(s) do { if (z3LogFn) z3LogFn(s); } while (0)
static int z3MainTid = -1;
#define Z3_ISMAIN() (z3MainTid < 0 || sceKernelGetThreadId() == z3MainTid)
#include "zone3.h"
void *zone3AllocX(int area, const char *who, unsigned size, unsigned align) { return zone3Alloc(area, who, size, align); }
void *zone3TryX(int area, const char *who, unsigned size, unsigned align) {
  if (!z3On || ZONE3_OLD || !z3A[area].base) return memalign(align, size ? size : 16);
  return z3AreaAlloc(&z3A[area], who, size);
}
int zone3FreeX(void *p) {
  if (!z3On) return 0;
  for (int i = 0; i < Z3_NAREA; i++) if (z3AreaFree(&z3A[i], p)) return 1;
  return 0;
}
int zone3OwnsX(const void *p) { return zone3Owns(p); }
unsigned zone3MarkX(int area) { return zone3Mark(area); }
int zone3ReleaseX(int area, unsigned mark) { return zone3Release(area, mark); }
int zone3CheckX(int area, const char *why) {
  unsigned mf, ft, nl; if (!z3On) return 0; z3AreaStat(&z3A[area], &mf, &ft, &nl);
  if (z3LogFn) { char s[160]; snprintf(s, sizeof s, "ZONE3 check %s %s live=%u free=%.2f/%.2f used_kb=%u\n", z3A[area].name, why ? why : "", nl, mf / 1048576.f, ft / 1048576.f, z3A[area].usedB >> 10); z3LogFn(s); }
  return (int)nl;
}
// 102: конец уровня - город проверяется так же громко, как «место» (канон п.12). Живые блоки в области города после выгрузки клеток = утечка владельца (хвост пула, ggBatch): счётчик leaks и строка WATCHDOG zone3 leak.
// Область НЕ стирается и остаётся прицепленной (владельцы хвоста могут её ещё держать): стирание за спиной владельца даёт висячие указатели. ZONE3CITYLEAK_OLD=1 / файл zone3cityleakold - как было (город не проверялся)
int zone3LeakCityX(const char *why) {
  unsigned mf, ft, nl; if (!z3On) return 0; z3AreaStat(&z3A[Z3A_CITY], &mf, &ft, &nl);
  if (nl) { z3A[Z3A_CITY].leaks += nl;
    if (z3LogFn) { char s[200]; snprintf(s, sizeof s, "WATCHDOG zone3 leak area=city live=%u kb=%u %s\n", nl, z3A[Z3A_CITY].usedB >> 10, why ? why : ""); z3LogFn(s); } }
  return (int)nl;
}
int zone3ResetX(int area, const char *why) {
  unsigned mf, ft, nl; if (!z3On) return 0; z3AreaStat(&z3A[area], &mf, &ft, &nl);
  if (nl) { z3A[area].leaks += nl;
    if (z3LogFn) { char s[200]; snprintf(s, sizeof s, "WATCHDOG zone3 leak area=%s live=%u used_kb=%u %s\n", z3A[area].name, nl, z3A[area].usedB >> 10, why ? why : ""); z3LogFn(s); }
    return (int)nl; }
  z3A[area].resets++; return 0;
}
int zone3InitX(unsigned missionB, unsigned placeB, unsigned cityB, unsigned budget) { if (z3MainTid < 0) z3MainTid = sceKernelGetThreadId(); return zone3Init(missionB, placeB, cityB, budget); }
// диагностика (Лестница-96): живые блоки области по владельцам - "who=число/КБ ..."; живых больше 12 видов не бывает
void zone3DumpX(int area, char *o, unsigned n) {
  char w[12][16]; unsigned c[12], b[12]; int k = 0; if (n) o[0] = 0; if (!z3On || !z3A[area].base) return;
  Z3Area *a = &z3A[area];
  for (unsigned of = 0; of < a->cap; ) { Z3Hdr *h = (Z3Hdr *)(a->base + of);
    if (h->used) { int j = 0; for (; j < k; j++) if (!strcmp(w[j], h->who)) break;
      if (j == k && k < 12) { snprintf(w[k], 16, "%s", h->who); c[k] = b[k] = 0; k++; }
      if (j < k) { c[j]++; b[j] += h->size; } }
    of += h->size; }
  unsigned l = 0; for (int j = 0; j < k && l + 24 < n; j++) l += (unsigned)snprintf(o + l, n - l, "%s=%u/%uK ", w[j], c[j], b[j] >> 10);
}
int zone3CityOnX(void) { return z3On && z3A[Z3_CITY].base != 0; }
int zone3WantedX(void) { return !ZONE3_OLD && (z3Want[0] || z3Want[1] || z3Want[2]); }
int zone3AreaOnX(int area) { return z3On && z3A[area].base != 0; }
unsigned zone3ShrinkX(unsigned want, const char *why) { return zone3Shrink(want, why); }
int zone3RegrowX(unsigned heapFree, unsigned need, unsigned ring, const char *why) { return zone3Regrow(heapFree, need, ring, why); }   // need — уровню вне клеток; ring — кольцо клеток старта: его держит город, а без города — куча
void zone3SetRegionOldX(int old) { if (!z3On) z3RegionOld = old; }
unsigned zone3HomeX(int area) { return (unsigned)(unsigned long)z3Home[area]; }
unsigned zone3RegMissX(void) { return z3RegMiss; }
unsigned zone3FreeTotX(int area) { unsigned ft = 0; if (z3On) z3AreaStat(&z3A[area], 0, &ft, 0); return ft; }
unsigned zone3UsedX(int area) { return z3On ? z3A[area].usedB : 0; }
int zone3OnX(void) { return z3On; }
unsigned zone3MaxFreeX(int area) { unsigned mf = 0; if (z3On) z3AreaStat(&z3A[area], &mf, 0, 0); return mf; }
void zone3SetHooks(int (*reclaim)(int, const char *, unsigned, int), void (*logFn)(const char *)) { z3Reclaim = reclaim; z3LogFn = logFn; }
void zone3StatLineX(char *o, unsigned n, float t, const char *why) { zone3StatLine(o, n, t, why); }
int zone3RegionIsOldX(void) { return z3RegionOld; }
void zone3SetTightX(int area, unsigned bytes) { z3Tight[area] = bytes; }
void zone3SetFloorX(int on) { z3FloorOn = on; }
int zone3EnsureFloorX(const char *why) { return z3EnsureFloor(why); }
void zone3SetScribbleX(int on) { z3Scribble = on; }   // служебно (шаг 7 Fable): после освобождения и сброса блоки областей заливаются 0xFF
int zone3ScribbleOnX(void) { return z3Scribble; }
void zone3DangleX(int area, const char *who) {   // красный контроль: канарейка в области, указатель остаётся у проверки (zone3.h z3DangleCheck)
  if (!z3On || area < 0 || area >= Z3_NAREA || !z3A[area].base) { if (z3LogFn) { char l_[120]; snprintf(l_, sizeof l_, "ZONE3 dangle NOT armed area=%d (область не прицеплена)\n", area); z3LogFn(l_); } return; }
  unsigned char *p = (unsigned char *)z3AreaAlloc(&z3A[area], who ? who : "canary", 64); if (!p) return;
  memset(p, 0x5A, 64); z3Dangle = p; z3DangleArea = area; if (z3LogFn) { char l_[120]; snprintf(l_, sizeof l_, "ZONE3 dangle armed area=%d scribble=%d\n", area, z3Scribble); z3LogFn(l_); }
}
int zone3DangleHitsX(void) { return z3DangleHits; }
void zone3DangleFreeX(void) { if (z3Dangle) zone3FreeX(z3Dangle); }   // красный контроль: владелец возвращает блок канарейки, указатель у проверки остался (z3DangleCheck при заливке видит 0xFF)
