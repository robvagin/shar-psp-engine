// Голос разговоров и музыка миссий (пакет «Миссии: озвучка»), чистый C99 — общий для EBOOT (viewer/msn_view.c, звук
// viewer/sound.c) и игры без экрана (tools/bots: длительности из таблицы music.sap). Логика миссий — viewer/game_msn.c.
//
// Голос. Разговор миссии (цель dialogue, SetDialogueInfo) приходит списком реплик — файлов диска
// conversations/c_<имя>_<N>_<тип>_<кто>_l<уровень>[<миссия>].rsd (tools/missions/compile.py). Каждая реплика — дорожка
// music.sap по правилу msnAudioVoiceName (то же правило в tools/sound_export.py voice_track_name). Реплики звучат по
// порядку в потоке диалогов; конец последней = конец разговора (game_msn.c: msn_notify DIALOGUE_DONE). Реплика без
// дорожки: строка «VOICE none ... нет голоса», пауза MSN_AUDIO_NOVOICE_S (правило порта), разговор не зависает.
// Реплика завершения (SetCompletionDialog) и подбора — не блокируют миссию.
//
// Музыка. Композиция уровня sound/music/l1_music.rms задаёт события миссии <код>_start / _drama / _10sec_to_go / _win /
// _lose / _get_out_of_car (код M0..M7 сюжетных, Bonus для бонус-миссии). Когда что звучит (не из данных — двоичный граф
// событий не разобран; правило порта, сверить с эталоном PS2):
//   начало основной части миссии (после брифинга)            -> <код>_start
//   этап с AddStageMusicChange                               -> <код>_drama
//   StageStartMusicEvent("M3_drama")                          -> это событие (данные скрипта)
//   таймер этапа < 10 с                                       -> <код>_10sec_to_go
//   игрок вышел из машины (этап без SetStageMusicAlwaysOn)    -> <код>_get_out_of_car; сел обратно -> <код>_start
//   миссия пройдена / провалена                               -> <код>_win / <код>_lose; отмена / выход -> свободная езда
#ifndef MSN_AUDIO_H
#define MSN_AUDIO_H
#include "mission/msn_game.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSN_AUDIO_NOVOICE_S 1.5f   // правило порта: реплика без голоса держит разговор столько секунд
#define MSN_AUDIO_GUARD_S   2.0f   // реплика дольше своей длины + столько — считается кончившейся (поток завис)
#define MSN_AUDIO_QUEUE     MSN_MAX_LINES
#define MSN_AUDIO_LOGQ      8

typedef struct MsnAudioBackend {
  void *u;
  int   (*voice_track)(void *u, const char *name);     // дорожка music.sap по имени, -1 нет
  float (*track_seconds)(void *u, int track);          // длина дорожки, с
  void  (*voice_play)(void *u, int track);             // поток диалогов: -1 = стоп
  int   (*voice_playing)(void *u, int track);          // NULL — по часам (игра без экрана)
  // музыка: 1 запущена (*name — дорожка), 0 событие без смены музыки, -1 нет такого события в музыке уровня
  int   (*music_event)(void *u, const char *event, const char **name);
  void  (*music_release)(void *u);                     // отдать музыку свободной езде
  void  (*dialogue_mode)(void *u, int on);             // режим звука «Dialogue» на время разговора (шины диска)
  void  (*log)(void *u, const char *line);             // строка в smoke.txt / вывод бота
  int   logNow;                                        // 1 — строки сразу (бот); 0 — по одной за кадр (EBOOT: запись в файл)
  void  (*always_on)(void *u, int on);                 // круг 3 (g08, P09:1): этап «музыка всегда» включён/снят — фон улицы после выхода из машины не возвращается; NULL — ничего
  void  (*time_warn)(void *u);                         // сверка g08-sound (P09:20, P06 Д4): таймер пересёк 10 с вниз — реплика героя «Time» (музыки 10sec нет); NULL — ничего
  void  (*warm)(void *u);                              // Звук-96: старт миссии — заказать её первые звуки в кэш заранее; NULL — ничего
} MsnAudioBackend;

typedef struct MsnAudio {
  const MsnAudioBackend *b;
  float clock;                                          // секунды с начала игры (для строк журнала)
  // разговор
  char conv[32]; int blocking, nlines, line, track; float lineT, lineLen;
  const char *file[MSN_AUDIO_QUEUE]; const char *actor[MSN_AUDIO_QUEUE];
  int active, done;                                     // done — разговор только что кончился (забирает game_msn.c)
  unsigned voiced, missing;                             // реплик прозвучало / без голоса (с начала игры)
  // музыка
  char code[16];                                        // "M3", "Bonus", "StreetRace01"
  char stateCode[8], stateValue[16], currentEvent[40]; /* RMS state selector, emitted event before @value */
  float prevLeft;                                       // остаток таймера в прошлом кадре (пересечение 10 с)
  int startPend;                                        // сверка g08-sound (СВЕРКА_С_ИСХОДНИКОМ 4.1): миссия началась пешком — «_start» ждёт посадки в машину
  int started, alwaysOn, onFoot, tensec, out;           // музыка миссии идёт; этап «музыка всегда»; пешком; 10 с было; вышел из машины
  char logq[MSN_AUDIO_LOGQ][160]; int log0, nlog;       // строки журнала: наружу по одной за кадр
} MsnAudio;

// правило имени дорожки реплики: ".../c_icecream_1_convinit_mrg_l1.rsd" -> "v_icecream_1_ci_mrg_l1"; 0 — не влезает в 27
int  msnAudioVoiceName(const char *file, char out[28]);
void msnAudioInit(MsnAudio *a, const MsnAudioBackend *b);
// разговор: blocking = 1 — цель dialogue (конец -> a->done), 0 — реплика завершения/подбора (не ждём)
void msnAudioDialogue(MsnAudio *a, const MsnDialogue *d, int blocking);
void msnAudioStopVoice(MsnAudio *a);
// музыка миссии
void msnAudioMissionStart(MsnAudio *a, const char *missionId, int bonus);   // "m3" -> M3_start
void msnAudioStage(MsnAudio *a);                                             // начало этапа: «музыка всегда» сбрасывается
void msnAudioEvent(MsnAudio *a, const char *event);                          // StageStartMusicEvent
void msnAudioSuffix(MsnAudio *a, const char *suffix);                        // "_drama", "_win" ... к коду миссии
void msnAudioAlwaysOn(MsnAudio *a);
void msnAudioState(MsnAudio *a, const char *state, const char *value);
void msnAudioMissionEnd(MsnAudio *a);                                        // отмена / выход: свободная езда
// каждый кадр: dt, пешком ли игрок, таймер этапа (осталось, с; < 0 — нет таймера). 1 — блокирующий разговор кончился
int  msnAudioTick(MsnAudio *a, float dt, int onFoot, float timerLeft);

#ifdef __cplusplus
}
#endif
#endif
