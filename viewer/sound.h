// SHAR PSP: game sound (stage 6). Thin game-side layer over viewer/audio.c (mixer thread + streaming):
// events from car / props / stateprops / coins / Homer, the family car engine model, level music by game state,
// ambience zones (gates of the level) and positional loops, Homer's in-car lines with trigger rules.
// Data: build/audio/sfx.sab + music.sap from tools/sound_export.py (copy the folder next to EBOOT.PBP as audio/).
// All calls are from the main thread, cost microseconds, never block; without the pack everything is a silent no-op.
// Integration into main.c: docs/sound_integration.md.
#ifndef SHAR_SOUND_H
#define SHAR_SOUND_H
#include "msncar.h"

#ifdef __cplusplus
extern "C" {
#endif

// music states (l1_music.rms regions): what the game is doing now
enum {
  SND_MUS_OFF,            // silence (fade out)
  SND_MUS_FREEROAM,       // Level_Intro / Sunday_Drive_start: sunday_drive_intro -> sunday_drive (loop)
  SND_MUS_ONFOOT,         // Sunday_Drive_Get_out_of_Car: one of sunday_drive_end_sus1..3, then silence
  SND_MUS_MISSION,        // M1_start: collection02_pickup -> collection02_main (loop)
  SND_MUS_MISSION_WIN,    // M1_win: collection02_end_pos, then free roam
  SND_MUS_MISSION_LOSE,   // M1_lose: collection02_end_neg, then free roam
  SND_MUS_MISSION_ONFOOT, // M1_get_out_of_car: collection02_end_sus1/2, then silence
  SND_MUS_HITRUN,         // Hit_and_Run: hit_run_intro -> hit_run_main (loop)
  SND_MUS_HITRUN_CAUGHT,  // Hit_and_Run_Caught: hit_run_end_neg, then free roam
  SND_MUS_KWIKEMART,      // KwikEmart: Apu's muzak (loop)
  SND_MUS_STORE,          // Store: muzak (loop)
  SND_MUS_FRONTEND,       // FE_region (l?_music.rms): Simpsons_Theme -> simpsons_theme.rsd (loop) — главное меню
  SND_MUS_COUNT
};
// bus volume sets of the disc tuner (global.spt): Normal, Pause, MissionScreen, Letterbox, Dialogue, Store, OnFoot, ...
enum { SND_MODE_NORMAL, SND_MODE_PAUSE, SND_MODE_MISSIONSCREEN, SND_MODE_LETTERBOX, SND_MODE_DIALOGUE,
       SND_MODE_STORE, SND_MODE_ONFOOT, SND_MODE_MINIGAME, SND_MODE_JUSTMUSIC, SND_MODE_CREDITS };
// engine flags
enum { SND_ENG_ON = 1, SND_ENG_REVERSE = 2, SND_ENG_DAMAGED = 4 };
// Homer's in-car lines (dialog.spt D_<event>_Hom_*); правка 345: MSTART (✕ Continue карточки миссии) и GOC (выход из машины) —
// семейства W_Mstart_<код>_* / W_GOC_<код>_* водителя уровня (Hom, Brt, Lis, Mrg, Apu; tools/sound_export.py player_code)
enum { SND_SAY_BCRASH, SND_SAY_MCRASH, SND_SAY_DAMAGE, SND_SAY_BURN, SND_SAY_DCAR, SND_SAY_BREAK, SND_SAY_AIR,
       SND_SAY_HITP, SND_SAY_MISSP, SND_SAY_CARWAY, SND_SAY_DOOR, SND_SAY_ARRIVE, SND_SAY_TIME, SND_SAY_MSTART, SND_SAY_GOC, SND_SAY_GIC,
       // аудит 09.10 п. 12/58: W_Card, W_Mvic, W_Mfail, W_Springboard, W_Fall, W_NewAI, W_Pass, W_Passed водителя уровня
       SND_SAY_CARD, SND_SAY_MVIC, SND_SAY_MFAIL, SND_SAY_SPRINGBOARD, SND_SAY_FALL, SND_SAY_NEWAI, SND_SAY_PASS, SND_SAY_PASSED,
       SND_SAY_HITBYC, SND_SAY_TAIL, SND_SAY_AIDESTROY, SND_SAY_TURBO, SND_SAY_DODGE, SND_SAY_LONGJUMP,
       SND_SAY_COUNT };
// аудит 09.10: реплики соперника V_<семейство>_<код>_* (код — по имени водителя AddStageVehicle, правило порта: буквы кода по порядку
// в имени, первая совпадает — skinner -> skn/skin, smithers -> smi, cletus -> clt). V_Mvic — игрок победил, V_Mfail — проиграл,
// V_Damage — машину соперника ударили, V_Tail — соперник впереди (смысл — по записям диска, расшифровка 09.10)
enum { SND_V_MVIC, SND_V_MFAIL, SND_V_DAMAGE, SND_V_TAIL, SND_V_HITCAR, SND_V_COUNT };
// footstep ground (global.spt Footstep*ClipName + homer.spt): concrete / metal / wood / dirt
enum { SND_GROUND_CONCRETE, SND_GROUND_METAL, SND_GROUND_WOOD, SND_GROUND_DIRT };
// object sets bound to the pack's object table (sndBindObjects)
enum { SND_SET_PROPS, SND_SET_SPROPS, SND_NSET };

// dir: folder with sfx.sab and music.sap, ending in '/'. 0 = ok (<0: silent, sndError says why)
int  sndInit(const char *dir);
void sndShutdown(void);
const char *sndError(void);
int  sndReady(void);

// once per frame, after the camera is known: listener (camera position + its right vector on the ground),
// player position (car or Homer) for the ambience gates and positional loops; dt in seconds
void sndFrame(float dt, float camX, float camY, float camZ, float rightX, float rightZ, float px, float py, float pz);
// start of the level / teleport: pick the ambience zone of this point without waiting for a gate
void sndLevelStart(float px, float py, float pz);

// generic effects: script resource names ("car_hit_hydrant", "coin_collect_01", ...). Positional: full volume
// within minDist 4 m, silent past 60 m (the original's collision sounds are 3D with similar ranges).
int  sndId(const char *name);                                       // -1 if not in the pack
int  sndPlay(const char *name, float x, float y, float z, float vol);
int  sndPlayId(int id, float x, float y, float z, float vol);
int  sndPlay2D(int id, float vol);                                   // UI, pickups, stingers: centred, no distance
int  sndPlayUi(int id, float vol);                                   // правка 114: звуки меню на шине AUDIO_BUS_UI — пауза (набор «Pause») их не глушит
void sndStop(int handle);

// ---- player car (famil_v model of car_tune.spt: gear from speed / top speed, pitch per gear, shift dip, throttle
// envelope, reverse pitch, damaged-engine loop). speed m/s (signed ok), throttle 0..1.
void sndCarSetup(float topSpeedMs);
void sndEngine(float speedMs, float throttle, unsigned flags);
void sndCarPosition(float x,float y,float z,int outside);
void sndHorn(int on);
// сверка g08-sound (02.11.2026): состояние машины для модели мотора и реплик (sound.c), шаги по поверхности, режимы шин
void sndCarState(float topSpeedMs, float skid01, int air, int parked, float life01, int burnout, int brakeOrRev);
void sndMusicAlways(int on);                                    // этап «музыка всегда»
void sndCarMass(float kg);                                   // масса машины игрока, кг (интенсивность удара = импульс / 100000; по умолчанию 1500)
void sndStepFoot(float x, float y, float z, int run);        // касание ноги Гомера: 2D, клип по поверхности; run=1 — рывок (Turbo на 5 с)
void sndStore(int kind);                                     // «магазин»: 0 выкл, 1 будка/лавка машин, 2 провал миссии, 3 гардероб
void sndTubaNow(void);                                       // сверка g10b-loop круг 5: ближайший кадр пешком — случайная туба прогулки (если музыка миссии не держит)
void sndJustMusic(int on);                                   // «только музыка» на время перехода в помещение и из него
void sndLoadDuck(int on);                                    // газета не из меню: звук мира как в паузе
void sndCardFound(float x, float y, float z);                // карточка: card_collect — в машине, пешком — вставка OF_found_card
void sndMissionAmbience(int level, int mission, int onFoot); // атмосфера старта миссии по таблице уровень × номер (пешком)
void sndSwitchPress(void);                                   // нажат красный переключатель: щелчок switch на 0,3 с
int  sndAnimObj(const char *objKey, float x, float y, float z); // звук подвижного объекта по имени объекта уровня (запуск клипа)
void sndModeSnap(void);                                      // шины сразу в целевое значение (смена экрана)
#ifndef SV_GIC_OLD
#define SV_GIC_OLD 0                                         // P05 Д5: реплика GIC в начале посадки (△ принят), а не у двери; 1 — прежнее
#endif
#ifndef SV_CAB_OLD
#define SV_CAB_OLD 0                                         // круг 2 (g04 P10 Д7): «оставлена» после конца анимации выхода (homerCabEngine), а не с начала выхода; 1 — прежнее
#endif
#ifndef SV_PARK_OLD
#define SV_PARK_OLD 0                                        // P10-16: мотор оставленной машины не глохнет через 3 с — играет холостой клип; 1 — прежнее
#endif
void sndPoliceCars(const MsnCars *cars);
void sndBurnRepeatOld(int on);   // №180/№184: 1 — реплика горящей машины один раз (прежнее)
void sndPlayerCar(const char *name);
void sndPlayerCarState(const char *name, int wrecked);
void sndCarSndOld(int on);
float sndTestAmbOutDb(void);
void sndMiniGame(int on);
void sndSkid(float amount01, int dirt);                               // 0 = silent; loops while > 0
void sndCarAir(int airborne);                                          // правка 135.9: гул в полёте выключен (SND_CAR_AIR 0), было — гипотеза
void sndCarLand(float impactSpeed);                                    // suspension_ronks on a hard landing (> 4 m/s)
// hit of the player car: wall / ground / object, strongest impact of the frame (car.hitSpeed, m/s) at x,y,z.
// Picks small/medium/large_car_crash (2 / 6 / 12 m/s), the sound of a level object hit there (trees, poles,
// culverts: object table, kind InstStatPhys) and may trigger Homer's crash lines.
void sndCarHit(float hitSpeed, float x, float y, float z);             // crash only (no object sound): tests, old callers
// Правка Роба 135 (удар машины игрока о мир за кадр, viewer/carsnd.h): hardV — сила удара о твёрдое (стена, ствол, столб),
// softV — о листву (материал FOLIAGE), м/с; p — точка контакта (громкость crash — там); prim = 1 — самый сильный контакт
// пришёлся на цилиндр/коробку COL1 с центром o: звук статичного объекта звучит, только если этот примитив — его (центр в r 1.5 м).
// Металлический crash — только от твёрдого (>= 2 м/с); листва — hit_shrub своего объекта (нет объекта — hit_shrub), без crash.
void sndCarHitWorld(float hardV, float softV, float px, float py, float pz, int prim, float ox, float oy, float oz);
void sndCarHitCar(float hitSpeed, float x, float y, float z);         // other car: crash + "car in the way" line, no object sounds
// правка 135.6: события трафика — сбит пешеход (car_hit_pedestrian + реплика HitP), разбита машина трафика (generic_car_explode)
void sndPedHit(float x, float y, float z);
void sndTrafficWreck(float x, float y, float z);
// правка 135.7: журнал эффектов (флаг-файл sfxlog рядом с EBOOT): на каждый запущенный звук эффекта строка
// "SFX <клип> <событие> <x> <y> <z> vol=<громкость>" (без перевода строки); NULL — выключен
void sndLogHook(void (*fn)(const char *line));
void sndCarDamage(float damage01);                                      // every frame: Homer's damage/burn lines on thresholds
void sndCarExplode(float x, float y, float z);                          // generic_car_explode + D_Dcar
void sndCarDoor(int open);                                              // famil_open/close: not on the disc -> silent

// ---- level objects: bind positions once (after propsInit / spInit), then report hits by instance index.
// xyz: first float of each record is x, then y, z; stride in bytes (sizeof(PropDef) / sizeof(SpInst)).
void sndBindObjects(int set, unsigned n, const void *xyz, unsigned strideBytes);
extern void (*sndBindPump)(void);   // №517: кадр экрана загрузки изнутри долгой привязки звуков к предметам (NULL — нет)
void sndObjectHit(int set, unsigned i, float x, float y, float z, float speed);   // plays the object's own sound
                                                  // (правка 135.1: реплика Гомера D_Break — только в машине, sndOnFoot)
int  sndIsCrate(int set, unsigned i);                                              // круг 3 (g11): звук удара предмета — деревянный ящик диска (car_hit_crate)
int  sndObjectSound(int set, unsigned i);                                          // its resource id, -1 none

// ---- pickups and Homer
void sndCoin(void);                                  // coin_collect_01: one of 3 variants (script)
void sndCoinLose(float x, float y, float z);
void sndFootstep(int ground, float x, float y, float z);
void sndJump(float x, float y, float z);
void sndLand(int ground, float x, float y, float z);
void sndStomp(float x, float y, float z);
// Homer's line for an event (rules: min 5 s between lines, not over another line, per-event chance and cool-down,
// no immediate repeat). force = 1 ignores the chance (not the gap). Returns 1 if a line started.
int  sndSay(int event, int force);
int  sndRivalSay(const char *driver, int kind, int force);   // 1 — реплика соперника началась (правила как у sndSay)
int  sndRivalHas(const char *driver);                        // у водителя есть реплики V_* в пакете уровня
// правка 345: имя дорожки последней реплики sndSay (журнал смоука); выход из машины — фронт on (main.c: Гомер в HS_GET_OUT) ->
// реплика W_GOC_: вернёт её имя, "" — выход без реплики (шанс), NULL — не фронт; sndPcSayOld = 1 (flag pcsayold 1) — прежнее:
// реплик старта миссии и выхода нет
const char *sndSayLastName(void);
void sndSeed(unsigned t);   // №685: семя случайного звука от времени (не в смоуке)
int sndSayStats(char *buf, int n);   // №683: SAYSTAT
const char *sndGetOut(int on);
// Посадка: W_GIC героя; первый кадр уже за рулём не считается посадкой
const char *sndGetIn(int on);
extern int sndPcSayOld;
// правка 345: атмосфера комнаты (таблица RAMB пакета, tools/sound_export.py room_ambience): вход в комнату с атмосферой — её петля
// (L1_Kwik_E_Mart / L1_House / L1_School -> room_small_01, L2_DMV -> DMV), выход (NULL) — атмосфера улицы, что была; комната без
// записи — улица, как было. Возврат — имя дорожки атмосферы после вызова. sndRoomAmbOld = 1 (flag roomambold 1) — прежнее
const char *sndRoom(const char *room);
extern int sndRoomAmbOld;
// Голоса улицы (правки 180/184, близнец Б2): прохожий — kind SND_PED_HIT (сбит машиной, P_HitByC), SND_PED_NEAR (чуть не сбит,
// P_NHitByC), SND_PED_KICK (удар Гомера, P_HitByW); голос voice — sndPedVoice(имя модели). Водитель трафика — SND_DRV_WAY / MCRASH /
// BCRASH (D_CarWay / D_Mcrash / D_Bcrash), голос 0..3 (Vm1, Vm2, Vf1, Vf2). Поток реплик тот же, что у Гомера: не поверх реплики,
// не чаще раза в 1,5 с. Возврат 1 — реплика пошла
int sndPedNamedSayAt(int kind, const char *name, float x, float y, float z);
int sndDoorbellNear(float x, float y, float z);
int sndDoorbellSay(int index);
enum { SND_PED_HIT, SND_PED_NEAR, SND_PED_KICK, SND_PED_KINDS };
enum { SND_DRV_WAY, SND_DRV_MCRASH, SND_DRV_BCRASH, SND_DRV_KINDS };
int  sndPedVoice(const char *model);
int  sndPedSay(int kind, int voice);
int  sndDriverSay(int kind, int voice);
// Street reactions are world sources; scripted conversations keep their 2D mix.
int  sndPedSayAt(int kind,int voice,float x,float y,float z);
int  sndDriverSayAt(int kind,int voice,float x,float y,float z);
int  sndStreetMove(float x,float y,float z); // 1 while the same street track owns dialogue
void sndStreetStop(void);
extern int sndStreetOff;   // служебное (красный контроль): голоса улицы молчат

// ---- music / mode
void sndMusic(int state);
int  sndMusicState(void);
void sndMode(int mode);
int  sndModeCur(void);
int  sndModeDebug(char *out, int n);   // правка 279: служебно — последний применённый набор тюнера                               // последний sndMode (Normal по умолчанию)
void sndLetterbox(int on);                          // правка 55: полосы разговора — шины «Letterbox» поверх режима
void sndPause(int on);                              // правка 87: пауза — шины «Pause» тюнера диска (тишина)
int  sndModeApplied(void);                           // режим, который сейчас звучит (Letterbox, пока полосы)

// ---- музыка миссий: события композиции уровня (sound/music/l1_music.rms -> таблица MEVT, tools/sound_export.py):
// "M2_start", "M2_drama", "M4_10sec_to_go", "M1_win", "M1_lose", "M1_get_out_of_car", "Bonus_start" ...
// 1 — дорожка события запущена (*track — её имя); 0 — событие в композиции есть, но потоков у него нет (M0_start: музыка
// не меняется); -1 — такого события в музыке уровня нет. Пока миссия ведёт музыку (start/drama/10sec/get_out_of_car),
// sndMusic(ONFOOT / FREEROAM / MISSION*) из main.c её не перебивает; win/lose доигрывают концовку и отдают музыку
// свободной езде; sndMusicMission(0) — отдать сразу (конец/отмена миссии).
extern int sndSusLoopOld;   // правка 303: 1 — концовка get_out_of_car петлёй (прежнее, красный контроль)
int  sndMusicEvent(const char *event, const char **track);
void sndMusicMission(int own);
int  sndMusicOwned(void);
int  sndVoiceBusy(void);   // идёт реплика разговора миссии (гэг её не перебивает)
void sndOnFoot(int on);   // Гомер пешком/в машине: музыка после миссии и погони возвращается к своей (ONFOOT / FREEROAM)
// голос реплики разговора миссии: дорожка music.sap в потоке диалогов (-1 — стоп); 1 — эта дорожка ещё звучит
void sndVoice(int track);
int  sndVoicePlaying(int track);
void sndMaster(float vol);
void sndUserVolumes(float music, float effects, float vehicle, float voice);   // меню Sound: громкость игрока 0..1

typedef struct {
  int musicState, ambZone, posEmitter, dialogPlaying, voices, underruns;
  int modeApplied;   // набор шин тюнера, который сейчас слышен (пауза и полосы поверх режима)
  float cpu, mixUsMax;
  const char *music, *ambience, *positional, *dialog;
} SndStats;
void sndStats(SndStats *s);
void sndMusicRelease(void);   // правка 239: музыку больше не ведёт миссия, ничего не включая (выход в главное меню)

#ifdef __cplusplus
}
#endif
#endif
