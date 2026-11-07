// TEST90 MEMMAP (запрос дирижёра: «кто держит кучу» по игре Роба на PSP: 0,44 МБ наибольшего куска при 5–6 МБ свободных): таблица ЖИВЫХ выделений (адрес, размер, адрес вызывающего), раз в 30 с строки в чёрный ящик и журнал:
// число и килобайты по классам размера и пять-шесть самых тяжёлых групп по адресу вызывающего (владельцев — уровень, клетки, NPC, миссия/art, комнаты, звук, UI — по этим адресам называет tools/memmap_report.py по ELF сборки).
// Без выделений: таблица и буферы статические (≈ 17 КБ: таблица 2048 × 8 Б только для блоков ≥ 16 КБ — они и дробят кучу — плюс счётчики классов по ВСЕМ блокам), замер только читает их. Мелкие блоки (< 16 КБ) — суммой по классам, без владельцев. Чистая арифметика без PSP (вставка/удаление/отчёт): тест tests/memmap. MEMMAP_OLD=1 / файл memmapold — без учёта (обёртки только считают отказы, как в TEST89)
#ifndef MEMMAP_H
#define MEMMAP_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifndef MM_N
#define MM_N 2048u               // 11 бит хэша; в таблице только блоки ≥ MM_MIN (на L1 ≈ 500 живых): загрузка ≈ 0,25; переполнение считается (mmOver), а не теряется молча
#endif
#ifndef MM_MIN
#define MM_MIN 16384u            // блок меньше — только в счётчиках классов (владельца не помним)
#endif
#ifndef MM_BASE
#define MM_BASE 0x08800000u      // начало пользовательской памяти PSP
#endif
#define MM_LOG2N 11
typedef struct { uint32_t a, b; } MmE;   // a: (адрес-БАЗА)>>4 + 1 (22 бита) | размер/16 младшие 10 бит << 22; b: (ra-БАЗА)>>2 (23 бита) | размер/16 старшие 9 бит << 23; a == 0 пусто, 0xFFFFFFFF удалено
static MmE mmTab[MM_N]; static unsigned mmLive, mmOver, mmClamp, mmAdds, mmDels;
static inline unsigned mmHash(uint32_t pi) { return (pi * 2654435761u) >> (32 - MM_LOG2N); }
#define MM_NCLS 8
static const unsigned mmClsMax[MM_NCLS] = {256u, 1024u, 4096u, 16384u, 65536u, 262144u, 1048576u, 0xFFFFFFFFu};   // ≤256 Б, ≤1 К, ≤4 К, ≤16 К, ≤64 К, ≤256 К, ≤1 М, больше
static const char *const mmClsName[MM_NCLS] = {"256", "1K", "4K", "16K", "64K", "256K", "1M", "big"};
static unsigned mmCCnt[MM_NCLS], mmCByt[MM_NCLS];   // живые блоки и их байты по классам — по ВСЕМ блокам (размер — malloc_usable_size, на добавлении и удалении один и тот же)
static inline int mmCls(size_t n) { int c = 0; while (c < MM_NCLS - 1 && n > mmClsMax[c]) c++; return c; }
static inline void mmAdd(const void *p, size_t n, uint32_t ra) {
  if (!p) return; int c = mmCls(n); mmCCnt[c]++; mmCByt[c] += (unsigned)n; mmAdds++;
  if (n < MM_MIN) return;
  uint32_t off = (uint32_t)(uintptr_t)p - MM_BASE; if (off >= (1u << 26)) return;
  uint32_t pi = (off >> 4) + 1u, su = (uint32_t)((n + 15u) >> 4); if (su > 0x7FFFFu) { su = 0x7FFFFu; mmClamp++; }   // блок больше 8 МБ (участок уровня) учитывается как 8 МБ
  uint32_t ri = ((ra - MM_BASE) >> 2) & 0x7FFFFFu;
  unsigned h = mmHash(pi);
  for (unsigned k = 0; k < 64; k++) { unsigned i = (h + k) & (MM_N - 1u); if (mmTab[i].a == 0 || mmTab[i].a == 0xFFFFFFFFu) { mmTab[i].a = pi | ((su & 1023u) << 22); mmTab[i].b = ri | ((su >> 10) << 23); mmLive++; return; } }
  mmOver++;
}
static inline void mmDel(const void *p, size_t n) {
  if (!p) return;
#ifdef MM_NODEL
  return;   // ТОЛЬКО красный контроль теста: удаления не учитываются
#endif
  int c = mmCls(n); if (mmCCnt[c]) { mmCCnt[c]--; mmCByt[c] -= (mmCByt[c] >= (unsigned)n ? (unsigned)n : mmCByt[c]); } mmDels++;
  if (n < MM_MIN) return;
  uint32_t off = (uint32_t)(uintptr_t)p - MM_BASE; if (off >= (1u << 26)) return;
  uint32_t pi = (off >> 4) + 1u; unsigned h = mmHash(pi);
  for (unsigned k = 0; k < 64; k++) { unsigned i = (h + k) & (MM_N - 1u); uint32_t a = mmTab[i].a; if (a == 0) return; if (a != 0xFFFFFFFFu && (a & 0x3FFFFFu) == pi) { mmTab[i].a = 0xFFFFFFFFu; if (mmLive) mmLive--; return; } }
}
#define MM_NG 48
static struct { uint32_t ra; unsigned n, kb; } mmG[MM_NG]; static unsigned mmNG, mmTabN, mmTabKB;   // группы по вызывающему (блоки ≥ MM_MIN), число и КБ блоков в таблице
// Шаг 1 отчёта: обход таблицы в группы (2048 записей — десятки микросекунд). Группы сверх MM_NG не показываются, но в mmTabN/mmTabKB учтены
static inline void mmScan(void) {
  mmNG = 0; mmTabN = 0; unsigned kb = 0;
  for (unsigned i = 0; i < MM_N; i++) { uint32_t a = mmTab[i].a; if (a == 0 || a == 0xFFFFFFFFu) continue;
    unsigned su = ((a >> 22) & 1023u) | ((mmTab[i].b >> 23) << 10), sz = su << 4, ra = ((mmTab[i].b & 0x7FFFFFu) << 2) + MM_BASE;
    mmTabN++; kb += sz;
    unsigned g = 0; while (g < mmNG && mmG[g].ra != ra) g++;
    if (g == mmNG) { if (mmNG >= MM_NG) continue; mmG[g].ra = ra; mmG[g].n = 0; mmG[g].kb = 0; mmNG++; }
    mmG[g].n++; mmG[g].kb += sz; }
  mmTabKB = (kb + 512u) >> 10;
  for (unsigned g = 0; g < mmNG; g++) mmG[g].kb = (mmG[g].kb + 512u) >> 10;
}
// Шаг 2 отчёта: строки в buf (cap байт), до трёх, каждая не длиннее ~230 байт; возвращает длину. Классы — счётчики по всем блокам, группы — по блокам ≥ 16 КБ
static inline unsigned mmFormat(char *buf, unsigned cap, float t, unsigned freeKB, unsigned blkKB, unsigned costUs, unsigned maxStepUs, uint32_t ref) {
  unsigned total = 0, totalKB = 0; for (int c = 0; c < MM_NCLS; c++) { total += mmCCnt[c]; totalKB += mmCByt[c]; }
  unsigned o = 0, w;
  w = (unsigned)snprintf(buf + o, cap - o, "MEMMAP t=%.0f free=%u blk=%u live=%u kb=%u tab=%u/%u over=%u clamp=%u cost=%uus step=%uus ref=%08x |", t, freeKB, blkKB, total, (totalKB + 512u) >> 10, mmTabN, mmTabKB, mmOver, mmClamp, costUs, maxStepUs, ref); o += w < cap - o ? w : 0;
  for (int c = 0; c < MM_NCLS; c++) { w = (unsigned)snprintf(buf + o, cap - o, " %s:%u/%u", mmClsName[c], mmCCnt[c], (mmCByt[c] + 512u) >> 10); o += w < cap - o ? w : 0; }
  w = (unsigned)snprintf(buf + o, cap - o, "\n"); o += w < cap - o ? w : 0;
  for (int line = 0; line < 2; line++) {   // 2 строки по 3 самые тяжёлые группы
    w = (unsigned)snprintf(buf + o, cap - o, "MEMMAPRA"); o += w < cap - o ? w : 0;
    for (int k = 0; k < 3; k++) { unsigned bi = 0xFFFFFFFFu; for (unsigned g = 0; g < mmNG; g++) if (mmG[g].n && (bi == 0xFFFFFFFFu || mmG[g].kb > mmG[bi].kb)) bi = g;
      if (bi == 0xFFFFFFFFu) break; w = (unsigned)snprintf(buf + o, cap - o, " ra=%08x n=%u kb=%u", mmG[bi].ra, mmG[bi].n, mmG[bi].kb); o += w < cap - o ? w : 0; mmG[bi].n = 0; }
    w = (unsigned)snprintf(buf + o, cap - o, "\n"); o += w < cap - o ? w : 0; }
  return o;
}
static inline unsigned mmReport(char *buf, unsigned cap, float t, unsigned freeKB, unsigned blkKB, unsigned costUs, uint32_t ref) { mmScan(); return mmFormat(buf, cap, t, freeKB, blkKB, costUs, 0, ref); }
#endif
