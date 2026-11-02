// Сохранения на PSP: системная утилита сохранений + экраны оригинала (см. savemenu.h, savegame.h, tools/save_export.py).
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psputility.h>
#include <psprtc.h>
#include <pspge.h>
#include <pspgu.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "savemenu.h"
#include "ui.h"
#include "savefile.h"   // правка 274: страховка файлом save<NN>.bin рядом с игрой
#include "blackbox.h"   // правка 274: bbBeatTick между вызовами утилиты (цепочка, замер-тур) — сторож не пишет ложный STALL

// экраны пакета (tools/save_export.py SCREENS) и строки экрана strings (STRINGS): порядок сверяет tests/savegame/host.sh
enum { SCR_SLOTS = 0, SCR_PROMPT = 1, SCR_NOTICE = 2, SCR_MESSAGE = 3, SCR_STRINGS = 4 };
enum { SVS_EMPTY = 0, SVS_DAMAGED = 1, SVS_LEVEL1 = 2,
       SVS_MSG = 9,   // + SVM_MSG_*
};
enum { SVM_MSG_OVERWRITE, SVM_MSG_SAVE_CONFIRM, SVM_MSG_SAVING, SVM_MSG_SAVED, SVM_MSG_SAVE_FAILED, SVM_MSG_LOAD_CONFIRM,
       SVM_MSG_LOADING, SVM_MSG_LOADED, SVM_MSG_CORRUPT, SVM_MSG_SAVE_BEFORE_QUIT, SVM_MSG_DELETING, SVM_MSG_DELETE_FAILED,
       SVM_MSG_NO_DATA, SVM_MSG_COUNT };
// ключи библии сообщений по порядку SVM_MSG_* (= tools/save_export.py MSGS; сверяет tests/savegame/host.sh), для строки SVMSG
static const char *const msgKey[SVM_MSG_COUNT] = {
  "MSG_SAVE_CONFIRM_OVERWRITE_(PS2)", "MSG_SAVE_CONFIRM_(PS2)", "MSG_SAVING_GAME_(PS2)", "MSG_SAVE_SUCCESSFUL",
  "ERROR_HW_FAILURE_SAVING_(PS2)", "MSG_LOAD_CONFIRM_(PS2)", "MSG_LOADING_GAME_(PS2)", "MSG_LOAD_SUCCESSFUL",
  "ERROR_DATA_CORRUPT_LOADING_(PS2)", "MSG_CONFIRM_SAVE_BEFORE_QUIT", "MSG_DELETING_(PS2)", "MSG_DELETE_CORRUPT_FAIL_(PS2)",
  "MSG_LOAD_CARD_EMPTY_(PS2)"};
#define ROW_MSG 11                                // текст сообщения — поле ROW11 (save_export.py ROW_MSG)

// коды утилиты (результат base.result)
#define ERR_LOAD_NO_MS    0x80110301u
#define ERR_LOAD_EJECT_MS 0x80110302u
#define ERR_LOAD_NO_DATA  0x80110307u
#define ERR_DELETE_NO_DATA 0x80110347u
#define ERR_LOAD_PARAM    0x80110308u   // «неверный параметр» чтения (0x801103x8 — то же для записи 388, удаления 348)

// ---- правка 274 (04.10): варианты параметров утилиты. Факт железа — pspautotests tests/utility/savedata/secureversion.expected
// (записан на настоящей PSP, запрос 1536 Б, SDK 1.50…6.60): AUTOSAVE с secureVersion 0 и нулевым ключом -> 0x80110388 (SAVE_PARAM)
// на всех версиях SDK; secureVersion 1 без ключа и secureVersion 0 с ключом пишутся и читаются тем же набором (Data matched).
// Наш запрос — ровно такой: сборка идёт с _PSP_FW_VERSION=600 (pspdev.cmake), структура 1536 Б с полем key, ключ нулевой —
// отказ из чёрного ящика Роба (EBOOT 1e015988, 04.10 03:5x: SVIO op=save res=80110388 ms=150). PPSSPP-близнец ключ при записи
// не требует (SavedataParam.cpp Save), поэтому в эмуляторе запись шла. Правило порта: первый путь прежний (std); отказ параметров
// -> следующий вариант по убыванию вероятности; удачный запоминается на сессию и идёт первым для записи, чтения и удаления.
// «saveold 1» — только std (прежнее поведение, красный контроль гейта tests/savegame/fallback/run.py).
_Static_assert(sizeof(SceUtilitySavedataParam) == 1536, "запрос утилиты 1536 Б (V3, с ключом) — от этого зависят варианты");
enum { SVV_STD, SVV_SV1, SVV_KEY, SVV_S1500, SVV_S1480, SVV_NOICON, SVV_N };
static const char *const svvName[SVV_N] = {"std", "sv1", "key", "s1500", "s1480", "noicon"};
// std    — прежний: 1536 Б, secureVersion 0, ключ нули (на железе отвергается — см. выше)
// sv1    — secureVersion 1 без ключа (железо: пишется и читается, SFO mode 01) — самый вероятный
// key    — secureVersion 0 + постоянный ключ игры (железо: пишется и читается тем же ключом). Ключ НЕ МЕНЯТЬ: записи читаются только им
// s1500  — запрос 1500 Б (без полей key/secureVersion, как у игр SDK 2.x; PPSSPP PR #22339: «старые размеры пишутся без ключа»)
// s1480  — запрос 1480 Б (как у игр SDK 1.x)
// noicon — sv1 без ICON0 (на случай, если железо не принимает иконку из EBOOT)
static const char svvKey[17] = "SHAR-PSP-SLES518";
static int svBlock;   // автотур (flag nosave): ни утилита (запись, удаление), ни файл save<NN>.bin не пишутся
static int svFileOff, svUtilOff;   // служебное 274: «savefileoff» — без страховки файлом (красный); «saveutiloff» — утилита отвергает всё
static int svvGood = -1, svvOld, svvLast;   // удачный вариант сессии; служебное «saveold»; вариант последнего вызова (строки SVIO)
static unsigned svvFail;                     // служебное «savefail»: железо отвергает std с этим кодом (имитация в эмуляторе)

// ---- сверка с исходником, окно g09b-menu (02.11.2026, правки 1077–1081): логика вопросов и списка слотов. Каждая правка — свой выключатель
// (-D<ИМЯ>_OLD=1 — прежнее поведение). Вид экранов наш; из оригинала — порядок вопросов, кому что можно нажать, подпись слота, минимумы показа
#ifndef SVM_QUITASK_OLD
#define SVM_QUITASK_OLD 0   // g09b-menu: М2 Д2 / P13-1: QUIT GAME в паузе сначала спрашивает «Are you sure you want to quit?» (Yes/No, No — в паузу); было: сразу «сохранить?» (свободная езда) или выход
#endif
#ifndef SVM_NOBACK_OLD
#define SVM_NOBACK_OLD 0    // g09b-menu: P14-7: на запросах Yes/No, на «сохранить?» и на сообщениях △ и ○ не действуют, выйти можно только ответом; было: назад = No / закрыть
#endif
#ifndef SVM_LOADASK_OLD
#define SVM_LOADASK_OLD 0   // g09b-menu: M3 Д7: выбор слота в LOAD GAME всегда спрашивает подтверждение; было: только когда игра уже шла
#endif
#ifndef SVM_LATESTSEL_OLD
#define SVM_LATESTSEL_OLD 0 // g09b-menu: P14-4: список LOAD GAME открывается с курсором на слоте с самой свежей записью; было: первый слот
#endif
#ifndef SVM_LOADDAMAGED_OLD
#define SVM_LOADDAMAGED_OLD 0 // g09b-menu: P14-5: в LOAD GAME повреждённый слот серый и не выбирается, удаления из загрузки нет (PS2); было: выбирался, «corrupt… OK?» и удаление
#endif
#ifndef SVM_SLOTLABEL_OLD
#define SVM_SLOTLABEL_OLD 0 // g09b-menu: P14-6 / P16-24: подпись слота «(L<ур> M<мис>) дд/мм/гг чч:мм:сс» (PAL: день/месяц); было «LEVEL N дд/мм чч:мм»
#endif
#ifndef SVM_SLOTBACK_OLD
#define SVM_SLOTBACK_OLD 0  // g09b-menu: P13-Д6: ○ на списке слотов потока «выйти / конец уровня» возвращает на вопрос «сохранить?»; было: закрывало окно
#endif
#ifndef SVM_START_OLD
#define SVM_START_OLD 0     // g09b-menu: P13-Д5: START на слотах и вопросах записи закрывает паузу целиком (поток выхода — отмена потока); было: START ничего не делал
#endif
#ifndef SVM_SAVEDFLAG_OLD
#define SVM_SAVEDFLAG_OLD 0 // g09b-menu: P13-Д4: после удачной записи в этой паузе выход не спрашивает «сохранить?» второй раз; было: спрашивал снова
#endif
#ifndef SVM_BUSYMIN_OLD
#define SVM_BUSYMIN_OLD 0   // g09b-menu: P14-18: сообщение «Saving / Loading» в тихом пути живёт не меньше 1,0 с (PS2); было: два кадра
#endif
#ifndef SVM_TUTLABEL_OLD
#define SVM_TUTLABEL_OLD 0  // g09b-menu круг 3 (проверка): подпись слота обучения L1 «(TUT)» вместо «(L1 TUT)»; 1 — прежнее
#endif
#ifndef SVM_START3_OLD
#define SVM_START3_OLD 0    // g09b-menu круг 3 (проверка P13 Д5): START на записи и вопросах потока «выйти» из паузы возвращает в игру (пауза снята), в потоке конца уровня / игры START не действует; 1 — прежнее (возврат в меню паузы)
#endif
#ifndef SVM_DEFNO_OLD
#define SVM_DEFNO_OLD 0     // g09b-menu круг 2 (P14 Д1): курсор в запросах да/нет (перезапись слота, загрузка записи, Quit?, NEW GAME) стоит на НЕТ; на ДА — только «сохранить перед выходом»; было: на ДА
#endif
#define SVM_BUSY_MIN_S 1.0f
#define SVM_EXT_SEC 83      // g09b-menu: P14-6: секунды записи — ext[83] (бит 7 — поле записано); ext[0..44] заняты (savegame.h), хвост запаса свободен
static int pauseSaved;      // P13-Д4: в этой паузе уже была удачная запись (сбрасывает svmPauseEnter)
void svmPauseEnter(void) { pauseSaved = 0; }
int svmQuitAsks(void) { return !SVM_QUITASK_OLD; }
int svmQuitAsk(int freeRoam) { return SVM_QUITASK_OLD ? 1 : (freeRoam ? 3 : 2); }

static int on; static char dir[256]; static void (*logFn)(const char *);
static unsigned char *icon0; static unsigned icon0Size;          // ICON0.PNG из EBOOT.PBP
static int slotSt[SV_SLOTS] = {SV_EMPTY, SV_EMPTY, SV_EMPTY, SV_EMPTY}; static SaveGame slotSg[SV_SLOTS];
static int noWrite;
// правка 335: сохранение разработчика (savemenu.h). Уровень загрузки выбирается в списке LOAD GAME ◄ ► (на сессию)
static int devLevel = 1; static int (*devAvail)(int);
int svmIsDev(const SaveGame *g) { return g && g->seq == 0 && g->year != 0; }
static int devOk(int n) { return n == 1 || (devAvail && n >= 2 && n <= 7 && devAvail(n)); }
static int devLevels(void) { int n = 0; for (int k = 1; k <= 7; k++) n += devOk(k); return n; }
static void devTake(SaveGame *g) {   // загрузка DEV: уровень выбранный; миссии уровня считает игра (main.c -> GAME_MSN_STORY_DEV)
  if (!devOk(devLevel)) devLevel = 1;
  g->level = g->msnLevel = (uint8_t)devLevel; g->msnStory = 0xff; g->msnPhase = g->msnStage = 0; g->msnCompleted = 0;
}

// ---- правка 309 (Роб 04.10, голосом): «в save меню… Обычно на PSP оно тебя переводит в контекстное меню PSP и ты там подтверждаешь
// создание этого файла, что дает тебе уверенность, что ты сохраняешься. В нашем случае оно просто как на PS2 такое чик, сохранилось
// и пошло». Запись выбранного слота — системным окном утилиты сохранения PSP (режим SAVE с интерфейсом): «Сохранить эти данные?» /
// «Перезаписать?» Да/Нет -> «Сохранение…» -> «Сохранение завершено», в окне — иконка игры и строка слота из PARAM.SFO. Загрузка —
// так же окном LOAD («Загрузить эти данные?»), если слот читается записью утилиты (решён файлом save<NN>.bin — тихо, как раньше).
// Слоты оригинала (Scrooby) остаются: окно подтверждает выбранный слот. SAVE, а не LISTSAVE: список у нас уже есть — оригинальный;
// LISTSAVE повторил бы его системным списком (два выбора подряд), а SAVE на один слот — ровно «подтверждаешь создание файла».
// Своего «перезаписать?» и «Save Successful» в этом режиме нет — их говорит окно PSP (два подтверждения подряд не нужны).
// Параметры — варианты, прошедшие на железе (TEST 8: sv1 первым); std в окне не пробуется (на железе 0x80110388). Окно не
// поднялось / отвергло все варианты / ошибка записи -> прежний тихий путь (AUTOSAVE цепочкой + файл) с сообщениями оригинала.
// Пока окно на экране: каждый кадр GetStatus/Update, игра не рисует поверх — фон = застывший кадр меню (копия в буфере глубины
// при 16-битном кадре, иначе в куче), sceGuSync, sceDisplayWaitVblankStart, смена буферов; смен чётное число — буфер рисования
// main.c тот же. После окна — ждём, пока отпущены кнопки (✕/○ окна не уходят в меню паузы). Правило порта (у PS2 окна нет).
// По умолчанию окно — в обычном запуске; в служебном (scenario.txt рядом с EBOOT) — тихо, прежние гейты идут как раньше.
// «flag svdialog» — окно и в служебном запуске; «flag svquiet» / файл «savequiet» рядом с EBOOT — прежний тихий путь (красный
// контроль tests/savegame/dialog/run.py); «flag svdlgfail» — окно отвергает все варианты (имитация: живёт ли тихий запасной путь)
static int svDlg, svDlgFail;
// Тур финала L7 для железа (окно «Меню», 23.10, по просьбе Надзора): «flag svpickempty» — запись идёт только в ПУСТОЙ слот (первый пустой), занятые не трогаются;
// пустого нет — запись отменяется (строка ENDSAVE skip), игра идёт дальше. SVPICK_OLD=1 — флаг не действует. Только служебный запуск (сценарий), обычная игра не затронута
#ifndef SVPICK_OLD
#define SVPICK_OLD 0
#endif
static int svPick, svPickDone;
static int slotVar[SV_SLOTS] = {-1, -1, -1, -1};   // вариант, которым прочитана запись утилиты слота (-1 — нет её / слот решён файлом)
static int fileThere(const char *name) {
  char fn[300]; snprintf(fn, sizeof fn, "%s%s", dir, name);
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  sceIoClose(f); return 1;
}

static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logf_(const char *fmt, ...) {
  if (!logFn) return;
  char s[256]; va_list a; va_start(a, fmt); vsnprintf(s, sizeof s, fmt, a); va_end(a); logFn(s);
}
void svmSetLog(void (*log)(const char *)) { logFn = log; }
void svmNoWrite(int v) { noWrite = v; logf_("SVNOWRITE on=%d\n", v); }
int svmOn(void) { return on; }

static void iconLoad(void) {   // иконка сохранения = иконка игры: ICON0 из EBOOT.PBP (заголовок PBP: 8 смещений — SFO, ICON0, ICON1, PIC0 ...)
  if (icon0) return;
  char fn[300]; snprintf(fn, sizeof fn, "%sEBOOT.PBP", dir);
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (f >= 0) {
    unsigned h[10];
    if (sceIoRead(f, h, sizeof h) == (int)sizeof h && h[0] == 0x50425000u && h[4] > h[3] && h[4] - h[3] < (256u << 10)) {
      icon0Size = h[4] - h[3]; icon0 = malloc(icon0Size);
      if (icon0 && (sceIoLseek32(f, h[3], PSP_SEEK_SET) != (int)h[3] || sceIoRead(f, icon0, icon0Size) != (int)icon0Size)) {
        free(icon0); icon0 = 0; icon0Size = 0; }
    }
    sceIoClose(f);
  }
}
void svmInit(const char *ebootPath, int enabled) {
  on = enabled;
  strncpy(dir, ebootPath, sizeof dir - 1); char *sl = strrchr(dir, '/'); if (sl) sl[1] = 0; else dir[0] = 0;
  if (on) iconLoad();
  svDlg = on && !fileThere("scenario.txt") && !fileThere("savequiet");   // 309: окно PSP — в обычном запуске
}
void svmDialog(int v) { svDlg = v; logf_("SVDLGMODE on=%d src=flag\n", v); }
void svmPickEmpty(int v) { svPick = !SVPICK_OLD && v; logf_("SVPICK on=%d\n", svPick); }
void svmDialogFail(int v) { svDlgFail = v; logf_("SVDLGFAIL on=%d\n", v); }

// ---- правка 309: окно утилиты на экране (режимы SAVE / LOAD). Кадр за кадром: фон (застывший кадр меню) в буфер рисования, список
// закончен (sceGuSync), GetStatus/Update — окно рисует себя поверх в тот же буфер (свой список GE, состояние GE возвращает сама),
// обратный ход, смена буферов. Как samples/utility pspsdk (msgdialog: drawStuff -> Update -> sceDisplayWaitVblankStart ->
// sceGuSwapBuffers). Раскладка VRAM — main.c: буферы кадра 0 и fb, глубина 2·fb (BUF_W 512); показан один из двух
static unsigned int __attribute__((aligned(64))) dlgList[512];
static struct { int frames, vis, swaps; const char *bg; } D;   // для строки SVDLG: кадров, из них окно видно, смен буферов, где фон
static int dlgRun(SceUtilitySavedataParam *p) {
  unsigned vram = (unsigned)sceGeEdramGetAddr(); void *top = 0; int bw = 512, pf = 0;
  sceDisplayGetFrameBuf(&top, &bw, &pf, PSP_DISPLAY_SETBUF_NEXTFRAME);   // показанный буфер (последняя смена main.c)
  int psm = pf == PSP_DISPLAY_PIXEL_FORMAT_8888 ? GU_PSM_8888 : GU_PSM_5650;
  unsigned fb = (unsigned)bw * 272u * (psm == GU_PSM_8888 ? 4u : 2u), front = (unsigned)top & 0x001FFFFFu, draw = front ? 0u : fb;
  void *bg = NULL, *heap = NULL; D.bg = "clear";
  if (front == 0 || front == fb) {
    if (psm != GU_PSM_8888) { bg = (void *)(vram + 2u * fb); D.bg = "depth"; }   // 16 бит: кадр ровно с буфер глубины; в окне глубина не нужна,
    else if ((heap = memalign(64, fb))) {                                          // main.c чистит её каждый кадр
      sceKernelDcacheWritebackInvalidateRange(heap, fb); bg = heap; D.bg = "heap"; }   // 32 бит (служебный fb32): копия в куче
  }
  if (bg) {   // застывший кадр (оба буфера одинаковы: окно зовётся после двух кадров фона — ST_SYSDLG) -> хранилище
    sceGuStart(GU_DIRECT, dlgList);
    sceGuCopyImage(psm, 0, 0, 480, 272, bw, (void *)(vram + front), 0, 0, bw, bg); sceGuTexSync();
    sceGuFinish(); sceGuSync(0, 0);
  }
  int seen = 0, shut = 0, quiet = 0, done = 0, after = 0, r = -1;
  unsigned last = sceKernelGetSystemTimeLow();
  const unsigned btn = PSP_CTRL_SELECT | PSP_CTRL_START | PSP_CTRL_UP | PSP_CTRL_RIGHT | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_LTRIGGER |
                       PSP_CTRL_RTRIGGER | PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE | PSP_CTRL_CROSS | PSP_CTRL_SQUARE;
  D.frames = D.vis = D.swaps = 0;
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t;
    unsigned spad = svmDialogFrame(dt, D.frames == 0);   // служебное: сценарий смоука (press/wait/shot), кнопки сценария — эмулятору; на PSP 0
    sceGuStart(GU_DIRECT, dlgList);
    if (bg) { sceGuCopyImage(psm, 0, 0, 480, 272, bw, bg, 0, 0, bw, (void *)(vram + draw)); sceGuTexSync(); }
    else { sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT); }
    sceGuFinish(); sceGuSync(0, 0);
    if (!done) {
      int s = sceUtilitySavedataGetStatus();
      if (s >= 1) seen = 1;
      if (s == 2) { sceUtilitySavedataUpdate(1); D.vis++; }
      else if (s == 3 && !shut) { sceUtilitySavedataShutdownStart(); shut = 1; }
      else if (s == 0 && (shut || seen)) { done = 1; r = p->base.result; }
      if (!done && s != 2 && ++quiet > 60 * 20) break;   // 20 с не в окне (запуск/закрытие встало) — ошибка, а не зависшая игра
    }
    if (done) {   // окно закрыто: ждём, пока отпущены кнопки (✕/○ окна не уходят в меню), не дольше 0,75 с; смен буферов — чётно
      SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
      if ((!((pad.Buttons | spad) & btn) || ++after > 45) && !(D.swaps & 1)) break;
    }
    D.frames++;
    sceDisplayWaitVblankStart(); draw = (unsigned)sceGuSwapBuffers(); D.swaps++; bbBeatTick();
  }
  if (D.swaps & 1) {   // вышли по сторожу на нечётной смене: ещё кадр фона — буфер рисования main.c прежний
    sceGuStart(GU_DIRECT, dlgList);
    if (bg) { sceGuCopyImage(psm, 0, 0, 480, 272, bw, bg, 0, 0, bw, (void *)(vram + draw)); sceGuTexSync(); }
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); sceGuSwapBuffers(); D.swaps++;
  }
  if (heap) free(heap);
  return r;
}

// ---- утилита: один вызов целиком одним вариантом параметров (кадр стоит; «тихие» режимы ничего не рисуют). -> base.result или <0
// test — строка слота в PARAM.SFO для замер-тура (тестовая запись), иначе LEVEL N + время. 309: SAVE / LOAD — окно на экране (dlgRun)
static int utilVar(int var, int mode, const char *saveName, void *buf, unsigned bufSize, unsigned dataSize, const SaveGame *meta,
                   const char *test) {
  int dlg = mode == PSP_UTILITY_SAVEDATA_SAVE || mode == PSP_UTILITY_SAVEDATA_LOAD;
  if (svBlock && mode != PSP_UTILITY_SAVEDATA_LOAD && mode != PSP_UTILITY_SAVEDATA_AUTOLOAD && mode != PSP_UTILITY_SAVEDATA_LISTLOAD) { logf_("SAVEBLOCK util mode=%d\n", mode); return (int)0x80110300u; }
  if (svvFail && var == SVV_STD && (mode == PSP_UTILITY_SAVEDATA_AUTOSAVE || mode == PSP_UTILITY_SAVEDATA_SAVE)) return (int)svvFail;   // «savefail»: как железо, ничего не пишется
  if (dlg && svDlgFail) return (int)0x80110388u;   // 309 «svdlgfail»: окно отвергает параметры, ничего не показано
  static SceUtilitySavedataParam p;              // 1536 Б, не на стеке
  memset(&p, 0, sizeof p);
  p.base.size = var == SVV_S1500 ? 1500 : var == SVV_S1480 ? 1480 : sizeof p;   // 1500 = до поля key, 1480 = до поля focus
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &p.base.language);
  sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &p.base.buttonSwap);
  p.base.graphicsThread = 0x11; p.base.accessThread = 0x13; p.base.fontThread = 0x12; p.base.soundThread = 0x10;
  p.mode = (PspUtilitySavedataMode)mode; p.overwrite = 1;
  strcpy(p.gameName, SV_GAMENAME); snprintf(p.saveName, sizeof p.saveName, "%s", saveName);
  strcpy(p.fileName, "DATA.BIN");
  // чтение запасными вариантами — как в тесте железа и примере pspsdk: dataSize = dataBufSize (std — прежний 0)
  if (var != SVV_STD && (mode == PSP_UTILITY_SAVEDATA_AUTOLOAD || mode == PSP_UTILITY_SAVEDATA_LOAD) && buf) dataSize = bufSize;
  p.dataBuf = buf; p.dataBufSize = bufSize; p.dataSize = dataSize;
  if (meta) {   // PARAM.SFO: название игры, строка слота, подробности (правило порта: текстов PS2 icon.sys в данных нет)
    strcpy(p.sfoParam.title, "The Simpsons Hit & Run");
    if (test) snprintf(p.sfoParam.savedataTitle, sizeof p.sfoParam.savedataTitle, "%s", test);
    else if (svmIsDev(meta)) snprintf(p.sfoParam.savedataTitle, sizeof p.sfoParam.savedataTitle, "DEV SAVE  %02d/%02d/%04d  ALL LEVELS",
                                      meta->day, meta->month, meta->year);   // правка 335: в окне PSP и в меню сохранений — латиницей
    else snprintf(p.sfoParam.savedataTitle, sizeof p.sfoParam.savedataTitle, "LEVEL %d  %02d/%02d/%04d %02d:%02d", meta->level,
                  meta->day, meta->month, meta->year, meta->hour, meta->minute);
    if (svmIsDev(meta)) snprintf(p.sfoParam.detail, sizeof p.sfoParam.detail, "Developer save. LOAD GAME, bottom slot, left/right picks "
                                 "the level. Story missions done except the last, bonus missions and races open, all cars and "
                                 "costumes owned, coins %d. Cards and wasps not collected.", (int)meta->coins);
    else snprintf(p.sfoParam.detail, sizeof p.sfoParam.detail, "Missions %d  Cards %d  Coins %d", svMissionsDone(meta),
             __builtin_popcount(meta->cards[meta->level]), (int)meta->coins);
    p.sfoParam.parentalLevel = 1;
    if (icon0 && var != SVV_NOICON) { p.icon0FileData.buf = icon0; p.icon0FileData.bufSize = icon0Size; p.icon0FileData.size = icon0Size; }
  }
  p.focus = PSP_UTILITY_SAVEDATA_FOCUS_LATEST;
  if (var == SVV_SV1 || var == SVV_NOICON) p.secureVersion = 1;
  if (var == SVV_KEY) memcpy(p.key, svvKey, sizeof p.key);
  int r = sceUtilitySavedataInitStart(&p);
  if (r < 0) return r;
  if (dlg) return dlgRun(&p);   // 309: окно PSP — каждый кадр Update, фон — застывший кадр меню
  int shut = 0, seen = 0;
  for (int k = 0; k < 60 * 20; k++) {             // не дольше 20 с: зависшая утилита — ошибка, а не зависшая игра
    int s = sceUtilitySavedataGetStatus();
    if (s >= 1) seen = 1;
    if (s == 2) sceUtilitySavedataUpdate(1);
    else if (s == 3 && !shut) { sceUtilitySavedataShutdownStart(); shut = 1; }
    else if (s == 0 && (shut || seen)) {
      r = p.base.result;
      // «savefail»: железо читает запись чужого набора как отказ параметров (тест железа: «Version 1/0 (no key): 80110308»);
      // пустой слот — как на PSP Роба (SVSCAN E при std): «нет данных» проходит
      if (svvFail && var == SVV_STD && mode == PSP_UTILITY_SAVEDATA_AUTOLOAD && r == 0) r = (int)ERR_LOAD_PARAM;
      return r;
    }
    sceDisplayWaitVblankStart();
  }
  return -1;
}
// отказ, после которого есть смысл пробовать следующий вариант: «неверный параметр» (0x801103x8), размер запроса; при чтении —
// и «запись испорчена» (так железо отвечает на чужой ключ / secureVersion: «Version 0/1: Load error: 80110306»)
static int svvRetry(int mode, unsigned r) {
  if ((r & 0xFFFFFF0Fu) == 0x80110308u || r == 0x80110004u) return 1;
  return mode == PSP_UTILITY_SAVEDATA_AUTOLOAD && (r == 0x80110306u || r == 0x80110326u);
}
// цепочка: удачный вариант сессии (или std) первым, дальше остальные по порядку. Пока первый проходит — один вызов, как раньше,
// и ни одной новой строки. Отказ параметров — строка «SVIO try=<вариант> for=<op> name=<слот> res=<код> ms=…» на каждую попытку
static int util(int mode, int slot, void *buf, unsigned bufSize, unsigned dataSize, const SaveGame *meta) {
  int order[SVV_N], n = 0;
  if (svvOld) order[n++] = SVV_STD;
  else { if (svvGood >= 0) order[n++] = svvGood; for (int v = 0; v < SVV_N; v++) if (v != svvGood) order[n++] = v; }
  const char *op = mode == PSP_UTILITY_SAVEDATA_AUTOSAVE ? "save" : mode == PSP_UTILITY_SAVEDATA_AUTOLOAD ? "load" : "delete";
  char nm[8]; snprintf(nm, sizeof nm, "%02d", slot);
  if (svUtilOff) { svvLast = SVV_STD; return (int)0x80110388u; }   // имитация: железо отвергло все варианты
  int r = 0, r0 = 0, broken = 0;
  for (int i = 0; i < n; i++) {
    if (i > 0) bbBeatTick();   // прошлый вызов утилиты закончился — игра жива (каждый вызов сам по себе под сторожем)
    unsigned t = sceKernelGetSystemTimeLow();
    r = utilVar(order[i], mode, nm, buf, bufSize, dataSize, meta, NULL); svvLast = order[i];
    if (i == 0) r0 = r;
    int retry = r != 0 && svvRetry(mode, (unsigned)r);
    if (i > 0 || (retry && i + 1 < n))
      logf_("SVIO try=%s for=%s name=%s res=%08x ms=%u\n", svvName[order[i]], op, nm, (unsigned)r, (sceKernelGetSystemTimeLow() - t) / 1000);
    if (r == 0) {
      if (svvGood != order[i] && !svvOld) { svvGood = order[i]; if (i > 0) logf_("SVVAR good=%s for=%s\n", svvName[svvGood], op); }
      return 0;
    }
    if (!retry) return r;                                       // «нет данных», «нет карты», «нет места» — другие варианты не помогут
    if (!broken && ((unsigned)r == 0x80110306u || (unsigned)r == 0x80110326u)) broken = r;
  }
  return broken ? broken : r0;   // все отвергли: «испорчено» важнее «параметра» (запись есть), иначе ответ первого
}
// 309: окно утилиты (SAVE / LOAD) цепочкой. Запись: удачный вариант сессии первым (на железе это никогда не std), дальше sv1, key,
// s1500, s1480, noicon; отказ параметров (окно не показано) — следующий; окно показалось — дальше не пробуем (второго окна подряд
// не будет). Загрузка — только вариантом, которым слот прочитан при сканировании (чужой вариант окно назвало бы записью испорченной).
// -> 0 записано / прочитано, 1 — «Нет» / ○ (отмена), иное — окно не справилось (код утилиты / <0): решает тихий путь
static int utilDlg(int mode, int slot, void *buf, unsigned bufSize, unsigned dataSize, const SaveGame *meta) {
  int order[SVV_N], n = 0, load = mode == PSP_UTILITY_SAVEDATA_LOAD;
  if (load) { if (slotVar[slot] >= 0) order[n++] = slotVar[slot]; }
  else {
    if (svvGood >= 0) order[n++] = svvGood;
    for (int v = SVV_SV1; v < SVV_N; v++) if (v != svvGood) order[n++] = v;
  }
  char nm[8]; snprintf(nm, sizeof nm, "%02d", slot);
  int r = (int)0x80110388u;
  for (int i = 0; i < n; i++) {
    if (i > 0) bbBeatTick();
    unsigned t = sceKernelGetSystemTimeLow(); D.frames = D.vis = D.swaps = 0; D.bg = "-";
    r = utilVar(order[i], mode, nm, buf, bufSize, dataSize, meta, NULL); svvLast = order[i];
    logf_("SVDLG op=%s slot=%d try=%s res=%08x vis=%d frames=%d swaps=%d bg=%s ms=%u\n", load ? "load" : "save", slot, svvName[order[i]],
          (unsigned)r, D.vis, D.frames, D.swaps, D.bg, (sceKernelGetSystemTimeLow() - t) / 1000);
    if (r == 0) {
      if (svvGood != order[i] && !svvOld) { svvGood = order[i]; logf_("SVVAR good=%s for=%s dlg=1\n", svvName[svvGood], load ? "load" : "save"); }
      return 0;
    }
    if (r == 1 || D.vis > 0) return r;                                  // отмена / окно было на экране — второго не показываем
    if (!(r < 0 || ((unsigned)r & 0xFFFFFF0Fu) == 0x80110308u || (unsigned)r == 0x80110004u)) return r;   // «нет места» и т.п.
  }
  return r;
}

static void stampNow(SaveGame *s) {
  ScePspDateTime t; memset(&t, 0, sizeof t); sceRtcGetCurrentClockLocalTime(&t);
  s->year = t.year; s->month = (uint8_t)t.month; s->day = (uint8_t)t.day; s->hour = (uint8_t)t.hour; s->minute = (uint8_t)t.minute;
  if (!SVM_SLOTLABEL_OLD) s->ext[SVM_EXT_SEC] = (uint8_t)(0x80 | (t.second < 60 ? t.second : 59));   // P14-6: секунды в подпись слота
}
static uint8_t ioBuf[1024] __attribute__((aligned(64)));
// Правка Роба 274 (04.10: «сохранение не работает — поправить точно, стопроцентно»): у слота два места — запись утилиты (SLES51897NN,
// видна в меню PSP) и файл save<NN>.bin рядом с игрой (savefile.c). Пишется в оба, читается свежее по seq; утилита отвергла параметры
// во всех вариантах — она «не ответила» (не «испорчено»), слот решает файл. Правило порта
static int readSlot(int k, SaveGame *out, unsigned *res) {   // SV_OK / SV_EMPTY / SV_DAMAGED
  memset(ioBuf, 0, sizeof ioBuf);
  int r = util(PSP_UTILITY_SAVEDATA_AUTOLOAD, k, ioBuf, sizeof ioBuf, 0, NULL); *res = (unsigned)r;
  int st, param = r != 0 && ((unsigned)r & 0xFFFFFF0Fu) == 0x80110308u;
  if ((unsigned)r == ERR_LOAD_NO_DATA || (unsigned)r == ERR_LOAD_NO_MS || (unsigned)r == ERR_LOAD_EJECT_MS || param) st = SV_EMPTY;
  else st = r != 0 ? SV_DAMAGED : svRead(ioBuf, sizeof ioBuf, out);
  slotVar[k] = r == 0 && st == SV_OK ? svvLast : -1;   // 309: окно загрузки — этим вариантом
  if (svFileOff) return st;
  static uint8_t fb[SV_BYTES] __attribute__((aligned(64))); SaveGame fg;
  int n = svFileRead(dir, k, fb, sizeof fb), fst = n <= 0 ? SV_EMPTY : svRead(fb, (unsigned)n, &fg);
  if (fst == SV_OK && (st != SV_OK || fg.seq > out->seq)) { *out = fg; logf_("SVFILE op=read slot=%d seq=%u util=%c\n", k, (unsigned)fg.seq, "OED"[st]); st = SV_OK; slotVar[k] = -1; }
  else if (fst == SV_DAMAGED && st == SV_EMPTY) st = SV_DAMAGED;
  return st;
}

// ---- правка 274: служебное
void svmFailFirst(int n) {   // «savefail 388» -> 0x80110388: цифры n — последние три шестнадцатеричные цифры кода; 0 — выкл.
  unsigned c = 0, m = 1; for (int d = n > 0 ? n : 0; d > 0; d /= 10, m <<= 4) c += (unsigned)(d % 10) * m;
  svvFail = c ? 0x80110000u | (c & 0xFFFu) : 0; svvGood = -1;   // удачный вариант забыт — как новый запуск
  logf_("SVFAIL code=%08x\n", svvFail);
}
void svmOldPath(int v) { svvOld = v; logf_("SVOLD on=%d\n", v); }
void svmBlock(int v) { svBlock = v; logf_("SVBLOCK on=%d\n", v); }
void svmFileOff(int v) { svFileOff = v; logf_("SVFILEOFF on=%d\n", v); }
void svmUtilOff(int v) { svUtilOff = v; logf_("SVUTILOFF on=%d\n", v); }
// замер-тур: каждый вариант пишет тестовую запись T8 (папка SLES51897T8), читает её тем же вариантом, сверяет байты, удаляет.
// Строка «SVMATRIX variant=… res=… load=… cmp=… del=… ms=…» — в журнал, в чёрный ящик и в savematrix.txt рядом с EBOOT (кольцо
// ящика 40 строк — за минуты игры строки замера вытесняются). На удачный вариант сессии не влияет: чистый замер
void svmMatrix(void) {   // явная просьба — и без «flag saves» (тур TEST 8 на PSP идёт служебным запуском, сохранения там выключены)
  iconLoad();
  char fn[300]; snprintf(fn, sizeof fn, "%ssavematrix.txt", dir);
  SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  SaveGame g; memset(&g, 0, sizeof g); g.level = g.msnLevel = 1; g.seq = 1;   // целая запись (svRead): новая игра, громкость 1000
  g.music = g.effects = g.vehicle = g.voice = 1000; stampNow(&g);
  static uint8_t out[SV_BYTES] __attribute__((aligned(64))); svWrite(&g, out);
  unsigned T0 = sceKernelGetSystemTimeLow(); int nOk = 0; char s[200], good[64] = "";
  for (int v = 0; v < SVV_N; v++) {
    bbBeatTick(); unsigned t = sceKernelGetSystemTimeLow(); char ld[12] = "-", cmp = '-';   // тур дольше 5 с: сторож — на один вызов
    int rs = utilVar(v, PSP_UTILITY_SAVEDATA_AUTOSAVE, "T8", out, sizeof out, sizeof out, &g, "SAVE TEST (deleted by the game)");
    if (rs == 0) {
      memset(ioBuf, 0, sizeof ioBuf); bbBeatTick();
      int rl = utilVar(v, PSP_UTILITY_SAVEDATA_AUTOLOAD, "T8", ioBuf, sizeof ioBuf, 0, NULL, NULL);
      snprintf(ld, sizeof ld, "%08x", (unsigned)rl);
      if (rl == 0) { SaveGame b; static uint8_t chk[SV_BYTES] __attribute__((aligned(64)));   // как проверка записи в doOp
        cmp = svRead(ioBuf, sizeof ioBuf, &b) == SV_OK && svWrite(&b, chk) && !memcmp(chk, out, SV_BYTES) ? 'O' : 'D'; }
    }
    bbBeatTick(); int rd = utilVar(v, SCE_UTILITY_SAVEDATA_AUTODELETE, "T8", 0, 0, 0, NULL, NULL), dv = v;
    for (int w = 0; w < SVV_N && rd != 0 && (unsigned)rd != ERR_DELETE_NO_DATA; w++)   // тестовая запись не остаётся на карте
      if (w != v) { bbBeatTick(); rd = utilVar(w, SCE_UTILITY_SAVEDATA_AUTODELETE, "T8", 0, 0, 0, NULL, NULL); dv = w; }
    if (rs == 0 && cmp == 'O') { nOk++; if (!good[0]) snprintf(good, sizeof good, "%s", svvName[v]); }
    snprintf(s, sizeof s, "SVMATRIX variant=%s res=%08x load=%s cmp=%c del=%08x delvar=%s ms=%u\n", svvName[v], (unsigned)rs, ld, cmp,
             (unsigned)rd, svvName[dv], (sceKernelGetSystemTimeLow() - t) / 1000);
    logf_("%s", s); if (f >= 0) sceIoWrite(f, s, strlen(s));
  }
  snprintf(s, sizeof s, "SVMATRIX done ok=%d/%d first=%s fail=%08x ms=%u\n", nOk, SVV_N, good[0] ? good : "-", svvFail,
           (sceKernelGetSystemTimeLow() - T0) / 1000);
  logf_("%s", s); if (f >= 0) { sceIoWrite(f, s, strlen(s)); sceIoClose(f); }
}

void svmScan(void) {
  if (!on) return;
  { char fn[300]; snprintf(fn, sizeof fn, "%ssavematrix", dir);   // флаг-файл «savematrix» рядом с EBOOT (замер-тур на PSP): один раз
    SceUID h = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (h >= 0) { sceIoClose(h); sceIoRemove(fn); svmMatrix(); } }   // файл убирается до замера: сбой посреди тура не зациклит запуск
  unsigned t0 = sceKernelGetSystemTimeLow(), res[SV_SLOTS]; char s[128]; int n = 0;
  for (int k = 0; k < SV_SLOTS; k++) slotSt[k] = readSlot(k, &slotSg[k], &res[k]);
  int latest = -1;
  for (int k = 0; k < SV_SLOTS; k++) if (slotSt[k] == SV_OK && (latest < 0 || slotSg[k].seq > slotSg[latest].seq)) latest = k;
  n = snprintf(s, sizeof s, "SVSCAN slots=");
  for (int k = 0; k < SV_SLOTS; k++) n += snprintf(s + n, sizeof s - n, "%s%c", k ? "," : "", "OED"[slotSt[k]]);
  n += snprintf(s + n, sizeof s - n, " res=");
  for (int k = 0; k < SV_SLOTS; k++) n += snprintf(s + n, sizeof s - n, "%s%08x", k ? "," : "", res[k]);
  snprintf(s + n, sizeof s - n, " latest=%d ms=%u\n", latest, (sceKernelGetSystemTimeLow() - t0) / 1000);
  logf_("%s", s);
  logf_("SVDLGMODE on=%d src=start var=%d,%d,%d,%d\n", svDlg, slotVar[0], slotVar[1], slotVar[2], slotVar[3]);   // 309
}
int svmLatest(SaveGame *out) {
  int latest = -1;
  for (int k = 0; k < SV_SLOTS; k++) if (on && slotSt[k] == SV_OK && (latest < 0 || slotSg[k].seq > slotSg[latest].seq)) latest = k;
  if (latest < 0) return 0;
  if (out) { *out = slotSg[latest]; if (svmIsDev(out)) devTake(out); }   // 335: DEV — только если других записей нет (seq 0)
  logf_("SVLATEST slot=%d seq=%u\n", latest, (unsigned)slotSg[latest].seq);
  return 1;
}

// ---- правка 335: сохранение разработчика (savemenu.h). Запись — тем же тихим путём, что сохранение без окна (утилита цепочкой
// вариантов + файл save03.bin, подтверждение чтением); при запуске окна PSP нет — Роб ещё не в игре. Содержимое: монеты, всё
// купленное/полученное всех уровней (бит на BindReward), карточки/осы/гэги и монеты мира не собраны, настройки по умолчанию
// (загрузка DEV настройки не трогает — main.c saveApply), костюм обычный. Уровень и миссии ставит загрузка (devTake + main.c)
static void devFill(SaveGame *g) {
  svClear(g); g->seq = 0; g->coins = SVM_DEV_COINS;
  for (int L = 1; L < SV_LEVELS; L++) g->owned[L] = 0xffff;
  stampNow(g);
}
void svmDevBoot(int (*levelAvail)(int)) {
  devAvail = levelAvail;
  if (!on) return;
  char fn[300], w[16] = {0}; snprintf(fn, sizeof fn, "%sdevsave", dir);
  SceUID h = sceIoOpen(fn, PSP_O_RDONLY, 0); if (h < 0) return;
  sceIoRead(h, w, sizeof w - 1); sceIoClose(h);
  int k = SVM_DEV_SLOT, force = !strncmp(w, "force", 5); unsigned t0 = sceKernelGetSystemTimeLow();
  SaveGame g; devFill(&g);
  static uint8_t out[SV_BYTES] __attribute__((aligned(64))), chk[SV_BYTES] __attribute__((aligned(64)));
  const char *why = NULL;
  if (slotSt[k] == SV_OK && !svmIsDev(&slotSg[k]) && !force) why = "busy";        // обычная запись Роба — не трогаем
  else if (slotSt[k] == SV_DAMAGED && !force) why = "damaged";
  else if (slotSt[k] == SV_OK && svmIsDev(&slotSg[k])) {   // DEV уже есть: переписать, только если другой день или другое содержимое
    SaveGame a = slotSg[k]; a.hour = g.hour; a.minute = g.minute; a.ext[SVM_EXT_SEC] = g.ext[SVM_EXT_SEC]; svWrite(&a, chk); svWrite(&g, out);
    if (!memcmp(chk, out, SV_BYTES)) why = "same";
  }
  if (why) { logf_("SVDEV boot slot=%d skip=%s st=%c\n", k, why, "OED"[slotSt[k]]); return; }
  svWrite(&g, out);
  unsigned r = 0; int fw = 0, st = SV_OK;
  if (!noWrite) {
    r = (unsigned)util(PSP_UTILITY_SAVEDATA_AUTOSAVE, k, out, sizeof out, sizeof out, &g);
    fw = (svFileOff || svBlock) ? 1 : svFileWrite(dir, k, out, SV_BYTES);
    if (r == 0 || fw == 0) {   // как doOp: подтверждение чтением — те же байты
      SaveGame back; unsigned r2 = 0; st = readSlot(k, &back, &r2);
      if (st == SV_OK) { svWrite(&back, chk); if (memcmp(chk, out, SV_BYTES)) st = SV_DAMAGED; }
    } else st = SV_DAMAGED;
  }
  if (st == SV_OK) { slotSt[k] = SV_OK; slotSg[k] = g; }
  logf_("SVDEV boot slot=%d res=%08x file=%d verify=%c var=%s force=%d coins=%d owned=%x date=%02d/%02d/%04d ms=%u\n", k, r, fw,
        "OED"[st], svvName[svvLast], force, (int)g.coins, (unsigned)g.owned[1], g.day, g.month, g.year, (sceKernelGetSystemTimeLow() - t0) / 1000);
}

// ---- служебное (сборка с -DSVM_ALLSAVE): запись «пройдено всё» в выбранный слот. Флаг-файл allsave (в нём номер слота 0..3, пусто — 3) и allsave.bin (SV_BYTES, собран
// настоящими модулями игры: save_work/mkallsave) рядом с EBOOT. Запись — тем же путём, что DEV SAVE: утилита сохранения цепочкой вариантов + файл, подтверждение чтением;
// номер записи на единицу больше любого из слотов (RESUME GAME берёт её), время — часы устройства. В обычную игру не входит
void svmAllBoot(void) {
#ifdef SVM_ALLSAVE
  if (!on) return;
  char fn[300], w[16] = {0}; snprintf(fn, sizeof fn, "%sallsave", dir);
  SceUID h = sceIoOpen(fn, PSP_O_RDONLY, 0); if (h < 0) return;
  sceIoRead(h, w, sizeof w - 1); sceIoClose(h);
  int k = w[0] >= '0' && w[0] <= '3' ? w[0] - '0' : 3;
  static uint8_t in[SV_BYTES] __attribute__((aligned(64))), out[SV_BYTES] __attribute__((aligned(64))), chk[SV_BYTES] __attribute__((aligned(64)));
  snprintf(fn, sizeof fn, "%sallsave.bin", dir);
  h = sceIoOpen(fn, PSP_O_RDONLY, 0); if (h < 0) { logf_("SVALL boot slot=%d skip=nobin\n", k); return; }
  int rn = sceIoRead(h, in, sizeof in); sceIoClose(h);
  SaveGame g; if (rn != SV_BYTES || svRead(in, SV_BYTES, &g) != SV_OK) { logf_("SVALL boot slot=%d skip=badbin n=%d\n", k, rn); return; }
  unsigned mx = 0; for (int i = 0; i < SV_SLOTS; i++) if (slotSt[i] == SV_OK && slotSg[i].seq > mx) mx = slotSg[i].seq;
  if (slotSt[k] == SV_OK) {   // записываемый слот уже занят (сейв Роба): его настройки, кошелёк и костюм переносятся, чтобы запись «пройдено всё» не сбрасывала их
    const SaveGame *o = &slotSg[k];
    g.music = o->music; g.effects = o->effects; g.vehicle = o->vehicle; g.voice = o->voice; g.radar = o->radar; g.skin = o->skin;
    g.ext[SV_EXT_CAM] = o->ext[SV_EXT_CAM]; if (o->coins > g.coins) g.coins = o->coins;
  }
  g.seq = mx + 1; stampNow(&g); svWrite(&g, out);
  unsigned t0 = sceKernelGetSystemTimeLow(), r = 0; int fw = 0, st = SV_OK;
  if (!noWrite) {
    r = (unsigned)util(PSP_UTILITY_SAVEDATA_AUTOSAVE, k, out, sizeof out, sizeof out, &g);
    fw = (svFileOff || svBlock) ? 1 : svFileWrite(dir, k, out, SV_BYTES);
    if (r == 0 || fw == 0) { SaveGame back; unsigned r2 = 0; st = readSlot(k, &back, &r2); if (st == SV_OK) { svWrite(&back, chk); if (memcmp(chk, out, SV_BYTES)) st = SV_DAMAGED; } }
    else st = SV_DAMAGED;
  }
  if (st == SV_OK) { slotSt[k] = SV_OK; slotSg[k] = g; }
  logf_("SVALL boot slot=%d res=%08x file=%d verify=%c var=%s seq=%u missions=%d ms=%u\n", k, r, fw, "OED"[st], svvName[svvLast], g.seq, svMissionsDone(&g), (sceKernelGetSystemTimeLow() - t0) / 1000);
#endif
}

// ---- экраны
enum { ST_OFF, ST_ASK, ST_SLOTS, ST_CONFIRM, ST_BUSY, ST_NOTICE, ST_SYSDLG, ST_YESNO };   // ST_YESNO — g09b-menu: вопрос Yes/No (Quit? / NEW GAME)   // ST_SYSDLG — 309: окно PSP (фон — экран слотов)
enum { OP_NONE, OP_SAVE, OP_LOAD, OP_DELETE };
enum { AFTER_SLOTS, AFTER_CLOSE };
static struct {
  int st, mode, askQuit, confirmLoad, sel, psel, op, opSlot, drawn, msg, msgSlot, error, after, result, ask; unsigned busyT0;
  SaveGame game, loaded;
  char rows[SV_SLOTS][48], msgText[256];
} S;

static const char *str(int k) { return uiScrRowLabel(SCR_STRINGS, k); }
static void setMsg(int m, int slot, int error) {   // текст сообщения: строка библии из пакета, «$» -> номер слота (с 1)
  S.msg = m; S.msgSlot = slot; S.error = error;
  const char *a = str(SVS_MSG + m); int n = 0;
  for (; *a && n < (int)sizeof S.msgText - 4; a++) {
    if (*a == '$') n += snprintf(S.msgText + n, sizeof S.msgText - n, "%d", slot + 1); else S.msgText[n++] = *a;
  }
  S.msgText[n] = 0;
  uiScrRowText(SCR_PROMPT, ROW_MSG, S.msgText); uiScrRowText(SCR_NOTICE, ROW_MSG, S.msgText); uiScrRowText(SCR_MESSAGE, ROW_MSG, S.msgText);
  uiScrShow(SCR_NOTICE, UI_T_MISSION_FAILED, error); uiScrShow(SCR_PROMPT, UI_T_MISSION_FAILED, 0);
  uiScrShow(SCR_MESSAGE, UI_T_MISSION_FAILED, 0);
  logf_("SVMSG key=%s slot=%d error=%d text=%.60s\n", msgKey[m], slot, error, S.msgText);
}
// g09b-menu: вопрос с готовым текстом (английские строки с диска: подтверждение выхода и подтверждение новой игры; ключа в пакете нет)
static void setMsgRaw(const char *text, const char *key) {
  S.msg = -1; S.msgSlot = 0; S.error = 0;
  snprintf(S.msgText, sizeof S.msgText, "%s", text);
  uiScrRowText(SCR_PROMPT, ROW_MSG, S.msgText); uiScrShow(SCR_PROMPT, UI_T_MISSION_FAILED, 0);
  logf_("SVMSG key=%s slot=0 error=0 text=%.60s\n", key, S.msgText);
}
// круг 3: чем отвечает START: SVM_R_UNPAUSE — закрыть паузу целиком (запись из паузы, выход из паузы), -1 — START не действует (конец уровня / игры, список LOAD GAME)
static int startRes(void) {
  if (SVM_START3_OLD) return (S.askQuit & 1) ? SVM_R_BACK : S.mode == SVM_SAVE ? SVM_R_UNPAUSE : SVM_R_BACK;
  if (S.mode != SVM_SAVE || (S.askQuit & 3) == 1) return -1;
  return SVM_R_UNPAUSE;
}
static void yesNo(int kind) {   // kind: SVM_ASK_QUIT / SVM_ASK_NEWGAME; курсор на «No» (SVM_DEFNO_OLD=1 — на «Yes»)
  S.st = ST_YESNO; S.ask = kind; S.psel = SVM_DEFNO_OLD ? 0 : 1;
  if (kind == SVM_ASK_QUIT) setMsgRaw("Are you sure you want to quit?", "quit_confirm");
  else if (kind == SVM_ASK_RESTART) setMsgRaw("Are you sure you want to restart this mission?", "restart_confirm");   // сверка g09a-hud круг 5 (M2:Д6)
  else if (kind == SVM_ASK_ABORT) setMsgRaw("Are you sure you want to abort this mission?", "abort_confirm");
  else setMsgRaw("Restarting will cause all unsaved game progress to be lost. Restart anyway?", "newgame_confirm");
}
// P14-6 / P16-24: подпись слота «(L<ур> M<мис>) дд/мм/гг чч:мм:сс»; L1, миссия 0 — обучение: «(TUT)»; сокращения L, M, TUT — из текстов диска
static void slotLabel(char *o, int n, const SaveGame *g) {
  int L = g->level >= 1 && g->level <= 7 ? g->level : 1, sec = (g->ext[SVM_EXT_SEC] & 0x80) ? g->ext[SVM_EXT_SEC] & 0x7f : 0;
  char who[24];
  if (g->msnStory == 0xff) snprintf(who, sizeof who, "(L%d)", L);                                  // миссий нет (уровень без сюжета): только уровень
  else if (L == 1 && g->msnStory == 0) snprintf(who, sizeof who, SVM_TUTLABEL_OLD ? "(L1 TUT)" : "(TUT)");   // круг 3: у обучения одно слово вместо обеих аббревиатур
  else snprintf(who, sizeof who, "(L%d M%d)", L, (int)g->msnStory);
  snprintf(o, (size_t)n, "%s %02d/%02d/%02d %02d:%02d:%02d", who, g->day, g->month, g->year % 100, g->hour, g->minute, sec);
}
static void rowsRefresh(void) {   // подписи слотов: пустой — из пакета, испорченный — «< Damaged Save >», целый — LEVEL N + время
  for (int k = 0; k < SV_SLOTS; k++) {
    const SaveGame *g = &slotSg[k];
    if (slotSt[k] == SV_OK && svmIsDev(g)) {   // правка 335: «DEV SAVE < LEVEL N > дд/мм» (стрелки — в LOAD GAME, когда уровней больше одного)
      int lv = devOk(devLevel) ? devLevel : 1, arrows = S.mode == SVM_LOAD && devLevels() > 1;
      snprintf(S.rows[k], sizeof S.rows[k], arrows ? "DEV SAVE < %s > %02d/%02d" : "DEV SAVE  %s  %02d/%02d", str(SVS_LEVEL1 + lv - 1),
               g->day, g->month);
      uiScrRowText(SCR_SLOTS, k, S.rows[k]);
    } else if (slotSt[k] == SV_OK && !SVM_SLOTLABEL_OLD) {
      slotLabel(S.rows[k], sizeof S.rows[k], g); uiScrRowText(SCR_SLOTS, k, S.rows[k]);
    } else if (slotSt[k] == SV_OK) {
      snprintf(S.rows[k], sizeof S.rows[k], "%s   %02d/%02d  %02d:%02d", str(SVS_LEVEL1 + (g->level >= 1 && g->level <= 7 ? g->level - 1 : 0)),
               g->day, g->month, g->hour, g->minute);
      uiScrRowText(SCR_SLOTS, k, S.rows[k]);
    } else uiScrRowText(SCR_SLOTS, k, slotSt[k] == SV_DAMAGED ? str(SVS_DAMAGED) : NULL);
    uiScrEnable(SCR_SLOTS, k, S.mode == SVM_SAVE || (SVM_LOADDAMAGED_OLD ? slotSt[k] != SV_EMPTY : slotSt[k] == SV_OK));   // загрузка: пустые слоты серые, курсор их пропускает; P14-5: и повреждённые (PS2)
  }
}
static void gotoSlots(void) {
  S.st = ST_SLOTS; rowsRefresh();
  if (!uiScrEnabled(SCR_SLOTS, S.sel)) for (int k = 0; k < SV_SLOTS; k++) if (uiScrEnabled(SCR_SLOTS, k)) { S.sel = k; break; }
}
static int latestSlotIdx(void) {   // P14-4: слот с наибольшим seq среди читаемых (DEV, seq 0 — последним), иначе -1
  int b = -1; for (int k = 0; k < SV_SLOTS; k++) if (slotSt[k] == SV_OK && (b < 0 || slotSg[k].seq > slotSg[b].seq)) b = k;
  return b;
}
static void notice(int m, int slot, int error, int after) { S.st = ST_NOTICE; S.psel = 0; S.after = after; setMsg(m, slot, error); }
static void busy(int op, int slot) {
  S.st = ST_BUSY; S.op = op; S.opSlot = slot; S.drawn = 0; S.busyT0 = sceKernelGetSystemTimeLow();
  setMsg(op == OP_SAVE ? SVM_MSG_SAVING : op == OP_LOAD ? SVM_MSG_LOADING : SVM_MSG_DELETING, slot, 0);
}
static void closeWith(int r) {
  logf_("SVCLOSE r=%d\n", r);
  uiScrClose(); S.st = ST_OFF; S.result = r;
}
// 309: окно PSP для слота — два кадра экрана слотов (оба буфера: фон окна), потом doOp; загрузка окном — если слот читается записью
// утилиты (решён файлом — тихо, как раньше)
static int dlgFor(int op, int slot) { return svDlg && (op == OP_SAVE || (op == OP_LOAD && slotVar[slot] >= 0)); }
static void dlgStart(int op, int slot) { S.st = ST_SYSDLG; S.op = op; S.opSlot = slot; S.drawn = 0; }

int svmOpen(int mode, const SaveGame *game, int confirmLoad, int askQuit) {
  if (!on) return -1;
  char fn[300]; snprintf(fn, sizeof fn, "%s%s", dir, mode == SVM_SAVE ? "psave.shr" : "fsave.shr");
#ifndef C3_LEVEL_MENUS_OLD
  // SaveGame.scr uses the active level's PauseBgd, exactly like its pause menu.
  // File/utility persistence still uses dir; this selects only the UI package.
  if (mode == SVM_SAVE && game && game->level > 1 && game->level <= 7)
    snprintf(fn, sizeof fn, "%sl%d/psave.shr", dir, game->level);
#endif
  if (uiScrOpen(fn)) { logf_("SVOPEN mode=%d err=pack\n", mode); return -1; }
  memset(&S, 0, sizeof S); svPickDone = 0;
  S.mode = mode; S.askQuit = askQuit; S.confirmLoad = confirmLoad;
  if (game) S.game = *game;
  logf_("SVOPEN mode=%s ask=%d confirm=%d slots=%c,%c,%c,%c\n", mode == SVM_SAVE ? "save" : "load", askQuit, confirmLoad,
        "OED"[slotSt[0]], "OED"[slotSt[1]], "OED"[slotSt[2]], "OED"[slotSt[3]]);
  if (SVM_QUITASK_OLD) S.askQuit = askQuit &= 1;
  if (!SVM_LOADASK_OLD && mode == SVM_LOAD) S.confirmLoad = 1;   // M3 Д7: подтверждение загрузки слота — всегда
  if (askQuit & 12) { yesNo(askQuit & 4 ? SVM_ASK_RESTART : SVM_ASK_ABORT); return 0; }   // сверка g09a-hud круг 5: Restart / Abort Mission из паузы миссии
  if (askQuit & 2) { yesNo(SVM_ASK_QUIT); return 0; }            // М2 Д2 / P13-1: сначала «выйти?»
  if (askQuit) { S.st = ST_ASK; S.psel = 0; setMsg(SVM_MSG_SAVE_BEFORE_QUIT, 0, 0); return 0; }
  int any = 0; for (int k = 0; k < SV_SLOTS; k++) any |= (SVM_LOADDAMAGED_OLD || mode != SVM_LOAD ? slotSt[k] != SV_EMPTY : slotSt[k] == SV_OK);
  if (mode == SVM_LOAD && !any) { notice(SVM_MSG_NO_DATA, 0, 1, AFTER_CLOSE); S.result = SVM_R_BACK; return 0; }
  gotoSlots();
  if (mode == SVM_LOAD && !SVM_LATESTSEL_OLD) { int b = latestSlotIdx(); if (b >= 0) S.sel = b; }   // P14-4
  return 0;
}
// g09b-menu: отдельный вопрос Yes/No вне записи (NEW GAME при загруженной игре — M3 №7, P14-3); пакет fsave.shr (рамка-ТВ главного меню). 0 = открыт
int svmAskOpen(int kind) {
  if (!on) return -1;
  char fn[300]; snprintf(fn, sizeof fn, "%sfsave.shr", dir);
  if (uiScrOpen(fn)) { logf_("SVOPEN mode=ask err=pack\n"); return -1; }
  memset(&S, 0, sizeof S); S.mode = SVM_LOAD;
  logf_("SVOPEN mode=ask kind=%d\n", kind);
  yesNo(kind); return 0;
}
int svmActive(void) { return S.st != ST_OFF; }
const SaveGame *svmLoaded(void) { return &S.loaded; }

static void doOp(void) {   // сообщение уже на экране (оба буфера): утилита, потом итог
  unsigned t0 = sceKernelGetSystemTimeLow(), res = 0; int k = S.opSlot, st = -1;
  if (S.op == OP_SAVE) {
    SaveGame g = S.game; uint32_t seq = 0;
    for (int i = 0; i < SV_SLOTS; i++) if (slotSt[i] == SV_OK && slotSg[i].seq > seq) seq = slotSg[i].seq;
    g.seq = seq + 1; stampNow(&g);
    static uint8_t out[SV_BYTES] __attribute__((aligned(64)));
    svWrite(&g, out);
    const char *var = "-";
    int dlg = S.st == ST_SYSDLG;   // 309: подтверждение и запись — окном PSP
    if (noWrite) st = SV_OK;   // служебное: «сборка без записи» (отрицательный контроль гейта) — ничего не пишется
    else {
      int r = 1;
      if (dlg) {
        r = utilDlg(PSP_UTILITY_SAVEDATA_SAVE, k, out, sizeof out, sizeof out, &g);
        if (r == 1) {   // «Нет» / ○ в окне: ничего не пишется (и файлом тоже), назад к слотам
          logf_("SVIO op=save slot=%d res=cancel dlg=1 ms=%u\n", k, (sceKernelGetSystemTimeLow() - t0) / 1000); gotoSlots(); return; }
        if (r != 0) { logf_("SVDLGQUIET op=save slot=%d res=%08x\n", k, (unsigned)r); dlg = 0; }   // окно не справилось — тихий путь
      }
      if (!dlg) r = util(PSP_UTILITY_SAVEDATA_AUTOSAVE, k, out, sizeof out, sizeof out, &g);
      res = (unsigned)r; var = svvName[svvLast];
      int fw = (svFileOff || svBlock) ? 1 : svFileWrite(dir, k, out, SV_BYTES);   // правка 274: и файлом — запись жива, даже если утилита отказала
      if (!svFileOff) logf_("SVFILE op=save slot=%d res=%d util=%08x\n", k, fw, res);
      if (r == 0 || fw == 0) {                            // запись подтверждается чтением: те же байты (свежее из двух мест)
        SaveGame back; unsigned r2 = 0; static uint8_t chk[SV_BYTES] __attribute__((aligned(64)));
        st = readSlot(k, &back, &r2);
        if (st == SV_OK) { svWrite(&back, chk); if (memcmp(chk, out, SV_BYTES)) st = SV_DAMAGED; }
        if (st != SV_OK) slotSt[k] = st;   // слот показывает то, что на карте
        if (r != 0 && st == SV_OK) var = "file";
      } else st = SV_DAMAGED;
    }
    logf_("SVIO op=save slot=%d res=%08x verify=%c seq=%u ms=%u var=%s dlg=%d\n", k, res, "OED"[st], (unsigned)g.seq, (sceKernelGetSystemTimeLow() - t0) / 1000, var, dlg);
    if (st == SV_OK && !SVM_SAVEDFLAG_OLD) pauseSaved = 1;   // P13-Д4: после удачной записи в этой паузе выход «сохранить?» не спрашивает
    if (st == SV_OK && dlg) { slotSt[k] = SV_OK; slotSg[k] = g; closeWith(S.askQuit ? SVM_R_QUIT : SVM_R_SAVED); }   // 309: «завершено» сказало окно
    else if (st == SV_OK) { slotSt[k] = SV_OK; slotSg[k] = g; notice(SVM_MSG_SAVED, k, 0, AFTER_CLOSE); S.result = S.askQuit ? SVM_R_QUIT : SVM_R_SAVED; }
    else notice(SVM_MSG_SAVE_FAILED, k, 1, AFTER_SLOTS);   // прежние записи и игра в памяти не тронуты
  } else if (S.op == OP_LOAD) {
    int dlg = S.st == ST_SYSDLG;   // 309: «Загрузить эти данные?» — окном PSP тем вариантом, которым слот прочитан при сканировании
    if (dlg) {
      memset(ioBuf, 0, sizeof ioBuf);
      int r = utilDlg(PSP_UTILITY_SAVEDATA_LOAD, k, ioBuf, sizeof ioBuf, 0, NULL); res = (unsigned)r;
      if (r == 1) { logf_("SVIO op=load slot=%d res=cancel dlg=1 ms=%u\n", k, (sceKernelGetSystemTimeLow() - t0) / 1000); gotoSlots(); return; }
      SaveGame g;
      if (r == 0 && svRead(ioBuf, sizeof ioBuf, &g) == SV_OK) { S.loaded = g; st = SV_OK; }
      else { logf_("SVDLGQUIET op=load slot=%d res=%08x\n", k, (unsigned)r); dlg = 0; }   // окно не справилось — тихо: утилита цепочкой + файл
    }
    if (!dlg) st = readSlot(k, &S.loaded, &res);
    slotSt[k] = st; if (st == SV_OK) slotSg[k] = S.loaded;
    if (st == SV_OK && svmIsDev(&S.loaded)) { devTake(&S.loaded); logf_("SVDEV load slot=%d level=%d\n", k, S.loaded.level); }   // 335
    logf_("SVIO op=load slot=%d res=%08x st=%c ms=%u var=%s dlg=%d\n", k, res, "OED"[st], (sceKernelGetSystemTimeLow() - t0) / 1000, svvName[svvLast], dlg);
    if (st == SV_OK && dlg) closeWith(SVM_R_LOADED);   // 309: «Загрузка завершена» сказало окно
    else if (st == SV_OK) { notice(SVM_MSG_LOADED, k, 0, AFTER_CLOSE); S.result = SVM_R_LOADED; }
    else if (!SVM_LOADDAMAGED_OLD) { rowsRefresh(); notice(SVM_MSG_CORRUPT, k, 1, AFTER_SLOTS); S.op = OP_NONE; }   // P14-5: на PS2 удаления из загрузки нет — слот просто серый
    else { rowsRefresh(); notice(SVM_MSG_CORRUPT, k, 1, AFTER_SLOTS); S.op = OP_DELETE; }   // как PS2: «corrupt and must be deleted. OK?»
  } else if (S.op == OP_DELETE) {
    int r = util(SCE_UTILITY_SAVEDATA_AUTODELETE, k, 0, 0, 0, NULL); res = (unsigned)r;
    int fd = svFileOff ? 0 : svFileDelete(dir, k), gone = r == 0 || res == ERR_DELETE_NO_DATA;
    if (!gone && fd == 0 && ((unsigned)r & 0xFFFFFF0Fu) == 0x80110308u) { SaveGame g; unsigned r2; gone = readSlot(k, &g, &r2) == SV_EMPTY; }   // 274: утилита не ответила — слот пуст?
    logf_("SVIO op=delete slot=%d res=%08x file=%d ms=%u var=%s\n", k, res, fd, (sceKernelGetSystemTimeLow() - t0) / 1000, svvName[svvLast]);
    if (gone && fd == 0) { slotSt[k] = SV_EMPTY; gotoSlots(); }
    else notice(SVM_MSG_DELETE_FAILED, k, 1, AFTER_SLOTS);
    int any = 0; for (int i = 0; i < SV_SLOTS; i++) any |= slotSt[i] != SV_EMPTY;
    if (S.st == ST_SLOTS && S.mode == SVM_LOAD && !any) { notice(SVM_MSG_NO_DATA, 0, 1, AFTER_CLOSE); S.result = SVM_R_BACK; }
  }
}

int svmInput(unsigned hit) {
  if (S.st == ST_OFF) return SVM_R_NONE;
  int r;
  switch (S.st) {
    case ST_ASK:      // «Do you want to save your game?» YES — слоты, NO — выход без записи, △ — назад в паузу (P14-7: △ и ○ не действуют)
      if (!SVM_START_OLD && (hit & PSP_CTRL_START)) { int sr = startRes(); if (sr >= 0) { closeWith(sr); return sr; } }   // P13-Д5: START снимает паузу (круг 3); в конце уровня не действует
      if (!SVM_NOBACK_OLD) hit &= ~(unsigned)(PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE);
      r = uiScrMenuInput(SCR_PROMPT, &S.psel, hit);
      if (r == UI_SCR_BACK) { closeWith(SVM_R_BACK); return SVM_R_BACK; }
      if (r == 0) gotoSlots();
      else if (r == 1) { closeWith(SVM_R_QUIT); return SVM_R_QUIT; }
      break;
    case ST_YESNO:    // g09b-menu: Quit? / NEW GAME — Yes или No; △ и ○ не действуют (P14-7); START в потоке выхода — отмена
      if (!SVM_START_OLD && S.ask == SVM_ASK_QUIT && (hit & PSP_CTRL_START)) { int sr = startRes(); if (sr >= 0) { closeWith(sr); return sr; } }
      if (!SVM_NOBACK_OLD) hit &= ~(unsigned)(PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE);
      r = uiScrMenuInput(SCR_PROMPT, &S.psel, hit);
      if (r == UI_SCR_BACK) r = 1;                                  // прежнее поведение (SVM_NOBACK_OLD=1): назад = No
      if (r == 0) {
        if (S.ask == SVM_ASK_NEWGAME || S.ask == SVM_ASK_RESTART || S.ask == SVM_ASK_ABORT) { closeWith(SVM_R_YES); return SVM_R_YES; }
        if ((S.askQuit & 1) && !(pauseSaved && !SVM_SAVEDFLAG_OLD)) { S.st = ST_ASK; S.psel = 0; setMsg(SVM_MSG_SAVE_BEFORE_QUIT, 0, 0); }   // второй вопрос — только если в этой паузе не сохранялись
        else { closeWith(SVM_R_QUIT); return SVM_R_QUIT; }
      } else if (r == 1) { int rr = S.ask == SVM_ASK_QUIT ? SVM_R_BACK : SVM_R_NO; closeWith(rr); return rr; }
      break;
    case ST_SLOTS:
      if (!SVM_START_OLD && (hit & PSP_CTRL_START) && !svPick) {   // P13-Д5: START на слотах закрывает паузу целиком (и в потоке выхода из паузы — круг 3); в конце уровня не действует
        int sr = startRes(); if (sr >= 0) { closeWith(sr); return sr; } }
      if (svPick && S.mode == SVM_SAVE && !svPickDone) {   // тур для железа: первый пустой слот, иначе отмена записи
        int t = -1; svPickDone = 1;
        for (int k = 0; k < SV_SLOTS && t < 0; k++) if (slotSt[k] == SV_EMPTY) t = k;
        if (t < 0) { logf_("ENDSAVE skip no-empty-slot slots=%c%c%c%c\n", "OED"[slotSt[0]], "OED"[slotSt[1]], "OED"[slotSt[2]], "OED"[slotSt[3]]); closeWith(SVM_R_BACK); return SVM_R_BACK; }
        S.sel = t; hit |= PSP_CTRL_CROSS; logf_("SVPICK slot=%d\n", t);
      }
      if (S.mode == SVM_LOAD && (hit & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) && slotSt[S.sel] == SV_OK && svmIsDev(&slotSg[S.sel]) &&
          devLevels() > 1) {   // правка 335: ◄ ► на строке DEV SAVE — уровень загрузки (только уровни, чьи наборы рядом с EBOOT)
        int d = (hit & PSP_CTRL_RIGHT) ? 1 : -1, n = devOk(devLevel) ? devLevel : 1;
        for (int k = 0; k < 7; k++) { n = (n - 1 + d + 7) % 7 + 1; if (devOk(n)) break; }
        devLevel = n; rowsRefresh(); uiPopHookFire(UI_EV_SCROLL);
        logf_("SVDEV pick level=%d of=%d\n", devLevel, devLevels());
        hit &= ~(unsigned)(PSP_CTRL_LEFT | PSP_CTRL_RIGHT);
      }
      r = uiScrMenuInput(SCR_SLOTS, &S.sel, hit);
      if (hit) logf_("SVSEL sel=%d r=%d st=%c\n", S.sel, r, "OED"[slotSt[S.sel]]);
      if (r == UI_SCR_BACK) {
        if (!SVM_SLOTBACK_OLD && (S.askQuit & 1) && S.mode == SVM_SAVE) { S.st = ST_ASK; S.psel = 0; setMsg(SVM_MSG_SAVE_BEFORE_QUIT, 0, 0); break; }   // P13-Д6: ○ из слотов потока выхода — снова «сохранить?»
        closeWith(SVM_R_BACK); return SVM_R_BACK; }
      if (r < 0) break;
      if (S.mode == SVM_SAVE) {
        if (dlgFor(OP_SAVE, r)) dlgStart(OP_SAVE, r);   // 309: «сохранить?» / «перезаписать?» спросит окно PSP
        else if (slotSt[r] == SV_EMPTY) busy(OP_SAVE, r);
        else { S.st = ST_CONFIRM; S.op = OP_SAVE; S.opSlot = r; S.psel = SVM_DEFNO_OLD ? 0 : 1; setMsg(SVM_MSG_OVERWRITE, r, 0); }
      } else if (slotSt[r] == SV_DAMAGED) { S.op = OP_DELETE; S.opSlot = r; notice(SVM_MSG_CORRUPT, r, 1, AFTER_SLOTS); }
      else if (slotSt[r] == SV_OK) {
        if (S.confirmLoad) { S.st = ST_CONFIRM; S.op = OP_LOAD; S.opSlot = r; S.psel = SVM_DEFNO_OLD ? 0 : 1; setMsg(SVM_MSG_LOAD_CONFIRM, r, 0); }
        else if (dlgFor(OP_LOAD, r)) dlgStart(OP_LOAD, r);
        else busy(OP_LOAD, r);
      }
      break;
    case ST_CONFIRM:  // YES — выполнить, NO / △ — к слотам (P14-7: △ и ○ не действуют; START — как на слотах)
      if (!SVM_START_OLD && (hit & PSP_CTRL_START) && S.mode == SVM_SAVE) { int sr = startRes(); if (sr >= 0) { closeWith(sr); return sr; } }
      if (!SVM_NOBACK_OLD) hit &= ~(unsigned)(PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE);
      r = uiScrMenuInput(SCR_PROMPT, &S.psel, hit);
      if (r == 0) { if (dlgFor(S.op, S.opSlot)) dlgStart(S.op, S.opSlot); else busy(S.op, S.opSlot); }   // 309: загрузка поверх игры — окно после нашего «прогресс будет потерян»
      else if (r == 1 || r == UI_SCR_BACK) gotoSlots();
      break;
    case ST_BUSY:
      if (S.drawn >= 2) {
        unsigned t0 = S.busyT0; doOp();
        if (!SVM_BUSYMIN_OLD && S.st != ST_BUSY && S.st != ST_SYSDLG) {   // P14-18: сообщение «Saving / Loading» живёт не меньше SVM_BUSY_MIN_S (кадр стоит на сообщении)
          int left = (int)(SVM_BUSY_MIN_S * 1000.f) - (int)((sceKernelGetSystemTimeLow() - t0) / 1000);
          for (; left > 0; left -= 50) { sceKernelDelayThread(50 * 1000); bbBeatTick(); }
        }
      }
      break;
    case ST_SYSDLG:   // 309: окно PSP (кадр стоит на экране слотов, окно поверх); закрылось удачно — меню закрыто
      if (S.drawn >= 2) { doOp(); if (S.st == ST_OFF) return S.result; }
      break;
    case ST_NOTICE:   // Continue (X; P14-7: △ и ○ не действуют)
      if (!SVM_NOBACK_OLD) hit &= ~(unsigned)(PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE);
      r = uiScrMenuInput(SCR_NOTICE, &S.psel, hit);
      if (r == 0 || r == UI_SCR_BACK) {
        if (S.after == AFTER_CLOSE) { int res = S.result; closeWith(res); return res; }
        if (S.op == OP_DELETE && S.msg == SVM_MSG_CORRUPT) busy(OP_DELETE, S.opSlot);   // испорченная запись удаляется, как у PS2
        else gotoSlots();
      }
      break;
  }
  return SVM_R_NONE;
}

// 309: под окном PSP — игра без нашего меню, затемнённая на DLG_DIM/255 (окно PSP рисует поверх свою раскладку — иконку, строки,
// Да/Нет; наши слоты под ним сливались бы с его текстом). Правило порта: так у игр PSP — сцена игры за системным окном
#define DLG_DIM 0xA8
static void dlgBackdrop(void) {
  typedef struct { unsigned color; short x, y, z, pad; } DimV;
  DimV *v = (DimV *)sceGuGetMemory(2 * sizeof(DimV));
  v[0].color = v[1].color = (unsigned)DLG_DIM << 24; v[0].x = 0; v[0].y = 0; v[1].x = 480; v[1].y = 272; v[0].z = v[1].z = 0;
  int tex = sceGuGetStatus(GU_TEXTURE_2D), dep = sceGuGetStatus(GU_DEPTH_TEST), bl = sceGuGetStatus(GU_BLEND);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  sceGuSetStatus(GU_TEXTURE_2D, tex); sceGuSetStatus(GU_DEPTH_TEST, dep); sceGuSetStatus(GU_BLEND, bl);
}
void svmDraw(float t) {
  { static int lastSt = -1;   // гейт: что написано на кнопках вопроса/сообщения при их появлении (tests/savegame/emu_check.py)
    if (S.st != lastSt && (S.st == ST_ASK || S.st == ST_CONFIRM || S.st == ST_NOTICE || S.st == ST_YESNO)) {
      int sc = S.st == ST_NOTICE ? SCR_NOTICE : SCR_PROMPT;
      logf_("SVROWS scr=%s r0=%s r1=%s\n", sc == SCR_NOTICE ? "notice" : "prompt", uiScrRowShown(sc, 0), uiScrRowShown(sc, 1)); }
    lastSt = S.st; }
  switch (S.st) {
    case ST_SLOTS: uiScrMenuFlags(2); uiScrDrawMenu(SCR_SLOTS, S.sel, t); break;   // 413: GameSlots — CGuiMenu флаги 2 (SLES 0x1b7344/0x1dd73c)
    case ST_ASK: case ST_CONFIRM: case ST_YESNO: uiScrDrawMenu(SCR_PROMPT, S.psel, t); break;
    case ST_NOTICE: uiScrDrawMenu(SCR_NOTICE, S.psel, t); break;
    case ST_BUSY: uiScrDrawMenu(SCR_MESSAGE, -1, t); S.drawn++; break;
    case ST_SYSDLG: dlgBackdrop(); S.drawn++; break;   // 309: фон окна PSP — игра (город паузы / гостиная) под затемнением
    default: break;
  }
}
