// Служебное: смоук-тест для tools/smoke.sh и счётчики приёмки (аудит §10.8). На PSP без служебных файлов
// пишет только счётчики в bench.txt; smoke.txt появляется лишь при smokelog/scenario.txt рядом с EBOOT.
//   smokelog      -> раз в секунду строка в smoke.txt (см. заголовок файла) + STAT по каждой сцене
//   scenario.txt  -> сценарий в одном запуске эмулятора, по строке на команду:
//       scene NAME        новая сцена: STAT предыдущей, сброс счётчиков
//       goto X Z H        поставить машину (как goto.txt); счётчики ждут, пока догрузятся видимые клетки
//       foot X Z H [Y]    Гомер пешком в точку (x, z) курсом H (машина остаётся где стоит); Y — высота, от которой искать
//                         землю вниз (у двери под навесом; без Y — верх карты высот). Комнаты (пункт 11.2): у двери, дальше press;
//                         Y ниже −10 в комнате — точка в этой же комнате (снимок из каждой её камеры: tests/rooms/emu_cams.txt)
//       flag NAME [N]     autostraight [сек до тормоза] | autodrive | stop | testdmg | testboom | repair | long | view60
//                         отрицательные контроли (проверка проверок, смоук ОБЯЗАН упасть):
//                         longframe [мс, 80] — один длинный кадр; delaycell [с, 3] — выгрузить видимую клетку и
//                         задержать её чтение; badread — выгрузить видимую клетку и испортить её чтение;
//                         crash — исключение процессора (запись по пустому адресу), hang — главный поток встаёт навсегда
//                         (стенд tools/psplink_run.sh обязан сообщить «ВЫЛЕТ» и «ЗАВИСАНИЕ ИГРЫ», этап С.1 аудита 30.09)
//       wait S            ждать S секунд
//       press BTN [S]     держать кнопку S секунд (0.15): up down left right cross circle square triangle start select l r;
//                         main.c подмешивает её в pad.Buttons со следующего кадра (smokePad), как нажатие пальцем
//       shot NAME         строка "SHOT NAME t pos x y z курс" (машина — smokePose из main.c; сверка позиций гейтами, пункт 12);
//                         ждём файл shot_NAME.ok рядом (его кладёт ppsspp_run.sh после снимка окна),
//                         максимум 15 с (30.09, было 5) — иначе снимок запаздывал и ловил уже следующую сцену
//       end               STAT, "END" и выход (конец файла — то же самое)
// Главное меню (пункт 12а): служебные запуски (этот сценарий, smokelog, autobench, goto.txt и др., список — svcStart
// в main.c) стартуют сразу в игре, как раньше. Сценарий, у которого ПЕРВАЯ команда `flag menu`, идёт через меню:
// заставка → меню → загрузка → игра; в меню работают press/wait/shot/end (goto и прочие флаги там не действуют), QUIT GAME
// в паузе возвращает в меню. `flag loadshot` (в меню) — снимок "loading" с середины следующей загрузки.
// Строки меню в smoke.txt (ключ=значение, разбор — tools/menu_check.py):
//   MENU n=K t=.. new=I used=Б free=МБ maxblk=Б order=I,I..   вход в меню (после выгрузки города): куча для сверки
//                                                  кругов; order — номера пунктов сверху вниз, как нарисованы (uiMenuOrder)
//   MENUSEL key=up|down|cross sel=I act=A t=..     курсор после нажатия (A — запущенный пункт или -1)
//   MENUGO item=I t=..                              выбран пункт, дальше загрузка
//   GAME n=K t=.. cells=N ms=.. car=X,Z            уровень загружен, игра пошла
// Счётчики сцены (после успокоения: 0.5 с и 15 кадров подряд без недогруженных видимых клеток, максимум 10 с):
//   work p95/p99/max, пропуски целевого периода (2 обратных хода в 30 FPS / 1 в 60 FPS), кадры с видимой, но
//   не загруженной клеткой, ошибки чтения, отказы памяти, минимум свободной кучи (оценка).
#ifndef SMOKE_H
#define SMOKE_H
#include "exit739.h"
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <stddef.h>
#include <math.h>
#include <reent.h>
#include <pspctrl.h>
#include <psppower.h>

typedef struct { int op; float a, b, c, d; char name[24]; } SmokeCmd;   // op: 1 goto (a b c), 2 flag (name, a), 3 foot (a b c, d = Y или NAN)
// всегда (в том числе для bench.txt): ошибки чтения/памяти клеток, свободная куча
static volatile unsigned smkReadErr, smkAllocFail;       // ++ в loadCell (main.c)
static volatile unsigned smkDelayUs; static volatile int smkBadRead;   // отрицательные контроли для загрузчика
static volatile unsigned *volatile smkBadPtr = (volatile unsigned *)4;   // flag crash: пустой адрес из переменной — компилятор не заменит запись ловушкой
static unsigned smkHeapFree0, smkHeapUsed0; static float smkHeapMin = 1e9f;
// правка 103: список команд GE (main.c: пик заполнения сцены, байт; пропущенные из-за полного списка куски — всего, сцена — от
// smkGeSkip0) и учёт памяти viewer/memwatch.c (отказы любых выделений, наибольший свободный кусок кучи). Слабые ссылки: сборки
// без memwatch.c (тесты) дают 0
static unsigned smkGePeak, smkGeSkip, smkGeSkip0; static float smkBlkMin = 1e9f;
static void acctFlushNow(void);   // acct.h: выход сценария — дописать учёт
static unsigned smkGeBad, smkGeBad0;   // правка 103: куски и текстуры с пустым/битым адресом для GE (main.c pieceOk, bindTexLean) — не нарисованы
extern volatile unsigned memFails __attribute__((weak));
extern unsigned memMaxBlock(void) __attribute__((weak));
static unsigned memFailsNow(void) { return &memFails ? memFails : 0; }
static float memMaxBlockMB(void) { return memMaxBlock ? memMaxBlock() / 1048576.f : -1.f; }
static unsigned smkMemFail0;
static float smkCam[7]; static int smkCamOn, smkCamRel;   // близнецы Б3: камера из сценария («cam»)
static int smkEarly;                                     // smoke.txt открыт раньше smokeInit (smokeEarly, «flag bootshots»)
static int smkSceneFlash;   // TEST16E: кадров белой метки в углу после команды scene (viewer/hud.c hudOverlay)
static unsigned smkFrameAbs;   // №674: номер кадра движка за весь прогон (общий для framecap Камеры и diaglog)
static int smkOn; static char smkFile[256], smkScene[24] = "start", *smkScr, smkShot[24]; static float smkWait, smkT, smkShotT;
// счётчики сцены
#define SMK_BINS 800                                       // гистограмма work по 0.25 мс до 200 мс
static unsigned short smkHist[SMK_BINS]; static unsigned smkFrames, smkMissed, smkMissVisFr, smkMissVisMax, smkPer = 2;
// TEST14 (Сборка3, формат ФОРМАТ_ТЕСТ14_СТРОКИ): замер по кадрам. Состояние кадра задаёт main_loop (smkSetState): 0 game, 2 brief, 3 iris, 5 pause; 1 load — кадр дольше 1 с; 4 cutscene — зарезервировано (ролики идут вне кадров).
// rec — в кадре тест сам пишет снимок/лог на карту (smkWrNow), такие кадры в корзины не входят.
static const char *const SMK_ST[6] = {"game", "load", "brief", "iris", "cutscene", "pause"};
static int smkState; static unsigned smkWrNow, smkRowRec; static int smkRowSt = -1;
static unsigned smkHB[5], smkHGB[5], smkHN, smkHGN;   // HIST: все кадры кроме rec; HISTG: только st=game
typedef struct { float ms, t; int st; float ph[8], build, ge; } SmkWorst; static SmkWorst smkWorst[20]; static int smkNWorst;
static void smkSetState(int st) { smkState = st; }
#define SMK_NS_ST 0
#ifndef TEST14_TEL_OLD
#define TEST14_TEL_OLD 0   // 1 — без покадровой телеметрии (HIST/HISTG/WORST и поля st= rec= в строке таблицы)
#endif
static unsigned smkPress; static float smkPressT;       // press: кнопки сценария и сколько ещё держать
static unsigned smkTap; static float smkTapT;           // №920: tap — кнопка со своим таймером, не отпускает остальные (прыжок при зажатом стике)
static unsigned smkPressAt, smkPressN;                  // время (мкс) и номер последней команды press (отклик экранов, правка Роба 24)
static float smkPose[4];                                 // машина x y z курс (main.c раз в кадр) -> в строку SHOT
static float smkWorkMax, smkSettleT, smkSettleMax; static int smkSettled, smkCalm, smkSettleTimeout;
static unsigned smkErr0, smkAlloc0; static float smkSecMissed, smkSecMissVis;

// Честный запас кучи (пункт 12б): сколько ещё отдаёт sbrk (libcglue _sbrk упирается в настоящий край блока кучи — проба
// sbrk(n) и сразу sbrk(-n)) плюс свободные куски внутри malloc. Под замком malloc (рекурсивный), чтобы фоновый загрузчик
// клеток не вклинился между пробой и возвратом. Прежняя проба malloc с потолком 64 МБ на PSP ни разу не отказывала.
extern void __malloc_lock(struct _reent *); extern void __malloc_unlock(struct _reent *);
static unsigned heapFreeBytes(void) {
  __malloc_lock(_REENT);
  unsigned lo = 0, hi = 64u << 20;
  while (hi - lo > (16u << 10)) { unsigned m = (lo + hi) / 2; void *q = sbrk((ptrdiff_t)m);
    if (q != (void *)-1) { sbrk(-(ptrdiff_t)m); lo = m; } else hi = m; }
  unsigned f = lo + (unsigned)mallinfo().fordblks;
  __malloc_unlock(_REENT);
  return f;
}
static float smokeHeapFree(void) {   // МБ, честно (см. heapFreeBytes)
  float f = heapFreeBytes() / 1048576.f;
  if (f < smkHeapMin) smkHeapMin = f; return f;
}
static unsigned char *volatile smkBallast; static unsigned smkBallastMB;   // служебный балласт рядом с EBOOT (гейт 12б); volatile — иначе -O2 выбрасывает malloc+memset
static int smokeBadRead(void) { if (!smkBadRead) return 0; smkBadRead = 0; return 1; }   // в loadCell
static void smokeLoadHook(void) { if (smkDelayUs) { unsigned d = smkDelayUs; smkDelayUs = 0; sceKernelDelayThread(d); } }   // в загрузчике

// Держим файл журнала открытым: PPSSPP добавляет ~10 мс к каждому open, что искажало FPS даже после вычитания IO из work.
// Строки по-прежнему пишутся сразу (без буфера, чтобы видеть SHOT и след зависания); у игрока без smokelog/scenario файла нет.
static unsigned smkIoUs; static SceUID smkFd = -1;
// Разбор железа TEST19 (п. «11 кадров io 57–89 мс без BBOP»): io в SLOWFRAME — это время самого журнала (smkWrite), не работа игры; каждая строка — отдельный sceIoWrite на карту
// (3–6 мс), а на границе сцены и на старте миссии их 15–20 подряд (STAT, WORST, «MSN plan …»): 57–89 мс в одном кадре, видно и на видео теста. Теперь строки копятся в буфере и уходят
// ОДНОЙ записью не чаще раза в 0,25 с (конец кадра, smokeFrame); сразу — SHOT/END и переполнение. Потеря при вылете — не больше строк одного кадра (след вылета — blackbox.txt).
// SMK_BUF_OLD=1 — прежнее (каждая строка сразу). Счётчики — строка SMKIO в конце журнала
// №913: ежесекундный блок журнала (≈ 70 строк с форматом float, ≈ 3,9 мс эмулятора ≈ 18–22 мс PSP) идёт сразу после ожидания vblank и сдвигает начало следующего кадра: пропуск раз в секунду, которого нет в игре (там smkOn = 0), а work кадра его не показывает. Теперь семью кусками по кадрам; SMK_SPREAD_OLD=1 — одним кадром, как было
#ifndef SMK_SPREAD_OLD
#define SMK_SPREAD_OLD 0
#endif
#ifndef SMK_BUF_OLD
#define SMK_BUF_OLD 0
#endif
static char smkBuf[6144]; static unsigned smkBufN, smkFlushes, smkLines, smkFlushT;
#ifndef SMK_NEST_KEEP_OLD   // №775: сброс журнала из кадра газеты внутри чтения (замок карты у этого же потока) — msioWrite пропускает запись, а буфер
#define SMK_NEST_KEEP_OLD 0   // обнулялся: терялись строки загрузки (PLAYER, LEVEL free). Теперь буфер ждёт конца чтения; переполнение — строка пропадает (smkNestDrop)
#endif
static unsigned smkNestDrop;
static void smkFlush(void) {
  if (!smkBufN) return;
  if (!SMK_NEST_KEEP_OLD && msioOwned()) return;
  unsigned t = sceKernelGetSystemTimeLow();
  if (smkFd < 0) smkFd = sceIoOpen(smkFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
  if (smkFd >= 0 && sceIoWrite(smkFd, smkBuf, smkBufN) < 0) { sceIoClose(smkFd); smkFd = -1; }
  smkBufN = 0; smkFlushes++; smkFlushT = sceKernelGetSystemTimeLow();
  smkIoUs += sceKernelGetSystemTimeLow() - t;
}
static void smkWrite(const char *s) {
  if (!smkFile[0]) return;
  smkLines++;
  if (SMK_BUF_OLD) {
    unsigned t = sceKernelGetSystemTimeLow();
    if (smkFd < 0) smkFd = sceIoOpen(smkFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (smkFd >= 0 && sceIoWrite(smkFd, s, strlen(s)) < 0) { sceIoClose(smkFd); smkFd = -1; }
    smkIoUs += sceKernelGetSystemTimeLow() - t; smkFlushes++;
    return;
  }
  size_t n = strlen(s);
  if (smkBufN + n > sizeof smkBuf) smkFlush();
  if (smkBufN + n > sizeof smkBuf && n <= sizeof smkBuf) { smkNestDrop++; return; }   // №775: сброс отложен (внутри чтения), места нет
  if (n > sizeof smkBuf) { unsigned t = sceKernelGetSystemTimeLow(); if (smkFd < 0) smkFd = sceIoOpen(smkFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777); if (smkFd >= 0) sceIoWrite(smkFd, s, n); smkIoUs += sceKernelGetSystemTimeLow() - t; smkFlushes++; return; }
  memcpy(smkBuf + smkBufN, s, n); smkBufN += (unsigned)n;
  if (!strncmp(s, "SHOT", 4) || !strncmp(s, "END", 3) || sceKernelGetSystemTimeLow() - smkFlushT > 1000000u) smkFlush();   // и не реже раза в секунду (кадры не идут: загрузка, меню)   // раннер ждёт снимок по строке SHOT; конец — файл дописан
}
static float smkPct(float p) {
  unsigned want = (unsigned)(smkFrames * p + .999f), acc = 0;
  for (int i = 0; i < SMK_BINS; i++) { acc += smkHist[i]; if (acc >= want) return (i + 1) * .25f; }
  return SMK_BINS * .25f;
}
static void smkSettle(void) { smkSettled = 0; smkCalm = 0; smkSettleT = 0.f; }
static void lspStatLine(void);   // №599 (viewer/lightsprite.h): строка LSP на сцену
static void wrGlowStat(void);   // №690 (viewer/spritelight.h): строка WRGLOW на сцену
static void slStatLine(void);    // №658 (viewer/spritelight.h): строка SHADOWSPR на сцену
static void smkStat(void) {
  if (!smkOn) return;
  lspStatLine(); slStatLine(); wrGlowStat();
  char s[280]; snprintf(s, sizeof s, "STAT %s frames %u p95 %.2f p99 %.2f max %.2f per %u missed %u missvis %u missvismax %u "
                        "readerr %u allocfail %u heapmin %.2f settle %.1f gepeak %.1f geskip %u memfail %u blkmin %.2f gebad %u%s\n", smkScene, smkFrames, smkPct(.95f), smkPct(.99f),
                        smkWorkMax, smkPer, smkMissed, smkMissVisFr, smkMissVisMax, smkReadErr - smkErr0, smkAllocFail - smkAlloc0,
                        smkHeapMin, smkSettleMax, smkGePeak / 1024.f, smkGeSkip - smkGeSkip0, memFailsNow() - smkMemFail0,
                        smkBlkMin < 1e8f ? smkBlkMin : -1.f, smkGeBad - smkGeBad0, smkSettleTimeout ? " TIMEOUT" : "");
  smkWrite(s);
  if (!TEST14_TEL_OLD) {
    char h[200]; snprintf(h, sizeof h, "HIST scene=%s frames=%u le16=%u le33=%u le50=%u le100=%u gt100=%u\n", smkScene, smkHN, smkHB[0], smkHB[1], smkHB[2], smkHB[3], smkHB[4]); smkWrite(h);
    snprintf(h, sizeof h, "HISTG scene=%s frames=%u le16=%u le33=%u le50=%u le100=%u gt100=%u\n", smkScene, smkHGN, smkHGB[0], smkHGB[1], smkHGB[2], smkHGB[3], smkHGB[4]); smkWrite(h);
    for (int a = 0; a < smkNWorst; a++) for (int b = a + 1; b < smkNWorst; b++) if (smkWorst[b].ms > smkWorst[a].ms) { SmkWorst t = smkWorst[a]; smkWorst[a] = smkWorst[b]; smkWorst[b] = t; }
    for (int a = 0; a < smkNWorst; a++) { const SmkWorst *w = &smkWorst[a]; char wl[300];
      snprintf(wl, sizeof wl, "WORST rank=%d scene=%s t=%.1f ms=%.1f st=%s ph=ph0:%.1f,ph1:%.1f,ph2:%.1f,ph3:%.1f,ph4:%.1f,ph5:%.1f,ph6:%.1f,ph7:%.1f,build:%.1f,gesync:%.1f\n", a + 1, smkScene, w->t, w->ms, SMK_ST[w->st],
               w->ph[0], w->ph[1], w->ph[2], w->ph[3], w->ph[4], w->ph[5], w->ph[6], w->ph[7], w->build, w->ge); smkWrite(wl); }
  }
}
static void smkReset(void) {
  memset(smkHB, 0, sizeof smkHB); memset(smkHGB, 0, sizeof smkHGB); smkHN = smkHGN = 0; smkNWorst = 0;
  memset(smkHist, 0, sizeof smkHist); smkFrames = smkMissed = smkMissVisFr = smkMissVisMax = 0; smkWorkMax = 0.f;
  smkErr0 = smkReadErr; smkAlloc0 = smkAllocFail; smkHeapMin = 1e9f; smkSettleMax = 0.f; smkSettleTimeout = 0;
  smkGePeak = 0; smkGeSkip0 = smkGeSkip; smkMemFail0 = memFailsNow(); smkBlkMin = 1e9f; smkGeBad0 = smkGeBad;
}
// base — путь EBOOT (строка path из main.c). Зовётся всегда: оценка свободной кучи нужна и bench.txt
static void smokeInit(const char *base) {
  if (SHR_REL_SMK) { smkHeapFree0 = heapFreeBytes(); smkHeapUsed0 = mallinfo().uordblks; smkReset(); smkSettle(); return; }   // релиз (viewer/release.h): ни ballast, ни smokelog, ни scenario.txt
  char fn[256]; strcpy(fn, base); char *sl = strrchr(fn, '/'); char *tail = sl ? sl + 1 : fn;
  strcpy(tail, "ballast"); SceUID bf = sceIoOpen(fn, PSP_O_RDONLY, 0);   // файл «ballast» с числом МБ: занять и заполнить
  if (bf >= 0) { char t[16] = {0}; sceIoRead(bf, t, sizeof t - 1); sceIoClose(bf); smkBallastMB = (unsigned)atoi(t);
    if (smkBallastMB && smkBallastMB < 48) { smkBallast = malloc((size_t)smkBallastMB << 20); if (smkBallast) memset(smkBallast, 0x5a, (size_t)smkBallastMB << 20); else smkBallastMB = 0; } }
  smkHeapFree0 = heapFreeBytes(); smkHeapUsed0 = mallinfo().uordblks;
  strcpy(tail, "smokelog"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); smkOn = 1; }
  strcpy(tail, "scenario.txt"); f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (f >= 0) { int n = sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
    smkScr = malloc(n + 1); if (smkScr) { n = sceIoRead(f, smkScr, n); smkScr[n > 0 ? n : 0] = 0; smkOn = 1; } sceIoClose(f); }
  smkReset(); smkSettle();
  if (!smkOn) return;
  strcpy(tail, "smoke.txt"); strcpy(smkFile, fn);
  if (!smkEarly) { f = sceIoOpen(smkFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f >= 0) sceIoClose(f); }   // smokeEarly уже писал
  { __malloc_lock(_REENT); struct mallinfo mi = mallinfo(); void *b0 = sbrk(0); void *t1 = sbrk(1 << 20); if (t1 != (void *)-1) sbrk(-(1 << 20)); __malloc_unlock(_REENT);   // под замком, как heapFreeBytes
    char d[200]; snprintf(d, sizeof d, "# heapdbg sbrk0 %p sbrk1MB %s arena %u uord %u ford %u keep %u maxfree %u totalfree %u\n", b0, t1 == (void *)-1 ? "FAIL" : "ok",
                          (unsigned)mi.arena, (unsigned)mi.uordblks, (unsigned)mi.fordblks, (unsigned)mi.keepcost, (unsigned)sceKernelMaxFreeMemSize(), (unsigned)sceKernelTotalFreeMemSize()); smkWrite(d); }
  char s[160]; snprintf(s, sizeof s, "# heap free at start %.2f MB (ballast %u MB)\n# t scene fps work workmax cpu tris draws ramMB mode missed missvis readerr allocfail heapMB\n",
                        smkHeapFree0 / 1048576.f, smkBallastMB);
  smkWrite(s);
}
static unsigned smkBtn(const char *n) {
  static const char *nm[] = {"up", "down", "left", "right", "cross", "circle", "square", "triangle", "start", "select", "l", "r"};
  static const unsigned bt[] = {PSP_CTRL_UP, PSP_CTRL_DOWN, PSP_CTRL_LEFT, PSP_CTRL_RIGHT, PSP_CTRL_CROSS, PSP_CTRL_CIRCLE, PSP_CTRL_SQUARE,
                                PSP_CTRL_TRIANGLE, PSP_CTRL_START, PSP_CTRL_SELECT, PSP_CTRL_LTRIGGER, PSP_CTRL_RTRIGGER};
  for (int i = 0; i < 12; i++) if (!strcmp(n, nm[i])) return bt[i];
  return 0;
}
// Замер-тур на PSP (просьба Роба 03.10 вечер: «версия, которая сама походит в комнатах и по местам и всё запишет»): на железе
// снимок «shot» делает сама игра — кадр в shots/ИМЯ.bmp рядом с EBOOT (24 бита, 480×272) и строка SHOTFILE с числом пурпурных
// (непокрытых, при clearchk) пикселей; в эмуляторе снимок по-прежнему делает окно (shot_ИМЯ.ok), «flag shotdump 1» — как на железе
static int smkEmu = -1, smkShotDumpForce; static char smkDumpName[24];
static int smokeIsEmu(void) {   // PPSSPP отвечает на устройство «emulator:» (команда 3 — «это эмулятор»), настоящая PSP — ошибкой
  if (smkEmu < 0) { int v = 0; smkEmu = sceIoDevctl("emulator:", 3, NULL, 0, &v, 4) == 0 && v; }
  return smkEmu;
}
static void smkSettle(void);
// Диагностика окна «Оптимизация» (№932, 24.10): мерцание — доля пикселей верхней половины кадра (480×136), изменившихся против прошлого кадра; строка FLICK раз в 30 кадров («flag flick 1» в сценарии,
// только смоук). chg — любое изменение цвета, big — сумма |ΔRGB| ≥ 32 (не шаг квантования 5650); in* — полоса x 40..439, y 64..135 (ниже строк отладочного HUD, там мерцает цифрами). Метод: камера крутится на месте («flag camyaw»), мерцание = то, что сверх
// сдвига краёв, поэтому сравниваются варианты на одном движении
static unsigned short flickMap[480 * 136]; static int flickOn; static unsigned *flickPrev, *flickPrev2; static int flickHave; static unsigned flickN; static double flickS[8];
static double flickB[5][3]; static unsigned flickBN;   // полосы расстояния: 0 <30 м, 1 30–60, 2 60–120, 3 >120, 4 небо (глубины нет): [площадь, туда-обратно видимое, изменилось сильно]
static void flickStep(const void *fb, int fb16, int bufw, const void *zb, float zn, float zf) {
  if (SHR_REL_SMK) return;   // релиз: датчика нет (код выбрасывается, сборка байт в байт прежняя)
  if (!flickOn) { flickHave = 0; return; }
  if (!flickPrev) { flickPrev = (unsigned *)malloc(480 * 136 * 4); flickPrev2 = (unsigned *)malloc(480 * 136 * 4); if (!flickPrev || !flickPrev2) return; }
  const unsigned char *base = (const unsigned char *)((unsigned)fb | 0x40000000u); const unsigned short *zbase = (const unsigned short *)((unsigned)zb | 0x40000000u);
  unsigned bc[5][3] = {{0}}; unsigned c = 0, b = 0, ic = 0, ib = 0, os = 0, ios = 0, ov = 0, iov = 0;
  for (int y = 0; y < 136; y++) for (int x = 0; x < 480; x++) {
    unsigned r, g, bl;
    if (fb16) { unsigned p = ((const unsigned short *)base)[y * bufw + x]; r = (p & 31) << 3; g = ((p >> 5) & 63) << 2; bl = (p >> 11) << 3; }
    else { unsigned p = ((const unsigned *)base)[y * bufw + x]; r = p & 255; g = (p >> 8) & 255; bl = (p >> 16) & 255; }
    unsigned v = r | g << 8 | bl << 16, o = flickPrev[y * 480 + x], o2 = flickPrev2[y * 480 + x]; flickPrev2[y * 480 + x] = o; flickPrev[y * 480 + x] = v;
    if (!flickHave) continue;
    int d = abs((int)r - (int)(o & 255)) + abs((int)g - (int)((o >> 8) & 255)) + abs((int)bl - (int)((o >> 16) & 255));
    int inner = x >= 40 && x < 440 && y >= 64;
    int bnd = 4; { unsigned zv = zbase[y * bufw + x]; if (zv) { float zn_ = 1.f - zv / 32767.5f, dd_ = 2.f * zf * zn / ((zf + zn) - (zf - zn) * zn_); bnd = dd_ < 30.f ? 0 : dd_ < 60.f ? 1 : dd_ < 120.f ? 2 : 3; } }
    bc[bnd][0]++; if (d >= 32) bc[bnd][2]++;
    if (d > 0) { c++; ic += inner; } if (d >= 32) { b++; ib += inner; }
    if (flickHave > 1 && d >= 8) { int d2 = abs((int)r - (int)(o2 & 255)) + abs((int)g - (int)((o2 >> 8) & 255)) + abs((int)bl - (int)((o2 >> 16) & 255)); if (d2 <= 2) { os++; ios += inner; if (d >= 24) { ov++; iov += inner; bc[bnd][1]++; if (flickMap[y * 480 + x] < 65535) flickMap[y * 480 + x]++; } } }   // туда-обратно: кадр t равен t-2, а t-1 другой — мерцание, а не движение
  }
  if (flickHave < 2) { flickHave++; if (flickHave < 2) return; }
  for (int k = 0; k < 5; k++) for (int j = 0; j < 3; j++) flickB[k][j] += bc[k][j]; flickBN++;
  flickS[0] += c; flickS[1] += b; flickS[2] += ic; flickS[3] += ib; flickS[4] += os; flickS[5] += ios; flickS[6] += ov; flickS[7] += iov; flickN++;
  if (flickN >= 30) { char l[320]; double n = flickN, all = 480.0 * 136.0, in = 400.0 * 72.0;
    snprintf(l, sizeof l, "FLICK t=%.1f sc=%s n=%u chg=%.3f big=%.3f inchg=%.3f inbig=%.3f osc=%.3f inosc=%.3f vis=%.3f invis=%.3f (%% пикселей верхней половины за кадр; vis — туда-обратно с шагом цвета ≥ 24, не дизеринг)\n", smkT, smkScene, flickN, 100.0 * flickS[0] / n / all, 100.0 * flickS[1] / n / all, 100.0 * flickS[2] / n / in, 100.0 * flickS[3] / n / in, 100.0 * flickS[4] / n / all, 100.0 * flickS[5] / n / in, 100.0 * flickS[6] / n / all, 100.0 * flickS[7] / n / in);
    smkWrite(l);
    { char m[300]; double nn = flickBN ? flickBN : 1; snprintf(m, sizeof m, "FLICKB t=%.1f sc=%s area%%(<30,30-60,60-120,>120,sky)=%.1f/%.1f/%.1f/%.1f/%.1f vis%%=%.3f/%.3f/%.3f/%.3f/%.3f big%%=%.2f/%.2f/%.2f/%.2f/%.2f\n", smkT, smkScene,
        100.0 * flickB[0][0] / nn / all, 100.0 * flickB[1][0] / nn / all, 100.0 * flickB[2][0] / nn / all, 100.0 * flickB[3][0] / nn / all, 100.0 * flickB[4][0] / nn / all,
        flickB[0][0] ? 100.0 * flickB[0][1] / flickB[0][0] : 0, flickB[1][0] ? 100.0 * flickB[1][1] / flickB[1][0] : 0, flickB[2][0] ? 100.0 * flickB[2][1] / flickB[2][0] : 0, flickB[3][0] ? 100.0 * flickB[3][1] / flickB[3][0] : 0, flickB[4][0] ? 100.0 * flickB[4][1] / flickB[4][0] : 0,
        flickB[0][0] ? 100.0 * flickB[0][2] / flickB[0][0] : 0, flickB[1][0] ? 100.0 * flickB[1][2] / flickB[1][0] : 0, flickB[2][0] ? 100.0 * flickB[2][2] / flickB[2][0] : 0, flickB[3][0] ? 100.0 * flickB[3][2] / flickB[3][0] : 0, flickB[4][0] ? 100.0 * flickB[4][2] / flickB[4][0] : 0); smkWrite(m); memset(flickB, 0, sizeof flickB); flickBN = 0; }
    flickS[0] = flickS[1] = flickS[2] = flickS[3] = flickS[4] = flickS[5] = flickS[6] = flickS[7] = 0; flickN = 0; }
}
// карта мерцания за прогон: прошлый кадр верхней половины (тускло) + красным число раз, когда пиксель вернулся к значению двух кадров назад (osc); файл shots/osc_<сцена>.bmp, потом карта обнуляется
static void flickDump(const char *tag) {
  if (SHR_REL_SMK || !flickPrev || !smkFile[0]) return;
  char fn[256]; strcpy(fn, smkFile); char *sl = strrchr(fn, '/'); strcpy(sl ? sl + 1 : fn, "shots"); sceIoMkdir(fn, 0777);
  size_t l = strlen(fn); snprintf(fn + l, sizeof fn - l, "/osc_%s.bmp", tag);
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f < 0) return;
  int W = 480, H = 136; unsigned rowB = W * 3, size = 54 + rowB * H; unsigned char hd[54] = {'B', 'M'}; unsigned v = 54; memcpy(hd + 2, &size, 4); memcpy(hd + 10, &v, 4); v = 40; memcpy(hd + 14, &v, 4);
  memcpy(hd + 18, &W, 4); memcpy(hd + 22, &H, 4); hd[26] = 1; hd[28] = 24; v = rowB * H; memcpy(hd + 34, &v, 4); sceIoWrite(f, hd, 54);
  static unsigned char row[480 * 3];
  for (int y = H - 1; y >= 0; y--) { for (int x = 0; x < W; x++) { unsigned p = flickPrev[y * 480 + x], c = flickMap[y * 480 + x]; unsigned r = (p & 255) >> 1, g = ((p >> 8) & 255) >> 1, b = ((p >> 16) & 255) >> 1;
      if (c) { unsigned k = 90 + c * 40; if (k > 255) k = 255; r = k; g >>= 1; b >>= 1; }
      row[x * 3] = (unsigned char)b; row[x * 3 + 1] = (unsigned char)g; row[x * 3 + 2] = (unsigned char)r; } sceIoWrite(f, row, rowB); }
  sceIoClose(f); memset(flickMap, 0, sizeof flickMap);
}
// кадр готов (после sceGuSync, до смены буферов): fb — адрес буфера в VRAM, fb16 — 5650 или 8888, w — ширина строки буфера
static void smokeShotDump(const void *fb, int fb16, int bufw, int clearChkOn) {
#ifndef SMK_NO_DUMP
  if (!smkDumpName[0] || !smkFile[0]) return;
  smkWrNow = 1;   // TEST14: следующий кадр помечается rec=1
  char fn[256]; strcpy(fn, smkFile); char *sl = strrchr(fn, '/'); strcpy(sl ? sl + 1 : fn, "shots"); sceIoMkdir(fn, 0777);
  size_t l = strlen(fn); snprintf(fn + l, sizeof fn - l, "/%s.bmp", smkDumpName);
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  int W = 480, H = 272, mag = 0; unsigned rowB = W * 3, size = 54 + rowB * H;
  if (f >= 0) {
    unsigned char hd[54] = {'B', 'M'}; memcpy(hd + 2, &size, 4); unsigned v = 54; memcpy(hd + 10, &v, 4); v = 40; memcpy(hd + 14, &v, 4);
    memcpy(hd + 18, &W, 4); memcpy(hd + 22, &H, 4); hd[26] = 1; hd[28] = 24; v = rowB * H; memcpy(hd + 34, &v, 4);
    sceIoWrite(f, hd, 54);
    static unsigned char row[480 * 3 * 8];
    const unsigned char *base = (const unsigned char *)((unsigned)fb | 0x40000000u);   // некэшируемый адрес VRAM
    for (int y = H - 1; y >= 0;) {   // BMP — снизу вверх, по 8 строк за запись
      int n = 0;
      for (int k = 0; k < 8 && y >= 0; k++, y--, n++)
        for (int x = 0; x < W; x++) {
          unsigned char *o = row + n * rowB + x * 3; unsigned r, g, b;
          if (fb16) { unsigned p = ((const unsigned short *)base)[y * bufw + x]; r = (p & 31) << 3; g = ((p >> 5) & 63) << 2; b = (p >> 11) << 3; mag += p == 0xF81F; }
          else { unsigned p = ((const unsigned *)base)[y * bufw + x]; r = p & 255; g = (p >> 8) & 255; b = (p >> 16) & 255; mag += (p & 0xffffff) == 0xff00ff; }
          o[0] = (unsigned char)b; o[1] = (unsigned char)g; o[2] = (unsigned char)r;
        }
      sceIoWrite(f, row, n * rowB);
    }
    sceIoClose(f);
  }
  char s[128]; snprintf(s, sizeof s, "SHOTFILE %s ok=%d bytes=%u magenta=%d clearchk=%d t=%.1f\n", smkDumpName, f >= 0, size, mag, clearChkOn, smkT); smkWrite(s);
  smkDumpName[0] = 0; smkSettle();   // кадр записи на карту медленный — не в счёт сцены
#endif
}
static void smokePose(float x, float y, float z, float h) { smkPose[0] = x; smkPose[1] = y; smkPose[2] = z; smkPose[3] = h; }
static unsigned smokePad(void) { return smkPress | smkTap; }
static int smkStickOn; static unsigned char smkLx = 128, smkLy = 128; static float smkStickT;   // №947: команда сценария «stick LX LY сек» — аналоговый стик (-1..1; вверх = LY -1), по истечении отпускается
static void smokeStick(unsigned char *lx, unsigned char *ly) { if (smkStickOn) { *lx = smkLx; *ly = smkLy; } }   // сразу после sceCtrlPeekBufferPositive: pad.Buttons |= smokePad()
// раз в кадр (до рисования): 1 = есть команда для main.c в *c (goto/flag); wait/shot/scene/end/longframe — здесь
static int smokeNext(float dt, SmokeCmd *c) {
  if (!smkScr) return 0;
  smkT += dt; if (smkPressT > 0.f && (smkPressT -= dt) <= 0.f) smkPress = 0;
  if (smkTapT > 0.f && (smkTapT -= dt) <= 0.f) smkTap = 0;
  if (smkStickT > 0.f && (smkStickT -= dt) <= 0.f) smkStickOn = 0;
  if (smkWait > 0.f) { smkWait -= dt; return 0; }
  if (smkShot[0]) {                                        // ждём подтверждения снимка
    char fn[256], nm[40]; strcpy(fn, smkFile); char *sl = strrchr(fn, '/'); snprintf(nm, sizeof nm, "shot_%s.ok", smkShot);
    strcpy(sl ? sl + 1 : fn, nm); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    smkShotT += dt; if (f >= 0) sceIoClose(f); else if (smkShotT < 15.f) return 0;   // 30.09: 5 -> 15 с — под нагрузкой Мака (load 90)
                                                           // снимок окна идёт секунды, и кадр ловил уже следующий шаг сценария
    smkShot[0] = 0; smkSettle();                           // кадр снимка окна в эмуляторе медленный (~33 мс) — не в счёт
  }
  while (*smkScr) {
    char ln[96]; int n = 0; while (*smkScr && *smkScr != '\n' && n < 95) ln[n++] = *smkScr++;
    ln[n] = 0; while (*smkScr && *smkScr != '\n') smkScr++; if (*smkScr) smkScr++;
    char cmd[16] = "", arg[24] = ""; float a = 0, b = 0, d = 0;
    if (sscanf(ln, "%15s", cmd) != 1 || cmd[0] == '#') continue;
    char s[96];
    if (!strcmp(cmd, "press")) { float sec = .15f; sscanf(ln, "%*s %23s %f", arg, &sec); smkPress |= smkBtn(arg); smkPressT = sec;
      smkPressAt = sceKernelGetSystemTimeLow(); smkPressN++;
      snprintf(s, sizeof s, "PRESS %s %.1f\n", arg, smkT); smkWrite(s); continue; }
    if (!strcmp(cmd, "tap")) { float sec = .15f; sscanf(ln, "%*s %23s %f", arg, &sec); smkTap |= smkBtn(arg); smkTapT = sec; continue; }   // №920: «press» отпускает все кнопки вместе, а прыжок при зажатом стике нужен отдельно
    if (!strcmp(cmd, "stick")) { float sx = 0, sy = 0, sec = .15f; sscanf(ln, "%*s %f %f %f", &sx, &sy, &sec); smkLx = (unsigned char)(128.f + 127.f * sx); smkLy = (unsigned char)(128.f + 127.f * sy); smkStickOn = 1; smkStickT = sec; continue; }   // №947
    if (!strcmp(cmd, "wait")) { sscanf(ln, "%*s %f", &smkWait); return 0; }
    if (!strcmp(cmd, "scene")) { smkStat(); smkReset(); smkSceneFlash = 3; sscanf(ln, "%*s %23s", smkScene); snprintf(s, sizeof s, "SCENE %s %.1f\n", smkScene, smkT); smkWrite(s); continue; }
    if (!strcmp(cmd, "shot")) { smkWrNow = 1; sscanf(ln, "%*s %23s", arg); char t[128]; snprintf(t, sizeof t, "SHOT %s %.1f pos %.2f %.2f %.2f %.4f\n", arg, smkT,
                                  smkPose[0], smkPose[1], smkPose[2], smkPose[3]); smkWrite(t);
                                if (!smokeIsEmu() || smkShotDumpForce) { strcpy(smkDumpName, arg); return 0; }   // замер-тур: снимок пишет сама игра
                                strcpy(smkShot, arg); smkShotT = 0.f; return 0; }
    if (!strcmp(cmd, "goto")) { sscanf(ln, "%*s %f %f %f", &a, &b, &d); snprintf(s, sizeof s, "TELEPORT t=%.2f kind=goto x=%.0f z=%.0f\n", smkT, a, b); smkWrite(s); c->op = 1; c->a = a; c->b = b; c->c = d; smkSettle(); return 1; }
    if (!strcmp(cmd, "foot")) { float y = NAN; sscanf(ln, "%*s %f %f %f %f", &a, &b, &d, &y); snprintf(s, sizeof s, "TELEPORT t=%.2f kind=foot x=%.0f z=%.0f\n", smkT, a, b); smkWrite(s); c->op = 3; c->a = a; c->b = b; c->c = d; c->d = y; smkSettle(); return 1; }
    if (!strcmp(cmd, "flag")) {
      a = -1.f; b = NAN; sscanf(ln, "%*s %23s %f %f", c->name, &a, &b);
      if (!strcmp(c->name, "bisect")) { char hx_[16] = {0}; if (sscanf(ln, "%*s %*s %15s", hx_) == 1) a = (float)strtoul(hx_, 0, 16); }   // flag bisect HEX — маска bisect.h на ходу (hex, не десятичное)
      c->op = 2; c->a = a < 0.f ? 0.f : a; c->d = a; c->b = b;   // d, b — сырые числа (flag wp X Z)
      if (!strcmp(c->name, "repair")) { snprintf(s, sizeof s, "TELEPORT t=%.2f kind=repair\n", smkT); smkWrite(s); }   // «flag repair» пересаживает Гомера за руль = телепорт к машине (L6: 409 м)
      if (!strcmp(c->name, "longframe")) { sceKernelDelayThread((unsigned)((a > 0.f ? a : 80.f) * 1000.f)); snprintf(s, sizeof s, "FAULT longframe %.1f\n", smkT); smkWrite(s); continue; }
      if (!strcmp(c->name, "delaycell")) { smkDelayUs = (unsigned)((a > 0.f ? a : 3.f) * 1e6f); snprintf(s, sizeof s, "FAULT delaycell %.1f\n", smkT); smkWrite(s); }
      if (!strcmp(c->name, "badread")) { smkBadRead = 1; snprintf(s, sizeof s, "FAULT badread %.1f\n", smkT); smkWrite(s); }
      if (!strcmp(c->name, "crash")) { snprintf(s, sizeof s, "FAULT crash %.1f\n", smkT); smkWrite(s); *smkBadPtr = 0x5a; }   // PSPLink ловит исключение
      if (!strcmp(c->name, "hang")) { snprintf(s, sizeof s, "FAULT hang %.1f\n", smkT); smkWrite(s); for (;;) sceKernelDelayThread(1000000); }
      if (!strcmp(c->name, "long") || !strcmp(c->name, "view60")) smkSettle();
      return 1;
    }
    if (!strcmp(cmd, "expect")) {   // TEST14: «expect X Z [ДОПУСК_м [Y ДОПУСК_Y]]» — игрок там, где сцена рассчитывает; иначе SCENE_FAIL (разбор отделяет «не дошли до места» от скорости)
      float ex = 0.f, ez = 0.f, tol = 25.f, ey = NAN, ty = 8.f; sscanf(ln, "%*s %f %f %f %f %f", &ex, &ez, &tol, &ey, &ty);
      float dd = hypotf(smkPose[0] - ex, smkPose[2] - ez); int ok = dd <= tol && (isnan(ey) || fabsf(smkPose[1] - ey) <= ty); char es[220];
      snprintf(es, sizeof es, "%s scene=%s want=%.1f,%.1f got=%.1f,%.1f,%.1f dist=%.1f tol=%.1f t=%.1f\n", ok ? "SCENE_OK" : "SCENE_FAIL", smkScene, ex, ez, smkPose[0], smkPose[1], smkPose[2], dd, tol, smkT); smkWrite(es); continue; }
    if (!strcmp(cmd, "cam") || !strcmp(cmd, "camrel")) { smkCamRel = cmd[3] == 'r';   // №863: camrel dx dy dz fx fy fz — глаз от героя (ноги)
   // близнецы Б3: камера порта в позе оригинала (оси порта: z = −z оригинала) — «cam x y z fx fy fz [fovy]»,
      float v[7] = {0, 0, 0, 0, 0, 0, 0};   // «cam off» — своя камера; fovy в градусах (0 — как у игры)
      smkCamOn = sscanf(ln, "%*s %f %f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) >= 6;
      for (int k = 0; k < 7; k++) smkCam[k] = v[k];
      snprintf(s, sizeof s, "CAM %s %.1f\n", smkCamOn ? "on" : "off", smkT); smkWrite(s); continue; }
    if (!strcmp(cmd, "end")) break;
  }
  smkStat(); { char sio[64]; snprintf(sio, sizeof sio, "SMKIO flushes=%u lines=%u buffered=%d\n", smkFlushes, smkLines, !SMK_BUF_OLD); smkWrite(sio); } smkWrite("END\n"); smkFlush(); acctFlushNow(); if (smkFd >= 0 && EXIT739_OLD) { sceIoClose(smkFd); smkFd = -1; }   // №739: журнал закрывает exitSafeNow (строка EXIT739)
  sceKernelDelayThread(300000); if (EXIT739_OLD) sceKernelExitGame(); exitReq = 1; smkScr = 0; return 0;   // №739: smokeNext идёт в главном потоке — следующий кадр выходит через exitSafeNow (звук и загрузчик остановлены)
}
// 12а: первая команда сценария — «flag menu»? (читается до smokeInit: решение о меню принимается раньше)
static int smokeWantsMenu(const char *base) {
  char fn[256], b[2048]; strcpy(fn, base); char *sl = strrchr(fn, '/'); strcpy(sl ? sl + 1 : fn, "scenario.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  int n = sceIoRead(f, b, sizeof b - 1); sceIoClose(f); b[n > 0 ? n : 0] = 0;
  for (char *p = b; *p;) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '#') { while (*p && *p != '\n') p++; continue; }
    char c1[16] = "", c2[24] = ""; sscanf(p, "%15s %23s", c1, c2);
    return !strcmp(c1, "flag") && !strcmp(c2, "menu");
  }
  return 0;
}
// «flag NAME» среди первых подряд идущих flag сценария (читается до smokeInit)
static int smokeWantsFlag(const char *base, const char *name) {
  char fn[256], b[2048]; strcpy(fn, base); char *sl = strrchr(fn, '/'); strcpy(sl ? sl + 1 : fn, "scenario.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  int n = sceIoRead(f, b, sizeof b - 1); sceIoClose(f); b[n > 0 ? n : 0] = 0;
  for (char *p = b; *p;) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '#') { while (*p && *p != '\n') p++; continue; }
    char c1[16] = "", c2[24] = ""; sscanf(p, "%15s %23s", c1, c2);
    if (strcmp(c1, "flag")) return 0;
    if (!strcmp(c2, name)) return 1;
    while (*p && *p != '\n') p++;
  }
  return 0;
}
// «flag level N» среди первых flag сценария (tests/cars, эмулятор): служебный запуск уровня N — данные из папки l<N>/ рядом
// с EBOOT (viewer/main.c levelFile). Выбора уровня в меню ещё нет; без флага — 0 (уровень 1, как было)
static int smokeWantsLevel(const char *base) {
  char fn[256], b[2048]; strcpy(fn, base); char *sl = strrchr(fn, '/'); strcpy(sl ? sl + 1 : fn, "scenario.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  int n = sceIoRead(f, b, sizeof b - 1); sceIoClose(f); b[n > 0 ? n : 0] = 0;
  for (char *p = b; *p;) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p == '#') { while (*p && *p != '\n') p++; continue; }
    char c1[16] = "", c2[24] = ""; int lv = 0; sscanf(p, "%15s %23s %d", c1, c2, &lv);
    if (strcmp(c1, "flag")) return 0;
    if (!strcmp(c2, "level")) return lv >= 1 && lv <= 7 ? lv : 0;
    while (*p && *p != '\n') p++;
  }
  return 0;
}
// интро (viewer/movie.h): «flag intro» — служебный запуск всё же играет интро (и титульный экран)
static int smokeWantsIntro(const char *base) { return smokeWantsFlag(base, "intro"); }
// экраны до игры (tools/screens_gate.sh): «flag bootshots» — smoke.txt открывается сразу после заставки, чтобы снять
// лицензию и BootupLoad (main.c scrShot); smokeInit потом дописывает в тот же файл, не обнуляя его
static int smokeEarly(const char *base) {
  if (SHR_REL_SMK) return 0;   // релиз: снимков экранов до игры нет
  if (!smokeWantsFlag(base, "bootshots")) return 0;
  strcpy(smkFile, base); char *sl = strrchr(smkFile, '/'); strcpy(sl ? sl + 1 : smkFile, "smoke.txt");
  SceUID f = sceIoOpen(smkFile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f >= 0) sceIoClose(f);
  smkOn = 1; smkEarly = 1; return 1;
}
// 12а: снимок оттуда, где сценарий не идёт (экран загрузки): строка SHOT и ожидание shot_NAME.ok до 5 с
static void smokeShotNow(const char *name) {
  if (!smkOn) return;
  char s[64], fn[256], nm[40]; snprintf(s, sizeof s, "SHOT %s %.1f\n", name, smkT); smkWrite(s);
  strcpy(fn, smkFile); char *sl = strrchr(fn, '/'); snprintf(nm, sizeof nm, "shot_%s.ok", name); strcpy(sl ? sl + 1 : fn, nm);
  for (int k = 0; k < 250; k++) { SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f >= 0) { sceIoClose(f); break; } sceKernelDelayThread(20000); }
}
// 1 = main.c должен выгрузить одну видимую клетку (delaycell/badread)
static int smokeDropCell(const char *name) { return !strcmp(name, "delaycell") || !strcmp(name, "badread"); }
// раз в кадр после ожидания развёртки: missed = кадр дольше целевого периода per (в обратных ходах), missVis = видимые недогруженные клетки
// №ME-доля (10.10, Роб: «разрез по подсистемам на железе»): средние за секунду мс на кадр по фазам кадра (отметки smkMark 0..7: ввод, машина, Гомер+пропы,
// комнаты, миссии, трафик+камера+stream (6), отсечение+респаун (7)), сборка списка GE (cpu - logic), ожидание видеочипа после sceGuFinish (sceGuSync)
// и ожидание обратного хода (vblank). Строка PHSEC раз в секунду в smoke.txt; PHSEC_OLD=1 — без неё
#ifndef PHSEC_OLD
#define PHSEC_OLD 0
#endif
// Правка «таймеры фаз» (тур SHARTEST4: сборка списка 11–16 мс не разложена): время между соседними BBP() — в фазу, что началась первой.
// В PHSEC: x_city (город), x_car3d, x_npc, x_trf, x_mcr (миссионные машины), x_props, x_fx, x_ui — мс на кадр. Только при smokelog и не в релизе
// (SHAR_RELEASE: smokelog не открывается). PHSEC_OLD=1 — без них. Обновляет BBP в main_state.h
enum { BBP_NONE, BBP_INPUT, BBP_CAR, BBP_WORLD, BBP_ROOMS, BBP_MSN, BBP_TRAFFIC, BBP_STREAM, BBP_CITY, BBP_CAR3D, BBP_NPC3D,
       BBP_TRF3D, BBP_MCR3D, BBP_PROPS3D, BBP_FX3D, BBP_UI, BBP_GESYNC, BBP_VBLANK, BBP_SECOND, BBP_LOAD, BBP_N };
#ifndef SMK_SETTLE936_OLD
#define SMK_SETTLE936_OLD 0
#endif
static int smkSettle936 = !SMK_SETTLE936_OLD;
static unsigned smkBbT; static int smkBbPh = -1; static double smkBbSum[BBP_N];
// и вызовы рисования / треугольники / смены текстуры по тем же фазам (строка PHDRAW): d — вызовы, k — тысячи треугольников, b — привязки текстур
static double smkBbD[BBP_N], smkBbK[BBP_N], smkBbB[BBP_N], smkBbW[BBP_N]; static long smkBbLd, smkBbLk, smkBbLb; static int smkBbLw;   // W — слов команд, записанных в список GE за фазу
static inline void smkBbp(int n, int d, long k, int b, int w) {
  if (!smkOn || PHSEC_OLD || SHR_REL_SMK) return;
  unsigned t = sceKernelGetSystemTimeLow();
  if (smkBbPh >= 0) { smkBbSum[smkBbPh] += (t - smkBbT) / 1000.0;
    smkBbD[smkBbPh] += d >= smkBbLd ? d - smkBbLd : d; smkBbK[smkBbPh] += (k >= smkBbLk ? k - smkBbLk : k) / 1000.0; smkBbB[smkBbPh] += b >= smkBbLb ? b - smkBbLb : b; smkBbW[smkBbPh] += w >= smkBbLw ? (w - smkBbLw) / 4.0 : w / 4.0; }   // счётчики кадра обнуляются в начале кадра
  smkBbLd = d; smkBbLk = k; smkBbLb = b; smkBbLw = w; smkBbT = t; smkBbPh = n;
}
// подфазы x_props (место ТОП-1 тура SHARTEST6): smkSub(k) — начало подфазы k (0 пропы, 1 стейтпропы+пачка, 2 билборды, 3 сборное (collect), 4 монеты); smkSub(-1) — конец
static unsigned smkGgReplay, smkNearFrames, smkNearLow, smkNearWhy[7], smkNearCarOnly, smkNearPass; static double smkSubSum[5], smkGgMs, smkGgPieces, smkGgTris, smkSpPoseMs, smkSpFlushMs, smkSpLoopMs, smkSpInst, smkSpCand, smkSpDrawn, smkBbCand, smkBbQuads, smkBbCalls, smkBbLoopMs, smkBbDrawMs, smkBbTailMs, smkBbTm[6], smkSpT[3], smkSpT2[3], smkLspList, smkLspAll, smkLspViol, smkTrfCarMs, smkTrfPedMs, smkTrfN[3], smkTrfPose, smkTrfDraw, smkTrfPoseN, smkTrfWords, smkSkinW[7], smkTrfPc[5], smkPbT[5], smkSpVis, smkSpPcs, smkCoinAct, smkCoinDrawn, smkCoinPcs, smkCtMask, smkCtRisky, smkCtList, smkCtBuild; static unsigned smkSubT; static int smkSubK = -1;
static inline void smkSub(int k) {
  if (!smkOn || PHSEC_OLD || SHR_REL_SMK) return;
  unsigned t = sceKernelGetSystemTimeLow(); if (smkSubK >= 0) smkSubSum[smkSubK] += (t - smkSubT) / 1000.0; smkSubT = t; smkSubK = k;
}
static unsigned smkXT[3]; static double smkXSum[3]; static void smkMarkX(int i) { smkXT[i] = sceKernelGetSystemTimeLow(); }   // разбивка фазы 1: gameCarStep, carVis+skid, gameAfterCar (Гомер, пропы, монеты)
static double smkPhSum[8], smkBuildSum, smkGeWaitSum, smkVblSum; static unsigned smkPhN; static float smkGeWaitMs, smkVblMs;
static float smkCpuNow, smkLogicNow; static unsigned smkMarkT0, smkMarkT[8];   // отметки логики кадра (smkMark) — разбивка SLOWFRAME
// Разбивка слепых мест кадра (аудит FPS 16.10, наряд Дирижёра): ph5 (трафик, урон, камера, звук) и хвост x_fx (частицы, полупрозрачное, подмес, маркеры, ближний проход, магазин, будка). Только при smkOn
// (в релизе smkOn=0, цена нулевая: проверка флага). Каждый таймер — один sceKernelGetSystemTimeLow; цена вызова мерится в строке (tcall_us), вычитается вердиктом.
// Строки PH5SUB / FXSUB раз в секунду: мс на кадр по подфазам.
static unsigned smk5T0, smkFT0, smkSubRepN; static double smk5S[6], smkFS[9]; static float smkSubRepT;
#define SMK5(i) do { if (smkOn) { unsigned t_ = sceKernelGetSystemTimeLow(); smk5S[i] += (double)(t_ - smk5T0); smk5T0 = t_; } } while (0)
#define SMKF(i) do { if (smkOn) { unsigned t_ = sceKernelGetSystemTimeLow(); smkFS[i] += (double)(t_ - smkFT0); smkFT0 = t_; } } while (0)
#define SMK5_START() do { if (smkOn) smk5T0 = sceKernelGetSystemTimeLow(); } while (0)
// №1001б: разбивка блока ph5 «трафик и события» по подфазам (строка TRFSUB раз в секунду, мс на кадр): tu — trafficUpdate, ev — события трафика, hv — gameHnrVehicle,
// hp — hnrPursuit, dk — deathCheck, bs — bustedCheck и остаток
static double trfS[6]; static unsigned trfT0, trfN, trfRan; static float trfAcc;
#define TRFSUB_T(i) do { if (smkOn) { unsigned t_ = sceKernelGetSystemTimeLow(); if ((i) == 0) trfRan = 1; if ((i) > 0 && trfRan) trfS[(i) - 1] += (double)(t_ - trfT0); trfT0 = t_; } } while (0)
#define TRFSUB_REPORT(dt_) do { if (smkOn) { trfN += trfRan; trfRan = 0; if ((trfAcc += (dt_)) >= 1.f && trfN) { char s_[200]; double k_ = 1e-3 / trfN;\
  snprintf(s_, sizeof s_, "TRFSUB t=%.1f scene=%s n=%u tu=%.2f ev=%.2f hv=%.2f hp=%.2f dk=%.2f bs=%.2f | tu: zon=%.2f spc=%.2f spp=%.2f obs=%.2f drv=%.2f col=%.2f prk=%.2f ped=%.2f (мс на кадр)\n", smkT, smkScene, trfN, trfS[0] * k_, trfS[1] * k_, trfS[2] * k_, trfS[3] * k_, trfS[4] * k_, trfS[5] * k_,\
    trfProfUs[0] * k_, trfProfUs[1] * k_, trfProfUs[2] * k_, trfProfUs[3] * k_, trfProfUs[4] * k_, trfProfUs[5] * k_, trfProfUs[6] * k_, trfProfUs[7] * k_); smkWrite(s_);\
  for (int q_ = 0; q_ < 6; q_++) trfS[q_] = 0; for (int q_ = 0; q_ < 8; q_++) trfProfUs[q_] = 0; trfN = 0; trfAcc = 0.f; } } } while (0)
#define SMKF_START() do { if (smkOn) smkFT0 = sceKernelGetSystemTimeLow(); } while (0)
static void smkMark(int i) { smkMarkT[i & 7] = sceKernelGetSystemTimeLow(); }   // процессорная часть кадра (до sceGuFinish) и её логика (до sceGuStart) — строка SLOWFRAME
static void smkSubReport(float dt) {   // раз в секунду, из конца хвоста x_fx (раз за кадр)
  if (!smkOn) return;
  smkSubRepN++; smkSubRepT += dt;
  if (smkSubRepT < 1.f || !smkSubRepN) return;
  unsigned a = sceKernelGetSystemTimeLow(), b = a; for (int i = 0; i < 100; i++) b = sceKernelGetSystemTimeLow();
  double k = 1.0 / 1000.0 / smkSubRepN, tc = (double)(b - a) / 100.0; char s[420];
  snprintf(s, sizeof s, "PH5SUB t=%.1f scene=%s n=%u trf=%.2f dmg=%.2f cam=%.2f snd=%.2f tail=%.2f tcall_us=%.2f (мс на кадр; 5 вызовов таймера на кадр)\n", smkT, smkScene, smkSubRepN, smk5S[0] * k, smk5S[1] * k, smk5S[2] * k, smk5S[3] * k, smk5S[4] * k, tc);
  smkWrite(s);
  snprintf(s, sizeof s, "FXSUB t=%.1f scene=%s n=%u fxupd=%.2f tq=%.2f fxend=%.2f jit=%.2f marks=%.2f near=%.2f other=%.2f tcall_us=%.2f (мс на кадр; 8 вызовов таймера на кадр)\n", smkT, smkScene, smkSubRepN, smkFS[0] * k, smkFS[1] * k, smkFS[2] * k, smkFS[3] * k, smkFS[4] * k, smkFS[5] * k, smkFS[6] * k, tc);
  smkWrite(s);
  memset(smk5S, 0, sizeof smk5S); memset(smkFS, 0, sizeof smkFS); smkSubRepN = 0; smkSubRepT = 0.f;
}
static int e1002N; static double e1002Bb[BBP_N];   // №1002: сколько кадров после выхода из комнаты / старта уровня писать строкой EXIT1002 (даже пока сцена «устаивается»)
static void smokeFrame(float dt, float work, int missed, int per, int missVis) {
  smkPer = per;
  float io = smkIoUs / 1000.f; smkIoUs = 0;   // журнал этого кадра (см. smkWrite) — не работа игры
  if (!SMK_BUF_OLD && smkBufN && smkFile[0] && (sceKernelGetSystemTimeLow() - smkFlushT >= 250000u || smkBufN > 3072)) smkFlush();   // не чаще раза в 0,25 с одной записью (её время попадёт в io следующего кадра, work его вычитает)
  if (io > 0.f) { work -= io; if (work < 0.f) work = 0.f; if (missed && work < per * 16.4f) missed = 0; }
  if (e1002N > 0 && smkOn) { char sl_[256]; int n_ = snprintf(sl_, sizeof sl_, "EXIT1002 left=%d t=%.2f work=%.2f cpu=%.2f logic=%.2f scene=%s marks", e1002N, smkT, work, smkCpuNow, smkLogicNow, smkScene);
    unsigned pv_ = smkMarkT0; for (int i = 0; i < 8; i++) { unsigned m = smkMarkT[i] ? smkMarkT[i] : pv_; n_ += snprintf(sl_ + n_, sizeof sl_ - n_, " %d:%.1f", i, (m - pv_) / 1000.f); pv_ = m; }
    n_ += snprintf(sl_ + n_, sizeof sl_ - n_, " | city=%.1f car3d=%.1f npc=%.1f trf=%.1f mcr=%.1f props=%.1f fx=%.1f ui=%.1f", smkBbSum[BBP_CITY] - e1002Bb[BBP_CITY], smkBbSum[BBP_CAR3D] - e1002Bb[BBP_CAR3D], smkBbSum[BBP_NPC3D] - e1002Bb[BBP_NPC3D],
      smkBbSum[BBP_TRF3D] - e1002Bb[BBP_TRF3D], smkBbSum[BBP_MCR3D] - e1002Bb[BBP_MCR3D], smkBbSum[BBP_PROPS3D] - e1002Bb[BBP_PROPS3D], smkBbSum[BBP_FX3D] - e1002Bb[BBP_FX3D], smkBbSum[BBP_UI] - e1002Bb[BBP_UI]);
    snprintf(sl_ + n_, sizeof sl_ - n_, "\n"); smkWrite(sl_); e1002N--; }
  for (int i_ = 0; i_ < BBP_N; i_++) e1002Bb[i_] = smkBbSum[i_];
  if (!TEST14_TEL_OLD) {   // покадровая запись: все кадры (в том числе до settle), состояние и «пишем на карту»
    int st = work > 1000.f ? 1 : smkState; smkRowSt = st; smkRowRec += smkWrNow ? 1 : 0;
    if (!smkWrNow) {
      int bn = work <= 16.7f ? 0 : work <= 33.3f ? 1 : work <= 50.f ? 2 : work <= 100.f ? 3 : 4;
      smkHB[bn]++; smkHN++; if (st == 0) { smkHGB[bn]++; smkHGN++; }
      int slot = -1; if (smkNWorst < 20) slot = smkNWorst++; else { int mi = 0; for (int i = 1; i < 20; i++) if (smkWorst[i].ms < smkWorst[mi].ms) mi = i; if (work > smkWorst[mi].ms) slot = mi; }
      if (slot >= 0) { SmkWorst *w = &smkWorst[slot]; w->ms = work; w->t = smkT; w->st = st; unsigned prv = smkMarkT0;
        for (int i = 0; i < 8; i++) { unsigned m = smkMarkT[i] ? smkMarkT[i] : prv; w->ph[i] = (m - prv) / 1000.f; prv = m; }
        w->build = smkCpuNow - smkLogicNow; w->ge = smkGeWaitMs; }
    }
    smkWrNow = 0;
  }
  if (!smkSettled) {
    smkSettleT += dt; if (smkSettleT > smkSettleMax) smkSettleMax = smkSettleT;
    smkCalm = missVis ? 0 : smkCalm + 1;
    if ((smkSettleT >= .5f && smkCalm >= 15) || smkSettleT > 10.f) { smkSettled = 1; if (smkSettleT > 10.f) smkSettleTimeout = 1; }
    // №936 п.2 (разбор TEST58, F_SET +19,6 с и F_FLAT_L1S +4,7 с: x_props 114 и 44 мс при work 26–31): пока сцена «устаканивается» (после паузы, телепорта, загрузки), фазы BBP (smkBbp, PHDRAW), подфазы PHSUB и таймеры
    // накапливались, а счётчик кадров smkPhN — нет (он ниже, после return): первая строка PHSEC/PHSUB после этого делила сумму ВСЕХ кадров на число ТОЛЬКО устоявшихся (×15–25 по каждому полю разом).
    // Теперь кадры устаивания тоже считаются: делитель верный. SMK_SETTLE936_OLD=1 / «flag smksettle936 0» — прежнее
    if (smkSettle936 && !PHSEC_OLD) {
      unsigned prv = smkMarkT0;
      for (int i = 0; i < 8; i++) { unsigned m = smkMarkT[i] ? smkMarkT[i] : prv; smkPhSum[i] += (m - prv) / 1000.0; prv = m; }
      if (smkXT[0] && smkXT[1] && smkXT[2] && smkMarkT[0]) { smkXSum[0] += (smkXT[0] - smkMarkT[0]) / 1000.0; smkXSum[1] += (smkXT[1] - smkXT[0]) / 1000.0; smkXSum[2] += (smkXT[2] - smkXT[1]) / 1000.0; }
      smkXT[0] = smkXT[1] = smkXT[2] = 0;
      smkBuildSum += smkCpuNow - smkLogicNow; smkGeWaitSum += smkGeWaitMs; smkVblSum += smkVblMs; smkPhN++;
    }
    return;
  }
  int b = (int)(work * 4.f); if (b >= SMK_BINS) b = SMK_BINS - 1; if (b < 0) b = 0;
  if (smkHist[b] < 65535) smkHist[b]++; smkFrames++; if (work > smkWorkMax) smkWorkMax = work;
  if (missed) { smkMissed++; smkSecMissed++;   // что было в пропущенном кадре — рядом в smoke.txt (VOICE/MSNNPC/UISFX с тем же t)
    char sl[256]; int n = snprintf(sl, sizeof sl, "SLOWFRAME t=%.2f work=%.2f io=%.1f cpu=%.2f logic=%.2f per=%d scene=%s marks", smkT, work, io, smkCpuNow, smkLogicNow, per, smkScene);
    unsigned prev = smkMarkT0;   // от начала кадра: ввод/сценарий, машина, Гомер+пропы, комнаты, миссии+собираемое, трафик.., до отсечения, до GE
    for (int i = 0; i < 8; i++) { unsigned m = smkMarkT[i] ? smkMarkT[i] : prev; n += snprintf(sl + n, sizeof sl - n, " %d:%.1f", i, (m - prev) / 1000.f); prev = m; }
    snprintf(sl + n, sizeof sl - n, "\n"); smkWrite(sl); }
  if (!PHSEC_OLD) {
    unsigned prv = smkMarkT0;
    for (int i = 0; i < 8; i++) { unsigned m = smkMarkT[i] ? smkMarkT[i] : prv; smkPhSum[i] += (m - prv) / 1000.0; prv = m; }
    if (smkXT[0] && smkXT[1] && smkXT[2] && smkMarkT[0]) { smkXSum[0] += (smkXT[0] - smkMarkT[0]) / 1000.0; smkXSum[1] += (smkXT[1] - smkXT[0]) / 1000.0; smkXSum[2] += (smkXT[2] - smkXT[1]) / 1000.0; }
    smkXT[0] = smkXT[1] = smkXT[2] = 0;
    smkBuildSum += smkCpuNow - smkLogicNow; smkGeWaitSum += smkGeWaitMs; smkVblSum += smkVblMs; smkPhN++;
  }
  memset(smkMarkT, 0, sizeof smkMarkT);
  if (missVis) { smkMissVisFr++; if ((unsigned)missVis > smkMissVisMax) smkMissVisMax = missVis; if (missVis > smkSecMissVis) smkSecMissVis = missVis; }
}
// раз в секунду, там где считается FPS
static void smokeSecond(float fps, float work, float workMax, float cpu, long tris, int draws, unsigned ram, int longView) {
  float hf = smokeHeapFree();
  if (smkOn && smkSettled) { float b = memMaxBlockMB(); if (b >= 0.f && b < smkBlkMin) smkBlkMin = b; }
  if (smkScr) scePowerTick(PSP_POWER_TICK_ALL);   // сценарий жмёт кнопки не пальцем: без этого PSP на стенде PSPLink засыпает по «Автоспящему режиму»
  if (!smkOn) return;
  char s[230]; snprintf(s, sizeof s, "%.1f %s %.1f %.2f %.2f %.2f %ld %d %.2f %s %.0f %.0f %u %u %.2f%s\n", smkT, smkScene, fps, work, workMax, cpu,
                        tris, draws, ram / 1048576.f, longView ? "long" : "view60", smkSecMissed, smkSecMissVis,
                        smkReadErr, smkAllocFail, hf, smkSettled ? "" : " settling");
  if (!TEST14_TEL_OLD) { size_t n_ = strlen(s); if (n_ > 0 && s[n_ - 1] == '\n') s[--n_] = 0; snprintf(s + n_, sizeof s - n_, " st=%s rec=%u\n", SMK_ST[smkRowSt < 0 ? smkState : smkRowSt], smkRowRec ? 1u : 0u); smkRowSt = -1; smkRowRec = 0; }
  smkSecMissed = smkSecMissVis = 0.f;
  smkWrite(s);
#if LEAK696_TRACE && defined(LT_ABS)
  { static int ltN_; if (++ltN_ % 6 == 0) { extern void lt696Dump(void (*)(const char *), const char *); char tg_[40]; snprintf(tg_, sizeof tg_, "%.1f", smkT); lt696Dump(smkWrite, tg_); } }   // №962 (служебно): раз в 6 строк HEAPB все живые группы выделений по вызывающему
#endif
  { char hb_[200]; struct mallinfo mi_ = mallinfo(); snprintf(hb_, sizeof hb_, "HEAPB t=%.1f scene=%s bytes=%u blk=%u used=%u arena=%u free=%u\n", smkT, smkScene, heapFreeBytes(), memMaxBlock ? memMaxBlock() : 0u, (unsigned)mi_.uordblks, (unsigned)mi_.arena, (unsigned)mi_.fordblks); smkWrite(hb_); }   // №930 служебно (приёмка кучи)
  if (!PHSEC_OLD && smkPhN) {
    char p[1800]; double k = 1.0 / smkPhN;
    snprintf(p, sizeof p, "PHSEC t=%.1f scene=%s n=%u ph0=%.2f ph1=%.2f ph2=%.2f ph3=%.2f ph4=%.2f ph5=%.2f ph6=%.2f ph7=%.2f build=%.2f gesync=%.2f vbl=%.2f x_car=%.2f x_vis=%.2f x_homer=%.2f x_city=%.2f x_car3d=%.2f x_npc=%.2f x_trf=%.2f x_mcr=%.2f x_props=%.2f x_fx=%.2f x_ui=%.2f\n", smkT, smkScene, smkPhN,
             smkPhSum[0] * k, smkPhSum[1] * k, smkPhSum[2] * k, smkPhSum[3] * k, smkPhSum[4] * k, smkPhSum[5] * k, smkPhSum[6] * k, smkPhSum[7] * k, smkBuildSum * k, smkGeWaitSum * k, smkVblSum * k, smkXSum[0] * k, smkXSum[1] * k, smkXSum[2] * k,
             smkBbSum[BBP_CITY] * k, smkBbSum[BBP_CAR3D] * k, smkBbSum[BBP_NPC3D] * k, smkBbSum[BBP_TRF3D] * k, smkBbSum[BBP_MCR3D] * k, smkBbSum[BBP_PROPS3D] * k, smkBbSum[BBP_FX3D] * k, smkBbSum[BBP_UI] * k);
    smkWrite(p);
  }
}
static void smokeSecondB(void) {   // №913: PHDRAW отдельным кадром (SMK_SPREAD_OLD — подряд с остальным)
  if (!smkOn || PHSEC_OLD || !smkPhN) return;
  char p[1800]; double k = 1.0 / smkPhN;
    snprintf(p, sizeof p, "PHDRAW t=%.1f scene=%s city=%.0f/%.1f/%.0f car3d=%.0f/%.1f/%.0f npc=%.0f/%.1f/%.0f trf=%.0f/%.1f/%.0f mcr=%.0f/%.1f/%.0f props=%.0f/%.1f/%.0f fx=%.0f/%.1f/%.0f ui=%.0f/%.1f/%.0f (вызовы/ктрисы/привязки на кадр) kw: city=%.1f car3d=%.1f npc=%.1f trf=%.1f mcr=%.1f props=%.1f fx=%.1f ui=%.1f (тыс. слов списка GE на кадр)\n", smkT, smkScene,
             smkBbD[BBP_CITY] * k, smkBbK[BBP_CITY] * k, smkBbB[BBP_CITY] * k, smkBbD[BBP_CAR3D] * k, smkBbK[BBP_CAR3D] * k, smkBbB[BBP_CAR3D] * k,
             smkBbD[BBP_NPC3D] * k, smkBbK[BBP_NPC3D] * k, smkBbB[BBP_NPC3D] * k, smkBbD[BBP_TRF3D] * k, smkBbK[BBP_TRF3D] * k, smkBbB[BBP_TRF3D] * k,
             smkBbD[BBP_MCR3D] * k, smkBbK[BBP_MCR3D] * k, smkBbB[BBP_MCR3D] * k, smkBbD[BBP_PROPS3D] * k, smkBbK[BBP_PROPS3D] * k, smkBbB[BBP_PROPS3D] * k,
             smkBbD[BBP_FX3D] * k, smkBbK[BBP_FX3D] * k, smkBbB[BBP_FX3D] * k, smkBbD[BBP_UI] * k, smkBbK[BBP_UI] * k, smkBbB[BBP_UI] * k,
             smkBbW[BBP_CITY] * k / 1000.0, smkBbW[BBP_CAR3D] * k / 1000.0, smkBbW[BBP_NPC3D] * k / 1000.0, smkBbW[BBP_TRF3D] * k / 1000.0, smkBbW[BBP_MCR3D] * k / 1000.0, smkBbW[BBP_PROPS3D] * k / 1000.0, smkBbW[BBP_FX3D] * k / 1000.0, smkBbW[BBP_UI] * k / 1000.0);
    smkWrite(p);
}
static void smokeSecondC(void) {   // №913: PHSUB и обнуление сумм секунды
  if (!smkOn || PHSEC_OLD || !smkPhN) return;
  char p[1800]; double k = 1.0 / smkPhN;
    snprintf(p, sizeof p, "PHSUB t=%.1f scene=%s props=%.2f sprops=%.2f bill=%.2f coll=%.2f coins=%.2f gg_ms=%.2f gg_pieces=%.1f gg_ktris=%.2f (мс на кадр; gg — срез по полосе GE на процессоре внутри x_city, правка 340) sp_loop=%.2f sp_pose=%.2f sp_flush=%.2f sp_inst=%.0f sp_vis=%.0f sp_pieces=%.0f sp_cand=%.1f sp_drawn=%.1f bb_cand=%.1f bb_quads=%.1f bb_calls=%.1f bb_loop=%.3f bb_draw=%.3f bb_tail=%.3f bb_t=%.3f/%.3f/%.3f/%.3f/%.3f/%.3f sp_t=%.3f/%.3f/%.3f lsp_list=%.1f/%.1f lsp_viol=%.2f trf_cars=%.3f trf_peds=%.3f trf_n=%.1f/%.1f/%.1f ped_pose=%.3f/%.1f ped_draw=%.3f ped_words=%.0f ped_w=%.0f/%.0f/%.0f/%.0f/%.0f pb_t=toon%.3f/frame%.3f/call%.3f words%.0f calls%.1f batches=%.1f/%.1f car_pc=%.1f words(toon/piece)=%.0f/%.0f ms(toon/piece)=%.3f/%.3f sp_t2=%.3f/%.3f/%.1f ct_mask=%.2f ct_risky=%.2f ct_list=%.2f ct_build=%.2f gg_replay=%u near_frames=%u near_low=%u near_car_only=%u near_why=%u/%u/%u/%u/%u/%u near_pass=%u coin_active=%.1f coin_drawn=%.1f coin_pieces=%.1f (монеты: активных / реально нарисованных после дальности 60 м и боковых плоскостей / кусков в GE, на кадр)\n", smkT, smkScene, smkSubSum[0] * k, smkSubSum[1] * k, smkSubSum[2] * k, smkSubSum[3] * k, smkSubSum[4] * k, smkGgMs * k, smkGgPieces * k, smkGgTris * k / 1000.0, smkSpLoopMs * k, smkSpPoseMs * k, smkSpFlushMs * k, smkSpInst * k, smkSpVis * k, smkSpPcs * k, smkSpCand * k, smkSpDrawn * k, smkBbCand * k, smkBbQuads * k, smkBbCalls * k, smkBbLoopMs * k, smkBbDrawMs * k, smkBbTailMs * k, smkBbTm[0] * k, smkBbTm[1] * k, smkBbTm[2] * k, smkBbTm[3] * k, smkBbTm[4] * k, smkBbTm[5] * k, smkSpT[0] * k, smkSpT[1] * k, smkSpT[2] * k, smkLspList * k, smkLspAll * k, smkLspViol * k, smkTrfCarMs * k, smkTrfPedMs * k, smkTrfN[0] * k, smkTrfN[1] * k, smkTrfN[2] * k, smkTrfPose * k, smkTrfPoseN * k, smkTrfDraw * k, smkTrfWords * k, smkSkinW[0] * k, smkSkinW[1] * k, smkSkinW[2] * k, smkSkinW[3] * k, smkSkinW[4] * k, smkPbT[0] * k, smkPbT[1] * k, smkPbT[2] * k, smkPbT[3] * k, smkPbT[4] * k, smkSkinW[5] * k, smkSkinW[6] * k, smkTrfPc[0] * k, smkTrfPc[1] * k, smkTrfPc[2] * k, smkTrfPc[3] * k, smkTrfPc[4] * k, smkSpT2[0] * k, smkSpT2[1] * k, smkSpT2[2] * k, smkCtMask * k, smkCtRisky * k, smkCtList * k, smkCtBuild * k, smkGgReplay, smkNearFrames, smkNearLow, smkNearCarOnly, smkNearWhy[1], smkNearWhy[2], smkNearWhy[3], smkNearWhy[4], smkNearWhy[5], smkNearWhy[6], smkNearPass, smkCoinAct * k, smkCoinDrawn * k, smkCoinPcs * k);
    { extern unsigned spNearStat[2]; char q_[128]; snprintf(q_, sizeof q_, "SPNEAR t=%.1f scene=%s on=%d asleep=%u inst=%u (№1000: спящих стейтпропов в кадре)\n", smkT, smkScene, spNear1000, spNearStat[0], spNearStat[1]); smkWrite(q_); }
    smkWrite(p); memset(smkSubSum, 0, sizeof smkSubSum); smkGgMs = smkGgPieces = smkGgTris = smkSpPoseMs = smkSpFlushMs = smkSpLoopMs = smkSpInst = smkSpCand = smkSpDrawn = smkBbCand = smkBbQuads = smkBbCalls = smkBbLoopMs = smkBbDrawMs = smkBbTailMs = smkBbTm[0] = smkBbTm[1] = smkBbTm[2] = smkBbTm[3] = smkBbTm[4] = smkBbTm[5] = smkSpT[0] = smkSpT[1] = smkSpT[2] = smkSpT2[0] = smkSpT2[1] = smkSpT2[2] = smkLspList = smkLspAll = smkLspViol = smkTrfCarMs = smkTrfPedMs = smkTrfN[0] = smkTrfN[1] = smkTrfN[2] = smkTrfPose = smkTrfDraw = smkTrfPoseN = smkTrfWords = smkSkinW[0] = smkSkinW[1] = smkSkinW[2] = smkSkinW[3] = smkSkinW[4] = smkSkinW[5] = smkSkinW[6] = smkTrfPc[0] = smkTrfPc[1] = smkTrfPc[2] = smkTrfPc[3] = smkTrfPc[4] = smkPbT[0] = smkPbT[1] = smkPbT[2] = smkPbT[3] = smkPbT[4] = smkSpVis = smkSpPcs = smkCoinAct = smkCoinDrawn = smkCoinPcs = smkCtMask = smkCtRisky = smkCtList = smkCtBuild = 0; smkGgReplay = smkNearFrames = smkNearLow = smkNearCarOnly = smkNearPass = 0; memset(smkNearWhy, 0, sizeof smkNearWhy);
    memset(smkBbW, 0, sizeof smkBbW); memset(smkBbD, 0, sizeof smkBbD); memset(smkBbK, 0, sizeof smkBbK); memset(smkBbB, 0, sizeof smkBbB);
    memset(smkBbSum, 0, sizeof smkBbSum); memset(smkXSum, 0, sizeof smkXSum); memset(smkPhSum, 0, sizeof smkPhSum); smkBuildSum = smkGeWaitSum = smkVblSum = 0; smkPhN = 0;
}
#endif
