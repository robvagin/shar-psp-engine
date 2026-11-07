// Сторож: см. watchdog.h. Чистый C99, без PSP-заголовков, без malloc.
#include <stdio.h>
#include <string.h>
#include "watchdog.h"
typedef struct { char kind[24], actor[16]; WdCheckFn fn; void *user; unsigned n; } WdSlot;
static WdSlot slot[WD_KINDS];
static void (*logFn)(const char *);
static float (*clockFn)(void);
static float selfT;
static int on = 1;
static unsigned total;
void wdSetLog(void (*fn)(const char *line)) { logFn = fn; }
void wdSetClock(float (*now)(void)) { clockFn = now; }
void wdEnable(int e) { on = e; }
static WdSlot *find(const char *kind, int make) {
  WdSlot *free_ = 0;
  for (int i = 0; i < WD_KINDS; i++) {
    if (slot[i].kind[0] && !strcmp(slot[i].kind, kind)) return &slot[i];
    if (!slot[i].kind[0] && !free_) free_ = &slot[i];
  }
  if (make && free_) { memset(free_, 0, sizeof *free_); strncpy(free_->kind, kind, sizeof free_->kind - 1); return free_; }
  return 0;
}
int wdRegister(const char *kind, const char *actor, WdCheckFn fn, void *user) {
  WdSlot *s = find(kind, 1); if (!s) return -1;
  strncpy(s->actor, actor ? actor : "?", sizeof s->actor - 1); s->fn = fn; s->user = user; return 0;
}
void wdUnregister(const char *kind) { WdSlot *s = find(kind, 0); if (s) { s->fn = 0; s->kind[0] = 0; } }
void wdFire(const char *kind, const char *actor, const char *info) {
  WdSlot *s = find(kind, 1); if (s) { s->n++; if (!s->actor[0]) strncpy(s->actor, actor ? actor : "?", sizeof s->actor - 1); }
  total++;
  if (logFn) { char l[300]; snprintf(l, sizeof l, "WATCHDOG %s actor=%s t=%.2f %s", kind, actor ? actor : "?", clockFn ? clockFn() : selfT, info ? info : ""); logFn(l); }
}
void wdStep(float dt) {
  selfT += dt;
  if (!on || WATCHDOG_OFF) return;
  for (int i = 0; i < WD_KINDS; i++) {
    if (!slot[i].fn) continue;
    char info[200]; info[0] = 0;
    if (slot[i].fn(slot[i].user, dt, info, sizeof info)) wdFire(slot[i].kind, slot[i].actor, info);
  }
}
unsigned wdCount(const char *kind) { WdSlot *s = find(kind, 0); return s ? s->n : 0; }
unsigned wdTotal(void) { return total; }
void wdReset(void) { for (int i = 0; i < WD_KINDS; i++) slot[i].n = 0; total = 0; }
