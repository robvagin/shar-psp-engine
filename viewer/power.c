// 107б P0: питание — общий счётчик эпохи, строки PWR, имитация сна (см. power.h)
#include "power.h"
#include <stdio.h>
#include <string.h>
#ifdef __psp__
#include <pspkernel.h>
#include <psppower.h>
#define PWR_NOW() sceKernelGetSystemTimeLow()
#else
#define PWR_NOW() 0u
#endif
void audioSimSuspend(void); void audioSimResume(void);   // audio.c: те же вызовы обработчика питания звука, что на железе
// 107б (ревью Fable): обработчик питания работает в нити со стеком 4 КБ — в нём только счётчики и запись события в кольцо без snprintf; строку PWR собирает главная нить (pwrTakeLog)
#define PWR_LOGQ 8
#define PWR_RESUME_COMPLETE 0x00040000
typedef struct { const char *kind; unsigned n, epoch, sleeps, t; int asleep, a, b; } PwrEv;
static PwrEv evq[PWR_LOGQ]; static volatile unsigned logW, logR; static volatile int resumeNeedBump;
static void logPush(const char *kind, int a, int b) {
  PwrEv *e = &evq[logW % PWR_LOGQ]; e->kind = kind; e->n = pwrResumeN; e->epoch = pwrEpochV; e->sleeps = pwrSuspendN; e->asleep = pwrAsleepV; e->t = PWR_NOW(); e->a = a; e->b = b;
  __asm__ volatile("" ::: "memory"); logW++;
}
int pwrTakeLog(char *buf, unsigned n) {
  // запасной ход: пробуждение пришло только фазой RESUMING (RESUME_COMPLETE не было за 2 с) — эпоха растёт один раз здесь, на главной нити
  if (resumeNeedBump && pwrResumeAtUs && (unsigned)(PWR_NOW() - pwrResumeAtUs) > 2000000u) { resumeNeedBump = 0; pwrEpochV++; logPush("resume-epoch-fallback", 0, 0); }
  if (logR == logW) return 0;
  if (logW - logR > PWR_LOGQ) logR = logW - PWR_LOGQ;
  const PwrEv *e = &evq[logR % PWR_LOGQ];
  if (!strcmp(e->kind, "sim sleep")) snprintf(buf, n, "PWR sim sleep hooks=%d fail=%d\n", e->a, e->b);
  else snprintf(buf, n, "PWR %s n=%u epoch=%u sleeps=%u asleep=%d t_us=%u\n", e->kind, e->n, e->epoch, e->sleeps, e->asleep, e->t);
  logR++; return 1;
}
void pwrBump(const char *why) { pwrEpochV++; logPush(why && !strcmp(why, "level") ? "bump-level" : "bump", 0, 0); }
void pwrNoteSuspend(void) { pwrAsleepV = 1; pwrSuspendN++; resumeNeedBump = 0; logPush("suspend", 0, 0); }
void pwrNoteResume(int flags) {
  // одно пробуждение — одна смена эпохи (на RESUME_COMPLETE): окна не бросают только что открытый живой дескриптор второй раз
  if (flags & PWR_RESUME_COMPLETE) { pwrEpochV++; resumeNeedBump = 0; if (pwrAsleepV) { pwrAsleepV = 0; pwrResumeN++; pwrResumeAtUs = PWR_NOW() | 1u; } logPush("resume-complete", 0, 0); return; }
  if (pwrAsleepV) { pwrAsleepV = 0; pwrResumeN++; pwrResumeAtUs = PWR_NOW() | 1u; resumeNeedBump = 1; logPush("resume", 0, 0); } else logPush("resume2", 0, 0);
}
#define PWR_HOOKS 12
static struct { const char *name; void (*kill)(void); } hooks[PWR_HOOKS]; static int nhooks;
void pwrKillHook(const char *name, void (*kill)(void)) {
  for (int i = 0; i < nhooks; i++) if (hooks[i].name && !strcmp(hooks[i].name, name)) { hooks[i].kill = kill; return; }
  if (nhooks < PWR_HOOKS) { hooks[nhooks].name = name; hooks[nhooks].kill = kill; nhooks++; }
}
static int pwrArmPt;
void pwrSleepArm(int pt) { pwrArmPt = pt; }
void pwrSleepAtPoint(int pt) { if (pwrArmPt && pwrArmPt == pt) { pwrArmPt = 0; pwrSimSleep(2); logPush("sim sleep at point", pt, 0); } }   // тур: сон ровно во время ожидания (оценка Fable 6д)
void pwrSimSleep(int n) {
  audioSimSuspend();                                   // уход в сон: тот же обработчик, что на железе (звук держит ME, флаг сна)
  int k = 0; for (int i = 0; i < nhooks; i++) if (hooks[i].kill) { hooks[i].kill(); k++; }
  logPush("sim sleep", k, n);
  pwrSimN = n > 0 ? n : 0; pwrSimAt = PWR_NOW() | 1u;
  audioSimResume();                                    // пробуждение: те же события, что на железе
}
