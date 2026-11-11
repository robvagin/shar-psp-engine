// Бонус-игра главного меню (правка Роба 420): пункт MINI GAME -> выбор трассы и машины -> гонка на трассе мини-игры с
// соперниками ИИ -> итоги -> снова выбор / назад в меню. Подключается в main.c перед menuRun (нужны рисование города,
// модели машин пакета msncars, камера за машиной chaseCamUpdate, сценарий смоука).
// Правила гонки, данные и правила порта — viewer/bonus.h; трассы и машины — tools/bonus/export.py (папка bonus/ рядом с EBOOT).
// Камеры: TOP — вид сверху оригинала (направление и угол кадра sprint_camShape трассы, viewer/bonus.c brTopCam), CHASE — камера
// за машиной как в игре (chaseCamUpdate с профилем FollowCameraData машины) — расширение порта по слову Роба (у PS2 только вид
// сверху); переключение — SELECT в гонке или строкой CAMERA в меню выбора.
// Управление в гонке как в игре (viewer/game.c gameCarInput): R/X газ, L/○ тормоз, □ ручник, стик/←→ руль; △ — назад на путь
// (правило порта: из машины в бонус-игре не выйти), SELECT — камера, START — пауза (CONTINUE / QUIT GAME — MiniPause.pag).
// Служебно (сценарий смоука): flag bonusauto 1 — машиной игрока ведёт тот же водитель ИИ (гейт: круги проходят), flag bonuscam N
// (0 сверху, 1 за машиной), flag bonuslaps N, flag bonustrack N (1..7), flag bonusrev 0|1, flag bonusgo — старт без нажатий.
// Строки smoke.txt: BONUS ev=menu|load|start|lap|finish|done|cam|exit ..., BONUSFR (раз в секунду: кадры, треугольники).
// BONUS_OLD=1 — прежнее: пункт MINI GAME серый (красный контроль гейта tests/bonus/emu.sh)
// Правка 421: открыты только трассы уровней, где собраны все карточки (viewer/bonus_unlock.h, main.c bonusTrackIsOpen — одна правда);
// закрытые в строке TRACK затемнены, ◄ ► их пропускает, flag bonustrack N на закрытую — отказ (строка BONUS ev=track refused=1)
// Родные экраны, облёт, музыка заезда (09.10, tests/bonus/screens.sh): выбор персонажа и машины, выбор трассы со значками track1…7
// и числом кругов, итоги серии и пауза — страницы minigame.p3d диска (пакет bonus/mini.shr, tools/bonus/screens_export.py: экраны
// BUI_*); во время отсчёта — облёт камерой minigamecam.p3d (bonus/flyby.cam, tools/bonus/flyby_export.py; грузит scripts/ss.mfk);
// музыка заезда — своя у трассы (viewer/bonus_audio.c brAudioTune). Управление выбора: персонаж — ←→ (↑↓ — строка машины, ←→ машина),
// ✕ — к трассам; трасса ←→, круги ↑↓, направление □ (данные: «□ Change Track Direction»), ✕/START — заезд, △ — шаг назад;
// SELECT — камера сверху / за машиной (расширение порта). Облёт пропускается ✕/START (правило порта).
// BONUS_SCREENS_OLD=1 — всё как до 09.10 (временные экраны порта, без облёта, Casio01 на всех трассах): красный контроль гейта;
// по отдельности — BONUS_FLYBY_OLD=1 (без облёта), BONUS_TUNE_OLD=1 (viewer/bonus_audio.h: одна мелодия)
#ifndef BONUS_SCREENS_OLD
#define BONUS_SCREENS_OLD 0
#endif
#ifndef BONUS_FLYBY_OLD
#define BONUS_FLYBY_OLD BONUS_SCREENS_OLD
#endif
// Правка Роба (бонус-игра до оригинала, 10.10; ps2-bonus-b01-native): у оригинала DNF-гонщик без завершённого круга — «DNF / DNF», с завершённым —
// лучший круг остаётся; пока кто-то финишировал и гонка не кончилась, последние 10 с окна в 20 с показывают 9…0 по секунде.
// BONUS_SUMMARY_DNF_OLD=1 / BONUS_END_COUNT_OLD=1 — прежнее (прочерк «-» / без счёта)
#ifndef BONUS_SUMMARY_DNF_OLD
#define BONUS_SUMMARY_DNF_OLD 0
#endif
#ifndef BONUS_END_COUNT_OLD
#define BONUS_END_COUNT_OLD 0
#endif
enum { BUI_MMCHAR = 0, BUI_MMTRACK, BUI_MMHINTS, BUI_MSBASE, BUI_MSROW0, BUI_MINIPAUSE = BUI_MSROW0 + 4, BUI_N };   // экраны bonus/mini.shr
// метки копий значка P1 у лица k и курсора у значка трассы k (tools/bonus/screens_export.py CUR_TAGS — тот же порядок)
static const int buiTag[7] = {UI_T_TIMER, UI_T_PARTIME, UI_T_LAP, UI_T_POSITION, UI_T_DAMAGE, UI_T_PROXIMITY, UI_T_CARDS};
static const int buiLockTag[5] = {UI_T_RADAR, UI_T_SPEED, UI_T_HNR_METER, UI_T_OBJECTIVE, UI_T_HNR_TICKET};
static const int buiTrackTag[7] = {UI_T_COINS, UI_T_WASPS, UI_T_COLLECTIBLES, UI_T_ITEMS_COMPLETE, UI_T_STAGE_COMPLETE, UI_T_MISSION_COMPLETE, UI_T_MISSION_FAILED};
enum { BUI_KING_TOTAL = UI_T_SKIP, BUI_KING_LAP = UI_T_STREETRACE };   // короны лучшего общего времени / лучшего круга (KING_*)
enum { BF_RACE = 0, BF_PAUSE = 1, BF_FLY = 2, BF_SUMMARY = 3 };       // что рисует bnsFrame поверх трассы
// Часть «подборы, ловушки, огонь, DEATH» (09.10, viewer/bonus.h у BnsPick): модели подборов и объектов трассы — типы SPR1 трассы
// (рисует bnsSprops), подбор/нитро/подброс/DEATH/свет — viewer/bonus.c для игрока и соперников, ловушки — кадр клипа объекта ANM1
// (brTrapApply: позы SPR1 и живые объёмы COL1). Строки smoke.txt: BONUS ev=items|pick|death|trap|launch|light|effects.
// BONUS_PICKUPS_OLD=1 — прежнее (красный tests/bonus/pickups.sh и tests/bonus/emu_pickups.sh)
static const char *const bnsChar[5] = {"lisa", "bart", "homer", "marge", "apu"};      // MiniMenu.pag Character0..4 (SLES_518.97 тот же порядок)
static const char *const bnsCharCar[5] = {"lisa_v", "bart_v", "homer_v", "marge_v", "apu_v"};   // машина персонажа (правило порта)
static const char *const bnsVeh[] = {"apu_v", "bart_v", "bbman_v", "bookb_v", "burns_v", "carhom_v", "cArmor", "cCola", "cCurator",
  "cDonut", "cDuff", "cFire_v", "cHears", "cKlimo", "cletu_v", "cLimo", "cNerd", "comic_v", "cPolice", "cSedan", "cVan", "elect_v",
  "famil_v", "fone_v", "frink_v", "gramp_v", "hbike_v", "homer_v", "honor_v", "krust_v", "lisa_v", "marge_v", "moe_v", "mrplo_v",
  "otto_v", "plowk_v", "scorp_v", "skinn_v", "smith_v", "snake_v", "wiggu_v", "willi_v", "zombi_v"};   // MiniMenu.pag Vehicle0_Value (43)
#ifndef BONUS_NPC_MODELS_OLD
#define BONUS_NPC_MODELS_OLD 0
#endif
static const char *const bnsNpcCar[] = {"pickupA", "cLimo", "otto_v"}; // measured default Lisa/b01; original Vehicle bindings
#define BNS_NVEH ((int)(sizeof bnsVeh / sizeof bnsVeh[0]))
static struct { int track, laps, rev, cam, chr, veh; } bsel = {1, BR_LAPS_DEF, 0, 0, BONUS_SETUP_PS2_OLD ? 2 : 0, -1};
static float bnsQuitAt;   // служебное «flag bonusquitat N» (тур TEST88, №1168): выйти из гонки на N-й секунде гонки, как QUIT GAME из паузы
static int bnsShots, bnsQuit;                   // служебное (гейт tests/bonus/screens.sh): снимки экранов самой игрой; выход из первой гонки после снимков
static unsigned bnsShotDone;                     // какие снимки уже сняты (бит на имя)
static int bnsAuto, bnsGo, bnsAgainYes;                       // служебное: машина игрока на водителе ИИ; старт гонки без нажатий
static BonusRace BR; static MsnCars BM; static BnsTrack BT; static unsigned char *bnsMem;
static Traffic bnsTr; static int bnsRoads;       // №1116: дороги трассы (хвост BRD1 файла .bns) и признак «соперники едут по ним»
static BscMap bnsSec; static int bnsCodeOn, bnsCodeLast;   // №1185: сектора трассы, показ кода, последний показанный сектор
static int bnsBotWi = -1, bnsBotLine; static float bnsBotProgT, bnsBotS;   // №1180в: текущая точка WPR/WPL водителя за игрока
#ifndef BONUSBOTSEC1180_OLD
#define BONUSBOTSEC1180_OLD 1   // 0 — при «читере» или остановке водитель за игрока преследует точку линии секторов (проверено: хуже на трассе 7, поэтому выключено); 1 — цель всегда центр ждущего чекпойнта
#endif
#ifndef BONUSBOT1180_OLD
#define BONUSBOT1180_OLD 0   // 1 — водитель за игрока по прежним отрезкам (flag bonusauto)
#endif
static McCar bnsBot; 
                            // тень водителя ИИ для машины игрока (flag bonusauto)
#ifndef STRBOUND1195_OLD
#define STRBOUND1195_OLD 0   // №1195: 1 — прежний strcpy без проверки длины имени модели
#endif
#define BNS_CPY16(dst, src) do { if (STRBOUND1195_OLD) strcpy((dst), (src)); else snprintf((dst), 16, "%s", (src)); } while (0)   // имя модели из пакета: не больше 15 знаков + ноль
static char bnsCarName[4][16];                   // модели машин: 0 — игрок (глобальная car), 1..3 — соперники (BM.c[0..2])
// сверка g07b-msnview (02.11.2026): P17-14/20 нитро по гудку (↑ крестовины) и ИИ; P17-15 отсчёт 10 с; P17-22 «GO!» 0,5 с; P17 Д6 вид меняется только в гонке; P17 Д7 финишировавшего
// машина катится без управления и без торможения; P04 B3 перевёрнутый/разбитый ИИ через 1 с на месте. Выключатели (=1 — прежнее): BR_NITRO3_OLD, BR_DNF10_OLD (bonus.h),
// BONUS_GO_OLD, BONUS_SELECT_ANY_OLD, BONUS_FINISH_COAST_OLD, BONUS_AIFLIP_OLD
#ifndef BONUS_AIFLIP3_OLD
#define BONUS_AIFLIP3_OLD 0
#endif
#ifndef BONUS_AUTORESET_OLD
#define BONUS_AUTORESET_OLD 0
#endif
#ifndef BONUS_FLYPAUSE_OLD
#define BONUS_FLYPAUSE_OLD 0
#endif
#ifndef BONUS_GO_OLD
#define BONUS_GO_OLD 0
#endif
#ifndef BONUSCAM1146_OLD
#define BONUSCAM1146_OLD 0   // №1146 (камера): 1 — как после №1109 (вид за машиной без правила глаза и ближней плоскости основной игры, бампер с ближней 1,5 м)
#endif
#ifndef BONUSSELLABEL1150_OLD
#define BONUSSELLABEL1150_OLD 0   // №1150: 1 — в правом верхнем углу гонки надпись «SELECT: …» (следующий вид)
#endif
#ifndef BONUSCAM1109_OLD
#define BONUSCAM1109_OLD 0   // №1109 (камера): 1 — Select только сверху / за машиной, как в TEST84
#endif
#ifndef BONUS_SELECT_ANY_OLD
#define BONUS_SELECT_ANY_OLD 0
#endif
#ifndef BONUS_FINISH_COAST_OLD
#define BONUS_FINISH_COAST_OLD 0
#endif
#ifndef BONUS_AIFLIP_OLD
#define BONUS_AIFLIP_OLD 0
#endif
static int bnsGoDone; static float bnsAiFlipT[4];
static int bnsWins[4], bnsPts[4];                // серия гонок этой сессии (MiniSummary.pag: Wins, Points)
static int bnsRacerChr[4];                        // персонаж машины 0..3 (номер MiniMenu.pag Character0..4) — лица итогов
static int bnsUi;                                 // 1 — открыт пакет родных экранов bonus/mini.shr
static float bnsUiT;                              // часы экранов (пульс пункта паузы)
static struct { int n; float fps; float *f; } bnsFly;   // облёт: кадры BFC1 (12 чисел на кадр)
static float bnsFlyT;                             // время облёта, с (кадр = t * fps)
static unsigned char bnsRate[64];                 // рейтинг машины s0..s9 (Vehicle0_Rating), правило порта — по скорости .con
static int bnsPickInst[BNS_MAXPICK];             // подбор -> экземпляр SPR1 его модели (сверка по точке, как ключи уровня)
static uint32_t bnsTint[4];                      // свет зоны у машины в этом кадре (ARGB данных, 0 — нет)

static void bnsLog(const char *s) { if (smkOn) smkWrite(s); }
static int bnsLower(const char *a, char *o, int n) { int k = 0; for (; a[k] && k < n - 1; k++) o[k] = (char)tolower((unsigned char)a[k]); o[k] = 0; return k; }
static int bnsVehOf(const char *name) { for (int i = 0; i < BNS_NVEH; i++) if (!strcasecmp(bnsVeh[i], name)) return i; return 0; }

static void bnsRect(int x, int y, int w, int h, unsigned abgr) {   // 2D-плашка (полупрозрачная)
  typedef struct { unsigned c; short x, y, z; } V; V *v = sceGuGetMemory(2 * sizeof(V));
  v[0].c = v[1].c = abgr; v[0].x = (short)x; v[0].y = (short)y; v[1].x = (short)(x + w); v[1].y = (short)(y + h); v[0].z = v[1].z = 0;
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_DEPTH_TEST); sceGuEnable(GU_BLEND); sceGuShadeModel(GU_FLAT);
  sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  sceGuShadeModel(GU_SMOOTH); sceGuEnable(GU_DEPTH_TEST); resetState();
}
// №1145 (Роб 02.11: «бонус-игра: интерфейс старым шрифтом везде; сделай как в основной игре»): надписи гонки и запасных экранов — гранью bart_16bit (Bart vs the Space Mutants) с чёрной тенью
// на пиксель вправо-вниз, как цифры HUD основной игры; страницы родного пакета bonus/mini.shr (заголовок и пункты паузы, итоги) берут грани из панели типографики при выпуске пакета
// (tools/ui_typo.py: пауза бонуса = пауза основной игры). BONUSUI1145_OLD=1 — прежний шрифт пакета (грань 0) без тени HUD
#ifndef BONUSUI1145_OLD
#define BONUSUI1145_OLD 0
#endif
// №1149 (Роб): отсчёт 3-2-1-GO перед бонус-гонкой — тот же код, вид и шрифт, что у уличных гонок на уровнях (HUD, метка отсчёта). BONUSCD1149_OLD=1 — прежние цифры и «GO!» строкой
#ifndef BONUSCD1149_OLD
#define BONUSCD1149_OLD 0
#endif
// №1179 (повтор №1145: Роб в TEST88 по-прежнему видел старый шрифт): причина — грани в пакете называются с кратностью («bart_16bit@1», «msx_thin@1»), а поиск по точному имени искал «bart_16bit»
// и не находил: bnsFont() всегда давал -1 и рисовалась прежняя грань с плашкой. BONUSFONT1179_OLD=1 — прежние имена (как в TEST88)
#ifndef BONUSFONT1179_OLD
#define BONUSFONT1179_OLD 0
#endif
#ifndef BONUSHUDK1179
#define BONUSHUDK1179 2   // кратность грани HUD гонки (круг, место и время, нитро): точка грани — 2 x 2 px
// №1212 (Роб 05.11: «выходим из гонки в меню бонусной игры — там музыка как в главном меню… мне кажется, не это»): у оригинала в меню мини-игры
// играет её же музыка (событие мини-игры с её начала, после каждой гонки — заново), не музыка главного меню. BONUSMENUMUS1212_OLD=1 — как было
#ifndef BONUSMENUMUS1212_OLD
#define BONUSMENUMUS1212_OLD 0
#endif
#endif
static int bnsFont(void) { return BONUSUI1145_OLD ? -1 : uiFontFindExact(BONUSFONT1179_OLD ? "bart_16bit" : "bart_16bit@1"); }   // номер грани в открытом пакете UI, -1 — прежнее
static int bnsT(int x, int y, unsigned c, const char *s) { int f = bnsFont(); return f >= 0 ? uiTextSh(f, x, y, c, s) : uiText(0, x, y, c, s); }
static int bnsTK(int x, int y, unsigned c, const char *s, int k) { int f = bnsFont(); return f >= 0 ? uiTextShS(f, x, y, c, s, k) : uiText(0, x, y, c, s); }   // кратная грань (HUD гонки)
static int bnsTR(int x, int y, unsigned c, const char *s) { int f = BONUSUI1145_OLD ? -1 : uiFontFindExact(BONUSFONT1179_OLD ? "msx_thin" : "msx_thin@1"); return f >= 0 ? uiTextSh(f, x, y, c, s) : bnsT(x, y, c, s); }   // строки результатов: грань итогов по панели Роба (bonus_rows: msx_thin), нет её в пакете — bart
static int bnsTextW(const char *s) { int f = bnsFont(); return f >= 0 ? uiTextW(f, s) : uiText(0, 0, -100, 0x00000000u, s); }
static void bnsTextC(int y, unsigned c, const char *s) { bnsT((SCR_W - bnsTextW(s)) / 2, y, c, s); }
#ifndef BONUSHINT1198_OLD
#define BONUSHINT1198_OLD 0   // аудит 05.11 п. 15: подсказка камеры на экране выбора трассы — по сетке правой колонки. 1 — прежнее положение
#endif
// GitHub #4 (konsolenschatz): подписи запасных экранов бонус-гонки (выбор трассы, итоги серии, «ещё заезд?», пауза, «LOADING») были зашиты по-английски. Теперь на французском, немецком и
// испанском они берутся из lang/<буква>/bonuslabels.txt (tools/lang_bonus.py: строки текстовой библиотеки диска; «TRACK» и «CAR» отдельным словом на диске нет — слова диска для
// этих понятий), по строке на подпись в порядке BL_*; нет файла или пустая строка — английский из кода. Строки без текста на диске (DIRECTION, CHARACTER, CAMERA, подсказки) остаются английскими.
// BONUSLANG_OLD=1 — только английский, как было
#ifndef BONUSLANG_OLD
#define BONUSLANG_OLD 0
#endif
enum { BL_TITLE, BL_TRACK, BL_LAPS, BL_PRESS, BL_STANDINGS, BL_CAR, BL_WINS, BL_POINTS, BL_TOTAL, BL_BEST, BL_CONTINUE, BL_QUIT, BL_AGAIN, BL_YES, BL_NO, BL_LOADING, BL_GO, BL_CAMERA, BL_CAMTOP, BL_CAMBEHIND, BL_LAP, BL_NITRO, BL_PL1, BL_PL2, BL_PL3, BL_PL4, BL_N };
static const char *const bnsLblEn[BL_N] = {"SELECT TRACK AND NUMBER OF LAPS", "TRACK", "LAPS", "PRESS START", "Race Series Standings", "CAR", "Wins", "Points", "Total Time", "Best Lap",
                                           "CONTINUE", "QUIT GAME", "RACE AGAIN?", "YES", "NO", "LOADING", "GO!", "Camera", "top", "behind car", "LAP", "NITRO", "1st", "2nd", "3rd", "4th"};
static char bnsLblTxt[BL_N][64]; static int bnsLblLang = -1;
static int bnsLblOn(void) { return !BONUSLANG_OLD && gLangId > 0; }
static const char *bnsLbl(int k) {
  if (!bnsLblOn()) return bnsLblEn[k];
  if (bnsLblLang != gLangId) {
    bnsLblLang = gLangId; memset(bnsLblTxt, 0, sizeof bnsLblTxt);
    char fn[256]; sibling(fn, "bonuslabels.txt"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0);
    if (f >= 0) { static char b[BL_N * 64 + 64]; int n = sceIoRead(f, b, (int)sizeof b - 1); sceIoClose(f);
      if (n > 0) { b[n] = 0; char *q = b; for (int i = 0; i < BL_N && *q; i++) { char *e = strchr(q, '\n'); if (e) *e = 0; snprintf(bnsLblTxt[i], sizeof bnsLblTxt[i], "%s", q); q = e ? e + 1 : q + strlen(q); } } }
  }
  return bnsLblTxt[k][0] ? bnsLblTxt[k] : bnsLblEn[k];
}
// заголовки столбцов итогов: каждый стоит над своим столбцом (x0), но целиком на экране и не наезжает на соседа справа (на английском сдвига нет)
static void bnsHeads(const int *k, const int *x0, int n, int y, unsigned c) {
  int x[8], w[8], lim = SCR_W - 6; if (n > 8) n = 8;
  for (int i = 0; i < n; i++) { x[i] = x0[i]; w[i] = bnsTextW(bnsLbl(k[i])); }
  if (bnsLblOn()) for (int i = n - 1; i >= 0; i--) { if (x[i] + w[i] > lim) x[i] = lim - w[i]; lim = x[i] - 6; }
  for (int i = 0; i < n; i++) bnsT(x[i], y, c, bnsLbl(k[i]));
}

// ---------------------------------------------------------------- служебные флаги сценария
static void bonusOpenSet(unsigned m, const char *src);   // main_save.h
static void bnsFlag(const SmokeCmd *sc) {
  if (sc->op != 2) return;
  if (!strcmp(sc->name, "bonusauto")) bnsAuto = sc->a != 0.f;
  else if (!strcmp(sc->name, "bonuscam")) bsel.cam = sc->a != 0.f;
  else if (!strcmp(sc->name, "bonuslaps")) { int n = (int)sc->a; bsel.laps = n < BR_LAPS_MIN ? BR_LAPS_MIN : n > BR_LAPS_MAX ? BR_LAPS_MAX : n; }
  else if (!strcmp(sc->name, "bonustrack")) { int n = (int)sc->a; if (n >= 1 && n <= 7) {
      int ok = bonusTrackIsOpen(n); if (ok) bsel.track = n;   // правка 421: закрытая трасса не выбирается
      char s[96]; snprintf(s, sizeof s, "BONUS ev=track want=%d track=%d refused=%d open=%02x t=%.1f\n", n, bsel.track, !ok, bonusOpenMask(), smkT); bnsLog(s); } }
  else if (!strcmp(sc->name, "bonusrev")) bsel.rev = sc->a != 0.f;
  else if (!strcmp(sc->name, "bonusgo")) bnsGo = 1;
  else if (!strcmp(sc->name, "bonusshots")) bnsShots = sc->a != 0.f;
  else if (!strcmp(sc->name, "bonusquit")) bnsQuit = sc->a != 0.f;
  else if (!strcmp(sc->name, "bonusquitat")) bnsQuitAt = sc->a;
  else if (!strcmp(sc->name, "bonuscode")) bnsCodeOn = sc->a != 0.f;   // №1185
  else if (!strcmp(sc->name, "bonusveh")) { int n = (int)sc->a; if (n >= 0 && n < BNS_NVEH) bsel.veh = n; }   // служебное: машина игрока по номеру в списке bnsVeh
  else if (!strcmp(sc->name, "bonusfrag")) fragHeapMB = (unsigned)sc->a;   // №1167 тест: куча раздроблена до N МБ наибольшего куска перед загрузкой трассы (как у Роба после долгой игры; main_levels.h fragHeap)
  else if (!strcmp(sc->name, "tfbonusfail")) tfFailBonus = (int)sc->a;   /* Build 96 служебно: отказ памяти на BONUS (меню бонус-игры читает флаги само) */
  else if (!strcmp(sc->name, "tfmenufail")) tfFailMenu = (int)sc->a;   /* Build 96 служебно: отказ чтения гостиной при выходе из бонус-игры */
  else if (!strcmp(sc->name, "bonusopen")) { bonusSvcAll = 1; bonusOpenSet(bonusOpen, "scenario"); }   /* только включает: «flag bonusopen» без числа раньше выключал */   // и пункт MINI GAME главного меню включается сразу (в туре меню уже построено серым)   // №1168: все трассы открыты и из сценария (прежде — только файлом bonusopen рядом с EBOOT)
  else if (!strcmp(sc->name, "bonusagain")) bnsAgainYes = (int)sc->a;   // круг 6: сколько раз в смоуке ответить «RACE AGAIN?» — ДА (серия очков должна жить между заездами)
}

// ---------------------------------------------------------------- родные экраны (bonus/mini.shr), облёт (bonus/flyby.cam)
static int bnsUiOpen(void) {   // пакет экранов мини-игры во второй слот UI (load.shr уровня его вытесняет — открываем снова)
  bnsUi = 0;
  if (BONUS_SCREENS_OLD) return 0;
  char fn[256], s[160]; sibling(fn, BONUS_UI_PAK);
  int r = uiScrOpen(fn); bnsUi = !r && uiScrScreens() == BUI_N;
  if (!r && !bnsUi) uiScrClose();
  snprintf(s, sizeof s, "BONUS ev=ui pak=%s ok=%d screens=%d heap=%u t=%.1f\n", BONUS_UI_PAK, bnsUi, uiScrScreens(), heapFreeBytes(), smkT); bnsLog(s);
  return bnsUi;
}
static void bnsUiClose(void) { if (bnsUi) uiScrClose(); bnsUi = 0; }
static void bnsShot(int bit, const char *name) {   // снимок кадра, который сейчас на экране (один раз на имя за прогон)
  if (!bnsShots || (bnsShotDone >> bit & 1)) return;
  bnsShotDone |= 1u << bit; smokeShotNow(name);
}
static void bnsUiLog(const char *scr) { char s[96]; snprintf(s, sizeof s, "BONUS ev=uiscreen name=%s pak=%d t=%.1f\n", scr, bnsUi, smkT); bnsLog(s); }
// ПРАВИЛО ПОРТА: рейтинг машины (звёзды s0..s9 Vehicle0_Rating) в данных страницы не задан — по наибольшей скорости .con машины,
// линейно от самой медленной (s0) до самой быстрой (s9) из 43 машин выбора
static void bnsRatings(void) {
  float ts[BNS_NVEH], lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < BNS_NVEH; i++) { CarParams P; char n[16]; bnsLower(bnsVeh[i], n, 16); carParamsFor(n, &P); ts[i] = P.topSpeed;
    if (ts[i] < lo) lo = ts[i]; if (ts[i] > hi) hi = ts[i]; }
  for (int i = 0; i < BNS_NVEH && i < 64; i++) { int r = hi > lo ? (int)floorf((ts[i] - lo) / (hi - lo) * 9.f + .5f) : 0; bnsRate[i] = (unsigned char)(r < 0 ? 0 : r > 9 ? 9 : r); }
  if (!BONUS_SETUP_PS2_OLD) bnsRate[bnsVehOf("famil_v")] = 2; // measured native Family rating; other ratings remain port policy
}
static void bnsFlyLoad(void) {   // bonus/flyby.cam: 'BFC1', кадров, к/с, 0; кадр — позиция, взгляд, верх, fov, near, far
  free(bnsFly.f); memset(&bnsFly, 0, sizeof bnsFly);
  if (BONUS_FLYBY_OLD) return;
  char fn[256]; sibling(fn, BONUS_FLYBY_CAM); SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0); if (fd < 0) return;
  struct { char m[4]; unsigned n; float fps; unsigned z; } h;
  if (sceIoRead(fd, &h, sizeof h) == (int)sizeof h && !memcmp(h.m, "BFC1", 4) && h.n > 1 && h.n <= 2048 && h.fps > 0.f) {
    unsigned bytes = h.n * 12u * 4u; bnsFly.f = malloc(bytes);
    if (bnsFly.f && sceIoRead(fd, bnsFly.f, bytes) == (int)bytes) { bnsFly.n = (int)h.n; bnsFly.fps = h.fps; } else { free(bnsFly.f); bnsFly.f = 0; }
  }
  sceIoClose(fd);
}
static void bnsFlyPose(float t, float eye[3], float at[3], float up[3], float *fovyDeg) {   // кадр облёта: линейно между кадрами данных
  float fr = t * bnsFly.fps; if (fr < 0.f) fr = 0.f; if (fr > bnsFly.n - 1) fr = (float)(bnsFly.n - 1);
  int a = (int)fr, b = a + 1 < bnsFly.n ? a + 1 : a; float u = fr - a, v[12];
  for (int k = 0; k < 12; k++) v[k] = bnsFly.f[a * 12 + k] * (1.f - u) + bnsFly.f[b * 12 + k] * u;
  float ll = sqrtf(v[3] * v[3] + v[4] * v[4] + v[5] * v[5]), ul = sqrtf(v[6] * v[6] + v[7] * v[7] + v[8] * v[8]);
  for (int k = 0; k < 3; k++) { eye[k] = v[k]; at[k] = v[k] + v[3 + k] / (ll > 0.f ? ll : 1.f); up[k] = v[6 + k] / (ul > 0.f ? ul : 1.f); }
  *fovyDeg = 2.f * atanf(tanf(v[9] * .5f) * .75f) * (180.f / GU_PI);   // fov данных — горизонталь 4:3 (как sprint_camShape), Hor+
}

// выбор на страницах MiniMenu.pag: phase 0 — персонаж и машина (Foreground), 1 — трасса, круги, направление (Overlay поверх)
#ifndef BONUS_PERF_OLD
#define BONUS_PERF_OLD 0
#endif
static unsigned bnsPerfSimUs, bnsPerfSimIo;
static void bnsBudget(const char *kind, unsigned start, unsigned io0, unsigned sim, unsigned simIo) {
#if !BONUS_PERF_OLD
  if (smkOn) {
    unsigned end = sceKernelGetSystemTimeLow(); char line[192];
    snprintf(line, sizeof line, "BONUSPERF sc=%s kind=%s work_us=%u sim_us=%u io_us=%u state=%d track=%d\n",
             smkScene, kind, end-start+sim, sim, smkIoUs-io0+simIo, BR.state, bsel.track);
    smkWrite(line);
  }
#else
  (void)kind; (void)start; (void)io0; (void)sim; (void)simIo;
#endif
}
// №1181 (Роб 04.11: «странный момент ожидания других игроков, которые с PS2; у нас не может быть других игроков»): оригинал после выбора машины первым игроком включает таймер ожидания остальных
// трёх (по его исходному поведению: гонка стартует сразу, когда выбрали все четыре места, иначе по концу таймера; запись PS2 тоже показывает отсчёт 6 при одном контроллере). На PSP игрок один —
// после подтверждения машины гонка начинается сразу, без экрана ожидания. BONUSWAIT1181_OLD=1 — прежние 6 с ожидания
#ifndef BONUSWAIT1181_OLD
#define BONUSWAIT1181_OLD 0
#endif
static void bnsMenuNativeDraw(int phase, int focus, int chrReady, float waitReady, void **fbp) {
  GE_NEST_GUARD("bnsMenuNativeDraw");   // №1186/№1188
  unsigned budgetT0 = sceKernelGetSystemTimeLow(), budgetIo0 = smkIoUs;
  sceGuStart(GU_DIRECT, list); resetState();
  sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  for (int k = 0; k < 5; k++) { uiScrSetRow(k, k == bsel.chr); uiScrShow(BUI_MMCHAR, buiTag[k], k == bsel.chr); }   // лицо: кадр _s — выбран
  for (int k = 0; k < 5; k++) uiScrShow(BUI_MMCHAR, buiLockTag[k], chrReady && k == bsel.chr);
  uiScrShow(BUI_MMCHAR, UI_T_COINS, phase == 0);   // подпись «Change Character» — только на экране выбора игрока (под выбором трассы она просвечивала)
  uiScrShow(BUI_MMCHAR, UI_T_ACTION, waitReady >= 0.f);
  uiScrShow(BUI_MMCHAR, UI_T_COUNTDOWN, waitReady >= 0.f);
  uiScrSetRow(8, waitReady >= 0.f ? (int)ceilf(waitReady) : 0);
  uiScrSetRow(5, bsel.veh); uiScrSetRow(6, bnsRate[bsel.veh & 63]); uiScrSetRow(7, phase == 0 && focus == 1 ? 0 : 1);   // стрелки машины: яркие — строка выбрана
  uiScrDraw(BUI_MMCHAR, 0, bnsUiT, 0);
  if (phase == 1) {
    for (int k = 0; k < 7; k++) {
      uiScrShow(BUI_MMTRACK, buiTag[k], k == bsel.track - 1);
      uiScrShow(BUI_MMTRACK, buiTrackTag[k], 1);
      uiScrTint(BUI_MMTRACK, buiTrackTag[k], bonusTrackIsOpen(k + 1) ? 0xffffffffu : 0xff404040u);
    }
    uiScrSetRow(9, bsel.laps); uiScrSetRow(10, bsel.rev);
    uiScrDraw(BUI_MMTRACK, 0, bnsUiT, 0); resetState();
    // ПРАВИЛО ПОРТА: строка камеры (расширение порта по слову Роба, в данных её нет) — под «□ Change Track Direction», мелко
    char cm[96]; snprintf(cm, sizeof cm, "SELECT  %s: %s", bnsLbl(BL_CAMERA), bnsLbl(bsel.cam ? BL_CAMBEHIND : BL_CAMTOP)); if (BONUSHINT1198_OLD) bnsT(14, SCR_H - 24, 0xc0d0d0d0u, cm); else bnsT(27, 250 - uiFontLh(bnsFont()), 0xc0d0d0d0u, cm);   // аудит п. 15: строка на линии BACK правой колонки (низ 250), левый край — по значку □ (27)   // №1182: на F/G/S по-своему (lang/<буква>/bonuslabels.txt)
  }
  uiScrDraw(BUI_MMHINTS, 0, bnsUiT, 0);
  resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuFinish(); sceGuSync(0, 0); bnsBudget("menu", budgetT0, budgetIo0, 0, 0);
  sceDisplayWaitVblankStart(); *fbp = swapBuf();
}
static int bnsMenuNative(void **fbp) {   // 1 — старт гонки, 0 — назад в главное меню
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(); int phase = BONUS_SETUP_PS2_OLD ? 0 : 1, focus = 0, phLog = -1, chrReady = 0; float waitReady = -1.f; char s[200];
  if (bsel.veh < 0) bsel.veh = bnsVehOf(BONUS_SETUP_PS2_OLD ? bnsCharCar[bsel.chr] : "famil_v");
  bsel.track = bonusTrackPick(bonusOpenMask(), bsel.track);
  if (!BONUSMENUMUS1212_OLD && !BONUS_SOUND_OLD && sndLevel != -2) {   // №1212: звук мини-игры (bonus/audio) на всё время её меню и гонок, как отдельная «игра» у оригинала
    char ad_[256]; sibling(ad_, "bonus/audio/"); sndShutdown(); if (sndInit(ad_) == 0) { sndLevel = -2; cfgApply(); uiSfxInit(smkWrite); bbThForgetAudio(); } }
  if (!BONUSMENUMUS1212_OLD && !BONUS_SOUND_OLD) { int t_ = audioTrack(brAudioTuneNameDir(bsel.track, 0)); if (t_ >= 0) audioMusic(t_, .5f);
    char m_[96]; snprintf(m_, sizeof m_, "BONUS ev=menumusic tune=%s t=%.1f\n", t_ >= 0 ? audioTrackName(t_) : "-", smkT); bnsLog(m_); }
  bnsRatings();
  snprintf(s, sizeof s, "BONUS ev=menu track=%d laps=%d rev=%d cam=%d char=%s car=%s ui=1 open=%02x t=%.1f\n", bsel.track, bsel.laps, bsel.rev, bsel.cam,
           bnsChar[bsel.chr], bnsVeh[bsel.veh], bonusOpenMask(), smkT); bnsLog(s);
  for (;;) {
    unsigned tn = sceKernelGetSystemTimeLow(); float dt = (tn - last) * 1e-6f; last = tn; if (dt > .05f) dt = .05f; bnsUiT += dt;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);   // №706: зажатая стрелка листает список (0,4 с, затем 0,12 с)
    SmokeCmd sc; while (smokeNext(dt, &sc)) bnsFlag(&sc);
    if (waitReady >= 0.f) {
      waitReady -= dt;
      if (waitReady <= 0.f) { bnsUiLog("waitplayers_end"); return 1; }
      bnsMenuNativeDraw(0, 1, 1, waitReady, fbp); continue;
    }
    if (bnsGo) { bnsGo = 0; if (bonusTrackIsOpen(bsel.track)) return 1; }
    if (phase != phLog) { phLog = phase; bnsUiLog(phase ? "mmtrack" : "mmchar"); }
    int d = (hit & PSP_CTRL_RIGHT) ? 1 : (hit & PSP_CTRL_LEFT) ? -1 : 0;
    if (hit & PSP_CTRL_SELECT) { bsel.cam = !bsel.cam; uiPopHookFire(UI_EV_SCROLL); }
    if (phase == 0) {
      if (BONUS_SETUP_PS2_OLD && (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN))) { focus = !focus; uiPopHookFire(UI_EV_SCROLL); }
      if (d && focus == 0) { bsel.chr = (bsel.chr + d + 5) % 5; if (BONUS_SETUP_PS2_OLD) bsel.veh = bnsVehOf(bnsCharCar[bsel.chr]); uiPopHookFire(UI_EV_SCROLL); }
      if (d && focus == 1) { bsel.veh = (bsel.veh + d + BNS_NVEH) % BNS_NVEH; uiPopHookFire(UI_EV_SCROLL); }
      if (hit & (BONUS_SETUP_PS2_OLD ? (PSP_CTRL_CROSS | PSP_CTRL_START) : PSP_CTRL_CROSS)) {
        uiPopHookFire(UI_EV_ACCEPT);
        if (BONUS_SETUP_PS2_OLD) phase = 1;
        else if (!chrReady) { chrReady = 1; focus = 1; snprintf(s, sizeof s, "BONUS ev=confirm kind=character char=%s t=%.1f\n", bnsChar[bsel.chr], smkT); bnsLog(s); }
        else if (bonusTrackIsOpen(bsel.track)) { snprintf(s, sizeof s, "BONUS ev=confirm kind=vehicle car=%s t=%.1f\n", bnsVeh[bsel.veh], smkT); bnsLog(s); if (BONUSWAIT1181_OLD) { waitReady = 6.f; bnsUiLog("waitplayers"); } else { bnsUiLog("start_single"); return 1; } }
      } else if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) {
        uiPopHookFire(UI_EV_BACK);
        if (BONUS_SETUP_PS2_OLD) return 0;
        if (chrReady) { chrReady = 0; focus = 0; } else phase = 1;
      }
    } else {
      if (d) { bsel.track = bonusTrackStep(bonusOpenMask(), bsel.track, d); uiPopHookFire(UI_EV_SCROLL);
        snprintf(s, sizeof s, "BONUS ev=track d=%d track=%d open=%02x t=%.1f\n", d, bsel.track, bonusOpenMask(), smkT); bnsLog(s); }
      int dl = (hit & PSP_CTRL_UP) ? 1 : (hit & PSP_CTRL_DOWN) ? -1 : 0;
      if (dl) { bsel.laps += dl;
        if (bsel.laps < BR_LAPS_MIN) bsel.laps = BONUS_SETUP_PS2_OLD ? BR_LAPS_MAX : BR_LAPS_MIN;
        if (bsel.laps > BR_LAPS_MAX) bsel.laps = BONUS_SETUP_PS2_OLD ? BR_LAPS_MIN : BR_LAPS_MAX;
        uiPopHookFire(UI_EV_SCROLL); snprintf(s, sizeof s, "BONUS ev=laps laps=%d t=%.1f\n", bsel.laps, smkT); bnsLog(s); }
      if (hit & PSP_CTRL_SQUARE) { bsel.rev = !bsel.rev; uiPopHookFire(UI_EV_SCROLL); }   // «□ Change Track Direction» (данные)
      if ((hit & (BONUS_SETUP_PS2_OLD ? (PSP_CTRL_CROSS | PSP_CTRL_START) : PSP_CTRL_CROSS)) && bonusTrackIsOpen(bsel.track)) {
        uiPopHookFire(UI_EV_ACCEPT); if (BONUS_SETUP_PS2_OLD) return 1; phase = 0; focus = 0; chrReady = 0;
      }
      if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) { uiPopHookFire(UI_EV_BACK); if (!BONUS_SETUP_PS2_OLD) return 0; phase = 0; }
    }
    bnsMenuNativeDraw(phase, focus, chrReady, waitReady, fbp);
    if (bnsShots && !(bnsShotDone >> (phase ? 1 : 0) & 1)) {
      snprintf(s, sizeof s, "BONUS ev=uistate scr=%s chr=%d veh=%d rate=%d focus=%d track=%d laps=%d rev=%d open=%02x ready=%d t=%.1f\n", phase ? "mmtrack" : "mmchar",
               bsel.chr, bsel.veh, bnsRate[bsel.veh & 63], focus, bsel.track, bsel.laps, bsel.rev, bonusOpenMask(), chrReady, smkT); bnsLog(s);
      bnsShot(phase ? 1 : 0, phase ? "bonus_mmtrack" : "bonus_mmchar"); }
  }
}

// ---------------------------------------------------------------- выбор: трасса, круги, направление, персонаж, машина, камера
enum { BM_TRACK, BM_LAPS, BM_DIR, BM_CHAR, BM_CAR, BM_CAM, BM_N };
static void bnsMenuDraw(int sel, int mainSel, void **fbp) {
  GE_NEST_GUARD("bnsMenuDraw");   // №1186/№1188
  sceGuStart(GU_DIRECT, list); resetState();
  sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  feDraw(mainSel); resetState();
  bnsRect(40, 28, SCR_W - 80, SCR_H - 56, 0xc0201008u);
  bnsTextC(38, 0xff40e0ffu, bnsLbl(BL_TITLE));   // MiniMenu.pag SELECT_TRACK_AND_NUM_LAPS
  char t[96]; const char *veh = bnsVeh[bsel.veh >= 0 ? bsel.veh : bnsVehOf(bnsCharCar[bsel.chr])];
  for (int r = 0; r < BM_N; r++) {
    switch (r) {
      case BM_TRACK: snprintf(t, sizeof t, "%s  %d", bnsLbl(BL_TRACK), bsel.track); break;   // правка 421: без BONUS_UNLOCK_OLD — номера 1..7 ниже
      case BM_LAPS: snprintf(t, sizeof t, "%s  x %d", bnsLbl(BL_LAPS), bsel.laps); break;                       // MiniMenu.pag NumLaps "x N"
      case BM_DIR: snprintf(t, sizeof t, "DIRECTION  %s", bsel.rev ? "REVERSE" : "NORMAL"); break;   // «Change Track Direction»
      case BM_CHAR: snprintf(t, sizeof t, "CHARACTER  %s", bnsChar[bsel.chr]); break;          // «Change Character»
      case BM_CAR: snprintf(t, sizeof t, "%s  %s", bnsLbl(BL_CAR), veh); break;
      default: snprintf(t, sizeof t, "CAMERA  %s", bsel.cam ? "BEHIND CAR" : "TOP (ORIGINAL)"); break;
    }
    int y = 70 + r * 24; unsigned c = r == sel ? 0xff40ffffu : 0xffd0d0d0u;
    if (r == sel) bnsRect(60, y - 3, SCR_W - 120, 20, 0x60ffffffu);
    if (r == BM_TRACK && !BONUS_UNLOCK_OLD) {   // правка 421: «TRACK  1 2 3 4 5 6 7» — выбранная яркая, открытые светлые, закрытые тёмные
      char d[4], tl[72]; snprintf(tl, sizeof tl, "%s  ", bnsLbl(BL_TRACK)); int w = bnsTextW(tl), dw = bnsTextW("0 "), x = (SCR_W - w - 7 * dw) / 2;
      bnsT(x, y, c, tl); x += w;
      for (int k = 1; k <= 7; k++, x += dw) { snprintf(d, sizeof d, "%d", k);
        bnsT(x, y, k == bsel.track ? 0xff40ffffu : bonusTrackIsOpen(k) ? 0xffd0d0d0u : 0xff404040u, d); }
    } else bnsTextC(y, c, t);
  }
  bnsTextC(SCR_H - 50, 0xffffffffu, bnsLbl(BL_PRESS));   // MiniMenu.pag PRESS_START
  bnsTextC(SCR_H - 30, 0xffa0a0a0u, "< > CHANGE    TRIANGLE BACK");
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf();
}
static int bnsMenuOld(int mainSel, void **fbp) {   // временный экран порта (BONUS_SCREENS_OLD / нет пакета): 1 — старт гонки, 0 — назад
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(); int sel = 0; char s[160];
  if (bsel.veh < 0) bsel.veh = bnsVehOf(bnsCharCar[bsel.chr]);
  bsel.track = bonusTrackPick(bonusOpenMask(), bsel.track);   // правка 421: закрытая трасса не остаётся выбранной
  snprintf(s, sizeof s, "BONUS ev=menu track=%d laps=%d rev=%d cam=%d char=%s car=%s open=%02x t=%.1f\n", bsel.track, bsel.laps, bsel.rev, bsel.cam,
           bnsChar[bsel.chr], bnsVeh[bsel.veh], bonusOpenMask(), smkT); bnsLog(s);
  for (;;) {
    unsigned tn = sceKernelGetSystemTimeLow(); float dt = (tn - last) * 1e-6f; last = tn; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    hit = uiPadRepeat(pad.Buttons, hit);   // №706: зажатая стрелка листает список (0,4 с, затем 0,12 с)
    SmokeCmd sc; while (smokeNext(dt, &sc)) bnsFlag(&sc);
    if (bnsGo) { bnsGo = 0; if (bonusTrackIsOpen(bsel.track)) return 1; }   // правка 421: закрытая трасса не стартует
    if (hit & PSP_CTRL_UP) sel = (sel + BM_N - 1) % BM_N;
    if (hit & PSP_CTRL_DOWN) sel = (sel + 1) % BM_N;
    int d = (hit & PSP_CTRL_RIGHT) ? 1 : (hit & PSP_CTRL_LEFT) ? -1 : 0;
    if (d) {
      switch (sel) {
        case BM_TRACK: bsel.track = bonusTrackStep(bonusOpenMask(), bsel.track, d);   // правка 421: только открытые (прежнее — все 7)
          snprintf(s, sizeof s, "BONUS ev=track d=%d track=%d open=%02x t=%.1f\n", d, bsel.track, bonusOpenMask(), smkT); bnsLog(s); break;
        case BM_LAPS: bsel.laps += d; if (bsel.laps < BR_LAPS_MIN) bsel.laps = BR_LAPS_MAX; if (bsel.laps > BR_LAPS_MAX) bsel.laps = BR_LAPS_MIN; break;
        case BM_DIR: bsel.rev = !bsel.rev; break;
        case BM_CHAR: bsel.chr = (bsel.chr + d + 5) % 5; bsel.veh = bnsVehOf(bnsCharCar[bsel.chr]); break;
        case BM_CAR: bsel.veh = (bsel.veh + d + BNS_NVEH) % BNS_NVEH; break;
        default: bsel.cam = !bsel.cam; break;
      }
    }
    if ((hit & (PSP_CTRL_START | PSP_CTRL_CROSS)) && bonusTrackIsOpen(bsel.track)) return 1;
    if (hit & (PSP_CTRL_TRIANGLE | PSP_CTRL_CIRCLE)) return 0;
    bnsMenuDraw(sel, mainSel, fbp);
  }
}

static int bnsMenu(int mainSel, void **fbp) { return bnsUiOpen() ? bnsMenuNative(fbp) : bnsMenuOld(mainSel, fbp); }

// ---------------------------------------------------------------- загрузка трассы: набор уровня по папке bonus/ (levelFile)
static int bnsPlan(void *u, const McCar *c, const float g[3], float (*out)[3], int cap) {
  (void)u; (void)g; float p[3] = {c->car.x, c->car.y, c->car.z}; return brPlan(&BR, c->wp, p, out, cap);
}
// сверка g12-data №1109 (Роб: «трасса окружена панорамой, светом и текстурами того уровня»): трасса b0N не лежит на карте уровня N (у карты в этих координатах нет земли), а кольцо-панорама
// (pano917, снимки с центров клеток уровня) годится только в 96 м от центра снимка. Поэтому трасса «ставится» на центр ближайшей к старту панорамы уровня N: виртуальный глаз = глаз трассы + сдвиг
// на этот центр (только для выбора и положения кольца; кольцо рисуется вокруг настоящей камеры). Файл панорам берёт levelFile (main_state.h) у уровня N — своих данных у трассы нет. Свет трассы
// и небо уже взяты у уровня в данных (tools/bonus/export.py: группа света совпадает с группой уровня у b02…b07, фон BACKDROP). BONUSENV1109_OLD=1 — кольца нет
static int bnsEnvT = -1, bnsEnvOk; static float bnsEnvOff[3];
static void bnsEnvPano(const float eye[3]) {
  if (bnsEnvT != bsel.track) { bnsEnvT = bsel.track; bnsEnvOk = 0; pano917Force = 1; }
  if (pano917Force) { pano917Force = 0; pano917Load(); }   // панорамы уровня трассы (levelFile при bonusTrack), не уровня игры
  if (!bnsEnvOk) { float cx, cz, cey; if (!pano917Nearest(spawnP[0], spawnP[2], &cx, &cz, &cey)) return;
    bnsEnvOff[0] = cx - spawnP[0]; bnsEnvOff[2] = cz - spawnP[2]; bnsEnvOff[1] = cey - spawnP[1] - .5f; bnsEnvOk = 1;
    if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "BONUSENV track=%d pano=%u anchor=%.0f,%.0f off=%.0f,%.0f t=%.1f\n", bsel.track, pnoN, cx, cz, bnsEnvOff[0], bnsEnvOff[2], smkT); bnsLog(s_); } }
  pano917Draw(eye[0] + bnsEnvOff[0], eye[1] + bnsEnvOff[1], eye[2] + bnsEnvOff[2]);
}
// №1168 (Роб 03.11 на PSP: «бонус-игре плюс лодинга там не было и ченскрэч ничего там не было»): запись PS2 (ps2_bonus_b01_vehicle_accept, кадр 776) — между выбором машины и гонкой
// чёрный экран LoadingFE (Itchy & Scratchy, фитиль к бомбе, надпись LOADING), тот же, что при выходе из игры в меню; без газеты уровня. Раньше у порта был кадр с одним словом LOADING и обрыв
// картинки на гонку. Теперь: меню гаснет (8 кадров), LoadingFE идёт на всё чтение трассы (ходом загрузки, как у игры), огонь доходит до динамита не раньше минимального показа, трасса
// открывается из чёрного (10 кадров). На выходе — то же: гонка гаснет, LoadingFE на время чтения гостиной (уже было), меню. BONUSLOAD1168_OLD=1 — прежнее (слово LOADING, резкая смена)
#ifndef BONUSLOAD1168_OLD
#define BONUSLOAD1168_OLD 0
#endif
#define BNS_FADEIN_FRAMES 10
static void mem1060Fade(void **fbp);   // mem1060.h (подключён позже): мир гаснет за 8 кадров поверх последнего кадра
static int bnsFadeIn;   // кадров осталось до конца появления трассы из чёрного (bnsFrame накладывает чёрный слой)
static int bnsKeepLv = -1;   // номер уровня игры на время загрузки трассы (gameLevel ставится в 1 раньше, чем loadBegin)
static int bnsLoadIn(void **fbp) {   // 1 — LoadingFE открыт и идёт; 0 — пакет load.shr не открылся (старый кадр LOADING)
  bnsUiClose();   // mini.shr — до load.shr: пакет экранов один
  int rc = scrOpen1065("bonus"); if (rc != 0) { if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "BONUS ev=loadscr fe=0 rc=%d t=%.1f\n", rc, smkT); bnsLog(s_); } return 0; }
  scrTvOver = 0; bnsKeepLv = gameLevel; gameLevel = 1; bonusTrack = bsel.track;   // levelFile трассы уже сейчас: loadBegin читает заголовок level.shr через него (levelPreread)
  ldScr1060 = UI_LOAD_FE; ldMenuNew = 0; ldBase = LOAD_RES_SHARE; scrT0 = sceKernelGetSystemTimeLow();
  scrFrame(UI_LOAD_FE, 0, 1, fbp); loadBegin(fbp);
  return 1;
}
static void bnsLoadOut(void **fbp, int ok) {   // чтение кончилось: огонь доходит до динамита (не раньше минимального показа), экран закрыт
  unsigned t0 = sceKernelGetSystemTimeLow();
  loadFuseIoEnd(); fuseProbeFlush(); ldWarmStep_ = 0;
  while (ldOn && ldShown < 1000) { scrFrame(ldScr1060, ldProgress(1000), 1, fbp); if (ldShown > 500) { sceDisplayWaitVblankStart(); sceDisplayWaitVblankStart(); bnsShot(6, "bonus_load"); } }   // снимок экрана загрузки (flag bonusshots 1)
  ldOn = 0; ldBase = 0; ldWarmOn = 0; ldMenuNew = 0;
  SCR_CLOSE(); ldScr1060 = UI_LOAD_LEVEL;
  if (bnsKeepLv >= 0) gameLevel = bnsKeepLv; bnsKeepLv = -1;
  if (smkOn) { char s_[128]; snprintf(s_, sizeof s_, "BONUS ev=loadscr fe=1 ok=%d hold_ms=%.0f heap=%u t=%.1f\n", ok, (sceKernelGetSystemTimeLow() - t0) / 1000.f, heapFreeBytes(), smkT); bnsLog(s_); }
}
static int bnsLoad0(void **fbp, int fe);
static int bnsLoad1(void **fbp);
static int bnsLoad(void **fbp) {   // №1206: время перехода; Build 95 П2: класс BONUS виден машине переходов (TFSM bonus ...: при отказе simplify и WATCHDOG, не только строка BONUS ev=)
  unsigned t0_ = sceKernelGetSystemTimeLow(); Transit tb_; memset(&tb_, 0, sizeof tb_); tb_.name = "bonus"; tb_.screen = TFS_NEWS; tb_.need = 2u << 20;
  if (!tfsm95Old) tfExtBegin(&tb_);
  int r_ = (!tfsm95Old && tfFailBonus > 0) ? (tfFailBonus--, 0) : bnsLoad1(fbp); xnEnd("bonus_load", t0_, (int)gameLevel);
  if (!tfsm95Old) tfExtEnd(&tb_, r_);
  return r_; }
static int bnsLoad1(void **fbp) {
  if (BONUSLOAD1168_OLD) return bnsLoad0(fbp, 0);
  mem1060Fade(fbp);   // меню (или «RACE AGAIN?») гаснет, не обрывается
  int fe = bnsLoadIn(fbp); int ok = bnsLoad0(fbp, fe);
  if (fe) bnsLoadOut(fbp, ok);
  if (ok) bnsFadeIn = BNS_FADEIN_FRAMES;
  return ok;
}
static int bnsLoad0(void **fbp, int fe) {
  GE_NEST_GUARD_RET("bnsLoad0", 0);   // №1186/№1188
  unsigned t0 = sceKernelGetSystemTimeLow(); char s[200], fn[256];
  // правка 420: кадр «LOADING» над тёмным фоном, пока читается трасса (экран загрузки игры — газета уровня, у мини-игры своей нет)
  if (!fe) {
  sceGuStart(GU_DIRECT, list); resetState(); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  bnsTextC(SCR_H / 2 - 8, 0xffffffffu, bnsLbl(BL_LOADING)); sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  snprintf(fn, sizeof fn, BONUS_BNS, bsel.track); { char p[256]; sibling(p, fn); strcpy(fn, p); }
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return 0;
  int n = (int)sceIoLseek32(f, 0, PSP_SEEK_END); sceIoLseek32(f, 0, PSP_SEEK_SET);
  free(bnsMem); bnsMem = memalign(16, (unsigned)n);
  if (!bnsMem || sceIoRead(f, bnsMem, (unsigned)n) != n) { sceIoClose(f); return 0; }
  sceIoClose(f);
  if (bnsParse(&BT, bnsMem, (unsigned)n)) { snprintf(s, sizeof s, "BONUS ev=load bad=bns track=%d t=%.1f\n", bsel.track, smkT); bnsLog(s); return 0; }
  int keepLevel = bnsKeepLv >= 0 ? bnsKeepLv : gameLevel; gameLevel = 1; bonusTrack = bsel.track;   // набор трассы: levelFile -> bonus/ (miss — пакета нет, модуль молчит)
  if (loadedLevel) levelUnload();   // в памяти уровень игры (выход в меню из игры) — в кучу; menuRun прочтёт его снова (loadedLevel 0)
  if(!BONUS_SOUND_OLD){sndShutdown();sndLevel=-1;} // track is an audio namespace, even when previous level wasL1
  levelLoad(fbp); gameLevel = keepLevel;
  if (!haveCol) { snprintf(s, sizeof s, "BONUS ev=load bad=col track=%d t=%.1f\n", bsel.track, smkT); bnsLog(s); return 0; }
  if (bonusShrink107) {   // Лестница-96 (блокер 102, канон п.14): трасса целиком идёт в память напрямую (ниже), поэтому до чтения отцепляем пустые области Z3 (город/место/миссия), если кучи не хватает; иначе они держат ~10 МБ и бонус даёт MEMFAIL
    unsigned need_ = 2u << 20, hf_ = heapFreeBytes(); for (unsigned i = 0; i < ncell; i++) if (!cl[i].owner && !cl[i].state) need_ += cl[i].size;
    int g_ = 0; for (; g_ < 4 && hf_ < need_; g_++) { unsigned fr_ = zone3ShrinkX(1, "bonus"); if (!fr_) break; hf_ += fr_; }
    if (smkOn) { char l_[140]; snprintf(l_, sizeof l_, "BONUS ev=mem track=%d need_kb=%u heap_kb=%u->%u shrink=%d t=%.1f\n", bsel.track, need_ >> 10, (unsigned)(hf_ - 0) >> 10, heapFreeBytes() >> 10, g_, smkT); bnsLog(l_); }
  }
  float c[3] = {spawnP[0], spawnP[1], spawnP[2]}; int cells = cellsRingAt(c, ldOn ? 1 : 0, fbp);   // трасса целиком (100–250 м) — до первого кадра
  for (unsigned i = 0; i < ncell; i++) if (!cl[i].state) { cl[i].wantFar = 0; SceUID fd = sceIoOpen(levelShr, PSP_O_RDONLY, 0); loadCell(fd, i); sceIoClose(fd); cells++; if (ldOn) loadPump(); }
  snprintf(s, sizeof s, "BONUS ev=load track=%d cells=%d ncell=%u col=%d ms=%.0f heap=%u t=%.1f\n", bsel.track, cells, ncell, haveCol,
           (sceKernelGetSystemTimeLow() - t0) / 1000.f, heapFreeBytes(), smkT); bnsLog(s);
  return 1;
}

// круг 5 (g07b, P17-19): соперники — три машины ИИ случайно из списка 67 имён машин игры (данные диска), одна и та же не повторяется между четырьмя местами; три персонажа ИИ — случайно из пяти
// (Лиза, Барт, Гомер, Мардж, Апу), не повторяя занятых человеком, при занятом — следующий по кругу. Машины, которых нет в пакете msncars-b.shr, пропускаются (пакет пересобирается tools/bonus/export.py
// с полным списком). Было: машина ИИ = машина персонажа, у b01 с Лизой — запись PS2 одного прогона. BONUS_RANDOM_AI_OLD=1 — прежнее
#ifndef BONUS_RANDOM_AI_OLD
#define BONUS_RANDOM_AI_OLD 0
#endif
static const char *const bnsAiCars[] = {"snake_v", "bookb_v", "marge_v", "carhom_v", "krust_v", "bbman_v", "elect_v", "famil_v", "bart_v", "scorp_v", "honor_v", "hbike_v", "frink_v", "comic_v",
  "lisa_v", "smith_v", "mrplo_v", "fone_v", "cletu_v", "apu_v", "plowk_v", "wiggu_v", "otto_v", "moe_v", "skinn_v", "homer_v", "zombi_v", "burns_v", "willi_v", "gramp_v", "gramR_v", "atv_v",
  "knigh_v", "mono_v", "oblit_v", "hype_v", "rocke_v", "cArmor", "cCellA", "cSedan", "cCola", "cCube", "cCurator", "cDonut", "cDuff", "cBlbart", "cHears", "cKlimo", "cLimo", "cMilk", "cNerd",
  "cNonup", "cPolice", "cVan", "cBone", "compactA", "minivanA", "pickupA", "sedanA", "sedanB", "sportsA", "sportsB", "wagonA", "SUVA", "taxiA", "coffin", "hallo"};
#define BNS_NAICARS ((int)(sizeof bnsAiCars / sizeof bnsAiCars[0]))
static int bnsHasCar(const char *nm) { if (!mcrDir) return 0; for (unsigned i = 0; i < mcrN; i++) if (!strncasecmp((const char *)mcrDir + i * 176, nm, 16)) return 1; return 0; }   // модель есть в каталоге пакета (без чтения блока)
static unsigned bnsRng;
static unsigned bnsRnd(void) { bnsRng = bnsRng * 1664525u + 1013904223u; return bnsRng >> 8; }
static void bnsPickAi(void) {   // bnsCarName[1..3], bnsRacerChr[1..3]
  bnsRng = smkOn ? 0x9e3779b1u ^ (unsigned)bsel.track : sceKernelGetSystemTimeLow() ^ 0x5bd1e995u;   // в смоуке — фиксированное зерно (гейты воспроизводимы)
  int usedChr = 1 << bsel.chr;
  for (int i = 1; i < 4; i++) {
    int c = (int)(bnsRnd() % 5); while (usedChr >> c & 1) c = (c + 1) % 5;
    usedChr |= 1 << c; bnsRacerChr[i] = c;
    for (int tries = 0; tries < 400; tries++) {
      char nm[16]; bnsLower(bnsAiCars[bnsRnd() % BNS_NAICARS], nm, 16);
      int dup = 0; for (int j = 0; j < i; j++) if (!strcmp(bnsCarName[j], nm)) dup = 1;
      if (dup || !bnsHasCar(nm)) continue;
      BNS_CPY16(bnsCarName[i], nm); break;
    }
    if (!bnsCarName[i][0]) BNS_CPY16(bnsCarName[i], bnsCharCar[c]);   // нет подходящей модели в пакете — машина персонажа
  }
}
static McCar bnsSecCar; static McEnv bnsSecEnv;
static int bnsSecPlan(void *u, const float pos[3], float hdg, const float g[3], float (*out)[3], int cap) {   // №1185: план маршрута для листа секторов — как у водителя соперника
  (void)u; bnsSecCar.car.x = pos[0]; bnsSecCar.car.y = pos[1]; bnsSecCar.car.z = pos[2]; bnsSecCar.car.h = hdg; return mcPlanFor(&BM, &bnsSecEnv, &bnsSecCar, g, out, cap);
}
static void bnsRaceInit(void) {
  // машина игрока — car1, соперники — car2..car4 (правило порта); ходовые игрока — .con выбранной машины, кузов — модель пакета
  brInit(&BR, &BT, bsel.laps, bsel.rev, 4, 0); bnsGoDone = 0; memset(bnsAiFlipT, 0, sizeof bnsAiFlipT);
  const char *pv = bnsVeh[bsel.veh]; bnsLower(pv, bnsCarName[0], 16);
  float p[3], h; brStart(&BR, 0, p, &h);
  CarParams P; const McGeo *G = mcGeoFind(pv);
  if (!(G && G->hasInfo && carParamsFromInfo(G->info, &P) == 1)) carParamsFor(bnsCarName[0], &P);
  carInit(&car, &P, &world, p[0], p[1], p[2], h); BR.grow[0] = car.p.halfW; chase.init = 0;
  mcInit(&BM); mcPlanHook = 0; mcSprint = 0; bnsRoads = 0;
  // №1116 (сверка g03-aicars, правка Роба 1116): соперники ведутся по дорогам трассы от точки WPR (обратный ход — WPL) к следующей, как у оригинала
  // (по дорогам файла трассы); дорога и путь планируются тем же планировщиком msncar.c, что и улицы уровней. Было: свой плотный
  // путь порта (bnsPlan -> brPlan, tools/bonus/nav.py). BONUS_ROADS_OLD=1 или файл без хвоста дорог — прежнее
  if (!BONUS_ROADS_OLD && BT.rd && trafficInitMem(&bnsTr, (void *)BT.rd, BT.rdSize, 0) == 0 && (bsel.rev ? BT.h->nwpr : BT.h->nwpf) >= 2) { bnsRoads = 1; mcSprint = 1; }
  else mcPlanHook = bnsPlan;
  if (!BONUS_RANDOM_AI_OLD) { for (int i = 1; i < 4; i++) bnsCarName[i][0] = 0; bnsPickAi(); }
  for (int i = 1, k = 0; i < 4; i++) {
    if (!BONUS_RANDOM_AI_OLD) { k = bnsRacerChr[i]; goto aiPicked; }
    while (k == bsel.chr) k++;
    bnsRacerChr[i] = k;
    if (!BONUS_NPC_MODELS_OLD && bsel.track == 1 && bsel.chr == 0) {
      static const int nativeChr[3] = {2, 3, 1}; // P2 Homer, P3 Marge, P4 Bart
      bnsRacerChr[i] = nativeChr[i - 1];
      BNS_CPY16(bnsCarName[i], bnsNpcCar[i - 1]);
    } else BNS_CPY16(bnsCarName[i], bnsCharCar[k]); k++;
    aiPicked:;
    char con[24]; snprintf(con, sizeof con, "bonus/bg%d_%d", bsel.track, i);   // ходовые соперников — bgT_K.con (данные)
    brStart(&BR, i, p, &h);
    McCar *c = mcSpawn(&BM, &world, bnsRoads ? &bnsTr : NULL, bnsCarName[i], p, h, 1, "race", con, bnsChar[bnsRacerChr[i]]);
    if (c) BR.grow[i] = c->car.p.halfW;
  }
  if (bnsRoads) { const BnsWp *w_ = bsel.rev ? BT.wpr : BT.wpf; int nw_ = (int)(bsel.rev ? BT.h->nwpr : BT.h->nwpf);   // точки WPR/WPL в порядке круга (экспорт: lap_cycle)
    for (int k = 0; k < nw_; k++) mcWaypoint(&BM, k, w_[k].p, BR_LAPS_MAX + 1); }
  else for (int k = 0; k < BR.nwp; k++) mcWaypoint(&BM, k, BR.wp[k], BR_LAPS_MAX + 1);   // круги считает brUpdate
  bnsSec.n = bnsSec.nsec = 0; bnsCodeLast = 0;   // №1185: сектора — по следу круга лидера гонки полигона (bonus/bNN.sec, tools/bonus/sec_export.sh); файла нет — по плану маршрута водителя соперника
  { char fn_[256]; snprintf(fn_, sizeof fn_, "bonus/b%02d.sec", bsel.track); char p_[256]; sibling(p_, fn_); SceUID f_ = sceIoOpen(p_, PSP_O_RDONLY, 0);
    if (f_ >= 0) { static unsigned char sb_[BSC_MAXP * 12 + 16]; int n_ = sceIoRead(f_, sb_, sizeof sb_); sceIoClose(f_); if (n_ > 8) bscLoad(&bnsSec, sb_, (unsigned)n_); } }
  if (bnsSec.nsec < 1 && bnsRoads && BT.h->nwpf >= 2 && BT.pf && BT.h->npathf > 0 && BM.n >= 1) {   // запасной путь: план — копия водителя первого соперника
    bnsSecCar = BM.c[0]; memset(&bnsSecEnv, 0, sizeof bnsSecEnv); bnsSecEnv.w = &world; bnsSecEnv.tr = &bnsTr;
    bscInitRoute(&bnsSec, bnsSecPlan, 0, BT.wpf, (int)BT.h->nwpf, BT.pf[0].p); }
  memset(&bnsBot, 0, sizeof bnsBot); bnsBotS = 0.f; bnsBotLine = 0; bnsBotWi = -1; bnsBotProgT = 0.f; bnsBot.mode = MC_RACE; BNS_CPY16(bnsBot.name, bnsCarName[0]);
  bonusCarsPtr = &BM; bonusPlayerModel = bnsCarName[0];
  bnsRacerChr[0] = bsel.chr;
  if (!BONUS_SCREENS_OLD) bnsUiOpen();   // экраны пакета (пауза, итоги): load.shr трассы его закрыл
  bnsFlyLoad();
  // подборы и ловушки трассы: объекты ANM1 (pistons / spikes) и модели подборов в SPR1 (по точке сферы подбора)
  brTrapBind(&BR, haveAo ? &ao : 0); memset(bnsTint, 0, sizeof bnsTint);
  int mapped = 0;
  for (unsigned q = 0; q < BNS_MAXPICK; q++) {
    bnsPickInst[q] = -1;
    if (q >= BT.npick || !haveSP) continue;
    for (unsigned i = 0; i < sprops.ninst; i++) {
      const float *P = BT.pk[q].p; float dx = sprops.inst[i].x - P[0], dy = sprops.inst[i].y - P[1], dz = sprops.inst[i].z - P[2];
      if (dx * dx + dy * dy + dz * dz < .01f) { bnsPickInst[q] = (int)i; mapped++; break; }
    }
  }
  char s[200]; snprintf(s, sizeof s, "BONUS ev=items track=%d picks=%u models=%d haz=%u traps=%d trapobj=%u death=%u sp=%d ao=%d t=%.1f\n", (int)BT.h->track, BT.npick, mapped,
                        BT.nhaz, BR.ntrap, BT.h->ver >= 2 ? (unsigned)BT.h->ntrapobj : 0u, haveCol ? (unsigned)world.ndeath : 0u, haveSP, haveAo, smkT); bnsLog(s);
}

// подборы, нитро, подброс, DEATH и свет машины i (0 — игрок, 1..3 — соперники); thr — газ (соперник — 1, правило порта)
static void bnsEffects(int i, Car *k, float thr, float dt) {
  char s[160]; const char *nm = bnsCarName[i];
  if (BR.state == BR_RACE && !BR.r[i].finished) {
    int q = brPick(&BR, i, k);
    if (q >= 0) { char n_[13]; memcpy(n_, BT.pk[q].name, 12); n_[12] = 0;
      snprintf(s, sizeof s, "BONUS ev=pick car=%d name=%s kind=%s item=%s t=%.1f\n", i, nm, BT.pk[q].kind == BNS_PK_NITRO ? "nitro" : "wrench", n_, smkT); bnsLog(s);
      }   // звук подбора не подключён: в банке бонус-игры (tools/bonus/audio_export.py) нет wrench_collect — открыто
    brBoost(&BR, i, k, thr, dt);
    if (i > 0 && brAiHorn(&BR, i, k, dt)) { snprintf(s, sizeof s, "BONUS ev=horn car=%d name=%s nitro=%d t=%.1f\n", i, nm, BR.nitro[i], smkT); bnsLog(s); }
    if (brLaunch(&BR, i, k)) { snprintf(s, sizeof s, "BONUS ev=launch car=%d name=%s vy=%.1f t=%.1f\n", i, nm, k->vel[1], smkT); bnsLog(s); }
  }
  if (!BONUS_AIFLIP_OLD && i > 0 && BR.state == BR_RACE) {   // сверка g07b P04 B3: ИИ перевёрнут или разбит — через 1 с на месте (разбитый с ремонтом)
    // круг 3 (проверка g07b): наклон считается по вектору «вверх» кузова — вертикальная составляющая не выше 0,26 (больше 75° от вертикали), скорость не учитывается, отсчёт 1 с с первого кадра наклона
    // (раньше ждали flipT машины ≈ 2 с и скорость < 3 м/с); вышла из наклона — отсчёт сбрасывается (кроме разбитой). BONUS_AIFLIP3_OLD=1 — прежнее
    float upY_ = brUpY(k->q);
    int flip_;
    if (BONUS_AIFLIP3_OLD) { int tilt_ = k->flipT >= 1.f; if (tilt_ || k->destroyed) bnsAiFlipT[i] += dt; else bnsAiFlipT[i] = 0.f; flip_ = bnsAiFlipT[i] >= 1.f; if (flip_) bnsAiFlipT[i] = 0.f; }
    else flip_ = brAiFlipStep(upY_, k->destroyed, &bnsAiFlipT[i], dt);   // круг 6: порог cos75° = 0,258819 (было 0,26 на глаз), логика в bonus.h — её гоняет хост-тест
    if (flip_) { k->damage = 0.f; k->destroyed = 0; k->deadT = 0.f; carPlace(k, &world, k->x, k->y + 1.f, k->z, k->h); BM.c[i - 1].nroute = 0;
      snprintf(s, sizeof s, "BONUS ev=right car=%d name=%s t=%.1f\n", i, nm, smkT); bnsLog(s); }
  }
  if (!BONUS_AUTORESET_OLD && BR.state == BR_RACE && !BR.r[i].finished && carNeedsAutoReset(k)) {   // сверка g07b круг 2 (g01-cars, P17 Д1): автосброс всех четырёх машин по правилам оригинала (вверх колёсами в покое сразу, на боку дольше 1 с), без вспышки, на путь гонки
    float q[3], qh, pp[3] = {k->x, k->y, k->z};
    if (brRespawn(&BR, i, pp, q, &qh)) { carPlace(k, &world, q[0], q[1] + .5f, q[2], qh); if (i == 0) chase.init = 0; else BM.c[i - 1].nroute = 0;
      snprintf(s, sizeof s, "BONUS ev=autoreset car=%d name=%s at=%.1f,%.1f t=%.1f\n", i, nm, q[0], q[2], smkT); bnsLog(s); }
  }
  if (brDeath(&BR, i, k, &world)) {
    if (i == 0) chase.init = 0; else BM.c[i - 1].nroute = 0;
    snprintf(s, sizeof s, "BONUS ev=death car=%d name=%s at=%.1f,%.1f,%.1f h=%.2f t=%.1f\n", i, nm, k->x, k->y, k->z, k->h, smkT); bnsLog(s);
  }
  float lp[3] = {k->x, k->y + .5f, k->z}; uint32_t L = brLight(&BR, lp);
  if (L && !bnsTint[i]) { BR.nLight++; snprintf(s, sizeof s, "BONUS ev=light car=%d name=%s argb=%08X mul=%08X t=%.1f\n", i, nm, (unsigned)L, (unsigned)brLightMul(L), smkT); bnsLog(s); }
  bnsTint[i] = L;
}

// ---------------------------------------------------------------- кадр
static unsigned bnsTris; static float bnsTopDist;
static BrAudio bnsAudio;
#include "bonus_driver.h"
static void bnsCarDraw(const Car *k, const char *name, float wheel, uint32_t tint) {
  const McrModel *M = mcrGet(name); if (!M) return;
  // свет зоны (Event 65): цвет вершин машины × множитель зоны — освещение GE без источников, окружающий свет = множитель,
  // материал «окружающий» = цвет вершины (у машин порта цвет вершин — готовый свет). Полупрозрачные куски (очередь tq) — без него
  if (tint) { sceGuEnable(GU_LIGHTING); for (int l = 0; l < 4; l++) sceGuDisable(GU_LIGHT0 + l);
    sceGuModelColor(0, 0xffffffffu, 0, 0); sceGuColorMaterial(GU_AMBIENT); sceGuAmbient(brLightMul(tint)); }
  int b = (int)(mcrTexBase + M->tex0); trafficDrawModel(M->M, M->pc, M->vt, k, wheel, 0, mcDrawPiece, &b, &mcrTris);
  bnsDriverDraw(k, name, M);   // №1219: фигура водителя (под тем же светом зоны)
  if (tint) sceGuDisable(GU_LIGHTING);
}
// №1148 (Роб, бонус-игра: «у нитро на трассе нет свечения — как у гаечного ключа»): блик нитро — тот же векторный шейп (glintshape941.h), ход слоёв (glintmotion970.h), размер и прозрачность, что у ключа
// (spritelight.h wrGlowDraw: gvAdd, пачка одним вызовом), в центре модели нитро (центр коробки типа). Цвета — данные нитро (art/missions/generic/nitro.p3d, три плашки BQG): nitroShape3 (238,35,124) = 0xEE237C
// большая, nitroShape4 (255,91,72) = 0xFF5B48 вторая (nitroShape5 (255,70,64) — как у ключа третий слой, идёт в шейп вторым цветом). Свои плашки нитро (сложением) не рисуются — они дали бы второй блик
// поверх. Видим только не подобранный (brPickShown), дальше 40 м не рисуется, дальше 30 м гаснет. NITROGLINT1148_OLD=1 — как было (без блика нитро)
#ifndef NITROGLINT1148_OLD
#define NITROGLINT1148_OLD 0
#endif
static int bnsNitroInst(unsigned i) {
  for (unsigned q = 0; q < BT.npick && q < BNS_MAXPICK; q++) if (bnsPickInst[q] == (int)i && BT.pk[q].kind == BNS_PK_NITRO) return 1;
  return 0;
}
static void bnsNitroGlints(float ex, float ey, float ez) {
  if (NITROGLINT1148_OLD || !haveSP) return;
  float P[8 * 3], CY[8]; int n = 0;
  for (unsigned q = 0; q < BT.npick && q < BNS_MAXPICK && n < 8; q++) {
    int i = bnsPickInst[q]; if (BT.pk[q].kind != BNS_PK_NITRO || i < 0 || !sprops.run[i].alive) continue;
    memcpy(P + n * 3, BT.pk[q].p, 3 * sizeof(float)); CY[n] = sprops.type[sprops.inst[i].type].c[1]; n++;   // центр модели — центр коробки типа по высоте
  }
  if (n) nitroGlintBatch1148(P, CY, n, ex, ey, ez);
}
// модели SPR1 трассы: подборы (wrench, nitro), объекты ловушек (pistons, spikes) и прочие анимированные объекты — как стейтпропы
// уровня (viewer/main.c): куски на суставах, потом билборды (свечение подбора)
static void bnsSprops(float ex, float ey, float ez, float maxD) {
  if (BONUS_PICKUPS_OLD || !haveSP) return;
  for (unsigned i = 0; i < sprops.ninst; i++) {
    if (!spDrawable(&sprops, i, ex, ez, maxD)) continue;
    if (geOver(GE_SOFT)) break;
    const SpType *T = &sprops.type[sprops.inst[i].type];
    spPose(&sprops, i);
    for (unsigned k = 0; k < T->nPiece; k++) {
      if (!spPieceOn(&sprops, i, k)) continue;
      const SpPiece *pp = &sprops.piece[T->firstPiece + k];
      if (!pp->ni) continue;
      float m[16]; spPieceMatrix(&sprops, i, k, m);
      Piece tmp; memset(&tmp, 0, sizeof tmp);
      tmp.r.tex = pp->tex; tmp.r.flags = pp->flags & 0xFF; tmp.r.ni = pp->ni;
      tmp.v = (Vert *)((char *)sprops.vertBase + pp->vOff); tmp.idx = (unsigned short *)((char *)sprops.idxBase + pp->iOff);
      drawPieceM(&tmp, (const ScePspFMatrix4 *)m);
    }
  }
  int nb = 0;
  for (unsigned i = 0; i < sprops.ninst; i++) {
    if (!spDrawable(&sprops, i, ex, ez, maxD)) continue;
    if (geOver(GE_SOFT)) break;
    const SpType *T = &sprops.type[sprops.inst[i].type];
    if (T->nBill) spPose(&sprops, i);
    for (unsigned b = 0; b < T->nBill; b++) {
      float v[12]; uint32_t col, bfl; int bt;
      if (!spBillQuad(&sprops, i, b, ex, ey, ez, v, &col, &bt, &bfl) || !(col >> 24)) continue;
      if (neonArchBig957(v, bfl)) continue;   // №957б: арка, вывеска — без свечения
      if (!NITROGLINT1148_OLD && ((bfl >> 8) & 0xFF) == 2 && bnsNitroInst(i)) continue;   // №1148: у нитро блик свой (bnsNitroGlints), плашки сложением не рисуются
      if (!GLOW_GLINT_OLD && ((bfl >> 8) & 0xFF) == 2) { gq944Site = "bonus"; glowQuadQ(v, col, (float)i * .41f + (float)b); continue; }   // №839: свечение — бликом
      if (!nb++) {
        ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
        sceGuSetMatrix(GU_MODEL, &I); sceGuTexScale(1.f, 1.f);
        sceGuDisable(GU_ALPHA_TEST); sceGuEnable(GU_BLEND); sceGuDepthMask(GU_TRUE); curFlags = -1; setCull(0);
      }
      int blm = (bfl >> 8) & 0xFF; g944Bill("bonus_bill", blm, bt, v, col, i, b);   // №944; как viewer/main.c: 2 — сложение, 3 — вычитание, иначе прозрачность; у свечения тумана нет (правка 76)
      if (blm == 2) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
      else if (blm == 3) sceGuBlendFunc(GU_REVERSE_SUBTRACT, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
      else sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
      if (blm == 2 && !glowFogOld) sceGuDisable(GU_FOG); else if (bsel.cam) sceGuEnable(GU_FOG);
      typedef struct { float u, v; unsigned c; float x, y, z; } BV;
      BV *q = (BV *)sceGuGetMemory(4 * sizeof(BV));
      for (int k = 0; k < 4; k++) { q[k].u = (float)(k & 1); q[k].v = spBillTexV(k, spBillUvOld); q[k].c = col; q[k].x = v[3 * k]; q[k].y = v[3 * k + 1]; q[k].z = v[3 * k + 2]; }
      bindTexLean(bt);
      sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 4, 0, q);
    }
  }
  glowGlintFlush(ex, ey, ez);   // №839
  bnsNitroGlints(ex, ey, ez);   // №1148
  if (nb) { sceGuTexScale(128.f, 128.f); if (bsel.cam) sceGuEnable(GU_FOG); else sceGuDisable(GU_FOG);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0); resetState(); }
}
// №1182 (эталон PS2 от emu-ps2: в гонке у оригинала нет ни кругов, ни времени — единственный ориентир цветной ярлык «P1…P4» с треугольником над каждой машиной: красный, голубой, жёлтый, зелёный).
// Круги, место и нитро у нас остаются по слову Роба (№1145/№1179), ярлыки добавлены к ним. Вид сверху; BONUSMARK1182_OLD=1 — без ярлыков
#ifndef BONUSMARK1182_OLD
#define BONUSMARK1182_OLD 0
#endif
static float bnsPUp[3]; static int bnsMarkMode, bnsHudQuiet;   // «вверх» кадра гонки и «рисовать ли ярлыки» (задаёт bnsFrame)
// аудит типографики 05.11 п. 11 и 21: метка игрока P1 поверх остальных (рисуется последней), у каждой метки чёрная тень 1 px, на паузе меток нет. BONUSMARK1198_OLD=1 — прежнее
#ifndef BONUSMARK1198_OLD
#define BONUSMARK1198_OLD 0
#endif
static void bnsMarkers(void) {
  if (BONUSMARK1182_OLD || !bnsMarkMode || bsel.cam) return;
  const float *u = bnsPUp, f[3] = {cfX, cfY, cfZ};
  float r[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]}, rl = sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
  if (rl < 1e-4f) return;
  for (int k = 0; k < 3; k++) r[k] /= rl;
  float t[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};   // истинный «вверх» кадра
  float th = tanf(curFovY * .5f * (GU_PI / 180.f));
  static const unsigned col[4] = {0xff3030e6u, 0xffe0c820u, 0xff20d8f0u, 0xff30c850u};   // ABGR: красный, голубой, жёлтый, зелёный (как на PS2)
  for (int ii = 0; ii < 4; ii++) {
    int i = BONUSMARK1198_OLD ? ii : 3 - ii;
    const Car *c = i == 0 ? &car : (i - 1 < BM.n ? &BM.c[i - 1].car : 0); if (!c) continue;
    float d[3] = {c->x - eyeX, c->y + .5f - eyeY, c->z - eyeZ}, z = d[0] * f[0] + d[1] * f[1] + d[2] * f[2];
    if (z < 1.f) continue;
    float sx = SCR_W * .5f + (d[0] * r[0] + d[1] * r[1] + d[2] * r[2]) / z / (th * ASPECT) * SCR_W * .5f;
    float sy = SCR_H * .5f - (d[0] * t[0] + d[1] * t[1] + d[2] * t[2]) / z / th * SCR_H * .5f;
    if (sx < 14.f || sx > SCR_W - 14.f || sy < 22.f || sy > SCR_H - 8.f) continue;
    char s[4]; snprintf(s, sizeof s, "P%d", i + 1);
    int w = bnsTextW(s), x0 = (int)sx - w / 2, y0 = (int)sy - 24;
    bnsT(x0, y0, col[i], s);
    if (!BONUSMARK1198_OLD) for (int q = 0; q < 5; q++) bnsRect((int)sx - (4 - q) + 1, y0 + 12 + q + 1, 2 * (4 - q) + 1, 1, 0xff000000u);   // тень треугольника
    for (int q = 0; q < 5; q++) bnsRect((int)sx - (4 - q), y0 + 12 + q, 2 * (4 - q) + 1, 1, col[i]);   // треугольник вниз, остриём на машину
  }
}
#ifndef BONUSCODE1198_OLD
#define BONUSCODE1198_OLD 0   // аудит 05.11 п. 14: код места на паузе — белый непрозрачный, угол как у паузы основной игры. 1 — прежнее
#endif
// №1185: код места на трассе («B4-12») — служебная строка R+SELECT во время гонки и на паузе у человека (как код места «L1 J4-5» основной игры); viewer/bnssector.h, гейт tests/bonus1167/sector.sh.
// Строка BONUSSECTOR в журнале — при каждой смене сектора машины игрока. flag bonuscode 1 — показать без кнопок (смоук). BONUSSECTOR1185_OLD=1 — кода нет
static int bnsCodePause;
static void bnsSectorDraw(void) {
  if (BONUSSECTOR1185_OLD || bnsSec.nsec < 1) return;
  char c[16]; bscCode(&bnsSec, bsel.track, car.x, car.y, car.z, c, sizeof c);
  int k = bscSector(&bnsSec, car.x, car.y, car.z);
  if (smkOn && k != bnsCodeLast) { char s_[96]; snprintf(s_, sizeof s_, "BONUSSECTOR code=%s pos=%.1f,%.1f t=%.1f\n", c, car.x, car.z, smkT); bnsLog(s_); } bnsCodeLast = k;
  if (bnsCodeOn) { resetState();
    if (BONUSCODE1198_OLD) bnsT(SCR_W - 10 - bnsTextW(c), 10, 0xb0ffffffu, c);
    else if (bnsCodePause) bnsT(10, 259 - uiFontLh(bnsFont()), 0xffffffffu, c);   // аудит п. 14: пауза — тот же угол (внизу слева, низ строки 259 вровень с ACCEPT), что у кода места паузы основной игры; белый, тень 1 px
    else bnsT(SCR_W - 10 - bnsTextW(c), 10, 0xffffffffu, c); }   // №1182: справа вверху — слева внизу подсказка паузы, слева вверху HUD гонки
}
// №1196 (Роб: «место и круги — теми же рисованными цифрами, что в HUD уличных гонок; третья строка нитро — нашим пиксельным шрифтом; место на финише и прочие надписи — как в основной игре»):
// круг, место и время — метки HUD основной игры (UI_T_LAP / UI_T_POSITION / UI_T_TIMER, их стопка и шрифт), нитро — строка нашей гранью под стопкой. BONUSHUD1196_OLD=1 — прежние строки пиксельным шрифтом
#ifndef BONUSHUD1196_OLD
#define BONUSHUD1196_OLD 0
#endif
#ifndef BONUSNITRO1211_OLD
#define BONUSNITRO1211_OLD 0   // №1211 (Роб 05.11: «пиксельным оно слишком большое… в обычный размер мелким, нитро 3»); 1 — нитро ×2, как было
#endif
static void bnsHud(int paused, int psel) {
  char t[96]; const BrRacer *r = &BR.r[0]; unsigned hudT_ = 0; int hudFin_ = 0;
  if (BONUSMARK1198_OLD || !bnsHudQuiet) bnsMarkers();   // аудит 05.11 п. 21: на паузе ярлыки машин прячутся вместе с HUD
  if (bnsHudQuiet) return;   // пауза: круги, место и нитро не рисуются (доска паузы закрывала их наполовину); ярлыки машин остаются
  const int bf_ = bnsFont(), hk_ = BONUSHUDK1179 < 1 ? 1 : BONUSHUDK1179, ly0 = 10, lstep = bf_ >= 0 ? uiFontLh(bf_) * hk_ + 3 : 20;   // №1145: строки HUD гонки шагом высоты грани; плашки под ними нет — как у цифр HUD основной игры (тень на пиксель)
  if (bf_ < 0) bnsRect(6, 6, 150, BR_NITRO3_OLD ? 46 : 64, 0x80000000u);   // круг 5 (P07:17): строка запаса нитро
  if (!BONUSHUD1196_OLD) {
    int lap_ = r->lap + (r->finished ? 0 : 1); if (lap_ > BR.laps) lap_ = BR.laps;
    uiHudSet(UI_F_LAP, lap_); uiHudSet(UI_F_LAPS, BR.laps); uiHudSet(UI_F_POSITION, r->place); uiHudSet(UI_F_RACERS, BR.n); uiHudSet(UI_F_TIMER, (int)(r->totalT * 1000.f));
    uiHudShow(UI_T_LAP, 1); uiHudShow(UI_T_POSITION, 1); uiHudShow(UI_T_TIMER, 1);
    hudT_ = 1u << UI_T_LAP | 1u << UI_T_POSITION | 1u << UI_T_TIMER;   // рисуются одним вызовом HUD вместе с отсчётом (ниже): разные маски в одном кадре перезапускали всплытие цифры
  } else {
  snprintf(t, sizeof t, "%s %d/%d", bnsLbl(BL_LAP), r->lap + (r->finished ? 0 : 1) > BR.laps ? BR.laps : r->lap + (r->finished ? 0 : 1), BR.laps);
  bnsTK(12, ly0, 0xffffffffu, t, hk_);
  snprintf(t, sizeof t, "%s  %d:%05.2f", bnsLbl(BL_PL1 + ((r->place - 1) & 3)), (int)(r->totalT / 60.f), fmodf(r->totalT, 60.f));
  bnsTK(12, ly0 + lstep, 0xff40ffffu, t, hk_);
  if (!BR_NITRO3_OLD) { snprintf(t, sizeof t, "%s %d", bnsLbl(BL_NITRO), BR.nitro[0]); bnsTK(12, ly0 + 2 * lstep, BR.nitro[0] > 0 ? 0xff40ff40u : 0xff808080u, t, hk_); }
  }   // гудок (↑ крестовины) тратит заряд; свечение над машиной — вид, решает Роб
  { int nx_ = BONUSCAM1109_OLD ? !bsel.cam : camBonusNext(bsel.cam, g10bViewNext(camBonusCv(bsel.cam), !strcmp(car.p.name, "dune_v"), 0));   // №1109: что даст Select
    const char *nm_ = BONUSCAM1109_OLD ? (bsel.cam ? "SELECT: TOP" : "SELECT: CHASE") : nx_ == 0 ? "SELECT: TOP" : nx_ == 1 ? "SELECT: FAR" : nx_ == 2 ? "SELECT: BUMPER" : "SELECT: NEAR";
    if (BONUSSELLABEL1150_OLD) bnsT(SCR_W - 10 - bnsTextW(nm_), 10, 0xa0ffffffu, nm_); }   // №1150: подпись вида Робу не нужна; №1145: шрифт bnsT (сведение)
  if (!BONUSCD1149_OLD && !BONUS_COUNT_PS2_OLD) {   // №1149: отсчёт — общий код с уличными гонками (HUD: цифра 3-2-1 и слово GO!, их анимация и шрифт)
    static int cd_ = -1; static unsigned cdHide_;   // что показано: -1 ничего, 0 слово GO!, 1..9 цифра; когда убрали (угасание ещё рисуется)
    if (BR.state == BR_COUNT) { int dg_ = brCountdownDigit(BR.count); if (dg_ != cd_) { cd_ = dg_; uiCountdownText(0); uiHudSet(UI_F_COUNTDOWN, dg_); } uiHudShow(UI_T_COUNTDOWN, 1); }
    else if (BR.time < (BONUS_GO_OLD ? 1.f : .5f) && BR.state == BR_RACE) { if (cd_ != 0) { cd_ = 0; uiCountdownText("GO!"); } uiHudShow(UI_T_COUNTDOWN, 1); }
    else if (cd_ != -1) { cd_ = -1; uiHudShow(UI_T_COUNTDOWN, 0); cdHide_ = sceKernelGetSystemTimeLow() | 1u; }
    if (cd_ >= 0 || (cdHide_ && (unsigned)(sceKernelGetSystemTimeLow() - cdHide_) < 1500000u)) { if (BONUSHUD1196_OLD) uiDrawCountdownOnly(); else hudT_ |= 1u << UI_T_COUNTDOWN; }
  } else
  if (BR.state == BR_COUNT) {
    char digit[4]; snprintf(digit, sizeof digit, "%d", brCountdownDigit(BR.count));
    bnsTextC(SCR_H / 2 - 30, 0xff40ffffu, BONUS_COUNT_PS2_OLD ? "READY" : digit);
  }                 // SLES_518.97 «READY»
  else if (BR.time < (BONUS_GO_OLD ? 1.f : .5f)) bnsTextC(SCR_H / 2 - 30, 0xff40ff40u, bnsLbl(BL_GO));   // сверка g07b P17-22: 0,5 с (было 1 с)                     // SLES_518.97 «GO!»
  if (r->finished && BR.state != BR_DONE) {
    if (BONUSHUD1196_OLD) bnsTextC(SCR_H / 2 - 30, 0xffffffffu, bnsLbl(BL_PL1 + ((r->place - 1) & 3)));
    else { char pl_[16]; snprintf(pl_, sizeof pl_, "%s", bnsLbl(BL_PL1 + ((r->place - 1) & 3))); for (char *q_ = pl_; *q_; q_++) if (*q_ >= 'a' && *q_ <= 'z') *q_ -= 32;   // место крупным рисованным шрифтом отсчёта и слова GO!, как у уличных гонок
      uiCountdownText(pl_); uiHudShow(UI_T_COUNTDOWN, 1); hudT_ |= 1u << UI_T_COUNTDOWN; hudFin_ = 1; }
  }
  if (!BONUS_END_COUNT_OLD && !BR_DNF10_OLD && BR.state == BR_RACE && BR.dnfOn && BR.finishedN < BR.n) {   // сверка g07b P17-15: отсчёт 10…0, каждая секунда
    int left = (int)floorf(BR_DNF10_S - BR.dnfT); if (left < 0) left = 0; if (left > 9) left = 9;   // число 10 держится не больше кадра: сразу 9…0
    char dg[12]; snprintf(dg, sizeof dg, "%d", left); bnsTextC(SCR_H / 2 - 4, 0xff40ffffu, dg);
  } else
  if (!BONUS_END_COUNT_OLD && BR_DNF10_OLD && BR.state == BR_RACE && BR.finishedN > 0 && BR.finishedN < BR.n && BR_DNF_S - BR.sinceFirst < 10.f) {   // окно после первого финишёра: 9…0
    int left = (int)floorf(BR_DNF_S - BR.sinceFirst); if (left < 0) left = 0;
    char dg[12]; snprintf(dg, sizeof dg, "%d", left); bnsTextC(SCR_H / 2 - 4, 0xff40ffffu, dg);
  }
  if (!BONUSHUD1196_OLD && hudT_) {   // №1196: круг, место, время и отсчёт — одним вызовом HUD основной игры; нитро — строкой нашей гранью под стопкой
    resetState(); uiDrawHudTags(hudT_);
    uiHudShow(UI_T_LAP, 0); uiHudShow(UI_T_POSITION, 0); uiHudShow(UI_T_TIMER, 0); if (hudFin_) uiHudShow(UI_T_COUNTDOWN, 0);
    { int tg_[8], rc_[8][4], n_ = uiOverlayBoxes(tg_, rc_, 8), bot_ = 6, lx_ = 12; for (int k = 0; k < n_; k++) { if (rc_[k][3] > bot_) bot_ = rc_[k][3]; if (k == 0 || rc_[k][0] < lx_) lx_ = rc_[k][0]; }
      resetState(); if (!BR_NITRO3_OLD) { snprintf(t, sizeof t, "%s %d", bnsLbl(BL_NITRO), BR.nitro[0]); bnsTK(lx_ + 2, bot_ + 3, BR.nitro[0] > 0 ? 0xff40ff40u : 0xff808080u, t, BONUSNITRO1211_OLD ? hk_ : 1); } }   // №1211: нитро пиксельной гранью обычного мелкого размера (было ×2)
  }
  if (paused) {
    bnsRect(SCR_W / 2 - 90, SCR_H / 2 - 30, 180, 64, 0xc0000000u);
    bnsTextC(SCR_H / 2 - 22, psel == 0 ? 0xff40ffffu : 0xffd0d0d0u, bnsLbl(BL_CONTINUE));             // MiniPause.pag
    bnsTextC(SCR_H / 2 + 4, psel == 1 ? 0xff40ffffu : 0xffd0d0d0u, bnsLbl(BL_QUIT));
  }
}
static void bnsSummaryUi(void);
static void bnsFrame(int mode, int psel, void **fbp) {   // mode BF_*: гонка с HUD, пауза, облёт (без HUD), итоги поверх трассы
  GE_NEST_GUARD("bnsFrame");   // №1186/№1188
  unsigned budgetT0 = sceKernelGetSystemTimeLow(), budgetIo0 = smkIoUs;
  float eye[3], at[3], up[3] = {0.f, 1.f, 0.f}, fovy, zn, zf;
  int paused = mode != BF_RACE, fly = mode == BF_FLY && bnsFly.n > 1, chaseLike = bsel.cam || fly;
  if (fly) {   // облёт minigamecam: камера данных; ближняя плоскость — камеры порта (near 0,01 данных для 16-битной глубины мал)
    bnsFlyPose(bnsFlyT, eye, at, up, &fovy); zn = NEAR_Z; zf = DIST_LONG + 10.f;
  } else if (!bsel.cam) {   // вид сверху: направление и угол кадра sprint_camShape (горизонталь 4:3 -> вертикаль, как прочие камеры порта)
    fovy = 2.f * atanf(tanf(BT.h->fov * .5f) * .75f) * (180.f / GU_PI);
    brTopLens(&BT, &fovy, &zn, &zf);
    brTopCam(&BT, fovy, ASPECT, eye, at, up, &bnsTopDist);
    const float *bb = BT.h->bbox; float R = .5f * sqrtf((bb[3] - bb[0]) * (bb[3] - bb[0]) + (bb[4] - bb[1]) * (bb[4] - bb[1]) + (bb[5] - bb[2]) * (bb[5] - bb[2])) + 20.f;
    zn = bnsTopDist - R; if (zn < 1.f) zn = 1.f; zf = bnsTopDist + R; up[0] = 0.f; up[1] = 1.f; up[2] = 0.f;
    if (brTopIsPs2(&BT)) {   // №1109: таблица камер оригинала (верх кадра — из неё же)
      brTopLens(&BT, &fovy, &zn, &zf); { float e_[3], a_[3], d_, f_, n_, z_; camBonusTop((int)BT.h->track, e_, a_, up, &f_, &n_, &z_, &d_); }
    }
    if (BR.state == BR_RACE && BR.time < .05f) { char s[220];
      snprintf(s, sizeof s, "BONUS ev=topcamera track=%d eye=%.4f,%.4f,%.4f at=%.4f,%.4f,%.4f up=%.5f,%.5f,%.5f fovy=%.5f near=%.1f far=%.1f dist=%.4f t=%.1f\n",
               bsel.track, eye[0],eye[1],eye[2],at[0],at[1],at[2],up[0],up[1],up[2],fovy,zn,zf,bnsTopDist,smkT); bnsLog(s); }
  } else {
    // №1109 (камера): те же виды, что в машине в городе (Select по кругу, цикл g10bViewNext), вид от первого лица — тот же код бампера (chaseCamUpdate при chaseView 2): угол 90° по горизонтали, ближняя — как у погони.
    // BONUSCAM1109_OLD=1 — как было: только сверху и погоня
    int cvSave_ = chaseView; if (!BONUSCAM1109_OLD) chaseView = camBonusCv(bsel.cam);
    chaseCamUpdate(&car, 0.f, paused ? 0.f : 1.f / 60.f, eye, at); chaseView = cvSave_; fovy = FOVY; zn = NEAR_Z; zf = DIST_LONG + 10.f;
    if (!BONUSCAM1109_OLD && bsel.cam == 2) { fovy = g10bBumperFovY(FOVY, ASPECT); if (camUpOn) { up[0] = camUpV[0]; up[1] = camUpV[1]; up[2] = camUpV[2]; camUpOn = 0; } }
  }
  float f[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]}, fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  for (int k = 0; k < 3; k++) f[k] /= fl;
  // №1146: за машиной и от первого лица камера основной игры целиком — те же функции: правило глаза (глаз не за стеной и не в геометрии, camSafeEye; у бампера, как в городе, не применяется),
  // ближняя плоскость по свободному месту перед глазом (camSafeFrame, floorNearFrame, frameNear — у бампера до 0,25 м, а не 1,5 м трассы). BONUSCAM1146_OLD=1 — как после №1109
  if (!BONUSCAM1146_OLD && bsel.cam && !fly && haveCol) {
    float dtc_ = paused ? 0.f : 1.f / 60.f; CarBox cb_; memset(&cb_, 0, sizeof cb_); carBox(&car, &cb_);
    camFollow = 1; camPivotCar = 1; camPivot[0] = cb_.x; camPivot[1] = cb_.y + cb_.hi + .3f; camPivot[2] = cb_.z;
    if (bsel.cam != 2 && !camSafeOld) { csNearMinCur = CS_NEAR_MIN; camSafeEye(eye, at, fovy, dtc_);
      for (int k = 0; k < 3; k++) f[k] = at[k] - eye[k]; fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (fl > 1e-4f) for (int k = 0; k < 3; k++) f[k] /= fl; }
    camSafeFrame(0, 0, fovy, eye[0], eye[1], eye[2], f[0], f[1], f[2], dtc_);
    floorNearFrame(0, 0, eye[0], eye[1], eye[2], f[0], f[1], f[2], fovy); zn = frameNear(0, 0); camFollow = 0;
  }
  eyeX = eye[0]; eyeY = eye[1]; eyeZ = eye[2]; cfX = f[0]; cfY = f[1]; cfZ = f[2]; curFovY = fovy;
  bnsPUp[0] = up[0]; bnsPUp[1] = up[1]; bnsPUp[2] = up[2]; bnsMarkMode = (mode == BF_RACE || mode == BF_PAUSE) && !fly;   // №1182: ярлыки P1…P4 над машинами
  drawDist = chaseLike ? DIST_LONG : zf;
  smkPose[0] = car.x; smkPose[1] = car.y; smkPose[2] = car.z; smkPose[3] = car.h;
  setupPlanes(f[0], f[1], f[2]); cullCity(1);
  ggOn = 0; ggE[0] = eye[0]; ggE[1] = eye[1]; ggE[2] = eye[2];   bigCutBlock = 1;   // №640: оси камеры для среза здесь не считаются
  sceGuStart(GU_DIRECT, list); resetState(); drawnTris = 0; draws = 0;
  sceGuViewport(2048, 2048, SCR_W, SCR_H); sceGuScissor(0, 0, SCR_W, SCR_H);
  sceGuEnable(GU_DEPTH_TEST); sceGuDepthFunc(GU_GEQUAL); sceGuShadeModel(GU_SMOOTH);
  sceGuTexWrap(GU_REPEAT, GU_REPEAT); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
  sceGuTexScale(128.f, 128.f); sceGuTexOffset(0.f, 0.f); sceGuTexLevelMode(GU_TEXTURE_AUTO, 0.f); curLvlMode = 0;
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  fogBase(); sceGuClearColor(0xff000000 | fogCol); sceGuClearDepth(0);
  sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  sceGuDisable(GU_ALPHA_TEST); sceGuAlphaFunc(GU_GREATER, 0x40, 0xff); curFlags = -1;
  sceGumMatrixMode(GU_PROJECTION); sceGumLoadIdentity(); sceGumPerspective(fovy, ASPECT, zn, zf);
  sceGumMatrixMode(GU_VIEW); sceGumLoadIdentity();
  ScePspFVector3 E = {eye[0], eye[1], eye[2]}, A = {at[0], at[1], at[2]}, U = {up[0], up[1], up[2]}; sceGumLookAt(&E, &A, &U);
  sceGumMatrixMode(GU_MODEL); sceGumLoadIdentity(); sceGumUpdateMatrix();
  if (chaseLike && (opt & O_SKY)) drawSky(eye[0], eye[1], eye[2], 0);
  if (chaseLike) sceGuEnable(GU_FOG); else sceGuDisable(GU_FOG);   // сверху дымки нет: трасса целиком в кадре
  ntq = 0; tqOn = (opt & O_LEAN) != 0; tqEx = eye[0]; tqEy = eye[1]; tqEz = eye[2];
  { ScePspFMatrix4 vAbs; sceGumMatrixMode(GU_VIEW); sceGumStoreMatrix(&vAbs); sceGumMatrixMode(GU_MODEL);
    ScePspFMatrix4 vRel = vAbs; vRel.w.x = vRel.w.y = vRel.w.z = 0.f; sceGuSetMatrix(GU_VIEW, &vRel);   // город в осях камеры (швы 224/237/251)
    if (chaseLike && !BONUSENV1109_OLD) bnsEnvPano(eye);   // №1109: кольцо-панорама уровня вокруг трассы, до города (как в уровне)
    for (int i = 0; i < nd; i++) { Piece *d = &pc[sp[i]];
      if (chaseLike) fogRadial(d, sf[i]);
      if (!(tqOn && (d->r.flags & 2) && !(d->r.flags & 4))) drawPiece(d, sf[i], -eye[0], -eye[1], -eye[2], 1, 0);
      else drawPiece(d, sf[i], 0, 0, 0, 1, 0); }
    sceGuSetMatrix(GU_VIEW, &vAbs); }
  if (chaseLike) fogBase();
  resetState();
  bnsSprops(eye[0], eye[1], eye[2], bsel.cam ? drawDist : 1e4f);   // подборы, ловушки, объекты трассы (SPR1)
  resetState();
  memcpy(bdrvEye, eye, sizeof bdrvEye); bdrvDrawn = 0;
  if (BONUSCAM1109_OLD || bsel.cam != 2) bnsCarDraw(&car, bnsCarName[0], BR.r[0].lap * 0.f + car.wheelSpin[0], bnsTint[0]);   // №1109: от первого лица своей машины в кадре нет (как в городе)
  for (int i = 0; i < BM.n; i++) if (BM.c[i].on) bnsCarDraw(&BM.c[i].car, BM.c[i].name, BM.c[i].wheelAng, i < 3 ? bnsTint[i + 1] : 0);
  resetState(); tqFlush(); resetState();
  sceGuDisable(GU_FOG);
  if (mode == BF_RACE || (mode == BF_FLY && !BONUS_COUNT_PS2_OLD)) bnsHud(0, 0);
  else if (mode == BF_PAUSE && bnsUi) { bnsHudQuiet = !BONUSFONT1179_OLD; bnsHud(0, 0); bnsHudQuiet = 0; resetState(); uiScrMenuFlags(2); uiScrDrawMenu(BUI_MINIPAUSE, psel, bnsUiT); resetState(); }   // MiniPause.scr
  else if (mode == BF_PAUSE) bnsHud(1, psel);
  else if (mode == BF_SUMMARY && bnsUi) { resetState(); bnsSummaryUi(); resetState(); }   // MiniSummary.scr поверх трассы
  if (bnsFadeIn > 0) { resetState(); bnsRect(0, 0, SCR_W, SCR_H, (unsigned)(255 * bnsFadeIn / BNS_FADEIN_FRAMES) << 24); bnsFadeIn--; }   // №1168: трасса открывается из чёрного
  if (mode != BF_FLY && mode != BF_SUMMARY) { if (mode == BF_PAUSE) { int on_ = bnsCodeOn; bnsCodeOn = 1; bnsCodePause = 1; bnsSectorDraw(); bnsCodePause = 0; bnsCodeOn = on_; } else bnsSectorDraw(); }   // №1185
  bnsTris = (unsigned)drawnTris;
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  sceGuFinish(); sceGuSync(0, 0);
  bnsBudget(mode == BF_RACE ? "race" : mode == BF_FLY ? "flyby" : mode == BF_PAUSE ? "pause" : "summary",
            budgetT0, budgetIo0, mode == BF_RACE ? bnsPerfSimUs : 0, mode == BF_RACE ? bnsPerfSimIo : 0);
  bnsPerfSimUs = bnsPerfSimIo = 0;
  sceDisplayWaitVblankStart(); *fbp = swapBuf();
}

// Digit transitions use race time, independent of smoke timestamps.
static void bnsCountLog(int *lastDigit) {
  int digit = BR.state == BR_COUNT ? brCountdownDigit(BR.count) : 0;
  if (digit == *lastDigit) return; *lastDigit = digit;
  char s[160]; snprintf(s, sizeof s, "BONUS ev=countdigit digit=%d elapsed=%.3f remaining=%.3f t=%.1f\n", digit, BR_COUNT_S - BR.count, BR.count, smkT); bnsLog(s);
}
// ---------------------------------------------------------------- гонка
static int bnsRace(void **fbp) {   // 1 — гонка до конца (итоги), 0 — QUIT GAME из паузы
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), secT = last; int paused = 0, psel = 0, frames = 0, camLast = -1;
  char s[200]; int lastDigit = -1;
  brAudioInit(&bnsAudio); brAudioTuneDir(&bnsAudio, bsel.track, BR.reverse);   // музыка заезда своя у трассы (bonus_audio.c, BONUS_TUNE_OLD — Casio01 везде)
  snprintf(s, sizeof s, "BONUS ev=start track=%d laps=%d rev=%d cam=%d car=%s ai=%s,%s,%s cps=%d stops=%d path=%d t=%.1f\n", bsel.track, BR.laps,
           BR.reverse, bsel.cam, bnsCarName[0], bnsCarName[1], bnsCarName[2], bnsCarName[3], BR.ncp, BR.nwp, BR.npath, smkT); bnsLog(s);
  snprintf(s, sizeof s, "BONUS ev=music track=%d tune=%s id=%d t=%.1f\n", bsel.track, bnsAudio.music >= 0 ? audioTrackName(bnsAudio.music) : "-",
           bnsAudio.music, smkT); bnsLog(s);
  if (!BONUS_COUNT_PS2_OLD) { snprintf(s, sizeof s, "BONUS ev=count t=%.1f\n", smkT); bnsLog(s); bnsCountLog(&lastDigit); }
  if (bnsFly.n > 1) {   // облёт minigamecam.p3d перед отсчётом (ss.mfk); машины стоят на решётке, гонка не идёт
    float dur = (bnsFly.n - 1) / bnsFly.fps; int shown = 0, skip = 0, nextLog = 0; static const int logF[5] = {0, 30, 60, 90, 119};
    snprintf(s, sizeof s, "BONUS ev=flyby frames=%d fps=%.0f dur=%.2f t=%.1f\n", bnsFly.n, bnsFly.fps, dur, smkT); bnsLog(s);
    bnsFlyT = 0.f;
    for (;;) {
      unsigned tf = sceKernelGetSystemTimeLow(); float dt = (tf - last) * 1e-6f; last = tf; if (dt > .05f) dt = .05f;
      sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
      unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
      SmokeCmd sc; while (smokeNext(dt, &sc)) bnsFlag(&sc);
      // сверка g07b-msnview (P17 Д5, P17-4): START в облёте открывает паузу бонус-игры (CONTINUE / QUIT GAME), а не пропускает облёт. Пропуск ✕ оставлен правилом порта до слова Роба
      // (в оригинале пропуска нет вовсе). BONUS_FLYPAUSE_OLD=1 — START пропускает (прежнее)
      if (!BONUS_FLYPAUSE_OLD && (hit & PSP_CTRL_START)) {
        int ps = 0; snprintf(s, sizeof s, "BONUS ev=pause fly=1 t=%.1f\n", smkT); bnsLog(s);
        for (;;) {
          unsigned t2 = sceKernelGetSystemTimeLow(); float d2 = (t2 - last) * 1e-6f; last = t2; if (d2 > .05f) d2 = .05f;
          sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad(); unsigned h2 = pad.Buttons & ~prev; prev = pad.Buttons;
          if (h2 & (PSP_CTRL_UP | PSP_CTRL_DOWN)) ps = !ps;
          if (h2 & PSP_CTRL_CROSS) { if (ps == 1) { snprintf(s, sizeof s, "BONUS ev=quit t=%.1f\n", smkT); bnsLog(s); return 0; } break; }
          if (h2 & PSP_CTRL_START) break;
          bnsUiT += d2; brAudioFrame(&bnsAudio, &BR, &car, bnsCarName[0], &BM, d2, 0, 1); bnsFrame(BF_PAUSE, ps, fbp);
        }
        last = sceKernelGetSystemTimeLow(); prev = pad.Buttons | smokePad(); continue;
      }
      if (hit & (PSP_CTRL_CROSS | (BONUS_FLYPAUSE_OLD ? PSP_CTRL_START : 0))) { skip = 1; break; }   // правило порта: облёт можно пропустить
      while (nextLog < 5 && logF[nextLog] < bnsFly.n && bnsFlyT * bnsFly.fps >= logF[nextLog] - 1e-3f) {   // поза кадра данных (гейт сверяет с диском)
        float e[3], a[3], u[3], fv; bnsFlyPose(logF[nextLog] / bnsFly.fps, e, a, u, &fv);
        snprintf(s, sizeof s, "BONUS ev=flycam f=%d eye=%.3f,%.3f,%.3f at=%.3f,%.3f,%.3f fovy=%.1f t=%.1f\n", logF[nextLog], e[0], e[1], e[2], a[0], a[1], a[2], fv, smkT);
        bnsLog(s); nextLog++; }
      if (!BONUS_COUNT_PS2_OLD) {
        float pos[4][3] = {{car.x, car.y + .5f, car.z}};
        for (int i = 0; i < BM.n && i < 3; i++) { pos[i+1][0] = BM.c[i].car.x; pos[i+1][1] = BM.c[i].car.y + .5f; pos[i+1][2] = BM.c[i].car.z; }
        brUpdate(&BR, (const float (*)[3])pos, dt); bnsCountLog(&lastDigit);
        brAudioFrame(&bnsAudio, &BR, &car, bnsCarName[0], &BM, dt, 0, 0);
      }
      bnsFrame(BF_FLY, 0, fbp); shown++;
      if (bnsFlyT * bnsFly.fps >= 60.f) bnsShot(2, "bonus_flyby");   // середина облёта
      if (bnsFlyT >= dur) break;
      bnsFlyT += dt; if (bnsFlyT > dur) bnsFlyT = dur;
    }
    snprintf(s, sizeof s, "BONUS ev=flyby_end shown=%d skip=%d at=%.2f t=%.1f\n", shown, skip, bnsFlyT, smkT); bnsLog(s);
    sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = sceKernelGetSystemTimeLow(); secT = last; chase.init = 0;
  }
  if (BONUS_COUNT_PS2_OLD) { snprintf(s, sizeof s, "BONUS ev=count t=%.1f\n", smkT); bnsLog(s); bnsCountLog(&lastDigit); }
  for (;;) {
    unsigned tn = sceKernelGetSystemTimeLow(); float dt = (tn - last) * 1e-6f; last = tn; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    SmokeCmd sc; while (smokeNext(dt, &sc)) bnsFlag(&sc);
    if (hit & PSP_CTRL_START) { paused = !paused; psel = 0; if (paused) bnsUiLog("pause"); }
    if (paused) {
      if (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN)) psel = !psel;
      if (hit & PSP_CTRL_CROSS) { if (psel == 1) { snprintf(s, sizeof s, "BONUS ev=quit t=%.1f\n", smkT); bnsLog(s); return 0; } paused = 0; }
      bnsUiT += dt; brAudioFrame(&bnsAudio,&BR,&car,bnsCarName[0],&BM,dt,0,1);bnsFrame(BF_PAUSE, psel, fbp); continue;
    }
    unsigned stepBudgetT0 = sceKernelGetSystemTimeLow(), stepBudgetIo0 = smkIoUs;
    if ((hit & PSP_CTRL_SELECT) && (pad.Buttons & PSP_CTRL_RTRIGGER) && !BONUSSECTOR1185_OLD) { bnsCodeOn = !bnsCodeOn; uiPopHookFire(UI_EV_SCROLL); }   // №1185: R+SELECT — код сектора, а не смена вида
    else if ((hit & PSP_CTRL_SELECT) && (BONUS_SELECT_ANY_OLD || BR.state == BR_RACE)) bsel.cam = BONUSCAM1109_OLD ? !bsel.cam : camBonusNext(bsel.cam, g10bViewNext(camBonusCv(bsel.cam), !strcmp(car.p.name, "dune_v"), 0));   // сверка g07b P17 Д6: вид — только в состоянии «гонка идёт»
    if (bsel.cam != camLast) { camLast = bsel.cam; chase.init = 0; csPrevOn = csOfsOn = 0; csNearPrevOk = 0;   /* №1146: переключение вида — срез, без перелёта от прошлого глаза */
      snprintf(s, sizeof s, "BONUS ev=cam mode=%d t=%.1f\n", bsel.cam, smkT); bnsLog(s); }
    // машина игрока
    float ax = (pad.Lx - 128) / 128.f; if (fabsf(ax) < .2f) ax = 0;
    GamePad gp = {pad.Buttons, hit, ax, 0.f}; CarInput in; gameCarInput(&G, &gp, &in);
    McEnv e; memset(&e, 0, sizeof e); e.w = &world; e.player = &car; e.driving = 1; e.noNpcCollide = AIAI1147_OLD; e.ppos[0] = car.x; e.ppos[1] = car.y; e.ppos[2] = car.z;
    { const Car *pc_[4] = {&car, 0, 0, 0}; for (int i = 0; i < BM.n && i < 3; i++) pc_[i + 1] = &BM.c[i].car; brSetPeers(&BR, pc_, 1 + (BM.n < 3 ? BM.n : 3)); }   // №1147
    e.hold = BR.state == BR_COUNT; if (bnsRoads) e.tr = &bnsTr;
    if (bnsAuto && !BR.r[0].finished && bnsRoads && !BONUSBOT1180_OLD) {   // №1180в: водитель за игрока едет по дорогам трассы к центру ЖДАННОГО чекпойнта (по порядку данных), как человек: вход в чужой объём — «читер», круг не засчитан, пока не пройден нужный
      const BnsCp *cp_ = &BR.t.cp[BR.order[BR.r[0].next]]; (void)bnsBotWi; float goal_[3]; memcpy(goal_, cp_->c, 12);
      { static int cps_; static float stallT_, cheatT_; int cps0 = BR.r[0].cps;   // водитель по линии включается, когда цель-чекпойнт не работает: «читер» дольше 3 с или нет нового чекпойнта 15 с; выключается с новым чекпойнтом
        if (cps0 != cps_) { cps_ = cps0; stallT_ = cheatT_ = 0.f; bnsBotLine = 0; } else { stallT_ += dt; cheatT_ = BR.r[0].cheated ? cheatT_ + dt : 0.f; if ((stallT_ > 6.f || cheatT_ > .4f) && !bnsBotLine && bnsSec.nsec > 0) { bnsBotLine = 1; float dd_; bnsBotS = bscArc(&bnsSec, cp_->c[0], cp_->c[1], cp_->c[2], &dd_) - 25.f; if (bnsBotS < 0.f) bnsBotS = 0.f; } } }   // линия — с участка перед ждущим чекпойнтом
      if (bnsSec.nsec > 0 && !BONUSBOTSEC1180_OLD && bnsBotLine) {   // есть линия секторов (след лидера, проходит чекпойнты по порядку): цель — точка линии впереди (чистое преследование), а не центр чекпойнта — тот лежит рядом с другими объёмами у старта
        float d_; bnsBotS = bscArcNear(&bnsSec, car.x, car.y + .5f, car.z, bnsBotS, 25.f, 45.f, &d_); float s_ = bnsBotS + 8.f + .6f * fabsf(car.v); if (s_ > bnsSec.len) s_ = bnsSec.len;   /* окно у прошлой позиции: на трассах, где путь проходит рядом сам с собой, не перескакивает на круг назад */ float dr_[2]; bscPointAt(&bnsSec, s_, goal_, dr_); }
      bnsBot.car = car; bnsBot.mode = MC_RACE;
      mcBotInput(&BM, &bnsBot, &e, goal_, dt, &in);
    } else if (bnsAuto && !BR.r[0].finished) {   // служебное: водитель ИИ за машиной игрока по тому же пути (гейт: круги игрока)
      bnsBot.car = car; bnsBot.wp = brGoal(&BR, 0);
      float r[MC_ROUTE][3], pp[3] = {car.x, car.y, car.z}; int n = brPlan(&BR, bnsBot.wp, pp, r, MC_ROUTE);
      if (n > 0) mcBotInputPath(&BM, &bnsBot, &e, (const float (*)[3])r, n, dt, &in);
    }
    if (e.hold || (BONUS_FINISH_COAST_OLD && BR.r[0].finished)) { in.throttle = 0.f; in.steer = 0.f; in.handbrake = 1.f; in.brake = fabsf(car.v) > .5f ? 1.f : 0.f; }
    else if (BR.r[0].finished) { in.throttle = 0.f; in.steer = 0.f; in.handbrake = 0.f; in.brake = 0.f; }   // сверка g07b P17 Д7: финишировал — газ, тормоз, ручник и руль обнулены, катится по инерции
    if ((hit & PSP_CTRL_TRIANGLE) && BR.state == BR_RACE && !BR.r[0].finished) {   // правило порта: назад на путь
      float q[3], qh; float pp[3] = {car.x, car.y, car.z}; if (brRespawn(&BR, 0, pp, q, &qh)) { carPlace(&car, &world, q[0], q[1] + .5f, q[2], qh); chase.init = 0; } }
    carUpdate(&car, &world, &in, dt); brKeepAlive(&car);
    if (!BR_NITRO3_OLD && (hit & PSP_CTRL_UP) && BR.state == BR_RACE && brHorn(&BR, 0, &car)) {   // сверка g07b P17-14: гудок-нитро (↑ крестовины, как гудок игры)
      snprintf(s, sizeof s, "BONUS ev=horn car=0 name=%s nitro=%d t=%.1f\n", bnsCarName[0], BR.nitro[0], smkT); bnsLog(s); }
    bnsEffects(0, &car, in.throttle, dt);
    // соперники: цель — следующий чекпойнт по правилу гонки
    for (int i = 0; i < BM.n; i++) { McCar *c = &BM.c[i];
      if (BR.r[i + 1].finished) { c->mode = MC_PARKED; continue; }
      if (bnsRoads) continue;   // №1116: по дорогам трассы цель — точка WPR/WPL, её переключает сам водитель (msncar.c)
      int g = brGoal(&BR, i + 1); if (c->wp != g) { c->wp = g; c->nroute = 0; } }
    mcUpdate(&BM, &e, dt, 0, 0);
    for (int i = 0; i < BM.n && i < 3; i++) bnsEffects(i + 1, &BM.c[i].car, 1.f, dt);
    brPickTick(&BR, dt);
    for (unsigned q = 0; q < BT.npick && q < BNS_MAXPICK; q++) if (bnsPickInst[q] >= 0) sprops.run[bnsPickInst[q]].alive = (uint8_t)brPickShown(&BR, (int)q);
    if (!BONUS_PICKUPS_OLD && haveSP) spUpdate(&sprops, &world, &car, 0, dt);   // кадры клипов SPR1 (вращение подборов, анимации трассы)
    float pos[4][3] = {{car.x, car.y + .5f, car.z}};
    for (int i = 0; i < BM.n && i < 3; i++) { Car *k = &BM.c[i].car; brKeepAlive(k); pos[i + 1][0] = k->x; pos[i + 1][1] = k->y + .5f; pos[i + 1][2] = k->z;
      if (brStuck(&BR, i + 1, pos[i + 1], dt)) { float q[3], qh;
        if (brRespawn(&BR, i + 1, pos[i + 1], q, &qh)) { carPlace(k, &world, q[0], q[1] + .5f, q[2], qh); BM.c[i].nroute = 0;
          snprintf(s, sizeof s, "BONUS ev=respawn car=%d at=%.1f,%.1f t=%.1f\n", i + 1, q[0], q[2], smkT); bnsLog(s); } } }
    if (bnsAuto && brStuck(&BR, 0, pos[0], dt)) { float q[3], qh; if (brRespawn(&BR, 0, pos[0], q, &qh)) carPlace(&car, &world, q[0], q[1] + .5f, q[2], qh); }
    if (brTrapStep(&BR, (const float (*)[3])pos, 4, dt)) { snprintf(s, sizeof s, "BONUS ev=trap frames=%.0f fps=%.0f t=%.1f\n", BR.trap[0].last, BR.trap[0].fps, smkT); bnsLog(s); }
    if (haveAo) brTrapApply(&BR, &ao, &sprops, &world);
    int fin0[4]; for (int i = 0; i < 4; i++) fin0[i] = BR.r[i].finished;
    unsigned lm = brUpdate(&BR, (const float (*)[3])pos, dt); bnsCountLog(&lastDigit);
    if (BR.state == BR_RACE && !bnsGoDone) {   // сверка g07b P17-20: на «GO!» часть ИИ сразу тратит нитро
      bnsGoDone = 1; Car *cs[4] = {&car, 0, 0, 0}; for (int i = 0; i < BM.n && i < 3; i++) cs[i + 1] = &BM.c[i].car;
      int ok_ = 1; for (int i = 0; i < BR.n; i++) if (!cs[i]) ok_ = 0;
      int u_ = ok_ ? brGoNitro(&BR, cs) : 0; snprintf(s, sizeof s, "BONUS ev=gonitro used=%d t=%.1f\n", u_, smkT); bnsLog(s); }
    for (int i = 0; i < 4; i++) {
      if (lm & (1u << i)) { snprintf(s, sizeof s, "BONUS ev=lap car=%d name=%s lap=%d best=%.2f place=%d t=%.1f\n", i, bnsCarName[i], BR.r[i].lap,
                                     BR.r[i].bestLap, BR.r[i].place, smkT); bnsLog(s); }
      if (BR.r[i].finished && !fin0[i]) { snprintf(s, sizeof s, "BONUS ev=finish car=%d name=%s place=%d total=%.2f best=%.2f t=%.1f\n", i, bnsCarName[i],
                                                   BR.r[i].place, BR.r[i].finishT, BR.r[i].bestLap, smkT); bnsLog(s); }
    }
    brAudioFrame(&bnsAudio,&BR,&car,bnsCarName[0],&BM,dt,in.throttle,0);
    bnsPerfSimUs = sceKernelGetSystemTimeLow()-stepBudgetT0; bnsPerfSimIo = smkIoUs-stepBudgetIo0;
    bnsFrame(BF_RACE, 0, fbp); frames++;
    if (tn - secT >= 1000000u) {   // раз в секунду: кадры и треугольники (сверка производительности на PSP — Роб)
      snprintf(s, sizeof s, "BONUSFR fps=%.1f tris=%u cam=%d dist=%.0f place=%d lap=%d t=%.1f heap=%u blk=%u track=%d pv=%.1f pxz=%.0f,%.0f cps=%d step=%d cheat=%d rsp=%d wi=%d\n", frames * 1e6f / (float)(tn - secT), bnsTris,
               bsel.cam, bsel.cam ? 0.f : bnsTopDist, BR.r[0].place, BR.r[0].lap, smkT, heapFreeBytes(), memMaxBlock ? memMaxBlock() : 0u, bsel.track, car.v, car.x, car.z, BR.r[0].cps, BR.r[0].step, BR.r[0].cheated, BR.r[0].respawnN, bnsBotWi); bnsLog(s); 
      { char a_[200]; int l_ = snprintf(a_, sizeof a_, "BONUSAI track=%d t=%.1f", bsel.track, smkT); for (int i_ = 0; i_ < BM.n && i_ < 3; i_++) l_ += snprintf(a_ + l_, sizeof a_ - (size_t)l_, " %.1f,%.1f", BM.c[i_].car.x, BM.c[i_].car.z);
        snprintf(a_ + l_, sizeof a_ - (size_t)l_, "\n"); bnsLog(a_); }   // №1185: позиции соперников раз в секунду — проверка листа-карты (они едут по этому пути)
      frames = 0; secT = tn; }
    if (bnsShots && BR.state == BR_RACE && BR.time > 1.f && !(bnsShotDone >> 4 & 1)) {   // гейт: вид гонки и пауза MiniPause
      bnsShot(3, "bonus_top"); bnsUiLog("pause"); bnsFrame(BF_PAUSE, 0, fbp); bnsShot(4, "bonus_pause");
      if (bnsQuit) { bnsQuit = 0; snprintf(s, sizeof s, "BONUS ev=quit t=%.1f\n", smkT); bnsLog(s); return 0; }
      last = sceKernelGetSystemTimeLow();
    }
    if (bnsQuitAt > 0.f && BR.state == BR_RACE && BR.time >= bnsQuitAt) { bnsQuitAt = 0.f; snprintf(s, sizeof s, "BONUS ev=quit race_t=%.1f heap=%u blk=%u t=%.1f\n", BR.time, heapFreeBytes(), memMaxBlock ? memMaxBlock() : 0u, smkT); bnsLog(s); return 0; }   // №1168: сцена тура
    if (BR.state == BR_DONE) {
      brAudioResult(&bnsAudio,BR.r[0].place==1&&!BR.r[0].dnf);
      for (int i = 0; i < 4; i++) { if (BR.r[i].dnf) BR.r[i].place = 4; bnsPts[i] += BR.r[i].dnf ? 0 : brPoints(BR.r[i].place); if (BR.r[i].place == 1 && !BR.r[i].dnf) bnsWins[i]++; }
      snprintf(s, sizeof s, "BONUS ev=done time=%.2f places=%d,%d,%d,%d dnf=%d,%d,%d,%d t=%.1f\n", BR.time, BR.r[0].place, BR.r[1].place, BR.r[2].place,
               BR.r[3].place, BR.r[0].dnf, BR.r[1].dnf, BR.r[2].dnf, BR.r[3].dnf, smkT); bnsLog(s);
      snprintf(s, sizeof s, "BONUS ev=effects track=%d nitro=%u wrench=%u death=%u trap=%u launch=%u light=%u t=%.1f\n", (int)BT.h->track, BR.nNitro, BR.nWrench,
               BR.nDeath, BR.nTrap, BR.nLaunch, BR.nLight, smkT); bnsLog(s);
      return 1;
    }
  }
}

// итоги серии на страницах MiniSummary.pag: строки — по очкам серии (Race Series Standings), при равенстве — по месту этой гонки
// (правило порта: порядок строк в данных не задан); лицо строки — персонаж машины с настроением строки (Face0 happy … Face3 sad —
// данные); P1…P4 — номер машины (игрок — P1, правило порта); Total Time / Best Lap — этой гонки, «m:ss.ss» (правило порта)
static char bnsSumTxt[4][4][16]; static int bnsSumKing[4][2];
static void bnsSummaryOrder(int ord[4]) {
  for (int i = 0; i < 4; i++) ord[i] = i;
  for (int a = 0; a < 4; a++) for (int b = a + 1; b < 4; b++) {
    int x = ord[a], y = ord[b];
    if (bnsPts[y] > bnsPts[x] || (bnsPts[y] == bnsPts[x] && BR.r[y].place < BR.r[x].place)) { ord[a] = y; ord[b] = x; } }
}
static void bnsTime(char *o, int n, float t) { if (t <= 0.f) snprintf(o, n, "-"); else snprintf(o, n, "%d:%05.2f", (int)(t / 60.f), fmodf(t, 60.f)); }
static void bnsSummaryUi(void) {
  int ord[4]; bnsSummaryOrder(ord);
  int bt = -1, bl = -1;
  for (int i = 0; i < 4; i++) {
    if (BR.r[i].finished && !BR.r[i].dnf && (bt < 0 || BR.r[i].finishT < BR.r[bt].finishT)) bt = i;
    if (BR.r[i].bestLap > 0.f && (bl < 0 || BR.r[i].bestLap < BR.r[bl].bestLap)) bl = i;
  }
  uiScrDraw(BUI_MSBASE, 0, bnsUiT, 0);
  for (int k = 0; k < 4; k++) {
    int i = ord[k], sc = BUI_MSROW0 + k; char (*t)[16] = bnsSumTxt[k];
    uiScrSetRow(0, bnsRacerChr[i]); uiScrSetRow(1, i);
    snprintf(t[0], 16, "%d", bnsWins[i]); snprintf(t[1], 16, "%d", bnsPts[i]);
    if (BR.r[i].dnf || !BR.r[i].finished) snprintf(t[2], 16, "DNF"); else bnsTime(t[2], 16, BR.r[i].finishT);
    if (!BONUS_SUMMARY_DNF_OLD && BR.r[i].bestLap <= 0.f && (BR.r[i].dnf || !BR.r[i].finished)) snprintf(t[3], 16, "DNF");   // оригинал: без завершённого круга лучший круг тоже DNF
    else bnsTime(t[3], 16, BR.r[i].bestLap);   // DNF после завершённого круга сохраняет лучший круг (оригинал: Барт DNF / best 21.60)
    for (int r = 0; r < 4; r++) uiScrRowText(sc, 2 + r, t[r]);
    bnsSumKing[k][0] = i == bt; bnsSumKing[k][1] = i == bl;
    uiScrShow(sc, BUI_KING_TOTAL, i == bt); uiScrShow(sc, BUI_KING_LAP, i == bl);
    uiScrShow(sc, UI_T_RADAR, i == 0);   // №1183: красная метка «P1» — только у строки игрока (как на PS2)
    uiScrDraw(sc, 0, bnsUiT, 0);
  }
}
static void bnsSummary(void **fbp) {   // итоги серии (MiniSummary.pag: место, победы, очки, общее время, лучший круг)
  GE_NEST_GUARD("bnsSummary");   // №1186/№1188
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(); float shown = 0.f; int logged = 0;
  for (;;) {
    unsigned tn = sceKernelGetSystemTimeLow(); float dt = (tn - last) * 1e-6f; last = tn; if (dt > .05f) dt = .05f; shown += dt;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    SmokeCmd sc; while (smokeNext(dt, &sc)) bnsFlag(&sc);
    if ((hit & (PSP_CTRL_CROSS | PSP_CTRL_START)) || (bnsAuto && shown > 3.f)) return;
    if (bnsUi) {   // родной экран: MiniSummary.scr поверх трассы (машины стоят)
      bnsUiT += dt; bnsFrame(BF_SUMMARY, 0, fbp);
      if (!logged) { logged = 1; bnsUiLog("summary");   // что нарисовано в строках (гейт строит тот же кадр из пакета)
        int ord[4]; bnsSummaryOrder(ord); char s[200];
        for (int k = 0; k < 4; k++) { int i = ord[k];
          snprintf(s, sizeof s, "BONUS ev=sumrow k=%d racer=%d chr=%d wins=%s pts=%s total=%s best=%s king=%d,%d t=%.1f\n", k, i, bnsRacerChr[i],
                   bnsSumTxt[k][0], bnsSumTxt[k][1], bnsSumTxt[k][2], bnsSumTxt[k][3], bnsSumKing[k][0],
                   bnsSumKing[k][1], smkT); bnsLog(s); } }
      if (shown > 1.f) bnsShot(5, "bonus_summary");
      continue;
    }
    sceGuStart(GU_DIRECT, list); resetState(); sceGuClearColor(0xff102040u); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    bnsTextC(20, 0xff40e0ffu, bnsLbl(BL_STANDINGS));   // MiniSummary.pag RACE_SUMMARY
    { static const int hk[5] = {BL_CAR, BL_WINS, BL_POINTS, BL_TOTAL, BL_BEST}, hx[5] = {40, 190, 245, 315, 405}; bnsHeads(hk, hx, 5, 50, 0xffa0a0a0u); }
    int ord[4] = {0, 1, 2, 3};
    for (int a = 0; a < 4; a++) for (int b = a + 1; b < 4; b++) if (BR.r[ord[b]].place < BR.r[ord[a]].place) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
    for (int k = 0; k < 4; k++) { int i = ord[k]; char t[64]; int y = 76 + k * 34; unsigned c = i == 0 ? 0xff40ffffu : 0xffffffffu;
      bnsTR(12, y, c, BR.r[i].dnf ? "DNF" : bnsLbl(BL_PL1 + k)); bnsTR(40, y, c, bnsCarName[i]);
      snprintf(t, sizeof t, "%d", bnsWins[i]); bnsTR(200, y, c, t); snprintf(t, sizeof t, "%d", bnsPts[i]); bnsTR(260, y, c, t);
      if (BR.r[i].finished) { snprintf(t, sizeof t, "%.2f", BR.r[i].finishT); bnsTR(320, y, c, t); }
      snprintf(t, sizeof t, "%.2f", BR.r[i].bestLap); bnsTR(410, y, c, t); }
    bnsTextC(SCR_H - 26, 0xffffffffu, bnsLbl(BL_CONTINUE));   // MiniSummary.scr Continue.pag
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
}

// круг 5 (g07b, P17-23): после итогов — вопрос «RACE AGAIN?»: ДА — та же трасса и настройки, серия очков и побед сохраняется; НЕТ — назад в меню выбора. Серия (победы и очки) обнуляется
// при каждом новом заезде из меню выбора. Экран-запрос свой, простой (нет в данных Scrooby под мини-игру): ↑↓ выбор, ✕ принять, ○ — «нет». В смоуке (bnsAuto / smkOn) вопрос закрывается сам через 1 с
// как «нет» — прежние гейты не виснут. BONUS_AGAIN_OLD=1 — прежнее (без вопроса, серия живёт всю сессию)
#ifndef BONUS_AGAIN_OLD
#define BONUS_AGAIN_OLD 0
#endif
static int bnsAgain(void **fbp) {
  GE_NEST_GUARD_RET("bnsAgain", 0);   // №1186/№1188
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(); float shown = 0.f; int sel = 0;
  for (;;) {
    unsigned tn = sceKernelGetSystemTimeLow(); float dt = (tn - last) * 1e-6f; last = tn; if (dt > .05f) dt = .05f; shown += dt;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    SmokeCmd sc; while (smokeNext(dt, &sc)) bnsFlag(&sc);
    if (bnsAuto || (smkOn && shown > 1.f)) { int yes = bnsAgainYes > 0; if (yes) bnsAgainYes--; char s2[96]; snprintf(s2, sizeof s2, "BONUS ev=again yes=%d wins=%d,%d,%d,%d pts=%d,%d,%d,%d t=%.1f\n", yes, bnsWins[0], bnsWins[1], bnsWins[2], bnsWins[3], bnsPts[0], bnsPts[1], bnsPts[2], bnsPts[3], smkT); bnsLog(s2); return yes; }
    if (hit & (PSP_CTRL_UP | PSP_CTRL_DOWN)) sel = !sel;
    if (hit & PSP_CTRL_CROSS) { char s[80]; snprintf(s, sizeof s, "BONUS ev=again yes=%d t=%.1f\n", sel == 0, smkT); bnsLog(s); return sel == 0; }
    if (hit & (PSP_CTRL_CIRCLE | PSP_CTRL_TRIANGLE)) return 0;
    sceGuStart(GU_DIRECT, list); resetState(); sceGuClearColor(0xff102040u); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    bnsTextC(SCR_H / 2 - 50, 0xff40e0ffu, bnsLbl(BL_AGAIN));
    bnsTextC(SCR_H / 2 - 10, sel == 0 ? 0xff40ffffu : 0xffd0d0d0u, bnsLbl(BL_YES));
    bnsTextC(SCR_H / 2 + 20, sel == 1 ? 0xff40ffffu : 0xffd0d0d0u, bnsLbl(BL_NO));
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuFinish(); sceGuSync(0, 0); sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
}

// MINI GAME главного меню: выбор -> гонка -> итоги -> выбор ...; назад — гостиная меню снова
static void bonusRun(int mainSel, void **fbp) {
  char s[160]; int rowKeep[12]; for (int r = 0; r < 12; r++) rowKeep[r] = uiScrGetRow(r);   // поля ROW0..11 общие с меню OPTIONS / паузой
  int again = 0;
  while (again || bnsMenu(mainSel, fbp)) {
    if (!again && !BONUS_AGAIN_OLD) { memset(bnsWins, 0, sizeof bnsWins); memset(bnsPts, 0, sizeof bnsPts); }   // круг 5 P17-23: новый заезд из меню — серия с нуля
    again = 0;
    feFree();                                    // гостиная — в кучу на время гонки (как NEW GAME), потом читается снова
    bnsGo = 0;   // №1180: просроченный «flag bonusgo» не стартует следующую гонку без выбора
    int ok = bnsLoad(fbp);
    if (ok) { bnsRaceInit(); if (bnsRace(fbp)) { bnsSummary(fbp); if (!BONUS_AGAIN_OLD) again = bnsAgain(fbp); } }
    if (ok && !BONUSLOAD1168_OLD) { mem1060Fade(fbp); bnsFadeIn = 0; }   // №1168: гонка (итоги, пауза) гаснет перед выходом, не обрывается
    uiHudShow(UI_T_COUNTDOWN, 0); uiCountdownText(0);   // №1149: метка отсчёта не остаётся в HUD игры
    brAudioStop(&bnsAudio);bonusCarsPtr = 0; bonusPlayerModel = 0; mcPlanHook = 0; mcSprint = 0; bnsRoads = 0;
    free(bnsMem); bnsMem = 0; memset(&BT, 0, sizeof BT);
    levelUnload(); bonusTrack = 0; pano917Force = 1; bnsEnvT = -1;   // №1109: выход из трассы — панорамы уровня игры читаются заново   // loadedLevel = 0: NEW/LOAD прочтут уровень игры заново (menuRun: sw)
    if(!BONUS_SOUND_OLD && BONUSMENUMUS1212_OLD){   // №1212: звук главного меню возвращается при выходе из мини-игры (конец bonusRun), не после каждой гонки
      sndShutdown();sndLevel=-1;char ad[256];levelFile(ad,"audio/");
      if(sndInit(ad)==0){sndLevel=gameLevel;cfgApply();uiSfxInit(smkWrite);bbThForgetAudio();}
    }
    snprintf(s, sizeof s, "BONUS ev=exit ok=%d heap=%u t=%.1f\n", ok, heapFreeBytes(), smkT); bnsLog(s);
    bnsUiClose(); free(bnsFly.f); memset(&bnsFly, 0, sizeof bnsFly);
    feLoadShown(UI_LOAD_FE, "load", fbp);
    sndMode(SND_MODE_NORMAL); if (BONUSMENUMUS1212_OLD) sndMusic(SND_MUS_FRONTEND);   // №1212: мелодию меню мини-игры ставит bnsMenuNative (ниже по циклу), главное меню — после выхода из мини-игры
    bnsGo = 0;   // №1180: флаг-страховка тура, пришедший после старта гонки, не пропускает выбор следующей трассы
  }
  bnsUiClose();   // главное меню — без пакета экранов мини-игры (куча как до BONUS GAME)
  if (!BONUSMENUMUS1212_OLD && !BONUS_SOUND_OLD && sndLevel == -2) { sndShutdown(); sndLevel = -1; char ad_[256]; levelFile(ad_, "audio/");   // №1212: назад в главное меню — его звук и музыка
    if (sndInit(ad_) == 0) { sndLevel = gameLevel; cfgApply(); uiSfxInit(smkWrite); bbThForgetAudio(); }
    { char fn_[256]; sibling(fn_, FE_AUDIO_FILE); feAudioLoad(fn_); } }   // реплики сценок гостиной — к звуку главного меню заново (иначе молчат)
  if (!BONUSMENUMUS1212_OLD) { sndMode(SND_MODE_NORMAL); sndMusic(SND_MUS_FRONTEND); }
  for (int r = 0; r < 12; r++) uiScrSetRow(r, rowKeep[r]);
}
