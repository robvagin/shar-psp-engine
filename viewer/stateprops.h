// Стейтпропы (AnimDynaPhys уровня: ящики, автоматы, будки, колонки, кусты-шары, сено, заборы, лоток томако, щитки m4)
// и обломки BreakableObject (деревья, стекло Krusty, стоп-знак, вода гидранта) — этап 3б, docs/props_spec.md.
// Данные — подсекция SPR1 внутри PRP2 (tools/prp2_export.py). Машина состояний один в один из данных диска:
// видимость кусков по состояниям, позы суставов по кадрам (PTRN) и видимость по кадрам (PVIS), окна частиц (EFX),
// события (HIT / STOMP / VEHICLE_HIT / DESTROYED) и вызовы по времени (монеты, «убрать коллизию», «убрать из мира»).
// Времена outFrame/onFrame — секунды от входа в состояние. Кадры контроллера — кадры анимации (fps из клипа) × скорость.
// Чистый C99 без GU: рисует main.c (spPieceMatrix / spBillQuad), частицы — очередь spFxPop. Без malloc в кадре.
#ifndef SHAR_STATEPROPS_H
#define SHAR_STATEPROPS_H
#include <stdint.h>
#include "col.h"
#include "car.h"
#include "coins.h"

// №370: SPR1 shares the city's bottom-up textures; corners 0/1 are local bottom,
// 2/3 local top (spBillQuad). PNG sources are already flipped by psp_export.
static inline float spBillTexV(int corner, int old) { return old ? 1.f - (float)(corner >> 1) : (float)(corner >> 1); }

#ifdef __cplusplus
extern "C" {
#endif

// события (EventData.eventId в данных)
enum { SP_EV_DESTROYED = 7, SP_EV_HIT = 8, SP_EV_FEATHER = 9, SP_EV_STOMP = 10, SP_EV_VEHICLE_HIT = 11 };
// вызовы (экспорт переводит по имени)
enum { SP_OP_REMOVE, SP_OP_COINS, SP_OP_COINS_Z, SP_OP_COLOFF, SP_OP_DESTROYED, SP_OP_COLA, SP_OP_KILLSPEED,
       SP_OP_RADIATE, SP_OP_COL2OFF, SP_OP_BOLT,
       SP_OP_BREAK };   // 29.09: arg = BreakablesEnum — обломок этого номера в точке экземпляра (барьеры 0x3F0000F L2/L5)
// пресеты частиц (по текстурам системы частиц; main.c переводит в fxSpawn)
enum { SPFX_NONE, SPFX_DUST, SPFX_WOOD, SPFX_GLASS, SPFX_LEAVES, SPFX_SPARKS, SPFX_WATER, SPFX_FIRE, SPFX_SMOKE,
       SPFX_HAY, SPFX_TOMATO, SPFX_DEBRIS };

// ---- формат файла (размеры сверяются static_assert в stateprops.c и struct.pack в prp2_export.py)
typedef struct { float ox, oy, oz, sc; int32_t tex, flags; uint32_t nv, ni, vOff, iOff; } SpPiece;   // flags: 0-7 alpha/blend, 8-15 сустав, 16-23 слот, 24 authored additive
typedef struct {
  uint16_t firstPiece, nPiece, firstJoint, nJoint, firstState, nState, firstCol, nCol, firstBill, nBill;
  uint16_t kind, bindex;           // kind 0 стейтпроп, 1 обломок (bindex — номер BreakablesEnum)
  uint8_t cls, atc, fps, pool;
  uint32_t keyOff, visOff, frames, pvisMask;   // frames: младшие 16 — кадров клипа, старшие — дорожек
  float mass, life;
  uint32_t visWords, pvisX;      // слов маски видимости (0 = одно, как до 29.09; > 1 — тип больше 32 слотов) и индекс слов
                                 // 1.. маски PVIS в таблице vis (слово 0 — pvisMask); кадры PVIS — по visWords слов
  float c[3], h[3];                // коробка кусков (поза кадра 0) в осях объекта: центр, полуразмеры
} SpType;
typedef struct { int16_t parent, track; uint32_t pad; float q[4], t[3], s[3]; uint32_t pad2; } SpJoint;   // q = w,x,y,z
typedef struct { uint8_t slot, mode, cycles, preset; uint16_t min, max; float speed; } SpFx;   // mode 1 раз, 2 цикл
extern int spPoleFallOld, spPoleWallOld, spPoleBaseOld, spPole998Old;   // №975: 1 — прежнее (флаги сценария polefall975old / polewall975old)
typedef struct {
  uint32_t vis;                    // бит = слот рисунка (prop или effect) включён; слоты 32.. — слова visX (spSlotOn)
  uint8_t autoTo, nEvent, nCall, poseMode;   // autoTo 0xFF нет; poseMode 0 стоп, 1 раз, 2 цикл
  uint16_t poseMin, poseMax; float poseSpeed;
  uint16_t poseCycles, firstEvent, firstCall, nFx;
  float autoTime;                  // с
  SpFx fx[4];
  uint32_t visX;                   // visWords > 1: индекс слов 1.. маски vis в таблице vis (слово 0 — поле vis)
} SpState;
typedef struct { uint8_t event, to; uint16_t pad; } SpEvent;
typedef struct { uint8_t op, arg; uint16_t pad; float time; } SpCall;
typedef struct { uint8_t kind, second; uint16_t pad; float c[3], a[9], h[3]; } SpCol;   // kind 0 коробка, 1 сфера, 2 цилиндр; a — 3 оси строками
typedef struct { uint8_t joint, slot, mode, pad; int32_t tex; float t[3], w, h, dist; uint32_t rgba, flags; } SpBill;   // 0 NOAX, 1 AAX, 2 YAX (мир), 3 LYAX (сустав)
// SpBill.flags: 0-7 альфа (3), 8-15 смешивание; SP_BILL_PVIS — квадрат видим по кадрам PVIS своего слота, как куски (только огни
// барьеров 0x3F0000F, tools/prp2_export.py _add_animphys; у прочих билбордов PVIS не проверяется, как до 29.09)
#define SP_BILL_PVIS (1u << 16)
#ifndef SP_BILL_PVIS_ON
#define SP_BILL_PVIS_ON 1   // отрицательный контроль гейта tests/levelmech/barriers.sh: 0 — огни не мигают (горят всегда)
#endif
typedef struct { float x, y, z, q[4]; uint16_t type, tag; } SpInst;   // tag: номер щитка m4 (powerboxN), 0 нет

// ---- состояние экземпляра
typedef struct {
  uint8_t state, alive, colOn, col2On, poseValid, burst, posed;   // posed: поза суставов jm хоть раз посчитана (main_loop.h: далёкие — реже)
  uint8_t fxStarted;               // бит k: родная система окна EFX k уже запущена в этом состоянии
  uint16_t callDone;               // выполненные вызовы текущего состояния (бит = номер вызова)
  float t;                         // с от входа в состояние
  float poseF;                     // текущий кадр клипа
  float liftK;                     // №519: > 1 — вертикальный ход дорожек клипа растянут во столько раз от кадра 0 (подъёмник Duff: с любого ящика два прыжка до крыши); 0 — как в клипе
  float cool;                      // «остывание» контакта с машиной: новое событие только после 0.3 с без касания
  float away; uint8_t noRespawn;    // миссионные/постоянные объекты запрещают фоновое восстановление
  float fxAcc[4];
  float x, y, z, R[9];             // положение и поворот экземпляра (R по столбцам: мир = R*v + T)
  float *jm;                       // мировые (в осях объекта) матрицы суставов: 9 (R по столбцам) + 3 (t) на сустав
  float *jm0;                      // №923: матрицы суставов в кадре 0 обломка (rest) и поворот сдвига кусков (dirC, dirS) — осколки летят по ходу машины; NULL — как в клипе
  float dirC, dirS;
  uint8_t fall; float fallTh, fallW, fallD[2], fallR0[9];   // №975: 1 — столб валится на шарнире у основания (угол, скорость, направление, исходный поворот), 2 — лежит
  float hv[2];                     // №923: горизонтальная скорость машины (до стен кадра) при её ударе по этому предмету; обломок по вызову с задержкой берёт её отсюда
  uint16_t slN; uint8_t slMix; float slD0, slSum;   // №1000: далёкий экземпляр спит — сколько кадров не шагал, первый dt, все ли dt равны, сумма dt; при пробуждении время догоняется (stateprops.c spNearStep)
} SpRun;

typedef struct { uint8_t preset, first; int16_t sys; float x, y, z, dx, dz, t0; } SpFxOut;   // t0 — кадр начала окна EFX (правка 80)   // sys >= 0: родная система fx.shr (один раз), иначе пресет
#define SP_FXQ 96

typedef struct {
  const SpPiece *piece; const SpType *type; const SpJoint *joint; const float *key; const uint32_t *vis;
  int (*ensureKey)(unsigned type); // optional source loader for logic/EFX poses
  const float *const *typeKey; // optional detached, type-local animation tracks
  const SpState *state; const SpEvent *event; const SpCall *call; const SpCol *col; const SpBill *bill;
  const uint32_t *bqgIdx; const uint32_t *bqgRow; unsigned nbqgRow;   // хвост BQG1 (правка 76): на квад first, n; строки w, h (f32), ARGB
  const SpInst *inst; const uint8_t *dynBreak;
  unsigned ntype, ninst, npiece, ndyn;
  const void *vertBase; const unsigned short *idxBase;   // вершины — формат Vert main.c (16 Б), индексы 16-бит
  SpRun *run; void *mem;
  SpFxOut fxq[SP_FXQ]; unsigned nfx;
  // имена систем частиц эффектов (часть 15 SPR1): 32 Б = имя[28], u16 тип, u8 слот, u8 сустав; привязка к fx.shr — spFxBind
  const uint8_t *fxName; unsigned nfxName;
  int16_t *fxSys;                  // [slotBase[тип] + слот] -> система fx.shr, -1 нет (тогда пресеты SPFX_*)
  uint8_t *fxJoint;                // [slotBase[тип] + слот] -> сустав эффекта
  uint32_t *slotBase;              // [тип] -> первый слот типа в fxSys/fxJoint (32 × слов маски на тип; ntype + 1 чисел)
  int nativeBreakFx;               // 1: частицы обломков (kind 1) играет вызывающий (fxBurstBreakable), пресеты не слать
  // для миссий/отчёта
  uint32_t destroyedTags;          // OBJECT_DESTROYED: бит tag (щитки m4 1..9; 0 — без тега)
  int destroyed, cola, coins, killSpeed;
  int hits;                        // событий от машины всего (отладка)
  float lastImpact;                // правка 297: скорость удара машины в последнем касании пропа, м/с (гейт tests/smash/host.c)
  unsigned hnrHits;                // правка Роба 112 (30.09 «Hit & Run сразу, как что-то разбиваю»): ударов игрока (машина, пинок,
                                   // стомп), сменивших состояние, — шкала +7.5% за каждый (game.c забирает и обнуляет)
  // правка Роба 135.1: звук предмета — событие удара (HIT/STOMP/VEHICLE_HIT/DESTROYED), не любая смена состояния: авто-переход
  // (куст-шар сам встаёт через 1.3 с, фонарь через 1.0 с), возврат на место и обломок звука не дают. Очередь номеров экземпляров
  uint16_t sndQ[32]; unsigned sndN;
  int breaks;                      // обломков, запущенных вызовом SP_OP_BREAK (барьеры 0x3F0000F; отчёт, гейт tests/levelmech)
  uint8_t nearOn; float nearP[2][2]; unsigned nearSleep;   // №1000: nearOn — игра задала глаз и Гомера (main_loop.h, game.c), иначе никто не спит; nearP — глаз и Гомер (x, z); nearSleep — спящих в последнем кадре (отчёт)
} StateProps;

// section — начало SPR1 (выровнено на 16). 0 = ок, <0 = ошибка формата/памяти
extern int spNear1000;   // №1000: 1 — далёкие качающиеся деревья спят (stateprops.c)
int spInit(StateProps *sp, const void *section, unsigned size);
int spInitResident(StateProps *sp, const void *section, unsigned size, int detachedKeys);
void spFree(StateProps *sp);
// кадр: состояния, вызовы, частицы; машина (может быть NULL) — удары и упор в целые пропы; монеты — в coins (может быть NULL)
void spUpdate(StateProps *sp, const ColWorld *world, Car *car, Coins *coins, float dt);
// Contact pass without advancing the shared animation/state clocks.
void spHitCar(StateProps *sp, const ColWorld *world, Car *car, Coins *coins);
void spRespawnStep(StateProps *sp, float ex, float ez, float dt,
                   int (*seen)(void *, float, float, float, float), void *user);
// Правка Роба 334 (05.10, голосом: «ящики и монеты не возобновляются: собрал, сохранился — на том же месте больше не появляются,
// иначе бесконечные ящики»). Разовый предмет — стейтпроп с монетами: в машине состояний диска есть вызов SPAWN_N_COINS(_Z) (ящики,
// автоматы с газировкой, будки, тюки…), сам экземпляр не обломок (kind 0) и не миссионный (tag 0). В данных диска у них нет
// перехода назад в s0 — разбитый остаётся разбитым; фоновый возврат «5 с вне вида и дальше 60 м» (правило порта без источника,
// questions.tsv В6) их больше не трогает, состояние помнится в сохранении по уровню (savegame.h SvOnce). Прочие стейтпропы
// (заборы, гидранты, кусты без монет) возвращаются как раньше. spOnceOld = 1 — прежнее поведение (красный контроль гейтов:
// сборка -DSP_ONCE_OLD=1 или служебный флаг «svonceold» в main.c)
#ifndef SP_ONCE_OLD
#define SP_ONCE_OLD 0
#endif
extern int spOnceOld;
int spOnce(const StateProps *sp, unsigned i);           // 1 — экземпляр i разовый (см. выше)
// поставить экземпляр в состояние state без побочных действий (загрузка сохранения): монеты, счётчики миссий, звук, частицы не
// играют; «убрать из мира» / «убрать объём» применяются сразу, авто-переходы проходятся до конца цепочки
void spRestore(StateProps *sp, unsigned i, int state);
// событие экземпляру (Гомер: HIT / STOMP). 1 = состояние его обработало
int spEvent(StateProps *sp, unsigned i, int ev);
// правка 135.1: следующий экземпляр, получивший удар (звук предмета); 0 — очередь пуста
int spSndPop(StateProps *sp, unsigned *i);
// перевести экземпляр в состояние (время, вызовы, кадр позы — с начала состояния); оживляет его (пакет CLT1: осы, гэги)
void spEnter(StateProps *sp, unsigned i, int state);
// включить щитки миссии с тегом tag (s0 -> s1), 0 = все. Возврат: сколько включено
int spActivateTag(StateProps *sp, int tag);
// обломок для сбитого DynaPhys типа prpType из PRP1: номер типа обломка или -1
int spBreakOf(const StateProps *sp, unsigned prpType);
// проиграть обломок spType в точке pos с ориентацией q (w,x,y,z). 1 = у обломка есть меши (сбитый проп надо спрятать)
int spBreak(StateProps *sp, unsigned spType, const float pos[3], const float q[4]);
// №923 (Роб 24.10: «бью вперёд, а разбивается влево»): то же, но ход кусков поворачивается вокруг вертикали так, чтобы средний ход осколков шёл по горизонтальной
// скорости машины v (vx, vz) в кадре удара; куски остаются на местах покоя, поворачивается только их смещение. v == NULL или почти ноль — как spBreak
int spBreakDir(StateProps *sp, unsigned spType, const float pos[3], const float q[4], const float v[2]);

// ---- отрисовка (main.c)
// экземпляр надо рисовать (жив, что-то видно) и он ближе maxD к (ex, ez)
int spDrawable(const StateProps *sp, unsigned i, float ex, float ez, float maxD);
// центр коробки в мире и радиус описанной сферы (для пирамиды видимости)
void spBounds(const StateProps *sp, unsigned i, float c[3], float *r);
// посчитать позы суставов, если устарели (перед spPieceMatrix / spBillQuad)
void spPose(StateProps *sp, unsigned i);
// кусок k типа экземпляра виден в текущем состоянии/кадре
int spPieceOn(const StateProps *sp, unsigned i, unsigned k);
// слот рисунка включён в текущем состоянии экземпляра i (маска состояния любой ширины, без PVIS); слотов у типа — spSlotCount
int spSlotOn(const StateProps *sp, unsigned i, unsigned slot);
unsigned spSlotCount(const StateProps *sp, unsigned type);
// матрица куска k для GE (столбцы, как ScePspFMatrix4): мир = Inst · Joint · (o + sc·v)
void spPieceMatrix(const StateProps *sp, unsigned i, unsigned k, float m[16]);
// билборд b типа экземпляра: 4 угла (полоса: -,-  +,-  -,+  +,+) в мире, цвет ABGR для GE, текстура (после tmap)
int spBillQuad(const StateProps *sp, unsigned i, unsigned b, float ex, float ey, float ez, float v[12], uint32_t *abgr, int *tex, uint32_t *flags);
void spBillAxesOld(int old);   // service negative control: discard native YAX/LYAX
// имена систем частиц эффектов: число, имя k (с нулём), привязать систему fx.shr (sys < 0 — оставить пресет)
unsigned spFxNameCount(const StateProps *sp);
const char *spFxName(const StateProps *sp, unsigned k);
void spFxBind(StateProps *sp, unsigned k, int sys);
// забрать частицу из очереди (0 = пусто)
int spFxPop(StateProps *sp, SpFxOut *o);

#ifdef __cplusplus
}
#endif
#endif
