// №1141 (TEST84, L5, старт миссии: камера сразу проходит сквозь вывеску дома). Движковые viewer/camgrid.h + camsafe.c на Маке.
// Сцена: большая клетка (3000 лишних треугольников стены, сетка строится десятки кадров), в ней вывеска (вертикальная пластина 5 × 3 м, x = -3) между опорой (Гомер, x = 0)
// и желаемым глазом камеры (x = -7): после переноса героя «клетка только что загружена» — сетка пуста.
// ПРЕЖНЕЕ (TEST84, STARTCAM1141_OLD=1): за кадр строится сетка не больше CG_US2 мкс — первые кадры луч опора → глаз вывеску не видят (глаз за вывеской = камера проходит сквозь неё).
// НОВОЕ: перенос / первый вызов / загрузка уровня взводят окно (camStartWindow из camgrid.h — тот же код, что зовёт игра из camGeomCollect): клетки у глаза и опоры достраиваются сразу.
// Условия записаны заранее и одинаковы для любого режима; тест сам не знает, какой режим собран, и в прежнем режиме ОБЯЗАН падать (красный контроль — tests/camera/startcam1141_run.sh):
//   1) первый вызов, 2) перенос опоры > 6 м, 3) загрузка уровня (поколение уровня без переноса): вывеска видна с самого первого кадра;
//   4) езда (поколение клеток города растёт при выгрузках, опора ползёт): окно НЕ взводится (№1159, g10b) — ни одной достройки окном;
//   5) №1159 (g12) перебор раз в 4 вызова ничего не теряет: клетка, догруженная посреди окна, достроена не позже чем через 4 вызова; окно кончается (≤ 120 вызовов) и дальше молчит.
//   cc -std=c11 -I viewer tests/camera/startcam1141_host.c viewer/camsafe.c viewer/col.c -lm   (прежние режимы: -DSTARTCAM1141_OLD=1, -DCAMSTARTGEN1159_OLD=1 обязаны не пройти)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define F_V12 1
#define F_NOIDX 2
#define F_STRIP 4
typedef struct { int owner, flags, ni; float sc, ox, oy, oz; } Rec;
typedef struct { Rec r; const void *v; const unsigned *idx; } Piece;
typedef struct { int state; float bb[6], cx, cz; unsigned first, n; void *cg; unsigned char cgn[3], cgs; unsigned short cgk; int cgi; unsigned cgbytes; } Cell;
static Cell *cl; static unsigned ncell; static Piece *pc; static unsigned npc;
static unsigned fakeT; static unsigned sceKernelGetSystemTimeLow(void) { return fakeT += 1; }
static int smkOn; static void smkWrite(const char *s) { (void)s; }
static const void *pieceListT(const Piece *d, int slot, unsigned *n) { (void)d; (void)slot; *n = 0; return 0; }
#include "camgrid.h"
#include "camsafe.h"
#define NT 3002
int main(void) {
  static unsigned char vb[NT * 3 * 16]; static unsigned idx[NT * 3];
  float sc = 128.f, k = 32768.f / sc;
  int nv = 0;
  // вывеска: два треугольника в плоскости x = -3
  float S[6][3] = { {-3,1,-2.5f},{-3,4,-2.5f},{-3,1,2.5f},  {-3,4,-2.5f},{-3,4,2.5f},{-3,1,2.5f} };
  for (int i = 0; i < 6; i++, nv++) { short *xy = (short *)(vb + nv * 16 + 8); for (int j = 0; j < 3; j++) xy[j] = (short)lroundf(S[i][j] * k); idx[nv] = nv; }
  // лишние треугольники — стена далеко (x = 30), много мелких: сетка строится десятки кадров
  for (int t = 0; t < 3000; t++) { float z0 = -30.f + (t % 60) * 1.0f, y0 = (t / 60) * 0.12f; float V[3][3] = { {30,y0,z0},{30,y0+.5f,z0},{30,y0,z0+.8f} };
    for (int j = 0; j < 3; j++, nv++) { short *xy = (short *)(vb + nv * 16 + 8); for (int q = 0; q < 3; q++) xy[q] = (short)lroundf(V[j][q] * k); idx[nv] = nv; } }
  Piece P = {{0, 0, nv, sc, 0, 0, 0}, vb, idx}; pc = &P; npc = 1;
  static CsScene s; memset(&s, 0, sizeof s); s.geomRay = cgRayFracCam;
  float Pv[3] = {0, 1.8f, 0}, Pfar[3] = {500.f, 1.8f, 500.f}, E[3] = {-7, 3.f, 0};
  static Cell C[2]; int bad = 0;
#define FRESH() do { memset(C, 0, sizeof C); for (int q_ = 0; q_ < 2; q_++) { C[q_].state = 2; C[q_].bb[0] = -20; C[q_].bb[1] = -1; C[q_].bb[2] = -35; C[q_].bb[3] = 32; C[q_].bb[4] = 8; C[q_].bb[5] = 35; C[q_].cx = 6; C[q_].first = 0; C[q_].n = 1; } C[1].state = 0; cl = C; ncell = 2; cgGen++; cgFrame += 20; } while (0)
  // десять кадров: сбор сцены камеры (окно старта) + обычный шаг сетки + луч опора → глаз (маска как CS_PE); возвращает число кадров, где вывеска «не видна»
  #define FRAMES10(P, blind, first) do { blind = 0; first = -1; for (int fr_ = 0; fr_ < 10; fr_++) { cgFrame++; camStartWindow(E, P); cgStep(E[0], E[2]); if (csRayFrac(&s, Pv, E, 13) > .999f) blind++; else if (first < 0) first = fr_; } } while (0)
  // выжечь окно: 130 вызовов на месте (клеток рядом нет: опора далеко)
  #define EXHAUST(P) do { for (int k_ = 0; k_ < 130; k_++) { cgFrame++; camStartWindow(E, P); } } while (0)
  int blind, first;
  // 1) первый вызов (загрузка уровня)
  FRESH(); FRAMES10(Pv, blind, first);
  printf("1 первый вызов: вывеска не видна в %d из 10 кадров, впервые видна в кадре %d (сетка %s)\n", blind, first, C[0].cgs == 2 ? "достроена" : "ещё строится");
  if (blind != 0 || first != 0) { puts("  FAIL 1: вывеска не видна с первого кадра"); bad++; }
  // 2) перенос опоры > 6 м (старт миссии, сброс машины)
  EXHAUST(Pfar); FRESH(); FRAMES10(Pv, blind, first);
  printf("2 перенос на 700 м: не видна в %d кадрах, впервые в кадре %d\n", blind, first);
  if (blind != 0 || first != 0) { puts("  FAIL 2: после переноса вывеска не видна с первого кадра"); bad++; }
  // 3) загрузка уровня без переноса: опора на месте, меняется только поколение уровня
  EXHAUST(Pv); FRESH(); camLevelGen++; FRAMES10(Pv, blind, first);
  printf("3 загрузка уровня на месте: не видна в %d кадрах, впервые в кадре %d\n", blind, first);
  if (blind != 0 || first != 0) { puts("  FAIL 3: после загрузки уровня вывеска не видна с первого кадра"); bad++; }
  // 4) езда: клетки города выгружаются (cgGen растёт), опора ползёт 0,3 м за вызов — окно не взводится, свежая клетка окном не достраивается
  EXHAUST(Pv); FRESH();
  { unsigned b0 = camStartBuilt; float Pm[3] = {0, 1.8f, 0};
    for (int k = 0; k < 60; k++) { Pm[2] = .3f * k; cgGen++; cgFrame++; camStartWindow(E, Pm); }
    printf("4 езда (cgGen растёт, опора ползёт): окно достроило %u клеток (ждём 0)\n", camStartBuilt - b0);
    if (camStartBuilt != b0) { puts("  FAIL 4: окно взводится от выгрузки клеток города или от малого сдвига (№1159)"); bad++; } }
  // 5) окно идёт, рядом всё достроено; клетка догружается посреди окна в любой из 8 фаз счёта: перебор раз в 4 вызова обязан достроить её не позже чем через 4 вызова; потом окно кончается
  { int worst = 0, lost = 0; unsigned b0 = 0;
    for (int ph = 0; ph < 8; ph++) {
      EXHAUST(Pfar); FRESH(); b0 = camStartBuilt; cgFrame++; camStartWindow(E, Pv); if (C[0].cgs != 2) { puts("  FAIL 5: клетка у опоры не достроена первым вызовом"); bad++; }
      for (int k = 0; k < 5 + ph; k++) { cgFrame++; camStartWindow(E, Pv); }   // ничего нового: перебор редкий
      C[1].state = 2; int lat = -1;   // догруженная клетка (теперь «вокруг» опоры)
      for (int k = 1; k <= 20; k++) { cgFrame++; camStartWindow(E, Pv); if (C[1].cgs == 2 && lat < 0) lat = k; }
      if (lat < 1) lost++; else if (lat > worst) worst = lat;
    }
    printf("5 клетка, догруженная посреди окна (8 фаз): худшее ожидание %d вызовов (ждём ≤ 4), потеряно %d из 8\n", worst, lost);
    if (lost || worst > 4) { puts("  FAIL 5: догруженная клетка достроена позже чем через 4 вызова (перебор теряет её)"); bad++; }
    EXHAUST(Pv); FRESH(); b0 = camStartBuilt; C[1].state = 2; for (int k = 0; k < 20; k++) { cgFrame++; camStartWindow(E, Pv); }
    if (camStartBuilt != b0) { puts("  FAIL 5: окно не кончается после CAM_START_WIN вызовов"); bad++; } }
  // 6) аудит 04.11 (кадр после переноса на PSP 412 мс): рядом пять недостроенных клеток; ни один вызов окна не достраивает больше одной клетки, первой строится клетка под глазом (вывеска видна с первого кадра), все пять готовы не позже чем через 8 вызовов
  { static Cell C5[5]; EXHAUST(Pfar); memset(C5, 0, sizeof C5);
    for (int q_ = 0; q_ < 5; q_++) { C5[q_].state = 2; C5[q_].bb[0] = -20 + (q_ * 2); C5[q_].bb[1] = -1; C5[q_].bb[2] = -35; C5[q_].bb[3] = 32 + (q_ * 2); C5[q_].bb[4] = 8; C5[q_].bb[5] = 35; C5[q_].first = 0; C5[q_].n = 1; }
    cl = C5; ncell = 5; cgGen++; cgFrame += 20; int mx = 0, all = -1;
    for (int k = 1; k <= 12; k++) { unsigned b0 = camStartBuilt; cgFrame++; camStartWindow(E, Pv); int d = (int)(camStartBuilt - b0); if (d > mx) mx = d; if (k == 1 && C5[0].cgs != 2) { puts("  FAIL 6: клетка под глазом не достроена первым вызовом"); bad++; }
      int n2 = 0; for (int q_ = 0; q_ < 5; q_++) n2 += C5[q_].cgs == 2; if (n2 == 5 && all < 0) all = k; }
    printf("6 пять клеток рядом: больше всего за один вызов %d (ждём ≤ 1), все готовы к вызову %d (ждём ≤ 8)\n", mx, all);
    if (mx > 1) { puts("  FAIL 6: вызов окна достраивает несколько клеток сразу (длинный кадр после переноса)"); bad++; }
    if (all < 0 || all > 8) { puts("  FAIL 6: клетки у глаза не достроены за 8 вызовов"); bad++; }
    cl = C; ncell = 2; }
  // 7) порядок: клетка, через которую идёт луч опора → глаз (стена между героем и камерой, x от -4,5 до -2,5), строится раньше клеток у самого глаза, даже если те ближе к концам отрезка; иначе на первые кадры луч видит её пустой
  { static Cell C7[4]; EXHAUST(Pfar); memset(C7, 0, sizeof C7);
    float X0[4] = {-9.5f, -9.5f, -9.5f, -4.5f}, X1[4] = {-8.5f, -8.5f, -8.5f, -2.5f};   // три клетки у глаза (до него 1,5 м), клетка луча последняя в списке
    for (int q_ = 0; q_ < 4; q_++) { C7[q_].state = 2; C7[q_].bb[0] = X0[q_]; C7[q_].bb[1] = -1; C7[q_].bb[2] = -2; C7[q_].bb[3] = X1[q_]; C7[q_].bb[4] = 8; C7[q_].bb[5] = 2; C7[q_].first = 0; C7[q_].n = 1; }
    cl = C7; ncell = 4; cgGen++; cgFrame += 20; cgFrame++; camStartWindow(E, Pv);
    printf("7 клетка луча опора → глаз последняя в списке: после первого вызова окна %s\n", C7[3].cgs == 2 ? "достроена" : "НЕ достроена");
    if (C7[3].cgs != 2) { puts("  FAIL 7: первой строится не клетка луча (стена между героем и камерой пуста для луча)"); bad++; }
    cl = C; ncell = 2; }
  // обычная езда: клетка без окна достраивается порционно (цена кадра прежняя): после одного обычного cgStep сетка ещё не готова
  { FRESH(); cgStep(0, 0); printf("обычный кадр без переноса: сетка %s (ждём «ещё строится» — порции прежние)\n", C[0].cgs == 2 ? "достроена" : "ещё строится"); if (C[0].cgs == 2) { puts("  FAIL: обычный кадр достроил сетку целиком (цена кадра выросла)"); bad++; } }
  puts(bad ? "FAIL" : "PASS"); return bad;
}
