// №1195 (НАДЁЖНОСТЬ §4, риск 8): таблицы level.shr (куски по 80 байт, клетки по 48) читались без проверки: индекс вне таблицы или смещение за блобом давали запись
// по чужому адресу при битой или чужой копии файла. Проверка один раз при загрузке: числа внутри допустимого, диапазоны в пределах файла и таблиц; при порче — честный экран «level.shr is damaged».
// Чистая функция (без PSP): тест tests/g01cars/lvcheck.sh прогоняет её на настоящих level.shr всех семи уровней (ложных срабатываний нет) и на подменённых таблицах.
// LVCHECK1195_OLD=1 — как в TEST89 (проверки нет); файл lvcheckold рядом с EBOOT — то же на ходу.
#ifndef LVCHECK_H
#define LVCHECK_H
#include <string.h>
#ifndef LVCHECK1195_OLD
#define LVCHECK1195_OLD 0
#endif
static int lvCheckOld = LVCHECK1195_OLD;
static inline unsigned lvcU(const unsigned char *p) { unsigned v; memcpy(&v, p, 4); return v; }
static inline int lvcI(const unsigned char *p) { int v; memcpy(&v, p, 4); return v; }
// hdr — заголовок 104 байта; tab — npc записей по 80; cellTab — ncell записей по 48; fileSize — размер level.shr. 0 — цело, иначе код ошибки; *why — короткое имя причины
static inline int lvCheckTables(const unsigned char *hdr, const unsigned char *tab, const unsigned char *cellTab, unsigned fileSize, const char **why) {
  static const char *w0 = "";
  if (!why) why = &w0;
  if (lvCheckOld) { *why = ""; return 0; }
  unsigned ntex = lvcU(hdr + 4), npc = lvcU(hdr + 8), tableOff = lvcU(hdr + 76), ncell = lvcU(hdr + 80), cellOff = lvcU(hdr + 84);
  if (ntex > 4096u) { *why = "ntex"; return 1; }
  if (npc > (1u << 20)) { *why = "npc"; return 2; }
  if (ncell > 65534u) { *why = "ncell"; return 3; }
  if (tableOff < 104u || (unsigned long long)tableOff + 80ull * npc > cellOff) { *why = "table offsets"; return 4; }
  if ((unsigned long long)cellOff + 48ull * ncell > fileSize) { *why = "cell table beyond file"; return 5; }
  for (unsigned i = 0; i < ncell; i++) {   // клетка: диапазон кусков в таблице, блоб внутри файла
    const unsigned char *c = cellTab + 48u * i; unsigned first = lvcU(c + 24), n = lvcU(c + 28), off = lvcU(c + 36), size = lvcU(c + 40);
    if ((unsigned long long)first + n > npc) { *why = "cell piece range"; return 6; }
    if (off > fileSize || size > fileSize - off) { *why = "cell blob beyond file"; return 7; }
    if (n && !size) { *why = "cell empty blob"; return 8; }
    for (unsigned k = first; k < first + n; k++) {   // кусок клетки: смещение и размер внутри блоба клетки
      const unsigned char *r = tab + 80u * k; int flags = lvcI(r + 28), nv = lvcI(r + 32), ni = lvcI(r + 36); unsigned off2 = lvcU(r + 60), nf = lvcU(r + 64);
      unsigned vs = (flags & 32) ? 12u : 16u; unsigned long long need;
      if (nv < 0 || ni < 0 || nf > (1u << 24) || (unsigned)ni > (1u << 24)) { *why = "piece counts"; return 9; }
      if (flags & (1 << 26)) need = 16;                                  // полосы: заголовок; остальное — в разборе клетки
      else if (flags & 16) need = ((unsigned long long)ni + nf) * vs;    // без индексов: ближняя и дальняя версии подряд
      else need = (((unsigned long long)nv * 16u + 15u) & ~15ull) + 2ull * (unsigned)ni;
      if (off2 > size || need > size - off2) { *why = "piece beyond cell blob"; return 10; }
    }
  }
  for (unsigned k = 0; k < npc; k++) {   // кусок: текстура и клетка существуют
    const unsigned char *r = tab + 80u * k; int tex = lvcI(r + 24); unsigned cell = lvcU(r + 68);
    if (tex < -1 || tex >= (int)ntex) { *why = "piece texture index"; return 11; }
    if (cell >= ncell && cell < 0xFFFFu) { *why = "piece cell index"; return 12; }
  }
  *why = ""; return 0;
}
// заголовок msnart.shr (MSA2: h[1] — число строк, h[2] — смещение таблицы строк по rowSize байт): таблица целиком внутри файла
static inline int lvMsaHdrOk(const unsigned h[4], long fsz, unsigned rowSize) {
  if (lvCheckOld) return 1;
  return !(h[1] > (1u << 16) || fsz < 0 || h[2] < 16u || (unsigned long long)h[2] + (unsigned long long)rowSize * h[1] > (unsigned long)fsz);
}
#endif
