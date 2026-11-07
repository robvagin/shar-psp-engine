// Визуальный слой машины игрока (docs/car_spec.md, п.1–5 и 7): колёса (вращение, руль, ход подвески),
// шарнирные капот/двери/багажник, битые текстуры по зонам урона, стоп-сигналы и задний ход (аддитивные блики),
// мягкая тень-пятно на земле, остов huska при взрыве. Данные — секция CAR1 level.shr (@100 заголовка SHR7).
// car.c не трогаем: всё считается из того, что он уже даёт (comp[], eq, lim, steer, v, vel, slip, damage, destroyed, q).
// Модель машины в осях SHR: +X вправо, +Y вверх, смотрит в -Z; начало модели на высоте lift над низом колёс.
#ifndef SHAR_CARVIS_H
#define SHAR_CARVIS_H
#include "car.h"
#include "col.h"

#define CARVIS_MAXJ 32
#define CARVIS_MAXH 4
#define CARVIS_MAXD 8
#define CARVIS_MAXF 16

typedef struct { char name[12]; float x, y, z; unsigned parent; } CarJoint;                          // 28 Б
typedef struct { unsigned char joint, axis; signed char sign; unsigned char zone; float max, stiff, cx, cy, cz; } CarHinge;   // 24 Б
typedef struct { unsigned short texNorm, texDam; unsigned char zone, pad[3]; } CarDam;              // 8 Б
typedef struct { unsigned char joint, kind, mode, pad; unsigned short tex, pad2; unsigned abgr;
                 float w, h, dist, ox, oy, oz; } CarFlare;                                           // 36 Б

typedef struct {
  // --- из CAR1 ---
  int ok, njoint, nhinge, ndam, nflare;
  float lift, huskLift, wheelRad, maxSteer, shadowAdj[8];
  CarJoint joint[CARVIS_MAXJ]; CarHinge hinge[CARVIS_MAXH]; CarDam dam[CARVIS_MAXD]; CarFlare flare[CARVIS_MAXF];
  int wheelJ[4];            // суставы колёс в порядке car.comp[]: FL FR RL RR (w3 w2 w1 w0)
  int slJ;                  // сустав `sl` (точка дыма над капотом), -1 нет
  unsigned hideJ;           // правка 354: бит j — кусок на суставе j не рисуется (колёса-заглушки, carwheels.h); carVisSetCar
  float wheelUp[4];         // правка 354: подъём колеса над покоем не больше (FL FR RL RR), м; carVisSetCar
  // --- состояние ---
  float spin[4];            // угол вращения колёс (FL FR RL RR), рад
  float th[CARVIS_MAXH], thv[CARVIS_MAXH];   // угол и скорость шарниров
  int latched[CARVIS_MAXH]; // 1 = закрыт на замок (целый), 0 = отщёлкнут и болтается
  float zoneDam[4];         // накопленный урон по зонам: 0 перед, 1 зад, 2 левый борт, 3 правый
  float prevDamage, prevVel[3]; // velocity of the rendered model origin, not the shifting virtual COM
  int motionValid;            // first sample after creation/repair has no measured acceleration
  int brake, reverse;       // горят стоп-сигналы / задний ход
  // №433: one receiver plane per car, not sixteen competing COL hits
  float shadowX, shadowZ, shadowY, shadowSlopeX, shadowSlopeZ, shadowFade, shadowDt;
  const ColWorld *shadowWorld;
  int shadowValid;
  // №522: высоты тени по узлам сетки 6x6 (опора под каждым узлом, не одна плоскость): держатся между кадрами, растут сразу, спадают за 100 мс
  float shadowH[36]; unsigned char shadowHok[36]; int shadowGrid;
  int capStill;   // №878: диск колеса не крутится (хром с переливом, carwheels.h carWheelCapStill)
  // №1159: кэш матриц на кадр (carVisPieceMatrixEx зовут на каждый кусок — 30–40 раз за кадр, у колёс по 4 sin/cos каждый раз). Ключ — входные числа; совпали — берём готовое
  const void *cKeyCar; float cKey[8], cB[16]; int cOk;   // матрица тела: машина, x y z, кватернион, подъём
  float wKey[4][3], wR[4][9]; int wOk[4];                   // поворот колеса: руль, качение, сустав
} CarVis;

// разбор секции CAR1 (буфер можно освободить после вызова); 0 = ок
int carVisInit(CarVis *v, const void *sec, unsigned size);
// правка 354: машина по имени (.con): скрытые колёса и запас хода вверх из carwheels.h (после carVisInit)
void carVisSetCar(CarVis *v, const char *name);
// правка 354: кусок на этом суставе не рисуется (скрытое колесо оригинала)
static inline int carVisJointHidden(const CarVis *v, int joint) { return joint >= 0 && joint < 32 && (v->hideJ >> joint & 1u); }
// сброс состояния (ремонт, START)
void carVisReset(CarVis *v);
// шаг кадра после carUpdate: колёса, шарниры, зоны урона, фонари
void carVisUpdate(CarVis *v, const Car *c, const CarInput *in, float dt);
void carVisSpin(CarVis *v, const Car *c, float dt);   // №955б: только качение колёс (машина миссии/угнанная под игроком: carVisUpdate для неё не зовётся)
// матрица куска машины (столбцы, как ScePspFMatrix4): полная ориентация кузова * анимация сустава.
// variant 1 (остов) рисуется со своим huskLift.
void carVisPieceMatrix(const CarVis *v, const Car *c, int joint, int variant, float ox, float oy, float oz, float sc, float m[16]);
void carVisPieceMatrixEx(const CarVis *v, const Car *c, int joint, int variant, float ox, float oy, float oz, float sc, int noSpin, float m[16]);   // №867: noSpin — без качения колеса
// ТЕСТ90: на время рисования кусков машины в кадре общая часть матриц (поворот сустава и его столбцы 3x3) считается один раз на сустав и масштаб, а не на каждый кусок.
// Скобка: carVisMtxScope(v, c, 1) перед циклом кусков, (0,0,0) после; внутри машина и суставы не меняются. Числа те же, что при расчёте заново (проверка tests/g01cars/carmtx.c).
// CARMTX_OLD=1 / файл carmtxold рядом с EBOOT / «flag carmtxold 1» — прежний расчёт на каждый кусок
extern int carMtxOld; extern unsigned long carMtxCoreN;
void carVisMtxScope(const CarVis *v, const Car *c, int on);
int carVisWheelJoint(const CarVis *v, int joint);   // №867: сустав — колесо
// рисовать ли кусок этого варианта сейчас (машина целая / остов)
int carVisVariantOn(const Car *c, int variant);
// подмена текстуры по урону (целая -> битая)
int carVisTex(const CarVis *v, int tex);
// мировая точка сустава (с наклоном кузова), 0 = нет такого сустава
int carVisJointWorld(const CarVis *v, const Car *c, int joint, float out[3]);
// тень-пятно на земле (без текстуры, мягкий край). Меняет состояние GE (после — resetState в main.c)
void carVisDrawShadow(CarVis *v, const Car *c, const ColWorld *w);
// блики стоп-сигналов / заднего хода; bind — привязка текстуры level.shr (bindTexLean из main.c)
void carVisDrawFlares(const CarVis *v, const Car *c, float ex, float ey, float ez, void (*bind)(int tex));
// №465: один блик (куски owner 3 моделей трафика и машин миссий): P — место в мире, U — вверх машины (режим LYAX, mode 1), иначе вверх мира; ex, ey, ez — глаз
void carFlareOne(const float P[3], const float U[3], int mode, float w, float h, float dist, unsigned abgr, int tex, float ex, float ey, float ez, void (*bind)(int tex));

#endif
