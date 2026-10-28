#ifndef GEVB_GSHADE_OLD
#define GEVB_GSHADE_OLD 0   // -DGEVB_GSHADE_OLD=1 — тень Гомера на земле в список GE, как раньше
#endif
#include "load_fuse_io.h"
// ---------- 12а: заставка → главное меню → загрузка уровня 1 → игра; QUIT GAME в паузе → снова меню ----------
// Уровень (резидентная часть, текстуры, коллизия, секции пропов) читается один раз на заставке и живёт до выхода.
// Город вокруг спавна грузится после меню со шкалой (cellsRing), при выходе в меню выгружается (cellsDropAll), а
// состояние игры заново берётся из данных уровня (newGame): куча в меню одна и та же на каждом круге (tools/menu_check.py).
static void boothBase1052(StateProps *s);   // №1052 (main_levels.h)
static int mainOpt = -1;   // пункт OPTIONS главного меню (menu.shr)
static int mainLoad = -1, mainResume = -1;   // LOAD GAME / RESUME GAME (сохранения, viewer/savemenu.h)
static int menuOn, toMenu, mainNew = -1, loadShot; static float loadShotAt, feShotAt;   // снимки экранов загрузки: «flag loadshot/feshot [с]» — держать до стольких секунд часов экрана (гейт кадров огня/анимации: в эмуляторе загрузка мгновенна); spStInit (память состояний стейтпропов) — у Game G
static float spawnP[4]; static void *propSec; static unsigned propSecN;
static void aoBind(void) {   // ANM1 в PRP2: после spInit — к экземплярам SPR1 и живым объёмам COL1 (старый level.shr без таблицы — 1)
  int e = haveSP && haveCol ? aoInit(&ao, propSec, propSecN, &sprops, &world) : 1; haveAo = e == 0; aoAct = 0;
  char s[96]; snprintf(s, sizeof s, "AO init=%d obj=%u vol=%u\n", e, ao.nobj, ao.nvol); smkWrite(s);
}
// служебные запуски (автоматика эмулятора) стартуют сразу в игре, как раньше; переключатели картинки (view60, fb32…) — нет
static int svcStart(void) {
  static const char *f[] = {"autobench", "pvscheck", "autodrive", "autostraight", "goto.txt", "camang.txt", "carseq.txt",
                            "testdmg", "testboom", "slowmo", "opts.txt", "clearchk", "flypath", "streamlog", "smokelog", "scenario.txt", "stripbench"};
  char fn[256];
  for (unsigned i = 0; i < sizeof f / sizeof *f; i++) { sibling(fn, f[i]); SceUID h = sceIoOpen(fn, PSP_O_RDONLY, 0); if (h >= 0) { sceIoClose(h); return 1; } }
  return 0;
}
// Правка Роба 44 (28.09): служебные кнопки и строки — только в служебном режиме. Служебный режим = файл service рядом
// с EBOOT или служебный запуск (svcStart: сценарий смоука, autobench, goto.txt …). В игре игрока их нет: L+R+△ полёт,
// R+SELECT счётчик FPS/RAM, SELECT 30/60 FPS; SELECT в машине — RESET CAR, как в раскладке оригинала (ControllerPS2.pag).
// Сценарий с «flag player» проверяет режим игрока (гейт tests/svc).
static int svcMode;
static int menuLevel = 1;   // служебный режим: уровень для NEW GAME (L / R в главном меню, «flag lvset N»); у игрока NEW GAME — уровень 1
static int svcLabel;        // файл service рядом с EBOOT: подпись выбранного уровня в меню (у сценариев смоука её нет — снимки меню прежние)
static int lvNoSave;        // отрицательный контроль гейта сохранений («flag lvnosave»): загрузка берёт уровень 1, а не уровень записи
static int toLevel;         // уровень пройден: следующий (главный цикл, levelAdvance)
// правка 348: MISSION SELECT -> миссия другого уровня: тот уровень тем же путём (levelAdvance), строка экрана — после загрузки
// (mvMissionSelectStartRow); msPendDev — запись-перенос с меткой DEV (правка 335: сюжетные уровня, кроме последней, пройдены);
// msDevSession — игра загружена из DEV SAVE (уровни MISSION SELECT открыты все, чьи наборы лежат рядом с EBOOT)
static int msPendRow = -1, msPendDev, msDevSession;
// №893 (Роб 23.10, Барт L6: «при переключении миссий один раз не было загрузки с газетой»): MISSION SELECT внутри уровня стартовал миссию без загрузки, газета шла
// только если старт переносил игрока дальше 80 м от камеры (msnTeleport) — «плавающее» поведение: близкие миссии, текущая и соседние шли без неё. Правило: выбор миссии
// в MISSION SELECT — всегда газета (перенос ближе 80 м и отсутствие переноса — тоже). msNewsPend — кадры ожидания переноса от миссии (0 — нет выбора), по ним же
// запасной вариант без переноса. MS_NEWS_OLD=1 — прежнее (газета только при переносе дальше 80 м)
#ifndef MS_NEWS_OLD
#define MS_NEWS_OLD 0
#endif
#define MS_NEWS_WAIT 6   // кадров ждём перенос миссии (он приходит в кадре выбора или следующем), потом газета на месте игрока
static int msNewsPend;
// №894 (Роб 23.10: «Mission Select в паузе открылся в чужой рамке: листать ◄ ► нельзя, виден только текущий уровень, шрифт чужой и кривой»): это запасной список
// (msn_view.c mvMissionSelect: нет пакета экрана — selRows 0): пакет pextra.shr не открылся (чтение / память после череды загрузок и переносов), результат uiScrOpen
// не проверялся, повторов не было. Теперь: до 3 повторов с освобождением (готовый пакет будки, ожидание загрузчика, пауза), каждая неудача — строка в журнал и
// чёрный ящик (op fail) с кодом и памятью. MS_OPEN_OLD=1 — прежнее (одна попытка без проверки)
#ifndef MS_OPEN_OLD
#define MS_OPEN_OLD 0
#endif
// №1065 (журналы TEST74–77, окно Оптимизация; Роб 28.10: «переход на L4 без газеты»): load.shr (≈1,08 МБ, куски до 1 МБ) открывался на смене уровня без проверки результата — при отказе
// выделения (-3) оставался чёрный кадр без газеты (см. «нет пакета — чёрный кадр» ниже). Теперь: отказ -> место (cellsMakeRoom через mem1060Pre) и один повтор; на смене уровня, если не помогло,
// предыдущий уровень выгружается ДО газеты (куча пуста) и открытие повторяется. То же с банком звука (sfx.sab ≈ 2,1 МБ, TEST77 L5 остался без звука: levelLoad теперь открывает ui.shr и звук
// первыми после выгрузки уровня, пока куча пуста; sndLevel при отказе не выставляется). MEMLOAD1065_OLD=1 / файл memload1065old / «flag memload1065 0» — прежнее
#ifndef MEMLOAD1065_OLD
#define MEMLOAD1065_OLD 0
#endif
static int memLoad1065 = !MEMLOAD1065_OLD;
static int scrFail1065;   // служебное «flag scrfail1065 N»: следующие N открытий load.shr отказывают (-3) — проверка повтора без тесной кучи
static int scrOpen1065(const char *site) {
  char fn[256]; sibling(fn, "load.shr");
  int rc = scrFail1065 > 0 ? (scrFail1065--, uiScrClose(), -3) : uiScrOpen(fn);
  if (!memLoad1065 || rc != -3) return rc;
  unsigned f0 = heapFreeBytes(), b0 = memMaxBlock ? memMaxBlock() : 0;
  mem1060Pre(1u << 20, "load.shr");
  int rc2 = scrFail1065 > 0 ? (scrFail1065--, uiScrClose(), -3) : uiScrOpen(fn);
  if (smkOn) { char l_[200]; snprintf(l_, sizeof l_, "SCROPEN1065 site=%s rc=%d->%d free=%.2f->%.2f blk=%.2f->%.2f t=%.1f\n", site, rc, rc2, f0 / 1048576.f, heapFreeBytes() / 1048576.f, b0 / 1048576.f, memMaxBlock ? memMaxBlock() / 1048576.f : 0.f, smkT); smkWrite(l_); }
  return rc2;
}
static int msOpenFail;   // служебное «flag msopenfail N»: следующие N попыток открыть пакет MISSION SELECT отказывают (-3) — проверка повторов и запасного списка (гейт tests/menu896)
static int msOpenTry(const char *fn) { if (msOpenFail > 0) { msOpenFail--; uiScrClose(); return -3; } return uiScrOpen(fn); }
static int msPackOpen(const char *fn) {
  int rc = msOpenTry(fn);
  for (int t = 0; rc && !MS_OPEN_OLD && t < 3; t++) {
    char s_[112]; snprintf(s_, sizeof s_, "mssel_open_rc=%d_try=%d_lv=%d_free=%.1fM", rc, t, gameLevel, smokeHeapFree());
    bbOp("fail", s_, 1);
    if (smkOn) { snprintf(s_, sizeof s_, "MSNSEL openfail rc=%d try=%d level=%d free=%.2f t=%.1f\n", rc, t, gameLevel, smokeHeapFree(), smkT); smkWrite(s_); }
    if (t == 0) uiScrWarmClear();                                             // пакет будки в куче — первым
    { LdWait lw_ = {0}; lqn = 0; __asm__ volatile("" ::: "memory"); while (loaderBusy) { loaderWaitStep(&lw_, "busy", 0, 0); sceKernelDelayThread(1000); } }   // загрузчик — на месте
    if (t == 2) sceKernelDelayThread(100000);
    rc = msOpenTry(fn);
  }
  return rc;
}
static int saveBlocked;   // «flag nosave» среди первых flag сценария (или flag nosave 1): тур не пишет settings.bin и сохранения (савы Роба не трогать)
static int playerFirst;   // №автотур «ИГРОК»: сценарий начинается с «flag player» — режим игрока с загрузки (титул, меню, RESUME как у игрока); press/wait/shot работают
#define svcStartP() (svcStart() && !playerFirst)
static int svcFile(void) { char fn[256]; sibling(fn, "service"); SceUID h = sceIoOpen(fn, PSP_O_RDONLY, 0); if (h >= 0) { sceIoClose(h); return 1; } return 0; }
// Экраны вокруг игры из Scrooby оригинала (tools/load_export.py): boot.shr — лицензия, BootupLoad, титульный экран;
// load.shr — загрузка уровня (Loading.scr) и меню (LoadingFE.scr). Нет пакета — чёрный кадр: своего не рисуем.
// Кадр: чёрный фон + экран открытого пакета; progress 0..1000 — фитиль, часы — секунды с scrT0 (кадры огня/анимации).
// vsync 0 — между чтениями файлов (как прежняя шкала: загрузка не ждёт развёртку)
extern int memFailRingGet(unsigned i, unsigned *bytes, unsigned *ra);
static int newsLvRed;   // №663: красный контроль гейта «газета своего уровня» (flag newslvred 1): газета всегда уровня 1
#define LEVEL_IDX (newsLvRed ? 0 : gameLevel - 1)   // газета экрана загрузки уровня (кадр 0..6 пакета load.shr)
static unsigned scrT0; static int scrLastProg; static float scrLastT;   // для строки SCRSHOT (сверка гейтом со снимком)
// Правка Роба 154 (01.10) «попадание в меню с титульного или с демо — кружочком ирис: всё чёрное, в центре кружок; сейчас
// резко, без изинга и анимации». Ирис оригинала (transit.c, ключи iris.shr): кнопка на титульном/в демо — ирис закрывается
// над последним их кадром (transitClose), меню рисуется и ирис раскрывается над ним (кадры меню: irisOver). Простой меню ->
// титульный — тем же ирисом (меню закрывается, титульный раскрывается). «flag noiris» — без ириса (контроль гейта)
static int scrIris; static unsigned irisLast;
static void irisOver(void) {   // внутри открытого списка GE, поверх кадра: ирис раскрывается (время — между кадрами)
  unsigned now = sceKernelGetSystemTimeLow(); float dt = irisLast ? (now - irisLast) * 1e-6f : 0.f; irisLast = now;
  if (dt > .05f) dt = .05f;
  if (!transitBusy()) return;
  transitStep(dt); transitDraw(); resetState();
}
#define LOAD_BLINK_S 1.0f    // правка 244: период мигания LOADING на газете, с; доля видимого — LOAD_BLINK_ON (правило порта)
#define LOAD_BLINK_ON 0.65f
static int loadBlinkOff, loadBlinkLast = -1, loadBlinkN;   // служебное «flag loadblinkoff» — прежнее (горит всё время)
// №409 (Роб: «на PSP фон-градиент под логотипом ступеньками?»): ЗАМЕР (tests/title/title.sh, tools/title_band.py): страница
// логотипа CLUT8 держит свечение (25 из 27 разных цветов строки входа) — не причина; ступени даёт кадр 5650 (зелёный 6 бит,
// красный/синий 5): без дизеринга 10 цветов на строку и ровные полосы ~4,5 px, с дизерингом GE 4x4 (железо и программный
// рендер эмулятора) — правильная средняя, но сетка 4x4 на тёмном свечении (как у картинок будки, правка 230). Решение — в данных:
// страница логотипа заранее рассеяна (Флойд–Стейнберг) в палитру точных цветов 5650 (tools/ui_export.py quantize_page_565,
// группа logo в tools/load_export.py), а титульный рисуется без дизеринга GE — цвета ложатся в кадр без потерь и без сетки.
// -DTITLE_DITHER_OLD=1 — прежнее: дизеринг GE и на титульном (красный контроль вместе с UI_LOGO_FS_OLD=1 у экспорта)
#ifndef TITLE_DITHER_OLD
#define TITLE_DITHER_OLD 0
#endif
// №1060б: экран загрузки, который рисуют loadBegin / loadFrame / loadEnd: UI_LOAD_LEVEL (газета уровня) или UI_LOAD_FE (LoadingFE: Itchy & Scratchy, фитиль к бомбе — как при выходе из игры в меню); ставит mem1060.h на время «освободить → загрузить → вернуть»
static int ldScr1060 = UI_LOAD_LEVEL;
static int ldOn;   // №833: объявлено ниже (загрузка уровня идёт) — scrFrame выбирает векторные лучи только на экране загрузки уровня
static void scrFrame(int screen, int progress, int vsync, void **fbp) {
  scrLastProg = progress; scrLastT = (sceKernelGetSystemTimeLow() - scrT0) * 1e-6f;
  sceGuStart(GU_DIRECT, list); resetState();
  sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  if (screen == UI_LOAD_LEVEL) {   // правка Роба 244 (видео 03.10 1:02: «газета на загрузке стоит, хотя должно мигать»): LOADING
    int on = loadBlinkOff || fmodf(scrLastT, LOAD_BLINK_S) < LOAD_BLINK_S * LOAD_BLINK_ON;   // мигает, пока идёт загрузка (правило порта)
    if (on != loadBlinkLast) { loadBlinkLast = on; loadBlinkN++; if (smkOn && !loadFuseIoPumping()) { char s_[64]; snprintf(s_, sizeof s_, "LOADBLINK on=%d n=%d t=%.2f\n", on, loadBlinkN, scrLastT); smkWrite(s_); } }
    uiScrShow(UI_LOAD_LEVEL, UI_T_LOADING, on);
  }
  int dOff = fb16 && screen == UI_BOOT_SPLASH && !TITLE_DITHER_OLD;   // №409: логотип титульного — без дизеринга GE (см. TITLE_DITHER_OLD)
  if (dOff) sceGuDisable(GU_DITHER);
#ifndef LOAD_RAYS_VEC_OLD
#define LOAD_RAYS_VEC_OLD 0   // №833: 1 — звёзды экрана загрузки картинкой (drawSpin)
#endif
  if (!LOAD_RAYS_VEC_OLD && screen == UI_LOAD_LEVEL && ldOn) uiScrRays(UI_RAYS_LOAD);   // №833: лучи загрузки уровня — векторные (ui.c raysVec)
  uiScrDraw(screen, progress, scrLastT, LEVEL_IDX); resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  if (dOff) sceGuEnable(GU_DITHER);
  if (scrTvOver && tvFrameOn) { uiScrDraw(UI_BOOT_TVMOVIE, 0, 0.f, 0); resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); }
  if (scrIris) { irisOver(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); }   // правка 154: титульный под раскрывающимся ирисом
  sceGuFinish(); sceGuSync(0, 0); if (vsync) sceDisplayWaitVblankStart(); *fbp = swapBufG(vsync);
}
// снимок экрана пакета для гейта (tools/screens_check.py): строка с тем, что нарисовано, и SHOT (ждёт снимок окна)
static void scrShot(const char *name, const char *packName, int screen, const char *show) {
  if (!smkOn) return;
  char s[160]; snprintf(s, sizeof s, "SCRSHOT name=%s pack=%s scr=%d prog=%d t=%.6f level=%d show=%s\n", name, packName, screen,
                        scrLastProg, scrLastT, LEVEL_IDX, show);
  smkWrite(s); sceDisplayWaitVblankStart(); sceDisplayWaitVblankStart(); smokeShotNow(name);
}
// Показ экрана загрузки уровня (правка Роба 35, видео 28.09 2:28–2:36: «газета лагает на старте; газета почти на весь
// экран; загрузку длиннее, чтобы её было видно»). Сессия показа: loadBegin — газета влетает (разворот и приближение от
// центра её места, прозрачность) ровными кадрами по развёртке ДО чтения файлов (раньше огонь и часы кадров начинались
// заново в cellsRingAt — рывок на старте); loadFrame(доля 0..1000) — между чтениями, огонь по фитилю = не впереди
// настоящего чтения и не впереди хода минимального показа (монотонно); loadEnd — после чтения кадры по развёртке, пока
// показ не достигнет LOAD_MIN_S и огонь не дойдёт до динамита. Числа LOAD_SPIN_S и LOAD_MIN_S и ход разворота — правило
// порта: в данных оригинала их нет (разворот — код; эталон PS2 build/ps2ref/m4.png — газета в полёте повёрнута и крупнее
// места; на PS2 загрузка идёт дольше, чем на PSP). Строки LOADSPIN / LOADSHOW — для tools/screens_check.py.
// Служебные флаги гейта (отрицательные контроли): «flag loadfast» — без минимального показа, «flag spinlag» — 120 мс
// на кадр разворота (рывки).
#define LOAD_SPIN_S 0.9f
// 15.10 (видео TEST17 + замер PS2): у оригинала газета при NEW GAME держится ~2,0 с (от ✕ до мира 3,2 с вместе с наездом камеры и чёрным), при старте
// миссии газеты нет вовсе — карточка с LOADING 1,1 с и сразу Continue (ps2-newgame-newspaper.yaml, ps2-mission-card-hands.yaml; замер в эмуляторе — нижняя граница).
// Минимум показа 3,5 с был правилом порта; теперь 2,0 с. Хвост показа греет листы клеток вокруг старта (№538, loadWarmStep): при коротком минимуме
// греется меньше — смотреть первые кадры мира в туре (FIRSTFRAME, missed). LOAD_MIN_OLD=1 — прежние 3,5 с
#ifndef LOAD_MIN_OLD
#define LOAD_MIN_OLD 0
#endif
#define LOAD_MIN_S (LOAD_MIN_OLD ? 3.5f : 2.0f)
#define LOAD_RES_SHARE 500   // доля фитиля на чтение резидентной части уровня при первой загрузке (дальше — клетки города)
static unsigned ldT0; static int ldOn, ldShown, ldBase, ldFast, ldLag, ldAudio;   // ldAudio: звук уровня ждёт конца загрузки (newGame)
static void levelAudioStart(void);
static int audioDefer;   // правка 91: NEW GAME — звук уровня только после газеты и ролика
static int levelAudioPending;   // правка 105: звук уровня — с первым нарисованным кадром игры (не под чёрным экраном после ролика)
// Правка Роба 155 (01.10) «при загрузке первого уровня перед видео начинает играть музыка уровня»: правка 91 откладывала только
// levelAudioStart из newGame, а звук уровня запрашивали и другие места под газетой (levelLoad: sndLevelStart — атмосфера,
// и всё, что просит потоки до ролика). NEW GAME из меню: после влёта газеты (стингер Newspaper_Spin правки 134 уже звучит)
// потоки музыки и атмосферы держатся (audio.c audioHoldStreams) до первого кадра игры после ролика; там удержанное
// выбрасывается и звук уровня ставит levelAudioStart (туба/атмосфера). «flag musold» — прежний порядок (контроль гейта)
static int levelHold, musOld;
static void sfxLine(const char *l);
static int sfxLogOn;
static void streamNote(int s, int track, float fade, int held) {   // smoke.txt: запросы потоков музыки (0) и атмосферы (1)
  if (s == 0 && sfxLogOn) { char m[96]; snprintf(m, sizeof m, "MUS %s fade=%.2f held=%d", track >= 0 ? audioTrackName(track) : "-", fade, held); sfxLine(m); }   // правка 279: музыка в sfxlog.txt на PSP
  if (!smkOn || (s > 1 && s != AUDIO_ST_CAR_OVERLAY)) return;
  char t[160]; snprintf(t, sizeof t, "STREAM s=%s track=%s fade=%.2f held=%d t=%.1f\n", s == AUDIO_ST_CAR_OVERLAY ? "car" : s ? "amb" : "music",
                        track >= 0 ? audioTrackName(track) : "-", fade, held, smkT); smkWrite(t);
}
#ifndef TUT_RESUME_OLD
#define TUT_RESUME_OLD 0
#endif
#define TUT_RESUME_OLD_DEF TUT_RESUME_OLD
static int tutResumeOld = TUT_RESUME_OLD_DEF;   // №670: откат (файл tutresumeold рядом с EBOOT / flag tutresumeold 1)
static int tutSvc;   // служебный запуск: подсказки Барта только с «flag tutorial» (иначе сценарии смоука ждали бы X)
#define TUT_GIVER_M 20.f   // подсказки гонки / бонус-миссии / заезда: значок ближе стольких метров (правило порта; запись PS2 16:55, 22:55)
#define TUT_BOOM_S 2.f     // подсказка «машина взорвана» после взрыва (запись PS2: взрыв 33:33, подсказка 33:36 — Гомер уже упал)
static float ldSec(void) { return (sceKernelGetSystemTimeLow() - ldT0) * 1e-6f; }
// №663 (закон 7, гейт по кадру): «flag newsshot 1» — служебные снимки изнутри экрана загрузки на часах газеты 1,0 и 2,0 с (news_1s, news_2s), только в smkOn: у игрока ноль цены.
// Путь «ирис без газеты» (START_NEWS_OLD=1, красный контроль) снимает те же имена на чёрном экране: newsShotIris
static int newsShotOn, newsShotK; static const float newsShotT[2] = {1.0f, 2.0f}; static const char *const newsShotN[2] = {"news_1s", "news_2s"};
static void newsShotTick(void) {
  if (!smkOn || !newsShotOn || newsShotK >= 2 || ldSec() < newsShotT[newsShotK]) return;
  scrShot(newsShotN[newsShotK], "load", UI_LOAD_LEVEL, "-"); newsShotK++;
}
static void newsShotIris(void) { if (!smkOn || !newsShotOn) return; while (newsShotK < 2) { scrShot(newsShotN[newsShotK], "iris", UI_LOAD_LEVEL, "-"); newsShotK++; } }
// Правка Роба 508 («фитиль мигать и ровно ползти, загрузка пусть будет медленнее, лишь бы без рывков»): прежний ход фитиля — min(чтение, часы) шёл СТУПЕНЯМИ чтения (доля
// прыгала на 1/6 шкалы между этапами) и стоял, пока этап не кончится. Теперь фитиль ползёт по своим часам (LOAD_FUSE_TIME_S до динамита), чтение может лишь ускорить его
// (цель = max(часы, чтение)), скорость не больше 1000/(LOAD_MIN_S − LOAD_SPIN_S) в секунду — не рывками и к динамиту не раньше минимального показа; назад не идёт.
// -DLOAD_SMOOTH_OLD=1 — прежнее (вместе с порогом кадра 40 мс и без остановок ldStep)
#ifndef LOAD_SMOOTH_OLD
#define LOAD_SMOOTH_OLD 0
#endif
#define LOAD_FUSE_TIME_S 6.f
static float ldFuseF; static unsigned ldFuseLast; static int ldFuseJump, ldFuseCalls;
static int ldProgress(int real) {   // огонь: по своим часам, чтение лишь ускоряет, не быстрее предела скорости, только вперёд
  if (LOAD_SMOOTH_OLD) {
    float t = ldSec() - LOAD_SPIN_S; int tp = t <= 0.f ? 0 : (int)(t / (LOAD_MIN_S - LOAD_SPIN_S) * 1000.f);
    if (ldFast || tp > 1000) tp = 1000;
    int prev = ldShown; int p = real < tp ? real : tp; if (p > ldShown) ldShown = p; if (ldShown > 1000) ldShown = 1000;
    if (ldShown - prev > ldFuseJump) ldFuseJump = ldShown - prev; ldFuseCalls++;
    return ldShown;
  }
  unsigned now = sceKernelGetSystemTimeLow(); float dt = ldFuseLast ? (now - ldFuseLast) * 1e-6f : 0.f; ldFuseLast = now; if (dt > .1f) dt = .1f;
  float t = ldSec() - LOAD_SPIN_S; int clock_ = t <= 0.f ? 0 : (int)(t / LOAD_FUSE_TIME_S * 1000.f);
  int target = real > clock_ ? real : clock_; if (ldFast || target > 1000) target = 1000;
  float step = dt * 1000.f / (LOAD_MIN_S - LOAD_SPIN_S);
  if ((float)target > ldFuseF) ldFuseF += fminf((float)target - ldFuseF, step);
  if (ldFuseF > 1000.f) ldFuseF = 1000.f;
  int prev = ldShown; ldShown = ldFuseF >= 999.5f ? 1000 : (int)ldFuseF;
  if (ldShown - prev > ldFuseJump) ldFuseJump = ldShown - prev; ldFuseCalls++;   // журнал LOADFUSE: самый большой скачок фитиля между кадрами (‰) — ровность хода
  return ldShown;
}
static void newsFly(float u) {   // газета: u 0..1 — два оборота к месту, приближение и проявление, мягкая остановка
  if (u < 0.f) u = 0.f; if (u > 1.f) u = 1.f;
  float e = 1.f - (1.f - u) * (1.f - u) * (1.f - u);            // easeOutCubic
  uiScrNews(-4.f * 3.14159265f * (1.f - e), .08f + .92f * e, u < .3f ? u / .3f : 1.f);
}
static int fePend; static unsigned fePendAt; static void feStingerPoll(void);   // правка 134: стингер газеты ждёт долю темы — запускается из кадров разворота
// Правка Роба 336 (05.10): звёзды экрана загрузки теперь крутятся — два полупрозрачных слоя во весь экран поверх фона (viewer/ui.c
// drawSpin), заливка кадра газеты почти вдвое больше. Кадр между чтениями (loadFrame) рисуется не чаще LOAD_FRAME_MIN_MS: чаще
// он не нужен (подкачка loadPump — раз в 40 мс, экран — 60 Гц), а каждый лишний кадр ждёт GE (sceGuSync) и задерживает
// чтение с карты. Журнал: строка LOADREAD (smoke.txt) — длительность чтения, кадров нарисовано и пропущено.
// -DLOAD_FRAME_MIN_MS=0 — прежнее (кадр на каждый вызов)
#ifndef LOAD_FRAME_MIN_MS
#define LOAD_FRAME_MIN_MS 33
#endif
static unsigned ldReadT0; static int ldFrames, ldSkips;
/* #385 service evidence: capture completed framebuffer pixels in RAM while the
   msio lock is held; write them only after loading ends and the lock is free. */
static void *fuseProbePixels[2];
static int fuseProbeN, fuseProbeFrames, fuseProbeStride[2], fuseProbeFormat[2], fuseProbeProgress[2];
static float fuseProbeTime[2];
static void fuseProbeArm(unsigned us) {
  for (int i=0;i<2;i++) { free(fuseProbePixels[i]); fuseProbePixels[i]=us?malloc(512*272*4):0; }
  fuseProbeN=fuseProbeFrames=0; loadFuseIoSlow(us,8);
}
static void fuseProbeGrab(void) {
  if (!loadFuseIoProbe() || !loadFuseIoPumping()) return;
  fuseProbeFrames++;
  if (fuseProbeN>=2 || !fuseProbePixels[fuseProbeN]) return;
  if (fuseProbeN && ((int)(scrLastT*15.f)&1)==((int)(fuseProbeTime[0]*15.f)&1)) return;
  void *pixels=0; int stride=0,format=0;
  if (sceDisplayGetFrameBuf(&pixels,&stride,&format,PSP_DISPLAY_SETBUF_IMMEDIATE)<0 || !pixels || stride<480 || stride>512) return;
  int i=fuseProbeN; unsigned bytes=stride*272*(format==PSP_DISPLAY_PIXEL_FORMAT_8888?4:2);
  memcpy(fuseProbePixels[i],pixels,bytes); fuseProbeStride[i]=stride; fuseProbeFormat[i]=format;
  fuseProbeTime[i]=scrLastT; fuseProbeProgress[i]=scrLastProg; fuseProbeN++;
}
static void fuseProbeFlush(void) {
  if (!loadFuseIoProbe()) return;
  for (int i=0;i<fuseProbeN;i++) {
    char name[64],path_[256],line_[160]; snprintf(name,sizeof name,"rfsnap_fuse385_%d.bin",i); sibling(path_,name);
    unsigned bytes=fuseProbeStride[i]*272*(fuseProbeFormat[i]==PSP_DISPLAY_PIXEL_FORMAT_8888?4:2);
    SceUID fd_=sceIoOpen(path_,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC,0777);
    int wrote=fd_>=0?sceIoWrite(fd_,fuseProbePixels[i],bytes):-1; if(fd_>=0)sceIoClose(fd_);
    snprintf(line_,sizeof line_,"LOADFUSE385 image=%d t=%.6f prog=%d level=%d stride=%d format=%d bytes=%d pending=1\n",i,fuseProbeTime[i],fuseProbeProgress[i],LEVEL_IDX,fuseProbeStride[i],fuseProbeFormat[i],wrote);smkWrite(line_);
  }
  char line_[96];snprintf(line_,sizeof line_,"LOADFUSE385 frames=%d images=%d old=%d\n",fuseProbeFrames,fuseProbeN,LOAD_FUSE_ANIM_OLD);smkWrite(line_);
  for(int i=0;i<2;i++){free(fuseProbePixels[i]);fuseProbePixels[i]=0;}
  loadFuseIoSlow(0,0);
}

static void loadPump(void);
static void loadReadPump(void);
static unsigned ldFrameLast, ldGapMax; static int ldGapAt;
static unsigned ldStepT, ldStepGap;   // отметка прошлого ldStep (журнал LOADSTEP: какой шаг загрузки держит главный поток дольше 60 мс без кадра)
static void **ldFbp; static int ldPermLast, loadPumpOff;
static void levelPreread(void);
static void loadWaitNote(const char *what) { bbOp("loadwait", what, 0); if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "LOADWAIT %s t=%.1f\n", what, smkT); smkWrite(s_); } }   // №LOADWAIT: чтение с карты не завершилось (load_fuse_io.c)
static void loadBegin(void **fbp) {   // Loading.scr открыт: газета влетает ровными кадрами, потом начинается чтение
  levelPreread();   // №PREREAD: чтение резидентной части — в фон на время влёта
  loadFuseIoEnd(); ldFbp = fbp; ldPermLast = 0; ldFrameLast = ldGapMax = 0;
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "LOADNEWS idx=%d level=%d t=%.1f\n", LEVEL_IDX, gameLevel, smkT); smkWrite(s_); }   // №663: какая газета в ПЕРВОМ кадре (гейт tests/levels/news_levels.sh)
  ldOn = 1; ldShown = 0; ldFuseF = 0.f; ldFuseLast = 0; ldFuseJump = ldFuseCalls = 0; ldStepT = 0; scrT0 = ldT0 = sceKernelGetSystemTimeLow();
  unsigned prev = ldT0; float gap = 0.f; int n = 0;
  for (;;) {
    float t = ldSec(); if (t >= (ldScr1060 == UI_LOAD_LEVEL ? LOAD_SPIN_S : 0.f)) break;   // №1060б: LoadingFE без влёта газеты
    newsFly(t / LOAD_SPIN_S); feStingerPoll();
    if (ldLag) sceKernelDelayThread(120000);
    scrFrame(ldScr1060, 0, 1, fbp); n++;
    unsigned now = sceKernelGetSystemTimeLow(); float g = (now - prev) * 1e-3f; prev = now; if (g > gap) gap = g;
  }
  if (ldScr1060 == UI_LOAD_LEVEL) uiScrNews(0.f, 1.f, 1.f);
  loadFuseIoNote = loadWaitNote; loadFuseIoBegin(loadReadPump);
  if (ldSlowUs) loadFuseIoSlow(ldSlowUs, 1u << 30);   // №775: «flag ldslow N» — медленная карта на всю загрузку (сбрасывает loadEnd → fuseProbeFlush)
  ldReadT0 = sceKernelGetSystemTimeLow(); ldFrames = ldSkips = 0;   // правка 336: отсюда — чтение (журнал LOADREAD в loadEnd)
  if (fePend) { fePendAt = sceKernelGetSystemTimeLow(); feStingerPoll(); }   // доля не дождалась кадра — не теряем стингер
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "LOADSPIN frames=%d s=%.2f gap_ms=%.1f lag=%d t=%.1f\n", n, ldSec(), gap, ldLag, smkT); smkWrite(s); }
}

#define LOAD_PUMP_MS (LOAD_SMOOTH_OLD ? 40 : 33)   // правка 508: кадр газеты не реже, чем раз в 2 vblank (33 мс); правка 244: долгие циклы загрузки рисуют кадр газеты не реже (огонь и LOADING не стоят; правило порта)

static void loadFrame(int permille, void **fbp) {   // загрузка уровня: Loading.scr, огонь идёт по фитилю к динамиту
  if (LOAD_FRAME_MIN_MS > 0 && ldFrameLast && permille < 1000 && sceKernelGetSystemTimeLow() - ldFrameLast < LOAD_FRAME_MIN_MS * 1000u) {
    ldFbp = fbp; ldPermLast = permille; ldSkips++; return; }   // правка 336: кадр был только что — не ждём GE между чтениями
  ldFrames++;
  { unsigned now = sceKernelGetSystemTimeLow(); if (ldFrameLast && now - ldFrameLast > ldGapMax) { ldGapMax = now - ldFrameLast; ldGapAt = permille; }
    if (ldFrameLast && now - ldFrameLast > ldStepGap) ldStepGap = now - ldFrameLast;
    if (smkOn && !loadFuseIoPumping() && ldFrameLast && now - ldFrameLast > 150000u) { char s_[80]; snprintf(s_, sizeof s_, "LOADGAP ms=%u at=%d t=%.2f\n", (now - ldFrameLast) / 1000u, permille, scrLastT); smkWrite(s_); }
    ldFrameLast = now; }
  ldFbp = fbp; ldPermLast = permille;
  scrFrame(ldScr1060, ldProgress(permille), 0, fbp);
  fuseProbeGrab();
}
static void loadPump(void) {
  if (!ldOn || !ldFbp || loadPumpOff || sceKernelGetSystemTimeLow() - ldFrameLast < LOAD_PUMP_MS * 1000u) return;
  if (smkOn) { static unsigned musT; unsigned now_ = sceKernelGetSystemTimeLow();   // №665: музыка под газетой — раз в 0,5 с (дорожка, состояние потока, позиция); обрыв виден как смена дорожки/state
    if (now_ - musT >= 500000u) { musT = now_; char a_[200]; int l_ = snprintf(a_, sizeof a_, "LOADMUS ld=%.2f ", ldSec()); l_ += audioMusicState(a_ + l_, sizeof a_ - l_); snprintf(a_ + l_, sizeof a_ - l_, "\n"); smkWrite(a_); } }
  loadFrame(ldPermLast, ldFbp);
}
#ifndef LOAD_FIRE_PUMP_OLD
#define LOAD_FIRE_PUMP_OLD 0
#endif
// Шаг загрузки закончен: кадр газеты (если пора) и, в журнале, длина шага > 60 мс (LOADSTEP). Вызывается между шагами levelLoad / levelLoadAfter / newGame — каждый шаг,
// что считал без кадров, теперь виден и режется (правка 508). LOAD_SMOOTH_OLD=1 — только кадр, как раньше loadPump
#ifndef LOADSTEP_LOG_US
#define LOADSTEP_LOG_US 45000u   // №517: журнал LOADSTEP — промежутки > 45 мс (было 70): цель газеты — ни одного промежутка > 50 мс
#endif
#ifndef LOADPH
#define LOADPH 1   // строки LOADPH имя ms= в smoke.txt (0 — нет)
#endif
static void ldStep(const char *what) {
  bbStep = what;
  loadPump();
  if (LOAD_SMOOTH_OLD || !smkOn) return;
  unsigned now = sceKernelGetSystemTimeLow();
  unsigned gap = ldStepGap; if (ldFrameLast && now - ldFrameLast > gap) gap = now - ldFrameLast;   // самый долгий промежуток между кадрами внутри шага (и до конца шага)
  if (ldStepT && gap > LOADSTEP_LOG_US) { char s_[110]; snprintf(s_, sizeof s_, "LOADSTEP ms=%u gap=%u after=%s t=%.2f\n", (now - ldStepT) / 1000u, gap / 1000u, what, ldSec()); smkWrite(s_); }
  if (LOADPH && ldStepT) { char s_[80]; snprintf(s_, sizeof s_, "LOADPH %s ms=%u\n", what, (now - ldStepT) / 1000u); smkWrite(s_); }   // загрузка 2: время каждого шага (аудит 15.10 п.5)
  ldStepT = now; ldStepGap = 0;
}
static void loadReadPump(void) {
  /* Native two-frame flame runs at 15 fps (frame = (int)(t*15) & 1, phase 66.7 ms). Правка 499 (=385, 3-й повтор): прежний порог 66667 мкс равен самой фазе,
     а loadPump сам держит не меньше LOAD_PUMP_MS между кадрами: при джиттере чтения кадр пропускал фазу и огонь «замирал» на обоих кадрах подряд. Теперь порог
     в полфазы (внутри loadPump реально ≥40 мс): за каждую фазу минимум один кадр, кадры огня чередуются без пропусков. LOAD_FIRE_PUMP_OLD=1 — прежнее. */
  if (sceKernelGetSystemTimeLow() - ldFrameLast >= (LOAD_FIRE_PUMP_OLD ? 66667u : 33333u)) loadPump();
}
static void firstArmNow(void);
static void loadWarmStep(void);
static int loadWarmPending(void);
// №665 (Роб 16.10): «музыка на загрузке начинает играть и сбивается; должна доигрывать всю загрузку». Стингер газеты (Newspaper_Spin, 3,8 с) длиннее минимума показа (2,0 с), и
// levelAudioStart на первом кадре игры резал его (sndMusic(OFF)). Теперь газета держится, пока стингер не доиграл (не дольше LOAD_NEWS_MUSIC_CAP_S от начала загрузки).
// LOAD_NEWS_MUSIC_OLD=1 — прежнее (стингер режется концом загрузки); файл loadsilent на карте — тишина на газете, как у PS2 (запись New Game: музыка меню стихает за 0,2 с)
#ifndef LOAD_NEWS_MUSIC_OLD
#define LOAD_NEWS_MUSIC_OLD 0
#endif
#ifndef LOAD_NEWS_MUSIC_CAP_S
#define LOAD_NEWS_MUSIC_CAP_S 4.6f
#endif
static int loadNewsMusicOld;   // файл loadnewsmusicold рядом с EBOOT: стингер газеты режется концом загрузки (№665 откат)
static int loadSilent;   // файл loadsilent рядом с EBOOT (main_loop.h) или flag loadsilent 1
static float feNewsMusicLeft(void);   // секунд до конца играющего стингера газеты (0 — не играет), ниже у feStinger
static void loadEnd(void **fbp) {   // чтение кончилось: показ не короче LOAD_MIN_S, огонь доходит до динамита
  loadFuseIoEnd(); fuseProbeFlush();
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "LOADFUSE508 old=%d calls=%d max_jump=%d permille\n", LOAD_SMOOTH_OLD, ldFuseCalls, ldFuseJump); smkWrite(s_); }
  if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "LOADREAD ms=%u frames=%d skip=%d min_ms=%d t=%.1f\n",   // правка 336: время чтения
                                   (sceKernelGetSystemTimeLow() - ldReadT0) / 1000u, ldFrames, ldSkips, LOAD_FRAME_MIN_MS, smkT); smkWrite(s_); }
  ldWarmStep_ = 0;
  while (ldOn && ldShown < 1000) { scrFrame(ldScr1060, ldProgress(1000), 1, fbp); newsShotTick(); loadWarmStep(); }   // №538: простой показа (до 3 с) — прогрев листов клеток и B1 вокруг старта
  if (!LOAD_NEWS_MUSIC_OLD && !loadNewsMusicOld && ldOn) { float left0 = feNewsMusicLeft(), held = 0.f; unsigned h0 = sceKernelGetSystemTimeLow();
    while (ldOn && feNewsMusicLeft() > 0.05f && ldSec() < LOAD_NEWS_MUSIC_CAP_S) { scrFrame(ldScr1060, 1000, 1, fbp); newsShotTick(); loadWarmStep(); }
    held = (sceKernelGetSystemTimeLow() - h0) * 1e-3f;
    if (smkOn) { char s_[112]; snprintf(s_, sizeof s_, "NEWSMUS left0=%.2f held_ms=%.0f left=%.2f ld=%.2f t=%.1f\n", left0, held, feNewsMusicLeft(), ldSec(), smkT); smkWrite(s_); } }
  // LOAD_UNTIL_CELLS (слово дирижёра 15.10, TEST18: после газеты 2,0 вместо 3,5 с в первые секунды мира собиралось 109 и 18 клеток в секунду, пропуски кадров): газета держится, пока очередь
  // прогрева (клетки до 160 м от спавна без собранного листа) не пуста, но не дольше LOAD_MAX_S (3,5 с — прежний минимум). Минимум 2,0 с (как у оригинала) остаётся. LOAD_UNTIL_CELLS_OLD=1 — прежнее
#ifndef LOAD_UNTIL_CELLS_OLD
#define LOAD_UNTIL_CELLS_OLD 0
#endif
#define LOAD_MAX_S 3.5f
  int ldPend0 = 0, ldPendFr = 0; float ldPendT0 = ldSec();
  if (!LOAD_UNTIL_CELLS_OLD) {
    ldPend0 = loadWarmPending();
    // Ревью дифа 16.10: газета держится, только пока очередь прогрева убывает (не дольше LOAD_STALL_S без убыли), а не до потолка впустую. LOAD_STALL_OLD=1 — до LOAD_MAX_S как раньше
#ifndef LOAD_STALL_OLD
#define LOAD_STALL_OLD 0
#endif
#define LOAD_STALL_S 0.4f
    int ldLast = ldPend0; float ldLastT = ldSec();
    while (ldOn && ldSec() < LOAD_MAX_S && loadWarmPending() > 0) { scrFrame(ldScr1060, 1000, 1, fbp); newsShotTick(); loadWarmStep(); ldPendFr++;
      int pn_ = loadWarmPending(); if (pn_ < ldLast) { ldLast = pn_; ldLastT = ldSec(); } else if (!LOAD_STALL_OLD && ldSec() - ldLastT > LOAD_STALL_S) break; }
    if (smkOn) { char s[128]; snprintf(s, sizeof s, "LOADCELLS pending0=%d left=%d extra_ms=%.0f frames=%d t=%.1f\n", ldPend0, loadWarmPending(), (ldSec() - ldPendT0) * 1000.f, ldPendFr, smkT); smkWrite(s); }
  }
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "LOADSHOW ms=%.0f min_ms=%.0f spin_ms=%.0f fast=%d t=%.1f\n", ldSec() * 1000.f,
                                     LOAD_MIN_S * 1000.f, LOAD_SPIN_S * 1000.f, ldFast, smkT); smkWrite(s); }
  ldOn = 0; ldBase = 0; ldWarmOn = 0;
  if (ldAudio) { ldAudio = 0; levelAudioPending = 1; }   // атмосфера и музыка уровня — с первым кадром игры (правка 105), не под газетой
  firstArmNow();
}
static int firstArm, firstFr, firstMissFr, firstMissMax, firstMiss0; static unsigned firstT0;
static void firstArmNow(void) { firstArm = 1; firstFr = firstMissFr = firstMissMax = 0; firstMiss0 = -1; firstT0 = 0; }
static void firstCount(void) {   // показан кадр игры: видимые незагруженные клетки в первом кадре и за первую секунду
  if (!smkOn || !firstArm) return;
  if (fsInt > 0.f && fsN > 0) { fsLast = 0; framesheetTick(); fsInt = 0.f; }   // №663 лист: первый кадр уровня — последний снимок листа
  unsigned now = sceKernelGetSystemTimeLow(); if (!firstT0) firstT0 = now;
  firstFr++; if (stMissNow) firstMissFr++; if (stMissNow > firstMissMax) firstMissMax = stMissNow;
  if (firstMiss0 < 0) { firstMiss0 = stMissNow;
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "FIRSTFRAME level=%d missvis=%d t=%.1f\n", gameLevel, stMissNow, smkT); smkWrite(s); } }
  if (now - firstT0 >= 1000000u) { firstArm = 0;
    if (smkOn) { char s[112]; snprintf(s, sizeof s, "FIRSTSEC level=%d frames=%d miss_frames=%d max=%d t=%.1f\n", gameLevel, firstFr, firstMissFr, firstMissMax, smkT); smkWrite(s); } }
}

static int flyNoFrame;   // служебное (отрицательный контроль гейта tools/fe_gate.sh): в перелёте меню не рисуется совсем
// Правка 20 (29.09) «звуки перелётов меню»: музыкальные события композиции уровня l?_music.rms (tools/rms.py) —
// Loading_screen (play_region FE_trans01: один из FE_trans01..03) и Newspaper_Spin (FE_trans02: NewsPaper_Spin01/02); дорожки
// music.sap "loading_screen", "loading_screen~k", "newspaper_spin", "newspaper_spin~k" (tools/sound_export.py). play_region
// сменяет музыку (тема меню стихает), выбор — случайный (logic_or). Когда именно звучат — правило порта (код оригинала не на
// диске, в записи PS2 меню нет): × на NEW GAME — Loading_screen с началом перелёта камеры к газете; газета влетает на экране
// загрузки (из меню и при переходе на следующий уровень) — Newspaper_Spin
// Правка Роба 134 (01.10) «при наезде на газету неправильная музыка — должна быть стартовая, зажигательная». По данным
// l1_music.rms (tools/rms.py, гейт tests/fe_music/run.sh): переходы fade_transitions есть только FE_region -> FE_trans01 и
// FE_region -> FE_trans02 (затухание 400 мс, доли {0,1,2,3}), перехода FE_trans01 -> FE_trans02 НЕТ — за темой меню идёт ровно
// один стингер. Прежний порт давал два подряд (Loading_screen на X обрывал тему, через 1,3 с Newspaper_Spin обрывал его) —
// это и была «неправильная музыка». Решено терминалом за Роба: × на NEW GAME — тема Simpsons_Theme (зажигательная, 172 уд/мин)
// играет весь перелёт камеры к газете; газета влетает — один Newspaper_Spin, тема затухает 400 мс с ближайшей доли (темп из
// tempo_track потока). Вкус — FE_NEWS_MUSIC: 0 по данным; 1 тема звучит и под газетой (стингера нет); 2 прежнее.
// Служебный «flag femusold» = 2 (отрицательный контроль гейта). Путь следующего уровня (levelAdvance) без темы — стингер сразу
#ifndef FE_NEWS_MUSIC
#define FE_NEWS_MUSIC 0
#endif
#define FE_THEME_BPM 172.f     // l1_music.rms: streams Simpsons_Theme tempo_track.bpm (сверяет tools/fe_music_check.py)
#define FE_THEME_FADE_S 0.4f   // l1_music.rms: fade_transitions FE_region -> FE_trans01/02 source_time 400 мс
static int feMusicMode = FE_NEWS_MUSIC;
static int feStTrack = -1; static unsigned feStT0;           // последний стингер: дорожка и начало (для FESTCUT)
static char feStName[32]; static float feStLen;              // №789: имя и длина последнего стингера (продолжить на звуке нового уровня)
static char fePendEv[24]; static unsigned fePendT0;
static void feStingerFade(const char *ev, float fade, float waitMs) {
  int t[4], n = 0; char nm[40];
  for (int k = 0; k < 4; k++) { if (k) snprintf(nm, sizeof nm, "%s~%d", ev, k); else snprintf(nm, sizeof nm, "%s", ev);
                                int id = audioTrack(nm); if (id < 0) break; t[n++] = id; }
  int id = n ? t[rand() % n] : -1;
  int cur = audioStreamTrack(AUDIO_ST_MUSIC); unsigned now = sceKernelGetSystemTimeLow();
  if (smkOn && feStTrack >= 0 && cur == feStTrack && (now - feStT0) * 1e-6f < audioTrackSeconds(feStTrack) - 0.05f) {
    char s[112]; snprintf(s, sizeof s, "FESTCUT cut=%s at=%.2f len=%.2f by=%s t=%.1f\n", audioTrackName(feStTrack), (now - feStT0) * 1e-6f,
                          audioTrackSeconds(feStTrack), ev, smkT); smkWrite(s); }
  if (id >= 0) { audioMusic(id, fade); feStTrack = id; feStT0 = now; snprintf(feStName, sizeof feStName, "%s", audioTrackName(id)); feStLen = audioTrackSeconds(id); }
  if (smkOn) { char s[160]; snprintf(s, sizeof s, "FESTING ev=%s n=%d track=%s from=%s fade=%.2f wait_ms=%.0f mode=%d t=%.1f\n", ev, n,
                                      id >= 0 ? audioTrackName(id) : "-", cur >= 0 ? audioTrackName(cur) : "-", fade, waitMs, feMusicMode, smkT); smkWrite(s); }
}
static void feStinger(const char *ev) { feStingerFade(ev, 0.3f, 0.f); }
static void feStingerPoll(void) {   // отложенный стингер: пора — запускаем (из кадров разворота газеты)
  if (!fePend || (int)(sceKernelGetSystemTimeLow() - fePendAt) < 0) return;
  fePend = 0; feStingerFade(fePendEv, FE_THEME_FADE_S, (sceKernelGetSystemTimeLow() - fePendT0) * 1e-3f);
}
static float feNewsMusicLeft(void) {   // №665: сколько ещё играет последний стингер (газеты) на музыкальном потоке
  if (feStTrack < 0 || audioStreamTrack(AUDIO_ST_MUSIC) != feStTrack) return 0.f;
  float left = audioTrackSeconds(feStTrack) - (sceKernelGetSystemTimeLow() - feStT0) * 1e-6f; return left > 0.f ? left : 0.f;
}
// №789 (Роб 18.10: «с пятого на шестой — нет звука загрузки на газете»): стингер Newspaper_Spin стартует у LEVELUP на звуке старого
// уровня, а загрузка через ~1,5 с (шаг ui+sfx, levelUiSnd) закрывает звук старого уровня и открывает звук нового — стингер обрывался,
// газета дальше крутилась в тишине (журнал TEST34 на PSP: LOADMUS cur=newspaper_spin → cur=- после ui+sfx на всех 9 переходах).
// Теперь: звук нового уровня открыт, стингер газеты не доиграл — та же дорожка (по имени, в пакете нового уровня) продолжается с того
// же места. NEWS_RESUME_OLD=1 — прежнее (обрыв)
#ifndef NEWS_RESUME_OLD
#define NEWS_RESUME_OLD 0
#endif
static void feNewsResume(void) {
  if (NEWS_RESUME_OLD || loadSilent || strncmp(feStName, "newspaper_spin", 14)) return;
  float at = (sceKernelGetSystemTimeLow() - feStT0) * 1e-6f; if (at < 0.f || at > feStLen - .25f) return;
  int id = audioTrack(feStName); if (id < 0) return;
  audioMusicFrom(id, 0.f, at); feStTrack = id;   // feStT0 прежнее: «сколько осталось» (№665) считается от настоящего начала
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "NEWSRESUME track=%s at=%.2f len=%.2f t=%.1f\n", feStName, at, feStLen, smkT); smkWrite(s); }
}
// газета влетает: если на музыкальном потоке тема меню — стингер с ближайшей доли (как в данных), иначе сразу
static void feNewsStinger(const char *ev) {
  if (loadSilent) { audioMusic(-1, 0.2f); feStTrack = -1; if (smkOn) { char s[80]; snprintf(s, sizeof s, "FESTING ev=- n=0 track=- loadsilent=1 t=%.1f\n", smkT); smkWrite(s); } return; }   // №665: тишина на газете, как у PS2
  if (feMusicMode == 1) { if (smkOn) { char s[96]; snprintf(s, sizeof s, "FESTING ev=- n=0 track=- from=simpsons_theme mode=1 t=%.1f\n", smkT); smkWrite(s); } return; }
  int theme = audioTrack("simpsons_theme");
  if (feMusicMode == 2 || theme < 0 || audioStreamTrack(AUDIO_ST_MUSIC) != theme) { feStinger(ev); return; }
  AudioStats as; audioStats(&as);
  float beat = 60.f / FE_THEME_BPM, ph = as.trackPos[AUDIO_ST_MUSIC] / beat;
  float wait = (1.f - (ph - (float)(int)ph)) * beat; if (wait >= beat - 0.01f) wait = 0.f;   // уже на доле — сразу
  snprintf(fePendEv, sizeof fePendEv, "%s", ev); fePend = 1;
  fePendT0 = sceKernelGetSystemTimeLow(); fePendAt = fePendT0 + (unsigned)(wait * 1e6f);
  feStingerPoll();
}
static void menuBudget(unsigned t0,unsigned io0,const char *kind) {
#ifndef MENU_PERF_OLD
  if(smkOn){unsigned end=sceKernelGetSystemTimeLow(),io=smkIoUs-io0;
    char ln[144];snprintf(ln,sizeof ln,"MENUPERF sc=%s kind=%s work_us=%u io_us=%u fly=%d\n",smkScene,kind,end-t0,io,feFlying());smkWrite(ln);}
#endif
}
static void menuFrame(int sel, void **fbp) {   // главное меню Scrooby над 3D-гостиной (нет fe.shr — градиент), без служебного счётчика
  unsigned budgetT0=sceKernelGetSystemTimeLow(),budgetIo0=smkIoUs;
  sceGuStart(GU_DIRECT, list); resetState();
  sceGuScissor(0, 0, SCR_W, SCR_H); sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  feDraw(sel); resetState(); uiMenuBackdrop(!feReady());
  // правка Роба 49: пока камера летит к предмету и обратно (после NEW GAME — к газете) рамка-телевизор TVFrame.pag остаётся,
  // как в оригинале; скрыты только строка меню, подсказка и название уровня. «flag flynoframe» — прежний код (без рамки)
  uiMenuFlight(feFlying());
  if (!(flyNoFrame && feFlying())) uiDrawMenu(UI_MENU_MAIN, sel);
  if (smkOn && !feFlying() && uiMenuItemBox[0] >= 0.f) {   // правка 412: пульс выбранного пункта по кадрам (tests/menu/item_pulse.sh)
    char s_[160]; snprintf(s_, sizeof s_, "MENUITEM t=%.3f sel=%d ms=%.1f s=%.4f x0=%.2f y0=%.2f x1=%.2f y1=%.2f\n", smkT, sel, uiMenuItemBox[0], uiMenuItemBox[1],
                           uiMenuItemBox[2], uiMenuItemBox[3], uiMenuItemBox[4], uiMenuItemBox[5]); smkWrite(s_); }
  if (svcLabel && !feFlying()) { char t[48]; snprintf(t, sizeof t, "SERVICE  L/R  LEVEL %d", menuLevel); resetState(); uiText(0, 8, 8, 0xff40ffffu, t); }
  resetState(); irisOver();   // правка 154: меню после титульного/демо раскрывается из ириса
  sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
  feCapGrab((void *)((unsigned)sceGeEdramGetAddr() + (unsigned)*fbp), !fb16);   // правка 293 (служебное fecap): готовый кадр перелёта -> ОЗУ
  sceGuFinish(); sceGuSync(0, 0);
  menuBudget(budgetT0,budgetIo0,"main");
  sceDisplayWaitVblankStart(); *fbp = swapBuf();
}
// город вокруг спавна до первого кадра («весь видимый круг, без дыр на старте»); радиус как раньше на заставке —
// дальний режим + 10 м. bar: экран загрузки со шкалой по числу клеток. Возвращает число прочитанных клеток
// Загрузка 2, п.5: задание «круг клеток у точки c» загрузчику — главный поток в это время грузит остальное; cellsRingAt дождётся и дочитает остаток
static float ringC[3];   // точка, вокруг которой загрузчик читает круг
static void cellRingStart(const float *c) {
  if (CELLRING_BG_OLD || CELLRING_OLD || !loaderUp || ringState) return;
  const float r = DIST_LONG + 10.f; int n = 0;
  // высота точки старта в этот момент ещё не настоящая (L6: spawn y = -53 до carInit): отбор по расстоянию на плоскости — надмножество круга по 3D, лишнее потом уйдёт в stream()
  for (unsigned i = 0; i < ncell && n < RING_MAX; i++) { const Cell *q = &cl[i]; if (q->owner || q->state) continue;
    float dx = q->cx - c[0], dz = q->cz - c[2]; if (sqrtf(dx * dx + dz * dz) - q->rad < r) ringIdx[n++] = (short)i; }
  if (!n) return;
  ringC[0] = c[0]; ringC[1] = c[1]; ringC[2] = c[2]; ringN = n; ringDone = 0; __asm__ volatile("" ::: "memory"); ringState = 1; ldSignal();
}
static int cellsRingAt(const float *c, int bar, void **fbp) {
  const float r = DIST_LONG + 10.f; int need = 0, done = 0, shot = loadShot; loadShot = 0; unsigned rt0 = sceKernelGetSystemTimeLow(), bgN = 0, bgReads = 0, bgKb = 0; int relFreed = 0;
  if (bar) { if (!ldOn) loadBegin(fbp); loadFrame(ldBase, fbp); }
  if (ringState) {   // п.5: круг уже читает загрузчик — ждём (кадры газеты по готовым клеткам)
    bgN = (unsigned)ringN;
    { LdWait lw_ = {0}; while (ringState == 1) { if (loaderWaitStep(&lw_, "ring", ringDone, 1)) break; if (bar) loadFrame(ldBase + ringDone * (1000 - ldBase) / (ringN + 1), fbp); else sceKernelDelayThread(1000); } }   // №LOADWAIT: после 3 повторов круг дочитывает главный
    ringState = 0; bgReads = ringReads; bgKb = ringBytes >> 10;
    // игрок стартует не там, где машина (L6: пешком в 409 м от неё): клетки вокруг машины не нужны — вернуть память сразу, не ждать stream()
    { float dx = c[0] - ringC[0], dz = c[2] - ringC[2]; int freed = 0;
      if (dx * dx + dz * dz > 60.f * 60.f) for (int k = 0; k < ringN; k++) { Cell *q = &cl[ringIdx[k]]; if (q->state == 2 && !q->owner && cellDist(q, c[0], c[1], c[2]) >= r) { unloadCell(q); freed++; } }
      relFreed = freed;
      if (smkOn && freed) { char s2[96]; snprintf(s2, sizeof s2, "CELLRING released=%d far=%.0f\n", freed, sqrtf(dx * dx + dz * dz)); smkWrite(s2); } }
  }
  for (unsigned i = 0; i < ncell; i++) if (!cl[i].owner && !cl[i].state && cellDist(&cl[i], c[0], c[1], c[2]) < r) need++;
  if (CELLRING_OLD) {
    SceUID fd = sceIoOpen(levelShr, PSP_O_RDONLY, 0);
    for (unsigned i = 0; i < ncell; i++) {
      if (cl[i].owner || cl[i].state || cellDist(&cl[i], c[0], c[1], c[2]) >= r) continue;
      loadCell(fd, i); done++;
      if (!bar) continue;
      int pm = ldBase + done * (1000 - ldBase) / need;
      loadFrame(pm, fbp);
      if (shot && done * 2 >= need) { shot = 0;
        while ((sceKernelGetSystemTimeLow() - scrT0) * 1e-6f < loadShotAt) scrFrame(UI_LOAD_LEVEL, ldProgress(pm), 1, fbp);
        scrShot("loading", "load", UI_LOAD_LEVEL, "-"); }
    }
    sceIoClose(fd);
    return done;
  }
  // п.4: остаток (всё, если загрузчик не брал) — укрупнёнными чтениями, порциями по 24 клетки (между порциями кадр газеты)
  short *idx = need ? malloc((size_t)need * sizeof *idx) : 0; int n = 0;
  for (unsigned i = 0; idx && i < ncell; i++) if (!cl[i].owner && !cl[i].state && cellDist(&cl[i], c[0], c[1], c[2]) < r && n < need) idx[n++] = (short)i;
  if (n) { SceUID fd = sceIoOpen(levelShr, PSP_O_RDONLY, 0); unsigned reads = 0, bytes = 0;
    for (int a = 0; a < n; a += 24) { int m = n - a < 24 ? n - a : 24;
      cellsReadRuns(fd, idx + a, m, NULL); reads += ringReads; bytes += ringBytes; done += m;
      if (!bar) continue;
      int pm = ldBase + done * (1000 - ldBase) / need;
      loadFrame(pm, fbp);
      if (shot && done * 2 >= need) { shot = 0;
        while ((sceKernelGetSystemTimeLow() - scrT0) * 1e-6f < loadShotAt) scrFrame(UI_LOAD_LEVEL, ldProgress(pm), 1, fbp);
        scrShot("loading", "load", UI_LOAD_LEVEL, "-"); } }
    sceIoClose(fd);
    if (smkOn) { char s[112]; snprintf(s, sizeof s, "CELLRING cells=%d reads=%u kb=%u bg=%u bgreads=%u bgkb=%u wait_ms=%u\n", done, reads, bytes >> 10, bgN, bgReads, bgKb, (sceKernelGetSystemTimeLow() - rt0) / 1000u); smkWrite(s); }
  } else if (smkOn) { char s[112]; snprintf(s, sizeof s, "CELLRING cells=0 reads=0 kb=0 bg=%u bgreads=%u bgkb=%u wait_ms=%u\n", bgN, bgReads, bgKb, (sceKernelGetSystemTimeLow() - rt0) / 1000u); smkWrite(s); }
  free(idx);
  return done + (int)bgN - relFreed;
}
static int cellsRing(int bar, void **fbp) { return cellsRingAt(spawnP, bar, fbp); }
// пункт 8: сценарий миссии перенёс игрока далеко (старт миссии, MISSION SELECT, повтор с чекпоинта) — город вокруг новой
// точки читается сразу, на экране загрузки (как в оригинале), а не дырами в кадре. Загрузчик сперва останавливается
static void msnTeleport(float x, float y, float z, void **fbp) {
  lqn = 0; __asm__ volatile("" ::: "memory");
  { LdWait lw_ = {0}; while (loaderBusy) { loaderWaitStep(&lw_, "busy", 0, 0); sceKernelDelayThread(1000); } }
  scrOpen1065("teleport"); scrTvOver = 0;   // экран загрузки оригинала (Loading.scr) — на время чтения (№1065: отказ -> место и повтор)
  float c[3] = {x, y, z}; int n = cellsRingAt(c, 1, fbp); stInit = 0;
  ldWarmC[0] = x; ldWarmC[1] = y; ldWarmC[2] = z; ldWarmOn = 1;
  loadEnd(fbp); SCR_CLOSE();
  char s[96]; snprintf(s, sizeof s, "MSNLOAD cells=%d at %.1f %.1f\n", n, x, z); smkWrite(s);
}
static void cellsDropAll(void) {   // выход в меню: очередь загрузчика пуста, он простаивает, клетки города выгружены
  lqn = 0; __asm__ volatile("" ::: "memory");
  { LdWait lw_ = {0}; while (loaderBusy) { loaderWaitStep(&lw_, "busy", 0, 0); sceKernelDelayThread(1000); } }
  for (unsigned i = 0; i < ncell; i++) if (!cl[i].owner) unloadCell(&cl[i]);
  stInit = 0;
}
// NEW GAME: всё как после загрузки уровня — машина целая на спавне, пропы/стейтпропы/монеты на местах, Гомер за рулём,
// шкала H&R пуста, музыка свободной езды с начала. Модули заново из тех же секций файла (память: free + та же аллокация)
static SaveGame svPending; static int svResumeOn;   // сохранения: запись, с которой начнётся следующая игра (LOAD / RESUME GAME)
static int devCollOld;   // служебное «flag devcollold 1»: красный контроль правки 350 — DEV SAVE без собранных карточек, ос, гэгов
static int msnOldRoom;   // служебное «flag msnoldroom 1»: красный контроль правки 259 — старт миссий, не выходя из комнаты (как до 03.10)
static int viewBusyNow(void) { return phoneViewOn() || shopViewOn() || wardrobeOn(); }
static void msnStart(void) {   // пункт 8: старт уровня миссий + куча до/после (smoke.txt)
  float h0 = smokeHeapFree();
  mvViewBusy = viewBusyNow;   // №632
  mvCarPrefetch = mcrReadahead;   // №629: чтение машин этапа впрок (и из сценария смоука: flag msn)
  // Правка 259 (03.10, тур на PSP вставал 5 раз из 5): старт миссий ставит игрока на точку фазы в городе, а комната оставалась
  // открытой — Гомер в координатах города при комнате (падал сквозь пол), кадр рисовался «в комнате», и PSP вставала целиком.
  // NEW GAME выводил из комнаты сам (newGame), служебный старт — нет. Правило: старт миссий всегда из города; если точка фазы
  // в комнате (L4, L7), вход — msnRoomSync у вызывающего (levelLoadAfter, служебный «flag msn»)
  if (!msnOldRoom && (rs.room >= 0 || rg.mem)) {
    if (smkOn) { char s[96]; snprintf(s, sizeof s, "ROOM ev=msnstart room=%s t=%.1f\n", roomName() ? roomName() : "-", smkT); smkWrite(s); }
    roomLeave(); }
  MsnProgress rp; svMsnProgress(&svPending, &rp);
  msDevSession = svResumeOn ? svmIsDev(&svPending) : 0;   // правка 348: DEV SAVE (и перенос MISSION SELECT с меткой DEV) — все уровни
  if (svResumeOn && svmIsDev(&svPending)) { rp.level = (uint8_t)gameLevel; rp.story = GAME_MSN_STORY_DEV; }   // правка 335: DEV SAVE —
                                                   // сюжетные уровня пройдены, кроме последней (game_msn.c gameMsnDevMask)
  int started=mvNewGameAt(&G, svResumeOn ? &rp : NULL);
  ldStep("msn-newgame");   // №517: журнал LOADSTEP различает старт миссий и прогрев полиции
  if(!started)mcrWarmPolice();
  ldStep("msn-police");
  if (started == 0 && uiReady) { uiMenuEnable(UI_MENU_PAUSE, UI_P_MISSION_SELECT, 1);
    uiMenuEnable(UI_MENU_PAUSE, UI_P_LEVEL_PROGRESS, uiReady);   // LevelStats.scr (экран в ui.shr) — 29.09
    { char fn[256]; SceIoStat st; pausePackFile(fn, "pextra.shr");
#ifndef C3_LEVEL_MENUS_OLD
      uiMenuEnable(UI_MENU_PAUSE, UI_P_VIEW_CARDS, haveColl && sceIoGetstat(fn, &st) >= 0);
#else
      uiMenuEnable(UI_MENU_PAUSE, UI_P_VIEW_CARDS, haveColl && gameLevel == 1 && sceIoGetstat(fn, &st) >= 0);
#endif
    }
  }
  char s[96]; snprintf(s, sizeof s, "MSNMEM before=%.2f after=%.2f\n", h0, smokeHeapFree()); smkWrite(s);
}
// Правка Роба 40: «наезды» — камеры старта этапов миссий с диска (SetMissionStartCameraName / SetAnimatedCameraName, анимация
// CAM_ в p3d фазы). Пакет msncam.shr рядом с EBOOT (50 КБ, в памяти всё время игры). Когда играть — крючок миссий
// (mvStartCam -> viewer/game_msn.h gameMsnStartCam): фаза с камерой, и сценарий на её старте поставил игрока — старт уровня
// (m0sd: облёт улицы к дому Симпсонов, как у PS2 0:58–1:03), MISSION SELECT, загрузка сохранения, старт гонки. Игрок стоит,
// часы миссии стоят (отсчёт гонки — после камеры); X / START — пропустить (правило порта: в записи PS2 облёт не пропускали).
// В служебном запуске (смоук) камеры старта сами не играют — только с «flag startcam» (или «flag flyin N»)
static MsnCam mcam[24]; static int nmcam; static unsigned char *mcamBuf; static const MsnCam *mcPlay; static float mcT, mcFovY;
static int startCamSvc, mcPlant;   // служебное: камеры старта в смоуке; отрицательный контроль гейта (1 — задом наперёд, 2 — сдвиг 20 м)
// Правка 40: камера разговора (viewer/convcam.h через mvTalkCam) — в городе, пока идёт разговор; в комнате — её камеры (запись
// PS2: у Апу, в школе, в гостиной разговор снимает камера комнаты). План за стеной не берётся (луч от лица говорящего до глаза)
static int talkCamOn; static float talkFovY, talkNear = 1e9f, talkLogT = -1.f; static int talkLogLine = -9;
static int zoneCamOn; static float zoneNear = 1e9f;   // правка 40: камера зоны города (zonecamLoad ниже)
// Правка 40: экран разговора и облёта (Scrooby LetterBox.scr, viewer/ui.h uiDrawLetterbox): полосы выезжают за LB_SLIDE с
// (правило порта: в записи PS2 полосы въезжают за 1–2 кадра листа 2 к/с), HUD на это время скрыт (запись PS2: ни радара,
// ни монет — 0:58, 1:25, 3:15)
#define LB_SLIDE .25f
static float lbOpen; static int lbPrompt, lbLogPrompt = -1;
static int radarLog; static float radarLogT;   // служебное (flag radarlog): итог кадра радара в smoke.txt
#define TUT_CALM 1.0f
static float tutCalm;     // секунды свободной игры подряд (правка 60); 0 — на старте уровня и при включении подсказок
static int tutCalmOff;   // отрицательный контроль правки 60 (flag tutnow): подсказка сразу, как раньше
static int jackOff;   // отрицательный контроль правки 63 (flag jackoff): угона нет, как раньше
// Правки Роба 64/72 (запись PS2, голос Роба: «надпись HIT AND RUN, музыка погони, за тобой несутся полицейские»): шкала
// заполнилась — надпись «Hit & Run!» (поле HNR_TEXT Hud.pag), событие композиции Hit_and_Run (музыка погони), полицейская
// машина уровня (CreateChaseManager, SetNumChaseCars) едет за игроком. Шкала спала до нуля (SetHitAndRunDecay) — ушёл, полиция
// уходит. Полиция рядом (< 5 м) и игрок почти стоит 1 с — «BUSTED!», Hit_and_Run_Caught, шкала в ноль. Штрафа в данных нет
// Правка Роба 62 (подсказка оригинала «VEHICLE DESTROYED! FIND A WRENCH, OR GO TO A PHONEBOOTH TO REPAIR YOUR VEHICLE»): 22 будки
// уровня 1 — ActionEvent SummonVehiclePhone (JMP1). Пешком в сфере будки — △: своя машина целой встаёт рядом (справа по камере).
// Выбор машины из списка (PhoneBooth.scr) — следующий шаг
static int phoneNear(void) {
  if (!haveHomer || !haveCol || !onFoot() || inRoom()) return -1;
  for (unsigned k = 0; k < world.nphone; k++) { const ColWrench *P = &world.phone[k]; float dx = homer.x - P->p[0], dz = homer.z - P->p[2], r = P->r + 1.f;
    if (dx * dx + dz * dz < r * r) return (int)k; }
  return -1;
}
// Правка Роба 117 (30.09: «машина вызывается вдоль проезжей части… поперёк приезжает… чтобы стояла на парковочном месте»): вызванная
// у будки машина — на ближайшую к будке полосу дорог трафика, курсом по ходу полосы. Правило порта: у 22 будок уровня 1 в данных нет
// своей точки машины (триггер будки — шар без направления; локаторы RxPCy — места стоящих машин, не будок). Полосы ближе 30 м нет —
// как раньше: сбоку от Гомера, курсом камеры
// правка 166: камера будки (близнец ps2-phone-booth.yaml): после Accept камера сразу показывает новую машину ~1 с и за ~0,6 с
// возвращается к Гомеру у будки, дальше обычная камера пешком.
// Облёт — phonecamera.p3d (msncam.shr, фаза "phone": 70 кадров @30, 2,3 с) в осях будки (близнец 02.10, камера оригинала по кадрам —
// build/twins/o4/v12_booth_cam/walk.tsv): начало — ActionEvent-локатор будки, оси — его матрица, повёрнутая на 90° (x кадра -> −z
// будки, z кадра -> x будки; так машина у будок всех уровней в осях кадра на одном месте, ≈(0; −6…−10) м, а у Z1p1 глаз выходит
// ровно в (232,89; 6,16; −154,66) оригинала). После облёта камера пешком продолжает с той же стороны (у оригинала — с того же
// места), переход PHONE_ANIM_OUT. Нет анимации или осей будки (прежние файлы) — взгляд камеры пешком на машину: уходит за 0,25 с,
// держит до PHONE_CAM_HOLD, за PHONE_CAM_BACK возвращается
#define PHONE_CAM_HOLD 1.f
// правка 167 (близнец 02.10, камера PS2 по кадрам: build/twins/o4/v23_exit_cam/walk.tsv): выход — глаз за ~0,4 с подходит
// с 8 м до 6,45 м и за ~0,8 с опускается с 5,1 м до 2,4 м над Гомером, к ~1,2 с облетает ему за спину (кадры 110–170);
// посадка — камера машины уходит от позы пешком к своей медленно, без рывка: высота и дальность догоняют с постоянной ~3,8 с
// (кадры 460–720). Порт: выход — плавно за CAM_OUT_S; посадка — к камере машины по экспоненте CAM_IN_TAU в осях машины
// (машина может сразу уехать — начало перехода едет с ней, камера не растягивается)
#define CAM_OUT_S .9f
#define CAM_IN_TAU 1.5f
#define PHONE_CAM_BACK .6f
#define PHONE_ANIM_OUT .35f
#define PHONE_ANIM_WAIT .27f   // оригинал: первый кадр облёта стоит ~0,27 с (экран будки гаснет), потом облёт в темпе 30 к/с
static float phoneCamT = -1.f, phoneCamCar[3];
static const MsnCam *phoneAnim; static int phoneAnimK = -1, phoneCamOn, phonePlant; static float phoneAnimT = -1.f, phoneFovY;
static float camBT = -1.f, camBDur, camBE[3], camBL[3];   // правки 166/167: плавный переход камеры от позы (глаз, точка взгляда)
static int benchLog1034;   // №1034: flag benchlog
static int camBRel, camLogOn;   // 1 — поза начала в осях машины игрока, переход по экспоненте (camBDur — постоянная времени)
static int camIrisCut;   // правка 333: посадка ирисом (car_in) — ирис раскрывается сразу на камере машины, без перехода 167 (запись PS2 869,8 с); выход — с переходом, как у записи 1085,8–1086,1 с
static float camBFov, camBU; static int camBFovOn, footFovOld;   // угол кадра в начале перехода (0 — без смены угла), доля перехода этого кадра
static void camBlendFrom(const float e[3], const float l[3], float dur) { memcpy(camBE, e, 12); memcpy(camBL, l, 12); camBT = 0.f; camBDur = dur; camBRel = 0; camBFov = curFovY; }
static void camCarAxes(const float p[3], float o[3], int toLocal) {   // мир <-> оси машины игрока (поворот по курсу car.h)
  float a = toLocal ? -car.h : car.h, c = cosf(a), sn = sinf(a), x = p[0], y = p[1], z = p[2];
  if (toLocal) { x -= car.x; y -= car.y; z -= car.z; }
  o[0] = x * c + z * sn; o[1] = y; o[2] = -x * sn + z * c;
  if (!toLocal) { o[0] += car.x; o[1] += car.y; o[2] += car.z; }
}
static void phoneCamStart(int k, float x, float y, float z) {
  if (phoneAnim && k >= 0 && k < (int)world.nphonex) { phoneAnimK = k; phoneAnimT = 0.f; phoneCamT = -1.f; return; }
  phoneCamT = 0.f; phoneCamCar[0] = x; phoneCamCar[1] = y + .8f; phoneCamCar[2] = z;
}
static int phoneAnimPose(float t, float eye[3], float look[3], float *fovY) {   // кадр облёта будки -> мир (оси — см. выше)
  const ColPhoneX *b = &world.phonex[phoneAnimK]; float e[3], l[3], u[3];
  int going = msncamPose(phoneAnim, t, e, l, u, fovY);
  float d[3] = {l[0] - e[0], l[1] - e[1], l[2] - e[2]};
  for (int k = 0; k < 3; k++) {
    if (phonePlant) {   // отрицательный контроль: оси самой матрицы локатора (так облёт смотрит мимо машины — гейт обязан упасть)
      eye[k] = b->loc[k] + e[0] * b->ax[k] + e[1] * b->ay[k] - e[2] * b->az[k];
      look[k] = eye[k] + d[0] * b->ax[k] + d[1] * b->ay[k] - d[2] * b->az[k]; continue;
    }
    eye[k] = b->loc[k] - e[0] * b->az[k] + e[1] * b->ay[k] - e[2] * b->ax[k];
    look[k] = eye[k] - d[0] * b->az[k] + d[1] * b->ay[k] - d[2] * b->ax[k];
  }
  return going;
}
static void phoneCarSpot(int k, float *x, float *y, float *z, float *h) {
  float a = homer.camYaw, p[3], hh; *x = homer.x - cosf(a) * 3.2f; *z = homer.z + sinf(a) * 3.2f; *y = homer.y + 1.f; *h = a;
  // правка 166: место машины — CarStart-локатор того же имени, что будка (данные диска; близнец: оригинал ставит машину у бордюра
  // в 6,3 м от будки вдоль улицы, носом по улице — ps2-phone-booth.yaml). Нет блока будки+ — прежнее правило 117 (полоса у будки)
  if (k >= 0 && k < (int)world.nphonex && (world.phonex[k].flags & 1u)) {
    const ColPhoneX *e = &world.phonex[k]; *x = e->car[0]; *y = e->car[1] + 1.f; *z = e->car[2]; *h = e->carH;
    float g; if (ground(*x, *z, *y + 5.f, 0.f, &g)) *y = g;
    return;
  }
  if (haveTraffic && k >= 0 && k < (int)world.nphone && trafficLaneSpot(&traffic, world.phone[k].p[0], world.phone[k].p[2], 30.f, p, &hh)) {
    *x = p[0]; *y = p[1] + 1.f; *z = p[2]; *h = hh; }
  float g; if (ground(*x, *z, *y + 5.f, 0.f, &g)) *y = g;
}
static void phoneSummon(int k) {
  if (!mvSummon()) return;
  float x, y, z, a; phoneCarSpot(k, &x, &y, &z, &a);
  carPlace(&car, &world, x, y, z, a); car.damage = 0.f; car.destroyed = 0; wasDestroyed = 0; chase.init = 0; oldCarOn = 0;
  phoneCamStart(k, car.x, car.y, car.z);   // правка 166
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "PHONE summon %s at=%.1f,%.1f t=%.1f\n", world.phone[k].name, x, z, smkT); smkWrite(s); }
}
// правка Роба 107: костюм Гомера — модель костюма (скелет и клипы Гомера, tools/homer_export.py --skin) вместо homer.shr, на том же
// месте. Файлы поставки (tools/ship_files.sh ищет имена в исходниках): "homer-h_undr_m.shr", "homer-h_fat_m.shr", "homer-h_stcr_m.shr"
static uint32_t ownSeen; static int ownInit;   // купленное/полученное прошлого кадра (подсказка «UNLOCKED CAR»); 0 — заново после загрузки
static int skinWorn;
#define WARD_ICON_H .9f    // м над полом места костюмов: значок shirtdollar (правка 118)
#if defined(PC_OLD) && PC_OLD
#define PC_OLD_DEF 1
#else
#define PC_OLD_DEF 0
#endif
static char pcPath[256];   // правка 352: файл модели игрока уровня, который сейчас в homerM (playerLoad)
static int pcOld;          // правка 352: 1 — прежнее, на всех уровнях Гомер (flag pcold сценария, файл pcold рядом с EBOOT, -DPC_OLD=1)
static void homerWear(int k) {
  const char *f = wardrobeFile(k); if (!f && k == 0) f = "homer.shr";   // уровень без skins.txt (2–7): назад в обычного Гомера
  if (!f || !haveHomer) return;
  char fn[256]; sibling(fn, f);
#if !defined(PC_OLD) || !PC_OLD
  // правка 352: костюмы уровня — в его папке (l<N>/skins.txt, l<N>/skin<K>.shr; у L1 — рядом с EBOOT, как было); без skins.txt —
  // обычный костюм = модель игрока уровня (pcPath: homer.shr или l<N>/player.shr)
  if (!pcOld) { if (wardrobeFile(k)) levelFile(fn, f); else if (pcPath[0]) snprintf(fn, sizeof fn, "%s", pcPath); }
#endif
  SkinModel *m = skinLoad(fn); if (!m) return;
  float x = homer.x, y = homer.y, z = homer.z, h = homer.h;
  SkinModel *old = homerM; homerM = m;
  if (homerInit(&homer, homerM) != 0) { homerM = old; skinFree(m); homerInit(&homer, homerM); }
  else { skinFree(old); skinWorn = k;
         hnrHeat = 0.f; hnrCalm = G.hnrLatched ? 3.f : 0.f; }   // правка 121: «Hint : Switching clothes lowers the meter» (MISSION_FAILED_HIT_N_RUN_HINT1) — как смена машины
  homer.solid = homerSolidAll; homer.solidUser = 0;
  homerPlace(&homer, G.pHomerWorld ? G.pHomerWorld : &world, x, y, z, h);
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "WARDROBE wear %d %s t=%.1f\n", k, f, smkT); smkWrite(s); }
}
// правка 94: машина из галереи будки — своя (машина уровня) или награда уровня из пакета msncars (gameMsnPhoneCar); стоит рядом
static int phoneK;
static void phoneSummonCar(int k, const char *name) {
  bbOp("phone", name, 0);   // правка 103: самописец — машина из будки
  mcrPrefetch(name);        // правка 474: модель с карты памяти сразу (экран будки ещё гаснет), прежняя вызванная выгрузится в mcrSync
  if (!mvPhoneCar(name)) { if (smkOn) { char s_[96]; snprintf(s_, sizeof s_, "PHONECAR nomodel %.15s t=%.1f\n", name, smkT); smkWrite(s_); } phoneSummon(k); return; }   // машина уровня / нет модели — прежний вызов своей
  float x, y, z, a; phoneCarSpot(k, &x, &y, &z, &a);   // правка 117: на полосу у будки, вдоль дороги
  carPlace(&car, &world, x, y, z, a); car.damage = 0.f; car.destroyed = 0; wasDestroyed = 0; chase.init = 0; oldCarOn = 0;
  phoneCamStart(k, car.x, car.y, car.z);   // правка 166
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "PHONE car %s at=%.1f,%.1f t=%.1f\n", name, x, z, smkT); smkWrite(s); }
}
// Правка Роба 103 (30.09: «у школы не прогрузился кусок карты и выбило приставку») + аудит ведущего 30.09 (С.5): чёрный ящик.
// Кольцо BB_N строк в памяти: раз в BB_PERIOD с и на событии (отказ памяти любого модуля, клетка без памяти, ошибка чтения, пропуск
// кусков из-за полного списка GE) — время, код места, позиция, куча (свободно / наибольший кусок), клетки в памяти и без памяти,
// пик списка GE за секунду, экраны. На карту (blackbox.txt рядом с EBOOT, перезапись) — первой секундой игры (RUN), на событии
// (не чаще раза в BB_MINW с) и при выходе (EXIT). Прежняя запись раз в 5 с шла из главного потока: рывок кадра и спор с загрузчиком
// клеток за карту. При старте прошлый файл без EXIT переименовывается в blackbox_prev.txt — после вылета видно, что было перед ним
// Повтор 01.10 ~09:40 (7dec8b30, «приехал на спортивную площадку у школы — завис, приставка выключилась»): БОРТОВОЙ САМОПИСЕЦ.
// Ящик писал на карту раз в минуту — после вылета в файле состояние до минуты назад, приезда на площадку в нём нет. Теперь (все
// уровни): образец раз в секунду (кольцо BB_N строк ≈ 40 с), на карту — не реже раза в BB_EVERY с и сразу на событии, фоновым
// потоком записи «bbw» (ниже главного по приоритету: кадр карту не ждёт; туда же журнал звуков sfxlog — раньше раз в секунду из
// кадра). Файл фиксированной длины без усечения: вылет посреди записи не оставит пустой файл. Перед каждым рискованным действием
// главного потока (viewer/blackbox.h bbOp: модель персонажа или машины миссии с карты, комната, пропы гонки, запись msnart, этап
// миссии, будка) — строка «op», а перед чтениями с карты — запись синхронно ДО чтения: встанет посреди — последняя строка назовёт
// действие. В каждой строке: этап миссии, моделей персонажей и машин миссии в памяти, КБ записей msnart, последнее действие и его
// возраст (с), кадров с концом списка GE за бюджетом. Разбор — tests/crash_school/check.py, формат — tools/bb_check.py
#define BB_PERIOD 1.f
#define BB_N 40
#ifndef BBICON958_OLD
#define BBICON958_OLD 0   // №958б: 1 — без поля ico в строке ящика и без строк ICON (прежнее)
#endif
#define BB_LINE (BBICON958_OLD ? 448 : 512)   // 04.10: поля io/gg (259) и fpe (252) — строка ящика подошла к 320; + mus (279) — до 430
#define BB_EVERY 2.f    // запись на карту не реже раза в 2 с (фоном): при вылете в файле последние секунды до него
#define BB_FILE (BB_N * BB_LINE + 1024)   // длина файла на карте: шапка (до 1 КБ) + кольцо, хвост — пробелы (без усечения)
static char bbRing[BB_N][BB_LINE]; static unsigned bbHead, bbCnt, bbN; static float bbT, bbClock, bbLastW = -1e9f;   // правка 139: строка 200 -> 240 Б (поля трафика); 103: -> 320 (самописец)
static unsigned bbSeenMem, bbSeenCell, bbSeenRead, bbSeenGe, bbSeenBad, bbSecT0;
static char bbOpS[40] = "-", bbMsn[16] = "-"; static float bbOpAt = -1.f;   // последнее действие (bbOp), этап миссии
static char bbOut[BB_FILE]; static int bbPend;                               // кадр файла для потока записи; запись отложена (поток занят)
static SceUID bbSema = -1; static volatile int bbJob;                        // работа потоку «bbw»: 1 — ящик, 2 — журнал звуков (0 — свободен)
static char sfxOut[3072]; static int sfxOutLen;                              // журнал звуков, отданный потоку записи
static float bbNow(void) { return bbClock + (bbSecT0 ? (unsigned)(sceKernelGetSystemTimeLow() - bbSecT0) * 1e-6f : 0.f); }
static void bbNote(const char *line) {   // правка 238: строка события (сохранение) в чёрный ящик на PSP — видно и без smoke
  char *s = bbRing[bbHead]; bbHead = (bbHead + 1) % BB_N; if (bbCnt < BB_N) bbCnt++;
  snprintf(s, sizeof bbRing[0], "%6.0f NOTE    %s", bbClock, line);
  size_t n = strlen(s); if (n && s[n - 1] != '\n' && n + 1 < sizeof bbRing[0]) { s[n] = '\n'; s[n + 1] = 0; }
}
static char bbIconLog[4][124]; static unsigned bbIconN;   // №958б: последние 4 события значка цели — в хвост файла ящика (кольцо в 40 строк их вытесняет за ~40 с)
void bbNoteIcon(const char *line) {   // №958б: blackbox.h
  if (BBICON958_OLD) return;
  bbNote(line); snprintf(bbIconLog[bbIconN++ & 3], sizeof bbIconLog[0], "%.0f %s\n", bbClock, line);
  if (smkOn) { char t[160]; snprintf(t, sizeof t, "%s t=%.1f\n", line, smkT); smkWrite(t); }
}
static void svLog(const char *line) { smkWrite(line); bbNote(line); }
// Исследование 03.10 (docs/ИССЛЕДОВАНИЕ_ПАМЯТЬ_ME_ПОДГРУЗКА_2026-10-03.md, П1–П3): замеры без риска строкой в ящик, чтобы решать
// про Media Engine и память по числам железа. П1 — доля процессора потоков за секунду (sceKernelReferThreadRunStatus, runClocks);
// П2 — клетки: загружено за секунду, худшая задержка «запрос -> готова», дольше 100 мс, кадров с видимой незагруженной;
// П3 — проба «лишних 4 МБ» (volatile) при старте: взять и сразу отпустить, итог в шапке. BB_PROBES_OFF — без замеров (контроль)
#define BB_NTH 5
static const char *const bbThName[BB_NTH] = {"user_main", "loader", "audio_mix", "audio_io", "bbw"};
static SceUID bbThId[BB_NTH]; static unsigned long long bbThRun[BB_NTH]; static unsigned bbThPct[BB_NTH], bbThT0; static int bbThOk;
#include "bbcpu.h"   // правка 410: доля потока без переполнения после пересоздания потоков звука (bbPctStep)
static unsigned bbThBad;   // правка 410: секунд-событий «поток новый (runClocks меньше базы)» и «доля > 100, срез» (BBCPU bad=)
static void bbThForgetAudio(void) { bbThOk &= ~(1 << 2 | 1 << 3); bbThRun[2] = bbThRun[3] = 0; }   // sndInit: потоки звука — новые, искать по имени
static int bbVolRc = 1, bbVolKB;
static unsigned bbMissPrev, bbMissSec, bbIoR0, bbIoW0;
static void bbProbeVolatile(void) {
#ifndef BB_PROBES_OFF
  void *p = 0; int sz = 0; bbVolRc = sceKernelVolatileMemTryLock(0, &p, &sz);
  if (bbVolRc == 0) { bbVolKB = sz >> 10; sceKernelVolatileMemUnlock(0); }   // только узнать: сразу отпустить (окно сохранения и сон ждут её)
#endif
}
static void bbProbeThreadsSecond(void) {
#ifndef BB_PROBES_OFF
  unsigned now = sceKernelGetSystemTimeLow();
  if (bbThOk != (1 << BB_NTH) - 1) {   // потоки звука и записи появляются позже — ищем, пока не найдутся все
    SceUID ids[64]; int n = 0;
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, 64, &n) >= 0)
      for (int i = 0; i < n && i < 64; i++) {
        SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
        if (sceKernelReferThreadStatus(ids[i], &ti) < 0) continue;
        for (int k = 0; k < BB_NTH; k++) if (!(bbThOk & 1 << k) && !strcmp(ti.name, bbThName[k])) { bbThId[k] = ids[i]; bbThOk |= 1 << k; bbThRun[k] = 0; }
      }
  }
  unsigned dt = now - bbThT0; bbThT0 = now;
  for (int k = 0; k < BB_NTH; k++) {
    if (!(bbThOk & 1 << k)) { bbThPct[k] = 0; continue; }
    SceKernelThreadRunStatus st; memset(&st, 0, sizeof st); st.size = sizeof st;
    if (sceKernelReferThreadRunStatus(bbThId[k], &st) < 0) { bbThOk &= ~(1 << k); bbThPct[k] = 0; continue; }
    unsigned long long run = (unsigned long long)st.runClocks.hi << 32 | st.runClocks.low;
    unsigned ev = 0; bbThPct[k] = bbPctStep(&bbThRun[k], run, dt, &ev); if (ev) bbThBad++;
  }
  bbMissSec = (unsigned)stMissFrames - bbMissPrev; bbMissPrev = (unsigned)stMissFrames;
  if (smkOn) { char t[160]; snprintf(t, sizeof t, "BBCPU ok=%x pct=%u/%u/%u/%u/%u run0=%llu dt=%u bad=%u\n", bbThOk, bbThPct[0], bbThPct[1], bbThPct[2], bbThPct[3], bbThPct[4], bbThRun[0], dt, bbThBad); smkWrite(t); }
#endif
}
// правка 252: поле «fpe» строки ящика: «-» или флаги V (0/0, sqrt(<0), NaN в сравнении), Z (x/0), O (переполнение) и потоки:
// m главный, l загрузчик, x микшер звука, i чтение звука, w запись ящика, d сторож, v ролик, b звук заставки; nanN — машина
// с NaN возвращена на надёжное место N раз (viewer/car.c)
static void fpuTag(char *o, int n) {
  unsigned f = fpuSeenFlg, t = fpuSeenThr; int k = 0;
  if (!f && !carNanFixes) { snprintf(o, n, "-"); return; }
  if (f & 16) o[k++] = 'V'; if (f & 8) o[k++] = 'Z'; if (f & 4) o[k++] = 'O';
  if (t) { o[k++] = ':'; for (int i = 0; i < FPU_T_N && k < n - 6; i++) if (t >> i & 1) o[k++] = "mlxiwdvb"[i]; }
  o[k] = 0; if (carNanFixes) snprintf(o + k, n - k, "%snan%u", k ? "," : "", carNanFixes);
}
// правка 252: служебная проба (flag fpeprobe сценария или файл fpeprobe рядом с EBOOT — один раз на первой секунде игры): 0/0 и
// сравнение с NaN в главном потоке, как редкий NaN в физике. Новое правило: игра идёт, в ящике событие FPE «V:m». С файлом
// fputrap (прежнее, ловушки как у PSP): на PSP — исключение процессора (с sharexc — красный экран exc=15), в эмуляторе-
// проверяльщике (SHAR_HWCHK_TRAP=1) — остановка. Проверка на железе: ловушки FPU у PSP по умолчанию включены и sharexc их ловит
static void fpeProbe(void) {
  volatile float z = 0.f; volatile float r = z / z; int lt = r < 1.f;
  if (smkOn) { char t[96]; snprintf(t, sizeof t, "FPEPROBE t=%.1f lt=%d keep=%d fcr31=%08x\n", smkT, lt, fpuKeep, fpuGet()); smkWrite(t); }
}
// правка 279: музыка на железе — шина × игрок, пауза звука, поток (что играет и наполнение кольца, КБ): «mus 0.78x1.00 p0 hit_run_main 126»
static const char *bbMus(void) {
  static char m[64]; char a[200]; audioMusicState(a, sizeof a);
  float bus = 0, user = 0; int st = 0; unsigned buf = 0; char cur[24] = "-";
  const char *p;
  if ((p = strstr(a, "bus="))) bus = strtof(p + 4, 0);
  if ((p = strstr(a, "user="))) user = strtof(p + 5, 0);
  if ((p = strstr(a, "cur="))) { int i = 0; p += 4; while (*p && *p != ' ' && i < 23) cur[i++] = *p++; cur[i] = 0; }
  if ((p = strstr(a, "state="))) st = atoi(p + 6);
  if ((p = strstr(a, "buf="))) buf = (unsigned)strtoul(p + 4, 0, 10);
  char me[24] = ""; if ((p = strstr(a, " me=")) && p[4] != '-') { int i = 0; p += 4; me[i++] = ' '; me[i++] = 'm'; me[i++] = 'e';   // правка 276: ME жив
    while (*p && *p != ' ' && i < 23) me[i++] = *p++; me[i] = 0; }                                                               // (заданий/с, ожидание мс)
  char d[96]; sndModeDebug(d, sizeof d); int pz = strstr(d, "pause=1") != 0;
  snprintf(m, sizeof m, "mus %.2fx%.2f p%d %s s%d %u%s", bus, user, pz, cur, st, buf >> 10, me);
  return m;
}
static void bbSample(const char *ev) {
  int bad = 0, in = 0; for (unsigned i = 0; i < ncell; i++) { if (cl[i].state == 3) bad++; if (cl[i].state == 2) in++; }
  int foot = onFoot(); float x = foot ? homer.x : car.x, z = foot ? homer.z : car.z, y = foot ? homer.y : car.y; char pc[24]; placeCode(x, z, pc, sizeof pc);
  char *s = bbRing[bbHead]; bbHead = (bbHead + 1) % BB_N; if (bbCnt < BB_N) bbCnt++;
  // правка 139 (Роб 01.10: «слишком много машин — ботов, припаркованных; боюсь, жрут оперативку»): замер на железе — машин трафика
  // в движении, стоящих нарисовано / с физикой, пешеходов (счётчики последнего кадра traffic.c; без traffic.shr — нули)
  int trC = haveTraffic ? traffic.activeCars : 0, trPD = haveTraffic ? traffic.drawnPark : 0, trPA = haveTraffic ? traffic.awakePark : 0,
      trP = haveTraffic ? traffic.activePeds : 0;
  int ncar = 0; for (int k = 0; k < MCR_MAX; k++) ncar += mcr[k].blob != 0;
  char fe[24]; fpuTag(fe, sizeof fe);   // правка 252
  float opAge = bbOpAt >= 0.f ? bbNow() - bbOpAt : -1.f; if (opAge > 999.f) opAge = 999.f;
  char ico[64] = ""; if (!BBICON958_OLD) { char t_[56]; mvIconState(t_, sizeof t_); snprintf(ico, sizeof ico, " ico %s", t_); }   // №958б: значок цели в углу: имя/поле/картинка/уровень цели/текст
  snprintf(s, sizeof bbRing[0], "%6.0f %-7s L%d %-7s %5.0f %4.0f %5.0f  heap %5.2f blk %5.2f  cells %3d bad %d  ge %2u%% skip %u ptr %u  mem %u rd %u  "
           "trf %d park %d/%d ped %d  msn %s%s mdl %d car %d art %u over %u  op %s %.1f  cpu %u/%u/%u/%u/%u  cell %u %ums >100:%u miss %u  io %u/%u %ums gg %u fpe %s  %s  %s%s%s%s%s\n",
           bbClock, ev, gameLevel, pc, x, y, z, heapFreeBytes() / 1048576.f, memMaxBlockMB(), in, bad, gePeakShown * 100u / GE_BUDGET,
           smkGeSkip, smkGeBad, memFailsNow(), smkReadErr, trC, trPD, trPA, trP, bbMsn, ico, mvModels(), ncar, msnartBytes() >> 10, geOverBudget,
           bbOpS, opAge, bbThPct[0], bbThPct[1], bbThPct[2], bbThPct[3], bbThPct[4], bbCellN, bbCellMax, bbCellSlow, bbMissSec,
           msioReads - bbIoR0, msioWaits - bbIoW0, msioMaxWaitUs / 1000u, geGuardHits, fe, bbMus(),
           foot ? "foot " : "", inRoom() ? "room " : "", phoneViewOn() ? "phone " : "", shopViewOn() ? "shop " : "", mvCars() ? "msncars" : "");
  bbCellN = bbCellMax = bbCellSlow = 0; bbIoR0 = msioReads; bbIoW0 = msioWaits; msioMaxWaitUs = 0;   // правки 252/259: чтения карты за секунду, ожиданий очереди, худшее ожидание
  if (smkOn) { char t[BB_LINE + 8]; snprintf(t, sizeof t, "BB %s", s); smkWrite(t); }   // замер-тур: вся история ящика в smoke.txt (кольцо — 40 с)
  if (smkOn && !MEMLOAD1065_OLD) { static unsigned mfLog_; unsigned mfb_, mfra_;   // №1065: размер и вызывающий каждого нового отказа (адрес по ELF: tools/crash_addr.py)
    while (mfLog_ < memFailsNow() && memFailRingGet(mfLog_, &mfb_, &mfra_)) { char t[140]; snprintf(t, sizeof t, "MEMFAILSZ n=%u bytes=%u ra=%08x heap=%.2f blk=%.2f t=%.1f\n", mfLog_ + 1, mfb_, mfra_, heapFreeBytes() / 1048576.f, memMaxBlockMB(), bbClock); smkWrite(t); mfLog_++; }
    if (mfLog_ < memFailsNow()) mfLog_ = memFailsNow(); }
}
static int bbCompose(const char *head) {   // шапка + кольцо -> bbOut (BB_FILE байт, хвост — пробелы и перевод строки)
  char h[1536]; int n = snprintf(h, sizeof h, "%s n=%u t=%.0f EBOOT %s %s kexc %d vol %d %dK fpu %x>%x\n# t(с) событие уровень место x y z · куча МБ: свободно, наибольший кусок · клетки в памяти, "
                                "без памяти · пик списка GE за секунду · пропуски кусков · отказы памяти · ошибки чтения · трафик: машин, стоящих "
                                "нарисовано/проснулось, пешеходов · миссия: этап, моделей персонажей, машин миссии, КБ рисунков · кадров за бюджетом GE · "
                                "последнее действие и его возраст, с · процессор %% потоков main/loader/audio_mix/audio_io/bbw · клетки за секунду, худшая "
                                "задержка запрос->готова, дольше 100 мс, кадров с видимой незагруженной · FPU: 0/0, x/0, переполнение (fpe, viewer/fpuguard.h) · экраны\n", head, ++bbN, bbClock, __DATE__, __TIME__, kexcRc, bbVolRc, bbVolKB, fpuBoot, fpuApplied);
  if (n > (int)sizeof h - 1) n = sizeof h - 1;
  memcpy(bbOut, h, n);
  for (unsigned k = 0; k < bbCnt; k++) {
    const char *s = bbRing[(bbHead + BB_N - bbCnt + k) % BB_N]; int l = (int)strlen(s);
    if (n + l > BB_FILE - 1) break;
    memcpy(bbOut + n, s, l); n += l;
  }
  if (!BBICON958_OLD) for (unsigned k = bbIconN > 4 ? bbIconN - 4 : 0; k < bbIconN; k++) {   // №958б: события значка цели — после кольца, если есть место
    char t[140]; int l = snprintf(t, sizeof t, "# ICON %s", bbIconLog[k & 3]); if (l < 0 || n + l > BB_FILE - 1) break;
    memcpy(bbOut + n, t, l); n += l;
  }
  memset(bbOut + n, ' ', BB_FILE - 1 - n); bbOut[BB_FILE - 1] = '\n';
  return BB_FILE;
}
static void bbFileWrite(void) {   // bbOut -> blackbox.txt: на месте, без усечения (поток «bbw» или синхронно — EXIT, bbOp перед чтением)
  char fn[256]; sibling(fn, "blackbox.txt"); SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT, 0777); if (f < 0) return;
  sceIoWrite(f, bbOut, BB_FILE); sceIoClose(f);
}
static void sfxFileAppend(const char *b, int n) {
  if (n <= 0) return;
  char fn[256]; sibling(fn, "sfxlog.txt"); SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
  if (f >= 0) { sceIoWrite(f, b, n); sceIoClose(f); }
}
static int bbWriterThread(SceSize a, void *b) {   // «bbw»: ниже главного по приоритету — пишет, пока кадр ждёт обратный ход / GE
  (void)a; (void)b; fpuThreadInit();   // правка 252: ловушки FPU потока
  for (;;) {
    sceKernelWaitSema(bbSema, 1, NULL);
    fpuThreadPoll(FPU_T_BBW);
    int j = bbJob;
    if (j & 1) bbFileWrite();
    if (j & 2) sfxFileAppend(sfxOut, sfxOutLen);
    __asm__ volatile("" ::: "memory");
    bbJob = 0;
  }
  return 0;
}
static int bbWaitIdle(unsigned ms) { for (unsigned i = 0; bbJob && i < ms; i++) sceKernelDelayThread(1000); return !bbJob; }
// Сторож «bbdog» (правка 103, повтор 01.10): выше главного потока по приоритету, спит по BB_DOG_STEP. Кадры не показываются
// BB_STALL_MS (главный поток встал: GE не отдаёт список, бесконечный цикл, ожидание, сбой потока) — ящик на карту пишет сам, один
// раз на стоянку: «STALL ms=… phase=<фаза кадра> op=<последнее действие> list=<КБ списка GE сейчас> last=<КБ прошлого кадра>
// heap=<КБ>», затем кольцо. Только целые и строки: printf с плавающей точкой берёт кучу (dtoa), а главный поток мог встать, держа
// её замок. Файл начинается со «STALL» — при следующем старте он, как RUN, уходит в blackbox_prev.txt. Главный поток ожил —
// в кольце строка «op stall <фаза>_<мс>» (ложная тревога на долгой загрузке видна так же — по фазе load)
#define BB_STALL_MS 5000u
#define BB_DOG_STEP 250u
// Правка 194 (01.10 22:4x, зависание после прыжка с крыши заправки F10-5, как 103 у школы): сторож НЕ записал STALL — значит, его
// не пускал поток выше 0x19 (звук 0x12/0x16, загрузчик 0x18 — занятой цикл) или встал ввод-вывод. Теперь сторож выше всех потоков
// игры (BB_DOG_PRI) и на стоянке сначала пишет прямо на экран (кадровый буфер, без GE и без карты памяти): фаза кадра, последнее
// действие, куча и все потоки — имя, состояние (1 работает, 2 готов, 4 ждёт), приоритет, такты работы. Фото экрана — улика
#define BB_DOG_PRI 0x10
#ifndef TRACE_OPEN_OLD
static SceUID trFd=-1;
#endif
static void trLine(const char *ln, int l) {
  unsigned t_=sceKernelGetSystemTimeLow();
#ifdef TRACE_OPEN_OLD
  char fn[256];sibling(fn,"trace.txt");SceUID f=sceIoOpen(fn,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0777);
  if(f>=0){msioWriteRaw(f,ln,(unsigned)l);sceIoClose(f);}
#else
  // №402/410: immediate writes survive stalls; reuse the descriptor like smoke.txt.
  // Opening every phase adds ~10ms in PPSSPP, 16+ times in explosion frames.
  if(trFd<0){char fn[256];sibling(fn,"trace.txt");trFd=sceIoOpen(fn,PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND,0777);}
  if(trFd>=0 && msioWriteRaw(trFd,ln,(unsigned)l)<0){sceIoClose(trFd);trFd=-1;}
#endif
#ifndef SMK_TRACE_IO_OLD
  smkIoUs+=sceKernelGetSystemTimeLow()-t_;
#endif
}
static void trPhase(int n) {
  extern const char *const bbPhName[];
  char ln[96];int l=snprintf(ln,sizeof ln,"TR beat=%u left=%d ph=%s us=%u gg=%u\n",bbBeat,trLeft,n>=0&&n<BBP_N?bbPhName[n]:"?",sceKernelGetSystemTimeLow(),geGuardHits);
  trLine(ln,l);if(n==BBP_VBLANK)trLeft--;
}
// Правки 259/252 (03.10): камера кадра перед pspgum. Кадр с FOV 0 (комната без своей камеры) и взглядом строго вниз давал
// матрицы с бесконечностью и NaN (LookAt: взгляд параллелен «верху»), видеочип PSP вставал вместе с приставкой (эмулятор
// молчит). FOV вне 5–170° — обычный FOVY; глаз не число — прошлый годный; взгляд нулевой — вперёд по Z; взгляд по вертикали —
// «верх» кадра по оси Z. Каждый случай — строка GEGUARD (не чаще раза в секунду) в smoke.txt и в чёрный ящик
static int camGuardOff;   // служебное «flag camguardoff 1»: красный контроль (камера как до правки)
static unsigned camGuardHits; static float cgLastE[3] = {220.f, 5.f, 172.f}, cgLogAt = -1e9f;
static void camGuard(float *fov, float *ex, float *ey, float *ez, float *dx, float *dy, float *dz, ScePspFVector3 *up) {
  int why = 0; float f0 = *fov, d0[3] = {*dx, *dy, *dz};
  if (!geFinite(*fov) || *fov < 5.f || *fov > 170.f) { *fov = FOVY; why |= 1; }
  if (!geFinite(*ex) || !geFinite(*ey) || !geFinite(*ez)) { *ex = cgLastE[0]; *ey = cgLastE[1]; *ez = cgLastE[2]; why |= 2; }
  else { cgLastE[0] = *ex; cgLastE[1] = *ey; cgLastE[2] = *ez; }
  float l2 = *dx * *dx + *dy * *dy + *dz * *dz;
  if (!geFinite(*dx) || !geFinite(*dy) || !geFinite(*dz) || !geFinite(l2) || !(l2 > 1e-10f)) { *dx = 0.f; *dy = 0.f; *dz = 1.f; l2 = 1.f; why |= 4; }
  if (*dx * *dx + *dz * *dz < 1e-6f * l2) { up->x = 0.f; up->y = 0.f; up->z = 1.f; why |= 8; }
  if (!why) return;
  camGuardHits++;
  float now = bbNow(); if (now - cgLogAt < 1.f) return; cgLogAt = now;
  char s[160]; snprintf(s, sizeof s, "GEGUARD cam why=%d fov=%.2f dir=%.2f,%.2f,%.2f hits=%u t=%.1f\n", why, f0, d0[0], d0[1], d0[2], camGuardHits, smkT);
  if (smkOn) smkWrite(s);
  bbNote(s);
}
const char *const bbPhName[BBP_N] = {"-", "input", "car", "world", "rooms", "msn", "traffic", "stream", "city", "car3d", "npc3d",
                                           "traffic3d", "msncar3d", "props3d", "fx3d", "ui", "gesync", "vblank", "second", "load"};
static char bbDogOut[BB_FILE], bbDogHead[120]; static volatile int bbDogOn; static volatile unsigned bbStallMs, bbStallPh, bbStalls;
static char *bbPutS(char *p, const char *e, const char *s) { while (*s && p < e) *p++ = *s++; return p; }
static char *bbPutU(char *p, const char *e, unsigned v) {
  char t[12]; int n = 0; do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
  while (n && p < e) *p++ = t[--n];
  return p;
}
// №775: чего ждёт поток — вид ожидания (2 пауза, 3 семафор, 4 флаг событий, 9 конец потока…) и имя объекта (семафор, флаг или поток), если ядро его отдаёт.
// Так строка user_main на экране сторожа сама называет замок/GE/поток, на котором стоит загрузка. BB_DOG_WAIT_OLD=1 — прежние строки (без шага и ожиданий)
#ifndef BB_DOG_WAIT_OLD
#define BB_DOG_WAIT_OLD 0
#endif
static char *bbPutWait(char *p, const char *e, const SceKernelThreadInfo *ti) {
  if (BB_DOG_WAIT_OLD) return p;
  p = bbPutS(p, e, " w="); p = bbPutU(p, e, (unsigned)ti->waitType);
  if (ti->waitType && ti->waitId > 0) {
    SceKernelSemaInfo si; SceKernelEventFlagInfo ei; SceKernelThreadInfo wt;
    memset(&si, 0, sizeof si); si.size = sizeof si; memset(&ei, 0, sizeof ei); ei.size = sizeof ei; memset(&wt, 0, sizeof wt); wt.size = sizeof wt;
    const char *nm = 0;
    if (sceKernelReferSemaStatus(ti->waitId, &si) >= 0) nm = si.name;
    else if (sceKernelReferEventFlagStatus(ti->waitId, &ei) >= 0) nm = ei.name;
    else if (sceKernelReferThreadStatus(ti->waitId, &wt) >= 0) nm = wt.name;
    p = bbPutS(p, e, ":"); if (nm) { char n_[32]; memcpy(n_, nm, 31); n_[31] = 0; p = bbPutS(p, e, n_); } else p = bbPutU(p, e, (unsigned)ti->waitId);
  }
  return p;
}
static void bbDogScreen(unsigned ms, int ph) {   // правка 194: улика на экране зависшей PSP (только целые и строки, без кучи)
  void *top = 0; int bw = 0, fmt = 0;
  if (sceDisplayGetFrameBuf(&top, &bw, &fmt, PSP_DISPLAY_SETBUF_IMMEDIATE) < 0 || !top) return;
  pspDebugScreenInitEx((void *)(((unsigned)top & 0x0FFFFFFFu) | 0x40000000u), fmt, 0);
  pspDebugScreenSetBackColor(0xFF000000u); pspDebugScreenSetTextColor(0xFF00FFFFu); pspDebugScreenSetXY(0, 0);
  char ln[96], *p, *e = ln + sizeof ln - 2;
  if (ph < 0 || ph >= BBP_N) ph = 0;
  p = bbPutS(ln, e, "STALL "); p = bbPutU(p, e, ms); p = bbPutS(p, e, "ms phase="); p = bbPutS(p, e, bbPhName[ph]);
  p = bbPutS(p, e, " op="); p = bbPutS(p, e, bbOpS); p = bbPutS(p, e, " heap="); p = bbPutU(p, e, memFreeNow == ~0u ? 0u : memFreeNow >> 10);
  p = bbPutS(p, e, "K\n"); *p = 0; pspDebugScreenPuts(ln);
  if (!BB_DOG_WAIT_OLD) { p = bbPutS(ln, e, "step="); p = bbPutS(p, e, bbStep); p = bbPutS(p, e, "\n"); *p = 0; pspDebugScreenPuts(ln); }   // №775
  SceUID ids[40]; int n = 0;
  if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, 40, &n) < 0) return;
  for (int i = 0; i < n && i < 40; i++) {
    SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
    if (sceKernelReferThreadStatus(ids[i], &ti) < 0) continue;
    p = bbPutS(ln, e, ti.name); p = bbPutS(p, e, " st="); p = bbPutU(p, e, (unsigned)ti.status); p = bbPutS(p, e, " pri=");
    p = bbPutU(p, e, (unsigned)ti.currentPriority); p = bbPutS(p, e, " run="); p = bbPutU(p, e, ti.runClocks.low);
    p = bbPutWait(p, e, &ti);   // №775
    p = bbPutS(p, e, "\n"); *p = 0; pspDebugScreenPuts(ln);
  }
}
static void bbDogWrite(unsigned ms, int ph, unsigned beat) {
  char *p = bbDogOut, *e = bbDogOut + BB_FILE - 1;
  if (ph < 0 || ph >= BBP_N) ph = 0;
  p = bbPutS(p, e, "STALL n="); p = bbPutU(p, e, bbN + 1); p = bbPutS(p, e, " ms="); p = bbPutU(p, e, ms);
  p = bbPutS(p, e, " phase="); p = bbPutU(p, e, (unsigned)ph); p = bbPutS(p, e, " "); p = bbPutS(p, e, bbPhName[ph]);
  p = bbPutS(p, e, " beat="); p = bbPutU(p, e, beat); p = bbPutS(p, e, " op="); p = bbPutS(p, e, bbOpS);
  p = bbPutS(p, e, " list="); p = bbPutU(p, e, (unsigned)sceGuCheckList() >> 10); p = bbPutS(p, e, "K last="); p = bbPutU(p, e, geNow >> 10);
  p = bbPutS(p, e, "K heap="); p = bbPutU(p, e, memFreeNow == ~0u ? 0u : memFreeNow >> 10); p = bbPutS(p, e, "K msn="); p = bbPutS(p, e, bbMsn);
  p = bbPutS(p, e, " EBOOT " __DATE__ " " __TIME__ "\n");
  if (!BB_DOG_WAIT_OLD) {   // №775: последний законченный шаг загрузки и чего ждёт каждый поток (как на экране сторожа)
    p = bbPutS(p, e, "STALLSTEP "); p = bbPutS(p, e, bbStep); p = bbPutS(p, e, "\n");
    SceUID ids[40]; int n = 0;
    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, 40, &n) >= 0)
      for (int i = 0; i < n && i < 40; i++) {
        SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti;
        if (sceKernelReferThreadStatus(ids[i], &ti) < 0) continue;
        p = bbPutS(p, e, "STALLTH "); p = bbPutS(p, e, ti.name); p = bbPutS(p, e, " st="); p = bbPutU(p, e, (unsigned)ti.status);
        p = bbPutS(p, e, " pri="); p = bbPutU(p, e, (unsigned)ti.currentPriority); p = bbPutS(p, e, " run="); p = bbPutU(p, e, ti.runClocks.low);
        p = bbPutWait(p, e, &ti); p = bbPutS(p, e, "\n");
      }
  }
  for (unsigned k = 0; k < bbCnt && k < BB_N; k++) {   // кольцо как есть (строки уже готовы; главный поток стоит)
    const char *s = bbRing[(bbHead + BB_N - bbCnt + k) % BB_N]; unsigned l = (unsigned)strnlen(s, BB_LINE);
    if (p + l > e) break;
    memcpy(p, s, l); p += l;
  }
  unsigned used = (unsigned)(p - bbDogOut);
  memset(p, ' ', (size_t)(e - p)); bbDogOut[BB_FILE - 1] = '\n';
  { unsigned i = 0; for (; i < sizeof bbDogHead - 1 && bbDogOut[i] != '\n'; i++) bbDogHead[i] = bbDogOut[i]; bbDogHead[i] = 0; }   // гейт: шапка в BBSTALL
  for (int i = 0; bbJob && i < 200; i++) sceKernelDelayThread(1000);   // фоновая запись ящика — пусть закончит
  // №775: запись мимо замка карты (msioWriteRaw): если главный стоит внутри своего чтения с замком, обычная запись сторожа ждала бы его вечно и
  // улики не было бы. И копия в stall.txt — её не перепишет ящик, если игра потом оживёт (BB_DOG_WAIT_OLD=1 — прежнее: только blackbox.txt через замок)
  char fn[256]; sibling(fn, "blackbox.txt"); SceUID f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT, 0777); if (f < 0) return;
  if (BB_DOG_WAIT_OLD) sceIoWrite(f, bbDogOut, BB_FILE); else msioWriteRaw(f, bbDogOut, BB_FILE);
  sceIoClose(f);
  if (!BB_DOG_WAIT_OLD) { sibling(fn, "stall.txt"); f = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777); if (f >= 0) { msioWriteRaw(f, bbDogOut, used); sceIoClose(f); } }
}
static int bbDogThread(SceSize a, void *b) {
  (void)a; (void)b; unsigned last = bbBeat, still = 0; int told = 0; fpuThreadInit();   // правка 252: ловушки FPU потока
  for (;;) {
    sceKernelDelayThread(BB_DOG_STEP * 1000u);
    fpuThreadPoll(FPU_T_DOG);
    unsigned now = bbBeat;
    if (now != last || !bbDogOn) { if (told) bbStallMs = still; last = now; still = 0; told = 0; continue; }   // ожил: главный поток допишет
    still += BB_DOG_STEP;
    if (still >= BB_STALL_MS && !told) { told = 1; bbStalls++; int ph = bbPhase; bbStallPh = (unsigned)ph; bbDogScreen(still, ph); bbDogWrite(still, ph, now); }
  }
  return 0;
}
static void bbWriterStart(void) {
  bbProbeVolatile();   // исследование 03.10 П3: «лишние 4 МБ» доступны? — взять и сразу отпустить, итог в шапке ящика
  bbSema = sceKernelCreateSema("bbw", 0, 0, 1, NULL);
  SceUID t = bbSema >= 0 ? sceKernelCreateThread("bbw", bbWriterThread, 0x28, 0x2000, 0, 0) : -1;
  if (t < 0 || sceKernelStartThread(t, 0, 0) < 0) { if (bbSema >= 0) sceKernelDeleteSema(bbSema); bbSema = -1; }   // нет потока — пишем синхронно
  SceUID d = sceKernelCreateThread("bbdog", bbDogThread, BB_DOG_PRI, 0x2000, 0, 0);   // правка 194: выше всех потоков игры (звук 0x12, загрузчик 0x18, главный 0x20)
  if (d >= 0) sceKernelStartThread(d, 0, 0);
}
static char sfxBuf[3072]; static int sfxN, sfxLogOn;   // правка 135.7: журнал звуков (ниже)
// ящик на карту: sync — сейчас, из этого потока (подождать фоновую запись до 0.2 с); иначе — потоку «bbw» (занят — при следующем случае)
static unsigned bbSyncN; static int bbSyncOld = BB_SYNC_OLD;   // синхронных записей ящика с главного потока (гейт: у игрока только fail); «flag bbsyncold 1» — прежнее
static void bbFlush(const char *head, int sync) {
  if (sync || bbSema < 0) {
    if (!bbWaitIdle(200)) { bbPend = 1; return; }
    bbSyncN++;
    bbLastW = bbClock; bbPend = 0; bbCompose(head); bbFileWrite(); return;
  }
  if (bbJob) { bbPend = 1; return; }
  bbLastW = bbClock; bbPend = 0; bbCompose(head);
  int j = 1; if (sfxN) { memcpy(sfxOut, sfxBuf, sfxN); sfxOutLen = sfxN; sfxN = 0; j |= 2; }
  __asm__ volatile("" ::: "memory");
  bbJob = j; sceKernelSignalSema(bbSema, 1);
}
void bbOp(const char *op, const char *arg, int sync) {   // viewer/blackbox.h: что главный поток начинает сейчас
  int n = snprintf(bbOpS, sizeof bbOpS, "%s ", op); if (n < 0 || n >= (int)sizeof bbOpS) n = (int)sizeof bbOpS - 1;
  for (const char *a = arg ? arg : "-"; *a && n < (int)sizeof bbOpS - 1; a++) bbOpS[n++] = *a == ' ' ? '_' : *a;   // одно слово в строке
  bbOpS[n] = 0;
  bbOpAt = bbNow();
  if (sync && !bbSyncOld && strcmp(op, "fail") && strcmp(op, "melad")) sync = 0;   // см. blackbox.h: в кадре карту не ждём
  if (!strcmp(op, "msn") && arg) snprintf(bbMsn, sizeof bbMsn, "%s", arg);
  bbSample("op"); bbFlush("RUN", sync);   // и до первой секунды игры (NEW GAME: модель Мардж на старте m0) — файл RUN уже с действием
  if (smkOn) { char s[96]; snprintf(s, sizeof s, "BBOP %s sync=%d n=%u syncn=%u t=%.1f\n", bbOpS, sync, bbN, bbSyncN, smkT); smkWrite(s); }   // гейт tests/crash_school
}
static void sfxFlush(void);
static void blackBox(int exitNow) {   // выход (HOME, QUIT GAME): последняя строка и запись — синхронно (поток «cb» или главный)
  if (!exitNow) return;
  bbWaitIdle(300);
  bbSample("exit"); bbCompose("EXIT"); bbFileWrite(); sfxFlush();
}
static void blackBoxSecond(float sec) {   // раз в секунду из главного цикла: образец раз в BB_PERIOD с или на событии
  bbClock += sec; bbT += sec; bbSecT0 = sceKernelGetSystemTimeLow();
  bbProbeThreadsSecond();   // исследование 03.10 П1/П2: доли процессора потоков и кадры с видимой незагруженной клеткой за секунду
  { static unsigned seenBad, seenSteal, seenReop;   // ловушка 05.10: чужое/повторное закрытие файлов, переоткрытия файла уровня
    if (ioBadN != seenBad || ioStealN != seenSteal || loaderReopenN != seenReop) {
      char a_[96]; snprintf(a_, sizeof a_, "bad=%u@%08x(fd%d,%08x) steal=%u@%08x reopen=%u", ioBadN, ioBadRa, ioBadFd, (unsigned)ioBadRes, ioStealN, ioStealRa, loaderReopenN);
      bbOp("ioclose", a_, 1); if (smkOn) { char s_[160]; snprintf(s_, sizeof s_, "IOCLOSE %s t=%.1f\n", a_, smkT); smkWrite(s_); }
      seenBad = ioBadN; seenSteal = ioStealN; seenReop = loaderReopenN; } }
  unsigned mf = memFailsNow(), ce = smkAllocFail, re = smkReadErr, gs = smkGeSkip, gb = smkGeBad;
  static int bbSeenTight;
  const char *ev = gb != bbSeenBad ? "GEBAD" : mf != bbSeenMem ? "MEMFAIL" : ce != bbSeenCell ? "CELLMEM" : re != bbSeenRead ? "READERR"
                 : gs != bbSeenGe ? "GEFULL" : (memTight && !bbSeenTight) ? "MEMLOW" : 0;
  bbSeenMem = mf; bbSeenCell = ce; bbSeenRead = re; bbSeenGe = gs; bbSeenBad = gb; bbSeenTight = memTight;
  { static int probeSeen; if (!probeSeen) { probeSeen = 1; char ft[256]; sibling(ft, "fpeprobe"); SceUID h = sceIoOpen(ft, PSP_O_RDONLY, 0);
      if (h >= 0) { sceIoClose(h); fpeProbe(); } } }   // правка 252: служебная проба на железе (файл fpeprobe)
  fpuThreadPoll(FPU_T_MAIN);   // правка 252: 0/0, x/0, переполнение за секунду (любой поток) — событие FPE в ящик, строка в smoke.txt
  { unsigned fk = fpuSeenThr << 8 | fpuSeenFlg | carNanFixes << 16;
    if (fk != fpuSeenTold) { fpuSeenTold = fk; if (!ev) ev = "FPE";
      if (smkOn) { char t[96], g[32]; fpuTag(g, sizeof g); snprintf(t, sizeof t, "FPE t=%.1f %s\n", smkT, g); smkWrite(t); } } }
  bbDogOn = 1;   // сторож — с первой секунды игры (заставка и меню до неё — без него)
  if (bbStallMs) {   // сторож видел стоянку, главный поток ожил: строка в кольцо (фаза и длительность)
    unsigned ms = bbStallMs, ph = bbStallPh < BBP_N ? bbStallPh : 0; bbStallMs = 0;
    char a[32]; snprintf(a, sizeof a, "%s_%u", bbPhName[ph], ms); bbOp("stall", a, 0);
    if (smkOn) { char s[200]; snprintf(s, sizeof s, "BBSTALL phase=%s ms=%u n=%u t=%.1f head=%s\n", bbPhName[ph], ms, bbStalls, smkT, bbDogHead); smkWrite(s); }
  }
  if (!bbN) { bbSample("start"); bbFlush("RUN", bbSyncOld); bbT = 0.f; if (smkOn) { char s_[64]; snprintf(s_, sizeof s_, "BBSYNC first syncn=%u t=%.1f\n", bbSyncN, smkT); smkWrite(s_); } return; }   // первая секунда игры: файл RUN (вылет без событий тоже виден)
  if (ev || bbT >= BB_PERIOD) { bbT = 0.f; bbSample(ev ? ev : "-"); }
  if (ev || bbPend || bbClock - bbLastW >= BB_EVERY) bbFlush("RUN", 0);   // фоном: кадр карту не ждёт
  else if (sfxN && !bbJob && bbSema >= 0) {   // журнал звуков — тоже фоном (раз в секунду)
    memcpy(sfxOut, sfxBuf, sfxN); sfxOutLen = sfxN; sfxN = 0; __asm__ volatile("" ::: "memory"); bbJob = 2; sceKernelSignalSema(bbSema, 1);
  }
}
// Правка Роба 135.7 (01.10: «много разбивал, какой-то ненужный сэмпл»): журнал эффектов. Флаг-файл sfxlog рядом с EBOOT
// (или «flag sfxlog» сценария смоука): на каждый запущенный звук эффекта строка «SFX <клип> <событие> <x y z> vol=… t=… L… место»
// — в smoke.txt (служебный запуск) и в sfxlog.txt рядом с EBOOT (буфер в памяти, на карту раз в секунду и при выходе: без записи
// на карту в кадре удара). Событие: crash, obj, prop, sprop, land, air, gear, ped, wreck, explode, 2d, fx. Без флага — ничего.
// Правка 103 (01.10): раз в секунду журнал уходит на карту потоком записи самописца (blackBoxSecond); здесь — синхронно, только
// при переполнении буфера за секунду и на выходе (дождавшись фоновой записи — дописи в файл не перемешаются)
static void sfxFlush(void) {
  if (!sfxN) return;
  bbWaitIdle(100); sfxFileAppend(sfxBuf, sfxN);
  sfxN = 0;
}
static void sfxLine(const char *l) {
  int foot = onFoot(); char pc[24]; placeCode(foot ? homer.x : car.x, foot ? homer.z : car.z, pc, sizeof pc);
  char s[200]; int n = snprintf(s, sizeof s, "%s t=%.1f L%d %s\n", l, bbClock, gameLevel, pc);
  if (n > (int)sizeof s - 1) n = sizeof s - 1;
  if (smkOn) smkWrite(s);
  if (sfxN + n > (int)sizeof sfxBuf) sfxFlush();
  memcpy(sfxBuf + sfxN, s, n); sfxN += n;
}
static void sfxLogStart(void) {   // флаг-файл sfxlog: новый журнал на каждый запуск
  char fn[256]; sibling(fn, "sfxlog"); SceUID h = sceIoOpen(fn, PSP_O_RDONLY, 0); if (h < 0) return; sceIoClose(h);
  sibling(fn, "sfxlog.txt"); h = sceIoOpen(fn, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
  if (h >= 0) { const char *t = "# SFX клип событие x y z громкость t(с игры) уровень место\n"; sceIoWrite(h, t, strlen(t)); sceIoClose(h); }
  sfxLogOn = 1; sndLogHook(sfxLine);
}
// Ловушка вылета (аудит ведущего 01.10 22:35, обрыв F10-5 без STALL): модуль ядра sharexc.prx рядом с EBOOT — исключение
// процессора в любом потоке пишет строку CRASH (epc, ra, badvaddr, база модуля, цепочка вызовов) хвостом blackbox.txt и в crash.txt.
// Модулю передаётся папка игры. Нет файла (эмулятор, смоук) — kexcRc 1, игра идёт как раньше
static void kexcBoot(void) {
  { char a[256], b[256]; sibling(a, "crash.txt"); sibling(b, "crash_prev.txt"); SceUID c = sceIoOpen(a, PSP_O_RDONLY, 0);
    if (c >= 0) { sceIoClose(c); sceIoRemove(b); sceIoRename(a, b); } }   // прошлый вылет -> crash_prev.txt
  char fn[256], dir[200]; sibling(fn, "sharexc.prx");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) { kexcRc = 1; return; } sceIoClose(f);
  strncpy(dir, fn, sizeof dir - 1); dir[sizeof dir - 1] = 0; char *sl = strrchr(dir, '/'); if (sl) sl[1] = 0; else dir[0] = 0;
  SceUID m = kuKernelLoadModule(fn, 0, 0); if (m < 0) { kexcRc = m; return; }
  int st = 0; int rc = sceKernelStartModule(m, strlen(dir) + 1, dir, &st, 0); kexcRc = rc < 0 ? rc : 0;
}
static void blackBoxBoot(void) {   // старт: прошлый ящик без EXIT (вылет или выключение) -> blackbox_prev.txt
  // правка 252: ловушки FPU главного потока (viewer/fpuguard.h); служебный файл fputrap — прежнее, как у PSP по умолчанию
  { char ft[256]; sibling(ft, "fputrap"); SceUID h = sceIoOpen(ft, PSP_O_RDONLY, 0); if (h >= 0) { sceIoClose(h); fpuKeep = 1; } }
  fpuBoot = fpuThreadInit(); fpuApplied = fpuGet();
  char fn[256], fp[256], b[8] = {0}; sibling(fn, "blackbox.txt"); sibling(fp, "blackbox_prev.txt");
  SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  sceIoRead(f, b, 3); sceIoClose(f);
  if (!memcmp(b, "RUN", 3) || !memcmp(b, "STA", 3)) { sceIoRemove(fp); sceIoRename(fn, fp); }   // правка 103: STALL сторожа — тоже вылет
}
// Правка Роба 86: зоны DEATH оригинала (Event 4: овраг, горящие покрышки у свалки — R6D01): попал машиной или пешком — вернуть
// в точку локатора (у дороги), машина целой по состоянию (урон как был), скорость 0
// Правка 349 (L1 K2-8, двор горящих покрышек за забором): все объёмы локатора, курс машины — курс локатора (game.c gameDeathAt/Put);
// показ как у оригинала ([PS2] 05.10, transit.c): мир стоит, кадр гаснет за 0,92 с, перенос в чёрном, чёрный 0,26 с, картинка сразу.
// «flag deathold» / -DDEATH_OLD=1 — как до 349 (первый объём, мгновенно, курс прежний)
#ifndef ZONE_IRIS_OLD
#define ZONE_IRIS_OLD 0   // №349 (Роб 17.10: «просто ирис, во дворе, где угодно, на все обходные пути»): возврат из зоны DEATH и после BUSTED — ирисом,
                          // как вход в комнаты (transitClose; раскрытие над игрой). 1 — прежнее затемнение по записи PS2 (0,92 с / 0,8 с + чёрный)
#endif
#define DEATH_FADE_S .92f   // [PS2] 46 кадров 50 Гц до чёрного
#define DEATH_HOLD_S .26f   // [PS2] чёрный после переноса 12–14 кадров
static int deathCheck(void **fbp) {   // проверка и перенос — viewer/game.c (общие с миром ботов); -> показано кадров затемнения
  if (!haveCol || inRoom() || !world.ndeath) return 0;
  int foot = onFoot(), k = gameDeathAt(&G), shown = 0; if (k < 0) return 0;
  int iris = !ZONE_IRIS_OLD && !gameDeathIsOld() && transitReady(), fade = !iris && !gameDeathIsOld() && transitFadeReady();
  if (iris) shown = transitClose(fbp, "death");
  else if (fade) shown = transitFade(fbp, "death", DEATH_FADE_S);
  gameDeathPut(&G, k);
  const float *A = world.death[k].apex;
  if (!foot) chase.init = 0;
  if (fade) transitHold(DEATH_HOLD_S);
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "DEATH %s rec=%d -> %.1f,%.1f,%.1f h=%.3f foot=%d fade=%d iris=%d t=%.1f\n", world.death[k].name, k, A[0], A[1], A[2],
                                     foot ? homer.h : car.h, foot, fade, iris, smkT); smkWrite(s); }
  return shown;
}
// №627 (Роб; запись PS2 ps2-police-after-busted.yaml, L1, 50 кадр/с): после штрафа полицейская НЕ уезжает: стоит на месте с мигалкой, и через 1,5 с после ареста
// она и карточка BUSTED пропадают одним шагом на глазах; затем (кадр 1450, +1,8 с) мир гаснет ~0,6–0,8 с, чёрный 0,2 с, раскрытие (+2,8 с) в новом месте: машина
// игрока на проезжей части в 4 м от точки ареста, камера вдоль дороги. Раньше порт убирал полицейскую в момент ареста, карточка висела 2,5 с, машину ставил
// мгновенно без затемнения. Затемнение — то же, что у зоны DEATH (transitFade/transitHold). Шкала после штрафа 5 → 0 за ~3 с в порту не воспроизводится
// (шкала сразу 0, не видна под чёрным). BUSTED_PS2_OLD=1 — прежнее (красный контроль tests/hnr/busted_ps2.sh)
#ifndef BUSTED_PS2_OLD
#define BUSTED_PS2_OLD 0
#endif
#define BUSTED_FADE_DELAY .3f   // [PS2] карточка и полицейская пропали на 1,5 с, затемнение с 1,8 с
#define BUSTED_FADE_S .8f       // [PS2] кадры 1450–1490: до полного чёрного 0,8 с
#define BUSTED_HOLD_S .2f       // [PS2] чёрный до раскрытия на 1500: 0,2 с
static float bustFadeT = -1.f;  // >= 0: арест кончился, ждём затемнения (ввод заморожен)
static int bustPolice;          // полицейская ещё стоит у машины игрока (до конца таймера ареста)
static int bustedCheck(void **fbp, float dt) {   // №627: после ареста: пауза, затемнение, перенос машины на проезжую часть, чёрный, раскрытие; -> показано кадров
  if (bustFadeT < 0.f || (bustFadeT -= dt) > 0.f) return 0;
  bustFadeT = -1.f;
  int iris = !ZONE_IRIS_OLD && transitReady(), fade = !iris && transitFadeReady(), shown = 0; float p[3], hh; int placed = 0;
  if (iris) shown = transitClose(fbp, "busted");
  else if (fade) shown = transitFade(fbp, "busted", BUSTED_FADE_S);
  if (!onFoot() && haveTraffic && trafficLaneSpot(&traffic, car.x, car.z, 30.f, p, &hh)) { carPlace(&car, &world, p[0], p[1] + .5f, p[2], hh); chase.init = 0; placed = 1; }
  if (fade) transitHold(BUSTED_HOLD_S);
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "BUSTED fade=%d iris=%d placed=%d t=%.2f\n", fade, iris, placed, smkT); smkWrite(s); }
  return shown;
}
// Правка Роба 79 (запись PS2: монета у счёта крутится, на 3:49 видна ребром): у счёта монет — меш монеты мира в экранной
// проекции, поворот вокруг вертикали 1 оборот/с (правило порта), размер — высота иконки счёта, выезд и прозрачность — её же
static int coinSprHud(float cx, float cy, float hh, float al);   // №802: спрайт монеты у счёта (coinspr.h); 0 — нет ленты, рисуем 3D-монету
static void hudCoin3D(void) {
  float cx, cy, hh, al; if (!uiCoinIcon(&cx, &cy, &hh, &al) || !haveCoins || props.coinType < 0 || al <= .02f) return;
  if (coinSprHud(cx, cy, hh, al)) return;
  static float t; t += 1.f / 60.f; float a = t * 6.2831853f, cs_ = cosf(a), sn = sinf(a), k = hh / .62f;
  PropType *cty = &props.types[props.coinType];
  sceGumMatrixMode(GU_PROJECTION); sceGumPushMatrix(); sceGumLoadIdentity(); sceGumOrtho(0.f, 480.f, 272.f, 0.f, -100.f, 100.f);
  sceGumMatrixMode(GU_VIEW); sceGumPushMatrix(); sceGumLoadIdentity(); sceGumMatrixMode(GU_MODEL); sceGumUpdateMatrix();
  sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_FOG); resetState();
  for (unsigned q = 0; q < cty->nPiece; q++) {
    PropPiece *pp = &props.pieces[cty->firstPiece + q]; if (pp->ni <= 0) continue;
    float s_ = pp->sc * k, m[16] = {cs_ * s_, 0, -sn * s_, 0,  0, -s_, 0, 0,  sn * s_, 0, cs_ * s_, 0,
                                    cx + (cs_ * pp->ox + sn * pp->oz) * k, cy - (pp->oy - .31f) * k, 0.f, 1};   // правка 115: центр монеты (0.31 м над её низом) — в центр иконки; было «+ .31» — выше на целую высоту иконки
    Piece tmp; memset(&tmp, 0, sizeof tmp); tmp.r.tex = pp->tex; tmp.r.flags = pp->flags; tmp.r.ni = pp->ni;
    tmp.v = (Vert *)((char *)props.vertBase + pp->vOff); tmp.idx = (unsigned short *)((char *)props.idxBase + pp->iOff);
    bindTexLean(tmp.r.tex); setFlags(tmp.r.flags); sceGuSetMatrix(GU_MODEL, (const ScePspFMatrix4 *)m); drawGeo(&tmp, 0, tmp.r.ni);
  }
  sceGumMatrixMode(GU_VIEW); sceGumPopMatrix(); sceGumMatrixMode(GU_PROJECTION); sceGumPopMatrix();
  sceGumMatrixMode(GU_MODEL); sceGumUpdateMatrix(); sceGuEnable(GU_DEPTH_TEST); resetState();
}
// Правка Роба 78 (29.09: «Гомер сливается с дорогой — одна тональность, нет теней»): пятно тени под Гомером пешком, как у машины
// (carVisDrawShadow: мягкое тёмное пятно по земле; у персонажей оригинала тень-пятно под ногами). Круг 0.55 м, середина — альфа
// 120, край — 0; в прыжке бледнеет с высотой (8 м — нет)
static void homerShadow(const ColWorld *w) {
  if (!haveHomer || !onFoot() || !w) return;
  float gy; if (!colGround(w, homer.x, homer.z, homer.y + .6f, homer.y - 30.f, &gy, 0)) return;
  float fade = 1.f - fminf(1.f, fmaxf(0.f, (homer.y - gy) / 8.f)); if (fade <= 0.f) return;
  // TEST25 (№605, Роб на TEST23 «под ним тень прыгает»; у лавки на краю тротуара земля под Гомером перескакивает между бордюром и дорогой, а вершины круга берут высоту каждая по своей
  // точке): высота центра сглаживается (перескок до 0,5 м — плавно за ~4 кадра, больше — сразу), вершины не выше/ниже центра больше чем на 0,12 м (круг лежит плашмя, не ломается
  // уступом), подъём над землёй 0,06 вместо 0,04. HOMER_SHADOW_SMOOTH_OLD=1 — прежнее
#ifndef HOMER_SHADOW_SMOOTH_OLD
#define HOMER_SHADOW_SMOOTH_OLD 0
#endif
  static float gys; static int gysOn;
  if (!HOMER_SHADOW_SMOOTH_OLD) { if (!gysOn || fabsf(gy - gys) > .5f) gys = gy; else gys += (gy - gys) * .3f; gysOn = 1; gy = gys; }
  float lift_ = HOMER_SHADOW_SMOOTH_OLD ? .04f : .06f;
  typedef struct { unsigned c; float x, y, z; } CV;
  // №692/мультик (Роб: «тень под ногами не лужа, а чёткая, немного векторная»): в режиме Cartoon — ровный овал с жёстким краем (одна альфа по всему кругу,
  // 20 сегментов), радиус 0,6 м. TOON_OVAL_OLD=1 — как №675 (мягкий круг со слабой альфой)
#ifndef TOON_OVAL_OLD
#define TOON_OVAL_OLD 0
#endif
  int oval = skinToonEnabled() && !TOON_OVAL_OLD; int nSeg = oval ? 20 : 12; float rad = oval ? .6f : .55f;
  CV *v = (CV *)geVbAlloc((nSeg + 2) * sizeof(CV), GEVB_GSHADE_OLD); unsigned a = groundShadeAlpha((oval ? 100.f : 120.f * skinToonShadowK()) * fade);   // №675: в мультике слабее; овал — ровные ~40 %
  v[0] = (CV){a << 24, homer.x, gy + lift_, homer.z};
  for (int i = 0; i <= nSeg; i++) { float t = i * (6.2831853f / (float)nSeg), px=homer.x+cosf(t)*rad, pz=homer.z+sinf(t)*rad, py=gy;
    if(!groundShadeOld)colGround(w,px,pz,homer.y+.6f,homer.y-30.f,&py,0);
    if (!HOMER_SHADOW_SMOOTH_OLD) py = fminf(gy + .12f, fmaxf(gy - .12f, py));
    v[i + 1] = (CV){oval ? a << 24 : 0u, px, py + lift_, pz}; }
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f; sceGuSetMatrix(GU_MODEL, &I);
  groundShadeBegin(); sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE); sceGuDisable(GU_CULL_FACE);
  sceGuDrawArray(GU_TRIANGLE_FAN, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, nSeg + 2, 0, v);
  groundShadeEnd();
  sceGuDepthMask(GU_FALSE); resetState();
}
#define STICK_ACTIVE 40   // правка 159: стик дальше 40 из 128 от центра — активность игрока (не дрейф)
static int hnrForceBust;   // служебное: flag hnrbust — арест сразу (гейт правки 227 tests/hnr/bust_freeroam.sh)
static int hnrMusic, hnrActive; static float hnrMsgT, hnrCatchT, hnrSpawnT, hnrIdleT;
// Правка 121 (30.09, «доводи логику до оригинала, сверяйся напрямую»): погоня и арест — правила исполняемого файла PAL
// SLES_518.97 (разбор hit-and-run-web/src/hit-and-run.ts и pursuit.ts, MIT: обновление 0x251c78), порядок проверок — как там.
// Шкала дошла до 100 — погоня защёлкнута (G.hnrLatched), держится, пока шкала не спадёт ниже 5; пока защёлкнута и нет ареста —
// полиция: SetNumChaseCars машин уровня (L1 одна, Pursuit\L1cop.con) в 100 м, отставшая дальше 110 м — заново в 100 м. Полиция
// есть — погоня идёт (hnrActive); полиции нет и защёлка отпущена — ушёл. Арест: полиция ближе 10 м, а игрок медленнее 30 км/ч
// 0.75 с (пешком — любая скорость, 1.5 с): шкала 0, штраф 50 монет (не больше, чем есть), полиция исчезает, звук busted,
// «BUSTED!», 1.48 с заморозки (№430, правило порта: 1500 мс таймера PS2 с шагом кадра ареста, 74 кадра при 50 Гц; нарушения не считаются). Предупреждение rage_warning — шкала выше 78, снова — после спада ниже 60.
// Сверка с записью PS2 (tools/ps2ref/hnr_track.py): прибавки 5 / 7.5 / 10 / 13 / 25 на ходе указателя ~300°, спад 3/с после 3 с,
// в погоне 6/с. Ареста в записи нет — штраф и заморозка только по данным
#define HNR_FINE 50
#define ENG_OFF_S 3.f   // правка 175: мотор оставленной машины глохнет за 3 с (оригинал: −34 → −44 dB за 2–3 с после выхода)
// HNR_BUST_R / HNR_BUST_V / HNR_BUST_CAR / HNR_BUST_FOOT — game.h; таймер ареста и задержка полиции — game.c (№430, ARREST_PS2_OLD)
#define HNR_RELEASE .05f
#define HNR_WARN .78f
#define HNR_WARN_REARM .60f
#define HNR_REMOVE_R 110.f
// оракул PS2 30.09 (knowledge/facts/ps2-hnr-police): полицейская появляется через 0.72–0.76 с после начала погони; к стоящему игроку
// издалека не едет (аудит 09.10, msncar.c POLICE_STAND_OLD: в чистых записях PS2 93,7–118 м), дальше 110 м — заново в 100 м; после ареста по концу таймера (1,48 с, №430) машину игрока
// ставит в ближайшую точку дороги (6–16 м, трасса b3_bust)
// задержка полиции после полной шкалы — gameHnrChaseStart (game.c): №430 — 0,72 с, правило порта в диапазоне факта 0,72–0,76 с (записи b3_* 33–37 кадров ±1; было 0,6 с, ps2-hnr-chase ступень 4)
#define HNR_POLICE_CYCLE 10.f
static int hnrWarned;
#ifndef HNR_HUD_ALARM_LATCH   // 1 — прежнее (красный контроль гейта tests/hnr/hud.sh): кольцо красно-синее с полной шкалы, ещё до полиции
#define HNR_HUD_ALARM_LATCH 0
#endif
// правка 193 (близнец tw_hnr_text, оригинал PS2): при полной шкале — только крупная красная «Hit & Run!», 4 вспышки (кадры 166–172,
// 181–187, 199–205, 214–220 при 50 к/с после шкалы 100 на 150): с 0,2 с, период 0,32 с, горит 0,18 с; квитанция штрафа — только при
// аресте (k = 1, «BUSTED!»). Строки поля HNR_TEXT (tools/ui_export.py hnr_like_original): 0 — «Hit & Run!», 1 — «BUSTED!», 2 — пусто
#define HNR_BLINK_T0 .2f
#define HNR_BLINK_PER .32f
#define HNR_BLINK_ON .18f
#define HNR_BLINK_N 4
static float hnrBlinkT = -1.f;
static void hnrMsg(int k) {
  if (!uiReady) return;
  if (k == 0) { hnrBlinkT = 0.f; hnrMsgT = 0.f; uiHudSet(UI_F_HNR_TEXT, 2); return; }
  hnrBlinkT = -1.f; uiHudSet(UI_F_HNR_TEXT, k); hnrMsgT = 2.5f;
}
// Правка Роба 266 (04.10: «погоня почти как в оригинале, но не заканчивается — музыка не заканчивается»): тема погони ставится
// sndMusic(SND_MUS_HITRUN) — миссия музыкой не владеет, а конец погони звал sndMusicMission(0), который без владения молчит:
// петля Hit_Run_Main играла вечно. У оригинала в композиции уровня событий две — Hit_and_Run и Hit_and_Run_Caught (l1_music.rms);
// ушёл от полиции — музыка места, как после ареста (mReturn у Caught): в машине Sunday Drive, пешком — пешая. «flag hnrmusold» — прежнее
static int hnrMusOld;
static void hnrMusicOff(void) {
  if (hnrMusic) {
    if (hnrMusOld || sndMusicOwned()) sndMusicMission(0);
    else if (sndMusicState() == SND_MUS_HITRUN) sndMusic(onFoot() ? SND_MUS_ONFOOT : SND_MUS_FREEROAM);
    if (smkOn) { char s[64]; snprintf(s, sizeof s, "HNR music off t=%.1f old=%d\n", smkT, hnrMusOld); smkWrite(s); }
  }
  hnrMusic = 0;
}
static void hnrPursuit(float dt) {
  G.hnrOff = mvHnrOff(); G.hnrRoom = inRoom();
  if (G.hnrOff) {   // перепроверка 96: уличная гонка (DisableHitAndRun) — шкала пуста, погони нет (данные: выключение = сброс)
    hnrHeat = hnrCalm = 0.f; G.hnrBusted = hnrCatchT = 0.f; hnrWarned = 0;
    bustFadeT = -1.f; bustPolice = 0;
    if (G.hnrLatched || hnrActive) { G.hnrLatched = hnrActive = 0; mvPolice(0); }
    hnrMusicOff(); return;
  }
  if (hnrMsgT > 0.f) hnrMsgT -= dt;
  if (hnrBlinkT >= 0.f && uiReady) {   // правка 193: мигание «Hit & Run!» как у оригинала
    hnrBlinkT += dt; float u = hnrBlinkT - HNR_BLINK_T0;
    int on = u >= 0.f && u < HNR_BLINK_PER * (HNR_BLINK_N - 1) + HNR_BLINK_ON && fmodf(u, HNR_BLINK_PER) < HNR_BLINK_ON;
    uiHudSet(UI_F_HNR_TEXT, on ? 0 : 2);
    if (u >= HNR_BLINK_PER * HNR_BLINK_N) hnrBlinkT = -1.f;
  }
  if (G.hnrBusted > 0.f) {
    int bustEnd = gameHnrBustTick(&G, dt);
    if (bustEnd && !BUSTED_PS2_OLD && bustPolice) {   // №627: полицейская и карточка пропадают одним шагом; машину ставит затемнение (bustedCheck)
      mvPolice(0); bustPolice = 0; hnrMsgT = 0.f;
      if (smkOn) { char s[64]; snprintf(s, sizeof s, "BUSTED vanish t=%.2f\n", smkT); smkWrite(s); }
      if (HNR_ORACLE_POLICE && !onFoot() && haveTraffic && !inRoom() && transitFadeReady()) { bustFadeT = BUSTED_FADE_DELAY; bustEnd = 0; }
      else if (smkOn) { char s[96]; snprintf(s, sizeof s, "BUSTED nofade foot=%d traffic=%d room=%d ready=%d\n", onFoot(), haveTraffic, inRoom(), transitFadeReady()); smkWrite(s); }
    }
    if (bustEnd && HNR_ORACLE_POLICE && !onFoot() && haveTraffic) {   // оракул (за флагом, msncar.h): конец заморозки ареста — машина в ближайшую точку дороги
      float p[3], hh; if (trafficLaneSpot(&traffic, car.x, car.z, 30.f, p, &hh)) { carPlace(&car, &world, p[0], p[1] + .5f, p[2], hh); chase.init = 0; }
    }
  }
  if (uiReady) uiHudShow(UI_T_HNR_TICKET, hnrMsgT > 0.f || hnrBlinkT >= 0.f);
  if (hnrHeat < HNR_RELEASE) G.hnrLatched = 0;
  if (hnrHeat < HNR_WARN_REARM) hnrWarned = 0;
  else if (!hnrWarned && hnrHeat > HNR_WARN) { hnrWarned = 1; int w = sndId("rage_warning"); if (w >= 0) sndPlay2D(w, 1.f); }   // правка 241: подтверждено записью PS2 — warning_01 один раз на переход (8:59.1, 25:54.4); кольцо при этом мигает (ui_anim.h п. 3)
  if (!G.hnrLatched && hnrHeat >= 1.f) {
    G.hnrLatched = 1; hnrMsg(0); hnrSpawnT = 0.f; if (HNR_ORACLE_POLICE) gameHnrChaseStart(&hnrSpawnT, dt); hnrIdleT = 0.f;   // правка 112: «Hit & Run!» на старте погони, как в записи PS2 25:57
    // правка 174 (Роб 01.10: «нет музыки» в погоне): Hit_and_Run — не событие миссии (в MEVT его нет, sndMusicEvent молчал), а своё
    // состояние музыки: hit_run_intro -> hit_run_main петлёй (l1_music.rms, ps2-hnr-chase.yaml); миссия со своей музыкой — не перебиваем
    if (!hnrMusic) { hnrMusic = !sndMusicOwned(); if (hnrMusic) sndMusic(SND_MUS_HITRUN); }
    if (smkOn) { char s[64]; snprintf(s, sizeof s, "HNR chase t=%.1f\n", smkT); smkWrite(s); }
  }
  const MsnCars *m = mvCars(); float best = 1e9f; int chasers = 0;
  float px = onFoot() ? homer.x : car.x, pz = onFoot() ? homer.z : car.z;
  if (m) for (int i = 0; i < m->n; i++) if (m->c[i].on && mvIsPolice(m->c[i].name)) {
    const Car *k = &m->c[i].car; float d = hypotf(k->x - px, k->z - pz); chasers++; if (d < best) best = d; }
  if (G.hnrLatched && G.hnrBusted <= 0.f && !G.hnrRoom && m) {   // полиция нужна: нет или отстала дальше 110 м — заново в 100 м
    float pv_ = onFoot() ? homer.speed : fabsf(car.v);
    hnrIdleT = HNR_ORACLE_POLICE && chasers && best > HNR_BUST_R && pv_ < HNR_BUST_V ? hnrIdleT + dt : 0.f;   // стоящий игрок, полиция не рядом (за флагом)
    if (((!chasers || best > HNR_REMOVE_R) && gameHnrSpawnDue(&hnrSpawnT, dt)) || hnrIdleT > HNR_POLICE_CYCLE) {
      hnrSpawnT = 1.f; hnrIdleT = 0.f; if (chasers) mvPolice(0); mvPolice(1); chasers = 0; best = 1e9f;
      if (hnrActive) sndSay(SND_SAY_NEWAI, 0);   // аудит 09.10: W_NewAI («Another one?») — новая машина погони (близнец b2_loop кадр 11250, в погоне); первая — без реплики (правило порта)
      if (m) for (int i = 0; i < m->n; i++) if (m->c[i].on && mvIsPolice(m->c[i].name)) {
        const Car *k = &m->c[i].car; float d = hypotf(k->x - px, k->z - pz); chasers++; if (d < best) best = d; }
      if (smkOn) { char s[96]; snprintf(s, sizeof s, "HNR police t=%.1f cars=%d police=%d near=%.0f\n", smkT, m->n, chasers, chasers ? best : -1.f); smkWrite(s); }
    }
  } else if (!G.hnrLatched && chasers && !bustPolice) { mvPolice(0); chasers = 0; best = 1e9f; }   // защёлка отпущена — полиция уезжает (после ареста стоит, №627)
  if (chasers) hnrActive = 1;
  if (hnrActive && !chasers && !G.hnrLatched) {   // ушёл от полиции
    hnrActive = 0; hnrMusicOff();
    if (smkOn) { char s[64]; snprintf(s, sizeof s, "HNR lost t=%.1f\n", smkT); smkWrite(s); }
  }
  if (!hnrActive && !G.hnrLatched) hnrMusicOff();   // шкала спала раньше, чем доехала полиция
  float spd = onFoot() ? homer.speed : fabsf(car.v);
  int caught = gameHnrCatch(&hnrCatchT, dt, hnrActive && chasers && !G.hnrRoom && G.hnrBusted <= 0.f, best, spd, onFoot());   // №430: game.c
  if (caught || hnrForceBust) {   // арест (hnrForceBust — служебный flag hnrbust)
    hnrForceBust = 0;
    // №627: шкала после штрафа 5 и спадает до 0 за ~3 с (PS2, кадр 1700: 0): пауза 1,3 с + 5 ед. при спаде 3/с (1,7 с). BUSTED_PS2_OLD=1 — сразу 0
    hnrHeat = BUSTED_PS2_OLD ? 0.f : .05f; hnrCalm = BUSTED_PS2_OLD ? 3.f : 3.f - 1.3f; G.hnrLatched = hnrActive = 0; hnrCatchT = 0.f; G.hnrBusted = gameHnrBustTime();
    if (BUSTED_PS2_OLD) mvPolice(0); else { mvPoliceHold(); bustPolice = 1; }   // оракул PS2 30.09 (ps2-hnr-police): на аресте машина не останавливается — катится без газа 1.48 с заморозки ввода (№430); №627: полицейская стоит до конца таймера
    if (haveCoins) coins.collected = coins.collected > HNR_FINE ? coins.collected - HNR_FINE : 0;
    int b = sndId("busted"); if (b >= 0) sndPlay2D(b, 1.f);
    hnrMsg(1); if (!BUSTED_PS2_OLD) hnrMsgT = G.hnrBusted;   // №627: карточка до конца таймера (1,48 с), не 2,5 с
    if (hnrMusic) sndMusic(SND_MUS_HITRUN_CAUGHT); hnrMusic = 0;   // правка 174: hit_run_end_neg, потом музыка города
    mvBusted();   // данные миссий: MISSION_FAILED_HIT_N_RUN «Hit & Run: You got busted!» — идущая миссия провалена
    if (smkOn) { char s[80]; snprintf(s, sizeof s, "HNR busted t=%.1f coins=%d\n", smkT, haveCoins ? coins.collected : -1); smkWrite(s); }
    if (smkOn) { int pn = 0; const MsnCars *m2 = mvCars(); if (m2) for (int i = 0; i < m2->n; i++) if (m2->c[i].on && mvIsPolice(m2->c[i].name)) pn++;
      char s[96]; snprintf(s, sizeof s, "BUSTED arrest police_after=%d t=%.2f\n", pn, smkT); smkWrite(s); }   // №627: гейт tests/hnr/busted_ps2.sh
  }
  { static float lt; if (smkOn && (lt += dt) >= 1.f) { lt = 0.f; char s_[112]; snprintf(s_, sizeof s_, "HNRV t=%.1f heat=%.3f latch=%d police=%d near=%.0f busted=%.1f\n", smkT, hnrHeat, G.hnrLatched, chasers, chasers ? best : -1.f, G.hnrBusted); smkWrite(s_); } }   // служебное: шкала раз в секунду
}
// Правка Роба 63 (запись PS2 33:40): угон — Гомер пешком у машины трафика (едет или стоит), ближе, чем у своей: △ — машина
// игрока становится ею (ходовые из .con модели, размеры и посадка — модели трафика), машина трафика уходит с дороги/места
#define JACK_R 1.6f   // м от кузова (правило порта: как дверь своей машины)
static int jackNear(int *park) {
  if (!haveTraffic || !haveHomer || !haveCol || !onFoot() || inRoom() || jackOff) return -1;
  float d; int i = trafficNearest(&traffic, homer.x, homer.z, JACK_R, park, &d); if (i < 0) d = 1e9f;
  if (oldCarOn) { float dO = sqrtf((oldCar.x - homer.x) * (oldCar.x - homer.x) + (oldCar.z - homer.z) * (oldCar.z - homer.z)) - oldCar.p.halfW;
    if (dO < JACK_R && dO < d) { d = dO; i = -2; } }   // своя оставленная машина — назад в неё
  if (i == -1) return -1;
  float dOwn = sqrtf((car.x - homer.x) * (car.x - homer.x) + (car.z - homer.z) * (car.z - homer.z)) - car.p.halfW;
  if (!CAR_HUSK_OLD && car.destroyed && !carBoomed(&car)) dOwn = 1e9f;   // правка 355: в горящую (до взрыва) не сесть — угону не мешает
  return dOwn <= d ? -1 : i;
}
// The parked level car owns its damage, hinges and wheel pose across a hijack.
static void parkOwnCar(const Car *kept, const CarVis *visual) {
  oldCar = *kept; oldCv = *visual; oldCarOn = 1;
}
static void resumeOwnCar(void) {
  car = oldCar; cv = oldCv; cv.motionValid = 0; oldCarOn = 0;
}
static void carjack(int i, int park) {
  if (i == -2) {   // назад в свою: машина игрока — машина уровня на месте оставленной, угнанная уходит
    if (!mvSummon()) return;
    resumeOwnCar();
    if (smkOn) { char s[80]; snprintf(s, sizeof s, "CARJACK back t=%.1f\n", smkT); smkWrite(s); }
    return;
  }
  if (!oldCarOn && !mvForcedCar()) parkOwnCar(&car, &cv);   // из своей — она остаётся стоять
  const Car *k = trafficCarAt(&traffic, i, park); const TrModel *M = &traffic.model[trafficCarModelAt(&traffic, i, park)];
  char lc[16]; int n = 0; for (const char *q = M->name; *q && n < 15; q++) lc[n++] = (*q >= 'A' && *q <= 'Z') ? (char)(*q + 32) : *q; lc[n] = 0;
  CarParams P; carParamsFor(lc, &P);
  { const CarParams *tp = &traffic.params[trafficCarModelAt(&traffic, i, park)]; if (tp->seatData) { memcpy(P.seat, tp->seat, sizeof P.seat); P.seatAx = tp->seatAx; P.seatAz = tp->seatAz; P.doorX = tp->doorX; P.doorZ = tp->doorZ; P.seatData = 1; } }   // №666: сиденье модели трафика (TRF1 offSeat)
  P.halfW = M->halfW; P.halfL = M->halfL; P.bodyOfs = M->bodyOfs; P.wheelX = M->wheelX; P.wheelF = M->wheelF; P.wheelR = M->wheelR;
  P.wheelRad = M->wheelRad; P.bodyLo = M->bodyLo; P.bodyHi = M->bodyHi;
  if (M->camMag > 1.f && M->camElev > .1f) { P.camElev = M->camElev; P.camMag = M->camMag; }   // правка 198: камера своей машины (автобус дальше)
  float x = k->x, y = k->y, z = k->z, h = k->h;
  mvCarjack(M->name, &P, M->lift, x, y, z, h); trafficTake(&traffic, i, park);
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "CARJACK model=%s park=%d pos=%.1f,%.1f car=%.1f,%.1f t=%.1f\n", M->name, park, x, z, car.x, car.z, smkT); smkWrite(s); }
}
#ifndef HORN_OLD
#define HORN_OLD 0
#endif
static int hornOld;   // правка 413: служебное «flag hornold 1» — гудка нет (красный контроль tests/hover/carsnd.sh)
static int coinNoHide;   // служебное (flag coinstay): отрицательный контроль правки 51 — счёт монет висит всегда, как раньше
static int lbNoDuck;   // служебное (flag lbnoduck): отрицательный контроль правки 55 — разговор без шин «Letterbox»
// правка Роба 113 (30.09: «пешком у машины камера срезает кузов»): машины — не стены коллизии, homerCamera их не видит. Пешком в
// городе ближняя плоскость подходит к ближайшей машине (своя, оставленная, трафик, машины миссий): 0.8 × расстояния от глаза до
// кузова, не ближе FOOT_CAR_NEAR (16-бит глубина: ближе — мерцание вдали)
#define FOOT_CAR_NEAR .35f
static float carNearEye(void) {
  float best = 1e9f, d;
  d = hypotf(car.x - eyeX, car.z - eyeZ) - car.p.halfL; if (d < best) best = d;
  if (oldCarOn) { d = hypotf(oldCar.x - eyeX, oldCar.z - eyeZ) - oldCar.p.halfL; if (d < best) best = d; }
  if (haveTraffic) { int park; if (trafficNearest(&traffic, eyeX, eyeZ, 4.f, &park, &d) >= 0 && d < best) best = d; }
  const MsnCars *m = mvCars();
  if (m) for (int i = 0; i < m->n; i++) if (m->c[i].on) { d = hypotf(m->c[i].car.x - eyeX, m->c[i].car.z - eyeZ) - m->c[i].car.p.halfL; if (d < best) best = d; }
  return best;
}
// №500 B2: разложение ближней плоскости по причинам (причины пишутся счётчиками PHSUB near_*: сколько кадров с near < NEAR_Z и из-за чего: машина у глаза, камера у стены/дерева,
// зона, свободное место перед глазом, разговор). useCar = 0 — ближняя плоскость без правила «машина у глаза» (что нужно городу): решает, можно ли городу жить на NEAR_Z
static int frameNearWhy;   // последняя причина минимума: 0 нет (NEAR_Z), 1 база (камера пешком/за машиной у стены), 2 машина у глаза, 3 зона (рельс), 4 свободное место (camSafe), 5 разговор
static void carNearEyeSplit(float *own, float *other) {   // B2: машина игрока и оставленная (own) против трафика и машин миссий (other), как carNearEye
  float o = 1e9f, t = 1e9f, d;
  d = hypotf(car.x - eyeX, car.z - eyeZ) - car.p.halfL; if (d < o) o = d;
  if (oldCarOn) { d = hypotf(oldCar.x - eyeX, oldCar.z - eyeZ) - oldCar.p.halfL; if (d < o) o = d; }
  if (haveTraffic) { int park; if (trafficNearest(&traffic, eyeX, eyeZ, 4.f, &park, &d) >= 0 && d < t) t = d; }
  const MsnCars *m = mvCars();
  if (m) for (int i = 0; i < m->n; i++) if (m->c[i].on) { d = hypotf(m->c[i].car.x - eyeX, m->c[i].car.z - eyeZ) - m->c[i].car.p.halfL; if (d < t) t = d; }
  *own = o; *other = t;
}
// №861 (Роб 22.10: «убери обрезание асфальта, обрезание пола — ломает текстуру; прохождение в пол запретить, даже при ударе»): ближняя плоскость
// по полу — если нижний край кадра на её дистанции ниже опоры коллизии + FN_CLEAR (под краем, по трём точкам), плоскость ближе ровно настолько,
// чтобы край был над полом; не ближе FN_MIN (16-бит глубина). Для всех камер города, в том числе пролётов из данных (их путь не двигаем; у PS2
// ближняя плоскость мала, у порта 1,5 м — низкий авторский путь срезал асфальт у низа кадра). Считается раз в кадр (floorNearFrame). FLOOR_NEAR861_OLD=1 — как было
#ifndef FLOOR_NEAR861_OLD
#define FLOOR_NEAR861_OLD 0
#endif
#define FN_CLEAR .40f
#define FN_MIN .30f
static float nearForceV;   // №863 служебно: flag nearforce X (0 — выкл)
static float floorNearV = 1e9f, floorNearMin = 1e9f; static unsigned floorNearN, floorEyeN; static int floorNearOld = FLOOR_NEAR861_OLD;   // floorNearN — кадров с правилом; floorNearOld — служебно flag floornearold 1
static float frameNearEx(int inR, int fly, int useCar) {
  frameNearWhy = 0;
  if (inR) return (!roomCamOn && homer.camNear > 0.f) ? fminf(ROOM_NEAR, homer.camNear) : ROOM_NEAR;   // комната: камера пешком у стены — ближе
  float n = (onFoot() && homer.camNear > 0.f) ? fminf(nearZ, homer.camNear) : fminf(nearZ, chaseNear);   // Ф2: NEAR_Z (была 0.7); у кузова/стены — ближе (homerCamera)
  if (n < nearZ) frameNearWhy = 1;
  if (useCar && onFoot() && !fly) { float dc = carNearEye() * .8f; if (dc < n) { n = fmaxf(FOOT_CAR_NEAR, dc); frameNearWhy = 2; } }   // правка 113: машина рядом с глазом
  if (zoneCamOn && onFoot() && !fly && zoneNear < n) { n = zoneNear; frameNearWhy = 3; }   // камера зоны (рельс) бывает в 0.5 м от Гомера
  if (!fly && camSafeNearV < n) { n = camSafeNearV; frameNearWhy = 4; }   // правка 331: не дальше свободного места перед глазом (camSafeFrame)
  if (talkCamOn && !fly && talkNear < n) { n = talkNear; frameNearWhy = 5; }   // правка 40: крупный план разговора — голова слушающего в 1.3 м, затылок не срезать
  if (!floorNearOld && floorNearV < n) { n = floorNearV; frameNearWhy = 6; }   // №861: пол у нижнего края кадра
  if (nearForceV > 0.f) { n = nearForceV; frameNearWhy = 4; }   // №863 служебно (flag nearforce X): ближняя плоскость числом — проверка среза пола на всех значениях
  return n;
}
static float frameNear(int inR, int fly) {   // ближняя плоскость кадра: город NEAR_Z; комната ROOM_NEAR; у стены — ближе; разговор — не режет собеседников
  return frameNearEx(inR, fly, 1);
}
// №861: сверка с камерами PS2-диска (954 камеры в art/*.p3d, чанк Camera): ближняя плоскость оригинала 0,1 м (776 камер), у камер миссий, гонок и
// будки 0,01 м; у порта город 1,5 м (16-бит глубина). Низкий глаз (облёт, камера из данных, подъём защиты №837 упёрся) срезал асфальт у низа кадра.
// Раз в кадр: нижний край кадра (центр и углы) на NEAR_Z над опорой коллизии меньше FN_CLEAR — ближняя плоскость floorNearV, ровно чтобы край был над полом
static void floorNearFrame(int inR, int flyb, float ex, float ey, float ez, float fx, float fy, float fz, float fovY) {
  floorNearV = 1e9f;
  if (smkOn) { static float lt_; if (smkT - lt_ >= 1.f || smkT < lt_) { lt_ = smkT; if (floorNearN || floorEyeN) { char l_[140]; snprintf(l_, sizeof l_, "FLOORNEAR t=%.1f sc=%s frames=%u near_min=%.2f eye_fix=%u old=%d\n", smkT, smkScene, floorNearN, floorNearMin < 1e8f ? floorNearMin : 0.f, floorEyeN, floorNearOld); smkWrite(l_); }
      floorNearN = floorEyeN = 0; floorNearMin = 1e9f; } }
  if (floorNearOld || inR || flyb || !haveCol) return;
  float f[3] = {fx, fy, fz}, fl = sqrtf(fx * fx + fy * fy + fz * fz); if (fl < 1e-4f) return;
  for (int k = 0; k < 3; k++) f[k] /= fl;
  float r[3], u[3]; csBasis(f, r, u);
  float tv = tanf(fovY * (GU_PI / 360.f)), th = tv * ASPECT, dy = f[1] - tv * u[1];   // нижний край кадра идёт по f − tv·u, его вертикаль dy
  if (dy >= -1e-3f) return;   // край не опускается — пол не срезается
  const ColWorld *w = onFoot() && G.pHomerWorld ? G.pHomerWorld : &world;
  float e[3] = {ex, ey, ez}, gmax = -1e30f;
  for (int i = 0; i < 3; i++) { float sx = (float)(i - 1) * th, p[3], g;
    for (int k = 0; k < 3; k++) p[k] = e[k] + NEAR_Z * (f[k] - tv * u[k] + sx * r[k]);
    if (colGround(w, p[0], p[2], ey + .1f, p[1] - 4.f, &g, 0) && g > gmax) gmax = g; }
  if (gmax < -1e29f || ey + NEAR_Z * dy >= gmax + FN_CLEAR) return;
  float z = (ey - gmax - FN_CLEAR) / -dy;
  floorNearV = z < FN_MIN ? FN_MIN : z; floorNearN++; if (floorNearV < floorNearMin) floorNearMin = floorNearV;
}
// №861: глаз камеры над полом после тряски удара (collShake идёт после защиты №837): опора коллизии под глазом + FN_EYE_CLEAR
#define FN_EYE_CLEAR .35f
static void floorEyeClamp(float *ey, float ex, float ez, float ey0) {   // ey0 — глаз до тряски: опора не выше него (мост над камерой — не пол), подъём не выше него
  const ColWorld *w = onFoot() && G.pHomerWorld ? G.pHomerWorld : &world; float g;
  if (!colGround(w, ex, ez, ey0 + .05f, *ey - 8.f, &g, 0)) return;
  float lim = g + FN_EYE_CLEAR; if (lim > ey0) lim = ey0;
  if (*ey < lim) { *ey = lim; floorEyeN++; }
}
// Правка 331: глаз камеры игры (за машиной, пешком, переходы) — никогда не внутри (viewer/camsafe.h). Глаз годен, если от опоры
// камеры (над крышей своей машины / голова Гомера) до него нет мира (кузова — правило 284 у камеры машины; от людей и машин между
// опорой и глазом камера не уворачивается, как оригинал: запись PS2 17:16,7 — автобус проходит у края кадра), глаз не в теле, не ниже
// опоры коллизии под ним на CS_FLOOR, не ближе CS_BODY_CLEAR к опоре и вокруг него хватает места хотя бы на CS_NEAR_MIN ближней плоскости. Негодный — ближе к опоре по
// лучу (до препятствия минус зазор), не хватило — выше (над препятствием / над опорой). Поправка наружу — сразу (далёкое место —
// перелётом CS_GLIDE, не срезом), назад — плавно (CS_RELAX, ровно так, чтобы глаз оставался годным): без рывков
#define CS_FLOOR .45f        // м над опорой коллизии под глазом (как CAM_GROUND_MIN правки 284)
#define CS_GAP .3f           // м от препятствия до глаза по лучу от опоры
#define CS_BODY_CLEAR 1.0f   // м: ближе к опоре камеры глаз уже «в теле» (голова Гомера, кузов) — поднимается
#define CS_RELAX 3.f         // 1/с: возврат поправки, когда препятствие ушло
#define CS_GLIDE 25.f        // м/с: перелёт глаза к далёкому годному месту вместо среза
static float csOfs[3], csLastEd[3]; static int csOfsOn, csSceneOk; static float csSceneE[3];
static int csFloorY(const float e[3], const float P[3], float *gy) {   // опора коллизии под глазом (не выше max(глаз, опора) + 0,3)
  return colGround(csScene.w, e[0], e[2], fmaxf(e[1], P[1]) + .3f, e[1] - 60.f, gy, 0);
}
static float csNfE[3], csNfF[3], csNfTan = -1.f, csNfV;   // свободная глубина последнего годного глаза (кадр переиспользует)
// облёт будки (TEST19): глаз годен, только если перед ним свободно не меньше CAM_PHONE_CLEAR (изгородь вплотную занимала кадр); у остальных камер — CS_NEAR_MIN
static float csNearMinCur = CS_NEAR_MIN;
static int csEyeOk(const float e[3], const float P[3], const float lk[3], float tanV, float tanH) {
  float d[3] = {e[0] - P[0], e[1] - P[1], e[2] - P[2]}, f[3] = {lk[0] - e[0], lk[1] - e[1], lk[2] - e[2]}, gy;
  if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < CS_BODY_CLEAR * CS_BODY_CLEAR) return 0;
  if (csFloorY(e, P, &gy) && e[1] < gy + CS_FLOOR - .01f) return 0;
  if (csRayFrac(&csScene, P, e, CS_PE) < .999f || csInBody(&csScene, e, .15f)) return 0;
  float l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (l < 1e-3f) return 0;
  for (int k = 0; k < 3; k++) f[k] /= l;
  float nf = csNearFree(&csScene, e, f, tanV, tanH, NEAR_Z);
  memcpy(csNfE, e, 12); memcpy(csNfF, f, 12); csNfTan = tanV; csNfV = nf;
  return nf >= csNearMinCur;
}
static void csPull(float e[3], const float P[3]) {   // глаз — на луче от опоры до препятствия минус зазор; не ниже опоры коллизии + CS_FLOOR
  float gy; if (csFloorY(e, P, &gy) && e[1] < gy + CS_FLOOR) e[1] = gy + CS_FLOOR;
  float L = sqrtf((e[0] - P[0]) * (e[0] - P[0]) + (e[1] - P[1]) * (e[1] - P[1]) + (e[2] - P[2]) * (e[2] - P[2]));
  float f0 = csRayFrac(&csScene, P, e, CS_PE);   // мир; кузова и люди между опорой и глазом не мешают (оригинал от них не уворачивается), лишь бы глаз был не в них
  if (f0 < .999f && L > 1e-3f) { float k = fmaxf(0.f, f0 * L - CS_GAP) / L; for (int i = 0; i < 3; i++) e[i] = P[i] + (e[i] - P[i]) * k; }
}
// Негодный глаз: кандидаты (притянутый к опоре; он же выше на 0,5–2 м; над опорой; повёрнутый вокруг опоры на ±23°/±46°; прошлый
// глаз кадра) — по возрастанию «рывка» (до прошлого глаза + до желаемого); берётся первый годный. Годного нет — с наибольшим местом
// перед глазом из тех, что не за препятствием и не в теле. Тяжёлое (кандидаты) — только в кадрах, где желаемый глаз негоден
#ifndef CAMERA_SUBJECT_HANDOFF_OLD
#define CAMERA_SUBJECT_HANDOFF_OLD 0
#endif
static float csPrev[3], csPrevRel[3], csPrevP[3]; static int csPrevOn, csPrevCar;
#ifndef CAM_XFER_OLD
#define CAM_XFER_OLD 0
#endif
#define CAM_XFER_M 100.f   // м: опора ушла от прошлой дальше — перенос
static int camXferOld = CAM_XFER_OLD;
static float camKickD, camKickY;   // служебное: flag camkick D dy
static int csGeomBlocked; unsigned camJumpN; float camJumpMax;   // №916б: csGeomBlocked — сколько кадров подряд итоговый луч «опора → глаз» упирается в геометрию; camJump* — рывки глаза > CAM_JUMP_M за кадр (строка CAMJUMP)
static void csHeldEye(const float P[3],const float previous[3],const float relative[3],int sameSubject,float hold[3]) {
  for(int k=0;k<3;k++)hold[k]=(CAMERA_SUBJECT_HANDOFF_OLD || sameSubject)?P[k]+relative[k]:previous[k];
}
static int csWhyPick, csWhyGlide, csDesiredOk = 1, csDesiredN; static float csWhyEd[3];   // csDesiredOk — №872: желаемый глаз этого кадра свободен; csDesiredN — номер кадра проверки   // служебное (строка CAMJUMP): откуда глаз этого кадра
// №916в (рывок глаза при ходьбе, L1 x 223 z 168, t 73 с: человек шёл в 1,2 м за Гомером, ровно на линии «опора → глаз»; перелёт к желаемому глазу на 2,7 м
// шёл ~1 с, каждый промежуточный глаз упёрся в человека, перелёт сорвался и глаз прыгнул на 2,6 м за кадр). Правило для всего семейства: луч «опора → промежуточный
// глаз» перелёта не считает людей (цилиндры, маска 64): человек сам идёт мимо, оригинал от него не уворачивается; стены, геометрия, кузова и ближняя плоскость
// (в том числе человек у края кадра) — как были, желаемый и выбранный глаз тоже проверяются целиком. CAM_SOFT_PPL_OLD=1 / «flag camsoftpplold 1» — как было
#ifndef CAM_SOFT_PPL_OLD
#define CAM_SOFT_PPL_OLD 0
#endif
static int camSoftPplOld = CAM_SOFT_PPL_OLD, csSoftMask = -1;   // csSoftMask < 0 — маска луча по умолчанию (CS_PE); перелёт камеры ставит свою
// №924 (цена камеры в езде, тур TEST58 F_L4_CR/F_L7_CR: за кадр до трёх обсчётов ближней плоскости, 110 мкс на Маке каждый): промежуточный глаз перелёта, ставший глазом кадра, уже проверен csSoftOk —
// ответ (свободная глубина) запоминается, и camSafeFrame берёт его вместо повторного обсчёта (глаз, взгляд и угол те же, как у кэша csEyeOk; кадр правила глаза тот же). CAM_NFREUSE_OLD=1 / flag camnfreuseold 1 — как было
#ifndef CAM_NFREUSE_OLD
#define CAM_NFREUSE_OLD 0
#endif
#ifndef CAM_NFREUSE_RED
#define CAM_NFREUSE_RED 0   // красный контроль гейта camf924: запомненная глубина вдвое меньше — near в CAMF расходится
#endif
static int camNfReuseOld = CAM_NFREUSE_OLD, csSfOn; static float csSfE[3], csSfF[3], csSfTan, csSfV;
static int csSoftOk(const float q[3], const float P[3], const float lk[3], float tanV, float tanH) {   // промежуточный глаз перелёта
  float gy, f[3] = {lk[0] - q[0], lk[1] - q[1], lk[2] - q[2]}, l = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
  if (l < 1e-3f || csInBody(&csScene, q, .15f) || (csFloorY(q, P, &gy) && q[1] < gy + CS_FLOOR - .01f) || csRayFrac(&csScene, P, q, csSoftMask < 0 ? CS_PE : csSoftMask) < .999f) return 0;
  for (int k = 0; k < 3; k++) f[k] /= l;
  float nf_ = csNearFree(&csScene, q, f, tanV, tanH, NEAR_Z);
  if (!camNfReuseOld) { memcpy(csSfE, q, 12); memcpy(csSfF, f, 12); csSfTan = tanV; csSfV = CAM_NFREUSE_RED ? nf_ * .5f : nf_; csSfOn = 1; }
  return nf_ >= csNearMinCur;
}
// A clear destination can be separated from the last eye by a wall or a
// road edge. Walk around it in small clear steps instead of timing out into
// a cut. Every candidate and its travelled segment must remain clear.
static int csGlideAround(const float hold[3], const float end[3], const float P[3], const float lk[3],
                         float tanV, float tanH, float step, float out[3]) {
  float d[3] = {end[0]-hold[0],end[1]-hold[1],end[2]-hold[2]}, l = sqrtf(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
  if (l < 1e-3f || step <= 0.f) return 0;
  for (int k=0;k<3;k++) d[k]/=l;
  float best = 1e30f; int found=0;
  for (int i=0;i<12;i++) {
    float dir[3], q[3];
    if (i<8) {
      float a=(i&1?-1.f:1.f)*(i<4?.785398163f:1.570796327f), ca=cosf(a),sa=sinf(a);
      dir[0]=d[0]*ca+d[2]*sa;dir[2]=d[2]*ca-d[0]*sa;dir[1]=d[1]+(i%4<2?0.f:.7f);
    } else { dir[0]=d[0];dir[2]=d[2];dir[1]=(i&1?-1.f:1.f)*(i<10?1.f:2.f); }
    float dl=sqrtf(dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]);if(dl<1e-3f)continue;
    for(int k=0;k<3;k++)q[k]=hold[k]+dir[k]*(step/dl);
    float cost=0.f;for(int k=0;k<3;k++){float v=q[k]-end[k];cost+=v*v;}
    if(cost>=best || csRayFrac(&csScene,hold,q,1)<.999f || !csSoftOk(q,P,lk,tanV,tanH))continue;
    best=cost;memcpy(out,q,12);found=1;
  }
  return found;
}
#ifndef CAM_JUMP_M
#define CAM_JUMP_M 1.5f   // глаз сместился больше этого за кадр на одной опоре — «рывок»
#endif
static void camSafeEye(float cp[3], const float lk[3], float fovy, float dt) {
  float P[3] = {camPivot[0], camPivot[1], camPivot[2]}, ed[3] = {cp[0], cp[1], cp[2]}, tanV = tanf(fovy * (GU_PI / 360.f)), tanH = tanV * ASPECT;
  csSfOn = 0;
  camSafeScene(ed, 14.f); memcpy(csSceneE, ed, 12); csSceneOk = 1;
#if CAMERA_SUBJECT_HANDOFF_OLD
  if ((csOfsOn || csPrevOn) && (fabsf(ed[0] - csLastEd[0]) > 8.f || fabsf(ed[1] - csLastEd[1]) > 8.f || fabsf(ed[2] - csLastEd[2]) > 8.f ||
                                fabsf(P[0] - csPrevP[0]) > 4.f || fabsf(P[1] - csPrevP[1]) > 4.f || fabsf(P[2] - csPrevP[2]) > 4.f)) csOfsOn = csPrevOn = 0;
#else
  if ((csOfsOn || csPrevOn) && csPrevCar==camPivotCar && (fabsf(P[0] - csPrevP[0]) > 4.f || fabsf(P[1] - csPrevP[1]) > 4.f || fabsf(P[2] - csPrevP[2]) > 4.f)) csOfsOn = csPrevOn = 0;
  // №923 (перенос героя/машины на 700 м и дальше — RESET CAR, респавн, старт миссии: кадр 330 мс, из них 316 мс — перелёт от прошлого глаза; при смене опоры герой <-> машина прошлый
  // глаз не сбрасывался, перелёт мерил лучи «опора -> промежуточный глаз» длиной в сотни метров через незагруженный город и всё равно кончался прыжком к желаемому глазу).
  // Опора ушла от прошлой больше чем на CAM_XFER_M — это перенос, а не смена героя на машину: прошлый глаз забыт, камера сразу на новом месте (результат тот же, что давал перелёт).
  // CAM_XFER_OLD=1 / flag camxferold 1 — как было
  if (!camXferOld && (csOfsOn || csPrevOn) && (fabsf(P[0] - csPrevP[0]) > CAM_XFER_M || fabsf(P[1] - csPrevP[1]) > CAM_XFER_M || fabsf(P[2] - csPrevP[2]) > CAM_XFER_M)) csOfsOn = csPrevOn = 0;
#endif
  if (camKickD != 0.f) {   // служебное (гейт tests/eyejump, flag camkick D dy): прошлый глаз сдвинут от желаемого на D м вбок и на dy вверх — начало перелёта в известном месте
    float dx = ed[0] - P[0], dz = ed[2] - P[2], l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) { dx = 0.f; dz = 1.f; l = 1.f; }
    csPrev[0] = ed[0] - dz / l * camKickD; csPrev[1] = ed[1] + camKickY; csPrev[2] = ed[2] + dx / l * camKickD; memcpy(csPrevP, P, 12); for (int i = 0; i < 3; i++) csPrevRel[i] = csPrev[i] - P[i];
    csPrevCar = camPivotCar; csPrevOn = 1; csOfsOn = 1; for (int i = 0; i < 3; i++) csOfs[i] = csPrev[i] - ed[i]; camKickD = 0.f; }
  { static int flipSeen; if (homerCamFlipN != flipSeen) { flipSeen = homerCamFlipN; if (!homerCamFlipOld) csOfsOn = csPrevOn = 0; } }   // №916в: перескок камеры пешком (PS2) — одна смена места, без перелёта от старого глаза
  memcpy(csLastEd, ed, 12);
  float e[3] = {ed[0], ed[1], ed[2]}, hold[3];   // hold — прошлый глаз, сдвинутый вместе с опорой
  csWhyPick = -2; csWhyGlide = 0; memcpy(csWhyEd, ed, 12);
  if (csPrevOn) csHeldEye(P,csPrev,csPrevRel,csPrevCar==camPivotCar,hold);
  if (!camCrossOld) {   // №916б: опора прошла сквозь поверхность без коллизии — её воксель в CG_CROSS_R больше не «стена» для глаза
    if (!csPrevOn || csPrevCar != camPivotCar) cgCrossN = 0;
    else {
      for (int i = 0; i < cgCrossN;) { float dx = P[0] - cgCross[i][0], dy = P[1] - cgCross[i][1], dz = P[2] - cgCross[i][2]; if (dx * dx + dy * dy + dz * dz > 30.f * 30.f) { memcpy(cgCross[i], cgCross[--cgCrossN], 12); cgCrossNew = 1; } else i++; }
      if (csGeomBlocked <= 3) {   // луч был свободен до 3 кадров назад (ответ сетки может быть кэшем прошлого кадра)
        float fh = csRayFrac(&csScene, P, hold, 8), L_ = sqrtf((hold[0] - P[0]) * (hold[0] - P[0]) + (hold[1] - P[1]) * (hold[1] - P[1]) + (hold[2] - P[2]) * (hold[2] - P[2]));
        if (fh < .999f && fh * L_ <= 2.6f && csRayFrac(&csScene, P, hold, 5) >= .999f) {   // мир и тела не мешают, мешает только видимая геометрия рядом с опорой
          float *h_ = cgCross[cgCrossN < CG_CROSS_N ? cgCrossN++ : 0]; for (int k = 0; k < 3; k++) h_[k] = P[k] + (hold[k] - P[k]) * (fh + .02f); cgCrossNew = 1;
          if (smkOn && camLogOn) { char s_[160]; snprintf(s_, sizeof s_, "CAMCROSS t=%.3f at=%.2f,%.2f,%.2f d=%.2f n=%d\n", smkT, h_[0], h_[1], h_[2], fh * L_, cgCrossN); smkWrite(s_); } } } }
  }
  CPT0; int ok0_ = csEyeOk(e, P, lk, tanV, tanH); CPT(2); csDesiredOk = ok0_; csDesiredN++;
  if (!ok0_) {
    enum { NC = CAMERA_SUBJECT_HANDOFF_OLD ? 17 : 37 }; float c[NC][3], cost[NC]; int n = 0;
    float pl[3] = {ed[0], ed[1], ed[2]}; csPull(pl, P);
    memcpy(c[n++], pl, 12);
    for (int k = 1; k <= 4; k++) { float q[3] = {pl[0], pl[1] + .5f * k, pl[2]}; csPull(q, P); memcpy(c[n++], q, 12); }
    // TEST19 (скамья L2): низкое препятствие между глазом и героем — глаз поднимается над ним, а не подъезжает к герою (у оригинала у препятствия ближе и выше).
    // Желаемый глаз + 0,5…4 м по вертикали, без подтягивания: берётся, если годен — дешевле всех притянутых. CAM_RAISE_OLD=1 / flag camraiseold 1 — прежнее
    if (!camRaiseOld) for (int k = 1; k <= 12 && !CAMERA_SUBJECT_HANDOFF_OLD; k++) { float q[3] = {ed[0], ed[1] + .5f * k, ed[2]}; memcpy(c[n++], q, 12); }
    float bx = ed[0] - P[0], bz = ed[2] - P[2], bl = sqrtf(bx * bx + bz * bz); if (bl > 1e-3f) { bx /= bl; bz /= bl; } else { bx = 0.f; bz = -1.f; }
    for (int k = 0; k <= 3; k++) { float q[3] = {P[0] + bx * .4f, P[1] + CS_BODY_CLEAR + .2f + .5f * k, P[2] + bz * .4f}; csPull(q, P); memcpy(c[n++], q, 12); }
    for (int k = 0; k < 4; k++) {   // вдоль стены: тот же глаз, повёрнутый вокруг опоры
      float a = (k & 1 ? -1.f : 1.f) * (k < 2 ? .4f : .8f), ca = cosf(a), sa = sinf(a), dx = ed[0] - P[0], dz = ed[2] - P[2];
      float q[3] = {P[0] + dx * ca + dz * sa, ed[1], P[2] - dx * sa + dz * ca}; csPull(q, P); memcpy(c[n++], q, 12); }
    if (csPrevOn) { memcpy(c[n++], hold, 12); memcpy(c[n++], csPrev, 12); }
    if (csPrevOn && !CAMERA_SUBJECT_HANDOFF_OLD) {
      // A nearby escape around the previous eye can stay clear without the
      // distant inward cut produced by pulling only the new desired eye.
      float f[3]={lk[0]-hold[0],lk[1]-hold[1],lk[2]-hold[2]},r[3],u[3];
      float len=sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
      if(len>1e-3f) {
        for(int k=0;k<3;k++)f[k]/=len;csBasis(f,r,u);
        for(int j=0;j<8;j++) {
          const float *axis=(j&2)?u:r;float distance=(j<4?.5f:1.f)*(j&1?-1.f:1.f);
          for(int k=0;k<3;k++)c[n][k]=hold[k]+axis[k]*distance;n++;
        }
      }
    }
    const float *ref = csPrevOn ? hold : ed;
    for (int i = 0; i < n; i++) cost[i] = sqrtf((c[i][0] - ref[0]) * (c[i][0] - ref[0]) + (c[i][1] - ref[1]) * (c[i][1] - ref[1]) + (c[i][2] - ref[2]) * (c[i][2] - ref[2])) +
                                          sqrtf((c[i][0] - ed[0]) * (c[i][0] - ed[0]) + (c[i][1] - ed[1]) * (c[i][1] - ed[1]) + (c[i][2] - ed[2]) * (c[i][2] - ed[2]));
    int pick = -1; float bestFree = -1.f; int bestI = 0;
    for (int left = n; left > 0 && pick < 0; left--) {   // по возрастанию цены, первый годный
      int m = -1; for (int i = 0; i < n; i++) if (cost[i] < 1e8f && (m < 0 || cost[i] < cost[m])) m = i;
      if (m < 0) break;
      cost[m] = 1e9f;
      if (csEyeOk(c[m], P, lk, tanV, tanH)) { pick = m; break; }
      float gy, f3[3] = {lk[0] - c[m][0], lk[1] - c[m][1], lk[2] - c[m][2]}, l3 = sqrtf(f3[0] * f3[0] + f3[1] * f3[1] + f3[2] * f3[2]);
      if (l3 > 1e-3f && csRayFrac(&csScene, P, c[m], CS_PE) >= .999f && !csInBody(&csScene, c[m], .15f) && !(csFloorY(c[m], P, &gy) && c[m][1] < gy + CS_FLOOR - .01f)) {
        for (int k = 0; k < 3; k++) f3[k] /= l3;
        float fr = csNearFree(&csScene, c[m], f3, tanV, tanH, NEAR_Z); if (fr > bestFree) { bestFree = fr; bestI = m; } }
    }
    memcpy(e, c[pick >= 0 ? pick : bestFree >= 0.f ? bestI : 0], 12); csWhyPick = pick >= 0 ? pick : 100 + (bestFree >= 0.f ? bestI : 0); CPT(3);
  }
  // Поправка o = e − ed (то, чем правило сдвинуло глаз камеры) меняется от кадра к кадру не быстрее CS_GLIDE: перелёт к далёкому
  // годному месту и возврат на место камеры — не срезом. Уменьшается — ещё и плавно (CS_RELAX). Промежуточный глаз годен — он;
  // нет — прошлый глаз вместе с опорой (камера стоит); и он негоден — сразу новое место (глаз не внутри важнее плавности)
  float o[3] = {e[0] - ed[0], e[1] - ed[1], e[2] - ed[2]};
  float on2 = o[0] * o[0] + o[1] * o[1] + o[2] * o[2];
  float followDelta2=0.f;if(csPrevOn)for(int k=0;k<3;k++){float d=e[k]-hold[k];followDelta2+=d*d;}
  if (csOfsOn || on2 > 1e-6f || (!CAMERA_SUBJECT_HANDOFF_OLD && followDelta2>CS_GLIDE*dt*CS_GLIDE*dt)) {
    // откуда: прошлый глаз, сдвинутый с опорой (не прошлый желаемый + поправка: желаемый сам прыгает — камера машины подъезжает к стене,
    // камера пешком стягивается к голове Гомера)
    float op[3] = {0.f, 0.f, 0.f}, d[3], dl, pn2;
    if (csPrevOn) for (int i = 0; i < 3; i++) op[i] = hold[i] - ed[i]; else if (csOfsOn) memcpy(op, csOfs, 12);
    pn2 = op[0] * op[0] + op[1] * op[1] + op[2] * op[2];
    for (int i = 0; i < 3; i++) d[i] = o[i] - op[i];
    dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    float step = fminf(dl, CS_GLIDE * dt);
    if (!camGlideCapOld) step = fminf(step, CS_GLIDE_FR);
    if (on2 < pn2) step = fminf(step, fmaxf(dl * (1.f - expf(-CS_RELAX * dt)), .02f));   // поправка тает — плавно
    if (dl > 1e-3f && step < dl - 1e-3f) {
      float q[3]; for (int i = 0; i < 3; i++) q[i] = ed[i] + op[i] + d[i] * (step / dl);
      csSoftMask = CS_PE | (camSoftPplOld ? 0 : 64);   // №916в: перелёт не обрывается из-за человека на луче
      if (csSoftOk(q, P, lk, tanV, tanH)) { memcpy(e, q, 12); csWhyGlide = 1; }
      else if (csPrevOn && csGlideAround(hold,e,P,lk,tanV,tanH,step,q)) { memcpy(e,q,12);csWhyGlide=4; }
      else if (csPrevOn && csSoftOk(hold, P, lk, tanV, tanH)) { memcpy(e, hold, 12); csWhyGlide = 2; }   // безопасный глаз продолжает ехать с опорой
      else { csWhyGlide = 3; }
    }
    csSoftMask = -1;
    for (int i = 0; i < 3; i++) o[i] = e[i] - ed[i];
  }
  if (csPrevOn && csPrevCar == camPivotCar) { float jd_ = sqrtf((e[0] - csPrev[0]) * (e[0] - csPrev[0]) + (e[1] - csPrev[1]) * (e[1] - csPrev[1]) + (e[2] - csPrev[2]) * (e[2] - csPrev[2]));   // рывок глаза за кадр (гейт tests/eyejump, поиск по уровням)
    if (jd_ > CAM_JUMP_M) { camJumpN++; if (jd_ > camJumpMax) camJumpMax = jd_; if (smkOn && camLogOn) { char s_[200]; snprintf(s_, sizeof s_, "CAMJUMP t=%.3f d=%.2f P=%.2f,%.2f,%.2f prev=%.2f,%.2f,%.2f e=%.2f,%.2f,%.2f pick=%d glide=%d\n", smkT, jd_, P[0], P[1], P[2], csPrev[0], csPrev[1], csPrev[2], e[0], e[1], e[2], csWhyPick, csWhyGlide); smkWrite(s_); } } }
  if (!camCrossOld) csGeomBlocked = csRayFrac(&csScene, P, e, 8) >= .999f ? 0 : csGeomBlocked + 1;
  CPT(4); memcpy(csOfs, o, 12); csOfsOn = o[0] * o[0] + o[1] * o[1] + o[2] * o[2] > 1e-6f;
  memcpy(cp, e, 12); memcpy(csPrev, e, 12); memcpy(csPrevP, P, 12); for (int i = 0; i < 3; i++) csPrevRel[i] = e[i] - P[i]; csPrevCar=camPivotCar;csPrevOn = 1;
}
// Правка 331: кадр камеры игры — ближняя плоскость по свободному месту перед глазом (csNearFree) и замер «камера внутри» (flag camin)
// служебное (гейт tests/camera/cam_cars.py, правки 284/360): глаз камеры внутри кузова чужой машины (коробка carBox с запасом CAM_CAR_PAD —
// та же, от которой камера уворачивается); своя машина за рулём не в счёт. Не зависит от camcarsold — это замер, а не правило
static int eyeInCar(const float e[3], const Car *k) {
  if (hypotf(k->x - e[0], k->z - e[2]) > 20.f) return 0;
  CarBox cb; memset(&cb, 0, sizeof cb); carBox(k, &cb);
  float dx = e[0] - cb.x, dz = e[2] - cb.z, a = dx * cb.fx + dz * cb.fz, l = -dx * cb.fz + dz * cb.fx;

  return fabsf(a) < cb.hl && fabsf(l) < cb.hw && e[1] > cb.y + cb.lo && e[1] < cb.y + cb.hi;
}
static int camEyeInCar(const float e[3]) {
  int in = 0;
  if (haveTraffic) {
    for (int i = 0; i < traffic.maxCars && !in; i++) if (traffic.car[i].mode != TRM_OFF) in = eyeInCar(e, &traffic.car[i].car);
    for (int i = 0; i < traffic.npark && !in; i++) if (!traffic.park[i].gone) in = eyeInCar(e, &traffic.park[i].car);
  }
  { const MsnCars *m = mvCars(); if (m) for (int i = 0; i < m->n && !in; i++) if (m->c[i].on) in = eyeInCar(e, &m->c[i].car); }
  if (oldCarOn && !in) in = eyeInCar(e, &oldCar);
  return in;
}
#include "render_snapshot.h"
// При смене машины на Гомера две разные опоры нельзя вычитать как движение
// камеры. На самом переходе проверяем глаз в мировых координатах: настоящий
// скачок всё ещё учитывается. На одной опоре сохраняем прежний замер и фильтр телепорта.
static float camEyeStep(const float e[3], const float p[3], const float oldE[3], const float oldP[3], int samePivot) {
  float dp = fabsf(p[0] - oldP[0]) + fabsf(p[1] - oldP[1]) + fabsf(p[2] - oldP[2]);
  if (samePivot && dp >= 3.f) return -1.f;
  float d[3];
  for (int i = 0; i < 3; i++) d[i] = e[i] - oldE[i] - (samePivot ? p[i] - oldP[i] : 0.f);
  return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}
static float nfE[3], nfF[3], nfV = -1.f; static int nfAge = 99;   // кэш ближней плоскости (сброс — levelUnload)
static float csDbgNv = -1.f; static int csDbgHit, csDbgSame, nearLog;   // служебное «flag nearlog 1»: строка CAMNEAR каждые 0,1 с (№727)
static void camSafeFrame(int inR, int fly, float fovy, float ex, float ey, float ez, float fx, float fy, float fz) {
  csDbgNv = -1.f;
  int follow = camFollow && !inR && !fly && haveCol;
  float e[3] = {ex, ey, ez}, f[3] = {fx, fy, fz}, tanV = tanf(fovy * (GU_PI / 360.f)), tanH = tanV * ASPECT;
  camSafeNearV = NEAR_Z;
  if (follow && (!camSafeOld || (smkOn && camInOn)) && !(csSceneOk && fabsf(csSceneE[0] - ex) + fabsf(csSceneE[1] - ey) + fabsf(csSceneE[2] - ez) < 2.f)) camSafeScene(e, 12.f);
  csSceneOk = 0;
  if (follow && !camSafeOld) { unsigned t0_ = sceKernelGetSystemTimeLow(); int same = csNfTan == tanV && fabsf(csNfE[0] - ex) + fabsf(csNfE[1] - ey) + fabsf(csNfE[2] - ez) < 1e-3f &&
      fabsf(csNfF[0] - fx) + fabsf(csNfF[1] - fy) + fabsf(csNfF[2] - fz) < 1e-3f;   // глаз и взгляд те же, что проверило правило глаза
    // TEST24 (цена, Роб №639): впереди было совсем свободно (NEAR_Z), глаз и взгляд почти не сдвинулись — ответ прошлых кадров, пересчёт раз в 4-й кадр. CAM_NEAR_CACHE_OLD=1 / flag camnearcacheold 1 — каждый кадр
    int sameS = !same && csSfOn && csSfTan == tanV && fabsf(csSfE[0] - ex) + fabsf(csSfE[1] - ey) + fabsf(csSfE[2] - ez) < 1e-3f &&
      fabsf(csSfF[0] - fx) + fabsf(csSfF[1] - fy) + fabsf(csSfF[2] - fz) < 1e-3f;   // №924: глаз кадра — промежуточный глаз перелёта, его глубину уже посчитал csSoftOk
    csSfOn = 0;
    int hit_ = !camNearCacheOld && !same && !sameS && nfV >= NEAR_Z - 1e-3f && nfAge < 3 && fabsf(nfE[0] - ex) + fabsf(nfE[1] - ey) + fabsf(nfE[2] - ez) < .5f &&
               fabsf(nfF[0] - fx) + fabsf(nfF[1] - fy) + fabsf(nfF[2] - fz) < .08f;
    float nv_ = same ? csNfV : sameS ? csSfV : hit_ ? nfV : csNearFree(&csScene, e, f, tanV, tanH, NEAR_Z);
    if (hit_) nfAge++; else { nfAge = 0; nfV = nv_; memcpy(nfE, e, 12); memcpy(nfF, f, 12); }
    csDbgNv = nv_; csDbgHit = hit_; csDbgSame = same;
    camSafeNearV = fmaxf(CS_NEAR_MIN, nv_); if (cgProf) cpT[5] += sceKernelGetSystemTimeLow() - t0_;
    camSafeUs += sceKernelGetSystemTimeLow() - t0_;
    // TEST22 (№640, диагональные куски на миг): ближняя плоскость прыгала между кадрами (0,25 ↔ 1,5). Сужается сразу (безопасность), расширяется не быстрее CS_NEAR_RISE за кадр.
    // CAM_NEAR_SMOOTH_OLD=1 / flag camnearsmoothold 1 — как было
    if (!camNearSmoothOld) { if (camSafeNearV > csNearPrev && csNearPrevOk) camSafeNearV = fminf(camSafeNearV, csNearPrev + CS_NEAR_RISE); csNearPrev = camSafeNearV; csNearPrevOk = 1; } }
  else csNearPrevOk = 0;
  // №721 (Роб 17.10: «вижу внутренности своей машины», арест BUSTED): у камер из данных ближняя плоскость стояла на 1,5 м всегда — кузов и стены рядом
  // с такой камерой срезались. Плоскость — по свободному месту, как у камеры игры; глаз не двигаем (его ставит игра). flag camneardataold 1 — как было
  if (!follow && !camNearDataOld && !inR && !fly && haveCol) { unsigned t0_ = sceKernelGetSystemTimeLow(); camSafeScene(e, 12.f);
    camSafeNearV = fmaxf(CS_NEAR_MIN, csNearFree(&csScene, e, f, tanV, tanH, NEAR_Z)); csDbgNv = camSafeNearV; camSafeUs += sceKernelGetSystemTimeLow() - t0_; }
  if (smkOn && carCamLog && follow) { static float last = -1.f;
    if (smkT - last >= .25f || smkT < last) { last = smkT; char line[200];
      snprintf(line, sizeof line, "CAM_EYE t=%.3f sc=%s eye=%.3f,%.3f,%.3f incar=%d carsold=%d safeold=%d\n", smkT, smkScene, ex,ey,ez,camEyeInCar(e),camCarsOld,camSafeOld); smkWrite(line); } }
  if (!smkOn || !camInOn || smkT <= camInT0) return;
  if (!follow) {   // камера из данных (зона, рельс, наезд): замер №602 — глаз за стеной от героя / ближняя плоскость режет мир (справочно: databad)
    camInData++; camInRelOk = 0;
    if (!inR && !fly && haveCol) { camSafeScene(e, 12.f); int fl_ = csProbe(&csScene, e, f, tanV, tanH, frameNear(inR, fly), camPivot); if (fl_ & (CS_IN_EYE | CS_IN_NEAR)) camInDataBad++; }
    return; }
  float n = frameNear(inR, fly); int fl = csProbe(&csScene, e, f, tanV, tanH, n, camPivot);
  if (chase.bumpLift > .01f) { camInBump++; camInBumpMax = fmaxf(camInBumpMax, chase.bumpLift); }
  { float gy_; if (csFloorY(e, camPivot, &gy_) && n > 1e-3f && (ey - gy_) / n > GB_FLOOR_K) { camInGband++; if (camInLog) { char s_[160]; snprintf(s_, sizeof s_, "CAMGB t=%.2f h=%.2f near=%.2f k=%.1f eye=%.1f,%.1f,%.1f foot=%d\n", smkT, ey - gy_, n, (ey - gy_) / n, ex, ey, ez, onFoot()); smkWrite(s_); } } }   // №791: пол под глазом за полосой GE ±2048 (h/n > 7,67) — на PSP выброшен, если кусок не срезан на CPU
  camInFr++; if (fl & 31) camInAny++; for (int k = 0; k < 9; k++) if (fl & (1 << k)) camInFl[k]++;
  { float rel[3] = {ex - camPivot[0], ey - camPivot[1], ez - camPivot[2]};
    float oldE[3] = {camInRel[0] + camInPiv[0], camInRel[1] + camInPiv[1], camInRel[2] + camInPiv[2]};
    if (camInRelOk) { float j = camEyeStep(e, camPivot, oldE, camInPiv, camPivotCar == camInCarPrev);
      if (j > .5f) camInJump++; if (j > camInJmax) camInJmax = j;
      { int sec = (int)smkT; if (sec != jsSec) { if (jsSec >= 0 && jsMax > .5f && jsN < 12) { int l_ = (int)strlen(jsBuf); snprintf(jsBuf + l_, sizeof jsBuf - l_, "%s%d:%.2f", jsN ? "," : "", jsSec, jsMax); jsN++; } jsSec = sec; jsMax = 0.f; } if (j > jsMax) jsMax = j; }
      if (j > .5f && camInLog) { char s_[300]; snprintf(s_, sizeof s_, "CAMJUMP t=%.2f j=%.2f eye=%.2f,%.2f,%.2f prev=%.2f,%.2f,%.2f pivot=%.2f,%.2f,%.2f ed=%.2f,%.2f,%.2f pick=%d glide=%d ofs=%d\n",
        smkT, j, ex, ey, ez, camInRel[0] + camInPiv[0], camInRel[1] + camInPiv[1], camInRel[2] + camInPiv[2], camPivot[0], camPivot[1], camPivot[2], csWhyEd[0], csWhyEd[1], csWhyEd[2], csWhyPick, csWhyGlide, csOfsOn); smkWrite(s_); } }
    memcpy(camInRel, rel, 12); memcpy(camInPiv, camPivot, 12); camInCarPrev = camPivotCar; camInRelOk = 1; }
  if (n < camInNearMin) camInNearMin = n;
  if (camInLog && (fl & CS_IN_VFLOOR)) { char s_[160]; snprintf(s_, sizeof s_, "CAMVF t=%.2f p=%.2f,%.2f,%.2f col=%.2f d=%.2f\n", smkT, csDbgVf[0], csDbgVf[1], csDbgVf[2], csDbgVf[3], csDbgVf[1] - csDbgVf[3]); smkWrite(s_); }
  if (camInLog && (fl & (31 | CS_IN_VFLOOR | CS_IN_VWALL))) { char s[240]; snprintf(s, sizeof s, "CAMINF t=%.2f fl=%d near=%.2f eye=%.2f,%.2f,%.2f dir=%.2f,%.2f,%.2f pivot=%.2f,%.2f,%.2f foot=%d st=%d\n", smkT, fl, n,
                               ex, ey, ez, fx, fy, fz, camPivot[0], camPivot[1], camPivot[2], onFoot(), haveHomer ? (int)homer.state : -1); smkWrite(s); }
}
static int talkRay(const float from[3], const float to[3]) {
  const ColWorld *cw = G.pHomerWorld ? G.pHomerWorld : &world;
  return colRayFrac(cw, from, to, 0) < .999f;
}
// Правка 40: камеры зон города (tools/zonecam_export.py, пакет zonecam-l1.shr; записи — как у камер комнат, viewer/roomcam.c):
// пешком Гомер в триггере камеры — снимает она (у двери дома Симпсонов, дворы, проходы, мостки и кабинет АЭС). Разговор, начатый
// под такой камерой, снимает она же (запись PS2 18:30, кабинет АЭС). Камеры прыжков машины (Data 0) — не в пакете
#define ZONE_CAMS 48
static RoomCam zcam[ZONE_CAMS]; static int nzcam, zoneOff; static RoomCamState zcs = {-1}; static float zoneFovY;
static unsigned char *zcamBuf;   // пакет камер зон: точки рельсов zcam указывают в него (viewer/roomcam.h) — живёт с камерами
static void zonecamLoad(void) {
  nzcam = 0; free(zcamBuf); zcamBuf = 0;
  char fn[256]; levelFile(fn, "zonecam-l%d.shr"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int n = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  unsigned char *b = n > 128 && n < (1 << 20) ? malloc(n) : 0;
  if (b && sceIoRead(f, b, n) == n && !memcmp(b, "ZCM1", 4)) nzcam = roomCamsParse(zcam, ZONE_CAMS, b, (unsigned)n);
  sceIoClose(f); if (nzcam <= 0) { nzcam = 0; free(b); } else zcamBuf = b;
  if (smkOn) { char s[64]; snprintf(s, sizeof s, "ZONECAM load n=%d\n", nzcam); smkWrite(s); }
}
// №408 (Роб 09.10: «камеры, которые снимают сбоку во время прыжка, — как в оригинале на PS2»): камеры зон города для МАШИНЫ —
// пакет carzone-l<N>.shr (tools/zonecam_export.py --car: статичные Data 0 и рельсы у зон прыжка), поведение — viewer/carzone.c по
// записи PS2 (дом с трамплином L1: рельс r1_cam1 сбоку на перелёте, z1_housejump у стены). Гейт tests/camera/carzone_ps2.py.
// Прежнее (камер зон у машины нет) — сборкой с -DCAR_ZONE_CAM_OLD или «flag carzoneoff 1»
#ifndef CAR_ZONE_CAM_OLD
#include "carzone.h"
static RoomCam czcam[CARZONE_CAMS]; static int nczcam, carZoneOff; static CarZoneState czs = {-1};
static unsigned char *czcamBuf;
static void carzoneLoad(void) {
  nczcam = 0; free(czcamBuf); czcamBuf = 0; memset(&czs, 0, sizeof czs); czs.cur = -1;
  char fn[256]; levelFile(fn, "carzone-l%d.shr"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int n = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  unsigned char *b = n > 128 && n < (1 << 20) ? malloc(n) : 0;
  if (b && sceIoRead(f, b, n) == n && !memcmp(b, "ZCM1", 4)) nczcam = roomCamsParse(czcam, CARZONE_CAMS, b, (unsigned)n);
  sceIoClose(f); if (nczcam <= 0) { nczcam = 0; free(b); } else czcamBuf = b;
  if (smkOn) { char s[64]; snprintf(s, sizeof s, "CARZONE load n=%d\n", nczcam); smkWrite(s); }
}
#endif
static int carZoneOn; static float carZoneFovY;   // №408: снимает камера зоны машины (кадр — её угол)
static void msncamLoad(void) {   // камеры уровня gameLevel: у 2–7 — l<N>/msncam.shr (tools/msncam_export.py --level N)
  nmcam = 0; free(mcamBuf); mcamBuf = 0; mcPlay = 0; phoneAnim = 0; phoneAnimT = -1.f;
  char fn[256]; levelFile(fn, "msncam.shr"); SceUID f = sceIoOpen(fn, PSP_O_RDONLY, 0); if (f < 0) return;
  int n = (int)sceIoLseek(f, 0, PSP_SEEK_END); sceIoLseek(f, 0, PSP_SEEK_SET);
  if (n > 0 && n < (1 << 20) && (mcamBuf = memalign(16, n)) && sceIoRead(f, mcamBuf, n) == n) nmcam = msncamParse(mcam, 24, mcamBuf, (unsigned)n);
  sceIoClose(f); if (nmcam < 0) nmcam = 0;
  phoneAnim = msncamFind(mcam, nmcam, "phone", 0);   // правка 166: облёт будки (phonecamera.p3d)
}
// №1041 (Роб, TEST72 на PSP: «наезды подъезжают откуда-то издалека, лагают»): камеры старта с диска начинаются в 40–77 м от места игрока (m0 L1 70 м, m3 77 м; у PS2 те же 70 м, но Роб хочет короткий наезд).
// Теперь анимация идёт не с первого кадра, а с первого, где глаз не дальше CAMIN1041_FAR (30 м) от игрока по горизонтали: остаётся последний отрезок с торможением; у камер, что и так
// стартуют ближе, ничего не меняется. На выходе из наезда игровая камера включается не склейкой (3 тяжёлых кадра пересчёта вида: build 11–13 мс против 5–6 на Маке), а плавным переходом
// от последней позы наезда за CAMIN1041_BLEND секунд (как посадка в машину). CAMIN1041_OLD=1 / файл camin1041old / flag camin1041 0 — как было
#ifndef CAMIN1041_OLD
#define CAMIN1041_OLD 0
#endif
#define CAMIN1041_FAR 30.f
#define CAMIN1041_BLEND .7f
static int camIn1041 = !CAMIN1041_OLD;
static int camBusy1047(void) { return camIn1041 && (mcPlay != 0 || lbOpen > 0.f); }   // №1047: наезд идёт или полосы ещё на экране — пузырь цели ждёт
static void msncamPlay(const MsnCam *c) {
  mcPlay = c; mcT = 0.f;
  if (camIn1041 && c->nfr > 2 && c->fps > 1.f) {
    float ref[3] = {homer.x, homer.y, homer.z}; if (!onFoot()) { ref[0] = car.x; ref[1] = car.y; ref[2] = car.z; }
    float dur = (c->nfr - 1) / c->fps, e[3], l[3], u[3], fy;
    for (float t = 0.f; t < dur; t += 1.f / c->fps) {
      msncamPose(c, t, e, l, u, &fy);
      float dx = e[0] - ref[0], dz = e[2] - ref[2];
      if (dx * dx + dz * dz <= CAMIN1041_FAR * CAMIN1041_FAR) { mcT = t; break; }
    }
  }
  if (smkOn && trOn) { trLeft = TR_FRAMES; trDetail = trDetailOn; trPhase(BBP_MSN); }   // куски по строке держали кадр 5 с (сторож писал STALL) — только по флагу   // след зависания 259: кадры камеры миссии — по фазам в trace.txt
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "MSNCAM start m%d %s phase=%s frames=%u t=%.1f\n", c->mission, c->name, c->phase, c->nfr, smkT); smkWrite(s); }
}
static void msncamStart(int mission) {   // служебное (flag flyin N): камера миссии N по номеру
  for (int i = 0; i < nmcam; i++) if (mcam[i].mission == mission) { msncamPlay(&mcam[i]); return; }
}
static void msncamStartPhase(const char *phase, const char *cam) {   // крючок миссий: камера фазы (имя камеры сверяется с данными)
  const MsnCam *c = msncamFind(mcam, nmcam, phase, cam);
  if (smkOn) { char s[128]; snprintf(s, sizeof s, "MSNCAM req phase=%s cam=%s found=%d play=%d t=%.1f\n", phase ? phase : "-", cam ? cam : "-", c != 0, c && (!svcMode || startCamSvc), smkT); smkWrite(s); }
  if (c && (!svcMode || startCamSvc)) msncamPlay(c);
}
// правка Роба 69 (30.09, «музыка в начале новой игры не та — только пам-пам-пам»): композиция l1_music.rms — старт уровня
// играет событие Level_Intro (короткая туба Гомера), музыка езды (Sunday_Drive_Intro -> Sunday_Drive) — событием
// Sunday_Drive_start / M0_start, т. е. с посадкой в машину (HOMER_EV_IN_CAR). Пешком на старте — туба и атмосфера
static void feStinger(const char *ev);
static void levelAudioStart(void) {
  if (smkOn) { char s[112]; snprintf(s, sizeof s, "LEVELAUDIO onfoot=%d iris=%d newsleft=%.2f t=%.1f\n", haveHomer && onFoot(), transitBusy(), feNewsMusicLeft(), smkT); smkWrite(s); }   // правка 155: гейт; №665: ирис и недоигранный стингер газеты в момент старта
  sndLevelStart(spawnP[0], spawnP[1], spawnP[2]); sndMode(SND_MODE_NORMAL);
  if (haveHomer && onFoot()) { sndMusic(SND_MUS_OFF); feStinger("level_intro"); } else sndMusic(SND_MUS_FREEROAM);
}
// правка 53: машины мира (ракетная Гомера у локатора «rocke») — список viewer/worldcars.h; его же читает tools/msncar_export.py
// (правка 268: модели машин мира — в пакет msncars уровня, иначе машина твёрдая и невидимая)
#include "worldcars.h"
static void worldCarsPlace(void) {
  for (unsigned i = 0; i < sizeof WORLD_CAR_LIST / sizeof WORLD_CAR_LIST[0]; i++)
    if (WORLD_CAR_LIST[i].level == gameLevel) mvWorldCar(WORLD_CAR_LIST[i].name, WORLD_CAR_LIST[i].p, mcCarHeading(WORLD_CAR_LIST[i].h));
}
// правка 334: разовые предметы с монетами (stateprops.h spOnce) всех уровней этой игры — из загруженной записи; текущий уровень
// снимается с sprops при записи (saveCapture). NEW GAME — пусто
static SvOnce onceKeep[SV_LEVELS];
static int propKickLog946(void *u, unsigned i, float dx, float dz);   // №946: main_levels.h
static void newGame(void) {
  ldStep("ng-enter");
#if !TICKET_CONTENT_C3_OLD
  ticketViewNewGame(&ticketView);
#endif
  ldStep("ng-ticket");
  if (rs.room >= 0 || rg.mem) roomLeave();   // 11.2: новая игра — в городе, блоб комнаты в кучу
  ldStep("ng-room");
  if (haveCol) carInit(&car, lvlCarOk == 1 ? &lvlCar : NULL, &world, spawnP[0], spawnP[1], spawnP[2], spawnP[3]);
  if (haveCarVis) carVisReset(&cv);
  ldStep("ng-car");
  if (haveProps) { propsFree(&props); haveProps = propsInit(&props, propSec, propSecN) == 0; props.onKick = propKickLog946; props.kickUser = &G; }
  ldStep("ng-props");
  if (haveCoins) { coinsFree(&coins); haveCoins = haveProps && props.coinType >= 0 && props.ncoin && coinsInit(&coins, props.coinPos, props.ncoin) == 0; coins.dropAuto = 1; }
  ldStep("ng-coins");
  if (haveSP) {
    rfSpReset();   // render_fast О2: радиусы типов — от нового SPR1
    spFree(&sprops); haveSP = haveProps && props.spr && spInit(&sprops, props.spr, props.sprSize) == 0; if (haveSP) boothBase1052(&sprops);   // №1052
    if (haveSP) { for (unsigned k = 0; k < spFxNameCount(&sprops); k++) spFxBind(&sprops, k, fxFind(spFxName(&sprops, k)));
                  sprops.nativeBreakFx = props.typeFx && fxReady(); }
  }
  memset(onceKeep, 0, sizeof onceKeep);   // правка 334: новая игра — все разовые целы (LOAD / RESUME вернёт их в saveApply)
  ldStep("pre-ao"); aoBind(); ldStep("ao");   // правка 244: кадр газеты между шагами новой игры; объекты уровня: кадр 0, живые объёмы COL1 вернутся на место первым aoStep
  if (haveHomer) {
    homerInit(&homer, homerM); homer.solid = homerSolidAll; homer.solidUser = 0;   // стейтпропы + трафик (цепочка)
    if (haveCol) { HomerCar hc; homerCarFrom(&hc, &car, cv.lift); homerSetInCar(&homer, &hc); homerDoor(0, 0.f); }
  }
  ldStep("homer-init");   // №517: шаги разнесены, чтобы журнал называл виновника
  sndBindPump = ldOn ? loadPump : 0;   // №517: кадр газеты изнутри привязки звуков (sound.c sndBindObjects)
  if (haveProps) sndBindObjects(SND_SET_PROPS, props.ninst, props.def, sizeof(PropDef));
  ldStep("bind-props");
  if (haveSP) sndBindObjects(SND_SET_SPROPS, sprops.ninst, sprops.inst, sizeof(SpInst));
  sndBindPump = 0; ldStep("bind-sprops");
  bustFadeT = -1.f; bustPolice = 0;
  hnrHeat = hnrCalm = 0.f; G.hnrLatched = 0; G.hnrBusted = 0.f; hnrBlinkT = -1.f; G.hnrCarSet = 0; ownInit = 0; prevDmg = 0.f; wasDestroyed = 0; spStInit = 0; chase.init = 0; stInit = 0; oldCarOn = 0;
  ldStep("pre-traffic"); if (haveTraffic) trafficReset(&traffic, &world);   // стоящие машины на места, трафик и пешеходы — заново вокруг спавна
  if (ldOn || audioDefer) ldAudio = 1;   // правка 20: под экраном загрузки звучит стингер газеты — атмосфера и музыка уровня после него (loadEnd)
  else levelAudioStart();
  ldStep("pre-msn"); mvCamBusy = camBusy1047; msnartRoom = cellsMakeRoom; mvPump = loadPump; mvHeapRoom = memPrefetchRoom; msnStart();
  // NPC_ASYNC (видео TEST17: «подгрузка NPC роняет FPS до 11»): mvPump означает «идёт экран загрузки», а здесь он ставился один раз и жил до конца игры, поэтому фоновое
  // чтение моделей персонажей (msn_view.c modelFor/modelPrefetch, коммит 85ac58e4) не срабатывало ни в одном туре: все MSNNPC load шли синхронно в кадре (44–82 мс на
  // файл, 40 загрузок за тур, ни одной с async=1). Теперь после старта уровня насос снимается: дальше модели читаются в фоне, NPC до готовности не рисуется.
  // NPC_ASYNC_OLD=1 — прежнее (насос остаётся)
#ifndef NPC_ASYNC_OLD
#define NPC_ASYNC_OLD 0
#endif
  if (!NPC_ASYNC_OLD) mvPump = 0;
  ldStep("msnstart");   // миссии: старт уровня = m0 (Гомер выходит из дома к Мардж), как NEW GAME оригинала
  worldCarsPlace();
  tutCalm = 0.f, tutReset(mvTutorialMode() && (!svcMode || tutSvc));   // подсказки Барта (служебный запуск — с «flag tutorial»)
  // №670 (слово Роба 17.10): при RESUME пройденного сохранения Барт снова учил (settings.bin рядом с EBOOT нет, tutSeen=0). Правило: RESUME / LOAD GAME — подсказок Барта нет
  // совсем, независимо от settings.bin (все отмечены показанными); NEW GAME — как раньше. TUT_RESUME_OLD=1 — прежнее (маска из settings.bin)
  if (svResumeOn) tutSetShown(tutResumeOld ? cfg.tutSeen : (1u << TUT_COUNT) - 1u);
  else if (!svcMode && gameLevel == 1 && !cfg.tutorial) { cfg.tutorial = 1; tutSetUser(1); cfgSave(); }   // правка Роба 70: NEW GAME — Барт снова учит,
      // даже если в прошлой игре подсказки выключили □ «Disable Tutorials» (выключение живёт в settings.bin до новой игры)
  if (haveColl) { collectNewGame(&coll); cardHudT = itemsHudT = waspsHudT = 0.f; shakeG = gagStreamG = -1; }   // В5: карточки, осы, гэги заново
  mcPlay = 0;   // правка 40: облёт улицы к дому Симпсонов на старте уровня — крючок миссий (m0sd ставит игрока, mvStartCam в кадре)
}
static void menuMem(int n) {   // smoke.txt: куча при входе в меню (сверка кругов); наибольший блок — справочно
  if (!smkOn) return;
  unsigned lo = 0, hi = 64u << 20;
  while (hi - lo > (16u << 10)) { unsigned m = (lo + hi) / 2; void *p = malloc(m); if (p) { free(p); lo = m; } else hi = m; }
  int ord[32], no = uiMenuOrder(UI_MENU_MAIN, ord, 32), k = 0;   // пункты сверху вниз, как нарисованы
  char s[256]; k = snprintf(s, sizeof s, "MENU n=%d t=%.1f new=%d used=%u free=%.4f maxblk=%u order=", n, smkT, mainNew,
                            (unsigned)mallinfo().uordblks, smokeHeapFree(), lo);
  for (int i = 0; i < no && k < (int)sizeof s - 8; i++) k += snprintf(s + k, sizeof s - k, i ? ",%d" : "%d", ord[i]);
  k += snprintf(s + k, sizeof s - k, " on=");   // включённые пункты (остальные серые, курсор их пропускает)
  for (int i = 0, c = 0; i < uiMenuCount(UI_MENU_MAIN) && k < (int)sizeof s - 8; i++)
    if (uiMenuEnabled(UI_MENU_MAIN, i)) k += snprintf(s + k, sizeof s - k, c++ ? ",%d" : "%d", i);
  snprintf(s + k, sizeof s - k, "\n");
  smkWrite(s);
}
// Гостиная меню (fe.shr) читается кусками, между ними — LoadingFE.scr (load.shr) с ходом чтения при входе из игры;
// screen -1 — без экрана (титульный стоит сам). feShot — снимок LoadingFE с середины чтения (гейт)
static unsigned feU0; static int nMenu, feScr = -1, feShot; static const char *feScrPack = ""; static void **feFbp;
// №656 (ревью Загрузки): пока комната меню читается за титульным, короткое нажатие Start не должно теряться: каждый кусок чтения опрашивает кнопки и запоминает НОВОЕ нажатие (не зажатое с прежнего экрана),
// titleIdle обрабатывает его сразу после чтения. FE_TITLE_PRELOAD_OLD=1 — как до правки (чтение после нажатия)
static int fePollOn, fePollHit; static unsigned fePollPrev;
static void feHook(unsigned done, unsigned total) {
  if (fePollOn) { SceCtrlData p_; sceCtrlPeekBufferPositive(&p_, 1); if ((p_.Buttons & PSP_CTRL_START) && !(fePollPrev & PSP_CTRL_START)) fePollHit = 1; fePollPrev = p_.Buttons; }
  if (feScr < 0) return;
  scrFrame(feScr, total ? (int)((unsigned long long)done * 1000u / total) : 0, 0, feFbp);
  if (feShot && done * 2 >= total) { feShot = 0; int pr = (int)((unsigned long long)done * 1000u / total);
    while ((sceKernelGetSystemTimeLow() - scrT0) * 1e-6f < feShotAt) scrFrame(feScr, pr, 1, feFbp);
    scrShot("loadingfe", feScrPack, feScr, "-"); }
}
static void feGagLine(const char *l) { char s[176]; int n = (int)strlen(l); if (n > 0 && l[n - 1] == '\n') n--;   // №404
  snprintf(s, sizeof s, "%.*s t=%.1f\n", n, l, smkT); smkWrite(s); }
static void feVoiceEvent(int event, int index, int segment) {
  int readyBefore = feAudioPrepared(); unsigned serialBefore = audioStreamRequestSerial(AUDIO_ST_DIALOG);
  feAudioEvent(event, index, segment);
  if (smkOn) { char s[240]; snprintf(s, sizeof s, "FEAUDIO event=%d index=%d segment=%d name=%s status=%d prepared=%d track=%d req=%u ready_before=%d before_req=%u t=%.1f\n",
    event, index, segment, feAudioName(index, segment), feAudioStatus(), feAudioPrepared(), audioStreamTrack(AUDIO_ST_DIALOG), audioStreamRequestSerial(AUDIO_ST_DIALOG), readyBefore, serialBefore, smkT); smkWrite(s); }
}
static void feLoadShown(int screen, const char *packName, void **fbp) {   // вход в меню: куча (MENU), потом гостиная
  menuMem(++nMenu); feU0 = (unsigned)mallinfo().uordblks;   // куча сверяется без пакетов экранов (они на время показа)
  if (!strcmp(packName, "load")) { scrOpen1065("fe"); scrTvOver = 0; }   // LoadingFE — только на время чтения (№1065: отказ -> место и повтор)
  feScr = screen >= 0 && uiScrReady() ? screen : -1; feScrPack = packName; feFbp = fbp; scrT0 = sceKernelGetSystemTimeLow();
  if (feScr >= 0) scrFrame(feScr, 0, 1, fbp);
  { char fn[256]; sibling(fn, "fe.shr"); feReadHook = feHook; feLoad(fn); feReadHook = 0; }   // гостиная: только на время меню
  feGagLog = smkOn ? feGagLine : 0;   // №404: строки FEGAG сценок меню в smoke.txt
  { char fn[256]; sibling(fn, FE_AUDIO_FILE); int n = feAudioLoad(fn); feGagEvent = feVoiceEvent;
    if (smkOn) { char s[80]; snprintf(s, sizeof s, "FEAUDIO load=%d t=%.1f\n", n, smkT); smkWrite(s); } }
  feScr = -1;
  if (!strcmp(packName, "load")) SCR_CLOSE();
}
// Титульный экран оригинала (frontend.p3d Splash.scr: логотип и «Press START button») — экран ПРОСТОЯ (правка Роба 33,
// видео 28.09 1:45–2:04: «при старте сразу показывать меню; титульный — когда долго никто не играет»). В оригинале
// Splash стоит перед меню, а простой уводит в демо (SLES_518.97: CGuiScreenSplash, CGuiScreenDemo, movies\demo.rmv,
// DemoContext и scripts\missions\level0N\demo.mfk; время простоя — в коде, в данных его нет). Порт: после роликов
// сразу главное меню (гостиная читается под LoadingFE.scr, как при выходе из игры), а титульный показывается, когда в
// меню MENU_IDLE_S секунд не нажимали ни одной кнопки. Любая кнопка — сразу обратно в меню (правка Роба 24: отклик с
// первого нажатия; нажатие не проходит в меню — зажатое считается). Число MENU_IDLE_S — правило порта (вопрос Робу /
// эталону PS2). Гостиная остаётся в памяти, boot.shr открыт только на время титульного.
// Сценарий смоука идёт (press/wait/shot); снимки — со строкой SCRSHOT (tools/screens_check.py). «flag idle N» — простой
// N секунд вместо MENU_IDLE_S; «flag titledeaf» — отрицательный контроль гейта: первое нажатие теряется (прежний класс
// ошибки «START не с первого раза»). Строка TITLE (ключ=значение) — вход, выход, число нажатий и отклик, мс.
// Демо простоя (правка Роба 33, остаток): титульный DEMO_IDLE_S секунд без нажатий — ролик простоя оригинала movies\demo.rmv
// (build/movies/demo.smv, tools/movie_export.py: в окне рамки-телевизора, как все ролики) с надписью «- DEMO -» экрана
// оригинала Demo.scr (frontend.p3d Demo.pag, boot.shr экран 4); ролик кончился — снова титульный, и так по кругу; любая
// кнопка на титульном или в ролике — сразу меню. Данные: SLES_518.97 (строки: CGuiScreenDemo, movies\demo.rmv), Demo.scr
// в frontend.p3d, сам ролик (нарезка игры 64 с, в конце логотип). Внутриигровое демо (DemoContext, level0N\demo.mfk: машина
// ИИ по маршруту d1 за Скиннером, камеры democams.p3d) не перенесено: каким из двух розничная игра отвечает на простой,
// по строкам не видно; в эталоне PS2 простоя нет. DEMO_IDLE_S — правило порта. «flag demoidle N» — N с вместо DEMO_IDLE_S,
// «flag demodeaf» — отрицательный контроль: ролик не слышит кнопок, «flag demonolabel» — без надписи (tests/idle)
#define MENU_IDLE_S 60.f
#ifndef FE_DEMO_PS2_OLD
#define FE_DEMO_PS2_OLD 0
#endif
#ifndef FE_COLD_FLOW_OLD
#define FE_COLD_FLOW_OLD 0
#endif
#ifndef FE_START_INTRO_OLD
#define FE_START_INTRO_OLD 0
#endif
#if FE_DEMO_PS2_OLD
#define DEMO_IDLE_S 20.f
#else
// PS2 V101: no automatic demo during the observed 60s. PORT_RULE: disabled
// until its real native timeout is established; explicit smoke demoidle remains.
#define DEMO_IDLE_S 0.f
#endif
static int feColdRequested, coldTitleWait;
static float menuIdleS = MENU_IDLE_S, demoIdleS = DEMO_IDLE_S; static int titleDeaf, demoDeaf, demoNoLabel, demoTv;
static unsigned titleExitAt;   // время нажатия, которым ушли с титульного/демо (строка MENUBACK: отклик до кадра меню)
static int bootShots;   // служебное: «flag bootshots» — снимки лицензии и BootupLoad
static void feFrameLine(const char *l) {   // правка 293: журнал перелёта меню (frontend3d.c feFrameLog) + смена буфера прошлого
  char s[224]; int n = (int)strlen(l); if (n > 0 && l[n - 1] == '\n') n--;   // кадра: sw=строка луча,опоздала,ждали,guard (swapBufG)
  snprintf(s, sizeof s, "%.*s sw=%d,%d,%d,%d\n", n, l, swapLine, swapLate, swapWait, swapGuard); smkWrite(s);
}
// правка 293 (служебное, гейт tests/fe_guard/gate293.sh и тур TEST 10): swapold 1 — прежняя смена буфера (красный контроль),
// swaplate N — смена опаздывает на N мс (имитация), fecap N — следующий перелёт меню по кадрам в ОЗУ (N кадров, frontend3d.h),
// feglowcut 1 — прежнее: подсветка пункта пропадает в первом кадре перелёта (frontend3d.c FE_GLOW_FADE_S)
static int swapFlag(const SmokeCmd *sc) {
  if (sc->op != 2) return 0;
  if (!strcmp(sc->name, "swapold")) swapOld = sc->a != 0.f;
  else if (!strcmp(sc->name, "swaplate")) swapDelayUs = sc->a > 0.f ? (unsigned)(sc->a * 1000.f + .5f) : 0u;
  else if (!strcmp(sc->name, "fecap")) feCapReq = (int)sc->a;
  else if (!strcmp(sc->name, "feglowcut")) feGlowCut = sc->a != 0.f;   // прежнее: подсветка пункта пропадает за кадр (красный контроль)
  else if (!strcmp(sc->name, "febillold")) feBillOld = sc->a != 0.f;   // правка 325: прежнее — подсветки квадом без среза по полосе (красный)
  else if (!strcmp(sc->name, "fecapfrom")) feCapFrom = (int)sc->a;   // правка 325: fecap с N-го кадра перелёта (тур TEST 11, часть M11)
  else if (!strcmp(sc->name, "fecaptag")) feCapTag = (int)sc->a;     // правка 325: имена кадров fecap fe<N>_*.bmp
  else return 0;
  if (smkOn) { char s[176]; snprintf(s, sizeof s, "SWAPFLAG old=%d late_us=%u cap=%d glowcut=%d n=%u late=%u waited=%u game_late=%u t=%.1f billold=%d capfrom=%d captag=%d\n", swapOld,
                                     swapDelayUs, feCapReq, feGlowCut, swapN, swapLateN, swapWaitN, swapGameLateN, smkT, feBillOld, feCapFrom, feCapTag); smkWrite(s); }
  return 1;
}
static void idleFlag(const SmokeCmd *sc) {   // флаги простоя — где бы ни стоял сценарий (меню, титульный, ролик)
  if (sc->op != 2) return;
  if (swapFlag(sc)) return;
  if (!strcmp(sc->name, "idle")) menuIdleS = sc->a > 0.f ? sc->a : MENU_IDLE_S;   // гейт: простой N с
  else if (!strcmp(sc->name, "demoidle")) demoIdleS = sc->a > 0.f ? sc->a : DEMO_IDLE_S;
  else if (!strcmp(sc->name, "titledeaf")) titleDeaf = 1;   // отрицательный контроль отклика
  else if (!strcmp(sc->name, "demodeaf")) demoDeaf = 1;     // отрицательный контроль: из ролика нет возврата
  else if (!strcmp(sc->name, "demonolabel")) demoNoLabel = 1;
  else if (!strcmp(sc->name, "demotv")) demoTv = 1;         // правка 153: контроль — демо в рамке-телевизоре, как было
  else if (!strcmp(sc->name, "noiris")) transitOff(1);      // правка 154: контроль — выход в меню без ириса
  else if (!strcmp(sc->name, "musold")) musOld = 1;         // правка 155: контроль — звук уровня без удержания
  else if (!strcmp(sc->name, "fehero1021")) feHero1021 = sc->a != 0.f;   // №1021-герои: Гомер меню — путём игры (1) / как в TEST71 (0)
  else if (!strcmp(sc->name, "uitypo")) uiTypoShot((int)sc->a);   // правка 312: снимок фона твик-панели (1 без текста, 2 без интерфейса)
}
// ролик простоя: 1 — прерван кнопкой (*key, *pressAt — время первого нажатия сценария в ролике), 0 — доигран или его нет
static int demoMovie(void **fbp, unsigned *key, unsigned *pressAt, unsigned *presses) {
  char fn[256], s[224]; sibling(fn, "movies/demo.smv");
  int r = movieSystem(1), err = 0;
  Movie *m = r < 0 ? NULL : movieOpen(fn, &err);
  if (!m) { if (r >= 0) movieSystem(0);
    if (smkOn) { snprintf(s, sizeof s, "DEMO none jpeg=%d err=%d t=%.1f\n", r, err, smkT); smkWrite(s); } return 0; }
  sndMaster(0.f);   // музыка меню не звучит поверх ролика (как у роликов гэгов)
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last, n0 = smkPressN, firstN = 0;
  int shot = 0, stop = 0; *key = 0; *pressAt = 0;
  if (smkOn) { snprintf(s, sizeof s, "DEMO on frames=%d t=%.1f\n", 0, smkT); smkWrite(s); }
  while (movieUpdate(m)) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    if (smkPressN != n0 && !firstN) { firstN = smkPressN; *pressAt = smkPressAt; }
    SmokeCmd sc; while (smokeNext(dt, &sc)) idleFlag(&sc);
    if (hit && !demoDeaf) { *key = hit; stop = 1; break; }   // любая кнопка — меню (отрицательный контроль: не слышит)
    if (!!smkShot[0] != shot) {                  // снимок окна: ролик стоит, пока ppsspp_run.sh не снимет
      shot = !!smkShot[0]; moviePause(m, shot);
      if (shot) { snprintf(s, sizeof s, "DEMOSHOT name=%s frame=%d label=%d t=%.1f\n", smkShot, movieFrame(m), !demoNoLabel, smkT); smkWrite(s); }
    }
    // правка Роба 153: демо — без рамки-телевизора (ролик как есть: 4:3 на всю высоту, фон по бокам из того же кадра, без
    // чёрной каймы — tools/movie_export.py CLIP_LAYOUT/CLIP_CROP); «flag demotv» — прежняя рамка (контроль)
    sceGuStart(GU_DIRECT, list); if (demoTv) movieDrawTv(m); else movieDraw(m);   // boot.shr открыт титульным — надпись «- DEMO -»
    if (!demoNoLabel && uiScrReady()) { uiScrDraw(UI_BOOT_DEMO, 0, (t - t0) * 1e-6f, 0); resetState(); sceGuClutMode(GU_PSM_8888, 0, 0xff, 0); }
    sceGuFinish(); sceGuSync(0, 0);
    sceDisplayWaitVblankStart(); *fbp = swapBuf();
  }
  MovieStats st; memset(&st, 0, sizeof st); movieClose(m, &st); movieSystem(0); sndMaster(1.f);
  *presses = smkPressN - n0;
  if (smkOn) { unsigned now = sceKernelGetSystemTimeLow();
    snprintf(s, sizeof s, "DEMO off key=0x%x end=%d frames=%d shown=%d dropped=%d under=%d err=%d presses=%u lat_ms=%.0f ms=%u t=%.1f\n",
             *key, !stop, st.frames, st.shown, st.dropped, st.underruns, st.err, *presses, firstN ? (now - *pressAt) / 1000.f : -1.f,
             (now - t0) / 1000, smkT); smkWrite(s); }
  return stop;
}
static unsigned titleIrisMs;   // правка 154: сколько мс закрывался ирис выхода (строка MENUBACK: отклик = до начала ириса)
static void titleIdle(float idle, void **fbp) {
  char fn[256]; sibling(fn, "boot.shr");
  SceIoStat bst; if (sceIoGetstat(fn, &bst) < 0) return;   // нет пакета — титульного нет, меню стоит
  if (idle > 0.f) transitClose(fbp, "title_in");   // правка 154: простой меню — меню закрывается ирисом, титульный раскрывается
  if (uiScrOpen(fn)) return;
  scrTvOver = 1; scrIris = 1; titleIrisMs = 0;
  uiScrShow(UI_BOOT_SPLASH, UI_T_LOADING, 0); uiScrShow(UI_BOOT_SPLASH, UI_T_PRESS_START, 1);
  scrT0 = sceKernelGetSystemTimeLow(); scrFrame(UI_BOOT_SPLASH, 0, 1, fbp);
  SceCtrlData pad; sceCtrlPeekBufferPositive(&pad, 1);
  unsigned prev = pad.Buttons | smokePad(), last = sceKernelGetSystemTimeLow(), t0 = last, n0 = smkPressN, deaf = titleDeaf;
  unsigned firstAt = 0, firstN = 0, key = 0, tTitle = t0, demos = 0, presses = 0; int fromDemo = 0;
  // №656 (Роб 16.10: после «Press START» только ирис в комнату меню, без прелоадера Itchy & Scratchy): комната меню (fe.shr) читалась ПОСЛЕ нажатия под экраном LoadingFE.
// Теперь при холодном старте она читается за титульным: пока идёт чтение, на каждом куске перерисовывается титульный (экран и пульс «Press START» живые), нажатие в это время не теряется
// (кнопки сняты до чтения). После Start меню уже готово (feReady), остаётся ирис. FE_TITLE_PRELOAD_OLD=1 — чтение после нажатия под LoadingFE, как было
#ifndef FE_TITLE_PRELOAD_OLD
#define FE_TITLE_PRELOAD_OLD 0
#endif
  fePollHit = 0;
  if (!FE_TITLE_PRELOAD_OLD && coldTitleWait && !feReady()) { fePollPrev = prev; fePollOn = 1; feLoadShown(UI_BOOT_SPLASH, "title", fbp); fePollOn = 0; last = sceKernelGetSystemTimeLow(); }
  char logged[24] = "", s[200];
  if (smkOn) { snprintf(s, sizeof s, "TITLE on idle=%.1f t=%.1f cold=%d\n", idle, smkT, coldTitleWait); smkWrite(s); }
  for (;;) {
    unsigned t = sceKernelGetSystemTimeLow(); float dt = (t - last) * 1e-6f; last = t; if (dt > .05f) dt = .05f;
    sceCtrlPeekBufferPositive(&pad, 1); pad.Buttons |= smokePad();
    unsigned hit = pad.Buttons & ~prev; prev = pad.Buttons;
    if (fePollHit) { fePollHit = 0; hit |= PSP_CTRL_START; }   // №656: нажатие, пойманное во время чтения комнаты меню
    if (smkPressN != n0 && !firstN && (!coldTitleWait || FE_COLD_FLOW_OLD || (hit & PSP_CTRL_START))) { firstN = smkPressN; firstAt = smkPressAt; }   // первое нажатие сценария на этом экране
    SmokeCmd sc; while (smokeNext(dt, &sc)) idleFlag(&sc);
    if (hit && deaf) { deaf = 0; hit = 0; }   // отрицательный контроль: первое нажатие теряется
    if (hit && (!coldTitleWait || FE_COLD_FLOW_OLD || (hit & PSP_CTRL_START))) { key = hit; presses = smkPressN - n0; break; }
    if (pad.Buttons) tTitle = t;                // зажатая кнопка — не простой
    if (demoIdleS > 0.f && (t - tTitle) * 1e-6f >= demoIdleS) {
      unsigned k, at, pr; demos++;
      if (demoMovie(fbp, &k, &at, &pr)) { key = k; firstAt = at; firstN = at != 0; presses = pr; fromDemo = 1; break; }
      scrT0 = sceKernelGetSystemTimeLow(); scrFrame(UI_BOOT_SPLASH, 0, 1, fbp);   // доигран — снова титульный
      sceCtrlPeekBufferPositive(&pad, 1); prev = pad.Buttons | smokePad(); last = tTitle = sceKernelGetSystemTimeLow();
      n0 = smkPressN; firstN = 0; continue;
    }
    scrFrame(UI_BOOT_SPLASH, 0, 1, fbp);
    if (smkOn && uiPressStartBox[0] >= 0.f && uiPressStartBox[0] < 3.f) {   // №405: пульс «Press START» по кадрам (tests/title)
      snprintf(s, sizeof s, "TITLEPS t=%.4f x0=%.2f y0=%.2f x1=%.2f y1=%.2f irisbusy=%d cold=%d\n", uiPressStartBox[0], uiPressStartBox[1], uiPressStartBox[2],
               uiPressStartBox[3], uiPressStartBox[4], transitBusy(), coldTitleWait); smkWrite(s); }
    if (smkShot[0] && strcmp(logged, smkShot)) {   // снимок сценария: что на экране
      snprintf(s, sizeof s, "SCRSHOT name=%s pack=boot scr=%d prog=0 t=0 level=%d show=PRESS_START\n", smkShot, UI_BOOT_SPLASH, LEVEL_IDX);
      smkWrite(s); strcpy(logged, smkShot);
    }
  }
  SCR_CLOSE();
  titleExitAt = firstN ? firstAt : 0;
  if (smkOn) { unsigned now = sceKernelGetSystemTimeLow();
    snprintf(s, sizeof s, "TITLE off key=0x%x shown=%.2f presses=%u lat_ms=%.0f from=%s demos=%u t=%.1f\n", key, (now - t0) * 1e-6f,
             presses, firstN ? (now - firstAt) / 1000.f : -1.f, fromDemo ? "demo" : "title", demos, smkT); smkWrite(s); }
  // правка 154: ирис закрывается над последним кадром титульного / ролика простоя; меню раскроется под ним (menuFrame)
  { unsigned ti = sceKernelGetSystemTimeLow(); transitClose(fbp, fromDemo ? "demo" : "title"); titleIrisMs = (sceKernelGetSystemTimeLow() - ti) / 1000; }
  scrIris = 0;
}
