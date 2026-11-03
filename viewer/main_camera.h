#include "sverka_shared.h"   // сверка g06: svBoom (событие взрыва машины, окно g01) — тряска камеры
#include "camroll.h"   // №1084: закон крена камеры пешком
#include "camuser.h"   // №1151: настройки камеры игрока (меню CAMERA в паузе)
#include "camlaw.h"    // круг 6: законы камеры (чистые функции, tests/camera)
// ---- камера за машиной, как в оригинале: FollowCameraData (чанк 0x3000100) машины уровня (CarParams camElev/camMag из
// CAR1 v2; famil_v: rotation 180° (сзади), elevation 1.23 рад от вертикали, magnitude 7.2 м — 6.8 м позади, 2.4 м над целью;
// у всех семи машин игрока угол 1.2–1.23, дистанция 6.25–7.2 м; цель (0, 0.5, 3) данных не берём — правило порта CAM_TGT_H/LOOK_H).
// Опора камеры (точка машины) — пружина с критическим затуханием и упреждением по скорости машины: при любой
// скорости отставания нет (дистанция постоянна), а рывки положения (удары, неровный шаг физики) гасятся.
// Высота — своя, мягкая пружина с упреждением по скорости тела: подъём/спуск без задержки фильтра скорости,
// остаток предсказания гасится; при малой вертикальной скорости ограничены рывки камеры. Поворот — пружина к курсу
// без скачка при резкой смене направления кузова. Всё точным решением (не зависит от частоты кадров).
#define CAM_TGT_H 0.9f      // цель — середина кузова над началом машины (низ колёс)
#define CAM_LOOK_H 1.4f     // смотрим чуть выше цели: машина в нижней половине кадра, как было
#define CAM_WALL_MIN 2.5f   // правка 40: ближе к точке взгляда камера за машиной не подъезжает — поднимается
#define CAM_WALL_MARGIN .4f // зазор от стены до глаза
static float chaseNear = NEAR_Z;   // ближняя плоскость камеры за машиной (у стены — ближе, чтобы стена не резала кадр)
#ifndef CAM_WALL_ORG_OLD
#define CAM_WALL_ORG_OLD 0
#endif
static int camWallOrgOld = CAM_WALL_ORG_OLD;
static int chaseWallOff;           // служебное (красный контроль tests/camera): без обхода стен
static int camFrames, camBlocked; static float camStatT0;
// CAMPERF (23.10): служебная проверка «глаз за препятствием» (луч коллизии + опора под глазом каждый кадр) шла в любом смоуке, в туре тоже, — считала кадры, которые никто не читал
// (читает только строка CAMW по «flag camstat»). Теперь — только после первого «flag camstat». CAM_STAT_ALWAYS_OLD=1 / flag camstatalwaysold 1 — как было
#ifndef CAM_STAT_ALWAYS_OLD
#define CAM_STAT_ALWAYS_OLD 0
#endif
static int camStatAlwaysOld = CAM_STAT_ALWAYS_OLD, camStatOn;   // гейт tests/camera: кадры и кадры с глазом за препятствием
// сверка g06 (02.11.2026, правки 1077–1081): состояние новых правил камеры за машиной (фильтры, замирание, задний ход) — в конце структуры
typedef struct { int init; float ex, ez, ey, evx, evz, evy, vyLP, yaw, yawV, pull, lx, ly, lz, lvx, lvy, lvz, ay, lift, liftV, wallD, rise, tilt, bumpLift; int hits;
  int svInit; float fD, fDv, fE, fEv, fLk[3], fLkV[3], roll, stabT, bodyUpY;   // фильтры дальности, угла и взгляда (g06-camera:6,7,30–32); крен; сколько стоит устойчиво
  int fz; float fzE[3], fzL[3], preE[3], preL[3], bkE[3], bkEv[3], bkL[3], bkLv[3];   // замирание в полёте (g06-camera:3,42): смещения от опоры; плавный возврат
  int rev; float revV[3], revE[3], revL[3], revD, revOy; } ChaseCam;   // камера заднего хода (g06-camera:4,13)
static ChaseCam chase;
static void cdStep(float *e, float *ev, float w, float dt) {   // критическое затухание, точное решение за dt
  float x = *e, v = *ev, k = expf(-w * dt), c = v + w * x;
  *e = (x + c * dt) * k; *ev = (v - w * c * dt) * k;
}
// сверка g06: фильтр второго порядка с небольшим перелётом (ζ 0,8): подобран под цифры сверки (курс камеры: 63% за 0,29 с, 95% за 0,51 с, перелёт ~1%;
// угол: 0,45 и 0,82 с; дальность: 63% за 0,26 с; взгляд: 0,13 с). Шаг точным не решается — делим на подшаги по 4 мс
static void svStep(float *e, float *ev, float w, float z, float dt) {
  int n = (int)(dt / .004f) + 1; float h = dt / (float)n;
  for (int i = 0; i < n; i++) { float a = -w * w * *e - 2.f * z * w * *ev; *ev += a * h; *e += *ev * h; }
}
#define CAM_SV_YAW_W 6.6f    // g06-camera:30 — курс
#define CAM_SV_PITCH_W 4.3f  // g06-camera:31 — вертикальный угол (ζ 0,82)
#define CAM_SV_DIST_W 7.4f   // g06-camera:31 — дальность
#define CAM_SV_LOOK_W 14.7f  // g06-camera:32 — точка взгляда
#define CAM_SV_Z .8f
#ifndef CAM_YAW_SV_OLD
#define CAM_YAW_SV_OLD 0     // 1 — курс камеры прежней пружиной (критической, ω 8)
#endif
#ifndef CAM_PITCH_SV_OLD
#define CAM_PITCH_SV_OLD 0   // 1 — угол и дальность без своих фильтров
#endif
#ifndef CAM_LOOK_SV_OLD
#define CAM_LOOK_SV_OLD 0    // 1 — точка взгляда сразу от опоры, без отставания
#endif
#ifndef CAM_FREEZE_SV_OLD
#define CAM_FREEZE_SV_OLD 0  // 1 — без замирания камеры в полёте и при крене кузова, без крена в оси камеры
#endif
#ifndef CAM_REVERSE_SV_OLD
#define CAM_REVERSE_SV_OLD 0 // 1 — без камеры заднего хода
#endif
#ifndef CAM_DATA_SV_OLD
#define CAM_DATA_SV_OLD 0    // 1 — дальность и высота других машин подтягиваются до famil_v (правка 494)
#endif
#ifndef CAM_EYEH_SV_OLD
#define CAM_EYEH_SV_OLD 0    // 1 — высота глаза ещё поднимается «дорога видна над крышей» (правило порта)
#endif
#ifndef CAM_OCCL_SV_OLD
#define CAM_OCCL_SV_OLD 0    // 1 — при загораживании без подъёма на 6 м над машиной
#endif
#ifndef CAM_REVERSE2_SV_OLD
#define CAM_REVERSE2_SV_OLD 0 // круг 6 (СВЕРКА_КАМЕРА 1.11, P08:10): 1 — задний ход как в круге 1 (3D-дистанция, бросок вбок на полдистанции, вход и в воздухе)
#endif
#ifndef CAMCARS1127_OLD
#define CAMCARS1127_OLD 0   // №1127 (TEST84: камера резко «огибает» проезжающие машины): 1 — как в TEST84 (движущиеся чужие машины тянут глаз к опоре, режут срезом на идеальное место и двигают глаз по правилу ближней плоскости)
#endif
#ifndef CAM_OCCLCUT_SV_OLD
#define CAM_OCCLCUT_SV_OLD 0  // круг 6 (СВЕРКА_КАМЕРА 1.15): 1 — при закрытой линии камера только подтягивается вдоль луча, без среза на идеальное место за машиной
#endif
#ifndef CAM_SHAKEPH_SV_OLD
#define CAM_SHAKEPH_SV_OLD 0  // круг 6 (СВЕРКА_КАМЕРА 1.17): 1 — фаза тряски с нуля (sin(58,9·t)), а не 0,87 + шаг
#endif
#ifndef CAM_BUMPFWD_SV_OLD
#define CAM_BUMPFWD_SV_OLD 0  // круг 6 (СВЕРКА_КАМЕРА 1.20): 1 — глаз бампера на носу кузова минус 1,0 м, а не на передней оси минус 1,0 м
#endif
#ifndef CAM_LOOKUP_SV_OLD
#define CAM_LOOKUP_SV_OLD 0  // 1 — взгляд вверх не действует
#endif
#ifndef CAM_SHAKE_SV_OLD
#define CAM_SHAKE_SV_OLD 0   // 1 — без тряски камеры от ударов и взрыва
#endif
#ifndef CAM_BUMPER_SV_OLD
#define CAM_BUMPER_SV_OLD 0  // 1 — бампер: верх кадра мировой, глаз по записи PS2
#endif
// ввод для камеры (сверка g06): main_loop.h раз в кадр кладёт газ и тормоз игрока (0..1), L и R в машине — газ и тормоз
// (камера заднего хода, g06-camera:4,13); camLookUp 0..1 — «взгляд вверх» (кнопки PSP, решает Роб: ввод не подключён, см. журнал)
// сверка g06 круг 2: (g01-cars СВЕРКА_МАШИНЫ:2.2) рывок со старта (Car.burstOn) слегка расширяет кадр: tan(FOV/2) × CAM_BURST_K с запаздыванием 0,25 с (число «слегка» в отчёте не задано — подбор мой,
// на вид решает Роб); (g07-missions P04:Д9) втягивание машины лучом тарелки (ufoGrab 1) — камера бампера срезом на дальнюю следящую. CAM_BURST_SV_OLD=1 / CAM_UFO_SV_OLD=1 — как было
#ifndef CAM_BURST_SV_OLD
#define CAM_BURST_SV_OLD 0
#endif
#ifndef CAM_UFO_SV_OLD
#define CAM_UFO_SV_OLD 0
#endif
#define CAM_BURST_K 1.06f
static float camBurstK = 1.f;   // множитель tan(FOV/2) камеры за машиной, main_loop.h применяет к curFovY
static float camSvThr, camSvBrk, camLookUp;
static float camUpV[3] = {0.f, 1.f, 0.f}; static int camUpOn;   // вертикаль кадра бампера = вертикаль кузова (g06-camera:18); читает и сбрасывает main_loop.h при построении вида
// сверка g06 (28): тряска камеры — из ударов машины игрока (hitSpeed и сила взрыва svBoom); камеру качает по трём осям затухающей синусоидой
static float camShakeT = 1.f, camShakeDx, camShakeDz; static unsigned camBoomSeq;
// сверка g06 (g06-camera:28; P08 строка 1): тряска от удара. Удар машины игрока не слабее ~8 м/с по сближению (импульс 19000 оригинала) и взрыв рядом (толчок 20000/масса в радиусе 20 м)
// запускают тряску на 0,18 с: смещается только глаз, вдоль направления от точки удара к центру машины, синусоида 58,9 рад/с (0,942 рад за 16 мс), размах 1,1 м линейно к нулю.
// Только камера за машиной (дальняя и ближняя, и задний ход); у бампера и пешком её нет. CAM_SHAKE_SV_OLD=1 — без тряски
#define CAM_SHAKE_LEN .18f
#define CAM_SHAKE_AMP 1.1f
#define CAM_SHAKE_W 58.9f
#define CAM_SHAKE_V 8.f
static void camShakeFire(float dx, float dz) { float l = sqrtf(dx * dx + dz * dz); if (l < 1e-3f) { dx = 0.f; dz = 1.f; l = 1.f; } camShakeDx = dx / l; camShakeDz = dz / l; camShakeT = 0.f; }
static float wrapPi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
// Правка Роба 198+ (03.10, ответ на вопрос «камера за машиной: дальняя как у PS2 (9,7 м) или наша (6,8 м)»: «у нас широкая
// камера, шире, чем в оригинале, это нужно учитывать; школьный автобус закрывает весь экран, ничего не видно»). Кадр оригинала
// с поправкой на наш угол кадра, одно правило на все машины диска (viewer/carcam.h):
//  - у оригинала в езде ДАЛЬНИЙ вариант FollowCameraData машины (близнец v28_drive_cam: глаз 9,5–9,7 м позади начала модели
//    famil_v, данные 10,25 · sin 1,23 = 9,66; ps2-camera-fov.yaml), кадр 4:3, угол камеры машины 77° по горизонтали;
//  - у порта 62° по вертикали на 16:9 (94° по горизонтали): при той же дистанции машина занимает меньшую долю экрана. Дистанция
//    от начала модели — дальняя оригинала × CAM_FIT (корень отношения площадей кадра, tan·tan) — машина занимает ту же долю
//    ЭКРАНА, что у PS2 (famil_v 8,3 м вместо 6,8, автобус 9,3 вместо 7,9);
//  - высота глаза над началом модели — как у оригинала (mag · cos elev), не × CAM_FIT: ближе при той же высоте — дорога за
//    крышей открывается раньше, чем у PS2 (с высотой × 0,8 у автобуса дорога пряталась бы на 58 м вместо 31 м у PS2);
//  - правило порта (у оригинала нет): глаз не ниже, чем нужно, чтобы дорога в CAM_ROAD_SEE м перед носом была видна над крышей
//    с зазором CAM_ROAD_CLEAR (кузов машины из данных: bodyOfs + halfL, bodyHi); у PS2 автобус прячет 31 м дороги, грузовик — 23 м;
//  - цель — смещение данных (0; 0,5; 3) от начала модели: на 3 м вперёд по курсу камеры (поворот за машиной — прежний).
// Прежнее (ближний вариант, цель — середина машины на 1,4 м над низом колёс): -DCAM_NEAR_OLD=1 или «flag camnearold» (красный
// контроль гейта tests/camera/carcam_ps2.py). Машины без пары камер в данных (tt) — прежнее правило.
#ifndef CAM_NEAR_OLD
#define CAM_NEAR_OLD 0
#endif
#ifndef CAM_FIT_K
#define CAM_FIT_K 0.f                // > 0 — свой множитель дистанции вместо camFit() (вкус Роба: 0,745 — та же доля ШИРИНЫ экрана)
#endif
#define CAM_PS2_HFOV 77.f            // угол кадра камеры машины оригинала по горизонтали, ° (ps2-camera-fov.yaml, ±2°)
#define CAM_PS2_ASPECT (4.f / 3.f)   // кадр оригинала (запись Роба 4:3; широкий режим PS2 держит ту же горизонталь)
#define CAM_ROAD_SEE 20.f            // правило порта: дорога в стольких метрах перед носом видна над крышей,
#define CAM_ROAD_CLEAR .2f           // с таким зазором над крышей коробки кузова (лестница glastruc, мигалки — выше коробки)
// Правка Роба 284 (04.10: «камера проходит сквозь проезжающие машины — рядом вижу их изнутри; после прыжка с трамплина камера
// уходит под текстуры»): лучи камеры (за машиной и пешком) видят и кузова машин — трафик, стоящие, машины миссий, свою машину,
// когда Гомер пешком (за рулём — нет: камера смотрит на неё). Кузов — коробка carBox с запасом CAM_CAR_PAD (ближняя плоскость не
// режет борт). Луч, начавшийся внутри кузова, не считается (цель камеры у самой машины). Правило порта. «flag camcarsold 1» —
// прежнее; «flag camgroundold 1» — прежний подъём над землёй без жёсткого пола
#define CAM_CAR_PAD .25f
#define CAM_GROUND_MIN .45f   // м: глаз камеры за машиной не ниже земли под ним на столько (жёсткий пол, поверх плавного подъёма)
static int camCarsOld, camGroundOld, chaseLevelOld;
static float rayCarFrac(const float a[3], const float b[3], const Car *k) {
  CarBox cb; memset(&cb, 0, sizeof cb); carBox(k, &cb);
  float rx = -cb.fz, rz = cb.fx, ex = cb.hl + CAM_CAR_PAD, ev = cb.hw + CAM_CAR_PAD, ylo = cb.y + cb.lo - CAM_CAR_PAD, yhi = cb.y + cb.hi + CAM_CAR_PAD;
  float pa[3] = {(a[0] - cb.x) * cb.fx + (a[2] - cb.z) * cb.fz, a[1], (a[0] - cb.x) * rx + (a[2] - cb.z) * rz};
  float pb[3] = {(b[0] - cb.x) * cb.fx + (b[2] - cb.z) * cb.fz, b[1], (b[0] - cb.x) * rx + (b[2] - cb.z) * rz};
  float lo[3] = {-ex, ylo, -ev}, hi[3] = {ex, yhi, ev}, t0 = 0.f, t1 = 1.f;
  if (pa[0] > lo[0] && pa[0] < hi[0] && pa[1] > lo[1] && pa[1] < hi[1] && pa[2] > lo[2] && pa[2] < hi[2]) return 1.f;   // начало внутри
  for (int i = 0; i < 3; i++) {
    float d = pb[i] - pa[i];
    if (fabsf(d) < 1e-6f) { if (pa[i] < lo[i] || pa[i] > hi[i]) return 1.f; continue; }
    float u = (lo[i] - pa[i]) / d, v = (hi[i] - pa[i]) / d; if (u > v) { float w = u; u = v; v = w; }
    if (u > t0) t0 = u; if (v < t1) t1 = v; if (t0 > t1) return 1.f;
  }
  return t0;
}
static float camCarsFrac(const float a[3], const float b[3]) {
  if (camCarsOld) return 1.f;
  float f = 1.f, mx = (a[0] + b[0]) * .5f, mz = (a[2] + b[2]) * .5f, R = hypotf(b[0] - a[0], b[2] - a[2]) * .5f + 8.f;
// №1127: движущаяся чужая машина (трафик, машины миссий) не тянет глаз к опоре: в оригинале линия «машина → камера» проверяется по статике мира, а сфера камеры отталкивается от машины
  // с запаздыванием 0,2 за кадр; ближняя плоскость её по-прежнему видит (csNearFree, own = 3). Стоящие и своя машина — как были. CAMCARS1127_OLD=1 — как в TEST84
#define CAMCAR_MV(K) do { const Car *km_ = (K); float v_[3]; carAnchorVelocity(km_, v_); if (CAMCARS1127_OLD || !camCarMoving(v_[0], v_[2])) CAMCAR(km_); } while (0)
#define CAMCAR(K) do { const Car *k_ = (K); if (hypotf(k_->x - mx, k_->z - mz) < R) { float g_ = rayCarFrac(a, b, k_); if (g_ < f) { f = g_; CarBox cb_; carBox(k_, &cb_); homerCamCarTop = cb_.y + cb_.hi; } } } while (0)   // №982: верх задержавшего кузова — камере у машины над крышей
  if (haveTraffic) {
    for (int i = 0; i < traffic.maxCars; i++) if (traffic.car[i].mode != TRM_OFF) CAMCAR_MV(&traffic.car[i].car);
    for (int i = 0; i < traffic.npark; i++) if (!traffic.park[i].gone) CAMCAR_MV(&traffic.park[i].car);
  }
  { const MsnCars *m = mvCars(); if (m) for (int i = 0; i < m->n; i++) if (m->c[i].on) CAMCAR_MV(&m->c[i].car); }
  if (onFoot() && haveCol) { CAMCAR(&car); if (oldCarOn) CAMCAR(&oldCar); }
#undef CAMCAR_MV
#undef CAMCAR
  return f;
}
// Правка Роба 331 (04.10: «камера врезается по краям экрана, входит внутрь машин, домов, людей; после удара с трамплина — под
// асфальт; камера никогда не заходит за текстуры»), viewer/camsafe.h. Тела для лучей камеры: кузова (трафик, стоящие, миссии, своя,
// оставленная) и люди (прохожие, персонажи миссий, Гомер пешком) ближе R к глазу. «flag camsafeold 1» — прежнее (красный контроль)
static int inRoom(void);
#define CS_GAG_MAX 3.f   // м: полуось коробки гэга, больше — не тело камеры (правка 338)
static Collect coll; static int haveColl;
#include "roomcam.h"
static int camGagOld, zoneCutOld = ROOMCAM_TRANS_SV_OLD ? 0 : 1;   // сверка g06: переходы камер зон теперь делает roomCamStepFrom (roomcam.c), прежний плавный переход main_loop.h выключен   // правка 338: прежнее — сетки гэгов не тела камеры / камеры зон срезом
static float csLastCp[3], csLastLk[3]; static int csLastOk;   // правка 338: поза камеры прошлого кадра (как нарисована)
static CsScene csScene; static int camSafeOld, camInOn, camInLog, camFollow; static float camPivot[3], camSafeNearV = NEAR_Z;
#define GB_FLOOR_K 7.67f   // №791: полоса GE ±2048 px — пол под камерой на высоте h выбрасывается при h/near > 7,67 (psp-render §4, tests/guard713)
static int camInGband, camInBump; static float camInBumpMax; static unsigned camBumpUs, camBumpN;   // замер №791: кадры за полосой GE; кадры с подъёмом глаза бампера и наибольший подъём, м
static int camInDataBad; static int camInFr, camInAny, camInFl[9], camInData; static float camInT0, camInNearMin = 9.f;   // замер «камера внутри» (flag camin)
static unsigned camSafeUs, camSafeUsMax, camSafeUsN;   // правка 331: цена правила за кадр (мкс, строка CAMIN)
static int camInJump, camInRelOk, camInCarPrev, camPivotCar; static float camInJmax, camInRel[3], camInPiv[3];   // правка 331: рывки глаза относительно опоры (> 0,5 м за кадр)
// №602: камера не входит в чужие кузова и людей между опорой и глазом: глаз обходит их так же, как стены мира (подтягивается к опоре,
// назад отходит плавно). CAM_AVOID_OLD=1 / «flag camavoidold 1» — прежнее (красный контроль)
#ifndef CAM_AVOID_OLD
#define CAM_AVOID_OLD 0
#endif
static int camAvoidOld = CAM_AVOID_OLD;
#ifndef CAM_WALLUP_OLD
#define CAM_WALLUP_OLD 0
#endif
static int camWallUpOld = CAM_WALLUP_OLD;
#define CS_PE ((camAvoidOld ? 1 : (camGeomOld ? 5 : 13)) | (camWallUpOld ? 0 : 128))   // луч опора -> глаз: мир, чужие тела (своя машина за рулём не в счёт), видимая геометрия (8)
#ifndef CAM_VEL_PAD_OLD
#define CAM_VEL_PAD_OLD 0
#endif
#define CAM_VEL_PAD 1.2f
static int camVelPadOld = CAM_VEL_PAD_OLD;
#ifndef CAM_PROP_BODY_OLD
#define CAM_PROP_BODY_OLD 0
#endif
static int camPropOld = CAM_PROP_BODY_OLD;
#define CAM_PROP_CLEAR .4f
#ifndef CAM_RAISE_OLD
#define CAM_RAISE_OLD 0
#endif
static int camRaiseOld = CAM_RAISE_OLD;
#ifndef CAM_PHONE_GUARD_OLD
#define CAM_PHONE_GUARD_OLD 0
#endif
static int camPhoneGuardOld = CAM_PHONE_GUARD_OLD;
#define CAM_PHONE_CLEAR 1.0f   // облёт будки: свободно перед глазом не меньше (ближняя плоскость по csNearFree)
// TEST21 (видео 2:07-2:15, f3900): фон облёта будки менялся за кадр — глаз прыгал при обходе изгороди (перелёт CS_GLIDE считался в м/с, на медленных кадрах шаг рос).
// Облёту: глаз за кадр не дальше CAM_PHONE_STEP от прошлого (если промежуточная точка годна). CAM_PHONE_STEP_OLD=1 / flag camphonestepold 1 — как было
#ifndef CAM_PHONE_STEP_OLD
#define CAM_PHONE_STEP_OLD 0
#endif
#define CAM_PHONE_STEP 0.5f
// TEST21 (L5 погоня, железо jmax 2,1 м): перелёт глаза к желаемому месту — 25 м/с, на кадрах 50 мс это 1,25 м и больше. Предел шага за кадр CS_GLIDE_FR.
// CAM_GLIDE_CAP_OLD=1 / flag camglidecapold 1 — как было
#ifndef CAM_GLIDE_CAP_OLD
#define CAM_GLIDE_CAP_OLD 0
#endif
#define CS_GLIDE_FR 0.45f
static int camGlideCapOld = CAM_GLIDE_CAP_OLD; static int jsN, jsSec = -1; static float jsMax; static char jsBuf[200];   // посекундные рывки глаза (> 0,5 м): «t:величина» для привязки к видео
#ifndef CAR_ZONE_GROUND_OLD
#define CAR_ZONE_GROUND_OLD 0
#endif
static int carZoneGroundOld = CAR_ZONE_GROUND_OLD;
static int camUniformOld, camPs2Dist;   // №554: см. chaseCamUpdate (B — camUniformOld, C — camPs2Dist)
#ifndef CAM_NEAR_SMOOTH_OLD
#define CAM_NEAR_SMOOTH_OLD 0
#endif
#define CS_NEAR_RISE 0.08f
static int camNearSmoothOld = CAM_NEAR_SMOOTH_OLD, csNearPrevOk; static float csNearPrev;
#ifndef CAM_NEAR_CACHE_OLD
#define CAM_NEAR_CACHE_OLD 0
#endif
#ifndef CAM_DATA_GROUND_OLD
#define CAM_DATA_GROUND_OLD 0
#endif
#ifndef CAM_SAFE_SKIP_OLD
#define CAM_SAFE_SKIP_OLD 1   // выключено: пропуск кадра правила даёт рывок до 7 м (L3, 32 с), экономит 0,5-0,8 мс; включение: -DCAM_SAFE_SKIP_OLD=0 / flag camsafeskipold 0
#endif
#ifndef CAM_ROOM_FOV_OLD
#define CAM_ROOM_FOV_OLD 0
#endif
static int roomFovOld = CAM_ROOM_FOV_OLD;
#ifndef CG_AHEAD_OLD
#define CG_AHEAD_OLD 0
#endif
static int cgAheadOld = CG_AHEAD_OLD;
static int camSafeSkipOld = CAM_SAFE_SKIP_OLD, camSkipPrev, camSkipN;
static int camNearCacheOld = CAM_NEAR_CACHE_OLD, camDataGroundOld = CAM_DATA_GROUND_OLD, dgFix;
static int camPhoneStepOld = CAM_PHONE_STEP_OLD, phPrevOk, phJump, phClamp, phHold; static float phPrevE[3], phJmax, phDmax;
static int camInGeom; static unsigned cpT[8], camPN; static float pe[3]; static int peOk;   // служебное (flag camin): мкс по частям правила глаза: 0 сцена, 1 cgStep, 2 глаз годен?, 3 кандидаты, 4 перелёт, 5 ближняя плоскость
// CAMPERF (ускорение камеры, дирижёр 23.10): мкс на кадр по подфазам камеры, строка CAMSUB раз в секунду (только смоук): chase — погоня chaseCamUpdate, safe — правило глаза
// camSafeEye (с пропуском кадров), floor — защита пола №837, dbg — служебная проверка глаза за стеной (гейт tests/camera; в туре идёт всегда), camf — запись CAMF,
// fnear — floorNearFrame, nfr — camSafeFrame (лучи ближней плоскости csNearFree), rw — camRealWall; rays/nf — вызовы csRayFrac и csNearFree за кадр
static unsigned csubT[10], csubN, csubT0, csubRay0, csubNf0;
#define CSUBT0 unsigned csb_ = smkOn ? sceKernelGetSystemTimeLow() : 0
#define CSUB(i) do { if (smkOn) csubT[i] += sceKernelGetSystemTimeLow() - csb_; } while (0)
// Диагностика окна «Оптимизация» (24.10): участки фазы города x_city, мс на кадр (строка CITYSUB, только смоук): setup — сброс, ирис, камера до ближней плоскости; cam — floorNearFrame + camSafeFrame;
// frame — перспектива, ближняя плоскость, решение ggOn; view — матрицы вида; sky0 — небо до города; cover — земля №437; pre — горизонт, силуэты, подложка; dl — город листами (CITYDL);
// post — комната, небо после города, джиттер мира. Только при смоуке (цена 9 отсечек ≈ 10 мкс на кадр PSP)
#if SHR_REL_SMK   // релиз: диагностики нет, сборка байт в байт прежняя
#define CSM0() ((void)0)
#define CSM(i) ((void)0)
#define CSFLUSH() ((void)0)
#else
static unsigned csuA[9], csuN, csuT;
#define CSM0() do { if (smkOn) csuT = sceKernelGetSystemTimeLow(); } while (0)
#define CSM(i) do { if (smkOn) { unsigned n_ = sceKernelGetSystemTimeLow(); csuA[i] += n_ - csuT; csuT = n_; } } while (0)
#define CSFLUSH() do { if (smkOn && ++csuN >= 30) { char l_[260]; snprintf(l_, sizeof l_, "CITYSUB t=%.1f sc=%s n=%u setup=%.3f cam=%.3f frame=%.3f view=%.3f sky0=%.3f cover=%.3f pre=%.3f dl=%.3f post=%.3f (мс/кадр)\n", smkT, smkScene, csuN, \
      csuA[0] / 1000.0 / csuN, csuA[1] / 1000.0 / csuN, csuA[2] / 1000.0 / csuN, csuA[3] / 1000.0 / csuN, csuA[4] / 1000.0 / csuN, csuA[5] / 1000.0 / csuN, csuA[6] / 1000.0 / csuN, csuA[7] / 1000.0 / csuN, csuA[8] / 1000.0 / csuN); smkWrite(l_); memset(csuA, 0, sizeof csuA); csuN = 0; } } while (0)
#endif
static void camSubReport(void) {
  if (!smkOn) return;
  unsigned n_ = sceKernelGetSystemTimeLow(); csubN++; if (!csubT0) csubT0 = n_;
  if (n_ - csubT0 < 1000000u) return;
  char l_[300]; float k_ = 1.f / (float)csubN;
  snprintf(l_, sizeof l_, "CAMSUB t=%.1f n=%u chase=%.0f safe=%.0f floor=%.0f dbg=%.0f camf=%.0f fnear=%.0f nfr=%.0f rw=%.0f rays=%.1f nf=%.2f mm=%u still=%u (мкс на кадр)\n", smkT, csubN,
           csubT[0] * k_, csubT[1] * k_, csubT[2] * k_, csubT[3] * k_, csubT[4] * k_, csubT[5] * k_, csubT[6] * k_, csubT[7] * k_,
           (csRayCalls - csubRay0) * k_, (csNfCalls - csubNf0) * k_, csRaysMismatch, csNfStillHit);
  smkWrite(l_); memset(csubT, 0, sizeof csubT); csubN = 0; csubT0 = n_; csubRay0 = csRayCalls; csubNf0 = csNfCalls;
}
#define CPT0 unsigned cpt_ = cgProf ? sceKernelGetSystemTimeLow() : 0
#define CPT(i) do { if (cgProf) { unsigned n_ = sceKernelGetSystemTimeLow(); cpT[i] += n_ - cpt_; cpt_ = n_; } } while (0)
static int csMovingN;   // №1127: сколько движущихся чужих машин (own = 3) ближе CAM_CARNEAR_R к глазу в сцене камеры (сбрасывает camSafeSceneX)
#define CAM_CARNEAR_R 6.f
static void csAddCar(const Car *k, const float e[3], float R) {
  int own = k == &car && !onFoot();
  if (csScene.nbox >= CS_MAXBOX || hypotf(k->x - e[0], k->z - e[2]) > R || fabsf(k->y - e[1]) > R) return;
  CarBox cb; memset(&cb, 0, sizeof cb); carBox(k, &cb);
  CsBox *b = &csScene.box[csScene.nbox++];
  b->x = cb.x; b->z = cb.z; b->fx = cb.fx; b->fz = cb.fz; b->hw = cb.hw; b->hl = cb.hl; b->y = cb.y; b->gx = cb.gx; b->gz = cb.gz; b->lo = cb.lo; b->hi = cb.hi; b->own = own;
  // №1127: чужая машина, которая едет (скорость по земле > CAMLAW_CARMOVE_V) — тело только для ближней плоскости (own = 3): положение глаза от неё не меняется, кадр не режется у края.
  // Своя машина (при пешем Гомере) и оставленная — не в счёт: их скорость — наша. CAMCARS1127_OLD=1 — как в TEST84
  if (!own && !CAMCARS1127_OLD && k != &car && k != &oldCar) { float v1[3]; carAnchorVelocity(k, v1); if (camCarMoving(v1[0], v1[2])) { b->own = 3; if (hypotf(k->x - e[0], k->z - e[2]) < CAM_CARNEAR_R) csMovingN++; } }
  // TEST19 (видео 4:44, L3): проезжающая машина за кадр проходит 0,3-0,5 м, камера уворачивалась от кузова там, где он был, и он «входил» в неё. Ящик чужого
  // тела растёт по скорости (до CAM_VEL_PAD м); CAM_VEL_PAD_OLD=1 / flag camvelpadold 1 — прежнее
  if (!own && !camVelPadOld) { float v[3]; carAnchorVelocity(k, v); float sp = sqrtf(v[0] * v[0] + v[2] * v[2]), pad = fminf(CAM_VEL_PAD, sp * .06f); b->hw += pad; b->hl += pad; }
}
static void csAddCyl(float x, float y, float z, float r, float h, const float e[3], float R) {
  if (csScene.ncyl >= CS_MAXCYL || hypotf(x - e[0], z - e[2]) > R || fabsf(y - e[1]) > R) return;
  CsCyl *c = &csScene.cyl[csScene.ncyl++]; c->x = x; c->z = z; c->r = r; c->y0 = y; c->y1 = y + h;
}
// №602 (Роб 15.10: «камера всё время проходит в текстуры»): коллизия — сетка высот/стен, а рисуется больше (карнизы, крыши, листва, вывески,
// края домов). Треугольники отрисовываемых кусков города клеток рядом (владелец 0, не небо) в коробке луча опора -> глаз + CAM_GEOM_PAD
// собираются в сцену камеры раз за кадр; луч опора -> глаз и лучи ближней плоскости против них. CAM_GEOM_OLD=1 / «flag camgeomold 1» — прежнее
#ifndef CAM_GEOM_OLD
#define CAM_GEOM_OLD 0   // 15.10 вечер: дешёвая версия (CAM_GEOM_FAST, сетка + кэш, p95 эмулятора как без неё); 1 — выключена (на TEST18 была выключена: ph6 +1,3…3,3 мс на PSP у версии по треугольникам)
#endif
#define CAM_GEOM_PAD 3.5f
static int camGeomOld = CAM_GEOM_OLD;
#ifndef CAM_NEAR_SP_OLD
#define CAM_NEAR_SP_OLD 0
#endif
#define CS_SP_R 8.f      // №727: стейтпропы в этом радиусе от глаза — тела ближней плоскости
#define CS_SP_MAX 3.5f   // полуось коробки больше — не в счёт (деревья с кроной, вышки)
static int camNearSpOld = CAM_NEAR_SP_OLD, camNearDataOld, nearPassClrOld;   // nearPassClrOld — второй проход своей машины (main_loop.h, NEARPASS_CLR)
static int camGeomTris, camGeomOver;
#define CAM_START_JUMP 6.f   // №1141: м; опора ушла дальше за кадр — перенос героя
#define CAM_START_WIN 120    // вызовов сбора сцены (≈ 1–2 с) после переноса: клетки у глаза достраиваются целиком
#define CAM_START_R 16.f     // м от глаза и от опоры до ящика клетки
static unsigned camStartBuilt;   // служебное: сколько клеток достроено окном старта
static void camGeomCollect(const float P[3], const float e[3]) {
  { CPT0;
    // TEST25 (разовая стройка сетки на новой клетке = рывок при въезде): строим в первую очередь клетки по ходу глаза (через ~1,5 с вперёд), а не ближайшие к нему.
    // CG_AHEAD_OLD=1 / flag cgaheadold 1 — ближайшие к глазу
    float ax = e[0], az = e[2];
    if (!cgAheadOld && peOk) { float vx = e[0] - pe[0], vz = e[2] - pe[2]; if (fabsf(vx) < 3.f && fabsf(vz) < 3.f) { ax += fmaxf(-60.f, fminf(60.f, vx * 45.f)); az += fmaxf(-60.f, fminf(60.f, vz * 45.f)); } }
    memcpy(pe, e, 12); peOk = 1;
    cgAhX = cgAheadOld ? -1e9f : ax; cgAhZ = az; cgStep(e[0], e[2]); CPT(1); }
  // №1141: перенос опоры больше CAM_START_JUMP м (старт миссии, сброс машины) или первая загрузка уровня — окно CAM_START_WIN вызовов: клетки рядом достраиваются сразу (camgrid.h cgBuildNear),
  // чтобы глаз и ближняя плоскость видели вывески, навесы и стенки с первого кадра. STARTCAM1141_OLD=1 — как в TEST84
  if (!STARTCAM1141_OLD && !inRoom()) {
    static float lp[3]; static int lpOk; static unsigned lgen; static int win, nb_ = 1;
    unsigned gen_ = camStartGenSel(cgGen, camLevelGen, CAMSTARTGEN1159_OLD);   // №1159 (g10b): поколение уровня, а не клеток города
    if (camStartArm(lpOk, lgen, gen_, hypotf(hypotf(P[0] - lp[0], P[1] - lp[1]), P[2] - lp[2]), CAM_START_JUMP)) { win = CAM_START_WIN; nb_ = 1; }
    memcpy(lp, P, 12); lpOk = 1; lgen = gen_;
    // №1159 (g12): пока ничего не достроено (рядом всё готово или клетки ещё грузятся), перебор клеток — раз в 4 вызова, а не каждый (STARTCAM1159_OLD=1 — каждый, как в TEST85)
    if (win > 0) { win--; if (STARTCAM1159_OLD || nb_ > 0 || !(win & 3)) { nb_ = cgBuildNear(e, P, CAM_START_R, 6); camStartBuilt += (unsigned)nb_; } }
  }
  csScene.ntri = 0; camGeomTris = 0; csScene.geomRay = (CAM_GEOM_FAST && !camGeomOld && !inRoom()) ? cgRayFracCam : 0;
  if (!pc || !cl || inRoom()) return;
  if (CAM_GEOM_FAST ? !(smkOn && camInOn && camInGeom) : (camGeomOld && !(smkOn && camInOn))) return;   // быстрый путь: треугольники только для замера «flag camingeom 1» (с flag camin они раньше шли всегда и в туре давали ph6 +1,4…2,4 мс)
  float lo[3], hi[3]; for (int k = 0; k < 3; k++) { lo[k] = fminf(P[k], e[k]) - CAM_GEOM_PAD; hi[k] = fmaxf(P[k], e[k]) + CAM_GEOM_PAD; }
  for (unsigned c = 0; c < ncell; c++) {
    const Cell *C = &cl[c];
    if (C->state != 2 || C->bb[0] > hi[0] || C->bb[3] < lo[0] || C->bb[1] > hi[1] || C->bb[4] < lo[1] || C->bb[2] > hi[2] || C->bb[5] < lo[2]) continue;
    for (unsigned k = C->first; k < C->first + C->n && k < npc; k++) {
      const Piece *d = &pc[k]; const Rec *r = &d->r;
      if (r->owner != 0 || (r->flags & 4) || !d->v || r->ni < 3 || r->sc <= 0.f) continue;
      if (r->bb[0] > hi[0] || r->bb[3] < lo[0] || r->bb[1] > hi[1] || r->bb[4] < lo[1] || r->bb[2] > hi[2] || r->bb[5] < lo[2]) continue;
      int v12 = (r->flags & F_V12) != 0, vs = v12 ? 12 : 16, xo = v12 ? 6 : 8, noidx = (r->flags & F_NOIDX) != 0, n = r->ni - r->ni % 3;
      if (!noidx && !d->idx) continue;
      const char *vbase = (const char *)d->v;
      if (r->flags & F_STRIP) { unsigned nl_; vbase = (const char *)pieceListT(d, 1, &nl_); if (!vbase) continue; n = (int)(nl_ - nl_ % 3); }   // К4: полосы -> список (слот 1)
      for (int i = 0; i < n; i += 3) {
        float w[9];
        for (int j = 0; j < 3; j++) {
          unsigned vi = noidx ? (unsigned)(i + j) : d->idx[i + j];
          const short *xy = (const short *)(vbase + vi * vs + xo);
          w[3 * j] = r->ox + (r->sc * (1.f / 32768.f)) * xy[0]; w[3 * j + 1] = r->oy + (r->sc * (1.f / 32768.f)) * xy[1]; w[3 * j + 2] = r->oz + (r->sc * (1.f / 32768.f)) * xy[2];
        }
        float tl[3] = {fminf(w[0], fminf(w[3], w[6])), fminf(w[1], fminf(w[4], w[7])), fminf(w[2], fminf(w[5], w[8]))},
              th[3] = {fmaxf(w[0], fmaxf(w[3], w[6])), fmaxf(w[1], fmaxf(w[4], w[7])), fmaxf(w[2], fmaxf(w[5], w[8]))};
        if (tl[0] > hi[0] || th[0] < lo[0] || tl[1] > hi[1] || th[1] < lo[1] || tl[2] > hi[2] || th[2] < lo[2]) continue;
        if (csScene.ntri >= CS_MAXTRI) { camGeomOver++; return; }
        memcpy(csScene.tri[csScene.ntri++], w, sizeof w);
      }
    }
  }
  camGeomTris = csScene.ntri;
}
// №922: над точкой p есть видимая геометрия города (кусок, чей ящик по x, z содержит p и поднимается выше p, но не выше p + CAM_WALL_UP + 0,5 по низу): крыша за фасадом без коллизии
static int camRoofAbove(const float p[3]) {
  for (unsigned c = 0; c < ncell; c++) {
    const Cell *C = &cl[c];
    if (C->state != 2 || p[0] < C->bb[0] - .5f || p[0] > C->bb[3] + .5f || p[2] < C->bb[2] - .5f || p[2] > C->bb[5] + .5f) continue;
    for (unsigned k = C->first; k < C->first + C->n && k < npc; k++) {
      const Rec *r = &pc[k].r;
      if (r->owner != 0 || (r->flags & 4) || !pc[k].v) continue;
      if (p[0] < r->bb[0] - .5f || p[0] > r->bb[3] + .5f || p[2] < r->bb[2] - .5f || p[2] > r->bb[5] + .5f) continue;
      if (r->bb[4] > p[1] + .2f && r->bb[1] < p[1] + CAM_WALL_UP + .5f) return 1;
    }
  }
  return 0;
}
static void camSafeSceneX(const float e[3], float R) {
  csScene.roofAbove = camRoofAbove;   CPT0; camGeomCollect(camPivot, e); CPT(6); csScene.useTri = !CAM_GEOM_FAST && !camGeomOld;
  csScene.w = onFoot() && G.pHomerWorld ? G.pHomerWorld : &world; csScene.nbox = csScene.ncyl = 0; csMovingN = 0;
  if (inRoom()) return;   // комната: свои стены, машин и прохожих нет
  if (haveTraffic) {
    for (int i = 0; i < traffic.maxCars; i++) if (traffic.car[i].mode != TRM_OFF) csAddCar(&traffic.car[i].car, e, R);
    for (int i = 0; i < traffic.npark; i++) if (!traffic.park[i].gone) csAddCar(&traffic.park[i].car, e, R);
    for (int i = 0; i < traffic.maxPeds; i++) { const TrPed *p = &traffic.ped[i];
      if (p->mode == TRP_WALK || p->mode == TRP_STAND || p->mode == TRP_GETUP || p->mode == TRP_DODGE) {
        float ht = traffic.pedm ? traffic.pedm[p->model].height : 1.8f; csAddCyl(p->x, p->y, p->z, TR_PED_R, ht > .5f ? ht : 1.8f, e, R); } }
  }
  { const MsnCars *m = mvCars(); if (m) for (int i = 0; i < m->n; i++) if (m->c[i].on) csAddCar(&m->c[i].car, e, R); }
  CPT(7);
  // TEST19 (видео L2, скамья): пропы (скамьи, урны, будки) закрывали героя — глаз стоял за скамьёй. Проп в радиусе R от глаза — цилиндр тела камеры (радиус и высота
  // типа, основание в y), как прохожий: глаз выходит из него и подтягивается к опоре, если проп между ними. CAM_PROP_BODY_OLD=1 / flag campropold 1 — прежнее
  if (haveProps && !camPropOld && haveColl) {
    // кэш: номера пропов в 15 м от глаза обновляются раз в 10 кадров или при сдвиге глаза > 6 м; в кадре — только они и только в 9 м (цена: перебор всех пропов уровня не каждый кадр)
    static unsigned pn[48], pnFr = 1u << 31, pnCnt; static float pnE[3]; static int pnN;
    pnCnt++;
    if (pnCnt - pnFr >= 10u || fabsf(e[0] - pnE[0]) > 6.f || fabsf(e[1] - pnE[1]) > 6.f || fabsf(e[2] - pnE[2]) > 6.f) {
      pnN = 0; pnFr = pnCnt; memcpy(pnE, e, 12);
      for (unsigned i = 0; i < props.ninst && pnN < 48; i++) { const PropState *st = &props.st[i];
        if (st->hy < .2f || st->hy > 1.1f || fmaxf(st->hx, st->hz) < .3f || fmaxf(st->hx, st->hz) > 3.f) continue;
        float dx = st->x - e[0], dz = st->z - e[2]; if (dx * dx + dz * dz < 225.f) pn[pnN++] = i; } }
    for (int j = 0; j < pnN && csScene.nbox < CS_MAXBOX; j++) {
      const PropState *st = &props.st[pn[j]];   // коробка пропа в осях модели (hx, hy, hz — полуразмеры), поворот — кватернион (w, x, y, z); тело камеры — ориентированный ящик
      if (st->hidden) continue;
      float dx = st->x - e[0], dz = st->z - e[2]; if (dx * dx + dz * dz > 81.f || fabsf(st->y - e[1]) > 9.f) continue;
      float qw = st->q[0], qx = st->q[1], qy = st->q[2], qz = st->q[3], fx = 2.f * (qx * qz + qw * qy), fz = 1.f - 2.f * (qx * qx + qy * qy), fl = hypotf(fx, fz);
      if (fl < .3f) continue;   // лежит на боку: ящик по курсу не строим
      CsBox *b = &csScene.box[csScene.nbox++]; memset(b, 0, sizeof *b);
      b->x = st->x; b->z = st->z; b->fx = fx / fl; b->fz = fz / fl; b->hw = st->hx; b->hl = st->hz; b->y = st->y; b->lo = -st->hy; b->hi = st->hy + CAM_PROP_CLEAR; } }   // запас сверху: луч от головы героя должен пройти над пропом с запасом, иначе тело за ним закрыто
  csAddCar(&car, e, R); if (oldCarOn) csAddCar(&oldCar, e, R);
  if (haveHomer && homer.solid == gameMsnSolid && homer.solidUser) {   // персонажи миссий (капсулы, как их твёрдость для Гомера)
    const GameMsn *gm = (const GameMsn *)homer.solidUser;
    for (int i = 0; i < gm->nnpc; i++) { const GameNpc *n = &gm->npc[i]; if (n->knock == TRP_FLY || n->knock == TRP_DOWN) continue;
      csAddCyl(n->pos.x, n->pos.y, n->pos.z, GAME_NPC_RADIUS, HOMER_HEIGHT, e, R); }
  }
  if (onFoot()) csAddCyl(homer.x, homer.y, homer.z, HOMER_RADIUS, HOMER_HEIGHT, e, R);
  // Правка Роба 338 (05.10: «у гэгов кривые ракурсы»): у сеток гэгов нет коллизии (качели, барбекю, бомбоубежище, ацтек во дворе
  // Симпсонов) — камера могла стоять внутри сетки. Коробка кусков SPR1 гэга (поза кадра 0) — тело для камеры, как кузов: глаз не в ней,
  // ближняя плоскость её не режет. Больше CS_GAG_MAX по полуоси — не в счёт (gag_gas: коробка вместе с баком АЭС 16,8 × 11,8 × 11 м).
  // Правило порта; «flag camgagold 1» — без гэгов
  if (haveColl && !camGagOld && coll.haveSp) for (unsigned i = 0; i < coll.ngag && csScene.nbox < CS_MAXBOX; i++) {
    const CollGag *g = &coll.gag[i]; if (g->room >= 0 || g->inst < 0 || (unsigned)g->inst >= coll.sp.ninst) continue;
    const SpRun *r = &coll.sp.run[g->inst]; const SpType *T = &coll.sp.type[coll.sp.inst[g->inst].type];
    if (!r->alive || T->h[0] > CS_GAG_MAX || T->h[1] > CS_GAG_MAX || T->h[2] > CS_GAG_MAX) continue;
    float cx = r->R[0] * T->c[0] + r->R[3] * T->c[1] + r->R[6] * T->c[2] + r->x, cy = r->R[1] * T->c[0] + r->R[4] * T->c[1] + r->R[7] * T->c[2] + r->y;
    float cz = r->R[2] * T->c[0] + r->R[5] * T->c[1] + r->R[8] * T->c[2] + r->z;
    if (hypotf(cx - e[0], cz - e[2]) > R || fabsf(cy - e[1]) > R) continue;
    float fx = r->R[6], fz = r->R[8], fl = hypotf(fx, fz); if (fl < 1e-3f) { fx = 0.f; fz = 1.f; } else { fx /= fl; fz /= fl; }
    CsBox *b = &csScene.box[csScene.nbox++]; memset(b, 0, sizeof *b);
    b->x = cx; b->z = cz; b->fx = fx; b->fz = fz; b->hl = T->h[2]; b->hw = T->h[0]; b->y = cy - T->h[1]; b->lo = 0.f; b->hi = 2.f * T->h[1];
  }
  // №721/727/728 (Роб 17.10: «чтобы ничего не обрезалось по краям кадра — ни домов, ни машин, ни людей»): стейтпропов (ящики, заборы, кусты,
  // почтовые ящики, знаки, автоматы) в сцене камеры не было — ближняя плоскость 1,5 м резала их у края кадра раньше, чем они уходили из кадра.
  // Коробка кусков типа (поза кадра 0) в CS_SP_R м от глаза — тело только для ближней плоскости (own = 2): она уходит почти к стеклу (CS_NEAR_MIN),
  // глаз от них не уворачивается. Полуось больше CS_SP_MAX (деревья с кроной, большие конструкции) — не в счёт. CAM_NEAR_SP_OLD=1 / flag camnearspold 1 — как было
  if (haveSP && !camNearSpOld && sprops.run) {
    static unsigned sn[64], snFr = 1u << 31, snCnt; static float snE[3]; static int snN;
    snCnt++;
    if (snCnt - snFr >= 10u || fabsf(e[0] - snE[0]) > 5.f || fabsf(e[1] - snE[1]) > 5.f || fabsf(e[2] - snE[2]) > 5.f) {
      snN = 0; snFr = snCnt; memcpy(snE, e, 12);
      for (unsigned i = 0; i < sprops.ninst && snN < 64; i++) { const SpRun *r = &sprops.run[i]; if (!r->alive) continue;
        const SpType *T = &sprops.type[sprops.inst[i].type]; if (T->kind != 0) continue;
        float hm = fmaxf(T->h[0], fmaxf(T->h[1], T->h[2])), rr = CS_SP_R + 5.f + hm; if (hm > CS_SP_MAX || hm < .1f) continue;
        float dx = r->x - e[0], dz = r->z - e[2]; if (dx * dx + dz * dz < rr * rr) sn[snN++] = i; } }
    for (int j = 0; j < snN && csScene.nbox < CS_MAXBOX; j++) {
      unsigned i = sn[j]; const SpRun *r = &sprops.run[i]; if (!r->alive) continue; const SpType *T = &sprops.type[sprops.inst[i].type];
      float c[3], hm = fmaxf(T->h[0], fmaxf(T->h[1], T->h[2])), o[3] = {r->x, r->y, r->z};
      for (int k = 0; k < 3; k++) c[k] = r->R[k] * T->c[0] + r->R[3 + k] * T->c[1] + r->R[6 + k] * T->c[2] + o[k];
      if (hypotf(c[0] - e[0], c[2] - e[2]) > CS_SP_R + hm || fabsf(c[1] - e[1]) > CS_SP_R + hm) continue;
      float fx = r->R[6], fz = r->R[8], fl = hypotf(fx, fz); if (fl < .3f) { fx = r->R[0]; fz = r->R[2]; fl = hypotf(fx, fz); }
      if (fl < 1e-3f) { fx = 0.f; fz = 1.f; } else { fx /= fl; fz /= fl; }
      float hl = 0.f, hw = 0.f, hy = 0.f;   // полуразмеры повёрнутой коробки по курсу, вбок и по высоте
      for (int k = 0; k < 3; k++) { const float *col = &r->R[3 * k]; hl += fabsf(col[0] * fx + col[2] * fz) * T->h[k]; hw += fabsf(-col[0] * fz + col[2] * fx) * T->h[k]; hy += fabsf(col[1]) * T->h[k]; }
      CsBox *b = &csScene.box[csScene.nbox++]; memset(b, 0, sizeof *b);
      b->x = c[0]; b->z = c[2]; b->fx = fx; b->fz = fz; b->hl = hl; b->hw = hw; b->y = c[1]; b->lo = -hy; b->hi = hy; b->own = 2; }
  }
}
static void camSafeScene(const float e[3], float R) { CPT0; camSafeSceneX(e, R); CPT(0); }
#ifndef CAM_EYE_K
#if CAM_554_OLD
#define CAM_EYE_K 1.10f
#else
#define CAM_EYE_K 1.00f
#endif
#endif
#ifndef CAM_TILT_TAU
#define CAM_TILT_TAU .5f   // с: №407, запаздывание наклона камеры за уклоном (подбор по записям PS2 v28_drive_cam и jz2, follow_ps2.py)
#endif
static int camHighOld, cam554Old = CAM_554_OLD, camLensOld = CAM_LENS_OLD;
// линза (№650): вертикальный угол кадра машины / пешком (runtime-откат flag camlensold 1 / cam554old 1) и перевод FOV камер из данных (вертикаль 4:3 -> широкий кадр, горизонталь та же)
static float lensCar(void) { float b = cam554Old ? 62.f : camLensOld ? 55.f : FOVY; return CAMSET1151_OLD ? b : camUserFovY(b, camUserFovNow); }   // №1151: множитель угла из CAMERA
static float lensFoot(void) { return cam554Old ? 54.f : camLensOld ? 48.f : HOMER_CAM_FOVY; }
static float lensData(float fy) { return camLensOld || cam554Old ? fy : 2.f * atanf(tanf(fy * (GU_PI / 360.f)) * .75f) * (180.f / GU_PI); }
// №617 (решение Роба 15.10): SELECT за рулём переключает вид камеры по кругу из данных машины (FollowCameraData: дальний, ближний; бампера и капота в данных
// нет). Сброс машины остаётся в меню паузы. Пешком SELECT ничего не делает. SELECT_CAM_OLD=1 — прежнее (SELECT = сброс машины, как у PS2 в записи)
#ifndef EXITYAW_ANY_OLD
#define EXITYAW_ANY_OLD 0   // проверка 18.10: 1 — курс от машины при любом переходе из машины в пешком (и при переносе), как cherry-pick 704
#endif
#ifndef SELECT_CAM_OLD
#define SELECT_CAM_OLD 0
#endif
#ifndef SELECT_CYCLE_OLD
#define SELECT_CYCLE_OLD 0
#endif
#define CAM_BUMP_FWD 0.60f   // №617: бамперная камера PS2: вперёд от центра машины, м
#define CAM_BUMP_UP 0.60f    // и вверх от низа кузова (у PS2 0,86 от центра), м
// №791 (Роб 18.10 повторно, ср. №772: «вид с бампера — низ асфальта бьётся сразу»): у бампера правило глаза выключено (№617: глаз у носа, «не в кузове» его бы выбросило),
// ближняя плоскость не ближе 0,25 м — на посадке с трамплина, на гребне и при клевке носом её нижний край уходил в асфальт (замер jmp_bumper, сценарий трамплинов a331:
// ближняя плоскость резала видимый пол в 18–153 кадрах блока, угол под землёй в 154–247). Закон объектива: не пролазит — камера двигается. Глаз и низ ближней плоскости
// (три точки на глубине BUMP_NEAR) — не ниже опоры коллизии под ними + зазор; не хватает — глаз и взгляд поднимаются (вверх сразу, вниз плавно). BUMP_GROUND_OLD=1 — как было
#ifndef BUMP_GROUND_OLD
#define BUMP_GROUND_OLD 0
#endif
#define BUMP_NEAR .35f        // м: глубина, на которой низ кадра не должен быть под землёй (ближняя плоскость с запасом точности глубины)
#define BUMP_EYE_CLEAR .35f   // м: глаз над асфальтом не ниже
#define BUMP_EDGE_CLEAR .40f  // м: низ ближней плоскости над опорой коллизии; видимый асфальт бывает выше коллизии на 0,3–0,37 м (замер CAMVF у трамплина L1 −120,380), на ровной дороге
                              // глаз бампера ≈ 0,76 м над дорогой — подъёма нет, он включается на клевке, гребне и посадке
#define BUMP_RAISE_MAX 1.5f   // м: выше не поднимаем (дальше — стена/крыша, не наш случай)
static int bumpGroundOld = BUMP_GROUND_OLD;
static int selectCycleOld = SELECT_CYCLE_OLD;   // 1 / flag selectcycleold 1: SELECT = два вида (дальний/ближний), без вида от первого лица
static int selectCamOld = SELECT_CAM_OLD, chaseView;   // chaseView: 0 дальний (как был), 1 ближний
static int chaseOld = CAM_NEAR_OLD, carCamLog;   // carCamLog — служебное «flag carcamlog»: строка CARCAM раз в секунду
typedef struct { const char *name; int ps2; float back, eye, need, tgtY, tgtF, wall; } ChaseInfo;   // служебное: строка CARCAM
static ChaseInfo chaseInfo;
static const CarCamData *carCamFind(const char *name) {
  for (int i = 0; name && i < CAR_CAMS; i++) if (!strcmp(carCamTab[i].name, name)) return &carCamTab[i];
  return 0;
}
static float camFit(void) {
  if (CAM_FIT_K > 0.f) return CAM_FIT_K;
  if (!camLensOld && !cam554Old) return 1.f;   // №650: линза = широкий кадр PS2 (горизонталь 77°, 16:9) — дистанция данных 1:1, подгонка не нужна (на 4:3 вертикали давала ×1,155 = 11,2 м вместо 9,7)
  float fov = lensCar();
  float hp = tanf(CAM_PS2_HFOV * (GU_PI / 360.f)), vp = hp / CAM_PS2_ASPECT, vq = tanf(fov * (GU_PI / 360.f)), hq = vq * ASPECT;
  return sqrtf(hp * vp / (hq * vq));
}
static int chaseRetry;   // круг 6 (СВЕРКА_КАМЕРА 1.15): второй проход после среза на идеальное место
static void chaseCamUpdate(const Car *c, float yawOfs, float dt, float cam[3], float look[3]) {
  ChaseCam *s = &chase;
  camBurstK += ((!CAM_BURST_SV_OLD && c->burstOn ? CAM_BURST_K : 1.f) - camBurstK) * (1.f - expf(-dt / .25f));   // сверка g06 круг 2 (g01-cars 2.2)
  if (!CAM_UFO_SV_OLD && chaseView == 2 && haveHomer && homer.solid == gameMsnSolid && homer.solidUser && ((const GameMsn *)homer.solidUser)->ufoGrab == 1) { chaseView = 0; s->svInit = 0; }   // сверка g06 круг 2 (g07-missions P04:Д9): бампер → дальняя следящая срезом
  float anchorV[3];carAnchorVelocity(c,anchorV);
  float ty = wrapPi(c->h + yawOfs);
  if (s->init && (fabsf(c->x - s->lx) > 8.f || fabsf(c->z - s->lz) > 8.f || fabsf(c->y - s->ly) > 8.f)) s->init = 0;   // телепорт
  if (!s->init) { memset(s, 0, sizeof *s); s->init = 1; s->yaw = ty; s->ay = c->y; }
  else if (dt > 0.f) {
    // опора по горизонтали = машина + ошибка e. Ожидаем, что машина сдвинется на скорость * dt (упреждение);
    // сдвинулась иначе (рывок, удар, неровный шаг физики) — разницу берёт ошибка и плавно гасит (10 рад/с)
    s->ex += s->lx + (s->lvx + anchorV[0]) * .5f * dt - c->x;
    s->ez += s->lz + (s->lvz + anchorV[2]) * .5f * dt - c->z;
    cdStep(&s->ex, &s->evx, 10.f, dt); cdStep(&s->ez, &s->evz, 10.f, dt);
    float l = sqrtf(s->ex * s->ex + s->ez * s->ez); if (l > 1.2f) { s->ex *= 1.2f / l; s->ez *= 1.2f / l; }
    // №388: предсказываем высоту по скорости тела, как горизонтальное положение. Фильтр скорости
    // задерживал подъём на трамплине; пружина сглаживает только остаток предсказания (удар/рывок).
    s->vyLP += (anchorV[1] - s->vyLP) * (1.f - expf(-dt / 0.35f));
    s->ey += s->ly + (s->lvy + anchorV[1]) * .5f * dt - c->y;
    cdStep(&s->ey, &s->evy, 5.f, dt);
    if (s->ey > 2.f) s->ey = 2.f; if (s->ey < -2.f) s->ey = -2.f;
    float ay = c->y + s->ey, mv = (s->vyLP > 0.f ? 8.f : 14.f) * dt;   // скорость камеры по вертикали ограничена
    if (fabsf(anchorV[1]) < 6.f) { if (ay > s->ay + mv) ay = s->ay + mv; if (ay < s->ay - mv) ay = s->ay - mv; s->ey = ay - c->y; }
    // Поворот за курсом через пружину. Жёсткое ограничение отставания
    // переносило камеру сразу на другую сторону при развороте кузова от удара.
    // №407 (Роб 09.10: «камера отстаёт не так, как надо»): жёсткость поворота — по записи PS2 (близнец v28_drive_cam, 1500 кадров
    // езды по маршруту с поворотами, 50 Гц): та же пружина с ω = 8 повторяет курс камеры оригинала со средней ошибкой 0,36°
    // (p95 1,1°); прежние 6 — 1,26° (p95 4,6°), камера отставала сильнее оригинала. Гейт tests/camera/yaw_ps2.py. Прежнее: -DCAM_YAW_W=6
#ifndef CAM_YAW_W
#define CAM_YAW_W 8.f
#endif
    // сверка g06 (g06-camera:30): курс камеры догоняет курс машины фильтром оригинала (63% за 0,29 с, 95% за 0,51 с, перелёт ~1%); было — критическая
    // пружина ω 8 (63% за 0,21 с, 95% за 0,59 с). CAM_YAW_SV_OLD=1 — как было
    float e = wrapPi(s->yaw - ty), ev = s->yawV;
    if (CAM_YAW_SV_OLD) cdStep(&e, &ev, CAM_YAW_W, dt); else svStep(&e, &ev, CAM_SV_YAW_W, CAM_SV_Z, dt);
    s->yaw = wrapPi(ty + e); s->yawV = ev;
    // на скорости чуть дальше (до +0.8 м), плавно — прежнее правило порта. №407 (сверка с PS2, tests/camera/follow_ps2.py): у
    // оригинала дистанция от скорости не зависит — v28_drive_cam, позади машины 9,54 м выше 14 м/с и 9,51 ниже 3 м/с (+0,03), у порта
    // было +0,28. Прежнее: -DCAM_PULL_OLD=1
    float sp = sqrtf(anchorV[0] * anchorV[0] + anchorV[2] * anchorV[2]);
#if defined(CAM_PULL_OLD) && CAM_PULL_OLD
    s->pull += (fminf(sp * .025f, .8f) - s->pull) * (1.f - expf(-dt * 2.f));
#else
    (void)sp; s->pull = 0.f;
#endif
#if !defined(CAM_TILT_OLD) || !CAM_TILT_OLD
    // №407: наклон камеры за уклоном дороги (сверка с PS2, tests/camera/follow_ps2.py). У оригинала глаз и точка взгляда повёрнуты
    // вокруг начала модели машины на угол, который догоняет уклон пола под машиной по курсу с запаздыванием, а в полёте стоит:
    // v28_drive_cam — спуск −5,8° даёт поворот −5,4° (глаз выше на 1 м), jz2 — на трамплине до +13° (глаз 1,2 м над машиной вместо
    // 3,4, взгляд −5° вместо −15°), весь полёт держится 11–13°, после посадки уходит к 0 за ~1,5 с. Подбор по записям (1500 кадров
    // v28 + прыжок jz2): первый порядок с постоянной CAM_TILT_TAU — ошибка поворота к PS2 0,57° (v28) и 1,8° (jz2), без наклона
    // 1,1° и 5,9°. Вход — пол кузова (gx, gz) по курсу, пока колёса на земле. Прежнее (глаз над машиной по вертикали мира):
    // -DCAM_TILT_OLD=1
    if (c->wheels > 0) {
      float sl = atanf(c->gx * sinf(c->h) + c->gz * cosf(c->h));
      s->tilt += (sl - s->tilt) * (1.f - expf(-dt / CAM_TILT_TAU));
      if (!CAM_FREEZE_SV_OLD) s->roll += (c->roll - s->roll) * (1.f - expf(-dt / CAM_TILT_TAU));   // сверка g06 (g06-camera:42): и крен кузова, с той же постоянной
    }
#endif
  }
  s->lx = c->x; s->ly = c->y; s->lz = c->z; s->lvx = anchorV[0]; s->lvy = anchorV[1]; s->lvz = anchorV[2];
  float ax = c->x + s->ex, az = c->z + s->ez, ay = c->y + s->ey; s->ay = ay;
  // №388/378: Роб 06.10 расширил перенос родных профилей FollowCamera на все уровни
  const CarCamData *cd = (chaseOld || (chaseLevelOld && gameLevel != 1)) ? 0 : carCamFind(c->p.name);
  float back, eyeH, tgtY = CAM_LOOK_H, tgtF = 0.f, roadNeed = 0.f;
  if (cd) {   // правка 198+: кадр оригинала (дальний вариант) с поправкой на наш угол кадра; дорога перед носом над крышей
    float lift = carModelLift(c,cv.lift > 0.f ? cv.lift : 0.f), nose = c->p.bodyOfs + c->p.halfL;
    float vMag = chaseView ? cd->nearMag : cd->farMag, vEl = chaseView ? cd->nearElev : cd->farElev;   // №617: вид по SELECT
    back = vMag * sinf(vEl) * camFit() + s->pull;
    eyeH = lift + vMag * cosf(vEl);                         // высота глаза оригинала (над низом колёс)
    // Правка Роба 494 (10.10, «камера за машиной на всех уровнях как на L1»): у машин уровней 2–7 дальний вариант FollowCameraData короче, чем у
    // famil_v L1 (honor 7,8 · lisa 8,1 · marge 7,4 · apu 7,7 · bart 7,6 · homer 7,1 м против 8,3 у famil_v). Одно правило: дистанция не
    // меньше, чем у famil_v по тем же данным и тому же CAM_FIT (L1 — «красивый угол»: машина в нижней трети, дорога видна над крышей), высота
    // глаза над моделью растёт во столько же раз (угол камеры от вертикали сохраняется). Большие машины (автобусы, грузовики: дальность 9–12 м)
    // сохраняют свою (правка 198). CAM_UNIFORM_OLD=1 — прежнее (у каждой машины своя)
#ifndef CAM_UNIFORM_OLD
#define CAM_UNIFORM_OLD 0
#endif
    // сверка g06 (g06-camera:6): дальность, угол и смещение цели у каждой машины свои, по данным диска, без подтягивания до famil_v; CAM_DATA_SV_OLD=1 — выравнивание №494
    if (CAM_DATA_SV_OLD && !CAM_UNIFORM_OLD && !camUniformOld && !camPs2Dist && !chaseView) {   // №554: flag camuniformold 1 — дальность и высота за машиной по данным FollowCameraData каждой машины (как PS2), без выравнивания до famil_v (№494)
      const CarCamData *fm = carCamFind("famil_v");
      float bmin = fm ? fm->farMag * sinf(fm->farElev) * camFit() : 0.f, b0 = back - s->pull;
      if (bmin > b0 && b0 > 1e-3f) { float kk = bmin / b0; back = bmin + s->pull; eyeH = lift + (eyeH - lift) * kk; }
    }
    roadNeed = (c->p.bodyHi+c->modelRaise + CAM_ROAD_CLEAR) * (back + nose + CAM_ROAD_SEE) / CAM_ROAD_SEE;   // луч глаз -> дорога в CAM_ROAD_SEE м за носом над крышей
    // сверка g06 (g06-camera:7): высота глаза = дальность × косинус угла данных над началом модели, больше ничего (правило «дорога видна над крышей» — порта, у оригинала нет);
    // CAM_EYEH_SV_OLD=1 — прежнее
    if (CAM_EYEH_SV_OLD && eyeH < roadNeed) eyeH = roadNeed;
    if (!camHighOld) eyeH = lift + (eyeH - lift) * (cam554Old ? 1.10f : CAM_EYE_K); // 360/554: подъём над моделью
    // №554, вариант C (дирижёр, A/B/C для Роба): дальность и высота за машиной — числа PS2 Стенда3 (ps2-camera-follow-l2-l7 + ps2-camera-follow-numbers; по звуковому слушателю ±0,5 м,
    // famil_v — по настоящему глазу, Far Follow 8,4 / 3,8): назад / вверх от точки машины. Остальные машины (автобусы и др.) — как B. flag camps2dist 1; по умолчанию выкл
    if (camPs2Dist && !chaseView) {
      static const struct { const char *n; float d, h; } T[] = {{"famil_v", 8.4f, 3.8f}, {"honor_v", 7.0f, 3.0f}, {"lisa_v", 9.8f, 3.1f}, {"marge_v", 6.6f, 1.7f}, {"apu_v", 6.4f, 1.5f}, {"bart_v", 6.4f, 2.45f}, {"homer_v", 10.f, 2.7f}};
      for (unsigned i = 0; i < sizeof T / sizeof T[0]; i++) if (!strcmp(c->p.name, T[i].n)) { back = T[i].d + s->pull; eyeH = T[i].h; break; }
    }
    tgtY = lift + cd->ty; tgtF = cd->tz;
  } else { back = c->p.camMag * sinf(c->p.camElev) + s->pull; eyeH = CAM_TGT_H + c->p.camMag * cosf(c->p.camElev); }   // прежнее: ближний (CAR1 v2)
  if (chaseView != 2) { back *= camUserCarDe(homerCamUserD); eyeH = tgtY + (eyeH - tgtY) * camUserCarHe(homerCamUserH); }   // №1151: свои высота и дальность машины (CAMSET1151_OLD=1 — общие с пешком)   // №996: Distance и Height страницы CAMERA (дальше стены считают уже новый глаз); бампер — глаз у кузова, не трогаем
  // сверка g06 (g06-camera:31, 35, 45): дальность и вертикальный угол догоняют цель каждый своим фильтром (дальность 63% за 0,26 с, угол 63% за 0,45 с, 95% за 0,82 с), поэтому
  // смена вида (дальний/ближний), своя настройка Distance/Height и другие скачки цели идут плавно; в бампере и после него фильтры начинаются заново (срез, g06-camera:35, 45).
  // CAM_PITCH_SV_OLD=1 — как было (цель сразу)
  { static float lastUD = -1.f, lastUH = -1.f;
    float uD_ = camUserCarDe(homerCamUserD), uH_ = camUserCarHe(homerCamUserH);
    if (uD_ != lastUD || uH_ != lastUH) { lastUD = uD_; lastUH = uH_; s->svInit = 0; }   // №996: смена Height/Distance — сразу на новом месте
    if (chaseView == 2) s->svInit = 0;
    else if (!CAM_PITCH_SV_OLD) {
      float pl = carModelLift(c, cv.lift > 0.f ? cv.lift : 0.f), u = eyeH - pl, D = hypotf(back, u), E = atan2f(u, back);
      if (!s->svInit) { s->fD = D; s->fDv = 0.f; s->fE = E; s->fEv = 0.f; s->svInit = 1; }
      else if (dt > 0.f) {
        float e = s->fD - D, ev = s->fDv; svStep(&e, &ev, CAM_SV_DIST_W, CAM_SV_Z, dt); s->fD = D + e; s->fDv = ev;
        e = s->fE - E; ev = s->fEv; svStep(&e, &ev, CAM_SV_PITCH_W, CAM_SV_Z, dt); s->fE = E + e; s->fEv = ev;
      }
      back = s->fD * cosf(s->fE); eyeH = pl + s->fD * sinf(s->fE);
    } else s->svInit = 1;
  }
  chaseInfo.name = c->p.name; chaseInfo.ps2 = cd != 0; chaseInfo.wall = 1.f; chaseInfo.back = back; chaseInfo.eye = eyeH; chaseInfo.need = roadNeed;
  chaseInfo.tgtY = tgtY; chaseInfo.tgtF = tgtF;
  cam[0] = ax - sinf(s->yaw) * back; cam[2] = az - cosf(s->yaw) * back; cam[1] = ay + eyeH;
  float tgtFw = tgtF, tgtUp = tgtY, look_rollx = 0.f, look_rollz = 0.f;
#if !defined(CAM_TILT_OLD) || !CAM_TILT_OLD
  if (s->tilt != 0.f || (!CAM_FREEZE_SV_OLD && s->roll != 0.f)) {   // №407: поворот глаза и цели вокруг начала модели (над низом колёс на lift) на наклон s->tilt
    float pl = carModelLift(c, cv.lift > 0.f ? cv.lift : 0.f), C = cosf(s->tilt), S = sinf(s->tilt);
    float ef = -back, eu = eyeH - pl, f2 = ef * C - eu * S, u2 = ef * S + eu * C;
    cam[0] = ax + sinf(s->yaw) * f2; cam[2] = az + cosf(s->yaw) * f2; cam[1] = ay + pl + u2;
    float lf = tgtF, lu = tgtY - pl; tgtFw = lf * C - lu * S; tgtUp = pl + lf * S + lu * C;
    if (!CAM_FREEZE_SV_OLD && s->roll != 0.f) {   // сверка g06 (g06-camera:42): крен — вертикаль стрелы наклоняется вбок вокруг курса; вправо = (−cos, sin) в осях движка
      float cr = cosf(s->roll), sr = sinf(s->roll), rx = -cosf(s->yaw), rz = sinf(s->yaw);
      cam[0] += rx * u2 * sr; cam[2] += rz * u2 * sr; cam[1] = ay + pl + u2 * cr;
      float lu2 = tgtUp - pl; tgtUp = pl + lu2 * cr;
      look_rollx = rx * lu2 * sr; look_rollz = rz * lu2 * sr;
    }
  }
#endif
  // не под землю на склоне: подъём над землёй плавный (жёсткий упор давал рывки картинки на перепадах)
  float g, need = 0.f; if (ground(cam[0], cam[2], cam[1] + 1.f, 0.f, &g) && cam[1] < g + .8f) need = g + .8f - cam[1];
  if (need > 1.2f) need = 1.2f;   // выше — крыша/стена за машиной, не лезем на неё
  { float e = s->lift - need; cdStep(&e, &s->liftV, 6.f, dt); s->lift = need + e; }
  cam[1] += s->lift;
  // Обход стен проверяет ту же цель, которую затем получает рендер (включая родное смещение).
  look[0] = ax + sinf(s->yaw) * tgtFw + look_rollx; look[1] = ay + tgtUp; look[2] = az + cosf(s->yaw) * tgtFw + look_rollz;
  if (!CAM_LOOKUP_SV_OLD && camLookUp > 0.f && chaseView != 2) look[1] += 5.f * camLookUp;   // сверка g06 (g06-camera:26): взгляд вверх — цель выше на 5 м, пока держат (ввод: кнопки PSP решает Роб, camLookUp пока 0)
  // сверка g06 (g06-camera:32): точка взгляда отстаёт своим фильтром (63% за 0,13 с) по всем осям; бампер — без отставания. CAM_LOOK_SV_OLD=1 — сразу от опоры
  if (!CAM_LOOK_SV_OLD && chaseView != 2 && s->svInit && dt > 0.f) {
    float lk0[3] = {look[0], look[1], look[2]};
    if (s->fLk[0] == 0.f && s->fLk[1] == 0.f && s->fLk[2] == 0.f) memcpy(s->fLk, lk0, 12);
    for (int k = 0; k < 3; k++) { float e = s->fLk[k] - lk0[k], ev = s->fLkV[k]; svStep(&e, &ev, CAM_SV_LOOK_W, CAM_SV_Z, dt); s->fLk[k] = lk0[k] + e; s->fLkV[k] = ev; look[k] = s->fLk[k]; }
  } else if (chaseView == 2 || !s->svInit) { memset(s->fLk, 0, 12); memset(s->fLkV, 0, 12); }
  // сверка g06 (g06-camera:3, 42): замирание в полёте. Все колёса в воздухе или кузов наклонён больше 30° от вертикали (up.y < 0,866) — камера держит своё смещение от опоры
  // в осях мира (не едет за курсом) и ещё 0,2 с после; потом плавно возвращается (как догоняет остальное). CAM_FREEZE_SV_OLD=1 — без замирания
  if (!CAM_FREEZE_SV_OLD && chaseView != 2) {
    float upY = 1.f - 2.f * (c->q[1] * c->q[1] + c->q[3] * c->q[3]);
    int unstable = c->wheels == 0 || upY < .866f;
    if (unstable) {
      s->stabT = 0.f;
      if (!s->fz && s->preE[0] * s->preE[0] + s->preE[1] * s->preE[1] + s->preE[2] * s->preE[2] > 1.f) { s->fz = 1; memcpy(s->fzE, s->preE, 12); memcpy(s->fzL, s->preL, 12); }
    } else { s->stabT += dt;
      if (s->fz && s->stabT >= .2f) { s->fz = 0;
        for (int k = 0; k < 3; k++) { float ak = k == 0 ? ax : k == 1 ? ay : az; s->bkE[k] = ak + s->fzE[k] - cam[k]; s->bkEv[k] = 0.f; s->bkL[k] = ak + s->fzL[k] - look[k]; s->bkLv[k] = 0.f; } } }
    if (s->fz) { for (int k = 0; k < 3; k++) { float ak = k == 0 ? ax : k == 1 ? ay : az; cam[k] = ak + s->fzE[k]; look[k] = ak + s->fzL[k]; } }
    else { for (int k = 0; k < 3; k++) { float ak = k == 0 ? ax : k == 1 ? ay : az;
        s->preE[k] = cam[k] - ak; s->preL[k] = look[k] - ak;
        cdStep(&s->bkE[k], &s->bkEv[k], 6.f, dt); cdStep(&s->bkL[k], &s->bkLv[k], 6.f, dt); cam[k] += s->bkE[k]; look[k] += s->bkL[k]; } }
  }
  // сверка g06 (g06-camera:4, 13): камера заднего хода. Включается, когда тормоз нажат, машина едет назад (больше 0,5 м/с) и либо v² > 45 (6,7 м/с), либо контакт со стеной; выходит только
  // когда машина перестала ехать назад И газ ≥ 0,5; включается и выключается срезом. Не разворачивается: держит дистанцию (и по высоте) на момент включения, тянется за машиной
  // (запаздывание 0,05 + v²/400, не больше 1, за 16 мс), а ближе 7 м, если машина едет на неё и камера позади носа, уходит вбок на полдистанции; взгляд — прямо в машину, с запаздыванием 0,5.
  // CAM_REVERSE_SV_OLD=1 — камеры заднего хода нет
  if (!CAM_REVERSE_SV_OLD && chaseView != 2 && dt > 0.f) {
    int back_ = c->v < -.5f;
    if (!s->rev) {
      int stable_ = CAM_REVERSE2_SV_OLD || (c->wheels > 0 && 1.f - 2.f * (c->q[1] * c->q[1] + c->q[3] * c->q[3]) >= .866f);   // круг 6: не в воздухе и не опрокинута
      if (stable_ && camSvBrk > .3f && back_ && (c->v * c->v > 45.f || c->contacts > 0)) { s->rev = 1; memcpy(s->revE, cam, 12); memcpy(s->revL, look, 12);
        for (int k = 0; k < 3; k++) { float ak = k == 0 ? ax : k == 1 ? ay + tgtY : az; s->revV[k] = cam[k] - ak; }
        s->revD = sqrtf(s->revV[0] * s->revV[0] + s->revV[1] * s->revV[1] + s->revV[2] * s->revV[2]);
        { float f_[3] = {look[0] - cam[0], look[1] - cam[1], look[2] - cam[2]}, fl_ = sqrtf(f_[0] * f_[0] + f_[1] * f_[1] + f_[2] * f_[2]), un_[3] = {0.f, 1.f, 0.f};   // высота взгляда над машиной: пересечение вертикали машины с плоскостью камеры
          if (fl_ > 1e-3f) { for (int k = 0; k < 3; k++) f_[k] /= fl_; un_[0] = -f_[0] * f_[1]; un_[1] = 1.f - f_[1] * f_[1]; un_[2] = -f_[2] * f_[1]; }
          s->revOy = un_[1] > .2f ? (un_[0] * s->revV[0] + un_[1] * s->revV[1] + un_[2] * s->revV[2]) / un_[1] : s->revV[1]; } }
    } else if (!back_ && camSvThr >= .5f) s->rev = 0;   // выход срезом: камера за машиной стоит там, где её ведёт фильтр
    if (s->rev && !CAM_REVERSE2_SV_OLD) {
      // круг 6 (оригинал): желаемое место — от ТЕКУЩЕГО положения камеры: дальше дистанции включения — на неё, высота = высоте включения; ближе 7 м и машина едет на камеру — ползёт вбок
      // на (дистанция / 2) × 0,05 за кадр; подтяжка 0,05 + v²/400 (не больше 1) за кадр; взгляд — на машину, поднятый на высоту взгляда над машиной, запаздывание 0,5; закрыто стеной — над целью на 6 м
      float tg[3] = {ax, ay + tgtY, az}, d3[3] = {s->revE[0] - tg[0], s->revE[1] - tg[1], s->revE[2] - tg[2]}, des[3];
      float vv = c->v * c->v, lagc = .05f + fminf(vv / 400.f, 1.f), k1 = lagc >= 1.f ? 1.f : 1.f - powf(1.f - lagc, dt * 60.f), k2 = 1.f - powf(.5f, dt * 60.f);
      camRevDesired(d3, s->revD, s->revV[1], c->vel, sinf(c->h), cosf(c->h), dt, des);
      for (int k = 0; k < 3; k++) s->revE[k] += (tg[k] + des[k] - s->revE[k]) * k1;
      float lt[3] = {tg[0], tg[1] + s->revOy, tg[2]};
      for (int k = 0; k < 3; k++) s->revL[k] += (lt[k] - s->revL[k]) * k2;
      if (haveCol && colRayFrac(&world, s->revL, s->revE, 0) < 1.f) { s->revE[0] = s->revL[0]; s->revE[1] = s->revL[1] + 6.f; s->revE[2] = s->revL[2]; s->revD = 6.f; }
      memcpy(cam, s->revE, 12); memcpy(look, s->revL, 12);
    }
    else if (s->rev && CAM_REVERSE2_SV_OLD) {
      float tg[3] = {ax, ay + tgtY, az}, d3[3] = {s->revE[0] - tg[0], s->revE[1] - tg[1], s->revE[2] - tg[2]}, d = sqrtf(d3[0] * d3[0] + d3[1] * d3[1] + d3[2] * d3[2]);
      float D0 = sqrtf(s->revV[0] * s->revV[0] + s->revV[1] * s->revV[1] + s->revV[2] * s->revV[2]), vv = c->v * c->v, lagc = .05f + fminf(vv / 400.f, 1.f);
      float k1 = lagc >= 1.f ? 1.f : 1.f - powf(1.f - lagc, dt * 60.f), k2 = 1.f - powf(.5f, dt * 60.f);
      float want3[3] = {s->revE[0], s->revE[1], s->revE[2]};
      if (d > D0 && d > 1e-3f) for (int k = 0; k < 3; k++) want3[k] = tg[k] + d3[k] / d * D0;
      float rx = c->x - s->revE[0], rz = c->z - s->revE[2], rl = sqrtf(rx * rx + rz * rz), fwx = sinf(c->h), fwz = cosf(c->h);
      int toward = c->vel[0] * (s->revE[0] - c->x) + c->vel[2] * (s->revE[2] - c->z) > 0.f, behind = fwx * (s->revE[0] - c->x) + fwz * (s->revE[2] - c->z) < 0.f;
      if (rl < 7.f && toward && behind) {   // уходит вбок на полдистанции, в ту сторону от оси машины, где уже стоит
        float sx = -fwz, sz = fwx, sg = (s->revE[0] - c->x) * sx + (s->revE[2] - c->z) * sz >= 0.f ? 1.f : -1.f;
        want3[0] = tg[0] + sx * sg * D0 * .5f; want3[2] = tg[2] + sz * sg * D0 * .5f; }
      for (int k = 0; k < 3; k++) s->revE[k] += (want3[k] - s->revE[k]) * k1;
      for (int k = 0; k < 3; k++) s->revL[k] += (tg[k] - s->revL[k]) * k2;
      memcpy(cam, s->revE, 12); memcpy(look, s->revL, 12);
    }
  }
  if (haveCol && !chaseWallOff) {
    // Правка Роба 40 (28.09: «камера не должна входить в стены и текстуры»): лучи от точки взгляда к глазу и к краям
    // ближней плоскости по коллизии мира (colRayFrac). Упёрлась — камера подъезжает по лучу до стены минус зазор; ближе
    // CAM_WALL_MIN не подъезжает — поднимается над машиной (до +2.4 м). Стена появилась — сразу ближе, пропала — отпускает
    // плавно (4 м/с). Правило порта: в FollowCameraData только поворот, угол, дистанция и смещение цели; сверить с PS2.
    // №602/TEST19 (видео 2:46, J9-5): точка взгляда стоит на tz = 3 м впереди машины; у стены дома она внутри стены, луч от неё к глазу упирался сразу
    // (доля 0), и глаз подъезжал к ней — через машину на её нос, взгляд в стену, машина за нижним краем. Луч идёт от опоры (над машиной), как у
    // оригинала (машина — центр камеры). CAM_WALL_ORG_OLD=1 / flag camwallorgold 1 — прежнее (от точки взгляда)
    float T[3] = {look[0], look[1], look[2]};
    if (!camWallOrgOld && c->wheels > 0) { float A[3] = {ax, look[1], az}; if (colRayFrac(&world, look, A, 0) < 1.f && colRayFrac(&world, look, cam, 0) < .15f) { T[0] = ax; T[2] = az; } }   // только если точка взгляда за стеной относительно опоры (крыша/край: прежний луч строже)
    float want = sqrtf((cam[0] - T[0]) * (cam[0] - T[0]) + (cam[2] - T[2]) * (cam[2] - T[2]));
    float fx = want > 1e-3f ? (cam[0] - T[0]) / want : 0.f, fz = want > 1e-3f ? (cam[2] - T[2]) / want : 1.f, got = 0.f, rise = 0.f;
    s->hits = 0; int centerHit = 0;
    for (int k = 0; k < 4; k++) {
      float up = k * .8f, E[3] = {cam[0], cam[1] + up, cam[2]}, fr = 1.f;
      float L = sqrtf((E[0] - T[0]) * (E[0] - T[0]) + (E[1] - T[1]) * (E[1] - T[1]) + (E[2] - T[2]) * (E[2] - T[2]));
      float cfr_[5], Ar_[5][3], Br_[5][3];   // CAMPERF (23.10): 5 лучей стены к коллизии одним обходом клеток (colRaysFrac), ответ каждого тот же; CAM_RAYS_OLD=1 / flag camraysold 1 — по одному
      if (!camRaysOld) {
        for (int r = 0; r < 5; r++) { float sx = r == 1 ? .4f : r == 2 ? -.4f : 0.f, sy = r == 3 ? .25f : r == 4 ? -.25f : 0.f;
          memcpy(Ar_[r], T, 12); Br_[r][0] = E[0] - fz * sx; Br_[r][1] = E[1] + sy; Br_[r][2] = E[2] + fx * sx; }
        colRaysFrac(&world, 5, (const float (*)[3])Ar_, (const float (*)[3])Br_, cfr_);
        if (camRaysVerify) for (int r = 0; r < 5; r++) if (fabsf(colRayFrac(&world, Ar_[r], Br_[r], 0) - cfr_[r]) > 1e-6f) csRaysMismatch++;
      }
      for (int r = 0; r < 5; r++) {                             // центр и 4 края: вбок ±0.4 м, вверх/вниз ±0.25 м
        float sx = r == 1 ? .4f : r == 2 ? -.4f : 0.f, sy = r == 3 ? .25f : r == 4 ? -.25f : 0.f;
        float Er[3] = {E[0] - fz * sx, E[1] + sy, E[2] + fx * sx}, fWorld = camRaysOld ? colRayFrac(&world, T, Er, 0) : cfr_[r], f = fminf(fWorld, camCarsFrac(T, Er));   // 284: и машины
        // №1127: «закрыто» для подъёма на 6 м и среза на идеальное место — только по статике мира (в оригинале линия машина → камера проверяется по статике; проезжающая машина её не рвёт).
        // Машины по-прежнему подтягивают глаз ниже (f), но стоящие, не движущиеся. CAMCARS1127_OLD=1 — как в TEST84 (и машины)
        float fSee = CAMCARS1127_OLD ? f : fWorld;
        if (k == 0 && fSee < 1.f) s->hits++;
        if (k == 0 && r == 0 && fSee < 1.f) centerHit = 1;
        if (f < fr) fr = f;
      }
      float d = fr >= 1.f ? want : fmaxf(0.f, fr * L - CAM_WALL_MARGIN) / fmaxf(L, 1e-3f) * want;
      if (d >= CAM_WALL_MIN || fr >= 1.f) { got = d; rise = up; break; }
      if (d > got) { got = d; rise = up; }
    }
    // сверка g06 (g06-camera:5): загорожено ближе, чем дальность минус 2 м — камера поднимается на 6 м над машиной (у оригинала при этом линия от машины к камере закрыта);
    // у нас луч идёт от точки взгляда и четыре края, остальное правило порта (зазор 0,4, отпускание 4 м/с) не тронуто. CAM_OCCL_SV_OLD=1 — подъём только до 2,4 м
    if (!CAM_OCCL_SV_OLD && s->hits > 0 && want - got > 2.f) { float r6 = ay + 6.f - cam[1]; if (r6 > rise) rise = r6; }
    // круг 6 (СВЕРКА_КАМЕРА 1.15): линия от опоры к глазу закрыта (не при взгляде назад) — оригинал сразу, срезом, ставит камеру на идеальное место за машиной (без запаздываний), и только
    // если и оттуда закрыто, прижимает к точке касания; ближе «дальность − 2 м» — над целью на 6 м. Здесь: фильтры начинаются заново на идеальном месте, второй проход с dt 0.
    // CAM_OCCLCUT_SV_OLD=1 — как в круге 5 (только подтягивание вдоль луча)
    if (!CAM_OCCLCUT_SV_OLD && centerHit && !chaseRetry && !s->fz && !s->rev && chaseView != 2 && dt > 0.f) {
      chaseRetry = 1; s->yaw = ty; s->yawV = 0.f; s->svInit = 0; s->wallD = 0.f; s->rise = 0.f; memset(s->fLk, 0, sizeof s->fLk); memset(s->fLkV, 0, sizeof s->fLkV);
      chaseCamUpdate(c, yawOfs, 0.f, cam, look);
      chaseRetry = 0; return;
    }
    if (!(s->wallD > 0.f) || got < s->wallD) s->wallD = got; else s->wallD = fminf(got, s->wallD + 4.f * dt);
    if (chaseRetry) s->rise = rise; else s->rise += (rise - s->rise) * fminf(1.f, dt * (rise > s->rise ? 8.f : 2.f));
    float sk = want > 1e-3f ? fminf(1.f, s->wallD / want) : 1.f, E[3] = {cam[0], cam[1] + s->rise, cam[2]};
    for (int i = 0; i < 3; i++) cam[i] = T[i] + (E[i] - T[i]) * sk;
    chaseNear = sk < .999f ? fmaxf(.3f, fminf(NEAR_Z, .8f * CAM_WALL_MARGIN)) : NEAR_Z;
    chaseInfo.wall = sk;
  }
  if (!camGroundOld) { float g2;   // правка 284: жёсткий пол — после приземления с трамплина глаз не уходит под землю
    if (ground(cam[0], cam[2], fmaxf(cam[1], look[1]) + .3f, 0.f, &g2) && cam[1] < g2 + CAM_GROUND_MIN) cam[1] = g2 + CAM_GROUND_MIN; }
  if (chaseView == 2 && !selectCycleOld && !carBoomed(c)) {   // №617: бамперная камера PS2 (строка Camera: Far → Bumper → Near; запись Стенда, ps2-camera-views-cycle): глаз 0,60 м вперёд и 0,86 м выше центра машины, по оси, взгляд горизонтально по носу, машины в кадре нет
    float m[16], lift = carModelLift(c, cv.lift > 0.f ? cv.lift : 0.f); carPieceMatrix(c, lift, 0, 0, 0, 1, m);
    float fx = -m[8], fy = -m[9], fz = -m[10], ux = m[4], uy = m[5], uz = m[6];   // вперёд машины = −Z модели
    // у PS2 в кадре машины нет: глаз вынесен на нос (у порта центр машины и «car_cm» оригинала не совпадают, 0,6 м вперёд от нашего центра остаётся внутри кабины, кадр в крыше/капоте):
    // впереди носа на CAM_BUMP_FWD − 0,6 м, высота — над бампером на CAM_BUMP_UP. Числа PS2 как пределы: глаз по оси, взгляд горизонтально
    float fw = c->p.bodyOfs + c->p.halfL - .1f + (CAM_BUMP_FWD - .6f), up = c->p.bodyLo + c->modelRaise + CAM_BUMP_UP;
    if (CAM_BUMPER_SV_OLD) {
    cam[0] = c->x + fx * fw; cam[1] = c->y + fy * fw + up; cam[2] = c->z + fz * fw; (void)ux; (void)uy; (void)uz;
    look[0] = cam[0] + fx * 10.f; look[1] = cam[1] + fy * 10.f; look[2] = cam[2] + fz * 10.f; chaseInfo.wall = 1.f;
    } else {
    // сверка g06 (g06-camera:18, 29): глаз над кузовом на высоте верха коробки + 0,3 м, вперёд на половину базы минус 1,0 м (ближняя плоскость); цель на 2 м вперёд на той же высоте;
    // положение и взгляд — в осях кузова (с тангажем и креном), вертикаль кадра — вертикаль кузова (camUpV, main_loop.h берёт её для вида), без запаздывания. CAM_BUMPER_SV_OLD=1 — как было
    float fwB = CAM_BUMPFWD_SV_OLD ? c->p.bodyOfs + c->p.halfL - 1.0f : camBumperFwd(c->p.wheelF), upB = c->p.bodyHi + c->modelRaise + .3f, ul = sqrtf(ux * ux + uy * uy + uz * uz);
    if (ul > 1e-4f) { ux /= ul; uy /= ul; uz /= ul; } else { ux = 0.f; uy = 1.f; uz = 0.f; }
    cam[0] = c->x + fx * fwB + ux * upB; cam[1] = c->y + fy * fwB + uy * upB; cam[2] = c->z + fz * fwB + uz * upB;
    look[0] = cam[0] + fx * 2.f; look[1] = cam[1] + fy * 2.f; look[2] = cam[2] + fz * 2.f; chaseInfo.wall = 1.f;
    camUpV[0] = ux; camUpV[1] = uy; camUpV[2] = uz; camUpOn = 1;
    }
    if (!bumpGroundOld && haveCol) {   // №791: глаз бампера и низ кадра не в асфальте
      unsigned bt0_ = smkOn ? sceKernelGetSystemTimeLow() : 0;
      float tv = tanf(lensCar() * (GU_PI / 360.f)), th = tv * ASPECT, f[3] = {fx, fy, fz}, r[3], u[3], need = 0.f, top = fmaxf(cam[1], c->y + c->p.bodyHi + c->modelRaise) + .5f;
      float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (fl > 1e-4f) for (int k = 0; k < 3; k++) f[k] /= fl;
      csBasis(f, r, u);
      for (int i = -1; i < 3; i++) {   // глаз; левый, средний, правый низ ближней плоскости на BUMP_NEAR
        float p[3], g, cl = i < 0 ? BUMP_EYE_CLEAR : BUMP_EDGE_CLEAR;
        if (i < 0) { p[0] = cam[0]; p[1] = cam[1]; p[2] = cam[2]; }
        else { float sx = (float)(i - 1) * th; for (int k = 0; k < 3; k++) p[k] = cam[k] + BUMP_NEAR * (f[k] + sx * r[k] - tv * u[k]); }
        if (colGround(&world, p[0], p[2], top, p[1] - 3.f, &g, 0) && p[1] < g + cl) need = fmaxf(need, g + cl - p[1]);
      }
      if (need > BUMP_RAISE_MAX) need = BUMP_RAISE_MAX;
      if (need > s->bumpLift) s->bumpLift = need; else s->bumpLift += (need - s->bumpLift) * (1.f - expf(-dt * 3.f));
      cam[1] += s->bumpLift; look[1] += s->bumpLift;
      if (smkOn) { camBumpUs += sceKernelGetSystemTimeLow() - bt0_; camBumpN++; }   // замер цены: CAMV bump_us
    }
  } else s->bumpLift = 0.f;
  if (!CAM_SHAKE_SV_OLD && chaseView != 2) {   // сверка g06 (g06-camera:28)
    if (svBoom.seq != camBoomSeq) { camBoomSeq = svBoom.seq; float R = svBoom.radius > 1.f ? svBoom.radius : 20.f; if (hypotf(svBoom.x - c->x, svBoom.z - c->z) < R) camShakeFire(c->x - svBoom.x, c->z - svBoom.z); }
    if (c->hitSpeed >= CAM_SHAKE_V && camShakeT >= CAM_SHAKE_LEN) {
      if (c->wallV > 0.f) camShakeFire(c->wallN[0], c->wallN[1]);
      else if (c->carHitV > 0.f) camShakeFire(c->x - c->carHitP[0], c->z - c->carHitP[2]);
      else camShakeFire(-c->vel[0], -c->vel[2]); }
    if (camShakeT < CAM_SHAKE_LEN) {
      float a = CAM_SHAKEPH_SV_OLD ? CAM_SHAKE_AMP * (1.f - camShakeT / CAM_SHAKE_LEN) * sinf(CAM_SHAKE_W * camShakeT) : camShakeAt(camShakeT);   // круг 6: фаза оригинала 0,87 + шаг
      cam[0] += camShakeDx * a; cam[2] += camShakeDz * a; camShakeT += dt; }
  }
  if (!CAMSET1151_OLD && chaseView != 2 && camUserTiltNow != 0.f) camUserTilt(cam, look, camUserTiltNow);   // №1151: наклон взгляда из CAMERA (положение глаза не меняется — закон объектива цел)
}
// ---- сверка g06: камера пешком, добавки к позе homerCamera (homer.c — окно g04, не правится): склон, автозум, крен уровня 7 (g06-camera:8,17,33,34) ----
// walkCamSv вызывается из main_loop.h сразу после homerCamera. hp — ноги Гомера, cp/lk — поза камеры (правится), spd — скорость Гомера, м/с. Возврат: множитель tan(FOV/2) кадра
// (1 — без изменений); вертикаль кадра при крене — camUpV/camUpOn.
//  • склон (33): на склоне круче ~14° (нормаль земли под ногами y < 0,97) камера поднимается на 0,3 рад (угол над героем меньше), плавно; второе условие (0,2 рад, верхний луч свободен,
//    нижний закрыт) в отчётах без описания лучей — не сделано;
//  • автозум (34): не-прохожий персонаж миссии ближе 5 м и виден (проверка раз в 1,5 с) — кадр сужается пропорционально близости до 28° по горизонтали (0,49 рад). Единственный известный
//    нам такой персонаж — собеседник цели mvTalkNpcAt (список NPC миссии в этих файлах недоступен), поэтому частично;
//  • уровень 7 (17): тот же персонаж ближе 5 м и скорость Гомера меньше трети бега (8/3 м/с) — крен вокруг оси взгляда 0,176 рад × близость, спад 0,01 за 16 мс;
//  • взгляд вверх (8): цель вверх на 3 м, пока держат (camLookUp 0..1; кнопки PSP — решает Роб, ввод не подключён).
// CAM_WALK_SV_OLD=1 — ничего из этого
#ifndef CAM_WALK_SV_OLD
#define CAM_WALK_SV_OLD 0
#endif
static float walkFovK = 1.f;   // множитель tan(FOV/2) пешей камеры (автозум), main_loop.h применяет к curFovY
#ifndef CAM_NPCLIST_SV_OLD
#define CAM_NPCLIST_SV_OLD 0
#endif
// Круг 6 (сверка с оригиналом, СВЕРКА_КАМЕРА 2.6, 2.9, P08:20): автозум — угол пешком (по горизонтали 70°) уменьшается НА 0,49 рад × близость (до 42°), а не до 28°, с запаздыванием 0,022 за кадр;
// персонаж виден, если в пирамиде кадра с узким углом 0,567 рад (шар 1 м), стены не в счёт; склон: второе условие (−0,2 рад) — стик камеры не тронут, верхний луч свободен, нижний закрыт;
// крен — тем же кубическим шагом (0,01 за кадр) в обе стороны, скорость — вектор целиком. CAM_WALKZOOM_SV_OLD / CAM_SLOPE2_SV_OLD / CAM_TWIST_SV_OLD = 1 — как в TEST83
#ifndef CAM_WALKZOOM_SV_OLD
#define CAM_WALKZOOM_SV_OLD 0
#endif
#ifndef CAM_SLOPE2_SV_OLD
#define CAM_SLOPE2_SV_OLD 0
#endif
#ifndef CAM_TWIST_SV_OLD
#define CAM_TWIST_SV_OLD 0
#endif
static float wkSlope, wkClose, wkRollA, wkRollV, wkFov = CAMLAW_WALK_FOV_MAX, wkFovV, wkZoomT = 99.f; static int wkSeen, wkPick = -1;
static float walkCamSv(const float hp[3], float cp[3], float lk[3], float spd, float dt) {
  if (CAM_WALK_SV_OLD || dt <= 0.f) return 1.f;
  float hx = cp[0] - lk[0], hz = cp[2] - lk[2], hd = hypotf(hx, hz);
  if (hd < .05f) return 1.f;
  if (!CAM_LOOKUP_SV_OLD && camLookUp > 0.f) { lk[1] += 3.f * camLookUp; }   // сверка g06 (g06-camera:8)
  // склон под ногами
  { float g, n[3] = {0.f, 1.f, 0.f}; int steep = haveCol && colGround(&world, hp[0], hp[2], hp[1] + 1.f, hp[1] - 3.f, &g, n) && n[1] < .97f;
    int topFree = 1, botFree = 1, still = homer.camTurn == 0.f;
    if (!CAM_SLOPE2_SV_OLD && !steep && still && haveCol) {   // лучи оригинала: от глаза вверх на 0,8 м (радиус = ближняя плоскость 1,0 × 0,8) и вперёд на 1,0 к точке над головой; от пояса героя назад
      float tp[3] = {hp[0], hp[1] + 1.f, hp[2]}, f[3] = {lk[0] - cp[0], lk[1] - cp[1], lk[2] - cp[2]}, fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
      if (fl > 1e-3f) { f[0] /= fl; f[1] /= fl; f[2] /= fl;
        float ul = sqrtf(f[0] * f[0] * f[1] * f[1] + (1.f - f[1] * f[1]) * (1.f - f[1] * f[1]) + f[2] * f[2] * f[1] * f[1]), u[3] = {-f[0] * f[1], 1.f - f[1] * f[1], -f[2] * f[1]};
        if (ul > 1e-4f) { u[0] /= ul; u[1] /= ul; u[2] /= ul; }
        float A[3] = {cp[0] + u[0] * .8f + f[0], cp[1] + u[1] * .8f + f[1], cp[2] + u[2] * .8f + f[2]}, B[3] = {tp[0] + u[0] * .5f - f[0] * .5f, tp[1] + u[1] * .5f - f[1] * .5f, tp[2] + u[2] * .5f - f[2] * .5f};
        topFree = colRayFrac(&world, A, B, 0) >= 1.f;
        float Lo[3] = {tp[0], tp[1] + .5f, tp[2]}, Fr[3] = {cp[0] - f[0] * .2f - Lo[0], 0.f, cp[2] - f[2] * .2f - Lo[2]}, Fy = Lo[1] - Lo[1], mg = hypotf(cp[0] - tp[0], hypotf(cp[1] - tp[1], cp[2] - tp[2])) + .5f, fm = sqrtf(Fr[0] * Fr[0] + Fr[2] * Fr[2]);
        (void)Fy;
        if (fm > 1e-4f) { float C[3] = {Lo[0] + Fr[0] / fm * mg, Lo[1], Lo[2] + Fr[2] / fm * mg}; botFree = colRayFrac(&world, Lo, C, 0) >= 1.f; } }
    }
    float want = CAM_SLOPE2_SV_OLD ? (steep ? .3f : 0.f) : camSlopeWant(steep, still, topFree, botFree);
    wkSlope += (want - wkSlope) * (1.f - expf(-dt / .3f));
    if (wkSlope > 1e-3f) {
      float dy = cp[1] - lk[1], D = hypotf(hd, dy), e1 = atan2f(dy, hd) + wkSlope, nc[3] = {lk[0] + hx / hd * D * cosf(e1), lk[1] + D * sinf(e1), lk[2] + hz / hd * D * cosf(e1)};
      if (!haveCol || colRayFrac(&world, lk, nc, 0) >= 1.f) memcpy(cp, nc, 12);
    } }
  // персонаж миссии рядом
  // сверка g06 круг 5 (СВЕРКА_КАМЕРА 2.9, CAM_NPCLIST_SV_OLD): все персонажи миссии в 5 м из mvNpcNearList (g07b-msnview), по возрастанию расстояния, прохожих там нет; ближний ВИДИМЫЙ.
  // Круг 6: видимость — пирамида кадра (camWalkPedSeen), без луча к стене. CAM_WALKZOOM_SV_OLD=1 — полуплоскость и луч, как круг 5
  float npc[3] = {0.f, 0.f, 0.f}; int have = 0, viaList = 0;
  if (!CAM_NPCLIST_SV_OLD) { float lst[6][4]; int nl = mvNpcNearList(lst, 6, hp[0], hp[2], 5.f);
    if (nl > 0) { have = viaList = 1; npc[0] = lst[0][0]; npc[1] = lst[0][1]; npc[2] = lst[0][2];
      if (wkZoomT >= 1.5f) { float fx = lk[0] - cp[0], fz = lk[2] - cp[2]; int pick = -1;
        for (int i = 0; i < nl && pick < 0; i++) { float ch[3] = {lst[i][0], lst[i][1] + .9f * lst[i][3], lst[i][2]};
          int seen = CAM_WALKZOOM_SV_OLD ? (((lst[i][0] - cp[0]) * fx + (lst[i][2] - cp[2]) * fz) > 0.f && (!haveCol || colRayFrac(&world, cp, ch, 0) >= 1.f)) : camWalkPedSeen(cp, lk, lst[i]);
          if (seen) pick = i; }
        wkPick = pick; }
      if (wkPick >= 0 && wkPick < nl) { npc[0] = lst[wkPick][0]; npc[1] = lst[wkPick][1]; npc[2] = lst[wkPick][2]; } } }
  if (!have) have = mvTalkNpcAt(npc);
  float d = have ? hypotf(npc[0] - hp[0], npc[2] - hp[2]) : 99.f;
  wkZoomT += dt;
  if (wkZoomT >= 1.5f) { wkZoomT = 0.f; float fx = lk[0] - cp[0], fz = lk[2] - cp[2];   // видимость раз в 1,5 с
    wkSeen = have && d < 5.f && (viaList ? wkPick >= 0 : ((npc[0] - cp[0]) * fx + (npc[2] - cp[2]) * fz) > 0.f); }
  float want = wkSeen && d < 5.f ? 1.f - d / 5.f : 0.f;
  if (CAM_WALKZOOM_SV_OLD) wkClose += (want - wkClose) * (1.f - powf(.9f, dt * 60.f)); else wkClose = want;   // оригинал: близость берётся каждый кадр как есть, плавность даёт фильтр угла
  float k = 1.f;
  if (CAM_WALKZOOM_SV_OLD) { float tb = tanf(35.f * (GU_PI / 180.f)), tt = tanf(.245f); k = 1.f + wkClose * (tt / tb - 1.f); }   // было: 70° → 28,1° по горизонтали, пропорционально близости
  else { float wantF = camWalkZoomWant(wkClose), e = wkFov - wantF, ev = wkFovV; svStep(&e, &ev, CAMLAW_PED_W, CAM_SV_Z, dt); wkFov = wantF + e; wkFovV = ev; k = camWalkZoomK(wkFov); }
  // крен уровня 7
  if (CAM_TWIST_SV_OLD) { float wantR = gameLevel == 7 && wkClose > 0.f && spd < 8.f / 3.f ? .176f * wkClose : 0.f;
    wkRollA = ROLL1084_OLD ? (wkRollA > wantR ? fmaxf(wantR, wkRollA - .01f * dt * 60.f * .176f) : wantR) : camRollStepSv(wkRollA, gameLevel, wkClose, spd, dt); }
  else camTwistStep(&wkRollA, &wkRollV, camTwistWant(gameLevel, wkClose, spd), dt);
  if (wkRollA > 1e-4f || wkRollA < -1e-4f) { float fx = lk[0] - cp[0], fy = lk[1] - cp[1], fz = lk[2] - cp[2], fl = sqrtf(fx * fx + fy * fy + fz * fz); if (fl > 1e-4f) { fx /= fl; fy /= fl; fz /= fl;
      float rx = fy * 0.f - fz * 1.f, rz = fx * 1.f - fy * 0.f, ry = fz * 0.f - fx * 0.f, rl = sqrtf(rx * rx + ry * ry + rz * rz);
      if (rl > 1e-4f) { rx /= rl; ry /= rl; rz /= rl; float c_ = cosf(wkRollA), s_ = sinf(wkRollA);
        float ux = ry * fz - rz * fy, uy = rz * fx - rx * fz, uz = rx * fy - ry * fx;   // up = right × forward
        camUpV[0] = ux * c_ + rx * s_; camUpV[1] = uy * c_ + ry * s_; camUpV[2] = uz * c_ + rz * s_; camUpOn = 1; } } }
  return k;
}
// №837 (Роб 19.10, TEST36 на PSP: «снизу мигает текстура асфальта — камера проходит через асфальт; чтобы асфальт всегда и везде отображался нормально»):
// общее правило для камер города (за машиной всех видов, пешком, переходы, облёты): глаз — не ниже опоры коллизии + CF_EYE_CLEAR, низ ближней
// плоскости (левый, средний, правый край на её дистанции) — не ниже опоры + CF_EDGE_CLEAR (видимый асфальт бывает выше коллизии на 0,3–0,37 м —
// замер CAMVF у трамплина L1, №791). Не хватает — глаз и взгляд поднимаются вместе (направление взгляда то же): вверх сразу, вниз плавно.
// Комнаты — без правила (там своя камера). Цена — 4 запроса к опоре коллизии за кадр. CAM_FLOOR837_OLD=1 — как TEST36
#ifndef CAM_FLOOR837_OLD
#define CAM_FLOOR837_OLD 0
#endif
#define CF_EYE_CLEAR .35f
#define CF_EDGE_CLEAR .40f
#define CF_RAISE_MAX 1.5f
// №898 (Роб 23.10, L1 L5-5: «у мостов с перекладинами камера постоянно дёргается»): низ ближней плоскости ищет опору в 2 м ВЫШЕ глаза (top = глаз + 2) — под
// фермой/мостом поверх дороги верх балки (коробка коллизии, 2,7 м над точкой) считался полом под глазом, и камера прыгала вверх сразу на CF_RAISE_MAX (1,5 м), на выезде
// назад. Полом считается опора не выше точки + CF_OVER (видимый асфальт выше коллизии на 0,3–0,37, №791; запас 0,25 м). Настоящая земля под глазом, провалившимся
// глубже, ловится правилом пола за машиной (CAM_GROUND_MIN, правка 284) и комнатами. CAM_FLOOR_OVER_OLD=1 / flag camflooroverold 1 — как было (красный контроль)
#ifndef CAM_FLOOR_OVER_OLD
#define CAM_FLOOR_OVER_OLD 0
#endif
#define CF_OVER .6f
static int camFloorOverOld = CAM_FLOOR_OVER_OLD;
static float cfLift; static unsigned cfLiftN;   // cfLiftN — кадров с подъёмом (служебно, строка CAMFLOOR)
static void camFloorGuard(const ColWorld *w, float *cp, float *lk, float nearD, float fovY, float dt) {
  float f[3] = {lk[0] - cp[0], lk[1] - cp[1], lk[2] - cp[2]}, fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); if (fl < 1e-4f || !w) return;
  for (int k = 0; k < 3; k++) f[k] /= fl;
  float r[3], u[3]; csBasis(f, r, u);
  float tv = tanf(fovY * (GU_PI / 360.f)), th = tv * ASPECT, need = 0.f, top = cp[1] + 2.f;
  for (int i = -1; i < 3; i++) {   // глаз; левый, средний, правый низ ближней плоскости
    float p[3], g, cl = i < 0 ? CF_EYE_CLEAR : CF_EDGE_CLEAR;
    if (i < 0) { p[0] = cp[0]; p[1] = cp[1]; p[2] = cp[2]; }
    else { float sx = (float)(i - 1) * th; for (int k = 0; k < 3; k++) p[k] = cp[k] + nearD * (f[k] + sx * r[k] - tv * u[k]); }
    float tp = camFloorOverOld ? top : fminf(top, p[1] + CF_OVER);   // №898: пол — только то, что выше точки не больше CF_OVER; перекрытие, ферма, перекладина над глазом — не пол
    if (colGround(w, p[0], p[2], tp, p[1] - 3.f, &g, 0) && p[1] < g + cl) need = fmaxf(need, g + cl - p[1]);
  }
  if (need > CF_RAISE_MAX) need = CF_RAISE_MAX;
  if (need > cfLift) cfLift = need; else cfLift += (need - cfLift) * (1.f - expf(-dt * 3.f));
  if (cfLift > .005f) { cp[1] += cfLift; lk[1] += cfLift; cfLiftN++; }
}
static void (*skyPump)(void);   // правка 508: кадр газеты изнутри сортировки и обхода вершин неба (на экране загрузки стояли без кадров, 124 мс в эмуляторе); NULL — нет
static unsigned vcmpN;
static int vertCmp(const void *a, const void *b) { if (skyPump && !(++vcmpN & 4095)) skyPump(); return memcmp(a, b, sizeof(Vert)); }
static void phSetColor(unsigned rgb);   // правка 411: тексель заглушки = тон горизонта (ниже, у bindTexLean)
// Правка Роба 411 (повтор 372, 09.10): дальние дома в дымке — сплошной голубоватый/светлый силуэт, «палят, что текстур нет».
// Текстуры города резидентны (tex[] из level.shr при загрузке), клетка без памяти не рисуется вовсе — заглушки «без текстуры» у
// города нет; силуэт — это дымка: за 57 % дальности кусок почти целиком цвета тумана fogCol, а он (vertex-выборка неба по полосе
// 0..12° + приглушение FOG_LUM/FOG_SAT) на L2/L4/L5 светлее и голубее неба, что реально стоит за домами (замер tests/fartint).
// Теперь цвет тумана = цвет неба, каким его кладёт GE в полосе горизонта: лучи из глаза (центр неба) по HZ_YAW курсам и HZ_EL
// высотам в полосе 0..6° (та же полоса FOG_LOW, где холмы/дымка за дальними домами), по каждому лучу — слои неба в порядке
// рисования (drawSky): тексель × цвет вершины, альфа-тест 0x40, смешивание по альфе; среднее по лучам. Ни одного подобранного
// числа цвета: всё с диска. Прежнее — «flag fogtoneold 1» вместе с «flag fogzold 1» или сборка -DFOG_HORIZON_OLD=1
#ifndef FOG_HORIZON_OLD
#define FOG_HORIZON_OLD 0
#endif
static int fogHorizonOld = FOG_HORIZON_OLD;
#define HZ_YAW 32   // правило порта: шаг курса 11,25° (сетка лучей; на результат влияет < 1 уровня из 255, см. отчёт)
#define HZ_EL 6     // высоты 0,5..5,5° — середины шагов 1° полосы 0..6°
static void skyTexel(const Tex *t, float u, float v, float *c) {   // тексель уровня 0 (ближайший), как выборка skyFogColor
  int tx = (int)floorf(u / 256.f * t->w) % t->w, ty = (int)floorf(v / 256.f * t->h) % t->h;
  if (tx < 0) tx += t->w; if (ty < 0) ty += t->h;
  int bx = t->t4 ? tx >> 1 : tx, rb = texRowBytes(t->t4, t->pre, t->w);
  unsigned o = t->pre ? ((ty >> 3) * (rb >> 4) + (bx >> 4)) * 128 + (ty & 7) * 16 + (bx & 15) : ty * rb + bx;
  unsigned ix = ((unsigned char *)t->lv[0])[o]; if (t->t4) ix = (tx & 1) ? ix >> 4 : ix & 15;
  unsigned q = ((unsigned *)t->pal)[ix];
  c[0] *= (q & 255) / 255.f; c[1] *= (q >> 8 & 255) / 255.f; c[2] *= (q >> 16 & 255) / 255.f; c[3] *= (q >> 24) / 255.f;
}
// №437: sample authored texture × vertex colour from real nearby upward triangles.
// Runs once after ground cells load, then stays fixed for the level (no colour pumping).
static void groundCoverColour(void) {
  if(GROUND_COVER_OLD||groundCover.ready||!haveCol)return;
  float gy; if(!colGround(&world,car.x,car.z,car.y+1.f,car.y-30.f,&gy,NULL))return;
  float sum[3]={0},weight=0;
  for(unsigned j=0;j<npc;j++) {
    Piece*d=&pc[j];if(d->r.owner!=0||d->r.cell>=ncell||cl[d->r.cell].state!=2||!d->v||(d->r.flags&3)||d->r.tex<0||d->r.tex>=(int)ntex)continue;
    Tex*t=&tex[d->r.tex];if(!t->pal||!t->lv[0])continue;
    if(fabsf(d->cx-car.x)>60.f+d->rad||fabsf(d->cz-car.z)>60.f+d->rad)continue;
    int count=d->r.ni/3,nvl=d->r.nv;const char*vb_=(const char*)d->v;
    if(d->r.flags&F_STRIP){unsigned nl_;vb_=(const char*)pieceListT(d,2,&nl_);if(!vb_)continue;count=(int)(nl_/3);nvl=(int)nl_;}   // К4: полосы -> список (слот 2)
    int step=count/8+1;
    for(int q=0;q<count;q+=step){float p[3][3],c[3][4];
      for(int a=0;a<3;a++){
        int k=(d->r.flags&F_NOIDX)?q*3+a:d->idx?d->idx[q*3+a]:-1;
        if(k<0||k>=nvl)goto skip;
        short*s=(short*)((char*)vb_+k*((d->r.flags&F_V12)?12:16));
        unsigned rgb;int off;
        if(d->r.flags&F_V12){unsigned v=(unsigned short)s[2];rgb=((v&31)*255/31)|(((v>>5&63)*255/63)<<8)|(((v>>11&31)*255/31)<<16)|0xff000000u;off=3;}
        else{rgb=((Vert*)s)->c;off=4;}
        p[a][0]=s[off]*d->r.sc/32768.f+d->r.ox;p[a][1]=s[off+1]*d->r.sc/32768.f+d->r.oy;p[a][2]=s[off+2]*d->r.sc/32768.f+d->r.oz;
        for(int ch=0;ch<4;ch++)c[a][ch]=(rgb>>(ch*8)&255)/255.f;
        skyTexel(t,s[0],s[1],c[a]);
      }
      {float ux=p[1][0]-p[0][0],uy=p[1][1]-p[0][1],uz=p[1][2]-p[0][2];
       float vx=p[2][0]-p[0][0],vy=p[2][1]-p[0][1],vz=p[2][2]-p[0][2];
       float nx=uy*vz-uz*vy,ny=uz*vx-ux*vz,nz=ux*vy-uy*vx,area=sqrtf(nx*nx+ny*ny+nz*nz);
       float y=(p[0][1]+p[1][1]+p[2][1])/3.f;
       if(area<.01f||fabsf(ny)<area*.9f||fabsf(y-gy)>2.f)goto skip;
       float w=area*(c[0][3]+c[1][3]+c[2][3])/3.f;
       for(int ch=0;ch<3;ch++)sum[ch]+=w*(c[0][ch]+c[1][ch]+c[2][ch])/3.f;weight+=w;
      }
      skip:;
    }
  }
  if(weight>0){unsigned rgb=0;for(int ch=0;ch<3;ch++)rgb|=(unsigned)(fminf(255.f,fmaxf(0.f,sum[ch]/weight*255.f+.5f)))<<(ch*8);
    groundCover.rgb=rgb;groundCover.ready=1;fogK=-1.f;
  }
}
static void groundCoverDraw(float fov,float ey,float refy) {   // refy — высота земли у игрока (машина или Гомер): туман подстилки считает расстояния от неё, а не от дна мира
  if(farProbe)return;
  groundCoverColour();
  if(ey<groundCover.y)return; // camera below the world: do not paint an imaginary floor over it
  ScePspFMatrix4 view;sceGumMatrixMode(GU_VIEW);sceGumStoreMatrix(&view);sceGumMatrixMode(GU_MODEL);
  GroundCoverVertex*v=sceGuGetMemory(8*sizeof*v);
  float focal=136.f/tanf(fmaxf(1.f,fminf(179.f,fov))*(3.14159265358979f/360.f));
  int n=groundCoverBackground(&groundCover,-view.y.z,view.y.x,view.y.y,focal,v);if(n<3)return;
  sceGuDisable(GU_TEXTURE_2D);sceGuDisable(GU_BLEND);sceGuDisable(GU_ALPHA_TEST);sceGuDisable(GU_FOG);sceGuDisable(GU_CULL_FACE);
  sceGuEnable(GU_DEPTH_TEST);sceGuDepthMask(GU_FALSE);
  sceGuDrawArray(GU_TRIANGLE_FAN,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,n,0,v);
  if(fogSkyOn&&!FOG_SKY_OLD&&fogGroundOn&&!FOG_GROUND_OLD&&ey-groundCover.y>.5f){   // подстилка в тумане по ГОРИЗОНТАЛЬНОМУ расстоянию от глаза (xz): вода у острова остаётся землёй, к горизонту уходит в fogCol
    float fy_=-view.y.z,ry_=view.y.x,uy_=view.y.y,H=ey-refy,D=drawDist;   // высота глаза над землёй у игрока, не над дном мира
    if(H<1.f)H=1.f;
    unsigned gc=groundCover.rgb&0xffffff,fc=fogCol&0xffffff;
    static const float bd[]={9.f,4.f,2.4f,1.8f,1.4f,1.2f,1.05f,.95f,.85f,.75f,.65f,.55f,.4f};   // линии постоянной da на расстояниях (в долях дальности, по центру экрана): сверху вниз; между ними полоса, цвет по вершинам считается точно
    enum{NB=sizeof bd/sizeof bd[0],NC=4,MAXV=NB*NC*9};
    GroundCoverVertex*q=sceGuGetMemory(MAXV*sizeof*q);int nq=0;
    float daL[NB+1];daL[0]=0.f;for(int k=0;k<NB;k++)daL[k+1]=-H/(bd[k]*D);   // da: 0 (горизонт), затем линии; последняя — граница наложения
    for(int band=0;band<NB;band++){float hi=daL[band],lo=daL[band+1];
      for(int col=0;col<NC;col++){float x0=col*480.f/NC,x1=(col+1)*480.f/NC;
        GroundCoverVertex a[16],b[16];int na=4;
        a[0]=(GroundCoverVertex){0,x0,0,3};a[1]=(GroundCoverVertex){0,x1,0,3};a[2]=(GroundCoverVertex){0,x1,272,3};a[3]=(GroundCoverVertex){0,x0,272,3};
        for(int side=0;side<2&&na>=3;side++){float T=side?lo:hi;int nb2=0;
          for(int k=0;k<na;k++){const GroundCoverVertex*p0=&a[k],*p1=&a[(k+1)%na];
            float d0=fy_+(p0->x-240)*ry_/focal-(p0->y-136)*uy_/focal,d1=fy_+(p1->x-240)*ry_/focal-(p1->y-136)*uy_/focal;
            int in0=side?d0>=T:d0<=T,in1=side?d1>=T:d1<=T;
            if(in0)b[nb2++]=*p0;
            if(in0!=in1){float t=(T-d0)/(d1-d0);b[nb2++]=(GroundCoverVertex){0,p0->x+t*(p1->x-p0->x),p0->y+t*(p1->y-p0->y),3};}
            if(nb2>=14)break;}
          for(int k=0;k<nb2;k++)a[k]=b[k];na=nb2;}
        if(na<3||nq+(na-2)*3>MAXV)continue;
        for(int k=0;k<na;k++){float u=(a[k].x-240)/focal,v=(a[k].y-136)/focal,da=fy_+u*ry_-v*uy_;
          float F=1.f;if(da<-1e-4f){float rh2=1.f+u*u+v*v-da*da,dh=H*sqrtf(rh2>0.f?rh2:0.f)/-da;F=fminf(1.f,fmaxf(0.f,(dh-.75f*D)/(.25f*D)));}
          unsigned c=0;for(int ch=0;ch<3;ch++){float vf=(float)(fc>>(8*ch)&255),vg=(float)(gc>>(8*ch)&255);c|=(unsigned)(vg+(vf-vg)*F+.5f)<<(8*ch);}
          a[k].c=c|0xff000000u;}
        for(int k=1;k+1<na;k++){q[nq++]=a[0];q[nq++]=a[k];q[nq++]=a[k+1];}}}
    if(nq>=3)sceGuDrawArray(GU_TRIANGLES,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,nq,0,q);
  } else
  if(fogSkyOn&&!FOG_SKY_OLD){   // градиент у линии горизонта: цвет горизонта неба (fogCol) -> прозрачно за fogSkyG пикселей вниз, по вершинам, где край кадра пересекает горизонт
    GroundCoverVertex h[2];int nh=0;
    for(int i=0;i<n&&nh<2;i++){   // горизонтные вершины — не углы кадра: ровно одна из координат на границе кадра
      float x=v[i].x,y=v[i].y;int cx=(x<=.01f||x>=479.99f),cy=(y<=.01f||y>=271.99f);if(cx&&cy)continue;if(!cx&&!cy)continue;
      h[nh++]=v[i];}
    if(nh==2){unsigned c0=fogCol&0xffffff;
      GroundCoverVertex*q=sceGuGetMemory(4*sizeof*q);
      q[0]=(GroundCoverVertex){c0|0xff000000u,h[0].x,h[0].y,3};q[1]=(GroundCoverVertex){c0|0xff000000u,h[1].x,h[1].y,3};
      q[2]=(GroundCoverVertex){c0,h[1].x,h[1].y+fogSkyG,3};q[3]=(GroundCoverVertex){c0,h[0].x,h[0].y+fogSkyG,3};
      sceGuEnable(GU_BLEND);sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
      sceGuDrawArray(GU_TRIANGLE_FAN,GU_COLOR_8888|GU_VERTEX_32BITF|GU_TRANSFORM_2D,4,0,q);}
  }
}
static unsigned skyHorizonUs;
#ifndef SKY_HORIZON_CACHE_OLD
#define SKY_HORIZON_CACHE_OLD 0
#endif
static int skyHorizonCacheOld = SKY_HORIZON_CACHE_OLD, skyHorizonCacheValid;
static int skyHorizonRfPair;
static unsigned skyHorizonCacheColor, skyHorizonCacheHits, skyHorizonCalculations;
static void skyHorizonInvalidate(void) { skyHorizonCacheValid = 0; }
// №954 + №957 (Роб 25.10 после TEST59: «купол фона — вместо текстуры, ради еле видных звёзд, градиент того же перехода; макушка ровная; звёзды, может, вектором»): слои неба, у которых
// текстура — вертикальный градиент (непрозрачная, строка почти одного цвета: точек, отличных от медианы строки больше чем на 16, не больше 5 %, больше чем на 40 — не больше 0,5 %),
// теряют текстуру: цвет вершины = медиана строки по V вершины × цвет вершины (как множит GE), кусок — без текстуры и смешивания. Медиана по строке не видит звёзд (редкие точки),
// облака и рисунок (доля отличных точек больше) остаются текстурой. Строки берутся из ИСХОДНОЙ палитры при загрузке уровня (до palTone, main_levels.h), потом купол и слой идут одной
// цепочкой тона (skyDomeTone), поэтому стык слоя с куполом не расходится (прежде палитра слоя и цвет вершин купола красились разными путями, на L6 — ступенька на y 18 м).
// Звёзды (редкие яркие точки текстуры, ≤ SKY954_STARS на текстуру) — точками GU_POINTS в месте точки на куске (барицентрическая развёртка по UV), один вызов, 1 пиксель;
// цвет звезды — исходный цвет точки через toneColorRGB. SKY954_OLD=1 / файл sky954old рядом с EBOOT — как в TEST59; sky954starsold — без звёзд-точек
#ifndef SKY954_OLD
#define SKY954_OLD 0
#endif
#define SKY954_STARS 96
typedef struct { unsigned short h, nd; unsigned char *row; struct { unsigned short x, y; unsigned char c[3]; } dot[SKY954_STARS]; } SkyGrad;
static int sky954Old = SKY954_OLD, sky954StarsOld;
static SkyGrad **sky954G; static unsigned sky954N;
static struct { float x, y, z; unsigned c0, c; } sky954Star[512]; static int sky954NStar; static unsigned sky954StarKey = ~0u;
static struct { int tex; unsigned char stars; } *sky954Slot;   // по месту в skyP: tex — исходная текстура-градиент (−1 нет, −2 ещё не решено), stars — звёзды этого куска сняты
static int sky954Dome; static unsigned sky954Conv, sky954Seen;   // журнал: слоёв превращено / видено текстурных слоёв неба
static void sky954Free(void) { for (unsigned i = 0; i < sky954N; i++) { if (sky954G[i]) free(sky954G[i]->row); free(sky954G[i]); } free(sky954G); sky954G = 0; sky954N = 0; free(sky954Slot); sky954Slot = 0; sky954NStar = 0; sky954StarKey = ~0u; sky954Conv = sky954Seen = 0; }
// точка текстуры уровня 0 (как skyTexel: T4/T8, swizzle), RGBA исходной палитры
static void sky954Px(const Tex *t, int x, int y, unsigned char *o) {
  int bx = t->t4 ? x >> 1 : x, rb = texRowBytes(t->t4, t->pre, t->w);
  unsigned q = t->pre ? ((y >> 3) * (rb >> 4) + (bx >> 4)) * 128 + (y & 7) * 16 + (bx & 15) : y * rb + bx;
  unsigned ix = ((unsigned char *)t->lv[0])[q]; if (t->t4) ix = (x & 1) ? ix >> 4 : ix & 15;
  memcpy(o, (const unsigned char *)t->pal + ix * 4, 4);
}
static int sky954Med(const unsigned *hist, int n) { int a = 0; for (int k = 0; k < 256; k++) { a += (int)hist[k]; if (a * 2 >= n) return k; } return 255; }
// at — номер текстуры; зовут при загрузке уровня для текстур кусков неба, палитра ещё исходная
static void sky954Build(unsigned at, const Tex *t) {
  if (sky954Old || at >= sky954N || !t->pal || !t->lv[0] || t->w < 2 || t->h < 2 || t->w > 256 || t->h > 256) return;
  int w = t->w, h = t->h; unsigned char px[4]; unsigned char *row = malloc((size_t)h * 3), *dv = malloc((size_t)w * h);
  if (!row || !dv) { free(row); free(dv); return; }
  unsigned n16 = 0, n40 = 0; int ok = 1;
  for (int y = 0; y < h && ok; y++) {
    unsigned hs[3][256]; memset(hs, 0, sizeof hs);
    for (int x = 0; x < w; x++) { sky954Px(t, x, y, px); if (px[3] < 250) { ok = 0; break; } for (int c = 0; c < 3; c++) hs[c][px[c]]++; }
    if (!ok) break;
    for (int c = 0; c < 3; c++) row[y * 3 + c] = (unsigned char)sky954Med(hs[c], w);
    for (int x = 0; x < w; x++) { sky954Px(t, x, y, px); int dm = 0; for (int c = 0; c < 3; c++) { int e = abs((int)px[c] - (int)row[y * 3 + c]); if (e > dm) dm = e; }
      dv[y * w + x] = (unsigned char)(dm > 255 ? 255 : dm); n16 += dm > 16; n40 += dm > 40; }
  }
  unsigned all = (unsigned)w * (unsigned)h;
  if (ok && n16 * 20u <= all && n40 * 200u <= all) {
    SkyGrad *g = calloc(1, sizeof *g);
    if (g) { g->h = (unsigned short)h; g->row = row; row = 0;
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {   // звезда — точка, светлее медианы строки, максимум разницы в окрестности 3×3
        int d0 = dv[y * w + x]; if (d0 < 30) continue; sky954Px(t, x, y, px);
        int lum = px[0] + px[1] + px[2], lm = g->row[y * 3] + g->row[y * 3 + 1] + g->row[y * 3 + 2]; if (lum < lm + 45) continue;
        int mx = 1; for (int j = -1; j <= 1 && mx; j++) for (int i = -1; i <= 1; i++) { int xx = x + i, yy = y + j; if (xx < 0 || yy < 0 || xx >= w || yy >= h || (!i && !j)) continue;
          int d1 = dv[yy * w + xx]; if (d1 > d0 || (d1 == d0 && (j < 0 || (j == 0 && i < 0)))) { mx = 0; break; } }
        if (!mx) continue;
        if (g->nd < SKY954_STARS) { g->dot[g->nd].x = (unsigned short)x; g->dot[g->nd].y = (unsigned short)y; memcpy(g->dot[g->nd].c, px, 3); g->nd++; } }
      sky954G[at] = g; }
  }
  free(row); free(dv);
}
// развёртка звёзд на куске: точка текстуры (x+.5, y+.5) → положение в пространстве неба (то же, что множит drawPiece: sc × вершина + начало). Куски без индексов: тройки вершин подряд.
// u, v вершин — 1/256 повтора текстуры; точка тоже на повторе k = −1..ceil(maxU)
static void sky954Dots(const Piece *d, const SkyGrad *g, int w, int h) {
  if (sky954StarsOld || !g->nd || !(d->r.flags & F_NOIDX)) return;
  float k = d->r.sc / 32768.f;
  for (int j = 0; j + 2 < d->r.nv; j += 3) {
    const Vert *V[3] = {&d->v[j], &d->v[j + 1], &d->v[j + 2]}; float U[3], Vv[3];
    for (int q = 0; q < 3; q++) { U[q] = (unsigned short)V[q]->u / 256.f; Vv[q] = (unsigned short)V[q]->v / 256.f; }
    float u0 = fminf(U[0], fminf(U[1], U[2])), u1 = fmaxf(U[0], fmaxf(U[1], U[2])), v0 = fminf(Vv[0], fminf(Vv[1], Vv[2])), v1 = fmaxf(Vv[0], fmaxf(Vv[1], Vv[2]));
    float det = (U[1] - U[0]) * (Vv[2] - Vv[0]) - (U[2] - U[0]) * (Vv[1] - Vv[0]); if (fabsf(det) < 1e-7f) continue;
    for (int s = 0; s < g->nd; s++) {
      float dv_ = (g->dot[s].y + .5f) / h; if (dv_ < v0 - 1e-4f || dv_ > v1 + 1e-4f) continue;
      for (int rep = (int)floorf(u0) - 1; rep <= (int)ceilf(u1); rep++) {
        float du = (g->dot[s].x + .5f) / w + rep; if (du < u0 - 1e-4f || du > u1 + 1e-4f) continue;
        float a = ((du - U[0]) * (Vv[2] - Vv[0]) - (U[2] - U[0]) * (dv_ - Vv[0])) / det, b = ((U[1] - U[0]) * (dv_ - Vv[0]) - (du - U[0]) * (Vv[1] - Vv[0])) / det;
        if (a < -1e-3f || b < -1e-3f || a + b > 1.001f || sky954NStar >= (int)(sizeof sky954Star / sizeof sky954Star[0])) continue;
        float x = V[0]->x + a * (V[1]->x - V[0]->x) + b * (V[2]->x - V[0]->x), y = V[0]->y + a * (V[1]->y - V[0]->y) + b * (V[2]->y - V[0]->y), z = V[0]->z + a * (V[1]->z - V[0]->z) + b * (V[2]->z - V[0]->z);
        sky954Star[sky954NStar].x = x * k + d->r.ox; sky954Star[sky954NStar].y = y * k + d->r.oy; sky954Star[sky954NStar].z = z * k + d->r.oz;
        sky954Star[sky954NStar].c0 = (unsigned)g->dot[s].c[0] | (unsigned)g->dot[s].c[1] << 8 | (unsigned)g->dot[s].c[2] << 16 | 0xff000000u; sky954Star[sky954NStar].c = sky954Star[sky954NStar].c0; sky954NStar++; break; }
    }
  }
}
// цвета вершин слоя из строки текстуры; piece — номер в pc, буфер в исходном виде (прочитан из файла). Метка pad 0x954 у вершины 0 — буфер уже превращён (перечитанный метки не имеет)
#define SKY954_MARK 0x3954
static void sky954Fill(Piece *d, const SkyGrad *g) {
  int h = g->h;
  for (int i = 0; i < d->r.nv; i++) { Vert *v = &d->v[i]; int ty = (int)floorf((unsigned short)v->v / 256.f * h) % h; if (ty < 0) ty += h;
    unsigned c = v->c, r = (c & 255) * g->row[ty * 3] / 255u, gg = (c >> 8 & 255) * g->row[ty * 3 + 1] / 255u, b = (c >> 16 & 255) * g->row[ty * 3 + 2] / 255u;
    v->c = r | gg << 8 | b << 16 | 0xff000000u; }
  d->v[0].pad = SKY954_MARK; sceKernelDcacheWritebackRange(d->v, d->r.nv * sizeof(Vert));
}
// №785 (Роб 18.10 после TEST34: «самая верхушка неба, звёзды, не окрашивается цветокоррекцией мультика — пусть цветокоррекция распространяется на небо»): купол неба —
// куски неба БЕЗ текстуры (tex −1; L1 — 4 куска по 507–537 вершин), градиент в цветах вершин, а тон менял только палитры. В мультике цвета вершин купола идут той же
// цепочкой, что запись палитры (toneColorRGB); исходные хранятся, смена настройки (toneKey) или перечитанная клетка — пересчёт. В обычном режиме купол как был.
// Зовут skyFogColor (тон сменился, уровень прочитан) и drawSky (каждый кадр — только сверка). SKY_TONE_OLD=1 — купол без тона (как TEST34)
#ifndef SKY_TONE_OLD
#define SKY_TONE_OLD 0
#endif
#define SKY_TONE_N 64
// запись — по буферу вершин (несколько кусков купола делят один буфер: L1 — 4 куска по 507–537 вершин), каждый буфер тонируется один раз
static struct { Vert *v; unsigned *orig; unsigned n, key, c0; } skyTone[SKY_TONE_N]; static const void *skyToneLevel;
static void skyDomeTone(void) {
  if (SKY_TONE_OLD) return;
  if (skyToneLevel != (const void *)pc || (!sky954Slot && nskyP > 0)) { for (int s = 0; s < SKY_TONE_N; s++) { free(skyTone[s].orig); memset(&skyTone[s], 0, sizeof skyTone[s]); } skyToneLevel = pc;   // новый уровень — новый массив кусков
    free(sky954Slot); sky954Slot = nskyP > 0 ? calloc((size_t)nskyP, sizeof *sky954Slot) : 0; for (int s = 0; sky954Slot && s < nskyP; s++) sky954Slot[s].tex = -2; sky954NStar = 0; sky954StarKey = ~0u;
    sky954Dome = 0; for (int s = 0; s < nskyP; s++) { const Piece *d = &pc[skyP[s]]; if (d->r.tex < 0 && !(d->r.flags & (1 << 24))) sky954Dome = 1; } }   // №954: есть купол без текстуры (цвета вершин, тон мультика) — слой-градиент к верху переходит в его тон
  unsigned key = toneToon() ? toneKey() : 0u; int changed = 0;
  for (int s = 0; s < nskyP; s++) {
    Piece *d = &pc[skyP[s]];
    if (cl[d->r.cell].state != 2 || !d->v || (d->r.flags & F_V12) || !d->r.nv) continue;
    if (sky954Slot && !sky954Old) {   // №954: слой-градиент → цвета вершин (строка текстуры × цвет вершины), без текстуры
      if (sky954Slot[s].tex == -2) { int t_ = d->r.tex, sh_ = 0; sky954Slot[s].tex = -1;
        if (t_ >= 0 && (unsigned)t_ < sky954N && sky954G[t_]) { sky954Seen++; for (int s2 = 0; s2 < nskyP; s2++) if (s2 != s && pc[skyP[s2]].v == d->v) sh_ = 1;   // общий буфер с другим куском — не трогаем
          if (!sh_) sky954Slot[s].tex = t_; } else if (t_ >= 0) sky954Seen++; }
      if (sky954Slot[s].tex >= 0 && d->v[0].pad != SKY954_MARK) { const SkyGrad *g_ = sky954G[sky954Slot[s].tex]; const Tex *tx_ = &tex[sky954Slot[s].tex];
        if (!sky954Slot[s].stars) { sky954Slot[s].stars = 1; sky954Dots(d, g_, tx_->w, tx_->h); }   // до смены цветов: у звёзд своя развёртка по UV
        sky954Fill(d, g_); d->r.tex = -1; d->r.flags &= ~3; sky954Conv++; changed = 1; }
    }
    if (d->r.tex >= 0) continue;
    unsigned pk = sky954Slot && sky954Slot[s].tex >= 0 ? (toneKey() | 0x80000000u | (sky954Dome ? 0x40000000u : 0u)) : key;   // превращённый слой: тон как у палитры слоя и в обычном режиме (купол — как был)
    int e = -1, fr = -1;
    for (int k = 0; k < SKY_TONE_N; k++) { if (skyTone[k].v == d->v) { e = k; break; } if (!skyTone[k].v && fr < 0) fr = k; }
    unsigned n = d->r.nv;
    for (int s2 = s + 1; s2 < nskyP; s2++) { const Piece *d2 = &pc[skyP[s2]]; if (d2->v == d->v && d2->r.nv > n) n = d2->r.nv; }   // общий буфер — по самому длинному куску
    if (e >= 0 && skyTone[e].key == pk && skyTone[e].n >= n && d->v[0].c == skyTone[e].c0) continue;   // уже в тоне (частый случай, каждый кадр)
    if (e >= 0 && (skyTone[e].n < n || d->v[0].c != skyTone[e].c0)) { free(skyTone[e].orig); memset(&skyTone[e], 0, sizeof skyTone[e]); fr = e; e = -1; }   // буфер перечитан или больше прежнего
    if (e < 0) {   // новый (или перечитанный) буфер: в памяти исходные цвета из файла
      if (fr < 0) continue;
      e = fr; skyTone[e].orig = malloc(n * 4u); if (!skyTone[e].orig) continue;
      skyTone[e].v = d->v; skyTone[e].n = n; skyTone[e].key = 0; skyTone[e].c0 = d->v[0].c;
      for (unsigned k = 0; k < n; k++) skyTone[e].orig[k] = d->v[k].c;
    }
    if (skyTone[e].key == pk) continue;
    n = skyTone[e].n;
    for (unsigned k = 0; k < n; k++) {
      unsigned c = skyTone[e].orig[k];
      if (pk) { unsigned char q[4] = {(unsigned char)c, (unsigned char)(c >> 8), (unsigned char)(c >> 16), (unsigned char)(c >> 24)};
        if (pk & 0x80000000u) {   // №954: слой-градиент: у горизонта — тон палитры слоя (как прежде, без шагов мультика), к верху слоя — тон купола (стык без ступеньки)
          unsigned char n_[4]; memcpy(n_, q, 4); toneColorRGBNT(n_); toneColorRGB(q);
          float t = (pk & 0x40000000u) ? fminf(1.f, fmaxf(0.f, (unsigned short)skyTone[e].v[k].v / 256.f)) : 0.f; t = t * t * (3.f - 2.f * t);
          for (int ch = 0; ch < 3; ch++) q[ch] = (unsigned char)(n_[ch] + ((int)q[ch] - (int)n_[ch]) * t + .5f); }
        else toneColorRGB(q);
        c = q[0] | (unsigned)q[1] << 8 | (unsigned)q[2] << 16 | (c & 0xff000000u); }
      skyTone[e].v[k].c = c;
    }
    sceKernelDcacheWritebackRange(skyTone[e].v, n * sizeof(Vert));
    skyTone[e].key = pk; skyTone[e].c0 = skyTone[e].v[0].c; changed = 1;
  }
  if (sky954NStar && sky954StarKey != key) { sky954StarKey = key;   // звёзды-точки: цвет точки той же цепочкой тона, что цвета купола
    for (int k = 0; k < sky954NStar; k++) { unsigned c = sky954Star[k].c0;
      if (key) { unsigned char q[4] = {(unsigned char)c, (unsigned char)(c >> 8), (unsigned char)(c >> 16), (unsigned char)(c >> 24)}; toneColorRGB(q); c = q[0] | (unsigned)q[1] << 8 | (unsigned)q[2] << 16 | 0xff000000u; }
      sky954Star[k].c = c; } }
  if (changed) skyHorizonInvalidate();
}
static unsigned skyHorizonColor(void) {
  if (!skyHorizonCacheOld && skyHorizonCacheValid) { skyHorizonUs = 0; skyHorizonCacheHits++; return skyHorizonCacheColor; }
  skyHorizonCalculations++;
  enum { NR = HZ_YAW * HZ_EL };
  static float dir[NR][3], out[NR][3], bt[NR], bc[NR][4]; static int hit[NR];
  unsigned t0 = sceKernelGetSystemTimeLow(); int any = 0;
  for (int i = 0; i < NR; i++) {
    float yaw = (i / HZ_EL) * (2.f * GU_PI / HZ_YAW), el = ((i % HZ_EL) + .5f) * (GU_PI / 180.f);
    dir[i][0] = cosf(el) * sinf(yaw); dir[i][1] = sinf(el); dir[i][2] = cosf(el) * cosf(yaw); out[i][0] = out[i][1] = out[i][2] = 0.f;
  }
  for (int s = 0; s < nskyP; s++) {
    Piece *d = &pc[skyP[s]]; if (cl[d->r.cell].state != 2 || !d->v || (d->r.flags & F_V12)) continue;
    const Tex *t = d->r.tex >= 0 && d->r.tex < (int)ntex ? &tex[d->r.tex] : 0; if (t && (!t->lv[0] || !t->pal)) t = 0;
    float k = d->r.sc / 32768.f; int noidx = (d->r.flags & F_NOIDX) != 0;
    for (int i = 0; i < NR; i++) { hit[i] = 0; bt[i] = 1e30f; }
    for (int j = 0; j + 2 < d->r.ni; j += 3) {
      if (skyPump && !((j / 3) & 15)) skyPump();   // правка 508: основная цена — луч × треугольник неба (NR направлений на каждый)
      const Vert *V[3]; float P[3][3];
      for (int q = 0; q < 3; q++) { V[q] = noidx ? &d->v[j + q] : &d->v[d->idx[j + q]];
        P[q][0] = V[q]->x * k + d->r.ox; P[q][1] = V[q]->y * k + d->r.oy; P[q][2] = V[q]->z * k + d->r.oz; }
      float e1[3] = {P[1][0] - P[0][0], P[1][1] - P[0][1], P[1][2] - P[0][2]}, e2[3] = {P[2][0] - P[0][0], P[2][1] - P[0][1], P[2][2] - P[0][2]};
      float tv[3] = {-P[0][0], -P[0][1], -P[0][2]}, qv[3] = {tv[1] * e1[2] - tv[2] * e1[1], tv[2] * e1[0] - tv[0] * e1[2], tv[0] * e1[1] - tv[1] * e1[0]};
      for (int i = 0; i < NR; i++) {   // Мёллер — Трумбор, луч из начала (глаз = центр неба)
        const float *D = dir[i]; float pv[3] = {D[1] * e2[2] - D[2] * e2[1], D[2] * e2[0] - D[0] * e2[2], D[0] * e2[1] - D[1] * e2[0]};
        float det = e1[0] * pv[0] + e1[1] * pv[1] + e1[2] * pv[2]; if (fabsf(det) < 1e-9f) continue;
        float inv = 1.f / det, u = (tv[0] * pv[0] + tv[1] * pv[1] + tv[2] * pv[2]) * inv; if (u < 0.f || u > 1.f) continue;
        float v = (D[0] * qv[0] + D[1] * qv[1] + D[2] * qv[2]) * inv; if (v < 0.f || u + v > 1.f) continue;
        float tt = (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2]) * inv; if (tt <= 0.f || tt >= bt[i]) continue;
        float w = 1.f - u - v, c[4];
        for (int ch = 0; ch < 4; ch++) c[ch] = (w * (V[0]->c >> (8 * ch) & 255) + u * (V[1]->c >> (8 * ch) & 255) + v * (V[2]->c >> (8 * ch) & 255)) / 255.f;
        if (t) skyTexel(t, w * V[0]->u + u * V[1]->u + v * V[2]->u, w * V[0]->v + u * V[1]->v + v * V[2]->v, c);
        bt[i] = tt; hit[i] = 1; memcpy(bc[i], c, sizeof c);
      }
    }
    for (int i = 0; i < NR; i++) { if (!hit[i]) continue; const float *c = bc[i]; float a = c[3];   // слой поверх: как setFlags неба
      if ((d->r.flags & 1) && a * 255.f <= 64.f) continue;                                      // альфа-тест GU_GREATER 0x40
      if (!(d->r.flags & 2)) a = 1.f;                                                          // без смешивания — замещает
      for (int ch = 0; ch < 3; ch++) out[i][ch] = out[i][ch] * (1.f - a) + c[ch] * a; any = 1; }
  }
  skyHorizonUs = sceKernelGetSystemTimeLow() - t0;
  if (!any) return ~0u;
  float m[3] = {0, 0, 0}; for (int i = 0; i < NR; i++) for (int ch = 0; ch < 3; ch++) m[ch] += out[i][ch] / NR;
  unsigned o = 0; for (int ch = 0; ch < 3; ch++) { int q = (int)(fminf(1.f, fmaxf(0.f, m[ch])) * 255.f + .5f); o |= (unsigned)q << (8 * ch); }
  skyHorizonCacheColor = o; skyHorizonCacheValid = 1;
  return o;
}
static void skyFogColor(void) {
  skyDomeTone();   // №785: купол в тоне до подсчёта цвета дали
  float acc[2][4] = {{0}};
  for (int s = 0; s < nskyP; s++) {
    if (skyPump) skyPump();
    Piece *d = &pc[skyP[s]]; if (cl[d->r.cell].state != 2 || !d->v) continue;
    Tex *t = d->r.tex >= 0 && d->r.tex < (int)ntex ? &tex[d->r.tex] : 0;
    float k = d->r.sc / 32768.f;
    // без индексов (флаг 16) вершины повторяются по треугольникам: берём каждую один раз, как в индексном файле
    Vert *uv_ = d->v; int nuv = d->r.nv;
    if ((d->r.flags & F_NOIDX) && !(d->r.flags & F_V12) && (uv_ = malloc(nuv * sizeof(Vert)))) {
      memcpy(uv_, d->v, nuv * sizeof(Vert)); qsort(uv_, nuv, sizeof(Vert), vertCmp);
      int m = 0; for (int i = 0; i < nuv; i++) if (!m || memcmp(&uv_[i], &uv_[m - 1], sizeof(Vert))) uv_[m++] = uv_[i];
      nuv = m;
    } else uv_ = d->v;
    for (int i = 0; i < nuv; i++) {
      if (skyPump && !(i & 2047)) skyPump();
      Vert *v = &uv_[i];
      float px = v->x * k + d->r.ox, py = v->y * k + d->r.oy, pz = v->z * k + d->r.oz, hz = sqrtf(px * px + pz * pz);
      if (hz < 1e-3f) continue;
      float el = atanf(py / hz) * (180.f / GU_PI); int band = el >= 0.f && el < 6.f ? 0 : el >= 6.f && el < 12.f ? 1 : -1;
      if (band < 0) continue;
      float r = (v->c & 255) / 255.f, g = (v->c >> 8 & 255) / 255.f, b = (v->c >> 16 & 255) / 255.f, a = (v->c >> 24) / 255.f;
      if (t && t->lv[0]) {
        int tx = (int)floorf(v->u / 256.f * t->w) % t->w, ty = (int)floorf(v->v / 256.f * t->h) % t->h;
        if (tx < 0) tx += t->w; if (ty < 0) ty += t->h;
        int bx = t->t4 ? tx >> 1 : tx, rb = texRowBytes(t->t4, t->pre, t->w);   // T4 (правка 129): байт = 2 тексела, младший полубайт — левый
        unsigned o = t->pre ? ((ty >> 3) * (rb >> 4) + (bx >> 4)) * 128 + (ty & 7) * 16 + (bx & 15) : ty * rb + bx;   // T4/T8 pitch, including linear textures
        unsigned ix = ((unsigned char *)t->lv[0])[o]; if (t->t4) ix = (tx & 1) ? ix >> 4 : ix & 15;
        unsigned c = ((unsigned *)t->pal)[ix];
        r *= (c & 255) / 255.f; g *= (c >> 8 & 255) / 255.f; b *= (c >> 16 & 255) / 255.f; a *= (c >> 24) / 255.f;
      }
      if (a < .25f) continue;
      acc[band][0] += r * a; acc[band][1] += g * a; acc[band][2] += b * a; acc[band][3] += a;
    }
    if (uv_ != d->v) free(uv_);
  }
  if (acc[0][3] <= 0.f || acc[1][3] <= 0.f) return;   // неба нет — оставляем старый цвет
  unsigned out = 0;
  for (int ch = 0; ch < 3; ch++) {
    float f = FOG_LOW_W * acc[0][ch] / acc[0][3] + (1.f - FOG_LOW_W) * acc[1][ch] / acc[1][3];
    int q = (int)(f * 255.f + .5f); if (q > 255) q = 255;
    out |= (unsigned)q << (8 * ch);
  }
  // Роб 27.09: дымка/фон не «светлый фосфорный голубой», а нейтральный серый, как даль дороги:
  // берём яркость горизонта, приглушаем (FOG_LUM) и оставляем от оттенка неба только FOG_SAT
  float r = (out & 255) / 255.f, g = (out >> 8 & 255) / 255.f, b = (out >> 16 & 255) / 255.f;
  fogSkyCol=out;
  fogCol = fogTone(r,g,b,FOG_LUM,FOG_SAT,fogToneOld);
#if !FOG_HORIZON_OLD
  if (!fogHorizonOld) { unsigned h = skyHorizonColor(); if (h != ~0u) fogCol = h; }
#endif
  phSetColor(fogCol);
  skinToonAir(fogCol);   // №779: контур мультика вдали светлеет к цвету дали
}
