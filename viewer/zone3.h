// Зона 3 «эпизод» (Fable-спецификация 08.11, раздел А, пакет П5 Зоны-95; аудит 07.11 §2.2, §5).
// Две области в одном постоянном блоке кучи, взятом один раз при старте игры (куча тогда цельная):
//   «миссия» (Z3_MISSION): модели персонажей и машин миссий, кэш картинок миссий, дорожки гэгов. Стирается на конце или смене миссии и на смене уровня.
//   «место»  (Z3_PLACE):   стек под комнату, будку, ролик, пакеты меню и паузы, лавку, гардероб, панорамы. Отметка перед входом, сброс до отметки при выходе.
// Внутри области — подходящий по размеру блок с склейкой соседей, заголовок 64 Б (строка кэша, данные 64-выровнены). Каждый блок помнит номер выдачи (seq):
// сброс до отметки освобождает все блоки с seq больше отметки, не трогая тех, кто жил до входа. Область не растёт и в кучу не выходит сама: отказ внутри области
// лечится ступенями этой же области (z3Reclaim, ставит движок), и только потом блок уходит в кучу «проливом» (строка журнала, счётчик) — игра не ломается, но дыру видно.
// Чужой указатель (из кучи) zone3Free отдаёт в free(): переключение режима при живых блоках безопасно (как cpFree).
// Выключатель: ZONE3_OLD=1 (сборка), файл `zone3old` рядом с EBOOT, flag zone3old 1 — прежние memalign/free у клиентов. Хост-проверка на Маке: tests/zone3/host.c (-DZ3_HOST).
#ifndef ZONE3_H
#define ZONE3_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef ZONE3_OLD
#define ZONE3_OLD 0
#endif
#ifndef Z3_HOST
extern void __malloc_lock(struct _reent *); extern void __malloc_unlock(struct _reent *);
#define Z3_LOCK() __malloc_lock(_REENT)     // замок malloc рекурсивный и общий с загрузчиком: клиенты зовут из главного потока и из потока чтения
#define Z3_UNLOCK() __malloc_unlock(_REENT)
#else
#define Z3_LOCK() ((void)0)
#define Z3_UNLOCK() ((void)0)
#endif
#ifndef Z3_LOGF
#define Z3_LOGF(s) ((void)0)    // движок подставляет запись в журнал смоука и чёрный ящик
#endif
#define Z3_HDR 64u
#define Z3_MAGIC 0x5A33B10Cu
enum { Z3_MISSION = 0, Z3_PLACE = 1, Z3_CITY = 2, Z3_NAREA = 3 };   // Z3_CITY — зона 2 «город»: клетки и их хвосты (cellpool.h); без проливов в кучу: нет места — стрим ждёт и отдаёт далёкое
typedef struct { unsigned size, used, seq, magic; char who[16]; unsigned pad[8]; } Z3Hdr;    // 64 байта; size — вместе с заголовком
typedef struct {
  unsigned char *base; unsigned cap, seq, usedB, peakB, allocs, frees, fails, spills, spillB, resets, releases, leaks;
  const char *name;
} Z3Area;
static Z3Area z3A[Z3_NAREA] = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "mission" }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "place" }, { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "city" } };
static int z3On, z3Scribble;       // z3On — области взяты; z3Scribble — после сброса заливать 0xFF (служебно: чужой указатель на сброшенное сразу виден)
static unsigned z3Cap0;            // кучи не хватило на области — z3On остаётся 0, клиенты идут как раньше
static void z3HdrInit(Z3Hdr *h, unsigned size) { h->size = size; h->used = 0; h->seq = 0; h->magic = Z3_MAGIC; h->who[0] = 0; }
static void z3AreaSet(Z3Area *a, void *base, unsigned cap) {
  a->allocs = a->frees = a->fails = a->spills = a->spillB = a->resets = a->releases = a->leaks = 0;
  a->base = (unsigned char *)base; a->cap = cap & ~63u; a->seq = 0; a->usedB = a->peakB = 0;
  if (a->base) z3HdrInit((Z3Hdr *)a->base, a->cap);
}
static int z3AreaOwns(const Z3Area *a, const void *p) { return a->base && (const unsigned char *)p >= a->base && (const unsigned char *)p < a->base + a->cap; }
// склейка соседних свободных блоков (проход по всей цепочке: блоков десятки)
static void z3Coalesce(Z3Area *a) {
  for (unsigned o = 0; o < a->cap; ) { Z3Hdr *h = (Z3Hdr *)(a->base + o);
    if (!h->used) { unsigned e = o + h->size; while (e < a->cap && !((Z3Hdr *)(a->base + e))->used) { h->size += ((Z3Hdr *)(a->base + e))->size; e = o + h->size; } }
    o += h->size; }
}
// Шаг 7 Fable, служебная сборка «висячих указателей» (файл zone3scribble рядом с EBOOT или -DZONE3_SCRIBBLE=1): освобождённые блоки областей и сброшенная область заливаются 0xFF.
// Красный контроль (flag zone3dangle N / zone3DangleX): «канарейка» (64 Б узора 0x5A в области N), указатель на неё остаётся у проверки; когда блок возвращён и залит, проверка читает
// его через оставленный указатель и пишет WATCHDOG zone3 dangling — так сборка доказывает, что чужой указатель на сброшенную зону виден, а не молчит. Без заливки проверка молчит.
static unsigned char *z3Dangle; static int z3DangleHits, z3DangleArea; static void (*z3DangleLog)(const char *);
static void z3DangleCheck(const char *why) {
  if (!z3Dangle || !z3Scribble) return;
  int ff = 1; for (int i = 0; i < 16; i++) if (z3Dangle[i] != 0xFF) { ff = 0; break; }
  if (!ff) return;
  z3DangleHits++; char s[160]; snprintf(s, sizeof s, "WATCHDOG zone3 dangling area=%d read_through_stale_pointer=0xFF %s\n", z3DangleArea, why ? why : ""); Z3_LOGF(s); z3Dangle = 0;
}
static void z3Scrub(Z3Area *a, Z3Hdr *h) { if (z3Scribble) memset((unsigned char *)h + Z3_HDR, 0xFF, (z3Scribble == 2 && h->size - Z3_HDR > 4096u) ? 4096u : h->size - Z3_HDR); }   // 2 — только первые 4 КБ блока (дёшево по времени: отделяет тайминг заливки от висячего указателя)
// блок из области: наименьший подходящий свободный; 0 — места нет
static void *z3AreaAlloc(Z3Area *a, const char *who, unsigned sz) {
  if (!a->base) return 0;
  unsigned need = ((sz + 63u) & ~63u) + Z3_HDR; if (!sz) need = Z3_HDR + 64u;
  Z3_LOCK();
  Z3Hdr *best = 0;
  for (unsigned o = 0; o < a->cap; ) { Z3Hdr *h = (Z3Hdr *)(a->base + o);
    if (!h->used && h->size >= need && (!best || h->size < best->size)) best = h;
    o += h->size; }
  void *r = 0;
  if (best) {
    if (best->size - need >= Z3_HDR + 64u) { Z3Hdr *n = (Z3Hdr *)((unsigned char *)best + need); z3HdrInit(n, best->size - need); best->size = need; }
    best->used = 1; best->seq = ++a->seq; snprintf(best->who, sizeof best->who, "%s", who ? who : "?");
    a->usedB += best->size; if (a->usedB > a->peakB) a->peakB = a->usedB; a->allocs++;
    r = (unsigned char *)best + Z3_HDR;
  } else a->fails++;
  Z3_UNLOCK(); return r;
}
// 1 — блок был из области и возвращён; 0 — чужой
static int z3AreaFree(Z3Area *a, void *p) {
  if (!z3AreaOwns(a, p)) return 0;
  Z3_LOCK();
  Z3Hdr *h = (Z3Hdr *)((unsigned char *)p - Z3_HDR);
  if (h->magic == Z3_MAGIC && h->used) { h->used = 0; a->usedB -= h->size; a->frees++; z3Scrub(a, h); z3DangleCheck("free"); z3Coalesce(a); }
  Z3_UNLOCK(); return 1;
}
static unsigned z3AreaMark(const Z3Area *a) { return a->seq; }
// сброс до отметки: все блоки с seq больше отметки свободны; возвращает, сколько блоков снято
static int z3AreaRelease(Z3Area *a, unsigned mark) {
  if (!a->base) return 0;
  int n = 0; Z3_LOCK();
  for (unsigned o = 0; o < a->cap; ) { Z3Hdr *h = (Z3Hdr *)(a->base + o);
    if (h->used && h->seq > mark) { h->used = 0; a->usedB -= h->size; a->frees++; z3Scrub(a, h); n++; }
    o += h->size; }
  z3DangleCheck("release"); z3Coalesce(a); a->releases++; Z3_UNLOCK(); return n;
}
// стирание всей области (конец или смена миссии, смена уровня); возвращает, сколько живых блоков было (утечки владельцев)
static int z3AreaReset(Z3Area *a) {
  if (!a->base) return 0;
  int n = 0; Z3_LOCK();
  for (unsigned o = 0; o < a->cap; ) { Z3Hdr *h = (Z3Hdr *)(a->base + o); if (h->used) n++; o += h->size; }
  if (z3Scribble) memset(a->base + Z3_HDR, 0xFF, (z3Scribble == 2) ? 4096u : a->cap - Z3_HDR);
  z3DangleCheck("reset");
  z3HdrInit((Z3Hdr *)a->base, a->cap); a->usedB = 0; a->seq = 0; a->resets++; a->leaks += (unsigned)n;
  Z3_UNLOCK(); return n;
}
static void z3AreaStat(Z3Area *a, unsigned *biggestHole, unsigned *freeTot, unsigned *nlive) {
  unsigned mf = 0, ft = 0, nl = 0;
  for (unsigned o = 0; a->base && o < a->cap; ) { Z3Hdr *h = (Z3Hdr *)(a->base + o);
    if (h->used) nl++; else { ft += h->size; if (h->size > mf) mf = h->size; }
    o += h->size; }
  if (biggestHole) *biggestHole = mf > Z3_HDR ? mf - Z3_HDR : 0; if (freeTot) *freeTot = ft; if (nlive) *nlive = nl;
}
// проверка целостности цепочки (хост-тест и сторож): 0 — цепочка цела, иначе смещение первого испорченного заголовка + 1
static unsigned z3AreaVerify(const Z3Area *a) {
  unsigned o = 0, used = 0;
  while (a->base && o < a->cap) { const Z3Hdr *h = (const Z3Hdr *)(a->base + o);
    if (h->magic != Z3_MAGIC || h->size < Z3_HDR + 64u || (h->size & 63u) || o + h->size > a->cap) return o + 1u;
    if (h->used) used += h->size;
    o += h->size; }
  return a->base && (o != a->cap || used != a->usedB) ? o + 1u : 0;
}

// ---- слой клиентов ----
// Ступени области (z3Reclaim, ставит движок): step 1.. — что отдать, чтобы в области появился кусок need; возвращает 1, если что-то освобождено. Для «места» это
// сброс кэша «миссии» и тёплых пакетов, для «миссии» — кэш картинок и предчтение машин. 0 — ступеней нет.
static int (*z3Reclaim)(int area, const char *who, unsigned need, int step);
static unsigned z3Want[Z3_NAREA];   // сколько область просит (КБ<<10); отцепленная область возвращает память куче, пока уровню тесно
#define Z3_STEPS 3
static unsigned z3SpillTotal;
#ifndef Z3_ISMAIN
#define Z3_ISMAIN() 1   // движок подставляет проверку «зовёт главный поток»: ступени области трогают чужие дескрипторы и кэши, из потока чтения их звать нельзя
#endif
static void *zone3Alloc(int area, const char *who, unsigned sz, unsigned al) {
  if (!z3On || ZONE3_OLD) return memalign(al, sz ? sz : 16);
  Z3Area *a = &z3A[area];
  if (!a->base) return memalign(al, sz ? sz : 16);   // область отцеплена на этот уровень (zone3Shrink): куча, без шума
  void *p = z3AreaAlloc(a, who, sz);
  for (int st = 1; !p && st <= Z3_STEPS && z3Reclaim && Z3_ISMAIN(); st++) if (z3Reclaim(area, who, sz, st)) p = z3AreaAlloc(a, who, sz);
  if (p) return p;
  p = memalign(al, sz ? sz : 16);   // пролив: область тесна — блок из кучи, как раньше, но дыра видна журналу и тесту
  a->spills++; a->spillB += sz; z3SpillTotal++;
  { char s[160]; unsigned mf, ft; z3AreaStat(a, &mf, &ft, 0);
    snprintf(s, sizeof s, "WATCHDOG mem who=%s zone=%s size=%u step=%d areafree=%u/%u %s\n", who ? who : "?", a->name, sz, Z3_STEPS, mf, ft, p ? "spill" : "fail"); Z3_LOGF(s); }
  return p;
}
static void zone3Free(void *p) {
  if (!p) return;
  if (z3On) for (int i = 0; i < Z3_NAREA; i++) if (z3AreaFree(&z3A[i], p)) return;
  free(p);
}
static int zone3Owns(const void *p) { return z3On && (z3AreaOwns(&z3A[0], p) || z3AreaOwns(&z3A[1], p)); }
static unsigned zone3Mark(int area) { return z3On ? z3AreaMark(&z3A[area]) : 0; }
static int zone3Release(int area, unsigned mark) { return z3On ? z3AreaRelease(&z3A[area], mark) : 0; }
// Build 95 шаг 9в (находка Опоры по 95c, d3f9f540): области - ОДИН регион сразу над Z0, до блока уровня ([Z0][области][блок уровня][хвост]). Размеры выбираются один раз при старте.
// Уступка уровню - только сжатием с ВЕРХНЕГО края региона: отцепляется область с наибольшим адресом (занята - отцепка стоит, середину не трогаем), освобождённое прилегает к месту блока уровня
// и сливается с ним. Прицепка - обратным ростом: области возвращаются снизу вверх на ТОТ ЖЕ адрес (память с другого адреса возвращается куче, прицепка откладывается). Посреди кучи - никогда.
// ZONE3_REGION_OLD=1 (сборка) или файл `zone3regold` - прежняя посадка: город-место-миссия по одной, отцепка в любом порядке.
#ifndef ZONE3_SHIFT_OLD
#define ZONE3_SHIFT_OLD 0   // 1 - прицепка только на точный домашний адрес (иначе промах), без сдвига вниз до 1 МБ
#endif
#ifndef ZONE3_REGION_OLD
#define ZONE3_REGION_OLD 0
#endif
#ifndef Z3_MA
#define Z3_MA(al, n) memalign((al), (n))   // хост-тест подставляет модель кучи с адресами по возрастанию
#define Z3_FR(p) free(p)
#endif
static int z3RegionOld = ZONE3_REGION_OLD;
static unsigned char *z3Home[Z3_NAREA];   // адрес области при посадке: прицепка возвращает её ровно туда
static unsigned z3RegMiss, z3MissGot, z3Shift;                 // прицепок отложено: память пришла не на «домашний» адрес
static int z3Attach(int area, unsigned bytes) {
  Z3Area *a = &z3A[area]; if (a->base || !bytes) return a->base != 0;
  bytes = (bytes + 63u) & ~63u; unsigned char *b = (unsigned char *)Z3_MA(64, bytes);
  if (!b) return 0;
  if (!z3RegionOld && z3Home[area] && b != z3Home[area]) {
    // ниже домашнего адреса освободился постоянный блок (кожа игрока 374 КБ под регионом, тур Публичного 13:54): свободный кусок начался раньше и область села на его начало. Это тот же регион, чуть ниже (до 1 МБ), щель
    // между областью и верхней соседкой мелкая и годна для мелких блоков; середину кучи это не занимает. Выше дома или дальше 1 МБ - по-прежнему промах
    if (!ZONE3_SHIFT_OLD && b < z3Home[area] && (unsigned)(z3Home[area] - b) <= (1u << 20)) { z3Shift++; z3Home[area] = b; }
    else { z3RegMiss++; z3MissGot = (unsigned)(unsigned long)b; Z3_FR(b); return 0; } }   // не на своё место: середину кучи не занимаем
  z3Home[area] = b; z3AreaSet(a, b, bytes); z3On = 1; return 1;
}
static unsigned z3Detach(int area) {   // сколько байт отдано куче; 0 — область не прицеплена или в ней есть живые блоки
  Z3Area *a = &z3A[area]; if (!a->base || a->usedB) return 0;
  unsigned char *b = a->base; unsigned cap = a->cap; a->base = 0; a->cap = 0; a->seq = 0;   // сперва забыть, потом free: иначе __wrap_free примет начало области за блок области
  Z3_FR(b); return cap;
}
static unsigned z3DetachTop(int *which) {   // верхняя область региона; занята - 0 (вниз не перепрыгиваем)
  int top = -1; for (int i = 0; i < Z3_NAREA; i++) if (z3A[i].base && (top < 0 || z3A[i].base > z3A[top].base)) top = i;
  if (top < 0) return 0; if (which) *which = top; return z3Detach(top);
}
// «Ужатие вместо отцепки» (шаг 9в-3, приёмка Будки по L4 на ballast 5 = куче PSP): на тесном уровне «место» сначала сжимается до z3Tight (2 МБ: комната 0,86 МБ и пакеты помещаются, ролик и пакет будки идут через кучу), и только потом отцепляется.
// Сжатие - отцепка и посадка меньшей области на тот же домашний адрес (область пуста), освобождается верхняя половина; рост обратно - то же в другую сторону. z3Tight[area] = 0 - без ужатия
#ifndef ZONE3_TIGHT_KB
#define ZONE3_TIGHT_KB 2048u
#endif
static unsigned z3Tight[Z3_NAREA] = { 0, ZONE3_TIGHT_KB << 10, 0 };
static int z3FloorOn = 1;   // пол ужатой области (на L1 выключен: мисс-арт старта L1 нужен куче, а комнаты в начале уровня не нужны - ballast 5, цепочка: отказ 679 КБ на t=1 с)
static unsigned z3Resize(int area, unsigned bytes) {   // новый размер пустой области на том же адресе; возвращает прежний размер при неудаче 0, иначе новый cap
  Z3Area *a = &z3A[area]; if (!a->base || a->usedB) return 0;
  unsigned old = a->cap; unsigned c = z3Detach(area); if (!c) return 0;
  if (z3Attach(area, bytes)) return a->cap;
  z3Attach(area, old); return 0;   // не легла - вернуть прежнюю (адрес только что освобождён)
}
static void z3LogShrink(const char *kind, int area, unsigned kb, const char *why);
static int z3EnsureFloor(const char *why) {   // с L2 «место» прицеплено всегда, хотя бы ужатым: комнаты и входы без него идут в тесную кучу
  Z3Area *a = &z3A[Z3_PLACE]; if (!z3FloorOn || !z3Tight[Z3_PLACE] || a->base || !z3Want[Z3_PLACE] || z3RegionOld || ZONE3_OLD) return 0;
  if (!z3Attach(Z3_PLACE, z3Tight[Z3_PLACE])) return 0;
  z3LogShrink("floor-attach", Z3_PLACE, z3Tight[Z3_PLACE] >> 10, why); return 1;
}
static unsigned z3ShrinkTop(int *which, int *small) {
  int top = -1; for (int i = 0; i < Z3_NAREA; i++) if (z3A[i].base && (top < 0 || z3A[i].base > z3A[top].base)) top = i;
  if (top < 0) return 0; if (which) *which = top; if (small) *small = 0;
  Z3Area *a = &z3A[top];
  if (z3Tight[top] && a->cap > z3Tight[top] && !a->usedB) { unsigned oc = a->cap, nc = z3Resize(top, z3Tight[top]); if (nc && nc < oc) { if (small) *small = 1; return oc - nc; } }
  if (z3Tight[top] && a->cap <= z3Tight[top] && z3FloorOn) return 0;   // пол: ужатая область не отцепляется совсем (комнатам и входам нужна всегда; приёмка L4 на ballast 5)
  return z3Detach(top);
}
static int zone3Init(unsigned missionB, unsigned placeB, unsigned cityB, unsigned budget) {   // budget - сколько байт областей можно взять при старте (регион: снизу вверх, пока влезает; остальные прицепит zone3Regrow)
  if (z3On || ZONE3_OLD) return z3On;
  z3Want[Z3_MISSION] = missionB; z3Want[Z3_PLACE] = placeB; z3Want[Z3_CITY] = cityB;
  if (z3RegionOld) { z3Attach(Z3_CITY, cityB); z3Attach(Z3_PLACE, placeB); z3Attach(Z3_MISSION, missionB); }   // как было: город — первым
  else {   // регион: адреса по возрастанию, город сверху - он уступает первым; берём, пока влезает в бюджет старта (первый уровень должен грузиться без отказов), остальное прицепит zone3Regrow
    const int ord[3] = { Z3_PLACE, Z3_MISSION, Z3_CITY };   // «место» снизу: уступает последним (комнаты, будка, ролик без него идут в тесную кучу - приёмка Будки: «место» прицеплено при входе в Kwik и дом на L4)
    for (int i = 0; i < 3; i++) { unsigned b = z3Want[ord[i]]; if (!b) continue; if (b > budget) break; if (z3Attach(ord[i], b)) budget -= (b + 63u) & ~63u; else break; }
  }
  if (!z3On) z3Cap0 = missionB + placeB + cityB;
  return z3On;
}
static void z3LogShrink(const char *kind, int area, unsigned kb, const char *why) {
  char s[160]; snprintf(s, sizeof s, "WATCHDOG zone3 %s area=%s kb=%u %s\n", kind, z3A[area].name, kb, why ? why : ""); Z3_LOGF(s);
}
static unsigned zone3Shrink(unsigned want, const char *why) {
  unsigned freed = 0; static const int order[3] = { Z3_CITY, Z3_PLACE, Z3_MISSION };
  if (z3RegionOld) { for (int i = 0; i < 3 && freed < want; i++) { unsigned c = z3Detach(order[i]); if (c) { freed += c; z3LogShrink("shrink", order[i], c >> 10, why); } } }
  else while (freed < want) { int w = 0, sm = 0; unsigned c = z3ShrinkTop(&w, &sm); if (!c) break; freed += c; z3LogShrink(sm ? "shrink-small" : "shrink", w, c >> 10, why); }   // с верхнего края региона, середину не трогаем
  if (freed) { int any = 0; for (int i = 0; i < Z3_NAREA; i++) if (z3A[i].base) any = 1; if (!any) { z3On = 0; z3LogShrink("off", Z3_CITY, 0, why); } }
  return freed;
}
static int zone3Regrow(unsigned heapFree, unsigned need, unsigned ring, const char *why) {   // need — уровню вне клеток; ring — кольцо клеток старта: его держит город, а без города — куча
  int n = 0; int order[3] = { Z3_MISSION, Z3_PLACE, Z3_CITY };
  if (!z3RegionOld) for (int i = 1; i < 3; i++) for (int j = i; j > 0; j--) { unsigned char *x = z3Home[order[j - 1]], *y = z3Home[order[j]]; if (y && (!x || y < x)) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; } }   // снизу вверх по домашним адресам
  for (int i = 0; i < 3; i++) { int a = order[i];
    if (!z3RegionOld && z3A[a].base && z3Want[a] > z3A[a].cap && !ZONE3_OLD) {   // ужатая область растёт обратно
      unsigned oc = z3A[a].cap, dl = z3Want[a] - oc;
      if (heapFree < dl + need + (a == Z3_CITY || z3A[Z3_CITY].base ? 0u : ring)) break;
      unsigned nc = z3Resize(a, z3Want[a]); if (nc > oc) { heapFree -= nc - oc; n++; z3LogShrink("regrow-small", a, (nc - oc) >> 10, why); } else break;
      continue; }
    if (z3A[a].base || !z3Want[a] || ZONE3_OLD) continue;
    if (heapFree < z3Want[a] + need + (a == Z3_CITY || z3A[Z3_CITY].base ? 0u : ring)) { if (z3RegionOld) continue; break; }   // регион растёт строго снизу вверх: не влезла нижняя - верхние не пробуем
    if (z3Attach(a, z3Want[a])) { heapFree -= z3Want[a]; n++; z3LogShrink("regrow", a, z3Want[a] >> 10, why); }
    else { if (!z3RegionOld) { char w_[200]; snprintf(w_, sizeof w_, "%s home=%x got=%x", why ? why : "", (unsigned)(unsigned long)z3Home[a], z3MissGot); z3LogShrink("regrow-miss", a, z3Want[a] >> 10, w_); } if (!z3RegionOld) break; } }
  return n;
}
// строка журнала: свободное в областях до входа — то, что проверяет приёмка («ZONE3 place free ≥ 2,5 МБ перед каждым входом»)
static void zone3StatLine(char *o, size_t n, float t, const char *why) {
  if (why && !strcmp(why, "room_in")) { Z3Area *a_ = &z3A[Z3_PLACE]; unsigned tot_ = 0;   // 99: что лежит в «месте» при входе в комнату (разбор Обсерватории L3)
    for (unsigned o_ = 0; a_->base && o_ < a_->cap; ) { Z3Hdr *h_ = (Z3Hdr *)(a_->base + o_); if (h_->used) { tot_ += h_->size; if (h_->size >= (16u << 10)) { char l_[100]; snprintf(l_, sizeof l_, "Z3BLK place who=%s kb=%u off=%u\n", h_->who, h_->size >> 10, o_ >> 10); Z3_LOGF(l_); } } o_ += h_->size; }
    { char l_[100]; snprintf(l_, sizeof l_, "Z3BLK place total_used_kb=%u cap_kb=%u\n", tot_ >> 10, a_->cap >> 10); Z3_LOGF(l_); } }
  unsigned mf[Z3_NAREA] = {0}, ft[Z3_NAREA] = {0}, nl[Z3_NAREA] = {0};
  for (int i = 0; i < Z3_NAREA; i++) z3AreaStat(&z3A[i], &mf[i], &ft[i], &nl[i]);
  snprintf(o, n, "ZONE3 t=%.1f %s on=%d mission free=%.2f/%.2f live=%u used_kb=%u peak_kb=%u fail=%u spill=%u place free=%.2f/%.2f live=%u used_kb=%u peak_kb=%u fail=%u spill=%u city free=%.2f/%.2f live=%u used_kb=%u peak_kb=%u fail=%u resets=%u leaks=%u\n",
           t, why ? why : "", z3On, mf[0] / 1048576.f, ft[0] / 1048576.f, nl[0], z3A[0].usedB >> 10, z3A[0].peakB >> 10, z3A[0].fails, z3A[0].spills,
           mf[1] / 1048576.f, ft[1] / 1048576.f, nl[1], z3A[1].usedB >> 10, z3A[1].peakB >> 10, z3A[1].fails, z3A[1].spills,
           mf[2] / 1048576.f, ft[2] / 1048576.f, nl[2], z3A[2].usedB >> 10, z3A[2].peakB >> 10, z3A[2].fails, z3A[0].resets + z3A[1].resets, z3A[0].leaks + z3A[1].leaks);
}
#endif
