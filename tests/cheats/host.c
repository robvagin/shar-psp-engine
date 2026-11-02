// №1104 (коды оригинала): хост-проверка ввода кодов (viewer/cheats.h) и эффектов в car.c на настоящих game.c / car.c с данными уровня (tools/bots/world.c).
// Каждая проверка обязана упасть на красной сборке: -DCHEATS1104_OLD=1 (кодов нет) — см. tests/cheats/host.sh.
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "world.h"
#include "cheats.h"
#include "cheatcam.h"
#include "g10b_calc.h"
static int fails;
#define CHECK(name, ok, ...) do { int o_ = (ok); printf("CHECK %-16s %s ", name, o_ ? "OK  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!o_) fails++; } while (0)
static const unsigned FACE[4] = {GP_CROSS, GP_CIRCLE, GP_SQUARE, GP_TRIANGLE};
// ввод четвёрки при зажатых L и R: по одному нажатию на кадр; возвращает последнее событие
static int enter(CheatSys *c, const unsigned char *seq, unsigned lr, int allStory, int *id) {
  int ev = CHEV_NONE, e, got = -1, t; cheatsFrame(c, lr, 0, allStory, &t);
  for (int i = 0; i < 4; i++) { e = cheatsFrame(c, lr, FACE[seq[i]], allStory, &t); if (e) { ev = e; got = t; } cheatsFrame(c, lr, 0, allStory, &t); }
  if (id) *id = got;
  return ev;
}
static BotWorld W;
static void drive(void) { bwNew(&W); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0); gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0); }
int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IONBF, 0);
  // ---- таблица и ввод ----
  { int dup = 0; for (int i = 0; i < CHEAT_TAB_N; i++) for (int j = i + 1; j < CHEAT_TAB_N; j++) if (!memcmp(CHEAT_TAB[i].seq, CHEAT_TAB[j].seq, 4)) dup++;
    CHECK("table", CHEAT_TAB_N == 12 && dup == 0, "кодов %d (ждём 12 выпускных), одинаковых комбинаций %d", CHEAT_TAB_N, dup); }
  { int good = 0, wrongOther = 0;   // каждый код включает своё и только своё
    for (int i = 0; i < CHEAT_TAB_N; i++) { CheatSys c; cheatsReset(&c); int id = -1; int ev = enter(&c, CHEAT_TAB[i].seq, GP_L | GP_R, 1, &id);
      if (ev == CHEV_OK && id == CHEAT_TAB[i].id && c.on == (1u << CHEAT_TAB[i].id) && cheatOn(&c, CHEAT_TAB[i].id)) good++; else wrongOther++; }
    CHECK("each_code", good == CHEAT_TAB_N && wrongOther == 0, "верно включили %d из %d", good, CHEAT_TAB_N); }
  { int stray = 0, bad = 0;   // все 256 четвёрок: кроме таблицы — ошибка и ничего не включено
    for (int n = 0; n < 256; n++) { unsigned char s[4] = {(unsigned char)(n & 3), (unsigned char)(n >> 2 & 3), (unsigned char)(n >> 4 & 3), (unsigned char)(n >> 6 & 3)};
      CheatSys c; cheatsReset(&c); int id = -1, ev = enter(&c, s, GP_L | GP_R, 1, &id), reg = cheatFind(s) >= 0;
      if (reg) continue; if (ev != CHEV_FAIL) bad++; if (c.on) stray++; }
    CHECK("wrong_input", bad == 0 && stray == 0, "неверных комбинаций без ошибки %d, включилось лишнее %d (ждём 0 и 0)", bad, stray); }
  { CheatSys c; cheatsReset(&c); int id; int e1 = enter(&c, CHEAT_TAB[1].seq, GP_L, 1, &id), e2 = enter(&c, CHEAT_TAB[1].seq, GP_R, 1, &id), e3 = enter(&c, CHEAT_TAB[1].seq, 0, 1, &id);
    CHECK("need_L_and_R", e1 == 0 && e2 == 0 && e3 == 0 && c.on == 0, "только L, только R, без них: событий %d/%d/%d, включено %x", e1, e2, e3, c.on); }
  { CheatSys c; cheatsReset(&c); int id; unsigned LR = GP_L | GP_R;   // три нажатия, R отпущена на кадр и снова — набор сброшен, четвёртое нажатие кода не завершает
    cheatsFrame(&c, LR, 0, 1, &id); for (int i = 0; i < 3; i++) cheatsFrame(&c, LR, FACE[CHEAT_TAB[1].seq[i]], 1, &id); cheatsFrame(&c, GP_L, 0, 1, &id); cheatsFrame(&c, LR, 0, 1, &id);
    int ev = cheatsFrame(&c, LR, FACE[CHEAT_TAB[1].seq[3]], 1, &id);
    CHECK("release_resets", ev == 0 && c.on == 0, "после отпускания R четвёртое нажатие: событие %d, включено %x (ждём 0 и 0)", ev, c.on); }
  { CheatSys c; cheatsReset(&c); int id; int ev = enter(&c, CHEAT_TAB[CH_VEHICLES].seq, GP_L | GP_R, 0, &id);
    CheatSys d; cheatsReset(&d); int ev2 = enter(&d, CHEAT_TAB[CH_VEHICLES].seq, GP_L | GP_R, 1, &id);
    CHECK("vehicles_gate", ev == CHEV_FAIL && c.on == 0 && ev2 == CHEV_OK && cheatOn(&d, CH_VEHICLES), "код машин без всех миссий: событие %d (ждём 2), с ними: %d (ждём 1)", ev, ev2); }
  { CheatSys c; cheatsReset(&c); int id; enter(&c, CHEAT_TAB[CH_SPEEDO].seq, GP_L | GP_R, 1, &id); int ev = enter(&c, CHEAT_TAB[CH_SPEEDO].seq, GP_L | GP_R, 1, &id);
    int on1 = cheatOn(&c, CH_SPEEDO); cheatsReset(&c);
    CHECK("again_keeps_on", ev == CHEV_OK && on1 && !cheatOn(&c, CH_SPEEDO), "повторный ввод не выключает (%d), сброс при выходе в меню выключает (%d)", on1, cheatOn(&c, CH_SPEEDO)); }
  { CheatSys c; cheatsReset(&c); int id; unsigned LR = GP_L | GP_R; cheatsFrame(&c, LR, 0, 1, &id);   // четыре нажатия в одном кадре подряд по порядку ✕ ○ □ △ не считаются «вне порядка»: код ✕○□△ не в выпускной таблице
    int ev = cheatsFrame(&c, LR, GP_CROSS | GP_CIRCLE | GP_SQUARE | GP_TRIANGLE, 1, &id);
    CHECK("same_frame_order", ev == CHEV_FAIL && c.on == 0, "✕○□△ одним кадром: событие %d (ждём 2: такой комбинации в выпускной сборке нет)", ev); }
  // ---- камеры кода Unlock All Cameras ----
  { int v = 1, seqOn[8]; for (int i = 0; i < 8; i++) { v = ccViewNext(v, 0, 1, g10bViewNext); seqOn[i] = v; }
    int w = 1; int seqOff[2]; for (int i = 0; i < 2; i++) { w = ccViewNext(w, 0, 0, g10bViewNext); seqOff[i] = w; }
    CHECK("cam_cycle", seqOn[0] == 3 && seqOn[1] == 4 && seqOn[2] == 5 && seqOn[3] == 6 && seqOn[4] == 7 && seqOn[5] == 8 && seqOn[6] == 0 && seqOff[0] == 0 && seqOff[1] == 2,
          "с кодом после дальнего: %d %d %d %d %d %d %d (ждём 3 4 5 6 7 8 0); без кода: %d %d (ждём 0 2, как раньше)", seqOn[0], seqOn[1], seqOn[2], seqOn[3], seqOn[4], seqOn[5], seqOn[6], seqOff[0], seqOff[1]);
    float fy = ccFovYDeg(16.f / 9.f);
    CHECK("cam_fov92", fabsf(fy - 60.4f) < .3f, "92,2° по горизонтали на 16:9 — %.1f° по вертикали (ждём 60,4)", fy);
    CcState S; CcIn in; float e[3], l[3], fh = 0; memset(&in, 0, sizeof in); in.dt = 1.f / 60.f; in.t[0] = 10.f; in.t[1] = 2.f; in.t[2] = -5.f; in.hx = 1.f;
    in.prevEye[0] = -20.f; in.prevEye[1] = 8.f; in.prevEye[2] = 0.f; in.prevLook[0] = 10.f; in.prevLook[1] = 2.f; in.prevLook[2] = -5.f;
    ccResetState(&S); for (int f = 0; f < 1500; f++) ccView(&S, CC_VIEW_CHASE, &in, e, l, &fh);
    CHECK("cam_chase", fabsf(e[0] - (in.t[0] - 15.f)) < .5f && fabsf(e[1] - (in.t[1] + 50.f)) < .5f && fabsf(l[0] - in.t[0]) < .2f && fh > .3f && fh < 1.76f,
          "chase через 25 с: глаз %.1f %.1f %.1f (ждём 15 м позади и 50 м над целью: -5, 52, -5), угол %.2f рад", e[0], e[1], e[2], fh);
    ccResetState(&S); ccView(&S, CC_VIEW_KULL, &in, e, l, &fh); float d0 = e[0] - in.t[0];
    CcIn i2 = in; i2.sx = 1.f; for (int f = 0; f < 60; f++) ccView(&S, CC_VIEW_KULL, &i2, e, l, &fh); float ang = atan2f(e[2] - in.t[2], e[0] - in.t[0]);
    i2.sx = 0.f; i2.sy = 0.f; i2.toggle = 1.f; i2.sy = 1.f; for (int f = 0; f < 400; f++) ccView(&S, CC_VIEW_KULL, &i2, e, l, &fh); float dist = sqrtf((e[0]-in.t[0])*(e[0]-in.t[0]) + (e[1]-in.t[1])*(e[1]-in.t[1]) + (e[2]-in.t[2])*(e[2]-in.t[2]));
    CHECK("cam_kull", fabsf(d0 - 5.f) < .01f && fabsf(ang - 1.044f) < .05f && fabsf(dist - 2.f) < .01f, "kull: старт %.2f м сбоку (ждём 5), стик за 1 с повернул на %.3f рад (ждём 1,044), кнопка+стик вперёд доводит до %.2f м (ждём 2)", d0, ang, dist);
    ccResetState(&S); in.prevEye[0] = -20.f; ccView(&S, CC_VIEW_TRACK, &in, e, l, &fh); float off0 = e[0] - in.t[0];
    CcIn i3 = in; i3.t[0] = 50.f; ccView(&S, CC_VIEW_TRACK, &i3, e, l, &fh); float off1 = e[0] - i3.t[0];
    CHECK("cam_tracker", fabsf(off0 + 30.f) < .01f && fabsf(off1 + 30.f) < .01f, "tracker держит смещение от цели при её движении: %.2f и %.2f м (ждём -30 и -30)", off0, off1);
    ccResetState(&S); memset(&i3, 0, sizeof i3); i3 = in; i3.prevEye[0] = 0.f; i3.prevEye[1] = 0.f; i3.prevEye[2] = 0.f; i3.prevLook[0] = 0.f; i3.prevLook[1] = 0.f; i3.prevLook[2] = 1.f; i3.toggle = 1.f; i3.sy = 1.f;
    for (int f = 0; f < 60; f++) ccView(&S, CC_VIEW_DEBUG, &i3, e, l, &fh);
    CHECK("cam_debug", fabsf(e[2] - 60.f) < .5f && fabsf(e[0]) < .5f, "debug: кнопка+стик вперёд — за 1 с вперёд на %.1f м по взгляду (ждём 60), в сторону %.2f", e[2], e[0]);
    ccResetState(&S); CcIn i4 = in; i4.t[0] = 0.f; i4.t[1] = 0.f; i4.t[2] = 0.f; for (int f = 0; f < 600; f++) ccView(&S, CC_VIEW_WRECK, &i4, e, l, &fh); float fnoCut = fh, hy = e[1];
    i4.cutEnter = 1; i4.cutPos[0] = 30.f; i4.cutPos[1] = 6.f; i4.cutPos[2] = 0.f; ccView(&S, CC_VIEW_WRECK, &i4, e, l, &fh); i4.cutEnter = 0; float eAfter = e[0];
    CHECK("cam_wreck", fabsf(hy - 10.f) < .1f && eAfter == 30.f && fnoCut < 1.58f && fnoCut >= .5f, "wreckless: без смены — на %.1f м над целью (ждём 10); после смены глаз x=%.1f (ждём 30); угол %.2f рад", hy, eAfter, fnoCut);
    ccResetState(&S); in.prevEye[0] = 40.f; in.prevEye[1] = 3.f; in.prevEye[2] = 0.f; for (int f = 0; f < 1500; f++) ccView(&S, CC_VIEW_COMEDY, &in, e, l, &fh);
    float dc = sqrtf((e[0]-l[0])*(e[0]-l[0]) + (e[1]-l[1])*(e[1]-l[1]) + (e[2]-l[2])*(e[2]-l[2]));
    CHECK("cam_comedy", fabsf(dc - 6.63f) < .1f && fabsf(l[1] - (in.t[1] + 1.f)) < .05f, "comedy: расстояние сходится к %.2f м (ждём 6,63), точка взгляда на 1 м выше цели (%.2f)", dc, l[1] - in.t[1]);
    { float ee[3] = {0, 0, 0}, ff[3] = {0, 0, 1}, rr[3] = {1, 0, 0}, uu[3] = {0, 1, 0}, o[3], pf[3] = {0, 0, 10}, pr[3] = {5, 0, 10}, pb2[3] = {0, 0, -5}; float th = tanf(.5f) * 16.f / 9.f, tv = tanf(.5f);
      int a1 = ccProject(ee, ff, rr, uu, th, tv, 480.f, 272.f, pf, o); float cx = o[0], cy = o[1]; int a2 = ccProject(ee, ff, rr, uu, th, tv, 480.f, 272.f, pr, o); float rx = o[0]; int a3 = ccProject(ee, ff, rr, uu, th, tv, 480.f, 272.f, pb2, o);
      CHECK("tree_project", a1 && fabsf(cx - 240.f) < .01f && fabsf(cy - 136.f) < .01f && a2 && rx > 240.f && !a3, "точка прямо впереди: экран %.1f %.1f (ждём 240 136), правее — x %.1f (> 240), за спиной видна: %d (ждём 0)", cx, cy, rx, a3); }
    CcCut cuts[2]; memset(cuts, 0, sizeof cuts); cuts[0].ntrig = 1; cuts[0].tr[0].shape = 0; cuts[0].tr[0].c[0] = 100.f; cuts[0].tr[0].h[0] = 5.f;
    cuts[1].ntrig = 1; cuts[1].tr[0].shape = 1; cuts[1].tr[0].c[0] = -50.f; cuts[1].tr[0].h[0] = 4.f; cuts[1].tr[0].h[1] = 2.f; cuts[1].tr[0].h[2] = 3.f; cuts[1].tr[0].ax[0] = 1.f; cuts[1].tr[0].ax[4] = 1.f; cuts[1].tr[0].ax[8] = 1.f;
    float pa[3] = {102.f, 0, 0}, pb[3] = {106.f, 0, 0}, pc[3] = {-53.f, 1.5f, 2.5f}, pd[3] = {-53.f, 2.5f, 0};
    CHECK("cam_cut_volumes", ccCutHit(cuts, 2, pa) == 0 && ccCutHit(cuts, 2, pb) == -1 && ccCutHit(cuts, 2, pc) == 1 && ccCutHit(cuts, 2, pd) == -1, "шар: внутри %d снаружи %d; коробка: внутри %d (выше высоты) %d (ждём 0, -1, 1, -1)", ccCutHit(cuts, 2, pa), ccCutHit(cuts, 2, pb), ccCutHit(cuts, 2, pc), ccCutHit(cuts, 2, pd)); }
  // ---- эффекты в игре ----
  if (argc >= 4 && !bwData(argv[1], argv[2], argv[3])) {
    float v1 = 0, v2 = 0;
    for (int k = 0; k < 2; k++) { drive(); gameCheatsSet(k ? 1u << CH_ACCEL : 0u); for (int f = 0; f < 120; f++) bwStep(&W, GP_R, 0, 0); (k ? &v2 : &v1)[0] = fabsf(W.car.v); }
    CHECK("effect_accel", v2 > v1 * 1.4f, "скорость за 2 с газа: без кода %.1f м/с, с кодом %.1f (ждём заметно больше, не меньше ×1,4)", v1, v2);
    float loss[2];
    for (int k = 0; k < 2; k++) { drive(); gameCheatsSet(k ? 1u << CH_NOTOP : 0u); float top = W.car.p.topSpeed; carSetSpeed(&W.car, top * 1.3f); float s0 = fabsf(W.car.v);
      for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0); loss[k] = s0 - fabsf(W.car.v); }
    CHECK("effect_notop", loss[0] > 1.f && loss[1] < loss[0] * .6f, "потеря скорости за 1 с накатом выше потолка: без кода %.2f м/с, с кодом %.2f (ждём меньше 60 %%)", loss[0], loss[1]);
    float dmg[2], other[2];
    for (int k = 0; k < 2; k++) { drive(); gameCheatsSet(k ? (1u << CH_INVINC) | (1u << CH_ONETAP) : 0u);
      Car o = W.car; o.huskDrive = 0; o.svPlayer = 0; o.damage = 0; o.destroyed = 0; o.deadT = 0;
      float fx = sinf(W.car.h), fz = cosf(W.car.h); o.x = W.car.x + fx * 4.2f; o.z = W.car.z + fz * 4.2f; o.h = W.car.h + 3.14159f; o.vel[0] = -fx * 6.f; o.vel[2] = -fz * 6.f;
      carSetSpeed(&W.car, 9.f); W.car.vel[0] = fx * 9.f; W.car.vel[2] = fz * 9.f; W.car.vel[1] = 0.f; o.vel[0] = 0.f; o.vel[2] = 0.f; float before = W.car.damage; int hit = 0;
      for (int f = 0; f < 4 && !hit; f++) hit = carHitCar(&W.car, &o, bwCol());
      dmg[k] = W.car.damage - before; other[k] = o.damage; (void)hit; if (getenv("DBG")) printf("DBG k=%d hit=%d dmg=%.4f other=%.4f noDmgT=%.2f hp=%.2f dist=%.2f\n", k, hit, W.car.damage, o.damage, W.car.noDmgT, W.car.p.hitPoints, hypotf(W.car.x - o.x, W.car.z - o.z)); }
    CHECK("effect_invincible", dmg[0] > .005f && dmg[1] == 0.f, "урон игроку при лобовом ударе: без кода %.3f, с кодом %.3f (ждём >0 и 0)", dmg[0], dmg[1]);
    float up[2];
    for (int k = 0; k < 2; k++) { drive(); gameCheatsSet(k ? 1u << CH_HORNJUMP : 0u); float y0 = W.car.y, ym = y0;
      for (int f = 0; f < 90; f++) { carHornLeap(&W.car, 1.f); bwStep(&W, 0, 0, 0); if (W.car.y - y0 > ym - y0) ym = W.car.y; } up[k] = ym - y0; }
    CHECK("effect_hornjump", up[0] < .5f && up[1] > 2.f, "подъём машины при зажатом гудке: без кода %.2f м, с кодом %.2f (ждём < 0,5 и > 2)", up[0], up[1]);
    CHECK("effect_onetap", other[0] < .5f && other[1] >= 1.f, "урон чужой машине: без кода %.2f, с кодом %.2f (ждём <0,5 и 1)", other[0], other[1]);
  } else printf("INFO данные уровня не даны — эффекты не проверялись\n");
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
