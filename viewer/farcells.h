// Модели-96 (101): дальние клетки. Читатель farcells.shr («FAR1», docs/handoff/ДАЛЬНИЕ_КЛЕТКИ_ФОРМАТ_2026-11-09.md, tools/farcells_export.py) и перевешивание кусков клетки между полной и дальней версией.
// Включается из main_stream.h ПОСЛЕ cellFinish (нужны Cell cl[], Piece pc[], cpFree, cpAllocCell, ggBatchCacheBuild, pieceBigMark, marksCellLoad/Free, dlFree, cgGen/cgDirty, loadedBytes, DCPOST96).
// Переключение версий, очередь загрузчика, бюджет города, выключатели — Лестница-96 (main_stream.h); здесь только данные и безопасная подмена указателей.
//   farFileLoad(farPath, levelPath) — один раз при загрузке уровня, ПОСЛЕ чтения таблицы кусков pc[] (полные r.ni/nv/nf/off запоминаются). 1 — файл принят; 0 — файла нет/ключ не сошёлся/другое число клеток: всё как раньше.
//   farCellOnDisk(i, &off, &size) — где в farcells.shr лежит дальний блоб клетки i (0 — у клетки дальней версии нет).
//   cellFinishFar(i, m) — как cellFinish, но m — дальний блоб (farSize байт): указатели кусков, ggBatch, pieceBigMark, плоские правки; ставит c->mem, loadedBytes += farSize, state = 2.
//   cellSwapVersion(i, newMem, toFar, oldSize, newSize) — клетка в state 2 в главном потоке: перевесить все её куски на новый блок (toFar 1 — дальний, 0 — полный), сбросить кэши от старых указателей, вернуть старый блок (освобождает вызывающий).
// Кэши, которые держат указатели/число вершин кусков клетки и сбрасываются при подмене (как в unloadCell): лист клетки dlFree(i) (display list на память клетки), разметка тушью marksCellFree/Load (toonflat.h), ggBatch куска
// (cpFree + пересборка ggBatchCacheBuild), unr (только F_STRIP, у дальних клеток нет), сетка камеры c->cg (cpFree, cgs = 0, cgGen++, cgDirty++), pieceBigMark (таблица по адресу вершин; у нового блока новый адрес, как при обычной загрузке клетки).
#ifndef FARCELLS_H
#define FARCELLS_H
#include <string.h>
#include <stdlib.h>
#ifndef FARCELL_OLD
#define FARCELL_OLD 1   // 103: как в farcell.h (дальние клетки выключены по умолчанию)
#endif
typedef struct { unsigned farOff, farSize, nvTotal, flags; } FarCell;
typedef struct { unsigned nvFar, offFar; } FarPiece;
typedef struct { int ni, nv, nf; unsigned off; } FarFull;
#define FAR_NONE 0xFFFFFFFFu
static FarCell *farCell; static FarPiece *farPiece; static FarFull *farFull; static unsigned farNCell, farNPiece, farBlobBase; static int farOk;
static unsigned farSwaps, farSwapsFar, farTfree, farTbind, farTmarks, farTother;   // цена по фазам обмена, мкс за период (диагностика)   // служебное: сколько подмен всего и в дальнюю
static unsigned farFnv(const unsigned char *p, unsigned n, unsigned size) {
  unsigned h = 0x811C9DC5u; for (unsigned i = 0; i < n; i++) h = (h ^ p[i]) * 0x01000193u;
  for (int k = 0; k < 4; k++) h = (h ^ (size >> (8 * k) & 255u)) * 0x01000193u;
  return h;
}
static void farFileFree(void) { free(farCell); free(farPiece); free(farFull); farCell = 0; farPiece = 0; farFull = 0; farNCell = farNPiece = 0; farOk = 0; }
static int farFileLoad(const char *farPath, const char *levelPath) {
  farFileFree();
  if (FARCELL_OLD) return 0;
  SceUID lf = sceIoOpen(levelPath, PSP_O_RDONLY, 0); if (lf < 0) return 0;
  unsigned lsize = (unsigned)sceIoLseek32(lf, 0, PSP_SEEK_END); sceIoLseek32(lf, 0, PSP_SEEK_SET);
  static unsigned char head[4096]; int got = sceIoRead(lf, head, sizeof head); sceIoClose(lf); if (got < 4096 && (unsigned)got != lsize) return 0;
  SceUID ff = sceIoOpen(farPath, PSP_O_RDONLY, 0); if (ff < 0) return 0;
  unsigned h[16]; int ok = sceIoRead(ff, h, sizeof h) == (int)sizeof h && !memcmp(h, "FAR1", 4) && h[1] == 1 && h[2] == farFnv(head, (unsigned)got, lsize) && h[3] == ncell && h[4] == npc && h[10] == lsize;
  if (ok) {
    farNCell = h[3]; farNPiece = h[4]; farBlobBase = h[7];
    farCell = malloc((size_t)farNCell * sizeof(FarCell)); farPiece = malloc((size_t)farNPiece * sizeof(FarPiece)); farFull = malloc((size_t)farNPiece * sizeof(FarFull));
    FarPiece *tmp = malloc((size_t)farNPiece * 16);
    ok = farCell && farPiece && farFull && tmp;
    if (ok) { sceIoLseek32(ff, (int)h[5], PSP_SEEK_SET); ok = sceIoRead(ff, farCell, (int)(farNCell * sizeof(FarCell))) == (int)(farNCell * sizeof(FarCell)); }
    if (ok) { sceIoLseek32(ff, (int)h[6], PSP_SEEK_SET); ok = sceIoRead(ff, tmp, (int)(farNPiece * 16)) == (int)(farNPiece * 16); }
    if (ok) for (unsigned k = 0; k < farNPiece; k++) { const unsigned *q = (const unsigned *)tmp + 4 * k; farPiece[k].nvFar = q[0]; farPiece[k].offFar = q[1];
      farFull[k].ni = pc[k].r.ni; farFull[k].nv = pc[k].r.nv; farFull[k].nf = pc[k].r.nf; farFull[k].off = pc[k].r.off; }
    free(tmp);
  }
  sceIoClose(ff);
  if (!ok) { farFileFree(); return 0; }
  farOk = 1; return 1;
}
static int farCellOnDisk(int i, unsigned *off, unsigned *size) {
  if (!farOk || i < 0 || (unsigned)i >= farNCell || farCell[i].farOff == FAR_NONE) return 0;
  *off = farBlobBase + farCell[i].farOff; *size = farCell[i].farSize; return 1;
}
// указатели куска на данные блока m: версия toFar — дальняя (offFar, nvFar), иначе полная (запомненные при farFileLoad)
static void farBindPieces(int i, void *m, int toFar) {
  Cell *c = &cl[i];
  for (unsigned k = c->first; k < c->first + c->n; k++) {
    Piece *d = &pc[k];
    if (toFar) { d->r.ni = d->r.nv = (int)farPiece[k].nvFar; d->r.nf = 0; d->r.off = farPiece[k].offFar; }
    else { d->r.ni = farFull[k].ni; d->r.nv = farFull[k].nv; d->r.nf = farFull[k].nf; d->r.off = farFull[k].off; }
    char *b = (char *)m + d->r.off;
    d->v = (Vert *)b; d->idx = d->fidx = 0; d->unr = 0; d->ggBatch = 0;
    if (!(d->r.flags & F_NOIDX)) { d->v = 0; continue; }   // у клетки с дальней версией все куски F_NOIDX (экспортёр иначе версию не пишет)
    if (flatVNoise && flatOn && d->r.owner == 0) flatVertexNoise(d, (unsigned char *)b, (unsigned)d->r.ni);
    if (zn7On && zpMul && d->r.owner == 0 && k < zpNpc) zpVertex(d, (unsigned char *)b, (unsigned)(d->r.ni + d->r.nf), zpMul + (size_t)k * 4);
    if (!GG_BATCH_OLD && d->r.owner == 0 && d->r.ni <= 65535) {
      unsigned nn = ggBatchStorage(d->r.ni), nf = ggBatchStorage(d->r.nf), vs = (d->r.flags & F_V12) ? 12 : 16, po = (d->r.flags & F_V12) ? 6 : 8;
      d->ggBatch = cpAlloc((nn + nf) * sizeof(GgBatch));
      if (d->ggBatch) { ggBatchCacheBuild(d->ggBatch, b, d->r.ni, vs, po); if (nf) ggBatchCacheBuild(d->ggBatch + nn, b + d->r.ni * vs, d->r.nf, vs, po); }
      pieceBigMark(i, d, (const unsigned char *)b, (unsigned)d->r.ni, vs, po);
    }
  }
}
static void cellFinishFar(int i, void *m) {
  Cell *c = &cl[i]; int ldLow_ = ldFinLow(); unsigned fs = farCell[i].farSize;
  if (i >= 0 && i < 4096) cellLt[i] = sceKernelGetSystemTimeLow() | 1u;
  sceKernelDcacheWritebackRange(m, fs);
  farBindPieces(i, m, 1);
  DCPOST96(m, fs);
  marksCellLoad((unsigned)i);
  c->mem = m; loadedBytes += fs;
  __asm__ volatile("" ::: "memory");
  c->state = 2; cgDirty++;
  ldFinHigh(ldLow_);
}
// Подмена версии у клетки в state 2. Главный поток, между кадрами (начало кадра): рисование и камера этой клетки в этот момент не идут. state остаётся 2.
static void *cellSwapVersion(int i, void *newMem, int toFar, unsigned oldSize, unsigned newSize) {
  Cell *c = &cl[i]; void *old = c->mem; unsigned t0 = sceKernelGetSystemTimeLow();
  dlFree((unsigned)i); marksCellFree((unsigned)i);
  for (unsigned k = c->first; k < c->first + c->n; k++) { cpFree(pc[k].ggBatch); pc[k].ggBatch = 0; cpFree(pc[k].unr); pc[k].unr = 0; }
  cpFree(c->cg); c->cg = 0; c->cgs = 0; cgGen++;
  unsigned t1 = sceKernelGetSystemTimeLow();
  sceKernelDcacheWritebackRange(newMem, newSize);
  farBindPieces(i, newMem, toFar);
  DCPOST96(newMem, newSize);
  unsigned t2 = sceKernelGetSystemTimeLow();
  marksCellLoad((unsigned)i);
  unsigned t3 = sceKernelGetSystemTimeLow(); farTfree += t1 - t0; farTbind += t2 - t1; farTmarks += t3 - t2;
  c->mem = newMem; loadedBytes += newSize; loadedBytes -= oldSize;
  __asm__ volatile("" ::: "memory");
  cgDirty++; farSwaps++; farSwapsFar += toFar ? 1u : 0u;
  return old;
}
#endif
