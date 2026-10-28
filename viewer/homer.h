// Гомер пешком (этап 4): управление, анимация, коллизия с COL1, удары (HIT / STOMP), посадка в машину и выход.
// Модель и клипы — build/homer.shr (tools/homer_export.py, формат CHR1 из skin.h; скиннинг на GE, skin.c).
// Самостоятельный модуль: в main.c не встроен, патч — docs/homer_integration.md. Чистый C99 + skin.c, без malloc в кадре.
//
// ЧИСЛА (оригинал, PAL SLES_518.97 по разбору hit-and-run-web docs/MOVEMENT_PHYSICS.md; клипы и смеси — homer.cho):
//   ходьба 4 м/с (стик до упора), бег 8 м/с (○ зажат), разгон 20 м/с², торможение 10 м/с²; гравитация 25 м/с²;
//   раскладка пешком — диск (frontend.p3d ControllerPS2.pag, подписи «пешком»): X JUMP, □ ATTACK, ○ DASH / SPRINT,
//   △ GET IN CAR / TALK TO CHARACTER; подсказка обучения srr2 «USE LEFT ANALOG STICK TO MOVE AROUND / USE X TO JUMP /
//   PRESS AND HOLD ○ TO RUN»: без ○ Гомер идёт (клип hom_loco_run 3.4 м/с ближе к 4), с ○ бежит (hom_loco_dash 9.2 м/с).
//   Удар: □ на земле — пинок (hom_break), после прыжка — удар в прыжке (hom_jump_kick, «JUMP INTO THE AIR WITH THE X
//   BUTTON AND THEN PRESS THE □ BUTTON TO KICK THE WASP»), после второго прыжка — стомп (X X □)
//   прыжок 1.9 м, второй прыжок +1 м (ДОБИВКА 25: по записи PS2 вершина 1,84 / 2,71 м — см. HOMER_JUMP_PS2_OLD; доступен, пока вертикальная скорость в [-12, 2]); в воздухе 4 м/с, разгон 4.68 м/с²;
//   стомп: гравитация ×3.22; блендинг клипов 0.1 с (cho default), jump_idle 0.05/0.1, jump_dash_in_air 0.5/0.05;
//   локомоция walkerLoco: idle / walk / run / dash по скорости (скорости корня клипов: 0.87 / 3.4 / 9.2 м/с).
//   Наше (в данных нет): капсула r 0.3, ступенька 0.4 м, поворот 720°/с (закомментированное в cho), камера пешком.
//
// КООРДИНАТЫ — как у level.shr / main.c: метры, y вверх; курс h: вперёд = (sin h, 0, cos h). Позиция = ступни.
#ifndef SHAR_HOMER_H
#define SHAR_HOMER_H
#ifndef DRIVER_POSE_OLD
#define DRIVER_POSE_OLD 0   // B5: =1 — поза водителя каждый кадр полностью и без отсечения по кадру (homer.c homerPose, main_loop.h)
#endif
#ifndef DRIVER_BOX_OLD
#define DRIVER_BOX_OLD 0    // №520: =1 — рамка «водитель вне кадра» от прежнего place (после сброса машины водитель пропадал)
#endif
#include "skin.h"
#include "col.h"
#include "car.h"

#ifdef __cplusplus
extern "C" {
#endif

// ---- параметры (оригинал; см. шапку)
#define HOMER_WALK        4.0f
#define HOMER_RUN         8.0f
#define HOMER_ACCEL       20.0f
#define HOMER_DECEL       10.0f
#define HOMER_GRAVITY     25.0f
#ifndef HOMER_JUMP_PS2_OLD
#define HOMER_JUMP_PS2_OLD 0     // ДОБИВКА 25: 1 — прежние 1.9 / 1.0 (разбор web-порта); 0 — по записи PS2 (корень 0xa245b0)
#endif
#if HOMER_JUMP_PS2_OLD
#define HOMER_JUMP_H      1.9f
#define HOMER_JUMP2_H     1.0f
#else
// Запись PS2 09.10 (наблюдатель, корень SimState 0xa245b0): ~/Downloads/shar-psp-ps2-w/build/twins/o4/ps2_homer_jump —
// вершина стоя 1,8345 м выборочно / 1,8383 парабола (g 25,19); ps2_homer_double_jump (второй X через 0,32 с) — вершина
// 2,7062 / 2,7121. Значения ниже дают ту же вершину у порта с его шагом интегрирования 1/120 с (moveBody): 1/60 —
// 1,838 / 2,699, 1/30 — 1,838 / 2,723 (tests/homer_jump/host.sh). Разбор: tools/twins/homer_jump_ps2.py
#define HOMER_JUMP_H      1.878f
#define HOMER_JUMP2_H     0.945f
#endif
#define HOMER_JUMP2_UP    2.0f     // второй прыжок: вертикальная скорость не больше этого
#define HOMER_JUMP2_DOWN  12.0f    // ... и падение не быстрее этого
#define HOMER_AIR_SPEED   4.0f
#define HOMER_AIR_ACCEL   4.68f    // 0.078 * 60
#define HOMER_STOMP_G     3.22f
// ---- наше
#define HOMER_RADIUS      0.30f    // капсула по xz
#define HOMER_HEIGHT      1.80f
#ifndef HOMER_STEP
#define HOMER_STEP        0.40f    // ступенька/бордюр, на которые заходит сам (-DHOMER_STEP=0.004f — красный контроль гейта garage_ramp)
#endif
#ifndef HOMER_STAIR_UP
#define HOMER_STAIR_UP 0      // №гараж АЭС L1: лесенка из плит (уступы до HOMER_STAIR_MAX) — Гомер встаёт сам; 0 до эталона PS2
#endif
#define HOMER_STAIR_MAX 1.6f
#define HOMER_STAIR_CHAIN_T 1.2f   // с: после подъёма ровные площадки и дорога в ту же сторону тоже берутся
#define HOMER_STAIR_NY 0.966f  // верх опоры не круче ~15°
#define HOMER_SNAP        0.45f    // вниз по лестнице/с бордюра не отрываясь от земли
#define HOMER_TURN        12.57f   // рад/с (720°/с)
#define HOMER_KICK_REACH  1.0f     // радиус события HIT вокруг точки удара
#define HOMER_STOMP_R     2.0f     // радиус события STOMP вокруг ступней
#define HOMER_ENTER_DIST  5.0f     // △ работает, если до двери водителя не дальше (м)
#define HOMER_ENTER_BODY  3.5f     // ... или до кузова с любой стороны не дальше (м)
#ifndef HOMER_DOOR_STALL_OLD
#define HOMER_DOOR_STALL_OLD 0   // правка 285: 1 — прежнее (к недоступной двери 4 с на месте)
#endif
#ifndef HOMER_IRIS_PATH
#define HOMER_IRIS_PATH   4.5f     // правка 212: путь к двери водителя длиннее (м) — посадка под ирисом (homer.c HS_TO_DOOR); со стороны водителя (до 5 м) — пешком, в обход — ирисом
#endif
#ifndef HOMER_IRIS_CAR_OLD
#define HOMER_IRIS_CAR_OLD 0     // правка 333: 1 — прежнее (машины с SetIrisTransition садятся и выходят клипами двери); служебно — flag caririsold
#endif

// ---- состояния
enum { HS_FOOT, HS_KICK, HS_STOMP, HS_HURT, HS_TO_DOOR, HS_GET_IN, HS_IN_CAR, HS_GET_OUT };

// ---- события наружу (homerPopEvent)
enum { HOMER_EV_HIT = 1,     // пинок / удар в прыжке: сфера (x,y,z,r), направление (dx,dz) — стейтпропам SP_EV_HIT, пропам — толчок
       HOMER_EV_STOMP,       // стомп: сфера вокруг ступней — стейтпропам SP_EV_STOMP (ящик сразу 30 монет)
       HOMER_EV_JUMP,        // прыжок (r = 1 первый, 2 второй) — звук/пыль
       HOMER_EV_LAND,        // приземление (r = скорость падения, м/с) — пыль, звук
       HOMER_EV_HURT,        // удар током / сбит: монеты разлетаются (coinsDrop в x,y,z)
       HOMER_EV_IN_CAR,      // сел: управление машиной
       HOMER_EV_ON_FOOT,     // вышел: управление Гомером
       HOMER_EV_EJECT,       // выброшен взрывом машины (r = сила, dx dz — куда летит); ON_FOOT приходит тоже
       HOMER_EV_BOUNCE,      // подкидывающая точка (правка Роба 46): взлёт с неё (x,y,z — точка на земле, r — высота до вершины, м;
                             // dx dz — куда летит) — звук trampoline, клуб пара/пыли
       HOMER_EV_STEP };      // шаг бега (правка Роба 45): на земле, клип бега hom_loco_dash преобладает — два шага на его цикл
                             // (фаза шага 0 и 0.5); x,y,z — ступни, r — скорость. Клуб пыли cloud.tga (слот 2 частиц оригинала
                             // «dust cloud when running/jumping»; запись PS2 18:13: клуб у ступни на каждый шаг бега)
// Г4 / правка Роба 142 (01.10, решено терминалом): «край без дна» — стена и для Гомера, как у машины (viewer/car.c, «край
// мира» why 16/32). Ночь 30.09→01.10: все 20 верхних мест провала под мир — Гомер пешком; он уходил туда, где под ступнями
// нет никакой опоры коллизии до самого низа: за край земли Intersect (дорога у склона холма, обрыв у забора), под
// нависающую землю (склон холма над дорогой — опора только выше ступенек) и на уступ выше HOMER_STEP без стенки (l1z2, 0.6 м).
// Оракул PS2 (build/ps2ref/falls, 30.09 22:59, те же места прошлой ночи): тело оригинала там стоит или падает на землю ниже,
// под мир не уходит нигде, кроме зон DEATH (там возвращает зона). Правило порта: шаг в точку, под которой нет опоры не выше
// ступенек, отменяется (как стена); исключения — щель до HOMER_CRACK с опорой по обе стороны и следы зон DEATH (туда можно,
// возвращает deathCheck, как в оригинале). Геометрию не добавляет и не вырезает. 0 — как до правки (отрицательный контроль)
#ifndef HOMER_VOID_WALL
#define HOMER_VOID_WALL 1
#endif
// Разбор ночи 11.10 (выпадения за мир, «КОЛЛИЗИИ_НОЧЬ_РАЗБОР»): Гомер стоит на крыше машины у обрыва/под нависающей землёй (крыша — не
// опора мира), прыгает, сходит с крыши — под ним по всей колонке нет опоры коллизии: падал 60 м до сброса airFrom - 60, а «надёжное
// место» (safe) запоминалось и с крыши — возврат туда снова над пустотой. Правило: колонка без опоры мира ниже тела (не след зоны DEATH) —
// возврат на «надёжное место» после HOMER_VOID_DROP м падения (≈ 1 с), а safe хранит только опору мира. Геометрию не меняет.
// 0 — как до правки (отрицательный контроль tests/falls, tools/bots/fallhunt)
#ifndef HOMER_VOID_RECOVER
#define HOMER_VOID_RECOVER 1
#endif
#define HOMER_VOID_DROP 6.0f
#define HOMER_CRACK 1.0f     // = CAR_CRACK (viewer/car.c): щель уже 2 м с опорой по обе стороны — не край
#ifndef HOMER_STEP_DUST
#define HOMER_STEP_DUST 1    // отрицательный контроль tests/jumps (--gate): 0 — шагов бега нет (как до правки 45)
#endif
typedef struct { int type; float x, y, z, r, dx, dz; } HomerEvent;

// вход за кадр
typedef struct {
  float mx, my;                // стик: вправо +, вперёд (вверх) +; -1..1, мёртвая зона уже вычтена
  float camYaw;                // курс камеры (вперёд = (sin, cos)); движение — относительно камеры
  unsigned char jump, attack;  // нажатия В ЭТОМ кадре: X прыжок, □ удар (пинок / в прыжке / стомп после второго прыжка);
                               // △ — main.c: homerEnterCar/ExitCar, двери, разговор
  unsigned char dash;          // ○ зажат: бег HOMER_RUN (DASH / SPRINT); без него стик — ходьба HOMER_WALK
  unsigned char stomp;         // служебное: стомп сразу в воздухе — прежняя раскладка порта (○ до 29.09), только
                               // отрицательный контроль tests/controls (Game.footPadOld)
} HomerInput;

// машина для посадки/выхода и толкания (homerCarFrom заполняет из car.h)
typedef struct {
  float m[16];                 // модель машины -> мир (carPieceMatrix(c, lift, 0,0,0, 1)): смотрит в -Z, +X вправо
  float lift;                  // модель на lift над низом колёс
  float x, y, z, h, v;         // низ колёс, курс, скорость
  CarBox box;                  // кузов (carBox)
  float roof;                  // высота крыши (на неё можно запрыгнуть)
  int ok;
  int dead;                    // машина взорвана (остов): сесть нельзя (txtbible: «VEHICLE DESTROYED! FIND A WRENCH, OR GO TO A PHONEBOOTH»)
  float door[2];               // точка у двери водителя (x, z модели; car.p.doorX/Z): отсюда начинается посадка
  float seatY; int seatData;   // №666: высота сиденья над землёй (car.p.seat[1] + lift, сустав dl) и признак «из данных машины»
  float seatA[2];              // якорь посадки (car.p.seatAx/Az): конец клипа посадки ставит Гомера на сиденье; у машины, где
                               // дверь дальше от борта (коробка кузова шире кабины), якорь доезжает от двери за время клипа посадки
  int husk;                    // правка 355: остов после взрыва — посадка/выход без двери и ириса (HOMER_HUSK_SEAT)
  int iris;                    // правка 333: в .con машины SetIrisTransition(1) (car.c carIrisFor) — посадка и выход ирисом, без клипов двери
} HomerCar;
void homerCarFrom(HomerCar *hc, const Car *c, float lift);
void homerSetDoorOut(float m);   // Роб 17.10: насколько дальше от борта стоит у двери, м (0,12 по умолчанию; 0 — как было) — рука не уходит в дверь

// дверь водителя: open 0 закрыта .. 1 открыта (main.c крутит шарнир двери CarVis); done: 1 = сел, 0 = вышел
typedef struct {
  void (*door)(void *user, float open);
  void (*done)(void *user, int inCar);
  void *user;
} HomerCarCb;

// Дополнительные твёрдые тела (не в COL1): стейтпропы и т.п. Вытолкнуть круг (x, z, радиус r) из всего, что выше
// ступеньки над ступнями y; в *top — самая высокая опора под (x, z) не выше y + HOMER_STEP (можно стоять), иначе не трогать.
// Возврат: 1 — вытолкнули (нормаль в *nx, *nz). Готовая реализация для стейтпропов — homerSolidSprops (user = StateProps*).
typedef int (*HomerSolidFn)(void *user, float *x, float *z, float y, float r, float *top, float *nx, float *nz);
struct StateProps;
extern unsigned homerSolidFrame;   // != 0 только внутри homerUpdate: твёрдость предметов считает список «кто рядом» раз за вызов (HOMER_SOLID_CACHE_OLD=1 — выкл.)
int homerSolidSprops(void *stateProps, float *x, float *z, float y, float r, float *top, float *nx, float *nz);

#define HOMER_MAXEV 8
typedef struct {
  const SkinModel *m;
  int state;
  // --- тело
  float x, y, z, h;            // ступни, курс модели
  float vx, vy, vz;            // скорость, м/с
  int grounded, jumps;         // на земле; прыжков с земли (0, 1, 2)
  float camTurn;               // правка Роба 75: L/R пешком — довернуть камеру (-1 влево, +1 вправо), main.c каждый кадр
  float groundY, airFrom;      // высота земли под ногами; с какой высоты начался полёт
  float speed;                 // горизонтальная скорость, м/с
  float st;                    // время в текущем состоянии, с
  int phase;                   // подфаза состояния (стомп: 0 замах, 1 падение, 2 приземление; машина: 0 открыть, 1 сесть/выйти, 2 закрыть)
  int hitSent;
  float idleT;                 // сколько стоит без дела (длинные idle)
  // --- машина
  HomerCar car; HomerCarCb cb; float door;
  float anchor[3];             // точка у двери в осях модели машины (ступни в начале посадки)
  // --- анимация
  int clip, clipLoop; float clipT, clipRate, clipFrom;   // одиночный клип (не локомоция); clip < 0 — локомоция
  int rootMode;                // 1 — поза с переносом корня (посадка/выход/в машине), в системе машины
  float phaseLoco, idleClipT;  // фаза шага (0..1), время idle
  float fade, fadeLen;         // переход из снимка позы snap
  int cLoco[4], cTake, cAir, cLand, cDash2, cBreak, cJKick, cStA, cStAir, cStLand,
      cOpenIn, cIn, cCloseIn, cOpenOut, cOut, cCloseOut, cInCar, cFlail, cGetUp, cIdle[3];
  // правки Роба 220/231: за рулём — набор водителя ndr оригинала (cInCar = ndr_in_car_idle; нет в пакете — hom_in_car_idle):
  // жесты поворота руля влево/вправо, газа, тормоза, удара (-1 — нет клипа). Вход — homerDriveInput (main.c / game.c каждый кадр)
  int cDrvTurn[2], cDrvGas, cDrvBrake, cDrvCrash;
  float drvSteer, drvGas, drvBrake, drvSpeed, drvPrevSteer, drvPrevGas, drvPrevBrake; int drvCrash, drvGestures;
  float locoV[4], locoStride[4];
  float seatRoot[3];           // корень позы «сидит» (конец get_into_car_close_door_driver)
  SkinXf xf[SKIN_MAXJ], tmp[SKIN_MAXJ], snap[SKIN_MAXJ];
  ScePspFMatrix4 pal[SKIN_MAXJ] __attribute__((aligned(16)));
  ScePspFMatrix4 palFresh[SKIN_MAXJ] __attribute__((aligned(16))), palPrev[SKIN_MAXJ] __attribute__((aligned(16)));   // B5: последние две полные позы водителя (homerPose)
  int drvPoseValid, drvPoseTick;   // B5: палитры palFresh/palPrev годны; чётность кадра
  ScePspFMatrix4 world[SKIN_MAXJ] __attribute__((aligned(16)));
  ScePspFMatrix4 place __attribute__((aligned(16)));       // модель Гомера -> мир (для skinDrawPal)
  SkinPlayer blink;            // только моргание (clip -1)
  // --- твёрдые тела вне COL1 (необязательно)
  HomerSolidFn solid; void *solidUser;
  // --- события
  HomerEvent ev[HOMER_MAXEV]; int nev;
  // --- камера пешком
  float camYaw, camY, frame, heldX, heldY, neutralT, steerGap, sweepT; int camInit, flipSeen, flipLatch; float flipLatchT;   // №920: neutralT — сколько стик уже в нуле (кадр управления держится STEER_NEUTRAL_T); steerGap — сколько steer не вызывался (после машины, пинка, падения кадр не считается свежим)
  float safe[3], launchV; int idleNext;
  // --- отладка / тесты
  float maxY, lastLandV; int landings;
  int voidRecovers;            // сколько раз возвращён с колонки без опоры (HOMER_VOID_RECOVER), для гейта fallhunt
  int stairUps;                // лесенка: сколько раз встал на уступ (HOMER_STAIR_UP)
  float stairChain, stairDx, stairDz;   // лесенка: остаток секунд цепочки подъёмов и её направление
  int voidStops;               // Г4: сколько подшагов отменено правилом «край без дна» (HOMER_VOID_WALL), для гейта tests/falls
  int voidFloors;              // правка 142 / ДОБИВКА 13: подшагов на полу над пустотой (voidFloor), для гейта tests/falls/void_floor
  // --- камера пешком и машина рядом (Роб 28.09: «подхожу к машине — вижу внутренности»): машина из последнего homerUpdate,
  // дистанция камеры и ближняя плоскость для main.c (камера у кузова/стены не режет кузов и Гомера)
  HomerCar camCar; float camDist, camNear;
  float camFlipT;              // CAM_WALL_PS2: с последнего перескока камеры на другую сторону (с), не чаще раза в 2 с
  float camPullD, camRise;     // правка 40: допустимая дистанция камеры (стена за спиной — сразу ближе, отпускает плавно) и подъём
  float camFx, camFz;          // правка 223: точка, за которой идёт камера (на посадке — держится, не дёргается с телом)
  float camHoldD, camHoldE;    // правка 349 (В86): поза камеры после переноса зоной DEATH — дистанция и глаз (0 — обычная камера)
  int camHits;                 // сколько лучей камеры упёрлось в коллизию в последнем кадре (журнал, гейт tests/camera)
  int overTop;                 // последний выход из машины — через верх (у двери было занято), для тестов и журнала
  // --- переходы (viewer/transit.c, пакет «переходы как в оригинале»): счётчики для ириса и журнала
  int irisOn;                  // 1 — вызывающий покажет ирис: посадка при занятой двери — сразу на сиденье (под ирисом)
  float stallD, stallT;        // правка 285: путь к двери — лучший остаток и время без продвижения (упёрся — ирис)
  int irisSeat;                // ++ при такой посадке (main.c по смене счётчика закрывает/открывает ирис)
  int huskIn, huskOut;         // правка 355: ++ при посадке / выходе у остова (без двери и ириса)
  int irisCarIn, irisCarOut;   // правка 333: ++ при посадке / выходе ирисом у машины с SetIrisTransition (transitHomerWhy: car_in / car_out)
  int ejected;                 // ++ при выбросе взрывом (homerEject)
  int carHits; float carHitCool;   // №709: ++ при ударе машины по Гомеру пешком (падает и встаёт); пауза до следующего удара, с
  // --- правка Роба 147: выброс взрывом как в оригинале — задержка в горящем остове, полёт головой вперёд (тело наклонено),
  // касание грудью, лёжа, hom_get_up. pitch — наклон тела вокруг оси X модели (0 стоит, pi/2 ничком, голова по курсу)
  float pitch, ejWait, ejPow, ejFromX, ejFromZ; int ejPh, jExtra;   // jExtra — сустав Extra_Attributes (не часть тела)   // ejPh: 0 нет, 1 полёт, 2 касание/скольжение, 3 встаёт
  // --- разговор миссии (правка Роба 38): жест реплики (клип dialogue_* из homer.shr, AddAmbientPcAnimation) и рот
  int cTalk, jaw; float jawOpen;
  // --- подкидывающие точки (правка Роба 46; блок JMP1 COL1, tools/jumps_export.py): последняя точка (-1 — вне точек;
  // пока стоит в той же, второй раз не подбрасывает), полёт с неё (без стика воздух скорость не гасит), счётчик для тестов
  int padLast, bouncing, bounces; float padApex[3];
} Homer;

// модель — skinLoad("homer.shr"); 0 = ок, <0 — в модели нет нужных клипов
int homerInit(Homer *H, const SkinModel *m);
// правка 352: префикс клипов последней homerInit (hom_ / brt_ / lsa_ / mrg_ / apu_ — игрок уровня, пакет l<N>/player.shr)
const char *homerClipPrefix(void);
// поставить на землю под (x, y + 2, z) с курсом h, пешком
void homerPlace(Homer *H, const ColWorld *w, float x, float y, float z, float h);
// шаг кадра. car — машина игрока (NULL — нет): посадка/выход/в машине и толкание кузовом
void homerUpdate(Homer *H, const ColWorld *w, const HomerCar *car, const HomerInput *in, float dt);
// посадка: △ у машины. 1 = пошёл к двери (дальше сам: подойти, открыть, сесть, закрыть -> cb.done(1)), 0 = далеко/занят
int homerEnterCar(Homer *H, const HomerCar *car, const HomerCarCb *cb);
// выход: △ в машине. 1 = начал (открыть, выйти, закрыть -> cb.done(0)), 0 = не в машине
int homerExitCar(Homer *H, const HomerCar *car, const HomerCarCb *cb);
// сесть сразу (старт уровня в машине), без анимации
void homerSetInCar(Homer *H, const HomerCar *car);
// удар током / сбит машиной: откинуть от (fromX, fromZ), флейл, встаёт. Событие HURT
void homerHurt(Homer *H, float fromX, float fromZ, float power);
// взрыв машины (Роб 28.09: «в оригинале при взрыве персонаж вылетал из машины»): Гомер в машине / садится / выходит —
// вылетает от центра взрыва (fromX, fromZ) со стороны двери водителя (занята — с крыши в другую сторону), флейл, встаёт.
// Пешком к двери — просто останавливается. События ON_FOOT и EJECT (без HURT: монеты не разлетаются). 1 — выброшен
#define HOMER_EJECT_POWER 0.8f     // правка Роба 73 (запись PS2 33:36: Гомер падает рядом с остовом): «не так сильно, в сторону» — ×0.8 (3.2 м/с вбок, 4 вверх)
#define HOMER_EJECT_Y     .5f      // правило порта: старт над землёй у двери (высота сиденья)
// Правка Роба 147 (01.10: «летят руками вперёд, и потом стоят, очухиваются; летят вперёд, а не сразу на ноги»).
// Данные диска: у Гомера для этого только hom_flail (17 кадров) и hom_get_up (31 кадр, кадр 0 — ничком, голова 0.22 м над землёй
// и 0.65 м впереди ступней). Запись PS2 (33:33.43 удар → 33:34.23 у двери → 33:34.30 летит горизонтально головой вперёд →
// 33:34.50 касание грудью, ноги вверх → 33:34.83 лежит → 33:34.90 get_up → 33:35.63 бежит). Числа ниже — правило порта,
// подобраны по записи (дальность ≈3–5 м от двери, ±30% — рост Гомера в кадре)
#define HOMER_HUSK_SEAT .7f        // правка 355 [PS2 близнец]: △ у остова -> за рулём через ~0,7 с, без двери и ириса
#define HOMER_EJECT_DELAY .7f      // запись: от взрыва до выброса Гомер ещё в горящем остове
#define HOMER_EJECT_VH    10.f     // ×power (0.8) = 8 м/с вбок: низко и быстро
#define HOMER_EJECT_VY    2.5f     // ×power = 2 м/с вверх
#define HOMER_EJECT_TILT  .13f     // с: корпус до горизонтали (33:34.23 стоит → 33:34.30 горизонтально)
#define HOMER_EJECT_SLIDE .3f      // с: касание грудью и кувырок ног (33:34.50–33:34.77)
#define HOMER_EJECT_ROCK  .6f      // рад (≈35°): ноги взлетают выше горизонтали при касании
#define HOMER_EJECT_REST  .35f     // рад (≈20°): лежит чуть головой вниз — на груди и кистях, как кадр 0 get_up (голова 0.22 м)
#define HOMER_EJECT_FRIC  26.f     // м/с²: скольжение по земле гаснет за время касания
#define HOMER_EJECT_PIV   .65f     // м: ось наклона на этой высоте модели (голова лежащего — на 0.65 м впереди ступней, как кадр 0 get_up)
#define HOMER_EJECT_CLEAR .03f     // м: суставы наклонённого тела не ниже опоры + это (кисти, носки — не в землю; касание грудью и кувырок)
#define HOMER_EJECT_LIE   .3f      // м: ось наклона над землёй, когда лежит (голова ≈0.22 м над землёй, как кадр 0 get_up)
// правка 147: в машине вызов только взводит выброс, сам выброс — в homerUpdate через HOMER_EJECT_DELAY (ejected++ тогда же);
// 1 — выброс взведён (или, с -DHOMER_EJECT_V1, прежний мгновенный выброс спиной вперёд — отрицательный контроль гейта)
int homerEject(Homer *H, const ColWorld *w, const HomerCar *car, float fromX, float fromZ, float power);
// забрать событие (0 = пусто)
int homerPopEvent(Homer *H, HomerEvent *e);
// попадает ли событие HIT/STOMP в объект со сферой (cx,cy,cz,r)
int homerHitTest(const HomerEvent *e, float cx, float cy, float cz, float r);
// точка подбора монет (coinsUpdate(..., inCar = 0)); в машине — центр машины
void homerCoinPoint(const Homer *H, float p[3]);
static inline int homerOnFoot(const Homer *H) { return H->state != HS_IN_CAR && H->state != HS_GET_IN && H->state != HS_GET_OUT && H->state != HS_TO_DOOR; }
static inline int homerDriving(const Homer *H) { return H->state == HS_IN_CAR; }
// можно ли сесть (у двери водителя ближе HOMER_ENTER_DIST или у кузова ближе HOMER_ENTER_BODY)
int homerCanEnter(const Homer *H, const HomerCar *car);
// правки 220/231: руль (−1 влево .. 1 вправо), газ, тормоз 0..1, скорость машины м/с, удар в этом кадре — для жестов водителя
void homerDriveInput(Homer *H, float steer, float gas, float brake, float speed, int crash);
// камера пешком: плавно за спиной (своя, в данных оригинала её нет). Выход: глаз и точка взгляда
extern float (*homerCamRayExtra)(const float a[3], const float b[3]);   // правка 284: доля луча до кузова машины (1 — свободно); NULL — только мир
extern int homerCamHeroOld;   // правка 361: 1 — прежнее: после выброса камера по курсу машины, в полёте курс не догоняет (красный контроль)
void homerCamera(Homer *H, const ColWorld *w, float dt, float eye[3], float look[3]);
// Правка 349 / ДОБИВКА 18 ([PS2] В86, knowledge/facts/ps2-r6-foot-death.yaml; запись ps2-w build/twins/o4/
// ps2_v86_r6_foot_death_physics/trace.tsv, кадры 606–906): после переноса пешком зоной DEATH активной остаётся WalkerCam, её глаз
// переставляется сразу (кадр 608, первый после переноса) — по горизонтали 4,165 м от Гомера С ЛИЦА (по его курсу), на 1,891 м выше
// ступней, взгляд в точку 1,0 м над ступнями (= HOMER_CAM_LOOK_H); стоя так и остаётся (до кадра 906). Здесь: камера сразу в эту
// позу; пошёл / повернул камеру — дистанция и высота уходят к обычным 2,5 м/с (правило порта, как отпуск стены — правка 40)
void homerCamDeathPose(Homer *H);
#define HOMER_CAM_DEATH_DIST 4.165f   // [PS2] В86 trace.tsv кадр 608: |cam − Гомер| по горизонтали
#define HOMER_CAM_DEATH_EYE 1.891f    // [PS2] В86 trace.tsv кадр 608: cam y − Гомер y
int camOverGround(const ColWorld *w, float x, float z, float y);   // правка 40: под глазом камеры есть опора коллизии
extern int homerIrisCarOld;   // правка 333: 1 — прежнее (клипы двери и у машин с SetIrisTransition); красный контроль tests/homer/iris_car.py
extern int homerWalkCam1059Old, homerViewOk; extern float homerWalkCamTau, homerWalkCamRate, homerViewYaw;   // №1059
extern int homerCtrl916Old, homerExitYawOld, homerCtrlFreezeOld, homerCtrlOn, homerCamBusy; extern float homerCtrlYaw;   // homerCamBusy — №836: камера в переходе/вращается (ставит main_loop.h)   // №704: курс управления, замороженный на время облёта камеры
extern int homerFlipStandOld;   // HOMER_FLIP_STAND_OLD=1 / flag homerflipstandold 1 — перескок камеры у стены и у стоящего Гомера (как было)
extern int homerCamCarOld;   // №982: 1 — машина у спины Гомера, как стена: перескок камеры через героя и глаз в кузове (flag camcar982old)
extern float homerCamCarTop;   // №982: верх кузова машины, задержавшей луч камеры в последнем вызове homerCamRayExtra (-1e9 — не было)
extern int homerCamFlipOld, homerCamFlipN, homerFlipBusyOld, homerFlipHold;   // №916в: перескок камеры у стены одним кадром (CAM_FLIP1_OLD=1 / flag camflip1old 1 — как было); счётчик перескоков
extern int homerCamPs2Old;   // CAM_WALL_PS2_OLD=1 / flag camwallps2old 1 — стена за камерой пешком как раньше (подъезд + подъём), не как у PS2
extern int homerCamWallOff;   // 1 — камера пешком без обхода стен (служебное, красный контроль tests/camera)
// №996 (Роб 26.10: «настройка высоты и дальности камеры в меню, и в машине, и пешком»): множители страницы CAMERA паузы (Height, Distance; 1 — как было): камера пешком и камера за машиной
// (main_camera.h chaseCamUpdate). Задаёт cfgApply (viewer/main_save.h). CAMCFG996_OLD=1 / файл camcfg996old рядом с EBOOT / «flag camcfg996 0» — множители всегда 1
extern float homerCamUserD, homerCamUserH;
extern float homerCamDist, homerCamEyeH, homerCamLookH;   // камера пешком (HOMER_CAM_DIST / _EYE_H / _LOOK_H; служебное — красный контроль)
#ifndef HOMER_CAM_SEAT_K
#define HOMER_CAM_SEAT_K (1.f / 3.8f)   // правка 223: на посадке камера догоняет Гомера с постоянной 3,8 с (близнец В23); 0 (-D) — жёстко, как было
#endif
#define HOMER_CAM_NEAR 1.5f          // ближняя плоскость камеры, когда рядом ничего нет (= NEAR_Z main.c)
#define HOMER_CAR_CLEAR .6f          // камера не ближе к кузову машины (порт: иначе ближняя плоскость режет кузов)
// разговор (msn_view.c): жест реплики — клип «dialogue_...» (суффикс имени; NULL / "none" / нет клипа — стоит как стоял),
// играет один раз стоя (движение — отмена); рот — открытие челюсти, рад (0 — закрыт; skinJawOpen)
void homerTalk(Homer *H, const char *gesture);
static inline void homerJaw(Homer *H, float rad) { H->jawOpen = rad; }
// Камера пешком как у оригинала (близнец 02.10, камера PS2 по кадрам cam_pos/cam_fwd + Гомер 0xa24a70: build/twins/o4/
// v23_exit_cam/walk.tsv, v12_booth_cam/walk.tsv): стоя — 6,45 м за спиной по горизонтали, глаз на 2,38 м над ступнями, взгляд вниз
// 12° — в точку ~1,0 м над ступнями (на ходу отстаёт до 7 м). Было своё правило порта: 4,2 м, глаз 1,9 м, взгляд в 1,2 м
#ifndef HOMER_CAM_DIST
#define HOMER_CAM_DIST 6.45f         // камера пешком: расстояние за спиной без препятствий
#endif
#ifndef HOMER_CAM_EYE_H
#define HOMER_CAM_EYE_H 2.38f        // глаз над ступнями
#endif
#ifndef HOMER_CAM_LOOK_H
#define HOMER_CAM_LOOK_H 1.0f        // точка взгляда над ступнями
#endif
// Угол кадра пешком: у оригинала у́же, чем у камеры машины (близнец 02.10 build/twins/scen/full/camfov: кадр порта в позе камеры
// PS2 против кадра PS2; PS2 в своём широком режиме держит горизонтальный угол: пешком ~68,6°, машина ~77°, будка 80° — данные).
// Порт держит вертикаль кадра 4:3 оригинала (Hor+, FOVY 62 у машины = 4:3 оригинала): пешком 4:3 — 54°
#ifndef HOMER_CAM_FOVY
#if CAM_554_OLD
#define HOMER_CAM_FOVY 54.f
#elif CAM_LENS_OLD
#define HOMER_CAM_FOVY 48.f
#else
#define HOMER_CAM_FOVY 43.1f
#endif
#endif
#define HOMER_CAM_MIN 1.2f           // ближе к Гомеру камера не подъезжает — поднимается (правка 40)
#ifndef HOMER_CAM_SIDE
#define HOMER_CAM_SIDE .6f           // правка 160: полуширина ближней плоскости 16:9 с запасом (тест: -DHOMER_CAM_SIDE=.3f — прежнее)
#endif
#define HOMER_CAM_UPDN .35f
#define HOMER_CAM_MARGIN .35f        // зазор от стены до глаза (ближняя плоскость 0.3 м — стена не режет кадр)
// поза для кадра (после homerUpdate; для невидимого — можно не звать)
void homerPose(Homer *H);
extern int (*homerDoorCan)(void);   // №667: 1 — у рисуемой машины водительская дверь может открыться (NULL — да, как прежде)
int homerSeatProbe(const Homer *H, float out[9]);   // №666: замер посадки для журнала (homer.c)
// рамка водителя в машине для отсечения по кадру (мир): 1 — рамка в bb[6] (xmin ymin zmin xmax ymax zmax), 0 — не за рулём; от текущей матрицы машины (№520)
int homerDriverBox(Homer *H, float bb[6]);
#ifndef SKIN_HOST
// нарисовать (внутри открытого списка GE; меняет состояние — см. skin.h, после — resetState() в main.c)
void homerDraw(const Homer *H);
#endif

#ifdef __cplusplus
}
#endif
#endif
