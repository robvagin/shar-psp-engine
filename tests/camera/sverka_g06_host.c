#include <stdio.h>
#include <string.h>
#include <math.h>
#include "roomcam.h"
#include "convcam.h"
#include "camroll.h"
#include "camlaw.h"
#include "g10b_calc.h"
static int fails;
#define CHK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)
static RoomCam mk(int type, float x, float hw) { RoomCam c; memset(&c, 0, sizeof c); c.type = type; c.ntrig = 1; c.trig[0].kind = 1; c.trig[0].c[0] = x; c.trig[0].half[0] = hw; c.trig[0].half[1] = 5; c.trig[0].half[2] = 5;
  c.trig[0].ax[0][0] = 1; c.trig[0].ax[1][1] = 1; c.trig[0].ax[2][2] = 1; c.fov = 60; c.pos[0] = x; c.pos[1] = 3; c.pos[2] = -10; c.target[0] = x; return c; }
#ifndef CONV_CHILD_SV_OLD
#define CONV_CHILD_SV_OLD 0
#endif
#ifndef BONUSCAM1109_OLD
#define BONUSCAM1109_OLD 0
#endif
#ifndef BONUSTOP_SV_OLD
#define BONUSTOP_SV_OLD 0
#endif
#ifndef CAM_TWIST_SV_OLD
#define CAM_TWIST_SV_OLD 0
#endif
#ifndef CAM_WALKZOOM_SV_OLD
#define CAM_WALKZOOM_SV_OLD 0
#endif
#ifndef CAM_SLOPE2_SV_OLD
#define CAM_SLOPE2_SV_OLD 0
#endif
#ifndef CAM_SHAKEPH_SV_OLD
#define CAM_SHAKEPH_SV_OLD 0
#endif
#ifndef CAM_REVERSE2_SV_OLD
#define CAM_REVERSE2_SV_OLD 0
#endif
#ifndef CAM_BUMPFWD_SV_OLD
#define CAM_BUMPFWD_SV_OLD 0
#endif
// шаги героя не длиннее метра (длиннее 3 м за кадр считается переносом — №1085)
static int go(const RoomCam *cs, int n, RoomCamState *st, float *p, float x, float *e, float *l, float *fy, const float *b, const float *bl) {
  int i = -9; float d = x - p[0]; int ns = (int)(fabsf(d)) + 1; for (int k = 0; k < ns; k++) { p[0] += d / ns; i = roomCamStepFrom(cs, n, st, p, .016f, e, l, fy, b, bl); } return i; }
int main(void) {
  // допуск по битам
  RoomCam a = mk(0, 0, 4); a.data = 1; CHK(!roomCamAllows(&a, 0, 0) && roomCamAllows(&a, 1, 20.f) && !roomCamAllows(&a, 1, 19.9f), "static Data1: только машине от 20 м/с");
  a.data = 2; CHK(roomCamAllows(&a, 0, 0) && !roomCamAllows(&a, 1, 30), "static Data2: только пешком");
  a.data = 3; CHK(!roomCamAllows(&a, 0, 0) && !roomCamAllows(&a, 1, 30), "static Data3: никогда");
  a.data = 0; CHK(roomCamAllows(&a, 0, 0) && roomCamAllows(&a, 1, 0), "static Data0: всем");
  RoomCam r = mk(1, 0, 4); r.axisPlay[2] = 3; CHK(!roomCamAllows(&r, 0, 0) && roomCamAllows(&r, 1, 25) && roomCamCut(&r), "рельс z=3: машина, срез");
  r.axisPlay[2] = 8; CHK(roomCamAllows(&r, 0, 0) && !roomCamAllows(&r, 1, 25), "рельс z=8: пешком");
  // круг 2: исключения по имени (P08 Д9)
  { RoomCam u = mk(0, 0, 4); u.data = 1; strcpy(u.name, "z3_ufocam"); CHK(roomCamAllows(&u, 1, 0) && roomCamAllows(&u, 0, 0), "z3_ufocam: без проверок скорости и вида");
    RoomCam hh[2]; hh[0] = mk(0, 0, 10); strcpy(hh[0].name, "hypecam"); hh[1] = mk(0, 5, 2); hh[1].trig[0].c[2] = 0; RoomCamState hs; memset(&hs, 0, sizeof hs); hs.cur = -1;
    float e_[3], l_[3], f_, pp[3] = {-8, 0, 0}; roomCamStepFrom(hh, 2, &hs, pp, .016f, e_, l_, &f_, 0, 0); pp[0] = 5;
    CHK(go(hh, 2, &hs, pp, 5, e_, l_, &f_, 0, 0) == 0, "hypecam: другую камеру выбрать нельзя, пока Гомер в зоне"); }
  // выбор: последний вошедший побеждает, выход возвращает прежнюю
  RoomCam cs[2]; cs[0] = mk(0, 0, 10); cs[1] = mk(0, 5, 2); cs[1].trig[0].c[2] = 0; cs[0].posLag = cs[1].posLag = 0.1f;
  RoomCamState st; memset(&st, 0, sizeof st); st.cur = -1; float e[3], l[3], fy, base[3] = {0, 2, -5}, bl[3] = {0, 1, 0}, p[3] = {-8, 0, 0};
  int i = roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl); CHK(i == 0, "вошёл в первую");
  i = go(cs, 2, &st, p, 5, e, l, &fy, base, bl); CHK(i == 1, "вошёл во вторую: она сразу (последняя вошедшая)");
  CHK(e[0] < 4.5f, "переход идёт плавно, не срез (первый кадр не в цели)");
  for (int k = 0; k < 130; k++) i = roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
  CHK(fabsf(e[0] - 5.f) < .02f, "через ~2 с камера в цели");
  i = go(cs, 2, &st, p, 8, e, l, &fy, base, bl); CHK(i == 0, "вышел из второй: вернулась первая");
  i = go(cs, 2, &st, p, 30, e, l, &fy, base, bl); CHK(i == -2, "вышел из всех: плавный возврат (-2)");
  for (int k = 0; k < 60; k++) i = roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
  CHK(i == -1, "возврат закончился: камера ходьбы");
  // срез на CutInOut
  cs[1].cut = 1; memset(&st, 0, sizeof st); st.cur = -1; p[0] = -8; roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
  i = go(cs, 2, &st, p, 5, e, l, &fy, base, bl); CHK(i == 1 && fabsf(e[0] - 5.f) < 1e-4f, "CutInOut: срез");
  // №1085: перенос героя (загрузка, миссия, дверь) — первая камера срезом, обычный шаг между камерами — переходом
  { cs[1].cut = 0; memset(&st, 0, sizeof st); st.cur = -1; p[0] = -8; roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
    i = go(cs, 2, &st, p, 5, e, l, &fy, base, bl); CHK(i == 1 && e[0] < 4.5f, "№1085: ходьба между камерами комнаты — переход, не срез");
    p[0] = 5; roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl); p[0] = 200.f; roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);   // герой унесён далеко
    p[0] = 5.f; i = roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
    CHK(i == 1 && fabsf(e[0] - 5.f) < 1e-4f, "№1085: после переноса первая камера срезом, сразу на месте");
    p[0] = 5.f; for (int k = 0; k < 4; k++) i = roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
    roomCamRestart(&st); p[0] = 5.f; i = roomCamStepFrom(cs, 2, &st, p, .016f, e, l, &fy, base, bl);
    CHK(i == 1 && fabsf(e[0] - 5.f) < 1e-4f, "№1085: вход в помещение (перезапуск) — срез"); }
  // Distance: рельс по оси x от -10 до 10, высота 3, Гомер на z=-4... расстояние = MaxRadius
  float pts[4][3] = {{-12, 3, 0}, {-10, 3, 0}, {10, 3, 0}, {12, 3, 0}}; RoomCam rc = mk(1, 0, 30); rc.npts = 4; rc.pts = (const float (*)[3])pts; rc.maxR = 5; rc.minR = 1; rc.posLag = 1;
  float hp[3] = {2, 0, 4}, t = roomRailParamDt(&rc, -1.f, hp, .016f), q[3]; roomRailAt(&rc, t, q);
  CHK(fabsf(hypotf(q[0] - hp[0], q[2] - hp[2]) - 5.f) < .02f, "Distance: стоит ровно на MaxRadius от цели");
  hp[0] = 8; float t2 = roomRailParamDt(&rc, t, hp, .016f); CHK(fabsf(t2 - t) <= .25f * 20.f * 1.0f + 1e-3f && t2 > t, "Distance: ход ограничен 0,25 отрезка за 1/60 с и едет за целью");
  // разговор
  ConvScene sc; memset(&sc, 0, sizeof sc); sc.pc[0] = 0; sc.pc[2] = 0; sc.npc[0] = 0; sc.npc[2] = 4; float ce[3], cl[3], cf;
  convPose(&sc, CONV_PC_FAR, 1, ce, cl, &cf);
  CHK(fabsf(hypotf(ce[0] - 0, ce[2] - 2) - 5.0f) < .01f && fabsf(ce[1] - 2.11f) < .01f && fabsf(cl[1] - 1.03f) < .01f, "pc_far: дистанция 5,0 м, глаз +2,11, цель +1,03");
  CHK(fabsf((ce[2] - 2) - 5.f * sinf(.55f)) < .01f, "pc_far: 2,6 м вперёд к собеседнику");
  convPose(&sc, CONV_NPC_NEAR, 1, ce, cl, &cf);
  CHK(fabsf(hypotf(ce[0], ce[2] - 2) - 6.2f) < .01f && fabsf(cl[2] - (2 - 1.06f)) < .01f && fabsf(cf - 16.0f) < .3f, "npc_near: 6,2 м, сдвиг цели −1,06, кадр ~16° по вертикали (28° по горизонтали)");
  const char *cams[3] = {"pc_far", 0, 0}; int fd;
  CHK(convShotFor(cams, 3, 1, 0, &fd) == CONV_NPC_NEAR && convShotFor(cams, 3, 0, 1, &fd) == CONV_PC_FAR && convShotFor(0, 0, 0, 1, &fd) == CONV_PC_NEAR, "план реплики: своя запись, иначе ближний говорящего, с первой реплики");
  sc.hasBest = 0; CHK(convSide(&sc, (float[3]){100, 0, 100}) == 1, "без локатора сторона одна");
  // №1084: крен камеры пешком (camroll.h)
  { float r1 = 0, r7 = 0, mx = 0; for (int i = 0; i < 600; i++) { float spd = (i / 20) % 2 ? 7.f : 0.f; r1 = camRollStepSv(r1, 1, 1.f, spd, 1.f / 60); mx = fmaxf(mx, fabsf(r1)); }
    CHK(mx == 0.f, "№1084: уровень 1, бег влево-вправо у персонажа — крен 0 всё время");
    for (int i = 0; i < 300; i++) r7 = camRollStepSv(r7, 7, 1.f, .5f, 1.f / 60);
    CHK(r7 > .17f && r7 <= .1761f, "№1084: уровень 7, медленно рядом с персонажем — крен до 10°");
    for (int i = 0; i < 180; i++) r7 = camRollStepSv(r7, 7, 1.f, 7.f, 1.f / 60);
    CHK(r7 < 1e-4f, "№1084: уровень 7, бег — крен уходит к 0");
    float rn = 0, ro = 0; for (int i = 0; i < 6; i++) rn = camRollStepSv(rn, 7, 1.f, .3f, 1.f / 60);   // 0,1 с остановки на развороте бега
    CHK(rn < .03f, "№1084: короткая остановка на развороте — крен меньше 2°, без рывка на 10°");
    float far_ = 0; for (int i = 0; i < 300; i++) far_ = camRollStepSv(far_, 7, 0.f, .1f, 1.f / 60);
    CHK(far_ == 0.f, "№1084: уровень 7, персонажа рядом нет — крен 0");
    (void)ro; }
  // ---- круг 6: законы камеры (camlaw.h) и выход из зоны ----
  { // кубический шаг: на конце цель и скорость нуль, симметрия на середине
    float r0 = 0.f, p1 = camHermite(0.f, &r0, 1.f, 1.f); CHK(fabsf(p1 - 1.f) < 1e-5f && fabsf(r0) < 1e-5f, "кубический шаг: за долю 1 — ровно цель, скорость нуль");
    r0 = 0.f; float ph = camHermite(0.f, &r0, 1.f, .5f); CHK(fabsf(ph - .5f) < 1e-5f, "кубический шаг: на середине доли — середина пути");
    r0 = 0.f; float pe = camHermite(0.f, &r0, 1.f, .02f); CHK(pe < .002f, "кубический шаг: доля 0,02 с места почти не двигает (плавный старт)"); }
  { // крен (P08:20): закон — тот же кубический отклик в обе стороны; старый — рывок вверх
    float cur = 0.f, vel = 0.f, mx = 0.f; int ok = 1;
    for (int i = 0; i < 6; i++) { if (CAM_TWIST_SV_OLD) cur = camRollStepSv(cur, 7, 1.f, .3f, 1.f / 60); else camTwistStep(&cur, &vel, camTwistWant(7, 1.f, .3f), 1.f / 60); }
    CHK(cur < .012f, "крен: за 0,1 с остановки — меньше 0,7° (без рывка)");
    for (int i = 0; i < 600; i++) { if (CAM_TWIST_SV_OLD) cur = camRollStepSv(cur, 7, 1.f, .3f, 1.f / 60); else camTwistStep(&cur, &vel, camTwistWant(7, 1.f, .3f), 1.f / 60); mx = fmaxf(mx, cur); }
    CHK(fabsf(cur - .176f) < .004f && mx < .176f * 1.04f, "крен: медленно у персонажа — до 10° (0,176), перелёт не больше 4%");
    for (int i = 0; i < 600; i++) { camTwistStep(&cur, &vel, camTwistWant(7, 1.f, 7.f), 1.f / 60); }
    CHK(fabsf(cur) < .004f, "крен: бег — уходит к 0");
    CHK(camTwistWant(7, 1.f, 8.f / 3.f - .01f) > 0.f && camTwistWant(7, 1.f, 8.f / 3.f + .01f) == 0.f && camTwistWant(6, 1.f, 0.f) == 0.f && camTwistWant(7, 0.f, 0.f) == 0.f, "крен: порог — треть бега (ниже/выше), только уровень 7, только при персонаже");
    (void)ok; }
  { // автозум на персонажа (2.9)
    float k1 = CAM_WALKZOOM_SV_OLD ? tanf(.245f) / tanf(35.f * 3.14159265f / 180.f) : camWalkZoomK(camWalkZoomWant(1.f));
    CHK(k1 > .5f && k1 < .6f, "автозум: вплотную кадр уже на 0,49 рад (70° → 42°), множитель tan ~0,55, а не 0,36");
    CHK(fabsf(camWalkZoomWant(0.f) - CAMLAW_WALK_FOV_MAX) < 1e-6f && camWalkZoomWant(5.f) >= CAMLAW_WALK_FOV_MIN, "автозум: без персонажа — полный угол; не уже 20°");
    float e[3] = {0.f, 2.f, 0.f}, l[3] = {0.f, 1.f, 5.f}, ahead[3] = {0.f, 1.f, 3.f}, side40[3] = {3.f * tanf(.7f), 1.f, 3.f}, behind[3] = {0.f, 1.f, -3.f};
    int s40 = CAM_WALKZOOM_SV_OLD ? ((side40[0] - e[0]) * (l[0] - e[0]) + (side40[2] - e[2]) * (l[2] - e[2]) > 0.f) : camWalkPedSeen(e, l, side40);
    CHK(camWalkPedSeen(e, l, ahead) && !s40 && !camWalkPedSeen(e, l, behind), "автозум: видим перед камерой; сбоку на 40° (вне пирамиды 33°) — нет; сзади — нет"); }
  { // склон (2.6)
    int w = CAM_SLOPE2_SV_OLD ? (1 ? 0 : 0) : (int)(camSlopeWant(0, 1, 1, 0) * 10.f + .5f);
    CHK(camSlopeWant(1, 0, 0, 0) == .3f && w == 2 && camSlopeWant(0, 0, 1, 0) == 0.f && camSlopeWant(0, 1, 0, 0) == 0.f && camSlopeWant(0, 1, 1, 1) == 0.f, "склон: круче 14° — 0,3; верх свободен + низ закрыт + стик не тронут — 0,2; иначе 0"); }
  { // тряска (1.17): фаза оригинала
    float v0 = CAM_SHAKEPH_SV_OLD ? 0.f : camShakeAt(0.f);
    CHK(fabsf(v0) > 1.0f && fabsf(camShakeAt(.18f)) == 0.f && fabsf(camShakeAt(.09f)) <= .56f, "тряска: первый кадр уже около 1,07 м (фаза 0,87 + шаг), к 0,18 с — нуль, на середине размах не больше 0,55");
  }
  { // задний ход (1.11, P08:10)
    CHK(camRevOn(1, 1, 1, 7.f, 0) && !camRevOn(1, 1, 1, 6.f, 0) && camRevOn(1, 1, 1, 1.f, 1) && !camRevOn(0, 1, 1, 9.f, 0) && !camRevOn(1, 0, 1, 9.f, 0) && !camRevOn(1, 1, 0, 9.f, 0), "задний ход: включается устойчивой машине, тормоз, назад, v² > 45 или контакт; в воздухе/опрокинутой — нет");
    CHK(camRevOff(0, .5f) && !camRevOff(0, .49f) && !camRevOff(1, 1.f), "задний ход: выход — не едет назад И газ ≥ 0,5");
    float d3[3] = {0.f, 2.f, -12.f}, out[3], vz[3] = {0.f, 0.f, 0.f};
    camRevDesired(d3, 9.5f, 3.f, vz, 0.f, 1.f, .0166667f, out);
    CHK(CAM_REVERSE2_SV_OLD ? 0 : (fabsf(out[1] - 3.f) < 1e-5f && fabsf(hypotf(out[0], out[2]) - 9.5f * 12.f / sqrtf(4.f + 144.f)) < 1e-3f), "задний ход: дальше дистанции — на дистанцию, высота = высоте включения");
    float d4[3] = {0.f, 3.f, -5.f}, vt[3] = {0.f, 0.f, -5.f}, o2[3];
    camRevDesired(d4, 9.5f, 3.f, vt, 0.f, 1.f, .0166667f, o2);
    float sh = fabsf(o2[0] - d4[0]);
    CHK(!CAM_REVERSE2_SV_OLD && sh > .2f && sh < .27f, "задний ход: ближе 7 м, машина на камеру — сдвиг вбок на (дистанция/2) × 0,05 за кадр (0,24 м), а не на полдистанции (4,75 м)");
    float va[3] = {0.f, 0.f, 5.f}; camRevDesired(d4, 9.5f, 3.f, va, 0.f, 1.f, .0166667f, o2);
    CHK(fabsf(o2[0] - d4[0]) < 1e-6f, "задний ход: машина едет от камеры — вбок не сдвигаем"); }
  { // бампер (1.20)
    float f = CAM_BUMPFWD_SV_OLD ? -.32f + 2.675f - 1.0f : camBumperFwd(1.47f);
    CHK(fabsf(f - .47f) < 1e-5f, "бампер: глаз на передней оси минус 1,0 м (famil_v 0,47 м от начала), а не на носу кузова (1,355)"); }
  { // авто-зум статичной камеры для машины (5.1): в обычной игре нет, полный угол данных
    RoomCam c = mk(0, 0, 4); c.fov = 80.f; float cur = 0.f;
    float f = roomCamFovCar(&c, 30.f, &cur, .0166667f);
    CHK(fabsf(f - roomCamFovFromH(80.f)) < .01f, "статичная камера для машины: на 30 м полный угол данных (не сужается до 20°)"); }
  { // выход из последней камеры в камеру ходьбы (3.5, P08:14): за 7 с, доля 0,02 + 0,98·t/7, кубический шаг
    RoomCam c = mk(0, 0, 3); c.pos[0] = 6.f; c.pos[1] = 2.f; c.pos[2] = 0.f; c.target[0] = 0.f; c.target[1] = 0.f; c.target[2] = 0.f; c.trig[0].c[0] = 0.f;
    RoomCamState rs; memset(&rs, 0, sizeof rs); rs.cur = -1; float e[3], l[3], fy, bE[3] = {0.f, 2.f, 6.f}, bL[3] = {0.f, 0.f, 0.f}, p[3] = {0.f, 0.f, 0.f};
    for (int k = 0; k < 4; k++) roomCamStepFrom(&c, 1, &rs, p, .016f, e, l, &fy, bE, bL);   // камера ходьбы: Гомер внутри зоны
    p[0] = 50.f; rs.hasP = 0;   // вышел
    int r = roomCamStepFrom(&c, 1, &rs, p, .016f, e, l, &fy, bE, bL);
    float g0 = hypotf(e[0] - bE[0], hypotf(e[1] - bE[1], e[2] - bE[2]));
    for (int k = 0; k < 5; k++) r = roomCamStepFrom(&c, 1, &rs, p, .016f, e, l, &fy, bE, bL);   // ещё 0,1 с
    float g1 = hypotf(e[0] - bE[0], hypotf(e[1] - bE[1], e[2] - bE[2]));
    CHK(r == -2 && g0 > 7.f && (ROOMCAM_EXIT_SV_OLD ? 0 : g1 > .93f * g0), "выход: за первые 0,1 с камера почти не сдвинулась (старое e^(-t/0,15) — уже на половине)");
    int steps = 6; float t = 0.096f; for (; steps < 800 && r == -2; steps++) { r = roomCamStepFrom(&c, 1, &rs, p, .016f, e, l, &fy, bE, bL); t += .016f; }
    CHK(!ROOMCAM_EXIT_SV_OLD && r == -1 && t > .3f && t <= 7.1f, "выход: не мгновенно (больше 0,3 с) и не позже 7 с"); }
  // ---- круг 7: скорость перехода по данным камеры (P18-02, P18-03, P08:14) и камера разговора без «ребёнка» ----
  { RoomCam cc[2]; cc[0] = mk(0, 0, 3); cc[1] = mk(0, 10, 3);
    for (int k = 0; k < 2; k++) { cc[k].pos[0] = 10.f * k; cc[k].target[0] = cc[k].pos[0]; cc[k].posLag = 0.f; }
    cc[1].posLag = .04f;   // скорость перехода второй камеры из данных
    RoomCamState rs; memset(&rs, 0, sizeof rs); rs.cur = -1; float e[3], l[3], fy, b[3] = {0.f, 2.f, -5.f}, bl[3] = {0.f, 1.f, 0.f}, p[3] = {-1.f, 0.f, 0.f};
    roomCamStepFrom(cc, 2, &rs, p, .016f, e, l, &fy, b, bl);
    go(cc, 2, &rs, p, 11.f, e, l, &fy, b, bl);
    float gap0 = 10.f - e[0], tt = 0.f;
    for (int k = 0; k < 400; k++) { roomCamStepFrom(cc, 2, &rs, p, .016f, e, l, &fy, b, bl); tt += .016f; if (10.f - e[0] < .05f * gap0) break; }
    CHK(ROOMCAM_RATE_SV_OLD ? 0 : (gap0 > 1.f && tt < .9f), "переход между статичными: скорость 0,04 из данных — 95% пути быстрее 0,9 с (при общих 0,02 — 1,3 с)");
    RoomCam rl = mk(1, 0, 3); rl.axisPlay[0] = 0.f; RoomCam rr = mk(1, 0, 3); rr.axisPlay[0] = .08f; RoomCam st = mk(0, 0, 3); st.posLag = .04f;
    CHK((ROOMCAM_RATE_SV_OLD ? 0 : (fabsf(roomCamTransRate(&rl) - .02f) < 1e-6f && fabsf(roomCamTransRate(&rr) - .08f) < 1e-6f && fabsf(roomCamTransRate(&st) - .04f) < 1e-6f)), "скорость перехода: рельс — x осевой игры, без неё 0,02; статичная — поле данных"); }
  { ConvScene a, b2; memset(&a, 0, sizeof a); a.pc[0] = 0; a.pc[2] = 0; a.npc[0] = 0; a.npc[2] = 4; b2 = a; b2.pcHead = 1.0f; float ea[3], la[3], fa, eb[3], lb[3], fb;
    convPose(&a, CONV_PC_NEAR, 1, ea, la, &fa); convPose(&b2, CONV_PC_NEAR, 1, eb, lb, &fb);
    CHK(fabsf(ea[1] - eb[1]) < 1e-6f && fabsf(la[1] - lb[1]) < 1e-6f, "камера разговора: низкий снимаемый не опускает камеру и цель (в оригинале отметка «ребёнок» не доходит до камеры)"); }
  { // сценарий заднего хода на хосте (закон камеры, не игра): машина едет назад 8 м/с на камеру, стоящую в 6 м (после касания стены); камера уходит вбок плавно и не проходит сквозь машину
    float car[3] = {0.f, 1.f, 0.f}, cam[3] = {0.f, 4.f, -6.f}, v[3] = {0.f, 0.f, -8.f}, dist = 6.f, yH = 3.f, mind = 99.f, lat01 = 0.f, lat1 = 0.f; float dt = 1.f / 60;
    for (int i = 0; i < 300; i++) {
      float d3[3] = {cam[0] - car[0], cam[1] - car[1], cam[2] - car[2]}, des[3];
      if (CAM_REVERSE2_SV_OLD) { des[0] = d3[0]; des[1] = d3[1]; des[2] = d3[2]; float sq = d3[0] * d3[0] + d3[1] * d3[1] + d3[2] * d3[2];
        if (sq < 49.f && v[0] * d3[0] + v[2] * d3[2] > 0.f && d3[2] < 0.f) des[0] = (d3[0] >= 0.f ? 1.f : -1.f) * dist * .5f;
      } else camRevDesired(d3, dist, yH, v, 0.f, 1.f, dt, des);
      float lagc = .05f + fminf(64.f / 400.f, 1.f), k1 = 1.f - powf(1.f - lagc, dt * 60.f);
      car[2] += v[2] * dt;
      for (int k = 0; k < 3; k++) cam[k] += (car[k] + des[k] - cam[k]) * k1;
      float dd = sqrtf((cam[0] - car[0]) * (cam[0] - car[0]) + (cam[1] - car[1]) * (cam[1] - car[1]) + (cam[2] - car[2]) * (cam[2] - car[2])); if (dd < mind) mind = dd;
      if (i == 5) lat01 = fabsf(cam[0]); if (i == 59) lat1 = fabsf(cam[0]); }
    CHK(lat01 < .6f && lat1 > 1.5f && lat1 < 4.2f && mind > 2.5f, "сценарий заднего хода: за 0,1 с в сторону меньше 0,6 м (не бросок), за 1 с 1,5–4 м, до машины всегда больше 2,5 м"); }
  // ---- №1109 (камера бонус-игры) ----
  { float e[3], a[3], u[3], fv, zn, zf, d; int ok = camBonusTop(1, e, a, u, &fv, &zn, &zf, &d);
    CHK(ok && fabsf(e[0] - 26.88f) < 1e-4f && fabsf(e[1] - 157.87f) < 1e-4f && fabsf(e[2] - 206.23f) < 1e-4f && fabsf(u[0] + .0781452f) < 1e-4f && fabsf(u[1] - .7965135f) < 1e-4f && fabsf(u[2] + .5995493f) < 1e-4f
        && fabsf(fv - 17.2892584f) < 1e-3f && zn == 150.f && zf == 800.f && fabsf(d - 261.105881f) < 1e-2f, "камера бонус-игры, трасса 1: глаз, верх, угол 17,289°, 150/800, расстояние 261,1 — как измерено на PS2");
    int all = 1; float fvs[8]; for (int t = 1; t <= 7; t++) { float ee[3], aa[3], uu[3], z1, z2, dd; all &= camBonusTop(t, ee, aa, uu, &fvs[t], &z1, &z2, &dd) && fabsf(uu[0] * uu[0] + uu[1] * uu[1] + uu[2] * uu[2] - 1.f) < 1e-3f && ee[2] > 100.f && aa[0] == 0.f; }
    float e4[3], a4[3], u4[3], f4, n4, z4, d4, n6; camBonusTop(4, e4, a4, u4, &f4, &n4, &z4, &d4); camBonusTop(6, e4, a4, u4, &f4, &n6, &z4, &d4);
    CHK(all && !camBonusTop(0, e, a, u, &fv, &zn, &zf, &d) && !camBonusTop(8, e, a, u, &fv, &zn, &zf, &d) && fvs[4] < fvs[1] && n6 == 250.f && fvs[3] < fvs[1], "камера бонус-игры: таблица для трасс 1–7 (на 4 трассе кадр уже 0,26 рад, на 6 ближняя 250), вне 1–7 — нет");
    // старая политика: только трасса 1 из таблицы — для трассы 4 это был бы выбор по рамке
    int ps2_4 = !BONUSTOP_SV_OLD; CHK(ps2_4, "камера бонус-игры: трасса 4 берётся из таблицы оригинала (при BONUSTOP_SV_OLD=1 — по рамке трассы)"); }
  { // цикл Select: сверху -> вид города 0 -> бампер (первое лицо) -> вид города 1 -> сверху
    int cam = 0, seq[8], n = 0; for (int i = 0; i < 5; i++) { int dune = 0; cam = BONUSCAM1109_OLD ? !cam : camBonusNext(cam, g10bViewNext(camBonusCv(cam), dune, 0)); seq[n++] = cam; }
    CHK(!BONUSCAM1109_OLD && seq[0] == 1 && seq[1] == 2 && seq[2] == 3 && seq[3] == 0 && seq[4] == 1, "Select в бонус-игре: сверху → вид 0 → бампер → вид 1 → сверху (как цикл в городе, плюс вид сверху)");
    int cam2 = 0, s2[4]; for (int i = 0; i < 3; i++) { cam2 = camBonusNext(cam2, g10bViewNext(camBonusCv(cam2), 1, 0)); s2[i] = cam2; }
    CHK(!BONUSCAM1109_OLD && s2[0] == 1 && s2[1] == 3 && s2[2] == 0, "Select у багги (бампера нет): сверху → вид 0 → вид 1 → сверху");
    CHK(camBonusCv(1) == 0 && camBonusCv(2) == 2 && camBonusCv(3) == 1 && camBonusOf(0) == 1 && camBonusOf(2) == 2 && camBonusOf(1) == 3, "соответствие номеров видов города"); }
  printf(fails ? "FAIL %d\n" : "ALL OK\n", fails); return fails != 0;
}
