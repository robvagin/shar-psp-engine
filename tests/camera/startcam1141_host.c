// №1141 (TEST84, L5, старт миссии: камера сразу проходит сквозь вывеску дома). Движковые viewer/camgrid.h + camsafe.c на Маке.
// Сцена: большая клетка (3000 лишних треугольников стены, сетка строится десятки кадров), в ней вывеска (вертикальная пластина 5 × 3 м, x = -3) между опорой (Гомер, x = 0)
// и желаемым глазом камеры (x = -7): после переноса героя «клетка только что загружена» — сетка пуста.
// ПРЕЖНЕЕ (TEST84, STARTCAM1141_OLD=1): за кадр строится сетка не больше CG_US2 мкс — первые кадры луч опора → глаз вывеску не видит (глаз за вывеской = камера проходит сквозь неё).
// НОВОЕ: перенос → cgBuildNear достраивает клетки у глаза и опоры сразу, список клеток луча обновляется при достроенной клетке: вывеска видна с самого первого кадра.
//   cc -std=c11 -I viewer tests/camera/startcam1141_host.c viewer/camsafe.c viewer/col.c -lm   (красный: -DSTARTCAM1141_OLD=1 обязан не пройти)
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
  static Cell C; memset(&C, 0, sizeof C); C.state = 2; C.bb[0] = -20; C.bb[1] = -1; C.bb[2] = -35; C.bb[3] = 32; C.bb[4] = 8; C.bb[5] = 35; C.cx = 6; C.cz = 0; C.first = 0; C.n = 1;
  cl = &C; ncell = 1;
  static CsScene s; memset(&s, 0, sizeof s); s.geomRay = cgRayFracCam;
  float Pv[3] = {0, 1.8f, 0}, E[3] = {-7, 3.f, 0}, eyeStart[3] = {-7, 3.f, 0};
  int blind = 0, firstSeen = -1;
  for (int fr = 0; fr < 10; fr++) {
    cgFrame++;   // как cgStep: счётчик кадров кэша лучей
    if (!STARTCAM1141_OLD) { cgBuildNear(eyeStart, Pv, 16.f, 6); }   // то, что camGeomCollect делает в окне старта
    cgStep(eyeStart[0], eyeStart[2]);
    float f = csRayFrac(&s, Pv, E, 13);   // луч опора → глаз (маска как CS_PE): вывеска между ними
    if (f > .999f) blind++; else if (firstSeen < 0) firstSeen = fr;
  }
  printf("первые 10 кадров после переноса: вывеска не видна в %d кадрах, впервые видна в кадре %d (сетка клетки %s)\n", blind, firstSeen, C.cgs == 2 ? "достроена" : "ещё строится");
  int bad = 0;
#if STARTCAM1141_OLD
  puts("(режим STARTCAM1141_OLD=1: прежнее; ждём слепые кадры)"); if (blind == 0) bad++;
#else
  if (blind != 0 || firstSeen != 0) bad++;
#endif
  // обычная езда: клетка без переноса достраивается порционно (цена кадра прежняя): после одного обычного cgStep сетка ещё не готова
  { static Cell D; memset(&D, 0, sizeof D); D.state = 2; memcpy(D.bb, C.bb, sizeof C.bb); D.first = 0; D.n = 1; cl = &D; ncell = 1; cgGen++;
    cgStep(0, 0); printf("обычный кадр без переноса: сетка %s (ждём «ещё строится» — порции прежние)\n", D.cgs == 2 ? "достроена" : "ещё строится"); if (D.cgs == 2) bad++; }
  puts(bad ? "FAIL" : "PASS"); return bad;
}
