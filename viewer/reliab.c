// Надёжность: см. reliab.h
#include "reliab.h"
#include "bootmark.h"
#include <stdio.h>
#include <string.h>
#if defined(__psp__) || defined(PSP)
#include <pspkernel.h>
#define REL_PSP 1
#endif

volatile int relLockOld = LOCKLIM1195_OLD, relStackOld = RELSTACK1195_OLD, relStackOn = RELSTACK1195_ON;

// ---- кольцо строк события: пишут любые потоки, читает главный (relFrame) ----
#define REL_RING 8
static struct { char s[120]; volatile unsigned done; } relRing[REL_RING];
static volatile unsigned relHead; static unsigned relTail;
void relNote(const char *fmt, ...) {
  unsigned t = __sync_fetch_and_add(&relHead, 1u), k = t % REL_RING;
  relRing[k].done = 0;
  va_list ap; va_start(ap, fmt); vsnprintf(relRing[k].s, sizeof relRing[k].s - 1, fmt, ap); va_end(ap);
  size_t n = strlen(relRing[k].s); if (!n || relRing[k].s[n - 1] != '\n') { relRing[k].s[n] = '\n'; relRing[k].s[n + 1] = 0; }
  __sync_synchronize(); relRing[k].done = t + 1u;
}
unsigned relNoteCount(void) { return relHead; }
static void relDrain(void (*sink)(const char *)) {
  unsigned h = relHead; if (h - relTail > REL_RING) relTail = h - REL_RING;   // писали быстрее, чем читали: старые строки потеряны
  while (relTail != h) { unsigned k = relTail % REL_RING; if (relRing[k].done != relTail + 1u) break; if (sink) sink(relRing[k].s); relTail++; }
}

// ---- замок с пределом ----
#ifdef REL_PSP
static unsigned relClockUs(void) { return sceKernelGetSystemTimeLow(); }   // реальные часы: сон потока на PSP и в эмуляторе длиннее заказанного шага, считать шаги нельзя
#else
static unsigned relVirtUs;                                                   // хост-тест: время идёт только шагами сна
static unsigned relClockUs(void) { return relVirtUs; }
#endif
int relSpinAcquire(volatile int *lock, unsigned limitUs, unsigned stepUs, void (*delayFn)(unsigned), const char *name) {
  unsigned t0 = relClockUs();
  while (__sync_lock_test_and_set(lock, 1)) {
    unsigned waited = relClockUs() - t0;
    if (!relLockOld && waited >= limitUs) { relNote("LOCK %s timeout %u ms: замок не отпущен, выхожу без него", name, waited / 1000u); return 0; }
    if (delayFn) delayFn(stepUs);
#ifndef REL_PSP
    relVirtUs += stepUs ? stepUs : 1u;
#endif
  }
  return 1;
}

// ---- стек ----
void relStackPaintRange(unsigned char *base, unsigned n) { if (relStackOn && !relStackOld && n) memset(base, REL_STACK_PAINT, n); }
unsigned relStackHeadroom(const unsigned char *base, unsigned size) {
  unsigned n = 0; while (n < size && base[n] == REL_STACK_PAINT) n++;
  return n;
}
int relStackWarn(unsigned headroom, unsigned size) { return size && headroom * 10u < size; }

#ifdef REL_PSP
#define REL_THREADS 16
static struct { SceUID id; char name[32]; unsigned char *base; unsigned size, lastNote, warnFree; int warned; } relTh[REL_THREADS];
void relStackPaint(void) {
  bmMarkU("thread-start tid", (unsigned)sceKernelGetThreadId());   // №1192: шаги старта на карту (bootmark.h) — до закраски стека потока
  if (!relStackOn || relStackOld) return;   // по умолчанию выключено: стек потока не трогаем
  SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti; SceUID me = sceKernelGetThreadId();
  if (sceKernelReferThreadStatus(me, &ti) < 0 || !ti.stack || ti.stackSize < 8192) return;
  if (!strcmp(ti.name, "user_main")) return;   // стек главного потока задаёт загрузчик модуля, не красим
  unsigned char *sp = (unsigned char *)__builtin_frame_address(0), *base = (unsigned char *)ti.stack;
  if ((uintptr_t)base < 0x08800000u || (uintptr_t)base + ti.stackSize > 0x0C000000u) return;   // не пользовательская память: не трогаем
  if (sp <= base || sp >= base + ti.stackSize) return;   // sp не в стеке потока
  unsigned char *top = sp - 4096; base += 512;   // 4 КБ под текущим кадром и 512 байт у нижней границы (служебные данные потока) не красим
  if (top <= base + 1024) return;
  bmMarkU("paint stack base", (unsigned)base); bmMarkU("paint stack bytes", (unsigned)(top - base));
  relStackPaintRange(base, (unsigned)(top - base));
  bmMark("painted");
  for (unsigned i = 0; i < REL_THREADS; i++) if (relTh[i].id == 0 && __sync_bool_compare_and_swap(&relTh[i].id, 0, -1)) {   // свободное место — слот кончившегося потока (потоки звука пересоздаются на каждом уровне)
    strncpy(relTh[i].name, ti.name, sizeof relTh[i].name - 1); relTh[i].name[sizeof relTh[i].name - 1] = 0; relTh[i].base = base; relTh[i].size = (unsigned)ti.stackSize; relTh[i].lastNote = relTh[i].size + 1u; relTh[i].warned = 0;
    __sync_synchronize(); relTh[i].id = me; return; }
}
static unsigned relNext; static int relNextSet;
void relFrame(void (*sink)(const char *), unsigned nowUs) {
  if (relStackOld) { relDrain(sink); return; }
  if (!relNextSet) { relNextSet = 1; relNext = nowUs + 5000000u; }
  else if ((int)(nowUs - relNext) >= 0) {
    relNext = nowUs + 5000000u; 
    for (unsigned i = 0; i < REL_THREADS; i++) {
      SceKernelThreadInfo ti; memset(&ti, 0, sizeof ti); ti.size = sizeof ti; if (relTh[i].id <= 0) continue; if (sceKernelReferThreadStatus(relTh[i].id, &ti) < 0) { relTh[i].id = 0; continue; }   // поток кончился — стек не читаем
      unsigned free_ = relStackHeadroom(relTh[i].base, relTh[i].size), sz = relTh[i].size;
      if (free_ + 1024u <= relTh[i].lastNote) { relNote("STACK %s size=%u free=%u used_max=%u", relTh[i].name, sz, free_, sz - free_); relTh[i].lastNote = free_; }
      if (relStackWarn(free_, sz) && (!relTh[i].warned || free_ + 512u <= relTh[i].warnFree)) { relNote("STACK %s WARN запас %u из %u (< 10 %%)", relTh[i].name, free_, sz); relTh[i].warned = 1; relTh[i].warnFree = free_; }
    }
  }
  relDrain(sink);
}
#else
void relStackPaint(void) {}
void relFrame(void (*sink)(const char *), unsigned nowUs) { (void)nowUs; relDrain(sink); }
#endif
