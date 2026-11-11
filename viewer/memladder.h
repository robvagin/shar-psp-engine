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
#ifndef MEMLADZ_OLD
#define MEMLADZ_OLD 0
#endif
#ifndef MEMLAD_RESKEEP
#define MEMLAD_RESKEEP 0   // опыт шага 9в (1): отложенный блок меню 448 КБ никогда не отдаётся куче (проверено: ballast 9, FREEMAP L1-L7 98-100 %, но минус 448 КБ кучи всегда). 0 - как у Опоры, плюс zone3FitLevel кладёт блок сразу над регионом
#endif
#ifndef MEMLADRES0_OLD
#define MEMLADRES0_OLD 0   // Лестница-96 (Fable шаг 9): 1 - как в 95 (отложенный блок 448 КБ убирается только при прицепленном «месте» >= 512 КБ). 0 - при включённой Z3 блока нет совсем (LAD_RES_KB = 0): пакет меню идёт в «миссию»/по лестнице
#endif
#ifndef MEMLADNPC_OLD
#define MEMLADNPC_OLD 0   // Лестница-96 (просьба дирижёра 08.11): 1 - как было (ступень 2 только клетки дальше дальности + 20 м; ступень 3 не действует при служебном форсе дальности pano917cap). 0 - просящему «npc»/«car» миссии ступень 2 отдаёт далёкие клетки за спиной и вне ближнего кольца (потолок), ступень 3 ужимает дальность и при форсе
#endif
static int memLadNpcOld = MEMLADNPC_OLD;   // flag ladnpcold 0/1 в сценарии: красный контроль на том же EBOOT
#define LADNPC_KEEP_M 90.f        // ближе этого (с поправкой на конус взгляда, как в стриме) клетки для миссионного просящего не трогаем
#define LADNPC_CAP 64u            // не больше стольких клеток за окно LADNPC_WIN_US: город у игрока не раздеть при повторах 1/с
#define LADNPC_WIN_US 10000000u
#define LAD_LOW (768u << 10)      // ниже этого наибольшего куска ступени 1–2 идут сами
#define LAD_HIGH (2048u << 10)    // выше этого очередь «вернуть» возвращает дальность
#define LAD_RES_KB 448u           // отложенный блок под пакет MISSION SELECT (pextra.shr 0,42 МБ кусками по 256 КБ)
#ifndef MEMLAD_L4_OLD
#define MEMLAD_L4_OLD 1           // 1 — блок 448 КБ и на L4 (по умолчанию с бисекции 07.11: на 50f013d1 с блоком 256 КБ tests/memladder s1new падает, MSNSEL openfail 42; с 448 — PASS). 0 — 256 КБ на L4 (было в 119b751d); прежний комментарий: и на L4 блок 448 КБ (тур TEST93 с PSP: куча после загрузки L4 4,5 -> 3,7-4,2 МБ, 4 отказа клеток MEMFAILSZ при куче 0,53 МБ)
#endif
#define LAD_RES_KB_L4 256u        // L4 самый тесный уровень: блок = один сегмент пакета (256 КБ); остальные сегменты ступени лестницы берут после выдачи блока (+192 КБ кучи на L4)
#define LAD_DD_MUL 0.6f
#ifndef MEMLADRESTORE_OLD
#define MEMLADRESTORE_OLD 0
#endif
enum { LAD_PRE = 0, LAD_NORM = 1, LAD_TOP = 2 };
static int memLadOn = !MEMLADDER_OLD, memLadResOn = 1;
static float memLadDdMul = 1.f;               // ступень 3: множитель дальности (main_loop.h drawDist)
static unsigned memLadRuns, memLadStep[6], memLadFail, memLadBlkMin = ~0u, memLadAutoT, memLadShrinkT, memLadResN, memLadResTook;
#ifndef MEMLADAUTO_OLD
#define MEMLADAUTO_OLD 0
#endif
static unsigned memLadAutoBlk; static int memLadAutoNoGrow;
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
// Лестница-96: ступень 2 для персонажа/машины миссии. Клетки дальше LADNPC_KEEP_M (за спиной и сбоку - дальше на 40 м, в конусе взгляда - ближе на 100 м, как в стриме) уходят по одной,
// самая дальняя первой, пока не вырос наибольший кусок до need; потолок LADNPC_CAP клеток за окно 10 с (повторы лестницы 1/с не раздевают город); стрим вернёт, когда освободится место
static unsigned memLadNpcWinT, memLadNpcWinN;
static int memLadMsnWho(const char *who) { return who && (!strcmp(who, "npc") || !strcmp(who, "car")); }
static int cellsMakeRoomMsn(unsigned need) {
  if (!memMaxBlock) return 0;
  unsigned now = sceKernelGetSystemTimeLow(); if (now - memLadNpcWinT > LADNPC_WIN_US) { memLadNpcWinT = now; memLadNpcWinN = 0; }
  int n = 0; unsigned best = memMaxBlock(), mark = 0;
  while (best < need && memLadNpcWinN < LADNPC_CAP) {
    int far = -1; float fd = LADNPC_KEEP_M;
    for (unsigned i = 0; i < ncell; i++) { Cell *c = &cl[i]; if (c->owner || c->state != 2) continue;
      float d = cellDist(c, eyeX, eyeY, eyeZ); if (streamAhead1042 && (stFx != 0.f || stFz != 0.f)) { float l_ = sqrtf(stFx * stFx + stFz * stFz); int sd_ = sta1042Side(c, eyeX, eyeZ, stFx / l_, stFz / l_); d += sd_ > 0 ? -STA1042_KEEP : sd_ < 0 ? 40.f : 0.f; }
      if (d > fd) { fd = d; far = (int)i; } }
    if (far < 0) break;
    cellThrashSiteNow = 3; unloadCellP(&cl[far]); n++; memLadNpcWinN++;
    unsigned b = memMaxBlock(); if (b > best) { best = b; mark = (unsigned)n; } else if ((unsigned)n - mark >= 16u) break;
  }
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "MAKEROOM msn need=%.2f freed=%d blk=%.2f cap=%u/%u t=%.1f\n", need / 1048576.f, n, memMaxBlock() / 1048576.f, memLadNpcWinN, LADNPC_CAP, smkT); smkWrite(s); }
  return n;
}
// Одна ступень; возвращает 1, если что-то выкинуто (кучу стоит перемерить)
static int memLadRun(int step, const char *who, unsigned need) {
  if (step == 1) {
    unsigned f = heapFreeBytes(); mcrPreDropAll(); uiScrWarmClear();
    if (heapFreeBytes() > f) memLadQPush("prefetch", heapFreeBytes() - f);
    return heapFreeBytes() > f;
  }
  if (step == 2) { unsigned f = heapFreeBytes(); cellsMakeRoom(need); if (!memLadNpcOld && memLadMsnWho(who) && memMaxBlock() < need) cellsMakeRoomMsn(need); if (heapFreeBytes() > f) memLadQPush("cells", heapFreeBytes() - f); return heapFreeBytes() > f; }
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
  memLadAutoNoGrow = 0;   // новый уровень - проба «auto» снова разрешена
  if (want == memLadResKb) return;
  memLadResKb = want; if (memLadRes) { free(memLadRes); memLadRes = 0; }
}
static void memLadResPut(void) {
  // Build 95 шаг 9: пакет меню идёт в область «место» (ui.c pakMem), пока она прицеплена, - отложенный блок 448 КБ ей не нужен и отдаётся куче. Область отцеплена (тесный уровень, L4) - блок нужен, как раньше.
  // Проверка s1 (куча забита, пакет меню открывается только из блока) показала: без блока при отцепленной области меню идёт запасным видом
  if (!MEMLADRES0_OLD && !MEMLADZ_OLD && !MEMLAD_RESKEEP && zone3WantedX()) { if (memLadRes && !memLadStopped()) { free(memLadRes); memLadRes = 0; memLadLog("MEMLAD", "menu-res", memLadResKb << 10, 0, 0, heapFreeBytes(), "zone3-drop"); } return; }   // шаг 9 (Fable): LAD_RES_KB -> 0 при ZONE3 всегда
  if (!MEMLADZ_OLD && !MEMLAD_RESKEEP && zone3AreaOnX(Z3A_PLACE) && zone3MaxFreeX(Z3A_PLACE) >= (512u << 10)) { if (memLadRes && !memLadStopped()) { free(memLadRes); memLadRes = 0; memLadLog("MEMLAD", "menu-res", memLadResKb << 10, 0, 0, heapFreeBytes(), "zone-drop"); } return; }
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
  // Лестница-96 (Fable шаг 9, «без роста не повторяется»): проба «auto» ступенью 2 без прироста наибольшего куска (>= 64 КБ) повторяется, только когда кусок сдвинулся на >= 256 КБ или прошло 120 с.
  // Прежнее условие смотрело на рост свободной кучи (клетки уходят, кусок не растёт - булавка посреди), и проба шла каждые 8 с: на L4 в туре 5 раз при ballast 5 (в туре PSP 16). MEMLADAUTO_OLD=1 - как было
  int autoSkip_ = !MEMLADAUTO_OLD && memLadAutoNoGrow && now - memLadAutoT < 120000000u && (blk > memLadAutoBlk ? blk - memLadAutoBlk : memLadAutoBlk - blk) < (256u << 10);
  if (blk < LAD_LOW && now - memLadAutoT > memLadAutoGap && !loaderBusy && !autoSkip_) {   // не помогло — следующая проба позже (8 с, 16, … до 64 с), помогло — снова 8 с
    unsigned f0 = heapFreeBytes(); memLadAutoT = now; memLadAutoBlk = blk; memLadderTo("auto", LAD_LOW, LAD_PRE, 2);
    { unsigned b1_ = memMaxBlock(); memLadAutoNoGrow = b1_ < blk + (64u << 10); if (!memLadAutoNoGrow) memLadAutoBlk = b1_; }
    memLadAutoGap = heapFreeBytes() > f0 ? 8000000u : memLadAutoGap < 64000000u ? memLadAutoGap * 2 : memLadAutoGap; }
  // П5 Зоны-95: дальность возвращается ступенью +0,1, только если свободно с запасом (резерв + гистерезис + 2 МБ): иначе круг «вернули дальность — клетки не влезли — ужали» повторялся каждые 15 с (cellthrash, MEMTRIM десятки тысяч за тур). MEMLADRESTORE_OLD=1 — как было
  if (memLadDdMul < 1.f && !memLadStopped() && now - memLadShrinkT > 10000000u && (MEMLADRESTORE_OLD || strFreeNow == ~0u || strFreeNow >= MEM_RESERVE + MEM_HYST + (2u << 20))) {   // очередь «вернуть»: через 10 с вне меню дальность плавно назад (повторный отказ ужмёт снова)
    memLadDdMul += 0.1f; if (memLadDdMul >= 1.f) { memLadDdMul = 1.f; memLadLog("MEMLAD", "range", 0, 0, blk, heapFreeBytes(), "restored"); } }
  // возврат после сторожа круга клеток: без событий «загрузил - отдал» CELLTHRASHREST_QUIET_S секунд - дальность растёт плавно (+0,016 в секунду от 250 м ≈ 4 м/с); новое срабатывание сторожа снова ставит ×0,6
  else if (!cellThrashRestOff && cellThrashHeld && memLadDdMul < 1.f && !memLadStopped() && now - cellThrashEvT > CELLTHRASHREST_QUIET_S * 1000000u) {
    memLadDdMul += 0.016f; if (memLadDdMul >= 1.f) { memLadDdMul = 1.f; cellThrashHeld = 0; memLadLog("WATCHDOG", "cellthrash", 0, 0, blk, heapFreeBytes(), "restored"); } }
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
// Build 95 шаг 9 (Fable, спецификация А.5): лестница по зоне просящего. Зоны Z3 «место» и «миссия» — свои области (zone3.h); отказ области уже лечат z3ReclaimHook и пролив в кучу, поэтому здесь
// «влезло» = есть кусок в своей области ИЛИ цельный кусок в куче (туда пойдёт пролив). Ступени: 1-2 - свои у зоны (место: тёплый пакет будки, предчтение машин; миссия: кэш картинок, предчтение
// машин), 3 - далёкие клетки города (cellsMakeRoom), 4 - мир стоит: все клетки вне кадра. Прежнего «отложенного блока под меню» (LAD_RES_KB, 448 КБ) при включённой Z3 нет: пакет меню идёт в «миссию».
// Попытка без роста не повторяется чаще раза в 30 с (иначе круг «ступень - ничего - ступень» каждый кадр). MEMLADZ_OLD=1 - как было (один список ступеней по куче, отложенный блок 448 КБ).
static int z3ReclaimHook(int area, const char *who, unsigned need, int step);   // main_levels.h
static unsigned memLadZNoGrow[3];
static int memLadFitsZ(int zone, unsigned need) {
  int a = zone == LADZ_PLACE ? Z3A_PLACE : Z3A_MISSION;
  if (zone3AreaOnX(a) && zone3MaxFreeX(a) >= need) return 1;
  return !memMaxBlock || memMaxBlock() >= need;
}
static int memLadRunZ(int zone, int step, const char *who, unsigned need) {
  int a = zone == LADZ_PLACE ? Z3A_PLACE : Z3A_MISSION;
  if (step == 1) { if (zone == LADZ_PLACE) return z3ReclaimHook(a, who, need, 1); unsigned f = heapFreeBytes(); uiScrWarmClear(); return heapFreeBytes() > f; }
  if (step == 2) return z3ReclaimHook(a, who, need, zone == LADZ_PLACE ? 2 : 1);
  if (step == 3) return memLadRun(2, who, need);
  if (step == 4) return memLadRun(4, who, need);
  return 0;
}
static int memLadderZ(int zone, const char *who, unsigned need, int prio) {
  if (MEMLADZ_OLD || !zone3WantedX()) return memLadder(who, need, prio);
  if (memLadFitsZ(zone, need)) return 1;
  unsigned now = sceKernelGetSystemTimeLow();
  if (memLadZNoGrow[zone] && now - memLadZNoGrow[zone] < 30000000u) return 0;
  memLadRuns++;
  for (int st = 1; st <= 4; st++) {
    if (prio == LAD_PRE && st > 2) break;
    unsigned b0 = memMaxBlock ? memMaxBlock() : 0, f0 = heapFreeBytes(); int did = memLadRunZ(zone, st, who, need); memLadStep[st]++;
    int ok = memLadFitsZ(zone, need);
    memLadLog("WATCHDOG", who, need, st, b0, f0, ok ? (zone == LADZ_PLACE ? "z=place ok" : "z=mission ok") : did ? "more" : "none");
    if (ok) { memLadZNoGrow[zone] = 0; return 1; }
  }
  memLadZNoGrow[zone] = sceKernelGetSystemTimeLow() | 1u;
  if (prio != LAD_PRE) { memLadStep[5]++; memLadFail++; memLadLog("WATCHDOG", who, need, 5, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), "simplify"); }
  return 0;
}
static int memLadRoomUi(unsigned need) { return !memLadOn ? cellsMakeRoom(need) : memLadderZ(LADZ_MISSION, "ui", need, LAD_TOP); }   // msnartRoom: пакеты картинок меню, паузы, миссий
#endif
