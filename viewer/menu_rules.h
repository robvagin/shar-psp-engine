// Чистые правила меню и переходов (сверка g09b-menu, круг 6, 02.11.2026): числа и условия без железа и без состояния игры.
// Игра вызывает их из savemenu.c, main_frontend.h, main_loop.h; тот же заголовок собирает Мак (tests/sverka_g09b) и проверяет на настоящих значениях
// и на выключателях *_OLD=1 (старое поведение обязано уронить свою проверку). Единицы: секунды, метры, метры в секунду, кадры сцены меню — 30 в секунду.
#ifndef MENU_RULES_H
#define MENU_RULES_H
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// ── выключатели (значения по умолчанию те же, что у объявлений рядом с кодом; 1 — прежнее поведение) ──
#ifndef SVM_DEFNO_OLD
#define SVM_DEFNO_OLD 0       // курсор запросов да/нет на «No»
#endif
#ifndef SVM_START3_OLD
#define SVM_START3_OLD 0      // START на записи и вопросах снимает паузу целиком
#endif
#ifndef SVM_QUITASK_OLD
#define SVM_QUITASK_OLD 0     // выход из паузы идёт через вопросы
#endif
#ifndef SVM_TUTLABEL_OLD
#define SVM_TUTLABEL_OLD 0    // подпись слота обучения «(TUT)»
#endif
#ifndef DEMOBLINK_OLD
#define DEMOBLINK_OLD 0       // «- DEMO -» первые 0,25 с скрыта
#endif
#ifndef DEMOIDLE_HELD_OLD
#define DEMOIDLE_HELD_OLD 0   // кнопки простой титульного не сбрасывают
#endif
#ifndef DEMO30_OLD
#define DEMO30_OLD 0          // простой титульного 30 с
#endif
#ifndef CAMBLEND_OLD
#define CAMBLEND_OLD 0        // S-кривая перехода камеры, 0,55 / 0,39 с
#endif
#ifndef LBSLIDE_OLD
#define LBSLIDE_OLD 0         // полосы разговора 0,5 с
#endif
#ifndef JACKSPEED_OLD
#define JACKSPEED_OLD 0       // угон только у машины не быстрее 4,5 м/с
#endif
#ifndef TUTGIVER_OLD
#define TUTGIVER_OLD 0        // подсказки обучения от выдавальщика 5 м
#endif
#ifndef HNRMSN3_OLD
#define HNRMSN3_OLD 0         // отмена миссии не победа; повтор после провала сбрасывает шкалу
#endif
#ifndef HNRONE_OLD
#define HNRONE_OLD 0          // погоня ставится и снимается по одной машине
#endif
#ifndef HNRSHOW_OLD
#define HNRSHOW_OLD 0         // индикатор после ареста отдельно от шкалы
#endif
#ifndef BUSTED_PS2_OLD
#define BUSTED_PS2_OLD 0
#endif
#ifndef TRIG3D_OLD
#define TRIG3D_OLD 0          // круг 7: триггер-шар по трём осям
#endif
#ifndef SHADOWLIN_OLD
#define SHADOWLIN_OLD 0       // круг 7: тень Гомера гаснет линейно до 3,8 м
#endif
#ifndef MSNREPAIR2_OLD
#define MSNREPAIR2_OLD 0      // круг 7: на старте миссии чинится только разбитая машина трафика
#endif
#ifndef MENUMUTE_ALL_OLD
#define MENUMUTE_ALL_OLD 0    // круг 7: уход из меню в игру глушит всё, кроме музыки, при любом пункте
#endif
#ifndef LANG1111_OLD
#define LANG1111_OLD 0        // №1111: язык игры — английский, французский, немецкий, испанский
#endif
#ifndef MINIFLY1143_OLD
#define MINIFLY1143_OLD 0     // №1143: наезд камеры на автотрек при MINI GAME один раз
#endif
#ifndef MINIFLY_OLD
#define MINIFLY_OLD 0
#endif
#ifndef CAMMENU1151_OLD
#define CAMMENU1151_OLD 0     // №1151: раздел CAMERA паузы
#endif
#ifndef NEWCAM_OLD
#define NEWCAM_OLD 0          // круг 7: NEW GAME «Yes» возвращает камеры у машины (Jump Cameras) в On
#endif
#ifndef HNRSUSP_OLD
#define HNRSUSP_OLD 0         // круг 7: на время разговора машины погони уходят в запас и возвращаются на то же место
#endif
#ifndef HNRMARK_OLD
#define HNRMARK_OLD 0         // круг 7: помеченная на удаление погоня снимается по одной (дальше 110 м; 5 с вне кадра)
#endif
#ifndef OPTFLY_OLD
#define OPTFLY_OLD 0
#endif

// ── числа ──
#define LB_SLIDE (LBSLIDE_OLD ? .25f : .5f)                 // полосы разговора въезжают и выезжают линейно 0,5 с
#define CAM_OUT_S (CAMBLEND_OLD ? .9f : .50f)               // выход из машины: S-кривая 0,50 с (круг 7: по пересчёту самого перехода оригинала — каждый кадр доля пути растёт от 2 % до 100 % за лимит 7 с, шаг кубический с нулевой начальной скоростью; наибольшее отклонение от кривой 0,034, было при 0,55 — 0,10)
#define CAM_IN_TAU (CAMBLEND_OLD ? 1.5f : .50f)             // посадка: у нового — длительность S-кривой, у прежнего — постоянная экспоненты
#define CAMIN1041_BLEND (CAMBLEND_OLD ? .7f : .37f)         // конец наезда меню: S-кривая 0,37 с (круг 7: тот же пересчёт для лимита 3 с, наибольшее отклонение 0,050, при 0,39 было 0,097)
#define TUT_GIVER_M (TUTGIVER_OLD ? 20.f : 5.f)             // подсказка обучения: персонаж-выдавальщик в 5 м
#define JACK_V_MAX 4.5f                                     // угон: машина едет не быстрее 4,5 м/с
#define HNR_PASS_S .7f                                      // проход постановки погони: раз в 0,7 с
#define HNR_FAR_S .5f                                       // проход снятия дальних машин: раз в 0,5 с
#define HNR_OUT_S 5.f                                       // помеченная машина погони дольше 5 с подряд вне кадра игрока — снимается
#define HNR_SHOW_LEVEL .05f                                 // после ареста индикатор держит 5 %…
#define HNR_SHOW_HOLD_S 1.3f                                // …1,3 с…
#define HNR_SHOW_DECAY .03f                                 // …и спадает на 0,03 в секунду
#if !DEMO30_OLD
#define MR_DEMO_IDLE_S 30.f                                 // титульный без показа ролика 30 с — включается demo
#else
#define MR_DEMO_IDLE_S 0.f
#endif

// ── запросы да/нет ──
static inline int mrYesNoCursor(void) { return SVM_DEFNO_OLD ? 0 : 1; }   // 0 — «Yes», 1 — «No»
// чем отвечает START на записи и вопросах: 1 — снять паузу целиком, 0 — вернуться в меню паузы, -1 — не действует (конец уровня или игры, список LOAD GAME)
// askQuit: бит 0 — «сохранить перед выходом?», бит 1 — «выйти?»; saveMode — окно записи (не загрузки)
static inline int mrStartRes(int saveMode, int askQuit) {
  if (SVM_START3_OLD) return (askQuit & 1) ? 0 : saveMode ? 1 : 0;
  if (!saveMode || (askQuit & 3) == 1) return -1;
  return 1;
}
static inline int mrQuitAsks(void) { return !SVM_QUITASK_OLD; }
static inline int mrQuitAsk(int freeRoam) { return SVM_QUITASK_OLD ? 1 : (freeRoam ? 3 : 2); }

// ── подпись слота: «(L<ур> M<мис>) дд/мм/гг чч:мм:сс», обучение первого уровня — «(TUT)», уровень без сюжета (msnStory 255) — «(L<ур>)» ──
static inline void mrSlotLabel(char *o, int n, int level, int msnStory, int day, int month, int year, int hour, int minute, int sec) {
  int L = level >= 1 && level <= 7 ? level : 1; char who[24];
  if (msnStory == 0xff) snprintf(who, sizeof who, "(L%d)", L);
  else if (L == 1 && msnStory == 0) snprintf(who, sizeof who, SVM_TUTLABEL_OLD ? "(L1 TUT)" : "(TUT)");
  else snprintf(who, sizeof who, "(L%d M%d)", L, msnStory);
  snprintf(o, (size_t)n, "%s %02d/%02d/%02d %02d:%02d:%02d", who, day, month, year % 100, hour, minute, sec);
}

// ── титульный экран ──
static inline int mrDemoShown(float sinceEnter) { return DEMOBLINK_OLD || ((int)(sinceEnter / .25f) & 1) == 1; }   // «- DEMO -»: 0,25 с нет, 0,25 с есть
static inline int mrIdleReset(int held) { return DEMOIDLE_HELD_OLD && held; }                                       // зажатая кнопка обнуляет простой (только прежнее)
static inline int mrDemoIdleDue(float idleS, float limit) { return limit > 0.f && idleS >= limit; }

// ── переход камеры (доля пути от времени t при длительности d) ──
static inline float mrCamU(float t, float d) {
  if (CAMBLEND_OLD) return 1.f - expf(-t / d);
  float x = t >= d ? 1.f : t <= 0.f ? 0.f : t / d; return x * x * (3.f - 2.f * x);
}

// ── перелёты камеры меню по подэкранам OPTIONS: кадры сцены fe.shr; экран 1 SOUND, 2 титры, 3 ролики; возврат -1 — нет перелёта ──
static inline int mrOptFly(int from, int to, int *a, int *b) {
  static const short go[4][2] = {{0, 0}, {660, 690}, {600, 630}, {914, 944}}, back[4][2] = {{0, 0}, {690, 720}, {630, 660}, {944, 974}};
  if (OPTFLY_OLD) return 0;
  if (from == 0 && to >= 1 && to <= 3) { *a = go[to][0]; *b = go[to][1]; return 1; }
  if (to == 0 && from >= 1 && from <= 3) { *a = back[from][0]; *b = back[from][1]; return 1; }
  return 0;
}

// триггер-шар: внутри, если расстояние по трём осям строго меньше радиуса (у оригинала шар; TRIG3D_OLD=1 — по горизонтали)
static inline int mrSphereIn(float dx, float dy, float dz, float r) { return (TRIG3D_OLD ? dx * dx + dz * dz : dx * dx + dy * dy + dz * dz) < r * r; }
// тень Гомера: масштаб = 1 − h·0,5/1,9 (h — высота над землёй, м; 1,9 — высота прыжка), яркость умножается на тот же масштаб; ниже нуля не уходит
static inline float mrShadowScale(float h) {
  if (SHADOWLIN_OLD) { float a = 1.f - .5f * fminf(1.f, h / 1.9f), b = 1.f - fminf(1.f, fmaxf(0.f, (h - 1.9f) / 6.1f)); return a * b; }   // прежнее: на вершине прыжка стоп, потом гаснет к 8 м
  float s = 1.f - h * (.5f / 1.9f); return s > 0.f ? s : 0.f;
}
// ремонт машины игрока на старте миссии: только разбитая (не просто помятая) и только машина трафика (чужая; своя остаётся как есть)
static inline int mrRepairOnStart(int destroyed, float damage, int stolen) { return stolen && (MSNREPAIR2_OLD ? (destroyed || damage > 0.f) : destroyed); }
// при уходе из меню в игру глушить всё, кроме музыки: оригинал — при любом старте (RESUME, NEW GAME, мини-игра, демо)
static inline int mrDuckOnStart(int resume) { return !MENUMUTE_ALL_OLD || resume; }
// машины погони на время разговора: в запас с позой (1) или совсем убрать и поставить заново (0)
static inline int mrHnrSuspKeep(void) { return !HNRSUSP_OLD; }
// NEW GAME, ответ «Yes» при загруженной игре: настройки возвращаются к умолчанию у тех, кто их хранит: радар On, подсказки Барта не показаны, ролики не виданы, камеры у машины (Jump Cameras) On.
// Громкости не трогаются (оригинал не сбрасывает звук, пока игра загружена), вибрация тоже. Invert Camera: умолчание порта On (решение Роба, правка 362) — здесь не сбрасывается
static inline void mrNewGameDefaults(int *radar, unsigned *tutSeen, unsigned *movies, int *jumpCam) { *radar = 1; *tutSeen = 0; *movies = 0; if (!NEWCAM_OLD) *jumpCam = 1; }
// ── язык (№1111) ──
// язык системы PSP (PSP_SYSTEMPARAM_ID_INT_LANGUAGE: 0 японский, 1 английский, 2 французский, 3 испанский, 4 немецкий, 5 итальянский, 6 нидерландский, 7 португальский, ...) -> 0 English, 1 French, 2 German, 3 Spanish;
// -1 — языка среди четырёх нет (итальянский в оригинале выключен, португальского на диске нет): экран выбора
static inline int mrLangFromPsp(int v) { if (LANG1111_OLD) return 0; return v == 1 ? 0 : v == 2 ? 1 : v == 4 ? 2 : v == 3 ? 3 : -1; }
// файлы с текстами (всё, что экспортёры собирают из текстовой библиотеки диска); прочее от языка не зависит
static inline int mrLangDepends(int lang, const char *name) {
  if (LANG1111_OLD || lang <= 0) return 0;
  const char *b = strrchr(name, '/'); b = b ? b + 1 : name; size_t n = strlen(b);
  static const char *const f[] = {"ui.shr", "msnart.shr", "load.shr", "menu.shr", "pmenu.shr", "pextra.shr", "fsave.shr", "psave.shr", "scrap.shr", "scrapgal.shr", "ending.shr", "shop.shr", "phone.shr", "phonecars.txt", "prompts.txt", "wardrobe.shr", "mini.shr", "ticket.shr", 0};
  for (int i = 0; f[i]; i++) if (!strcmp(b, f[i])) return 1;
  if (n > 4 && !strcmp(b + n - 4, ".msn")) return 1;
  if (!strncmp(b, "cardqr-", 7)) return 1;
  return 0;
}
static inline void mrLangFile(char *o, int cap, const char *dir, int lang, const char *name) { snprintf(o, (size_t)cap, "%slang/%c/%s", dir, "EFGS"[lang >= 0 && lang < 4 ? lang : 0], name); }
// экран выбора: стрелки вверх/вниз по кругу
static inline int mrLangMove(int cur, int dir) { return ((cur + dir) % 4 + 4) % 4; }
// MINI GAME: перелёт камеры к автотреку. В таблице перелётов данных пункт MINI уже есть (кадры 880–910), круг 2 добавил точный диапазон оригинала 880–913 — вместе вышло два наезда подряд (№1143).
// Теперь один: диапазон оригинала (880–913), табличный перелёт пропускается. MINIFLY_OLD=1 — как до круга 2 (только табличный), MINIFLY1143_OLD=1 — оба подряд (было в TEST85)
static inline int mrMiniFlyTable(int tableHas) { return tableHas && (MINIFLY1143_OLD || MINIFLY_OLD); }
static inline int mrMiniFlyRange(void) { return !MINIFLY_OLD; }
// ── раздел CAMERA паузы (№1151): ступени 0..4, середина 2 — как было; камера (g06-camera) читает значения и сама зажимает пределы ──
static inline int mrCamClamp(int step) { return step < 0 || step > 4 ? 2 : step; }
static inline float mrCamH(int step) { static const float v[5] = {.5f, .75f, 1.f, 1.25f, 1.5f}; return v[mrCamClamp(step)]; }          // высота глаза над точкой взгляда, множитель
static inline float mrCamD(int step) { static const float v[5] = {.7f, .85f, 1.f, 1.15f, 1.3f}; return v[mrCamClamp(step)]; }         // дальность, множитель
static inline float mrCamTilt(int step) { static const float v[5] = {-10.f, -5.f, 0.f, 5.f, 10.f}; return v[mrCamClamp(step)]; }       // наклон взгляда, градусы
static inline float mrCamFov(int step) { static const float v[5] = {.85f, .92f, 1.f, 1.08f, 1.15f}; return v[mrCamClamp(step)]; }      // множитель угла обзора
// settings.bin прежних версий короче (bytesRead < смещения footHeight): пешком получает прежние Height / Distance, что были на обе камеры — у игрока ничего не прыгает
static inline int mrCamFootMigrate(int bytesRead, int footOffset, int footNow, int camOld) { return !CAMMENU1151_OLD && bytesRead < footOffset ? camOld : footNow; }
// ── угон машины трафика: дверь открыта, если машина едет не быстрее JACK_V_MAX ──
static inline int mrJackBlocked(float v) { return !JACKSPEED_OLD && fabsf(v) > JACK_V_MAX; }

// ── Hit & Run и миссии: событие смены «в миссии»: 0 нет, 1 старт, 2 победа ──
// prevIn < 0 — ещё не было замера; failSeen — на экране провала побывали; aborted — миссию отменили из паузы (победой не считается)
static inline int mrHnrMsnEvent(int prevIn, int nowIn, int failSeen, int aborted) {
  if (prevIn < 0 || nowIn == prevIn) return 0;
  if (nowIn) return 1;
  return (!failSeen && !(!HNRMSN3_OLD && aborted)) ? 2 : 0;
}
static inline int mrHnrRetry(int failPrev, int failNow, int inMission) { return failPrev && !failNow && inMission; }   // вышли с экрана провала обратно в миссию = повтор
static inline int mrHnrRetryReset(void) { return !HNRMSN3_OLD; }                                                    // повтор — событие старта заново: шкала в 0
// хвост показа после ареста: сначала держим HNR_SHOW_LEVEL HNR_SHOW_HOLD_S, потом спад
static inline void mrHnrShowStep(float *show, float *hold, float dt) { if (*hold > 0.f) *hold -= dt; else if (*show > 0.f) *show = fmaxf(0.f, *show - HNR_SHOW_DECAY * dt); }
// помеченная машина погони: счёт секунд вне кадра (в кадре — с нуля); 1 — пора снять (строго дольше HNR_OUT_S)
static inline int mrHnrOutStep(float *t, int visible, float dt) { if (HNRMARK_OLD) return 0; if (visible) { *t = 0.f; return 0; } *t += dt; return *t > HNR_OUT_S; }
// значение для кольца: прежнее — шкала; новое — наибольшее из шкалы и хвоста
static inline float mrHnrRing(float heat, float show) { return HNRSHOW_OLD ? heat : fmaxf(heat, show); }
#endif
