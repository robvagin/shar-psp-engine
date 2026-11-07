// Стенд удельных цен подачи в GE (РОВНЫЕ_30 шаг 0, 04.11.2026): сколько стоит на PSP вызов ядра, который sceGuDrawArray и sceGuCallList делают в режиме GU_DIRECT (сдвиг стоп-адреса списка),
// смена матрицы, привязка текстуры с палитрой, смена состояния. Служебное (только сборка тура со смоуком): «flag gebench 1» в сценарии, одна сцена ≤ 20 с (3–5 с на железе).
// Строки GEBENCH в smoke.txt (мкс Мака — только проверка, что стенд работает: цены читать с PSP, поле emu=0).
// Как меряется: тело из N=1000 одинаковых вызовов, 5 прогонов, медиана и минимум по времени процессора (cpu_us — только выдача команд, пока GE работает параллельно; total_us — до конца исполнения GE).
//  direct — вызовы пишутся в открытый список GU_DIRECT (у DrawArray и CallList на каждый вызов — ядро: сдвиг стоп-адреса);
//  call   — те же вызовы записываются в список GU_CALL (ядра нет, команды просто ложатся в память), потом он вызывается одним sceGuCallList.
// Цена вызова ядра на один DrawArray = per_ns(draw direct) − per_ns(draw call). Нет вложенных списков: списки GU_CALL строятся до открытия GU_DIRECT.
// Состояние GU после стенда возвращается (sceGuSetAllStatus), кэши движка сбрасывает вызывающий (resetState). Данные — нули/степень двойки, на экран ничего не рисуется (вырожденный треугольник).
#ifndef GEBENCH1_H
#define GEBENCH1_H
#include "release.h"
#if !SHR_REL_SMK && defined(__psp__)
#include <pspkernel.h>
#include <pspgu.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#define GB_N 1000
// №1216 (TEST92, тур на PSP: STALL 5000ms phase=input, user_main ждёт SceGeQueue сразу после «GEBENCH start», до первой строки замера): списки стенда (L, S, E)
// пишутся мимо кэша (sceGuStart), а память под них из кучи могла держать в кэше грязные строки прежнего хозяина — при вытеснении они ложились поверх команд
// списка, и видеочип уходил в мусор (у листов клеток движка от этого стоит сброс ДО сборки, main_render.h dlBuildM). Теперь так же: кэш над L, S, E сброшен
// и очищен сразу после выделения; данные стенда (треугольник, текстуры, палитра) пишутся через кэш — после заполнения сброшены в память. GEBENCH1216_OLD=1 — как было
#ifndef GEBENCH1216_OLD
#define GEBENCH1216_OLD 0
#endif
static void gbFlush(const void *p, unsigned n) { if (!GEBENCH1216_OLD && n) sceKernelDcacheWritebackRange(p, n); }   // данные, записанные через кэш -> в память
#define GB_R 5
#define GB_LIST (384u << 10)
typedef struct { float u, v; unsigned c; float x, y, z; } GbVert;
static GbVert __attribute__((aligned(16))) gbTri[3];
static unsigned char __attribute__((aligned(16))) gbTex[2][64 * 64], gbPal[256 * 4];
static ScePspFMatrix4 __attribute__((aligned(16))) gbM;
static unsigned gbN;
static void gbBodyNone(void) {}
static void gbBodyDraw(void) { for (unsigned i = 0; i < gbN; i++) sceGuDrawArray(GU_TRIANGLES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 3, 0, gbTri); }
static void gbBodyMatrix(void) { for (unsigned i = 0; i < gbN; i++) sceGuSetMatrix(GU_MODEL, &gbM); }
static void gbBodyTexImage(void) { for (unsigned i = 0; i < gbN; i++) sceGuTexImage(0, 64, 64, 64, gbTex[i & 1]); }
static void gbBodyClut(void) { for (unsigned i = 0; i < gbN; i++) sceGuClutLoad(32, gbPal); }
static void gbBodyBind(void) {   // привязка как у движка: режим, палитра, картинка (два рисунка по очереди)
  for (unsigned i = 0; i < gbN; i++) { sceGuTexMode(GU_PSM_T8, 0, 0, 0); sceGuClutMode(GU_PSM_8888, 0, 255, 0); sceGuClutLoad(32, gbPal); sceGuTexImage(0, 64, 64, 64, gbTex[i & 1]); } }
static void gbBodyEnable(void) { for (unsigned i = 0; i < gbN; i++) { if (i & 1) sceGuEnable(GU_FOG); else sceGuDisable(GU_FOG); } }
static void gbBodyDepthMask(void) { for (unsigned i = 0; i < gbN; i++) sceGuDepthMask(i & 1); }
static void gbBodyBlend(void) { for (unsigned i = 0; i < gbN; i++) sceGuBlendFunc(GU_ADD, (i & 1) ? GU_SRC_ALPHA : GU_FIX, GU_FIX, 0, 0); }
static void gbBodyCallEmpty(void) { }   // пустой список вызывается из direct отдельно (см. ниже)
typedef struct { const char *name; void (*body)(void); int mode; } GbTest;   // mode 0 — и direct, и call; 1 — только direct (вызов списков)
static int gbCmp(const void *a, const void *b) { unsigned x = *(const unsigned *)a, y = *(const unsigned *)b; return x < y ? -1 : x > y; }
static void gbLine(const char *name, const char *mode, unsigned *cpu, unsigned *tot, unsigned n, int emu) {
  qsort(cpu, GB_R, sizeof *cpu, gbCmp); qsort(tot, GB_R, sizeof *tot, gbCmp);
  char s[200]; snprintf(s, sizeof s, "GEBENCH test=%s mode=%s n=%u cpu_us_med=%u cpu_us_min=%u per_ns=%u total_us_med=%u emu=%d\n", name, mode, n, cpu[GB_R / 2], cpu[0], cpu[GB_R / 2] * 1000u / (n ? n : 1u), tot[GB_R / 2], emu);
  smkWrite(s);
}
static unsigned gbPerNs[16][2];
// cpu — время выдачи, total — до конца работы GE; direct: тело пишется в открытый GU_DIRECT; call: тело строится в списке GU_CALL (время сборки = cpu), потом один sceGuCallList
static void gbMeasure(const char *name, void (*body)(void), unsigned n, int doCall, void *L, void *S, int emu, int slot) {
  unsigned cpu[GB_R], tot[GB_R]; gbN = n;
  for (int r = 0; r < GB_R; r++) {
    sceGuStart(GU_DIRECT, L); unsigned long long t0 = sceKernelGetSystemTimeWide(); body(); unsigned long long t1 = sceKernelGetSystemTimeWide(); sceGuFinish(); sceGuSync(0, 0); unsigned long long t2 = sceKernelGetSystemTimeWide();
    cpu[r] = (unsigned)(t1 - t0); tot[r] = (unsigned)(t2 - t0); }
  gbLine(name, "direct", cpu, tot, n, emu); gbPerNs[slot][0] = cpu[GB_R / 2] * 1000u / (n ? n : 1u);
  if (!doCall) return;
  for (int r = 0; r < GB_R; r++) {
    sceGuStart(GU_CALL, S); unsigned long long t0 = sceKernelGetSystemTimeWide(); body(); unsigned long long t1 = sceKernelGetSystemTimeWide(); sceGuFinish();
    sceGuStart(GU_DIRECT, L); unsigned long long t2 = sceKernelGetSystemTimeWide(); sceGuCallList(S); unsigned long long t3 = sceKernelGetSystemTimeWide(); sceGuFinish(); sceGuSync(0, 0); unsigned long long t4 = sceKernelGetSystemTimeWide();
    cpu[r] = (unsigned)(t1 - t0); tot[r] = (unsigned)(t4 - t2); (void)t3; }
  gbLine(name, "call", cpu, tot, n, emu); gbPerNs[slot][1] = cpu[GB_R / 2] * 1000u / (n ? n : 1u);
}
static void geBenchRun(int emu) {
  unsigned long long tStart = sceKernelGetSystemTimeWide();
  sceGuSync(0, 0); int st = sceGuGetAllStatus();
  void *L = memalign(64, GB_LIST), *S = memalign(64, GB_LIST), *E = memalign(64, 256);
  if (!L || !S || !E) { smkWrite("GEBENCH skipped: no memory\n"); free(L); free(S); free(E); return; }
  if (!GEBENCH1216_OLD) { sceKernelDcacheWritebackInvalidateRange(L, GB_LIST); sceKernelDcacheWritebackInvalidateRange(S, GB_LIST); sceKernelDcacheWritebackInvalidateRange(E, 256); }   // №1216: до сборки списков
  memset(gbTri, 0, sizeof gbTri); gbTri[0].x = gbTri[1].x = gbTri[2].x = 0.f; memset(gbTex, 0x11, sizeof gbTex); for (int i = 0; i < 256; i++) ((unsigned *)gbPal)[i] = 0xff000000u | (unsigned)i * 0x010101u;
  memset(&gbM, 0, sizeof gbM); gbM.x.x = gbM.y.y = gbM.z.z = gbM.w.w = 1.f;
  gbFlush(gbTri, sizeof gbTri); gbFlush(gbTex, sizeof gbTex); gbFlush(gbPal, sizeof gbPal);   // №1216: видеочип читает их из памяти
  { char s[160]; snprintf(s, sizeof s, "GEBENCH start n=%d reps=%d emu=%d (cpu — выдача команд, total — до конца GE; цены читать при emu=0)\n", GB_N, GB_R, emu); smkWrite(s); }
  // пустой список, вызванный из direct (цена CallList без содержимого) и список из одного треугольника
  { unsigned cpu[GB_R], tot[GB_R]; sceGuStart(GU_CALL, E); sceGuFinish();
    for (int r = 0; r < GB_R; r++) { sceGuStart(GU_DIRECT, L); unsigned long long t0 = sceKernelGetSystemTimeWide(); for (unsigned i = 0; i < GB_N; i++) sceGuCallList(E); unsigned long long t1 = sceKernelGetSystemTimeWide(); sceGuFinish(); sceGuSync(0, 0); unsigned long long t2 = sceKernelGetSystemTimeWide(); cpu[r] = (unsigned)(t1 - t0); tot[r] = (unsigned)(t2 - t0); }
    gbLine("calllist_empty", "direct", cpu, tot, GB_N, emu); gbPerNs[10][0] = cpu[GB_R / 2] * 1000u / GB_N;
    sceGuStart(GU_CALL, S); gbN = 1; gbBodyDraw(); sceGuFinish();
    for (int r = 0; r < GB_R; r++) { sceGuStart(GU_DIRECT, L); unsigned long long t0 = sceKernelGetSystemTimeWide(); for (unsigned i = 0; i < GB_N; i++) sceGuCallList(S); unsigned long long t1 = sceKernelGetSystemTimeWide(); sceGuFinish(); sceGuSync(0, 0); unsigned long long t2 = sceKernelGetSystemTimeWide(); cpu[r] = (unsigned)(t1 - t0); tot[r] = (unsigned)(t2 - t0); }
    gbLine("calllist_1tri", "direct", cpu, tot, GB_N, emu); gbPerNs[11][0] = cpu[GB_R / 2] * 1000u / GB_N; }
  gbMeasure("none", gbBodyNone, GB_N, 0, L, S, emu, 0);          // накладная самой разметки времени
  gbMeasure("drawarray_1tri", gbBodyDraw, GB_N, 1, L, S, emu, 1);
  gbMeasure("setmatrix", gbBodyMatrix, GB_N, 1, L, S, emu, 2);
  gbMeasure("teximage", gbBodyTexImage, GB_N, 1, L, S, emu, 3);
  gbMeasure("clutload", gbBodyClut, GB_N, 1, L, S, emu, 4);
  gbMeasure("bind_full", gbBodyBind, GB_N, 1, L, S, emu, 5);
  gbMeasure("enable_fog", gbBodyEnable, GB_N, 1, L, S, emu, 6);
  gbMeasure("depthmask", gbBodyDepthMask, GB_N, 1, L, S, emu, 7);
  gbMeasure("blendfunc", gbBodyBlend, GB_N, 1, L, S, emu, 8);
  { char s[260]; snprintf(s, sizeof s, "GEBENCH kernel_ns draw=%d (direct %u − call %u) matrix=%d teximage=%d bind_full=%d enable=%d calllist_empty_ns=%u calllist_1tri_ns=%u emu=%d\n",
      (int)gbPerNs[1][0] - (int)gbPerNs[1][1], gbPerNs[1][0], gbPerNs[1][1], (int)gbPerNs[2][0] - (int)gbPerNs[2][1], (int)gbPerNs[3][0] - (int)gbPerNs[3][1], (int)gbPerNs[5][0] - (int)gbPerNs[5][1], (int)gbPerNs[6][0] - (int)gbPerNs[6][1], gbPerNs[10][0], gbPerNs[11][0], emu); smkWrite(s); }
  sceGuSetAllStatus(st); free(L); free(S); free(E);
  { char s[96]; snprintf(s, sizeof s, "GEBENCH done total_ms=%u\n", (unsigned)((sceKernelGetSystemTimeWide() - tStart) / 1000ull)); smkWrite(s); }
}
#else
static void geBenchRun(int emu) { (void)emu; }
#endif
#endif
