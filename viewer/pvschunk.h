// №1068 (второй тур TEST78 на PSP, 29.10): пул PVS1 кусками. На L1/L4 участок lva занят резидентной частью, COL1 и PRP2 (17,86 из 17,96 МБ), и PVS1
// (2,2 / 2,6 МБ) шёл в кучу одним куском; после нескольких смен уровня наибольший кусок кучи 2,6 МБ при 7–20 МБ свободных — отказ памяти, уровень без PVS.
// Списки клеток лежат в пуле подряд и не пересекаются (все 7 уровней, проверка tests/pvschunk/host.c), поэтому пул режется по началу списка на куски
// ≤ PVSCH_U16 номеров (256 КБ), каждый список целиком в одном куске. Чистый C99 без PSP-заголовков: проверка tests/pvschunk/host.c.
#ifndef SHAR_PVSCHUNK_H
#define SHAR_PVSCHUNK_H
#define PVSCH_MAX 16
#define PVSCH_U16 (128u << 10)   // номеров u16 на кусок: 256 КБ
// План разреза пула npool номеров по таблице ent ({u32 start, u32 n} на клетку, n 0xFFFFFFFF — клетки нет): b[0] = 0 < b[1] < … < b[k] = npool,
// разрез только в начале списка. Возвращает k (1…PVSCH_MAX) или 0 — резать нельзя (список длиннее куска, список за концом пула, кусков больше PVSCH_MAX)
static int pvsChPlan(const unsigned *ent, unsigned nent, unsigned npool, unsigned chU16, unsigned *b) {
  if (!npool || !chU16) return 0;
  unsigned ch = chU16; if ((npool + ch - 1) / ch > PVSCH_MAX) ch = (npool + PVSCH_MAX - 1) / PVSCH_MAX;
  int k = 0; b[0] = 0;
  while (b[k] < npool) {
    if (k >= PVSCH_MAX) return 0;
    unsigned P = b[k] + ch;
    if (P >= npool || P < b[k]) { b[++k] = npool; break; }
    unsigned best = b[k];   // начало последнего списка, который начинается не дальше P: разрез перед ним
    for (unsigned i = 0; i < nent; i++) { unsigned s = ent[2 * i], n = ent[2 * i + 1]; if (!n || n == 0xFFFFFFFFu) continue; if (s <= P && s > best) best = s; }
    if (best == b[k]) return 0;
    b[++k] = best;
  }
  for (unsigned i = 0; i < nent; i++) {   // каждый список внутри пула и целиком внутри одного куска
    unsigned s = ent[2 * i], n = ent[2 * i + 1]; if (!n || n == 0xFFFFFFFFu) continue;
    if (s >= npool || n > npool - s) return 0;
    int j = 0; while (j + 1 < k && s >= b[j + 1]) j++;
    if (s + n > b[j + 1]) return 0;
  }
  return k;
}
// Адрес номера s пула, разложенного на k кусков p[j] = номера [b[j], b[j+1]). s == npool (пустой список в конце) — конец последнего куска
static const unsigned short *pvsChAt(unsigned short *const *p, const unsigned *b, int k, unsigned s) {
  int j = 0; while (j + 1 < k && s >= b[j + 1]) j++;
  return p[j] + (s - b[j]);
}
#endif
