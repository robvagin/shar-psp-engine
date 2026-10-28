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

extern int propsPole968Old;   // №968: 1 — столб (высокий предмет без обломка) падает физикой коробки, как до правки (flag pole968old)
extern int pole1046Old;   // №1046: 1 — столб валится вяло, как в TEST73 (flag pole1046 0)
float propsPoleKick1046(float v, float w0, float wk, float v0);   // №1046: начальная угловая скорость столба от скорости машины (толчок бампера)
extern int propsTallFallOld;   // №946: 1 — высокий предмет падает медленно, как было (flag tallfall946old)
typedef struct { float ox, oy, oz, sc; int tex, flags; unsigned nv, ni, vOff, iOff; } PropPiece;  // кусок типа (файл psp_export.py)
typedef struct { float radius, halfH, mass; unsigned firstPiece, nPiece; } PropType;               // тип пропа
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
} PropState;

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
  float *trunk;            // №488: по типу [x, z центра ствола в осях модели, радиус ствола] в полосе кузова; радиус 0 — не тонкий, бьёт по ty->radius
} Props;

// section — PRP1 (заголовок 32 Б) или PRP2 (64 Б: первые 32 как PRP1; @32 смещение SPR1, @36 её размер, @40 размер
// всей секции; @44 смещение таблицы частиц типов, @48 число типов — 0 нет). 0 = ок, <0 = ошибка формата/памяти
int propsInit(Props *p, const void *section, unsigned size);
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
extern int bench1034;   // №1034: 1 — скамейки, баки, большие ящики твёрдые для Гомера пешком и держат сверху (BENCH1034_OLD=1 / flag bench1034 0 — как было)
int propsSolidHomer(const Props *p, float *x, float *z, float y, float r, float stepH, float bodyH, float *top, float *onx, float *onz);   // правка 290: пропы твёрдые для Гомера
// Обычные пропы возвращаются только далеко и вне вида (исходное И текущее место), после 5 с
void propsRespawnStep(Props *p, float ex, float ez, float dt,
                      int (*seen)(void *, float, float, float, float), void *user);
// матрица кусочка пропа для GE (как carPieceMatrix): мир = T + R(q) * (o + sc*v), столбцы как ScePspFMatrix4
void propPieceMatrix(const PropState *st, float ox, float oy, float oz, float sc, float m[16]);

#ifdef __cplusplus
}
#endif
#endif
