// homer.c — Гомер пешком (этап 4). См. homer.h: числа, события, API. Рисование — skin.c (GE-скиннинг).
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "homer.h"
#include "stateprops.h"

#ifndef SKIN_HOST
#include <pspgu.h>
#endif

#define PI_F 3.14159265f
// точка у двери водителя в осях модели машины famil_v (модель смотрит в -Z, +X вправо): x = -(полуширина + 0.37),
// z позади центра. Подобрано так, что поза «сидит» (корень конца hom_get_into_car_close_door_driver = (-0.995, 0.46, 0.72)
// в осях Гомера, повёрнутого на pi) ставит таз на сиденье водителя (-0.48, ~0.46, -0.15). y = -lift (земля).
// С 29.09 точка у двери и якорь посадки — свои у каждой машины (HomerCar.door/seatA из CarParams, CAR1 v2: сиденье — сустав
// dl машины, правило tools/psp_export.car_info); эти числа — только начальное значение до первой машины (famil_v)
#ifndef HOMER_DOOR_X
#define HOMER_DOOR_X (-1.475f)
#endif
#ifndef HOMER_DOOR_Z
#define HOMER_DOOR_Z 0.57f
#endif

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }
// правка 252: угол-бесконечность или 1e30 крутил бы цикл вечно (a - 2π == a) — такой угол сводим fmodf, NaN/inf -> 0; |a| < 64 — как было
static float wrapPi(float a) {
  if (!(a > -64.f && a < 64.f)) a = (a > -1e30f && a < 1e30f) ? fmodf(a, 2.f * PI_F) : 0.f;
  while (a > PI_F) a -= 2.f * PI_F; while (a < -PI_F) a += 2.f * PI_F; return a;
}
static float smooth01(float u) { u = clampf(u, 0.f, 1.f); return u * u * (3.f - 2.f * u); }

static void pushEv(Homer *H, int type, float x, float y, float z, float r, float dx, float dz) {
  if (H->nev >= HOMER_MAXEV) return;
  HomerEvent *e = &H->ev[H->nev++];
  e->type = type; e->x = x; e->y = y; e->z = z; e->r = r; e->dx = dx; e->dz = dz;
}
int homerPopEvent(Homer *H, HomerEvent *e) {
  if (!H->nev) return 0;
  *e = H->ev[0]; H->nev--; memmove(H->ev, H->ev + 1, sizeof(HomerEvent) * H->nev); return 1;
}
int homerHitTest(const HomerEvent *e, float cx, float cy, float cz, float r) {
  float dx = cx - e->x, dy = cy - e->y, dz = cz - e->z, R = e->r + r;
  if (e->type == HOMER_EV_STOMP) dy *= .5f;             // стомп — приплюснутая сфера (волна по земле)
  return dx * dx + dy * dy + dz * dz <= R * R;
}

// ------------------------------------------------------------------ машина
// Роб 17.10 (видео IMG_7282 3:21.2–3:21.4: «чтобы рука не проходила сквозь дверь машины»): у двери Гомер тянется к ручке, пока дверь ещё закрыта, и
// кисть уходит внутрь двери — точка у двери по правилу экспорта (0,375 м от борта, tools/psp_export.car_info) близко к борту. Стоит дальше на
// homerDoorOut (у всех машин, водительская дверь слева: x меньше); посадка доводит до сиденья (якорь едет от двери к сиденью за клип посадки).
// «flag doorout 0» — как было, «flag doorout 0.2» — другой отступ; HOMER_DOOR_OUT_OLD=1 — как было
#ifndef HOMER_DOOR_OUT_OLD
#define HOMER_DOOR_OUT_OLD 0
#endif
static float homerDoorOut = HOMER_DOOR_OUT_OLD ? 0.f : .12f;   // замер tests/homer/door_hand.sh: при 0 запястье в 1–5 см от борта (пальцы в двери), при 0,12 — в 13 см (пальцы у поверхности)
void homerSetDoorOut(float m) { homerDoorOut = m; }
void homerCarFrom(HomerCar *hc, const Car *c, float lift) {
  if(!carBoomed(c))lift=carModelLift(c,lift);
  carPieceMatrix(c, lift, 0, 0, 0, 1, hc->m);
  hc->lift = lift; hc->x = c->x; hc->y = c->y; hc->z = c->z; hc->h = c->h; hc->v = c->v;
  carBox(c, &hc->box);
  hc->roof = c->y + c->p.bodyHi; hc->ok = 1;
  hc->dead = c->destroyed && !carHuskDrives(c);   // правка 355: разбита и горит — не сесть; остов после взрыва — можно
  hc->husk = carHuskDrives(c);
  hc->seatY = c->p.seat[1] + lift; hc->seatData = c->p.seatData;
  hc->door[0] = c->p.doorX - homerDoorOut; hc->door[1] = c->p.doorZ; hc->seatA[0] = c->p.seatAx; hc->seatA[1] = c->p.seatAz;
  hc->iris = carIrisFor(c->p.name);   // правка 333: SetIrisTransition(1) в .con — посадка и выход ирисом
}
static void xform(const float *m, float x, float y, float z, float out[3]) {
  out[0] = m[0] * x + m[4] * y + m[8] * z + m[12];
  out[1] = m[1] * x + m[5] * y + m[9] * z + m[13];
  out[2] = m[2] * x + m[6] * y + m[10] * z + m[14];
}
// система «у двери»: мир = M * T(anchor) * Ry(pi). Гомер смотрит вперёд машины.
static void anchorFrame(const Homer *H, float F[16]) {
  const float *M = H->car.m; float o[3]; xform(M, H->anchor[0], H->anchor[1], H->anchor[2], o);
  for (int k = 0; k < 3; k++) { F[k] = -M[k]; F[4 + k] = M[4 + k]; F[8 + k] = -M[8 + k]; }
  F[3] = F[7] = F[11] = 0.f; F[12] = o[0]; F[13] = o[1]; F[14] = o[2]; F[15] = 1.f;
}
static float anchorHeading(const Homer *H) { const float *M = H->car.m; return atan2f(-M[8], -M[10]); }
// якорь: u = 0 — точка у двери, 1 — якорь посадки (сиденье). У famil_v они совпадают (якорь не двигается, как до 29.09)
// №666: высота посадки. Таз героя в позе сидения (seatRoot, клип посадки) стоит на 0,46 м над землёй у всех, а сиденье машины (сустав dl,
// car.p.seat[1] + lift) у разных машин от 0,32 до 1,04 м: у обычных совпадает (±0,04), у высоких (marge_v: 1,04 м) герой сидел на ~0,58 м ниже
// сиденья. Правило: таз — на сиденье машины; якорь поднимается на разницу вместе с движением к сиденью (u). Нет данных сиденья (запасное
// famil_v) — как прежде. SEAT_OLD=1 — прежнее (красный контроль: Мардж в marge_v низко)
#ifndef SEAT_OLD
#define SEAT_OLD 0
#endif
#define SEAT_REF_Y 0.499f   // famil_v: seat[1] −0,186 + lift 0,685: на этой высоте таз Гомера (0,46 м) принят Робом; остальные машины — разницей к ней (как seatAx/Az в psp_export.car_info)
static float seatLift(const Homer *H) {
  const HomerCar *c = &H->car;
  if (SEAT_OLD || !c->seatData) return 0.f;
  float d = c->seatY - SEAT_REF_Y;
  return d < -.3f ? -.3f : d > .8f ? .8f : d;
}
static void setAnchor(Homer *H, float u) {
  const HomerCar *c = &H->car;
  H->anchor[0] = c->door[0] + (c->seatA[0] - c->door[0]) * u; H->anchor[1] = -c->lift + seatLift(H) * u;
  H->anchor[2] = c->door[1] + (c->seatA[1] - c->door[1]) * u;
}

// ------------------------------------------------------------------ анимация
static void playClip(Homer *H, int clip, int loop, float fade, float from) {
  if (clip == H->clip && loop && H->clipLoop) return;         // та же петля — не перезапускаем; одиночные — всегда с начала
  memcpy(H->snap, H->xf, sizeof(SkinXf) * H->m->h->nJoints);
  H->fade = 0.f; H->fadeLen = fade;
  H->clip = clip; H->clipLoop = loop; H->clipT = from; H->clipRate = 1.f;
}
static void playLoco(Homer *H, float fade) {
  if (H->clip < 0) return;
  memcpy(H->snap, H->xf, sizeof(SkinXf) * H->m->h->nJoints);
  H->fade = 0.f; H->fadeLen = fade; H->clip = -1;
}
static float clipLen(const Homer *H, int c) { return c >= 0 ? skinClipLength(H->m, c) : 0.f; }
static int clipDone(const Homer *H) { return H->clip >= 0 && !H->clipLoop && H->clipT >= clipLen(H, H->clip) - 1e-4f; }

static void animTick(Homer *H, float dt) {
  if (H->fade < H->fadeLen) H->fade += dt;
  if (H->clip >= 0) {
    float len = clipLen(H, H->clip);
    H->clipT += dt * H->clipRate;
    if (H->clipLoop && len > 0.f) H->clipT = fmodf(H->clipT, len);
    else if (H->clipT > len) H->clipT = len;
  }
  // локомоция: фаза шага по пройденному пути (ноги не скользят)
  float v = H->speed; int k = 0; float w;
  while (k < 2 && v > H->locoV[k + 1]) k++;
  w = clampf((v - H->locoV[k]) / (H->locoV[k + 1] - H->locoV[k]), 0.f, 1.f);
  float stride = k == 0 ? H->locoStride[1] : H->locoStride[k] + (H->locoStride[k + 1] - H->locoStride[k]) * w;
  if (stride > 1e-3f) {
    float ph = H->phaseLoco + dt * v / stride;
    // правка 45: шаг бега — фаза прошла 0.5 или 1 (два шага на цикл), на земле, бег (клип hom_loco_dash весит больше половины)
    if (HOMER_STEP_DUST && H->grounded && k == 2 && w >= .5f && floorf(ph * 2.f) > floorf(H->phaseLoco * 2.f))
      pushEv(H, HOMER_EV_STEP, H->x, H->y, H->z, v, 0, 0);
    H->phaseLoco = fmodf(ph, 1.f);
  }
  H->idleClipT += dt;
  skinTick(&H->blink, dt);
}

static void sampleLoco(Homer *H, SkinXf *out) {
  const SkinModel *m = H->m; int nj = (int)m->h->nJoints;
  float v = H->speed; int k = 0;
  while (k < 2 && v > H->locoV[k + 1]) k++;
  float w = clampf((v - H->locoV[k]) / (H->locoV[k + 1] - H->locoV[k]), 0.f, 1.f);
  if (k == 0) {
    float il = clipLen(H, H->cLoco[0]);
    skinSample(m, H->cLoco[0], il > 0.f ? fmodf(H->idleClipT, il) : 0.f, out);
    if (w > 0.f) { skinSample(m, H->cLoco[1], H->phaseLoco * clipLen(H, H->cLoco[1]), H->tmp); skinBlend(out, H->tmp, w, nj); }
  } else {
    skinSample(m, H->cLoco[k], H->phaseLoco * clipLen(H, H->cLoco[k]), out);
    if (w > 0.f) { skinSample(m, H->cLoco[k + 1], H->phaseLoco * clipLen(H, H->cLoco[k + 1]), H->tmp); skinBlend(out, H->tmp, w, nj); }
  }
}

static int isDrvClip(const Homer *H, int c) {
  return c >= 0 && (c == H->cDrvTurn[0] || c == H->cDrvTurn[1] || c == H->cDrvGas || c == H->cDrvBrake || c == H->cDrvCrash);
}
void homerDriveInput(Homer *H, float steer, float gas, float brake, float speed, int crash) {
  H->drvSteer = steer; H->drvGas = gas; H->drvBrake = brake; H->drvSpeed = speed; H->drvCrash |= crash != 0;
}
// Жесты водителя (правки Роба 220/231): у оригинала Гомер за рулём играет набор ndr — покой (руки на руле, 2 с, по кругу) и
// жесты по 0,67–0,83 с, каждый из покоя в покой: поворот руля, когда руль пошёл в сторону, газ с места, тормоз на ходу, удар.
// Пороги — правило порта (в данных их нет): руль за 0,5, газ с 0,3 до 0,7 ниже 8 м/с, тормоз за 0,5 быстрее 5 м/с; удар —
// всегда, поверх жеста. Без входа (боты) — только покой
static void driverGestures(Homer *H) {
  if (H->cDrvTurn[0] < 0) return;
  int gest = H->clip != H->cInCar;
  if (gest && clipDone(H)) { playClip(H, H->cInCar, 1, .2f, 0.f); gest = 0; }
  int k = -1;
  if (H->drvCrash && H->cDrvCrash >= 0 && H->clip != H->cDrvCrash) k = H->cDrvCrash;
  else if (!gest) {
    if (fabsf(H->drvSteer) > .5f && fabsf(H->drvPrevSteer) <= .5f) k = H->cDrvTurn[H->drvSteer > 0.f ? 1 : 0];
    else if (H->drvGas > .7f && H->drvPrevGas <= .3f && fabsf(H->drvSpeed) < 8.f) k = H->cDrvGas;
    else if (H->drvBrake > .5f && H->drvPrevBrake <= .5f && H->drvSpeed > 5.f) k = H->cDrvBrake;
  }
  if (k >= 0) { playClip(H, k, 0, .1f, 0.f); H->drvGestures++; }
  H->drvPrevSteer = H->drvSteer; H->drvPrevGas = H->drvGas; H->drvPrevBrake = H->drvBrake; H->drvCrash = 0;
}

// B5 (ОПТИМИЗАЦИЯ_ПЛАН_ФИНАЛ): Гомер за рулём стоит почти неподвижно (покой ndr, 2 с по кругу, редкие жесты), а позу на каждый кадр считали целиком:
// выборка клипа по ~60 суставам и палитра (x_homer 1,5–2,3 мс в езде). Теперь полная поза — через кадр, в промежуточном кадре палитра достраивается
// по скорости последних двух полных поз (pal = fresh + (fresh − prev) / 2: без задержки, без прыжка), положение в машине (anchorFrame) — каждый кадр.
// Только пока водитель в машине, без перехода (fade), без рта и клип — покой ndr или жест руля/газа/тормоза/удара; любая смена (выход, перелёт, разговор) —
// полный пересчёт, значит поза при выходе из машины та же, что без приёма. DRIVER_POSE_OLD=1 — каждый кадр полностью, как раньше
// №520 (Роб, железо, сборка 0516): после удара и сброса машины водителя не было видно. Причина в B5: рамку «вне кадра» брали из H->place — а он обновляется только самой
// позой, которую вне кадра не считают; машина телепортируется сбросом, place остаётся на старом месте вне кадра, поза больше не считается никогда. Теперь рамка — от
// ТЕКУЩЕЙ матрицы машины (anchorFrame), а place заодно освежается. DRIVER_BOX_OLD=1 — прежняя рамка от place (с этой ошибкой)
int homerDriverBox(Homer *H, float bb[6]) {
  if (!H->rootMode || !H->car.ok) return 0;
  if (!DRIVER_BOX_OLD) anchorFrame(H, (float *)&H->place);
  bb[0] = H->place.w.x - 1.6f; bb[1] = H->place.w.y - .6f; bb[2] = H->place.w.z - 1.6f; bb[3] = H->place.w.x + 1.6f; bb[4] = H->place.w.y + 3.f; bb[5] = H->place.w.z + 1.6f;
  return 1;
}

void homerPose(Homer *H) {
  const SkinModel *m = H->m; int nj = (int)m->h->nJoints, root = (int)m->h->rootJoint;
  int drv = !DRIVER_POSE_OLD && H->rootMode && H->car.ok && H->clip >= 0 && (H->clip == H->cInCar || isDrvClip(H, H->clip)) &&
            !(H->fade < H->fadeLen && H->fadeLen > 0.f) && H->jawOpen == 0.f && H->pitch == 0.f;
  if (drv && H->drvPoseValid && (++H->drvPoseTick & 1)) {   // промежуточный кадр: палитра по скорости двух последних полных поз
    for (int j = 0; j < nj; j++) {
      const float *a = (const float *)&H->palFresh[j], *b = (const float *)&H->palPrev[j]; float *o = (float *)&H->pal[j];
      for (int k = 0; k < 16; k++) o[k] = a[k] + (a[k] - b[k]) * .5f;
      o[15] = 1.f; o[3] = o[7] = o[11] = 0.f;
    }
    anchorFrame(H, (float *)&H->place);
    return;
  }
  if (!drv) H->drvPoseValid = 0;
  if (H->clip < 0) sampleLoco(H, H->xf);
  else {
    skinSample(m, H->clip, H->clipT, H->xf);
    if (H->rootMode) {
      if (H->clip == H->cInCar || isDrvClip(H, H->clip)) memcpy(H->xf[root].t, H->seatRoot, 12);   // в машине: поза сиденья водителя (и жесты ndr)
      else skinRootMotion(m, H->clip, H->clipT, H->xf[root].t);
    }
  }
  if (H->fade < H->fadeLen && H->fadeLen > 0.f) {
    float u = smooth01(H->fade / H->fadeLen);
    memcpy(H->tmp, H->snap, sizeof(SkinXf) * nj);
    skinBlend(H->tmp, H->xf, u, nj);
    memcpy(H->xf, H->tmp, sizeof(SkinXf) * nj);
  }
  if (H->jawOpen != 0.f) skinJawOpen(H->xf, H->jaw, H->jawOpen);   // разговор: рот (правка 38)
  skinPalette(m, H->xf, H->pal, H->world);
  if (drv) {   // полная поза водителя: запомнить для промежуточных кадров (до неё — предыдущая полная)
    if (H->drvPoseValid) memcpy(H->palPrev, H->palFresh, sizeof(ScePspFMatrix4) * nj); else memcpy(H->palPrev, H->pal, sizeof(ScePspFMatrix4) * nj);
    memcpy(H->palFresh, H->pal, sizeof(ScePspFMatrix4) * nj); H->drvPoseValid = 1; H->drvPoseTick = 0;
  }
  if (H->rootMode && H->car.ok) anchorFrame(H, (float *)&H->place);
  else if (H->pitch != 0.f) {                                // правка 147: выброс — тело наклонено, ось опускается к земле
    float u = clampf(H->pitch / (.5f * PI_F), 0.f, 1.f);
    skinPlaceTilt(&H->place, H->x, H->y - (HOMER_EJECT_PIV - HOMER_EJECT_LIE) * u, H->z, H->h, H->pitch, HOMER_EJECT_PIV);
    const ScePspFMatrix4 *P = &H->place; float lo = 1e9f;    // тело не уходит в землю: самый низкий сустав (без Motion_Root) над опорой
    for (int j = 0; j < nj; j++) {
      if (j == root || j == H->jExtra) continue;
      const ScePspFVector4 *q = &H->world[j].w;
      float y = P->x.y * q->x + P->y.y * q->y + P->z.y * q->z + P->w.y;
      if (y < lo) lo = y;
    }
#ifndef HOMER_EJECT_NOCLAMP                                   // служебное: красный контроль tests/eject147
    if (lo < H->groundY + HOMER_EJECT_CLEAR) H->place.w.y += H->groundY + HOMER_EJECT_CLEAR - lo;
#endif
  }
  else skinPlace(&H->place, H->x, H->y, H->z, H->h);
}

void homerTalk(Homer *H, const char *g) {   // жест реплики разговора: стоя на земле, один раз (см. HS_FOOT, longIdle)
  int c = g && *g && strcmp(g, "none") ? skinFindClip(H->m, g) : -1;
  if (c >= 0 && H->state == HS_FOOT && H->grounded) { H->cTalk = c; playClip(H, c, 0, .2f, 0.f); H->idleT = 0.f; }
  else if (c < 0 && H->cTalk >= 0 && H->clip == H->cTalk) { H->cTalk = -1; playLoco(H, .2f); }
}

// ------------------------------------------------------------------ init
static int need(int c, int *bad) { if (c < 0) *bad = 1; return c; }
// правка Роба 352 (05.10): игрок уровня — Гомер, Барт, Лиза, Мардж или Апу (пакет игрока уровня l<N>/player.shr, tools/homer_export.py
// --level N). [диск] у каждого свои модель, скелет и клипы <имя>_a.p3d с префиксом hom_/brt_/lsa_/mrg_/apu_ и теми же именами
// движений; префикс пакета — по клипу «<префикс>loco_idle_rest». У Гомера префикс hom_ — имена клипов прежние байт в байт.
// HOMER_PC_OLD=1 (-D) — прежнее: только имена hom_ (у пакета другого персонажа homerInit вернёт -1)
#ifndef HOMER_PC_OLD
#define HOMER_PC_OLD 0
#endif
static char pcPre[8] = "hom_";
static int clipP(const SkinModel *m, const char *suf) { char nm[64]; snprintf(nm, sizeof nm, "%s%s", pcPre, suf); return skinFindClip(m, nm); }
static void pcPrefix(const SkinModel *m) {
  snprintf(pcPre, sizeof pcPre, "hom_");
#if !HOMER_PC_OLD
  int c = skinFindClip(m, "loco_idle_rest");   // точное имя или «<префикс>_loco_idle_rest» (skin.c skinFindClip)
  if (c >= 0) { const char *nm = m->c[c].name; size_t l = strlen(nm), k = strlen("loco_idle_rest");
    if (l > k && l - k < sizeof pcPre) { memcpy(pcPre, nm, l - k); pcPre[l - k] = 0; } }
#endif
}
const char *homerClipPrefix(void) { return pcPre; }

int homerInit(Homer *H, const SkinModel *m) {
  memset(H, 0, sizeof *H);
  H->m = m; int bad = 0; pcPrefix(m);   // правка 352: префикс клипов персонажа
  H->cLoco[0] = need(clipP(m, "loco_idle_rest"), &bad);
  H->cLoco[1] = need(clipP(m, "loco_walk"), &bad);
  H->cLoco[2] = need(clipP(m, "loco_run"), &bad);
  H->cLoco[3] = need(clipP(m, "loco_dash"), &bad);
  H->cTake = need(clipP(m, "jump_idle_take_off"), &bad);
  H->cAir = need(clipP(m, "jump_idle_in_air"), &bad);
  H->cLand = need(clipP(m, "jump_idle_land"), &bad);
  H->cDash2 = need(clipP(m, "jump_dash_in_air"), &bad);
  H->cBreak = need(clipP(m, "break"), &bad);
  H->cJKick = need(clipP(m, "jump_kick"), &bad);
  H->cStA = need(clipP(m, "stomp_antic"), &bad);
  H->cStAir = need(clipP(m, "stomp_in_air"), &bad);
  H->cStLand = need(clipP(m, "stomp_land"), &bad);
  H->cOpenIn = need(clipP(m, "get_into_car_open_door_driver"), &bad);
  H->cIn = need(clipP(m, "get_into_car_driver"), &bad);
  H->cCloseIn = need(clipP(m, "get_into_car_close_door_driver"), &bad);
  H->cOpenOut = need(clipP(m, "get_out_of_car_open_door_driver"), &bad);
  H->cOut = need(clipP(m, "get_out_of_car_driver"), &bad);
  H->cCloseOut = need(clipP(m, "get_out_of_car_close_door_driver"), &bad);
#if !HOMER_DRV_OLD   // красный контроль tests/homer/host.sh: -DHOMER_DRV_OLD=1 — прежняя поза пассажира без жестов
  H->cInCar = skinFindClip(m, "ndr_in_car_idle");   // правки 220/231: поза водителя оригинала (homer.cho in_car_idle_driver)
#else
  H->cInCar = -1;
#endif
  if (H->cInCar < 0) H->cInCar = need(clipP(m, "in_car_idle"), &bad);   // старый пакет: поза пассажира
  H->cDrvTurn[0] = skinFindClip(m, "ndr_turn_left"); H->cDrvTurn[1] = skinFindClip(m, "ndr_turn_right");
  H->cDrvGas = skinFindClip(m, "ndr_accelerate"); H->cDrvBrake = skinFindClip(m, "ndr_decelerate"); H->cDrvCrash = skinFindClip(m, "ndr_crash");
#if HOMER_DRV_OLD
  H->cDrvTurn[0] = H->cDrvTurn[1] = H->cDrvGas = H->cDrvBrake = H->cDrvCrash = -1;
#endif
  H->cFlail = need(skinFindClip(m, "flail"), &bad);      // по суффиксу (правка 147: есть и у brt_/lsa_/mrg_/apu_)
  H->cGetUp = need(skinFindClip(m, "get_up"), &bad);
  H->cTalk = -1; H->jaw = skinFindJoint(m, "Jaw"); H->jExtra = skinFindJoint(m, "Extra_Attributes");        // разговор: жест реплики и рот
  H->cIdle[0] = clipP(m, "idle_scratch");       // необязательные
  H->cIdle[1] = clipP(m, "idle_check_shoes");
  H->cIdle[2] = clipP(m, "idle_chili");
#if !HOMER_PC_OLD
  // правка 352: [диск] у Барта, Лизы, Мардж и Апу свои длинные idle обычного костюма (<имя>.cho <имя>_idle0..2: brt_idle_hands_on_hips,
  // handstand, backflip ...) — пакет кладёт ровно эти три клипа «<префикс>idle_*» в порядке .cho (tools/homer_export.py cho_idles)
  if (H->cIdle[0] < 0 && H->cIdle[1] < 0 && H->cIdle[2] < 0) {
    size_t pl = strlen(pcPre); int k = 0;
    for (unsigned i = 0; i < m->h->nClips && k < 3; i++) if (!strncmp(m->c[i].name, pcPre, pl) && !strncmp(m->c[i].name + pl, "idle_", 5)) H->cIdle[k++] = (int)i;
  }
#endif
  if (bad) return -1;
  for (int i = 0; i < 4; i++) {
    H->locoV[i] = i ? skinClipSpeed(m, H->cLoco[i]) : 0.f;           // 0, 0.87, 3.4, 9.2 м/с
    H->locoStride[i] = H->locoV[i] * clipLen(H, H->cLoco[i]);         // м за цикл шага
  }
  if (H->locoV[1] < .2f) { H->locoV[1] = .87f; H->locoStride[1] = .87f; }
  for (int i = 2; i < 4; i++) if (H->locoV[i] <= H->locoV[i - 1] + .1f) { H->locoV[i] = H->locoV[i - 1] + 2.f; H->locoStride[i] = H->locoV[i] * .53f; }
  skinRootMotion(m, H->cCloseIn, 0.f, H->seatRoot);
  H->anchor[0] = HOMER_DOOR_X; H->anchor[2] = HOMER_DOOR_Z; H->padLast = -1;
  H->clip = -1; H->fade = H->fadeLen = 0.f;
  skinInit(&H->blink, m);
  skinSample(m, H->cLoco[0], 0.f, H->xf); memcpy(H->snap, H->xf, sizeof(SkinXf) * m->h->nJoints);
  skinPlace(&H->place, 0, 0, 0, 0);
  return 0;
}

void homerPlace(Homer *H, const ColWorld *w, float x, float y, float z, float h) {
  float g, n[3];
  H->x = x; H->z = z; H->h = h; H->y = y;
  if (w && colGround(w, x, z, y + 2.f, y - 100.f, &g, n)) H->y = g;
  H->vx = H->vy = H->vz = 0.f; H->grounded = 1; H->jumps = 0; H->groundY = H->y; H->airFrom = H->y;
  H->state = HS_FOOT; H->rootMode = 0; H->st = 0.f; H->speed = 0.f; H->nev = 0;
  H->safe[0] = H->x; H->safe[1] = H->y; H->safe[2] = H->z;
  H->clip = -1; H->fade = H->fadeLen = 0.f; H->camInit = 0; H->maxY = H->y;
  H->frame = h; H->heldX = H->heldY = 0.f; H->padLast = -1; H->bouncing = 0;
  H->pitch = 0.f; H->ejWait = 0.f; H->ejPh = 0;
  H->camHoldD = H->camHoldE = 0.f;
}

// ------------------------------------------------------------------ коллизия
// круг Гомера против кузова машины (прямоугольник в xz), выталкивание наружу
static void pushFromCar(Homer *H, const HomerCar *c) {
  if (!c || !c->ok) return;
  const CarBox *b = &c->box;
  if (H->y >= c->roof - .15f || H->y + HOMER_HEIGHT < c->y + .1f) return;   // выше крыши / ниже
  float dx = H->x - b->x, dz = H->z - b->z, rx = -b->fz, rz = b->fx;
  float a = dx * b->fx + dz * b->fz, s = dx * rx + dz * rz;
  float ca = clampf(a, -b->hl, b->hl), cs = clampf(s, -b->hw, b->hw);
  float ea = a - ca, es = s - cs, d2 = ea * ea + es * es, R = HOMER_RADIUS;
  if (d2 >= R * R) return;
  float na, ns, push;
  if (d2 > 1e-8f) { float d = sqrtf(d2); na = ea / d; ns = es / d; push = R - d; }
  else {                                                     // центр внутри: по ближайшей стороне
    float pa = b->hl - fabsf(a), ps = b->hw - fabsf(s);
    if (pa < ps) { na = a < 0 ? -1.f : 1.f; ns = 0.f; push = pa + R; } else { na = 0.f; ns = s < 0 ? -1.f : 1.f; push = ps + R; }
  }
  float nx = na * b->fx + ns * rx, nz = na * b->fz + ns * rz;
  H->x += nx * push; H->z += nz * push;
  float vn = H->vx * nx + H->vz * nz; if (vn < 0.f) { H->vx -= vn * nx; H->vz -= vn * nz; }
}
// крыша машины как земля (можно запрыгнуть и стоять)
static int carRoofAt(const HomerCar *c, float x, float z, float *y) {
  if (!c || !c->ok) return 0;
  const CarBox *b = &c->box; float dx = x - b->x, dz = z - b->z;
  float a = dx * b->fx + dz * b->fz, s = dx * -b->fz + dz * b->fx;
  if (fabsf(a) > b->hl - .3f || fabsf(s) > b->hw - .15f) return 0;
  *y = c->roof; return 1;
}

// край без дна (HOMER_VOID_WALL, homer.h): из (x0, z0) в (x1, z1), под которыми нет опоры не выше top. 1 — край (шаг отменить).
// Щель между кусками коллизии (опора по обе стороны в HOMER_CRACK по одной из 4 осей: x, z, диагонали) — не край, и Гомер
// стоит на её краю (*bridge — высота опоры, иначе -1e9): точка Гомера уже щели, без моста он проваливался бы в неё до самого
// низа. Оси мира, не хода (у машины — вдоль хода и поперёк): щель — свойство точки, иначе мост держит, пока идёшь, и
// пропадает, когда стоишь (место 65 ночи: мост вдоль хода, стоя — провал сквозь землю на -8 м)
static int voidEdge(const ColWorld *w, float x0, float z0, float x1, float z1, float top, float *bridge) {
  static const float AX[4] = {1.f, 0.f, .70710678f, .70710678f}, AZ[4] = {0.f, 1.f, .70710678f, -.70710678f};
  float g_, ga, gb, dx = x1 - x0, dz = z1 - z0, l = sqrtf(dx * dx + dz * dz);
  *bridge = -1e9f;
  if (colGround(w, x1, z1, top, -1e6f, &g_, 0)) return 0;
  for (int k = 0; k < 4; k++) {
    float ax = AX[k], az = AZ[k];
    if (colGround(w, x1 + ax * HOMER_CRACK, z1 + az * HOMER_CRACK, top, -1e6f, &ga, 0) &&
        colGround(w, x1 - ax * HOMER_CRACK, z1 - az * HOMER_CRACK, top, -1e6f, &gb, 0)) { *bridge = fmaxf(ga, gb); return 0; }
  }
  for (uint32_t i = 0; i < w->ndeath; i++) {   // след зоны DEATH (по высоте — её середина): падение туда — как в оригинале
    const ColZone *v = (const ColZone *)&w->death[i];
    if (colZoneContains(v, x1, v->c[1], z1, .5f)) return 0;
  }
  return l > 0.f;   // любой сдвиг, и микронный (иначе тело ползёт за край по 1e-6 м за подшаг); стоял на месте — отменять нечего
}

// колонка без опоры мира ниже (x, y, z) до самого низа и не след зоны DEATH (туда падать можно — возвращает deathCheck, как в оригинале)
static int voidColumn(const ColWorld *w, float x, float y, float z) {
  float g_;
  if (colGround(w, x, z, y + 1.f, -1e6f, &g_, 0)) return 0;
  for (uint32_t i = 0; i < w->ndeath; i++) { const ColZone *v = (const ColZone *)&w->death[i]; if (v->c[1] <= y + 1.f && colZoneContains(v, x, v->c[1], z, .5f)) return 0; }   // зона ниже тела — туда падаем; выше (R7D14 над ямой АЭС) — не в счёт
  return 1;
}

#ifndef HOMER_VOID_FLOOR_OLD
// Правка 142, семейство «над пустотой без шага» (ночи 02.10–07.10, L1 J14-3/J14-6/K14-1 у дороги l1r2 под холмом): шаг в край без
// дна уже стена (voidEdge), но Гомер попадает в столб без опоры и без шага — выход через верх на крышу машины, которая стоит у
// края дороги под нависающим склоном, и машина отъезжает/скатывается из-под него (или он сходит с крыши, а крыша уже не над
// землёй). Тогда каждый подшаг в стороны отменён, а вниз опоры нет — падение до «выпал из мира» (60 м). Оракул PS2 f03/f08
// (build/ps2ref/answers, фазы S/P): машина оригинала в таком месте (под склоном, Intersect нет) не падает, а держится на
// продолжении опоры — S f03 и S f08 стоят на 3.19, как на земле фазы L (3.18), P f08 едет 120 кадров по ровной 2.36 без
// Intersect под собой. Пешком оригинал не мерен; правило порта (как «край мира — стена и пол» у колёс машины, car.c): столб
// без опоры не выше top, вне щели и вне следа зоны DEATH — пол на высоте ближайшей опоры рядом (кольца до HOMER_VOID_FLOOR_R,
// первая дистанция с попаданием, самая высокая из них). Оттуда Гомер шагает к опоре (там земля — не край), вглубь пустоты
// шаг по-прежнему стена. Над следом DEATH — падение, как раньше (возвращает зона). Геометрию не добавляет и не вырезает
#define HOMER_VOID_FLOOR_R 6.f
static int voidFloor(const ColWorld *w, float x, float z, float top, float *fy) {
  static const float AX[8] = {1.f, .70710678f, 0.f, -.70710678f, -1.f, -.70710678f, 0.f, .70710678f};
  static const float AZ[8] = {0.f, .70710678f, 1.f, .70710678f, 0.f, -.70710678f, -1.f, -.70710678f};
  static const float RR[] = {.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, HOMER_VOID_FLOOR_R};
  float g;
  if (colGround(w, x, z, top, -1e6f, &g, 0)) return 0;          // опора под ногами есть — не пустота
  for (uint32_t i = 0; i < w->ndeath; i++) {                     // след зоны DEATH: падение законно (возвращает зона)
    const ColZone *v = (const ColZone *)&w->death[i];
    if (colZoneContains(v, x, v->c[1], z, .5f)) return 0;
  }
  for (unsigned r = 0; r < sizeof RR / sizeof RR[0]; r++) {
    float best = -1e9f;
    for (int k = 0; k < 8; k++)
      if (colGround(w, x + AX[k] * RR[r], z + AZ[k] * RR[r], top, -1e6f, &g, 0) && g > best) best = g;
    if (best > -1e8f) { *fy = best; return 1; }
  }
  return 0;
}
#endif

// один подшаг движения с коллизией. g — гравитация (стомп: ×3.22)
static void stepMove(Homer *H, const ColWorld *w, const HomerCar *car, float dt, float g) {
  float x0 = H->x, z0 = H->z;
  float nx = H->x + H->vx * dt, nz = H->z + H->vz * dt;
  float oldY = H->y, ny;
  if (H->grounded) ny = H->y; else { H->vy -= g * dt; if (H->vy < -50.f) H->vy = -50.f; ny = H->y + H->vy * dt; }
  float ix = nx, iz = nz; (void)ix; (void)iz;                  // куда шёл сам, до выталкивания стенами
  if (w) {
    CarBox b; memset(&b, 0, sizeof b);
    b.x = nx; b.z = nz; b.fx = 0.f; b.fz = 1.f; b.hw = b.hl = HOMER_RADIUS;
    b.y = ny; b.lo = H->grounded ? HOMER_STEP : .15f; b.hi = HOMER_HEIGHT;
    float mx, mz, cnx, cnz;
    float pvx = H->vx, pvz = H->vz;                         // желаемая скорость до гашения стеной
    int pushed = colPushCar(w, &b, &mx, &mz, &cnx, &cnz);
    if (pushed) {
      for (int k = 0; k < b.n; k++) {                        // скорость в стену гасим (скольжение вдоль)
        float vn = H->vx * b.nx[k] + H->vz * b.nz[k];
        if (vn < 0.f) { H->vx -= vn * b.nx[k]; H->vz -= vn * b.nz[k]; }
      }
    }
    nx = b.x; nz = b.z;
#if HOMER_STAIR_UP
    // Лесенка из плит (гараж АЭС L1: уступы 1–1,55 м из наклонных ящиков). Узкое правило, чтобы не лезть на заборы и крыши:
    //  начало цепочки — упёрся в стену, а впереди верх НАКЛОННОЙ плиты (n.y в [HOMER_STAIR_NY, 0,9995)), выше шага, не выше HOMER_STAIR_MAX;
    //  продолжение — стоит на наклонной плите и идёт вверх по её уклону (не поперёк) на следующую наклонную плиту, либо в течение HOMER_STAIR_CHAIN_T с последнего
    //  подъёма идёт в ту же сторону (площадки и дорога в продолжении лесенки, они ровные). Ровный выступ сам цепочку не начинает.
    // Выключено по умолчанию до эталона PS2 (ходит ли Гомер там пешком): -DHOMER_STAIR_UP=1
    if (H->stairChain > 0.f) H->stairChain -= dt;
    int slabStand = 0; float upx = 0.f, upz = 0.f;
    if (H->grounded) { float cy, cn[3]; int cm = 0; uint32_t cr = 0; if (colGroundMatId(w, x0, z0, oldY + .15f, oldY - .3f, &cy, cn, &cm, &cr) && cm == COL_MAT_HARD && (cr >> 29) != COL_CYL && cn[1] < .9995f && cn[1] >= HOMER_STAIR_NY - .02f) {
        float ul = sqrtf(cn[0] * cn[0] + cn[2] * cn[2]); if (ul > 1e-4f) { slabStand = 1; upx = -cn[0] / ul; upz = -cn[2] / ul; } } }   // стоит на наклонной плите; вверх по уклону — против горизонтальной части нормали
    if (H->grounded && (pushed || slabStand || H->stairChain > 0.f)) {
      float sp = sqrtf(pvx * pvx + pvz * pvz);
      if (sp > .05f) {
        float fx = pvx / sp, fz = pvz / sp;
        int chain = H->stairChain > 0.f && fx * H->stairDx + fz * H->stairDz > .7f;
        int up = slabStand && fx * upx + fz * upz > .5f;
        for (float d = HOMER_RADIUS + .1f; d <= HOMER_RADIUS + .7f; d += .2f) {
          float sy, sn[3];
          int smat = 0; uint32_t sref = 0;
          if (colGroundMatId(w, x0 + fx * d, z0 + fz * d, oldY + HOMER_STAIR_MAX, oldY + HOMER_STEP, &sy, sn, &smat, &sref) && smat == COL_MAT_HARD && (sref >> 29) != COL_CYL && sn[1] >= HOMER_STAIR_NY && sy > oldY + HOMER_STEP && sy <= oldY + HOMER_STAIR_MAX) {   // не куст и не купол цилиндра/капсулы
            int tilted = sn[1] < .9995f;
            if (!(((pushed || up) && tilted && (sref >> 29) == COL_BOX) || chain)) continue;   // начало цепочки только на наклонную плиту-ящик (COL_BOX, не треугольник дороги); площадки и дорога — только в цепочке сразу после подъёма
            float cy; int hasCeil = colGround(w, x0 + fx * d, z0 + fz * d, sy + HOMER_HEIGHT + 1.f, sy + HOMER_HEIGHT, &cy, 0);   // над ней нет потолка в рост Гомера
            if (!hasCeil) { ny = oldY = sy; nx = x0 + fx * (d - .05f); nz = z0 + fz * (d - .05f); H->stairUps++; H->stairChain = HOMER_STAIR_CHAIN_T; H->stairDx = fx; H->stairDz = fz; break; }
          }
        }
      }
    }
#endif
  }
  H->x = nx; H->z = nz; H->y = ny;
  pushFromCar(H, car);
  float stop = -1e9f;                                         // опора сверху твёрдого тела (ящик, автомат)
  if (H->solid) {
    float sx, sz;
    if (H->solid(H->solidUser, &H->x, &H->z, H->y, HOMER_RADIUS, &stop, &sx, &sz)) {
      float vn = H->vx * sx + H->vz * sz; if (vn < 0.f) { H->vx -= vn * sx; H->vz -= vn * sz; }
    }
  }
  // земля
  float gy = -1e9f, n[3]; int has = 0;
  float top = (H->grounded ? oldY : fmaxf(oldY, ny)) + HOMER_STEP;
  if (w) has = colGround(w, H->x, H->z, top, ny - 200.f, &gy, n);
  { float ry; if (carRoofAt(car, H->x, H->z, &ry) && ry <= top && (!has || ry > gy)) { gy = ry; has = 1; } }
  if (stop > -1e8f && stop <= top && (!has || stop > gy)) { gy = stop; has = 1; }
  float bridge = -1e9f; int onVoid = 0;
#ifndef HOMER_VOID_FLOOR_OLD
  { float g0, fy;   // уже стоит над пустотой (пол voidFloor): шаги по полу к опоре и вдоль неё — не край (иначе заперт в 1–2 м от дороги)
    if (HOMER_VOID_WALL && w && !has && !colGround(w, x0, z0, top, -1e6f, &g0, 0) && voidFloor(w, H->x, H->z, top, &fy)) onVoid = 1; }
#endif
#if !defined(HOMER_VOID_FLOOR_OLD) && !defined(HOMER_VOID_SQUEEZE_OLD)
  // ДОБИВКА 13, L1 F10-8 (ящик b188): Гомер спрыгнул со склона в щель между стенкой b188 и пустотой под склоном — сам шёл на
  // опору, а стена выдавила его за край без дна; voidEdge возвращал назад, стена снова выдавливала — заперт навсегда. Стена
  // главнее края: если в пустоту вытолкнула стена, а сам он шёл на опору, — шаг не отменяется, дальше пол voidFloor
  { float g1, fy;
    if (!onVoid && HOMER_VOID_WALL && w && !has && fabsf(H->x - ix) + fabsf(H->z - iz) > 1e-5f &&
        colGround(w, ix, iz, top, -1e6f, &g1, 0) && voidFloor(w, H->x, H->z, top, &fy)) onVoid = 1; }
#endif
  if (HOMER_VOID_WALL && w && !has && !onVoid && voidEdge(w, x0, z0, H->x, H->z, top, &bridge)) {   // край без дна — стена: шаг назад, ход в край гасим
    float dx = H->x - x0, dz = H->z - z0, l = sqrtf(dx * dx + dz * dz), vn = (H->vx * dx + H->vz * dz) / l;   // l > 0: voidEdge
    if (vn > 0.f) { H->vx -= vn * dx / l; H->vz -= vn * dz / l; }
    H->x = x0; H->z = z0; H->voidStops++;
    has = colGround(w, H->x, H->z, top, ny - 200.f, &gy, n);
    { float ry; if (carRoofAt(car, H->x, H->z, &ry) && ry <= top && (!has || ry > gy)) { gy = ry; has = 1; } }
    if (!has && voidEdge(w, H->x, H->z, H->x, H->z, top, &bridge) == 0 && bridge > -1e8f) { gy = bridge; has = 1; n[0] = n[2] = 0.f; n[1] = 1.f; }
  } else if (HOMER_VOID_WALL && w && !has && bridge > -1e8f) { gy = bridge; has = 1; n[0] = n[2] = 0.f; n[1] = 1.f; }   // щель — мост
#ifndef HOMER_VOID_FLOOR_OLD
  if (HOMER_VOID_WALL && w && !has) { float fy; if (voidFloor(w, H->x, H->z, top, &fy)) { gy = fy; has = 1; n[0] = n[2] = 0.f; n[1] = 1.f; H->voidFloors++; } }   // над пустотой без шага — пол
#endif
  if (has) H->groundY = gy;
  if (H->grounded) {
    if (has && gy >= oldY - HOMER_SNAP) { H->y = gy; H->vy = 0.f; }
    else { H->grounded = 0; H->vy = 0.f; H->airFrom = oldY; H->launchV = sqrtf(H->vx * H->vx + H->vz * H->vz); }   // сошёл с края
  } else if (has && H->vy <= 0.f && H->y <= gy) {
    H->lastLandV = -H->vy; H->y = gy; H->vy = 0.f; H->grounded = 2; H->bouncing = 0;   // 2 = приземлился в этом подшаге
    H->landings++;
  }
  if (H->y > H->maxY) H->maxY = H->y;
}

static void moveBody(Homer *H, const ColWorld *w, const HomerCar *car, float dt, float g) {
  int n = (int)ceilf(dt * 120.f); if (n < 1) n = 1; if (n > 8) n = 8;   // подшаги по 1/120 с
  float sdt = dt / n; int landed = 0;
  for (int i = 0; i < n; i++) { stepMove(H, w, car, sdt, g); if (H->grounded == 2) { landed = 1; H->grounded = 1; } }
  H->speed = sqrtf(H->vx * H->vx + H->vz * H->vz);
  if (landed) {
    H->jumps = 0;
    pushEv(H, HOMER_EV_LAND, H->x, H->y, H->z, H->lastLandV, 0, 0);
    H->grounded = 3;                                          // флаг для автомата состояний (сбрасывается там)
  }
  // выпал из мира: назад на последнее надёжное место
  if (HOMER_VOID_RECOVER && w && !H->grounded && H->vy < 0.f && H->y < H->airFrom - HOMER_VOID_DROP && voidColumn(w, H->x, H->y, H->z)) {
    H->x = H->safe[0]; H->y = H->safe[1]; H->z = H->safe[2]; H->vx = H->vy = H->vz = 0.f; H->grounded = 1; H->voidRecovers++;   // колонка без опоры мира: незачем падать 60 м
  } else
  if (!H->grounded && H->y < H->airFrom - 60.f) {
    H->x = H->safe[0]; H->y = H->safe[1]; H->z = H->safe[2]; H->vx = H->vy = H->vz = 0.f; H->grounded = 1;
  }
}

// ------------------------------------------------------------------ управление
static void steer(Homer *H, const HomerInput *in, float dt, int allowMove) {
  float mx = in ? in->mx : 0.f, my = in ? in->my : 0.f;
  float mag = sqrtf(mx * mx + my * my); if (mag > 1.f) { mx /= mag; my /= mag; mag = 1.f; }
  if (!allowMove) mag = 0.f;
  if (H->steerGap > .1f) H->neutralT = 9.f;   // №920: steer не звали (машина, пинок, падение) — стик «в нуле давно», как до правки
  H->steerGap = 0.f;
  // направление: курс камеры в момент, когда стик изменился (как в оригинале: держишь — мир не крутится с камерой)
  // №836 (Роб 18.10: «после перебивки камеры — выход из будки, из дома — начинаешь бежать, а направление уезжает за камерой; управление должно
  // остаться таким, как было начато относительно вида, а когда камера выровнялась и меняешь направление — уже от вида»): курс вида берётся,
  // когда стик тронут из центра; дальше — только при настоящей смене направления (угол стика больше STEER_RECAP_A, не сила нажима и не дрожь
  // ниппеля) и только когда камера не в переходе (homerCamBusy); в переходе курс держится. STEER836_OLD=1 — прежнее правило (сдвиг стика .25)
#ifndef STEER836_OLD
#define STEER836_OLD 0
#endif
#define STEER_RECAP_A .61f   // рад ≈ 35°
#define STEER_SWEEP_T 1.f     // №947б: окно серии смен угла стика
  if (in && STEER836_OLD && (fabsf(mx - H->heldX) > .25f || fabsf(my - H->heldY) > .25f || (mag > 0.f && H->heldX == 0.f && H->heldY == 0.f))) {
    H->frame = in->camYaw; H->heldX = mx; H->heldY = my;
  }
  if (in && !STEER836_OLD && mag > 0.f) {
    int start_ = H->heldX == 0.f && H->heldY == 0.f;
    float da_ = start_ ? 0.f : fabsf(wrapPi(atan2f(mx, my) - atan2f(H->heldX, H->heldY)));
    // №916 (Роб: «подходим к машине или к двери — камера мечется, и от этого меняется управление; как шли, так и идём, просто меняется point of view»):
    // пока стик не отпущен, кадр управления остаётся тем, с которого начато движение; смена угла стика поворачивает ход внутри этого кадра, а не берёт вид
    // заново (вид к тому времени уже ушёл за курсом ходьбы, обошёл кузов, встал у двери). Новый кадр — только когда стик отпущен и тронут заново.
    // CTRL916_OLD=1 / flag ctrl916old 1 — прежнее правило №836 (смена угла стика > 35° берёт текущий вид)
    // №947б: стик, ведомый дугой через центр (разворот бегом), даёт серию «смен угла» подряд; каждая брала бы вид, уже ушедший за курсом Гомера (камера идёт за ним), и ход уходил боком.
    // Вид берётся один раз на серию: пока с прошлого взятия не прошло STEER_SWEEP_T, новый угол только запоминается
    H->sweepT = fmaxf(0.f, H->sweepT - dt);
    if (start_) { if (!homerFlipHold && !(H->flipLatch && !homerWalkCam1059Old)) H->frame = in->camYaw; H->heldX = mx; H->heldY = my; }   // №947б: перескок камеры у стены (игра) — кадр прежний
    else if (homerCtrl916Old && mag > .3f) {
      if (H->sweepT > 0.f) { H->heldX = mx; H->heldY = my; }   // серия: угол стика только запоминается
      else if (da_ > STEER_RECAP_A) { if (!homerCamBusy) { H->frame = in->camYaw; H->sweepT = STEER_SWEEP_T; } H->heldX = mx; H->heldY = my; }   // камера игры (homerCamBusy) — вид не берём
    }
  }
  // №920 (друг Роба: «прыжки по платформам у стены, камера уходит от стены, и направление стика смещается, особенно если держать стик непрерывно»): стик, который
  // «держат непрерывно», на деле на 0,05–0,15 с уходит в ноль (палец на ✕, дрожь ниппеля в мёртвой зоне); кадр управления сбрасывался, и ход брался от нынешнего вида,
  // а вид у стены как раз ушёл за курсом (проверка: перерыв 0,1 с на 1-й секунде отхода камеры — курс 180° -> −146° за кадр). Кадр держится ещё STEER_NEUTRAL_T после того,
  // как стик ушёл в ноль; настоящее «отпустил и тронул заново» (дольше) берёт вид, как раньше. CTRL916_OLD=1 / flag ctrl916old 1 — сброс сразу
#define STEER_NEUTRAL_T .3f
  if (mag == 0.f) { if (homerCtrl916Old || (H->neutralT += dt) > STEER_NEUTRAL_T) { H->heldX = H->heldY = 0.f; } }
  else H->neutralT = 0.f;
  // №1059 (Роб 27.10: «идёшь, камера меняется, а как адаптироваться, дезориентация небольшая; сверь с PS2»). Замер PS2 (tools/twins/walkcam1059.py, 7 прогонов, knowledge/facts/ps2-walk-camera-stick.yaml):
  // пока камера игры в сценарном облёте (выход из машины) курс держится; когда камера следует за Гомером и встала, стик снова от ВИДА, постоянно: зажатый «влево» ведёт по кругу (ход − вид ≈ −76°
  // вместо −90° из-за разгона), «вверх» после облёта сходится с видом за ≤ 0,5 с. Порт держал кадр от нажатия до отпускания: после облёта «влево» шёл прямо «вверх по экрану» (ход − вид 0° вместо −90°).
  // Теперь пока стик зажат, кадр идёт за видом (homerViewYaw) со скоростью не выше WALKCAM1059_RATE (камера игрока L/R — сразу); в облёте (homerCamBusy) и 1,5 с после перескока у стены (homerFlipHold) кадр держится.
  // WALKCAM1059_OLD=1 / файл walkcam1059old / flag walkcam1059old 1 — кадр держится до отпускания, как было
  if (homerCamFlipN != H->flipSeen) { H->flipSeen = homerCamFlipN; if (!homerFlipBusyOld) H->flipLatch = 1; }   // перескок камеры у стены (игра): кадр держится, пока стик не отпущен (№947б, гейт ctrl920)
  if (H->camTurn != 0.f) H->flipLatch = 1;   // игрок крутит камеру L/R (у PS2 такой кнопки пешком нет): курс держится, пока стик не отпущен или не сменил угол (№947б, гейт walk947 W_camhold)
  if (mag == 0.f) { H->flipLatchT += dt; if (H->flipLatchT > STEER_NEUTRAL_T) H->flipLatch = 0; } else H->flipLatchT = 0.f;   // дрожь стика в ноль (0,1 с) не снимает; настоящее отпускание (> 0,3 с) снимает
  if (in && !homerWalkCam1059Old && homerCtrl916Old && homerViewOk && mag > .1f && !(H->heldX == 0.f && H->heldY == 0.f) && !homerCamBusy && !homerFlipHold && !H->flipLatch) {
    // кадр идёт за ВИДИМЫМ курсом камеры (передний вектор, как cam fwd PS2: при ходе точно на камеру он стоит, курс −177,8 → −177,2 за 90 кадров; желаемый camYaw опережает видимую камеру на ≈10°) первым порядком с запаздыванием
    // WALKCAM1059_TAU: у PS2 курс запаздывает за вращающейся камерой на ≈7° при 36°/с (ход − вид 83°, не 90°) и сходится после облёта за 0,4–0,6 с; скорость не выше WALKCAM1059_RATE
    float d_ = wrapPi(homerViewYaw - H->frame), mt_ = homerWalkCamRate * dt, k_ = 1.f - expf(-dt / homerWalkCamTau);
    H->frame = wrapPi(H->frame + clampf(d_ * k_, -mt_, mt_));
  }
  float dir = H->frame - atan2f(mx, my);
  float top = H->grounded ? ((in && in->dash) ? HOMER_RUN : HOMER_WALK) : HOMER_AIR_SPEED;   // ○ зажат — бег (ControllerPS2.pag)
  float target = mag * top;
  if (!H->grounded && H->bouncing && mag == 0.f) return;       // полёт с подкидывающей точки: без стика дуга не гаснет
  float dvx = sinf(dir) * target - H->vx, dvz = cosf(dir) * target - H->vz;
  float cur = sqrtf(H->vx * H->vx + H->vz * H->vz);
  float rate = H->grounded ? (target > cur ? HOMER_ACCEL : HOMER_DECEL) : HOMER_AIR_ACCEL;
  float dl = sqrtf(dvx * dvx + dvz * dvz), mxd = rate * dt;
  if (dl > mxd) { dvx *= mxd / dl; dvz *= mxd / dl; }
  H->vx += dvx; H->vz += dvz;
  if (!H->grounded) {                                         // в воздухе не быстрее скорости отрыва (и не меньше 4 м/с)
    float cap = fmaxf(HOMER_AIR_SPEED, H->launchV), s = sqrtf(H->vx * H->vx + H->vz * H->vz);
    if (s > cap) { H->vx *= cap / s; H->vz *= cap / s; }
  }
  if (mag > .1f) {
    float d = wrapPi(dir - H->h), mt = HOMER_TURN * dt;
    H->h = wrapPi(H->h + clampf(d, -mt, mt));
  }
}

// Подкидывающая точка (правка Роба 46: «вентиляции/паровые отдушины на крышах подбрасывают, чтобы попасть на второй
// этаж; пример — крыша красного грузовика у Kwik-E-Mart»). Данные: Event 6 диска (duffjump, kwikjump, gasjump, steam1,
// spring1..3, sv01/02…; блок JMP1 COL1, tools/jumps_export.py): объём TriggerVolume — где встать, положение локатора —
// куда взлететь. Правило порта (формы дуги в данных нет, сверить с эталоном PS2): вершина дуги — точка локатора при
// гравитации Гомера (HOMER_GRAVITY), по горизонтали за время подъёма ровно до неё; дальше обычное падение с той же
// скоростью вбок (на крышу/этаж). Срабатывает, когда Гомер стоит (ступни + 0.3 м) в объёме, раздутом на свой радиус;
// пока не вышел из этой точки — повторно не подбрасывает.
int homerBounceOff;   // служебное: отрицательный контроль (tests/jumps) — подкидывающие точки выключены
static int tryBounce(Homer *H, const ColWorld *w) {
  if (!w || !w->npad || homerBounceOff) return 0;
  int p = colBouncePadAt(w, H->x, H->y + .3f, H->z, HOMER_RADIUS);
  if (p < 0) { H->padLast = -1; return 0; }
  if (p == H->padLast) return 0;
  const float *A = w->pad[p].apex; float dy = A[1] - H->y;
  H->padLast = p;
  if (dy < .5f) return 0;                                    // вершина ниже ступней — не пружина
  float vy = sqrtf(2.f * HOMER_GRAVITY * dy), t = vy / HOMER_GRAVITY;
  H->vx = (A[0] - H->x) / t; H->vz = (A[2] - H->z) / t; H->vy = vy;
  H->grounded = 0; H->jumps = 1; H->airFrom = H->y; H->bouncing = 1; H->bounces++;
  H->launchV = sqrtf(H->vx * H->vx + H->vz * H->vz); H->speed = H->launchV;
  memcpy(H->padApex, A, sizeof H->padApex);
  if (H->launchV > .3f) H->h = atan2f(H->vx, H->vz);         // лицом по полёту
  playClip(H, H->cTake, 0, .01f, 0.f);
  float l = H->launchV > 1e-3f ? H->launchV : 1.f;
  pushEv(H, HOMER_EV_BOUNCE, H->x, H->y, H->z, dy, H->vx / l, H->vz / l);
  return 1;
}

static void doJump(Homer *H, int second) {
  H->vy = sqrtf(2.f * HOMER_GRAVITY * (second ? HOMER_JUMP2_H : HOMER_JUMP_H));
  H->jumps = second ? 2 : 1; H->grounded = 0; H->airFrom = H->y;
  H->launchV = sqrtf(H->vx * H->vx + H->vz * H->vz);         // скорость отрыва = потолок скорости в воздухе
  if (!second) playClip(H, H->cTake, 0, .01f, 0.f);           // cho: jump_idle_launch blendIn 0.01
  else playClip(H, H->cDash2, 0, .5f, 0.f);                   // cho: jump_dash_in_air blendIn 0.5
  pushEv(H, HOMER_EV_JUMP, H->x, H->y, H->z, second ? 2.f : 1.f, sinf(H->h), cosf(H->h));
}

static void kickHit(Homer *H, float fwd, float up) {
  float fx = sinf(H->h), fz = cosf(H->h);
  pushEv(H, HOMER_EV_HIT, H->x + fx * fwd, H->y + up, H->z + fz * fwd, HOMER_KICK_REACH, fx, fz);
  H->hitSent = 1;
}

int homerCanEnter(const Homer *H, const HomerCar *car) {
  if (!car || !car->ok || car->dead || (H->state != HS_FOOT && H->state != HS_KICK)) return 0;   // в остов не садится
  float p[3]; xform(car->m, car->door[0], -car->lift, car->door[1], p);
  if (fabsf(p[1] - H->y) > 2.f) return 0;
  float dx = p[0] - H->x, dz = p[2] - H->z;
  if (dx * dx + dz * dz < HOMER_ENTER_DIST * HOMER_ENTER_DIST) return 1;
  // с любой стороны: до кузова не дальше HOMER_ENTER_BODY (дальше сам обойдёт машину к двери водителя)
  const CarBox *b = &car->box; float ex = H->x - b->x, ez = H->z - b->z;
  float a = fabsf(ex * b->fx + ez * b->fz) - b->hl, s = fabsf(-ex * b->fz + ez * b->fx) - b->hw;
  a = a > 0.f ? a : 0.f; s = s > 0.f ? s : 0.f;
  return a * a + s * s < HOMER_ENTER_BODY * HOMER_ENTER_BODY;
}

int homerEnterCar(Homer *H, const HomerCar *car, const HomerCarCb *cb) {
  if (!homerCanEnter(H, car)) return 0;
  H->car = *car; if (cb) H->cb = *cb; else memset(&H->cb, 0, sizeof H->cb);
  setAnchor(H, 0.f);
  H->state = HS_TO_DOOR; H->st = 0.f; H->phase = 0;
  return 1;
}

static void startSeq(Homer *H, int state, int clip) {
  H->state = state; H->st = 0.f; H->phase = 0; H->rootMode = 1;
  H->vx = H->vy = H->vz = 0.f; H->speed = 0.f;
  playClip(H, clip, 0, .15f, 0.f);
}

void homerSetInCar(Homer *H, const HomerCar *car) {
  H->car = *car; setAnchor(H, 1.f);
  H->state = HS_IN_CAR; H->rootMode = 1; H->door = 0.f; H->pitch = 0.f; H->ejWait = 0.f; H->ejPh = 0;
  H->clip = H->cInCar; H->clipLoop = 1; H->clipT = 0.f; H->clipRate = 1.f; H->fade = H->fadeLen = 0.f;
}

int homerExitCar(Homer *H, const HomerCar *car, const HomerCarCb *cb) {
  if (H->state != HS_IN_CAR || H->ejWait > 0.f) return 0;   // правка 147: остов горит — выброс уже взведён
  H->car = *car; if (cb) H->cb = *cb;
  startSeq(H, HS_GET_OUT, H->cOpenOut); H->fadeLen = .1f;     // cho: get_out_of_car_open_door_driver blendIn .1
  return 1;
}

void homerHurt(Homer *H, float fromX, float fromZ, float power) {
  if (!homerOnFoot(H) || H->state == HS_HURT) return;
  float dx = H->x - fromX, dz = H->z - fromZ, l = sqrtf(dx * dx + dz * dz);
  if (l < 1e-3f) { dx = -sinf(H->h); dz = -cosf(H->h); l = 1.f; }
  dx /= l; dz /= l; if (power <= 0.f) power = 1.f;
  H->vx = dx * 4.f * power; H->vz = dz * 4.f * power; H->vy = 5.f * power; H->grounded = 0; H->airFrom = H->y;
  H->launchV = 4.f * power;
  H->h = atan2f(-dx, -dz);                                   // лицом к удару, летит спиной
  H->state = HS_HURT; H->st = 0.f; H->phase = 0;
  H->ejPh = 0; H->pitch = 0.f;
  playClip(H, H->cFlail, 0, .05f, 0.f);
  pushEv(H, HOMER_EV_HURT, H->x, H->y + .9f, H->z, power, dx, dz);
}

static void setDoor(Homer *H, float open) {
  open = clampf(open, 0.f, 1.f);
  if (fabsf(open - H->door) < 1e-4f) return;
  H->door = open; if (H->cb.door) H->cb.door(H->cb.user, open);
}

static void exitOverTop(Homer *H);
// выход закончен: переносим корень позы в положение/курс Гомера
static void finishExit(Homer *H, const ColWorld *w) {
  const SkinModel *m = H->m; int root = (int)m->h->rootJoint;
  setAnchor(H, 0.f);
  float F[16]; anchorFrame(H, F);
  float rt[3]; skinRootMotion(m, H->cCloseOut, clipLen(H, H->cCloseOut), rt);
  float p[3]; xform(F, rt[0], 0.f, rt[2], p);
  skinSample(m, H->cCloseOut, clipLen(H, H->cCloseOut), H->tmp);
  const float *q = H->tmp[root].q;                            // курс корня в конце выхода (поворот вокруг Y)
  float yaw = 2.f * atan2f(q[1], q[3]);
  // Г4 (01.10): машина за время выхода сползла к краю без дна (места 95/143 ночи: конец выхода над пустотой) — через верх
  float br_; if (HOMER_VOID_WALL && w && voidEdge(w, H->car.x, H->car.z, p[0], p[2], p[1] + HOMER_STEP, &br_)) { exitOverTop(H); return; }
  H->x = p[0]; H->y = p[1]; H->z = p[2]; H->h = wrapPi(anchorHeading(H) + yaw);
  float g, n[3];
  if (w && colGround(w, H->x, H->z, H->y + 1.f, H->y - 5.f, &g, n)) H->y = g;
  H->vx = H->vy = H->vz = 0.f; H->grounded = 1; H->jumps = 0; H->state = HS_FOOT; H->rootMode = 0; H->st = 0.f;
  H->groundY = H->y; H->airFrom = H->y; H->frame = H->h;
  // снимок позы — в новой системе (корень уже в положении/курсе)
  memcpy(H->snap, H->xf, sizeof(SkinXf) * m->h->nJoints);
  memcpy(H->snap[root].q, m->j[root].q, 16); memcpy(H->snap[root].t, m->j[root].t, 12);
  H->clip = -1; H->fade = 0.f; H->fadeLen = .15f;
  H->camInit = 0;
  H->overTop = 0;
  pushEv(H, HOMER_EV_ON_FOOT, H->x, H->y, H->z, 0, sinf(H->h), cosf(H->h));
  if (H->cb.done) H->cb.done(H->cb.user, 0);
}

// выход через верх (Роб 28.09 «если прижат и не может выйти — повалился поверх машины», так в оригинале): место у двери
// водителя занято (стена COL1 или твёрдое тело — стейтпроп, персонаж) — дверь не открывается, Гомер оказывается на
// крыше и спрыгивает/падает оттуда обычной физикой прыжка (клипа выхода через верх на диске нет: только _driver и
// пассажирские выходы, пассажирскими пользуются сидящие справа персонажи). Правило порта: проба у двери и в точке конца выхода
static int exitBlocked(Homer *H, const ColWorld *w) {
  float a0[3]; memcpy(a0, H->anchor, sizeof a0); setAnchor(H, 0.f);   // проба — у двери, куда выйдет Гомер
  float F[16]; anchorFrame(H, F); memcpy(H->anchor, a0, sizeof a0);
  float rt[3]; skinRootMotion(H->m, H->cCloseOut, clipLen(H, H->cCloseOut), rt);
  float pe[3], pd[3]; xform(F, rt[0], 0.f, rt[2], pe); xform(F, 0.f, 0.f, 0.f, pd);
  for (int k = 1; k <= 2; k++) {                              // середина пути от двери и конец выхода
    float px = pd[0] + (pe[0] - pd[0]) * k * .5f, pz = pd[2] + (pe[2] - pd[2]) * k * .5f, py = H->car.y;
    if (w) {
      CarBox b; memset(&b, 0, sizeof b);
      b.x = px; b.z = pz; b.fx = 0.f; b.fz = 1.f; b.hw = b.hl = HOMER_RADIUS; b.y = py; b.lo = .35f; b.hi = HOMER_HEIGHT;
      if (colProbeCar(w, &b) && b.maxDepth > .15f) return 1;   // упёрся, а не задел краем (изнанку забора не считаем)
      // Г4 (01.10): у двери край без дна (машина у обрыва, место 14 ночи: выход ставил Гомера над пустотой) — тоже занято
      float br_; if (HOMER_VOID_WALL && voidEdge(w, H->car.x, H->car.z, px, pz, py + HOMER_STEP, &br_)) return 1;
    }
    if (H->solid) {
      float x = px, z = pz, top = -1e9f, nx, nz;
      if (H->solid(H->solidUser, &x, &z, py, HOMER_RADIUS, &top, &nx, &nz) && (x - px) * (x - px) + (z - pz) * (z - pz) > .15f * .15f) return 1;
    }
  }
  return 0;
}
static void exitOverTop(Homer *H) {
  const SkinModel *m = H->m; int root = (int)m->h->rootJoint;
  const HomerCar *c = &H->car;
  H->x = c->x; H->z = c->z; H->y = c->roof; H->h = c->h;
  H->vx = H->vz = 0.f; H->vy = sqrtf(2.f * HOMER_GRAVITY * .5f); H->speed = 0.f; H->launchV = 0.f;
  H->grounded = 0; H->jumps = 1; H->airFrom = H->y; H->groundY = H->y;
  H->state = HS_FOOT; H->rootMode = 0; H->st = 0.f; H->frame = H->h; H->overTop = 1;
  memcpy(H->snap, H->xf, sizeof(SkinXf) * m->h->nJoints);
  memcpy(H->snap[root].q, m->j[root].q, 16); memcpy(H->snap[root].t, m->j[root].t, 12);
  H->clip = -1; playClip(H, H->cAir, 1, .15f, 0.f);
  H->camInit = 0;
  setDoor(H, 0.f);
  pushEv(H, HOMER_EV_ON_FOOT, H->x, H->y, H->z, 0, sinf(H->h), cosf(H->h));
  if (H->cb.done) H->cb.done(H->cb.user, 0);
}

// посадка, когда дверь водителя открыть некуда (Роб 28.09: «посадка невозможна — прижата, дверь некуда открыть» — ирисом,
// как в оригинале): только при H->irisOn — вызывающий закрывает кадр ирисом (viewer/transit.c), Гомер сразу на сиденье.
// Без ириса (боты, host-тесты) — прежнее поведение: бежит к двери, через 4 с садится как есть
static void seatIris(Homer *H, int *count) {
  homerSetInCar(H, &H->car); setDoor(H, 0.f); (*count)++;
  pushEv(H, HOMER_EV_IN_CAR, H->x, H->y, H->z, 0, 0, 0);
  if (H->cb.done) H->cb.done(H->cb.user, 1);
}
static void seatUnderIris(Homer *H) { seatIris(H, &H->irisSeat); }

// Правка Роба 333 (05.10: «купил у Барни Plow King — Гомер садится с анимацией, как в низкую машину; в оригинале, видимо, круг к
// центру и от центра»). У оригинала признак — в данных машины: SetIrisTransition(1) в scripts/cars/<машина>.con (36 машин из 89:
// plowk_v, schoolbu, rocke_v, грузовики, машины трафика sedana/compacta/…; famil_v и машины персонажей — без него). Запись PS2
// yt-bziOipI4gDI (картинка 4:3): Plow King — △ у кузова, круг к центру с 869,25 с, темно 869,52–869,68, раскрытие до 870,00 —
// Гомер уже за рулём; выход 1085,20–1086,00 — Гомер стоит у двери водителя; ракетная 1978,45 и угон седана трафика 2061,23 —
// так же: круг начинается, когда Гомер у борта, дверь не открывается; famil_v (флага нет) — клипы посадки, 111 с. Здесь: такая машина —
// посадка и выход под ирисом (transitHomerWhy: car_in / car_out, темп ириса — данных, transit.c), клипы двери не играют.
// Без ириса (боты, host-тесты, нет iris.shr) и при homerIrisCarOld (flag caririsold, красный контроль) — прежнее
int homerIrisCarOld = HOMER_IRIS_CAR_OLD;
// №667 (Роб 16.10: «нет анимации посадки или не может открыться дверь — просто ирис»): у машин без шарнира двери (угнанные у трафика, машины
// миссий, дверь сорвана) клип двери открывал бы воздух — посадка и выход ирисом, как у SetIrisTransition. Спрашиваем хозяина кадра
// (homerDoorCan: шарнир водительской двери цел у машины, что рисуется). Клипы посадки есть у всех героев (homerInit: need). ENTER_IRIS_OLD=1 — прежнее
#ifndef ENTER_IRIS_OLD
#define ENTER_IRIS_OLD 0
#endif
int (*homerDoorCan)(void);
static int irisCar(const Homer *H) {
  if (!H->irisOn || homerIrisCarOld) return 0;
  return H->car.iris || (!ENTER_IRIS_OLD && !H->car.husk && homerDoorCan && !homerDoorCan());
}
static void carExitUnderIris(Homer *H, const ColWorld *w) {   // выход: сразу стоит там, где кончается клип выхода (finishExit)
  setDoor(H, 0.f); H->irisCarOut++;
  finishExit(H, w);
}

static int ejectNow(Homer *H, const ColWorld *w, float fromX, float fromZ, float power) {
  const SkinModel *m = H->m; int root = (int)m->h->rootJoint;
  const HomerCar *c = &H->car;
  setAnchor(H, 0.f);
  setDoor(H, 0.f);
  float p[3]; xform(c->m, c->door[0], -c->lift, c->door[1], p);   // у двери водителя, на земле (0.37 м от борта)
  float dx = p[0] - fromX, dz = p[2] - fromZ, l = sqrtf(dx * dx + dz * dz);
  if (l < 1e-3f) { dx = -cosf(c->h); dz = sinf(c->h); l = 1.f; }   // влево от машины
  dx /= l; dz /= l;
  int top = exitBlocked(H, w);                                // у двери стена/предмет: с крыши в другую сторону
  if (top) { H->x = c->x; H->z = c->z; H->y = c->roof; dx = -dx; dz = -dz; }
  else { H->x = p[0]; H->z = p[2]; H->y = p[1] + HOMER_EJECT_Y; }
  if (power <= 0.f) power = 1.f;
#ifdef HOMER_EJECT_V1                                         // прежний выброс (до правки 147): спиной вперёд, корпус вертикально
  H->vx = dx * 4.f * power; H->vz = dz * 4.f * power; H->vy = 5.f * power; H->speed = 0.f;   // как homerHurt, сильнее
  H->h = atan2f(-dx, -dz);                                   // лицом к взрыву, летит спиной
  H->ejPh = 0;
#else                                                         // правка 147: головой вперёд, низко и быстро (запись PS2 33:34.30)
  H->vx = dx * HOMER_EJECT_VH * power; H->vz = dz * HOMER_EJECT_VH * power; H->vy = HOMER_EJECT_VY * power; H->speed = 0.f;
  H->h = atan2f(dx, dz);                                     // курс по полёту
  H->ejPh = 1;
#endif
  H->pitch = 0.f;
  H->grounded = 0; H->jumps = 2; H->airFrom = H->y; H->groundY = H->y; H->launchV = sqrtf(H->vx * H->vx + H->vz * H->vz);
  H->state = HS_HURT; H->st = 0.f; H->phase = 0; H->rootMode = 0; H->frame = H->h; H->overTop = top;
  memcpy(H->snap, H->xf, sizeof(SkinXf) * m->h->nJoints);    // из позы «сидит» — в флейл (как exitOverTop)
  memcpy(H->snap[root].q, m->j[root].q, 16); memcpy(H->snap[root].t, m->j[root].t, 12);
  H->clip = -1; playClip(H, H->cFlail, 0, .05f, 0.f);
  H->camYaw = homerCamHeroOld ? c->h : atan2f(H->vx, H->vz);   // правка 361: камера по курсу полёта Гомера (было — правка 73: по курсу машины)
  H->camY = H->y; H->camInit = 1; H->camPullD = 0.f; H->camRise = 0.f; H->ejected++;
  pushEv(H, HOMER_EV_ON_FOOT, H->x, H->y, H->z, 0, sinf(H->h), cosf(H->h));
  pushEv(H, HOMER_EV_EJECT, H->x, H->y, H->z, power, dx, dz);
  if (H->cb.done) H->cb.done(H->cb.user, 0);
  return 1;
}

int homerEject(Homer *H, const ColWorld *w, const HomerCar *car, float fromX, float fromZ, float power) {
  if (H->state == HS_TO_DOOR) { H->state = HS_FOOT; H->st = 0.f; playLoco(H, .1f); return 0; }   // шёл к двери — остановился
  if (H->state != HS_IN_CAR && H->state != HS_GET_IN && H->state != HS_GET_OUT) return 0;
  if (H->ejWait > 0.f) return 0;                              // уже взведён (game.c зовёт каждый кадр, пока остов)
  if (car && car->ok) H->car = *car;
#ifdef HOMER_EJECT_V1
  return ejectNow(H, w, fromX, fromZ, power);
#else
  H->ejWait = HOMER_EJECT_DELAY; H->ejFromX = fromX; H->ejFromZ = fromZ; H->ejPow = power;   // правка 147: ещё в горящем остове
  return 1;
#endif
}

// ------------------------------------------------------------------ кадр
// №709 (Роб, видео TEST29S: «когда машина тебя сбивает, ты должен упасть на землю и встать, как NPC; анимации нет»): быстрая машина (быстрее HOMER_CARHIT_V,
// движется на Гомера) касается его пешком — он отлетает по ходу машины (flail), падает и встаёт (get_up), как прохожий трафика (traffic.c trafficCarHitsBody:
// те же скорости 3 м/с и подъём 3,5 + 0,3 v, но ниже предел). Медленная машина — как прежде отталкивает. События HURT нет (монеты не летят, шкала не трогается).
// HOMER_CARHIT_OLD=1 — прежнее (только толкает)
#ifndef HOMER_CARHIT_OLD
#define HOMER_CARHIT_OLD 0
#endif
#define HOMER_CARHIT_V 3.f      // м/с: как TR_PED_HIT_V прохожих
#define HOMER_CARHIT_COOL 1.5f  // с: пауза до следующего удара (пока встаёт)
static int carHitsHomer(Homer *H, const HomerCar *c) {
  if (c->dead || fabsf(c->v) <= HOMER_CARHIT_V || fabsf(c->y - H->y) > 1.8f) return 0;
  const CarBox *b = &c->box; float dx = H->x - b->x, dz = H->z - b->z, a = dx * b->fx + dz * b->fz, s = -dx * b->fz + dz * b->fx, r = HOMER_RADIUS;
  if (fabsf(a) >= b->hl + r || fabsf(s) >= b->hw + r) return 0;
  float vx = b->fx * c->v, vz = b->fz * c->v, sp = fabsf(c->v);
  if (vx * dx + vz * dz <= 0.f) return 0;                     // уезжает от Гомера (он стоит позади бампера): не сбивает
  // отлёт: по ходу машины, добавка наружу от кузова
  float ox = fabsf(a) - b->hl > fabsf(s) - b->hw ? (a < 0 ? -b->fx : b->fx) : (s < 0 ? b->fz : -b->fz), oz = fabsf(a) - b->hl > fabsf(s) - b->hw ? (a < 0 ? -b->fz : b->fz) : (s < 0 ? -b->fx : b->fx);
  H->vx = vx * .6f + ox * 2.f; H->vz = vz * .6f + oz * 2.f; H->vy = fminf(2.5f + sp * .25f, 6.f);
  H->grounded = 0; H->airFrom = H->y; H->launchV = sqrtf(H->vx * H->vx + H->vz * H->vz);
  float fl = sqrtf(H->vx * H->vx + H->vz * H->vz); if (fl > 1e-3f) H->h = atan2f(-H->vx / fl, -H->vz / fl);   // лицом к машине, летит спиной
  H->state = HS_HURT; H->st = 0.f; H->phase = 0; H->ejPh = 0; H->pitch = 0.f; H->speed = 0.f;
  playClip(H, H->cFlail, 0, .05f, 0.f); H->carHits++;
  return 1;
}
static void homerUpdateBody(Homer *H, const ColWorld *w, const HomerCar *car, const HomerInput *in, float dt);
void homerUpdate(Homer *H, const ColWorld *w, const HomerCar *car, const HomerInput *in, float dt) {
  static unsigned gen; if (++gen == 0) gen = 1;
  homerSolidFrame = gen; homerUpdateBody(H, w, car, in, dt); homerSolidFrame = 0;
}
static void homerUpdateBody(Homer *H, const ColWorld *w, const HomerCar *car, const HomerInput *in, float dt) {
  if (dt <= 0.f) return;
  if (dt > .1f) dt = .1f;
  if (car && car->ok) {
    if (H->state == HS_TO_DOOR || H->state == HS_GET_IN || H->state == HS_IN_CAR || H->state == HS_GET_OUT) H->car = *car;
  }
  if (H->ejWait > 0.f && (H->ejWait -= dt) <= 0.f) {         // правка 147: задержка в остове вышла — выброс
    H->ejWait = 0.f;
    if (H->state == HS_IN_CAR || H->state == HS_GET_IN || H->state == HS_GET_OUT) ejectNow(H, w, H->ejFromX, H->ejFromZ, H->ejPow);
  }
  const HomerCar *solid = (car && car->ok && homerOnFoot(H)) ? car : NULL;
  if (car && car->ok) H->camCar = *car; else H->camCar.ok = 0;   // для камеры пешком
  H->st += dt; H->steerGap += dt;
  int wantsMove = in && (in->mx * in->mx + in->my * in->my) > .01f;

  if (H->carHitCool > 0.f) H->carHitCool -= dt;
  if (!HOMER_CARHIT_OLD && solid && H->state != HS_HURT && H->carHitCool <= 0.f && carHitsHomer(H, solid)) H->carHitCool = HOMER_CARHIT_COOL;   // №709: сбит машиной — падает и встаёт

  switch (H->state) {
  case HS_FOOT: {
    // кнопки
    if (in && in->jump) {
      if (H->grounded) doJump(H, 0);
      else if (H->jumps == 1 && H->vy <= HOMER_JUMP2_UP && H->vy >= -HOMER_JUMP2_DOWN) doJump(H, 1);
    }
    int doStomp = in && in->stomp && !H->grounded;             // только прежняя раскладка (отрицательный контроль); игрок — X X □
    if (in && in->attack) {
      if (H->grounded) {                                      // пинок на земле: hom_break ("break")
        H->state = HS_KICK; H->st = 0.f; H->hitSent = 0; playClip(H, H->cBreak, 0, .08f, 0.f);   // cho break_quick blendIn .08
        break;
      } else if (H->jumps >= 2) doStomp = 1;                   // второй прыжок + пинок = стомп (оригинал)
      else if (H->clip != H->cJKick) { H->hitSent = 0; playClip(H, H->cJKick, 0, .05f, 0.f); }
    }
    if (doStomp) {
      H->state = HS_STOMP; H->st = 0.f; H->phase = 0; H->hitSent = 0;
      playClip(H, H->cStA, 0, .05f, 0.f);
      break;
    }
    steer(H, in, dt, 1);
    moveBody(H, w, solid, dt, HOMER_GRAVITY);
    if (H->grounded) tryBounce(H, w);                         // подкидывающая точка под ногами (JMP1)
    // анимация
    if (H->clip == H->cJKick && !H->hitSent && H->clipT >= 5.f / 30.f) kickHit(H, .7f, .4f);   // удар ногой: кадры 4–11
    if (H->grounded == 3) {                                   // приземлился
      H->grounded = 1;
      if (H->speed < 1.f && !wantsMove) playClip(H, H->cLand, 0, .01f, 0.f);   // cho jump_idle_land blendIn .01
      else playLoco(H, .1f);
    }
    if (!H->grounded) {
      if (H->clip == H->cTake && clipDone(H)) playClip(H, H->cAir, 0, .05f, 0.f);
      else if (H->clip < 0 || H->clip == H->cLand || (H->clip != H->cTake && H->clip != H->cAir && H->clip != H->cDash2 && H->clip != H->cJKick))
        playClip(H, H->cAir, 0, .1f, 0.f);                    // сошёл с края
      else if (H->clip == H->cJKick && clipDone(H)) playClip(H, H->cAir, 0, .1f, 0.f);
      H->idleT = 0.f;
    } else {
      int longIdle = H->clip >= 0 && (H->clip == H->cIdle[0] || H->clip == H->cIdle[1] || H->clip == H->cIdle[2] || H->clip == H->cTalk);
      if (wantsMove || H->speed > .05f) H->idleT = 0.f; else H->idleT += dt;
      if (longIdle) { if (wantsMove || clipDone(H)) { if (H->clip == H->cTalk) H->cTalk = -1; playLoco(H, .2f); H->idleT = 0.f; } }
      else if (H->clip == H->cLand && !clipDone(H) && !wantsMove) {}
      else if (H->idleT > 8.f && H->cIdle[H->idleNext % 3] >= 0) {   // долго стоит: почесаться / ботинки / чили (cho homer_idle0..2)
        playClip(H, H->cIdle[H->idleNext % 3], 0, .2f, 0.f); H->idleNext++;
      }
      else if (H->clip >= 0) playLoco(H, .1f);
    }
    break; }

  case HS_KICK: {
    steer(H, NULL, dt, 0);                                    // тормозит, не поворачивает
    moveBody(H, w, solid, dt, HOMER_GRAVITY);
    if (H->grounded == 3) H->grounded = 1;
    if (!H->hitSent && H->clipT >= 8.f / 30.f) kickHit(H, .85f, .45f);   // носок впереди всего: кадры 7–10 (0.8 м, 0.45 м)
    int canCancel = H->clipT > .45f && (wantsMove || (in && in->jump));
    if (clipDone(H) || canCancel) { H->state = HS_FOOT; H->st = 0.f; playLoco(H, .1f); }
    else if (in && in->attack && H->clipT > .4f) { H->st = 0.f; H->hitSent = 0; playClip(H, H->cBreak, 0, .08f, 0.f); H->clipT = 0.f; }
    break; }

  case HS_STOMP: {
    if (H->phase == 0) {                                      // замах: зависает в воздухе
      H->vx = H->vz = 0.f; H->vy = 0.f; H->speed = 0.f;
      if (H->clipT >= clipLen(H, H->cStA)) { H->phase = 1; H->vy = -4.f; playClip(H, H->cStAir, 0, .05f, 0.f); }
    } else if (H->phase == 1) {                               // падение ×3.22
      H->vx = H->vz = 0.f;
      moveBody(H, w, solid, dt, HOMER_GRAVITY * HOMER_STOMP_G);
      if (H->grounded) {
        H->grounded = 1; H->phase = 2;
        pushEv(H, HOMER_EV_STOMP, H->x, H->y + .3f, H->z, HOMER_STOMP_R, sinf(H->h), cosf(H->h));
        playClip(H, H->cStLand, 0, .03f, 8.f / 30.f);         // ступни касаются земли на кадре 9
      }
    } else {
      moveBody(H, w, solid, dt, HOMER_GRAVITY);
      if (H->grounded == 3) H->grounded = 1;
      if (clipDone(H)) { H->state = HS_FOOT; H->st = 0.f; playLoco(H, .1f); }
    }
    break; }

  case HS_HURT: {
    if (H->ejPh) {                                            // правка 147: выброс взрывом (фазы — см. HOMER_EJECT_* в homer.h)
      if (H->ejPh == 2) {                                     // касание: скольжение гаснет
        float v = sqrtf(H->vx * H->vx + H->vz * H->vz), nv = fmaxf(v - HOMER_EJECT_FRIC * dt, 0.f);
        if (v > 1e-4f) { H->vx *= nv / v; H->vz *= nv / v; }
      } else if (H->ejPh == 3) H->vx = H->vz = 0.f;
      moveBody(H, w, solid, dt, HOMER_GRAVITY);
      if (H->grounded == 3) H->grounded = 1;
      if (H->ejPh == 1) {                                     // полёт: флейл, корпус до горизонтали
        H->pitch = .5f * PI_F * smooth01(H->st / HOMER_EJECT_TILT);
        if (H->grounded) { H->ejPh = 2; H->st = 0.f; }
      } else if (H->ejPh == 2) {                              // грудью в землю, ноги вверх и обратно
        float u = clampf(H->st / HOMER_EJECT_SLIDE, 0.f, 1.f);   // ложится грудью (носки не упираются), как кадр 0 get_up
        H->pitch = .5f * PI_F + HOMER_EJECT_REST * u + HOMER_EJECT_ROCK * sinf(PI_F * u);
        if (H->st >= HOMER_EJECT_SLIDE) {                     // лежит: get_up с кадра 0 (сам ничком), наклон уходит вместе с переходом
          H->ejPh = 3; H->st = 0.f; H->vx = H->vz = 0.f; H->pitch = .5f * PI_F + HOMER_EJECT_REST; playClip(H, H->cGetUp, 0, .1f, 0.f);
        }
      } else {
        H->pitch = (.5f * PI_F + HOMER_EJECT_REST) * (1.f - smooth01(H->fadeLen > 0.f ? H->fade / H->fadeLen : 1.f));
        if (clipDone(H)) { H->state = HS_FOOT; H->st = 0.f; H->ejPh = 0; H->pitch = 0.f; playLoco(H, .15f); }
      }
      break;
    }
    moveBody(H, w, solid, dt, HOMER_GRAVITY);
    if (H->phase == 0) {
      if (H->grounded) { H->grounded = 1; H->phase = 1; H->vx = H->vz = 0.f; playClip(H, H->cGetUp, 0, .1f, 0.f); }
    } else {
      if (H->grounded == 3) H->grounded = 1;
      H->vx = H->vz = 0.f;
      if (clipDone(H)) { H->state = HS_FOOT; H->st = 0.f; playLoco(H, .15f); }
    }
    break; }

  case HS_TO_DOOR: {                                          // сам бежит к двери водителя, обходя машину
    if (H->car.husk) {                                       // правка 355: остов — без двери и ириса, ~0,7 с у кузова
      H->speed = 0.f; if (H->st >= HOMER_HUSK_SEAT) seatIris(H, &H->huskIn);
      break;
    }
    if (irisCar(H)) { seatIris(H, &H->irisCarIn); break; }   // правка 333: SetIrisTransition — сразу ирисом, там где стоит
    if (H->irisOn && H->st <= dt * 1.5f && exitBlocked(H, w)) { seatUnderIris(H); break; }   // дверь некуда открыть: ирисом
    const CarBox *b = &H->car.box; setAnchor(H, 0.f);
    float p[3]; xform(H->car.m, H->anchor[0], H->anchor[1], H->anchor[2], p);
    float dx = H->x - b->x, dz = H->z - b->z, rx = -b->fz, rz = b->fx;
    float a = dx * b->fx + dz * b->fz, s = dx * rx + dz * rz;
    float tx = p[0], tz = p[2];
    if (s > -b->hw - .2f) {                                   // не со стороны водителя: сначала за угол машины
      float sa = a >= 0.f ? 1.f : -1.f, wa = sa * (b->hl + .8f), ws = fabsf(a) < b->hl + .5f ? s : -(b->hw + .7f);
      tx = b->x + b->fx * wa + rx * ws; tz = b->z + b->fz * wa + rz * ws;
    }
    // Правка Роба 212 (03.10: «Гомер обходит к двери очень долго — если далеко, садиться с другой стороны или через ирис-кружок»):
    // путь к двери водителя (с обходом угла) длиннее HOMER_IRIS_PATH — сразу на сиденье под ирисом (тот же переход, что при
    // занятой двери); рядом с дверью — как было, дверь и посадка анимацией оригинала. HOMER_IRIS_PATH = 0 (-D) — прежнее
    if (H->irisOn && H->st <= dt * 1.5f && HOMER_IRIS_PATH > 0.f) {
      float path = hypotf(tx - H->x, tz - H->z) + hypotf(p[0] - tx, p[2] - tz);
      if (path > HOMER_IRIS_PATH) { seatUnderIris(H); break; }
    }
#if !HOMER_DOOR_STALL_OLD
    // Правка Роба 285 (04.10: «подхожу к школьному автобусу, пытаюсь сесть — он какое-то время тупит, не понимает, что у него нет
    // анимации входа; надо сразу ирис»): точка у двери не дальше 0,25 м от борта (у автобуса и грузовиков она у самой стенки —
    // дойти и открыть дверь нечем) — сразу на сиденье под ирисом; и если Гомер 0,6 с не приблизился к цели на 0,15 м (упёрся) —
    // тоже ирис, а не 4 с на месте. Правило порта. -DHOMER_DOOR_STALL_OLD=1 — прежнее (красный tests/homer/door_stall)
    if (H->irisOn && H->st <= dt * 1.5f) {
      float ax = p[0] - b->x, az = p[2] - b->z, da = fabsf(ax * b->fx + az * b->fz) - b->hl, ds = fabsf(ax * rx + az * rz) - b->hw;
      if (fmaxf(da, ds) < .25f) { seatUnderIris(H); break; }
      H->stallD = 1e9f; H->stallT = 0.f;
    }
    if (H->irisOn) {
      float dn = hypotf(tx - H->x, tz - H->z) + hypotf(p[0] - tx, p[2] - tz);
      if (dn < H->stallD - .15f) { H->stallD = dn; H->stallT = 0.f; }
      else if ((H->stallT += dt) > .6f) { seatUnderIris(H); break; }
    }
#endif
    float ex = tx - H->x, ez = tz - H->z, d = sqrtf(ex * ex + ez * ez);
    HomerInput ai; memset(&ai, 0, sizeof ai); ai.dash = 1;   // к двери бегом (правило порта, как было до раскладки ○)
    float hA = anchorHeading(H);
    int atDoor = (tx == p[0] && tz == p[2]) && d < .15f;
    if (!atDoor && d > 1e-3f) {                               // «стик» к цели: скорость по расстоянию (бег, у двери тише)
      int wp = !(tx == p[0] && tz == p[2]);                  // к углу — бегом, к двери — с торможением (успеть встать)
      float mag = wp ? 1.f : clampf(sqrtf(2.f * HOMER_DECEL * fmaxf(d - .05f, 0.f)) / HOMER_RUN, .12f, 1.f);
      ai.camYaw = atan2f(ex, ez); ai.my = mag; H->frame = ai.camYaw; H->heldX = 0.f; H->heldY = mag;
      steer(H, &ai, dt, 1);
    } else steer(H, NULL, dt, 0);
    moveBody(H, w, &H->car, dt, HOMER_GRAVITY);               // сквозь машину не ходит (точка у двери — в 0.37 м от борта)
    if (H->grounded == 3) H->grounded = 1;
    if (atDoor) {
      float dh = wrapPi(hA - H->h), mt = HOMER_TURN * dt;
      H->h = wrapPi(H->h + clampf(dh, -mt, mt));
      if (atDoor && fabsf(dh) < .25f) { startSeq(H, HS_GET_IN, H->cOpenIn); break; }
    }
    if (H->st > 4.f) { if (H->irisOn) seatUnderIris(H); else startSeq(H, HS_GET_IN, H->cOpenIn); break; }   // застрял — под ирисом на сиденье (без ириса — садимся как есть)
    break; }

  case HS_GET_IN: {
    float t = H->clipT;
    if (H->phase == 0) {                                      // открыть дверь: рука у ручки на кадре 5, открыта к кадру 10
      setDoor(H, smooth01((t - 5.f / 30.f) / (5.f / 30.f)));
      if (clipDone(H)) { H->phase = 1; playClip(H, H->cIn, 0, .1f, 0.f); }
    } else if (H->phase == 1) {
      setDoor(H, 1.f);
      if (clipDone(H)) { H->phase = 2; playClip(H, H->cCloseIn, 0, .1f, 0.f); }   // cho: close_door_driver blendIn .1
    } else {
      setDoor(H, 1.f - smooth01((t - .03f) / (clipLen(H, H->cCloseIn) - .08f)));
      if (clipDone(H)) {
        setDoor(H, 0.f);
        H->state = HS_IN_CAR; playClip(H, H->cInCar, 1, .1f, 0.f);
        pushEv(H, HOMER_EV_IN_CAR, H->x, H->y, H->z, 0, 0, 0);
        if (H->cb.done) H->cb.done(H->cb.user, 1);
      }
    }
    setAnchor(H, H->phase == 0 ? 0.f : H->phase == 1 ? smooth01(H->clipT / fmaxf(clipLen(H, H->cIn), 1e-3f)) : 1.f);   // якорь доезжает от двери к сиденью за клип посадки
    { float F[16]; anchorFrame(H, F); float r[3]; skinRootMotion(H->m, H->clip, H->clipT, r);
      float pp[3]; xform(F, r[0], 0.f, r[2], pp); H->x = pp[0]; H->y = pp[1]; H->z = pp[2]; }
    break; }

  case HS_IN_CAR: {
    setAnchor(H, 1.f); driverGestures(H);
    float F[16]; anchorFrame(H, F); float pp[3]; xform(F, H->seatRoot[0], 0.f, H->seatRoot[2], pp);
    H->x = pp[0]; H->y = pp[1]; H->z = pp[2]; H->h = anchorHeading(H); H->speed = 0.f;
    break; }

  case HS_GET_OUT: {
    float t = H->clipT;
    if (H->phase == 0 && H->st <= dt * 1.5f && exitBlocked(H, w)) { exitOverTop(H); break; }   // первый кадр выхода   // у двери занято: через верх
    if (H->phase == 0 && H->st <= dt * 1.5f && H->car.husk) { setDoor(H, 0.f); H->huskOut++; finishExit(H, w); break; }   // правка 355: из остова — без двери и ириса (как посадка)
    if (H->phase == 0 && H->st <= dt * 1.5f && irisCar(H)) { carExitUnderIris(H, w); break; }   // правка 333: SetIrisTransition — выход ирисом
    if (H->phase == 0) {                                      // открыть дверь изнутри
      setDoor(H, smooth01((t - .05f) / .22f));
      if (clipDone(H)) { H->phase = 1; playClip(H, H->cOut, 0, .1f, 0.f); }
    } else if (H->phase == 1) {
      setDoor(H, 1.f);
      if (clipDone(H)) { H->phase = 2; playClip(H, H->cCloseOut, 0, .05f, 0.f); }
    } else {
      setDoor(H, 1.f - smooth01((t - .05f) / (clipLen(H, H->cCloseOut) - .1f)));
      if (clipDone(H)) { setDoor(H, 0.f); finishExit(H, w); }
    }
    if (H->state == HS_GET_OUT) {
      setAnchor(H, H->phase == 0 ? 1.f : H->phase == 1 ? 1.f - smooth01(H->clipT / fmaxf(clipLen(H, H->cOut), 1e-3f)) : 0.f);
      float F[16]; anchorFrame(H, F); float r[3]; skinRootMotion(H->m, H->clip, H->clipT, r);
      float pp[3]; xform(F, r[0], 0.f, r[2], pp); H->x = pp[0]; H->y = pp[1]; H->z = pp[2];
    }
    break; }
  }
  if (H->grounded == 3) H->grounded = 1;
  if (homerOnFoot(H) && H->grounded) {                        // надёжное место (для выпадения из мира)
    float gs_;   // только опора мира: крыша машины или ящик над пустотой — не надёжное место (HOMER_VOID_RECOVER)
    if (!HOMER_VOID_RECOVER || !w || colGround(w, H->x, H->z, H->y + .3f, H->y - .6f, &gs_, 0)) { H->safe[0] = H->x; H->safe[1] = H->y; H->safe[2] = H->z; }
  }
  animTick(H, dt);
}

void homerCoinPoint(const Homer *H, float p[3]) {
  if (H->state == HS_IN_CAR && H->car.ok) { p[0] = H->car.x; p[1] = H->car.y + .8f; p[2] = H->car.z; return; }
  p[0] = H->x; p[1] = H->y + .8f; p[2] = H->z;
}

// ------------------------------------------------------------------ камера пешком
// В данных диска параметров камеры пешком нет (FollowCameraData 0x3000100 — только у машин); расстояние и высоты — по камере
// оригинала в PS2 (близнец 02.10, homer.h HOMER_CAM_DIST / _EYE_H / _LOOK_H): 6,45 м за спиной, глаз 2,38 м, взгляд в 1,0 м над
// ступнями; курс догоняет курс Гомера, пока он идёт; высота сглажена (прыжок не трясёт кадр); если сзади стена — камера
// подъезжает ближе (правило порта).
// Машина рядом (правило порта, Роб 28.09): глаз не заходит ближе HOMER_CAR_CLEAR к кузову — у самого кузова поднимается над
// крышей; ближняя плоскость (camNear) — по зазору до кузова, стены и Гомера, чтобы ни кузов, ни Гомер не резались (при 1.5 м
// у двери было видно салон насквозь).
static float carClear(const HomerCar *c, float x, float y, float z) {   // расстояние от точки до кузова (коробка carBox)
  const CarBox *b = &c->box; float dx = x - b->x, dz = z - b->z;
  float u = fabsf(dx * b->fx + dz * b->fz) - b->hl, v = fabsf(-dx * b->fz + dz * b->fx) - b->hw;
  float top = b->y + b->hi, bot = b->y + b->lo, dy = y > top ? y - top : (y < bot ? bot - y : 0.f);
  u = fmaxf(u, 0.f); v = fmaxf(v, 0.f); return sqrtf(u * u + v * v + dy * dy);
}
#ifndef CAM_WALL_PS2_OLD
#define CAM_WALL_PS2_OLD 0
#endif
int homerCamPs2Old = CAM_WALL_PS2_OLD;
// №916в (рывок глаза пешком, L6 x 73 z −22: перескок камеры у стены по записи PS2 шёл в два кадра, глаз прыгал дважды, 3,5 и 5,3 м; на PS2 перескок один): курс глаза в кадре перескока уже новый,
// камера безопасного глаза (camSafeEye) по счётчику homerCamFlipN режет перелёт и идёт к желаемому глазу сразу. CAM_FLIP1_OLD=1 / flag camflip1old 1 — как было (красный контроль)
#ifndef CAM_FLIP1_OLD
#define CAM_FLIP1_OLD 0
#endif
int homerCamFlipOld = CAM_FLIP1_OLD, homerCamFlipN;
#ifndef HOMER_FLIP_STAND_OLD
#define HOMER_FLIP_STAND_OLD 0
#endif
int homerFlipStandOld = HOMER_FLIP_STAND_OLD;
#ifndef HOMER_CTRL_FREEZE_OLD
#define HOMER_CTRL_FREEZE_OLD 1   // по умолчанию ВЫКЛ: на выходе из машины и у будки красного/зелёного контроля нет (курс и так не меняется), эффект не доказан; включать flag ctrlfreezeold 0
#endif
#ifndef HOMER_EXITYAW_OLD
#define HOMER_EXITYAW_OLD 0
#endif
#ifndef CTRL916_OLD
#define CTRL916_OLD 1   // №947б (Роб 25.10): пешком управление как до №916/№920 — стик относительно ТЕКУЩЕГО вида, разворот назад бежит назад. 0 / flag ctrl947old 1 — кадр держится, пока стик не отпущен (№916/№920)
#endif
int homerCtrl916Old = CTRL916_OLD;   // №916; с №947б 1 по умолчанию
#ifndef HOMER_FLIPBUSY_OLD
#define HOMER_FLIPBUSY_OLD 0
#endif
// №982 (находка регрессии финала, eyejump WK_L7: Гомер идёт по L7 между двумя машинами трафика, «разворот через героя» 10,23 м за кадр и 29 кадров «глаз в кузове»): луч камеры к глазу,
// задержанный кузовом машины (homerCamRayExtra), считался упором в стену: при d < 1 м камера перескакивала на другую сторону Гомера (рывок через героя) или стояла в метре от него внутри кузова.
// Перескок по записи PS2 — для стен мира, не для машин. Теперь перескок только от стены мира, а у машины камера остаётся в 1 м и поднимается над крышей (до +1,8 м). HOMER_CAMCAR982_OLD=1 / flag camcar982old 1 — как было
#ifndef HOMER_CAMCAR982_OLD
#define HOMER_CAMCAR982_OLD 0
#endif
int homerCamCarOld = HOMER_CAMCAR982_OLD; float homerCamCarTop = -1e9f;
#ifndef WALKCAM1059_OLD
#define WALKCAM1059_OLD 0   // №1059 (вариант А, слово Роба 27.10): камера пешком по закону PS2 + кадр стика за видом; 1 / файл walkcam1059old / flag walkcam1059old 1 — как было в TEST74
#endif
#ifndef WALKCAM1059_TAU
#define WALKCAM1059_TAU .2f   // с
#endif
#ifndef WALKCAM1059_RATE
#define WALKCAM1059_RATE 5.2f   // рад/с (300°/с): догоняющая скорость кадра управления за видом (камера пешком по закону PS2 не быстрее 36°/с, запас — после облёта игры)
#endif
int homerWalkCam1059Old = WALKCAM1059_OLD, homerViewOk; float homerWalkCamTau = WALKCAM1059_TAU, homerWalkCamRate = WALKCAM1059_RATE, homerViewYaw;   // №1059: homerViewYaw — вид на экране (от глаза к герою), ставит main_loop.h
int homerFlipHold, homerFlipBusyOld = HOMER_FLIPBUSY_OLD;   // №947б: 1 — перескок камеры у стены не замораживает курс управления
int homerExitYawOld = HOMER_EXITYAW_OLD; int homerCtrlFreezeOld = HOMER_CTRL_FREEZE_OLD; float homerCtrlYaw; int homerCtrlOn, homerCamBusy;
int homerCamWallOff;   // служебное (красный контроль tests/camera): камера пешком без обхода стен
float homerCamUserD = 1.f, homerCamUserH = 1.f;   // №996 (homer.h)
float homerCamDist = HOMER_CAM_DIST, homerCamEyeH = HOMER_CAM_EYE_H, homerCamLookH = HOMER_CAM_LOOK_H;   // красный контроль правки 167: прежние 4.2 / 1.9 / 1.2
int camOverGround(const ColWorld *w, float x, float z, float y) {   // под точкой (x, z) опора коллизии не ниже 40 м под глазом
  float gy; return colGround(w, x, z, y + .5f, y - 40.f, &gy, 0);
}
static void homerCameraAt(Homer *H, const ColWorld *w, float dt, float eye[3], float look[3]);
// Правка Роба 223 (03.10: «при посадке камера очень сильно следит и трясётся — сгладить»): на посадке (HS_GET_IN) тело Гомера
// идёт по движению корня клипа посадки (до 1 м рывками за 0,5 с), а камера была привязана к нему жёстко. У оригинала (близнец В23,
// build/twins/o4/v23_exit_cam): камера на посадке почти стоит и уходит к камере машины с постоянной ~3,8 с. Здесь точка, за которой
// идёт камера, на посадке догоняет Гомера с той же постоянной; пешком и у двери — как была (жёстко)
int homerCamHeroOld;   // правка 361 (homer.h)
float (*homerCamRayExtra)(const float a[3], const float b[3]);   // правка 284: лучи камеры пешком видят и машины (main.c)
void homerCamera(Homer *H, const ColWorld *w, float dt, float eye[3], float look[3]) {
  float hx = H->x, hz = H->z;
  if (!H->camInit || H->state != HS_GET_IN || HOMER_CAM_SEAT_K <= 0.f) { H->camFx = hx; H->camFz = hz; }
  else { float k = fminf(1.f, dt * HOMER_CAM_SEAT_K); H->camFx += (hx - H->camFx) * k; H->camFz += (hz - H->camFz) * k; }
  H->x = H->camFx; H->z = H->camFz;
  homerCameraAt(H, w, dt, eye, look);
  H->x = hx; H->z = hz;
}
static void homerCameraAt(Homer *H, const ColWorld *w, float dt, float eye[3], float look[3]) {
  float fx, fz, yb = H->y;
  if (!H->camInit) { H->camYaw = H->h; H->camY = H->y; H->camInit = 1; H->camPullD = 0.f; H->camRise = 0.f; }
  float camD = homerCamDist, camE = homerCamEyeH;   // правка 349 (В86): после переноса DEATH — поза WalkerCam оригинала, пока стоит
  if (H->camHoldD > 0.f) {
    if (!homerOnFoot(H) || H->speed > .5f || H->camTurn != 0.f) {   // пошёл / повернул камеру / сел — к обычной 2,5 м/с (правило порта)
      float k = camD > H->camHoldD ? fminf(1.f, 2.5f * dt / (camD - H->camHoldD)) : 1.f;
      H->camHoldD += (camD - H->camHoldD) * k; H->camHoldE += (camE - H->camHoldE) * k;
      if (k >= 1.f) H->camHoldD = H->camHoldE = 0.f;
    }
    if (H->camHoldD > 0.f) { camD = H->camHoldD; camE = H->camHoldE; }
  }
  camD *= homerCamUserD; camE = homerCamLookH + (camE - homerCamLookH) * homerCamUserH;   // №996: Distance и Height страницы CAMERA (высота — отступ глаза над точкой взгляда)
  if (H->speed > .5f && homerOnFoot(H)) {
    float d = wrapPi(H->h - H->camYaw), sf_ = clampf(H->speed / 4.f, .3f, 1.f);
    // №1059: PS2 (tools/twins/walkcam1059.py, ps2-walk-camera-stick): камера пешком держится на расстоянии r за героем, поэтому крутится с геометрической скоростью
    // ω = (v / r) · sin(курс − вид): ходьба r 6,3 м даёт 36°/с, бег (v 8, r 6,9) — 66°/с, первая миссия (r 4,3–4,7) — 49–53°/с; ход точно на камеру (180°) — не крутится
    if (!homerWalkCam1059Old) H->camYaw = wrapPi(H->camYaw + (H->speed / fmaxf(camD, 1.5f)) * sinf(d) * dt);
    else H->camYaw = wrapPi(H->camYaw + d * fminf(1.f, dt * 1.6f * sf_));
  }
  else if (!homerCamHeroOld && homerOnFoot(H) && (H->state == HS_HURT || !H->grounded)) {   // правка 361: выброс, падение, отлёт — камера
    float hv = sqrtf(H->vx * H->vx + H->vz * H->vz);                                        // догоняет курс полёта Гомера, как курс шага
    if (hv > 1.f) { float d = wrapPi(atan2f(H->vx, H->vz) - H->camYaw); H->camYaw = wrapPi(H->camYaw + d * fminf(1.f, dt * 1.6f * clampf(hv / 4.f, .3f, 1.f))); }
  }
  if (H->camTurn != 0.f && homerOnFoot(H)) H->camYaw = wrapPi(H->camYaw + H->camTurn * dt * 1.6f);   // правка 75: L/R — поворот ~90°/с
  if (!H->grounded && homerOnFoot(H)) yb = fminf(H->y, fmaxf(H->groundY, H->airFrom) + .6f);   // в прыжке почти не поднимаем
  H->camY += (yb - H->camY) * fminf(1.f, dt * 5.f);
  const HomerCar *c = H->camCar.ok && homerOnFoot(H) ? &H->camCar : NULL;
  // Машина рядом (правка 167, близнец 02.10 v23_exit_cam): у оригинала камера пешком кузов не обходит — линия взгляда идёт вдоль
  // борта или поверх крыши (после выхода глаз в 6,45 м прямо за Гомером, машина сбоку в кадре). От кузова уходит только сам глаз
  // (правило порта, Роб 28.09: ближняя плоскость не режет кузов): глаз в кузове или ближе HOMER_CAR_CLEAR — ближайший курс со
  // свободным глазом (шаг 15°, до 90°, поворот ~4 рад/с); нет такого — глаз ближе к Гомеру, у самого кузова над крышей.
  // Было: обход всего пути взгляда — камера у двери прижималась к Гомеру и вылетала вбок (кадр выхода e3)
  float ey0 = H->camY + camE;
  if (c && carClear(c, H->x - sinf(H->camYaw) * camD, ey0, H->z - cosf(H->camYaw) * camD) < HOMER_CAR_CLEAR) {
    float best = 0.f;
    for (int k = 1; k <= 12; k++) {
      float off = (float)((k + 1) / 2) * .2618f * ((k & 1) ? 1.f : -1.f), sx = sinf(H->camYaw + off), sz = cosf(H->camYaw + off);
      if (carClear(c, H->x - sx * camD, ey0, H->z - sz * camD) >= HOMER_CAR_CLEAR) { best = off; break; }
    }
    if (best != 0.f) H->camYaw = wrapPi(H->camYaw + best * fminf(1.f, dt * 4.f));
  }
  fx = sinf(H->camYaw); fz = cosf(H->camYaw);
  float dist = camD, clear = 1e9f, ey = ey0;
  if (c) {
    float ok = 0.f;                                           // самый дальний глаз (от полной дальности к Гомеру) вне кузова
    for (float dd = dist; dd >= 1.2f - 1e-3f; dd -= .1f)
      if (carClear(c, H->x - fx * dd, ey, H->z - fz * dd) >= HOMER_CAR_CLEAR) { ok = dd; break; }
    if (ok <= 0.f) {                                          // прижат к кузову спиной: над крышей
      dist = 1.2f; ey = fmaxf(ey, c->box.y + c->box.hi + HOMER_CAR_CLEAR);
    } else dist = ok;
    clear = fminf(clear, carClear(c, H->x - fx * dist, ey, H->z - fz * dist));
  }
  if (w && !homerCamWallOff) {
    // Правки Роба 40 и 27 (28.09: «камера не должна входить в стены и текстуры»): лучи от головы Гомера к глазу и к краям
    // ближней плоскости по коллизии мира/комнаты (colRayFrac — сплошной луч, тонкие стены не проскакивают). Упёрлась —
    // камера подъезжает по лучу до стены минус зазор; ближе HOMER_CAM_MIN не подъезжает — поднимается (до +1.8 м) над
    // препятствием. Стена появилась — сразу ближе, пропала — отпускает плавно (2.5 м/с). Правило порта (в данных у камеры
    // пешком параметров нет), сверить с эталоном PS2.
    float T[3] = {H->x, H->camY + 1.5f, H->z}, rise = 0.f, got = 0.f; int hits = 0;
    for (int k = 0; k < 4; k++) {
      float up = k * .6f, E[3] = {H->x - fx * dist, ey + up, H->z - fz * dist}, fr = 1.f; int hk = 0;
      float L = sqrtf((E[0] - T[0]) * (E[0] - T[0]) + (E[1] - T[1]) * (E[1] - T[1]) + (E[2] - T[2]) * (E[2] - T[2])), frW = 1.f, carTop = -1e9f;   // №982: frW — доля луча только по миру, carTop — верх кузова, задержавшего луч
      for (int r = 0; r < 5; r++) {   // центр и 4 края; правка Роба 160 (01.10): кадр 16:9 широкий — края ближней плоскости дальше,
        // у стены дома сбоку она просвечивала: вбок ±HOMER_CAM_SIDE, вверх/вниз ±HOMER_CAM_UPDN (было ±0.3 / ±0.2)
        float sx = r == 1 ? HOMER_CAM_SIDE : r == 2 ? -HOMER_CAM_SIDE : 0.f, sy = r == 3 ? HOMER_CAM_UPDN : r == 4 ? -HOMER_CAM_UPDN : 0.f;
        float Er[3] = {E[0] + fz * sx, E[1] + sy, E[2] - fx * sx}, f = colRayFrac(w, T, Er, 0);
        if (f < frW) frW = f;
        if (homerCamRayExtra) { homerCamCarTop = -1e9f; float g = homerCamRayExtra(T, Er); if (g < f) { f = g; if (homerCamCarTop > carTop) carTop = homerCamCarTop; } }   // правка 284: кузова машин рядом
        if (f < 1.f) hk++;
        if (f < fr) fr = f;
      }
      float d = fr >= 1.f ? dist : fmaxf(0.f, fr * L - HOMER_CAM_MARGIN) / fmaxf(L, 1e-3f) * dist;
      if (k == 0) hits = hk;
      if (!homerCamPs2Old) {   // CAM_WALL_PS2 (съёмка PS2 L1: камера упирается в стену, сама стоит, Гомер идёт на неё: 6,7 -> 1,0 м, наклон вниз растёт до 67°, стена героя не прячет;
        // ближе ~1 м — скачок камеры на другую сторону Гомера: 3,8-4,2 м, 12°, +0,9 м над целью, дальше обычная). Подъёма над препятствием нет — наклон растёт сам
        H->camFlipT += dt;
        // №678 (Роб: «выходишь, камера крутится, хочешь вперёд, а идёшь назад»): перескок на другую сторону Гомера по записи PS2 — когда Гомер ИДЁТ на стену; у стоящего
        // Гомера (только что вышел из машины у стены) камера разворачивалась на 180° сама, и «вперёд» стика уводил назад (tests/smoke_scenes/exitcar_delay.txt, EXW_1_0:
        // камера +14° -> +159° за 0,2 с). Перескок только при ходьбе (скорость > 0,5 м/с). HOMER_FLIP_STAND_OLD=1 / flag homerflipstandold 1 — как было
        if ((homerCamCarOld ? fr : frW) < 1.f && d < 1.0f && H->camFlipT > 2.f && (homerFlipStandOld || H->speed > .5f)) {   // упор: другая сторона свободна на 4 м — перескок (курс камеры +180°)
          float Eo[3] = {H->x + fx * 4.f, ey + .9f, H->z + fz * 4.f}; float fo = colRayFrac(w, T, Eo, 0);
          if (fo >= 1.f) { H->camYaw = wrapPi(H->camYaw + 3.14159265f); H->camPullD = 4.f; H->camRise = .9f; H->camFlipT = 0.f; d = 4.f; fr = 1.f;
            homerCamFlipN++; if (!homerCamFlipOld) { fx = -fx; fz = -fz; } }   // №916в: перескок целиком в этом кадре (раньше глаз шёл по старому курсу ещё кадр: два рывка 3,5 и 5,3 м вместо одного)
        }
        float riseCar = 0.f;
        if (!homerCamCarOld && d < 1.0f && fr < frW && carTop > -1e8f) riseCar = fminf(1.8f, fmaxf(0.f, carTop + .3f - ey));   // №982: упор в кузов, не в стену мира — над крышей, не в кузове
        if (d < 1.0f) d = 1.0f;
        hits = hk; rise = riseCar; got = d; break;
      }
      if (d >= HOMER_CAM_MIN || fr >= 1.f) { rise = up; got = d; break; }
      if (d > got) { got = d; rise = up; }
    }
    H->camHits = hits;
    if (!(H->camPullD > 0.f) || got < H->camPullD) H->camPullD = got; else H->camPullD = fminf(got, H->camPullD + 2.5f * dt);
    H->camRise += (rise - H->camRise) * fminf(1.f, dt * (rise > H->camRise ? 8.f : 2.f));
    float use = fminf(dist, H->camPullD), sk = dist > 1e-3f ? use / dist : 1.f;   // глаз — на луче от головы к желаемому
    float E[3] = {H->x - fx * dist, ey + H->camRise, H->z - fz * dist};
    // глаз — над опорой коллизии (в комнате у внешних стен коллизии часто нет: пол кончается — дальше пустота за стеной)
    if (!camOverGround(w, E[0] + (T[0] - E[0]) * (1.f - sk), E[2] + (T[2] - E[2]) * (1.f - sk), E[1])) {
      float lo = 0.f, hi = sk;                                 // самая дальняя точка пути, где под глазом есть опора
      for (int it = 0; it < 7; it++) { float m = .5f * (lo + hi); if (camOverGround(w, T[0] + (E[0] - T[0]) * m, T[2] + (E[2] - T[2]) * m, E[1])) lo = m; else hi = m; }
      float Lh = dist; sk = fmaxf(0.f, lo - HOMER_CAM_MARGIN / fmaxf(Lh, 1e-3f)); H->camPullD = fminf(H->camPullD, sk * dist); H->camHits++;
    }
    eye[0] = T[0] + (E[0] - T[0]) * sk; eye[1] = T[1] + (E[1] - T[1]) * sk; eye[2] = T[2] + (E[2] - T[2]) * sk;
    if (use < dist - 1e-3f) clear = fminf(clear, HOMER_CAM_MARGIN);
    dist = use; ey = eye[1];
  } else { eye[0] = H->x - fx * dist; eye[1] = ey; eye[2] = H->z - fz * dist; }
  float toHomer = sqrtf(dist * dist + (ey - H->camY - 1.2f) * (ey - H->camY - 1.2f)) - .45f;   // до спины/головы Гомера (1,2 м — его середина)
  H->camDist = dist;
  H->camNear = clampf(.8f * fminf(clear, toHomer), .25f, HOMER_CAM_NEAR);
  look[0] = H->x; look[1] = H->camY + homerCamLookH; look[2] = H->z;
}

void homerCamDeathPose(Homer *H) {   // правка 349 / ДОБИВКА 18 (homer.h): глаз сразу с лица Гомера, как WalkerCam PS2 на кадре 608
  H->camInit = 1; H->camYaw = wrapPi(H->h + 3.14159265f); H->camY = H->y; H->camPullD = 0.f; H->camRise = 0.f;
  H->camFx = H->x; H->camFz = H->z; H->camHoldD = HOMER_CAM_DEATH_DIST; H->camHoldE = HOMER_CAM_DEATH_EYE;
}

// ------------------------------------------------------------------ стейтпропы как твёрдые тела
// Объёмы SPR1 (коробка / сфера / цилиндр) — как у машины в stateprops.c (hitVolume): срез по высоте, след на xz —
// прямоугольник по самой горизонтальной оси коробки или круг; круг Гомера выталкивается. Кусты-шары (cls 2) — насквозь.
static void mulR(const float R[9], const float v[3], float o[3]) {
  o[0] = R[0] * v[0] + R[3] * v[1] + R[6] * v[2]; o[1] = R[1] * v[0] + R[4] * v[1] + R[7] * v[2]; o[2] = R[2] * v[0] + R[5] * v[1] + R[8] * v[2];
}
// Правка «плавность» (тур SHARTEST4: пешком у остова x_homer 1,9 → 6,1 мс): твёрдость для Гомера (homerSolidAll) на каждом из 4 подшагов
// кадра обходила ВСЕ стейтпропы и ВСЕ пропы с 3–5 корнями на экземпляр. Предметы за время одного homerUpdate не двигаются, поэтому
// список «кто в пределах HS_CACHE_M от начала шага» строится один раз за вызов (в индексном порядке — те же толчки в том же порядке),
// а подшаги идут по короткому списку; ушёл дальше HS_CACHE_M от центра списка — список строится заново. Радиус типа — в таблице.
// Вне homerUpdate (тесты, прямые вызовы) кеша нет: homerSolidFrame == 0. -DHOMER_SOLID_CACHE_OLD=1 — прежний обход
#ifndef HOMER_SOLID_CACHE_OLD
#define HOMER_SOLID_CACHE_OLD 0
#endif
#define HS_CACHE_M 6.f
#define HS_CACHE_MAX 1024
unsigned homerSolidFrame;   // != 0 только внутри homerUpdate; меняется на каждый вызов
static float *hsBrT; static const void *hsBrOwner; static unsigned hsBrN;
static float hsTypeBr(const StateProps *sp, unsigned ti) {   // sqrt(|h|²) + sqrt(c.x²+c.z²): не зависит от экземпляра
  if (hsBrOwner != sp || hsBrN != sp->ntype || !hsBrT) {
    free(hsBrT); hsBrT = NULL; hsBrT = (float *)malloc(sizeof(float) * (sp->ntype ? sp->ntype : 1));
    if (!hsBrT) { hsBrOwner = NULL; hsBrN = 0; const SpType *T = &sp->type[ti]; return sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) + sqrtf(T->c[0] * T->c[0] + T->c[2] * T->c[2]); }
    for (unsigned t = 0; t < sp->ntype; t++) { const SpType *T = &sp->type[t]; hsBrT[t] = sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) + sqrtf(T->c[0] * T->c[0] + T->c[2] * T->c[2]); }
    hsBrOwner = sp; hsBrN = sp->ntype; }
  return hsBrT[ti];
}
int homerSolidSprops(void *user, float *x, float *z, float y, float r, float *top, float *onx, float *onz) {
  const StateProps *sp = (const StateProps *)user; int pushed = 0;
  if (!sp) return 0;
  static unsigned cacheFrame, cacheN, cacheList[HS_CACHE_MAX]; static float cacheX, cacheZ, cacheR; static const void *cacheOwner; static int cacheOk;
  int useCache = !HOMER_SOLID_CACHE_OLD && homerSolidFrame != 0;
  if (useCache && (cacheFrame != homerSolidFrame || cacheOwner != sp || cacheR != r || !cacheOk ||
                   (*x - cacheX) * (*x - cacheX) + (*z - cacheZ) * (*z - cacheZ) > HS_CACHE_M * HS_CACHE_M)) {
    if (cacheFrame != homerSolidFrame) hsBrOwner = NULL;   // таблица радиусов типов — заново раз за вызов (уровень мог смениться на тот же адрес)
    cacheFrame = homerSolidFrame; cacheOwner = sp; cacheR = r; cacheX = *x; cacheZ = *z; cacheN = 0; cacheOk = 1;
    for (unsigned i = 0; i < sp->ninst; i++) {
      const SpRun *R = &sp->run[i]; const SpType *T = &sp->type[sp->inst[i].type];
      if (!R->alive || !R->colOn || !T->nCol || T->cls == 2) continue;
      float dx0 = *x - R->x, dz0 = *z - R->z, br = hsTypeBr(sp, sp->inst[i].type) + r + .5f + HS_CACHE_M;
      if (dx0 * dx0 + dz0 * dz0 > br * br) continue;
      if (cacheN >= HS_CACHE_MAX) { cacheOk = 0; break; }
      cacheList[cacheN++] = i; }
  }
  if (useCache && !cacheOk) useCache = 0;
  unsigned nIter = useCache ? cacheN : sp->ninst;
  for (unsigned q = 0; q < nIter; q++) {
    unsigned i = useCache ? cacheList[q] : q;
    const SpRun *R = &sp->run[i]; const SpType *T = &sp->type[sp->inst[i].type];
    if (!R->alive || !R->colOn || !T->nCol || T->cls == 2) continue;
    float dx0 = *x - R->x, dz0 = *z - R->z, br = (useCache ? hsTypeBr(sp, sp->inst[i].type) : sqrtf(T->h[0] * T->h[0] + T->h[1] * T->h[1] + T->h[2] * T->h[2]) +
              sqrtf(T->c[0] * T->c[0] + T->c[2] * T->c[2])) + r + .5f;
    if (dx0 * dx0 + dz0 * dz0 > br * br) continue;
    for (unsigned k = 0; k < T->nCol; k++) {
      const SpCol *C = &sp->col[T->firstCol + k];
      if (C->second && !R->col2On) continue;
      float c[3], a[3][3], ey = 0.f;
      mulR(R->R, C->c, c); c[0] += R->x; c[1] += R->y; c[2] += R->z;
      for (int m = 0; m < 3; m++) mulR(R->R, C->a + 3 * m, a[m]);
      int circle = C->kind == 1 || (C->kind == 2 && fabsf(a[1][1]) > .9f);
      if (C->kind == 1) ey = C->h[0]; else for (int m = 0; m < 3; m++) ey += C->h[m] * fabsf(a[m][1]);
      float lo = c[1] - ey, hi = c[1] + ey;
      // правка Роба 82 («на ящике Гомер будто прыгает много раз, камера дёргается, двойной прыжок не выходит»): верх, на котором
      // Гомер стоит (hi == y), отбрасывался условием «ниже ступней + 5 см» — кадр без опоры, кадр с выталкиванием наверх. Верх
      // до 35 см ниже ступней остаётся опорой
      if (hi < y - .35f || lo > y + HOMER_HEIGHT) continue;         // ниже ступней / выше головы
      float dx = *x - c[0], dz = *z - c[2], nx, nz, depth;
      if (circle) {
        float rad = C->h[0] + r, d2 = dx * dx + dz * dz;
        if (d2 >= rad * rad) continue;
        if (hi <= y + HOMER_STEP) { if (hi > *top) *top = hi; continue; }   // невысокое — можно встать сверху
        float d = sqrtf(d2); if (d < 1e-4f) { nx = 1.f; nz = 0.f; } else { nx = dx / d; nz = dz / d; }
        depth = rad - d;
      } else {
        int hk = 0; for (int m = 1; m < 3; m++) if (fabsf(a[m][1]) < fabsf(a[hk][1])) hk = m;
        float ux = a[hk][0], uz = a[hk][2], ul = sqrtf(ux * ux + uz * uz);
        if (ul < 1e-4f) { ux = 1.f; uz = 0.f; } else { ux /= ul; uz /= ul; }
        float vx = -uz, vz = ux, e1 = 0.f, e2 = 0.f;
        for (int m = 0; m < 3; m++) { e1 += C->h[m] * fabsf(a[m][0] * ux + a[m][2] * uz); e2 += C->h[m] * fabsf(a[m][0] * vx + a[m][2] * vz); }
        float lu = dx * ux + dz * uz, lv = dx * vx + dz * vz;
        float qu = clampf(lu, -e1, e1), qv = clampf(lv, -e2, e2), eu = lu - qu, ev = lv - qv, d2 = eu * eu + ev * ev;
        if (d2 >= r * r) continue;
        if (hi <= y + HOMER_STEP) { if (hi > *top) *top = hi; continue; }
        if (d2 > 1e-8f) { float d = sqrtf(d2); eu /= d; ev /= d; depth = r - d; }
        else { float pu = e1 - fabsf(lu), pv = e2 - fabsf(lv);
          if (pu < pv) { eu = lu < 0 ? -1.f : 1.f; ev = 0.f; depth = pu + r; } else { eu = 0.f; ev = lv < 0 ? -1.f : 1.f; depth = pv + r; } }
        nx = eu * ux + ev * vx; nz = eu * uz + ev * vz;
      }
      *x += nx * depth; *z += nz * depth; *onx = nx; *onz = nz; pushed = 1;
    }
  }
  return pushed;
}

#ifndef SKIN_HOST
void homerDraw(const Homer *H) { skinDrawPal(H->m, H->pal, &H->place, H->blink.eyeTex); }
// №666: замер посадки (только журнал). out: [0] макушка над землёй под машиной, м; [1] крыша машины над землёй; [2] таз (сустав, дети Motion_Root) над землёй;
// [3] рост персонажа стоя (bbox); [4] макушка ниже крыши, м (минус — торчит). Макушка = голова сустава Head в позе + (верх bbox − Head в позе покоя)
int homerSeatProbe(const Homer *H, float out[9]) {
  if (!H->m || !H->car.ok || homerOnFoot(H)) return 0;
  const SkinModel *m = H->m; int hj = skinFindJoint(m, "Head"); if (hj < 0) return 0;
  SkinXf rest[SKIN_MAXJ]; ScePspFMatrix4 pr[SKIN_MAXJ], wr[SKIN_MAXJ];
  skinSample(m, H->cLoco[0], 0.f, rest); skinPalette(m, rest, pr, wr);
  float top = m->h->bbox[4] - wr[hj].w.y;                 // макушка над суставом головы стоя
  const float *P = (const float *)&H->place; const ScePspFVector4 *q = &H->world[hj].w;
  float hy = P[1] * q->x + P[5] * q->y + P[9] * q->z + P[13];
  float ground; { float F[16]; anchorFrame(H, F); ground = F[13] - seatLift(H); }   // земля под машиной: якорь стоит на -lift от модели
  out[0] = hy + top - ground; out[1] = H->car.roof - ground; out[2] = 0.f; out[3] = m->h->bbox[4]; out[4] = out[1] - out[0]; out[5] = H->car.lift; out[6] = P[0] * q->x + P[4] * q->y + P[8] * q->z + P[12]; out[7] = hy; out[8] = P[2] * q->x + P[6] * q->y + P[10] * q->z + P[14];   // голова в мире
  int pj = (int)m->h->rootJoint; for (unsigned j = 0; j < m->h->nJoints; j++) if (m->j[j].parent == pj) { const ScePspFVector4 *b = &H->world[j].w; out[2] = P[1] * b->x + P[5] * b->y + P[9] * b->z + P[13] - ground; break; }
  return 1;
}
#endif
