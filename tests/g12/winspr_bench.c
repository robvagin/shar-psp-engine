// Замер цены №1099: viewer/winspr.h с заглушками GE. Что проверяет: разбор файла (чужой отпечаток отвергается), «рисуется только дальше WINSPR_DIST», основной и дальний
// проходы делят клетки по farPlainD, не загруженная клетка и клетка вне пирамиды не рисуются, один вызов sceGuDrawArray на проход, куски текстур-окон дальней клетки пропускаются.
// cc -Iviewer tests/g12/winspr_test.c -lm -o /tmp/winspr_test && /tmp/winspr_test   (с -DWINSPR1099_OLD=1 обязан упасть)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
typedef struct { float x, y, z, w; } V4;
typedef struct { V4 x, y, z, w; } ScePspFMatrix4;
#define GU_MODEL 2
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
#include "cullhier.h"
static int smkOn; static float farPlainD = 100.f, drawDist = 200.f, eyeX, eyeY, eyeZ; static unsigned ncell; static Cell cl[4096]; static float pl[4][3];
static unsigned ggVbUsed; static unsigned char ggVbBuf[1 << 22]; static int curBias;
static int zfTwo = 1; static int zfBegin(void) { return zfTwo ? 2 : 1; } static int zfFarPiece(const Piece *d) { (void)d; return 0; }
static float boxD2(const float *b) { return chBoxD2(b, eyeX, eyeY, eyeZ); }
static void *ggVbAlloc(unsigned n) { void *p = ggVbBuf + ggVbUsed; ggVbUsed += n; return p; } static void ggVbWriteback(unsigned b) { (void)b; }
static void setCull(int on) { (void)on; } static void resetState(void) {} static unsigned sceKernelGetSystemTimeLow(void) { return 0; }
static int drawCalls, drawVerts; static float firstX[3000];
static void sceGuSetMatrix(int a, ScePspFMatrix4 *m) { (void)a; (void)m; } static void sceGuDisable(int a) { (void)a; } static void sceGuEnable(int a) { (void)a; }
static void sceGuDepthMask(int a) { (void)a; } static void sceGuDepthOffset(int a) { (void)a; }
static void sceGuDrawArray(int p, int f, int n, const void *i, const void *v) { (void)p; (void)f; (void)i; drawCalls++; drawVerts += n; for (int k = 0; k < n && k < 3000; k++) firstX[k] = ((const float *)v)[4 * k + 1]; }
static unsigned char *winTex; static unsigned winTexN;   // как в main_render.h (там объявлены раньше winspr.h)
#define WINSPR_SKIP(d) (winTexN && (unsigned)(d)->r.tex < winTexN && winTex[(d)->r.tex] && zfBegin() == 2 && zfFarPiece(d))
#include "winspr.h"

#include <time.h>
int main(int argc, char **argv) {
  FILE *g = fopen(argv[1], "rb"); static unsigned char b[1 << 20]; size_t n = fread(b, 1, sizeof b, g); fclose(g);
  FILE *l = fopen(argv[2], "rb"); unsigned hd[26]; fread(hd, 4, 26, l); ncell = hd[20]; fseek(l, hd[21], SEEK_SET);
  for (unsigned i = 0; i < ncell && i < 4096; i++) { unsigned char row[48]; fread(row, 1, 48, l); memcpy(cl[i].bb, row, 24); cl[i].state = 2; } fclose(l);
  if (!winsprParse(b, (unsigned)n, atoi(argv[3]), hd[1], hd[2], hd[20])) { printf("файл не принят\n"); return 1; }
  srand(5); unsigned N = 400, tot = 0, mx = 0, calls = 0; double us = 0; unsigned hist[8] = {0};
  for (unsigned k = 0; k < N; k++) {   // случайные глаза у окон (клетки с окнами), случайный курс
    const WspCell *c = &winCell[rand() % winNCell]; const WspTri *t = &winTri[c->first];
    eyeX = t->v[0] + (rand() % 120 - 60); eyeY = t->v[1] + 6; eyeZ = t->v[2] + (rand() % 120 - 60);
    float yaw = (rand() % 628) / 100.f, cs = cosf(yaw), sn = sinf(yaw), th = 0.9f;   // полуугол по горизонту ~ 52°
    // боковые плоскости (внутренние нормали): влево/вправо от курса; верх/низ не режем (как у города — 4 боковых)
    float fx = sn, fz = cs, rx = cs, rz = -sn;
    float nl[2] = {fx * sinf(th) + rx * cosf(th), fz * sinf(th) + rz * cosf(th)}, nr[2] = {fx * sinf(th) - rx * cosf(th), fz * sinf(th) - rz * cosf(th)};
    pl[0][0] = nl[0]; pl[0][1] = 0; pl[0][2] = nl[1]; pl[1][0] = nr[0]; pl[1][1] = 0; pl[1][2] = nr[1]; pl[2][0] = fx; pl[2][1] = 0; pl[2][2] = fz; pl[3][0] = fx; pl[3][1] = 0; pl[3][2] = fz;
    drawCalls = drawVerts = 0; ggVbUsed = 0; struct timespec a, d; clock_gettime(CLOCK_MONOTONIC, &a); winsprFlush(0, 0); winsprFlush(1, 0); clock_gettime(CLOCK_MONOTONIC, &d);
    us += (d.tv_sec - a.tv_sec) * 1e6 + (d.tv_nsec - a.tv_nsec) / 1e3; unsigned tr = drawVerts / 3; tot += tr; if (tr > mx) mx = tr; calls += drawCalls; hist[tr * 8 / (WINSPR_MAXTRI + 1)]++;
  }
  printf("кадров %u: треугольников на кадр среднее %.0f, максимум %u (предел %d), вызовов на кадр %.2f, Mac %.1f мкс на кадр (PSP ×5,8 ≈ %.2f мс)\n", N, (double)tot / N, mx, WINSPR_MAXTRI, (double)calls / N, us / N, us / N * 5.8 / 1000);
  printf("гистограмма (доли предела): "); for (int i = 0; i < 8; i++) printf("%u ", hist[i]); printf("\n"); return 0;
}
