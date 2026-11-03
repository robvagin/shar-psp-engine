// №1159: окно старта камеры (№1141) взводится только первым вызовом, загрузкой уровня и переносом опоры; выгрузка клеток города (езда) его не взводит.
// Тест вызывает те же функции камеры (viewer/camlaw.h), что и игра (main_camera.h camGeomCollect), и сверяет подключение в игре по исходнику.
//   cc -std=c11 -I viewer tests/camera/startarm1159_host.c -lm   (красный: -DCAMSTARTGEN1159_OLD=1 обязан не пройти)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "camlaw.h"
#ifndef CAMSTARTGEN1159_OLD
#define CAMSTARTGEN1159_OLD 0
#endif
static int fails;
static void check(const char *n, int ok, const char *fmt, int a, int b) { printf("CHECK %-28s %s  ", n, ok ? "OK" : "FAIL"); printf(fmt, a, b); printf("\n"); if (!ok) fails++; }
// 10 с езды при 30 кадрах: опора идёт 0,5 м за кадр, клетка города выгружается раз в 7 кадров, уровень один
static int armsDriving(void) {
  unsigned cellGen = 1, levelGen = 1, last = 0; int have = 0, arms = 0; float p = 0.f, lp = 0.f;
  for (int f = 0; f < 300; f++) { p += .5f; if (f % 7 == 0) cellGen++;
    unsigned g = camStartGenSel(cellGen, levelGen, CAMSTARTGEN1159_OLD);
    if (camStartArm(have, last, g, fabsf(p - lp), 6.f)) arms++;
    lp = p; have = 1; last = g; }
  return arms;
}
int main(void) {
  int a = armsDriving();
  check("start.driving_arms_once", a == 1, "взведено %d раз за 300 кадров езды (ждём 1: первый вызов)", a, 0);
  // загрузка уровня и перенос опоры по-прежнему взводят
  unsigned g0 = camStartGenSel(5, 1, CAMSTARTGEN1159_OLD), g1 = camStartGenSel(9, 2, CAMSTARTGEN1159_OLD);
  int lvl = camStartArm(1, g0, g1, 0.f, 6.f), jump = camStartArm(1, g0, g0, 40.f, 6.f), none = camStartArm(1, g0, g0, 5.9f, 6.f), first = camStartArm(0, 0, g0, 0.f, 6.f);
  check("start.level_jump_first", lvl && jump && !none && first, "новый уровень %d, перенос 40 м %d (нужно 1/1), 5,9 м и тот же уровень — нет, первый вызов — да", lvl, jump);
  // подключение в игре
  char buf[1 << 16]; int ok = 0; FILE *f;
  f = fopen("viewer/main_camera.h", "r"); size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0; if (f) fclose(f); buf[n] = 0;
  int use = strstr(buf, "camStartGenSel(cgGen, camLevelGen, CAMSTARTGEN1159_OLD)") && strstr(buf, "camStartArm(lpOk, lgen, gen_");
  f = fopen("viewer/main_levels.h", "r"); n = f ? fread(buf, 1, sizeof buf - 1, f) : 0; if (f) fclose(f); buf[n] = 0;
  int lv = strstr(buf, "cgGen++; camLevelGen++;") != 0;
  ok = use && lv; check("start.wired_in_game", ok, "main_camera.h зовёт правило %d, main_levels.h растит поколение уровня %d", use != 0, lv);
  printf("RESULT %s %d\n", fails ? "FAIL" : "OK", fails); return fails ? 1 : 0;
}
