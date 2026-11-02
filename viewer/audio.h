// SHAR PSP audio: software mixer for the original PS2 sound data (PS-ADPCM / VAG, bit-exact decode).
// Pack files (tools/sound_export.py -> build/audio/): sfx.sab (resident sounds + car engine params + level tables),
// music.sap (music, ambience, Homer's lines, positional loops; streamed from the memory stick by a background thread).
// Game-level API on top of this: viewer/sound.h.
//
// Threads started by audioInit: "audio_mix" prio 0x12 (sceAudio output, 512 frames @ 44.1 kHz stereo = 11.6 ms),
// "audio_io" prio 0x16 (own file handle, 16 KB reads into rings of 128+128+64+64 KB). All calls below are for the main
// thread; they only write a few words into voice slots (no locks, no waiting) and cost microseconds.
#ifndef SHAR_AUDIO_H
#define SHAR_AUDIO_H

// AUDIO_BUS_UI (правка 114): звуки меню — пауза, прокрутка, выбор; наборы шин тюнера (applyMode в sound.c) её не трогают
enum { AUDIO_BUS_SFX, AUDIO_BUS_CAR, AUDIO_BUS_MUSIC, AUDIO_BUS_AMBIENCE, AUDIO_BUS_DIALOG, AUDIO_BUS_UI, AUDIO_NBUS };
enum { AUDIO_ENGINE_ON = 1, AUDIO_ENGINE_REVERSE = 2, AUDIO_ENGINE_DAMAGED = 4 };
enum { AUDIO_ST_MUSIC, AUDIO_ST_AMBIENCE, AUDIO_ST_DIALOG, AUDIO_ST_POSITIONAL, AUDIO_ST_CAR_OVERLAY, AUDIO_NSTREAM };

typedef struct {
  float cpu;              // mixer thread busy time, % of wall time (last second)
  float mixUs, mixUsMax;  // average / worst microseconds per 11.6 ms mix (last second)
  int voices;             // voices sounding now
  int underruns;          // stream buffer ran dry (since init)
  int dropped;            // sounds not started because all voices were busy
  int fill[AUDIO_NSTREAM];        // stream ring fill, % (music, ambience, dialogue, positional)
  int track[AUDIO_NSTREAM];       // current track id per stream (-1 = none)
  float trackPos[AUDIO_NSTREAM];  // seconds into the current track
  unsigned ioBytes;       // bytes read by the stream thread
  int ioMaxMs;            // slowest single stream read, ms
  int gear;               // engine model state
  float enginePitch, engineVol;
  float limiter;          // output limiter gain now (1 = idle)
  unsigned limiterBuffers;// 11.6 ms buffers where the limiter was reducing gain (since init)
} AudioStats;

// dir: folder with sfx.sab and music.sap, ending with '/', e.g. "ms0:/PSP/GAME/SHARVIEW/audio/".
// Returns 0 on success, <0 if a file is missing/invalid (the game can run silent; all calls stay safe).
int  audioInit(const char *dir);
void audioShutdown(void);
// №1069: банк sfx.sab живёт в постоянном месте (одно выделение на всю игру). audioSlotOld(1) — как до правки: из кучи на каждый уровень.
// audioBankCapSim(N) — тест: выделение банка из кучи больше N байт отказывает (раздробленная куча PSP; эмулятор так не дробится), 0 — выкл
void audioSlotOld(int on);
void audioBankCapSim(unsigned bytes);
const char *audioError(void);

// ids: look up once at load time, keep the int.
int  audioFind(const char *name);          // sound resource (names from the disc .spt scripts), -1 if absent
int  audioTrack(const char *name);         // music/ambience track, -1 if absent
int  audioSoundCount(void);  const char *audioSoundName(int id);
// правка 135.7: журнал эффектов — звуки, которые audio.c запускает сам (gearshift на повышении передачи); ставит sound.c
extern void (*audioNote)(int id, const char *why);
int  audioTrackCount(void);  const char *audioTrackName(int id);

// Resident sounds. Random variant and random pitch range come from the original script; vol 0..1,
// pan -1..1, pitch multiplier. Looping resources loop until audioStop. Returns a handle (0 = not started).
int  audioPlay(int id, float vol, float pan, float pitch);
int  audioPlayBus(int id, float vol, float pan, float pitch, int bus);   // то же на заданной шине (AUDIO_BUS_UI — меню)
void audioSet(int h, float vol, float pan, float pitch);
void audioStop(int h);                     // ~12 ms fade, no click
int  audioPlaying(int h);

// Positional helpers: listener = camera position + its right vector (x,z).
void audioListener(float x, float y, float z, float rightX, float rightZ);
int  audioPlayAt(int id, float x, float y, float z, float minDist, float maxDist);
int  audioPlayAtGain(int id, float vol, float x, float y, float z, float minDist, float maxDist);
void audioSetAt(int handle, float x, float y, float z, float minDist, float maxDist);
void audioStreamAt(int stream, float x, float y, float z, float minDist, float maxDist);
void audioEnginePosition(float x, float y, float z, int outside);
// volume 0..1 and pan -1..1 of a point for the current listener (full inside minDist, silent past maxDist)
void audioSpatial(float x, float y, float z, float minDist, float maxDist, float *vol, float *pan);

// Streams. Tracks chain as authored (e.g. sunday_drive_intro -> sunday_drive, which loops seamlessly).
void audioMusic(int track, float fadeSec);     // switch now (fade out old); -1 = stop
void audioMusicOnce(int track, float fade);   // правка 303: один проход (без петли и next)
void audioMusicFrom(int track, float fade, float startSec);   // №789: с места startSec секунд (за концом — с начала)
void audioMusicNext(int track);                // switch at the end of the current loop pass (no cut)
void audioAmbience(int track, float fadeSec);
void audioStream(int stream, int track, float fadeSec);   // any stream (AUDIO_ST_*); -1 = stop
void audioStreamSet(int stream, float vol, float pan);    // stream voice level/pan (default 1, 0)
// Separate frontend SAP: fixed IDs >=0x4000, no changes to level track tables.
// Bank stays open until audioShutdown, avoiding close/read races at FE exit.
int audioExtraStreams(const char *file);
int audioExtraTrack(const char *name);
// Prefetch a stream without decoding/advancing or sounding it; release at event181.
int audioStreamPrepare(int stream,int track);
int audioStreamPrepared(int stream,int track);
int audioStreamStartPrepared(int stream,int track);
unsigned audioStreamRequestSerial(int stream); // identify a request even when the track repeats
int  audioStreamTrack(int stream);                        // playing or about to play, -1 = none
// Правка Роба 155: NEW GAME — газета, ролик, и только потом звук уровня. audioHoldStreams(1): запросы потоков музыки и
// атмосферы (AUDIO_ST_MUSIC, 1) не исполняются, запоминается последний на поток; (0) — исполнить запомненные, (-1) —
// выбросить их (звук уровня ставит заново levelAudioStart). audioStreamNote — журнал запросов (s, дорожка, fade, держится ли)
int  audioHoldStreams(int mode);                          // -> сколько запросов было удержано
extern void (*audioStreamNote)(int stream, int track, float fade, int held);
int  audioTrackBus(int track);                            // 0 music, 1 ambience, 2 dialogue, 3 positional
float audioTrackSeconds(int track);
const void *audioBankExt(unsigned *size);                 // level tables of sfx.sab ("SXT1"), NULL = none
void audioBus(int bus, float vol);             // defaults from the disc's global tuner (music 0.78, car 0.88...)
void audioMaster(float vol);                   // final gain (1.0 default)
void audioUserVolume(int bus, float vol);     // player volume per bus (Sound menu), multiplies the tuner bus volume

// Player car, driven by the original car_tune.spt model of the car in the pack (default famil_v).
// Call once per frame. speedKmh signed or not; topKmh = the car's top speed.
// Three nearby traffic sources, at most two existing resident voices each.
// profile indexes an authored SAB car row; source=-1 stops with the normal fade.
#define AUDIO_WORLD_ENGINES 3
void audioWorldSkid(int slot, float amount, int dirt); // №446: same positional source as its own engine
int audioCarProfile(const char *name);
int audioPlayerCar(const char *name);   // правка 413а: профиль звука машины игрока по её имени (.con); нет в пакете — машина уровня
void audioCarSndOld(int on);           // 413а: красный контроль — всегда машина уровня
void audioWorldEngine(int slot,int source,int profile,float x,float y,float z,
                      float speedKmh,float topKmh,float throttle01,float dt);
void audioEngine(float speedKmh, float topKmh, float throttle01, unsigned flags);
// сверка g08-sound (P10-8…16): состояние машины игрока для модели мотора, раз в кадр до audioEngine; audioClockUs — монотонные микросекунды
void audioEngineEx(float skid01, int spinInPlace, int air, int parked, float life01);
unsigned audioClockUs(void);
void audioHorn(int on);
// сверка g08-sound (P10-1,2,21): мотор трафика и ИИ миссий — один клип на вид, до AUDIO_SV_SLOTS слотов каждого; audioSvReady — клипы ferrari и
// mission_ai_vehicle есть в пакете (иначе вызывающий остаётся на прежнем пути)
#define AUDIO_SV_SLOTS 5
int audioSvReady(void);
int audioSvEngine(int kind, int slot, int on, int profile, float x, float y, float z, float speed, float ref, float mult, float dt);
void audioSvStopAll(void);
void audioSkid(float amount01, int dirt);      // 0 = silent; loops while > 0
void audioResumeOld(int on);                    // №560: 1 — откат возобновления музыки после сна PSP (красный контроль)

void audioStats(AudioStats *s);
#ifndef AUDIO_HOST
// test: copy the next maxFrames stereo frames sent to sceAudio into buf (44.1 kHz s16); audioCaptured = frames so far
void audioCapture(short *buf, unsigned maxFrames);
unsigned audioCaptured(void);
// speaker volume only (after the mix and the capture): 0 = silent output, the capture still gets the full mix
void audioOutputVolume(float vol);
#endif

#ifdef AUDIO_HOST   // Mac-side test harness only (tests/audio/host)
void audioHostMix(short *out, int frames);     // runs the stream reader + one mix, like the PSP threads do
void audioTestForceUnity(int on);              // step = 1.0, gains = 1.0: output equals the decoded samples
int  audioTestPlayFile(int fileIndex);         // one pass of a bank file, no loop
int  audioTestFileCount(void);
void audioTestMusicOnce(int track);            // one pass of a stream track, no loop/chain (-1: back to normal)
int  audioTestFileInfo(int fileIndex, int *ch, int *rate, unsigned *nsamp);
int  audioTestResLoops(int id);
int  audioTestTrackInfo(int t, int *loop, int *next, int *bus, unsigned *nsamp, unsigned *loopStart, unsigned *loopEnd);
int  audioTestSeamCount(void);                 // loop jumps so far (resident + stream)
void audioTestSeed(unsigned seed);             // random variant/pitch generator state (tests/sfx250: same draw in two renders)
unsigned audioTestFramesOut(void);
float audioTestAmbGain(void);                  // tests/amb161: AMB_GAIN этой сборки
void audioTestPwrEvent(int flags);
void audioTestSimulateFdInvalidate(void);
extern void (*audioTestSeamHook)(unsigned frame);
#endif

void audioMix250(int on);   // правка 250: сведение по замерам L1 (без щелчка передачи, мотор ×0,84); sound.c sndInit
int audioMusicState(char *out, int n);   // правка 279: служебно — мастер, шина и громкость игрока музыки, что играет поток
void audioDspUs(float *dsp, float *buf);   // правка 313 (замер для ME, «flag meprof»): счёт смешивания и весь буфер на главном, мкс на буфер
void audioProfKern(int on); float audioKernCopyUs(void);   // правка 313 (замер): перенос задания звука в ядро и обратно (схема k), мкс
// правка 276 (пилот Media Engine): смешивание на ME — только с флаг-файлом meaudio (viewer/me/meprobe.c meAudioBoot), только PSP.
// audioMeHold(1) — ME звуку не отдавать (ролик sceJpeg тоже на ME), ждёт до 100 мс; audioMeHold(0) — можно снова.
// audioMeState: «-» нет, «on86/0.4» — заданий за секунду / наибольшее ожидание ответа ME, мс, «hold…», «off:timeout@N»
void audioMeHold(int on);
int audioMeState(char *out, int n);
// правка 410 (09.10, docs/handoff/ME_ПЛАН_2026-10-09.md): AUDQ — опоздавшие буферы (треск), тишина при звучащем голосе, разбивка отдачи
// на ME за прошлую секунду (только PSP); audioMeWbOld(1) — сброс задания звука целиком, как до К1 (файл mewbold, сравнение в туре)
int audioQualityLine(char *out, int n);
void audioMeWbOld(int on);
#endif

// №1100: окно колец потоков в задании звука не больше 1024 Б (ME_HAND), выключатель на ходу и строка AUDH
void audioMeHand(int on); int audioMeHandOn(void); int audioHandLine(char *out, int n);
void audioEngineBurn(float level01);   // круг 7: уровень бернаута (0 — нет), до audioEngineEx
void audioSetLanguage(int lang);   // №1111: 0 English, 1 French, 2 German, 3 Spanish — читается в audioInit (со следующей загрузки уровня)
int audioLanguageTracks(void);    // сколько дорожек с озвучкой языка в пакете сейчас
void audioPauseAll(int on);   // №1144: пауза без таблицы режимов (бонус-игра): всё в 0 за 750 мс, потоки стоят
float audioPauseLevel(void);
