// Коллизия-96: COL1 кольцом (страницы треугольников и тайлы ссылок клеток подгружаются за фокусом). Формат COLR2 — tools/col_ring_build.py.
// Включается только при COLRING_OLD=0 (col.h); по умолчанию прежнее поведение, этот модуль в сборку не входит.
// Закон Роба (принцип доводки п.3, п.11): запрос НИКОГДА не отвечает «пусто» из-за того, что кольцо не угадало — недостающее дочитывается тут же
// (синхронная страховка), строка WATCHDOG colring sync. Номера примитивов не меняются; ящики, капсулы, заборы резидентны.
#ifndef SHAR_COLRING_H
#define SHAR_COLRING_H
#include <stdint.h>
#include "col.h"

#define CRING_TP 256u          // треугольников на страницу (12288 Б)
#define CRING_PAGEB (CRING_TP * 48u)
#define CRING_TL 16u           // клеток в стороне тайла

typedef struct {
  int (*read)(void *user, unsigned off, void *dst, unsigned n);   // 0 — прочитано; нельзя звать из других потоков
  void *user;
  void (*log)(const char *line);                                  // строки WATCHDOG/COLRING (может быть NULL)
} ColRingIO;

typedef struct { uint32_t off, bytes, nrefs, pgOff, npg; } ColRingTile;

typedef struct {
  unsigned syncPages, syncTiles, loadPages, loadTiles, evictPages, evictTiles, ioErr, poolShort;
  unsigned heapPages, heapPeak, residentPages, residentPeak, blobBytes, blobPeak, replans, wantPeak,
  syncWantPage, syncWantTile, syncTileDist[6], farHitPage, farHitTile, syncByKind[9];   // дочитано внутри плана кольца (подгрузка не успела) / вне плана; тайлы вне плана по расстоянию до фокуса: <250/<300/<350/<400/<500/дальше
} ColRingStats;

typedef struct ColRing {
  ColRingIO io;
  float ox, oz, cell, minY; uint32_t W, H, ntri, nbox, ncyl, nseg, nref, TW, TH, npages, crc, mch, secSize, fp, flags, need250pg, need250blob, need340pg, need340blob;
  uint32_t offTri, offBlob, lenJmp, lenSrf, lenBake;
  uint32_t *slot; uint8_t *small, *jmp, *srf, *bake;
  ColRingTile *tile; uint32_t *tilePg; uint32_t ntile;
  const ColTri **pg; uint8_t *pgSrc;          // pgSrc: 0 нет, 1 пул, 2 куча
  const uint32_t **blob;
  const ColTri **pgFar; const uint32_t **blobFar;   // дочитанное вне плана кольца (далёкие игровые объекты: NPC, машины миссий): читается только через *Sync, там же обновляется давность — повторный запрос не идёт на карту
  uint8_t *poolMem; uint32_t poolPages, nfree; uint16_t *freeStk;
  // план кольца
  float R, planX, planZ; int planValid; uint32_t tickNo;
  uint8_t *wantTile, *wantPage; uint32_t *stampTile, *stampPage; uint32_t *order, norder; uint32_t *pendT, *pendP; uint32_t npendT, npendP;
  uint32_t nsync;
  long whoRel[12]; unsigned whoN[12];   // кто чаще всего дочитывал землю (colGroundRel), для журнала
  int inTick;
  ColRingStats st;
} ColRing;

// poolMem/poolBytes — память под страницы треугольников (кратно 12288 Б); NULL/0 — всё из кучи. 0 — не открылось (формат, чтение, память), err — причина
ColRing *colRingOpen(const ColRingIO *io, void *poolMem, unsigned poolBytes, int *err);
void colRingClose(ColRing *r);
float colRingRadius(const ColRing *r);
void colRingSetRadius(ColRing *r, float R);                    // радиус кольца, м (по умолчанию 250)
void colRingTick(ColRing *r, float fx, float fz);              // раз в кадр, вне запросов: план кольца, выгрузка, подгрузка с бюджетом (2 страницы и 1 тайл)
void colRingEnsure(ColRing *r, float fx, float fz);            // синхронно: всё кольцо вокруг точки готово (загрузка уровня, телепорт, расстановка миссии)
unsigned colRingResidentBytes(const ColRing *r);               // примитивы кольца + блобы + резидентные таблицы
const ColTri *colRingPageSync(ColRing *r, uint32_t page);
const uint32_t *colRingTileSync(ColRing *r, uint32_t tile);

static inline const ColTri *colRingTri(ColRing *r, uint32_t id) {
  uint32_t s = r->slot[id]; const ColTri *p = r->pg[s >> 8];
  if (!p) p = colRingPageSync(r, s >> 8);
  return p + (s & 255u);
}
static inline void colRingCell(ColRing *r, uint32_t gx, uint32_t gz, const uint32_t **b, const uint32_t **e) {
  uint32_t t = (gz >> 4) * r->TW + (gx >> 4); const uint32_t *bl = r->blob[t];
  if (!bl) bl = colRingTileSync(r, t);
  uint32_t li = ((gz & 15u) << 4) | (gx & 15u);
  *b = bl + 257 + bl[li]; *e = bl + 257 + bl[li + 1];
}
#endif
