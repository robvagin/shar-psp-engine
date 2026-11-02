#include <stdio.h>
#include <string.h>
#include <math.h>
#include "roomcam.h"
#include "convcam.h"
#include "camroll.h"
static int fails;
#define CHK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); fails++; } else printf("ok   %s\n", m); } while (0)
static RoomCam mk(int type, float x, float hw) { RoomCam c; memset(&c, 0, sizeof c); c.type = type; c.ntrig = 1; c.trig[0].kind = 1; c.trig[0].c[0] = x; c.trig[0].half[0] = hw; c.trig[0].half[1] = 5; c.trig[0].half[2] = 5;
  c.trig[0].ax[0][0] = 1; c.trig[0].ax[1][1] = 1; c.trig[0].ax[2][2] = 1; c.fov = 60; c.pos[0] = x; c.pos[1] = 3; c.pos[2] = -10; c.target[0] = x; return c; }
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
  printf(fails ? "FAIL %d\n" : "ALL OK\n", fails); return fails != 0;
}
