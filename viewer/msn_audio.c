// Голос разговоров и музыка миссий (см. msn_audio.h). Чистый C99, без PSP: EBOOT и боты на Маке.
#include <stdio.h>
#include <string.h>
#include "msn_audio.h"

#ifndef MSN_AUDIO_BREAK
#define MSN_AUDIO_BREAK 0   // отрицательный контроль гейта tests/missions/voice_gate.sh: 1 — правило имени сломано (голоса нет)
#endif

static void say(MsnAudio *a, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
#ifndef MSN_AUDIO_SV_OLD
#define MSN_AUDIO_SV_OLD 0   // сверка g08-sound: 1 — прежнее поведение музыки миссий (круг 3: выключатель в стиле остальных, значение 0 = новое)
#endif
// строки журнала — в очередь, наружу по одной за кадр (msnAudioTick): запись в smoke.txt открывает файл, несколько строк в
// одном кадре — лишние миллисекунды кадра, которых в игре нет. Время события — внутри строки (t=)
static void say(MsnAudio *a, const char *fmt, ...) {
  if (!a->b || !a->b->log) return;
  if (a->b->logNow) {
    char s[160]; va_list v; va_start(v, fmt); vsnprintf(s, sizeof s, fmt, v); va_end(v);
    a->b->log(a->b->u, s); return;
  }
  if (a->nlog >= MSN_AUDIO_LOGQ) { a->b->log(a->b->u, a->logq[a->log0]); a->log0 = (a->log0 + 1) % MSN_AUDIO_LOGQ; a->nlog--; }
  char *s = a->logq[(a->log0 + a->nlog) % MSN_AUDIO_LOGQ];
  va_list v; va_start(v, fmt); vsnprintf(s, sizeof a->logq[0], fmt, v); va_end(v);
  a->nlog++;
}
static void flushOne(MsnAudio *a) {
  if (!a->nlog || !a->b || !a->b->log) return;
  a->b->log(a->b->u, a->logq[a->log0]); a->log0 = (a->log0 + 1) % MSN_AUDIO_LOGQ; a->nlog--;
}

int msnAudioVoiceName(const char *file, char out[28]) {
  static const char *const kind[3][2] = {{"_convinit_", "_ci_"}, {"_noboxconv_", "_nb_"}, {"_tutorial_", "_tu_"}};
  const char *b = file, *p;
  for (p = file; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
  char t[96]; size_t n = 0;
  for (p = b; *p && n < sizeof t - 1; p++) t[n++] = (char)(*p >= 'A' && *p <= 'Z' ? *p + 32 : *p);
  t[n] = 0;
  if (n > 4 && !strcmp(t + n - 4, ".rsd")) t[n -= 4] = 0;
  const char *s = t; if (!strncmp(s, "c_", 2)) s += 2;
  char r[96]; size_t k = 0; r[k++] = 'v'; r[k++] = '_';
  while (*s && k < sizeof r - 1) {
    int hit = 0;
    for (int i = 0; i < 3 && !MSN_AUDIO_BREAK; i++) {
      size_t l = strlen(kind[i][0]);
      if (!strncmp(s, kind[i][0], l)) { size_t m = strlen(kind[i][1]); if (k + m >= sizeof r) break; memcpy(r + k, kind[i][1], m); k += m; s += l; hit = 1; break; }
    }
    if (!hit) r[k++] = *s++;
  }
  r[k] = 0;
  if (k > 27) { out[0] = 0; return 0; }
  memcpy(out, r, k + 1); return 1;
}

void msnAudioInit(MsnAudio *a, const MsnAudioBackend *b) { memset(a, 0, sizeof *a); a->b = b; a->track = -1; }

// ------------------------------------------------------------------ голос
static void startLine(MsnAudio *a) {
  char nm[28]; int t = -1;
  a->lineT = 0.f; a->track = -1;
  if (a->b && a->b->voice_track && msnAudioVoiceName(a->file[a->line], nm)) t = a->b->voice_track(a->b->u, nm);
  if (t < 0) {   // нет дорожки: разговор не зависает (правило порта), строка в журнал
    a->lineLen = MSN_AUDIO_NOVOICE_S; a->missing++;
    say(a, "VOICE none %s %d/%d %s file=%s (нет голоса) t=%.1f\n", a->conv, a->line + 1, a->nlines, a->actor[a->line] ? a->actor[a->line] : "-",
        a->file[a->line], a->clock);
    return;
  }
  a->track = t; a->lineLen = a->b->track_seconds ? a->b->track_seconds(a->b->u, t) : 0.f;
  if (a->b->voice_play) a->b->voice_play(a->b->u, t);
  a->voiced++;
  say(a, "VOICE start %s %d/%d %s track=%s dur=%.2f t=%.1f\n", a->conv, a->line + 1, a->nlines, a->actor[a->line] ? a->actor[a->line] : "-",
      nm, a->lineLen, a->clock);
}
static void endLine(MsnAudio *a) {
  if (a->track >= 0) say(a, "VOICE end %s %d/%d t=%.1f\n", a->conv, a->line + 1, a->nlines, a->clock);
  a->track = -1;
}
static void finish(MsnAudio *a) {
  if (a->b && a->b->voice_play) a->b->voice_play(a->b->u, -1);
  if (a->blocking && a->b && a->b->dialogue_mode) a->b->dialogue_mode(a->b->u, 0);
  say(a, "VOICE done %s lines=%d t=%.1f\n", a->conv, a->nlines, a->clock);
  if (a->blocking) a->done = 1;
  a->active = 0;
}
void msnAudioDialogue(MsnAudio *a, const MsnDialogue *d, int blocking) {
  if (a->active && a->blocking && !blocking) return;   // реплика подбора не перебивает разговор миссии
  if (a->active) { endLine(a); if (a->blocking && a->b && a->b->dialogue_mode) a->b->dialogue_mode(a->b->u, 0); a->active = 0; }
  snprintf(a->conv, sizeof a->conv, "%s", d->name && *d->name ? d->name : "-");
  a->nlines = d->nlines < MSN_AUDIO_QUEUE ? d->nlines : MSN_AUDIO_QUEUE;
  for (int i = 0; i < a->nlines; i++) { a->file[i] = d->file[i]; a->actor[i] = d->actor[i]; }
  a->blocking = blocking; a->line = 0; a->done = 0;
  say(a, "VOICE conv %s lines=%d %s t=%.1f\n", a->conv, a->nlines, blocking ? "dialogue" : "oneliner", a->clock);
  if (!a->nlines) { a->active = 1; finish(a); return; }
  a->active = 1;
  if (blocking && a->b && a->b->dialogue_mode) a->b->dialogue_mode(a->b->u, 1);
  startLine(a);
}
void msnAudioStopVoice(MsnAudio *a) {
  if (!a->active) return;
  endLine(a);
  if (a->b && a->b->voice_play) a->b->voice_play(a->b->u, -1);
  if (a->blocking && a->b && a->b->dialogue_mode) a->b->dialogue_mode(a->b->u, 0);
  a->active = 0;
}

// ------------------------------------------------------------------ музыка
static void event(MsnAudio *a, const char *ev) {
  const char *nm = "-", *selected = ev; char key[64]; int r = -1;
  snprintf(a->currentEvent, sizeof a->currentEvent, "%s", ev);
#ifndef C3_CONTENT_MUSIC_OLD
  size_t nc = strlen(a->stateCode);
  if (nc && a->stateValue[0] && !strncmp(ev, a->stateCode, nc) && ev[nc] == '_' && a->b && a->b->music_event) {
    snprintf(key, sizeof key, "%s@%s", ev, a->stateValue);
    r = a->b->music_event(a->b->u, key, &nm); if (r >= 0) selected = key;
  }
#endif
  if (r < 0) r = a->b && a->b->music_event ? a->b->music_event(a->b->u, ev, &nm) : -1;
  say(a, "MUSIC event %s -> %s t=%.1f\n", selected, r > 0 ? nm : r == 0 ? "без смены (у события нет потоков)" : "нет в музыке уровня", a->clock);
  if (r > 0 && strstr(ev, "_start")) a->started = 1;
}
void msnAudioSuffix(MsnAudio *a, const char *suf) {
  if (!a->code[0]) return;
  char ev[40]; snprintf(ev, sizeof ev, "%s%s", a->code, suf); event(a, ev);
}
void msnAudioMissionStart(MsnAudio *a, const char *id, int bonus) {
  a->started = a->alwaysOn = a->tensec = a->out = a->startPend = 0; a->code[0] = 0;
  a->stateCode[0] = a->stateValue[0] = a->currentEvent[0] = 0;
  if (!id) return;
  if (a->b && a->b->warm) a->b->warm(a->b->u);   // Звук-96: первые звуки миссии заказаны до её старта
#ifndef MUSIC_RACE_OLD
  /* Street races also carry MSN_MF_BONUS; select their authored code first. */
  if (id[0] == 's' && id[1] == 'r' && id[2] >= '1' && id[2] <= '3' && !id[3])
    snprintf(a->code, sizeof a->code, "StreetRace0%c", id[2]);
  else
#endif
  if (bonus) snprintf(a->code, sizeof a->code, "Bonus");
  else if ((id[0] == 'm' || id[0] == 'M') && id[1] >= '0' && id[1] <= '9' && !id[2]) snprintf(a->code, sizeof a->code, "M%c", id[1]);
  else { say(a, "MUSIC mission %s: своей музыки в композиции нет (уличные гонки — не покрыто) t=%.1f\n", id, a->clock); return; }
#if !MSN_AUDIO_SV_OLD
  // сверка g08-sound (СВЕРКА_С_ИСХОДНИКОМ 4.1): старт или перезапуск миссии пешком — музыка миссии не начинается (звучит прогулка и фон места);
  // начнётся, когда игрок сядет в машину. MSN_AUDIO_SV_OLD=1 — прежнее: «_start» сразу, на следующем кадре «_get_out_of_car»
  if (a->onFoot) { a->startPend = 1; say(a, "MUSIC mission %s: старт пешком, музыка миссии ждёт машину t=%.1f\n", id, a->clock); return; }
#endif
  msnAudioSuffix(a, "_start");
}
void msnAudioStage(MsnAudio *a) { a->alwaysOn = 0; a->tensec = 0; a->prevLeft = -1.f; if (a->b && a->b->always_on) a->b->always_on(a->b->u, 0); }
void msnAudioEvent(MsnAudio *a, const char *ev) { if (ev && *ev) event(a, ev); }
void msnAudioAlwaysOn(MsnAudio *a) { a->alwaysOn = 1; if (a->b && a->b->always_on) a->b->always_on(a->b->u, 1); }
void msnAudioState(MsnAudio *a, const char *state, const char *value) {
#ifndef C3_CONTENT_MUSIC_OLD
  if (!state || strncmp(state, "Mission", 7) || state[7] < '0' || state[7] > '9' || state[8] || !value || strlen(value) >= sizeof a->stateValue) return;
  char code[8]; snprintf(code, sizeof code, "M%c", state[7]);
  int change = strcmp(code, a->stateCode) || strcmp(value, a->stateValue);
  snprintf(a->stateCode, sizeof a->stateCode, "%s", code); snprintf(a->stateValue, sizeof a->stateValue, "%s", value);
  /* Port transition policy: reselect the current event immediately. The RMS
     matrices prove which region, not the PS2 crossfade timing. */
#if !MSN_AUDIO_SV_OLD
  if (a->onFoot) change = 0;   // сверка g08-sound (P09 Д16): пешком состояние только запоминается; в машине событие переигрывается с новым значением
#endif
  if (change && !strcmp(code, a->code) && a->currentEvent[0]) { char ev[40]; snprintf(ev, sizeof ev, "%s", a->currentEvent); event(a, ev); }
#else
  (void)a; (void)state; (void)value;
#endif
}
void msnAudioMissionEnd(MsnAudio *a) {
  if (a->code[0] && a->b && a->b->music_release) a->b->music_release(a->b->u);
  if (a->code[0]) say(a, "MUSIC release %s t=%.1f\n", a->code, a->clock);
  a->code[0] = 0; a->started = 0; a->out = 0; a->startPend = 0;
  a->stateCode[0] = a->stateValue[0] = a->currentEvent[0] = 0;
}

int msnAudioTick(MsnAudio *a, float dt, int onFoot, float timerLeft) {
  a->clock += dt;
  flushOne(a);
#if !MSN_AUDIO_SV_OLD
  if (a->startPend && !onFoot) { a->startPend = 0; if (a->code[0]) msnAudioSuffix(a, "_start"); }   // сел в машину после старта пешком
#endif
  if (a->code[0] && a->started && onFoot && !a->onFoot && !a->alwaysOn) { msnAudioSuffix(a, "_get_out_of_car"); a->out = 1; }
  else if (a->code[0] && !onFoot && a->onFoot && a->out) { a->out = 0; msnAudioSuffix(a, "_start"); }   // сел обратно: музыка миссии снова
  a->onFoot = onFoot;
  if (a->code[0] && !a->tensec && timerLeft >= 0.f && timerLeft < 10.f) {
#if !MSN_AUDIO_SV_OLD
    // сверка g08-sound (P09:20, P06 Д4; круг 3): реплика Time — только в момент пересечения 10 с вниз (был не меньше 10, стал меньше); этап, начавшийся с остатком
    // меньше 10 с, пересечения не даёт. Условия «последний этап» и «считает вниз» проверяет вызывающий (game_msn.c передаёт остаток только тогда)
    if (a->prevLeft >= 10.f) { a->tensec = 1; if (a->b && a->b->time_warn) a->b->time_warn(a->b->u); }
#else
    a->tensec = 1; msnAudioSuffix(a, "_10sec_to_go");
#endif
  }
  a->prevLeft = timerLeft;
  if (a->active) {
    a->lineT += dt;
    int playing = a->track >= 0 && a->b && a->b->voice_playing ? a->b->voice_playing(a->b->u, a->track) : a->lineT < a->lineLen;
    if (a->lineT > a->lineLen + MSN_AUDIO_GUARD_S) playing = 0;   // поток не ответил — не держать миссию
    if (a->track < 0) playing = a->lineT < a->lineLen;
    if (!playing && a->lineT > 0.05f) {
      endLine(a);
      if (++a->line < a->nlines) startLine(a); else finish(a);
    }
  }
  if (a->done) { a->done = 0; return 1; }
  return 0;
}
