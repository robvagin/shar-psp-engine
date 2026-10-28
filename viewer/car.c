#ifndef CAR_SPRING_SCALE
#define CAR_SPRING_SCALE 1.f   // №525/368 профиль трамплина: множитель пружины колеса (подбор по PS2 jz16/jz17)
#endif
#ifndef CAR_DAMP_SCALE
#define CAR_DAMP_SCALE 1.f     // множитель демпфера
#endif
#ifndef CAR_COAST_DRAG_K
#define CAR_COAST_DRAG_K 1.f   // опыт №388: сопротивление на накате (газ 0); 1 — прежнее
#endif
#ifndef CAR_COAST_LIN
#define CAR_COAST_LIN 1   // 0 — прежнее (сопротивление по TopSpeed и без газа); 1 — накат по PS2: A + C·v² (№388)
#endif
#ifndef CAR_COAST_A
#define CAR_COAST_A 2.2f   // м/с²: постоянная часть наката (качение + трение трансмиссии), PS2 famil_v
#endif
#ifndef CAR_COAST_C
#define CAR_COAST_C 0.0021f   // 1/м: квадратичная часть (PS2 famil_v), у прежнего порта 2·gas/Vmax² = 0,0081
#endif
float carRollGain = 0.f;   // добавка к плечу крена боковой силы шины, м (файл rollps2); 0 — как было
#ifndef CAR_CURB_TOL_OLD
#define CAR_CURB_TOL_OLD 0
#endif
#ifndef CAR_CURB_TOL
#define CAR_CURB_TOL 0.08f
#endif
#ifndef SEAM_WALL_OLD
#define SEAM_WALL_OLD 0
#endif
// Разбивка №581/582/584 для бисекции (15.10, Sonnet-Стены): каждая часть по умолчанию = SEAM_WALL_OLD (поведение прежнее); -DSEAM_x_OLD=1 откатывает только её
#ifndef SEAM_WHEEL_SKIP_OLD
#define SEAM_WHEEL_SKIP_OLD SEAM_WALL_OLD   // колесо: проходимые грани (n.y > COL_WALK_NY) не стена
#endif
#ifndef SEAM_WHEEL_TOP_OLD
#define SEAM_WHEEL_TOP_OLD SEAM_WALL_OLD    // верх граней не выше точки контакта + 0.35
#endif
// №485 (Obliteratatron с места вставал до 63° на задние колёса; PS2 по записи ps2-oblit-launch.yaml: пик 5,5–6,1° в первые 8 кадров, потом 2–4°, на дыбы не встаёт): .con даёт WheelieOffsetZ −1,0 м, и при полном газе
// ниже WheelieRange·TopSpeed центр масс уходит назад на метр, тяжёлая машина (масса 4000) раскручивается и не возвращается. Порт: смещение не больше WHEELIE_Z_CAP (0,6 м): oblit_v 63,5° → 5,6°, остальные машины ≤ 8,6°
// (тест tests/cars/wheelie.c). WHEELIE_CAP_OLD=1 — как было
#ifndef WHEELIE_CAP_OLD
#define WHEELIE_CAP_OLD 0
#endif
#define WHEELIE_Z_CAP 0.6f
#ifndef SEAM_WHEEL_CN_SMALLBOX_OLD
#define SEAM_WHEEL_CN_SMALLBOX_OLD 0
#endif
#define SEAM_WHEEL_CN_SMALLBOX 2.f
#ifndef SEAM_WHEEL_CN_OLD
#define SEAM_WHEEL_CN_OLD SEAM_WALL_OLD     // срез встречной компоненты нормали у бокового контакта колеса (бордюр, угол). Известный остаток: у стены «на стене» L2/L5 6 против 3 прежнего (tests/pockets F), разделить высотой, углом и двусторонним ударом не вышло, seam_wall #22 краснеет
#endif
#ifndef SEAM_HULL_OLD
#define SEAM_HULL_OLD SEAM_WALL_OLD    // №582/581: нормаль днища на проходимом склоне без встречной компоненты
#endif
#ifndef FENCE_PUSH_CAP_OLD
#define FENCE_PUSH_CAP_OLD 0
#endif
#ifndef FENCE_PUSH_SLACK
#define FENCE_PUSH_SLACK 0.06f
#endif
#ifndef SEAM_HULL_NYMAX
#define SEAM_HULL_NYMAX 0.95f   // срез нормали днища только на наклонной земле (n.y < порога; склон полянки J9-8 ниже 0.9); ровная опора (дорога, труба, ящик) держит карман. 1.01 = как у Gemini (на любой проходимой)
#endif
#ifndef RIDGE_ALL_TYPES_OLD
#define RIDGE_ALL_TYPES_OLD 0   // 1 — №584 как у Gemini: срез нормали у боков на любом примитиве (ломал таран в стену двора K13-2, tests/pockets E); 0 — только капсулы-колонны/выступы (Cyl 62, 64, 74)
#endif
// Машина — твёрдое тело (v10), см. car.h. Подшаг p.sub (1/120 с):
//   1) силы: вес (+ момент «неваляшки»: вес приложен на WeebleOffset ниже центра масс — качнувшаяся машина сама
//      встаёт на колёса), 4 колеса-луча colGround (пружина k*(x + 5x^2) + амортизатор + упор хода), шины (вбок —
//      жёсткость NormalSteering/SlipSteering по углу увода с пределом TireGrip * нагрузка; вдоль — газ задними,
//      тормоз, накат, ручник; круг трения — газ в повороте срывает зад в занос), сопротивление по TopSpeed;
//      в воздухе — затухание качания (кузов выравнивается, как игрушка);
//   2) интегрирование: скорость центра масс, угловая скорость (тензор инерции ящика кузова), кватернион;
//   3) стены: colPushCar выталкивает кузов (прямоугольник + слой над «полом» кузова), затем по каждому контакту —
//      импульс в точке удара (угол/грань кузова) с отскоком и трением: лобовой — отскок назад и подброс, в угол —
//      закрутка, по касательной — скольжение вдоль. Не вытолкнулось / забор — пробуем без поворота/без подъёма,
//      иначе подшаг отменяется целиком (машина остаётся там, где была чистой), скорость отражается;
//   4) земля кузовом: 5 точек днища (+4 крыши, если лежит на боку/крыше) против colGround с «заметанием» по высоте
//      (от прошлого положения точки до нового — не проваливаемся на любой скорости), выталкивание по нормали и
//      импульс с отскоком — посадка на днище подпрыгивает;
//   5) страховка v9: край мира и склон сбоку — как стена; финальная проверка слоя кузова; надёжные места;
//      возвраты (нет земли 1.5 с, застрял 0.5 с, сидит на днище 2 с, в воздухе 8 с, упал ниже 20 м); перевёрнутую
//      машину через 2 с ставит на колёса на месте.
// Урон: удар о стену быстрее 2 м/с (о землю — 6 м/с) добавляет (v - порог) * 0.85 / 100 / (HitPoints / 3).
#include "car.h"
#include "xsub.h"
#include "carparams.h"
#include "car_inertia.h"
#include "car_wheel_collision.h"
#include <math.h>
#include <string.h>
#include <stddef.h>
#ifdef COL_DEBUG
#include <stdio.h>
extern int colDebug;
#endif

#define SAFE_EVERY 0.25f
#define SAFE_MOVE 3.f         // надёжное место записывается, только если машина с прошлого отъехала хотя бы на столько (застрявшая стоит)
#define RESET_AWAY 5.f        // RESET CAR ставит не ближе этого к месту, где машина сейчас (иначе вернёт в тот же тупик)
#define PEN_REVERT 0.02f     // осталось больше 2 см после выталкивания — подшаг отменяем
#define EMBED_PEN 0.3f
#define CAR_CRACK 1.0f       // щель в коллизии уже 2 м с землёй по обе стороны — не край мира (проверка «край мира» ниже)
// Куст (материал FOLIAGE = в данных диска physProp 10, строка ATC «smash / eShrub», звук hit_shrub: изгороди l*_hedgeunit,
// кусты-шары l*_shrubround, l2/l5_shrubsml — все cls 2, неподвижные). Строка ATC несёт три числа (195, 5, 18) против
// (100, m, 100) у обычных предметов; первое и третье читаем как трение и упругость в процентах от обычного (правило порта по
// данным, единицы в данных не подписаны). Правка Роба 71 (повтор 01.10: «кусты выбрасывают в разные стороны; в оригинале с
// кустами проблем не было никогда»): куст — неподвижная мягкая преграда: упругость удара 18 % от обычной, трение вдоль
// куста 195 %, без подброса стенкой (pop) и без «игрового» подскока сбоку и при посадке — их в данных нет.
// FOLIAGE_PREV — прежняя реакция (подскок сбоку до 7 м/с, посадка до 2.5 м/с, упругость 0.4): только отрицательный контроль гейтов.
// Правка Роба 173 (01.10, L1 J9-4, гонка: «перестань делать из кустов стены — хочу проехать через кусты, доработай везде»):
// машина проходит кусты и изгороди насквозь (col.h colSetFoliagePass), в листве вязнет — скорость спадает как e^(−FOLIAGE_DRAG·t)
// (изгородь 1 м на 12 м/с — минус ~13 %) — и шуршит (hit_shrub). -DCAR_FOLIAGE_PASS=0 — прежняя мягкая преграда (красный гейта)
#ifndef CAR_FOLIAGE_PASS
#ifdef FOLIAGE_PREV
#define CAR_FOLIAGE_PASS 0   // прежняя реакция куста — без проезда (отрицательный контроль гейтов)
#else
#define CAR_FOLIAGE_PASS 1
#endif
#endif
#ifndef FOLIAGE_DRAG
#define FOLIAGE_DRAG 1.6f   // -DFOLIAGE_DRAG=0 — опорная езда без вязкости листвы (гейт tests/foliage/cover.c сравнивает с ней)
#endif
// Правка Роба 304 (04.10, L1 H10-5: «кусты тормозят машину — прям затормаживают»): вязкость листвы — доля кузова внутри куста
// (col.c colFoliageCover), а не пересечение габаритных коробок широкой фазы. Было: у ряда наклонных изгородей H10-5 (коробка
// капсулы 5.3 × 3.8 м) машина в 2.6 м от куста на тротуаре вязла в полную силу — с 17 до 6 м/с на полном газу.
// -DFOLIAGE_DRAG_BOX=1 — прежнее (красный гейта tests/foliage/cover.c)
#ifndef FOLIAGE_DRAG_BOX
#define FOLIAGE_DRAG_BOX 0
#endif
#define FOLIAGE_E 0.18f          // упругость куста к обычной (ATC 18 / 100)
#define FOLIAGE_MU 1.95f         // трение вдоль куста к обычному (ATC 195 / 100)
#ifndef FOLIAGE_PUSH
#define FOLIAGE_PUSH 2.5f        // откат у куста: скорость назад не меньше этой (не больше своей), м/с (правило порта)
#endif
#define FOLIAGE_VY 1.5f          // предел вертикальной скорости от опоры-куста при переезде, м/с (правило порта)
// Правка Роба 449 (10.10, «кусты покатые: мягко заехать и объехать, машина не встаёт»): лист не тормозит в ноль и не держит
// упёршуюся машину. (1) вязкость листвы не опускает скорость ниже FOLIAGE_DRAG_FLOOR (газ в кусте всегда ведёт дальше), (2) машина,
// упёршаяся в мягкое (куст/изгородь) почти с нулевой скоростью, получает FOLIAGE_PUSH_REST назад от газа и выезжает, а не стоит
// (откат при v≈0 раньше давал e = 0). Стволы, церковь и стенка изгороди №421 не затронуты: откат остаётся откатом. -DCAR_BUSH_SOFT_OLD=1 — прежнее
#ifndef CAR_BUSH_SOFT_OLD
#define CAR_BUSH_SOFT_OLD 0
#endif
#ifndef FOLIAGE_DRAG_FLOOR
#define FOLIAGE_DRAG_FLOOR 3.f
#endif
#ifndef FOLIAGE_PUSH_REST
#define FOLIAGE_PUSH_REST 1.5f
#endif
#define FOLIAGE_LAND_MIN 3.f
#define FOLIAGE_LAND_K 0.2f      // правка Роба 71 (29.09: «выкинуло с кустов над крышей; в оригинале с кустами проблем не было»): было 0.45
#define FOLIAGE_LAND_MAX 2.5f    // правка 71: мягкий подскок ~0.3 м (было 6 м/с — ~1.8 м, выносило на крыши)
// одна машина над другой (пункт 8): начало верхней выше начала нижней на эту долю высоты крыши — встаёт на крышу
#define STACK_DY 0.5f
#define REF_MASS 1500.f      // масса, для которой настроены restitution/pop
#define MAXW 12.f            // предел угловой скорости, рад/с

typedef struct { float x, y, z; } V3;
typedef struct { V3 L, U, F; } M3;   // столбцы: оси тела (влево, вверх, вперёд) в мире

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
static V3 v3(float x, float y, float z) { V3 r; r.x = x; r.y = y; r.z = z; return r; }
static V3 add(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 sub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 scl(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static V3 madd(V3 a, V3 b, float s) { return v3(a.x + b.x * s, a.y + b.y * s, a.z + b.z * s); }
static float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static float len(V3 a) { return sqrtf(dot(a, a)); }
static V3 mulM(const M3 *M, V3 b) { return v3(M->L.x * b.x + M->U.x * b.y + M->F.x * b.z, M->L.y * b.x + M->U.y * b.y + M->F.y * b.z,
                                              M->L.z * b.x + M->U.z * b.y + M->F.z * b.z); }
static V3 mulMT(const M3 *M, V3 w) { return v3(dot(M->L, w), dot(M->U, w), dot(M->F, w)); }

static M3 quatM(const float *q) {
  float w = q[0], x = q[1], y = q[2], z = q[3]; M3 M;
  M.L = v3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y));
  M.U = v3(2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x));
  M.F = v3(2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y));
  return M;
}
static void mQuat(const M3 *M, float *q) {   // ортонормированная матрица -> кватернион
  float m00 = M->L.x, m11 = M->U.y, m22 = M->F.z, tr = m00 + m11 + m22;
  if (tr > 0.f) { float s = sqrtf(tr + 1.f) * 2.f; q[0] = .25f * s; q[1] = (M->U.z - M->F.y) / s; q[2] = (M->F.x - M->L.z) / s; q[3] = (M->L.y - M->U.x) / s; }
  else if (m00 > m11 && m00 > m22) { float s = sqrtf(1.f + m00 - m11 - m22) * 2.f; q[0] = (M->U.z - M->F.y) / s; q[1] = .25f * s; q[2] = (M->U.x + M->L.y) / s; q[3] = (M->F.x + M->L.z) / s; }
  else if (m11 > m22) { float s = sqrtf(1.f + m11 - m00 - m22) * 2.f; q[0] = (M->F.x - M->L.z) / s; q[1] = (M->U.x + M->L.y) / s; q[2] = .25f * s; q[3] = (M->F.y + M->U.z) / s; }
  else { float s = sqrtf(1.f + m22 - m00 - m11) * 2.f; q[0] = (M->L.y - M->U.x) / s; q[1] = (M->F.x + M->L.z) / s; q[2] = (M->F.y + M->U.z) / s; q[3] = .25f * s; }
}
static void quatNorm(float *q) { float l = 1.f / sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]); for (int i = 0; i < 4; i++) q[i] *= l; }
static void quatYaw(float *q, float h) { q[0] = cosf(h * .5f); q[1] = 0; q[2] = sinf(h * .5f); q[3] = 0; }

// ------------------------------------------------------------------------------------------------ параметры
#ifndef CAR_REST_TYRE_OLD
#define CAR_REST_TYRE_OLD 0
#endif
#ifndef CAR_REST_SOFT_OLD
#define CAR_REST_SOFT_OLD 0
#endif
#ifndef CAR_NATIVE_PHYSICS_OLD
#define CAR_NATIVE_PHYSICS_OLD 0
#endif
// №487 (Роб 10.10, «касание забора наискось как на PS2»; сверка стен, пробы hedge003_A/B, fence_f001_B, fence_f007_A, teleport000_A):
// после касания забора вперёд PS2 едет дальше 2,5–6,0 м/с и разворачивается на 19–26°, порт терял скорость: кузов скользил боком вдоль
// забора, а боковые силы шин (жёсткость normalSteering) тормозили его как тяга против заноса (замер: жёсткость ×0 — 10 м/с, ×1 — 1,2–3,5).
// Правило порта: 0,25 с после касания забора с лица (вперёд, не сильный удар) боковая жёсткость шин ×CAR_FENCE_LAT (0,65 — по пяти
// пробам: 2,42/4,03/5,89/4,80/3,82 против PS2 2,47/2,63/6,0/5,23/4,64, разворот 20–25°). -DCAR_FENCE_LAT_OLD=1 — прежнее (×1)
#ifndef CAR_CRASH_SLIDE_OLD
#define CAR_CRASH_SLIDE_OLD 0
#endif
#ifndef CAR_CRASH_E
#define CAR_CRASH_E 0.095f   // отскок по нормали от удара ≥ CAR_CRASH_V2 (PS2 b2_ram: +1.73 из 18.19)
#endif
#ifndef CAR_CRASH_MU
#define CAR_CRASH_MU 0.13f   // трение стены: потеря касательной = MU · (1+E) · удар (b2_ram: 2.56 м/с из 19.9)
#endif
#ifndef CAR_FENCE_LAT_OLD
#define CAR_FENCE_LAT_OLD 0
#endif
#ifndef CAR_FENCE_LAT
#define CAR_FENCE_LAT (CAR_FENCE_LAT_OLD ? 1.f : 0.65f)
#endif
#ifndef CAR_FENCE_LAT_T
#define CAR_FENCE_LAT_T 0.25f
#endif
#ifndef CAR_REST_SKIP_OLD
#define CAR_REST_SKIP_OLD 0
#endif
#ifndef CAR_REST_SKIP_STEPS
#define CAR_REST_SKIP_STEPS 240
#endif
// ПРОТОТИП (окно «Разбор x_car3d», 24.10): пустая машина игрока на ручнике (водитель вышел, gameCarLimits: газ, тормоз, руль 0, ручник 1) 4 подшага на кадр считала нулевой результат:
// стоящая на опоре с ручником она не попадает в quietSupport (там «!hb», и один из четырёх склонов опоры мягче .9975), значит CAR_REST_SKIP не включается (причал L3: x_car 0,8 мс эмулятора, 1,2-1,4 мс PSP).
// Правило: ввод тот же (ручник, ничего больше), не разбита, не на опоре-крыше, за CAR_PARK_N посчитанных кадров поза сдвинулась меньше CAR_PARK_EPS (метр, угол) и скорости < CAR_PARK_V — дальше
// считаем один кадр из CAR_PARK_EVERY, остальные как CAR_REST_SKIP (время идёт, подшаги нет). Любой толчок (скорость), смена ввода, телепорт (поза не равна последней посчитанной) — обычный счёт.
// CAR_PARK_SKIP_OLD=1 — прежнее
#ifndef CAR_PARK_SKIP_OLD
#define CAR_PARK_SKIP_OLD 0
#endif
#define CAR_PARK_N 30
#define CAR_PARK_EVERY 8
#ifndef CAR_PARK_EPS
#define CAR_PARK_EPS 1e-3f
#endif
#define CAR_PARK_V .02f
#ifndef CAR_WHEEL_RADIUS_MESH_OLD
#define CAR_WHEEL_RADIUS_MESH_OLD 0
#endif
static float bodyMass(const Car *c) { return c->p.bodyMass > 0.f ? c->p.bodyMass : c->p.mass; }
static void nativeBodyFill(CarParams *p, const char *name) {
  p->bodyMass = 0.f; memset(p->bodyInertia, 0, sizeof p->bodyInertia);
  memset(p->wheelCollision, 0, sizeof p->wheelCollision);
  memset(p->wheelCollisionOffset, 0, sizeof p->wheelCollisionOffset);
  if (CAR_NATIVE_PHYSICS_OLD) return;
  for (unsigned i=0;i<sizeof carWheelCollision/sizeof *carWheelCollision;i++) {
    if (!strcmp(carWheelCollision[i].name,name)) {
      memcpy(p->wheelCollision,carWheelCollision[i].sphere,sizeof p->wheelCollision);
      memcpy(p->wheelCollisionOffset,carWheelCollision[i].offset,sizeof p->wheelCollisionOffset);break;
    }
  }
  for (unsigned i = 0; i < sizeof carBodyPhysics / sizeof *carBodyPhysics; i++) {
    const CarBodyPhysics *b = &carBodyPhysics[i]; if (strcmp(b->name, name)) continue;
    p->bodyMass = p->mass * b->massFraction; memcpy(p->bodyInertia, b->inertia, sizeof p->bodyInertia);
    memcpy(p->bodyCentre,b->centre,sizeof p->bodyCentre);p->modelLift=b->lift;
#ifndef CAR_HITY_OLD
#define CAR_HITY_OLD 0   // 1 — высота точки удара о стену 0,62 м над низом колёс у всех машин (прежнее)
#endif
    // Скользящий удар о стену (В102, Стенд 16.10, PS2 schoolbu и marge_v на 10 м/с под 35°): у оригинала кузов валится боком (крен 14–16°, тангаж +4…+6), на нос
    // не клюёт. Порт бил всех на 0,62 м над колёсами (центр масс famil_v 0,68), а у высоких центр масс выше (marge_v 1,32, schoolbu 1,86): продольный импульс
    // ниже центра масс давил нос вниз на −10° и поднимал зад на 20–40 кадров (две оси в воздухе). Теперь точка удара поднимается вместе с центром масс на 0,55 его превышения над 0,68
    if (!CAR_HITY_OLD) { float cmh = b->centre[1] + b->lift - 0.68f; if (cmh > 0.f) p->hitY = 0.62f + 0.55f * cmh; }
    memcpy(p->wheelCentreWeight,b->wheelWeight,sizeof p->wheelCentreWeight);
    memcpy(p->bodyOriginInertia,b->originInertia,sizeof p->bodyOriginInertia);
    memcpy(p->wheelCentre,b->wheelCentre,sizeof p->wheelCentre);
    float *g[12]={&p->halfW,&p->halfL,&p->bodyOfs,&p->wheelX,&p->wheelF,&p->wheelR,&p->wheelRad,&p->bodyLo,&p->bodyHi,&p->bandLo,&p->bandHi,&p->step};
    for(int j=0;j<12;j++)*g[j]=b->geom[j];
    // The suspension radius is the resting wheel-centre height when all four
    // authored wheel spheres share one height. Hidden proxy meshes (honor_v)
    // can have a different half-height from the actual wheel geometry.
    if (!CAR_WHEEL_RADIUS_MESH_OLD) {
      float lo=1e9f,hi=-1e9f,mean=0.f;int count=0;
      for(int j=0;j<4;j++) if(p->wheelCollision[j][3]>0.f) {
        float h=p->wheelCollision[j][1]+p->modelLift;
        lo=fminf(lo,h);hi=fmaxf(hi,h);mean+=h;count++;
      }
      if(count==4 && hi-lo<=.01f && mean/count>.05f) p->wheelRad=mean/count;
    }
    return;
  }
}
static void conFill(const CarCon *C, CarParams *p);
int carParamsFor(const char *name, CarParams *p) {
  int k = -1, dflt = 0;
  for (int i = 0; i < CAR_CONS; i++) {
    if (name && !strcmp(carCons[i].name, name)) k = i;
    if (!strcmp(carCons[i].name, "famil_v")) dflt = i;
  }
  conFill(&carCons[k >= 0 ? k : dflt], p);
  return k >= 0;
}
int carIrisFor(const char *name) {
  if (name) for (int i = 0; i < CAR_IRIS_CONS; i++) if (!strcmp(carIrisCons[i], name)) return 1;
  return 0;
}
// ИИ-водитель машины миссии: физика из .con 4-го аргумента AddStageVehicle ("Missions\level01\M1race.con" -> ключ
// "missions/level01/m1race"; путь без "Missions\" тоже ищется под missions/). Нет такого файла на диске (L5 m6
// "level05\M4chase.con") — ходовые самой машины car (.con верхнего уровня), иначе famil_v. Геометрию задаёт вызывающий (модель)
int carParamsForAi(const char *con, const char *car, CarParams *p) {
  char key[48]; int n = 0;
  for (const char *s = con ? con : ""; *s && n < (int)sizeof key - 1; s++) key[n++] = *s == '\\' ? '/' : (*s >= 'A' && *s <= 'Z') ? (char)(*s + 32) : *s;
  key[n] = 0; if (n > 4 && !strcmp(key + n - 4, ".con")) key[n -= 4] = 0;
  char lc[16]; int k = 0; for (const char *s = car ? car : ""; *s && k < 15; s++) lc[k++] = (*s >= 'A' && *s <= 'Z') ? (char)(*s + 32) : *s;
  lc[k] = 0;
  for (int pass = 0; pass < 2 && n; pass++) {
    char k2[64]; strcpy(k2, pass ? "missions/" : ""); strcat(k2, key);
    for (int i = 0; i < CAR_CONS_AI; i++) if (!strcmp(carConsAi[i].name, k2)) {
      char nm[12]; carParamsFor(lc, p); memcpy(nm, p->name, sizeof nm);
      CarCon M = carConsAi[i];
#ifndef CAR_TUNING_PERCAR_OLD
      // Правка Роба 10.10 («у каждой машины своя физика, свои торможения»): .con водителя — последовательность Set*, команды которых
      // нет (у 164 из 165 файлов: пробуксовка, пончик, неваляшка, вилли, тяга на скорости) оставляют значение самой машины, а не
      // общий запасной из con2h. Прежнее — -DCAR_TUNING_PERCAR_OLD=1
      { const CarCon *own = 0; for (int j = 0; j < CAR_CONS; j++) if (!strcmp(carCons[j].name, lc)) own = &carCons[j];
        if (own) { const float *o = (const float *)&own->mass; float *m = (float *)&M.mass; const unsigned mask = carConsAiMask[i];
                   for (unsigned f = 0; f < (sizeof(CarCon) - offsetof(CarCon, mass)) / sizeof(float) && f < 32; f++) if (!(mask >> f & 1u)) m[f] = o[f]; } }
#endif
      conFill(&M, p); memcpy(p->name, nm, sizeof nm);   // имя — машины (рисунок, звук), физика — водителя
      if (strcmp(p->name, lc)) { memset(p->name, 0, sizeof p->name); strncpy(p->name, lc, sizeof p->name - 1); }
      nativeBodyFill(p, p->name); return 1;
    }
  }
  carParamsFor(lc, p);
  return 0;
}
static void conFill(const CarCon *C, CarParams *p) {
  memset(p, 0, sizeof *p);
  p->mass = C->mass; p->gasScale = C->gasScale; p->slipGasScale = C->slipGasScale; p->hsGasScale = C->hsGasScale;
  p->gasSpeedThreshold = C->gasSpeedThreshold; p->brakeScale = C->brakeScale; p->topSpeed = C->topSpeedKmh / 3.6f;
  p->maxWheelTurn = C->maxWheelTurnDeg * (3.14159265f / 180.f); p->hsSteeringDrop = C->hsSteeringDrop; p->tireGrip = C->tireGrip;
  p->normalSteering = C->normalSteering; p->slipSteering = C->slipSteering; p->eBrakeEffect = C->eBrakeEffect;
  p->slipSteeringNoEBrake = C->slipSteeringNoEBrake; p->slipEffectNoEBrake = C->slipEffectNoEBrake;
  p->cmX = C->cmOffsetX; p->cmY = C->cmOffsetY; p->cmZ = C->cmOffsetZ;
  p->suspensionLimit = C->suspensionLimit; p->springK = C->springK; p->damperC = C->damperC; p->suspensionYOffset = C->suspensionYOffset;
  p->hitPoints = C->hitPoints; p->burnoutRange = C->burnoutRange; p->donutTorque = C->donutTorque;
  p->weebleOffset = C->weebleOffset; p->wheelieRange = C->wheelieRange; p->wheelieY = C->wheelieOffsetY; p->wheelieZ = WHEELIE_CAP_OLD ? C->wheelieOffsetZ : fmaxf(C->wheelieOffsetZ, -WHEELIE_Z_CAP);
  // геометрия famil_v; машина уровня переписывает её из CAR1 v2 (carParamsFromCar1), машины трафика — из TRF1
  p->halfW = 1.10f; p->halfL = 2.675f; p->bodyOfs = -0.32f;
  p->wheelX = 0.91f; p->wheelF = 1.47f; p->wheelR = 1.82f; p->wheelRad = 0.37f;
  p->bodyLo = 0.28f; p->bodyHi = 1.65f;
  p->bandLo = 0.35f; p->bandHi = 1.6f; p->step = 0.35f;
  // посадка и камера famil_v: сустав dl, якорь посадки Гомера (прежние HOMER_DOOR_X/Z viewer/homer.c), FollowCameraData
  p->seatData = 0;
  p->seat[0] = -0.436739266f; p->seat[1] = -0.185813189f; p->seat[2] = 0.173990607f;
  p->seatAx = p->doorX = -1.475f; p->seatAz = p->doorZ = 0.57f;
  p->camElev = 1.23f; p->camMag = 7.2f;
  // отклик
  p->gravity = 16.f;   // measured original PAL ballistic trace: knowledge/facts/ps2-car-gravity.yaml
  p->restitution = 0.42f; p->wallFriction = 0.2f;
  // Правило порта: коэффициент материала калибруется полной позой PS2, не одной скоростью
  // original-drop: пик 3,45021; 0,1 даёт 2,313, 0,3 даёт 5,212 (native_replay)
#ifndef CAR_DROP_RESTITUTION_OLD
#define CAR_DROP_RESTITUTION_OLD 0 // 0.18 passes the clean original drop; OLD=1 restores 0.1
#endif
  p->groundRestitution = CAR_DROP_RESTITUTION_OLD ? 0.1f : 0.18f; p->groundFriction = 0.4f;
  p->pop = 0.10f; p->popMax = 2.6f; p->hitY = 0.62f;
  p->angDamp = 0.8f; p->airLevel = 0.5f; p->steerResp = 10.f; p->reverseSpeed = 50.f / 3.6f;
  p->fallReset = 20.f; p->sub = 1.f / 120.f;
  strncpy(p->name, C->name, sizeof p->name - 1);
  nativeBodyFill(p, p->name);
}
void carDefaults(CarParams *p) { carParamsFor("famil_v", p); }

// CAR1 v2: заголовок 32 Б ('CAR1', u32 версия, njoint, nhinge, ndam, nflare, смещение блока машины, его длина), блок —
// tools/psp_export.CAR_INFO_FMT: char car[16]; 12 чисел кузова; seat[3]; seatAx, seatAz, doorX, doorZ; камера rot, elev,
// mag, target[3], farMag; u32 flags
#define CAR1_INFO_LEN (16 + 27 * 4)
static int finiteIn(float v, float lo, float hi) { return v == v && v >= lo && v <= hi; }
int carParamsFromCar1(const void *sec, unsigned size, CarParams *p) {
  carDefaults(p);
  const unsigned char *b = sec;
  if (!b || size < 32 || memcmp(b, "CAR1", 4)) return -1;
  unsigned u[7]; memcpy(u, b + 4, sizeof u);
  if (u[0] < 2) return 0;                                  // версия 1: машины в секции нет — famil_v, как до 29.09
  if (u[6] < CAR1_INFO_LEN || u[5] < 32 || u[5] > size || size - u[5] < CAR1_INFO_LEN) return -2;
  return carParamsFromInfo(b + u[5], p);
}
// блок машины CAR1 v2 (128 Б, psp_export.car_info_bytes): и у машины уровня, и в таблице пакета машин миссий msncars (MCR1 v2)
int carParamsFromInfo(const void *info, CarParams *p) {
  const unsigned char *q = info;
  char name[17]; memcpy(name, q, 16); name[16] = 0;
  float f[27]; memcpy(f, q + 16, sizeof f);
  if (!carParamsFor(name, p)) { carDefaults(p); return -3; }
  // кузов (порядок как в car.h и psp_export.car_info_bytes)
  float *g[12] = {&p->halfW, &p->halfL, &p->bodyOfs, &p->wheelX, &p->wheelF, &p->wheelR, &p->wheelRad, &p->bodyLo, &p->bodyHi,
                  &p->bandLo, &p->bandHi, &p->step};
  for (int i = 0; i < 12; i++) *g[i] = f[i];
  memcpy(p->seat, f + 12, sizeof p->seat);
  p->seatAx = f[15]; p->seatAz = f[16]; p->doorX = f[17]; p->doorZ = f[18];
  p->camElev = f[20]; p->camMag = f[21];                   // f[19] поворот (180°: сзади), f[22..24] цель, f[25] дальняя камера
  int ok = finiteIn(p->halfW, .3f, 3.f) && finiteIn(p->halfL, .8f, 8.f) && finiteIn(p->bodyOfs, -2.f, 2.f) &&
           finiteIn(p->wheelX, .2f, 2.5f) && finiteIn(p->wheelF, .2f, 5.f) && finiteIn(p->wheelR, .2f, 5.f) &&
           finiteIn(p->wheelRad, .1f, 1.5f) && finiteIn(p->bodyLo, 0.f, 2.f) && finiteIn(p->bodyHi, p->bodyLo + .2f, 6.f) &&
           finiteIn(p->bandLo, 0.f, 2.f) && finiteIn(p->bandHi, p->bandLo + .1f, 6.f) && finiteIn(p->step, .05f, 1.f) &&
           finiteIn(p->camElev, .2f, 1.6f) && finiteIn(p->camMag, 2.f, 20.f);
  for (int i = 0; ok && i < 3; i++) ok = finiteIn(p->seat[i], -8.f, 8.f);
  ok = ok && finiteIn(p->seatAx, -8.f, 8.f) && finiteIn(p->seatAz, -8.f, 8.f) && finiteIn(p->doorX, -8.f, 8.f) && finiteIn(p->doorZ, -8.f, 8.f);
  if (!ok) { carDefaults(p); return -4; }
  p->seatData = 1;
  nativeBodyFill(p,p->name); // CAR1's legacy wheel proxy must not replace the authored suspension geometry
  return 1;
}
const char *carParamsError(int e) {
  switch (e) {
    case 1: return "машина уровня из CAR1"; case 0: return "CAR1 версии 1: famil_v";
    case -1: return "не секция CAR1"; case -2: return "блок машины CAR1 за концом секции";
    case -3: return "машины CAR1 нет в carparams.h"; case -4: return "числа машины CAR1 вне разумного";
  }
  return "?";
}

// производные величины: пружины по колёсам (статическая нагрузка -> одинаковое сжатие eq, кузов в покое ровно),
// точка крепления подвески, центр масс, тензор инерции ящика кузова
static void tensorSet(Car *c,const float I[6]) {
  float xx=I[0],xy=I[1],xz=I[2],yy=I[3],yz=I[4],zz=I[5];
  float a=yy*zz-yz*yz,b=xz*yz-xy*zz,d=xy*yz-xz*yy,e=xx*zz-xz*xz,f=xy*xz-xx*yz,g=xx*yy-xy*xy;
  float det=xx*a+xy*b+xz*d;
  if(isfinite(det)&&det>1e-9f) {
    float s=1.f/det;float inv[6]={a*s,b*s,d*s,e*s,f*s,g*s};
    memcpy(c->bodyTensor,I,sizeof c->bodyTensor);memcpy(c->invInertia,inv,sizeof inv);
    c->inertia[0]=xx;c->inertia[1]=yy;c->inertia[2]=zz;
  }
}
static void updateBodyTensor(Car *c,const float comp[4]) {
  const CarParams *P=&c->p;if(P->bodyMass<=0.f)return;
  float I[6],centre[3];memcpy(I,P->bodyOriginInertia,sizeof I);memcpy(centre,P->bodyCentre,sizeof centre);
  for(int i=0;i<4;i++) {
    float w=P->wheelCentreWeight[i],dy=P->suspensionYOffset+comp[i];const float *p=P->wheelCentre[i];
    float h=w*(2.f*p[1]*dy+dy*dy);I[0]+=h;I[5]+=h;I[1]-=w*p[0]*dy;I[4]-=w*p[2]*dy;centre[1]+=w*dy;
  }
  float x=centre[0],y=centre[1],z=centre[2];
  I[0]-=y*y+z*z;I[3]-=x*x+z*z;I[5]-=x*x+y*y;I[1]+=x*y;I[2]+=x*z;I[4]+=y*z;
  for(int i=0;i<6;i++)I[i]*=bodyMass(c);tensorSet(c,I);
}
static void derive(Car *c) {
  const CarParams *P = &c->p;
  float m = bodyMass(c), mg = m * P->gravity;
  c->lim = fmaxf(0.03f, P->wheelRad * P->suspensionLimit);
  float kb = P->springK * mg / c->lim;
  c->eq = (sqrtf(1.f + 20.f * (mg * .25f) / kb) - 1.f) / 10.f;          // k(x + 5x^2) = mg/4
  if (c->eq > c->lim * 0.9f) c->eq = c->lim * 0.9f;
  c->mountY = P->wheelRad + c->lim - c->eq;
  c->modelRaise=P->bodyMass>0.f?-(P->suspensionYOffset+c->eq):0.f;
  c->comBase[0] = P->cmX; c->comBase[1] = P->bodyHi * .5f + P->cmY; c->comBase[2] = P->bodyOfs + P->cmZ;
  if (P->bodyMass>0.f) {
    c->comBase[0]=P->bodyCentre[0]+P->cmX;
    float wheelShift=0.f;for(int i=0;i<4;i++)wheelShift+=P->wheelCentreWeight[i]*(P->suspensionYOffset+c->eq);
    c->comBase[1]=P->modelLift+c->modelRaise+P->bodyCentre[1]+P->cmY+wheelShift;
    c->comBase[2]=P->bodyCentre[2]+P->cmZ;
  }
  float wb = P->wheelF + P->wheelR, zc = clampf(c->comBase[2], -P->wheelR * .9f, P->wheelF * .9f);
  float xc = clampf(c->comBase[0], -P->wheelX * .9f, P->wheelX * .9f);
  for (int i = 0; i < 4; i++) {
    float ax = i < 2 ? (zc + P->wheelR) / wb : (P->wheelF - zc) / wb;           // доля оси
    float sd = (i & 1) ? (P->wheelX - xc) / (2.f * P->wheelX) : (P->wheelX + xc) / (2.f * P->wheelX);   // доля борта (x влево)
    float share = 4.f * ax * sd;                                              // 1 = ровно четверть веса
    c->kS[i] = kb * share * CAR_SPRING_SCALE;
    c->cS[i] = P->damperC * CAR_DAMP_SCALE * 2.f * sqrtf(c->kS[i] * m);
  }
  float W = 2.f * P->halfW, H = P->bodyHi - P->bodyLo * .5f, L = 2.f * P->halfL;
  c->inertia[0] = m * (H * H + L * L) / 12.f;    // тангаж (ось x тела)
  c->inertia[1] = m * (W * W + L * L) / 12.f;    // рыскание
  c->inertia[2] = m * (W * W + H * H) / 12.f;    // крен
  float box[6]={c->inertia[0],0,0,c->inertia[1],0,c->inertia[2]};tensorSet(c,box);
  float equilibrium[4]={c->eq,c->eq,c->eq,c->eq};updateBodyTensor(c,equilibrium);
}

static V3 invIw(const Car *c, const M3 *M, V3 t) {
  V3 b = mulMT(M, t); const float *I=c->invInertia;
  V3 r=v3(I[0]*b.x+I[1]*b.y+I[2]*b.z,I[1]*b.x+I[3]*b.y+I[4]*b.z,I[2]*b.x+I[4]*b.y+I[5]*b.z);
  return mulM(M, r);
}
static V3 inertiaW(const Car *c, const M3 *M, V3 w) {
  V3 b=mulMT(M,w),r;
  const float *I=c->bodyTensor;r=v3(I[0]*b.x+I[1]*b.y+I[2]*b.z,I[1]*b.x+I[3]*b.y+I[4]*b.z,I[2]*b.x+I[4]*b.y+I[5]*b.z);
  return mulM(M,r);
}
#ifndef CAR_DYNAMIC_INERTIA_OLD
#define CAR_DYNAMIC_INERTIA_OLD 0
#endif
static V3 changeBodyTensor(Car *c,const M3 *M,const float compression[4],V3 omega) {
  V3 momentum=inertiaW(c,M,omega);
  updateBodyTensor(c,compression);
  // PS2 stores angular momentum and derives omega through the current
  // inverse tensor. Wheel motion must not create a free angular impulse.
  return CAR_DYNAMIC_INERTIA_OLD?omega:invIw(c,M,momentum);
}
static float effMass(const Car *c, const M3 *M, V3 n, V3 r) {   // 1 / K для импульса вдоль n в точке r
  float K = 1.f / bodyMass(c) + dot(n, cross(invIw(c, M, cross(r, n)), r));
  return K > 1e-9f ? 1.f / K : 0.f;
}

// Backward-Euler normal suspension forces. All wheel contacts share the same
// body's linear/angular response; treating each damper explicitly can amplify
// rotation when the native inertia is small. No allocation or per-car rules.
#ifndef CAR_EXPLICIT_SUSPENSION_OLD
#define CAR_EXPLICIT_SUSPENSION_OLD 0
#endif
#ifndef CAR_MASS_RESTITUTION_OLD
#define CAR_MASS_RESTITUTION_OLD 0
#endif
#ifndef CAR_TYRE_AXLE_OLD
#define CAR_TYRE_AXLE_OLD 0
#endif
// Правка 388/377 (08.10, стена горба cyl 563, tests/humps H): машина съезжает с уступа в канаву между трубами, заднее колесо входит в упор
// подвески (сжатие 0,59 при ходе 0,27), и упор два кадра толкает кузов на пределе 6·mg — +3,2 м/с вверх (порог 3,0). Упор — правило
// порта (не .con): 60·k на сжатии, 8·k на отбое, демпфер ×13. Отбой 8·k на пределе 6·mg — тот же подброс; совсем без отбоя (0)
// кузов после жёсткой посадки на 100 км/ч ложится на землю и встаёт (jz16_k100 итог 28 м вместо 79, jz2_k70 33 вместо 79); 2·k —
// jz2_k70 ещё падает; 4·k — H 2,87 (все 168 стен держат), посадки 1068, трамплины и ramps как прежде. CAR_BUMPSTOP_REBOUND_OLD=1 —
// прежние 8·k (красный tests/humps H: cyl 563 вверх 3,21)
#ifndef CAR_BUMPSTOP_REBOUND_OLD
#define CAR_BUMPSTOP_REBOUND_OLD 0
#endif
#ifndef CAR_BUMPSTOP_REBOUND_K
#define CAR_BUMPSTOP_REBOUND_K 4.f   // жёсткость упора на отбое, доли k (прежняя 8)
#endif
#ifndef CAR_BUMPSTOP_DAMP
#define CAR_BUMPSTOP_DAMP 13.f   // демпфер за ходом подвески, доли амортизатора (прежняя 13)
#endif
static float contactRestitution(const Car *c,float e) {
  // Mass already enters the contact's effective impulse. Scaling material
  // restitution a second time made a light body rebound much faster.
  return CAR_MASS_RESTITUTION_OLD?e*sqrtf(REF_MASS/bodyMass(c)):e;
}
static void suspensionForces(const Car *c,const M3 *M,const V3 r[4],const V3 n[4],
                             const float spring[4],const float tangent[4],const float damping[4],
                             const float freeV[4],const int active[4],float dt,float force[4]) {
  float A[4][4]={{0}},b[4]={0},cap=6.f*bodyMass(c)*c->p.gravity;
  for(int i=0;i<4;i++) {
    force[i]=0.f;if(!active[i])continue;
    float d=damping[i]+dt*tangent[i];b[i]=spring[i]-d*freeV[i];
    for(int j=0;j<4;j++)if(active[j]) {
      float W=dot(n[i],n[j])/bodyMass(c)+dot(cross(r[i],n[i]),invIw(c,M,cross(r[j],n[j])));
      A[i][j]=(i==j?1.f:0.f)+dt*d*W;
    }
  }
  for(int it=0;it<24;it++) {
    float change=0.f;
    for(int i=0;i<4;i++)if(active[i]) {
      float f=b[i];for(int j=0;j<4;j++)if(j!=i)f-=A[i][j]*force[j];
      f=clampf(f/A[i][i],0.f,cap);change=fmaxf(change,fabsf(f-force[i]));force[i]=f;
    }
    if(change<cap*1e-6f)break;
  }
}

// «пол» кузова для слоя стен: плоскость через начало машины с нормалью = ось вверх тела (наклон ограничен)
static void floorOf(const M3 *M, float *gx, float *gz) {
  float uy = fmaxf(M->U.y, 0.5f);
  *gx = clampf(-M->U.x / uy, -1.2f, 1.2f); *gz = clampf(-M->U.z / uy, -1.2f, 1.2f);
}
static void fwdH(const M3 *M, float h, float *fx, float *fz) {   // горизонтальное «вперёд»
  float x = M->F.x, z = M->F.z, l = x * x + z * z;
  if (l < 0.04f) { x = -M->U.x * (M->F.y > 0 ? 1.f : -1.f); z = -M->U.z * (M->F.y > 0 ? 1.f : -1.f); l = x * x + z * z; }   // нос вверх/вниз
  if (l < 1e-6f) { *fx = sinf(h); *fz = cosf(h); return; }
  l = 1.f / sqrtf(l); *fx = x * l; *fz = z * l;
}
static void boxAt(const Car *c, V3 pos, const M3 *M, CarBox *b) {
  float fx, fz, gx, gz; fwdH(M, c->h, &fx, &fz); floorOf(M, &gx, &gz);
  b->x = pos.x + fx * c->p.bodyOfs; b->z = pos.z + fz * c->p.bodyOfs;
  b->fx = fx; b->fz = fz; b->hw = c->p.halfW; b->hl = c->p.halfL;
  b->y = pos.y + gx * (b->x - pos.x) + gz * (b->z - pos.z); b->gx = gx; b->gz = gz;
  b->lo = c->p.bandLo + c->modelRaise; b->hi = c->p.bandHi + c->modelRaise;
  // правка 339: опора для внутренних заборов (col.h CarBox.fsup) — машина на земле (колёсами или днищем) на своей высоте; в полёте — нет
  b->fsupOn = c->grounded; b->fsup = c->y;
}
void carBox(const Car *c, CarBox *b) { float q[4]; memcpy(q, c->q, sizeof q); M3 M = quatM(q); boxAt(c, v3(c->x, c->y, c->z), &M, b); }

// опора-крыша другой машины (пункт 8): точка внутри прямоугольника крыши и крыша в [yBot, yTop]
static int supportAt(const Car *c, float x, float z, float yTop, float yBot, float *gy) {
  if (!c->supOn) return 0;
  float dx = x - c->supX, dz = z - c->supZ, lf = dx * c->supFx + dz * c->supFz, lr = -dx * c->supFz + dz * c->supFx;
  if (fabsf(lf) > c->supHL || fabsf(lr) > c->supHW || c->supY > yTop || c->supY < yBot) return 0;
  *gy = c->supY; return 1;
}

// ------------------------------------------------------------------------------------------------ установка
static void pushSafe(Car *c) {
  if (c->nsafe > 0) { const float *s = &c->safe[c->safeHead].x; if (hypotf(c->x - s[0], c->z - s[2]) < SAFE_MOVE && fabsf(c->y - s[1]) < 2.f) return; }
  c->safeHead = (c->safeHead + 1) % CAR_SAFE;
  c->safe[c->safeHead].x = c->x; c->safe[c->safeHead].y = c->y; c->safe[c->safeHead].z = c->z;
  c->safe[c->safeHead].h = c->h; c->safe[c->safeHead].t = c->t;
  if (c->nsafe < CAR_SAFE) c->nsafe++;
}

// ориентация по курсу и наклону пола (gx, gz)
static void orientQ(float *q, float h, float gx, float gz) {
  M3 M; float l;
  M.U = v3(-gx, 1.f, -gz); l = 1.f / len(M.U); M.U = scl(M.U, l);
  V3 f = v3(sinf(h), 0.f, cosf(h)); f = madd(f, M.U, -dot(f, M.U)); M.F = scl(f, 1.f / len(f));
  M.L = cross(M.U, M.F);
  mQuat(&M, q); quatNorm(q);
}

// опора колёсами в покое: высота начала машины и наклон по 4 лучам (колесо без опоры — как стояло)
static int restOn(Car *c, const ColWorld *w, float h, float *yT, float *gxT, float *gzT) {
  const CarParams *P = &c->p;
  float fx = sinf(h), fz = cosf(h), rx = -fz, rz = fx, g[4]; int n = 0;
  for (int k = 0; k < 4; k++) {
    float lon = k < 2 ? P->wheelF : -P->wheelR, lat = (k & 1) ? P->wheelX : -P->wheelX, gg;   // k&1 — правое
    float ox = fx * lon + rx * lat, oz = fz * lon + rz * lat, yw = c->y + c->gx * ox + c->gz * oz;
    g[k] = yw;
    if (colGround(w, c->x + ox, c->z + oz, yw + P->step + 0.5f, yw - 1.5f, &gg, 0)) { g[k] = gg; n++; }
  }
  if (!n) return 0;
  float wb = P->wheelF + P->wheelR;
  float hF = (g[0] + g[1]) * .5f, hR = (g[2] + g[3]) * .5f, hL = (g[0] + g[2]) * .5f, hRt = (g[1] + g[3]) * .5f;
  float s1 = clampf((hF - hR) / wb, -1.f, 1.f), s2 = clampf((hRt - hL) / (2.f * P->wheelX), -1.f, 1.f);
  *yT = (hF * P->wheelR + hR * P->wheelF) / wb;
  *gxT = fx * s1 + rx * s2; *gzT = fz * s1 + rz * s2;
  return n;
}

static void syncOut(Car *c);
#ifndef EDGE_DEATH_OLD
#define EDGE_DEATH_OLD 0   // 1: край пустоты всегда стена («край мира», revert 16), даже над зоной DEATH
#endif
#ifndef EDGE_DEATH_DROP
#define EDGE_DEATH_DROP 80.f   // зона DEATH ниже края не дальше стольких метров
#endif
int carEdgeDeathOld = EDGE_DEATH_OLD;
unsigned long long edgeDeathN;   // сколько раз край принят из-за зоны DEATH под ним (счётчик гейта)
static int edgeDeathBelow(const ColWorld *w, float x, float y, float z) { return w->ndeath && colDeathBelow(w, x, y, z, EDGE_DEATH_DROP) >= 0; }
// путь через забор на высоте кузова b (внутренние заборы с конечным верхом — tools/psp_export.py col_fence_tops)
static int crossAt(const ColWorld *w, const CarBox *b, float x0, float z0, float x1, float z1) {
  return colCrossFenceBox(w, x0, z0, x1, z1, b, 0, 0);
}
// Зона прыжка. Данные — блок JMP1 (tools/jumps_export.py): Event 48 jumpzone* из файла, который сценарий уровня подписывает
// «jump boost zones» (level.mfk); объём зоны — её TriggerVolume (у jumpzone17 — склон перед домом, над которым снимает
// статичная камера z1_housejump). Прежнее правило порта (правка Роба 47 «с трамплина нельзя перелететь дом», с правки 342
// выключено, CAR_JUMP_BOOST=1): машина, оторвавшаяся от земли внутри объёма с подъёмом (vy > JUMP_BOOST_VY) и не медленнее
// JUMP_BOOST_V, получает вертикальную скорость, с которой низ колёс поднимается до верха объёма зоны (если своя меньше).
#ifndef JUMP_BOOST_VY
#define JUMP_BOOST_VY 0.5f
#endif
#ifndef JUMP_BOOST_V
#define JUMP_BOOST_V 8.f
#endif
// Правка Роба 342 (05.10: «у трамплина как будто есть ускоритель — не надо; на всех трамплинах физику как в оригинале»).
// Близнец оригинала (tools/twins/jumpref.py, свой PCSX2 без окна; knowledge/facts/ps2-ramps.yaml, гейт tests/jumps/twin.sh):
// на jumpzone16 (F13), jumpzone17 (дом J10), jumpzone2 (G14) и jumpzone4 (F12) вертикального подброса нет — полёт от кромки
// баллистический (g 16), машина взлетает со склоном трамплина. Зона «jump boost» у оригинала — РАЗГОН на въезде: пока
// машина (её габарит, ~2,6 м до края объёма) в объёме зоны, колёса на земле и газ нажат, продольное ускорение на 8–10 м/с2
// больше, чем вне зоны (на всех четырёх зонах, 50–100 км/ч), и скорость выходит за предел машины (28,4 м/с при пределе 27,8).
// В полёте газ ничего не даёт. Правило порта по этим данным: JUMP_ZONE_ACC × газ вдоль курса, колёса на земле, в объёме зоны,
// раздутом на полудлину машины. Прежнее правило правки 47 (подброс вверх до верха объёма) — CAR_JUMP_BOOST=1 или carJumpBoost = 1.
#ifndef CAR_JUMP_BOOST
#define CAR_JUMP_BOOST 0
#endif
#ifndef JUMP_ZONE_ACC
#define JUMP_ZONE_ACC 9.f   // м/с2 сверх тяги (оригинал: +9,6 jumpzone17, +10,6 jumpzone16, +8 jumpzone4, ~+10 jumpzone2)
#endif
int carJumpBoost = CAR_JUMP_BOOST;   // 1 — прежнее правило 47 вместо разгона (отрицательный контроль tests/jumps/twin.sh --gate)
int carJumpZoneDriveOff;             // служебное: разгон зоны выключен (отрицательный контроль)
int carJumpBoostOff;   // служебное: отрицательный контроль (tests/jumps) — зоны прыжка выключены (и разгон, и прежний подброс)
// №705 часть 2 (Роб 17.10: «L5 J4-5, на трамплине невозможно прыгнуть»; PS2, knowledge/facts/ps2-h82-fire-escape.yaml, ps2_h82_v25a): зона рампы пожарной лестницы L5 — тонкие «ворота» поперёк въезда
// (полудлина вдоль въезда 0,92 м, поперёк 3,86; у остальных зон, где сверено близнецом, вдоль въезда 4–8 м). По записи PS2 машина набирает скорость на рампе на отрезке ~17 м (x 53 -> 37 у
// центра зоны x 45,3), а по правилу «в объёме + полудлина машины» окно было ~7 м: порт вылетал с 22,4 м/с против 24,3, вершина ниже на 1,3 м, на 25 м/с не садился на крышу.
// Правило: полудлина зоны ВДОЛЬ курса меньше JUMP_ZONE_MIN_ALONG (6 м) — окно разгона считается как у зоны с такой полудлиной (раздув объёма на разницу). Зоны с полудлиной вдоль въезда от 6 м
// (все четыре сверенные близнецом, tests/jumps/twin.sh) не меняются. JUMP_ZONE_GATE_OLD=1 / carJumpZoneGateOld = 1 / файл jzgate705old / flag jzgate705 0 — прежнее
#ifndef JUMP_ZONE_MIN_ALONG
#define JUMP_ZONE_MIN_ALONG 6.f
#endif
#ifndef JUMP_ZONE_GATE_OLD
#define JUMP_ZONE_GATE_OLD 0
#endif
int carJumpZoneGateOld = JUMP_ZONE_GATE_OLD;
static float zoneAlong(const ColZone *v, float fx, float fz) {   // полудлина объёма вдоль курса (fx, fz) в плане
  if (!v->shape) return v->h[0];
  const float *A[3] = {v->ax, v->ay, v->az}; float e = 0.f;
  for (int k = 0; k < 3; k++) e += v->h[k] * fabsf(A[k][0] * fx + A[k][2] * fz);
  return e;
}
// Правило только там, где запись PS2 его подтверждает (дирижёр 27.10): L5/L2 «L2_jumpzone» (H8-2, ps2_h82_v20a/v25a) — да. L1 jumpzone3 (J9-5, ps2_jz3_*, 50/70/84 км/ч): запись PS2 правило не подтвердила
// (скорость на кромке на 84 км/ч и газом у прежнего правила ближе: +4,2 и +6,5 против +4,9 и +9,6 км/ч у нового), зона исключена. Остальные зоны без записи PS2 — тоже исключены; расширить список — только с записью
static int zoneGateConfirmed(const ColZone *v) { return !strcmp(v->name, "L2_jumpzone"); }
static void jumpZoneDrive(Car *c, const ColWorld *w, float thr, float dt) {
  if (carJumpBoost || carJumpZoneDriveOff || carJumpBoostOff || !w->nzone || thr <= 0.f || c->wheels <= 0) return;
  float fx = sinf(c->h), fz = cosf(c->h);
  uint32_t i = 0;
  for (; i < w->nzone; i++) {
    float grow = c->p.halfL;
    if (!carJumpZoneGateOld && zoneGateConfirmed(&w->zone[i])) { float e = zoneAlong(&w->zone[i], fx, fz); if (e < JUMP_ZONE_MIN_ALONG) grow += JUMP_ZONE_MIN_ALONG - e; }
    if (colZoneContains(&w->zone[i], c->x, c->y + .5f, c->z, grow)) break;
  }
  if (i == w->nzone) return;
  float a = JUMP_ZONE_ACC * thr * dt;
  c->vel[0] += fx * a; c->vel[2] += fz * a;
  c->boosts++; c->boostZone = (int)i;   // подшаги разгона зоны (прежнее правило 47 — отрывы с подбросом)
}
#ifndef CAR_CRASH_POP
#define CAR_CRASH_POP 0
#endif
#ifndef CAR_LANDING_RESET_OLD
#define CAR_LANDING_RESET_OLD 0
#endif
int carCrashPopOld = CAR_CRASH_POP;   // правка 346: 1 — подброс при ударе краша, как до правки (красный tests/pockets)
static void jumpBoost(Car *c, const ColWorld *w) {
  if (!carJumpBoost || carJumpBoostOff || !w->nzone || c->vel[1] <= JUMP_BOOST_VY) return;
  if (c->vel[0] * c->vel[0] + c->vel[2] * c->vel[2] < JUMP_BOOST_V * JUMP_BOOST_V) return;
  int z = colJumpZoneAt(w, c->x, c->y + .5f, c->z); if (z < 0) return;
  float dh = colZoneTop(&w->zone[z]) - c->y; if (dh <= 0.f) return;
  float vy = sqrtf(2.f * c->p.gravity * dh);
  if (vy > c->vel[1]) { c->vel[1] = vy; c->boosts++; c->boostZone = z; }
}

static void settle(Car *c, const ColWorld *w) {
  for (int it = 0; it < 4; it++) {
    CarBox b; carBox(c, &b); float mx, mz, nx, nz, g;
    colPushCar(w, &b, &mx, &mz, &nx, &nz);
    if (colGround(w, c->x + mx, c->z + mz, c->y + 2.f, -1e6f, &g, 0) && !crossAt(w, &b, c->x, c->z, c->x + mx, c->z + mz) &&
        !crossAt(w, &b, b.x - mx, b.z - mz, b.x, b.z)) { c->x += mx; c->z += mz; }
    float yT, gx, gz; int n = restOn(c, w, c->h, &yT, &gx, &gz);
    if (n) { c->y = yT; c->gx = gx; c->gz = gz; orientQ(c->q, c->h, gx, gz); c->grounded = 1; c->wheels = n; }
  }
}

void carPlace(Car *c, const ColWorld *w, float x, float y, float z, float h) {
  c->x = x; c->z = z; c->h = h; c->gx = c->gz = 0; c->steer = 0; c->slip = 0;
  memset(c->vel, 0, sizeof c->vel); memset(c->w, 0, sizeof c->w);
  memcpy(c->com, c->comBase, sizeof c->com);
  quatYaw(c->q, h);
  float g;
  c->y = colGround(w, x, z, y + 2.f, -1e6f, &g, 0) ? g : y;
  c->grounded = 0; settle(c, w);
  for (int i = 0; i < 4; i++) c->comp[i] = c->eq;
  syncOut(c);
  c->ry = c->y; c->rvy = 0; c->rpitch = c->pitch; c->rroll = c->roll;
  c->quietSteps = 0; c->airT = c->noGroundT = c->embedT = c->bellyT = c->flipT = c->nofitT = 0; c->fallPending = 0; c->fallPendT = 0; c->fallTop = c->y; c->fallT0 = c->t;
  c->flightZone = -1;
  c->acc = 0; c->contacts = 0; c->hitSpeed = 0; c->pen = 0; c->hardV = c->foliV = c->carHitV = 0;   // телепорт: без удара
  c->nhkeep = 0; c->nfkeep = 0; c->nbkeep = 0;                                                                                    // и без памяти горбов
}

void carResetNearby(Car *c, const ColWorld *w) {
  float x = c->x, y = c->y + 1.f, z = c->z, h = c->h;
  // Б2 28.09 (миссионер, тупик m0): новейшее надёжное место не ближе RESET_AWAY к нынешнему; более новые забываются
  while (c->nsafe > 0) {
    int k = c->safeHead; float d = hypotf(c->safe[k].x - c->x, c->safe[k].z - c->z);
    x = c->safe[k].x; y = c->safe[k].y; z = c->safe[k].z; h = c->safe[k].h;
    c->nsafe--; c->safeHead = (k - 1 + CAR_SAFE) % CAR_SAFE;
    if (d >= RESET_AWAY || c->nsafe == 0) break;
  }
  carPlace(c, w, x, y, z, h);
  c->safeT = c->t;
}

void carInit(Car *c, const CarParams *p, const ColWorld *w, float x, float y, float z, float h) {
  memset(c, 0, sizeof *c); c->boostZone = -1; c->hitEpAge = 99.f;
  if (p) c->p = *p; else carDefaults(&c->p);
  derive(c);
  carPlace(c, w, x, y, z, h);
  c->homeX = c->x; c->homeY = c->y; c->homeZ = c->z; c->homeH = c->h;
  c->safeT = -1e9f;
}

void carSetSpeed(Car *c, float v) {
  float q[4]; memcpy(q, c->q, sizeof q); M3 M = quatM(q);
  c->vel[0] = M.F.x * v; c->vel[1] = M.F.y * v; c->vel[2] = M.F.z * v; memset(c->w, 0, sizeof c->w);
  syncOut(c);
}
float carModelLift(const Car *c,float storedLift) { return storedLift+c->modelRaise; }
void carAnchorVelocity(const Car *c,float velocity[3]) {
  M3 M=quatM(c->q);
  V3 offset=mulM(&M,v3(c->com[0],c->com[1],c->com[2]));
  V3 v=sub(v3(c->vel[0],c->vel[1],c->vel[2]),cross(v3(c->w[0],c->w[1],c->w[2]),offset));
  velocity[0]=v.x;velocity[1]=v.y;velocity[2]=v.z;
}

// вернуть на надёжное место не новее, чем before (сек. времени машины); более новые места забываются
static void recover(Car *c, const ColWorld *w, float before) {
  int pick = -1, age = 0;
  for (int i = 0; i < c->nsafe; i++) {
    int k = (c->safeHead - i + CAR_SAFE) % CAR_SAFE;
    if (c->safe[k].t <= before) { pick = k; age = i; break; }
  }
  if (pick < 0 && c->nsafe) { age = c->nsafe - 1; pick = (c->safeHead - age + CAR_SAFE) % CAR_SAFE; }
  float x = c->homeX, y = c->homeY, z = c->homeZ, h = c->homeH;
  if (pick >= 0) {
    x = c->safe[pick].x; y = c->safe[pick].y; z = c->safe[pick].z; h = c->safe[pick].h;
    c->nsafe -= age + 1; c->safeHead = (pick - 1 + CAR_SAFE) % CAR_SAFE;   // и само место — чтобы не вернуться туда же снова
  }
  float t = c->t; int rec = c->recoveries, rev = c->reverts;
  carPlace(c, w, x, y, z, h);
  c->t = t; c->recoveries = rec + 1; c->reverts = rev; c->safeT = t;
}

// выходные поля для main.c / тестов из твёрдого тела
static void syncOut(Car *c) {
  float q[4]; memcpy(q, c->q, sizeof q); M3 M = quatM(q);
  float fx, fz; fwdH(&M, c->h, &fx, &fz);
  c->h = atan2f(fx, fz);
  V3 vel = v3(c->vel[0], c->vel[1], c->vel[2]);
  c->v = vel.x * fx + vel.z * fz; c->vl = -vel.x * fz + vel.z * fx; c->vy = vel.y;   // вправо = (-fz, fx)
  floorOf(&M, &c->gx, &c->gz);
  c->pitch = asinf(clampf(M.F.y, -1.f, 1.f)); c->roll = asinf(clampf(-M.L.y, -1.f, 1.f));
}

// ------------------------------------------------------------------------------------------------ контакты
typedef struct { V3 n, r; float vn0, jn, K, e; } Ct;

// последовательные импульсы по контактам (нормаль + трение); pop — подброс при сильном ударе о стену; flat — трение
// только горизонтальное (о стену: скольжение вдоль, без «подъёма» трением)
static void solve(Car *c, const M3 *M, V3 *vel, V3 *om, Ct *ct, int n, float mu, float popK, float popMax, V3 popR, int flat) {
  float im = 1.f / bodyMass(c);
  for (int k = 0; k < n; k++) ct[k].K = effMass(c, M, ct[k].n, ct[k].r);
  for (int it = 0; it < 4; it++)
    for (int k = 0; k < n; k++) {
      Ct *q = &ct[k];
      float vn = dot(add(*vel, cross(*om, q->r)), q->n);
      float target = q->vn0 < -1.f ? -q->e * q->vn0 : 0.f;
      float nj = fmaxf(q->jn + (target - vn) * q->K, 0.f), dj = nj - q->jn; q->jn = nj;
      *vel = madd(*vel, q->n, dj * im); *om = add(*om, invIw(c, M, cross(q->r, scl(q->n, dj))));
    }
  float jsum = 0;
  for (int k = 0; k < n; k++) {
    Ct *q = &ct[k]; jsum += q->jn;
    V3 vp = add(*vel, cross(*om, q->r)); V3 vt = madd(vp, q->n, -dot(vp, q->n));
    if (flat) vt.y = 0.f;
    float ts = len(vt);
    if (ts < 0.01f || q->jn <= 0.f) continue;
    V3 t = scl(vt, 1.f / ts); float Kt = effMass(c, M, t, q->r), jt = fminf(mu * q->jn, ts * Kt);
    *vel = madd(*vel, t, -jt * im); *om = add(*om, invIw(c, M, cross(q->r, scl(t, -jt))));
  }
  if (popK > 0.f && jsum > 0.f) {
    float jv = fminf(popK * jsum, popMax * bodyMass(c));
    V3 J = v3(0, jv, 0);   // ближе к центру масс, чем точка удара: подброс, а не «на дыбы»
    *vel = madd(*vel, J, im); *om = add(*om, invIw(c, M, cross(scl(popR, 0.3f), J)));
  }
}

// Правка Роба 176 (01.10: «машины трафика на скорости — сразу горелый каркас, даже не взрываются»). Оракул PS2 (близнец В14,
// knowledge/facts/ps2-traffic-hit.yaml, ступень 4): минивэн (HitPoints 1.0) от удара в бок на 20 и 35 м/с цел — ни дыма, ни огня
// 20 с; у порта (скорость − 2) × 0.0255 / HitPoints разбивал его с ~41 м/с одним ударом. Урон машина-машина × CAR_HIT_DMG_K
// (правило порта: 35 м/с — 0.43 урона минивэну, без дыма; разбить — 3 таких удара; сколько точно — ждёт замера близнеца)
#ifndef CAR_HIT_DMG_K
#define CAR_HIT_DMG_K 0.45f
#endif
// Правки Роба 164/187 (невидимые стены, «швыряет в разные стороны»): близнец В22 (knowledge/facts/ps2-jumps-walls.yaml, свой
// PCSX2, кадры по 50/с): при сильном ударе о твёрдое оригинал оставляет машине 11–15 % скорости за 6–8 кадров и не отбрасывает:
// J11-9 бок цистерны 21,7 -> 2,4 м/с (+0,3 м отката), башня у моста F12-6 18,2 -> 2,7, сундук C7-6 16 -> 1. У порта удар упругий
// (0,42) с подбросом — машину задирало носом на 64–79°, разворачивало и сносило вбок на 3–6 м. Правило: нормальная скорость удара
// выше CAR_CRASH_V — горизонтальная скорость × k (CAR_CRASH_KEEP при CAR_CRASH_V2 и сильнее, плавно к 1 у CAR_CRASH_V), вращение ×
// CAR_CRASH_OM (так же плавно). Слабые касания (скольжение вдоль стены, бордюр) — прежние. Гейт tests/airwalls/crash.sh
#ifndef CAR_CRASH_V
#define CAR_CRASH_V 8.f
#endif
#ifndef CAR_CRASH_V2
#define CAR_CRASH_V2 12.f
#endif
#ifndef CAR_CRASH_KEEP
#define CAR_CRASH_KEEP 0.13f
#endif
#ifndef CAR_CRASH_OM
#define CAR_CRASH_OM 0.25f
#endif
// Прочность машины игрока о стены (аудит 09.10, п. 6): у PS2 (близнец tools/twins/boom.py, build/twins/o4/v199_boom) famil_v
// (HitPoints 3,0) разбита 12-м ударом в изгородь и задом в стену (13,9–20,4 м/с); тот же бот в порту (tests/cars/boom_ps2.c)
// разбивал её 9-м ударом. Множитель удара о мир подогнан под запись: разбита 12-м ударом при 0,019 на м/с (было 0,0255,
// 0,0085 × 3 — правило порта). Машина-машина и посадка — прежние (своя сверка: ps2-traffic-hit, CAR_HIT_DMG_K).
// Прежнее: -DCAR_WALL_DMG_OLD=1
#ifndef CAR_WALL_DMG_OLD
#define CAR_WALL_DMG_OLD 0
#endif
#ifndef CAR_REVERT_DMG_OLD
#define CAR_REVERT_DMG_OLD 0   // 1 — удар с отменой подшага урона не даёт (как было)
#endif
#ifndef LAND_DAMAGE_OLD
#define LAND_DAMAGE_OLD 0
#endif
#ifndef LAND_NY
#define LAND_NY 0.7f
#endif
static int carDmgGroundCall; float carDmgSrc[3];   // служебное (хост-гейт посадки): накопленный урон по источникам: 0 днище/крыша об опору, 1 стена/предмет, 2 машина
static void addDamageK(Car *c, float closing, float thr, float k) {
  if (closing <= thr) return;
  carDmgSrc[1] += (closing - thr) * k / fmaxf(0.1f, c->p.hitPoints);
  c->damage += (closing - thr) * k / fmaxf(0.1f, c->p.hitPoints);
  c->lastHit = closing;
  if (c->damage >= 1.f) { c->damage = 1.f; c->destroyed = 1; }
}
static void addDamage(Car *c, float closing, float thr) {
  if (closing <= thr) return;
  carDmgSrc[carDmgGroundCall ? 0 : 2] += (closing - thr) * 0.0085f * 3.f / fmaxf(0.1f, c->p.hitPoints);
  c->damage += (closing - thr) * 0.0085f * 3.f / fmaxf(0.1f, c->p.hitPoints);
  c->lastHit = closing;
  if (c->damage >= 1.f) { c->damage = 1.f; c->destroyed = 1; }
}

// точка удара на прямоугольнике кузова: угол, глубже всех заходящий в препятствие; почти лобовой/боковой удар —
// ближе к середине грани (плоский удар не крутит)
static void hitPoint(const CarBox *b, float nx, float nz, float *px, float *pz) {
  float fx = b->fx, fz = b->fz, rx = -fz, rz = fx;
  float a = nx * fx + nz * fz, s = nx * rx + nz * rz;       // нормаль в осях кузова: вдоль и вправо
  float sa = a > 0 ? 1.f : -1.f, ss = s > 0 ? 1.f : -1.f;
  float wa = 1.f, ws = 1.f;
  if (fabsf(a) >= fabsf(s)) ws = clampf(fabsf(s) / (fabsf(a) * 0.3f), 0.f, 1.f);   // перёд/зад: сдвиг вбок до угла
  else wa = clampf(fabsf(a) / (fabsf(s) * 0.3f), 0.f, 1.f);                       // бок: сдвиг вдоль до угла
  *px = b->x - fx * b->hl * sa * wa - rx * b->hw * ss * ws;
  *pz = b->z - fz * b->hl * sa * wa - rz * b->hw * ss * ws;
}

// где именно удар: colPushCar даёт только нормаль и глубину. Грань кузова, в которую смотрит препятствие, режем на
// 4 тонкие полосы (0.6 м вглубь) и пробуем каждую colProbeCar в положении до выталкивания: точка удара — среднее
// центров задетых полос по глубине (столб под правой фарой — удар справа, стена под углом — в угол). Ничего не
// задето (всё глубже 0.6 м) — угол по нормали (hitPoint).
static void hitSlices(const ColWorld *w, const CarBox *b0, float nx, float nz, float *px, float *pz) {
  float fx = b0->fx, fz = b0->fz, rx = -fz, rz = fx, t = 0.6f;
  float a = nx * fx + nz * fz, s = nx * rx + nz * rz, wsum = 0.f, osum = 0.f;
  int front = fabsf(a) >= fabsf(s);
  float sg = front ? (a < 0.f ? 1.f : -1.f) : (s < 0.f ? 1.f : -1.f);   // грань: +перёд/-зад или +право/-лево
  float cx, cz, ux, uz, half;                                          // центр полосы грани, ось вдоль грани
  // полосы выступают за грань на 5 см: касание вскользь (заход на доли миллиметра) тоже находится
  if (front) { cx = b0->x + fx * sg * (b0->hl + 0.05f - t * .5f); cz = b0->z + fz * sg * (b0->hl + 0.05f - t * .5f); ux = rx; uz = rz; half = b0->hw; }
  else { cx = b0->x + rx * sg * (b0->hw + 0.05f - t * .5f); cz = b0->z + rz * sg * (b0->hw + 0.05f - t * .5f); ux = fx; uz = fz; half = b0->hl; }
  for (int j = 0; j < 4; j++) {
    float o = (j - 1.5f) * 0.5f * half;
    CarBox sb = *b0; sb.x = cx + ux * o; sb.z = cz + uz * o;
    if (front) { sb.hw = half * .25f; sb.hl = t * .5f; } else { sb.hw = t * .5f; sb.hl = half * .25f; }
    colProbeCar(w, &sb);
    float d = fminf(sb.maxDepth, t);
    if (d > 1e-3f) { wsum += d; osum += d * o; }
  }
#ifdef COL_DEBUG
  if (colDebug) printf("  hitSlices n %.3f %.3f front %d wsum %.4f osum %.4f\n", nx, nz, front, wsum, osum);
#endif
  if (wsum <= 0.f) { hitPoint(b0, nx, nz, px, pz); return; }
  float o = osum / wsum;
  if (front) { *px = b0->x + fx * sg * b0->hl + ux * o; *pz = b0->z + fz * sg * b0->hl + uz * o; }
  else { *px = b0->x + rx * sg * b0->hw + ux * o; *pz = b0->z + rz * sg * b0->hw + uz * o; }
}

// ------------------------------------------------------------------------------------------------ подшаг
// A spherical tyre touches the ground plane when the centre's normal
// distance is its radius. Project both that distance and the radius onto
// the suspension axis; subtracting radius after division sinks tilted tyres.
// Callers require axisNormal >= .4 before using this projection.
static float suspensionExtension(float planeDistance, float axisNormal, float radius) {
#ifdef CAR_TYRE_PROJECTION_OLD
  return planeDistance / axisNormal - radius;
#else
  return (planeDistance - radius) / axisNormal;
#endif
}
// Lateral collision uses each authored sphere. The mesh-derived averaged
// radius/axle proxy can differ by 20 cm on vehicles with unequal wheels.
// The sphere follows the current wheel pose: suspension travel, then its
// authored offset from the joint pivot (up to 1.7 cm) rolled about the axle
// and turned with the physics steering angle of the front wheels.
static V3 wheelColliderLocal(const Car *c, int i, float *radius) {
  const CarParams *p=&c->p;
  float compression=clampf(c->comp[i],0.f,c->lim);
#ifndef CAR_WHEEL_COLLISION_PROXY_OLD
  if (p->wheelCollision[i][3]>0.f) {
    const float *s=p->wheelCollision[i],*o=p->wheelCollisionOffset[i];*radius=s[3];
    float x=o[0],y=o[1],z=o[2];
#ifndef CAR_WHEEL_COLLIDER_RIGID_OLD
    float a=i<2?c->wheelSteer:0.f,ca=cosf(a),sa=sinf(a),cb=cosf(c->wheelSpin[i]),sb=sinf(c->wheelSpin[i]);
    float yr=y*cb-z*sb,zr=y*sb+z*cb;            // roll about the axle (body X)
    x=o[0]*ca+zr*sa;y=yr;z=zr*ca-o[0]*sa;       // then steer about body Y, as the tyre force direction
#endif
    return v3(s[0]-o[0]+x,s[1]-o[1]+y+carModelLift(c,p->modelLift)+p->suspensionYOffset+compression,s[2]-o[2]+z);
  }
#endif
  *radius=p->wheelRad;
  return v3((i&1)?-p->wheelX:p->wheelX,c->mountY-c->lim+compression,i<2?p->wheelF:-p->wheelR);
}
static void substep(Car *c, const ColWorld *w, const CarInput *in, float dt) {
  const CarParams *P = &c->p;
  c->t += dt;
  c->foliageCooldown = fmaxf(0.f, c->foliageCooldown - dt);
  colSetFoliagePass(CAR_FOLIAGE_PASS);   // правка 173: счёт листвы этого подшага с нуля
  colHumpKeep(c->hkeep, c->nhkeep);      // правка 200: горбы, над которыми кузов был на прошлом подшаге
  colFolKeep(c->fkeep, c->nfkeep);       // правка 304: заборы-изгороди, пропущенные на прошлом подшаге
  colSetFenceBack(1);                    // сверка PS2 09.10: забор держит только с лица (col.h)
  colBackKeep(c->bkeep, c->nbkeep);      // и заборы, через которые кузов выезжает с изнанки
  const int wasAir = !c->grounded; const float vyIn = c->vel[1];   // для посадки на куст (конец подшага)
  const float m = bodyMass(c), mg = m * P->gravity, im = 1.f / m;
  float st = clampf(in->steer, -1.f, 1.f), thr = clampf(in->throttle, 0.f, 1.f), brk = clampf(in->brake, 0.f, 1.f);
  c->brkLit = (brk > 0.f && c->v > .3f) ? 1.f : 0.f;   // №465: стоп-сигналы у машин трафика (после удара) и миссий, как carvis.c у машины игрока
  int hb = in->handbrake > 0.5f;
  c->steer += (st - c->steer) * fminf(1.f, dt * P->steerResp);

  V3 pos = v3(c->x, c->y, c->z), vel = v3(c->vel[0], c->vel[1], c->vel[2]), om = v3(c->w[0], c->w[1], c->w[2]);
  M3 M = quatM(c->q);
  const V3 pos0 = pos; const M3 M0 = M; float q0[4]; memcpy(q0, c->q, sizeof q0);
  const float h0 = c->h;

  float speed = len(vel), along = dot(vel, M.F), ratio = fminf(1.f, speed / P->topSpeed);
  // центр масс: вилли — полный газ на малой скорости сдвигает его (назад: нос поднимается); точка тела при сдвиге
  // сохраняет скорость: v_cm += w x dr
  if(P->bodyMass<=0.f) {
    int wh = P->wheelieRange > 0.f && thr > 0.99f && ratio < P->wheelieRange && c->wheels >= 2;
    float tgt[3] = {c->comBase[0], c->comBase[1] + (wh ? P->wheelieY : 0.f), c->comBase[2] + (wh ? P->wheelieZ : 0.f)};
    if(P->bodyMass>0.f)for(int i=0;i<4;i++)tgt[1]+=P->wheelCentreWeight[i]*(c->comp[i]-c->eq);
    float k = fminf(1.f, dt * 4.f); V3 d;
    d.x = (tgt[0] - c->com[0]) * k; d.y = (tgt[1] - c->com[1]) * k; d.z = (tgt[2] - c->com[2]) * k;
    if (d.x != 0.f || d.y != 0.f || d.z != 0.f) {
      c->com[0] += d.x; c->com[1] += d.y; c->com[2] += d.z;
      vel = add(vel, cross(om, mulM(&M, d)));
    }
  }
  V3 comB = v3(c->com[0], c->com[1], c->com[2]);
  V3 comW = add(pos, mulM(&M, comB));

  // ---- 1) силы
  V3 Fs = v3(0.f, -mg, 0.f), Ts = v3(0, 0, 0);
  // «неваляшка»: вес приложен ниже центра масс -> момент, возвращающий ось вверх к вертикали
  // №406 (Роб 09.10, PSP: «машины в поворотах встают на два колеса и переворачиваются»): «неваляшка» .con (SetWeebleOffset, PS2
  // famil_v −0,85) и на колёсах (прошлый подшаг), как в прежнем порту; в воздухе у родной — сдвиг центра масс (PS2 −0,95), не дважды.
  // Прежнее: -DCAR_WEEBLE_GROUND_OLD=1
#ifndef CAR_WEEBLE_GROUND_OLD
#define CAR_WEEBLE_GROUND_OLD 0
#endif
  // Правка Роба 353 (09.10, «нос в конце полёта макает вниз — как у PS2»): в полёте у PS2 тангаж возвращается к горизонту пружиной.
  // Подгонка θ'' = −k·sinθ − c·θ' по 13 свободным полётам записей (12 прыжков pitch_ps2.tsv агента 353 и ps2_v103_house100_coast после
  // касания конька: нос −25° → 0 за 0,64 с без опоры): k 3,5–17 (медиана 7,8) 1/с², c 0,1–4,6 1/с. «Неваляшка» диска (SetWeebleOffset
  // famil_v −0,85, масса и тензор диска) даёт k = 0,85·16·1500/3813 = 5,35 — в этом разбросе; у порта в полёте её не было (только
  // затухание airLevel), и нос после касания конька не возвращался (jz17 100 км/ч: −54° к посадке, у PS2 −25° → 0).
  // Теперь в полёте (колёса без опоры) — та же «неваляшка», что на колёсах, и затухание качания в полёте — критическое для неё (блок «качание»
  // ниже, вместе с angDamp). Затухание — правило порта: ни одна доля от 0 до 2 не даёт сразу и 12 прыжков, и дом (tests/jumps/house.sh): слабее
  // ×1,4 прежнего машина на 80–90 км/ч подлетает к скату носом вниз и падает во двор перед домом (у PS2 нос −1,4°/+5° — висит на скате
  // / переползает конёк), сильнее ×1,6 — снова гасит нос прыжков; критическое (×1,49) — внутри окна. Прежнее: -DCAR_AIR_PITCH_OLD=1
#ifndef CAR_AIR_PITCH_OLD
#define CAR_AIR_PITCH_OLD 0
#endif
  // только свободный полёт: без опоры колёсами и днищем и без стен дольше CAR_AIR_MIN_T (подскок на горбе, удар о стену — не полёт:
  // tests/humps G)
#ifndef CAR_AIR_MIN_T
#define CAR_AIR_MIN_T .2f
#endif
  const int airWeeble = !CAR_AIR_PITCH_OLD && !CAR_WEEBLE_GROUND_OLD && P->weebleOffset < 0.f && !c->grounded && c->contacts == 0 && c->airT >= CAR_AIR_MIN_T;
  if ((CAR_WEEBLE_GROUND_OLD ? P->bodyMass<=0.f : P->bodyMass<=0.f || c->wheels > 0 || airWeeble) && P->weebleOffset < 0.f) Ts = add(Ts, cross(scl(M.U, P->weebleOffset), v3(0.f, -mg, 0.f)));
  float wheelAngle = -c->steer * P->maxWheelTurn * (1.f - P->hsSteeringDrop * ratio);
  c->wheelSteer = wheelAngle;
  // Rolling angle for the tyre collider, same rule as the drawn wheels (carvis.c):
  // locked rear on the handbrake, spinning rear in a burnout.
  for (int i = 0; i < 4; i++) {
    float r = P->wheelCollision[i][3] > 0.f ? P->wheelCollision[i][3] : P->wheelRad, wi = c->v / r;
    if (i >= 2 && c->slip == 2) wi = 0.f;
    if (i >= 2 && c->slip == 1) wi = (c->v >= 0.f ? 1.f : -1.f) * fmaxf(fabsf(wi), 25.f);
    c->wheelSpin[i] = fmodf(c->wheelSpin[i] + wi * dt, 6.28318531f);
  }
  int ng = 0, nw = 0, nd = 0; float maxTyre = 0.f;   // колёс с опорой (с продолжением земли за краем мира) / на настоящей земле
  float share = m * .25f / dt;      // сила, гасящая скорость колеса за подшаг (доля массы на колесо)
  int burn = ratio < P->burnoutRange && thr > 0.99f;
  // лучи колёс; колесо над пустотой (под ним нет земли вообще — край мира) опирается на продолжение земли на
  // высоте соседних колёс: край мира — стена и пол, машина не свешивается и не опрокидывается за него
  V3 mws[4], gn[4]; float gys[4]; uint32_t groundRef[4]={~0u,~0u,~0u,~0u}; int hit[4], vd[4], wmat[4] = {0, 0, 0, 0}, onsup[4] = {0, 0, 0, 0}, nreal = 0; float gmax = -3.0e38f;
  for (int i = 0; i < 4; i++) {
    c->comp[i] = 0.f; hit[i] = vd[i] = 0;
    if (M.U.y < 0.3f) continue;   // на боку / на крыше колёса не достают
    float bx = (i & 1) ? -P->wheelX : P->wheelX, bz = i < 2 ? P->wheelF : -P->wheelR, n3[3], g2;
    mws[i] = add(pos, mulM(&M, v3(bx, c->mountY, bz)));
    float reach = (c->lim + P->wheelRad) / M.U.y + 0.05f;
    int got = colGroundMatId(w, mws[i].x, mws[i].z, mws[i].y + 0.05f, mws[i].y - reach, &gys[i], n3, &wmat[i], &groundRef[i]); float gs;
    if (supportAt(c, mws[i].x, mws[i].z, mws[i].y + 0.05f, mws[i].y - reach, &gs) && (!got || gs > gys[i])) {
      got = 1; groundRef[i]=~0u; gys[i] = gs; n3[0] = 0.f; n3[1] = 1.f; n3[2] = 0.f; wmat[i] = COL_MAT_HARD; onsup[i] = 1;   // крыша машины
    }
#ifndef STEP_SEAM_OLD
#define STEP_SEAM_OLD 0   // №607 (L3 C7-4 причал): 1 — прежнее, нормаль опоры колеса как есть
#endif
#ifndef STEP_SEAM_NY
#define STEP_SEAM_NY 0.95f
#endif
#ifndef STEP_SEAM_PROBE
#define STEP_SEAM_PROBE 0.7f
#endif
    // №607: короткая ступень-шов в ровной земле (ступенька 0,2–0,4 м с крутой гранью n.y 0,7–0,95 между двумя ровными площадками, шов причала
    // L3 C7-4): грань как нормаль опоры кренит силу пружины колеса вперёд-назад и «выбивает» машину. Склон (по обе стороны от точки
    // земля не ровная на расстоянии STEP_SEAM_PROBE вдоль уклона) остаётся склоном: нормаль как есть
    if (got && !STEP_SEAM_OLD && !onsup[i] && n3[1] > COL_WALK_NY && n3[1] < STEP_SEAM_NY) {
      float hx = n3[0], hz = n3[2], hl = sqrtf(hx * hx + hz * hz);
      if (hl > 1e-3f) {
        hx /= hl; hz /= hl; float ga, gb, na[3], nb[3];
        int oa = colGround(w, mws[i].x + hx * STEP_SEAM_PROBE, mws[i].z + hz * STEP_SEAM_PROBE, gys[i] + 0.6f, gys[i] - 0.6f, &ga, na);
        int ob = colGround(w, mws[i].x - hx * STEP_SEAM_PROBE, mws[i].z - hz * STEP_SEAM_PROBE, gys[i] + 0.6f, gys[i] - 0.6f, &gb, nb);
        if (oa && ob && na[1] >= STEP_SEAM_NY && nb[1] >= STEP_SEAM_NY) { n3[0] = 0.f; n3[1] = 1.f; n3[2] = 0.f; }
      }
    }
    if (got) {
      hit[i] = 1; gn[i] = v3(n3[0], n3[1], n3[2]); nreal++; if (gys[i] > gmax) gmax = gys[i];
    } else vd[i] = !colGround(w, mws[i].x, mws[i].z, mws[i].y + 0.05f, -1e6f, &g2, 0);
  }
  for (int i = 0; i < 4; i++) if (vd[i] && nreal) { hit[i] = 1; gys[i] = gmax; gn[i] = v3(0.f, 1.f, 0.f); }
#ifndef CAR_STEPLOSS_OLD
#define CAR_STEPLOSS_OLD 0   // №164 (L1 C7-6): 1 — прежнее, на въезде на крутой скат (ступени, трамплин) скорость не теряется на трение удара
#endif
#ifndef CAR_STEPLOSS_MU
#define CAR_STEPLOSS_MU 0.2f   // PS2: 28 -> 23,4 м/с на скате ~20° (cos 20° даёт 26,3; остальное — трение μ·v·sin Δθ, μ ≈ 0,2 с учётом подвески)
#endif
#ifndef CAR_STEPLOSS_DSTEP
#define CAR_STEPLOSS_DSTEP 0.12f   // рад за подшаг (7°): наклон земли под колёсами меняется скачком (ступень, обрыв, кромка), не плавно (ровный трамплин дома J10 — 0,08)
#endif
#ifndef CAR_STEPLOSS_THR
#define CAR_STEPLOSS_THR 0.14f   // рад (8°): наклон земли убежал от своего сглаженного (0,4 с) значения — скат, а не пологий подъём
#endif
  if (!CAR_STEPLOSS_OLD) {
    int nh = 0; V3 na = v3(0.f, 0.f, 0.f);
    for (int i = 0; i < 4; i++) if (hit[i] && !vd[i] && !onsup[i]) { na = add(na, gn[i]); nh++; }
    float nl = nh >= 3 ? sqrtf(dot(na, na)) : 0.f, fl = sqrtf(M.F.x * M.F.x + M.F.z * M.F.z);
    if (nl > 1e-3f && fl > 1e-3f) {
      float fx = M.F.x / fl, fz = M.F.z / fl, th = atan2f(-(na.x * fx + na.z * fz), na.y);
      if (!c->stInit) { c->stInit = 1; c->stPitch = c->stThPrev = th; }
      float sp = sqrtf(vel.x * vel.x + vel.z * vel.z), dstep = th - c->stThPrev;
      if (dstep > CAR_STEPLOSS_DSTEP && th - c->stPitch > CAR_STEPLOSS_THR && sp > 8.f && vel.x * fx + vel.z * fz > 0.f) {
        float loss = fminf(CAR_STEPLOSS_MU * sp * dstep, 0.1f * sp);
        vel.x -= vel.x / sp * loss; vel.z -= vel.z / sp * loss;
      }
      c->stPitch += (th - c->stPitch) * fminf(1.f, dt * 2.5f); c->stThPrev = th;
    }
  }
  if(P->bodyMass>0.f) {
    int supported=0;float compression[4]={0};
    for(int i=0;i<4;i++)if(hit[i]) {
      float un=dot(M.U,gn[i]);if(un<.4f)continue;
      float ext=suspensionExtension((mws[i].y-gys[i])*gn[i].y,un,P->wheelRad);
      if(c->lim-ext>0.f) { compression[i]=fminf(c->lim,c->lim-ext);supported++; }
    }
    om=changeBodyTensor(c,&M,compression,om);
    int wh=P->wheelieRange>0.f && thr>.99f && ratio<P->wheelieRange && supported>=2;
    float tgt[3]={c->comBase[0],c->comBase[1],c->comBase[2]};
    for(int i=0;i<4;i++)tgt[1]+=P->wheelCentreWeight[i]*(compression[i]-c->eq);
    if(!supported)tgt[1]+=P->weebleOffset; // observed cmOffsetY in PS2: -0.1 -> -0.95 airborne, restored on landing
    if(wh) { tgt[1]+=P->wheelieY;tgt[2]+=P->wheelieZ; }
    V3 d=v3(tgt[0]-c->com[0],tgt[1]-c->com[1],tgt[2]-c->com[2]);
    memcpy(c->com,tgt,sizeof c->com);vel=add(vel,cross(om,mulM(&M,d)));
    comB=v3(tgt[0],tgt[1],tgt[2]);comW=add(pos,mulM(&M,comB));
  }
#ifndef CAR_WHEEL_SUPPORT_OLD
#define CAR_WHEEL_SUPPORT_OLD 0
#endif
  if(!CAR_WHEEL_SUPPORT_OLD) {
    // A real wheel contact with a walkable crown is physical support. Keep
    // that identity through the body's collision queries; a body-origin
    // height threshold must not turn its supporting surface into a wall.
    uint32_t keep[COL_HUMP_KEEP];int count=0;
    for(int i=0;i<4;i++)if(hit[i]&&!vd[i]&&!onsup[i]&&(groundRef[i]>>29)==COL_CYL) {
      uint32_t j=groundRef[i]&0x1fffffffu;const ColCyl *C=&w->cyl[j];
      float axial=hypotf(C->axis[0],C->axis[2]);float un=dot(M.U,gn[i]);
      float ext=un>.4f?suspensionExtension((mws[i].y-gys[i])*gn[i].y,un,P->wheelRad):c->lim;
      if(ext>=c->lim||C->pad[0]==1.f||fabsf(C->axis[1])>.3f||2*C->halfLen*axial<COL_HUMP_LEN)continue;
      int seen=0;for(int k=0;k<count;k++)seen|=keep[k]==j;
      if(!seen&&count<COL_HUMP_KEEP)keep[count++]=j;
    }
    for(int i=0;i<c->nhkeep;i++) {int seen=0;for(int k=0;k<count;k++)seen|=keep[k]==c->hkeep[i];if(!seen&&count<COL_HUMP_KEEP)keep[count++]=c->hkeep[i];}
    colHumpKeep(keep,count);
  }
  float normalForce[4]={0};
  if(!CAR_EXPLICIT_SUSPENSION_OLD) {
    V3 r[4],n[4];float spring[4]={0},tangent[4]={0},damping[4]={0},freeV[4]={0};int active[4]={0};
    V3 vfree=madd(vel,Fs,dt*im),wfree=madd(om,invIw(c,&M,Ts),dt);
    for(int i=0;i<4;i++) {
      if(!hit[i])continue;n[i]=gn[i];float un=dot(M.U,n[i]);if(un<.4f)continue;
      float ext=suspensionExtension((mws[i].y-gys[i])*n[i].y,un,P->wheelRad),x=c->lim-ext;if(x<=0.f)continue;
      active[i]=1;r[i]=sub(madd(mws[i],M.U,-ext),comW);
      V3 vp=add(vfree,cross(wfree,r[i]));if(onsup[i])vp=sub(vp,v3(c->supV[0],c->supV[1],c->supV[2]));
      freeV[i]=dot(vp,n[i]);float xs=fminf(x,c->lim);
      spring[i]=c->kS[i]*(xs+5.f*xs*xs);tangent[i]=c->kS[i]*(1.f+10.f*xs);damping[i]=c->cS[i];
      if(x>c->lim) {
        float vnI=dot(add(vel,cross(om,r[i])),n[i]),k=(vnI<0.f?60.f:CAR_BUMPSTOP_REBOUND_OLD?8.f:CAR_BUMPSTOP_REBOUND_K)*c->kS[i];
        spring[i]+=k*(x-c->lim);tangent[i]=k>0.f?k:c->kS[i]*(1.f+10.f*c->lim);damping[i]*=CAR_BUMPSTOP_DAMP;
      }
    }
    suspensionForces(c,&M,r,n,spring,tangent,damping,freeV,active,dt,normalForce);
  }
  for (int i = 0; i < 4; i++) {
    if (!hit[i]) continue;
    V3 mw = mws[i], n = gn[i]; float gy = gys[i];
    float un = dot(M.U, n); if (un < 0.4f) continue;
    float ext = suspensionExtension((mw.y - gy) * n.y, un, P->wheelRad), x = c->lim - ext;   // ход вдоль оси вверх тела
    if (x <= 0.f) continue;
    c->comp[i] = x; ng++; if (!vd[i]) nw++; if (i >= 2) nd++;
    V3 pc = madd(mw, M.U, -ext);                                      // wheel centre
    // Tyre forces act at the ground patch, not at the axle. Moving along
    // the contact normal preserves spring torque but corrects tyre torque.
    V3 r = sub(madd(pc,n,CAR_TYRE_AXLE_OLD?0.f:-P->wheelRad), comW);
    V3 vp = add(vel, cross(om, r));
    if (onsup[i]) vp = sub(vp, v3(c->supV[0], c->supV[1], c->supV[2]));   // на крыше едущей машины — относительно неё
    float xs = fminf(x, c->lim), vn = dot(vp, n);
    // пружина с прогрессией; за ходом подвески — резиновый упор: жёсткий (60k), вязкий (x12 амортизатора) и с
    // гистерезисом (на отбое держит в 7 раз слабее) — жёсткая посадка гасится, а не выстреливает машину обратно
    float fs = c->kS[i] * (xs + 5.f * xs * xs) - c->cS[i] * vn;
    if (x > c->lim) fs += (vn < 0.f ? 60.f : CAR_BUMPSTOP_REBOUND_OLD ? 8.f : CAR_BUMPSTOP_REBOUND_K) * c->kS[i] * (x - c->lim) - (CAR_BUMPSTOP_DAMP - 1.f) * c->cS[i] * vn;
    fs = clampf(fs, 0.f, 6.f * mg);
    if(!CAR_EXPLICIT_SUSPENSION_OLD)fs=normalForce[i];
    V3 f = scl(n, fs), fdrive = v3(0.f, 0.f, 0.f);   // fdrive — тяга (своё плечо ниже)
    // шина: оси колеса на земле
    V3 fw = madd(M.F, n, -dot(M.F, n)); float fl = len(fw);
    if (fl > 1e-3f) {
      fw = scl(fw, 1.f / fl);
      V3 lf = cross(n, fw);
      float da = i < 2 ? wheelAngle : 0.f, ca = cosf(da), sa = sinf(da);
      V3 fwd = add(scl(fw, ca), scl(lf, sa)), side = add(scl(lf, ca), scl(fw, -sa));
      float vlat = dot(vp, side), vlon = dot(vp, fwd), ws = fmaxf(0.01f, len(vp));
      // жёсткость вбок по режиму; множители заноса/ручника (перёд x(1+E), зад x(1-E)) — и к пределу сцепления
      float stiff = c->slip == 0 ? P->normalSteering : c->slip == 2 ? P->slipSteering : P->slipSteeringNoEBrake, gm = 1.f;
      if (c->slip == 1) gm = i < 2 ? 1.f + P->slipEffectNoEBrake : 1.f - P->slipEffectNoEBrake;
      if (hb && ratio > .02f) gm *= i < 2 ? 1.f + P->eBrakeEffect : 1.f - P->eBrakeEffect;
      stiff *= gm;
      if (c->fenceT > 0.f) stiff *= CAR_FENCE_LAT;   // №487: скользим вдоль забора — боковые силы шин не гасят скорость скольжения
      // предел сцепления по настоящей нагрузке (не меньше трети статической): подскочившее колесо скользит
      float Nl = fmaxf(mg * .25f * .35f, fminf(fs, mg)), grip = P->tireGrip * Nl * fminf(gm, 1.f), Fdrive = 0.f;
      float Flat = -m * .25f * (vlat / ws) * stiff * fminf(1.f, ratio / .15f), Flon = 0.f;
      float sgn = vlon >= 0.f ? 1.f : -1.f, cap = fabsf(vlon) * share;
      if (i >= 2) {                                                        // ведущие — задние
        float gs = c->slip ? P->slipGasScale : (ratio > P->gasSpeedThreshold ? P->hsGasScale : P->gasScale);
        Fdrive = hb ? 0.f : gs * thr * (burn ? .75f : 1.f) * m;   // тяга — мимо круга трения (как в hit-and-run-web)
        if (brk > 0.f && along <= .15f && along > -P->reverseSpeed) Flon -= brk * P->gasScale * m;   // назад
        if (hb && speed > .5f) {   // ручник: задние заблокированы — скользят, трение против всей скорости пятна
          float vs = sqrtf(vlat * vlat + vlon * vlon), fk = P->tireGrip * Nl * (1.f - P->eBrakeEffect);
          if (vs > 1e-3f) { float k = fminf(fk, vs * share) / vs; Flat = -vlat * k; Flon = -vlon * k; }
        }
        if (burn && fabsf(c->steer) > .6f) Ts = madd(Ts, M.U, -c->steer * P->donutTorque * m * (1.f - ratio) * .5f);
      }
      // №406, тормоз (09.10): у PS2 famil_v полный тормоз по прямой (запись 388, фаза brake, ○ до остановки) гасит 12,5 → 3 м/с
      // за 0,52 с и 14,1 → 3 за 0,56 с — 17–18,5 м/с²; у порта с «+3» из hit-and-run-web (BrakeScale + 3 на колесо ×0,5) 22,7–23,4.
      // Без «+3» — только BrakeScale диска. Прежнее: -DCAR_BRAKE_PLUS3_OLD=1
#ifndef CAR_BRAKE_PLUS3_OLD
#define CAR_BRAKE_PLUS3_OLD 0
#endif
      if (brk > 0.f && along > .15f) Flon -= sgn * fminf(brk * (P->brakeScale + (CAR_BRAKE_PLUS3_OLD ? 3.f : 0.f)) * m * .5f, cap);
      if (thr <= 0.f && !(brk > 0.f && along <= .15f)) {
        if (speed < 0.5f) {                                                   // стоит: держится (трение покоя, и на склоне)
          V3 gt = v3(n.x * n.y * mg, (n.y * n.y - 1.f) * mg, n.z * n.y * mg);   // скатывающая часть веса
          float restShare=share;
#if !CAR_REST_TYRE_OLD
          // Four tyre patches share translation AND angular response. The
          // old quarter-mass stop force overshoots with authored inertia.
          V3 hub=madd(r,n,P->wheelRad);const float *ii=c->invInertia;
          float tensorNorm=sqrtf(ii[0]*ii[0]+ii[3]*ii[3]+ii[5]*ii[5]+2.f*(ii[1]*ii[1]+ii[2]*ii[2]+ii[4]*ii[4]));
          if(c->wheels>=3 && !c->bodyHit && !c->contacts)
            restShare=1.f/(4.f*dt*(im+len(r)*len(hub)*tensorNorm));
#endif
          Flon = -vlon * restShare - .25f * dot(gt, fwd); Flat = -vlat * restShare - .25f * dot(gt, side);
          // Стены по PS2 (09.10, семья «PS2 сползает»): брошенная на склоне машина PS2 катится вниз — 21 постановка L1
          // (build/twins/placement/l1/orig.jsonl): на 4,9–12° через ~2,8 с 0,8–2,6 м/с, на 3,3° стоит; порт держал её на любом
          // склоне. Без тормоза и ручника вдоль колёс держит только сопротивление качению — тот же накат 0,3·m на колесо (1,2 м/с²: машина стоит до
          // ~4,3° при g 16, круче катится); поперёк — как было. С ручником — как было. Прежнее: -DCAR_SLOPE_HOLD_OLD=1
#ifndef CAR_SLOPE_HOLD_OLD
#define CAR_SLOPE_HOLD_OLD 0
#endif
          if (!CAR_SLOPE_HOLD_OLD && !hb && brk <= 0.f) Flon = -clampf(vlon * restShare + .25f * dot(gt, fwd), -.3f * m, .3f * m);
        }
        else Flon -= sgn * fminf(.3f * m, cap);                               // накат: 1.2 м/с2 на машину
      }
      // круг трения: тормоз/накат — в нём; тяга — мимо (как в hit-and-run-web: газ не буксует).
      // №406, поворот (09.10): правило hit-and-run-web «тяга съедает боковое сцепление задних колёс» снято — на полном руле с
      // газом famil_v на 14,3 м/с (средняя скорость PS2) поворачивал 86 °/с, на 14,6 — 91 °/с и уходил в занос; у PS2 на
      // 14,0–14,6 м/с 77,5 °/с (квартили 71–82,5, скольжение p90 2°; 37 участков b2_loop, b2_loop2, v28_drive_cam). Без него
      // 80 °/с, скольжение 1,5°. Прежнее: -DCAR_GAS_GRIP_OLD=1
#ifndef CAR_GAS_GRIP_OLD
#define CAR_GAS_GRIP_OLD 0
#endif
      float gl = CAR_GAS_GRIP_OLD ? sqrtf(fmaxf(grip * grip - 0.36f * Fdrive * Fdrive, 0.25f * grip * grip)) : grip;
      float ft = sqrtf(Flat * Flat + Flon * Flon);
      if (ft > gl) { Flat *= gl / ft; Flon *= gl / ft; }
      Flon += Fdrive;
      ft = sqrtf(Flat * Flat + Flon * Flon);
      if (ft > maxTyre) maxTyre = ft;
      f = add(f, add(scl(side, Flat), scl(fwd, Flon))); fdrive = scl(fwd, Fdrive);
    }
    // №406: плечо сил шины — от точки «неваляшки» (центр масс + WeebleOffset по оси вверх), а не от центра масс: у машин с
    // настоящей колеёй диска (ship 1,2 м, высокие фургоны) боковая сила из пятна под высоким центром масс клала кузов на бок
    // уже при 0,7 g (cornering.c: перевороты 78 из 178 заездов, у прежнего порта с колеёй famil_v на всех — 7). Прежнее (Fable,
    // всё в пятне): -DCAR_TYRE_LEVER_OLD=1
#ifndef CAR_TYRE_LEVER_OLD
#define CAR_TYRE_LEVER_OLD 0
#endif
    // №406, шаг 2 (09.10, сверка с записями PS2, docs/handoff/ФИЗИКА_СВЕРКА_PS2_2026-10-09.md): плечо неваляшки для ВСЕХ сил шины
    // переворачивало знак тангажа от торможения/трения (тормоз — нос вверх +0,6°, у PS2 famil_v на скорости нос вниз 0,03–0,07°
    // на м/с²) и клало famil_v в повороте ВНУТРЬ (PS2: наружу 0,20° на м/с² бокового, 9 записей 388); на коньке
    // дома jz17 машина садилась носом вверх +11° (PS2 −7,8°) и после посадки уходила в стену (35 м против 71).
    // Теперь сцепление шины (боковое и продольное трение, тормоз) входит в кузов через ступицу колеса — центр колеса, wheelRad
    // диска над пятном (подвеска держит ступицу, через неё сила шины доходит до кузова): крен наружу 0,11–0,12°/(м/с²), тормоз —
    // нос вниз 0,03°/(м/с²) (мягче PS2; полная подгонка под famil_v кладёт грузовики — см. отчёт, В102).
    // Тяга (газ, мимо круга трения) — по-прежнему от точки неваляшки: у PS2 famil_v на скорости приседания нет (−0,05°/(м/с²)),
    // у этого плеча −0,033, у пятна +0,10 (и «свечки» atv_v, oblit_v… на газу). Прежнее (406, всё от неваляшки): -DCAR_TYRE_HUB_OLD=1
#ifndef CAR_TYRE_HUB_OLD
#define CAR_TYRE_HUB_OLD 0
#endif
    Fs = add(Fs, f);
    {
      float wo = P->weebleOffset < 0.f ? -P->weebleOffset : 0.f;   // плечо от точки «неваляшки»: на wo выше пятна
      if (CAR_TYRE_LEVER_OLD) Ts = add(Ts, cross(r, f));
      else if (CAR_TYRE_HUB_OLD) Ts = add(Ts, cross(madd(r, M.U, wo), f));   // 406: по оси вверх тела, для всех сил
      // от пятна r: силы шины, кроме тяги, — на wheelRad выше по нормали n (ступица; нормальная сила вдоль n — плечо то же),
      // тяга — на wo выше пятна по той же нормали (по оси тела вместо нормали jz16_gas садится на 28,4 м при допуске от 28,5 —
      // граничный заезд, 28–30 м у всех вариантов)
      else {
        Ts = add(Ts, add(cross(r, f), add(cross(scl(n, P->wheelRad), sub(f, fdrive)), cross(scl(n, wo), fdrive))));
        // файл rollps2 рядом с EBOOT (main_loop.h): боковая сила шины приложена на carRollGain м ниже (плечо от ступицы до центра масс удлиняется на эту величину, одинаково для всех машин: крен ближе к PS2, famil_v 0,20°/(м/с²) у PS2, 0,11 у порта; пропорциональное увеличение клало marge_v на бок);
        // 0 (нет файла) — побайтно прежнее. Только боковая часть: продольная (тормоз, тяга) и тангаж не меняются
        if (carRollGain != 0.f) { V3 fl = sub(f, fdrive); V3 flat = madd(fl, M.F, -dot(fl, M.F)); float arm = fmaxf(0.f, -dot(add(r, scl(n, P->wheelRad)), n)); Ts = add(Ts, cross(scl(n, -carRollGain * (arm > 0.f ? 1.f : 0.f)), flat)); }
      }
    }
  }
  // «застряла»: кузов лежит на бордюре/заборе/краю, ведущие колёса висят — через 0.35 с тяга прикладывается к кузову
  // по горизонтали (как в hit-and-run-web: аркадное спасение; в полёте газ по-прежнему не действует)
  {
    int stranded = nd < 2 && speed < 4.f && M.U.y > 0.7f && (c->contacts > 0 || c->bodyHit || nw > 0);
    c->strandT = stranded ? fminf(1.f, c->strandT + dt) : fmaxf(0.f, c->strandT - 2.f * dt);
    if (stranded && c->strandT > 0.35f) {
      float a = P->gasScale * thr - brk * (along > .15f ? P->brakeScale + 3.f : P->gasScale);
      float fx, fz; fwdH(&M, c->h, &fx, &fz);
      Fs = madd(Fs, v3(fx, 0.f, fz), a * m * (float)(2 - nd));
    }
  }
  // занос: ручник — 2; сила шины за пределом сцепления — 1; выход — без газа, медленно или едет прямо
  if (hb && ratio > .05f) c->slip = 2;
  else if (c->slip == 0 && maxTyre > P->tireGrip * mg * .25f) c->slip = 1;
  else if (speed < 1.f || thr <= 0.f || (speed > 0.01f && dot(M.F, vel) > .9659f * speed && fabsf(c->steer) < .1f)) c->slip = 0;
  // сопротивление: на ровном тяга уравновешивается ровно на TopSpeed; в воздухе — слабое
  if (speed > .01f) {
    float gTop = P->gasSpeedThreshold < 1.f ? P->hsGasScale : P->gasScale;
    float drag = ng?2.f*gTop*m/(P->topSpeed*P->topSpeed)*speed*speed*(thr <= 0.f ? CAR_COAST_DRAG_K : 1.f):.01f*m*speed;
#if CAR_COAST_LIN
    // накат по PS2 (famil_v 10/20/28 м/с, Стенд 16.10, после лага отпуска 0,2 с): замедление A + C·v² м/с² (2,39 / 2,70 / 3,37 на средних 6,7 / 16,2 / 23,0 м/с), качение 1,2 уже в шинах — добавка выше
    if (ng && thr <= 0.f && brk <= 0.f && !hb) drag = m * (CAR_COAST_A - 1.2f + CAR_COAST_C * speed * speed);
#endif   // накат без газа: коэффициент к сопротивлению (1 — как было; PS2 famil_v 28 м/с: 3,5 м/с² против 6,5 у порта)
    Fs = madd(Fs, vel, -drag / speed);
  }
  // качание по тангажу/крену: в воздухе и когда сильно наклонена — затухание (доля критического для «неваляшки»),
  // на колёсах в обычной езде качает сама подвеска
  {
    V3 wb = mulMT(&M, om);
    int tilted = M.U.y < 0.985f;
    if (!ng || tilted) {
      float K = mg * fabsf(P->weebleOffset) + mg * 0.2f, al = P->airLevel;
      float cx = al * 2.f * sqrtf(K * c->inertia[0]), cz = al * 2.f * sqrtf(K * c->inertia[2]);
      if (!ng && airWeeble) {   // правка 353 (выше, у «неваляшки»): в полёте — критическое для «неваляшки» вместе с angDamp
        float Kw = mg * fabsf(P->weebleOffset);
        cx = fmaxf(0.f, 2.f * sqrtf(Kw * c->inertia[0]) - P->angDamp * c->inertia[0]);
        cz = fmaxf(0.f, 2.f * sqrtf(Kw * c->inertia[2]) - P->angDamp * c->inertia[2]);
      }
      Ts = add(Ts, mulM(&M, v3(-cx * wb.x, 0.f, -cz * wb.z)));
    }
  }

  // ---- 2) интегрирование
  vel = madd(vel, Fs, dt * im);
  V3 torque=Ts;
  if (P->bodyMass > 0.f) torque=sub(torque,cross(om,inertiaW(c,&M,om))); // Euler rigid-body coupling
  om = add(om, scl(invIw(c, &M, torque), dt));
  om = scl(om, 1.f - fminf(1.f, P->angDamp * dt));
  { float wl = len(om); if (wl > MAXW) om = scl(om, MAXW / wl); }
  comW = madd(comW, vel, dt);
  {
    float *q = c->q, hx = .5f * dt * om.x, hy = .5f * dt * om.y, hz = .5f * dt * om.z;
    float w0 = q[0], x0 = q[1], y0 = q[2], z0 = q[3];
    q[0] = w0 - hx * x0 - hy * y0 - hz * z0;
    q[1] = x0 + hx * w0 + hy * z0 - hz * y0;
    q[2] = y0 - hx * z0 + hy * w0 + hz * x0;
    q[3] = z0 + hx * y0 - hy * x0 + hz * w0;
    quatNorm(q);
  }
  M = quatM(c->q);
  pos = sub(comW, mulM(&M, comB));

  // ---- 3) стены
  float pen0 = c->pen, ok = fmaxf(PEN_REVERT, pen0);
  CarBox b; boxAt(c, pos, &M, &b);
  float bx0, bz0;   // центр кузова в начале подшага
  { CarBox b0; boxAt(c, pos0, &M0, &b0); bx0 = b0.x; bz0 = b0.z; }
  float mx, mz, nx, nz;
  int revert = 0, tries = 0;
  CarBox bPre;   // кузов до выталкивания (для точки удара)
  for (;; tries++) {
    boxAt(c, pos, &M, &b); bPre = b;
    colPushCar(w, &b, &mx, &mz, &nx, &nz);
    revert = (b.maxDepth > ok) | (b.behindFence ? 2 : 0);
    // 15.10 (pockets C, звено 432 L2/L5): угол кузова, чистый в начале подшага, касается торца низкого забора под опорой, и минимальный выход SAT — вдоль звена ВПЕРЁД по ходу (0,24 м
    // при шаге 0,06): кузов «подпихивается» сквозь торец без потери скорости, +0,27 м/с подскока (у оригинала по пробе PS2 сдвига сверх скорости нет, 0…0,04 м). Выталкивание вперёд
    // по ходу, большее шага подшага + FENCE_PUSH_SLACK, от забора (COL_SEG) — как удар: подшаг отменяется (общий путь гашения). FENCE_PUSH_CAP_OLD=1 — прежнее
    if (!FENCE_PUSH_CAP_OLD && !revert && pen0 <= 0.03f && b.n > 0) {
      int allSeg = 1; for (int k = 0; k < b.n; k++) if (b.type[k] != COL_SEG) allSeg = 0;
      float pl = hypotf(mx, mz), ml = hypotf(pos.x - pos0.x, pos.z - pos0.z);
      if (allSeg && pl > ml + FENCE_PUSH_SLACK && mx * vel.x + mz * vel.z > 0.f) revert = 16;
    }
    if (!revert && crossAt(w, &b, bx0, bz0, b.x, b.z)) revert = 4;
    if (!revert && crossAt(w, &b, pos0.x, pos0.z, pos.x + mx, pos.z + mz)) revert = 8;
    if (revert != 1 || tries >= 2) break;
    // тесно: сначала без подъёма/наклона (высота и тангаж/крен — как были), потом ещё и без поворота
    V3 wb = mulMT(&M, om);
    if (tries == 0) {
      float fx, fz; fwdH(&M, c->h, &fx, &fz);
      float gx, gz; floorOf(&M0, &gx, &gz);
      orientQ(c->q, atan2f(fx, fz), gx, gz);
      pos.y = pos0.y; om = mulM(&M, v3(0.f, wb.y, 0.f)); if (vel.y > 0.f) vel.y = 0.f;
    } else { memcpy(c->q, q0, sizeof q0); om = v3(0, 0, 0); }
    M = quatM(c->q);
    comW = add(pos, mulM(&M, comB));
  }
#ifdef COL_DEBUG
  if (colDebug) printf("  substep: move %.4f %.4f %.4f -> maxDepth %.4f pen0 %.4f behind %d revert %d tries %d n %d | box %.4f %.4f f %.4f %.4f y %.4f g %.4f %.4f\n",
                       pos.x - pos0.x, pos.y - pos0.y, pos.z - pos0.z, b.maxDepth, pen0, b.behindFence, revert, tries, b.n, bPre.x, bPre.z, bPre.fx, bPre.fz, bPre.y, bPre.gx, bPre.gz);
#endif
  if (carDbgOn1050 && (revert || b.n) && !carDbg1050.revert && !carDbg1050.bn) { CarDbg1050 *d = &carDbg1050; d->revert = revert; d->tries = tries; d->bn = b.n < 8 ? b.n : 8; d->bdepth = b.maxDepth;
    for (int k = 0; k < d->bn; k++) { d->btype[k] = b.type[k]; d->bidx[k] = b.index[k]; d->bnx[k] = b.nx[k]; d->bnz[k] = b.nz[k]; d->bd[k] = b.depth[k]; } }
  c->contacts = 0;
  float wallHit = 0.f;
  if (revert) {
    // не вытолкнулось / за забором — подшаг отменяем: положение и ориентация как были, скорость отражаем назад
    pos = pos0; M = M0; memcpy(c->q, q0, sizeof q0); c->reverts++; c->revWhy = revert;
    V3 vh = v3(vel.x, 0.f, vel.z); float e = contactRestitution(c,P->restitution); int bushRest = 0;
#ifndef FOLIAGE_PREV
    // не протиснулся сквозь куст (или забор по изгороди, revert 2/4/8 без контактов) — куст гасит, не отбрасывает назад
    { int soft = b.n > 0; for (int k = 0; k < b.n; k++) if (colContactMaterial(w, &b, k) != COL_MAT_FOLIAGE) soft = 0;
      if ((revert & 14) && colFoliageNear(w, b.x, b.y + b.lo, b.z, P->halfL + COL_FOLIAGE_FENCE + 0.5f)) soft = 1;   // + угол кузова наискось
      // куст гасит: упругость 18 %, но не меньше FOLIAGE_PUSH назад — чтобы не прилипнуть к изгороди на газу (откат
      // возвращает и курс, без зазора машина не развернётся; правило порта, «возможность выехать» — принцип доводки п. 4)
      if (soft && !CAR_BUSH_SOFT_OLD && thr > 0.f && sqrtf(vh.x * vh.x + vh.z * vh.z) < .5f) bushRest = 1;
      if (soft) { float vl = sqrtf(vh.x * vh.x + vh.z * vh.z); e = vl > 1e-3f ? fmaxf(e * FOLIAGE_E, fminf(FOLIAGE_PUSH, vl) / vl) : 0.f; } }
#endif
    { // правки 164/187: подшаг отменён о твёрдое на скорости удара оригинала — гасим на месте (оригинал: откат 0,3 м), не отражаем
      float vn = 0.f; int hard = b.n > 0;   // скорость сближения с твёрдым по нормали контакта (не модуль скорости: скольжение вдоль — не удар)
      for (int k = 0; k < b.n; k++) { if (colContactMaterial(w, &b, k) == COL_MAT_FOLIAGE) hard = 0;
                                      float a = -(vh.x * b.nx[k] + vh.z * b.nz[k]); if (a > vn) vn = a; }
      if (hard && vn > CAR_CRASH_V) { float k = CAR_CRASH_KEEP + (1.f - CAR_CRASH_KEEP) * clampf((CAR_CRASH_V2 - vn) / (CAR_CRASH_V2 - CAR_CRASH_V), 0.f, 1.f);
                                      if (e > k) e = k; c->crashes++;
                                      // удар, на котором подшаг отменён (изгородь на цоколе L1 z≈150,4, капсула 421): гасим, как раньше, но и урон — как у
                                      // обычного удара о твёрдое (правка №388: у PS2 такой удар считается, 12-й разбивает; в порту отмена его не считала)
                                      if (!CAR_REVERT_DMG_OLD) { addDamageK(c, vn, 2.f, CAR_WALL_DMG_OLD ? 0.0085f * 3.f : 0.019f); if (vn > c->hardV) c->hardV = vn; } }
    }
    vel = v3(-vh.x * e, fminf(vel.y, 0.f), -vh.z * e); om = scl(om, 0.5f);
    if (bushRest) { float fl = hypotf(M0.F.x, M0.F.z); if (fl > 1e-3f) { vel.x = -M0.F.x / fl * FOLIAGE_PUSH_REST; vel.z = -M0.F.z / fl * FOLIAGE_PUSH_REST; } }
    comW = add(pos, mulM(&M, comB));
    CarBox pb; boxAt(c, pos, &M, &pb); colProbeCar(w, &pb);
    c->pen = pb.maxDepth;
    for (int k = 0; k < pb.n && c->contacts < 4; k++) { c->cnx[c->contacts] = pb.nx[k]; c->cnz[c->contacts] = pb.nz[k]; c->ctype[c->contacts] = pb.type[k]; c->contacts++; }
  } else {
    // сдвиг — вдоль «пола» кузова, как считал colPushCar (иначе слой уезжает по высоте на g*сдвиг и край ящика/тумбы,
    // задевший слой на миллиметры, даёт в пробе сантиметры)
    float my = b.gx * mx + b.gz * mz;
    pos.x += mx; pos.z += mz; pos.y += my; comW.x += mx; comW.z += mz; comW.y += my; c->pen = b.maxDepth;
    if (b.n) {
      Ct ct[COL_MAX_CONTACTS]; int n = 0; float e = clampf(contactRestitution(c,P->restitution), 0.f, 0.85f);
      V3 popR = v3(0, 0, 0); float foliageHit = 0.f, hardHit = 0.f; int nSoft = 0;
      for (int k = 0; k < b.n; k++) {
        if (c->contacts < 4) { c->cnx[c->contacts] = b.nx[k]; c->cnz[c->contacts] = b.nz[k]; c->ctype[c->contacts] = b.type[k]; c->contacts++; }
        float px, pz; hitSlices(w, &bPre, b.nx[k], b.nz[k], &px, &pz); px += mx; pz += mz;
        V3 p = v3(px, b.y + b.gx * (px - b.x) + b.gz * (pz - b.z) + P->hitY, pz);
        Ct *q = &ct[n++]; q->n = v3(b.nx[k], 0.f, b.nz[k]); q->r = sub(p, comW); q->jn = 0; q->e = e;
#ifndef PITS_SMOOTH_OLD
#define PITS_SMOOTH_OLD 0
#endif
#ifndef RIDGE_SLIDE_OLD
#define RIDGE_SLIDE_OLD PITS_SMOOTH_OLD
#endif
        // №584: боковое касание колонн и выступов (Cyl 62, 64, 74) кузовом при движении вперед:
        // нормаль реакции отталкивает кузов вбок, устраняя встречную компоненту, чтобы машина соскальзывала вдоль препятствия
        if (!RIDGE_SLIDE_OLD && (RIDGE_ALL_TYPES_OLD || b.type[k] == COL_CYL) && dot(vel, M.F) > 0.f) {
          float lat = dot(q->r, M.L);
          if (fabsf(lat) > P->halfW * 0.45f) {
            float fwd = dot(q->n, M.F);
            if (fwd < 0.f) {
              V3 ns = sub(q->n, scl(M.F, fwd));
              float nsl = len(ns);
              if (nsl > 1e-4f) q->n = scl(ns, 1.f / nsl);
            }
          }
        }
        q->vn0 = dot(add(vel, cross(om, q->r)), q->n);
        if (colContactMaterial(w, &b, k) == COL_MAT_FOLIAGE) {
#ifdef FOLIAGE_PREV
          q->e = .4f;
#else
          q->e = e * FOLIAGE_E;
#endif
          foliageHit = fmaxf(foliageHit, -q->vn0); nSoft++;
        } else hardHit = fmaxf(hardHit, -q->vn0);
        if (-q->vn0 > wallHit) { wallHit = -q->vn0; popR = q->r; }
        if (-q->vn0 > c->wallV) {   // наблюдение для переписи стен, на физику не влияет
          c->wallV = -q->vn0; c->wallType = b.type[k]; c->wallIdx = b.index[k]; c->wallMat = colContactMaterial(w, &b, k);
          c->wallN[0] = b.nx[k]; c->wallN[1] = b.nz[k]; c->wallP[0] = px; c->wallP[1] = p.y - P->hitY; c->wallP[2] = pz;
        }
      }
#ifdef FOLIAGE_PREV
      float popK = P->pop * clampf(sqrtf(REF_MASS / m), 0.5f, 1.1f) * clampf((wallHit - 4.f) / 10.f, 0.f, 1.f);
      solve(c, &M, &vel, &om, ct, n, P->wallFriction, popK, P->popMax, popR, 1);
      (void)nSoft;   // Requested playful vegetation response: a bounded hop on actual impact, not proximity.
      if (foliageHit > 2.f && c->foliageCooldown <= 0.f && M.U.y > .35f) {
        vel.y = fmaxf(vel.y, fminf(7.f, 2.f + foliageHit * .35f));
        c->foliageCooldown = .65f; c->foliageBounces++;
      }
#else
      // подброс стенкой (pop) — только от твёрдого; о куст — трение куста, если твёрдого в контакте нет
      float popK = P->pop * clampf(sqrtf(REF_MASS / m), 0.5f, 1.1f) * clampf((hardHit - 4.f) / 10.f, 0.f, 1.f);
      float vy0 = vel.y, vx0 = vel.x, vz0 = vel.z;   // правка 346: вертикаль до удара; №487: горизонталь до удара (касательная закона краша)
      // Касание забора с лица наискось (сверка стен 09.10, 11 точек L1: fence_f001_Bf, fence_f007_Af, teleport000_Af …): машина PS2
      // разворачивается вдоль забора на 19–26° и едет дальше 2,6–6,1 м/с; с трением кузова о стену 0,2 (правило порта) машина порта
      // поворачивала на 5–6° и вставала (≤ 0,8 м/с), без трения — 18–20° и 2,6–4,1 м/с. Сильный удар (нормаль выше CAR_CRASH_V) —
      // как был: его гасит правило краша по замеру В22, и с трением совпадают места F12-6 (tests/airwalls/crash.sh). Ящики, капсулы и стены-треугольники
      // — как были. Забор под изгородью тоже скользит: hedge003_Af/Bf (421), прежнее трение листвы 0,39 останавливало нос. Задом — как было: в пробе parked005_Ar (задний ход, 48° к
      // нормали, как у трёх проб вперёд) машина PS2 встаёт у забора — правило порта по этим четырём пробам: вперёд скользит, задом нет.
      // Прежнее: -DCAR_WALL_FRICTION_OLD=1
#ifndef CAR_FENCE_MU
#define CAR_FENCE_MU 0.f
#endif

#ifndef CAR_WALL_FRICTION_OLD
#define CAR_WALL_FRICTION_OLD 0
#endif
      int allSeg = 1; for (int k = 0; k < b.n; k++) allSeg &= b.type[k] == COL_SEG;   // только заборы (ColSeg), не ящики и стены-треугольники
      float muWall = CAR_WALL_FRICTION_OLD || hardHit > CAR_CRASH_V || !allSeg || dot(vel, M.F) <= 0.f ? P->wallFriction : CAR_FENCE_MU;
      if (!CAR_FENCE_LAT_OLD && allSeg && hardHit <= CAR_CRASH_V && dot(vel, M.F) > 0.f) c->fenceT = CAR_FENCE_LAT_T;
      solve(c, &M, &vel, &om, ct, n, nSoft == n && (!allSeg || CAR_WALL_FRICTION_OLD) ? P->wallFriction * FOLIAGE_MU : muWall, popK, P->popMax, popR, 1);
#endif
      if (hardHit > CAR_CRASH_V) {   // правки 164/187: удар оригинала — машина гасит скорость на месте, без отскока и разворота
        float k = CAR_CRASH_KEEP + (1.f - CAR_CRASH_KEEP) * clampf((CAR_CRASH_V2 - hardHit) / (CAR_CRASH_V2 - CAR_CRASH_V), 0.f, 1.f);
        float wk = clampf((CAR_CRASH_V2 - hardHit) / (CAR_CRASH_V2 - CAR_CRASH_V), 0.f, 1.f), nl = hypotf(c->wallN[0], c->wallN[1]);
        if (!CAR_CRASH_SLIDE_OLD && nl > .5f && ng >= 3) {   // только на земле: бросок в воздухе (F12-6L, стенд crash.sh) — прежнее правило
          // №487 К-60 (b2_ram): стена на 19 м/с под 17° к нормали — PS2 по нормали −18.19 → +1.73 (отскок 0.095), вдоль 5.62 → 3.06 (трение
          // стены ≈ 0.13 · импульс удара: 0.13·19.9 = 2.6 м/с), не «всё ×0.13». Нормаль и касательная раздельно: нормаль → CAR_CRASH_E · удар
          // от стены, касательная теряет CAR_CRASH_MU · (1 + e) · удар (не ниже нуля); плавно к прежнему на 8–12 м/с (wk)
          float nx = c->wallN[0] / nl, nz = c->wallN[1] / nl, vn = vel.x * nx + vel.z * nz, vn0 = vx0 * nx + vz0 * nz;
          float tx = vx0 - vn0 * nx, tz = vz0 - vn0 * nz, tl = hypotf(tx, tz);   // касательная — по скорости ДО удара (трение решателя уже съело часть)
          float vnNew = (1.f - wk) * CAR_CRASH_E * hardHit + wk * vn * k, tlNew = fmaxf(0.f, tl - (1.f - wk) * CAR_CRASH_MU * hardHit * (1.f + CAR_CRASH_E)) * 1.f;
          tlNew = wk * tl * k + (1.f - wk) * tlNew;
          if (tl > 1e-4f) { tx /= tl; tz /= tl; } else { tx = tz = 0.f; }
          vel.x = vnNew * nx + tlNew * tx; vel.z = vnNew * nz + tlNew * tz;
        } else { vel.x *= k; vel.z *= k; }
        om = scl(om, CAR_CRASH_OM + (1.f - CAR_CRASH_OM) * (k - CAR_CRASH_KEEP) / (1.f - CAR_CRASH_KEEP));
#ifndef FOLIAGE_PREV
        // Правка Роба 346 (05.10, L1 K13-2: «на скорости заезжаем на эти заборы»): у стены двора 2,1 м перед ней газон-бровка 12°;
        // на 80–110 км/ч бровка даёт машине 4–5 м/с вверх, удар добавлял подброс (pop, до 2,6 м/с) — колёса взлетали на 2,5–2,7 м,
        // выше верха стены (2,3), машина «въезжала» на неё. Оригинал (близнец В22) при ударе о твёрдое встаёт как вкопанная — без
        // подброса: при ударе краша вертикаль не больше, чем до удара. carCrashPopOld = 1 (или -DCAR_CRASH_POP=1) — прежнее
        if (!carCrashPopOld && vel.y > vy0) vel.y = vy0;
#endif
        c->crashes++;
      }
      addDamageK(c, hardHit, 2.f, CAR_WALL_DMG_OLD ? 0.0085f * 3.f : 0.019f);
      if (hardHit > c->hardV) c->hardV = hardHit;          // правка 135: звук удара — твёрдое и листва раздельно
      if (foliageHit > c->foliV) c->foliV = foliageHit;
    }
  }
#ifndef CAR_CURB_STEP_OLD
#define CAR_CURB_STEP_OLD 0
#endif
// Правка Роба 475 (находки ботов, ступень/бордюр): у низкой плоской ступени из переписи боты нашли остаток 0,36–0,41 м над полом —
// на 1–6 см выше порога P->step (0,35), стена хотя рисуется как обычный бордюр. Для плоской опоры с проходимой землёй за контактом
// порог шины = P->step + CAR_CURB_TOL; выше (0,5 м и больше), капсулы и стены с высокой гранью остаются стеной, как у e3a288b.
// CAR_CURB_TOL_OLD=1 — порог ровно P->step
#ifndef CAR_CURB_TOL_OLD
#define CAR_CURB_TOL_OLD 0
#endif
#ifndef CAR_CURB_TOL
#define CAR_CURB_TOL 0.08f
#endif
#ifndef CAR_WHEEL_WALLS_OLD
#define CAR_WHEEL_WALLS_OLD 0
#endif
  if(!CAR_WHEEL_WALLS_OLD && !revert && P->bodyMass>0.f) {
    Ct wheelCt[16];V3 point[16];float depths[16];uint32_t refs[16];int count=0;
#ifdef COL_DEBUG
    int wheelOf[16];
#endif
    for(int i=0;i<4;i++) {
      float radius;V3 wb=wheelColliderLocal(c,i,&radius);
      V3 centre=add(pos,mulM(&M,wb));float xyz[3]={centre.x,centre.y,centre.z};ColSphereContact contact[4];
      int n=colWheelWallContacts(w,xyz,radius,contact,4);
      for(int j=0;j<n;j++) {
        if(!CAR_CURB_STEP_OLD && contact[j].n[1]<COL_WALK_NY) {
          float nx=contact[j].n[0],nz=contact[j].n[2],nl=sqrtf(nx*nx+nz*nz);
          if(nl>.1f) {
            float foot=centre.y-radius,g,gn[3],top=1e30f;
            unsigned ref=contact[j].ref,kind=ref>>29,id=ref&0x1fffffffu;
            if(kind==COL_TRI){const ColTri*t=&w->tri[id];
              if(!SEAM_WHEEL_SKIP_OLD && t->n[1]>COL_WALK_NY) continue;
              top=SEAM_WHEEL_TOP_OLD ? fmaxf(t->a[1],fmaxf(t->b[1],t->c[1])) : fminf(fmaxf(t->a[1],fmaxf(t->b[1],t->c[1])), contact[j].p[1] + 0.35f);
            }
            if(kind==COL_BOX){const ColBox*b=&w->box[id];top=b->c[1]+fabsf(b->ax[1])*b->h[0]+fabsf(b->ay[1])*b->h[1]+fabsf(b->az[1])*b->h[2];}
            // A low support beside a tall wall is not permission to cross the wall.
            // Rounded capsules retain their dedicated slope/hump contact rules.
            float tx=contact[j].p[0]-.08f*nx/nl,tz=contact[j].p[2]-.08f*nz/nl;
            float lim=foot+P->step+(CAR_CURB_TOL_OLD?0.f:CAR_CURB_TOL);
            if(top>foot+.01f && top<=lim && colGround(w,tx,tz,lim,foot+.01f,&g,gn) && gn[1]>COL_WALK_NY)continue;
          }
        }
        V3 cn = v3(contact[j].n[0], contact[j].n[1], contact[j].n[2]);
        // №187: срез встречной компоненты нормали нужен у НЕБОЛЬШИХ рёбер (бордюр, шов, торец балки); у малого тела (цистерна J11-9, коробка 2×2×2 м) это лобовая стена: срез проталкивал колесо и машина
        // сползала с цистерны на 2,7 м после остановки (гейт airwalls/crash: допуск 1,2 м). Коробка со всеми полуразмерами < SEAM_WHEEL_CN_SMALLBOX (2 м) режется как стена. SEAM_WHEEL_CN_SMALLBOX_OLD=1 — как было
        int cnSmall = 0;
        if (!SEAM_WHEEL_CN_SMALLBOX_OLD && (contact[j].ref >> 29) == COL_BOX) { const ColBox *cb = &w->box[contact[j].ref & 0x1fffffffu]; cnSmall = fmaxf(cb->h[0], fmaxf(cb->h[1], cb->h[2])) < SEAM_WHEEL_CN_SMALLBOX; }
        if (!SEAM_WHEEL_CN_OLD && !cnSmall) {
          V3 cp_diff = sub(v3(contact[j].p[0], contact[j].p[1], contact[j].p[2]), centre);
          float sideDist = dot(cp_diff, M.L);
          if (fabsf(sideDist) > 0.08f) {
            float fwd = dot(cn, M.F);
            if (fwd < 0.f) {
              cn = sub(cn, scl(M.F, fwd));
              float cnl = len(cn);
              if (cnl > 1e-4f) cn = scl(cn, 1.f / cnl);
            }
          }
        }
        Ct*q=&wheelCt[count];q->n=cn;
        point[count]=v3(contact[j].p[0],contact[j].p[1],contact[j].p[2]);depths[count]=contact[j].depth;refs[count]=contact[j].ref;
#ifdef COL_DEBUG
        wheelOf[count]=i;
#endif
        q->r=sub(point[count],comW);q->jn=0;q->e=0.1f;q->vn0=dot(add(vel,cross(om,q->r)),q->n);count++;
      }
    }
    if(count) {
      if (carDbgOn1050 && !carDbg1050.wn) { CarDbg1050 *d = &carDbg1050; d->wn = count < 8 ? count : 8; for (int k = 0; k < d->wn; k++) { d->wref[k] = refs[k]; d->wn3[k][0] = wheelCt[k].n.x; d->wn3[k][1] = wheelCt[k].n.y; d->wn3[k][2] = wheelCt[k].n.z; d->wdepth[k] = depths[k]; } }
      V3 shift=v3(0,0,0);for(int it=0;it<2;it++)for(int i=0;i<count;i++){float d=depths[i]-dot(shift,wheelCt[i].n);if(d>0)shift=madd(shift,wheelCt[i].n,d);}
      pos=add(pos,shift);comW=add(comW,shift);
      for(int i=0;i<count;i++) {wheelCt[i].r=sub(add(point[i],shift),comW);
        if(c->contacts<4){c->cnx[c->contacts]=wheelCt[i].n.x;c->cnz[c->contacts]=wheelCt[i].n.z;c->ctype[c->contacts]=(unsigned char)(refs[i]>>29);c->contacts++;}}
#ifdef COL_DEBUG
      V3 velIn=vel,omIn=om;
#endif
      solve(c,&M,&vel,&om,wheelCt,count,P->groundFriction,0,0,v3(0,0,0),0);
#ifdef COL_DEBUG
      if(colDebug) {   // 388/377: сферы колёс — какой контакт даёт импульс (разбор ударов F12/J11)
        printf("  wheelCt %d shift %.4f %.4f %.4f vel %.3f %.3f %.3f -> %.3f %.3f %.3f om %.2f %.2f %.2f -> %.2f %.2f %.2f\n",count,shift.x,shift.y,shift.z,
               velIn.x,velIn.y,velIn.z,vel.x,vel.y,vel.z,omIn.x,omIn.y,omIn.z,om.x,om.y,om.z);
        for(int i=0;i<count;i++)printf("   wheel %d ref %u#%u n %.3f %.3f %.3f p %.3f %.3f %.3f depth %.4f vn0 %.3f jn %.1f K %.1f\n",wheelOf[i],refs[i]>>29,refs[i]&0x1fffffffu,
               wheelCt[i].n.x,wheelCt[i].n.y,wheelCt[i].n.z,point[i].x,point[i].y,point[i].z,depths[i],wheelCt[i].vn0,wheelCt[i].jn,wheelCt[i].K);
      }
#endif
    }
  }
  // под началом машины должна быть проходимая поверхность не выше, чем можно въехать (как v9): нет её, но чуть
  // дальше по ходу есть — щель в сетке, едем; есть выше (до 6 м) — склон сбоку, стена; нет ничего — край мира
  if (!revert && (pos.x != pos0.x || pos.z != pos0.z)) {
    float g_, mdx = pos.x - pos0.x, mdz = pos.z - pos0.z, ml = sqrtf(mdx * mdx + mdz * mdz);
    // до 0.6 м выше начала машины земля — дело колёс (крепление подвески ~0.5 м) и точек днища, не стена
    float top = fmaxf(pos.y, pos0.y) + fmaxf(P->step, 0.6f) + 1.8f * ml;
    int why = 0;
    if (!colGround(w, pos.x, pos.z, top, -1e6f, &g_, 0)) {
      static const float look[4] = {0.3f, 1.f, 2.f, 3.f};
      why = 16;
      // щель в сетке переезжаем только на колёсах; в полёте/кувырком над пустотой — стена (не влететь в яму без дна)
      int drive = c->wheels >= 2 && M0.U.y > 0.9f && vel.y > -2.f;
      for (int k = 0; k < 4 && why && drive; k++)
        if (colGround(w, pos.x + mdx / ml * look[k], pos.z + mdz / ml * look[k], top + 0.3f, -1e6f, &g_, 0)) why = 0;
      // узкая щель между кусками коллизии (стык трамплина и дороги и т.п.): земля есть с обеих сторон в пределах CAR_CRACK
      // вдоль хода или поперёк — не край мира и не яма, и в полёте тоже (27.09: щель 0.2 м за трамплином у стекла Krusty
      // отбрасывала машину назад в воздухе). Настоящий край (земли нет с одной стороны) остаётся стеной
      for (int k = 0; k < 2 && why; k++) {
        float ax = k ? -mdz / ml : mdx / ml, az = k ? mdx / ml : mdz / ml;
        if (colGround(w, pos.x + ax * CAR_CRACK, pos.z + az * CAR_CRACK, top + 0.3f, -1e6f, &g_, 0) &&
            colGround(w, pos.x - ax * CAR_CRACK, pos.z - az * CAR_CRACK, top + 0.3f, -1e6f, &g_, 0)) why = 0;
      }
      if (why && colGround(w, pos.x, pos.z, pos.y + 6.f, top, &g_, 0)) why = 32;
    }
    // ДОБИВКА 18 (правки Роба 339/349, вопрос В87 — knowledge/facts/ps2-r6-car-deadend.yaml): тупик перед забором покрышек L1
    // (258; −511): начало машины оказалось ПОД поверхностью кучи покрышек (замер: на 0,76 м ниже её, под началом опоры нет вовсе), и
    // каждый шаг в любую сторону отменялся как «земля выше, чем можно въехать» (why 32) — машина стояла навсегда, ни газ, ни задний
    // ход, ни возвраты (нет удара, нет днища) не выводили. В записи PS2 машина в этом месте идёт НАД кучей: из снимка над кучей и газ,
    // и задний ход 3 с — через забор 462 во двор, DEATH R6D01, перенос на кадре 98 (запись ps2-w build/twins/o4/
    // ps2_v87_deadend_gas_3s, _reverse_3s); случай «под кучей» у оригинала не воспроизведён (В87, limitation). Правило порта: если и под началом машины опоры нет,
    // а над ним в пределах DEADEND_LIFT есть поверхность (машина под ней, не под мостом — под мостом есть дорога снизу, у щели — опора
    // по обе стороны в CAR_CRACK), машина ставится на эту поверхность на месте (горизонтальная скорость та же, падение гасится) вместо
    // отмены шага. DEADEND_OLD=1 — как было. ВАЖНО (второй круг 09.10): правило снимает только бесконечную отмену шага под кучей —
    // сам тупик НЕ закрыт: после подъёма машина сползает к забору 462 и клинится в щели «куча — забор» (назад: 258,7; 8,6; −511,3,
    // заезд: 259,1; 7,1; −515,1); замер исхода — tests/death/r6.sh deadend и sweep (docs/handoff/DEATH_ZONES_CLAUDE_2026-10-09.md)
#ifndef DEADEND_OLD
#define DEADEND_OLD 1   // по умолчанию ВЫКЛ (дирижёр 09.10): по замеру sweep с подъёмом хуже — застреваний 19 → 26, переносов DEATH 15 → 6; включать -DDEADEND_OLD=0 для опытов
#endif
#define DEADEND_LIFT 2.f   // правило порта: выше 2 м не поднимаем (просвет моста/эстакады больше; заглубление в тупике — 0,76 м, замер C1)
    if (why == 32 && !DEADEND_OLD) {
      float lim0 = pos0.y + fmaxf(P->step, 0.6f), g0;
      int under = !colGround(w, pos0.x, pos0.z, lim0, -1e6f, &g0, 0);
      for (int k = 0; k < 2 && under; k++)   // щель (как проверка щели выше): опора по обе стороны вдоль x или вдоль z — не «под»
        if (colGround(w, pos0.x + (k ? 0.f : CAR_CRACK), pos0.z + (k ? CAR_CRACK : 0.f), lim0, -1e6f, &g0, 0) &&
            colGround(w, pos0.x - (k ? 0.f : CAR_CRACK), pos0.z - (k ? CAR_CRACK : 0.f), lim0, -1e6f, &g0, 0)) under = 0;
      if (under && colGround(w, pos0.x, pos0.z, pos0.y + DEADEND_LIFT, lim0, &g0, 0)) {
        pos = v3(pos0.x, g0, pos0.z); M = M0; memcpy(c->q, q0, sizeof q0);
        if (vel.y < 0.f) vel.y = 0.f;
        c->deadendLifts++; why = 0;
      }
    }
    // Ночь 11.10 (L3 (−76,−69,−290), (89,−20,−303), L6 то же): у края пустоты, под которой данные уровня положили зону DEATH (ловушка падения,
    // R5D03 / Z4D18), каждый шаг отменялся как «край мира» (why 16), и машина стояла навсегда: зона не достаётся (у Z4D18 верх на 0,6 м ниже пола, у
    // R5D03 на 35 м ниже), сброса нет. У оригинала машина падает в зону и переносится в точку возврата. Правило порта: край, под которым в пределах
    // EDGE_DEATH_DROP метров по xz лежит зона DEATH, — не стена, шаг принимается; дальше работает зона (gameDeathAt) или, без неё, возврат на надёжное
    // место (noGroundT). EDGE_DEATH_OLD=1 (carEdgeDeathOld) — как было. Остальные края (зоны под ними нет) остаются стеной
    if (why == 16 && !carEdgeDeathOld && edgeDeathBelow(w, pos.x, pos.y, pos.z)) { why = 0; edgeDeathN++;
#ifdef EDGE_DBG
      { extern int printf(const char *, ...); static int n; if (n++ % 10 == 0) printf("EDGE %.1f %.1f %.1f v %.1f\n", pos.x, pos.y, pos.z, c->v); }
#endif
    }
    if (why) {
      pos = pos0; M = M0; memcpy(c->q, q0, sizeof q0); c->reverts++; c->revWhy = why; revert = why; c->pen = pen0;
      V3 d = v3(mdx / ml, 0.f, mdz / ml); float vdir = dot(vel, d), e = contactRestitution(c,P->restitution);
      if (vdir > 0.f) vel = madd(vel, d, -(1.f + e) * vdir);
      if (vel.y < 0.f) vel.y = 0.f;   // у края не копим падение и кувырок, пока отменяем
      om = v3(0.f, om.y, 0.f);
      comW = add(pos, mulM(&M, comB));
    }
  }

  // ---- 4) земля кузовом: точки днища (и крыши, если лежит) с заметанием по высоте от прошлого положения
  // №536 (Роб, 11.10, железо: «разбитая машина от посадки взрывается»): у оригинала посадка после прыжка (трамплин, обрыв) на колёса/днище на
  // опору урона не даёт при любой скорости падения; урон только от столкновений со стенами, предметами, машинами. Контакт днища/крыши с опорой
  // (нормаль вверх, n.y >= LAND_NY) урон не считает; крутой склон/стенка (n.y ниже) и всё остальное — как раньше. -DLAND_DAMAGE_OLD=1 — прежнее
  int hullN = 0, hullSoft = 0; float groundHit = 0.f, groundHitDmg = 0.f; V3 shift = v3(0, 0, 0);
  {
    // днище: углы бамперов подняты (углы въезда/съезда ~40/27 градусов, как у настоящей машины: трамплин и бордюр
    // не цепляют свесом), середина днища — на bodyLo (сесть брюхом на гребень); крыша — 4 угла
    // правка 164б (03.10): ещё две точки по оси днища — посередине переднего и заднего края. Узкая опора вдоль машины (верх перил
    // 1,4 м уже днища 2,2 м: углы свисают по обе стороны) держит днище по длине, как ящик кузова оригинала; без них машина висела
    // на одной средней точке, клевала носом (центр масс на 0,35 м впереди, клевок до 25°), и слой кузова принимал верх перил за
    // стену — проход стен уносил её назад по перилам на 16 м. Подняты выше углов (CAR_BELLY_KEEL_Y): берут только клевок больше
    // ~6° к опоре, обычные касания днищем — углы и середина, как было. CAR_BELLY_KEEL=0 — прежние 5 точек (контроль гейта)
// Правка 388/377 (08.10, F12-6L левые перила): днище — прямоугольник, а не 7 точек. Машина, легшая боком на узкую балку (1,4 м) с
// креном ~30°, у оригинала (v22_f126l, кадры 7984–8180, png) стоит: ребро балки держит днище. У порта ребро балки проходило между
// точкой середины и угловой (та — на 1 см за краем) — опоры не было, машина докручивалась, сфера колеса о бок балки толкала её, и
// она «ползла» на 2,6 м назад (attempt55/f12, покадрово). Теперь верхние рёбра УЗКИХ почти ровных ящиков (уже колеи, n.y ≥ 0,9:
// балки, парапеты; col.h colBoxEdgesUnderPlane) под прямоугольником днища — такие же контакты днища: F12-6L 2,79 → 0,33 м; широкие
// и наклонные плиты по-прежнему только точками (иначе J11-9 0,86 → 1,9 на плите 927 и толчок у cyl 582 от ребра откоса 1363 —
// attempt55/edges, attempt58/narrow). CAR_BELLY_EDGES=0 — прежние точки без рёбер (красный tests/airwalls/crash.sh F12-6L)
#ifndef CAR_BELLY_EDGES
#define CAR_BELLY_EDGES 1
#endif
#define CAR_BELLY_EDGES_MAX 8
#if !defined(CAR_BELLY_KEEL) || CAR_BELLY_KEEL
#ifndef CAR_BELLY_KEEL_Y
#define CAR_BELLY_KEEL_Y 0.25f
#endif
    static const float HX[11] = {0.9f, -0.9f, 0.9f, -0.9f, 0.f, 0.f, 0.f, 0.85f, -0.85f, 0.85f, -0.85f};
    static const float HZ[11] = {0.85f, 0.85f, -0.85f, -0.85f, 0.f, 0.85f, -0.85f, 0.8f, 0.8f, -0.8f, -0.8f};
    static const float HY[11] = {0.12f, 0.12f, 0.12f, 0.12f, 0.f, CAR_BELLY_KEEL_Y, CAR_BELLY_KEEL_Y, 0.f, 0.f, 0.f, 0.f};
    enum { NB = 7, NH = 11 };   // точек днища, всего (с крышей — если лежит)
#else
    static const float HX[9] = {0.9f, -0.9f, 0.9f, -0.9f, 0.f, 0.85f, -0.85f, 0.85f, -0.85f};
    static const float HZ[9] = {0.85f, 0.85f, -0.85f, -0.85f, 0.f, 0.8f, 0.8f, -0.8f, -0.8f};
    static const float HY[9] = {0.12f, 0.12f, 0.12f, 0.12f, 0.f, 0.f, 0.f, 0.f, 0.f};
    enum { NB = 5, NH = 9 };
#endif
    int np = M.U.y > 0.5f ? NB : NH;
    Ct ct[NH + CAR_BELLY_EDGES_MAX]; V3 cp[NH + CAR_BELLY_EDGES_MAX]; float cd[NH + CAR_BELLY_EDGES_MAX];
    for (int k = 0; k < np; k++) {
      V3 pb = v3(HX[k] * P->halfW, (k < NB ? P->bodyLo : P->bodyHi) + c->modelRaise + HY[k], P->bodyOfs + HZ[k] * P->halfL);
      V3 pw = add(pos, mulM(&M, pb)), pw0 = add(pos0, mulM(&M0, pb));
      float mv = sqrtf((pw.x - pw0.x) * (pw.x - pw0.x) + (pw.z - pw0.z) * (pw.z - pw0.z));
      float gy, n3[3], gs, gtop = fmaxf(pw.y, pw0.y) + 0.1f + 1.8f * mv; int gm = COL_MAT_HARD;
      int got = colGroundMat(w, pw.x, pw.z, gtop, pw.y, &gy, n3, &gm);
      if (supportAt(c, pw.x, pw.z, gtop, pw.y, &gs) && (!got || gs > gy)) {
        got = 1; gy = gs; n3[0] = 0.f; n3[1] = 1.f; n3[2] = 0.f; gm = COL_MAT_HARD;                        // крыша машины
      }
      if (!got) continue;
      V3 n = v3(n3[0], n3[1], n3[2]);
      cp[hullN] = pw; cd[hullN] = (gy - pw.y) * n.y;
      V3 bn = n;
      // №582: ямы L2 (#14, #15, #17, #21, #24) — сглаживание нормали днища на проходимом склоне ям при вывешивании
      if (!PITS_SMOOTH_OLD && !SEAM_HULL_OLD && (c->grounded || c->wheels >= 1 || c->airT < 0.15f) && n3[1] > COL_WALK_NY && n3[1] < SEAM_HULL_NYMAX) {
        float fwd = dot(bn, M.F);
        if (fwd < 0.f) {
          bn = sub(bn, scl(M.F, fwd));
          float bnl = len(bn);
          if (bnl > 1e-4f) bn = scl(bn, 1.f / bnl);
        }
      }
      Ct *q = &ct[hullN++]; q->n = bn; q->r = sub(pw, comW); q->jn = 0;
      q->e = clampf(contactRestitution(c,P->groundRestitution), 0.f, 0.8f);
      q->vn0 = dot(add(vel, cross(om, q->r)), n);
      // Правило порта: сильный отскок калиброван падением PS2; слабое касание сохраняет 0,1
      // 6 м/с уже разделяют жёсткую посадку и касание в addDamage ниже
      if (q->vn0 >= -6.f) q->e = contactRestitution(c,0.1f);
      if (gm == COL_MAT_FOLIAGE) { hullSoft++; q->e = 0.f; }         // куст: удар гасит, подброс — ниже, ограниченный
      else { if (-q->vn0 > groundHit) groundHit = -q->vn0;              // урон только от твёрдой земли
        if ((LAND_DAMAGE_OLD || n.y < LAND_NY) && -q->vn0 > groundHitDmg) groundHitDmg = -q->vn0; }
    }
    int nPt = hullN;   // точки днища (на кузове); дальше — рёбра ящиков (в мире)
    if (CAR_BELLY_EDGES && np == NB) {
      ColSphereContact ec[CAR_BELLY_EDGES_MAX];
      V3 bo = add(pos, mulM(&M, v3(0.f, P->bodyLo + c->modelRaise, P->bodyOfs)));
      float o3[3] = {bo.x, bo.y, bo.z}, u3[3] = {M.U.x, M.U.y, M.U.z}, f3[3] = {M.F.x, M.F.y, M.F.z}, l3[3] = {M.L.x, M.L.y, M.L.z};
      int ne = colBoxEdgesUnderPlane(w, o3, u3, f3, l3, 0.85f * P->halfL, 0.9f * P->halfW, P->step, 2.f * P->wheelX, ec, CAR_BELLY_EDGES_MAX);
      for (int k = 0; k < ne; k++) {
        V3 pw = v3(ec[k].p[0], ec[k].p[1], ec[k].p[2]);
        cp[hullN] = pw; cd[hullN] = ec[k].depth;
        Ct *q = &ct[hullN++]; q->n = M.U; q->r = sub(pw, comW); q->jn = 0;
        q->e = clampf(contactRestitution(c, P->groundRestitution), 0.f, 0.8f);
        q->vn0 = dot(add(vel, cross(om, q->r)), q->n);
        if (q->vn0 >= -6.f) q->e = contactRestitution(c,0.1f);
        if (-q->vn0 > groundHit) groundHit = -q->vn0;
        if (LAND_DAMAGE_OLD && -q->vn0 > groundHitDmg) groundHitDmg = -q->vn0;   // ребро балки/парапета под днищем — опора, нормаль вверх
      }
    }
    if (hullN) {
      for (int it = 0; it < 2; it++)
        for (int k = 0; k < hullN; k++) { float d = cd[k] - dot(shift, ct[k].n); if (d > 0.f) shift = madd(shift, ct[k].n, d); }
      pos = add(pos, shift); comW = add(comW, shift);
      for (int k = 0; k < nPt; k++) ct[k].r = sub(add(cp[k], shift), comW);
      for (int k = nPt; k < hullN; k++) ct[k].r = sub(cp[k], comW);
#ifdef COL_DEBUG
      V3 velIn = vel, omIn = om;
#endif
      solve(c, &M, &vel, &om, ct, hullN, P->groundFriction, 0.f, 0.f, v3(0, 0, 0), 0);
#ifdef COL_DEBUG
      if (colDebug) {   // 388/377: точки днища/крыши — какой контакт даёт импульс
        printf("  hullCt %d shift %.4f %.4f %.4f vel %.3f %.3f %.3f -> %.3f %.3f %.3f om %.2f %.2f %.2f -> %.2f %.2f %.2f\n", hullN, shift.x, shift.y, shift.z,
               velIn.x, velIn.y, velIn.z, vel.x, vel.y, vel.z, omIn.x, omIn.y, omIn.z, om.x, om.y, om.z);
        for (int k = 0; k < hullN; k++) printf("   pt%s n %.3f %.3f %.3f p %.3f %.3f %.3f cd %.4f vn0 %.3f jn %.1f K %.1f\n", k >= nPt ? " edge" : "", ct[k].n.x, ct[k].n.y, ct[k].n.z,
                                              cp[k].x, cp[k].y, cp[k].z, cd[k], ct[k].vn0, ct[k].jn, ct[k].K);
      }
#endif
      carDmgGroundCall = 1; addDamage(c, groundHitDmg, 6.f); carDmgGroundCall = 0;
    }
  }
  // финальная проверка слоя кузова: подъём/наклон не должен вгонять кузов в препятствие (посадка на край ящика,
  // навес): вытолкнуть ещё раз; не выходит — без выталкивания землёй; не выходит — отменить подшаг
  // Все решения — по colProbeCar в итоговом положении (colPushCar держит наклонный «пол» от центра на момент вызова,
  // после большого сдвига его «чисто» может не совпасть с пробой): принятое положение не глубже, чем было в начале.
  if (!revert) {
    float okV = fmaxf(PEN_REVERT, pen0);
    CarBox vb; boxAt(c, pos, &M, &vb); colProbeCar(w, &vb);
    if (vb.maxDepth > okV) {
      int fixed = 0;
      CarBox pb = vb; colPushCar(w, &pb, &mx, &mz, &nx, &nz);
      if (!pb.behindFence && !crossAt(w, &pb, bx0, bz0, pb.x, pb.z) &&
          !crossAt(w, &pb, pos0.x, pos0.z, pos.x + mx, pos.z + mz)) {
        float my = pb.gx * mx + pb.gz * mz;
        V3 p1 = v3(pos.x + mx, pos.y + my, pos.z + mz); boxAt(c, p1, &M, &vb); colProbeCar(w, &vb);
        if (vb.maxDepth <= okV) {
          pos = p1; comW.x += mx; comW.z += mz; comW.y += my; fixed = 1;
          float vn = vel.x * nx + vel.z * nz; if (vn < 0.f) { vel.x -= vn * nx; vel.z -= vn * nz; }
        }
      }
      if (!fixed && (shift.x != 0.f || shift.y != 0.f || shift.z != 0.f)) {
        V3 p2 = sub(pos, shift); boxAt(c, p2, &M, &vb); colProbeCar(w, &vb);
        if (vb.maxDepth <= okV) { pos = p2; comW = sub(comW, shift); if (vel.y > 0.f) vel.y = 0.f; fixed = 1; }
      }
#ifdef COL_DEBUG
      if (colDebug) { CarBox tb; boxAt(c, pos, &M, &tb); colProbeCar(w, &tb); printf("  final: probe %.4f > ok %.4f (pen0 %.4f) shift %.3f %.3f %.3f fixed %d n %d types %d %d | box %.4f %.4f f %.4f %.4f y %.4f g %.4f %.4f -> %.4f\n", vb.maxDepth, okV, pen0, shift.x, shift.y, shift.z, fixed, vb.n, vb.n ? vb.type[0] : -1, vb.n > 1 ? vb.type[1] : -1, tb.x, tb.z, tb.fx, tb.fz, tb.y, tb.gx, tb.gz, tb.maxDepth); }
#endif
      if (!fixed) {
        pos = pos0; M = M0; memcpy(c->q, q0, sizeof q0); c->reverts++; c->revWhy = 1; revert = 1;
        vel = v3(vel.x * -0.2f, 0.f, vel.z * -0.2f); om = v3(0, 0, 0);
        comW = add(pos, mulM(&M, comB));
        boxAt(c, pos, &M, &vb); colProbeCar(w, &vb);
      }
    }
    c->pen = vb.maxDepth;
  }
  // зажало (не вытолкнулось): выталкиваем из положения начала подшага, если там станет мельче — туда
  if (revert & 1) {
    CarBox pb; boxAt(c, pos, &M, &pb); colPushCar(w, &pb, &mx, &mz, &nx, &nz);
    float my = pb.gx * mx + pb.gz * mz;
    V3 p1 = v3(pos.x + mx, pos.y + my, pos.z + mz); CarBox qb; boxAt(c, p1, &M, &qb); colProbeCar(w, &qb);
    if (qb.maxDepth < c->pen - 0.002f && !qb.behindFence && !crossAt(w, &qb, bx0, bz0, qb.x, qb.z) &&
        !crossAt(w, &qb, pos0.x, pos0.z, p1.x, p1.z)) { pos = p1; comW.x += mx; comW.z += mz; comW.y += my; c->pen = qb.maxDepth; }
  }
  c->nhkeep = colHumpRiding(c->hkeep, COL_HUMP_KEEP);   // правка 200: горбы под кузовом — на следующий подшаг
  c->nfkeep = colFolRiding(c->fkeep, COL_FOL_KEEP);      // правка 304: пропущенные заборы-изгороди — на следующий подшаг
  c->nbkeep = colBackRiding(c->bkeep, COL_BACK_KEEP);    // сверка PS2 09.10: заборы, через которые кузов выезжает с изнанки
  c->nofitT = (revert & 1) ? c->nofitT + dt : 0.f;
  if (wallHit > c->hitSpeed) c->hitSpeed = wallHit;
  if (groundHit > c->hitSpeed) c->hitSpeed = groundHit;

  // посадка на куст (пункт 7): днище о куст гасит удар (упругость 0, выше), колёса — подвеска; отдельного подскока нет
  // (правка 71, 01.10). Прежний подскок min(2.5, 0.2 × удар) — только для отрицательного контроля FOLIAGE_PREV
#ifdef FOLIAGE_PREV
  if (wasAir && !revert && (ng > 0 || hullN > 0)) {
    int soft = hullSoft, hard = hullN - hullSoft;
    for (int i = 0; i < 4; i++) if (hit[i] && !vd[i] && c->comp[i] > 0.f) { if (wmat[i] == COL_MAT_FOLIAGE) soft++; else hard++; }
    float impact = -vyIn;
    if (soft > hard && impact > FOLIAGE_LAND_MIN && c->foliageCooldown <= 0.f && M.U.y > .35f) {
      vel.y = fmaxf(vel.y, fminf(FOLIAGE_LAND_MAX, FOLIAGE_LAND_K * impact));
      c->foliageCooldown = .65f; c->foliageBounces++;
    }
  }
#else
  (void)wasAir; (void)vyIn;
  // переезд низкой изгороди: опора-куст (хотя бы одно колесо или точка днища на кусте) не придаёт машине вертикальной
  // скорости больше FOLIAGE_VY — куст сминается, а не работает трамплином (правило порта, правка 71)
  if (!revert && vel.y > FOLIAGE_VY) {
    int soft = hullSoft, hard = hullN - hullSoft;
    for (int i = 0; i < 4; i++) if (hit[i] && !vd[i] && c->comp[i] > 0.f) { if (wmat[i] == COL_MAT_FOLIAGE) soft++; else hard++; }
    (void)hard; if (soft > 0) vel.y = FOLIAGE_VY;
  }
#endif

  if (CAR_FOLIAGE_PASS && colFoliageHits()) {   // правка 173: сквозь куст/изгородь — вязнет и шуршит
#if FOLIAGE_DRAG_BOX   // прежнее (до правки 304): вязнет в полную силу, едва габариты кузова и куста пересеклись (красный гейта)
    float cover = 1.f;
#else                  // правка 304: пропорционально доле кузова в листве (col.c colFoliageCover); рядом с кустом — 0
    CarBox fb; boxAt(c, pos, &M, &fb); float cover = colFoliageCover(w, &fb);
#endif
    c->foliCover = cover;
    if (cover > 0.f) {
      float k = expf(-FOLIAGE_DRAG * cover * dt);
      if (!CAR_BUSH_SOFT_OLD) { float s0 = sqrtf(vel.x * vel.x + vel.z * vel.z); if (s0 <= FOLIAGE_DRAG_FLOOR) k = 1.f; else if (s0 * k < FOLIAGE_DRAG_FLOOR) k = FOLIAGE_DRAG_FLOOR / s0; }
      vel.x *= k; vel.z *= k; om = scl(om, k);
      float sp = sqrtf(vel.x * vel.x + vel.z * vel.z);
      if (c->foliageCooldown <= 0.f && sp > 2.f) { if (sp > c->foliV) c->foliV = sp; c->foliageCooldown = .45f; }
    }
  } else c->foliCover = 0.f;
#if !CAR_REST_TYRE_OLD
  // Static friction's settled state: reject sub-millimetre numerical creep,
  // but wake for input, impulses, a steep grade, a missing wheel or a body hit.
  // A stable support keeps the pose; nothing is snapped to a new surface.
  int quietSupport=ng==4 && nw==4 && !hullN && !c->contacts && !c->bodyHit &&
    thr==0.f && brk==0.f && !hb && speed<.005f && len(vel)<.005f &&
    len(om)<.001f && len(v3(c->w[0],c->w[1],c->w[2]))<.001f;
  for(int i=0;i<4 && quietSupport;i++)
    if(onsup[i] || vd[i] || gn[i].y<.9975f ||
       (!CAR_REST_SOFT_OLD && wmat[i]==COL_MAT_FOLIAGE))quietSupport=0;
  if(!CAR_REST_SOFT_OLD && c->foliCover>0.f)quietSupport=0;
#ifdef CAR_REST_SLEEP_TEST
  extern void carRestSleepTest(const Car *,int);
  carRestSleepTest(c,quietSupport);
#endif
  if(quietSupport) {
    pos=pos0;M=M0;memcpy(c->q,q0,sizeof q0);
    vel=om=v3(0.f,0.f,0.f);c->slip=0;
  }
  if(quietSupport&&fabsf(c->steer)<1e-3f) {
    if(c->quietSteps<1<<20)c->quietSteps++;
    if(c->quietSteps==CAR_REST_SKIP_STEPS) { c->restPose[0]=pos.x;c->restPose[1]=pos.y;c->restPose[2]=pos.z;memcpy(c->restPose+3,c->q,sizeof c->q); }
  } else c->quietSteps=0;
#else
  c->quietSteps=0;
#endif
  // ---- запись состояния
  c->x = pos.x; c->y = pos.y; c->z = pos.z;
  c->vel[0] = vel.x; c->vel[1] = vel.y; c->vel[2] = vel.z; c->w[0] = om.x; c->w[1] = om.y; c->w[2] = om.z;
  syncOut(c);
  if (revert) c->h = h0;
  int was = c->grounded;
  c->wheels = revert ? c->wheels : nw;
  c->grounded = revert ? was : (ng > 0 || hullN > 0);
  c->bodyHit = hullN > 0;
  if (c->fenceT > 0.f) c->fenceT -= dt;

  // ---- 5) страховка
  float g_; int any = c->wheels > 0 || colGround(w, c->x, c->z, c->y + P->step + 1.f, -1e6f, &g_, 0);
  if (!c->grounded) {
    if (was) {
      c->flightZone = -1;
      for (unsigned j = 0; j < w->nzone; j++)
        if (colZoneContains(w->zone + j, pos0.x, pos0.y + .5f, pos0.z, P->halfL)) { c->flightZone = (int)j; break; }
      c->fallTop = c->y; c->fallT0 = c->t; jumpBoost(c, w);
    }   // высота отрыва; зона прыжка JMP1
    c->airT += dt;
  } else {
    c->flightZone = -1;
    jumpZoneDrive(c, w, thr, dt);   // правка 342: разгон в зоне прыжка, пока колёса на земле
    // №377: a valid landing stays in the city, regardless of height lost
    // Void/embedded-body/DEATH recovery still checks actual invalid state
    if (CAR_LANDING_RESET_OLD && !was && c->fallTop - c->y > P->fallReset && !c->fallPending) { c->fallPending = 1; c->fallPendT = 0; }
    c->airT = 0;
  }
  float up = M.U.y;
  // без опоры под машиной: страховка от провала сквозь мир. Полёт с трамплина над канавой/рекой без коллизии (зона прыжка,
  // правка 47), пока машина не ниже высоты отрыва на 2 м, — не провал (раньше ускорения полёт над пустотой был короче 1.5 с)
  c->noGroundT = (any || (c->boosts && !c->grounded && c->y > c->fallTop - 2.f)) ? 0.f : c->noGroundT + dt;
  c->bellyT = c->grounded && c->wheels == 0 && up >= 0.35f && speed < 1.f ? c->bellyT + dt : 0.f;   // сел на днище
  c->flipT = hullN && up < 0.35f && speed < 3.f ? c->flipT + dt : 0.f;                              // на боку/крыше
  c->embedT = c->pen > EMBED_PEN ? c->embedT + dt : 0.f;
  if (c->fallPending) c->fallPendT += dt;
  // Аудит 09.10 п. 17 (сверка стен F9-5 lawn010): машина 2 с на днище (2 колеса на земле) без надёжного места в памяти (только что
  // поставлена) уносилась в спавн уровня — на 386 м. До замера PS2 (вопрос наблюдателю п. 17) правило порта: на днище без надёжного
  // места — на месте (carPlace ставит на опору под собой), через карту не переносим; с надёжным местом — как было.
  // Прежнее: -DCAR_BELLY_HOME_OLD=1
#ifndef CAR_BELLY_HOME_OLD
#define CAR_BELLY_HOME_OLD 0
#endif
  if (!CAR_BELLY_HOME_OLD && c->bellyT > 2.f && c->nsafe == 0 && !(c->y < -150.f || c->noGroundT > 1.5f || c->embedT > 0.5f || c->airT > 8.f || c->nofitT > 0.5f)) {
    float t = c->t; int rec = c->recoveries, rev = c->reverts;
    carPlace(c, w, c->x, c->y + 1.f, c->z, c->h);
    c->t = t; c->recoveries = rec + 1; c->reverts = rev; c->safeT = t; return;
  }
  if (c->y < -150.f || c->noGroundT > 1.5f || c->embedT > 0.5f || c->airT > 8.f || c->bellyT > 2.f || c->nofitT > 0.5f) { recover(c, w, c->t - 1.f); return; }
  if (c->fallPending && c->fallPendT > 1.f) { recover(c, w, c->fallT0 - 0.5f); return; }
  if (c->flipT > 2.f) {   // лежит — ставим на колёса на том же месте (как в оригинале)
    float t = c->t; int rec = c->recoveries, rev = c->reverts, rg = c->rights; float dmg = c->damage, dt0 = c->deadT; int ds = c->destroyed;
    carPlace(c, w, c->x, c->y + 1.f, c->z, c->h);
    CarBox pb; carBox(c, &pb); colProbeCar(w, &pb);
    c->t = t; c->recoveries = rec; c->reverts = rev; c->rights = rg + 1; c->damage = dmg; c->destroyed = ds; c->deadT = dt0;   // правка 199: часы взрыва
    if (pb.maxDepth > PEN_REVERT) recover(c, w, c->t);
    return;
  }
  if (c->grounded && c->wheels == 4 && up > 0.9f && !c->fallPending && c->pen < 0.01f && c->contacts == 0 && !hullN && !c->supOn &&
      c->t - c->safeT >= SAFE_EVERY) {
    pushSafe(c); c->safeT = c->t;
  }
}

// Правка 252 (03.10, зависание всей PSP у особняка F10-4/F10-5): NaN или бесконечность в состоянии машины. На PSP ловушки FPU
// по умолчанию включены (FCR31 0x00000e00 при старте потока — по PPSSPP; на железе покажет шапка ящика «fpu»): операция, давшая
// такое число, раньше была исключением процессора; теперь ловушки выключены (viewer/fpuguard.h), и машина с NaN осталась бы
// «нигде»: возвращаем на надёжное место, счёт — в чёрный ящик (поле fpe «nanN»). Проверка по битам (без операций FPU).
// В обычной игре не срабатывает — прежние числа те же
unsigned carNanFixes;
static int finBits(float f) { union { float f; unsigned u; } v; v.f = f; return (v.u & 0x7f800000u) != 0x7f800000u; }
static int carFinite(const Car *c) {
  const float a[] = {c->x, c->y, c->z, c->vel[0], c->vel[1], c->vel[2], c->w[0], c->w[1], c->w[2], c->q[0], c->q[1], c->q[2], c->q[3], c->acc, c->h};
  for (unsigned i = 0; i < sizeof a / sizeof *a; i++) if (!finBits(a[i])) return 0;
  return 1;
}
#ifndef DEADEND32_OLD
#define DEADEND32_OLD 0   // 1 — машина, у которой подшаги отменяются подряд на скорости ~0 (причины 1/2/4/8/16/32), стоит навсегда
#endif
#define DEADEND32_N 60    // подшагов с отменой подряд (~0,5 с при 2 подшагах за кадр 60 Гц)
// Общий выход из тупика (№ J9-4 L1: капсула куста 105, дыра в земле под ней — любой шаг «земля выше, чем можно въехать» (32); L2/L5 забор 402: машина
// на верху стены, шаг не вытесняется (1)): после DEADEND32_N отменённых подшагов подряд при скорости ~0 машину выдавливает на ближайшую допустимую
// точку вокруг (кольца 0,5…8 м, сначала назад по кузову, не выше шага вверх и до 10 м вниз; сначала — только вниз с опоры (верх стены/крыши)): земля твёрдая и не купол куста, вне мягкого куста,
// кузов без проникновения. Поворот (курс) тот же, скорость 0. У PS2 машину из такого положения выталкивает. DEADEND32_OLD=1 — прежнее
static int escapeOutEx0(Car *c, const ColWorld *w, int pass0, int nRing, int rMin, float *tgt) {   // pass0 1 — без прохода «только вниз с опоры»; nRing — сколько колец (0,5…8 м) пробовать
  float fq[4]; orientQ(fq, c->h, 0.f, 0.f); M3 M = quatM(fq);
  float fx = sinf(c->h), fz = cosf(c->h), lim0 = c->y + fmaxf(c->p.step, .6f) + .3f;
  static const float RR[14] = {.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, 5.f, 6.5f, 8.f, 12.f, 16.f, 24.f, 32.f, 40.f};   // кольца 9…13 — только широкий поиск зажатой (WEDGE738_WIDE)
  for (int pass = pass0; pass < 2; pass++) for (int ri = rMin; ri < nRing; ri++) for (int k = 0; k < 16; k++) {   // проход 0 — только вниз с опоры (машина на верху стены/крыши), 1 — любая допустимая точка
    float a = 3.14159265f + (k & 1 ? 1 : -1) * ((k + 1) >> 1) * 0.3926991f, ca = cosf(a), sa = sinf(a);   // от «назад» по кузову, поочерёдно влево/вправо
    float dx = fx * ca - fz * sa, dz = fx * sa + fz * ca, qx = c->x + dx * RR[ri], qz = c->z + dz * RR[ri], gq, nq[3]; int mq = 0; uint32_t rq = 0;
    if (!colGroundMatId(w, qx, qz, lim0, c->y - 10.f, &gq, nq, &mq, &rq) || mq == COL_MAT_FOLIAGE || (rq >> 29) == COL_CYL || nq[1] < .9f) continue;
    if (pass == 0 && gq > c->y - 1.2f) continue;
    if (colFoliageNear(w, qx, gq + .5f, qz, c->p.halfW + 1.2f)) continue;
    CarBox pb; boxAt(c, v3(qx, gq, qz), &M, &pb);
    if (colProbeCar(w, &pb) && pb.maxDepth > 0.05f) continue;
    if (tgt) { tgt[0] = qx; tgt[1] = gq + .3f; tgt[2] = qz; return 1; }   // только найти точку (плавный выход)
    float t = c->t; int rec = c->recoveries, rev = c->reverts, e32 = c->escapes32;
    carPlace(c, w, qx, gq + .3f, qz, c->h);
    c->t = t; c->recoveries = rec; c->reverts = rev; c->escapes32 = e32 + 1; c->stuck32 = 0;
    return 1;
  }
  return 0;
}
static int escapeOutEx(Car *c, const ColWorld *w, int pass0, int nRing, int rMin, float *tgt);
static int escapeOut(Car *c, const ColWorld *w) { return escapeOutEx(c, w, 0, 9, 0, 0); }
// L6 (-157,-32,55) seg#116 (бот-рой 24.10, трасса seed 2971948 кадры 2904–2922): поиск выхода (перч, №738, тупик 32) идёт внутри шага машины, где забор держит только с лица
// (colSetFenceBack(1), память заборов bkeep), поэтому точка с центром НА линии забора проходит проверку «без проникновения»; а плавный выход кончается carPlace → settle уже
// вне шага, где забор твёрд с обеих сторон, и settle выдавливает машину на ближайшую сторону забора (3,5 м, на изнанку: «проезд сквозь забор» при v=0).
// Теперь точки выхода проверяются так же, как их потом поставит carPlace: забор твёрд с обеих сторон. ESCAPE_FENCE_OLD=1 — как было
#ifndef ESCAPE_FENCE_OLD
#define ESCAPE_FENCE_OLD 0
#endif
static int escapeOutEx(Car *c, const ColWorld *w, int pass0, int nRing, int rMin, float *tgt) {
  if (ESCAPE_FENCE_OLD) return escapeOutEx0(c, w, pass0, nRing, rMin, tgt);
  colSetFenceBack(0); colBackKeep(0, 0);
  int r = escapeOutEx0(c, w, pass0, nRing, rMin, tgt);
  colSetFenceBack(1); colBackKeep(c->bkeep, c->nbkeep);   // вызовы только из carUpdate: там забор держит только с лица
  return r;
}

// Зависание на кромке (№339 остаток, J10-6 (193,1; 248,2) y 6,5: машина стоит носом вниз 17° на краю скоса, носом упёрта в угол забора, назад не выехать,
// ни одна из 8 проб не отъезжает больше 1,6 м; подшаги не отменяются, поэтому общий выход из тупика (DEADEND32) молчит; 17.10): газ или тормоз нажаты, машина
// на земле, скорость ~0, кузов наклонён больше PERCH_TILT (на ровном месте у стены крена нет), и она дольше PERCH_T с не уходила дальше PERCH_MOVE от места —
// её выдавливает на ближайшую допустимую плоскую точку (escapeOut, как DEADEND32). PERCH_ESCAPE_OLD=1 — прежнее
#ifndef PERCH_ESCAPE_OLD
#define PERCH_ESCAPE_OLD 0
#endif
#define PERCH_TILT 0.1745f   // 10°
#define PERCH_T 4.f
#define PERCH_T2 6.f
#define PERCH_T_BELLY 1.5f   // на днище (колёс на земле нет): раньше сторожа застревания (~2 с), чтобы машину не телепортировало на трамплин за десятки метров
#define PERCH_GLIDE 0.3f   // с: плавный сдвиг в найденную точку вместо мгновенного переноса (PERCH_GLIDE_OLD=1 — мгновенно)
#ifndef PERCH_GLIDE_OLD
#define PERCH_GLIDE_OLD 0
#endif
#define PERCH_MOVE 0.6f
static int escapeOutEx(Car *c, const ColWorld *w, int pass0, int nRing, int rMin, float *tgt);
static void perchGo(Car *c, const ColWorld *w, const float *t3) {
  c->perchEsc++; c->perchT = 0.f;
  if (PERCH_GLIDE_OLD) { float t = c->t; int rec = c->recoveries, rev = c->reverts, e32 = c->escapes32; carPlace(c, w, t3[0], t3[1], t3[2], c->h); c->t = t; c->recoveries = rec; c->reverts = rev; c->escapes32 = e32; return; }
  c->gx0 = c->x; c->gy0 = c->y; c->gz0 = c->z; c->gx1 = t3[0]; c->gy1 = t3[1]; c->gz1 = t3[2]; c->glideT = PERCH_GLIDE;
}
// шаг плавного выхода: позиция по сглаженной кривой к найденной точке, скорости нули, без обычных подшагов; в конце — обычная постановка (поза по земле)
static int perchGlide(Car *c, const ColWorld *w, float dt) {
  if (c->glideT <= 0.f) return 0;
  c->glideT -= dt; float u = 1.f - fmaxf(c->glideT, 0.f) / PERCH_GLIDE; u = u * u * (3.f - 2.f * u);
  if (c->glideT <= 0.f) { float t = c->t; int rec = c->recoveries, rev = c->reverts, e32 = c->escapes32; carPlace(c, w, c->gx1, c->gy1, c->gz1, c->h); c->t = t + dt; c->recoveries = rec; c->reverts = rev; c->escapes32 = e32; c->glideT = 0.f; return 1; }
  c->x = c->gx0 + (c->gx1 - c->gx0) * u; c->y = c->gy0 + (c->gy1 - c->gy0) * u; c->z = c->gz0 + (c->gz1 - c->gz0) * u;
  memset(c->vel, 0, sizeof c->vel); memset(c->w, 0, sizeof c->w); c->t += dt; c->acc = 0.f; c->ry = c->y; c->rvy = 0.f;
  return 1;
}
// №738 (Роб 17.10, видео IMG_7280 12:19–12:54: «застрял в ящике и у стены дома — из таких мест, как с покрышек, переносить на дорогу рядом, как после
// полиции»): машина стоит ровно (без крена — правило перча молчит), колёса на земле, а газ и задний ход оба пробовали, и за WEDGE_T с она не вышла из круга
// WEDGE_MOVE — зажата. Выход тем же поиском, что у перча (кольца рядом, на том же уровне, плавным сдвигом). Одно направление (упёрся в стену газом,
// назад не пробовал) — не трогаем: так можно толкать. WEDGE738_OLD=1 — как было
#ifndef WEDGE738_OLD
#define WEDGE738_OLD 0
#endif
// №738, остаток (бот-рой 24.10, берег L7 (109,−4,442), L4 (116,−7,429), L1): зажата на склоне 39° у пустоты, зависание № 738 срабатывает (газ и назад, ≥ 3 с, круг 1,5 м),
// но поиск выхода находит 0 ровных точек (n.y ≥ 0,9) в кольцах до 8 м: все 46 точек земли там с уклоном 39°; ближайшая ровная — 15 м (L7) и 32 м (L4, L1). Если за WEDGE_WIDE_T с зажатости
// обычный поиск не нашёл точки, ищем в кольцах 12…40 м (то же условие: ровно, на уровне, без проникновения). WEDGE738_WIDE_OLD=1 — как было
#ifndef WEDGE738_WIDE_OLD
#define WEDGE738_WIDE_OLD 0
#endif
#define WEDGE_WIDE_T 6.f
// Яма глубже всей ровной земли вокруг (L4 (125,−9,409): ложбина −10 м, ровное только выше шага вверх, в окне поиска [y−10, y+0,9] ровных 0 и в 40 м): выхода «рядом на том же уровне»
// нет, поэтому если и широкий поиск молчит ещё WEDGE_RECOVER_T − WEDGE_WIDE_T с — возврат на надёжное место, как сторож застревания (recover). WEDGE738_RECOVER_OLD=1 — как было
#ifndef WEDGE738_RECOVER_OLD
#define WEDGE738_RECOVER_OLD 0
#endif
#define WEDGE_RECOVER_T 9.f
#define WEDGE_T 3.f
#define WEDGE_TURN 0.35f  // рад (20°): разворот в тесноте меняет курс, зажатая машина — нет
#define WEDGE_MOVE 1.5f   // м: в тесноте машина качается туда-сюда, но из круга не выходит
static void wedgeEscape(Car *c, const ColWorld *w, const CarInput *in, float dt) {
  int fwd = in->throttle > .3f, back = in->brake > .3f;
  if (!(fwd || back)) return;
  if (!c->grounded || c->destroyed) { c->wedgeT = 0.f; c->wedgeTried = 0; return; }
  if (c->wedgeT <= 0.f) { c->wedgeX = c->x; c->wedgeZ = c->z; c->wedgeH = c->h; c->wedgeTried = 0; }
  float dh_ = fabsf(remainderf(c->h - c->wedgeH, 6.2831853f));
  if (hypotf(c->x - c->wedgeX, c->z - c->wedgeZ) > WEDGE_MOVE || dh_ > WEDGE_TURN) { c->wedgeT = 0.f; c->wedgeTried = 0; return; }   // ушла из круга или разворачивается (в три приёма в переулке) — свободна
  c->wedgeT += dt; c->wedgeTried |= (fwd ? 1 : 0) | (back ? 2 : 0);
  if (c->wedgeTried != 3 || c->wedgeT < WEDGE_T) return;
  if ((int)(c->wedgeT * 4.f) == (int)((c->wedgeT - dt) * 4.f)) return;   // неудачный поиск — повтор раз в 0,25 с
  float t3[3]; int rMin = c->wedgeEsc >= 2 ? 2 : 0;
  if (escapeOutEx(c, w, 1, 5, rMin, t3) || escapeOutEx(c, w, 0, 9, 0, t3) || (!WEDGE738_WIDE_OLD && c->wedgeT >= WEDGE_WIDE_T && escapeOutEx(c, w, 1, 14, 9, t3))) { c->wedgeEsc++; c->wedgeT = 0.f; c->wedgeTried = 0; perchGo(c, w, t3); return; }
  if (!WEDGE738_RECOVER_OLD && c->wedgeT >= WEDGE_RECOVER_T) { c->wedgeEsc++; c->wedgeT = 0.f; c->wedgeTried = 0; recover(c, w, c->t - 1.f); }
}
static void perchEscape(Car *c, const ColWorld *w, const CarInput *in, float dt) {
  // №377 (L7 jumpzone9: после прыжка машина садится днищем на верх тонкой стены (ящик 234, 0,58 м) колёсами 0 и ползёт/стоит до сброса сторожем застревания, который телепортирует её на
  // трамплин за 36 м): «на днище, колёс на земле нет» — тоже зависание, срабатывает раньше сторожа (PERCH_T_BELLY) и сдвигает на землю рядом
  int belly = c->wheels == 0 && c->contacts > 0 && hypotf(c->vel[0], c->vel[2]) < 4.f;
  int want = (in->throttle > .3f || in->brake > .3f) && c->grounded && !c->destroyed && (hypotf(c->vel[0], c->vel[2]) < .4f || belly) && (belly || fabsf(c->pitch) > PERCH_TILT || fabsf(c->roll) > PERCH_TILT);
  if (want && !belly) {   // наклон кузова ОТНОСИТЕЛЬНО земли под центром: стоящая на склоне у забора под газом не «зависла» (угол между осью вверх кузова и нормалью земли)
    const float *q = c->q; float ux = 2 * (q[1] * q[2] - q[0] * q[3]), uy = 1 - 2 * (q[1] * q[1] + q[3] * q[3]), uz = 2 * (q[2] * q[3] + q[0] * q[1]), g, n[3];
    if (colGround(w, c->x, c->z, c->y + 1.5f, c->y - 3.f, &g, n)) { float d = ux * n[0] + uy * n[1] + uz * n[2]; if (d > cosf(PERCH_TILT)) want = 0; }
  }
  if (!want) { c->perchT = 0.f; if (!WEDGE738_OLD) wedgeEscape(c, w, in, dt); return; }
  c->wedgeT = 0.f; c->wedgeTried = 0;
  if (c->perchT <= 0.f) { c->perchX = c->x; c->perchZ = c->z; }
  if (hypotf(c->x - c->perchX, c->z - c->perchZ) > PERCH_MOVE) { c->perchT = dt; c->perchX = c->x; c->perchZ = c->z; return; }
  c->perchT += dt;
  if (c->perchT < (belly ? PERCH_T_BELLY : PERCH_T)) return;
#ifndef PERCH_RETRY_OLD
  if (c->perchT - dt >= (belly ? PERCH_T_BELLY : PERCH_T) && (int)(c->perchT * 4.f) == (int)((c->perchT - dt) * 4.f)) return;   // проверка 18.10: неудачный выход — повтор раз в 0,25 с, а не каждый кадр (до 80 проб на кадр)
#endif
  // сначала близко и на том же уровне (назад по кузову на плоскую точку, кольца до 3 м): выглядит как обычный сдвиг, а не прыжок через забор; далёкий выход (вниз с опоры, до 8 м) — только после PERCH_T2
  { int rMin = c->perchEsc >= 3 ? 3 : c->perchEsc >= 1 ? 2 : 0; float t3[3];   // повторное зависание в том же месте — выход дальше (1,5 м, потом 2 м+), 0,5 м не хватило
    if (escapeOutEx(c, w, 1, 5, rMin, t3)) { perchGo(c, w, t3); return; }
    if (c->perchT >= PERCH_T2 && escapeOutEx(c, w, 0, 9, 0, t3)) { perchGo(c, w, t3); return; } }
  if (c->perchT >= PERCH_T2) c->perchT = 0.f;
}

// №1050 диагностика (flag carcon1050 1): что держит машину — контакты кузова (тип, индекс, нормаль, глубина), колёсные контакты, откат подшага; строка CARCON в main_loop.h
CarDbg1050 carDbg1050; int carDbgOn1050;
void carUpdate(Car *c, const ColWorld *w, const CarInput *in, float dt) {
  unsigned xt_ = XS_T(), xw_ = XS_W();
  if (dt > 0.1f) dt = 0.1f;
  if (dt < 0.f) dt = 0.f;
  c->hitEpAge += dt;
  c->velPre[0] = c->vel[0]; c->velPre[1] = c->vel[1]; c->velPre[2] = c->vel[2];   // №923
  if (!PERCH_ESCAPE_OLD && perchGlide(c, w, dt)) { XS_LAP(14, xt_, xw_); return; }
  // правка 199: разбитая машина не слушает руль и газ (оригинал: после смертельного удара стоит), часы до взрыва
  static const CarInput none;
  CarInput husk;
  if (c->destroyed) {
#if !defined(CAR_DEAD_DRIVES) || !CAR_DEAD_DRIVES
    if (carHuskDrives(c)) {   // правка 355: остов едет медленно — газ не разгоняет выше CAR_HUSK_TOP (в обе стороны)
      husk = *in;
      if (c->v > CAR_HUSK_TOP && husk.throttle > 0.f) husk.throttle = 0.f;
      if (c->v < -CAR_HUSK_TOP && husk.brake > 0.f) husk.brake = 0.f;
      in = &husk;
    } else in = &none;
#endif
    if (c->deadT < CAR_BOOM_DELAY + 1.f) c->deadT += dt; } else c->deadT = 0.f;
  c->acc += dt; c->hitSpeed = 0; c->wallV = 0; c->hardV = c->foliV = c->carHitV = 0;
  int n = 0;
  colSetFoliagePass(CAR_FOLIAGE_PASS);   // правка 173: весь шаг машины — сквозь листву (и пробы после подшагов)
  colSetFenceBack(1);                    // сверка PS2 09.10: забор держит только с лица
  // Правка «плавность» (10.10, тур SHARTEST4: пешком у остова фаза машины 3,9 → 9–10,5 мс): стоящая на опоре машина без ввода (остов у
  // Гомера, брошенная машина) 4 подшага на кадр считала нулевой результат — покой уже держит quietSupport (поза и скорость те же).
  // От CAR_REST_SKIP_STEPS подшагов покоя, при нулевом вводе, нулевых скоростях, том же положении и повороте — подшаги не считаем
  // (время идёт). Любой ввод, скорость (удар, толчок), смещение или телепорт — обычный счёт. -DCAR_REST_SKIP_OLD=1 — прежнее
  int skip = 0;
  if (!CAR_REST_SKIP_OLD && c->quietSteps >= CAR_REST_SKIP_STEPS && in->throttle == 0.f && in->brake == 0.f && in->steer == 0.f && in->handbrake == 0.f &&
      c->vel[0] == 0.f && c->vel[1] == 0.f && c->vel[2] == 0.f && c->w[0] == 0.f && c->w[1] == 0.f && c->w[2] == 0.f && c->supOn == 0) {
    skip = c->x == c->restPose[0] && c->y == c->restPose[1] && c->z == c->restPose[2] && !memcmp(c->restPose + 3, c->q, sizeof c->q);
  }
  int parkIn = !CAR_PARK_SKIP_OLD && !c->destroyed && in->handbrake > .5f && in->throttle == 0.f && in->brake == 0.f && in->steer == 0.f && c->supOn == 0;
  int parkSkip = 0;
  if (!skip) {
    if (!parkIn) c->parkN = 0;
    else if (c->parkN >= CAR_PARK_N && c->x == c->parkPose[0] && c->y == c->parkPose[1] && c->z == c->parkPose[2] && !memcmp(c->parkPose + 3, c->q, sizeof c->q) &&
             fabsf(c->vel[0]) + fabsf(c->vel[1]) + fabsf(c->vel[2]) < CAR_PARK_V && fabsf(c->w[0]) + fabsf(c->w[1]) + fabsf(c->w[2]) < CAR_PARK_V && ++c->parkTick % CAR_PARK_EVERY) parkSkip = 1;
  }
  if (skip) { c->t += dt; c->acc = 0.f; c->quietSteps += (int)(dt / c->p.sub + .5f); }
  else if (parkSkip) { c->t += dt; c->acc = 0.f; }
  else while (c->acc >= c->p.sub * 0.999f && n < 16) {
    int r0 = c->reverts; substep(c, w, in, c->p.sub); c->acc -= c->p.sub; n++;
    if (!DEADEND32_OLD) {
      if (c->reverts != r0) c->stuck32++; else c->stuck32 = 0;
      if (c->stuck32 >= DEADEND32_N && hypotf(c->vel[0], c->vel[2]) < .8f && !c->destroyed) { if (!escapeOut(c, w)) c->stuck32 = DEADEND32_N / 2; }   // не нашли — повтор через полцикла
    }
  }
  if (c->acc < 0.f) c->acc = 0.f;
  XS_LAP(1, xt_, xw_); XS_ADD(3, n); XS_ADD(16, skip); XS_ADD(17, parkSkip);   // подшаги: время и счёт; пропуск покоя и сна (CAR_PARK)
  if (parkIn && !skip && !parkSkip) {   // посчитанный кадр покоя: сдвинулась ли поза против прошлого посчитанного
    float d_ = fabsf(c->x - c->parkPose[0]) + fabsf(c->y - c->parkPose[1]) + fabsf(c->z - c->parkPose[2]) + fabsf(c->q[0] - c->parkPose[3]) + fabsf(c->q[1] - c->parkPose[4]) + fabsf(c->q[2] - c->parkPose[5]) + fabsf(c->q[3] - c->parkPose[6]);
    if (d_ < CAR_PARK_EPS && fabsf(c->vel[0]) + fabsf(c->vel[1]) + fabsf(c->vel[2]) < CAR_PARK_V && fabsf(c->w[0]) + fabsf(c->w[1]) + fabsf(c->w[2]) < CAR_PARK_V && !c->contacts && !c->bodyHit) { if (c->parkN < 1 << 20) c->parkN++; } else c->parkN = 0;
        c->parkPose[0] = c->x; c->parkPose[1] = c->y; c->parkPose[2] = c->z; memcpy(c->parkPose + 3, c->q, sizeof c->q);
  }
  if (!PERCH_ESCAPE_OLD) perchEscape(c, w, in, dt);
#ifndef CAR_NAN_OLD   // -DCAR_NAN_OLD — прежнее (без проверки): красный контроль tests/crash_f10/nan.sh
  if (!carFinite(c)) { carNanFixes++; recover(c, w, c->t); }   // правка 252: NaN/inf — на надёжное место (выше)
#endif
  c->supOn = 0;                                   // опору-крышу carHitCar ставит заново каждый кадр
  // высота для камеры: пружина с критическим затуханием (телепорт — сразу)
  float off = c->ry - c->y, om = 25.f;
  if (fabsf(off) > 2.f) { off = 0.f; c->rvy = 0.f; }
  c->rvy += (-om * om * off - 2.f * om * c->rvy) * dt;
  off += c->rvy * dt;
  if (off > 0.3f) { off = 0.3f; if (c->rvy > 0) c->rvy = 0; }
  if (off < -0.3f) { off = -0.3f; if (c->rvy < 0) c->rvy = 0; }
  c->ry = c->y + off;
  c->rpitch = c->pitch; c->rroll = c->roll;
  colSetFoliagePass(0);   // правка 173: дальше (Гомер, пробы мира) — листва как была
  colHumpKeep(0, 0);      // правка 200: вне шага машины памяти горбов нет
  colFolKeep(0, 0);       // правка 304: и пропущенных заборов
  colSetFenceBack(0); colBackKeep(0, 0);   // сверка PS2 09.10: вне шага машины забор — как был (Гомер)
  XS_LAP(15, xt_, xw_);
}

// ------------------------------------------------------------------------------------------------ машина о машину
// какую долю сдвига (dx, dz) кузов c примет без углубления в мир (не глубже, чем был, и не больше PEN_REVERT) и без
// перехода забора с лица на изнанку: 1 — весь, 0 — никакой (подбор делением пополам, 4 шага)
static float shiftRoom(const Car *c, const ColWorld *w, float dx, float dz) {
  if (!w || (dx == 0.f && dz == 0.f)) return 1.f;
  M3 M = quatM(c->q); V3 pos = v3(c->x, c->y, c->z);
  CarBox b; boxAt(c, pos, &M, &b); colProbeCar(w, &b);
  float ok = fmaxf(PEN_REVERT, b.maxDepth), lo = 0.f, hi = 1.f;
  for (int it = 0; it < 5; it++) {
    float t = it ? (lo + hi) * .5f : 1.f;
    CarBox q; boxAt(c, v3(c->x + dx * t, c->y, c->z + dz * t), &M, &q); colProbeCar(w, &q);
    int good = q.maxDepth <= ok && !crossAt(w, &q, c->x, c->z, c->x + dx * t, c->z + dz * t);
    if (!it) { if (good) return 1.f; continue; }
    if (good) lo = t; else hi = t;
  }
  return lo;
}

// верхняя машина t на крыше нижней b (пункт 8): крыша — опора для колёс и днища t в следующем кадре (прямоугольник b,
// высота по оси вверх b, скорость b); провалившуюся ниже крыши поднять днищем на крышу; удар сверху неупругий по вертикали
static void stackOn(Car *t, Car *b) {
  M3 Mb = quatM(b->q); CarBox B; carBox(b, &B);
  float roof = b->y + Mb.U.y * (b->p.bodyHi+b->modelRaise);
  t->supOn = 1; t->supY = roof; t->supX = B.x; t->supZ = B.z; t->supFx = B.fx; t->supFz = B.fz;
  t->supHW = B.hw; t->supHL = B.hl; memcpy(t->supV, b->vel, sizeof t->supV);
  float bottom = t->y + t->p.bodyLo+t->modelRaise;
  if (bottom < roof) t->y += roof - bottom;
  float vrel = t->vel[1] - b->vel[1];
  if (vrel < 0.f && bottom <= roof + 0.05f) {                     // удар — только при касании крыши, не над ней
    float mt = bodyMass(t), mb = bodyMass(b), vc = (mt * t->vel[1] + mb * b->vel[1]) / (mt + mb);
    t->vel[1] = vc; b->vel[1] = vc;
    float hit = -vrel * mb / (mt + mb); if (hit > t->hitSpeed) t->hitSpeed = hit;
    if (hit > t->carHitV) { t->carHitV = hit; t->carHitP[0] = t->x; t->carHitP[1] = roof; t->carHitP[2] = t->z; }   // правка 135.3
  }
  syncOut(t); syncOut(b);
}

// №879 (Роб 23.10 после TEST50: «можем наехать на машину или заехать под неё — в зависимости от того, едем в горку или с горки; в оригинале в NPC
// врезались»): высота одной машины над другой мерилась по мировой отметке центров (a->y − b->y). На склоне 25 % машины в 5 м друг от друга
// расходятся по отметке на 1,2 м: нижняя «под» верхней (больше 2,5 м — удара нет вовсе), верхняя «на крыше» нижней (STACK_DY) — наезд и
// проныр. PS2 (объёмные тела) этого не знает. Теперь высота — вдоль общей оси «вверх» обеих машин (на склоне это поперёк склона, а не по отвесу),
// а толчок удара — вдоль склона (нормаль без составляющей по этой оси): удар не подбрасывает въезжающую на крышу. Гейт tests/carslope (уклоны
// 0–35 %, в горку/с горки, вдоль/поперёк). CARSLOPE879_OLD=1 — как было
#ifndef CARSLOPE879_OLD
#define CARSLOPE879_OLD 0
#endif
int carHitCar(Car *a, Car *b, const ColWorld *w) {
  V3 Uc = v3(0.f, 1.f, 0.f);
  if (!CARSLOPE879_OLD) { M3 Ma_ = quatM(a->q), Mb_ = quatM(b->q); V3 s_ = add(Ma_.U, Mb_.U); float l_ = len(s_);
    if (l_ > 1e-3f && s_.y > .7f * l_) Uc = scl(s_, 1.f / l_); }   // перевёрнутая / на боку — ось мира
  float sab = CARSLOPE879_OLD ? a->y - b->y : dot(v3(a->x - b->x, a->y - b->y, a->z - b->z), Uc);   // a над b поперёк склона
  if (fabsf(sab) > 2.5f) return 0;
  CarBox A, B; carBox(a, &A); carBox(b, &B);
  float ax[4][2] = {{A.fx, A.fz}, {-A.fz, A.fx}, {B.fx, B.fz}, {-B.fz, B.fx}};
  float dx = A.x - B.x, dz = A.z - B.z, depth = 1e9f, nx = 0, nz = 0;
  for (int k = 0; k < 4; k++) {
    float ux = ax[k][0], uz = ax[k][1];
    float ea = A.hw * fabsf(-A.fz * ux + A.fx * uz) + A.hl * fabsf(A.fx * ux + A.fz * uz);
    float eb = B.hw * fabsf(-B.fz * ux + B.fx * uz) + B.hl * fabsf(B.fx * ux + B.fz * uz);
    float o = dx * ux + dz * uz, ov = ea + eb - fabsf(o);
    if (ov <= 0.f) return 0;
    if (ov < depth) { depth = ov; nx = o < 0 ? -ux : ux; nz = o < 0 ? -uz : uz; }   // от B к A
  }
  // одна над другой (пункт 8): верхняя встаёт на крышу нижней, вбок не сталкивается
  if (sab >= STACK_DY * b->p.bodyHi) { stackOn(a, b); return 1; }
  if (-sab >= STACK_DY * a->p.bodyHi) { stackOn(b, a); return 1; }
  float ma = bodyMass(a), mb = bodyMass(b), sa = mb / (ma + mb), sb = ma / (ma + mb);
  // разведение с учётом мира (пункт 6: машина у стены не вдавливается в неё соседкой): каждая сдвигается, насколько
  // пускает мир; недостачу упёршейся берёт другая; упёрлись обе — остаётся перекрытие, его разведёт импульс
  float da = depth * sa, db = depth * sb;
  float ta = shiftRoom(a, w, nx * da, nz * da), tb = shiftRoom(b, w, -nx * db, -nz * db);
  float ga = da * ta, gb = db * tb, rest = depth - ga - gb;
  if (rest > 1e-4f && ta < 1.f && tb >= 1.f) gb = fmaxf(gb, (db + rest) * shiftRoom(b, w, -nx * (db + rest), -nz * (db + rest)));
  else if (rest > 1e-4f && tb < 1.f && ta >= 1.f) ga = fmaxf(ga, (da + rest) * shiftRoom(a, w, nx * (da + rest), nz * (da + rest)));
  a->x += nx * ga; a->z += nz * ga; b->x -= nx * gb; b->z -= nz * gb;
  float pax, paz, pbx, pbz; hitPoint(&A, nx, nz, &pax, &paz); hitPoint(&B, -nx, -nz, &pbx, &pbz);
  V3 p = v3((pax + pbx) * .5f, (a->y + b->y) * .5f + a->p.hitY, (paz + pbz) * .5f), n = v3(nx, 0.f, nz);
  if (!CARSLOPE879_OLD) { V3 t_ = sub(n, scl(Uc, dot(n, Uc))); float l_ = len(t_); if (l_ > .3f) n = scl(t_, 1.f / l_); }   // №879: толчок вдоль склона
  M3 Ma = quatM(a->q), Mb = quatM(b->q);
  V3 ca = add(v3(a->x, a->y, a->z), mulM(&Ma, v3(a->com[0], a->com[1], a->com[2])));
  V3 cb = add(v3(b->x, b->y, b->z), mulM(&Mb, v3(b->com[0], b->com[1], b->com[2])));
  V3 ra = sub(p, ca), rb = sub(p, cb);
  V3 va = v3(a->vel[0], a->vel[1], a->vel[2]), wa = v3(a->w[0], a->w[1], a->w[2]);
  V3 vb = v3(b->vel[0], b->vel[1], b->vel[2]), wb = v3(b->w[0], b->w[1], b->w[2]);
  V3 vrel = sub(add(va, cross(wa, ra)), add(vb, cross(wb, rb)));
  float vn = dot(vrel, n);
  if (vn < 0.f) {
    float e = 0.5f * (a->p.restitution + b->p.restitution) * 0.8f;
    float K = 1.f / effMass(a, &Ma, n, ra) + 1.f / effMass(b, &Mb, n, rb);
    float j = -(1.f + e) * vn / K;
    V3 J = scl(n, j);
    va = madd(va, J, 1.f / ma); wa = add(wa, invIw(a, &Ma, cross(ra, J)));
    vb = madd(vb, J, -1.f / mb); wb = sub(wb, invIw(b, &Mb, cross(rb, J)));
    memcpy(a->vel, &va, sizeof a->vel); memcpy(a->w, &wa, sizeof a->w); memcpy(b->vel, &vb, sizeof b->vel); memcpy(b->w, &wb, sizeof b->w);
    // урон: скорость удара, которую «почувствовала» каждая (лёгкая меняет скорость сильнее)
    float ka = a->hitDmgK > 0.f ? a->hitDmgK : CAR_HIT_DMG_K, kb = b->hitDmgK > 0.f ? b->hitDmgK : CAR_HIT_DMG_K;   // правка 176: трафик
    addDamage(a, -vn * 2.f * sa * ka, 2.f); addDamage(b, -vn * 2.f * sb * kb, 2.f);
    if (-vn > a->hitSpeed) a->hitSpeed = -vn;
    if (-vn > b->hitSpeed) b->hitSpeed = -vn;
    // правка 135.3: удар машина-машина — свой звук (sndCarHitCar), не путь удара о стену
    if (-vn > a->carHitV) { a->carHitV = -vn; memcpy(a->carHitP, &p, sizeof a->carHitP); }
    if (-vn > b->carHitV) { b->carHitV = -vn; memcpy(b->carHitP, &p, sizeof b->carHitP); }
  }
  syncOut(a); syncOut(b);
  return 1;
}

void carPieceMatrix(const Car *c, float lift, float ox, float oy, float oz, float sc, float m[16]) {
  // модель: X = вправо = -L, Y = U, Z = назад = -F
  float q[4]; memcpy(q, c->q, sizeof q); M3 M = quatM(q);
  float Rx = -M.L.x, Ry = -M.L.y, Rz = -M.L.z, Ux = M.U.x, Uy = M.U.y, Uz = M.U.z, Fx = M.F.x, Fy = M.F.y, Fz = M.F.z;
  float Tx = c->x + Ux * lift, Ty = c->y + Uy * lift, Tz = c->z + Uz * lift;
  m[0] = Rx * sc; m[1] = Ry * sc; m[2] = Rz * sc; m[3] = 0;
  m[4] = Ux * sc; m[5] = Uy * sc; m[6] = Uz * sc; m[7] = 0;
  m[8] = -Fx * sc; m[9] = -Fy * sc; m[10] = -Fz * sc; m[11] = 0;
  m[12] = Tx + Rx * ox + Ux * oy - Fx * oz; m[13] = Ty + Ry * ox + Uy * oy - Fy * oz; m[14] = Tz + Rz * ox + Uz * oy - Fz * oz; m[15] = 1;
}
