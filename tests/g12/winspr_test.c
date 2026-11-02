// Хост-тест №1099: viewer/winspr.h с заглушками GE. Что проверяет: разбор файла (чужой отпечаток отвергается), «рисуется только дальше WINSPR_DIST», основной и дальний
// проходы делят клетки по farPlainD, не загруженная клетка и клетка вне пирамиды не рисуются, один вызов sceGuDrawArray на проход, куски текстур-окон дальней клетки пропускаются.
// cc -Iviewer -DWINFLAT1155_OLD=1 tests/g12/winspr_test.c (с №1155 спрайты выключены по умолчанию, тест включает их) -lm -o /tmp/winspr_test && /tmp/winspr_test   (с -DWINSPR1099_OLD=1 обязан упасть)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
typedef struct { float x, y, z, w; } V4; typedef struct { V4 x, y, z, w; } ScePspFMatrix4;
#define GU_MODEL 2
#define GU_VIEW 1
#define ZF932_NEAR_K .85f
#define GU_TEXTURE_2D 1
#define GU_ALPHA_TEST 2
#define GU_BLEND 3
#define GU_FOG 4
#define GU_TRUE 1
#define GU_TRIANGLES 3
#define GU_COLOR_8888 1
#define GU_VERTEX_32BITF 2
#define GU_TRANSFORM_3D 4
typedef struct { float bb[6]; int state; } Cell; typedef struct { struct { int tex; unsigned cell; } r; } Piece;
static int smkOn; static float farPlainD = 100.f, drawDist = 200.f, eyeX, eyeY, eyeZ; static unsigned ncell = 4; static Cell cl[4]; static float pl[4][3];
static unsigned ggVbUsed; static unsigned char ggVbBuf[1 << 20]; static int curBias;
static int zfTwo = 1; static int zfBegin(void) { return zfTwo ? 2 : 1; } static int zfFarPiece(const Piece *d) { return d->r.cell < ncell && cl[d->r.cell].bb[0] > 100.f; }
static float boxD2(const float *b) { float dx = b[0] > eyeX ? b[0] - eyeX : (eyeX > b[3] ? eyeX - b[3] : 0), dz = b[2] > eyeZ ? b[2] - eyeZ : (eyeZ > b[5] ? eyeZ - b[5] : 0), dy = 0; return dx * dx + dy * dy + dz * dz; }
static void *ggVbAlloc(unsigned n) { void *p = ggVbBuf + ggVbUsed; ggVbUsed += n; return p; } static void ggVbWriteback(unsigned b) { (void)b; }
static void setCull(int on) { (void)on; } static void resetState(void) {} static unsigned sceKernelGetSystemTimeLow(void) { return 0; }
static int drawCalls, drawVerts; static float firstX[3000], allZ[3000], firstY, firstZ, viewAtDraw[3];
static float zfNear = 1.5f; static float viewW[3]; static int viewCalls;   // №1125: перенос вида в момент рисования
static void sceGumMatrixMode(int m) { (void)m; } static void sceGumStoreMatrix(ScePspFMatrix4 *m) { memset(m, 0, sizeof *m); m->x.x = m->y.y = m->z.z = 1.f; m->w.x = 5.f; m->w.y = 6.f; m->w.z = 7.f; m->w.w = 1.f; }
static void sceGuSetMatrix(int a, ScePspFMatrix4 *m) { if (a == GU_VIEW) { viewW[0] = m->w.x; viewW[1] = m->w.y; viewW[2] = m->w.z; viewCalls++; } } static void sceGuDisable(int a) { (void)a; } static void sceGuEnable(int a) { (void)a; }
static void sceGuDepthMask(int a) { (void)a; } static void sceGuDepthOffset(int a) { (void)a; }
static void sceGuDrawArray(int p, int f, int n, const void *i, const void *v) { (void)p; (void)f; (void)i; drawCalls++; drawVerts += n; firstY = ((const float *)v)[2]; firstZ = ((const float *)v)[3]; memcpy(viewAtDraw, viewW, sizeof viewW); for (int k = 0; k < n && k < 3000; k++) { firstX[k] = ((const float *)v)[4 * k + 1]; allZ[k] = ((const float *)v)[4 * k + 3]; } }
#include "cullhier.h"
static unsigned char *winTex; static unsigned winTexN;   // как в main_render.h (там объявлены раньше winspr.h)
#define WINSPR_SKIP(d) (winTexN && (unsigned)(d)->r.tex < winTexN && winTex[(d)->r.tex] && zfBegin() == 2 && zfFarPiece(d))
#include "winspr.h"
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("ПАДЕНИЕ: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
static unsigned char file[4096]; static unsigned flen;
static void put(unsigned off, const void *p, unsigned n) { memcpy(file + off, p, n); }
static void tri(unsigned *o, float x, unsigned c) { float t[9] = {x, 3, 0, x + 1, 3, 0, x, 4, 0}; put(*o, t, 36); put(*o + 36, &c, 4); *o += 40; }
int main(void) {
  // клетки: 0 — x 20..40 (ближняя), 1 — x 60..80 (ближняя), 2 — x 120..140 (дальняя), 3 — x 160..180 (дальняя, не загружена)
  for (int i = 0; i < 4; i++) { cl[i].bb[0] = 20.f + 40 * i + (i >= 2 ? 20.f * 0 : 0); cl[i].bb[1] = 0; cl[i].bb[2] = -10; cl[i].bb[3] = cl[i].bb[0] + 20; cl[i].bb[4] = 10; cl[i].bb[5] = 10; cl[i].state = 2; }
  cl[2].bb[0] = 120; cl[2].bb[3] = 140; cl[3].bb[0] = 160; cl[3].bb[3] = 180; cl[3].state = 0;
  // пирамида: смотрим по +x; плоскости пропускают всё впереди (вес по x)
  for (int i = 0; i < 4; i++) { pl[i][0] = 1.f; pl[i][1] = pl[i][2] = 0.f; }
  unsigned h[12] = {0x31505357u, 1, 4, 100, 50, 4, 4, 48, 0, 0, 0, 0}; memcpy(file, h, 48);
  unsigned o = 48 + 12 * 4; unsigned cellrec[4][3]; unsigned first = 0; (void)cellrec;
  // клетка 0: треугольник на x=25 (до 50 м от глаза x=0 — ближе порога) и x=35 (ближе порога); клетка 1: на x=65 (дальше 50 м); клетка 2: на x=125; клетка 3: на x=165 (не загружена)
  unsigned recs[4][3] = {{0, 0, 2}, {1, 2, 1}, {2, 3, 1}, {3, 4, 1}}; unsigned to = o;
  tri(&o, 25.f, 0xff0000ffu); tri(&o, 35.f, 0xff0000ffu); tri(&o, 65.f, 0xff00ff00u); tri(&o, 125.f, 0xffff0000u); tri(&o, 165.f, 0xffffffffu);
  put(48, recs, sizeof recs); unsigned nt = 5, oc = 48, ot = to, tx = o; int texid = 7; put(o, &texid, 4); o += 4; flen = o;
  unsigned hh[12] = {0x31505357u, 1, 4, 100, 50, 4, 4, oc, nt, ot, 1, tx}; put(0, hh, 48);
  CHECK(winsprParse(file, flen, 4, 100, 50, 4) == 1 && winNCell == 4, "верный файл не принят");
  CHECK(winsprParse(file, flen, 3, 100, 50, 4) == 0, "файл другого уровня принят");
  CHECK(winsprParse(file, flen, 4, 101, 50, 4) == 0, "файл с чужим числом текстур принят");
  CHECK(winsprParse(file, flen, 4, 100, 50, 4) == 1 && winTexN == 100 && winTex[7] && !winTex[8], "таблица текстур-окон");
  Piece pcs = {{7, 2}}; (void)pcs;
  CHECK(WINSPR_SKIP(&pcs), "кусок окна дальней клетки не пропускается"); Piece pn = {{7, 0}}; CHECK(!WINSPR_SKIP(&pn), "кусок окна ближней клетки пропускается"); Piece po = {{8, 2}}; CHECK(!WINSPR_SKIP(&po), "кусок другой текстуры пропускается");
  eyeX = 0; eyeY = 0; eyeZ = 10.f; drawCalls = drawVerts = 0; ggVbUsed = 0;   // глаз перед окнами (нормаль окон +z)
  winsprFlush(0, 0);   // основной проход: ближние клетки 0 и 1, дальше 50 м от глаза — только x=65
  CHECK(drawCalls == 1 && drawVerts == 3 && firstX[0] == 65.f, "основной проход: вызовов %d вершин %d (нужно 1 и 3: только окно на 65 м)", drawCalls, drawVerts);
  // №1125: вершины в осях камеры (v − глаз), вынос к камере по нормали на шаги глубины, вид без переноса при рисовании, после основного прохода вид возвращён
  CHECK(fabsf(firstY - 3.f) < 1e-4f && firstZ > -10.f + 0.12f && firstZ < -10.f + 0.30f, "вершина не в осях камеры или без выноса: y=%.3f z=%.3f (нужно y=3, z≈-10+0,2)", firstY, firstZ);
  CHECK(viewAtDraw[0] == 0.f && viewAtDraw[1] == 0.f && viewAtDraw[2] == 0.f, "при рисовании вид с переносом %.1f %.1f %.1f", viewAtDraw[0], viewAtDraw[1], viewAtDraw[2]);
  CHECK(viewW[0] == 5.f && viewW[2] == 7.f, "после основного прохода вид не возвращён к абсолютному: %.1f %.1f", viewW[0], viewW[2]);
  float eps65 = firstZ + 10.f; eyeZ = 40.f; ggVbUsed = 0; drawCalls = 0; winsprFlush(0, 0); float eps76 = -1.f;   // дальше от стены — вынос больше (шаг глубины растёт с квадратом расстояния)
  for (int k = 0; k < drawVerts && k < 3000; k++) if (fabsf(firstX[k] - 65.f) < 1e-3f) { eps76 = allZ[k] + 40.f; break; }
  CHECK(drawCalls == 1 && eps76 > eps65 * 1.15f, "вынос не растёт с расстоянием: %.3f -> %.3f", eps65, eps76);
  eyeZ = -10.f; ggVbUsed = 0; drawCalls = drawVerts = 0; winsprFlush(0, 0); CHECK(drawCalls == 0, "окно, обращённое от камеры, рисуется (%d)", drawCalls);
  eyeZ = 10.f;
  drawCalls = drawVerts = 0; ggVbUsed = 0; winsprFlush(1, 0);   // дальний: клетка 2 (x=125), клетка 3 не загружена
  CHECK(viewW[0] == 0.f && viewW[2] == 0.f, "после дальнего прохода вид не оставлен без переноса: %.1f %.1f", viewW[0], viewW[2]);   // №1125
  CHECK(drawCalls == 1 && drawVerts == 3, "дальний проход: вызовов %d вершин %d (нужно 1 и 3)", drawCalls, drawVerts);
  drawCalls = drawVerts = 0; winsprFlush(0, 1); CHECK(drawCalls == 0, "в комнате рисуется");
  eyeX = 300.f; drawCalls = 0; winsprFlush(0, 0); winsprFlush(1, 0); CHECK(drawCalls == 0, "клетки за спиной рисуются");
  eyeX = 0; for (int i = 0; i < 4; i++) pl[i][0] = 1.f; zfTwo = 0; drawCalls = drawVerts = 0; ggVbUsed = 0; winsprFlush(0, 0);
  CHECK(drawCalls == 1 && drawVerts == 6, "без дальнего прохода: вызовов %d вершин %d (нужно 1 и 6: окна на 65 и 125 м)", drawCalls, drawVerts);
  printf(bad ? "КРАСНЫЙ: %d\n" : "ЗЕЛЁНЫЙ\n", bad); return bad != 0;
}
