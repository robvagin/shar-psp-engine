// Лестница памяти (канон Роба 06.11 «Лестница памяти», CLAUDE.md п.13, правка 435).
// Вся крупная память игры (пакеты картинок меню и паузы, будка, комната, ролик, газета, гэги, модели миссий, звук, кольцо чтения клеток) просит место одной
// функцией memLadder(who, need, prio). Нужного цельного куска нет: идёт по ступеням, после каждой проверяет снова; помогло — стоп.
//   1  подгруженное впрок: чтение моделей миссий вперёд (mcrPre), тёплый пакет будки (uiScrWarmClear)
//   2  далёкое за спиной: клетки дальше дальности + 20 м (cellsMakeRoom, самая дальняя первой)
//   3  ужать: дальность на 40 % (memLadDdMul), клетки дальше новой дальности — в кучу        (prio >= LAD_NORM)
//   4  мир стоит (пауза, меню, будка, лавка, экран загрузки): все клетки вне кадра — в кучу  (prio >= LAD_NORM и мир стоит)
//   5  упростить просившего: вернуть 0; просящий сам идёт запасным видом (меню текстом, будка с машиной по умолчанию) — но не ломается и не падает
// Всё выкинутое стоит в очереди «вернуть» (memLadQ): дальность возвращается плавно, как только наибольший кусок вырос; клетки и модели стрим/читатель грузит сам, ближнее первым.
// Заранее: раз в секунду смотрим наибольший цельный кусок (memLadTick); ниже LAD_LOW ступени 1–2 идут до всякого отказа. Отложенный блок под меню (memLadRes) лежит с начала
// уровня и отдаётся перед открытием пакета меню (память под него уже цельная), после закрытия меню откладывается снова.
// Каждая ступень пишет «WATCHDOG mem who=… size=… step=… blk до->после free до->после»; боты считают строку дырой (CLAUDE.md п.12), tools/memladder_report.py — график наибольшего куска.
// Выключатель: MEMLADDER_OLD=1 (сборка) / flag memladder 0 — прежние частные обходы (mem1060Pre / cellsMakeRoom / ступени msPackOpen №432) без лестницы.
// flag memladres 0 — без отложенного блока под меню. Цена на PSP не измерена (отложенный блок LAD_RES_KB уменьшает свободную кучу на столько же; проверять по shar-psp-perf-truth).
#ifndef MEMLADDER_H
#define MEMLADDER_H
#ifndef MEMLADDER_OLD
#define MEMLADDER_OLD 0
#endif
#define LAD_LOW (768u << 10)      // ниже этого наибольшего куска ступени 1–2 идут сами
#define LAD_HIGH (2048u << 10)    // выше этого очередь «вернуть» возвращает дальность
#define LAD_RES_KB 448u           // отложенный блок под пакет MISSION SELECT (pextra.shr 0,42 МБ кусками по 256 КБ)
#ifndef MEMLAD_L4_OLD
#define MEMLAD_L4_OLD 1           // 1 — блок 448 КБ и на L4 (по умолчанию с бисекции 07.11: на 50f013d1 с блоком 256 КБ tests/memladder s1new падает, MSNSEL openfail 42; с 448 — PASS). 0 — 256 КБ на L4 (было в 119b751d); прежний комментарий: и на L4 блок 448 КБ (тур TEST93 с PSP: куча после загрузки L4 4,5 -> 3,7-4,2 МБ, 4 отказа клеток MEMFAILSZ при куче 0,53 МБ)
#endif
#define LAD_RES_KB_L4 256u        // L4 самый тесный уровень: блок = один сегмент пакета (256 КБ); остальные сегменты ступени лестницы берут после выдачи блока (+192 КБ кучи на L4)
#define LAD_DD_MUL 0.6f
enum { LAD_PRE = 0, LAD_NORM = 1, LAD_TOP = 2 };
static int memLadOn = !MEMLADDER_OLD, memLadResOn = 1;
static float memLadDdMul = 1.f;               // ступень 3: множитель дальности (main_loop.h drawDist)
static unsigned memLadRuns, memLadStep[6], memLadFail, memLadBlkMin = ~0u, memLadAutoT, memLadShrinkT, memLadResN, memLadResTook;
static void *memLadRes; static unsigned memLadAutoGap = 8000000u, memLadResKb = LAD_RES_KB;
typedef struct { char what[10]; unsigned t; unsigned size; } MemLadQ;
static MemLadQ memLadQ[16]; static int memLadQN;
static void mcrPreDropAll(void);
static int memLadStopped(void) { return (uiReady && uiPaused()) || ldOn || viewBusyNow() || stBoothHold1051 || mvMissionSelectOn(); }
static void memLadQPush(const char *what, unsigned size) {
  for (int i = 0; i < memLadQN; i++) if (!strcmp(memLadQ[i].what, what)) { memLadQ[i].t = sceKernelGetSystemTimeLow(); memLadQ[i].size += size; return; }
  if (memLadQN < 16) { snprintf(memLadQ[memLadQN].what, sizeof memLadQ[0].what, "%s", what); memLadQ[memLadQN].t = sceKernelGetSystemTimeLow(); memLadQ[memLadQN++].size = size; }
}
static void memLadLog(const char *kind, const char *who, unsigned need, int step, unsigned b0, unsigned f0, const char *note) {
  char s[220]; snprintf(s, sizeof s, "%s mem who=%s size=%u step=%d blk=%.2f->%.2f free=%.2f->%.2f %s t=%.1f\n", kind, who, need, step,
    b0 / 1048576.f, (memMaxBlock ? memMaxBlock() : 0) / 1048576.f, f0 / 1048576.f, heapFreeBytes() / 1048576.f, note, smkT);
  if (smkOn) smkWrite(s);
  bbNote(s);
}
static void memLadWaitLoader(void) {
  lqn = 0; __asm__ volatile("" ::: "memory");
  LdWait lw_ = {0}; while (loaderBusy) { loaderWaitStep(&lw_, "busy", 0, 0); ldWaitLimit(&lw_, "busy_ladder", WAITLIMIT_LOADER_MS); sceKernelDelayThread(1000); }
}
// Одна ступень; возвращает 1, если что-то выкинуто (кучу стоит перемерить)
static int memLadRun(int step, const char *who, unsigned need) {
  if (step == 1) {
    unsigned f = heapFreeBytes(); mcrPreDropAll(); uiScrWarmClear();
    if (heapFreeBytes() > f) memLadQPush("prefetch", heapFreeBytes() - f);
    return heapFreeBytes() > f;
  }
  if (step == 2) { unsigned f = heapFreeBytes(); cellsMakeRoom(need); if (heapFreeBytes() > f) memLadQPush("cells", heapFreeBytes() - f); return heapFreeBytes() > f; }
  if (step == 3) {
    float keep = drawDist; unsigned f = heapFreeBytes(); memLadDdMul = LAD_DD_MUL; memLadShrinkT = sceKernelGetSystemTimeLow();
    drawDist = keep * LAD_DD_MUL; cellsMakeRoom(need); drawDist = keep;
    memLadQPush("range", 0); if (heapFreeBytes() > f) memLadQPush("cells", heapFreeBytes() - f);
    return 1;
  }
  if (step == 4) {
    if (!memLadStopped()) return 0;
    memLadWaitLoader(); unsigned f = heapFreeBytes(); int n = cellsDropBeyond(-1.f);
    memLadQPush("cells", heapFreeBytes() > f ? heapFreeBytes() - f : 0); return n > 0;
  }
  (void)who; return 0;
}
// Главная функция: 1 — цельный кусок need есть, 0 — ступени кончились (просящий идёт запасным видом). prio: LAD_PRE впрок (только 1–2), LAD_NORM (1–3, 4 при стоящем мире), LAD_TOP то же
static int memLadderTo(const char *who, unsigned need, int prio, int maxStep) {
  if (!memMaxBlock || memMaxBlock() >= need) return 1;
  memLadRuns++;
  for (int st = 1; st <= maxStep; st++) {
    if (prio == LAD_PRE && st > 2) break;
    unsigned b0 = memMaxBlock(), f0 = heapFreeBytes(); int did = memLadRun(st, who, need); memLadStep[st]++;
    int ok = memMaxBlock() >= need;
    if (ok || did || strcmp(who, "auto")) memLadLog("WATCHDOG", who, need, st, b0, f0, ok ? "ok" : did ? "more" : "none");   // «auto» без толку не пишет: не засорять журнал, пока кучу нечем лечить
    if (ok) return 1;
  }
  if (maxStep >= 4) { memLadStep[5]++; memLadFail++; memLadLog("WATCHDOG", who, need, 5, memMaxBlock(), heapFreeBytes(), "simplify"); }
  return 0;
}
static int memLadder(const char *who, unsigned need, int prio) { return memLadderTo(who, need, prio, 4); }
// Ступень, которую просящий прошёл своим путём (будка №1051/№1060: клетки дальше 50 / 20 м / все / газета и пересборка): та же строка журнала и счётчики; b0/f0 — куча до ступени
static void memLadNote(const char *who, int step, unsigned b0, unsigned f0, const char *note) {
  if (!memLadOn) return;
  memLadRuns++; memLadStep[step]++; if (step == 5) memLadFail++;
  memLadLog("WATCHDOG", who, 0, step, b0, f0, note);
}
// Перед открытием пакета меню: отложенный блок — в кучу (цельный кусок под первые сегменты пакета)
static void memLadResTake(void) {
  if (!memLadRes) return;
  memLadWaitLoader(); unsigned b0 = memMaxBlock ? memMaxBlock() : 0, f0 = heapFreeBytes(); free(memLadRes); memLadRes = 0; memLadResTook++;
  memLadLog("MEMLAD", "menu-res", memLadResKb << 10, 0, b0, f0, "take");
}
// После загрузки уровня n (main_levels.h): размер отложенного блока по уровню; блок другого размера уходит в кучу и ложится заново на ближайшем тике
static void memLadSetLevel(int n) {
  unsigned want = (!MEMLAD_L4_OLD && n == 4) ? LAD_RES_KB_L4 : LAD_RES_KB;
  if (want == memLadResKb) return;
  memLadResKb = want; if (memLadRes) { free(memLadRes); memLadRes = 0; }
}
static void memLadResPut(void) {
  if (memLadRes || !memLadResOn || !memMaxBlock || memLadStopped()) return;
  if (memMaxBlock() < (memLadResKb << 10) + (256u << 10)) return;
  memLadRes = memalign(64, memLadResKb << 10); if (memLadRes) { memLadResN++; memset(memLadRes, 0, 64); }
}
// Раз в секунду: наибольший цельный кусок; ниже порога — ступени 1–2 до отказа; очередь «вернуть»; отложенный блок
static void memLadTick(float dt) {
  static float acc; static unsigned blkLog;
  if (!memMaxBlock) return;   // замер идёт и при выключенной лестнице (график «до»), действия — только при включённой
  acc += dt; if (acc < 1.f) return; acc = 0.f;
  unsigned blk = memMaxBlock(), now = sceKernelGetSystemTimeLow();
  if (blk < memLadBlkMin) memLadBlkMin = blk;
  if (smkOn && ++blkLog % 5u == 1u) { char s[100]; snprintf(s, sizeof s, "MEMBLK blk=%.2f free=%.2f min=%.2f dd=%.2f q=%d t=%.1f\n", blk / 1048576.f, heapFreeBytes() / 1048576.f, (memLadBlkMin == ~0u ? 0 : memLadBlkMin) / 1048576.f, memLadDdMul, memLadQN, smkT); smkWrite(s); }
  if (!memLadOn) return;
  if (blk < LAD_LOW && now - memLadAutoT > memLadAutoGap && !loaderBusy) {   // не помогло — следующая проба позже (8 с, 16, … до 64 с), помогло — снова 8 с
    unsigned f0 = heapFreeBytes(); memLadAutoT = now; memLadderTo("auto", LAD_LOW, LAD_PRE, 2);
    memLadAutoGap = heapFreeBytes() > f0 ? 8000000u : memLadAutoGap < 64000000u ? memLadAutoGap * 2 : memLadAutoGap; }
  if (memLadDdMul < 1.f && !memLadStopped() && now - memLadShrinkT > 10000000u) {   // очередь «вернуть»: через 10 с вне меню дальность плавно назад (повторный отказ ужмёт снова)
    memLadDdMul += 0.1f; if (memLadDdMul >= 1.f) { memLadDdMul = 1.f; memLadLog("MEMLAD", "range", 0, 0, blk, heapFreeBytes(), "restored"); } }
  if (memLadQN && !memLadStopped() && memLadDdMul >= 1.f && heapFreeBytes() >= MEM_RESERVE + MEM_HYST && now - memLadQ[0].t > 3000000u) {   // клетки и модели стрим возвращает сам; запись снимаем, когда куча снова с запасом
    for (int i = 0; i + 1 < memLadQN; i++) memLadQ[i] = memLadQ[i + 1]; memLadQN--; }
  if (!mvMissionSelectOn()) memLadResPut();
}
// P0.2 Опора-93: клетка не легла (CELLRETRY mem), а клеток дальше дальности + 20 м уже нет — идут ступени 1–3 как при любом отказе (канон п.13: клетки города берутся той же лестницей).
// Тур TEST93 с PSP, F_GO5_L4: 4 отказа MEMFAILSZ при куче 0,53 МБ, кусок 0,11 МБ; ступень 2 пришла позже отказа и ничего не нашла (блок 0,23->0,23). MEMLAD_CELLOOM_OLD=1 — прежнее
#ifndef MEMLAD_CELLOOM_OLD
#define MEMLAD_CELLOOM_OLD 0
#endif
static int memLadCellOom(void) {
  if (MEMLAD_CELLOOM_OLD || !memLadOn || !memMaxBlock || memLadStopped()) return 0;
  unsigned f0 = heapFreeBytes(); memLadderTo("cells", LAD_LOW, LAD_NORM, 3);
  unsigned f1 = heapFreeBytes(); return f1 > f0 ? 1 : 0;
}
static int memLadRoomUi(unsigned need) { return !memLadOn ? cellsMakeRoom(need) : memLadder("ui", need, LAD_TOP); }   // msnartRoom: пакеты картинок меню, паузы, миссий
#endif
