// Чистые правила главного цикла окна g10b-loop (круг 6): без состояния игры, чтобы их проверял хост-тест на Маке
// (tests/g10b/loop_rules.c). Каждое правило под своим выключателем: _OLD=1 — прежнее поведение. Числа — поведение оригинала своими
// словами; где число наше, а не из оригинала, это сказано рядом.
#ifndef G10B_CALC_H
#define G10B_CALC_H
#include <math.h>
#include <string.h>

// --- шкала Hit & Run (сверка 2.1 / 2.5): спад в секунду. Уровень 1: 3,0; уровень 2: 1,5; уровни 3–7: 1,0. В погоне (шкала на защёлке) 6 в секунду,
// в помещении 10 в секунду — оба от уровня не зависят. Всё в единицах шкалы за секунду (не за кадр).
#ifndef HNR_LEVEL_DECAY_OLD
#define HNR_LEVEL_DECAY_OLD 0   // 1 — спад 3/с на всех уровнях (прежнее)
#endif
static inline float g10bHnrBase(int level) { return HNR_LEVEL_DECAY_OLD ? 3.f : level <= 1 ? 3.f : level == 2 ? 1.5f : 1.f; }
static inline float g10bHnrRate(int level, int inRoomNow, int latched) { return inRoomNow ? 10.f : latched ? 6.f : g10bHnrBase(level); }

// --- запрет паузы (M2:Д5, P13:12, P17-3): идёт смена этапа (затемнение HUD), ждёт смена уровня или выход в меню, ждёт новость или выбор миссии
#ifndef PAUSE_BAN_OLD
#define PAUSE_BAN_OLD 0   // 1 — пауза открывается и в эти моменты (прежнее)
#endif
static inline int g10bPauseBanned(float hudFade, int toLevel, int toMenu, int newsPend, int pendRow) {
  return !PAUSE_BAN_OLD && (hudFade > 0.f || toLevel || toMenu || newsPend > 0 || pendRow >= 0);
}

// --- вид назад (СВЕРКА_КАМЕРА 1.12): штанга от машины до глаза отражается по горизонтали (высота та же), взгляд остаётся; у бампера глаз у заднего края кузова и смотрит назад на 10 м
#ifndef REARVIEW_OLD
#define REARVIEW_OLD 0   // 1 — вида назад нет (прежнее)
#endif
// Круг 7 (сверено с оригиналом): включение и выключение вида назад принимается только пока камера «стабильна»: машина не в воздухе (все четыре колеса без опоры), не наклонена больше 30° от вертикали
// и прошло 0,2 с после последнего неустойчивого кадра; в неустойчивом состоянии вид остаётся как был. У бампера камера без этого правила: смотрит назад ровно пока кнопка зажата.
// Бампер назад: глаз внутри кузова — на (задняя ось − 1,0 м) от начала машины, высота верха кузова (у переднего вида +0,3 м, у заднего нет), взгляд на 2 м назад на той же высоте
// №1130 (Роб: «вообще убрал эту функцию»): вид назад по стрелке вниз (№1078) убран совсем — стрелка вниз в машине камеру не трогает (до №1078 она в машине ничего не делала).
// LOOKBACK1130_OLD=1 — как TEST84 (вид назад есть)
#ifndef LOOKBACK1130_OLD
#define LOOKBACK1130_OLD 0
#endif
static inline int g10bCamUnstable(const float comp[4], const float q[4], float *delay, float dt) {
  int air = comp[0] <= 0.f && comp[1] <= 0.f && comp[2] <= 0.f && comp[3] <= 0.f;
  float upY = 1.f - 2.f * (q[1] * q[1] + q[3] * q[3]);
  int now = air || upY < .866f;
  if (now) *delay = .2f; else if (*delay > 0.f) { *delay -= dt; if (*delay < 0.f) *delay = 0.f; }
  return now || *delay > 0.f;
}
static inline int g10bRearLook(int *state, int held, int bumper, int unstable) {
  if (!LOOKBACK1130_OLD) { *state = 0; return 0; }   // №1130: вида назад нет
  if (REARVIEW_OLD) return held;
  if (bumper || !unstable) *state = held;
  return *state;
}
static inline int g10bRearCam(int held, int bumper, float cx, float cz, float h, float wheelR, const float ccp[3], const float clk[3], float cp[3], float lk[3]) {
  if (!LOOKBACK1130_OLD || REARVIEW_OLD || !held) return 0;
  float sh = sinf(h), ch = cosf(h);
  if (bumper) { float back = wheelR - 1.f; if (back < .3f) back = .3f;
    cp[0] = cx - sh * back; cp[1] = ccp[1] - .3f; cp[2] = cz - ch * back; lk[0] = cp[0] - sh * 2.f; lk[1] = cp[1]; lk[2] = cp[2] - ch * 2.f; }
  else { cp[0] = 2.f * cx - ccp[0]; cp[2] = 2.f * cz - ccp[2]; lk[0] = clk[0]; lk[1] = clk[1]; lk[2] = clk[2]; }
  return 1;
}

// --- угол кадра у бампера (СВЕРКА_КАМЕРА 1.19): 90° по горизонтали; по вертикали — через соотношение сторон экрана. Прежнее: как за машиной (curFovY не трогаем)
#ifndef BUMPER_FOV_OLD
#define BUMPER_FOV_OLD 0   // 1 — угол как за машиной
#endif
static inline float g10bBumperFovY(float curFovY, float aspect) { return BUMPER_FOV_OLD ? curFovY : 2.f * atanf(1.f / aspect) * (180.f / 3.14159265f); }

// --- дым двигателя (P01-11): с 60 % урона, интенсивность (урон − 0,6) × 2,5, то есть 0 при 60 % и 1,0 при полном; с 98 % — полный средний дым.
// Прежнее: дым с 33 % урона с долей 0,4, полный с 60 %. Тяжёлый дым и огонь (3, 4) — не трогаются (запись PS2)
#ifndef SMOKE_D_OLD
#define SMOKE_D_OLD 0   // 1 — прежнее
#endif
static inline int g10bSmokeStage(float damage, int burnOut, int boomed, int destroyed) {
  return burnOut ? 0 : boomed ? 4 : destroyed ? 3 : damage >= (SMOKE_D_OLD ? .6f : .98f) ? 2 : damage >= (SMOKE_D_OLD ? .33f : .6f) ? 1 : 0;
}
static inline float g10bSmokeRate(float damage, int stage) {
  // круг 7 (сверено с оригиналом): лёгкий и средний дым (до 99 %) идут по одной формуле (урон − 0,6) × 2,5; полный «средний» 1,0 с 98 % было ошибкой — при 98 % формула даёт 0,95
  return SMOKE_D_OLD ? (stage == 1 ? .4f : 1.f) : (stage == 1 || stage == 2 ? fminf(1.f, (damage - .6f) * 2.5f) : 1.f);
}

// --- реплика «устал» (Turbo, P10-23): через 5 с непрерывного турбо (○ зажат, пешком, на земле, быстрее ходьбы, не в помещении). Вышел раньше — таймер с нуля. Возвращает 1 в момент реплики
#ifndef TURBO_TIRED_OLD
#define TURBO_TIRED_OLD 0   // 1 — реплика на первом шаге бега (main_state.h)
#endif
static inline int g10bTurboStep(float *t, int *said, int active, float dt) {
  if (TURBO_TIRED_OLD || dt <= 0.f) return 0;
  if (!active) { *t = 0.f; *said = 0; return 0; }
  *t += dt; if (*t >= 5.f && !*said) { *said = 1; return 1; } return 0;
}

// --- искры при посадке (P01-29), круг 7 сверено с оригиналом: при ударе подвески в упор (любое колесо сжато до предела) и если за последнюю секунду были все четыре колеса в воздухе — один раз за полёт
// 30 искр радиально по земле (горизонтальное направление, чуть вверх), на поверхности асфальт, металл или гравий. Прежнее (до круга 7): 10 искр, если вертикальная скорость вниз больше 4 м/с (число порта)
#ifndef LAND_SPARK_OLD
#define LAND_SPARK_OLD 0   // 1 — прежнее: 10 искр по скорости, без искр от упора подвески
#endif
#define G10B_SPARK_N 30
static inline int g10bBottomOut(const float comp[4], float lim) { for (int i = 0; i < 4; i++) if (comp[i] >= lim - 1e-4f && lim > 0.f) return 1; return 0; }
static inline int g10bBottomSparkStep(float *since, float dt, int allAir, int bottomed, int surfOk) {
  if (LAND_SPARK_OLD) return 0;
  if (allAir) *since = 0.f;
  *since += dt;
  if (bottomed && *since <= 1.f) { *since = 9.f; return surfOk; }
  return 0;
}
static inline int g10bLandSpark(float vy0, int surfOk) { return LAND_SPARK_OLD && -vy0 > 4.f && surfOk; }   // только прежний путь

// --- точки машины для локатора (P18-08): центр и четыре угла рамки — низ-сзади слева и справа, верх-спереди слева и справа. a: -1 сзади, +1 спереди; u: 0 низ, 1 верх. Прежнее: все 8 углов
#ifndef CARZONE_PTS_OLD
#define CARZONE_PTS_OLD 0   // 1 — центр и все 8 углов
#endif
static inline int g10bCarZoneCorner(int a, int u) { return CARZONE_PTS_OLD || (u == 0) == (a < 0); }

// --- вид по SELECT в машине (СВЕРКА_КАМЕРА 1.2 / P08:15): цикл ближний(0) → бампер(2) → дальний(1) → ближний; у багги бампера нет — ближний → дальний.
// Под камерой зоны, комнаты и вступления выбор игнорируется. Возвращает новый вид
#ifndef SELECT_VIEW_OLD
#define SELECT_VIEW_OLD 0   // 1 — SELECT меняет вид и под камерой зоны, и у багги бампер остаётся
#endif
static inline int g10bViewNext(int cur, int dune, int cinematic) {
  if (!SELECT_VIEW_OLD && cinematic) return cur;
  return cur == 0 ? (!SELECT_VIEW_OLD && dune ? 1 : 2) : cur == 2 ? 1 : 0;
}

// --- вопрос «сохранить?» после пройденного уровня (P13:4): задаётся в обычной игре; служебные запуски и туры не спрашивают
#ifndef LV_SAVEASK_OLD
#define LV_SAVEASK_OLD 0   // 1 — сразу следующий уровень
#endif
static inline int g10bLevelAsk(int mainNew, int smkOn, int svcMode, int svmOn, int uiReady, int bpass) { return !LV_SAVEASK_OLD && mainNew >= 0 && !smkOn && !svcMode && svmOn && uiReady && !bpass; }

// --- кнопка действия △ (P05:19): после сработавшего нажатия следующие 3 кадра не принимается (между срабатываниями не меньше 4 кадров).
// P05:2: ранг важности (меньше = важнее): миссия 0, своя машина 1, будка 2, дверь 3, выключатель и гэг 4, покупки 5–6, чужая машина 7
#ifndef ACTION_RANK_OLD
#define ACTION_RANK_OLD 0   // 1 — старая цепочка без ранга и без паузы
#endif
enum { G10B_ACT_MISSION, G10B_ACT_OWNCAR, G10B_ACT_BOOTH, G10B_ACT_DOOR, G10B_ACT_SWITCH, G10B_ACT_BUYCAR, G10B_ACT_BUYSUIT, G10B_ACT_FOREIGN, G10B_ACT_N };
static inline int g10bActionRank(int kind) {
  static const int r[G10B_ACT_N] = {0, 1, 2, 3, 4, 5, 6, 7};
  static const int old[G10B_ACT_N] = {0, 6, 4, 3, 1, 7, 2, 5};   // прежняя цепочка: выключатель/гэг, гардероб, дверь, будка, чужая машина, своя машина последней
  return (ACTION_RANK_OLD ? old : r)[kind];
}
// Кто побеждает среди одновременно доступных (маска по kind): наименьший ранг
static inline int g10bActionPick(unsigned mask) { int best = -1; for (int k = 0; k < G10B_ACT_N; k++) if ((mask >> k & 1u) && (best < 0 || g10bActionRank(k) < g10bActionRank(best))) best = k; return best; }
// Круг 7: оригинал — не раньше 4-го кадра после сработавшего нажатия (3 кадра подряд мимо); кадр оригинала принят 1/30 с, поэтому в секундах: 0,12 с (30 кадр/с: 4-й кадр, 60 кадр/с: 8-й)
static inline int g10bTriStep(float *cool, int pressed, float dt) {
  if (ACTION_RANK_OLD) return pressed;
  *cool -= dt; if (*cool < 0.f) *cool = 0.f;
  return *cool > 0.f ? 0 : pressed;
}
static inline void g10bTriFired(float *cool) { if (!ACTION_RANK_OLD) *cool = .12f; }

// --- свободные машины мира (P03-20, круг 5): уходят, когда игрок дальше farR от их места или миссия ставит свою машину ближе missR; возвращаются, когда игрок ближе farR − 20 м
// и рядом нет машины миссии. Круг 6 (цена, №1086): проверка идёт раз в 10 кадров, а не каждый кадр — ходьба и езда за 1/6 с меняют расстояние на единицы метров при запасе в 20 м
#ifndef WCAR_ZONE_OLD
#define WCAR_ZONE_OLD 0   // 1 — машины стоят весь уровень (прежнее до круга 5)
#endif
#ifndef WCAR_STEP_COST_OLD
#define WCAR_STEP_COST_OLD 0   // 1 — проверка каждый кадр (круг 5)
#endif
static inline int g10bWorldCarAway(int away, float d2, int mis, float farR) {
  if (WCAR_ZONE_OLD) return 0;   // прежнее: машина не уходит
  if (!away) return d2 > farR * farR || mis;
  return !(d2 < (farR - 20.f) * (farR - 20.f) && !mis);
}
static inline int g10bEvery(int *c, int n) { if (WCAR_STEP_COST_OLD) return 1; if (++*c >= n) { *c = 0; return 1; } return 0; }

// --- меню и загрузка (круг 6)
#ifndef MENU_SNAP_OLD
#define MENU_SNAP_OLD 0   // 1 — RESUME GAME только по записи на карте, курсор на нём лишь при первом входе, SCRAP BOOK серый без записи
#endif
#ifndef LICENSE5S_OLD
#define LICENSE5S_OLD 0   // 1 — лицензия 3,0 с
#endif
#ifndef LOAD_NOMIN_OLD
#define LOAD_NOMIN_OLD 0  // 1 — показ газеты не короче 2,0 с
#endif
#ifndef TUT_PAUSE_OLD
#define TUT_PAUSE_OLD 0   // 1 — START в подсказке Барта проглатывается
#endif
#ifndef NEWGAME_FMV_NOSKIP_OLD
#define NEWGAME_FMV_NOSKIP_OLD 0   // 1 — ролик новой игры пропускается
#endif
// RESUME GAME виден, пока игра загружена (запись на карте или игра в памяти после выхода в меню); P14:1
static inline int g10bMenuResumeShown(int hlSaved, int snapOn) { return hlSaved || (!MENU_SNAP_OLD && snapOn); }
// курсор на RESUME GAME, если игра загружена (при каждом входе в меню), иначе NEW GAME; P14:2
static inline int g10bMenuCursorResume(int firstOpen, int hlSaved, int snapOn) { return (firstOpen && hlSaved) || (!MENU_SNAP_OLD && snapOn); }
// SCRAP BOOK: всегда доступен (серым бывает только MINI GAME); прежнее — по записи или игре этой сессии; P14:9
static inline int g10bMenuScrapEnabled(int hlSaved, int sessionGame) { return MENU_SNAP_OLD ? (hlSaved || sessionGame) : 1; }
// экран лицензии: не короче 5,0 с (PS2-релиз); P17-1
static inline int g10bLicenseMs(void) { return LICENSE5S_OLD ? 3000 : 5000; }
// минимум показа газеты на загрузке: у оригинала минимума нет — влёт газеты и запас огню 0,2 с (spin — влёт, с); 5.13
static inline float g10bLoadMinS(float spin, int min35Old) { return min35Old ? 3.5f : LOAD_NOMIN_OLD ? 2.0f : spin + .2f; }
// START в подсказке Барта открывает паузу, пока звучит реплика; после реплики не делает ничего; P13:17
static inline int g10bTutPauseOpens(int tutActive, int start, int uiReady, int bpass, int voiceBusy) { return tutActive && !TUT_PAUSE_OLD && start && uiReady && !bpass && voiceBusy; }
// ролик новой игры не пропускается (в служебном режиме можно); P14:14
static inline int g10bFmvNoSkip(int svcMode) { return !NEWGAME_FMV_NOSKIP_OLD && !svcMode; }

// --- полосы разговора (M2:23): выход за время въезда (LB_SLIDE, у оригинала 0,5 с линейно); прежнее — 0,25 с. Сходящиеся полосы: разговор последнего этапа вступления и пара Пэтти и Сельма —
// на выходе полосы сходятся к центру за 0,5 с линейно (экран закрыт), пауза 0,5 с, потом ирис. t — время с начала схождения; всего 1,0 с
#ifndef LB_EXIT_OLD
#define LB_EXIT_OLD 0   // 1 — выход 0,25 с
#endif
#ifndef LB_CONV_OLD
#define LB_CONV_OLD 0   // 1 — полосы просто уезжают
#endif
static inline float g10bLbExitSecs(float slide) { return LB_EXIT_OLD ? .25f : slide; }
static inline int g10bLbConvStart(int wantPrev, int want, int latched, float t, int mcPlay) { return !LB_CONV_OLD && wantPrev && !want && latched && t < 0.f && !mcPlay; }
static inline float g10bLbCover(float t) { return t < 0.f ? 0.f : t >= .5f ? 1.f : t / .5f; }   // доля закрытия экрана: 0..1 за 0,5 с, дальше держится (чёрный кадр)
static inline int g10bLbConvDone(float t) { return t >= 1.f; }   // 0,5 с схождение + 0,5 с пауза → ирис

// --- переход камеры в разговор и из него (P08:11), круг 7 сверено с оригиналом: путь за кадр = ((1 − 0,02) × доля прошедшего от 7 с + 0,02) × (мс кадра / 16), с памятью скорости (кубическая кривая).
// Доли пути от начала: 50 % за 0,27 с, 90 % за 0,42 с, 95 % за 0,45 с, 99 % за 0,50 с. Наша плавная кривая u²(3 − 2u) за T секунд даёт те же четыре точки при T = 0,515 с
// (0,26 / 0,41 / 0,45 / 0,49); прежнее значение 0,45 с было временем 95 %, а не всего перехода
#ifndef TALKCAM_DUR_OLD
#define TALKCAM_DUR_OLD 0   // 1 — переход 0,45 с (95 % за 0,39 с)
#endif
static inline float g10bTalkBlendSecs(void) { return TALKCAM_DUR_OLD ? .45f : .515f; }
static inline float g10bSmooth(float t, float d) { float x = t >= d ? 1.f : t <= 0.f ? 0.f : t / d; return x * x * (3.f - 2.f * x); }

#endif
