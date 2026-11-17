// 108 шаг 0: служебные замеры тура (см. smkdiag.h). Собирается только в сборке тура (viewer/CMakeLists.txt), в игре файла нет.
#include "smkdiag.h"
#if !SHR_REL_SMK
#include <pspkernel.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int pieceBench;
unsigned mathCnt[5];
float __real_sinf(float), __real_cosf(float), __real_atan2f(float, float), __real_hypotf(float, float), __real_floorf(float);
float __wrap_sinf(float x) { mathCnt[0]++; return __real_sinf(x); }
float __wrap_cosf(float x) { mathCnt[1]++; return __real_cosf(x); }
float __wrap_atan2f(float y, float x) { mathCnt[2]++; return __real_atan2f(y, x); }
float __wrap_hypotf(float x, float y) { mathCnt[3]++; return __real_hypotf(x, y); }
float __wrap_floorf(float x) { mathCnt[4]++; return __real_floorf(x); }

// З-1: проходы 0..passes-1; проход 0 — первый («холодный» по кадру), остальные — повторы подряд
void pbReport(const unsigned *us, const int *n, int passes, void (*w)(const char *)) {
  char s[420]; int o = snprintf(s, sizeof s, "PIECEBENCH pieces=%d pass0_us=%u per_piece0=%.1f", n[0], us[0], n[0] ? (double)us[0] / n[0] : 0.0);
  double rest = 0; int rn = 0; for (int i = 1; i < passes; i++) { rest += us[i]; rn += n[i]; }
  o += snprintf(s + o, sizeof s - o, " repeat_us_avg=%.0f per_piece_repeat=%.1f passes_us=", passes > 1 ? rest / (passes - 1) : 0.0, rn ? rest / rn : 0.0);
  for (int i = 0; i < passes && o < (int)sizeof s - 12; i++) o += snprintf(s + o, sizeof s - o, "%u%s", us[i], i + 1 < passes ? "/" : "");
  snprintf(s + o, sizeof s - o, "\n"); w(s);
}

// З-2: цена промаха кэша данных. Читаем по слову на строку 64 байта (256 строк на массив 16 КБ), перед каждым проходом кэш вытесняется чтением 64 КБ
static unsigned cbSink;
static unsigned char cbStatic[16384] __attribute__((aligned(64)));
static void cbEvict(const volatile unsigned char *big) { unsigned s = 0; for (int i = 0; i < 65536; i += 64) s += big[i]; cbSink += s; }
static double cbRun(const volatile unsigned *a, const volatile unsigned char *big, int evict) {
  unsigned tot = 0; for (int i = 0; i < 256; i++) cbSink += a[i * 16];   // прогрев
  for (int r = 0; r < 100; r++) {
    if (evict) cbEvict(big);
    unsigned t0 = sceKernelGetSystemTimeLow(), s = 0; for (int i = 0; i < 256; i++) s += a[i * 16]; tot += sceKernelGetSystemTimeLow() - t0; cbSink += s;
  }
  return tot / 100.0 / 256.0;
}
#define SCR_BASE ((volatile unsigned *)0x00010000)
static int scrArmed; static unsigned scrBad;
void cacheBenchRun(int mode, int emu, void (*w)(const char *)) {
  static unsigned char cbHeap[16384] __attribute__((aligned(64))), cbBig[65536] __attribute__((aligned(64)));   // статические массивы тура (rawalloc_gate: сырых выделений ≥ 16 КБ нет); «куча» = обычная память игры, не быстрая
  unsigned char *heap = cbHeap, *big = cbBig;
  memset(heap, 1, 16384); memset(cbStatic, 2, sizeof cbStatic); memset(big, 3, 65536);
  int ev = mode != 3; char s[300];
  double h = cbRun((volatile unsigned *)heap, big, ev), st = cbRun((volatile unsigned *)cbStatic, big, ev);
  int o = snprintf(s, sizeof s, "CACHEBENCH emu=%d mode=%d evict=%d heap_us_per_line=%.3f static_us_per_line=%.3f", emu, mode, ev, h, st);
  if (mode == 2) {   // быстрая память: запись по адресу читается обратно? цена строки
    volatile unsigned *sp = SCR_BASE; unsigned bad = 0;
    for (int i = 0; i < 4096; i++) sp[i] = 0xA5A50000u + (unsigned)i;
    for (int i = 0; i < 4096; i++) if (sp[i] != 0xA5A50000u + (unsigned)i) bad++;
    double sc = cbRun(sp, big, 1);
    o += snprintf(s + o, sizeof s - o, " scratch_us_per_line=%.3f scratch_readback_bad=%u", sc, bad);
    for (int i = 0; i < 4096; i++) sp[i] = 0x5AC30000u + (unsigned)i;   // узор для З-3
    scrArmed = 1; scrBad = 0;
  }
  snprintf(s + o, sizeof s - o, "\n"); w(s);
}
// З-3: сверка узора раз в секунду (пишет ли туда кто-то ещё)
void scrGuardTick(float t, const char *scene, void (*w)(const char *)) {
  if (!scrArmed) return;
  volatile unsigned *sp = SCR_BASE; unsigned bad = 0;
  for (int i = 0; i < 4096; i++) if (sp[i] != 0x5AC30000u + (unsigned)i) bad++;
  scrBad += bad; char s[120]; snprintf(s, sizeof s, "SCRPADGUARD t=%.1f scene=%s bad_words=%u total_bad=%u\n", t, scene, bad, scrBad); w(s);
}

// ---- CELLSTAT: разбор клетки и повторные чтения
static unsigned cpN, cpTot, cpMax, crReads, crRe; static unsigned char crSeen[4096]; static int crLevel = -1;
void cellParseNote(int cell, int level, unsigned us) {
  if (level != crLevel) { memset(crSeen, 0, sizeof crSeen); crLevel = level; }
  cpN++; cpTot += us; if (us > cpMax) cpMax = us; crReads++;
  if ((unsigned)cell < sizeof crSeen) { if (crSeen[cell]) crRe++; if (crSeen[cell] < 255) crSeen[cell]++; }
}
int cellStatLine(char *buf, unsigned n, float t, const char *scene) {
  if (!cpN) return 0;
  int r = snprintf(buf, n, "CELLSTAT t=%.1f scene=%s parse_n=%u parse_us_avg=%.0f parse_us_max=%u rereads=%u/%u (повторное чтение клетки за уровень / все чтения; время стенное)\n", t, scene, cpN, (double)cpTot / cpN, cpMax, crRe, crReads);
  cpN = cpTot = cpMax = crReads = crRe = 0; return r;
}
// ---- GEONCE: сколько видеочип реально занят кадром
int geOnceOn; static unsigned goN; static double goGe, goWords, goCpu, goWait;
void geOnceAdd(float geMs, unsigned words, float cpuMs, float waitMs) { goN++; goGe += geMs; goWords += words; goCpu += cpuMs; goWait += waitMs; }
int geOnceLine(char *buf, unsigned n, float t, const char *scene) {
  if (!goN) return 0;
  int r = snprintf(buf, n, "GEONCE t=%.1f scene=%s frames=%u ge_ms=%.2f list_kwords=%.1f cpu_ms=%.2f ge_wait_ms=%.2f (ge_ms: видеочип один, список без процессора; ge_wait_ms: сколько кадр ждал его в игре)\n", t, scene, goN, goGe / goN, goWords / goN / 1000.0, goCpu / goN, goWait / goN);
  goN = 0; goGe = goWords = goCpu = goWait = 0; return r;
}
#endif
