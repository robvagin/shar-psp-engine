// Машина: твёрдое тело (v10). Масса, центр масс, тензор инерции, 4 луча-колеса с пружиной и амортизатором,
// шины со сцеплением и заносом, «неваляшка», удары о стены и землю импульсами (отскок, закрутка, подброс).
// Все ходовые параметры — из .con машины (viewer/carparams.h, tools/con2h.py). Чистый C99, float, без malloc.
// Мир: метры, y вверх; курс h: вперёд = (sin h, cos h), руль +1 = вправо (h уменьшается), как в main.c.
// Оси тела: x — влево, y — вверх, z — вперёд (при курсе h поворот тела = поворот вокруг Y на h).
// Модель машины смотрит в -Z и рисуется матрицей carPieceMatrix (полная ориентация тела).
#ifndef SHAR_CAR_H
#define SHAR_CAR_H
#include <stdlib.h>
#include <math.h>
#include "col.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  // --- из .con машины (carparams.h: единицы и смысл; carParamsFor переводит км/ч -> м/с, градусы -> рад) ---
  float mass, gasScale, slipGasScale, hsGasScale, gasSpeedThreshold, brakeScale;
  float bodyMass, bodyInertia[6]; // native rigid body; mass remains the nominal .con value (XX XY XZ YY YZ ZZ per kg)
  float bodyCentre[3], modelLift; // original rest-pose centre, relative to the model origin
  float wheelCentreWeight[4]; // whole articulated centre's wheel volume fractions, FL FR RL RR
  float bodyOriginInertia[6], wheelCentre[4][3]; // source whole tensor about model origin and wheel centres
  float wheelCollision[4][4]; // authored sphere centre in model/body axes and radius; FL FR RL RR
  float wheelCollisionOffset[4][3]; // authored sphere centre minus wheel joint pivot, body axes
  float topSpeed;                // м/с
  float maxWheelTurn;            // рад
  float hsSteeringDrop, tireGrip, normalSteering, slipSteering, eBrakeEffect, slipSteeringNoEBrake, slipEffectNoEBrake;
  float cmX, cmY, cmZ;           // смещение центра масс от центра кузова (x влево, y вверх, z вперёд), м
  float suspensionLimit, springK, damperC, suspensionYOffset, hitPoints, burnoutRange, donutTorque;
  float weebleOffset, wheelieRange, wheelieY, wheelieZ;
  // --- геометрия модели: машина уровня из CAR1 v2 (carParamsFromCar1, правило tools/psp_export.car_info); без неё —
  //     famil_v (кузов 2.21 x 5.35 м без зеркал) ---
  float halfW, halfL, bodyOfs;   // полуширина, полудлина, сдвиг центра кузова вдоль (м, минус = назад)
  float wheelX, wheelF, wheelR;  // колёса: вбок, передняя ось вперёд, задняя ось назад (от начала машины)
  float wheelRad;                // радиус колеса, м
  float bodyLo, bodyHi;          // низ днища и крыша над землёй в покое, м (точки кузова для ударов о землю)
  float bandLo, bandHi;          // твёрдый слой кузова для стен (colPushCar): ниже — бордюры (колёса заезжают), выше — стены
  float step;                    // на какую ступеньку можно въехать
  // --- наша настройка отклика (не из .con) ---
  float gravity;                 // м/с2
  float restitution;             // отскок от стены (доля скорости удара) для машины 1500 кг; масса m: * sqrt(1500/m)
  float wallFriction;            // трение кузова о стену (скольжение вдоль, закрутка)
  float groundRestitution, groundFriction;   // кузов о землю (посадка на днище/крышу/бок)
  float pop, popMax;             // подброс при сильном ударе: вертикальный импульс = pop * удар, не больше popMax м/с
  float hitY;                    // высота точки удара о стену над низом колёс, м
  float angDamp;                 // затухание вращения, 1/с
  float airLevel;                // выравнивание в воздухе: доля критического затухания качания «неваляшки»
  float steerResp;               // отклик руля, 1/с
  float reverseSpeed;            // предел скорости назад, м/с
  float fallReset;               // legacy height-only reset threshold (CAR_LANDING_RESET_OLD negative control)
  float sub;                     // фиксированный подшаг, с
  // --- посадка и камера машины (CAR1 v2; без неё — famil_v): оси модели SHR (+X вправо, смотрит в -Z, y от начала модели) ---
  float seat[3];                 // сиденье водителя (сустав dl)
  int seatData;                  // №666: сиденье seat[] прочитано из блока машины (CAR1/MCR1), а не запасное famil_v
  float seatAx, seatAz;          // якорь посадки Гомера: конец клипа посадки ставит его на сиденье (y якоря — земля, -lift)
  float doorX, doorZ;            // точка у двери водителя, откуда начинается посадка (снаружи коробки кузова)
  float camElev, camMag;         // камера за машиной: FollowCameraData (угол от вертикали, рад; дистанция, м)
  char name[12];                 // имя машины (.con)
} CarParams;

// газ 0..1, тормоз/назад 0..1, руль -1..1 (+ вправо), ручник 0/1 (необязательно: main.c задаёт три поля — будет 0)
typedef struct { float throttle, brake, steer, handbrake; } CarInput;

#define CAR_SAFE 16

typedef struct {
  // --- то, что читает main.c ---
  float x, y, z, h;         // начало машины (низ колёс в покое, начало модели) и курс для камеры
  float v, vl, vy;          // скорость вдоль курса (со знаком, для км/ч), вбок (вправо +), вверх
  float gx, gz;             // «пол» кузова: плоскость по оси вверх тела, Y(X,Z) = y + gx*(X-x) + gz*(Z-z)
  float pitch, roll;        // рад: нос вверх +, правый борт вверх +
  float steer;              // сглаженный руль
  float wheelSteer, wheelSpin[4]; // угол поворота передних колёс в физике и угол качения колёс, рад
  int grounded, wheels;     // касается земли (колесом или кузовом); колёс на земле
  float ry, rvy, rpitch, rroll;   // ry — сглаженная высота для камеры
  // --- твёрдое тело ---
  float q[4];               // ориентация (w, x, y, z): тело -> мир
  float vel[3], w[3];       // скорость центра масс, угловая скорость (мир)
  float com[3];             // центр масс в осях тела сейчас (с вилли)
  float comp[4];            // сжатие подвески колёс FL FR RL RR, м (0 — колесо в воздухе)
  float modelRaise;         // model origin offset from native suspension equilibrium
  int slip;                 // 0 сцепление, 1 занос на газу, 2 ручник
  // --- урон ---
  float damage;             // 0..1 (1 = разбита)
  int destroyed;            // damage достиг 1
  float deadT;              // правка 199: секунд с разбития (0 у целой); взрыв — CAR_BOOM_DELAY после разбития (carBoomed)
  float lastHit;            // скорость последнего засчитанного удара, м/с
  float hitEpD0, hitEpAge;  // №176 повтор: урон до удара игрока (начало «одного удара») и секунд с последнего удара игрока (traffic.c, carRamDamage)
  float stPitch, stThPrev; int stInit;   // №164: сглаженный наклон земли вдоль курса и прошлый подшаг (потеря скорости на въезде на крутой скат)
  float hitDmgK;            // множитель урона машина-машина, который получает эта машина; 0 — CAR_HIT_DMG_K (правка 176:
                            // трафик и машина игрока); машины миссии — 1 (цель тарана m3 — как до правки 176, ночь ботов 03.10)
  // --- последний кадр: контакты ---
  int contacts;             // контактов со стенами в последнем подшаге
  float cnx[4], cnz[4];     // их нормали (от стены к машине)
  unsigned char ctype[4];   // и типы (COL_TRI / COL_BOX / COL_CYL / COL_SEG)
  int quietSteps;          // подшагов подряд в покое на опоре (quietSupport); от CAR_REST_SKIP_STEPS carUpdate не считает подшаги (поза и скорость те же)
  float restPose[7];       // поза покоя (x, y, z, q), с которой начался пропуск
  int parkN, parkTick;     // ПРОТОТИП «Разбор x_car3d»: кадров подряд пустая машина на ручнике не сдвинулась (≤ CAR_PARK_EPS); номер кадра в пачке пропуска
  float parkPose[7];       // поза в конце последнего посчитанного кадра покоя (x, y, z, q)
  float fenceT;            // №487: сколько ещё с после касания забора с лица вперёд боковая жёсткость шин ослаблена (CAR_FENCE_LAT)
  float hitSpeed;           // самый сильный удар за кадр (стена или земля), м/с
  // самый сильный удар о стену мира за кадр (только наблюдение, для переписи стен tools/cartest/walls.c): скорость
  // сближения м/с (0 — не было), примитив COL1 (тип, номер), нормаль xz, точка удара на полу кузова, материал
  float wallV, wallN[2], wallP[3]; int wallType, wallMat; unsigned wallIdx;
  // правка Роба 135 (звук удара, viewer/carsnd.h): за кадр — сильнейший удар о твёрдое (стена, ствол, столб; не днище о землю)
  // и о листву (материал FOLIAGE), м/с; удар машина-машина (carHitCar, в том числе на крышу) и его точка. hitSpeed не меняется
  float hardV, foliV, carHitV, carHitP[3];
  float velPre[3];          // №923: скорость в начале carUpdate (до стен и столкновений кадра): направление, куда летит разбитое стекло — по ходу машины к удару, а не после гашения стеной
  float pen;                // остаточное проникновение после подшага, м
  int reverts, recoveries;  // подшаги, отменённые целиком; возвраты на надёжное место (всего)
  int crashes;              // правки 164/187: удары с гашением скорости, как у оригинала (car.c CAR_CRASH_V)
  int revWhy;               // причина последней отмены: 1 не вытолкнулось, 2 за забором, 4/8 путь через забор, 16 край мира,
                            // 32 земля выше, чем можно въехать
  int rights;               // сколько раз перевёрнутую машину поставили на колёса
  float foliageCooldown;   // no repeated pumping by a held throttle
  unsigned foliageBounces; // observed impact events for regression checks
  float foliCover;          // правка 304: доля кузова в листве на последнем подшаге (0..1), гейт tests/foliage/cover.c
  // --- опора-машина (пункт 8): крыша другой машины как земля для колёс и днища; ставит carHitCar, живёт один кадр ---
  int supOn;                                         // 1 — в этом кадре под машиной крыша другой машины
  float supY, supX, supZ, supFx, supFz, supHW, supHL; // высота крыши и её прямоугольник в xz (центр, ось вперёд, полуразмеры)
  float supV[3];                                     // скорость крыши (трение колёс — относительно неё)
  // --- служебное ---
  float acc, t, airT, noGroundT, embedT, bellyT, flipT, nofitT, strandT, fallTop, fallT0, fallPendT; int fallPending, bodyHit;
  struct { float x, y, z, h, t; } safe[CAR_SAFE]; int nsafe, safeHead; float safeT;
  float homeX, homeY, homeZ, homeH;
  float kS[4], cS[4], eq, lim, mountY, inertia[3], invInertia[6], comBase[3];   // производные от параметров (carInit)
  float bodyTensor[6]; // operative whole-car tensor, updated from wheel positions, kg m²
  CarParams p;
  int flightZone;           // jump-zone of this uninterrupted flight; -1 after landing/placement
  int boosts, boostZone;    // зоны прыжка JMP1: подшаги разгона зоны (правка 342; прежнее правило 47 — отрывы с подбросом), номер последней зоны (-1 — не было)
  uint32_t fkeep[COL_FOL_KEEP]; int nfkeep;     // правка 304: заборы-изгороди, пропущенные на прошлом подшаге (col.h colFolKeep)
  uint32_t hkeep[COL_HUMP_KEEP]; int nhkeep;   // правка 200: капсулы-горбы, над которыми кузов был на прошлом подшаге (col.h colHumpKeep)
  uint32_t bkeep[COL_BACK_KEEP]; int nbkeep;   // сверка PS2 09.10: заборы, пропущенные с изнанки на прошлом подшаге (col.h colBackKeep)
  float glideT, gx0, gy0, gz0, gx1, gy1, gz1;   // плавный выход с кромки: оставшиеся секунды, откуда, куда
  float perchT, perchX, perchZ; int perchEsc;
  float wedgeT, wedgeX, wedgeZ, wedgeH; int wedgeTried, wedgeEsc;   // №738: зажата ровно (ящик, стена дома): секунд без хода, где, пробовали вперёд (1) и назад (2), сколько раз вывело   // зависание на кромке (PERCH_ESCAPE): секунд без хода под вводом при крене/клевке, где началось, сколько раз вывело
  int stuck32, escapes32;   // №тупик why 32: подряд отменённых подшагов «земля выше» и сколько раз машину выдавило наружу (DEADEND32_OLD)
  int deadendLifts;         // ДОБИВКА 18 (car.c DEADEND_OLD): сколько раз машину из-под поверхности (тупик В87) поставили на неё
  int huskDrive;            // правка 355: машина игрока — её остов после взрыва едет (carHuskDrives); у трафика и миссий 0
  float brkLit;             // №465: 1 — стоп-сигналы горят (тормоз / ждёт у стоп-линии), ставит carUpdate и viewer/traffic.c; рисует trafficDrawModel
} Car;

#ifndef CULL_SPHERE_OLD
#define CULL_SPHERE_OLD 0   // №544/546 включены по умолчанию (слово Роба 27.10: машины, что бьют игрока, видны всегда; грузовик не пропадает у края кадра; выключили без слова Роба 15.10); прежнее -DCULL_SPHERE_OLD=1 / -DNEAR_CAR_DRAW_OLD=1; TEST17 (слово Роба/дирижёра 15.10: отрисовка как в 1721 f32c8c31): прежний радиус; 0 — полная сфера кузова №546
#endif

// №546: полная 3D ограничивающая сфера машины с запасом 1.5 м (длинные грузовики glastruc/schoolbu/garbage не режутся на кромке)
static inline float carSphereR(const Car *k) {
#if CULL_SPHERE_OLD
  return k->p.halfL + fabsf(k->p.bodyOfs) + .5f;
#else
  float hl = k->p.halfL > 0.1f ? k->p.halfL + fabsf(k->p.bodyOfs) : 3.0f;
  float hw = k->p.halfW > 0.1f ? k->p.halfW : 1.2f;
  float H = (k->p.bodyHi > k->p.bodyLo) ? (k->p.bodyHi - k->p.bodyLo) : (k->p.bodyHi > 0.5f ? k->p.bodyHi : 2.0f);
  float hh = H * 0.5f;
  // №546: описанная сфера = половина диагонали √(L²+W²+H²)/2 + 1.5 м запаса
  return sqrtf(hl * hl + hw * hw + hh * hh) + 1.5f;
#endif
}

// №544: проверка близости двух машин в плоскости с учётом полных сфер
static inline int carNear2(const Car *a, const Car *b, float extra) {
  float dx = a->x - b->x, dz = a->z - b->z, r = carSphereR(a) + carSphereR(b) + extra;
  return dx * dx + dz * dz < r * r && fabsf(a->y - b->y) < 4.f;
}

// параметры машины по имени .con (famil_v, schoolbu, ...): 1 = нашлась, 0 = нет (тогда famil_v)
int carParamsFor(const char *name, CarParams *p);
extern float carRollGain;   // файл rollps2 рядом с EBOOT: коэффициент крена (0 — как без файла, добавка к плечу крена, м); carRollGainParse — разбор содержимого
#define CAR_ROLLPS2_GAIN 0.22f   // м: добавка к плечу бокового крена (файл rollps2 без числа)
static inline float carRollGainParse(const char *s) { float k = 0.f; char *e = 0; if (s && *s) { k = (float)strtod(s, &e); if (e == s) k = 0.f; } return (k > 0.f && k <= 1.f) ? k : CAR_ROLLPS2_GAIN; }   // пустой/неверный файл — значение по PS2
// правка 333: 1 — у машины в .con SetIrisTransition(1) (carparams.h carIrisCons): посадка и выход ирисом оригинала, без клипов двери
int carIrisFor(const char *name);
void carDefaults(CarParams *p);   // famil_v
// машина миссии: физика ИИ-водителя из .con AddStageVehicle ("Missions\level01\M1race.con"), имя и запасные ходовые — машины
// car ("skinn_v"). 1 = .con водителя найден; 0 = нет на диске — ходовые машины car (или famil_v). Геометрия — famil_v
int carParamsForAi(const char *con, const char *car, CarParams *p);
// блок машины CAR1 v2 (128 Б: имя .con, кузов, сиденье, дверь, камера) — тот же, что внутри секции CAR1, и запись таблицы машин
// пакета msncars-l<N>.shr (MCR1 v2; машина, которую сценарий даёт игроку). -> 1 / -3 / -4, как carParamsFromCar1
int carParamsFromInfo(const void *info, CarParams *p);
// машина уровня из секции CAR1 level.shr (@100 заголовка SHR7): версия 2 несёт имя машины (-> ходовые из её .con) и кузов,
// сиденье, дверь, камеру по данным. 1 = прочитано; 0 = секция версии 1 (p = famil_v, как было); < 0 — битая секция
// (-1 не CAR1, -2 блок за концом, -3 машины нет в carparams.h, -4 числа вне разумного) — p = famil_v
int carParamsFromCar1(const void *sec, unsigned size, CarParams *p);
const char *carParamsError(int e);
// поставить машину (x, z, курс h) на землю под высотой y + 2 м, ровно и без скорости (START, телепорт, тест)
void carPlace(Car *c, const ColWorld *w, float x, float y, float z, float h);
// RESET CAR: последнее надёжное место (снимается из истории), иначе выпрямить на месте; урон сохраняется
void carResetNearby(Car *c, const ColWorld *w);
// инициализация: параметры (или NULL = famil_v) + carPlace; эта же точка — запасной «дом» для возврата
void carInit(Car *c, const CarParams *p, const ColWorld *w, float x, float y, float z, float h);
// шаг кадра: dt любой (режется на подшаги p.sub, не больше 0.1 с за вызов)
// Правка Роба 199 (03.10, «недоработана анимация, что они взрываются»). Оригинал (близнец boom.py, PS2, 50 к/с): после
// смертельного удара машина больше не едет и горит (огонь, чёрный дым), Гомера выбрасывает через ~0,75 с (правка 147), а взрыв —
// вспышка, обломки, облако дыма и остов — через 3,0 с после удара (кадры 3360 -> 3512). Было: взрыв и остов в кадр удара
#ifndef CAR_BOOM_DELAY
#define CAR_BOOM_DELAY 3.0f
#endif
static inline int carBoomed(const Car *c) { return c->destroyed && c->deadT >= CAR_BOOM_DELAY; }
// Правка 355 [PS2, близнец a355_huskdrive2]: в остов (после взрыва) сесть МОЖНО — △, без двери и ириса, ~0,7 с; остов едет
// 4–5 м/с (huska ~20 км/ч). До взрыва (3 с горит) — не едет и не садится, как в правке 199. CAR_HUSK_OLD=1 — прежнее: остов
// не едет, в него не сесть (красный контроль tests/husk/run.sh)
#ifndef HUSK_BURN_OLD
#define HUSK_BURN_OLD 0
#endif
#ifndef HUSK_BURN_SEC
#define HUSK_BURN_SEC 3.4f   // №697: огонь и дым после взрыва, потом чёрный остов без огня; PS2 a355_boom: огонь ~3,4 с, дым ~3 с (одна запись, на глаз по кадрам)
#endif
#ifndef CAR_HUSK_OLD
#define CAR_HUSK_OLD 0
#endif
#ifndef CAR_HUSK_TOP
#define CAR_HUSK_TOP 4.5f   // м/с: газ остова не разгоняет быстрее (близнец: 4–5 м/с)
#endif
// только машина игрока (Car.huskDrive ставит game.c gameCarStep): остовы трафика, стоянки и машин миссий — как прежде, без входа
// №471 (Роб: машина иногда сначала становится каркасом, потом взрывается — должно быть: взрыв, потом каркас): остов рисуется со взрыва
// (carBoomed), а не с разбития (destroyed): до взрыва горит целой моделью. CAR_HUSK_AFTER_BOOM_OLD=1 — прежнее (остов с разбития)
#ifndef CAR_HUSK_AFTER_BOOM_OLD
#define CAR_HUSK_AFTER_BOOM_OLD 0
#endif
static inline int carShowHusk(const Car *c) { return CAR_HUSK_AFTER_BOOM_OLD ? c->destroyed : carBoomed(c); }
static inline int carHuskDrives(const Car *c) { return !CAR_HUSK_OLD && c->huskDrive && carBoomed(c); }
// №1050: диагностика остановок (car.c carDbg1050, flag carcon1050): контакты кузова и колёс на последнем подшаге
typedef struct { int revert, tries, bn, wn; float bdepth; unsigned char btype[8]; unsigned bidx[8]; float bnx[8], bnz[8], bd[8]; unsigned wref[8]; float wn3[8][3], wdepth[8]; } CarDbg1050;
extern CarDbg1050 carDbg1050; extern int carDbgOn1050;
void carUpdate(Car *c, const ColWorld *w, const CarInput *in, float dt);
extern unsigned carNanFixes;   // правка 252: сколько раз машину с NaN/inf вернули на надёжное место (чёрный ящик, поле fpe)
// кузов в формате colPushCar/colProbeCar для текущего положения
void carBox(const Car *c, CarBox *b);
// задать скорость вдоль носа (м/с), без вращения (тесты, сцены)
void carSetSpeed(Car *c, float v);
// Скорость точки (x,y,z), за которой следит камера; Car.vel — скорость COM.
// Вращение кузова меняет скорость COM относительно этого якоря.
void carAnchorVelocity(const Car *c,float velocity[3]);
extern int carEdgeDeathOld;   // 1 (-DEDGE_DEATH_OLD=1): край пустоты над зоной DEATH — стена, как до 11.10
float carModelLift(const Car *c,float storedLift);
// удар двух машин (кузова — прямоугольники в xz): разводит и обменивается импульсом с учётом масс. 1 = было касание.
// w (может быть NULL): мир, в который разведение не должно вдавливать кузов (упёршаяся машина отдаёт сдвиг другой)
int carHitCar(Car *a, Car *b, const ColWorld *w);
// матрица куска машины для GE (столбцы, как ScePspFMatrix4): мир = T + R * (o + sc * v); модель смотрит в -Z,
// её низ колёс на -lift. Полная ориентация тела.
void carPieceMatrix(const Car *c, float lift, float ox, float oy, float oz, float sc, float m[16]);

#ifdef __cplusplus
}
#endif

// Правка Роба 185 (близнец В14 с нажатиями, knowledge/facts/ps2-traffic-hit.yaml, ступень 4): минивэн оригинала (HitPoints 1.0)
// выдержал 14 таранов машиной Гомера (build/twins/o4/v14_ram4: 1,3–14,5 м/с) — ни дыма, ни огня; удары в бок 20 и 35 м/с — цел.
// У порта (176: урон × 0,45) такая серия давала дым. Урон машины трафика от тарана игрока — доля CAR_RAM_DMG_K общего правила
// (traffic.c после carHitCar(player, …); гейт tests/trafhit «rams14»)
#ifndef CAR_RAM_DMG_K
#define CAR_RAM_DMG_K 0.25f   // серия v14_ram4 без доли — 1,0 (разбит); 0,25 — 0,25, ниже дыма 0,33
#endif
// №176 повтор (Роб, TEST27: «машина трафика с одного удара на скорости взорвалась»): тяжёлая машина игрока (автобус, плуг, Мардж) разбивала трафик одним
// ударом (по формуле с массами). Один удар (контакт без пауз дольше 0,8 с) отнимает у трафика не больше CAR_RAM_CAP: оригинал при 35 м/с цел (В14), разбивается серией.
// CAR_RAM_CAP_OLD=1 — прежнее (без предела)
#ifndef CAR_RAM_CAP_OLD
#define CAR_RAM_CAP_OLD 0
#endif
#ifndef CAR_RAM_CAP
#define CAR_RAM_CAP 0.45f
#endif
// предел урона за «один удар» для любого источника в кадре (удар о стену разогнанной машины игрока, трафик о трафик): не больше hitEpD0 + CAR_RAM_CAP, пока с
// последнего удара игрока меньше CAR_RAM_CAP_T секунд; не ниже урона до кадра (before)
#ifndef CAR_RAM_CAP_T
#define CAR_RAM_CAP_T 2.0f
#endif
static inline void carRamCapFrame(Car *k, float before) {
  (void)before; if (CAR_RAM_CAP_OLD || k->hitEpAge >= CAR_RAM_CAP_T || k->hitEpD0 >= 1.f) return;
  float cap = k->hitEpD0 + CAR_RAM_CAP;
  if (k->damage > cap) { k->damage = cap; if (k->damage < 1.f) k->destroyed = 0; }
}
static inline void carRamDamage(Car *k, float before) {
  if (!CAR_RAM_CAP_OLD && k->hitEpAge > 0.8f) k->hitEpD0 = before;   // новый удар: отсчёт от текущего урона
  if (!CAR_RAM_CAP_OLD) k->hitEpAge = 0.f;
  if (k->damage <= before || before >= 1.f) return;
  k->damage = before + (k->damage - before) * CAR_RAM_DMG_K;
  if (!CAR_RAM_CAP_OLD && k->damage > k->hitEpD0 + CAR_RAM_CAP) k->damage = (k->hitEpD0 + CAR_RAM_CAP > before ? k->hitEpD0 + CAR_RAM_CAP : before);
  if (k->damage < 1.f) k->destroyed = 0;
}

extern int carJumpZoneGateOld;   // №705 ч.2: 1 — окно разгона зоны прыжка без расширения тонких зон (как в TEST73); 0 — полудлина вдоль въезда не меньше JUMP_ZONE_MIN_ALONG
#endif
