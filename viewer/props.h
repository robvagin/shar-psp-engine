// Пропы: сбиваемые объекты (гидранты, ящики, знаки, мелкие деревья, фонари) — этап 3б. Секция PRP1 (после COL1,
// перед PVS1; смещение в заголовке SHR7 @96, tools/psp_export.py). Спят, пока не задеты машиной (окружность
// радиуса типа против кузова машины, carBox); после — простое твёрдое тело: гравитация 15 м/с2, отскок от земли
// через colGround (0.3), затухание, засыпание при остановке. Лёгкая потеря скорости машины о тяжёлый проп.
// Чистый C99, float, без malloc в кадре (одна аллокация в propsInit).
#ifndef SHAR_PROPS_H
#define SHAR_PROPS_H
#include "col.h"
#include "car.h"

#ifdef __cplusplus
extern "C" {
#endif

// 107б П5 (сверка физики Fable-2, закон 1276б «всё как в оригинале»): потеря скорости машины о тяжёлый предмет (фонарь 1000 кг, столб) шла двумя формулами: простой предмет по записи PS2 с потолком по импульсу, стейтпроп по 0,3 × масса / сумма масс.
// Запись PS2 (стенд 16.10, фонарь 1000 кг, газ держит скорость): сразу после касания скорость падает на 1,0 + 0,28 × v (2,5 / 3,75 / 6,4 м/с при 5,3 / 10,3 / 20,2). Одна формула на оба пути: dv = (1,0 + 0,28 × v) × min(1, масса / 1000), не больше 0,9 v.
// PROPLOSS107_OLD=1 / «flag proploss107old 1» — прежние две формулы
extern int propsLoss107Old;
static inline float propsHeavyLossDv(float v, float mass) { float dv = (1.f + .28f * v) * (mass < 1000.f ? mass / 1000.f : 1.f); return dv > .9f * v ? .9f * v : dv; }
// 107б П12 (сверка физики Fable-2, закон 1276б): импульс удара Гомера по предмету: пинок 400 / масса, прыжок сверху (стомп) 800 / масса (у стейтпропов уже так, game.c spImpulse); у простых предметов стомп бил как пинок (400).
// Скорость вылета предмета = импульс / масса (масса 0 — 100 кг). PROPSTOMP107_OLD=1 / «flag propstomp107old 1» — стомп 400, как в 106
extern int propsStomp107Old, propsBoom107Old;   // 107б П11: 1 — взрыв задевает все предметы в радиусе (flag propboom107old)
static inline float propsHomerKickSpeed(int stomp, float mass) { return ((stomp && !propsStomp107Old) ? 800.f : 400.f) / (mass > 0.f ? mass : 100.f); }
extern int propsVol107Old;   // 107в П4: 1 — контакт по стволу/кругу/тонкой коробке, как в 106 (flag propvol107old)
extern int propsGhost107Old;   // 107б П1/П2: 1 — задетый уснувший предмет призрак для машин у всех, как в 106 (flag propghost107old)
extern int propsPole968Old;   // №968: 1 — столб (высокий предмет без обломка) падает физикой коробки, как до правки (flag pole968old)
// №1096 (Роб, TEST83 на PSP: «у почтового ящика то появляется, то пропадает контур — мигает»): причина — регулятор кадра (main_state.h frameGovStep: два кадра дольше 30 мс — перегрузка на полсекунды)
// целиком выключал проход контура пропов (proptoon.h), рядом с трафиком кадры тяжёлые, контур всех предметов гас и возвращался. Теперь регулятор контур пропов не трогает (проход стоит ~0,05 мс,
// не больше PROP_TOON_MAXP кусков), а предметы, которые тронул игрок (PropState.inkTouch), не отсекаются потолком кусков. INK1096_OLD=1 — как в TEST83
#ifndef INK1096_OLD
#define INK1096_OLD 0
#endif
static inline int propsInkGov(int gov) { return INK1096_OLD ? gov : 0; }   // 1 — регулятор кадра отключает контур предметов
// Круг 7 (g11, по оригиналу): частица ATC подвижного предмета (почтовый ящик, урна, скамья) — один раз, когда предмет НЕ разбит обломком и импульс удара больше 5 × масса предмета (масса из строки ATC).
// Импульс — приведённый: машина m и предмет mp, импульс = m·mp/(m+mp) · скорость сближения · (1 + упругость), упругость 0,1 (приближение: SP_CAR_E, в данных PRP1 её нет). У ящика 125 кг при машине 1500 кг
// это сближение от 4,9 м/с. Было: частица на каждый удар любой скорости (и вместе с обломком). PROPFXIMP_SV_OLD=1 — как было
#ifndef PROPFXIMP_SV_OLD
#define PROPFXIMP_SV_OLD 0
#endif
static inline int propFxOk(float carMass, float propMass, float closing, int broken) {
  if (PROPFXIMP_SV_OLD) return 1;
  if (broken) return 0;
  if (propMass <= 0.f) propMass = 100.f;
  return carMass * propMass / (carMass + propMass) * closing * 1.1f > 5.f * propMass;
}
extern int propsHotRed;   // красный контроль стенда (flag prophotred 1)
extern int propsHot108Old;   // 108 О6/О9: 1 — прежние обходы по PropState (flag prophot108old)
extern unsigned propsHitIter;   // №1086: обойдено экземпляров в propsHitCar (служебный счётчик стенда)
extern int propsKickOther;   // круг 3 (g11): 1 — удар наносит машина трафика или ИИ, а не игрока
extern int pole1046Old;   // №1046: 1 — столб валится вяло, как в TEST73 (flag pole1046 0)
extern int propsPoleKick1276Old;   // 107б П6: 1 — толчок бампера 1046, как в 106 (flag pole1276old)
float propsPoleKick1046(float v, float w0, float wk, float v0);   // №1046: начальная угловая скорость столба от скорости машины (толчок бампера)
extern int propsTallFallOld;   // №946: 1 — высокий предмет падает медленно, как было (flag tallfall946old)
typedef struct { float ox, oy, oz, sc; int tex, flags; unsigned nv, ni, vOff, iOff; } PropPiece;  // кусок типа (файл psp_export.py)
typedef struct { float radius, halfH, mass; unsigned firstPiece, nPiece; } PropType;               // тип пропа
// 107в П4 (сверка физики Fable-2, закон 1276б): объёмы столкновения типов предметов из данных диска (CollisionObject: ящик 0, сфера 1, цилиндр 2), таблица PVL1 в хвосте PRP2 (@60, tools/psp_export.py propvol_table).
// Объём в осях модели (зеркало z как у вершин): центр, оси строками, полуразмеры. Контакт машины с предметом берёт объёмы вместо ствола по вершинам / круга / тонкой коробки. PROPVOL107_OLD=1 / «flag propvol107old 1» — прежнее
typedef struct { unsigned char kind, pad[3]; float c[3], a[9], h[3]; } PropVol;   // 64 Б
typedef struct { float x, y, z, q[4]; unsigned type; } PropDef;                                     // как расставлено на уровне

typedef struct {
  float x, y, z;          // текущее положение ЦЕНТРА коробки пропа (не начала модели)
  float cx, cy, cz;       // центр коробки в осях модели (начало модели обычно у основания)
  float hx, hy, hz;       // полуразмеры коробки в осях модели
  float q[4];              // ориентация (w, x, y, z)
  float vel[3], w[3];      // скорость центра, угловая скорость (мир)
  int asleep;               // 1 — спит, не считаем, рисуем в последней позе
  float gOff; int fresh;    // насколько низ коробки утоплен в землю в покое (не выталкиваем при пробуждении)
  float sleepT;              // сколько подряд почти без движения — для засыпания
  int hidden;                // 1 — спрятан (вместо него играет обломок BreakableObject), не рисовать, не считать
  float home[3], away, hitCool; // исходный центр, время вне вида, защита от повторного неудачного удара
  int noRespawn;              // владелец миссии/коллекции запрещает фоновое восстановление
  float fadeT;                // №589: время после сбития для ухода в фейд
  int pole;                   // №968: 1 — столб падает на шарнире у основания (кинематика по PS2), 2 — упал; 0 — обычная физика коробки
  float poleTh, poleW, poleB[3], poleD[2], poleQ0[4];   // №968: угол от вертикали и его скорость, основание, направление падения (xz), исходная ориентация
  int kicked;                 // сверка g11 P11-02: предмет уже тронут машиной (повторное касание на малой скорости его не будит); сбрасывается возвратом домой
  int inkTouch;               // №1096: предмет тронул ИГРОК (его машина или взрыв) — контур мультика у него закреплён до возврата домой; машины трафика и ИИ его не ставят
  int gone;                   // сверка g11 P11-17/Д6: ударенный высокий предмет (тип 10) вышел из вида и убран из мира до перезагрузки зоны (hidden=1 навсегда)
} PropState;

// 108 О6 + О9 (Бабл, по документу оптимизации Fable 13.11): горячий массив предметов, 16 байт на предмет (быстрая память PSP 16 КБ, запись PropState 184 байта: шаг и возврат каждый кадр
// обходили все предметы уровня и на каждом платили промахом мимо кэша ради «ничего не делать»). Здесь положение центра и три признака, выведенных из PropState (источник остаётся PropState,
// горячий массив его копия): PH_HID — спрятан (шаг пропускает); PH_STEP — шагу есть что делать (не спит, или hitCool > 0, или лежит сбитый: !fresh и не миссионный — копит fadeT); PH_RESP — возврату
// есть что делать (не «целый на месте» и не миссионный, либо away != 0). Целый предмет на месте (почти все): шаг только проверяет расстояние до игрока по x, z из массива, возврат его не трогает.
// Каждое место, где пишутся x, y, z, hidden, asleep, fresh, noRespawn, hitCool, away, зовёт propsHotSync(i); propsHotVerify — сверка для стенда и смоука (flag prophotchk 1).
// PROPHOT108_OLD=1 / «flag prophot108old 1» / файл prophot108old — прежние обходы по PropState
typedef struct { float x, y, z; unsigned fl; } PropHot;
#define PH_HID 1u
#define PH_STEP 2u
#define PH_RESP 4u
#define PH_VERMASK 0xFFFF0000u   // 108 О7: версия записи: каждая запись в предмет (propsHotSync) увеличивает её; запечённые листы (propbake.h) не читают PropState предмета, пока версия та же
#define PH_VERINC 0x10000u

typedef struct {
  PropPiece *pieces; unsigned npiece;
  PropType *types; unsigned ntype;
  PropDef *def; PropState *st; unsigned ninst;
  void *vertBase; unsigned vertBytes;         // общий блоб вершин (формат Vert из main.c: u,v,c,x,y,z,pad — 16 Б)
  unsigned short *idxBase; unsigned idxBytes;  // общий блоб индексов (16-бит)
  void *mem;
  int coinType; unsigned ncoin; const float *coinPos;   // монеты: тип-меш (-1 нет), число и точки x,y,z
  const void *spr; unsigned sprSize;           // PRP2: подсекция SPR1 (стейтпропы, stateprops.h), иначе NULL
  // PRP2 @44/@48: частицы удара по типу — 2 Б на тип: [0 нет, 1 искры кодом (eSpark), 2 звёзды (не рисуем),
  // 3..19 номер InstParticleSystem (3 куст, 4 мусор, 5 листья дуба, 6 почта)], [обломок BreakablesEnum, 0xFF нет]. NULL — нет
  const unsigned char *typeFx; unsigned ntypeFx;
  // удар машиной по спящему пропу i (направление удара dirx, dirz). Вернуть 1 — проп спрятать (играет обломок)
  int (*onKick)(void *user, unsigned i, float dirx, float dirz); void *kickUser;
  CarBox previousCar; int havePreviousCar;
  // №1086 (g11): предметы рядом с игроком — список раз в кадр (propsUpdate), чтобы машины трафика и ИИ не перебирали все предметы уровня: в список идут предметы в радиусе PROP_NEAR_R от игрока и все
  // не спящие; nearOk — список построен; ownCar — машина игрока (остальные — «чужие»: слабый вызов, без шкалы Hit & Run)
  unsigned *nearList; unsigned nearN; float nearC[2]; int nearOk; const Car *ownCar;
  PropHot *hot;            // 108 О6/О9: горячий массив (см. PropHot); NULL — нет памяти, прежние обходы
  const PropVol *vol; const unsigned short *volIdx; unsigned nvol;   // 107в П4: PVL1 (NULL — таблицы нет, прежний контакт); volIdx[2*тип] — первый объём, [2*тип+1] — число
  float *trunk;            // №488: по типу [x, z центра ствола в осях модели, радиус ствола] в полосе кузова; радиус 0 — не тонкий, бьёт по ty->radius
} Props;

// section — PRP1 (заголовок 32 Б) или PRP2 (64 Б: первые 32 как PRP1; @32 смещение SPR1, @36 её размер, @40 размер
// всей секции; @44 смещение таблицы частиц типов, @48 число типов — 0 нет). 0 = ок, <0 = ошибка формата/памяти
int propsInit(Props *p, const void *section, unsigned size);
void propsHotSync(Props *p, unsigned i);   // пересчитать запись горячего массива предмета i после записи в PropState
unsigned propsHotVerify(const Props *p);   // сколько предметов горячий массив описывает неверно (0 — верно)
extern unsigned propsHotBad; extern int propsHotChk;   // сверка каждый кадр (flag prophotchk 1)
extern unsigned propsHotMiss;              // счётчик стенда: предметов, прочитанных из PropState (медленный путь) в шаге и возврате
// сверка g11 (props.c): толчок вверх всем подвижным предметам в шаре r вокруг точки (RADIATE_FORCE статуй Иедедайи: сила 200 / масса); возврат — сколько тронуто
int propsRadiate(Props *p, float x, float y, float z, float r, float force);
void propsFree(Props *p);
// частицы удара типа t из таблицы PRP2 (*particle: 0 нет, 1 искры, 2 звёзды, 3.. InstParticleSystem; *breakIdx: -1 нет)
void propsTypeFx(const Props *p, unsigned t, int *particle, int *breakIdx);
// Ближайшая точка текущей коробки пропа к точке мира; учитывает высоту и поворот.
void propsClosestPoint(const Props *p, unsigned i, const float point[3], float closest[3]);
// шаг кадра
void propsUpdate(Props *p, const ColWorld *world, Car *car, float dt);
// Several actors: advance the shared objects once, then submit each actor's own sweep.
void propsAdvance(Props *p, const ColWorld *world, float dt);
void propsHitCar(Props *p, const ColWorld *world, Car *car, const CarBox *previous);
extern int glassSolid1224;   // №1224: 1 — целое стекло-проп (обломок 19) твёрдое для Гомера пешком, разбитое — проход (GLASS1224_OLD=1 / файл glass1224old / flag glass1224 0 — как было)
extern int bench1034;   // №1034: 1 — скамейки, баки, большие ящики твёрдые для Гомера пешком и держат сверху (BENCH1034_OLD=1 / flag bench1034 0 — как было)
int propsSolidHomer(const Props *p, float *x, float *z, float y, float r, float stepH, float bodyH, float *top, float *onx, float *onz);   // правка 290: пропы твёрдые для Гомера
// Обычные пропы возвращаются только далеко и вне вида (исходное И текущее место), после 5 с
void propsRespawnStep(Props *p, float ex, float ez, float dt,
                      int (*seen)(void *, float, float, float, float), void *user);
// сверка g09b-menu круг 5 (P12-03): перезагрузка зон — все обычные пропы (кроме миссионных и ещё целых) сразу возвращаются на свои места, как после выгрузки и новой загрузки зон
void propsZoneReload(Props *p);
// матрица кусочка пропа для GE (как carPieceMatrix): мир = T + R(q) * (o + sc*v), столбцы как ScePspFMatrix4
void propPieceMatrix(const PropState *st, float ox, float oy, float oz, float sc, float m[16]);

#ifdef __cplusplus
}
#endif
#endif
