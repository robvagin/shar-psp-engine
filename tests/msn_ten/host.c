// Аудит полноты п.10 (g04-homer): «10 секунд» — реплика героя «Time» один раз в момент, когда таймер пересёк 10 с вниз; этап, начавшийся с остатком меньше 10 с, её не даёт
// (в оригинале музыки 10sec нет — сверка g08-sound P09:20; «только последний этап» проверяет вызывающий game_msn.c). Красный: -DMSN_AUDIO_SV_OLD=1 (звучит на любом этапе, музыкой).
#include <stdio.h>
#include <string.h>
#include "msn_audio.h"
#ifndef MSN_AUDIO_SV_OLD
#define MSN_AUDIO_SV_OLD 0
#endif
static int warns;
static void tw(void *u) { (void)u; warns++; }
static int mevent(void *u, const char *e, const char **n) { (void)u; (void)e; *n = ""; return 0; }
static int fails;
static void chk(const char *w, int got, int want) { int ok = got == want; if (!ok) fails++; printf("%s %s: %d (ждём %d)\n", ok ? "ok  " : "FAIL", w, got, want); }
static void run(MsnAudio *a, float from, float to, float step) { for (float t = from; t >= to; t -= step) msnAudioTick(a, 1.f / 30.f, 1, t); }
int main(void) {
  static MsnAudioBackend b; b.time_warn = tw; b.music_event = mevent; b.logNow = 1;
  static MsnAudio a; memset(&a, 0, sizeof a); a.b = &b;
  msnAudioMissionStart(&a, "m1", 0); a.started = 1; a.prevLeft = -1.f;
  warns = 0; run(&a, 20.f, 9.f, 1.f / 30.f);           // таймер идёт 20 → 9: пересечение 10 с — одна реплика
  chk("пересечение 10 с вниз — одна реплика", warns, 1);
  warns = 0; run(&a, 9.f, 2.f, 1.f / 30.f);            // дальше вниз — больше не звучит
  chk("ниже 10 с повторно не звучит", warns, 0);
  msnAudioMissionStart(&a, "m1", 0); a.started = 1; a.prevLeft = -1.f; warns = 0;
  run(&a, 8.f, 1.f, 1.f / 30.f);                       // этап начался с остатком 8 с: пересечения нет
  chk("этап начался с остатком меньше 10 с — реплики нет", warns, 0);
  msnAudioMissionStart(&a, "m1", 0); a.started = 1; a.prevLeft = -1.f; warns = 0;
  run(&a, 10.f, 9.f, 1.f / 30.f);                      // ровно с 10,0: «был не меньше 10, стал меньше» — звучит
  chk("ровно с 10 с вниз — одна реплика", warns, 1);
  printf("%s: провалов %d\n", MSN_AUDIO_SV_OLD ? "old" : "new", fails); return fails ? 1 : 0;
}
