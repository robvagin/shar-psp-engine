// ---------- стриминг клеток ----------
static int loaderUp;   // загрузчик клеток запущен (тентативное определение; main_levels.h)
static volatile int cellFail;   // правка 103: клетка не легла: 2 — нет памяти (главный поток освобождает место), 1 — ошибка чтения; повтор — stream
// после чтения: указатели кусков клетки, кэш пакетов (общий хвост loadCell и укрупнённого чтения круга)
// №840: есть ли в списке треугольников (по 3 вершины подряд) ребро длиннее GG_BIG_EDGE (вершины short в осях куска, мир = sc/32768 · вершина)
static int pieceBigEdge(const unsigned char *v, unsigned n, unsigned vs, unsigned po, float sc) {
  if (!v || sc <= 0.f) return 0;
  float k = sc / 32768.f, lim = GG_BIG_EDGE * GG_BIG_EDGE / (k * k);
  for (unsigned t = 0; t + 2 < n; t += 3) {
    const short *p0 = (const short *)(v + t * vs + po), *p1 = (const short *)(v + (t + 1) * vs + po), *p2 = (const short *)(v + (t + 2) * vs + po);
    for (int e = 0; e < 3; e++) { const short *a = e == 0 ? p0 : e == 1 ? p1 : p2, *b = e == 0 ? p1 : e == 1 ? p2 : p0;
      float dx = (float)a[0] - b[0], dy = (float)a[1] - b[1], dz = (float)a[2] - b[2]; if (dx * dx + dy * dy + dz * dz > lim) return 1; }
  }
  return 0;
}
static void pieceBigMark(int i, Piece *d, const unsigned char *list, unsigned n, unsigned vs, unsigned po) {
  if (GG_BIG840_OLD || d->r.owner != 0 || ((d->r.flags & 2) && !(d->r.flags & 4))) return;
  if (pieceBigEdge(list, n, vs, po, d->r.sc)) { d->r.pad[1] = 1u; if (cellBigF) cellBigF[i] = 1; }
}
static void cellFinish(int i, void *m) {
  Cell *c = &cl[i];
  sceKernelDcacheWritebackRange(m, c->size);
  for (unsigned k = c->first; k < c->first + c->n; k++) {
    Piece *d = &pc[k]; char *b = (char *)m + d->r.off;
    d->v = (Vert *)b; if (smkOn && flatDump && (d->r.flags & F_STRIP)) fdPiece(d, 0);
    if (d->r.flags & F_STRIP) {   // К4: полосы (stripdraw.h); STRIPS_OLD=1 — развернуть в список при загрузке
      const StripHdr *h = (const StripHdr *)b; const unsigned vs_ = (d->r.flags & F_V12) ? 12u : 16u, po_ = (d->r.flags & F_V12) ? 6u : 8u;
      if (flatVNoise && flatOn && d->r.owner == 0) flatVertexNoise(d, (unsigned char *)b + 16, h->nsvN);
      if (zn7On && zpMul && d->r.owner == 0 && k < zpNpc) zpVertex(d, (unsigned char *)b + 16, h->nsvN, zpMul + (size_t)k * 4);
      d->v = (Vert *)(b + 16); d->idx = (unsigned short *)(b + 16 + (size_t)h->nsvN * vs_); d->fidx = (unsigned short *)((const unsigned char *)d->idx + ((h->nsN * 2u + 3u) & ~3u) + (size_t)h->nsvF * vs_);
      if (zn7On && zpMul && d->r.owner == 0 && k < zpNpc && h->nsvF) zpVertex(d, (unsigned char *)stripFarV(d), h->nsvF, zpMul + (size_t)k * 4);   // №1007б: дальние вершины
      d->unr = 0; d->ggBatch = 0;
      if (STRIPS_OLD) {   // список в куче (unr), как до полос
        unsigned nN = stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs_, 0), nF = stripExpand(stripFarV(d), d->fidx, h->nsF, vs_, 0);
        unsigned char *lst = (unsigned char *)cpAlloc((size_t)(nN + nF) * vs_ + 16);
        if (lst) { stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs_, lst); if (nF) stripExpand(stripFarV(d), d->fidx, h->nsF, vs_, lst + (size_t)nN * vs_);
          sceKernelDcacheWritebackRange(lst, (nN + nF) * vs_); d->unr = lst; d->v = (Vert *)lst; d->idx = d->fidx = 0; d->r.flags &= ~F_STRIP; d->r.ni = (int)nN; d->r.nf = nF; d->r.nv = (int)(nN + nF); b = (char *)lst; }
        else { d->v = 0; d->idx = d->fidx = 0; continue; }   // нет памяти: кусок не рисуется (pieceOk), как при сбое
      }
      else {
        if (!GG_BATCH_OLD && d->r.owner == 0 && d->r.ni <= 65535) {   // кэш срезов — по развёртке (тот же порядок, что у pieceListP/drawGeoG)
          unsigned nn = ggBatchStorage(d->r.ni), nf = ggBatchStorage(d->r.nf); d->ggBatch = cpAlloc((nn + nf) * sizeof(GgBatch));
          if (d->ggBatch) {
            unsigned c1 = stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs_, 0), c2 = stripExpand(stripFarV(d), d->fidx, h->nsF, vs_, 0);
            unsigned char *t = stripScratchGet((c1 > c2 ? c1 : c2) * vs_ + 16);
            if (t) { if (c1) { stripExpand((const unsigned char *)d->v, d->idx, h->nsN, vs_, t); ggBatchCacheBuild(d->ggBatch, t, c1, vs_, po_); pieceBigMark(i, d, t, c1, vs_, po_); }
                     if (c2) { stripExpand(stripFarV(d), d->fidx, h->nsF, vs_, t); ggBatchCacheBuild(d->ggBatch + nn, t, c2, vs_, po_); } }
            stripScratchPut(); } }
        continue;
      }
    }
    if (d->r.flags & F_NOIDX) {
      d->idx = d->fidx = 0; if (smkOn && flatDump) fdPiece(d, (const unsigned char *)b);
      if (flatVNoise && flatOn && d->r.owner == 0) flatVertexNoise(d, (unsigned char *)b, (unsigned)d->r.ni);
      if (zn7On && zpMul && d->r.owner == 0 && k < zpNpc) zpVertex(d, (unsigned char *)b, (unsigned)(d->r.ni + d->r.nf), zpMul + (size_t)k * 4);   // №1007б: ближняя и дальняя версии подряд
      if (!GG_BATCH_OLD && d->r.owner == 0 && d->r.ni <= 65535) {
        unsigned nn=ggBatchStorage(d->r.ni),nf=ggBatchStorage(d->r.nf),vs=(d->r.flags&F_V12)?12:16,po=(d->r.flags&F_V12)?6:8;
        d->ggBatch=cpAlloc((nn+nf)*sizeof(GgBatch));
        if(d->ggBatch){ggBatchCacheBuild(d->ggBatch,b,d->r.ni,vs,po);if(nf)ggBatchCacheBuild(d->ggBatch+nn,b+d->r.ni*vs,d->r.nf,vs,po);}
        pieceBigMark(i, d, (const unsigned char *)b, (unsigned)d->r.ni, vs, po);   // №840
      }
      continue; }   // без индексов: дальняя версия = v + ni вершин
    if (smkOn && flatDump) fdPiece(d, 0);
    d->idx = (unsigned short *)(b + ((d->r.nv * sizeof(Vert) + 15) & ~15));
    d->fidx = (unsigned short *)((char *)d->idx + ((d->r.ni * 2 + 15) & ~15));
  }
  marksCellLoad((unsigned)i);   // №912: разметка клетки (flat912/marks912)
  c->mem = m; loadedBytes += c->size;
  __asm__ volatile("" ::: "memory");      // указатели кусков записаны раньше, чем state = 2
  c->state = 2; cgDirty++;   // №924: перебор клеток камеры (camgrid.h) увидит новую клетку сразу
}
static void loadCell(SceUID fd, int i) {
  Cell *c = &cl[i];
  void *m = cpAlloc(c->size);
  if (!m) { c->state = 3; smkAllocFail++; cellFail |= 2; return; }   // счётчики приёмки: viewer/smoke.h
  sceIoLseek32(fd, c->off, PSP_SEEK_SET);
  if (sceIoRead(fd, m, c->size) != (int)c->size || smokeBadRead()) { cpFree(m); c->state = 3; smkReadErr++; cellFail |= 1; return; }
  cellFinish(i, m);
}
// Загрузка 2, пункты 4–5: круг клеток вокруг старта читается укрупнёнными чтениями. Клетки круга лежат в файле подряд (по строкам карты):
// 214 клеток L3 = 14 стыков с мостами через зазор до RING_GAP (замер по таблице клеток, tests/load2). Одно sceIoRead на стык (не больше
// RING_RUNMAX) во временный буфер, клетка копируется в свой блок memalign (unloadCell освобождает клетки по одной) — память, ошибки
// и состояния как у loadCell. CELLRING_OLD=1 — прежнее чтение по клетке.
#ifndef CELLRING_OLD
#define CELLRING_OLD 0
#endif
#define RING_GAP (64u << 10)
#define RING_RUNMAX (512u << 10)
static unsigned ringReads, ringBytes;   // служебное: чтений и байт последнего круга (строка CELLRING)
static int ringCmp(const void *a, const void *b) { unsigned x = cl[*(const short *)a].off, y = cl[*(const short *)b].off; return x < y ? -1 : x > y; }
// idx[0..n): клетки state 0 (не owner); читает fd; done — сколько готово (для шкалы; пишет только вызывающий поток). -> n
static int cellsReadRuns(SceUID fd, short *idx, int n, volatile int *done) {
  if (n > 1) qsort(idx, (size_t)n, sizeof *idx, ringCmp);
  unsigned char *tmp = CELLRING_OLD ? 0 : memalign(16, RING_RUNMAX); ringReads = ringBytes = 0;
  for (int a = 0; a < n; ) {
    int b = a + 1; unsigned s = cl[idx[a]].off, e = s + cl[idx[a]].size;
    while (tmp && b < n && cl[idx[b]].off >= e && cl[idx[b]].off - e <= RING_GAP && cl[idx[b]].off + cl[idx[b]].size - s <= RING_RUNMAX) { e = cl[idx[b]].off + cl[idx[b]].size; b++; }
    if (!tmp || b - a == 1 || e - s > RING_RUNMAX) { loadCell(fd, idx[a]); ringReads++; ringBytes += cl[idx[a]].size; b = a + 1; }
    else {
      sceIoLseek32(fd, s, PSP_SEEK_SET);
      if (sceIoRead(fd, tmp, e - s) != (int)(e - s) || smokeBadRead()) { for (int k = a; k < b; k++) loadCell(fd, idx[k]); ringReads += (unsigned)(b - a); }   // сбой: по клетке — ошибки и повтор как раньше
      else {
        ringReads++; ringBytes += e - s;
        for (int k = a; k < b; k++) { Cell *c = &cl[idx[k]];
          void *m = cpAlloc(c->size);
          if (!m) { c->state = 3; smkAllocFail++; cellFail |= 2; continue; }
          memcpy(m, tmp + (c->off - s), c->size); cellFinish(idx[k], m); }
      }
    }
    if (done) *done += b - a;
    a = b;
  }
  free(tmp);
  return n;
}
static float fogSoft = 1.f;   // конец тумана города = drawDist × fogSoft (1 — как всегда; > 1 — мягкая граница, дальше HLOD: flag fogsoft, hlod.h)
static int visBox(const float *b, float ox, float oy, float oz); static float boxD2(const float *b); static float drawDist, eyeX, eyeY, eyeZ, cfX, cfY, cfZ;
// очередь загрузчика (как стриминг GTA LCS / Crash): главный поток раз в кадр кладёт до LQ_N лучших клеток
// (ближние и те, что впереди по ходу), загрузчик берёт их подряд, не дожидаясь следующего кадра.
// state: 0 -> 1 ставит только загрузчик, 2/3 -> 0 только главный поток — гонок нет.
#define LQ_N 12   // очередь загрузчика: 8 в обычной езде, до 12 на скорости (STREAM_AHEAD)
static volatile short lq[LQ_N]; static volatile int lqn, loaderBusy;
// №1051 (будка всегда открывается: описание — у boothRoom1051 ниже)
#ifndef BOOTHMEM1051_OLD
#define BOOTHMEM1051_OLD 0
#endif
#define BOOTH1051_R1 50.f
#define BOOTH1051_HOLD_R 45.f
static int boothMem1051 = !BOOTHMEM1051_OLD, stBoothHold1051;
// №1042 (Роб, L6 C6-9 у дома: «впереди не прогружает, не видно конца причала; сделай, чтобы впереди грузилось, как будто я ничего не делал»): очередь ставила
// вперёд только по скорости (стоя и пешком взгляд не учитывался), видимая клетка ближе дальности шла лишь «чуть раньше» остальных, а при нехватке кучи уходила самая дальняя,
// не глядя, впереди она или сзади. Теперь: (1) клетки в конусе взгляда камеры (±45°) получают приоритет и радиус загрузки +40 м и стоя; (2) видимая незагруженная клетка ближе
// STA1042_HEAD м — всегда голова очереди; (3) на ходу упреждение не меньше STA1042_MINAHEAD м по курсу; (4) при нехватке кучи (MEMTRIM, место под крупное) первыми уходят
// дальние, боковые и задние: клетка в конусе взгляда считается ближе на STA1042_KEEP м, позади — дальше на 40 м. STREAMAHEAD1042_OLD=1 / файл streamahead1042old / flag streamahead1042 0 — как было
#ifndef STREAMAHEAD1042_OLD
#define STREAMAHEAD1042_OLD 0
#endif
#define STA1042_HEAD 120.f
#define STA1042_MINAHEAD 40.f
#define STA1042_KEEP 100.f
#define STA1042_CONE .707f   // косинус половины конуса взгляда (45°)
static int streamAhead1042 = !STREAMAHEAD1042_OLD; static unsigned sta1042Head, sta1042Cone;   // счётчики: клеток-голов очереди, клеток в конусе, взятых раньше
// положение клетки относительно взгляда: 1 — в конусе (с запасом по радиусу клетки), -1 — позади (дальше назад), 0 — сбоку
static int sta1042Side(const Cell *c, float x, float z, float lx, float lz) {
  float dx = c->cx - x, dz = c->cz - z, hl = sqrtf(dx * dx + dz * dz) + .01f, al = (dx * lx + dz * lz) / hl, r = c->rad / hl;
  return al > STA1042_CONE - r ? 1 : al < -.2f - r ? -1 : 0;
}
static volatile unsigned accN, accLatSum, accLatMax, accSlow;   // Загрузка 3: учёт подгрузки раз в секунду (loadAccStep), свои счётчики — ящик обнуляет bbCell* сам
static volatile unsigned bbCellN, bbCellMax, bbCellSlow;   // исследование 03.10 П2: клеток за секунду, худшая задержка мс, дольше 100 мс
static unsigned slowIoKBs;   // служебное (файл slowio рядом с EBOOT): имитация медленной карты памяти, КБ/с
static unsigned stLoads, stLoadUs;   // статистика: число загрузок и суммарное время чтения
// смена уровня (levelUnload / levelLoad ниже): загрузчик простаивает (loaderIdle — ждёт семафор, клеток не трогает), файл
// уровня открывается заново, когда поменялось поколение lvGen (level.shr другого уровня)
static volatile unsigned lvGen; static volatile int loaderIdle;
static volatile int loaderErrRun, loaderFixOld; static volatile unsigned loaderReopenN;   // подряд упавших чтений; переоткрытий файла уровня (ловушка 05.10)
// Ловушка 05.10 (Роб: «загрузил DEV — Гомер в пустоте без текстур, карта не догрузилась, всё мигает»; чёрный ящик: rd растёт
// ~160/с, клетки падают за 0 мс, куча 20 МБ свободна — у потока загрузки отняли файл). На PSP номер закрытого файла тут же
// получает следующий открытый; повторное закрытие старого номера где-то в коде закрывает level.shr загрузчика, а эмулятор
// номера не переиспользует и молчит. Все sceIoClose идут через обёртку (CMakeLists --wrap=sceIoClose): закрыт номер файла
// загрузчика чужим кодом или закрыт неоткрытый — адрес вызова в ящик (IOCLOSE) и в журнал; загрузчик открывает файл заново.
int __real_sceIoClose(SceUID fd);
static volatile SceUID loaderFd = -1; static volatile int loaderFdLost, loaderSelfClose;
static volatile unsigned ioBadN, ioStealN, ioBadRa, ioStealRa; static volatile int ioBadFd, ioBadRes;
int __wrap_sceIoClose(SceUID fd) {
  unsigned ra = (unsigned)__builtin_return_address(0);
  if (fd >= 0 && fd == loaderFd && !loaderSelfClose) { loaderFdLost = 1; ioStealN++; ioStealRa = ra; }
  int r = __real_sceIoClose(fd);
  if (r < 0) { ioBadN++; ioBadRa = ra; ioBadFd = fd; ioBadRes = r; }
  return r;
}
// Загрузка 2, пункт 5: круг клеток у старта читает загрузчик, пока главный поток грузит остальное (cellRingStart в main_frontend.h, ждёт cellsRingAt).
// ringState: 0 нет задания, 1 идёт (задание кладёт главный поток, загрузчик читает), 2 готово (главный сбрасывает в 0)
#define RING_MAX 1200
static short ringIdx[RING_MAX]; static volatile int ringN, ringState, ringDone;
#ifndef CELLRING_BG_OLD
#define CELLRING_BG_OLD 0
#endif
// Загрузка 2, пункт 6: COL1 и PRP2 (4,9 МБ) читает загрузчик, пока главный поток считает текстуры (палитры, таблицы кусков и клеток);
// главный потом ждёт и разбирает (levelColAdopt / levelPropAdopt в main_levels.h). tailState: 0 нет, 1 идёт, 2 готово
#ifndef TAILBG_OLD
#define TAILBG_OLD 0
#endif
static volatile int tailState; static unsigned tailColOff, tailColSz, tailPropOff, tailPropSz; static void *tailCol, *tailProp;
// №962б (тур TEST62 на PSP: запас lva берёт 1,2 МБ из 2,6, softmiss +1 на каждой смене уровня; в эмуляторе softmiss стоит): место под COL1/PRP2 загрузчик занимает в участке сразу, а tailCol/tailProp
// выставляет только по окончании чтения. Эмулятор читает мгновенно (к разбору PVS оба уже готовы, резерв 0), на PSP чтение с карты идёт ещё в этот момент — резерв считал уже занятое место второй раз и запрос
// PV2S/PVS1 уходил в кучу. Теперь резерв считается по факту занятия места (tailColAl/tailPropAl), а не по готовности чтения. LVATAIL962B_OLD=1 / файл lvatail962bold рядом с EBOOT — по готовности, как было
#ifndef LVATAIL962B_OLD
#define LVATAIL962B_OLD 0
#endif
static volatile int tailColAl, tailPropAl; static int lvaTail962bOld = LVATAIL962B_OLD;
// №775/№776 (TEST33 и TEST27 на PSP: смена уровня после долгой игры встаёт на загрузке, L4 без столкновений — «только машина»): уровню нужны три цельных куска —
// резидентная часть level.shr (8,0–10,4 МБ), COL1 (3,7–5,8 МБ) и PRP2 (1,0–1,7 МБ), вместе до 17,7 МБ (L1, L4). Ящик с PSP перед загрузкой: свободно 39 МБ, но
// наибольший кусок 11,4 / 9,5 МБ — куча раздроблена долгоживущими выделениями долгой игры, а игрок, интерфейс и звук уровня (≈3,5 МБ) выделяются раньше:
// резидентной части или COL1 цельного места не хватает (fail «Not enough memory» под экраном сторожа; без COL1 — машина без столкновений и без выхода).
// Эмулятор этого не видел: каждый прогон с чистой кучей. Теперь участок «lva» под эти три куска выделяется один раз при первой загрузке уровня игры (куча чистая)
// и не отдаётся: каждый следующий уровень кладёт их туда же подряд, дробиться нечему. Нет участка (не выделился, бонус-трасса) или кусок не влез — memalign, как раньше.
// LVA_OLD=1 — прежнее (каждый кусок отдельно из кучи). Тест: «flag fragheap N» (main_levels.h) — куча раздроблена до N МБ перед загрузкой
#ifndef LVA_OLD
#define LVA_OLD 0
#endif
// Хвост резидентной части level.shr (таблица кусков 80 Б × npc и таблица клеток 48 Б × ncell: 0,43–0,69 МБ) нужен только пока levelLoad копирует его в pc[] и cl[]. Раньше он лежал
// в участке lva до выгрузки уровня и участок был на эти 0,7 МБ больше. Теперь резидентная часть = [0, tableOff), хвост читается отдельно: в запас участка (уровни, где он есть) или во временный буфер кучи
// (L1, L4), освобождается после таблицы клеток. LEVEL_TAIL_OLD=1 — как было (весь кусок в участке, LVA_BYTES прежний)
#ifndef LEVEL_TAIL_OLD
#define LEVEL_TAIL_OLD 0
#endif
#define LVA_BYTES ((17u << 20) + ((LEVEL_TAIL_OLD ? 832u : 128u) << 10))   // 17,13 МБ: max(рез. до таблицы кусков + COL1 + PRP2) по уровням 1–7 = 17,04 МБ (L1); с LEVEL_TAIL_OLD=1 прежние 17,81 МБ (max рез. + COL1 + PRP2 = 17,71 МБ, L4)
static unsigned char *lvaBase; static unsigned lvaUsed, lvaLast, lvaMiss; static SceUID lvaSema = -1;
static int lvaOwns(const void *p) { return lvaBase && (const unsigned char *)p >= lvaBase && (const unsigned char *)p < lvaBase + LVA_BYTES; }
static void lvaInit(void) {
  if (LVA_OLD || lvaBase || bonusTrack) return;
  lvaBase = (unsigned char *)memalign(64, LVA_BYTES); lvaUsed = lvaLast = 0;
  if (lvaBase && lvaSema < 0) lvaSema = sceKernelCreateSema("lva", 0, 1, 1, NULL);
}
static void *lvaAlloc(unsigned n) {   // кусок уровня: из участка подряд (64 Б), иначе из кучи
  if (!LVA_OLD && lvaBase) {
    if (lvaSema >= 0) sceKernelWaitSema(lvaSema, 1, NULL);
    unsigned off = (lvaUsed + 63u) & ~63u; void *p = 0;
    if (off <= LVA_BYTES && n <= LVA_BYTES - off) { p = lvaBase + off; lvaLast = off; lvaUsed = off + n; } else lvaMiss++;
    if (lvaSema >= 0) sceKernelSignalSema(lvaSema, 1);
    if (p) return p;
  }
  return memalign(16, n);
}
static void lvaFree(void *p) {   // кусок участка: последний — место возвращается (PREREAD отдал буфер), прочие — до lvaReset; из кучи — free
  if (!p) return;
  if (!lvaOwns(p)) { free(p); return; }
  if (lvaSema >= 0) sceKernelWaitSema(lvaSema, 1, NULL);
  if ((unsigned)((unsigned char *)p - lvaBase) == lvaLast) lvaUsed = lvaLast;
  if (lvaSema >= 0) sceKernelSignalSema(lvaSema, 1);
}
static int lvaRoom(unsigned n) { return !LVA_OLD && lvaBase && ((lvaUsed + 63u) & ~63u) + n <= LVA_BYTES; }   // влезет ли n байт (запас под ещё не выделенное считает вызывающий)
static void lvaReset(void) { lvaUsed = lvaLast = 0; }   // уровень выгружен целиком (levelUnload): участок снова пуст
// №962 (куча TEST61 на L2, L3, L5–L7 меньше на 1,5–2,5 МБ): участок lva рассчитан на L1/L4 (17,9 МБ), а на остальных уровнях COL1+PRP2+резидентная часть занимают 13,7–14,6 МБ — 3–4 МБ участка пусты
// всю игру. Куски уровня, живущие ровно до его выгрузки (PVS1 1,3 МБ, PV2S 0,6 МБ, …), кладём в этот запас (lvaAllocSoft), а не в кучу: куча получает их размер назад. Резерв reserve — сколько ещё должен
// получить загрузчик (COL1/PRP2), чтобы мы не заняли их место (иначе они уйдут в кучу, как до №775). Не влезло — куча, как раньше. LVAMEM962_OLD=1 / файл lvamem962old рядом с EBOOT — всё в куче
#ifndef LVAMEM962_OLD
#define LVAMEM962_OLD 0
#endif
static int lvaMem962Old = LVAMEM962_OLD; static unsigned lvaSoftBytes, lvaSoftMiss;   // служебно: сколько байт легло в запас, сколько раз не влезло
static unsigned lvaTailReserve(int tailBg) {   // сколько ещё должен занять загрузчик в участке под COL1/PRP2, пока главный поток кладёт PVS1/PV2S в запас
  if (!tailBg) return 8u << 20;
  if (lvaTail962bOld) return (tailCol ? 0u : tailColSz) + (tailProp ? 0u : (2u << 20)) + (64u << 10);
  return (tailColAl ? 0u : tailColSz) + (tailPropAl ? 0u : (2u << 20)) + (64u << 10);
}
typedef struct { unsigned n, res, used; int ts, al, hit; } LvaSoftRec;   // служебно (№962б): каждый запрос в запас — размер, резерв, занято, состояние хвоста, занял ли загрузчик COL1/PRP2, попал ли
static LvaSoftRec lvaSoftRec[8]; static int lvaSoftRecN;
static void *lvaAllocSoft(unsigned n, unsigned reserve, int tailBg) {   // tailBg >= 0: резерв считается здесь, в момент запроса (загрузчик мог занять COL1/PRP2 после того, как вызывающий его прикинул), иначе заданный reserve
#ifdef TAILSLOW_MS
  for (int w_ = 0; w_ < 100 && tailState == 1 && !tailColAl; w_++) sceKernelDelayThread(2000);   // только стенд №962б: на PSP загрузчик успевает занять место под COL1 раньше разбора PVS, в эмуляторе нет
#endif
  if (tailBg >= 0) reserve = lvaTailReserve(tailBg);
  LvaSoftRec *rc_ = lvaSoftRecN < 8 ? &lvaSoftRec[lvaSoftRecN++] : 0; if (rc_) { rc_->n = n; rc_->res = reserve; rc_->used = lvaUsed; rc_->ts = tailState; rc_->al = tailColAl * 2 + tailPropAl; rc_->hit = 0; }
  if (!lvaMem962Old && lvaBase && lvaRoom(n + reserve)) { void *p = lvaAlloc(n); if (p && lvaOwns(p)) { lvaSoftBytes += n; if (rc_) rc_->hit = 1; return p; } if (p) return p; }
  if (!lvaMem962Old) lvaSoftMiss++;
  return memalign(64, n);
}
// №PFETCH (аудит времени загрузки 16.10): целые файлы traffic.shr (1,4 МБ) и fx.shr (0,5 МБ) читает загрузчик следом за COL1/PRP2, пока главный поток считает; трафик и частицы берут готовый буфер
// (pfTake в main_levels.h) и разбирают на прежнем месте — данные те же байты, порядок разбора прежний. PFETCH_OLD=1 / файл pfetchold рядом с EBOOT — читают сами, как раньше
#ifndef PFETCH_OLD
#define PFETCH_OLD 0
#endif
#define PF_MAX 4
typedef struct { char path[320]; char key[48]; void *buf; unsigned size; volatile int state; } PfSlot;   // state: 0 свободно, 1 идёт, 2 готово, 3 не вышло, 4 забрано
static PfSlot pf[PF_MAX]; static int pfOld = PFETCH_OLD;
// №PREREAD (слово дирижёра 16.10): пока газета влетает (0,9 с — раньше мёртвое время до чтения), загрузчик читает резидентную часть level.shr в буфер, который levelLoad заберёт как есть.
// Только когда набор уровня в памяти ещё не лежит (первый старт из меню): при смене уровня кучи на два набора нет. prState: 0 нет, 1 идёт, 2 готово, 3 не вышло. LOAD_PREREAD_OLD=1 / файл prereadold — читает главный, как раньше
#ifndef LOAD_PREREAD_OLD
#define LOAD_PREREAD_OLD 0
#endif
static volatile int prState; static unsigned prOff, prSize; static unsigned char *prBuf; static char prPath[256]; static int prereadOld = LOAD_PREREAD_OLD, prereadSum;
static unsigned levelPropSize(const unsigned char *ph, int pn) {   // размер секции PRP1/PRP2 по её заголовку; 0 — это не она
  if (!(pn >= 32 && (!memcmp(ph, "PRP1", 4) || (pn == 64 && !memcmp(ph, "PRP2", 4))))) return 0;
  const unsigned *pu = (const unsigned *)(ph + 4);
  unsigned sz = 32 + pu[3] + pu[4] + pu[0] * (unsigned)sizeof(PropPiece) + pu[1] * (unsigned)sizeof(PropType) + pu[2] * (unsigned)sizeof(PropDef) + pu[6] * 12u;
  if (!memcmp(ph, "PRP2", 4)) sz = *(const unsigned *)(ph + 40);
  return sz;
}
static int loaderThread(SceSize a, void *b) {
  SceUID fd = -1; unsigned gen = ~0u; fpuThreadInit();   // правка 252: ловушки FPU потока (viewer/fpuguard.h)
  for (;;) {
    loaderIdle = 1;
    sceKernelWaitSema(sema, 1, NULL);
    fpuThreadPoll(FPU_T_LOADER);
    loaderIdle = 0;
    for (;;) {                                       // пока в очереди есть незагруженное — грузим подряд
      if (prState == 1) {                            // №PREREAD: резидентная часть level.shr (после заголовка и lmap) в буфер главного потока
        loaderBusy = 1; SceUID pfd = sceIoOpen(prPath, PSP_O_RDONLY, 0); int ok = 0;
        if (pfd >= 0) { sceIoLseek32(pfd, prOff, PSP_SEEK_SET); unsigned o = prOff; ok = 1;
          while (o < prSize) { unsigned n = prSize - o; if (n > (512u << 10)) n = 512u << 10; if (sceIoRead(pfd, prBuf + o, n) != (int)n) { ok = 0; break; } o += n; }
          sceIoClose(pfd); }
        __asm__ volatile("" ::: "memory"); prState = ok ? 2 : 3; loaderBusy = 0; }
      if (tailState == 1) {                          // загрузка 2 п.6: COL1 и PRP2 уровня (задание главного потока)
        loaderBusy = 1; SceUID tf = sceIoOpen(levelShr, PSP_O_RDONLY, 0);
        if (tf >= 0) {
          if (tailColSz) { void *m = lvaAlloc(tailColSz); tailColAl = 1;
#ifdef TAILSLOW_MS
            sceKernelDelayThread(TAILSLOW_MS * 1000);   // только стенд №962б: чтение карты на PSP идёт дольше разбора PVS; эмулятор читает мгновенно
#endif
            sceIoLseek32(tf, tailColOff, PSP_SEEK_SET);
            if (m && sceIoRead(tf, m, tailColSz) == (int)tailColSz) tailCol = m; else lvaFree(m); }
          tailColAl = 1;
          if (tailPropOff) { unsigned char ph[64]; sceIoLseek32(tf, tailPropOff, PSP_SEEK_SET); int pn = sceIoRead(tf, ph, 64); unsigned sz = levelPropSize(ph, pn);
            if (sz) { void *m = lvaAlloc(sz); tailPropAl = 1; sceIoLseek32(tf, tailPropOff, PSP_SEEK_SET); if (m && sceIoRead(tf, m, sz) == (int)sz) { tailProp = m; tailPropSz = sz; } else lvaFree(m); } }
#ifdef TAILSLOW_MS
          sceKernelDelayThread(TAILSLOW_MS * 1000);
#endif
          tailPropAl = 1; sceIoClose(tf);
        }
        tailColAl = tailPropAl = 1;
        __asm__ volatile("" ::: "memory"); tailState = 2; loaderBusy = 0;
      }
      for (int ps = 0; ps < PF_MAX; ps++) if (pf[ps].state == 1) {   // №PFETCH: целый файл (после хвоста COL1/PRP2, до круга клеток)
        loaderBusy = 1; SceUID pfd = sceIoOpen(pf[ps].path, PSP_O_RDONLY, 0); void *pm = 0; unsigned psz = 0;
        if (pfd >= 0) { psz = (unsigned)sceIoLseek32(pfd, 0, PSP_SEEK_END); sceIoLseek32(pfd, 0, PSP_SEEK_SET); pm = psz ? memalign(64, psz) : 0;
          if (pm && sceIoRead(pfd, pm, psz) != (int)psz) { free(pm); pm = 0; } sceIoClose(pfd); }
        pf[ps].buf = pm; pf[ps].size = psz; __asm__ volatile("" ::: "memory"); pf[ps].state = pm ? 2 : 3; loaderBusy = 0; }
      if (ringState == 1) {                          // загрузка 2 п.5: круг клеток у старта (задание главного потока)
        loaderBusy = 1; SceUID rf = sceIoOpen(levelShr, PSP_O_RDONLY, 0);
        if (rf >= 0) { unsigned t0 = sceKernelGetSystemTimeLow(); cellsReadRuns(rf, ringIdx, ringN, &ringDone); sceIoClose(rf);
                       stLoadUs += sceKernelGetSystemTimeLow() - t0; stLoads += (unsigned)ringN; }
        __asm__ volatile("" ::: "memory"); ringState = 2; loaderBusy = 0;
      }
      if (pnoReq >= 0) pano917Serve();   // №917: слот кольца-панорамы
      if (marksReqTail != marksReqHead) marksServe();   // №912: мелочь разметки ближних клеток
      int n = lqn, i = -1;
      for (int k = 0; k < n && k < LQ_N; k++) { int j = lq[k]; if (j >= 0 && j < (int)ncell && cl[j].state == 0) { i = j; break; } }
      if (i >= 0 && (gen != lvGen || (!loaderFixOld && (loaderFdLost || (loaderErrRun >= 2 && loaderReopenN < 16))))) {   // новый уровень,
                                                       // файл отняли или чтения подряд падают (не больше 16 переоткрытий за запуск)
        int healing = gen == lvGen;
        if (fd >= 0 && !healing) { loaderSelfClose = 1; sceIoClose(fd); loaderSelfClose = 0; }   // при лечении номер, возможно, уже чужой — не закрывать
        if (healing) loaderReopenN++;
        loaderFdLost = 0; loaderErrRun = 0; fd = sceIoOpen(levelShr, PSP_O_RDONLY, 0); loaderFd = fd; gen = lvGen; }
      if (i < 0 || fd < 0) break;
      loaderBusy = 1; cl[i].state = 1;
      unsigned t0 = sceKernelGetSystemTimeLow();
      if (slowIoKBs) sceKernelDelayThread(6000 + cl[i].size * 1000u / slowIoKBs);   // ~6 мс на поиск + скорость чтения
      smokeLoadHook();   // служебное: отрицательный контроль delaycell (viewer/smoke.h)
      loadCell(fd, i);
      if (cl[i].state == 3 && (cellFail & 1)) loaderErrRun++; else if (cl[i].state == 2) loaderErrRun = 0;
      stLoadUs += sceKernelGetSystemTimeLow() - t0; stLoads++;
      if (cl[i].reqT) { unsigned lat = (sceKernelGetSystemTimeLow() - cl[i].reqT) / 1000u;   // П2: от запроса до готовности, мс
                        bbCellN++; if (lat > bbCellMax) bbCellMax = lat; if (lat > 100u) bbCellSlow++; accN++; accLatSum += lat; if (lat > accLatMax) accLatMax = lat; if (lat > 100u) accSlow++; cl[i].reqT = 0; }
      __asm__ volatile("" ::: "memory");
    }
    loaderBusy = 0;
  }
  return 0;
}
static void loaderEnsure(void) {   // поток чтения: один на запуск; поднимается при первом нужном (предчтение уровня на старте из меню идёт раньше инициализации главного цикла)
  if (loaderUp) return;
  sema = sceKernelCreateSema("load", 0, 0, 1, NULL);
  SceUID lt = sceKernelCreateThread("loader", loaderThread, 0x18, 0x4000, 0, 0);   // выше главного: чтение короткое, ждёт карту памяти
  if (lt >= 0) loaderUp = sceKernelStartThread(lt, 0, 0) >= 0;
}
// №LOADWAIT (Роб, TEST27/28: «зависло на загрузке с газетой», СТОП L6 на PSP; по фото главный и загрузчик оба «ждут»): ожидания главного потока флага, который ставит только загрузчик, были без
// тайм-аута. Теперь: нет движения > 2 с -> повторный SignalSema загрузчику (строка ящика «loadwait что_retryN»); для круга клеток и хвоста COL1/PRP2 после 3 повторов — мягкий выход
// (круг дочитывается главным, хвост читается синхронно), игра не висит. Для выгрузки уровня и ожидания простоя загрузчика выход небезопасен (загрузчик ещё пишет в клетки) — только повторы и строки.
// LOADWAIT_OLD=1 / файл loadwaitold / flag loadwaitold 1 — прежнее. Тест: flag loadwaitlose N — первые N сигналов загрузчику «теряются» (гейт tests/levels/loadwait.sh)
#ifndef LOADWAIT_OLD
#define LOADWAIT_OLD 0
#endif
static int loadWaitOld = LOADWAIT_OLD, lwLose, lwRetries, lwGiveups; static unsigned lwMs = 2000;   // lwMs: 2 с по умолчанию; тест (медленный безголовый эмулятор): flag loadwaitms N
static void ldSignal(void) { if (lwLose > 0) { lwLose--; return; } sceKernelSignalSema(sema, 1); }
typedef struct { unsigned last, prog; int n, init; } LdWait;
static int loaderWaitStep(LdWait *w, const char *what, unsigned prog, int canGiveUp) {   // 1 — выйти из ожидания (только если canGiveUp)
  unsigned now = sceKernelGetSystemTimeLow();
  if (!w->init || prog != w->prog) { if (!w->init && smkOn) { char s_[80]; snprintf(s_, sizeof s_, "LWINIT %s old=%d t=%.1f\n", what, loadWaitOld, smkT); smkWrite(s_); } w->init = 1; w->prog = prog; w->last = now; return 0; }
  if (loadWaitOld || now - w->last < lwMs * 1000u) return 0;
  w->last = now; char a[48];
  if (w->n < 3 || !canGiveUp) { w->n++; lwRetries++; snprintf(a, sizeof a, "%s_retry%d", what, w->n); bbOp("loadwait", a, 0); sceKernelSignalSema(sema, 1); }
  else { lwGiveups++; snprintf(a, sizeof a, "%s_giveup", what); bbOp("loadwait", a, 0); }
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "LOADWAIT %s n=%d giveup=%d t=%.1f\n", what, w->n, !(w->n < 3 || !canGiveUp), smkT); smkWrite(s_); }
  return !(w->n < 3 || !canGiveUp);
}
static void unloadCell(Cell *c) {
  dlFree((unsigned)(c - cl)); marksCellFree((unsigned)(c - cl));   // правка 275: лист клетки ссылается на её память — уходит вместе с ней
  if (c->state == 2) {
    for(unsigned k=c->first;k<c->first+c->n;k++){cpFree(pc[k].ggBatch);pc[k].ggBatch=0;cpFree(pc[k].unr);pc[k].unr=0;}
    loadedBytes -= c->size; cpFree(c->cg); c->cg = 0; c->cgs = 0; cgGen++; cpFree(c->mem); c->mem = 0; }
  c->state = 0; c->reqT = 0;
}
static float cellDist(Cell *c, float x, float y, float z) {
  float dx = c->cx - x, dy = c->cy - y, dz = c->cz - z;
  return sqrtf(dx * dx + dy * dy + dz * dz) - c->rad;
}
// выгрузить далёкое, отдать загрузчику очередь нужных клеток; 1 = ещё что-то грузится.
// Приоритет: расстояние до глаза или до точки, где камера будет через STREAM_AHEAD с (что меньше);
// клетки в кадре — вперёд. Радиус загрузки LOAD_DIST (с запасом над дальностью 115 м), выгрузки — DROP_DIST.
#define STREAM_AHEAD 2.5f
// Загрузка 2: порядок клеток по курсу и скорости. Глубина упреждения (STREAM_AHEAD с хода) растёт с потолком 160 м вместо 90, клетки впереди по
// движению идут раньше боковых и задних (бонус ±STREAM_FWD_M м к очереди), очередь загрузчику до 12 клеток на скорости > 12 м/с.
// STREAM_AHEAD_OLD=1 — прежнее (потолок 90 м, без курса, очередь 8)
#ifndef STREAM_AHEAD_OLD
#define STREAM_AHEAD_OLD 0
#endif
// №637 Роба (TEST23, «текстуры лагают, дальняя прорисовка плохая»): упреждение ×1,5 (глубина по скорости 3,75 с вместо 2,5, потолок 240 м) и запас выгрузки +40 м при развороте, пока куча не в обрезе (клетка за спиной не выгружается и не перечитывается, если
// игрок развернулся). STREAM_AHEAD15_OLD=1 — прежнее
// Ревью дифа 16.10: пользы по цифрам нет (очередь и недогруз те же, тур с откатами против текущих: свободно 7,08 против 6,79 МБ), поэтому по умолчанию ВЫКЛЮЧЕНО (было включено). Вернуть: -DSTREAM_AHEAD15_OLD=0
#ifndef STREAM_AHEAD15_OLD
#define STREAM_AHEAD15_OLD 1
#endif
#define STREAM_FWD_M 30.f
#define CELL_RETRY_US 1000000u
static unsigned cellFailT, cellRetries;   // правка 103: последняя попытка освободить место под клетку, сколько раз
// Замер железа 30.09 (ведущий, build/psp-logs/20260930-1904): на PSP свободно 9–10 МБ, эмулятор в тех же местах показывает 18–19.
// Клетки впрок (LOAD_DIST до 325 м, DROP_DIST 395 м при дальности 250 м) забирают остаток кучи, и отказ получает уже не клетка, а
// комната, машина миссии, звук. Резерв кучи MEM_RESERVE: ниже него впрок не грузим (только ближе дальности + 20 м), а загруженное
// дальше дальности + 20 м (его не видно) отдаём, начиная с самых дальних, пока свободно не станет MEM_RESERVE + MEM_HYST.
// Видимое не трогаем — картинка та же. Проверка кучи не чаще раза в MEM_CHECK_US.
#define MEM_RESERVE (3u << 20)
#define MEM_HYST (1u << 20)
#define MEM_CHECK_US 250000u
static unsigned stCand;   // клеток в радиусе загрузки ещё не взято в работу (очередь ожидания) — на последнем кадре stream()
// №1060 (скорость при дали 250 м, тур TEST73 на PSP: MEMTRIM сработал 67 раз против 0 в TEST71, по 44–87 клеток за один кадр stream() — кадры 28–54 мс; у L4_CR1 86 из 503 кадров дольше 45 мс, p95 61,8 мс):
// выгрузка дальних клеток при тесной куче растягивается на кадры, MEMTRIM1060_STEP клеток за кадр (те же клетки в том же порядке — дальше дальности + 20 м, их не видно; картинка та же), пока
// не вернётся MEM_RESERVE + MEM_HYST. MEMTRIM1060_OLD=1 / файл memtrim1060old рядом с EBOOT / flag memtrim1060 0 — всё разом, как было
#ifndef MEMTRIM1060_OLD
#define MEMTRIM1060_OLD 0
#endif
#ifndef MEMTRIM1060_STEP
#define MEMTRIM1060_STEP 4
#endif
static int memTrimPend1060, memTrim1060 = !MEMTRIM1060_OLD; static unsigned memTrimAcc1060;
// №1060в (разбор тура TEST75 на L4: MEMTRIM1060 крутил «выгрузил — тут же загрузил»: выгружено 7079 клеток в 6 непрерывных эпизодах (3056 за один в 973_L4 при 1138 клетках уровня) против 3157 в 58 всплесках у TEST74, загрузок клеток
// в секунду +24…28 на L4, stream() +1,1 мс за кадр): эпизод кончался по ИЗМЕРЕННОЙ куче каждый кадр, а загрузчик сразу занимал освобождённое — условие не наступало. Теперь (1) эпизод кончается по оценке байт, посчитанной на его старте
// и накопленной между кадрами (как прежний всплеск: got += размер клетки), кучу внутри эпизода не перемеряет; (2) клетки, выгруженные в этом эпизоде, загрузчик не берёт, пока эпизод идёт — каждая выгружается не больше одного раза за эпизод.
// MEMTRIM2_OLD=1 / файл memtrim2old рядом с EBOOT / flag memtrim2 0 — как MEMTRIM1060 (перемер каждый кадр); отдельно от MEMTRIM1060_OLD
#ifndef MEMTRIM2_OLD
#define MEMTRIM2_OLD 0
#endif
static int memTrim2 = !MEMTRIM2_OLD; static unsigned memTrimGot2;
#define MT2_MAX 4096u
static unsigned char memTrimSet2[MT2_MAX], memTrimCnt2[MT2_MAX]; static unsigned memTrimEpN2;   // клетки эпизода (метка «не грузить») и сколько раз каждая выгружена за эпизод (служебно, строка MEMTRIM2)
static unsigned memChkT, memFreeNow = ~0u, memTrims, memTrimBytes; static int memTight;
static int memPrefetchRoom(unsigned need) { return memFreeNow == ~0u || memFreeNow >= MEM_RESERVE + need; }   // ревизия 1.1: чтение моделей впрок (msn_view.c mvHeapRoom) не съедает резерв стриминга
static float stVx, stVz, stPx, stPz; static unsigned stT; static int stInit, oldStream;   // oldStream: служебное сравнение со старым (1 клетка за кадр, 150/200 м)
// Большой прыжок глаза (goto сценария, выход из ролика, перенос миссии): обычная очередь стрима даёт загрузчику 8 клеток в кадр и по одной,
// на прыжке видимых недогруженных клеток десятки (L6 мост: до 70, 19 кадров). Клетки впереди по взгляду и ближние (до дальности) уходят
// загрузчику одним заданием, укрупнёнными чтениями (то же задание, что круг клеток у старта). JUMPRUSH_OLD=1 — как раньше
// Загрузка 2: ворота подгрузки (zonegate.h) включают регионы воротами: в кадре срабатывания 20+ клеток региона разом недогружены (L3, ворота 11 около
// (46; −30): MISSCELL 19 клеток). Регионы ворот рядом по ходу (до GATE_PRE_M, впереди по ходу или совсем рядом) читаются заранее, но не рисуются, пока ворота не сработали
// (cullCity берёт набор regRes). Не при нехватке памяти. GATE_PRE_OLD=1 — как раньше (читается только после срабатывания)
#ifndef GATE_PRE_OLD
#define GATE_PRE_OLD 0
#endif
static int gatePreOn = !GATE_PRE_OLD;   // «flag gatepre 0/1» в сценарии (для A/B в одном туре на одном маршруте); по умолчанию как собрано
// №REGPRE-гистерезис (16.10, аудит TEST26: «мигание REGPRE на первой ходьбе из гаража» — 8 включений при свободных 8–10 МБ, ворота 7 МБ не закрывались): условие «рядом / впереди по ходу»
// пересчитывалось с нуля каждый кадр, и регион мигал на границе GATE_PRE_M и на шуме вектора хода (скорость около 4 м/с, угол около 0,3). Теперь регион, однажды взятый впрок,
// держится ещё REGPRE_HOLD_S с после последнего «да» и пока игрок ближе GATE_PRE_M + REGPRE_OUT_M (вход строго прежний, выход с запасом по дистанции и по времени).
// REGPRE_HYST_OLD=1 или «flag regprehyst 0» — прежнее, без удержания
#ifndef REGPRE_HYST_OLD
#define REGPRE_HYST_OLD 0
#endif
#ifndef REGPRE_HOLD_S
#define REGPRE_HOLD_S 4.f
#endif
#ifndef REGPRE_OUT_M
#define REGPRE_OUT_M 40.f
#endif
static int regPreHyst = !REGPRE_HYST_OLD;
#include "regpre.h"
static int regPreMemOld = REGPRE_MEM_OLD;   // Б TEST57: «flag regprememold 1» / файл regprememold рядом с EBOOT — прежнее условие по памяти
#ifndef GATE_PRE_M
#define GATE_PRE_M 90.f
#endif
static unsigned regPre;   // регионы ворот рядом (читаем заранее)
#ifndef JUMPRUSH_OLD
#define JUMPRUSH_OLD 0
#endif
#ifndef JUMP_CONE
#define JUMP_CONE .55f   // косинус полуугла конуса отбора (±57°; кадр PSP ±~35°, остальное — запас на поворот и размер клетки)
#endif
#ifndef JUMP_RUSH_M
#define JUMP_RUSH_M 60.f   // глаз сместился между кадрами больше, м
#endif
static int stMissNow;   // (ниже — полное объявление счётчиков)
static float stFx, stFz;   // горизонтальный взгляд камеры этого кадра (main_loop.h кладёт перед stream)
static unsigned jumpT0, jumpN, jumpMissFr; static int jumpWatch, jumpMissMax;   // служебное: когда после прыжка видимых недогруженных стало 0 (строка JUMPDONE)
static void jumpRush(float x, float y, float z, float moved, unsigned now) {
  if (JUMPRUSH_OLD || !loaderUp || ringState == 1) return;
  if (heapFreeBytes() < MEM_RESERVE + (2u << 20)) return;   // мало кучи: обычный путь (он считает резерв)
  ringState = 0; int n = 0; unsigned bytes = 0; float fl = sqrtf(stFx * stFx + stFz * stFz);
  for (unsigned i = 0; i < ncell && n < RING_MAX; i++) {
    Cell *c = &cl[i]; if (c->owner || c->state) continue;
    if (zgOn && CELL_REG(c) && CELL_REG(c) <= 16 && !(((regRes | regPre) >> (CELL_REG(c) - 1)) & 1u)) continue;
    float dx = c->cx - x, dz = c->cz - z, d = sqrtf(dx * dx + (c->cy - y) * (c->cy - y) + dz * dz) - c->rad;
    if (d >= drawDist + 10.f) continue;
    if (fl < .5f && d > 80.f) continue;   // камера круто вниз/вверх (направления на земле нет): только ближние, остальное — обычная очередь
    if (fl >= .5f && d > 40.f) { float hl = sqrtf(dx * dx + dz * dz) + .01f;   // вне конуса взгляда (±57° с поправкой на размер клетки) и не рядом — обычным путём
      if ((dx * stFx + dz * stFz) / (fl * hl) < JUMP_CONE - c->rad / hl) continue; }
    if (bytes + c->size > (3u << 20)) break;
    ringIdx[n++] = (short)i; bytes += c->size;
  }
  if (n < 4) return;
  ringN = n; ringDone = 0; __asm__ volatile("" ::: "memory"); ringState = 1; ldSignal();
  jumpN = (unsigned)n;
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "JUMPRUSH moved=%.0f n=%d kb=%u look=%.2f,%.2f t=%.2f\n", moved, n, bytes >> 10, stFx, stFz, smkT); smkWrite(s); }
}
// №864 пропечка 2 «загрузка вдоль дороги» (окно «Оптимизация», как у Crash Bandicoot: кто едет по дороге, тот уже на 2-3 с вперёд знает, какие куски уровня ему нужны).
// tools/roadq864.py: по дорожному графу traffic.shr для каждой клетки и каждого из 8 направлений хода (0 = +Z, дальше по 45 градусов к +X) до RQ_M клеток впереди по дороге в пределах 260 м,
// ближние первыми (roadq864.bin рядом с level.shr, формат RQ1). Движение быстрее 4 м/с по такой клетке: клетки списка идут в очередь загрузчика раньше остальных (бонус RQ_BONUS_M м)
// и берутся дальше обычного радиуса загрузки (до 260 м). Не при нехватке памяти (memTight) и без запаса кучи (memRoom). Нет файла, не сошлось число клеток или ROADQ_OLD=1 / «flag roadqold 1» — как раньше.
// ПО УМОЛЧАНИЮ ВЫКЛЮЧЕНО (ROADQ_OLD=1): замер езды (тур по 7 уровням, автопилот ≈10 м/с, и с медленной картой 2 МБ/с) пропусков видимых клеток не показал ни с таблицей, ни без (0 и 0);
// счёт загрузок от прогона к прогону гуляет на 20-40 % и на одном и том же коде, различия не доказывает. Включить: -DROADQ_OLD=0 (тогда «flag roadqold 1/0» в сценарии переключает на лету), файл roadq864.bin едет по ship_rules
#ifndef ROADQ_OLD
#define ROADQ_OLD 1
#endif
#define RQ_BONUS_M 70.f
#define RQ_REACH_M 260.f
static int roadqOld = ROADQ_OLD;
static unsigned *rqOff; static unsigned short *rqEnt; static unsigned rqN, rqTot;   // таблица RQ1 текущего уровня (rqOff — (ncell * 8 + 1) смещений)
static unsigned char *rqMk;   // отметка клетки «впереди по дороге» на этот кадр (ncell байт)
static unsigned rqFrames, rqMarked, rqQueued;   // кадров с курсом по дороге, отмеченных клеток, из них реально поставлено в очередь (state 0)
static int rqCur = -1;
static void roadqFree(void) { free(rqOff); free(rqEnt); free(rqMk); rqOff = 0; rqEnt = 0; rqMk = 0; rqN = rqTot = 0; rqCur = -1; }
static void roadqLoad(const char *fn, unsigned ncell_) {
  roadqFree(); if (ROADQ_OLD || roadqOld || !ncell_) return;
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  unsigned hd[4]; int ok = sceIoRead(f, hd, 16) == 16 && hd[0] == 0x00315152u && hd[1] == ncell_ && hd[2] == 8;   // 'RQ1\0'
  unsigned n = ok ? ncell_ * 8u + 1u : 0;
  rqOff = ok ? (unsigned *)malloc(n * 4u) : 0; rqEnt = ok && hd[3] ? (unsigned short *)malloc(hd[3] * 2u) : 0;
  if (!rqOff || !rqEnt || sceIoRead(f, rqOff, (int)(n * 4u)) != (int)(n * 4u) || sceIoRead(f, rqEnt, (int)(hd[3] * 2u)) != (int)(hd[3] * 2u) || rqOff[n - 1] != hd[3]) { sceIoClose(f); roadqFree(); return; }
  sceIoClose(f); rqMk = (unsigned char *)calloc(ncell_, 1); if (!rqMk) { roadqFree(); return; }
  rqN = ncell_; rqTot = hd[3]; rqFrames = rqMarked = rqQueued = 0;
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "ROADQ level=%d ncell=%u entries=%u kb=%u\n", gameLevel, ncell_, hd[3], (n * 4u + hd[3] * 2u) >> 10); smkWrite(s); }
}
static int stream(float x, float y, float z) {
  unsigned now = sceKernelGetSystemTimeLow();
  float jumpMoved = stInit ? hypotf(x - stPx, z - stPz) : 0.f;
  if (ringState == 2) ringState = 0;   // задание круга/прыжка выполнено и принято
  if (jumpWatch) { if (stMissNow > 0) { jumpMissFr++; if (stMissNow > jumpMissMax) jumpMissMax = stMissNow; }
    if (stMissNow == 0 && now - jumpT0 > 30000u) { jumpWatch = 0;
      if (smkOn) { char s[112]; snprintf(s, sizeof s, "JUMPDONE n=%u ms=%u missframes=%u maxmiss=%d t=%.2f\n", jumpN, (now - jumpT0) / 1000u, jumpMissFr, jumpMissMax, smkT); smkWrite(s); } } }
  if (!stInit) { stInit = 1; stPx = x; stPz = z; stT = now; }
  float dt = (now - stT) * 1e-6f;
  if (dt > 0.f) {
    float vx = (x - stPx) / dt, vz = (z - stPz) / dt;
    if (vx * vx + vz * vz > 120.f * 120.f) vx = vz = 0.f;          // телепорт (goto, сброс) — не скорость
    float k = dt * 4.f; if (k > 1.f) k = 1.f;
    stVx += (vx - stVx) * k; stVz += (vz - stVz) * k;
  }
  stPx = x; stPz = z; stT = now;
  int memRoom = memFreeNow == ~0u || memFreeNow >= MEM_RESERVE + (4u << 20);   // железо TEST23: MEMTRIM 79 клеток разом и отказ памяти у будки при 10 МБ (дробление кучи) — упреждение ×1,5 только при запасе
  float aMul = STREAM_AHEAD_OLD || STREAM_AHEAD15_OLD || !memRoom ? 1.f : 1.5f, ax = stVx * STREAM_AHEAD * aMul, az = stVz * STREAM_AHEAD * aMul, al = sqrtf(ax * ax + az * az);
  float loadD = oldStream ? 150.f : LOAD_DIST, dropD = oldStream ? 200.f : DROP_DIST; int qmax = oldStream ? 1 : LQ_N;
  if (oldStream) ax = az = 0.f;
  float aheadMax = STREAM_AHEAD_OLD ? 90.f : STREAM_AHEAD15_OLD || !memRoom ? 160.f : 240.f, spd = sqrtf(stVx * stVx + stVz * stVz), dirx = spd > 4.f ? stVx / spd : 0.f, dirz = spd > 4.f ? stVz / spd : 0.f;
  if (al > aheadMax) { ax *= aheadMax / al; az *= aheadMax / al; }
  if (!oldStream) qmax = !STREAM_AHEAD_OLD && spd > 12.f ? LQ_N : 8;   // 8 в обычной езде, 12 на скорости
  float lkx = 0.f, lkz = 0.f, lkl = sqrtf(stFx * stFx + stFz * stFz); if (streamAhead1042 && !oldStream && lkl > 1e-3f) { lkx = stFx / lkl; lkz = stFz / lkl; }   // №1042: взгляд камеры (горизонтально)
  if (streamAhead1042 && !oldStream && spd > 4.f && al < STA1042_MINAHEAD) { ax = dirx * STA1042_MINAHEAD; az = dirz * STA1042_MINAHEAD; }   // №1042: на ходу заглядываем не ближе 40 м
  float px = x + ax, pz = z + az;
  if (!oldStream && (now - memChkT > MEM_CHECK_US || memTrimPend1060)) {   // резерв кучи (замер железа 30.09, см. MEM_RESERVE); №1060: недоделанная выгрузка — в каждом кадре
    memChkT = now; memFreeNow = heapFreeBytes(); memTight = memFreeNow < MEM_RESERVE;
    if (memTight || memTrimPend1060) {
      unsigned want = MEM_RESERVE + MEM_HYST, got = memFreeNow; int n = 0; const int nmax = memTrim1060 ? MEMTRIM1060_STEP : 1 << 20;
      if (memTrim2 && memTrimPend1060) got = memTrimGot2;   // №1060в: продолжение эпизода — оценка байт с его старта, куча внутри эпизода не перемеряется
      memTrimPend1060 = 0;
      while (got < want) {
        if (n >= nmax) { memTrimPend1060 = 1; break; }   // №1060: остаток — в следующих кадрах
        int far = -1; float fd = drawDist + 20.f;
        for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || c->state != 2) continue;
          float d = cellDist(c, x, y, z); if (lkl > 1e-3f && streamAhead1042) { int sd_ = sta1042Side(c, x, z, lkx, lkz); d += sd_ > 0 ? -STA1042_KEEP : sd_ < 0 ? 40.f : 0.f; }   // №1042
          if (d > fd) { fd = d; far = (int)i; } }
        if (far < 0) break;
        unsigned cf0 = cpChunksFreed, csz = cl[far].size; memTrimBytes += csz; unloadCell(&cl[far]); n++;
        if ((unsigned)far < MT2_MAX) { memTrimSet2[far] = 1; if (memTrimCnt2[far] < 255) memTrimCnt2[far]++; }
        got += CELLPOOL_OLD || csz > CP_DIRECT ? csz : (cpChunksFreed - cf0) * CP_CHUNK;   // пул: место вернулось в кучу только вместе с освободившимся куском
      }
      memTrimGot2 = got;
      memTrims += (unsigned)n; memTrimAcc1060 += (unsigned)n;
      if (smkOn && memTrimAcc1060 && !memTrimPend1060) { char s[112]; snprintf(s, sizeof s, "MEMTRIM freed=%d heap=%.2f t=%.1f\n", (int)memTrimAcc1060, heapFreeBytes() / 1048576.f, smkT); smkWrite(s); }
      if (!memTrimPend1060) { unsigned dup_ = 0, tot_ = 0, mx_ = 0; for (unsigned k_ = 0; k_ < MT2_MAX && k_ < ncell; k_++) if (memTrimCnt2[k_]) { tot_++; if (memTrimCnt2[k_] > 1) dup_++; if (memTrimCnt2[k_] > mx_) mx_ = memTrimCnt2[k_]; }
        if (smkOn && tot_) { char s2_[128]; snprintf(s2_, sizeof s2_, "MEMTRIM2 on=%d cells=%u unique=%u dup=%u maxper=%u t=%.1f\n", memTrim2, memTrimAcc1060, tot_, dup_, mx_, smkT); smkWrite(s2_); }
        memset(memTrimSet2, 0, sizeof memTrimSet2); memset(memTrimCnt2, 0, sizeof memTrimCnt2); memTrimAcc1060 = 0; }
    }
  }
  if (memTight && loadD > drawDist + 20.f) loadD = drawDist + 20.f;   // мало памяти: впрок не грузим
  if (stBoothHold1051 && boothMem1051) loadD = BOOTH1051_HOLD_R;   // №1051: открыта будка — город не подгружаем, пока она закрывает экран
  // Правка Роба 103 («не прогрузился кусок карты»): клетка без памяти (state 3) раньше оставалась дырой, пока игрок не отъедет за
  // DROP_DIST (~400 м). Теперь не чаще раза в CELL_RETRY с: загруженные клетки дальше дальности + 20 м (их не видно) — в кучу,
  // неудачные — снова в очередь загрузчика. Первыми освобождаются самые дальние: сброс всех невидимых разом освобождает и соседние
  // куски кучи (клетки разного размера дробят её)
  if (cellFail && now - cellFailT > CELL_RETRY_US) {
    int oom = cellFail & 2; cellFail = 0; cellFailT = now; int freed = 0, again = 0;
    for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner) continue;
      if (oom && c->state == 2 && cellDist(c, x, y, z) > drawDist + 20.f) { unloadCell(c); freed++; }   // ошибке чтения место не нужно
      else if (c->state == 3) { c->state = 0; again++; } }
    cellRetries++;
    if (smkOn) { char s[112]; snprintf(s, sizeof s, "CELLRETRY %s freed=%d again=%d heap=%.2f t=%.1f\n", oom ? "mem" : "read", freed, again, heapFreeBytes() / 1048576.f, smkT); smkWrite(s); }
  }
  static unsigned regPreLockT; unsigned regPreWas = regPre; regPre = 0;   // Б TEST57: что было взято в прошлом кадре — для зазора по памяти (regpre.h)
  if (gatePreOn && zgOn && !memTight && zg && regPreMemOk(memFreeNow, MEM_RESERVE, regPreWas != 0, now, &regPreLockT, regPreMemOld)) {   // и кучи с запасом: регионы впрок — это МБ
    for (int i = 0; i < nzg; i++) { float dx = zg[i].c[0] - x, dz = zg[i].c[2] - z;
      int yes = regPreYes(i, dx, dz, dirx, dirz, spd, now, regPreHyst);   // viewer/regpre.h: вход как раньше, выход с удержанием
      if (yes) regPre |= zg[i].load; }
  }
  { static unsigned rpPrev; if (smkOn && regPre != rpPrev) { char s_[96]; snprintf(s_, sizeof s_, "REGPRE %04x->%04x res=%04x at %.0f,%.0f t=%.1f\n", rpPrev, (unsigned)regPre, (unsigned)regRes, x, z, smkT); smkWrite(s_); rpPrev = regPre; } }
  short best[LQ_N]; float bs[LQ_N]; int nb = 0; stCand = 0;
  int rqOn = 0; unsigned rqE0 = 0, rqE1 = 0;   // №864: список клеток впереди по дороге для клетки под игроком и курса хода
  if (rqN == ncell && rqMk && !roadqOld && !oldStream && !memTight && memRoom && spd > 4.f) {
    int cur = -1;
    if (rqCur >= 0 && rqCur < (int)ncell) { const Cell *c = &cl[rqCur]; if (!c->owner && x >= c->bb[0] && x <= c->bb[3] && z >= c->bb[2] && z <= c->bb[5] && y >= c->bb[1] - 6.f && y <= c->bb[4] + 6.f) cur = rqCur; }
    for (unsigned i = 0; cur < 0 && i < ncell; i++) { const Cell *c = &cl[i]; if (c->owner) continue;
      if (x >= c->bb[0] && x <= c->bb[3] && z >= c->bb[2] && z <= c->bb[5] && y >= c->bb[1] - 6.f && y <= c->bb[4] + 6.f) cur = (int)i; }
    rqCur = cur;
    if (cur >= 0) {
      int b = (int)lrintf(atan2f(dirx, dirz) * 1.27323954f); b &= 7;   // 4 / pi: сектора по 45 градусов, 0 = +Z
      unsigned k = (unsigned)cur * 8u + (unsigned)b; rqE0 = rqOff[k]; rqE1 = rqOff[k + 1];
      for (unsigned e = rqE0; e < rqE1; e++) { rqMk[rqEnt[e]] = 1; rqOn++; }
      if (rqOn) { rqFrames++; rqMarked += (unsigned)rqOn; }
    }
  }
  for (unsigned i = 0; i < ncell; i++) {
    Cell *c = &cl[i]; if (c->owner) continue;
    if (zgOn && CELL_REG(c) && CELL_REG(c) <= 16 && !(((regRes | regPre) >> (CELL_REG(c) - 1)) & 1u)) {   // правка 181: регион не в наборе ворот — не держим
      if (c->state >= 2) { unloadCell(c); zgUnloads++; } else if (c->state == 3) c->state = 0;
      continue;
    }
    float dx = c->cx - x, dy = c->cy - y, dz = c->cz - z, d2 = dx * dx + dy * dy + dz * dz;
    float qx = c->cx - px, qz = c->cz - pz, q2 = qx * qx + dy * dy + qz * qz;
    if (c->state >= 2) {
      float lim = dropD + c->rad + (!STREAM_AHEAD_OLD && !STREAM_AHEAD15_OLD && memRoom && !oldStream ? 40.f : 0.f);
      if (d2 > lim * lim && q2 > lim * lim) unloadCell(c);
    } else if (c->state == 0) {
      if (memTrim2 && memTrimPend1060 && i < MT2_MAX && memTrimSet2[i]) { c->reqT = 0; continue; }   // №1060в: клетку, выгруженную в этом эпизоде, загрузчик не берёт, пока эпизод идёт
      float lim = loadD + c->rad; int rqm = rqOn && rqMk[i];
      int sd1042 = lkl > 1e-3f && streamAhead1042 ? sta1042Side(c, x, z, lkx, lkz) : 0; if (sd1042 > 0) lim += 40.f;   // №1042: клетка в конусе взгляда — радиус загрузки +40 м и стоя
      if (rqm && lim < RQ_REACH_M + c->rad) lim = RQ_REACH_M + c->rad;   // №864: клетка впереди по дороге — берём дальше обычного радиуса
      if (d2 >= lim * lim && q2 >= lim * lim) { c->reqT = 0; continue; }   // П2: ушла из радиуса — запрос снят
      stCand++;
      float d = sqrtf(d2) - c->rad, q = sqrtf(q2) - c->rad + 15.f, sc = d < q ? d : q;
      if (rqm) { sc -= RQ_BONUS_M; rqQueued++; }
      if (!oldStream && d < drawDist && visBox(c->bb, 0, 0, 0)) sc -= 60.f;   // уже должна быть видна — первой
      if (streamAhead1042 && !oldStream) { int vis_ = d < drawDist && visBox(c->bb, 0, 0, 0);
        if (vis_ && d < STA1042_HEAD) { sc -= 1000.f; sta1042Head++; }   // №1042: видимая ближе 120 м — голова очереди
        else if (sd1042 > 0) { sc -= 40.f; sta1042Cone++; } }   // №1042: в конусе взгляда — раньше остальных, и стоя
      if (!STREAM_AHEAD_OLD && spd > 4.f) { float al_ = (c->cx - x) * dirx + (c->cz - z) * dirz; sc += al_ > -c->rad ? -STREAM_FWD_M : (al_ < -c->rad - 20.f ? STREAM_FWD_M : 0.f); }   // впереди по ходу — раньше, сзади — позже
      if (nb == qmax && sc >= bs[nb - 1]) continue;
      int j = nb < qmax ? nb++ : nb - 1;
      while (j > 0 && bs[j - 1] > sc) { bs[j] = bs[j - 1]; best[j] = best[j - 1]; j--; }
      bs[j] = sc; best[j] = (short)i;
    }
  }
  for (unsigned e = rqE0; e < rqE1; e++) rqMk[rqEnt[e]] = 0;
  lqn = 0; __asm__ volatile("" ::: "memory");
  for (int k = 0; k < nb; k++) { lq[k] = best[k]; if (!cl[best[k]].reqT) cl[best[k]].reqT = sceKernelGetSystemTimeLow() | 1u; }   // П2
  __asm__ volatile("" ::: "memory");
  lqn = nb;
  if (!oldStream && jumpMoved > JUMP_RUSH_M) { jumpWatch = 1; jumpT0 = now; jumpN = jumpMissFr = 0; jumpMissMax = 0; jumpRush(x, y, z, jumpMoved, now); }
  if (nb && !loaderBusy) sceKernelSignalSema(sema, 1);
  return nb > 0 || loaderBusy || ringState == 1;
}
// №637: цельный кусок под крупное одноразовое выделение (запись art миссии, комната): далёкие клетки (дальше дальности + 20 м, их не видно) уходят
// по одной, самая дальняя первой, пока самый большой свободный кусок не станет >= need (не больше 120 клеток за вызов). Только главный поток
// №1060: общий приём «освободить → загрузить → вернуть» (описание — viewer/mem1060.h). Здесь его низ, доступный раньше всех: ступень «все клетки» под закрытым экраном
#ifndef MEM1060_OLD
#define MEM1060_OLD 0
#endif
static int mem1060 = !MEM1060_OLD;
static int ldOn;   // main_frontend.h: идёт загрузка уровня (экран загрузки закрывает мир)
static int viewBusyNow(void);   // main_frontend.h: будка, лавка или гардероб закрывают мир
static int cellsDropBeyond(float r);
static int cellsMakeRoom(unsigned need) {
  if (!memMaxBlock) return 0;
  int n = 0; unsigned best = memMaxBlock(), mark = 0;   // не помогает (дробят долгоживущие блоки, не клетки) — бросить после 24 клеток без роста, чтобы не гонять перечитывание впустую
  while (best < need && n < 120) {
    int far = -1; float fd = drawDist + 20.f;
    for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || c->state != 2) continue;
      float d = cellDist(c, eyeX, eyeY, eyeZ); if (streamAhead1042 && (stFx != 0.f || stFz != 0.f)) { float l_ = sqrtf(stFx * stFx + stFz * stFz); int sd_ = sta1042Side(c, eyeX, eyeZ, stFx / l_, stFz / l_); d += sd_ > 0 ? -STA1042_KEEP : sd_ < 0 ? 40.f : 0.f; }   // №1042
      if (d > fd) { fd = d; far = (int)i; } }
    if (far < 0) break;
    unloadCell(&cl[far]); n++;
    unsigned b = memMaxBlock(); if (b > best) { best = b; mark = (unsigned)n; } else if ((unsigned)n - mark >= 24u) break;
  }
  if (mem1060 && memMaxBlock() < need && (ldOn || viewBusyNow())) {   // №1060: далёких клеток не хватило, а мир закрыт экраном (газета, будка, лавка, гардероб) — все клетки уходят в кучу, стрим вернёт их сам
    unsigned f0_ = heapFreeBytes(), b0_ = memMaxBlock();
    lqn = 0; __asm__ volatile("" ::: "memory");
    { LdWait lw_ = {0}; while (loaderBusy) { loaderWaitStep(&lw_, "busy", 0, 0); sceKernelDelayThread(1000); } }
    int all_ = cellsDropBeyond(-1.f); n += all_;
    if (smkOn) { char s[160]; snprintf(s, sizeof s, "MEM1060 all need=%.1f dropped=%d free=%.2f->%.2f blk=%.2f->%.2f t=%.1f\n", need / 1048576.f, all_, f0_ / 1048576.f, heapFreeBytes() / 1048576.f, b0_ / 1048576.f, memMaxBlock() / 1048576.f, smkT); smkWrite(s); }
  }
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "MAKEROOM need=%.1f freed=%d blk=%.2f t=%.1f\n", need / 1048576.f, n, memMaxBlock() / 1048576.f, smkT); smkWrite(s); }
  return memMaxBlock() >= need;
}
// №842 (Роб 19.10, L3 F4-8, TEST38: «у телефонной будки TELEPHONE BOOTH UNAVAILABLE»): будка (phone.shr 3,1 МБ + модели) не влезла — cellsMakeRoom
// отдаёт только клетки дальше дальности + 20 м (при Draw Distance 250 м их почти нет). Будка закрывает весь экран, город за ней не виден: все клетки
// дальше r метров — в кучу (после будки загрузчик вернёт их сам, ближние остаются). Возвращает число выгруженных
#ifndef PHONE_ROOM842_OLD
#define PHONE_ROOM842_OLD 0
#endif
static int phoneFailTest;   // №842 тест: «flag phonefail N»
// №1060 служебное («flag boothstarve N»): потребитель уровня, который забирает всё освобождённое, оставляя кучу не больше N МБ свободной (модель долгоживущих блоков кроме клеток: движок их сам не отдаёт).
// Отдаёт всё при выгрузке уровня (levelUnload) — как отдавали бы они. Гейт tests/mem1060: красный контроль (выключатель) доходит до «unavailable», зелёный — пересборкой
static float starve1060 = -1.f;
static void heapLeft(float mb);   // main_levels.h
static int cellsDropBeyond(float r) {
  int n = 0;
  for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || c->state != 2) continue;
    if (cellDist(c, eyeX, eyeY, eyeZ) > r) { unloadCell(c); n++; } }
  if (n && starve1060 >= 0.f) heapLeft(starve1060);
  return n;
}
// №1060: перед крупным выделением на закрытом экране (ролик, комната, бонус-игра): наибольшего блока хватает — ничего (цена ноль); нет — далёкие клетки по одной, затем все. 1 — место есть
static int mem1060Pre(unsigned need, const char *who) {
  if (!mem1060 || !memMaxBlock || memMaxBlock() >= need) return 1;
  unsigned f0 = heapFreeBytes(), b0 = memMaxBlock(); int ok = cellsMakeRoom(need);
  if (smkOn) { char s[160]; snprintf(s, sizeof s, "MEM1060 pre %s need=%.1f ok=%d free=%.2f->%.2f blk=%.2f->%.2f t=%.1f\n", who, need / 1048576.f, ok, f0 / 1048576.f, heapFreeBytes() / 1048576.f, b0 / 1048576.f, memMaxBlock() / 1048576.f, smkT); smkWrite(s); }
  return ok;
}
// №1051 (Роб, TEST72 на PSP, L5 J4-7: «почему телефонная будка unavailable? Всё лагает, почему оно всё держит в памяти»): при дальности 250 м (FAR1042) и стриме вперёд (№1042) куча занята клетками,
// и пакет будки (3,1 МБ несколькими кусками) не влезал, а одна попытка №842 («клетки дальше 50 м») шла уже ПОСЛЕ неудачи и догружалась стримом назад. Теперь будка открывается всегда: ещё до первой
// попытки дальние клетки уходят в кучу (дальше BOOTH1051_R1 м), на время будки радиус подгрузки малый (BOOTH1051_HOLD_R м), не вышло — ступенями: дальше 20 м, потом все чужие клетки; сначала место, потом
// проверка «unavailable». Закрыли будку — стрим догружает клетки сам. BOOTHMEM1051_OLD=1 / файл boothmem1051old / flag boothmem1051 0 — как было (№842, одна попытка после неудачи)
static void bbNote(const char *line);   // main_frontend.h
static unsigned boothHogN1051; static void *boothHog1051[256];   // служебное (flag boothhog МБ): занять кучу блоками по 256 КБ — имитация нехватки памяти для гейта
static void boothHog1051Set(int mb) { while (boothHogN1051) free(boothHog1051[--boothHogN1051]); for (int i = 0; i < mb * 4 && i < 256; i++) { void *m = malloc(256u << 10); if (!m) break; memset(m, 1, 256u << 10); boothHog1051[boothHogN1051++] = m; } }
static void boothMem1051Log(const char *what, int dropped, unsigned f0, unsigned b0) {
  char s_[200]; snprintf(s_, sizeof s_, "BOOTH1051 %s level=%d dropped=%d free=%.2f->%.2f blk=%.2f->%.2f dd=%.0f t=%.1f\n", what, gameLevel, dropped, f0 / 1048576.f, heapFreeBytes() / 1048576.f, b0 / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, drawDist, smkT);
  if (smkOn) smkWrite(s_); bbNote(s_);
}
// ступень 0 — до первой попытки: дальше 50 м; 1 — дальше 20 м; 2 — все чужие клетки. Возвращает число выгруженных
static int boothRoom1051(int stage) {
  unsigned f0 = heapFreeBytes(), b0 = memMaxBlock ? memMaxBlock() : 0; stBoothHold1051 = 1;
  int n = cellsDropBeyond(stage == 0 ? BOOTH1051_R1 : stage == 1 ? 20.f : -1.f);
  boothMem1051Log(stage == 0 ? "pre" : stage == 1 ? "stage1" : "stage2", n, f0, b0);
  return n;
}
static void boothRelease1051(void) { stBoothHold1051 = 0; }
// служебное: сколько клеток города в кадре (ближе дальности видимости) ещё не загружено — это и есть «дыры»
static int stMissNow, stMissMax, stMissFrames, stFrames; static float stWorkSum, stWorkMax; static unsigned stRamMax;
static void streamMiss(void) {
  int m = 0;
  for (unsigned i = 0; i < ncell; i++) {
    Cell *c = &cl[i]; if (c->owner || c->state == 2) continue;
    if (zgOn && CELL_REG(c) && CELL_REG(c) <= 16 && !((regRes >> (CELL_REG(c) - 1)) & 1u)) continue;   // правка 181: регион вне набора ворот не рисуется — не дыра
    if (boxD2(c->bb) < drawDist * drawDist && visBox(c->bb, 0, 0, 0)) {
      if (m < 4 && smkOn && !stMissNow && !jumpWatch) {   // служебное: первые видимые недогруженные клетки пачки — почему не загружены (строка MISSCELL)
        float dx = c->cx - eyeX, dz = c->cz - eyeZ, hl = sqrtf(dx * dx + dz * dz) + .01f, fl = sqrtf(stFx * stFx + stFz * stFz) + .001f;
        char s_[200]; snprintf(s_, sizeof s_, "MISSCELL i=%u state=%d d=%.0f rad=%.0f ang=%.0f vel=%.1f,%.1f lqn=%d busy=%d tight=%d ring=%d dd=%.0f t=%.2f\n", i, c->state, hl, c->rad,
          acosf(fmaxf(-1.f, fminf(1.f, (dx * stFx + dz * stFz) / (hl * fl)))) * 57.3f, stVx, stVz, lqn, loaderBusy, memTight, ringState, drawDist, smkT);
        smkWrite(s_); }
      m++;
    }
  }
  stMissNow = m;
}
// цвет тумана = цвет горизонта самого неба уровня (как в GTA: туман = нижний цвет неба). Берём вершины
// кусков неба (owner 2) в полосе 0..6° над горизонтом (холмы/дымка за дальними домами) и 6..12° (низ неба),
// цвет = тексель x цвет вершины, вес = альфа. Раньше был постоянный голубой 0xe0b070 — ярче фона, дома за 60 м «светились».
static unsigned fogCol = SKY & 0xffffff, fogSkyCol; static int oldFog, fogToneOld;
// ДАЛЬ (слово Роба 16.10: «главное, чтобы дальность рисовалась красиво»): под подстилкой земли (№437) город таял в цвет земли, и за дальностью под закатным небом
// оставалась тёмная полоса цвета земли с резкой линией горизонта (L3 причал: оливковая). flag fogsky G (G > 1 — высота градиента в пикселях, 1 — 40): туман города — цвет горизонта
// неба (fogCol), а подстилка у линии горизонта плавно переходит в него. TEST27: включено по умолчанию; файл fogskyold рядом с EBOOT или FOG_SKY_OLD=1 / flag fogsky 0 — прежнее побайтно
#ifndef FOG_SKY_OLD
#define FOG_SKY_OLD 0
#endif
#ifndef FOG_SKY
#define FOG_SKY 1
#endif
static int fogSkyOn = FOG_SKY && !FOG_SKY_OLD; static float fogSkyG = 40.f;
// ПОДСТИЛКА В ТУМАНЕ (задача TEST28, кадры Камеры с высоты): подстилка цвета земли держала жёсткую границу с небом под закатом. При fogsky подстилка красится по расстоянию точки земли от глаза так же, как туман города
// (до 0,75 дальности цвет земли, до дальности плавно в fogCol, дальше fogCol): полосы параллельны горизонту, шов с небом исчезает с любой высоты. FOG_GROUND_OLD=1 / flag fogground 0 — прежний градиент 40 пикселей
#ifndef FOG_GROUND_OLD
#define FOG_GROUND_OLD 0
#endif
static int fogGroundOn = !FOG_GROUND_OLD;
#define FOG_COVER_RGB() (fogSkyOn && !FOG_SKY_OLD ? fogCol : groundCover.rgb)
// цвет, в который тает город (тот же идёт в sceGuFog города; для ленты Горизонта и согласования цвета): с подстилкой — fogCol при fogsky, иначе groundCover.rgb; без подстилки — fogCol
static unsigned cityFogColor(void) { return groundCoverFog(&groundCover) ? FOG_COVER_RGB() : fogCol; }
// правка 411, служебный замер (гейт tests/fartint): «flag farprobe M D» — 1: город дальше D м по глубине кадра сплошным ключом
// (маска дальних кусков), 2: без дымки (свой цвет поверхности), 3: город не рисуется (фон за ним — небо); 0 — как обычно
static int farProbe; static float farProbeD = 175.f;
// Правка Роба 411 (уточнение 09.10: «тумана не нужно»): дымка города выключена — дальние куски своего цвета, фон — тон горизонта
// (fogCol = skyHorizonColor, им же чистится кадр). Прежняя дымка — «flag fogcityold 1» или сборка -DFOG_CITY_OLD=1 (красный контроль)
#ifndef FOG_CITY_OLD
#define FOG_CITY_OLD 0
#endif
static int fogCityOld = FOG_CITY_OLD;
#define FARPROBE_KEY 0xff00ffu   // ABGR без альфы: пурпурный
#define FOG_LOW_W .65f   // доля полосы 0..6° (остальное — 6..12°)
#define FOG_NEAR (longView ? .06f : .12f)   // Роб 27.09: воздушная перспектива — дымка плавно с ~15 м (было с 50–55%: вблизи и вдали трава одного тона)
#define FOG_NEAR_OLD (longView ? .55f : .50f)   // туман с 50% дальности (дальний режим 55%: 138..250 м) до 100% на дальности отсечения: дом появляется уже растворённым
// служебное: файл streamlog рядом с EBOOT (число секунд) -> через столько секунд пишем stream.txt
static float stLogAt; static unsigned stT0;
// Загрузка 3 (слово Роба 16.10: «оптимизировать подгрузку»): строка учёта раз в секунду в буфер ящика (bbNote, на карту уходит вместе с ним), без записи на карту
// в кадре. Включается переключателем: файл loadacc рядом с EBOOT или «flag loadacc 1» в сценарии; по умолчанию выключено. Поля: q — в очереди загрузчика сейчас,
// cand — в радиусе загрузки и ещё не взято, ld/ldms — клеток прочитано и мс чтения за секунду, lat/latmax/slow — задержка от запроса до готовности (среднее, худшая мс,
// дольше 100 мс), late — кадров с недогруженной видимой клеткой и сколько клеток сейчас, texlate — позже геометрии текстур нет: таблица текстур резидентна с загрузки
// уровня (поле для сверки, всегда 0), dd/ddS — дальность и множитель регулятора, free — куча КБ, tight — режим нехватки кучи, pre — регионов впрок
static int loadAccOn;
static unsigned silBlocks, silTris, silCalls;   // silh.h (силуэты): последний кадр
extern void acctSetLoad(const char *s) __attribute__((weak));   // автотур «Игрок» (ветка Аудита-FPS, viewer/acct.h): раз в секунду короткая строка подгрузки; нет хука — не зовём
static void bbNote(const char *line);   // main_frontend.h
static void loadAccStep(unsigned now) {
  static unsigned t0, ld0, us0, mf0, n0; if (!loadAccOn && !acctSetLoad) return;
  if (!t0) { t0 = now; ld0 = stLoads; us0 = stLoadUs; mf0 = stMissFrames; return; }
  if (now - t0 < 1000000u) return;
  unsigned n = accN, sum = accLatSum, mx = accLatMax, sl = accSlow; accN = accLatSum = accLatMax = accSlow = 0; (void)n0;
  char l_[256]; snprintf(l_, sizeof l_, "LOADACC q=%d cand=%u ld=%u ldms=%u lat=%u latmax=%u slow=%u late=%u/%d texlate=0 dd=%.0f ddS=%.2f free=%u tight=%d pre=%u silh=%u/%u\n", lqn, stCand, stLoads - ld0, (stLoadUs - us0) / 1000u,
    n ? sum / n : 0u, mx, sl, (unsigned)stMissFrames - mf0, stMissNow, drawDist, ddScale, memFreeNow == ~0u ? 0u : memFreeNow >> 10, memTight, (unsigned)regPre, silBlocks, silTris);
  if (loadAccOn) { bbNote(l_); if (smkOn) smkWrite(l_); }
  if (acctSetLoad) { char a_[96]; snprintf(a_, sizeof a_, "rd_ms=%u q=%d cand=%u late_tex=0 lat=%u slow=%u ring=%d", (stLoadUs - us0) / 1000u, lqn, stCand, n ? sum / n : 0u, sl, ringState); acctSetLoad(a_); }
  t0 = now; ld0 = stLoads; us0 = stLoadUs; mf0 = stMissFrames;
}
static void streamStat(float work) {
  stFrames++; if (stMissNow) stMissFrames++; if (stMissNow > stMissMax) stMissMax = stMissNow;
  stWorkSum += work; if (work > stWorkMax) stWorkMax = work; if (loadedBytes > stRamMax) stRamMax = loadedBytes;
  if (!stT0) stT0 = sceKernelGetSystemTimeLow();
  if (stLogAt <= 0.f || (sceKernelGetSystemTimeLow() - stT0) * 1e-6f < stLogAt) return;
  stLogAt = 0.f;
  char fn[256], b[512]; sibling(fn, "stream.txt");
  int n = snprintf(b, sizeof b, "frames %d, frames with missing visible cells %d (%.1f%%), max missing %d\n"
    "work avg %.2f ms max %.2f ms; cells loaded %u, avg read %.1f ms; RAM cells max %.2f MB; heap used %.1f of %.1f MB; slowio %u KB/s\n"
    "fog color 0x%06x\n",
    stFrames, stMissFrames, 100.f * stMissFrames / (stFrames ? stFrames : 1), stMissMax, stWorkSum / (stFrames ? stFrames : 1), stWorkMax,
    stLoads, stLoads ? stLoadUs / 1000.f / stLoads : 0.f, stRamMax / 1048576.f, mallinfo().uordblks / 1048576.f, mallinfo().arena / 1048576.f, slowIoKBs, fogCol);
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f >= 0) { sceIoWrite(f, b, n); sceIoClose(f); }
}
// «радиальный» туман: GE считает туман по глубине кадра (z), а отсекаем мы по расстоянию (сфера 115 м) —
// у краёв кадра дом появлялся на 115 м, но по z это ~80 м и туман там был ~45%: видно, как он «выскакивает»,
// и туман менялся при повороте камеры. Для дальних кусков (> 50 м) масштабируем near/far тумана на cos угла
// до центра куска: туман ≈ f(расстояния), на границе отсечения — 100% в любом месте кадра. +1 команда GE на кусок.
static float fogK = -1.f;
// №1042 (Роб: «никаких белых стен, никакого тумана, никакого тумана цвета неба»): туман города выключен полностью — какой бы ни была дымка (прежняя fogcityold, подложка земли, радиальный туман дальних кусков),
// город рисуется от глаза до края дальности в полном цвете, без смешивания с цветом неба; слой дальних дорог №934 тоже без дымки. FOGCITY1042_OLD=1 / файл fogcity1042old / «flag fogcity1042 0» — как до правки
#ifndef FOGCITY1042_OLD
#define FOGCITY1042_OLD 0
#endif
static int fogCity1042On = !FOGCITY1042_OLD;
static void fogBase(void) { fogK = 1.f;
  if (fogCity1042On && farProbe != 1) { sceGuFog(1e6f, 2e6f, fogCol); return; }   // №1042: туман города выключен
  if (pano917Cap) { sceGuFog(1e6f, 2e6f, fogCol); return; }   // №917б: кадр бота дальнего кольца — без дымки
  if(!farProbe && groundCoverFog(&groundCover)){sceGuFog(drawDist*.75f,drawDist*fogSoft,FOG_COVER_RGB());return;}
  if (farProbe == 2 || (!fogCityOld && farProbe != 1)) { sceGuFog(1e6f, 2e6f, fogCol); return; }
  sceGuFog(drawDist * (oldFog ? .52f : FOG_NEAR), drawDist * fogSoft, fogCol); }
static void fogRadial(Piece *d, int far) {
  if (pano917Cap) return;   // №917б: без дымки (fogBase уже поставил)
  int cover=!farProbe && groundCoverFog(&groundCover);
  if (fogCity1042On && farProbe != 1) return;   // №1042: туман города выключен, команда на кусок не нужна
  if (!cover && !fogCityOld && farProbe != 1) return;   // правка 411: дымки нет — команда на кусок не нужна
  if (farProbe == 1 || farProbe == 2) { if (fogK != -2.f) { fogK = -2.f;   // правка 411, служебное: маска / без дымки
      if (farProbe == 1) sceGuFog(farProbeD, farProbeD + .01f, FARPROBE_KEY); else sceGuFog(1e6f, 2e6f, fogCol); } return; }
  float k = 1.f;
  if (far && !oldFog) {
    float dx = d->cx - eyeX, dy = d->cy - eyeY, dz = d->cz - eyeZ, r = sqrtf(dx * dx + dy * dy + dz * dz);
    if (r > 1.f) { k = (dx * cfX + dy * cfY + dz * cfZ) / r; if (k < .35f) k = .35f; if (k > 1.f) k = 1.f; }
    k = floorf(k * 32.f + .5f) * (1.f / 32.f);
  }
  if (k == fogK) return; fogK = k;
  sceGuFog(drawDist * (cover?.75f:oldFog ? .52f : FOG_NEAR) * k, drawDist * fogSoft * k, cover?FOG_COVER_RGB():fogCol);
}
// служебное: файл flypath рядом с EBOOT (скорость м/с) — камера летит по кругу через весь уровень на высоте 3 м
// над землёй, машина стоит; нужно для замера стриминга без аварий автопилота
static float flyPathV, flyPathS; static int ground(float x, float z, float y, float step, float *out);
static void flyPathStep(float dt, float *ex, float *ey, float *ez, float *fx, float *fy, float *fz) {
  static const float W[][2] = {{220, 172}, {-300, 200}, {-300, -250}, {350, -150}, {350, -550}, {300, 500}, {-50, 550}, {220, 172}};
  const int nw = sizeof W / sizeof W[0]; float len = 0;
  for (int i = 0; i + 1 < nw; i++) len += hypotf(W[i + 1][0] - W[i][0], W[i + 1][1] - W[i][1]);
  flyPathS = fmodf(flyPathS + flyPathV * dt, len);
  float s = flyPathS;
  for (int i = 0; i + 1 < nw; i++) {
    float dx = W[i + 1][0] - W[i][0], dz = W[i + 1][1] - W[i][1], l = hypotf(dx, dz);
    if (s > l) { s -= l; continue; }
    float x = W[i][0] + dx * s / l, z = W[i][1] + dz * s / l, g = 0;
    if (!ground(x, z, 300.f, 0.f, &g)) g = *ey - 3.f;
    *ex = x; *ey = g + 3.f; *ez = z; *fx = dx / l; *fy = -.05f; *fz = dz / l;
    float n = sqrtf(*fx * *fx + *fy * *fy + *fz * *fz); *fx /= n; *fy /= n; *fz /= n; return;
  }
}
