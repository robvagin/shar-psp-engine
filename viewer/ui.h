// UI: the original SHAR Scrooby interface (HUD, pause menu, main menu) re-laid out for the
// 480x272 PSP screen and drawn by the GE in 2D.  Data comes from build/ui.shr (ui_l<N>.shr for
// level N: the radar map is per level), written by tools/ui_export.py from the owner's disc
// (art/frontend/scrooby/*.p3d; radar map = resource/pure3d/l<N>hudmap.p3d, engine x/z metres).
//
// Widescreen rules (applied offline by the exporter, nothing is scaled at run time):
//   one uniform scale 272/480 for every element (no stretching, circles stay round), each
//   element group anchored to the left/centre/right of the 16:9 screen, full-width backgrounds
//   extended, the TV frame widened by a 3-slice, fonts re-rasterised at their pixel size.
//
// Usage (once):  sceGuInit ... sceGuDisplay(GU_TRUE); if (uiInit("ui.shr") == 0) uiUseVram(off, max);
// Per frame:     uiSetCoins(n); uiSetSpeed(kmh); uiSetRadar(x, z, heading);
// Mission only: uiSetDamage(objectiveProgress); law state: uiSetHitAndRun(heat, pursuit, elapsed).
//                act = uiPauseInput(hit);   (after uiPause(1) on START)
//                sceGuStart(...); <3D scene>; uiDraw(); <re-establish GU state>; [hudDraw()]; sceGuFinish();
// Main menu:     uiDrawMenu(UI_MENU_MAIN, sel) (vertical list, items 0..uiMenuCount-1, see uiMenuLabel);
//                item = uiMenuInput(UI_MENU_MAIN, &sel, hit)  (main.c: menuRun, item 12a of the queue).
// Not thread-safe: call every ui* function from the thread that builds the display list.
//
// Pack (UIP1 = build/ui.shr, little endian, all offsets from the file start, loaded whole into one 64-byte
// aligned block; textures are used in place, so the block must stay allocated):
//   header 128 B  (see UiHdr in ui.c)  -> textures (T8 CLUT8 pages / T4 font + radar map,
//   all pre-swizzled), 64 B draw items per screen, fonts (256 glyphs, hinted integer metrics),
//   sprite fonts, radar icon table, image slots, polygon points, Latin-1 strings.
//
// ---------------------------------------------------------------------------------------
// GU STATE LEFT CHANGED BY uiDrawHud() / uiDrawMenu() / uiText()  (only what they touch):
//   GU_TEXTURE_2D      enabled or disabled (whatever the last item needed)
//   texture mode       sceGuTexMode(GU_PSM_T8 or GU_PSM_T4, 0, 0, 1)   (swizzled, no mipmaps)
//   texture level 0    sceGuTexImage(0, ..., <ui page>)                 (levels 1..7 untouched)
//   CLUT               sceGuClutMode(GU_PSM_8888, 0, 0xff or 0x0f, 0) + sceGuClutLoad(<ui clut>)
//                      -> the caller MUST re-send its own CLUT mode and CLUT before the next
//                         CLUT draw (main.c: resetState() + sceGuClutMode(GU_PSM_8888,0,0xff,0)).
//   texture filter     sceGuTexFilter(GU_NEAREST or GU_LINEAR, same)
//   texture function   sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA)   (same values as main.c)
//   texture wrap       sceGuTexWrap(GU_CLAMP, GU_CLAMP)              (main.c uses GU_REPEAT!)
//   GU_BLEND           ENABLED, sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0)
//   GU_ALPHA_TEST      DISABLED   (sceGuAlphaFunc NOT touched)
//   GU_DEPTH_TEST      DISABLED   (sceGuDepthFunc NOT touched)
//   depth write        sceGuDepthMask(GU_TRUE) = writes OFF
//   GU_FOG             DISABLED
//   shade model        GU_SMOOTH  (gradient rectangles, radar cone)
//   vertex type        TRANSFORM_2D formats (re-sent by every draw)
// NOT touched: tex scale/offset (ignored in 2D), tex level mode, alpha/depth func, cull,
//   clip planes, scissor, viewport/offset, matrices, stencil, logic op, dither, clear colour.
// REQUIRES: GU_CULL_FACE disabled (as in main.c) - the radar fan is not wound for culling.
// ---------------------------------------------------------------------------------------
//
// Cost (see report): the HUD is ~30-60 visible items -> ~25-45 sceGuDrawArray calls, 5-8
// texture binds, ~200-600 vertices; CPU well under 0.2 ms, GE fill ~40-70k blended pixels.
// Memory: the pack (~555 KB, loaded once) + nothing per frame (vertices live in the list).
#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

// screens in the pack
enum { UI_SCREEN_HUD = 0, UI_SCREEN_PAUSE = 1, UI_SCREEN_MAIN = 2, UI_SCREEN_MISSIONLOAD = 3, UI_SCREEN_MISSIONOVER = 4,
       UI_SCREEN_LETTERBOX = 5,   // LETTERBOX — правка 40: подписи экрана разговора (LetterBoxButtons.pag)
       UI_SCREEN_PAUSEMSN = 6,    // пауза во время миссии (PauseMission.scr): CONTINUE, RESTART / CANCEL MISSION, OPTIONS, QUIT GAME
       UI_SCREEN_LEVELSTATS = 7,  // LEVEL PROGRESS паузы (LevelStats.scr) и конец уровня (LevelEnd.scr): LevelStats.pag
       UI_SCREEN_TUTORIAL = 8,    // подсказки обучения Барта (Tutorial.scr): SmallBoard, Tutorial.pag, Continue.pag
       UI_SCREEN_MESSAGE = 9,     // правка 122: окно сообщения (Message.scr) — «Congratulations! You've unlocked …», Continue / ✕ Accept
       UI_SCREEN_COUNT = 10,
       UI_SCREEN_MAX = 12 };  // MAX — экраны пакета и второго пакета (uiScr*: загрузка, меню оригинала)
enum { UI_MENU_MISSIONOVER = UI_SCREEN_MISSIONOVER };   // YES / NO (SKIP скрыт)
enum { UI_MENU_PAUSE = UI_SCREEN_PAUSE, UI_MENU_MAIN = UI_SCREEN_MAIN };

// visibility tags (same order as TAGS in tools/ui_export.py)
enum {
  UI_T_ALWAYS = 0, UI_T_RADAR, UI_T_SPEED, UI_T_HNR_METER, UI_T_COINS, UI_T_OBJECTIVE, UI_T_TIMER,
  UI_T_PARTIME, UI_T_LAP, UI_T_POSITION, UI_T_DAMAGE, UI_T_PROXIMITY, UI_T_HNR_TICKET, UI_T_ACTION,
  UI_T_CARDS, UI_T_CARD_TEXT, UI_T_ITEMS_COMPLETE, UI_T_WASPS, UI_T_COUNTDOWN, UI_T_STAGE_COMPLETE,
  UI_T_MISSION_COMPLETE, UI_T_MISSION_FAILED, UI_T_ITEM_DROPPED, UI_T_TUTORIAL, UI_T_COLLECTIBLES,
  UI_T_PRESS_START, UI_T_LEVEL_NAME, UI_T_HNR_FULL, UI_T_LOADING,
  UI_T_GAMBLE, UI_T_STREETRACE,   // брифинг миссии (MissionLoad.pag): сведения о ставке / итог уличных гонок
  UI_T_SKIP,                      // провал (MissionOver.pag): пункт SKIP — пропуска миссии в порте нет, скрыт
  UI_T_COUNT
};

// dynamic fields (same order as FIELDS in tools/ui_export.py)
enum {
  UI_F_NONE = 0,
  UI_F_SPEED,         // number, shown as is (km/h or mph: caller converts)
  UI_F_SPEED_UNIT,    // 0 "kmh", 1 "mph"
  UI_F_COINS,         // number
  UI_F_TIMER,         // milliseconds -> "M:SS"
  UI_F_PARTIME,       // milliseconds -> "M:SS"
  UI_F_LAP, UI_F_LAPS,              // "LAP/LAPS"
  UI_F_POSITION,      // race position 1.. ("2" + "nd")
  UI_F_DAMAGE,        // 0..1000 bar fill
  UI_F_PROXIMITY,     // 0..1000 bar fill
  UI_F_HNR,           // 0..1000 hit & run meter, continuous clockwise fill from twelve o'clock
  UI_F_HNR_LIGHT,     // 0..2 frame of the "HIT & RUN" siren label
  UI_F_OBJECTIVE,     // mission objective string index (0 = MISSION_OBJECTIVE_00 ...)
  UI_F_OBJICON,       // objective icon 0..31 (q*.png, alphabetical: qannounc, qapu, qbart ...)
  UI_F_CARDS, UI_F_CARDS_TOTAL,     // "3/7"
  UI_F_COUNTDOWN,     // number
  UI_F_HNR_TEXT,      // 0 "Hit & Run!", 1 "BUSTED!"
  UI_F_CARD_TITLE,    // collector card name index
  UI_F_COLLECTED, UI_F_COLLECT_TOTAL,
  UI_F_MENU_ITEM,     // main menu: set by uiDrawMenu
  UI_F_LEVEL,         // main menu level name index; экран загрузки: газета уровня 0..6
  UI_F_PROGRESS,      // экраны загрузки: доля загрузки 0..1000 (фитиль, uiScrDraw)
  UI_F_ANIM, UI_F_FIRE, UI_F_DOTS,   // кадры анимации / огня / точки LOADING (по часам uiScrDraw)
  UI_F_MSN_TITLE,     // брифинг миссии: название (uiSetBriefing)
  UI_F_MSN_INFO,      // брифинг миссии: описание задания (uiSetBriefing), переносится по ширине
  UI_F_MSN_FAIL,      // провал миссии: причина (uiSetMissionOver)
  UI_F_MSN_HINT,      // провал миссии: подсказка
  UI_F_ROW0, UI_F_ROW11 = UI_F_ROW0 + 11,   // меню оригинала (menu.shr / pmenu.shr): значение строки k (uiScrSetRow)
  UI_F_STAT_MODE, UI_F_STAT_PCT,            // LevelStats.pag: 0 LEVEL PROGRESS / 1 Level Complete; процент уровня
  UI_F_STAT0,                               // пары «сделано, всего»: гэги, осы, машины, одежда, карточки, гонки, бонусы, сюжет
  UI_F_VC0 = UI_F_STAT0 + 16,               // VIEW CARDS (pextra.shr): карточка k уровня собрана (1) / нет (0), k = 0..6
  UI_F_MS0 = UI_F_VC0 + 7,                  // MISSION SELECT (pextra.shr): строка k — 0 dash (закрыта), 1 cross (текущая), 2 check (пройдена)
  UI_F_MSGBOX = UI_F_MS0 + 7,               // правка 122: текст окна сообщения (uiMessageBox)
  UI_F_MSN_DONE,                            // правка 122: вспышка конца миссии (спрайтовый шрифт Hud.pag MissionComplete), uiMissionDoneText
  UI_F_ACTION_TEXT,                         // правка 206: подпись рядом со значком △ (uiActionText)
  UI_F_MS_LEVEL,                            // правка 348: MISSION SELECT — уровень на экране 0..6 (строка LEVEL N и названия миссий уровня)
  UI_F_MS_LARROW, UI_F_MS_RARROW,           // правка 348: шарики за стрелками планки уровня — 0 серый, 1 красный (нажата ◄ / ►)
  UI_F_RACERS,                              // аудит 09.10: число участников гонки — «1/2» в поле места (uiPosText)
  UI_F_PLACE,                               // №598: код клетки на паузе («L1 K9-7»), строка ставится uiPlaceCode, рисуется элементом PlaceCode пакета (шрифт и место — из ui.shr)
  UI_F_WAGER_FEE, UI_F_WAGER_PAR, UI_F_WAGER_PAY,   // Роб 17.10 «добивай»: карточка гонки на ставку — ставка (монеты), время, которое надо побить (мс), выплата (монеты)
  UI_F_COUNT = UI_F_WAGER_PAY + 1
};

// radar icon types (Scrooby HudMap icon groups)
enum {
  UI_R_PLAYERCAR = 0, UI_R_BONUS, UI_R_WAGER, UI_R_STREETRACE, UI_R_PHONE, UI_R_PURCHASE,
  UI_R_CHECKER, UI_R_WAYPOINT, UI_R_HARASS, UI_R_AICAR, UI_R_MISSION, UI_R_COLLECTIBLE, UI_R_COUNT
};
#define UI_RADAR_MAX 64

// Load the pack (whole file, one allocation), write the dcache back.  0 = ok, <0 = error.
// Emits no GU commands.  Default tags: HUD radar+speed+H&R meter+coins; pause coins.
int uiInit(const char *path);
void uiShutdown(void);
// One immutable screen pack prepared under level loading, released on UI/level change
int uiScrWarm(const char *path);
int uiScrWarmReady(const char *path);
void uiScrWarmClear(void);
// Optional: copy the textures the in-game HUD samples (sprite page, fonts, radar map: ~0.4 MB)
// to VRAM at sceGeEdramGetAddr()+vramOffset (16-byte aligned, must not overlap frame/depth
// buffers).  Textures that do not fit in maxBytes stay in RAM.  Returns bytes used.
int uiUseVram(unsigned vramOffset, unsigned maxBytes);

// ---- game-facing API (stage 7) ---------------------------------------------------------
void uiSetCoins(int n);                            // top-right coin count
void uiSetDamage(float d01);                       // mission objective bar, 0 hides it
// Mission damage is not the player's car health and must not be updated from car.damage.
void uiSetHitAndRun(float heat01, int pursuit, float elapsed); // caller supplies actual law state
void uiSetSpeed(float kmh);                        // "88 kmh" above the radar
void uiSetRadar(float x, float z, float heading);  // engine metres + main.c heading; map rotates, car arrow up
// Message box (objective box with the mission icon).  text: Latin-1, wraps to 2 lines;
// icon 0..31 = UI_F_OBJICON order; seconds <= 0 = until replaced.  NULL/"" hides it.
void uiMessage(const char *text, int icon, float seconds);
void uiObjectiveCollapse(int on);   // 1 (по умолчанию): задание через 3.5 с сворачивается — значок в угол вверху слева (как PS2); 0 — рамка всё время
extern int icon993Old;                       // №993: 1 — значок дома №623 вне задания, как было (красный контроль)
int uiHomeShown(void);                       // №993: диагностика — в углу сейчас значок дома №623
const void *uiObjPix(void);                  // №993: указатель на пиксели значка угла (диагностика)
void uiObjDebug(int *icon, int *hasPix, const char **txt, const char **last);   // №958б: диагностика угла (flag msnhud)
void uiObjective(int index, int icon);             // original MISSION_OBJECTIVE_<index> string (-1 hides)
// Значок задания из данных миссии (msnicons, viewer/msnart.c): icon = UI_OBJICON_EXTERNAL в uiMessage/uiObjective рисует
// эту картинку (RGBA 8888, текстура tw x th, картинка w x h в её левом верхнем углу) в рамке ObjectiveIcon, по центру с
// сохранением пропорций; icon < 0 — рамка без значка. pix = NULL — картинки нет.
#define UI_OBJICON_EXTERNAL 1000
void uiSetObjIconImage(const void *pix, int tw, int th, int w, int h);
void uiSetObjIconTop(int top);   // Роб 17.10: верх значка в углу из экспорта (строка картинки), -1 — считать по альфе
// Брифинг миссии (Scrooby MissionLoad.scr, экран UI_SCREEN_MISSIONLOAD): название и описание (Latin-1, строки библии),
// картинка задания SetPresentationBitmap (RGBA 8888, как uiSetObjIconImage; NULL — пусто). uiDrawBriefing — вместо uiDraw
void uiSetBriefing(const char *title, const char *info, const void *pix, int tw, int th, int w, int h);
void uiSetWager(int on);   // карточка гонки на ставку: картинка задания левее, справа строки ставки (UI_T_GAMBLE, поля UI_F_WAGER_*)
void uiBriefingBonus(int on);   // 1 — карточка бонусной миссии (бордовая, как PS2 23:56)
void uiDrawBriefing(void);
void uiLastBubbleGrow(int *gw, int *gh);   // №619
int  uiBriefPicSettled(int frames);   // №612: картинка карточки миссии уже frames кадров подряд готова (до этого карточку не показываем)
void uiBriefAnim(int on);        // №512: 1 — карточка миссии появилась (въезд), 0 — нажат Continue (выезд)
void uiBriefHold(void);        // №994: брифинг открыт, карточка ещё не готова: рисуется только LOADING, въезд стартует следующим uiBriefAnim(1) (CARD994_OLD / flag card994 0 — без этого)
void uiCard994Set(int old);       // №994: 1 — карточка как в TEST70 (стоит на месте до второго запуска въезда)
void uiBriefExitGo(void);         // №633: конец первого кадра после Continue — карточка начинает выезд (до этого стоит)
int  uiBriefExitBusy(void);      // 1 — карточка ещё уезжает (после Continue): main рисует её поверх мира (uiDrawBriefing)
// Провал миссии (Scrooby MissionOver.scr, экран UI_SCREEN_MISSIONOVER): FAILED, причина, подсказка, «Retry Mission?»,
// YES / NO — курсор sel (uiMenuInput(UI_MENU_MISSIONOVER, ...)). uiDrawMissionOver — вместо uiDraw
void uiSetMissionOver(const char *reason, const char *hint);
void uiDrawMissionOver(int sel);
void uiFailynPulseOld(int on);   // №572: 1 — прежнее поведение (без пульса YES/NO на экране провала)
// Экран разговора и камеры старта (правка 40; Scrooby LetterBox.scr): чёрные полосы сверху и снизу, open 0..1 — насколько
// выехали (в данных полосы — полигоны на пол-экрана LetterBox.pag, выезд задаёт код оригинала; толщина — ПРАВИЛО ПОРТА по
// записи PS2: 14.5% высоты кадра, как у 4:3 оригинала — вертикаль кадра та же, по ширине Hor+). prompt — подписи
// LetterBoxButtons.pag в нижней полосе: UI_LB_SKIP «✕ SKIP» (разговор), UI_LB_START «✕ Start Mission / △ Cancel»
// (разговор перед миссией, запись PS2 1:25–1:40), 0 — без подписей (облёт старта). Рисовать после 3D, до/вместо HUD
enum { UI_LB_NONE = 0, UI_LB_SKIP = 1, UI_LB_START = 2 };
enum { UI_T_LB_SKIP = UI_T_SKIP, UI_T_LB_START = UI_T_STREETRACE };   // метки подписей экрана LETTERBOX (tools/ui_export.py)
// главное меню: метка строки меню и подсказки (MainMenu.pag, Accept3.pag; tools/ui_export.py). Правка Роба 49: пока камера
// летит к предмету (перелёт после NEW GAME и других пунктов), рамка-телевизор TVFrame.pag остаётся на экране, как в
// оригинале, а строка меню, подсказка и название уровня скрыты — uiMenuFlight(1)
enum { UI_T_MAIN_ROW = UI_T_SKIP };
void uiMenuFlight(int on);
// LEVEL PROGRESS / конец уровня (LevelStats.pag, 29.09): метки экрана UI_SCREEN_LEVELSTATS — фон паузы и Back (пауза) или
// Accept и «Use Mission Select to return to this level» (конец уровня, LevelEnd.scr)
enum { UI_T_LS_PAUSE = UI_T_SKIP, UI_T_LS_END = UI_T_STREETRACE };
// экраны паузы pextra.shr (tools/menu_export.py): 0 — VIEW CARDS (сетка 7 карточек с рамкой выбора / выбранная крупно с текстами),
// 1 — MISSION SELECT (MissionSelect.pag)
enum { UI_PX_VIEWCARDS = 0, UI_PX_MISSIONSELECT = 1 };
enum { UI_T_VC_GRID = UI_T_SKIP, UI_T_VC_ZOOM = UI_T_STREETRACE };
// Подсказка обучения (Tutorial.scr, 29.09): msg — номер строки TUTORIAL_NNN библии (PS2-вариант), текст — спрайтовым шрифтом
// TutorialMessage с кнопками PS2 (знаки-заместители + £ ® ¿ µ § © @, tools/ui_export.py BUTTON_CHARS), строки по «\»;
// cont — показать «Continue» (Continue.pag). 0 — экрана нет в пакете
enum { UI_T_TUT_CONTINUE = UI_T_STREETRACE };
int  uiDrawTutorial(int msg, int cont);
void uiSubFade(float a);                       // №639: прозрачность всего рисуемого экрана (кроссфейд подэкранов паузы), 1 — как есть
void uiTutorialScale(float k);                 // №516: масштаб всей подсказки из центра экрана (ui_anim.h 2г), 1 — как есть; ставится на один вызов uiDrawTutorial
void uiTutLayoutOld(int on);
void uiMsgHardBreaks(int on);   // правка 267: 1 — в нижнем пузыре прежние жёсткие переносы (красный контроль)   // правка 267: 1 — прежняя раскладка подсказки (служебно, красный контроль гейта)
void uiMissionDoneText(const char *s);      // правка 122: текст вспышки MISSION COMPLETE (\n — вторая строка); "" — из пакета
int  uiDrawMessageBox(const char *text);   // правка 122: окно сообщения поверх игры (Message.scr); 0 — экрана нет в пакете
int  uiTutorialCount(void);                      // строк TUTORIAL_NNN в пакете (0 — старый ui.shr)
const char *uiTutorialText(int msg);
enum { UI_LS_GAGS, UI_LS_WASPS, UI_LS_CARS, UI_LS_SKINS, UI_LS_CARDS, UI_LS_RACES, UI_LS_BONUS, UI_LS_STORY, UI_LS_COUNT };
typedef struct { int done[UI_LS_COUNT], total[UI_LS_COUNT]; } UiLevelStats;
// процент уровня (строка Level Complete) — правило порта: всё сделанное / всё, что есть, по восьми строкам (формулы в данных нет)
int  uiLevelStatsPct(const UiLevelStats *s);
void uiDrawLevelStats(const UiLevelStats *s, int levelEnd);   // 0 — LEVEL PROGRESS паузы, 1 — конец уровня
#define UI_LB_BAR 39.5f   // px кадра 272: 0.145 высоты
void uiDrawLetterbox(float open, int prompt);
// правка 206: подпись рядом со значком действия △ (что делает действие: «Get Into Car», «Talk To Marge»); NULL / "" — без подписи
void uiActionText(const char *s);
void uiPlaceCode(const char *s);   // №598: текст кода клетки для элемента PlaceCode (поле UI_F_PLACE)
int  uiHasPlaceCode(void);          // №598: в пакете есть элемент PlaceCode (старый ui.shr — нет: код рисует uiText)
// Оверлеи миссии (таймер, пар-время, круги, место, счётчик) — стопкой без наложения (ui.c, правило у OV_ORDER);
// uiOverlayStack(0) — отрицательный контроль гейта. uiOverlayBoxes: итоговые рамки видимых оверлеев последнего кадра HUD.
void uiOverlayStack(int on);
int  uiOverlayBoxes(int *tags, int (*rect)[4], int max);
// Текст задания в рамке HUD: 1 — влезает (строк после переноса <= помещается, самая широкая строка <= ширины рамки)
int  uiTextFit(const char *s, int *lines, int *maxLines, int *widest, int *boxW);
// то же для любого переносимого текста экрана с полем field (провал: UI_F_MSN_FAIL / UI_F_MSN_HINT)
int  uiTextFitField(int screen, int field, const char *s, int *lines, int *maxLines, int *widest, int *boxW);
// Pause (PauseSunday.pag + RESET CAR).  uiPauseInput takes the just-pressed buttons:
// UP/DOWN move (disabled items skipped), X activates, START/O = continue.  Returns UI_P_*.
// CONTINUE and RESET CAR close the pause themselves; the caller acts on the returned item.
enum { UI_P_NONE = -1, UI_P_CONTINUE = 0, UI_P_RESET_CAR, UI_P_MISSION_SELECT, UI_P_LEVEL_PROGRESS,
       UI_P_VIEW_CARDS, UI_P_OPTIONS, UI_P_SAVE_GAME, UI_P_QUIT_GAME,
       UI_P_RESTART_MISSION, UI_P_ABORT_MISSION,   // только пауза миссии (PauseMission.pag); закрывают паузу сами
       UI_P_COUNT };
// пункты паузы миссии по порядку строк PauseMission.pag (сверху вниз) — для uiMenuEnable(UI_SCREEN_PAUSEMSN, …)
enum { UI_PM_CONTINUE = 0, UI_PM_RESTART, UI_PM_ABORT, UI_PM_OPTIONS, UI_PM_QUIT };
// 1 — следующая uiPause(1) открывает паузу миссии (PauseMission.scr), 0 — паузу свободной езды (PauseSunday.scr + RESET CAR);
// нет экрана в пакете (старый ui.shr) — всегда пауза свободной езды. uiPauseInput возвращает UI_P_* обеих
void uiPauseMission(int on);
int  uiPauseIsMission(void);
void uiPause(int on);
void uiPauseMenuHide(int on);                     // 1: пункты паузы не рисуются (подменю рисует свой список в той же рамке)
void uiPauseSlow(float k);        // служебно (smoke, flag pauseslow K): анимация паузы медленнее в K раз
int  uiPaused(void);
void uiPadRepeatLR(int on);   // №1057: 1 — автоповтор и у L R (короче пауза и шаг), на один кадр; звать перед uiPadRepeat
unsigned uiPadRepeat(unsigned buttons, unsigned hit);   // правка 202/№706: автоповтор ↑ ↓ ◄ ► (0,4 с, затем 0,12 с) в списках
void uiPadRepeatOld(int on);                            // №706: 1 — прежнее (только ↑/↓, 0,35/0,09 с)
void uiBubble992Set(int old);                         // №992: 1 — появление пузыря как в TEST70 (8 шагов, текст после рамки); 0 — короче, содержимое вместе с рамкой (flag bubble992 0 / файл bubble992old)
void uiRadarIndoor(int on);                           // №1035: 1 — игрок в помещении: радар без карты, значков, клина и фона круга (остаётся рамка-кольцо)
void uiRadar1035Set(int old);                         // №1035: 1 — радар в комнате как в TEST72
void uiMenuVec1012Set(int old);                        // №1012: 1 — выделенный пункт меню как в TEST71 (только жёлтый текст); 0 — в Cartoon векторная плашка с контуром и бликом
void uiTextLate1004Set(int old);                     // №1004: 1 — текст пузыря растёт с рамкой, как в TEST71 (flag textlate1004 0 / файл textlate1004old)
extern int uiPadRepeated;                                // 1 — в hit шаг повтора (у края списка не по кругу)
int  uiPauseSelected(void);
int  uiPauseInput(unsigned buttonsJustPressed);
// Not-yet-implemented menu items are drawn dimmed and skipped (pause: MISSION SELECT, LEVEL
// PROGRESS, VIEW CARDS, OPTIONS, SAVE GAME are off by default; main menu: main.c leaves only NEW GAME on).
void uiMenuEnable(int menu, int item, int on);
int  uiMenuEnabled(int menu, int item);
void uiMenuHide(int menu, int item, int on);       // правка Роба 137: 1 — строка скрыта (не рисуется, курсор пропускает; коды UI_P_* те же)
int  uiMenuHidden(int menu, int item);
// Any menu with the cursor kept by the caller (main menu: draw with uiDrawMenu(UI_MENU_MAIN, *sel)):
// UP/DOWN move *sel (disabled items skipped, wraps), X on an enabled item returns it, otherwise -1.
int  uiMenuInput(int menu, int *sel, unsigned buttonsJustPressed);
// Items of a menu as drawn, top to bottom (by the y of each item's text line): out[] = item numbers, returns the count.
int  uiMenuOrder(int menu, int *out, int max);
// Draw the HUD, or the pause screen while paused (appends to the open GU list, 2D, no depth).
void uiDraw(void);
extern int uiProfOn;   // тест-апка: разбивка кадра HUD по типам элементов и тегам
int uiProfTake(char *out, int n);   // строка UIT (среднее мс на кадр HUD за прошедшее время) или 0
// ----------------------------------------------------------------------------------------

void uiCountdownText(const char *text);             // №630: слово отсчёта в центре экрана (NULL — число UI_F_COUNTDOWN)
void uiHudSet(int field, int value);              // UI_F_*
int  uiHudGet(int field);
void uiShow(int screen, int tag, int on);          // UI_T_* per screen
void uiMenuBackdrop(int on);                       // 0: main menu without the full-screen placeholder of the 3D room (frontend3d draws it)
static inline void uiHudShow(int tag, int on) { uiShow(UI_SCREEN_HUD, tag, on); }

// Radar: heading-up.  x/z in world metres (engine coords), heading as in main.c
// (forward = (sin h, cos h)).  range = metres from the centre to the rim (default 150).
void uiRadarSetPlayer(float x, float z, float heading);
void uiRadarSetRange(float metres);
void uiRadarClear(void);
void uiRadarAdd(int type, float x, float z);       // UI_R_*; за краем у обода — только цели (правка 50), прочие скрыты
void uiRadarAddH(int type, float x, float z, float heading);   // правка 122: то же со стрелкой по курсу (машины миссии)
void uiRadarHarassOld(int on);                    // №241: 1 — стрелка полиции за краем радара не показывается (прежнее)
void uiRadarConeOff(int on);                       // отрицательный контроль правки 50: клин вверх всегда, все значки у обода
void uiBriefMixedCase(int on);                     // отрицательный контроль правки 52: название брифинга не прописными
void uiCoin3D(int on);                             // правка 79: счёт монет — плоскую иконку не рисовать (3D-монета — main.c)
int  uiCoinIcon(float *cx, float *cy, float *h, float *alpha);   // место иконки монеты этого кадра; 0 — счёта не было
const char *uiBriefTitle(void);                    // служебно: название брифинга, как нарисовано
void uiRadarDebug(int *drawn, int *rim, int *rimOther, int *cone, float *coneAng, float *tx, float *tz);   // служебно: итог кадра радара
int uiRadarDrawn(int type);                      // реально нарисованные значки типа в последнем HUD-кадре
void uiMarkerView(const float *eye, const float *forward, const float *right, const float *up, float focal);
int uiMarkerSmall(int type, const float *center, const float *half); // №392: родной значок 16 px, если 3D меньше 14 px; 0 — оставить 3D
int uiMarkerDrawn(void);                         // служебно: плоские маркеры текущего кадра

// Append to the CURRENTLY OPEN GU list.
void uiDrawHud(void);
void uiDrawMenu(int menu, int selected);           // UI_MENU_PAUSE (item 0 = CONTINUE) / UI_MENU_MAIN
int  uiMenuCount(int menu);                        // pause: 8 (UI_P_*), main: 6 (LOAD GAME ... RESUME GAME)
const char *uiMenuLabel(int menu, int item);

// Plain text with a pack font (0 = Tt2001m 9 px, 1 = boulder 11 px, 2 = boulder 16 px; order
// as in the pack, see uiFontName).  Returns the advance width in pixels.
int  uiText(int font, int x, int y, unsigned abgr, const char *s);
const char *uiFontName(int font);
int uiFontFind(const char *name);                 // номер шрифта по грани ("Tt2001m__14"; её вариант для миссий «*k» — первым), 0 — нет

// ---- экраны вокруг игры (tools/load_export.py): отдельный пакет UIP1 только на время показа ----------------
// boot.shr: лицензия, BootupLoad (лицензия с закрытым текстом + LOADING), титульный экран Splash;
// load.shr: загрузка уровня Loading (газета, фитиль к динамиту) и загрузка меню LoadingFE (фитиль к бомбе, анимация).
// Основной ui.shr не трогается; можно открыть и до uiInit. Состояние GE — как у uiDrawMenu.
enum { UI_BOOT_LICENSE = 0, UI_BOOT_LOAD = 1, UI_BOOT_SPLASH = 2, UI_BOOT_TVMOVIE = 3,   // 3 — рамка-телевизор роликов (правка Роба 32)
       UI_BOOT_DEMO = 4 };   // 4 — «- DEMO -» поверх ролика простоя (frontend.p3d Demo.scr, правка Роба 33)
enum { UI_LOAD_LEVEL = 0, UI_LOAD_FE = 1 };
// №833: крутящиеся слои лучей (экран загрузки, будка, гардероб) на следующий вызов uiScrDraw — векторными клиньями (UI_RAYS_LOAD / UI_RAYS_SHOP), 0 — картинкой
enum { UI_RAYS_TEX = 0, UI_RAYS_LOAD = 1, UI_RAYS_SHOP = 2 };
void uiScrRays(int style);
int  uiScrOpen(const char *path);                  // 0 = ок; файл целиком одной аллокацией (как uiInit)
void uiScrClose(void);                             // освободить: куча как до uiScrOpen
int  uiScrReady(void);
void uiScrShow(int screen, int tag, int on);       // теги экрана пакета (Splash: UI_T_LOADING / UI_T_PRESS_START)
void uiScrTint(int screen, int tag, unsigned abgr); // sprite multiplier; 0 clears, reset on packet close
extern float uiPressStartBox[5];   // №405 (служебное): часы экрана, x0 y0 x1 y1 «Press START», как нарисована (пульс)
extern float uiMenuItemBox[6];     // правка 412 (служебное): мс выбранного пункта главного меню, масштаб, x0 y0 x1 y1 строки, как нарисована
// В открытый список GE (2D). progress 0..1000 — фитиль; t — секунды показа (кадры огня/анимации/точек по частотам
// из пакета); level 0..6 — газета уровня.
void uiScrDraw(int screen, int progress, float t, int level);
// №763: крутящимся картинкам экрана (лучи будки и гардероба) — своё время вращения t, с (не сбрасывается сменой фазы экрана), и доля яркости a 0..1
// (затухание вместе с фоном); действует на один следующий вызов uiScrDraw / uiScrDrawMenu. t < 0 — часы экрана, как раньше
void uiScrSpin(float t, float a);
// Газета экрана загрузки (Loading, поле UI_F_LEVEL) в полёте (правка Роба 35: разворот при показе, viewer/main.c
// loadBegin): поворот ang (рад) и масштаб scale от центра её места, прозрачность alpha; (0, 1, 1) — на месте, как в пакете.
// Повёрнутая газета рисуется четырёхугольником (линейный фильтр), на месте — спрайтом 1:1, как раньше.
void uiScrNews(float ang, float scale, float alpha);

// ---- меню оригинала (tools/menu_export.py): menu.shr — экраны главного меню в рамке ТВ (0 options, 1 sound), pmenu.shr —
// экраны паузы (0 pauseoptions, 1 pausesound, 2 pausesettings). Открывается uiScrOpen, рисуется поверх гостиной / города.
// Строка k: подпись (пункт), варианты _Value (значение = номер строки Scrooby), ползунок (0..1000, шаг 100 по ◄ ►).
enum { UI_ROW_ITEM = 0, UI_ROW_OPTIONS = 1, UI_ROW_SLIDER = 2 };
enum { UI_SCR_NONE = -1, UI_SCR_BACK = -2, UI_SCR_CHANGED = -3 };
int  uiScrRows(int screen);
int  uiScrRowKind(int screen, int row, int *nopt);   // UI_ROW_*; nopt — число вариантов
const char *uiScrRowLabel(int screen, int row);
int  uiScrTagRect(int screen, int tag, int *r);    // №557: прямоугольник первой картинки с меткой экрана второго пакета (px); 0 — нет
void uiScrSetRow(int row, int value);
int  uiScrGetRow(int row);
void uiScrEnable(int screen, int row, int on);      // выключенная строка — серая, курсор её пропускает
int  uiScrEnabled(int screen, int row);
void uiScrDrawMenu(int screen, int sel, float t);
// правка 348: подменю паузы (MISSION SELECT) скрывает пункты и заголовок ПАУЗЫ (uiPauseMenuHide); экран второго пакета с тем же номером
// (pextra.shr: 1 — MissionSelect) их не теряет. on=1 — прежнее (до 348: названия миссий и заголовок MISSION SELECT не рисовались)
void uiScrPauseLeakOld(int on);
// ↑↓ — строка (выключенные пропускаются), ◄ ► — значение, X — пункт (номер строки) или следующий вариант, △/○ — UI_SCR_BACK
int  uiScrMenuInput(int screen, int *sel, unsigned buttonsJustPressed);
// Текст строки k из кода (слот сохранения, сообщение с номером слота): подпись строки меню k и текст с полем ROW<k>
// рисуются строкой s (Latin-1; указатель живёт, пока открыт пакет), NULL — как в пакете. Сбрасывается uiScrOpen/uiScrClose
void uiScrRowText(int screen, int row, const char *s);   // текст строки row экрана screen второго пакета (NULL — из пакета)
const char *uiScrRowShown(int screen, int row);   // что нарисовано в строке меню row экрана screen (с подменами из кода; гейт сохранений)
int  uiScrScreens(void);                           // экранов во втором пакете (0 — не открыт)
int  uiScrBackInput(unsigned hit);                 // экран без строк: △ / ○ — 1 (звук back)
// VIEW CREDITS (ViewCredits.scr): фон, титры (блоки CREDITS_NN, сдвиг вверх на scroll px), рамка-телевизор и Back поверх;
// 1 — титры ещё на экране, 0 — все ушли за верх. info (может быть NULL) — для гейта: что нарисовано
typedef struct { int blocks, lines, first, top, bottom, x0, x1; float end; } UiCredits;
int  uiScrDrawCredits(int screen, float scroll, UiCredits *info);

// ---- анимации и звуки HUD (viewer/ui_anim.h — числа и откуда они; звуки — viewer/ui_sfx.c) -------------------------
// Всплывание уведомлений (задание, этап/миссия пройдены, карточка, осы, отсчёт ...), пульс △ и ровное кольцо Hit & Run
// работают сами внутри uiDraw. uiPopHook: fn(ev, on) — тег HUD UI_T_* показан (on=1) / снят (0) в нарисованном кадре
// (новый текст задания и новое число отсчёта — снова on=1), и события паузы UI_EV_* (звук интерфейса).
enum { UI_EV_PAUSE = 64, UI_EV_SCROLL, UI_EV_ACCEPT, UI_EV_LOCKED, UI_EV_BACK };   // BACK — △/○ в меню OPTIONS и слотах (frontend.spt back)
void uiPopHook(void (*fn)(int ev, int on));
void uiPopHookFire(int ev);   // событие интерфейса UI_EV_* (звук: back / accept …) от экранов вне ui.c
// служебное для гейтов (tools/ui_anim_gate.sh): строки в smoke.txt и отрицательные контроли
enum { UI_TRACE_PULSE = 1, UI_TRACE_HNR = 2, UI_TRACE_POP = 4 };            // UIPULSE / UIHNR / UIPOP — раз в кадр HUD
enum { UI_TRACE_AVATAR = 8 };
enum { UI_TRACE_MENU = 16 };     // MENUITEM — рамка пульсирующего выбранного пункта меню (правки 412/413, tests/menu/pulse_all.sh)
void uiScrMenuFlags(int flags);   // 413: флаги пункта CGuiMenu оригинала для следующего uiScrDrawMenu (2 — пульс масштабом), SLES_518.97   // UIAVATAR / UIBUBBLE — нарисованные рамки аватара и пузыря задания (правка 22, tests/ui/avatar_check.py)
void uiPix8(int on);            // правка 126: 1 (по умолчанию) — сообщения и уведомления пиксельным шрифтом 8×8 (msx); 0 — шрифтами пакета
void uiBatchOld(int on);      // служебное (наряд 11.10): 1 — раскладка текста без кэша, как до UI_BATCH
void uiDrawLog1040Set(int on);
void uiBubble1040Set(int on);   // №1040: «flag bubble1040 0|1», файл bubble1040old
void uiWrap1037Set(int on);   // №1037: «flag wrap1037 0|1», файл wrap1037old
void uiObjWidowOld(int on);   // служебное: текст задания с висячим словом в последней строке, как до правки 215
void uiObjTextOld(int on);      // служебное: 1 — текст задания без значка с места значка (прежний код; отрицательный контроль правки 48)
void uiAvatarOld(int on);       // служебное: 1 — аватар прежнего размера (рамка первого слота; отрицательный контроль)
void uiTypoShot(int mode);      // правка 312 (твик-панель типографики): снимок фона панели — 1 без текста, 2 без интерфейса, 0 как есть
enum { UI_ANIM_NOPULSE = 1, UI_ANIM_NOPOP = 2, UI_ANIM_OLDHNR = 4, UI_ANIM_NOACTFADE = 8 };   // выключить пульс / всплывание / прежнее кольцо / проявление подписи △ (272)
void uiSetLog(void (*log)(const char *));
void uiTrace(unsigned bits);
void uiAnimOff(unsigned bits);
void uiForceShow(int tag, float seconds);   // тег HUD виден seconds с (поверх игры); 0 — снять
void uiClkTest(int mode, float v);             // служебно (сценарий): 1 — часы HUD стоят, 2 — сдвиг на v с, 0 — идут
void uiHnrScript(int on);                   // кольцо H&R по сценарию гейта (удары +7.5% до полного, погоня, сброс)

// Stats of the last uiDraw* call (for the lead's frame budget).
typedef struct { int draws, binds, verts, items, pixels; } UiStats;   // pixels = covered area (fill estimate)
UiStats uiLastStats(void);

#ifdef __cplusplus
}
#endif
#endif
extern void (*uiPakProgress1060)(unsigned done, unsigned total);   // №1060б: вызывается после каждого куска чтения пакета (NULL — нет)
void uiPakSegMax1051(unsigned bytes);   // №1051: потолок куска при чтении пакета (0 — 1 МБ), будка в тесной куче ставит 256 КБ
