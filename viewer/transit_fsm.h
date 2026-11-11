// Будка-93 / Build 95, пакет П2 (спец Fable «порядок», раздел Б): машина состояний переходов, ОДИН загрузчик сверху. Каркас с классом ROOM_IN; остальные 14 классов (таблица Б.3) переводятся по одному.
//   IDLE -> PLAN -> CLOSE -> FREE -> LOAD -> (FREE_MORE x до 4 | SIMPLIFY) -> WAIT_SCENE -> OPEN -> IDLE
// Ключ канона п.14 CLAUDE.md: экран закрыт до тяжёлой работы; план памяти до закрытия; отказ идёт ступенями под экраном, потом честный упрощённый путь, тишины нет; на каждом шаге строка TFSM для ботов.
// Заявка Transit: имя (строка журнала), экран (AUTO/NEWS/PRELOADER/IRIS/NONE), центр выхода, сколько нужно цельным куском, замыкания free / load / simplify, ожидание сцены.
// Экран выбирает ldrRun (спец Б.1): AUTO = IRIS, если `transitPlan` говорит «хватает» и ирис доступен; иначе PRELOADER (Itchy & Scratchy). NEWS здесь только заготовка (LEVEL_*, MISSION_SELECT переводятся позже).
// Раскрытие ириса остаётся за transitStep (держит спец-крючок A1/1226: пол, сцена, гэги); прелоадер закрывает сам ldrRun. TFSM95_OLD=1 / файл tfsm95old / flag tfsm95old 1 — переход идёт прежним путём (main_rooms.h).
#ifndef TRANSIT_FSM_H
#define TRANSIT_FSM_H
enum { TF_IDLE, TF_PLAN, TF_CLOSE, TF_FREE, TF_LOAD, TF_FREE_MORE, TF_WAIT_SCENE, TF_OPEN, TF_SIMPLIFY, TF_RESCUE };
enum { TFS_AUTO, TFS_NEWS, TFS_PRELOADER, TFS_IRIS, TFS_NONE };
typedef struct Transit {
  const char *name; int screen; float at[3]; unsigned need;
  int (*freeFn)(void *u);       // FREE: выбросить по плану; возвращает число освобождённого (клеток)
  int (*load)(void *u);         // LOAD: 1 — готово, 0 — мало памяти / ошибка чтения
  int (*simplify)(void *u);     // SIMPLIFY: честный упрощённый путь
  void *u; int needFloor, cameraStart; unsigned maxWaitMs;
  int freeAllOnPreloader;       // 1 — под прелоадером в FREE выбросить ВСЕ клетки города и пакеты (комната: «запомнить место, выбросить лишнее, грузить только комнату», решение Роба 2)
  int (*stepFn)(void *u, int step);   // своя ступень FREE_MORE (1..4): 1 — обработана (generic не идёт), 0 — generic
  int rebuildStep;              // номер ступени, где stepFn сам держит экран (пересборка уровня): прелоадер ldrRun перед ней закрывается, минимум показа не доигрывается
  int noFreeAllStep;            // 1 — ступень 3 (все клетки в кучу) пропускается: за экраном виден город (лавка)
  int deferReturn;              // 1 — ldrRun не возвращает выброшенное и не сбрасывает mem1060Back: вернёт вызывающий после своей работы (ролик)
  int noReturn;                 // 1 — выброшенное НЕ возвращается после перехода (комната: город вернёт выход у двери, п.14); 0 — возврат под прелоадером (ролик)
  const char *simplifyText;     // надпись игроку при SIMPLIFY (не тишина)
  int (*cleanReload)(void *u, void **fbp);   // Build 96: ступень 5 «чистая перезагрузка» перед SIMPLIFY (уровень выгружается и читается заново из снимка состояния); 1 — выполнена (прелоадер, если был, открывается снова), 2 — выполнена, экран не открывать (читающему нужен свободный слот экрана: лавка, гардероб), 0 — нельзя (миссия и т.п.) или не вышла
  void (*closeFn)(void *u);     // screen == TFS_NEWS: открыть газету уровня (Build 96: LEVEL_NEXT, LEVEL_START); без closeFn газета заменяется прелоадером, как раньше
} Transit;
#ifndef TFSM95_OLD
#define TFSM95_OLD 0
#endif
#ifndef TF_PRELOADER_MIN_MS
#define TF_PRELOADER_MIN_MS 1200u   // минимум показа прелоадера, мс (спец Б.1, решение Роба 1)
#endif
static int tfsm95Old = TFSM95_OLD;
static int tfState = TF_IDLE;          // текущее состояние машины (сторож: «экран закрыт ⇔ не IDLE/OPEN»)
static int tfIrisClosed, tfIrisFrames; // последний ldrRun закрыл ирис сам; сколько кадров развёртки показано (для счётчика кадров цикла)
static int tfFailBonus, tfFailMsn, tfFailMenu;   // служебное (Build 96): flag tfbonusfail/tfmsnfail/tfmenufail N — следующие N чтений тела класса газеты отказывают (красный/зелёный контроль; под tfsm95old не действует)
#ifndef CLEANRELOAD96_OLD
#define CLEANRELOAD96_OLD 0
#endif
static int cleanReload96 = !CLEANRELOAD96_OLD;   // Build 96: файл cleanreload96old / flag cleanreload96 0 — ступени нет, после лестницы сразу SIMPLIFY
static int tfFailTest;                 // служебное flag tffail N: следующие N попыток LOAD отказывают (красный/зелёный контроль ступеней)
static int ldrRun(const Transit *t, void **fbp);   // ниже (TFSM_IMPL, после mem1060.h)
static void tfExtBegin(const Transit *t); static void tfExtEnd(const Transit *t, int ok);   // ниже (TFSM_IMPL)
#endif

#if defined(TFSM_IMPL) && !defined(TFSM_IMPL_DONE)
#define TFSM_IMPL_DONE
static void tfLog(const Transit *t, const char *what, unsigned a, unsigned b) {
  if (!smkOn) return;
  char s_[160]; snprintf(s_, sizeof s_, "TFSM %s %s a=%u b=%u blk=%u free=%u t=%.1f\n", t->name, what, a, b, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), smkT); smkWrite(s_);
}
static int tfLoadTry(const Transit *t) { if (tfFailTest > 0) { tfFailTest--; return 0; } return t->load ? t->load(t->u) : 1; }
// Класс, чьё тело пока остаётся снаружи (будка: внутри свои экраны и пересборка уровня): ldrRun его не ведёт, но класс ВИДЕН машине. tfExtBegin пишет plan, tfExtEnd пишет load / open или SIMPLIFY с
// WATCHDOG, как ldrRun; строки одни и те же для ботов. Перевод тела на ldrRun по таблице Б.3 — следующим шагом (спец Г.1 П2).
static void tfExtBegin(const Transit *t) { tfState = TF_LOAD; transitPlan(t->name, t->need, t->at); tfLog(t, "plan", t->need, (unsigned)t->screen); tfLog(t, "close", (unsigned)t->screen, 0); }
static void tfExtEnd(const Transit *t, int ok) {
  tfLog(t, "load", (unsigned)ok, 0);
  if (ok) { tfLog(t, "ready", 0, 0); tfLog(t, "open", (unsigned)t->screen, 0); }
  else { tfLog(t, "simplify", 0, 0); if (smkOn) { char s_[140]; snprintf(s_, sizeof s_, "WATCHDOG mem who=%s size=%u step=5 simplify blk=%u free=%u fsm t=%.1f\n", t->name, t->need, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), smkT); smkWrite(s_); } }
  tfState = TF_IDLE;
}
// 1 — переход выполнен (экран остаётся закрытым до раскрытия снаружи: ирис — transitStep, прелоадер — здесь); 0 — отказ: сработал SIMPLIFY (или ничего не сделано)
static int ldrRun(const Transit *t, void **fbp) {
  unsigned t0 = sceKernelGetSystemTimeLow();
  tfIrisClosed = 0; tfIrisFrames = 0; tfState = TF_PLAN;
  int fits = transitPlan(t->name, t->need, t->at);   // план до закрытия экрана
  int scr = t->screen;
  int irisOk = fbp && fbpG1226 && transitReady() && !transitBusy();
  if (scr == TFS_AUTO) scr = (fits && irisOk) ? TFS_IRIS : TFS_PRELOADER;
  if (scr == TFS_NEWS && !t->closeFn) scr = TFS_PRELOADER;   // газета со своим closeFn (LEVEL_NEXT, LEVEL_START, Build 96) остаётся газетой; остальные — прелоадер
  if (scr == TFS_IRIS && !irisOk) scr = TFS_NONE;
  tfLog(t, "plan", t->need, (unsigned)scr);
  tfState = TF_CLOSE; int fe = 0, preloader = 0;
  if (scr == TFS_IRIS) { tfIrisFrames = transitClose(fbp, t->name); tfIrisClosed = 1; }
  else if (scr == TFS_PRELOADER) { preloader = 1; mem1060Bar = 0; mem1060Fade(fbp); fe = mem1060FeIn(fbp); if (fe) mem1060FeTo(fbp, 150, 6, 0); else mem1060BarTo(fbp, 150, 6); }
  else if (scr == TFS_NEWS) t->closeFn(t->u);   // Build 96: газета уровня — один экран на весь переход, прелоадер поверх неё не открывается
  tfLog(t, "close", (unsigned)scr, (unsigned)((sceKernelGetSystemTimeLow() - t0) / 1000u));
  tfState = TF_FREE; int freed = t->freeFn ? t->freeFn(t->u) : 0;
  if (preloader && t->freeAllOnPreloader) { freed += mem1060Free(); mem1060Back = 1;   // все клетки и пакеты про запас в кучу; load.shr закрыт до чтения (последний кадр прелоадера на экране)
    if (fe) { mem1060FeTo(fbp, 300, 2, 0); mem1060FeOut(); fe = 0; } }
  tfLog(t, "free", (unsigned)freed, 0);
  tfState = TF_LOAD; unsigned l0 = sceKernelGetSystemTimeLow(); int ok = tfLoadTry(t); int step = 0, skipMin = 0;
  while (!ok && step < 4) {   // FREE_MORE: ступени под закрытым экраном, шкала не откатывается
    step++; tfState = TF_FREE_MORE; unsigned b0 = memMaxBlock ? memMaxBlock() : 0, f0 = heapFreeBytes();
    int own = 0;
    if (t->rebuildStep && step == t->rebuildStep && fe) { mem1060FeOut(); fe = 0; skipMin = 1; }
    if (t->stepFn) own = t->stepFn(t->u, step);
    if (own) {}
    else if (step == 1) { if (memLadOn) memLadder(t->name, t->need, 2); }
    else if (step == 2 && !preloader && scr != TFS_NEWS) { preloader = 1; mem1060Bar = 0; fe = mem1060FeIn(fbp); if (fe) mem1060FeTo(fbp, 300, 6, 0); else mem1060BarTo(fbp, 300, 6); }   // отказ при ирисе: прелоадер поверх закрытого ириса (решение Роба 2)
    else if (step == 3) { if (t->noFreeAllStep) { if (memLadOn) memLadder(t->name, t->need, 2); } else { int n = mem1060Free(); mem1060Back = 1; tfLog(t, "free-all", (unsigned)n, 0); } }
    else if (step == 4) { if (memLadOn) { memLadResTake(); memLadder(t->name, t->need, 2); } }
    if (smkOn) { char s_[140]; snprintf(s_, sizeof s_, "WATCHDOG mem who=%s size=%u step=%d blk=%u->%u free=%u->%u fsm t=%.1f\n", t->name, t->need, step, b0, memMaxBlock ? memMaxBlock() : 0, f0, heapFreeBytes(), smkT); smkWrite(s_); }
    ok = tfLoadTry(t);
  }
  if (!ok && t->cleanReload && cleanReload96) {   // ступень 5: чистая перезагрузка (Build 96); экран тот же, при прелоадере он переоткрывается после неё
    tfState = TF_RESCUE; unsigned c0 = sceKernelGetSystemTimeLow(); unsigned b0 = memMaxBlock ? memMaxBlock() : 0, f0 = heapFreeBytes();
    if (fe) { mem1060FeOut(); fe = 0; }
    tfLog(t, "reload", b0, f0);
    int rr = t->cleanReload(t->u, fbp);
    if (rr) skipMin = 1;
    if (rr == 1 && preloader) { mem1060Bar = 0; fe = mem1060FeIn(fbp); if (fe) mem1060FeTo(fbp, 300, 6, 0); else mem1060BarTo(fbp, 300, 6); skipMin = 1; }
    unsigned cms = (sceKernelGetSystemTimeLow() - c0) / 1000u;
    if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "WATCHDOG mem who=%s size=%u step=5 reload=%d blk=%u->%u free=%u->%u ms=%u fsm t=%.1f\n", t->name, t->need, rr ? 1 : 0, b0, memMaxBlock ? memMaxBlock() : 0, f0, heapFreeBytes(), cms, smkT); smkWrite(s_); }
    tfLog(t, "reloaded", rr ? 1u : 0u, cms);
    if (rr) ok = tfLoadTry(t);
  }
  tfLog(t, "load", (unsigned)ok, (unsigned)((sceKernelGetSystemTimeLow() - l0) / 1000u));
  if (!ok) {   // SIMPLIFY: честный путь, тишины нет
    tfState = TF_SIMPLIFY; if (t->simplify) t->simplify(t->u); tfLog(t, "simplify", 0, 0);
    if (smkOn) { char s_[140]; snprintf(s_, sizeof s_, "WATCHDOG mem who=%s size=%u step=5 simplify blk=%u free=%u fsm t=%.1f\n", t->name, t->need, memMaxBlock ? memMaxBlock() : 0, heapFreeBytes(), smkT); smkWrite(s_); }
    if (uiReady && t->simplifyText && *t->simplifyText) uiMessage(t->simplifyText, -1, 2.f);   // надпись из библии оригинала (ТЕКСТЫ_SIMPLIFY_94.md Данных); пустая строка: без надписи, но WATCHDOG для ботов остаётся
    if (preloader && fe) mem1060FeOut();
    if (preloader && mem1060Back && !t->deferReturn) { if (t->noReturn) mem1060Back = 0; else mem1060Return(fbp); }
    tfState = TF_IDLE; return 0;
  }
  tfState = TF_WAIT_SCENE; tfLog(t, "ready", (unsigned)((sceKernelGetSystemTimeLow() - t0) / 1000u), 0);   // пол и сцену ждёт крючок ириса (A1); у прелоадера после возврата клеток
  tfState = TF_OPEN;
  if (preloader) {   // «мы не спешим» (решение Роба 1): прелоадер показан не короче TF_PRELOADER_MIN_MS, потом закрыт
    while (!skipMin && (sceKernelGetSystemTimeLow() - t0) / 1000u < TF_PRELOADER_MIN_MS) { if (fe) mem1060FeTo(fbp, 950, 1, 0); else mem1060BarTo(fbp, 950, 1); }
    if (fe) mem1060FeOut(); if (mem1060Back && !t->deferReturn) { if (t->noReturn) mem1060Back = 0; else mem1060Return(fbp); } }
  tfLog(t, "open", (unsigned)scr, 0);
  tfState = TF_IDLE; return 1;
}
#endif
