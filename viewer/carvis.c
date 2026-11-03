// Визуальный слой машины игрока: реализация (см. carvis.h, docs/car_spec.md).
#include "gevb.h"
#ifndef GEVB_CARSHADOW_OLD
#define GEVB_CARSHADOW_OLD 0   // -D…=1 — вершины тени/бликов машины в список GE, как раньше
#endif
#ifndef GEVB_FLARE_OLD
#define GEVB_FLARE_OLD 0
#endif
#include "carvis.h"
#include "fx.h"   // №839: fxGlowGlint
#include "carwheels.h"
#include "groundshade.h"
#include "skin.h"
#include <pspgu.h>
#include <math.h>
#include <string.h>

// --- подобрано на глаз / гипотезы (в данных игры этого нет, см. docs/car_spec.md §2 и §7) ---
#define HINGE_K 40.f          // пружина шарнира, 1/с² (умножается на stiffness из PhysicsJoint)
#define HINGE_C 3.f           // затухание, 1/с
#define HINGE_G 0.35f         // доля момента от силы тяжести/ускорения кузова (иначе капот лежит закрытым)
#define HINGE_REST 0.6f       // отщёлкнутая часть тянется к max*0.6 (сломанный замок: приоткрыта)
#define HINGE_BOUNCE 0.3f     // отскок от упоров 0 и max
#ifndef CAR_HINGE_FROZEN_FORCE_OLD
#define CAR_HINGE_FROZEN_FORCE_OLD 0
#endif
#ifndef CAR_HINGE_COM_VELOCITY_OLD
#define CAR_HINGE_COM_VELOCITY_OLD 0
#endif
#ifndef CAR_HINGE_FIRST_SAMPLE_OLD
#define CAR_HINGE_FIRST_SAMPLE_OLD 0
#endif
#define ZONE_TEX 0.12f        // зона набрала столько урона (доля 0..1) -> её текстура битая
#define ZONE_OPEN 0.20f       // ...а столько -> её часть отщёлкивается
#define ALL_TEX (1.f / 3.f)   // общий урон >= 1/3 -> все 4 текстуры битые, капот и багажник отщёлкнуты
#define ALL_OPEN (2.f / 3.f)  // общий урон >= 2/3 -> отщёлкнуты и двери
#ifndef SV_DAMVIS_OLD
#define SV_DAMVIS_OLD 0
#endif
#ifndef SV_CARCOST1159_OLD
#define SV_CARCOST1159_OLD 0   // №1159: 1 — без кэша матриц куска (считать заново на каждый кусок)
#endif
#ifndef SV_STOPLIGHT2_OLD
#define SV_STOPLIGHT2_OLD 0
#endif
#ifndef SV_STOPLIGHT_OLD
#define SV_STOPLIGHT_OLD 0
#endif
#define SHADOW_SOFT 0.35f     // ширина мягкого края тени, м
#define SHADOW_A 130.f        // непрозрачность середины тени (0..255)
#ifndef CAR_SHADOW_V2_OLD
#define CAR_SHADOW_V2_OLD 0
#endif
// №522 (Роб, железо, сборка 0516: «тень ломается и уходит под текстуры земли»): одна плоскость по опоре под центром (№433) на склонах, бордюрах и стыках клеток
// уходит под землю углами. Теперь высота — по опоре под КАЖДЫМ узлом сетки 6x6 (в полосе ±0,75 м от плоскости), рост сразу, спад за 100 мс; подъём над опорой
// 0,10 м плюс до 0,07 м по крутизне; узел без опоры (край, обрыв) гаснет по альфе, а не проваливается; мягкий край — кольцо узлов по краю. Тень рисуется после
// города и подложки, без записи глубины, со смещением глубины 4. В воздухе — прежняя плоскость. SHADOW_OLD=1 — прежнее (№433)
#ifndef SHADOW_OLD
#define SHADOW_OLD 0
#endif
#define SH_N 6
#ifndef SH_LIFT
#define SH_LIFT .10f   // подъём над опорой, м (плюс до 0,07 по крутизне): нарисованная земля местами выше опоры на 5–10 см
#endif
#define FLARE_DIST 60.f       // дальше блики не рисуем

static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }

static float hingeAcceleration(const CarHinge *h, float angle, float velocity, const float force[3]) {
  float t = h->sign * angle, ct = cosf(t), st = sinf(t), r[3];
  if (h->axis == 0) { r[0] = h->cx; r[1] = h->cy * ct - h->cz * st; r[2] = h->cy * st + h->cz * ct; }
  else { r[0] = h->cx * ct + h->cz * st; r[1] = h->cy; r[2] = -h->cx * st + h->cz * ct; }
  float r2 = r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + 1e-3f;
  float tq = h->axis == 0 ? r[1] * force[2] - r[2] * force[1] : r[2] * force[0] - r[0] * force[2];
  return h->sign * tq / r2 * HINGE_G - HINGE_K * h->stiff * (angle - h->max * HINGE_REST) - HINGE_C * velocity;
}

static int findJoint(const CarVis *v, const char *n) {
  for (int i = 0; i < v->njoint; i++) if (!strncmp(v->joint[i].name, n, 12)) return i;
  return -1;
}

void carVisReset(CarVis *v) {
  memset(v->spin, 0, sizeof v->spin); memset(v->th, 0, sizeof v->th); memset(v->thv, 0, sizeof v->thv);
  for (int i = 0; i < CARVIS_MAXH; i++) v->latched[i] = 1;
  memset(v->zoneDam, 0, sizeof v->zoneDam); v->prevDamage = 0.f; memset(v->prevVel, 0, sizeof v->prevVel);
  v->motionValid = 0;
  v->brake = v->reverse = 0;
  v->shadowValid = 0; v->shadowDt = 1.f / 60.f; v->shadowGrid = 0;
}

int carVisInit(CarVis *v, const void *sec, unsigned size) {
  memset(v, 0, sizeof *v);
  const unsigned char *b = sec;
  if (size < 80 || memcmp(b, "CAR1", 4)) return -1;
  const unsigned *u = (const unsigned *)(b + 4);
  v->njoint = u[1]; v->nhinge = u[2]; v->ndam = u[3]; v->nflare = u[4];
  if (v->njoint > CARVIS_MAXJ || v->nhinge > CARVIS_MAXH || v->ndam > CARVIS_MAXD || v->nflare > CARVIS_MAXF) return -2;
  unsigned need = 80 + 28 * v->njoint + 24 * v->nhinge + 8 * v->ndam + 36 * v->nflare;
  if (need > size) return -3;
  const float *f = (const float *)(b + 32);
  v->lift = f[0]; v->huskLift = f[1]; v->wheelRad = f[2] > .05f ? f[2] : .38f; v->maxSteer = f[3];
  memcpy(v->shadowAdj, f + 4, sizeof v->shadowAdj);
  const unsigned char *p = b + 80;
  memcpy(v->joint, p, 28 * v->njoint); p += 28 * v->njoint;
  memcpy(v->hinge, p, 24 * v->nhinge); p += 24 * v->nhinge;
  memcpy(v->dam, p, 8 * v->ndam); p += 8 * v->ndam;
  memcpy(v->flare, p, 36 * v->nflare);
  // car.comp[]: FL FR RL RR -> суставы колёс w0..w3 по их месту в модели (перед — z < 0, модель смотрит в -Z; лево — x < 0):
  // у famil_v это w3 w2 w1 w0, у honor_v и lisa_v w2 — левое переднее (имена у машин разные, место — данные)
  for (int i = 0; i < 4; i++) v->wheelJ[i] = -1;
  for (int k = 0; k < 4; k++) {
    char nm[3] = {'w', (char)('0' + k), 0}; int j = findJoint(v, nm); if (j < 0) continue;
    int slot = (v->joint[j].z < 0.f ? 0 : 2) + (v->joint[j].x < 0.f ? 0 : 1);
    if (v->wheelJ[slot] < 0) v->wheelJ[slot] = j;
  }
  v->slJ = findJoint(v, "sl");
  v->hideJ = 0; for (int i = 0; i < 4; i++) v->wheelUp[i] = 1e9f;   // до carVisSetCar — как прежде
  carVisReset(v);
  v->ok = 1;
  return 0;
}

void carVisSetCar(CarVis *v, const char *name) {
  // [PS2 код SLES 0x27a140] у frink_v, honor_v, hbike_v, witchcar, ship, mono_v (rocke_v — w2, w3) куски на суставах "w%d" не
  // рисуются: колёса вылеплены в кузове, на суставах — заглушки. Ход вверх — не больше запаса модели (tools/wheel_clear.py)
  unsigned hide = carWheelHide(name); v->hideJ = 0; v->capStill = carWheelCapStill(name);   // №878
  for (int k = 0; k < 4; k++) {
    char nm[3] = {'w', (char)('0' + k), 0}; int j = findJoint(v, nm); if (j < 0 || j >= 32) continue;
    if (hide >> k & 1) v->hideJ |= 1u << j;
    for (int i = 0; i < 4; i++) if (v->wheelJ[i] == j) v->wheelUp[i] = carWheelUp(name, k, 1e9f);
  }
}

// №955б (Роб 25.10, TEST59: «колёса не крутятся у NPC и у многих других»; кадры: у машины трафика колёса крутятся, а у угнанной/машины миссии под игроком угол качения стоял — carVisUpdate
// для неё не вызывается (mvForcedCar), cv.spin оставался нулевым при любой скорости). Качение колёс одно: угол по пройденному пути, радиус — у машины под игроком (c->p.wheelRad).
// FORCEDSPIN955_OLD=1 — как было
#ifndef FORCEDSPIN955_OLD
#define FORCEDSPIN955_OLD 0
#endif
// сверка g01-cars, P01 Д3 + P01-23: ведущие колёса не вращаются (угол 0), пока держат ручник, газа нет и колесо на земле — на любой скорости, и стоя; при пробуксовке (старт с места, торможение
// с ходом назад ниже 50 км/ч, разворот задним ходом) крутятся с постоянной скоростью ≈30 % TopSpeed (famil_v 27 рад/с) в сторону газа/тормоза. Было: стоп только в заносе 2, быстрые колёса только в заносе 1.
// -DSV_WHEELSPIN_OLD=1
#ifndef SV_WHEELSPIN_OLD
#define SV_WHEELSPIN_OLD 0
#endif
static float wheelSpinRate(const Car *c, int i, float w, float rad) {
  if (SV_WHEELSPIN_OLD) {
    if (i >= 2 && c->slip == 2) return 0.f;
    if (i >= 2 && c->slip == 1) return (c->v >= 0.f ? 1.f : -1.f) * fmaxf(fabsf(w), 25.f);
    return w;
  }
  if (i >= 2) {
    if (c->lastHb && c->lastThr <= 0.f && c->comp[i] > 0.f) return 0.f;
    if (c->spinDrive) return (float)c->spinDrive * .3f * c->p.topSpeed / (rad > .05f ? rad : .37f);
  }
  return w;
}
void carVisSpin(CarVis *v, const Car *c, float dt) {
  if (FORCEDSPIN955_OLD || dt <= 0.f) return;
  float rad = c->p.wheelRad > .05f ? c->p.wheelRad : v->wheelRad, w = rad > .05f ? c->v / rad : 0.f;
  for (int i = 0; i < 4; i++) {
    float wi = wheelSpinRate(c, i, w, rad);
    v->spin[i] = fmodf(v->spin[i] + wi * dt, 2.f * GU_PI * 64.f);
  }
}
void carVisUpdate(CarVis *v, const Car *c, const CarInput *in, float dt) {
  if (!v->ok || dt <= 0.f) return;
  // ремонт (START) или новая машина
  if (c->damage < v->prevDamage - 1e-3f) carVisReset(v);
  v->shadowDt = dt;
  // --- колёса: вращение по пройденному пути; ручник — задние стоят; пробуксовка — задние быстрее
  float w = c->v / v->wheelRad;
  for (int i = 0; i < 4; i++) {
    float wi = wheelSpinRate(c, i, w, v->wheelRad);
    v->spin[i] = fmodf(v->spin[i] + wi * dt, 2.f * GU_PI * 64.f);
  }
  // --- урон по зонам: удар из стороны контакта (нормаль от стены к машине)
  float m[16]; carPieceMatrix(c, carModelLift(c,v->lift), 0, 0, 0, 1, m);   // столбцы: X вправо, Y вверх, Z назад (мир)
  float modelVelocity[3];
  if (CAR_HINGE_COM_VELOCITY_OLD) memcpy(modelVelocity, c->vel, sizeof modelVelocity);
  else {
    carAnchorVelocity(c, modelVelocity);
    float lift = carModelLift(c, v->lift), r[3] = {m[4] * lift, m[5] * lift, m[6] * lift};
    modelVelocity[0] += c->w[1] * r[2] - c->w[2] * r[1];
    modelVelocity[1] += c->w[2] * r[0] - c->w[0] * r[2];
    modelVelocity[2] += c->w[0] * r[1] - c->w[1] * r[0];
  }
  if (!v->motionValid) {
    if (!CAR_HINGE_FIRST_SAMPLE_OLD) memcpy(v->prevVel, modelVelocity, sizeof modelVelocity);
    v->motionValid = 1;
  }
  if (c->damage > v->prevDamage + 1e-4f) {
    float d = c->damage - v->prevDamage; int zone;
    // куда пришёлся удар: к стене (против нормали контакта); контакты car.c — только последнего подшага,
    // поэтому иначе — против скачка скорости за кадр (стена толкнула машину в сторону dv)
    float hx = 0.f, hz = 0.f;
    if (c->contacts > 0) { hx = -c->cnx[0]; hz = -c->cnz[0]; }
    else { hx = -(modelVelocity[0] - v->prevVel[0]); hz = -(modelVelocity[2] - v->prevVel[2]); }
    if (hx * hx + hz * hz > 1e-6f) {
      float lx = hx * m[0] + hz * m[2], lz = hx * m[8] + hz * m[10];
      zone = fabsf(lz) >= fabsf(lx) ? (lz < 0.f ? 0 : 1) : (lx < 0.f ? 2 : 3);
    } else zone = c->v >= 0.f ? 0 : 1;
    v->zoneDam[zone] += d;
  }
  v->prevDamage = c->damage;
  // --- шарниры: отщёлкивание и подпружиненное болтание от ускорения кузова
  float a[3] = {0, 0, 0};
  if (dt > 1e-4f) for (int k = 0; k < 3; k++) a[k] = (modelVelocity[k] - v->prevVel[k]) / dt;
  for (int k = 0; k < 3; k++) v->prevVel[k] = modelVelocity[k];
  float gw[3] = {-a[0], -9.8f - a[1], -a[2]};                  // g - a в мире
  float len = 0; for (int k = 0; k < 3; k++) len += gw[k] * gw[k];
  if (len > 60.f * 60.f) { float s = 60.f / sqrtf(len); for (int k = 0; k < 3; k++) gw[k] *= s; }   // удар — не рвём на части
  float f[3];                                                  // в осях модели
  for (int k = 0; k < 3; k++) f[k] = gw[0] * m[4 * k] + gw[1] * m[4 * k + 1] + gw[2] * m[4 * k + 2];
  for (int i = 0; i < v->nhinge; i++) {
    CarHinge *h = &v->hinge[i]; int z = h->zone < 4 ? h->zone : 0;
    // сверка g01-cars, P01-10: пороги по общему урону D — створка места удара отпускается при D > 0,1, все четыре при D > 0,3. Было: зона 0,20, всё 1/3 и 2/3. -DSV_DAMVIS_OLD=1
    int open = SV_DAMVIS_OLD ? (v->zoneDam[z] >= ZONE_OPEN || c->damage >= ALL_OPEN || (h->axis == 0 && c->damage >= ALL_TEX))
                             : (c->damage > .3f || (c->damage > .1f && v->zoneDam[z] > 0.f));
    if (v->latched[i]) {
      if (!open || c->destroyed) continue;
      v->latched[i] = 0; v->thv[i] = 3.f;                      // отщёлкнулась — с толчком
    }
#if CAR_HINGE_FROZEN_FORCE_OLD
    float heldAcc = hingeAcceleration(h, v->th[i], v->thv[i], f);
#endif
    float sdt = dt, left = dt;
    while (left > 0.f) {                                       // подшаги по 1/120 с
      sdt = fminf(left, 1.f / 120.f); left -= sdt;
      // Spring, damper and gravity torque follow the current hinge pose,
      // including a bounce at an end stop earlier in this same frame.
#if CAR_HINGE_FROZEN_FORCE_OLD
      float acc = heldAcc;
#else
      float acc = hingeAcceleration(h, v->th[i], v->thv[i], f);
#endif
      v->thv[i] += acc * sdt; v->th[i] += v->thv[i] * sdt;
      if (v->th[i] < 0.f) { v->th[i] = 0.f; if (v->thv[i] < 0.f) v->thv[i] = -v->thv[i] * HINGE_BOUNCE; }
      if (v->th[i] > h->max) { v->th[i] = h->max; if (v->thv[i] > 0.f) v->thv[i] = -v->thv[i] * HINGE_BOUNCE; }
    }
  }
  // --- фонари: стоп — тормоз на ходу вперёд; задний ход — едем назад
  // сверка g01-cars, P01-25: стоп-сигнал горит при любом нажатом тормозе после ограничения (1 − доля) — и стоя; задний ход включается при нажатом тормозе и скорости назад > 0,5 м/с.
  // Было: тормоз на ходу вперёд быстрее 0,3 м/с; задний ход — просто едем назад. -DSV_STOPLIGHT_OLD=1
  if (SV_STOPLIGHT_OLD) { v->brake = in && in->brake > 0.f && c->v > 0.3f; v->reverse = c->v < -0.3f; }
  else {
    float br = in ? in->brake : 0.f, ratio = c->p.topSpeed > 0.f ? fminf(1.f, fabsf(c->v) / c->p.topSpeed) : 0.f;
    if (c->v > .15f) br = fminf(br, 1.f - ratio);
    v->brake = br > 0.f; v->reverse = in && in->brake > 0.f && c->v < -0.5f;
  }
  // круг 7, P01-25: у машины игрока при общем уроне > 0,6 стоп-сигналы больше не показываются (в оригинале на этом пороге выставляется запрет стоп-сигналов; багажник отдельно с 0,3 — места удара у нас нет). -DSV_STOPLIGHT2_OLD=1
  if (!SV_STOPLIGHT2_OLD && (c->huskDrive || c->svPlayer) && c->damage > 0.6f) v->brake = 0;
}

int carVisVariantOn(const Car *c, int variant) { return carBoomed(c) ? variant == 1 : variant == 0; }   // правка 199: остов — со взрыва

int carVisTex(const CarVis *v, int tex) {
  if (!v->ok || tex < 0) return tex;
  for (int i = 0; i < v->ndam; i++) {
    const CarDam *d = &v->dam[i];
    if (d->texNorm != tex || d->texDam == 0xFFFF) continue;
    int z = d->zone < 4 ? d->zone : 0;
    // «целого» урона в data нет: v->prevDamage == car.damage после carVisUpdate
    // сверка g01-cars, P01-10: с створками (игрок) — битая текстура места удара при D > 0,3, все четыре при D > 0,6; без створок (миссии) — при D > 0,1 и D > 0,4. Капот скрыть при 0,6 — не сделано (передано)
    if (SV_DAMVIS_OLD ? (v->zoneDam[z] >= ZONE_TEX || v->prevDamage >= ALL_TEX)
                      : (v->nhinge > 0 ? (v->prevDamage > .6f || (v->prevDamage > .3f && v->zoneDam[z] > 0.f)) : (v->prevDamage > .4f || (v->prevDamage > .1f && v->zoneDam[z] > 0.f)))) return d->texDam;
  }
  return tex;
}

// поворот сустава в осях модели (3x3 по строкам) и смещение
static void jointXf(const CarVis *v, const Car *c, int j, float R[9], float t[3], int noSpin) {   // noSpin — №867: боковины колеса без качения
  R[0] = 1; R[1] = 0; R[2] = 0; R[3] = 0; R[4] = 1; R[5] = 0; R[6] = 0; R[7] = 0; R[8] = 1;
  if (j < 0 || j >= v->njoint) { t[0] = t[1] = t[2] = 0; return; }
  t[0] = v->joint[j].x; t[1] = v->joint[j].y; t[2] = v->joint[j].z;
  for (int i = 0; i < 4; i++) if (v->wheelJ[i] == j) {
    float dy = clampf(c->comp[i] - c->eq, -c->eq, fminf(c->lim - c->eq, v->wheelUp[i]));   // правка 354: не выше запаса модели
    if(c->p.bodyMass>0.f)dy=clampf(c->comp[i]+c->p.suspensionYOffset,c->p.suspensionYOffset,
                                fminf(c->lim+c->p.suspensionYOffset,v->wheelUp[i]));
    t[1] += dy;
    float ph = i < 2 ? -c->steer * v->maxSteer : 0.f, sp = noSpin ? 0.f : -v->spin[i];
    CarVis *vm = (CarVis *)v;   // кэш — служебное, не часть состояния
    if (!SV_CARCOST1159_OLD && vm->wOk[i] && vm->wKey[i][0] == ph && vm->wKey[i][1] == sp) { memcpy(R, vm->wR[i], 9 * sizeof(float)); return; }
    float cp = cosf(ph), sph = sinf(ph), cs = cosf(sp), ss = sinf(sp);
    // Ry(ph) * Rx(sp)
    R[0] = cp; R[1] = sph * ss; R[2] = sph * cs;
    R[3] = 0;  R[4] = cs;       R[5] = -ss;
    R[6] = -sph; R[7] = cp * ss; R[8] = cp * cs;
    if (!SV_CARCOST1159_OLD) { vm->wKey[i][0] = ph; vm->wKey[i][1] = sp; memcpy(vm->wR[i], R, 9 * sizeof(float)); vm->wOk[i] = 1; }
    return;
  }
  for (int i = 0; i < v->nhinge; i++) if (v->hinge[i].joint == j) {
    float a = v->hinge[i].sign * v->th[i], ca = cosf(a), sa = sinf(a);
    if (v->hinge[i].axis == 0) { R[4] = ca; R[5] = -sa; R[7] = sa; R[8] = ca; }
    else { R[0] = ca; R[2] = sa; R[6] = -sa; R[8] = ca; }
    return;
  }
}

void carVisPieceMatrix(const CarVis *v, const Car *c, int joint, int variant, float ox, float oy, float oz, float sc, float m[16]) {
  carVisPieceMatrixEx(v, c, joint, variant, ox, oy, oz, sc, 0, m);
}
int carVisWheelJoint(const CarVis *v, int joint) {   // №867: сустав — колесо (качение)
  if (!v || joint < 0) return 0;
  for (int i = 0; i < 4; i++) if (v->wheelJ[i] == joint) return 1;
  return 0;
}
void carVisPieceMatrixEx(const CarVis *v, const Car *c, int joint, int variant, float ox, float oy, float oz, float sc, int noSpin, float m[16]) {
  float B[16], lift = variant == 1 ? v->huskLift : carModelLift(c, v->lift);
  if (SV_CARCOST1159_OLD) carPieceMatrix(c, lift, 0, 0, 0, 1, B);
  else {   // №1159: матрица тела одна на кадр для всех кусков (ключ: машина, положение, поворот, подъём)
    CarVis *vm = (CarVis *)v; const float key[8] = {c->x, c->y, c->z, c->q[0], c->q[1], c->q[2], c->q[3], lift};
    if (!vm->cOk || vm->cKeyCar != c || memcmp(vm->cKey, key, sizeof key)) { carPieceMatrix(c, lift, 0, 0, 0, 1, vm->cB); memcpy(vm->cKey, key, sizeof key); vm->cKeyCar = c; vm->cOk = 1; }
    memcpy(B, vm->cB, sizeof B);
  }
  float R[9], t[3];
  if (variant == 1) { memset(R, 0, sizeof R); R[0] = R[4] = R[8] = 1; t[0] = t[1] = t[2] = 0; }
  else jointXf(v, c, joint, R, t, noSpin);
  float p[3] = {t[0] + R[0] * ox + R[1] * oy + R[2] * oz, t[1] + R[3] * ox + R[4] * oy + R[5] * oz, t[2] + R[6] * ox + R[7] * oy + R[8] * oz};
  for (int col = 0; col < 3; col++) {                          // столбец col матрицы = B * (столбец col of R) * sc
    float rx = R[col], ry = R[3 + col], rz = R[6 + col];
    for (int k = 0; k < 3; k++) m[4 * col + k] = (B[k] * rx + B[4 + k] * ry + B[8 + k] * rz) * sc;
    m[4 * col + 3] = 0;
  }
  for (int k = 0; k < 3; k++) m[12 + k] = B[12 + k] + B[k] * p[0] + B[4 + k] * p[1] + B[8 + k] * p[2];
  m[15] = 1;
}

int carVisJointWorld(const CarVis *v, const Car *c, int j, float out[3]) {
  if (!v->ok || j < 0 || j >= v->njoint) return 0;
  float B[16]; carPieceMatrix(c, carModelLift(c,v->lift), v->joint[j].x, v->joint[j].y, v->joint[j].z, 1, B);
  out[0] = B[12]; out[1] = B[13]; out[2] = B[14];
  return 1;
}

typedef struct { unsigned c; float x, y, z; } SV;
typedef struct { float u, v; unsigned c; float x, y, z; } FV;

static void shadowDrawGrid(CarVis *v, const Car *c, const ColWorld *w, float xx, float xz, float zx, float zz, int quiet, int sameNear, float gy, float fade) {
  const float *A = v->shadowAdj;
  const CarParams *P = &c->p; float hw = P->halfW, nose = P->halfL + P->bodyOfs, tail = P->halfL - P->bodyOfs;
  float cxs[4] = {-(hw + A[0]), hw + A[2], -(hw + A[4]), hw + A[6]};   // углы пятна: передний левый, передний правый, задний левый, задний правый
  float czs[4] = {-(nose + A[1]), -(nose + A[3]), tail + A[5], tail + A[7]};
  const int NG = SH_N + 2;   // 8 линий: кольцо мягкого края и 6x6 внутренних узлов
  SV *vb = (SV *)sceGuGetMemory(NG * NG * sizeof(SV));
  unsigned alpha = (unsigned)(SHADOW_A * fade * skinToonShadowK());   // №675: в мультике слабее
  float a = clampf(v->shadowDt, .001f, .1f) / (.1f + clampf(v->shadowDt, .001f, .1f));
  int keep = v->shadowGrid && sameNear;   // сетка прошлого кадра пригодна для сглаживания спада
  float H[SH_N * SH_N], lift[SH_N * SH_N]; unsigned char ok[SH_N * SH_N];
  for (int j = 0; j < SH_N; j++) for (int i = 0; i < SH_N; i++) {
    float u = i / (float)(SH_N - 1), t = j / (float)(SH_N - 1);
    float lx = (cxs[0] * (1 - u) + cxs[1] * u) * (1 - t) + (cxs[2] * (1 - u) + cxs[3] * u) * t;
    float lz = (czs[0] * (1 - u) + czs[1] * u) * (1 - t) + (czs[2] * (1 - u) + czs[3] * u) * t;
    float px = c->x + xx * lx + zx * lz, pz = c->z + xz * lx + zz * lz, k = (float)(j * SH_N + i);
    float yp = gy + v->shadowSlopeX * (px - v->shadowX) + v->shadowSlopeZ * (pz - v->shadowZ), yv, n[3];
    int idx = j * SH_N + i;
    if (quiet && v->shadowGrid) { H[idx] = v->shadowH[idx]; ok[idx] = v->shadowHok[idx]; lift[idx] = SH_LIFT; (void)k; continue; }
    if (colGround(w, px, pz, yp + .75f, yp - .75f, &yv, n)) {
      float sl = sqrtf(fmaxf(0.f, 1.f - n[1] * n[1])) / fmaxf(n[1], .5f);
      lift[idx] = SH_LIFT + fminf(.07f, .25f * sl); ok[idx] = 1;
      if (keep && v->shadowHok[idx]) H[idx] = yv > v->shadowH[idx] ? yv : v->shadowH[idx] + a * (yv - v->shadowH[idx]);   // растём сразу, спадаем за 100 мс
      else H[idx] = yv;
    } else { H[idx] = yp; lift[idx] = SH_LIFT; ok[idx] = 0; }
  }
  memcpy(v->shadowH, H, sizeof H); memcpy(v->shadowHok, ok, sizeof ok); v->shadowGrid = 1;
  for (int j = 0; j < NG; j++) for (int i = 0; i < NG; i++) {
    int ci = i < 1 ? 0 : i > SH_N ? SH_N - 1 : i - 1, cj = j < 1 ? 0 : j > SH_N ? SH_N - 1 : j - 1, idx = cj * SH_N + ci;   // кольцо берёт высоту ближайшего внутреннего узла
    float u = ci / (float)(SH_N - 1), t = cj / (float)(SH_N - 1);
    float lx = (cxs[0] * (1 - u) + cxs[1] * u) * (1 - t) + (cxs[2] * (1 - u) + cxs[3] * u) * t + (i < 1 ? -SHADOW_SOFT : i > SH_N ? SHADOW_SOFT : 0.f);
    float lz = (czs[0] * (1 - u) + czs[1] * u) * (1 - t) + (czs[2] * (1 - u) + czs[3] * u) * t + (j < 1 ? -SHADOW_SOFT : j > SH_N ? SHADOW_SOFT : 0.f);
    SV *s = &vb[j * NG + i];
    s->x = c->x + xx * lx + zx * lz; s->z = c->z + xz * lx + zz * lz; s->y = H[idx] + lift[idx];
    int inner = i >= 1 && i <= SH_N && j >= 1 && j <= SH_N;
    s->c = (inner && ok[idx]) ? (alpha << 24) : 0;
  }
  static unsigned short ix[(SH_N + 1) * (SH_N + 1) * 6] __attribute__((aligned(16)));
  static int ixDone;
  if (!ixDone) {
    int k = 0;
    for (int j = 0; j < NG - 1; j++) for (int i = 0; i < NG - 1; i++) {
      unsigned short q0 = j * NG + i, q1 = q0 + 1, q2 = q0 + NG, q3 = q0 + NG + 1;
      ix[k++] = q0; ix[k++] = q1; ix[k++] = q3; ix[k++] = q0; ix[k++] = q3; ix[k++] = q2;
    }
    ixDone = 1;
  }
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &I);
  int dither = sceGuGetStatus(GU_DITHER);
  sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_DITHER);
  sceGuDepthOffset(4);
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE);
  sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, (NG - 1) * (NG - 1) * 6, ix, vb);
  sceGuDepthOffset(0); if (dither) sceGuEnable(GU_DITHER);
  sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND);
}

void carVisDrawShadow(CarVis *v, const Car *c, const ColWorld *w) {
  if (!v->ok) return;
  float B[16]; carPieceMatrix(c, carModelLift(c,v->lift), 0, 0, 0, 1, B);
  float xx = B[0], xz = B[2], zx = B[8], zz = B[10], l;
  l = sqrtf(xx * xx + xz * xz); if (l < .2f) return; xx /= l; xz /= l;   // на боку — без тени
  l = sqrtf(zx * zx + zz * zz); if (l < .2f) return; zx /= l; zz /= l;
  float gy, n[3], cy = c->y;
  float dx = c->x - v->shadowX, dz = c->z - v->shadowZ;
  float travel2 = dx * dx + dz * dz;
  int same = v->shadowValid && v->shadowWorld == w;
  // Правило порта №433: quiet receiver stays fixed; movement/flight resumes queries.
  int quiet = !CAR_SHADOW_V2_OLD && same && c->grounded && travel2 < .002f * .002f &&
    fabsf(c->vel[0]) + fabsf(c->vel[1]) + fabsf(c->vel[2]) < .01f;
  float fade;
  if (quiet) { gy = v->shadowY; fade = v->shadowFade; }
  else {
    if (!colGround(w, c->x, c->z, c->y + 1.f, c->y - 30.f, &gy, n)) {
      v->shadowValid = 0; return;
    }
    fade = 1.f - clampf((cy - gy) / 8.f, 0.f, 1.f);
    if (!CAR_SHADOW_V2_OLD) {
      float sx = -n[0] / fmaxf(n[1], .5f), sz = -n[2] / fmaxf(n[1], .5f);
      // Transport the old plane to the new centre before smoothing height/normal.
      // 100 ms time constant is a port visual rule, not a PS2 physics parameter.
      float a = clampf(v->shadowDt, .001f, .1f) / (.1f + clampf(v->shadowDt, .001f, .1f));
      if (same && travel2 < 16.f) {
        float previous = v->shadowY + v->shadowSlopeX * dx + v->shadowSlopeZ * dz;
        gy = previous + a * (gy - previous);
        sx = v->shadowSlopeX + a * (sx - v->shadowSlopeX);
        sz = v->shadowSlopeZ + a * (sz - v->shadowSlopeZ);
        fade = v->shadowFade + a * (fade - v->shadowFade);
      }
      v->shadowY = gy; v->shadowSlopeX = sx; v->shadowSlopeZ = sz;
      v->shadowX = c->x; v->shadowZ = c->z; v->shadowFade = fade;
      v->shadowWorld = w; v->shadowValid = 1;
    }
  }
  if (fade <= 0.f) return;
  if (!SHADOW_OLD && !CAR_SHADOW_V2_OLD && c->grounded && cy - gy < .6f) { shadowDrawGrid(v, c, w, xx, xz, zx, zz, quiet, same && travel2 < 16.f, gy, fade); return; }
  v->shadowGrid = 0;
  const float *A = v->shadowAdj;   // SetShadowAdjustments: 4 угла (x, z); порядок углов — гипотеза FL FR RL RR
  // пятно — коробка кузова машины (CarParams: борт halfW, нос -(halfL + bodyOfs), зад halfL - bodyOfs в осях модели; у famil_v
  // до 29.09 числа были вписаны: ±1.10, -2.35 … 3.0 — теперь -2.355 … 2.995)
  const CarParams *P = &c->p; float hw = P->halfW, nose = P->halfL + P->bodyOfs, tail = P->halfL - P->bodyOfs;
  float cxs[4] = {-(hw + A[0]), hw + A[2], -(hw + A[4]), hw + A[6]};
  float czs[4] = {-(nose + A[1]), -(nose + A[3]), tail + A[5], tail + A[7]};
  SV *vb = (SV *)geVbAlloc(16 * sizeof(SV), GEVB_CARSHADOW_OLD);
  unsigned alpha = CAR_SHADOW_V2_OLD ? groundShadeAlpha(SHADOW_A * fade * skinToonShadowK()) : (unsigned)(SHADOW_A * fade * skinToonShadowK());
  for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
    int ci = (i < 2 ? 0 : 1) + (j < 2 ? 0 : 2);
    float mx = cxs[ci] + (i == 0 ? -SHADOW_SOFT : i == 3 ? SHADOW_SOFT : 0.f);
    float mz = czs[ci] + (j == 0 ? -SHADOW_SOFT : j == 3 ? SHADOW_SOFT : 0.f);
    float px = c->x + xx * mx + zx * mz, pz = c->z + xz * mx + zz * mz, py = gy;
    if (CAR_SHADOW_V2_OLD) {
      if (colGround(w, px, pz, c->y + 1.f, c->y - 30.f, &py, n) == 0) py = gy;
    } else py += v->shadowSlopeX * (px - v->shadowX) + v->shadowSlopeZ * (pz - v->shadowZ);
    SV *s = &vb[j * 4 + i];
    s->x = px; s->y = py + 0.05f; s->z = pz;
    s->c = ((i == 1 || i == 2) && (j == 1 || j == 2)) ? (alpha << 24) : 0;
  }
  static unsigned short ix[54] __attribute__((aligned(16)));
  static int ixDone;
  if (!ixDone) {
    int k = 0;
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) {
      unsigned short a = j * 4 + i, b = a + 1, d = a + 4, e = a + 5;
      ix[k++] = a; ix[k++] = b; ix[k++] = e; ix[k++] = a; ix[k++] = e; ix[k++] = d;
    }
    ixDone = 1;
  }
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &I);
  int dither = sceGuGetStatus(GU_DITHER);
  if (CAR_SHADOW_V2_OLD) groundShadeBegin();
  else {
    sceGuDisable(GU_TEXTURE_2D); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_DITHER);
    sceGuDepthOffset(4);
  }
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDepthMask(GU_TRUE);
  sceGuDrawArray(GU_TRIANGLES, GU_INDEX_16BIT | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 54, ix, vb);
  if (CAR_SHADOW_V2_OLD) groundShadeEnd();
  else { sceGuDepthOffset(0); if (dither) sceGuEnable(GU_DITHER); }
  sceGuDepthMask(GU_FALSE); sceGuDisable(GU_BLEND);
}

// №970 (Роб 25.10: «блики у фар нравятся, можно чуть крупнее»): блик огня машины (стоп-сигнал, задний ход) крупнее в CAR_GLINT970_K раз; GLINT970_OLD=1 / flag glint970old 1 — как было
#ifndef CAR_GLINT970_K
#define CAR_GLINT970_K 1.3f
#endif
void carFlareOne(const float P[3], const float U[3], int mode, float w, float h, float dist, unsigned abgr, int tex, float ex, float ey, float ez, void (*bind)(int tex)) {
  float tx = ex - P[0], ty = ey - P[1], tz = ez - P[2], d = sqrtf(tx * tx + ty * ty + tz * tz);
  if (d < .3f || d > FLARE_DIST) return;
  tx /= d; ty /= d; tz /= d;
  float ux = 0.f, uy = 1.f, uz = 0.f;
  if (mode == 1) { float l = sqrtf(U[0] * U[0] + U[1] * U[1] + U[2] * U[2]); if (l > 1e-3f) { ux = U[0] / l; uy = U[1] / l; uz = U[2] / l; } }   // LYAX — ось машины, YAX — мира
  float rx = uy * tz - uz * ty, ry = uz * tx - ux * tz, rz = ux * ty - uy * tx, rl = sqrtf(rx * rx + ry * ry + rz * rz);
  if (rl < 1e-3f) return;
  float hw = w * .5f / rl, hh = h * .5f;
  rx *= hw; ry *= hw; rz *= hw;
  float cx = P[0] + tx * dist, cy = P[1] + ty * dist, cz = P[2] + tz * dist;
  if (fxGlowGlint && !GLOW_GLINT_OLD) { fxGlowGlint(cx, cy, cz, fmaxf(w, h) * .7f * (fxGlint970Old ? 1.f : CAR_GLINT970_K), abgr, (float)(abgr >> 24) / 255.f, P[0] * .37f); return; }   // №839: огонь машины — бликом
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_FOG); sceGuDisable(GU_ALPHA_TEST);
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
  sceGuDepthMask(GU_TRUE);
  sceGuTexScale(1.f, 1.f);
  FV *q = (FV *)sceGuGetMemory(6 * sizeof(FV));
  FV a = {0, 0, abgr, cx - rx + ux * hh, cy - ry + uy * hh, cz - rz + uz * hh};
  FV b = {1, 0, abgr, cx + rx + ux * hh, cy + ry + uy * hh, cz + rz + uz * hh};
  FV e = {1, 1, abgr, cx + rx - ux * hh, cy + ry - uy * hh, cz + rz - uz * hh};
  FV f = {0, 1, abgr, cx - rx - ux * hh, cy - ry - uy * hh, cz - rz - uz * hh};
  q[0] = a; q[1] = b; q[2] = e; q[3] = a; q[4] = e; q[5] = f;
  if (bind) bind(tex < 0 ? -1 : tex);
  sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, q);
  sceGuTexScale(128.f, 128.f);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDisable(GU_BLEND); sceGuDepthMask(GU_FALSE); sceGuEnable(GU_FOG);
}

void carVisDrawFlares(const CarVis *v, const Car *c, float ex, float ey, float ez, void (*bind)(int tex)) {
  if (!v->ok || c->destroyed || (!v->brake && !v->reverse)) return;
  float B[16]; carPieceMatrix(c, carModelLift(c,v->lift), 0, 0, 0, 1, B);
  int n = 0;
  for (int i = 0; i < v->nflare; i++) {
    int k = v->flare[i].kind;
    if ((k == 0 && v->brake) || (k == 1 && v->reverse)) n++;
  }
  if (!n) return;
  ScePspFMatrix4 I; memset(&I, 0, sizeof I); I.x.x = I.y.y = I.z.z = I.w.w = 1.f;
  sceGuSetMatrix(GU_MODEL, &I);
  sceGuDisable(GU_FOG); sceGuDisable(GU_ALPHA_TEST);
  sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffff);
  sceGuDepthMask(GU_TRUE);
  sceGuTexScale(1.f, 1.f);
  for (int i = 0; i < v->nflare; i++) {
    const CarFlare *fl = &v->flare[i];
    if (!((fl->kind == 0 && v->brake) || (fl->kind == 1 && v->reverse))) continue;
    int j = fl->joint < v->njoint ? fl->joint : 0;
    float px = v->joint[j].x + fl->ox, py = v->joint[j].y + fl->oy, pz = v->joint[j].z + fl->oz;
    float P[3]; for (int k = 0; k < 3; k++) P[k] = B[12 + k] + B[k] * px + B[4 + k] * py + B[8 + k] * pz;
    float tx = ex - P[0], ty = ey - P[1], tz = ez - P[2], d = sqrtf(tx * tx + ty * ty + tz * tz);
    if (d < .3f || d > FLARE_DIST) continue;
    tx /= d; ty /= d; tz /= d;
    float ux, uy, uz;
    if (fl->mode == 1) { ux = B[4]; uy = B[5]; uz = B[6]; } else { ux = 0; uy = 1; uz = 0; }   // LYAX — ось машины, YAX — мира
    float rx = uy * tz - uz * ty, ry = uz * tx - ux * tz, rz = ux * ty - uy * tx, rl = sqrtf(rx * rx + ry * ry + rz * rz);
    if (rl < 1e-3f) continue;
    float hw = fl->w * .5f / rl, hh = fl->h * .5f;
    rx *= hw; ry *= hw; rz *= hw;
    float cx = P[0] + tx * fl->dist, cy = P[1] + ty * fl->dist, cz = P[2] + tz * fl->dist;
    if (fxGlowGlint && !GLOW_GLINT_OLD) { fxGlowGlint(cx, cy, cz, fmaxf(fl->w, fl->h) * .7f * (fxGlint970Old ? 1.f : CAR_GLINT970_K), fl->abgr, (float)(fl->abgr >> 24) / 255.f, (float)i * .37f); continue; }   // №839: стоп-сигнал — бликом
    FV *q = (FV *)geVbAlloc(6 * sizeof(FV), GEVB_FLARE_OLD);
    unsigned col = fl->abgr;
    FV a = {0, 0, col, cx - rx + ux * hh, cy - ry + uy * hh, cz - rz + uz * hh};
    FV b = {1, 0, col, cx + rx + ux * hh, cy + ry + uy * hh, cz + rz + uz * hh};
    FV e = {1, 1, col, cx + rx - ux * hh, cy + ry - uy * hh, cz + rz - uz * hh};
    FV f = {0, 1, col, cx - rx - ux * hh, cy - ry - uy * hh, cz - rz - uz * hh};
    q[0] = a; q[1] = b; q[2] = e; q[3] = a; q[4] = e; q[5] = f;
    if (bind) bind(fl->tex == 0xFFFF ? -1 : fl->tex);
    sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, q);
  }
  sceGuTexScale(128.f, 128.f);
  sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
  sceGuDisable(GU_BLEND); sceGuDepthMask(GU_FALSE); sceGuEnable(GU_FOG);
}
