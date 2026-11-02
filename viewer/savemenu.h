// Сохранения на PSP (пакет «Сохранения», 28.09): системная утилита сохранений и экраны оригинала LoadGame / SaveGame.
// Формат записи и что сохраняется — viewer/savegame.h; экраны — пакеты fsave.shr / psave.shr (tools/save_export.py).
//
// Хранение как у настоящих игр PSP: sceUtilitySavedata в «тихом» режиме (AUTOLOAD / AUTOSAVE / AUTODELETE — свой интерфейс
// игры, как у большинства игр), папка ms0:/PSP/SAVEDATA/SLES51897<00..03>, файл DATA.BIN, PARAM.SFO (название игры, строка
// слота, пройденные миссии), ICON0.PNG — иконка из EBOOT.PBP. Запись подтверждается чтением (docs/missions_integration.md,
// проверка 10): не совпало — «Save failed!», прежний прогресс в памяти не трогается, игра идёт дальше.
// На время работы утилиты кадр стоит (на экране сообщение оригинала «Saving data...» / «Loading data...»), как у PS2.
//
// Порядок в main.c:
//   svmInit(путь EBOOT, включено) один раз после uiInit; svmScan() — на экране BootupLoad: четыре слота читаются, самое
//   свежее сохранение запоминается («Auto-loading most recent saved game», MSG_AUTO_LOADING_GAME_(PS2)) -> RESUME GAME
//   главное меню: LOAD GAME -> svmOpen(SVM_LOAD, ...) и цикл svmInput/svmDraw над гостиной; SVM_R_LOADED -> svmLoaded()
//   пауза: SAVE GAME -> svmOpen(SVM_SAVE, &текущая игра, 0, 0); QUIT GAME в свободной езде -> svmOpen(SVM_SAVE, .., 0, 1)
//   («Do you want to save your game?» YES/NO, MSG_CONFIRM_SAVE_BEFORE_QUIT); кадр: svmInput(hit), рисование svmDraw
// Выключено (служебные запуски без «flag saves» — чтобы прежние гейты меню шли как раньше): ни чтения, ни пунктов.
#ifndef SAVEMENU_H
#define SAVEMENU_H
#include "savegame.h"

enum { SVM_LOAD = 0, SVM_SAVE = 1 };
enum { SVM_R_NONE = 0, SVM_R_BACK, SVM_R_LOADED, SVM_R_SAVED, SVM_R_QUIT,
       SVM_R_YES, SVM_R_NO,   // g09b-menu: ответы svmAskOpen
       SVM_R_UNPAUSE };       // g09b-menu (P13-Д5): START на экране записи — закрыть паузу целиком

void svmInit(const char *ebootPath, int enabled);
int  svmOn(void);
void svmSetLog(void (*log)(const char *line));   // строки SV... в smoke.txt
void svmScan(void);                              // все слоты (утилита, кадр стоит); строка SVSCAN
int  svmLatest(SaveGame *out);                   // самое свежее целое сохранение -> 1 (RESUME GAME)
// экраны: mode SVM_LOAD (fsave.shr) / SVM_SAVE (psave.shr, game = что записать); confirmLoad — «Loading this data will cause
// current game progress to be lost» (игра уже шла); askQuit — сначала «Do you want to save your game?» (выход в меню). 0 = ок
int  svmOpen(int mode, const SaveGame *game, int confirmLoad, int askQuit);
// g09b-menu (сверка с исходником, 02.11.2026): askQuit — бит 0 «сохранить перед выходом?», бит 1 сначала «выйти?» (пауза; 3 — свободная езда, 2 — миссия).
// svmQuitAsks/svmQuitAsk — что передать в savePauseOpen для QUIT GAME; svmPauseEnter — вход в паузу (флаг «записано в этой паузе» сбрасывается);
// svmAskOpen(SVM_ASK_NEWGAME) — вопрос «Restart anyway?» главного меню: svmInput -> SVM_R_YES / SVM_R_NO
enum { SVM_ASK_QUIT = 1, SVM_ASK_NEWGAME = 2, SVM_ASK_RESTART = 3, SVM_ASK_ABORT = 4, SVM_ASK_STARTMSN = 5 };   // 5 — №1105 (сверка g11b-collect): «Would you like to load this mission?» из галереи миссий SCRAP BOOK   // 3, 4 — g09a-hud круг 5: svmOpen(SVM_SAVE, .., askQuit 4 / 8) — вопрос Restart / Abort Mission
int  svmAskOpen(int kind);
int  svmQuitAsks(void);
int  svmQuitAsk(int freeRoam);
void svmPauseEnter(void);
int  svmActive(void);
int  svmInput(unsigned hit);                     // раз в кадр (и без нажатий): SVM_R_* (не NONE — экраны закрыты)
void svmDraw(float t);                           // в открытый список GE, 2D (состояние GE — как uiScrDrawMenu)
const SaveGame *svmLoaded(void);                 // после SVM_R_LOADED
void svmNoWrite(int on);                         // служебное (гейт, flag svnowrite): запись не выполняется — прогресс должен слететь
// Правка 274 (04.10): на PSP утилита отвергала запись (0x80110388 SAVE_PARAM: запрос 1536 Б с нулевым ключом и secureVersion 0 —
// факт железа pspautotests secureversion). Запись, чтение и удаление идут цепочкой вариантов параметров (savemenu.c SVV_*):
// первый прежний; при отказе параметров — следующий, строка «SVIO try=…» в журнал и чёрный ящик; удачный — первым до конца сессии.
void svmFailFirst(int n);                        // служебное (flag savefail 388): std отвергается с кодом 0x80110388, удачный забыт
void svmOldPath(int on);                         // служебное (flag saveold 1): только прежний путь, без запасных (красный контроль)
void svmBlock(int on);                           // автотур (flag nosave): записи не пишутся (утилита и файл), чтение работает
void svmFileOff(int on);                         // служебное (flag savefileoff 1): без страховки файлом save<NN>.bin (красный контроль)
void svmUtilOff(int on);                         // служебное (flag saveutiloff 1): утилита отвергает все варианты (имитация худшего железа)
void svmMatrix(void);                            // замер всех вариантов на тестовой записи T8: строки SVMATRIX + savematrix.txt;
                                                 // и без «flag saves»; на PSP — тур TEST 8 (tests/save/psp_test8_save.txt) или пустой файл «savematrix» рядом с EBOOT (один раз при запуске)
// Правка 309 (04.10, Роб: «на PSP оно тебя переводит в контекстное меню PSP и ты там подтверждаешь создание этого файла»): запись
// слота — системным окном утилиты сохранения PSP (режим SAVE: «сохранить?» / «перезаписать?», полоса записи, «сохранение завершено»),
// загрузка слота с записью утилиты — окном LOAD; слоты оригинала остаются, окно подтверждает выбранный (savemenu.c, ST_SYSDLG).
// Окно — в обычном запуске; служебный (scenario.txt рядом с EBOOT) — тихо, как раньше. Окно не справилось — прежний тихий путь
void svmDialog(int on);                          // служебное: flag svdialog — окно и в служебном запуске; flag svquiet — прежний тихий путь (красный)
void svmPickEmpty(int on);                       // служебное: flag svpickempty — запись только в пустой слот, пустого нет — отмена (ENDSAVE skip)
void svmDialogFail(int on);                      // служебное: flag svdlgfail — окно отвергает все варианты (живёт ли тихий запасной путь)
unsigned svmDialogFrame(float dt, int start);    // в main.c: раз в кадр, пока окно на экране (start — первый кадр окна), — сценарий смоука
                                                 // (press/wait/shot) и кнопки сценария в контроллер эмулятора (строка TWPAD); -> кнопки (на PSP 0)
// Правка Роба 335 (05.10, голосом): «сохранение разработчика — самый нижний слот, с сегодняшней датой, понятно, что dev save;
// максимальное прохождение — всё открыто, все уровни». Пустой файл «devsave» рядом с EBOOT: при запуске нижний слот (4-й) —
// если он пуст или в нём уже DEV — пишется записью разработчика тихим путём (утилита цепочкой + файл save03.bin, проверка
// чтением); твоя обычная запись в слоте 4 не трогается (в файле слово «force» — перезаписать и её). Запись: seq 0 (обычные
// — всегда с 1, поэтому RESUME GAME её не берёт, пока есть другие), монеты, купленное всех уровней; уровень выбирается при
// загрузке: LOAD GAME -> слот «DEV SAVE» -> ◄ ► — уровни, чьи наборы лежат рядом с EBOOT. Миссии — main.c (GAME_MSN_STORY_DEV)
#define SVM_DEV_SLOT (SV_SLOTS - 1)
#define SVM_DEV_COINS 99999
int  svmIsDev(const SaveGame *g);                // запись разработчика: seq 0 и есть дата записи (в памяти игры записи без даты — не DEV)
void svmDevBoot(int (*levelAvail)(int level));   // после svmScan: флаг-файл devsave -> слот 4; levelAvail — набор уровня рядом с EBOOT
void svmAllBoot(void);                           // служебное (сборка с -DSVM_ALLSAVE): флаг-файл allsave + allsave.bin рядом с EBOOT -> запись «пройдено всё» в слот; в обычной игре пусто
#endif
