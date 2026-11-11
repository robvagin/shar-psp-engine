// Пул клеток (№637, TEST23: отказ памяти у будки при 10 МБ свободно, самый большой кусок 2,3 МБ; шаг 1Б разбора памяти 04.11, TEST90).
// Клетки города (десятки КБ) и их мелкие хвосты (ggBatch, сетка камеры, развёртка полос) раньше шли прямо в malloc и дробили кучу:
// между ними застревали долгоживущие блоки, и крупное выделение (комната, карточка, машина миссии) не находило цельного куска.
// Теперь два пула:
//  1) КЛЕТКИ (cpAllocCell) — ячейки по размеру. Класс = ближайший сверху размер из таблицы (по умолчанию 16/32/64/128/192 КБ); ячейки одного класса
//     лежат в плите (слабе), плита берётся из кучи одним блоком и возвращается в кучу, когда в ней не осталось живых ячеек. Ячейка освобождённой
//     клетки подходит любой следующей клетке того же класса — снаружи плит куча не дробится клетками. Выбор плиты — самая заполненная со свободным местом
//     (остальные имеют шанс опустеть). Клетка крупнее старшего класса и отказ на новую плиту идут прямо в memalign, как раньше.
//  2) ХВОСТЫ (cpAlloc) — куски по CP_CHUNK, внутри первый подходящий свободный блок со склейкой соседей, кусок возвращается в кучу целиком, когда
//     в нём не осталось живых блоков; блок крупнее CP_DIRECT идёт прямо в memalign.
// cpFree узнаёт, чей блок, по адресу (плиты, потом куски), иначе free(): переключение режима при живых блоках безопасно.
// Замок malloc (рекурсивный) общий с загрузчиком: аллокаторы зовут загрузчик и главный поток, cpFree — главный.
// Выключатели: CELLPOOL_OLD=1 (сборка), файл `cellpoolold` рядом с EBOOT (выключает), `cellpoolon` (включает при CELLPOOL_ON=0), `cellpooltails` (только хвосты), «flag cellpoolold 1»; таблица классов — файл `cellpoolcls` (одно слово: a — 16…192 КБ, b — мельче: 8/12/16/24/32/48/64/96/128/192 КБ).
#pragma once
// ВЫКЛЮЧЕН по умолчанию (итог 1Б, TEST90). Прежний замер эмулятора, тур 36 сцен с балластом 3 МБ (пул кусков 256 КБ без плит): на L1 свободной кучи
// на 3 МБ меньше (4,4 против 7,0 МБ), цельный кусок не больше — дробят не клетки, а записи миссий art. Новый замер — docs/handoff/ОТЧЁТ_1Б_ПУЛ_КЛЕТОК_2026-11-04.md.
#ifndef CP_MEM
#define CP_MEM(al, sz) memalign((al), (sz))   // откуда берутся плиты, куски хвостов и клетки вне классов; движок (main_state.h) ставит область «город» зоны 2 (П5 Зоны-95)
#endif
#ifndef CELLPOOL_ON
#define CELLPOOL_ON 0   // выключен: замер 1Б (цепочка уровней в эмуляторе) — пул клеток без зон кучу ухудшает; включить: файл `cellpoolon` или -DCELLPOOL_ON=1
#endif
#ifndef CELLPOOL_OLD
#define CELLPOOL_OLD (!CELLPOOL_ON)
#endif
#define CP_CHUNK (256u << 10)
#define CP_DIRECT (96u << 10)
#define CP_MAXCH 96
#define CP_MAXSLAB 160
#define CP_MAXCLS 12
#ifdef CP_HOST   // проверка на Маке (tests/cellpool1b/host.c): без замка PSP
#define CP_LOCK() ((void)0)
#define CP_UNLOCK() ((void)0)
#else
extern void __malloc_lock(struct _reent *); extern void __malloc_unlock(struct _reent *);
#define CP_LOCK() __malloc_lock(_REENT)
#define CP_UNLOCK() __malloc_unlock(_REENT)
#endif
typedef struct { unsigned size, used, pad[14]; } CpHdr;   // 64 байта перед каждым блоком хвоста (строка кэша: заголовок не делит строку с данными, которые пишет чтение карты): размер вместе с заголовком, занят ли
typedef struct { unsigned char *base; unsigned live; } CpChunk;
typedef struct { unsigned char *base; unsigned cell, n, nfree, freeMask, bytes; unsigned char cls; } CpSlab;   // freeMask: бит i = ячейка i свободна (n ≤ 32)
static CpChunk cpCh[CP_MAXCH]; static int cpN;
static CpSlab cpSl[CP_MAXSLAB]; static int cpSlN;
static unsigned cpClsKB[CP_MAXCLS] = { 16, 32, 64, 128, 192 }; static int cpNCls = 5;
static int cpOld = CELLPOOL_OLD, cpCellsOff;   // режим на выделение (на освобождение не влияет); cpCellsOff — клетки прямо в кучу, пул только для хвостов (замер 1Б)
static unsigned cpAllocs, cpDirect, cpChunksMade, cpChunksFreed, cpChunkFail, cpLiveBytes;   // хвосты
static unsigned cpCellAllocs, cpCellDirect, cpSlabsMade, cpSlabsFreed, cpSlabFail, cpCellsLive, cpCellReq, cpSlabHeld, cpHeapFreed, cpCellPeakHeld;   // клетки; cpHeapFreed — байт вернулось в кучу (хвосты+плиты)
static void cpSetTable(char t) {
  static const unsigned A[] = { 16, 32, 64, 128, 192 }, B[] = { 8, 12, 16, 24, 32, 48, 64, 96, 128, 192 };
  const unsigned *s = t == 'b' ? B : A; int n = t == 'b' ? 10 : 5;
  if (cpSlN && cpSlabHeld) return;   // при живых плитах таблицу не меняем
  for (int i = 0; i < n; i++) cpClsKB[i] = s[i];
  cpNCls = n;
}
static unsigned cpSlabBytes(unsigned cell) { unsigned b = 256u << 10; while (b / cell < 4u) b += 256u << 10; if (b / cell > 32u) b = 32u * cell; return b; }   // плита: ≥ 4 ячеек, ≤ 32 (маска 32 бита), кратна 64 Б
static void *cpAlloc(size_t sz) {
  if (cpOld || sz > CP_DIRECT) { cpDirect++; return CP_MEM(16, sz ? sz : 16); }
  unsigned need = (((unsigned)sz + 63u) & ~63u) + 64u;
  CP_LOCK();
  void *r = 0;
  for (int i = 0; i < cpN && !r; i++) { CpChunk *c = &cpCh[i]; if (!c->base) continue;
    for (unsigned o = 0; o < CP_CHUNK; ) { CpHdr *h = (CpHdr *)(c->base + o);
      if (!h->used && h->size >= need) {
        if (h->size - need >= 128u) { CpHdr *n = (CpHdr *)(c->base + o + need); n->size = h->size - need; n->used = 0; h->size = need; }
        h->used = 1; c->live++; cpLiveBytes += h->size; r = (unsigned char *)h + 64; break; }
      o += h->size; } }
  if (!r) {
    int k = -1; for (int i = 0; i < cpN; i++) if (!cpCh[i].base) { k = i; break; }
    if (k < 0 && cpN < CP_MAXCH) k = cpN++;
    unsigned char *b = k >= 0 ? CP_MEM(64, CP_CHUNK) : 0;
    if (b) { cpCh[k].base = b; cpChunksMade++;
      CpHdr *h = (CpHdr *)b;
      if (CP_CHUNK - need >= 128u) { CpHdr *n = (CpHdr *)(b + need); n->size = CP_CHUNK - need; n->used = 0; h->size = need; } else h->size = CP_CHUNK;
      h->used = 1; cpCh[k].live = 1; cpLiveBytes += h->size; r = b + 64; }
    else cpChunkFail++;
  }
  CP_UNLOCK();
  if (!r) { cpDirect++; return CP_MEM(16, sz ? sz : 16); }
  cpAllocs++; return r;
}
// клетка: ячейка по размеру; sz — размер данных клетки
static void *cpAllocCell(size_t sz) {
  int cls = -1;
  if (!cpOld && !cpCellsOff) for (int i = 0; i < cpNCls; i++) if ((size_t)cpClsKB[i] * 1024u >= sz) { cls = i; break; }
  if (cls < 0) { cpCellDirect++; return CP_MEM(16, sz ? sz : 16); }
  unsigned cell = cpClsKB[cls] * 1024u;
  CP_LOCK();
  void *r = 0; CpSlab *best = 0;
  for (int i = 0; i < cpSlN; i++) { CpSlab *s = &cpSl[i]; if (s->base && s->cls == cls && s->nfree && (!best || s->nfree < best->nfree)) best = s; }
  if (!best) {
    int k = -1; for (int i = 0; i < cpSlN; i++) if (!cpSl[i].base) { k = i; break; }
    if (k < 0 && cpSlN < CP_MAXSLAB) k = cpSlN++;
    unsigned bytes = cpSlabBytes(cell); unsigned char *b = k >= 0 ? CP_MEM(64, bytes) : 0;
    if (b) { CpSlab *s = &cpSl[k]; s->base = b; s->cls = (unsigned char)cls; s->cell = cell; s->n = bytes / cell; s->nfree = s->n; s->freeMask = s->n >= 32 ? 0xFFFFFFFFu : (1u << s->n) - 1u; s->bytes = bytes;
      cpSlabsMade++; cpSlabHeld += bytes; if (cpSlabHeld > cpCellPeakHeld) cpCellPeakHeld = cpSlabHeld; best = s; }
    else cpSlabFail++;
  }
  if (best) { unsigned m = best->freeMask; int bit = 0; while (!((m >> bit) & 1u)) bit++;
    best->freeMask &= ~(1u << bit); best->nfree--; r = best->base + (size_t)bit * best->cell; cpCellsLive++; cpCellReq += (unsigned)sz; }
  CP_UNLOCK();
  if (!r) { cpCellDirect++; return CP_MEM(16, sz ? sz : 16); }   // плиту не дали: прежний путь, клетка нужного размера
  cpCellAllocs++; return r;
}
static void cpFree(void *p) {
  if (!p) return;
  CP_LOCK();
  for (int i = 0; i < cpSlN; i++) { CpSlab *s = &cpSl[i];
    if (s->base && (unsigned char *)p >= s->base && (unsigned char *)p < s->base + s->bytes) {
      unsigned bit = (unsigned)((unsigned char *)p - s->base) / s->cell;
      if (!((s->freeMask >> bit) & 1u)) { s->freeMask |= 1u << bit; s->nfree++; cpCellsLive--; }
      if (s->nfree == s->n) { cpSlabHeld -= s->bytes; cpHeapFreed += s->bytes; free(s->base); s->base = 0; cpSlabsFreed++; }
      CP_UNLOCK(); return; } }
  for (int i = 0; i < cpN; i++) { CpChunk *c = &cpCh[i];
    if (c->base && (unsigned char *)p >= c->base && (unsigned char *)p < c->base + CP_CHUNK) {
      CpHdr *h = (CpHdr *)((unsigned char *)p - 64); h->used = 0; cpLiveBytes -= h->size; c->live--;
      for (unsigned o = 0; o < CP_CHUNK; ) { CpHdr *a = (CpHdr *)(c->base + o);   // склейка соседних свободных
        if (!a->used) { unsigned e = o + a->size; while (e < CP_CHUNK && !((CpHdr *)(c->base + e))->used) { a->size += ((CpHdr *)(c->base + e))->size; e = o + a->size; } }
        o += a->size; }
      if (c->live == 0) { free(c->base); c->base = 0; cpChunksFreed++; cpHeapFreed += CP_CHUNK; }
      CP_UNLOCK(); return; } }
  CP_UNLOCK();
  free(p);
}
// строка журнала пула (раз в секунду в смоуке): held — сколько плит держит в куче, req — сколько из них занято данными клеток
static void cpStatLine(char *o, size_t n, float t) {
  unsigned heldCh = 0; for (int i = 0; i < cpN; i++) if (cpCh[i].base) heldCh += CP_CHUNK;
  unsigned liveSl = 0; for (int i = 0; i < cpSlN; i++) if (cpSl[i].base) liveSl++;
  snprintf(o, n, "CELLPOOL t=%.1f old=%d tailsonly=%d cells=%u direct=%u slabs=%u/%u held_kb=%u peak_kb=%u slabfail=%u tail_chunks=%u/%u tailheld_kb=%u chunkfail=%u freed_kb=%u cpAllocs=%u cpDirect=%u\n",
           t, cpOld, cpCellsOff, cpCellsLive, cpCellDirect, liveSl, cpSlabsMade, cpSlabHeld >> 10, cpCellPeakHeld >> 10, cpSlabFail, cpChunksMade - cpChunksFreed, cpChunksMade, heldCh >> 10, cpChunkFail, cpHeapFreed >> 10, cpAllocs, cpDirect);
}
