// SHAR PSP: game sound layer (stage 6). API and rules: sound.h; data: tools/sound_export.py; mixer: audio.c.
// Everything here runs on the main thread in microseconds: lookups are done once in sndInit, per-frame work is a
// few dozen gate tests (segment vs box) and one nearest-emitter search. No allocation after sndInit/sndBindObjects.
#include "sound.h"
#include "audio.h"
#include "police_sound_data.h"
#include "sverka_shared.h"   // сверка g08-sound: поверхность земли (шаги), взрыв машины
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// ---------------------------------------------------------------- pack extension (tools/sound_export.py "SXT1")
typedef struct { char name[16]; float v[5]; } XMode;                                          // 36 B
typedef struct { float x, y, z, r; unsigned char kind, pad; short res; } XObj;                 // 20 B
typedef struct { unsigned type, event; float half[3], ax[9], t[3]; char track[28]; } XZone;   // 96 B
typedef struct { float x, y, z, r, mn, mx; unsigned short track, pad; float gain; } XEmit;    // 32 B
typedef char chk_mode[sizeof(XMode) == 36 ? 1 : -1];
typedef char chk_obj[sizeof(XObj) == 20 ? 1 : -1];
typedef char chk_zone[sizeof(XZone) == 96 ? 1 : -1];
typedef char chk_emit[sizeof(XEmit) == 32 ? 1 : -1];
typedef struct { char event[28]; short track[3]; unsigned char n, kind; } XMEvt;              // 36 B: музыка миссий
typedef char chk_mevt[sizeof(XMEvt) == 36 ? 1 : -1];
enum { MEV_START, MEV_DRAMA, MEV_WIN, MEV_LOSE, MEV_SUS, MEV_TENSEC, MEV_INSERT, MEV_ZONE };   // сверка g08: INSERT — вставка поверх музыки (один проход), ZONE — особый кусок зоны (держится до следующей зоны)   // tools/sound_export.py MEV_KIND

static const XMEvt *mevts; static unsigned nmevt;
typedef struct { char room[28], track[28]; } XRamb;                                           // 56 B: атмосфера комнаты (правка 345)
typedef char chk_ramb[sizeof(XRamb) == 56 ? 1 : -1];
static const XRamb *rambs; static unsigned nramb;
static int mOwned;                 // музыку ведёт миссия (sndMusicEvent), main.c её не перебивает
static int curMode;
static int pauseMode;              // правка 87: пауза — шины «Pause» поверх всего (sndPause)
static int lbMode;                 // правка 55: полосы разговора — шины «Letterbox» поверх curMode (sndLetterbox)
static const XMode *modes; static unsigned nmode;
static const XObj *objs; static unsigned nobj;
static const XZone *zones; static unsigned nzone; static short *zoneTrack;
static const XEmit *emits; static unsigned nemit;
typedef struct { char prefix[28]; unsigned kind; float p[3], half[3], axes[9]; unsigned pad; } XDoor;
_Static_assert(sizeof(XDoor) == 96, "DORB record");
static const XDoor *doors; static unsigned ndoor;

#define SND_HEADROOM 0.7f   // master gain: see sndInit
static int ready; static char err[96];
static unsigned rng = 0x9E3779B9u;
// №685 (Роб, TEST27: «рандом звуков сломался, все NPC говорят одно и то же»): семя генератора было одно на каждый запуск (0x9E3779B9), первые реплики и треки каждой игры совпадали.
// Теперь в игре семя от времени старта; в смоуке/туре фиксированное (повторяемость), гейт tests/levels/seed685.sh. SND_SEED_OLD=1 — прежнее
#ifndef SND_SEED_OLD
#define SND_SEED_OLD 0
#endif
void audioSeed(unsigned);
void sndSeed(unsigned t) { if (SND_SEED_OLD || !t) return; rng = 0x9E3779B9u ^ (t * 2654435761u); if (!rng) rng = 1; audioSeed(t); }   // №685: семя от времени старта (в игре); смоук его не зовёт
static float frand(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng >> 8) * (1.f / 16777216.f); }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

// resource ids (looked up once)
static int rCrash[3], rCoin, rCoinLose, rJump, rStomp, rFeet[4], rLand[4], rRonks, rCarJump, rExplode, rSpray, rHydrant,
           rDoor[2], rShrub, rPed, rTree, rCrate;
static float topKmh = 120.f;
static int hAir;
static int rSiren, hSiren[MC_MAX];
static float crashCool, lastCrash;
static float objCool, lastObjV; static int lastObj = -1;   // правка 135.2: звук объекта при скрежете вдоль него — не каждый кадр
// правка 135.7: журнал эффектов (sndLogHook): событие текущего запуска — crash, obj, prop, sprop, land, air, gear, ped, wreck…
static void (*sfxLog)(const char *line);
static const char *sfxWhy = "fx";
static void sfxNote(int id, float x, float y, float z, float vol, const char *why) {
  if (!sfxLog || id < 0) return;
  char s[128]; const char *nm = audioSoundName(id);
  snprintf(s, sizeof s, "SFX %s %s %.1f %.1f %.1f vol=%.2f", nm ? nm : "?", why, x, y, z, vol);
  sfxLog(s);
}
static void audioNoteHook(int id, const char *why) { sfxNote(id, 0, 0, 0, 1.f, why); }   // audio.c: gearshift
void sndLogHook(void (*fn)(const char *)) { sfxLog = fn; audioNote = fn ? audioNoteHook : 0; }

// music
static int mState = SND_MUS_OFF, mReturn = -1;
// Правка 179 (близнец Б2, ступень 3): пока Гомер пешком, оригинал каждые 15–20 с играет новую короткую тубу (событие
// OF_explore_mission композиции, 47 туб, logic_or) — когда своя музыка молчит. Правка 182: в тоннеле и зале Stonecutters
// (атмосфера amb_stonecutters_tunnel / amb_stonecutters_hall, ворота 37/38) — Vivaldi (Enter_StoneCutters_Tunnel), вышел — как было
#define TUBA_MAX 64
// №468 (Роб: пешком музыка изредка короткими отрывками, больше слышно улицу; в машине музыка уровня всегда; туннель со своей темой — в машине
// как на диске, пешком в нём — как пешком): тубы пешком раз в 40–70 с вместо 15–20 с, Vivaldi тоннеля Stonecutters только из машины.
// MUSIC_FOOT_OLD=1 — прежнее
#ifndef MUSIC_FOOT_OLD
#define MUSIC_FOOT_OLD 0
#endif
#define TUBA_MIN (MUSIC_FOOT_OLD ? 15.f : 40.f)
#define TUBA_MAX_S (MUSIC_FOOT_OLD ? 20.f : 70.f)
static short tubaId[TUBA_MAX]; static int nTuba, tubaLast = -1, vivId = -1, vivOn; static float tubaT;
// правка 593: музыка клуба Legitimate Businessmen (L2 E7-6, L5): события 61 локаторов legit_business_music*, композиция lN_music.rms: Social_Club -> social_club.rsd, петля.
// Зона на шине музыки: вошёл (внутри объёма) — играет дорожка, вышел — обычная музыка. Громкость и переход — константы ниже (замер PS2 ps2-l2-club-music.yaml подставить). SND_CLUB_OFF=1 — как было (тихий регион)
#ifndef SND_CLUB_OFF
#define SND_CLUB_OFF 0
#endif
#ifndef CLUB_FADE_IN
#define CLUB_FADE_IN 2.0f   // [PS2 ps2-l2-club-music.yaml] переход ~8 с: фейд ~2 с + узнавание ~6 с (внешнее видео, YouTube cIcRJ1YMEjw)
#endif
#ifndef CLUB_EXIT_R
#define CLUB_EXIT_R 350   // м от центра ближайшей плиты клуба: [PS2] зона широкая, 3,7 мин езды по району; точной границы нет (оценка порта: район от (-150; 0) до (-270; -230))
#endif
#ifndef CLUB_FADE_OUT
#define CLUB_FADE_OUT 4.0f  // [PS2] тема сходит за ~4–5 с
#endif
#ifndef CLUB_GAIN
#define CLUB_GAIN 0.708f    // [PS2] клубная на ~3 дБ тише обычной
#endif
#define CLUB_EVENT 61
static int clubId = -1, clubOn;
static int onFootNow;   // Гомер пешком (sndOnFoot из main.c, в том числе во время миссии): возврат после миссии/погони — ONFOOT, не FREEROAM
enum { T_SD_INTRO, T_SD, T_SD_SUS1, T_SD_SUS2, T_SD_SUS3, T_M_PICKUP, T_M_MAIN, T_M_POS, T_M_NEG, T_M_SUS1, T_M_SUS2,
       T_KWIK, T_MUZAK, T_HR_INTRO, T_HR_MAIN, T_HR_NEG, T_FE, NT };
static const char *const tName[NT] = {"sunday_drive_intro", "sunday_drive", "sunday_drive_end_sus1",
  "sunday_drive_end_sus2", "sunday_drive_end_sus3", "collection02_pickup", "collection02_main", "collection02_end_pos",
  "collection02_end_neg", "collection02_end_sus1", "collection02_end_sus2", "kwik_e_mart_muzak", "muzak",
  "hit_run_intro", "hit_run_main", "hit_run_end_neg", "simpsons_theme"};
static int tId[NT];

// ambience / positional
static int ambZone = -1, ambTrack = -1, emitCur = -1;
static float ppx, ppy, ppz; static int havePrev;

// dialogue (Homer's in-car lines): tracks grouped by name prefix d_<event>_hom_
// Правка 345: выход из машины (W_GOC_). [PS2] своя pcsx2-twins, 05.10 (agent345/ps2 goc1, goc2: state_l1_incar, △ выход / △ посадка
// по кадрам; атлас m0d): реплика через 6–8 кадров после △ на 5 выходах из 11 (m0d 1/1; goc1 1/6 — выходы через 6 с; goc2 3/4 — через
// 40, 20 с да, через 30 с нет); строки подряд разные (legs -> walk -> horse). Правило порта: шанс GOC_CHANCE, без охлаждения
#ifndef GOC_CHANCE
#define GOC_CHANCE .5f
#endif
static const char *const sayKey[SND_SAY_COUNT] = {"bcrash", "mcrash", "damage", "burn", "dcar", "break", "air", "hitp",
  "missp", "carway", "door", "arrive", "time", "mstart", "goc", "gic",
  "card", "mvic", "mfail", "springboard", "fall", "newai", "pass", "passed", "hitbyc", "tail", "aidestroy", "turbo", "dodge", "longjump"};   // аудит 09.10 п. 12/58
// аудит 09.10: SND_VO_OLD=1 — как до аудита (новых реплик нет: красный tests/sound/vo_check.sh)
#ifndef SND_VO_OLD
#define SND_VO_OLD 0
#endif
// rules (the original's trigger code is not on the disc; these are our tuning, see docs/sound_integration.md)
// Правка 345 (PS2, своя pcsx2-twins, 05.10): MSTART — 3 старта m0 из 3 с репликой (✕ Continue на 420 -> w_mstart_hom_03, на 500 ->
// _04, атлас m0d — _03): каждый раз, выбор случайный. GOC — см. GOC_CHANCE выше
// аудит 09.10, ПРАВИЛО ПОРТА (шанса и паузы оригинала в данных нет; у PS2 записана одна реплика w_newai_hom_02 в погоне, близнец
// b2_loop кадр 11250): карточка, итог миссии, падение — всегда; подкидывающая точка, новая машина погони, обгон — через раз и не чаще
// 10–15 с (у оригинала обгон около равенства мигает 4 раза за 2 с — ps2-race-position)
// GIC chance 4/11 and cooldown0 are PORT_RULE, not a probability proved by В84.
// Extra-event chance/cooldown are port rules; event contexts must be measured before wiring unknown triggers.
static const float sayChance[SND_SAY_COUNT] = {.5f, .3f, 1.f, 1.f, 1.f, .25f, .5f, .5f, .3f, .4f, 1.f, 1.f, 1.f, 1.f, GOC_CHANCE, 4.f / 11.f,
                                               1.f, 1.f, 1.f, .5f, 1.f, .5f, .5f, .5f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
static const float sayCool[SND_SAY_COUNT] = {12, 12, 0, 0, 0, 15, 15, 10, 10, 12, 0, 0, 20, 0, 0, 0,
                                             0, 0, 0, 10, 10, 15, 10, 10, 0, 10, 0, 10, 10, 10};
#ifndef LONGJUMP_AIR_OLD
#define LONGJUMP_AIR_OLD 0
#endif
#define SAY_MAX 16   // правка 183: D_ + W_ (Bcrash 8 + 4, Break 5 + 9, HitP 5 + 8)
static short sayTrack[SND_SAY_COUNT][SAY_MAX]; static unsigned char sayN[SND_SAY_COUNT], sayLast[SND_SAY_COUNT];
static float sayT[SND_SAY_COUNT], sinceLine = 99.f;
static float vT[SND_V_COUNT] = {99, 99, 99, 99}; static int vLast = -1;   // аудит 09.10: реплики соперников (sndRivalSay)
static int getInWas = -1; // reset при sndInit, загрузка за рулём не озвучивается
// голоса улицы (правки 180/184): пулы дорожек p_<вид>_<голос>_* и d_<событие>_<голос>_* (tools/sound_export.py STREET)
static const char *const pedPre[SND_PED_KINDS] = {"p_hitbyc_", "p_nhitbyc_", "p_hitbyw_"};
static const char *const pedVoc[8] = {"mn1", "mn2", "fm1", "fm2", "by1", "by2", "gr1", "gr2"};
static const char *const drvPre[SND_DRV_KINDS] = {"d_carway_", "d_mcrash_", "d_bcrash_"};
static const char *const drvVoc[4] = {"vm1", "vm2", "vf1", "vf2"};
#define STREET_MAX 12
static short pedTrack[SND_PED_KINDS][8][STREET_MAX], drvTrack[SND_DRV_KINDS][4][STREET_MAX];
static unsigned char pedN[SND_PED_KINDS][8], drvN[SND_DRV_KINDS][4];
static int streetTrack = -1;static unsigned streetSerial;
static float streetT = 99.f;   // с последней реплики улицы, с
int sndStreetOff;   // служебное: красный контроль гейта tests/traffic/streetvoice_twins.py — голоса улицы молчат
static int dmgStage;

// ======================================================================== сверка с исходником, окно g08-sound (02.11.2026)
// Источник правды — исходник оригинала (правки Роба 1077–1081); числа и поведение из docs/handoff/СВЕРКА_ПОЛНАЯ/P09, P10, M1, M2, СВЕРКА_С_ИСХОДНИКОМ.
// Каждый выключатель *_OLD=1 возвращает прежнее поведение (список: docs/handoff/СВЕРКА_ВНЕДРЕНИЕ/g08-sound_журнал.md)
#ifndef SV_MODEMIX_OLD
#define SV_MODEMIX_OLD 0     // P09:14,15 P10-18 M2 Д1 M1 Д4: шина = минимум по включённым режимам, переход 750 мс
#endif
#ifndef SV_AMBFOOT_OLD
#define SV_AMBFOOT_OLD 0     // P09:1,28: уличная атмосфера звучит, только пока Гомер пешком или в помещении
#endif
#ifndef SV_SAYTAB_OLD
#define SV_SAYTAB_OLD 0      // P09:5,12,13,33: таблица шансов оригинала, очередь с жизнью строки, круг вариантов, лишние реплики убраны
#endif
#ifndef SV_DMGBURN_OLD
#define SV_DMGBURN_OLD 0     // P09:2,3: Damage — сильный удар о неподвижное, Burn — начало пробуксовки, Bcrash/Mcrash — удар в машину
#endif
#ifndef SV_CRASH_OLD
#define SV_CRASH_OLD 0       // P09:25,36 P09 Д1,Д2: размер crash по интенсивности (импульс/100000), 15–60 м на всех уровнях
#endif
#ifndef SV_STEP_OLD
#define SV_STEP_OLD 0        // P02-34 P09:4 P09 Д3: шаги Гомера по поверхности (2D), Turbo на 5 с рывка
#endif
#ifndef SV_STREETCH_OLD
#define SV_STREETCH_OLD 0    // P09:6: прохожие и водители говорят с шансом 20 %, позиционно 50/100 м
#endif
#ifndef SV_LISTEN_OLD
#define SV_LISTEN_OLD 0      // P09:27 P10-24: слушатель не дальше 10 м от игрока
#endif
#ifndef SV_BREAK_OLD
#define SV_BREAK_OLD 0       // P09 Д12 P11-21: Break на слом предметов (кроме стейтпропов) и взрыв машины, в том числе пешком
#endif
#ifndef SV_VOICE_OLD
#define SV_VOICE_OLD 0       // P09:7: голос прохожего по таблице моделей оригинала (модель вне таблицы молчит)
#endif
#ifndef SV_ROOM_OLD
#define SV_ROOM_OLD 0        // P09:17, P09 Д9: комнаты вне списка — атмосфера и музыка дома; после выхода сразу туба прогулки
#endif
#ifndef SV_RANGE_OLD
#define SV_RANGE_OLD 0       // P10-33: дальность по умолчанию 3–100 м; suspension_ronks — P09:30
#endif
#ifndef SV_MUSZONE_OLD
#define SV_MUSZONE_OLD 0     // P09:18,31,38 Д3 Д4: особые куски музыки по зонам атмосферы, вставки (карточка, фанфары)
#endif
#define SV_MASS_KG 1500.f    // масса машины для интенсивности удара (P01-13: e≈0,5, масса 1500; 19000 Н·с ≈ 8 м/с)
static float svCarMass = SV_MASS_KG;
#ifndef SV_MASS_OLD
#define SV_MASS_OLD 0   // круг 6 (g08, P09:25): масса машины для размера удара — своя у каждой машины (car.p.mass); 1 — всегда 1500 кг
#endif
void sndCarMass(float kg) { svCarMass = !SV_MASS_OLD && kg > 100.f ? kg : SV_MASS_KG; }
static float impI(float v) { return v * svCarMass * 1.5f * (1.f / 100000.f); }   // 0..1 по шкале оригинала (1 = импульс 100000)
// очередь реплик героя (P09:12): до 16 мест, строка ждёт жизнь по таблице и пропадает; одинаковую реплику второй раз не ставят
#define SQ_N 16
static struct { short ev; short tier; float life; } sq[SQ_N]; static int sqn;   // tier: 0 случайная, 2 обязательная (события без броска)
static float stepLastT, stepTurboT, stepClock; static int turboSaid;
static unsigned lastBoomSeq; static float boomT = 99.f;
static int emitMute;
#ifndef SV_POSLOOP_OLD
#define SV_POSLOOP_OLD 0
#endif
static int ambLatchSp;
static int specialOn, houseMus, alwaysMus; static float zoneAt[128]; static int zoneLast = -1;   // особые куски музыки по зонам, музыка дома
void sndMusicAlways(int on) { alwaysMus = !!on; }   // этап «музыка всегда» (msn_audio): после выхода из машины фон улицы не возвращается
static float busNow[5]; static int busLive, modeSnap, storeKind, justMusicOn, loadDuckOn; static unsigned modeClkUs;   // шины режимов
static float ambFootG = 1.f; static int ambLatch;   // фон улицы только пешком
#define IMP_EVENT 0.06f      // ниже события нет
#define IMP_MEDIUM 0.15f
#define IMP_LARGE 0.25f

// bound object sets
static short *setRes[SND_NSET]; static unsigned setN[SND_NSET];

const char *sndError(void) { return err; }
int sndReady(void) { return ready; }

static void parseExt(void) {
  unsigned size; const unsigned char *e = audioBankExt(&size);
  if (!e || size < 8 || memcmp(e, "SXT1", 4)) return;
  unsigned ns = *(const unsigned *)(e + 4);
  for (unsigned i = 0; i < ns && 8 + i * 16 + 16 <= size; i++) {
    const unsigned char *s = e + 8 + i * 16;
    unsigned off = *(const unsigned *)(s + 4), cnt = *(const unsigned *)(s + 8), stride = *(const unsigned *)(s + 12);
    if (off + cnt * stride > size) continue;
    const void *p = e + off;
    if (!memcmp(s, "MODE", 4) && stride == 36) { modes = p; nmode = cnt; }
    else if (!memcmp(s, "OBJS", 4) && stride == 20) { objs = p; nobj = cnt; }
    else if (!memcmp(s, "ZONE", 4) && stride == 96) { zones = p; nzone = cnt; }
    else if (!memcmp(s, "EMIT", 4) && stride == 32) { emits = p; nemit = cnt; }
    else if (!memcmp(s, "MEVT", 4) && stride == 36) { mevts = p; nmevt = cnt; }
    else if (!memcmp(s, "DORB", 4) && stride == 96 && !SND_VO_OLD) { doors = p; ndoor = cnt; }
    else if (!memcmp(s, "RAMB", 4) && stride == 56) { rambs = p; nramb = cnt; }   // правка 345: атмосфера комнат
  }
  if (nzone) {
    zoneTrack = malloc(nzone * sizeof *zoneTrack);
    if (!zoneTrack) nzone = 0;
    for (unsigned i = 0; i < nzone; i++) {
      char nm[29]; memcpy(nm, zones[i].track, 28); nm[28] = 0;
      zoneTrack[i] = (short)audioTrack(nm);
    }
  }
}

#ifndef SND_250_OLD
#define SND_250_OLD 0
#endif
static int s250 = !SND_250_OLD;   // правка 250 на этом уровне (sndInit: только audio/ уровня 1)
static void applyMode(int m);
static int lastApplied = -1, applyCalls; static float lastAppliedMus = -1.f;   // правка 279: служебно
static int levelOfDir(const char *dir) {   // «…/l3/audio/» -> 3, «…/audio/» уровня 1 -> 1 (viewer/main.c levelFile)
  for (const char *p = dir; p && *p; p++) if ((p == dir || p[-1] == '/') && p[0] == 'l' && p[1] >= '2' && p[1] <= '7' && p[2] == '/') return p[1] - '0';
  return 1;
}
const char *sndLevelChangeStinger(void) { return NEWSMUS1128_OLD ? "newspaper_spin" : "loading_screen"; }
int sndVariantPick(int n, unsigned clockUs) {
  if (n <= 1) return 0;
  if (NEWSMUS1128_OLD) return rand() % n;
  unsigned x = clockUs * 2654435761u; x ^= x >> 15; x *= 2246822519u; x ^= x >> 13; return (int)(x % (unsigned)n);   // перемешанные часы: разные запуски и разные переходы дают разные варианты
}
int sndLang; static int sndLangApplied; static char sndAudioDir[256];
int audioLangAvail(const char *dir, int lang);
void sndSetLanguage(int lang) { sndLang = lang >= 0 && lang <= 3 ? lang : 0; }
int sndLangAvail(int lang) { return lang == 0 ? 1 : (LANGVO1111_OLD || lang < 0 || lang > 3 || !sndAudioDir[0]) ? 0 : audioLangAvail(sndAudioDir, lang); }
int sndLangPending(void) { return !LANGVO1111_OLD && sndLang != sndLangApplied; }
int sndInit(const char *dir) {
  ready = 0; snprintf(sndAudioDir, sizeof sndAudioDir, "%s", dir); audioSetLanguage(LANGVO1111_OLD ? 0 : sndLang); sndLangApplied = sndLang;
  s250 = !SND_250_OLD && levelOfDir(dir) == 1; audioMix250(s250);   // правка 250: сведение по замерам уровня 1 — только на уровне 1
  if (audioInit(dir) < 0) { snprintf(err, sizeof err, "%s", audioError()); return -1; }
  snprintf(err, sizeof err, "%s", audioError());   // "music.sap not found: effects only" is not fatal
  static const char *const crash[3] = {"small_car_crash", "medium_car_crash", "large_car_crash"};
  static const char *const feet[4] = {"feet_concrete", "feet_metal", "feet_wood", "feet_dirt"};
  static const char *const land[4] = {"feet_concrete_jump", "feet_metal_jump", "feet_wood_jump", "feet_dirt_jump"};
  for (int i = 0; i < 3; i++) rCrash[i] = audioFind(crash[i]);
  for (int i = 0; i < 4; i++) { rFeet[i] = audioFind(feet[i]); rLand[i] = audioFind(land[i]); }
  rCoin = audioFind("coin_collect_01"); rCoinLose = audioFind("coin_lose"); rJump = audioFind("jump");
  rSiren = audioFind("siren"); memset(hSiren,0,sizeof hSiren);
  rStomp = audioFind("homer_stomp"); rRonks = audioFind("suspension_ronks"); rCarJump = audioFind("car_jump");
  rExplode = audioFind("generic_car_explode"); rSpray = audioFind("hydrant_spray"); rHydrant = audioFind("car_hit_hydrant");
  rShrub = audioFind("hit_shrub"); rPed = audioFind("car_hit_pedestrian"); rTree = audioFind("car_hit_tree"); rCrate = audioFind("car_hit_crate");   // правка 135.5 / 135.6
  rDoor[0] = audioFind("famil_close"); rDoor[1] = audioFind("famil_open");   // not on the disc: stay -1 (silent)
  for (int i = 0; i < NT; i++) tId[i] = audioTrack(tName[i]);
  nTuba = 0; vivId = audioTrack("vivaldi"); clubId = SND_CLUB_OFF ? -1 : audioTrack("social_club"); clubOn = 0;   // правки 179 и 182 (tools/sound_export.py music_table)
  for (int t = 0, n = audioTrackCount(); t < n && nTuba < TUBA_MAX; t++) {
    const char *nm = audioTrackName(t);
    if (!strcmp(nm, "of_explore") || !strncmp(nm, "of_explore~", 11)) tubaId[nTuba++] = (short)t;
  }
  tubaT = TUBA_MIN + frand() * (TUBA_MAX_S - TUBA_MIN); tubaLast = -1; vivOn = 0;
  // Семейства вождения принадлежат герою уровня; код берём из его стартовой реплики
  char sayCode[8] = "hom";
#ifndef SOUND_DRIVER_OLD
  for (int t = 0, n = audioTrackCount(); t < n; t++) {
    const char *nm = audioTrackName(t);
    if (audioTrackBus(t) == 2 && !strncmp(nm, "w_mstart_", 9)) {
      const char *end = strchr(nm + 9, '_'); size_t len = end ? (size_t)(end - nm - 9) : 0;
      if (len && len < sizeof sayCode) { memcpy(sayCode, nm + 9, len); sayCode[len] = 0; break; }
    }
  }
#endif
  getInWas = -1;
  memset(sayN, 0, sizeof sayN);
  for (int t = 0, n = audioTrackCount(); t < n; t++) {
    if (audioTrackBus(t) != 2) continue;
    const char *nm = audioTrackName(t);
    for (int e = 0; e < SND_SAY_COUNT; e++) {   // правка 183: реплики события — из наборов D_ и W_ (оригинал читает оба, близнец Б2)
      char pre[24]; snprintf(pre, sizeof pre, "d_%s_%s_", sayKey[e], sayCode);
      int hit = !strncmp(nm, pre, strlen(pre)); pre[0] = 'w'; hit |= !strncmp(nm, pre, strlen(pre));
      if (e >= SND_SAY_MSTART) { snprintf(pre, sizeof pre, "w_%s_", sayKey[e]); hit = !strncmp(nm, pre, strlen(pre)); }   // правка 345: водитель уровня (пакет кладёт только его)
      if (hit && SND_VO_OLD && e >= SND_SAY_CARD) hit = 0;
      if (hit && sayN[e] < SAY_MAX) { sayTrack[e][sayN[e]++] = (short)t; break; }
    }
  }
  // №467: у диска нет группы W_LongJump (в пакете пусто, реплика молчала) — длинный прыжок пешком говорит W_Air (те же дорожки, что у события air). ПРАВИЛО ПОРТА; LONGJUMP_AIR_OLD=1 — прежнее
  if (!LONGJUMP_AIR_OLD && !sayN[SND_SAY_LONGJUMP])
    for (int t = 0, n = audioTrackCount(); t < n; t++)
      if (audioTrackBus(t) == 2 && !strncmp(audioTrackName(t), "w_air_", 6) && sayN[SND_SAY_LONGJUMP] < SAY_MAX) sayTrack[SND_SAY_LONGJUMP][sayN[SND_SAY_LONGJUMP]++] = (short)t;
  for (int e = 0; e < SND_SAY_COUNT; e++) { sayT[e] = 99.f; sayLast[e] = 0xFF; }
  for (int k = 0; k < SND_V_COUNT; k++) vT[k] = 99.f;
  vLast = -1;
  memset(pedN, 0, sizeof pedN); memset(drvN, 0, sizeof drvN); streetT = 99.f; streetTrack = -1;
  for (int t = 0, n = audioTrackCount(); t < n; t++) {   // правки 180/184: голоса улицы
    if (audioTrackBus(t) != 2) continue;
    const char *nm = audioTrackName(t); char pre[24];
    for (int k = 0; k < SND_PED_KINDS; k++) for (int v = 0; v < 8; v++) {
      snprintf(pre, sizeof pre, "%s%s_", pedPre[k], pedVoc[v]);
      if (!strncmp(nm, pre, strlen(pre)) && pedN[k][v] < STREET_MAX) pedTrack[k][v][pedN[k][v]++] = (short)t;
    }
    for (int k = 0; k < SND_DRV_KINDS; k++) for (int v = 0; v < 4; v++) {
      snprintf(pre, sizeof pre, "%s%s_", drvPre[k], drvVoc[v]);
      if (!strncmp(nm, pre, strlen(pre)) && drvN[k][v] < STREET_MAX) drvTrack[k][v][drvN[k][v]++] = (short)t;
    }
  }
  doors = 0; ndoor = 0; parseExt();
  audioMaster(SND_HEADROOM);   // the disc levels sum above full scale (engine + music + ambience + hits)
  mState = SND_MUS_OFF; mReturn = -1; ambZone = -1; ambTrack = -1; emitCur = -1; havePrev = 0; dmgStage = 0;
  mOwned = 0; curMode = SND_MODE_NORMAL; lbMode = 0; pauseMode = 0;
  busLive = 0; storeKind = justMusicOn = loadDuckOn = 0; ambFootG = 1.f; ambLatch = 0; sqn = 0; stepLastT = -99.f; stepTurboT = 0.f; stepClock = 0.f;
  ready = 1;
  // правка 279 (страховка): шины пакета до первого sndMode — музыка 0; пути запуска 263 (sndInit без sndMode) полагались на
  // поздний вызов sndMode. Запуск звука сам ставит набор тюнера «Normal»
  applyMode(SND_MODE_NORMAL);
  return 0;
}

void sndShutdown(void) {
  if (!ready) return;
  audioShutdown(); ready = 0;
  for (int s = 0; s < SND_NSET; s++) { free(setRes[s]); setRes[s] = 0; setN[s] = 0; }
  free(zoneTrack); zoneTrack = 0; nzone = 0;
  mevts = 0; nmevt = 0; mOwned = 0; rambs = 0; nramb = 0;
}

// ---------------------------------------------------------------- effects
int sndId(const char *name) { return ready ? audioFind(name) : -1; }
int sndPlayId(int id, float x, float y, float z, float vol) {
  if (!ready || id < 0 || vol <= 0) return 0;
  const float mn = SV_RANGE_OLD ? 4.f : 3.f, mx = SV_RANGE_OLD ? 60.f : 100.f;   // сверка g08 (P10-33): без своей настройки — 3–100 м
  float g, pan; audioSpatial(x, y, z, mn, mx, &g, &pan);
  g *= vol; if (g <= 0.005f) return 0;
  sfxNote(id, x, y, z, clampf(g, 0, 1), sfxWhy);
  return audioPlayAtGain(id,clampf(vol,0,1),x,y,z,mn,mx);
}
static int playW(const char *why, int id, float x, float y, float z, float vol) {   // запуск с событием для журнала 135.7
  sfxWhy = why; int h = sndPlayId(id, x, y, z, vol); sfxWhy = "fx"; return h;
}
int sndPlay(const char *name, float x, float y, float z, float vol) { return sndPlayId(sndId(name), x, y, z, vol); }
int sndPlay2D(int id, float vol) {
  if (!ready || id < 0) return 0;
  sfxNote(id, 0, 0, 0, clampf(vol, 0, 1), sfxWhy[0] == 'f' && sfxWhy[1] == 'x' ? "2d" : sfxWhy);
  return audioPlay(id, clampf(vol, 0, 1), 0, 1.f);
}
int sndPlayUi(int id, float vol) { return ready && id >= 0 ? audioPlayBus(id, clampf(vol, 0, 1), 0, 1.f, AUDIO_BUS_UI) : 0; }   // правка 114
void sndStop(int h) { if (ready) audioStop(h); }

// ---------------------------------------------------------------- car
void sndCarSetup(float topSpeedMs) { topKmh = topSpeedMs > 1.f ? topSpeedMs * 3.6f : 120.f; }
void sndCarPosition(float x,float y,float z,int outside) { if(ready)audioEnginePosition(x,y,z,outside); }
static float engSpd, engThr;
void sndEngine(float speedMs, float throttle, unsigned flags) {
  if (!ready) return;
  engSpd = speedMs; engThr = throttle;
  unsigned f = (flags & SND_ENG_ON ? AUDIO_ENGINE_ON : 0) | (flags & SND_ENG_REVERSE ? AUDIO_ENGINE_REVERSE : 0) |
               (flags & SND_ENG_DAMAGED ? AUDIO_ENGINE_DAMAGED : 0);
  audioEngine(speedMs * 3.6f, topKmh, clampf(throttle, 0, 1), f);
}
void sndHorn(int on) { if (ready) audioHorn(on); }
// сверка g08-sound (P10-5,8,10,11,15,16, P09:2): состояние машины игрока раз в кадр до sndEngine. topSpeedMs — TopSpeed именно этой машины (шкала передач);
// skid01 — уровень заноса/буксования (skidSoundLevel); air — все колёса вне земли; parked — игрок вышел (после конца анимации выхода: только холостой
// клип, 15/50 м); life01 — остаток жизни машины 0..1. Буксование с места = занос при скорости < 3 м/с и газе > 0,3; его начало — реплика Burn (15 %)
void sndBurnLevel(float level01) { audioEngineBurn(level01); }
void sndCarState(float topSpeedMs, float skid01, int air, int parked, float life01, int burnout, int brakeOrRev) {
  static int skidWas;
  if (!ready) return;
  if (topSpeedMs > 1.f) topKmh = topSpeedMs * 3.6f;
  int spin = skid01 > 0.02f && fabsf(engSpd) < 3.f && engThr > 0.3f;
  audioEngineEx(skid01, spin, air, parked, life01);
  int now = burnout && !brakeOrRev && !parked;   // круг 3 (проверка P09:2): бернаут из данных машины (полный газ ниже доли BurnoutRange), тормоз и задний ход подавляют
  if (!SV_DMGBURN_OLD && now && !skidWas) sndSay(SND_SAY_BURN, 0);   // P09:2: Burn — начало пробуксовки (газ), 15 %
  skidWas = now;
}
#ifndef POLICE_SIREN_OLD
#define POLICE_SIREN_OLD 0
#endif
void sndPoliceCars(const MsnCars *cars) {
  if (!ready) return;
  for (int i=0;i<MC_MAX;i++) {
    const McCar *c=cars && i<cars->n ? &cars->c[i] : NULL;
    int active=!POLICE_SIREN_OLD && rSiren>=0 && c && c->on && c->police && !c->car.destroyed;
    if (active) {
      if (!audioPlaying(hSiren[i])) {
        sfxNote(rSiren,c->car.x,c->car.ry,c->car.z,1.f,"police");
        hSiren[i]=audioPlayAtGain(rSiren,1.f,c->car.x,c->car.ry,c->car.z,POLICE_SIREN_MIN,POLICE_SIREN_MAX);
      } else audioSetAt(hSiren[i],c->car.x,c->car.ry,c->car.z,POLICE_SIREN_MIN,POLICE_SIREN_MAX);
    } else if (hSiren[i]) { audioStop(hSiren[i]); hSiren[i]=0; }
  }
}
void sndMiniGame(int on) {
  static const char *const normal[3]={"small_car_crash","medium_car_crash","large_car_crash"};
  static const char *const mini[3]={"small_crash","med_crash","lrg_crash"};
  if(ready)for(int i=0;i<3;i++)rCrash[i]=audioFind(on?mini[i]:normal[i]);
}
void sndSkid(float a, int dirt) { if (ready) audioSkid(clampf(a, 0, 1), dirt); }
// правка 413а: мотор, занос, гудок — профилем машины, которую ведёт игрок (имя .con; нет в пакете — машина уровня). Раз в кадр до sndEngine
void sndPlayerCar(const char *name) { static char last[16]; static int lastP = -2; if (!ready) return;
  int p = audioPlayerCar(name);   // до 10 профилей по имени — дешевле, чем следить за сменой пакета
  if (p != lastP || strncmp(last, name, sizeof last)) { lastP = p; strncpy(last, name, sizeof last);
    if (sfxLog) { char m[80]; snprintf(m, sizeof m, "CARSND car=%.15s profile=%d", name, p); sfxLog(m); } } }
void sndCarSndOld(int on) { audioCarSndOld(on); }
void sndPlayerCarState(const char *name, int wrecked) {
#ifdef CAR_WRECK_AUDIO_OLD
  wrecked = 0;
#endif
  /* The rendered/drivable wreck is huskA, loaded by every level.mfk. */
  sndPlayerCar(wrecked ? "huskA" : name);
}
// Правка Роба 135.9 (01.10 ~09:40): «гул, как будто вращение колёс, когда прыгаю с трамплина где угодно — этого звука быть не должно».
// Петля car_jump в полёте была гипотезой порта (в данных оригинала не привязана к полёту) — выключена: SND_CAR_AIR 0
#ifndef SND_CAR_AIR
#define SND_CAR_AIR 0
#endif
void sndCarAir(int on) {
  if (!ready || rCarJump < 0) return;
  if (SND_CAR_AIR && on && !audioPlaying(hAir)) { sfxNote(rCarJump, 0, 0, 0, 0.6f, "air"); hAir = audioPlay(rCarJump, 0.6f, 0, 1.f); }
  else if (!on && hAir) { audioStop(hAir); hAir = 0; }
}
void sndCarLand(float v) {
  static float lastRonks = -99.f;
  if (!SV_RANGE_OLD) {   // сверка g08 (P09:30): suspension_ronks — на проседание подвески после полёта, не чаще раза в 0,5 с, 2D, всегда полной громкости
    if (ready && v > 4.f && stepClock - lastRonks >= 0.5f) { lastRonks = stepClock; sfxWhy = "land"; sndPlay2D(rRonks, 1.f); sfxWhy = "fx"; }
    return;
  }
  if (ready && v > 4.f) { sfxWhy = "land"; sndPlay2D(rRonks, clampf(0.4f + (v - 4.f) / 8.f, 0, 1)); sfxWhy = "fx"; }
}

// статичный объект уровня (kind InstStatPhys: деревья, столбы, кульверты…) со звуком, которому принадлежит примитив COL1
// с центром x,y,z. Правка Роба 135.2: звук объекта принадлежит ему (CollisionEffect 0x3000600 его InstStatPhys, данные диска),
// поэтому звучит только при контакте с его собственным примитивом: центр примитива в r (1.5 м, tools/sound_export.py) от
// объекта. Раньше — любой удар (стена дома, земля, другая машина) в r + 2 = 3.5 м от центра машины: шорох изгороди, стук
// столба, «бонг» кульверта поверх удара о соседний дом. ПРАВИЛО ПОРТА по данным (кода движка на диске нет)
static int staticObjAt(float x, float y, float z) {
  int best = -1; float bd = 1e9f;
  for (unsigned i = 0; i < nobj; i++) {
    const XObj *o = &objs[i];
    if (o->kind != 2 || o->res < 0) continue;
    float dx = o->x - x, dz = o->z - z, dy = o->y - y, d = dx * dx + dz * dz;
    if (dy > 12.f || dy < -6.f) continue;
    float lim = o->r;
    if (d < lim * lim && d < bd) { bd = d; best = (int)i; }
  }
  return best;
}

// Правка Роба 250 (03.10, «эффекты слишком громкие — свести, как в оригинале», видео IMG_7707 2:06–2:13). Удары машины игрока —
// металлический crash и звук задетого или сбитого предмета (collide.spt) — по замерам оригинала (гейт tests/sfx250):
//  - клип: таран машиной 10,5 м/с у оригинала — small_car_crash (выход SPU2 близнеца, build/twins/o4/v13_wreck, тараны 12 и 13);
//    в записи PS2 (43 мин, скан клипов диска) small — 52 удара (в том числе о деревья и рекламные щиты), medium — 3 (сильные удары в
//    машины), large — 8 проверенных (телефонные будки, бельевая верёвка, тараны грузовиком-снегоуборщиком и внедорожником): около
//    80 % ударов — small. У порта medium звучал с 6 м/с, large с 12 м/с — больше половины ударов (журнал Роба 03.10: 5 small,
//    5 medium, 1 large за 80 с), а эти клипы длиннее и плотнее (энергия medium в 2,4 раза, large в 7 раз больше small). Пороги ×2:
//    medium с 12, large с 24 м/с — ПРАВИЛО ПОРТА (как оригинал выбирает размер — по силе, массе или предмету — вопрос близнецу);
//  - громкость: множитель клипа к множителю музыки у оригинала 0,457 при 10,5 м/с (−6,8 дБ; тараны 0,431 и 0,483); в записи PS2
//    26 ударов small: медиана 0,63 (−4,0 дБ; 13 ударов 0,58–0,67, 6 — 0,69–0,76), слабые — 0,40–0,51 (7); medium и large там же
//    (0,45–0,74); фонарь, сбитый машиной, — только свой клип, 0,52 и 0,53. У порта было 0,45 + s/20 при 4–60 м: полная громкость
//    уже с 11 м/с (0,669 при 10,5 — на 3,3 дБ громче). Теперь громкость растёт с силой удара и упирается в медиану оригинала:
//    0,745 × min(1, s/14,5) (0,457 при 10,5 м/с, 0,63 с 14,5 м/с) — ПРАВИЛО ПОРТА по замерам;
//  - расстояние: positionalsettings.spt collision_sounds — полная громкость до 15 м, тишина за 60 м (диск); было 4–60 м, как у всех;
//  - реплики Гомера на удар — прежние пороги 6 и 12 м/с (голоса не трогаем); пешком (пинок Гомера) — прежняя громкость.
// SND_250_OLD=1 — прежнее сведение (красный контроль гейта)
// Второй проход 04.10 (перепроверка 03.10): только уровень 1 (s250 — пакет звука audio/ уровня 1, у L2–L7 прежнее, пока они «потом»);
// неподвижные предметы (деревья, щиты — «obj») — прежнее расстояние 4–60 м (у оригинала удар о дерево в записи PS2 не найден ни разу:
// громче делать нельзя), слабые касания — не тише 0,47 (= 0,40 музыки, слабые удары small в записи PS2 0,40–0,51)
#ifndef SND_250_OLD
#define SND_250_OLD 0
#endif
#ifndef COLL_FULL
#define COLL_FULL 14.5f    // сила удара (м/с), с которой громкость упирается в потолок
#endif
#ifndef COLL_VMAX
#define COLL_VMAX 0.745f   // потолок: 0,63 музыки / (трим 0,75 × шина эффектов 0,88 / шина музыки 0,78)
#endif
#define COLL_FLOOR 0.47f   // 0,40 музыки: слабые удары оригинала (запись PS2, 7 ударов 0,40–0,51)
#define COLL_MIN 15.f      // positionalsettings.spt collision_sounds: SetMinDistance 15, SetMaxDistance 60
#define COLL_MAX 60.f
static int cs250(void) { return s250 || !SV_CRASH_OLD; }   // сверка g08 (P09:25): расстояние 15–60 м и сведение удара — на всех уровнях
static float collVol(float s, float oldBase, float oldDiv) {   // громкость удара машины силой s (м/с) до расстояния
  if (!cs250()) return clampf(oldBase + s / oldDiv, 0, 1);
  float v = COLL_VMAX * clampf(s / COLL_FULL, 0, 1); return v < COLL_FLOOR ? COLL_FLOOR : v;
}
static int collPlay(const char *why, int id, float x, float y, float z, float vol, int nearDisc) {   // nearDisc: расстояние диска 15–60 м
  if (!cs250() || !nearDisc) return playW(why, id, x, y, z, vol);
  if (!ready || id < 0 || vol <= 0) return 0;
  float g, pan; audioSpatial(x, y, z, COLL_MIN, COLL_MAX, &g, &pan);
  g *= vol; if (g <= 0.005f) return 0;
  sfxNote(id, x, y, z, clampf(g, 0, 1), why);
  return audioPlayAtGain(id,clampf(vol,0,1),x,y,z,COLL_MIN,COLL_MAX);
}

// металлический удар (small/medium/large_car_crash = *_vehicle_crash_01..03, collide.spt) в точке контакта + реплики
#ifndef SV_CRASH7_OLD
#define SV_CRASH7_OLD 0   // круг 7: 1 — границы размера удара нестрогие (>=)
#endif
static int crashSize(float s) {   // 0 small, 1 medium, 2 large
  if (SV_CRASH_OLD) return s250 ? (s >= 24.f ? 2 : s >= 12.f ? 1 : 0) : (s >= 12.f ? 2 : s >= 6.f ? 1 : 0);   // правка 250: пороги ×2 (правило порта)
  float it = impI(s); return SV_CRASH7_OLD ? (it >= IMP_LARGE ? 2 : it >= IMP_MEDIUM ? 1 : 0) : (it > IMP_LARGE ? 2 : it > IMP_MEDIUM ? 1 : 0);   // круг 7: границы строгие (> 0,15 medium, > 0,25 large)   // сверка g08 (P09:25): до 0,15 small, 0,15–0,25 medium, выше 0,25 large
}
static float crashMin(void) { return SV_CRASH_OLD ? 2.f : IMP_EVENT / (svCarMass * 1.5f * (1.f / 100000.f)); }   // события нет ниже интенсивности 0,06 (≈ 2,7 м/с)
static void crash(float s, float x, float y, float z) {
  if (!ready || s < crashMin()) return;
  if (crashCool > 0 && s < lastCrash * 1.5f) return;       // scraping along a wall: not a new crash every frame
  crashCool = 0.25f; lastCrash = s;
  int size = crashSize(s);
  collPlay("crash", rCrash[size], x, y, z, collVol(s, 0.45f, 20.f), 1);
  if (SV_DMGBURN_OLD) { if (s >= 12.f) sndSay(SND_SAY_BCRASH, 0); else if (s >= 6.f) sndSay(SND_SAY_MCRASH, 0); }   // реплики — прежние пороги (правка 250); новое — по цели удара ниже
}
void sndCarHit(float s, float x, float y, float z) { crash(s, x, y, z); }
// Правка Роба 135.2 / 135.4 / 135.5: удар машины игрока о мир. crash — только от твёрдого (hardV): посадка (днище о землю) сюда
// не приходит, её звук — suspension_ronks (sndCarLand, homer_car_land по имени файла на диске); листва (FOLIAGE, у изгородей и
// кустов атрибут hit_shrub) не гремит железом, а шуршит своим клипом. ПРАВИЛО ПОРТА по данным (имена клипов и атрибуты объектов)
void sndCarHitWorld(float hardV, float softV, float px, float py, float pz, int prim, float ox, float oy, float oz) {
  if (!ready) return;
  crash(hardV, px, py, pz);
  if (!SV_DMGBURN_OLD && impI(hardV) >= IMP_MEDIUM) sndSay(SND_SAY_DAMAGE, 0);   // сверка g08 (P09:2,3): сильный удар (≥ 0,15) о неподвижное — Damage, 20 %
  float v = hardV > softV ? hardV : softV;
  if (v < 2.f) return;
  int o = prim ? staticObjAt(ox, oy, oz) : -1;
  int r = o >= 0 ? objs[o].res : softV >= 2.f && softV >= hardV ? rShrub : -1;
  if (r < 0) return;
  if (objCool > 0 && o == lastObj && v < lastObjV * 1.5f) return;   // скрежет вдоль того же объекта
  objCool = 0.25f; lastObjV = v; lastObj = o;
  float x = o >= 0 ? objs[o].x : px, y = o >= 0 ? objs[o].y : py, z = o >= 0 ? objs[o].z : pz;
  collPlay("obj", r, x, y, z, collVol(v, 0.5f, 15.f), 0);   // правка 250: громкость как удар машины, расстояние прежнее (деревья не громче)
  // №459 (Роб: удар о дерево должен звучать глухим «тук» с треском): к глухому клипу дерева car_hit_tree добавляется древесный треск
  // деревянного клипа диска car_hit_crate, вполовину тише. Подбор по именам клипов диска, на слух не проверен. TREE_SND_OLD=1 — прежнее
#ifndef TREE_SND_OLD
#define TREE_SND_OLD 0
#endif
  if (!TREE_SND_OLD && r >= 0 && r == rTree && rCrate >= 0) collPlay("obj", rCrate, x, y, z, .5f * collVol(v, 0.5f, 15.f), 0);
}
void sndCarHitCar(float s, float x, float y, float z) {   // правка 135.3: машина о машину — без объектных звуков
  if (!ready || s < crashMin()) return;
  crash(s, x, y, z);
  if (SV_DMGBURN_OLD) { sndSay(SND_SAY_CARWAY, 0); return; }
  // сверка g08 (P09:3,29): удар в машину — слабый (0,01–0,15) Mcrash 20 %, сильный (≥ 0,15) Bcrash без броска; CarWay — не удар, а пробка (событие трафика)
  sndSay(impI(s) >= IMP_MEDIUM ? SND_SAY_BCRASH : SND_SAY_MCRASH, 0);
}
// правка 135.6: события трафика (связь по именам ресурсов collide.spt / dialog.spt): car_hit_pedestrian = car_hit_body_01..03
// (удар по телу) + реплика D_HitP_Hom; разбитая машина трафика — generic_car_explode (car_explosion_*). ПРАВИЛО ПОРТА по именам
static float hitpWait;   // секунд, пока герой ещё ждёт конца реплики прохожего, чтобы сказать w_hitp (№183)
void sndPedHit(float x, float y, float z) {
  if (!ready) return;
  playW("ped", rPed, x, y, z, 1.f);
  if (onFootNow) return;
  // №183/№685 (журнал Персонажей 17.10: STREETSAY p_hitbyc_* занимает поток диалога и ставит паузу 5 с, поэтому w_hitp героя при наезде не звучала ни разу):
  // реплика прохожего идёт раньше, реплика героя ждёт её конца (до 4 с) и звучит следом. SND_HITP_QUEUE_OLD=1 — прежнее (герой молчит, если прохожий кричит)
#ifndef SND_HITP_QUEUE_OLD
#define SND_HITP_QUEUE_OLD 0
#endif
  if (!sndSay(SND_SAY_HITP, 0) && !SND_HITP_QUEUE_OLD && audioStreamTrack(AUDIO_ST_DIALOG) >= 0) hitpWait = 4.f;
}
// сверка g08 (P10-32, P09 Д11/Д12/Д19): взрыв машины трафика или ИИ — generic_car_explode по collision_sounds (15–60 м); на взрыв любой машины
// пешком герой говорит Dcar, Break (20 %) — на любой взрыв. Противоречие частей: P09 Д19 («для трафика отдельного клипа нет») и P10-32 — оставлен клип, дальность 15–60
static void boomReplies(void) {
  if (SV_BREAK_OLD) return;
  if (onFootNow) sndSay(SND_SAY_DCAR, 0);
  sndSay(SND_SAY_BREAK, 0);
}
void sndTrafficWreck(float x, float y, float z) {
  if (!ready) return;
  if (!SV_CRASH_OLD && boomT < 0.3f) return;   // тот же взрыв уже озвучен событием svBoom (sverka_shared.h)
  if (SV_CRASH_OLD) playW("wreck", rExplode, x, y, z, 1.f); else collPlay("wreck", rExplode, x, y, z, 1.f, 1);
  if (boomT > 0.3f) boomReplies(); boomT = 0.f;
}
#ifndef BURN_REPEAT_OLD
#define BURN_REPEAT_OLD 0
#endif
static int burnRepeatOld = BURN_REPEAT_OLD; static float burnT;   // секунд до следующей реплики горящей машины
void sndBurnRepeatOld(int on) { burnRepeatOld = on; }
static void burnNext(void) { burnT = 22.f + frand() * 7.f; }
void sndCarDamage(float d) {
  if (!ready) return;
  if (!SV_DMGBURN_OLD) return;   // сверка g08 (P09:2): от степени разрушения и огня реплик Damage/Burn нет
  if (d < 0.05f) { dmgStage = 0; return; }                 // repaired
  int st = d >= .85f ? 3 : d >= .6f ? 2 : d >= .33f ? 1 : 0;
  if (st > dmgStage) { sndSay(st == 3 ? SND_SAY_BURN : SND_SAY_DAMAGE, 1); dmgStage = st; if (st == 3) burnNext(); }
  // №180/№184 (запись PS2 ps2_dlg_homer_drive, 17.10): горящая машина — W_Burn_Hom_04 повторяется каждые 22–29 с, пока условие держится (6,25 / 31,48 / 53,86 / 82,96 с,
  // сила 0,83–0,86; тот же вариант); у порта реплика звучала один раз при переходе в стадию. Повтор — только в машине; BURN_REPEAT_OLD=1 / flag burnrepold 1 — прежнее
  if (!burnRepeatOld && st == 3 && dmgStage == 3 && !onFootNow && burnT <= 0.f) { sndSay(SND_SAY_BURN, 1); burnNext(); }
}
void sndCarExplode(float x, float y, float z) {
  if (!ready) return;
  playW("explode", rExplode, x, y, z, 1.f);
  sinceLine = 99.f; sndSay(SND_SAY_DCAR, 1);
  if (!SV_BREAK_OLD && boomT > 0.3f) sndSay(SND_SAY_BREAK, 0);   // сверка g08 (P09 Д12): Break на взрыв любой машины
  boomT = 0.f;
}
void sndCarDoor(int open) { if (ready) sndPlay2D(rDoor[open ? 1 : 0], 1.f); }

// ---------------------------------------------------------------- level objects
// №517 (газета без рывков 70–100 мс): привязка звуков к сотням предметов — n × nobj проверок подряд (~40 мс на PSP за вызов, два вызова в уровне). Кадр экрана загрузки
// зовётся каждые 32 предмета (loadPump сам не рисует чаще LOAD_PUMP_MS), список объектов нужного вида собирается один раз. SNDBIND_PUMP_OLD=1 — прежний полный перебор без кадров
#ifndef SNDBIND_PUMP_OLD
#define SNDBIND_PUMP_OLD 0
#endif
void (*sndBindPump)(void);
void sndBindObjects(int set, unsigned n, const void *xyz, unsigned stride) {
  if (set < 0 || set >= SND_NSET) return;
  free(setRes[set]); setRes[set] = 0; setN[set] = 0;
  if (!ready || !n || !xyz || !nobj) return;
  setRes[set] = malloc(n * sizeof(short));
  if (!setRes[set]) return;
  setN[set] = n;
  int want = set == SND_SET_PROPS ? 0 : 1;                 // DynaPhys / AnimDynaPhys
  unsigned *ks = SNDBIND_PUMP_OLD ? 0 : malloc(nobj * sizeof *ks), nk = 0;   // индексы объектов нужного вида (результат тот же, перебор короче)
  if (ks) for (unsigned k = 0; k < nobj; k++) if (objs[k].kind == want) ks[nk++] = k;
  for (unsigned i = 0; i < n; i++) {
    if (!SNDBIND_PUMP_OLD && sndBindPump && (i & 31) == 31) sndBindPump();
    const float *p = (const float *)((const char *)xyz + (size_t)i * stride);
    int best = -1; float bd = 0.5f * 0.5f;                  // same object: same placement (export rounding only)
    for (unsigned kk = 0, kn = ks ? nk : nobj; kk < kn; kk++) {
      unsigned k = ks ? ks[kk] : kk;
      if (objs[k].kind != want) continue;
      float dx = objs[k].x - p[0], dy = objs[k].y - p[1], dz = objs[k].z - p[2], d = dx * dx + dy * dy + dz * dz;
      if (d < bd) { bd = d; best = objs[k].res; }
    }
    setRes[set][i] = (short)best;
  }
  free(ks);
}
int sndObjectSound(int set, unsigned i) { return set >= 0 && set < SND_NSET && i < setN[set] ? setRes[set][i] : -1; }
int sndIsCrate(int set, unsigned i) { return ready && rCrate >= 0 && sndObjectSound(set, i) == rCrate; }   // круг 3 (g11): ящик
void sndObjectHit(int set, unsigned i, float x, float y, float z, float speed) {
  int r = sndObjectSound(set, i);
  if (!ready || r < 0) return;
  const char *why = set == SND_SET_PROPS ? "prop" : "sprop";
  if (onFootNow) {                                          // пинок/стомп Гомера — прежняя громкость (правка 250 только про машину)
    float v = clampf(0.6f + speed / 15.f, 0, 1);
    playW(why, r, x, y, z, v);
    if (r == rHydrant) playW(why, rSpray, x, y, z, v);     // the water jet of the hydrant (collide.spt hydrant_spray)
  } else {                                                  // правка 250: предмет сбит машиной — как удар машины
    float v = collVol(speed, 0.6f, 15.f);
    if (!SV_CRASH_OLD) {   // сверка g08 (P11-22): громкость звука сбитого предмета = импульс / 100000 (не выше 1); ниже 0,06 события нет; стейтпроп — всегда 1
      float itv = set == SND_SET_SPROPS ? 1.f : impI(speed);
      if (itv < IMP_EVENT) return;
      v = clampf(itv, 0.f, 1.f);
    }
    if (!SV_CRASH_OLD) {   // сверка g08 (P09 Д1,Д2,36; P11-22): предмет с физикой — crash по интенсивности (импульс/100000), стейтпроп (ящик, будка, автомат) — всегда large
      int big = set == SND_SET_SPROPS; float it = big ? 1.f : impI(speed);
      if (it >= IMP_EVENT) {
        collPlay("crash", rCrash[big ? 2 : (it >= IMP_LARGE ? 2 : it >= IMP_MEDIUM ? 1 : 0)], x, y, z, big ? COLL_VMAX : v, 1);
        if (it >= IMP_MEDIUM) sndSay(SND_SAY_DAMAGE, 0);   // сильный удар (≥ 0,15) — Damage, 20 %
      }
    }
    collPlay(why, r, x, y, z, v, 1);
    if (r == rHydrant) collPlay(why, rSpray, x, y, z, v, 1);
    if (!TREE_SND_OLD && r == rTree && rCrate >= 0) collPlay(why, rCrate, x, y, z, .5f * v, 1);   // №459/№460: сбитое дерево и заборы-барьеры звучат одинаково: глухой «тук» с древесным треском
  }
  // правка 135.1: реплики D_* — набор «в машине» (dialog.spt D_<event>_Hom); пинок Гомера пешком — без D_Break
  // сверка g08 (P09 Д12, P11-21): Break (20 %) говорится на слом любого ломаемого предмета (гидрант, дерево, знак, стекло, барьер), в том числе пешком; ящик, будка, автомат (стейтпропы) события не дают
  if (SV_BREAK_OLD) { if (!onFootNow) sndSay(SND_SAY_BREAK, 0); }
  else if (set == SND_SET_PROPS) sndSay(SND_SAY_BREAK, 0);
}

// ---------------------------------------------------------------- pickups, Homer
void sndCoin(void) { sndPlay2D(rCoin, 1.f); }
// сверка g08-sound (P05:3 Д1, P05:8, P10-20): нажатие красного переключателя — на 0,3 с щелчок «switch» (2D); звук подвижного объекта по имени объекта
// уровня: люки (TD), грузовик truck1, дверь hdoortrans1, библиотека library — обычные эффекты без позиции, платформы Splatform*/Aplatform6 и лифт
// elevator — позиционные (пара дальностей из positionalsettings.spt в пакет не выгружается — 3–100 м по умолчанию). Звать при запуске клипа объекта
static float swT = -1.f;
void sndSwitchPress(void) { if (ready) swT = 0.3f; }
int audioResLoops(int id);   // audio.c: ресурс банка с петлёй (№1174)
#ifndef SNDANIMLEAK1174_OLD
#define SNDANIMLEAK1174_OLD 0
#endif
#ifndef ANIMOBJ_RANGE_OLD
#define ANIMOBJ_RANGE_OLD 0   // круг 6 (g08, P05:8): дальность звуков подвижных объектов по настройкам диска (платформа 14-20 м, кран 25-45, лифт 50-75, рука-робот 10-25); 1 — общие 3-100 м
#endif
static float animStreamT;   // сколько ещё идёт дорожка подвижного объекта на позиционном потоке (лифт L2/L5 не помещается в резидентный банк: звучит из music.sap)
#ifndef ANIMCRANE_SV_OLD
#define ANIMCRANE_SV_OLD 0
#endif
#define ANIMCRANE_NAME (ANIMCRANE_SV_OLD ? "cranes" : "crane")
int sndAnimObj(const char *obj, float x, float y, float z) {
  // mn/mx — настройки positionalsettings.spt по названию (platform_settings 14-20, crane_settings 25-45, loud_elevator_settings 50-75, robo_arm 10-25); какая настройка у какого объекта
  // в данных диска не записано (записаны только имена) — пара выбрана по имени. Люки, дверь, грузовик, библиотека без позиции
  static const struct { const char *obj, *clip; int pos; float mn, mx; } T[] = {
    {"TD", "trapdoor", 0, 0, 0}, {"Splatform1Trans", "platform_02", 1, 14, 20}, {"Splatform4Trans", "platform_01", 1, 14, 20}, {"Splatform5Trans", "platform_01", 1, 14, 20},
    {"Splatform6Trans", "platform_01", 1, 14, 20}, {"Splatfrom7Trans", "platform_01", 1, 14, 20}, {"Aplatform6Trans", "aplatform6", 1, 14, 20}, {"truck1", "duff_truck", 0, 0, 0},
    {"hdoortrans1", "plantdoor", 0, 0, 0}, {"elevator", "elevator", 1, 50, 75}, {"library", "gag_library", 0, 0, 0},
    {"Splatform2Trans", "platform_02", 1, 14, 20}, {"Aplatform1Trans", "platform_01", 1, 14, 20}, {"Aplatform2Trans", "platform_01", 1, 14, 20}, {"trapdoor1Trans", "platform_01", 1, 14, 20},   // круг 5 (g11, P11-15): остальные платформы, кран, рука-робот
    {ANIMCRANE_NAME, "platform_02", 1, 25, 45}, {"roboarm", "robo_arm", 1, 10, 25}};   // круг 7 (g11, по оригиналу): имя объекта крана в таблице звуков — "crane" (L6) точно; "cranes" (L5) в таблице нет и звука не имеет. ANIMCRANE_SV_OLD=1 — "cranes"
  if (!ready || !obj) return 0;
  for (unsigned i = 0; i < sizeof T / sizeof *T; i++) if (sndAnimNameMatch(T[i].obj, obj)) {
    int id = sndId(T[i].clip);
    if (id < 0) {   // нет в резидентном банке уровня: длинный лифт L2 и L5 идёт позиционным потоком (дорожка anim_<клип> в music.sap)
      char tn[40]; snprintf(tn, sizeof tn, "anim_%s", T[i].clip); int t = audioTrack(tn);
      if (t < 0 || ANIMOBJ_RANGE_OLD) return 0;
      audioStream(AUDIO_ST_POSITIONAL, t, 0.f); audioStreamSet(AUDIO_ST_POSITIONAL, 1.f, 0.f); audioStreamAt(AUDIO_ST_POSITIONAL, x, y, z, T[i].mn, T[i].mx);
      animStreamT = audioTrackSeconds(t) + 0.02f; emitCur = -2; return 1;   // круг 7: дорожка позиционного потока зациклена в пакете — период ровно на длину клипа, чтобы второй проход не начался
    }
    sfxWhy = "animobj";
    // №1174: платформы (platform_01/02, aplatform6) в банке — петли: голос живёт, пока его не остановят, а запуск идёт на каждый круг клипа и никто не останавливает.
    // Один голос на запись таблицы: прежний (если ещё звучит) гасим перед новым. SNDANIMLEAK1174_OLD=1 — как в TEST87
    static int animH[32];
    if (!SNDANIMLEAK1174_OLD && i < 32 && animH[i] && audioResLoops(id)) { if (audioPlaying(animH[i])) audioStop(animH[i]); animH[i] = 0; }
    int h = !T[i].pos ? sndPlay2D(id, 1.f) : ANIMOBJ_RANGE_OLD ? sndPlayId(id, x, y, z, 1.f) : audioPlayAtGain(id, 1.f, x, y, z, T[i].mn, T[i].mx);
    if (!SNDANIMLEAK1174_OLD && i < 32) animH[i] = audioResLoops(id) ? h : 0;
    sfxWhy = "fx"; return h;
  }
  return 0;
}
// сверка g08-sound (P09 Д3): card_collect звучит, только если игрок в машине; пешком — музыкальная вставка OF_found_card поверх тубы
void sndCardFound(float x, float y, float z) {
  if (!ready) return;
  if (SV_MUSZONE_OLD || !onFootNow) { sndPlay("card_collect", x, y, z, 1.f); return; }
  sndMusicEvent("OF_found_card", 0);
}
const char *sndPickupClip(int level, int mission) {
  static char nm[40];
  if (level < 1 || level > 7 || (SV_PICKUP_OLD && level > 6)) return "";
  const char *want = !SV_PICKUP_OLD && level == 7 ? "nuclear_waste_collect" : !SV_PICKUP_OLD && level == 2 && mission == 6 ? "monkey_collect" : 0;
  if (!want) { snprintf(nm, sizeof nm, "level_%d_pickup_sfx", level); want = nm; }
  // круг 6: экспорт сводит ресурсы диска с тем же первым файлом в один (level_6_pickup_sfx, nuclear_waste_collect — это card_collect_01, monkey_collect — card_collect_01 и _02):
  // названного клипа в банке может не быть, тогда играет тот, под которым файл лежит в банке (card_collect; для обезьяны 50/50 с level_2_pickup_sfx = card_collect_02)
  if (ready && sndId(want) < 0) want = !strcmp(want, "monkey_collect") && sndId("level_2_pickup_sfx") >= 0 && frand() < .5f ? "level_2_pickup_sfx" : "card_collect";
  return want;
}
void sndCoinLose(float x, float y, float z) { sndPlayId(rCoinLose, x, y, z, 1.f); }
void sndFootstep(int g, float x, float y, float z) {
  // круг 3 (проверка P02-34): один звук на касание ноги — этот вызов из homer.c (поверхность уже выбрана), 2D и вытесняемый; SV_STEP_OLD=1 — позиционный, как до сверки
  int r = rFeet[g & 3] >= 0 ? rFeet[g & 3] : rFeet[0];
  if (SV_STEP_OLD) { sndPlayId(r, x, y, z, 1.f); return; }
  sfxWhy = "step"; sndPlay2D(r, 1.f); sfxWhy = "fx";
}
// сверка g08-sound (P02-34, P09:4, P09 Д3): шаг Гомера — 2D (вытесняемый), клип по поверхности под ногой: металл, дерево, иначе бетон (земля и трава —
// тоже бетон, feet_dirt в игре не звучит); в помещении всегда бетон. Звать на каждое касание ноги при желаемой скорости > 1 м/с (бег — HOMER_EV_STEP;
// шаги ходьбы по кадрам клипа — homer.c, окно g04). run=1 — рывок: непрерывные 5 с рывка один раз дают реплику Turbo (P09:8, P10-23)
void sndStepFoot(float x, float y, float z, int run) {
  if (!ready || SV_STEP_OLD) return;
  int indoor = 0, surf = svSurfaceAt(x, y, z, &indoor), g = indoor ? 0 : surf == SURF_METAL ? 1 : surf == SURF_WOOD ? 2 : 0;
  int r = rFeet[g] >= 0 ? rFeet[g] : rFeet[0];
  sfxWhy = "step"; sndPlay2D(r, 1.f); sfxWhy = "fx";
  if (!run) { stepTurboT = 0.f; turboSaid = 0; stepLastT = stepClock; return; }
  if (stepClock - stepLastT > 0.8f) { stepTurboT = 0.f; turboSaid = 0; } else stepTurboT += stepClock - stepLastT;
  stepLastT = stepClock;
  if (stepTurboT >= 5.f && !turboSaid) { turboSaid = 1; sndSay(SND_SAY_TURBO, 2); }
}
void sndJump(float x, float y, float z) { sndPlayId(rJump, x, y, z, 1.f); }
void sndLand(int g, float x, float y, float z) { sndPlayId(rLand[g & 3] >= 0 ? rLand[g & 3] : rLand[0], x, y, z, 1.f); }
void sndStomp(float x, float y, float z) { sndPlayId(rStomp, x, y, z, 1.f); }

int sndPcSayOld;
static const char *sayLastName;
// №683 (Роб, TEST27: «у Барта нет ни одной фразы»): учёт попыток реплик водителя уровня по событиям — сколько раз звали, сколько сказано, почему отказ (flag saystat → строка SAYSTAT)
static char sayHist[256];   // имена сказанных дорожек подряд (гейт семени: две игры с разным семенем говорят по-разному)
static unsigned sayCall[SND_SAY_COUNT], sayOk[SND_SAY_COUNT], sayNoPool[SND_SAY_COUNT], sayGap[SND_SAY_COUNT], sayBusy[SND_SAY_COUNT], sayRoll[SND_SAY_COUNT];
int sndSayStats(char *b, int n) {
  int l = snprintf(b, n, "SAYSTAT"); unsigned all = 0;
  for (int e = 0; e < SND_SAY_COUNT && l < n - 40; e++) { all += sayOk[e]; if (sayCall[e] || sayN[e]) l += snprintf(b + l, n - l, " %d:pool%d,call%u,ok%u,nopool%u,gap%u,busy%u,roll%u", e, sayN[e], sayCall[e], sayOk[e], sayNoPool[e], sayGap[e], sayBusy[e], sayRoll[e]); }
  l += snprintf(b + l, n - l, " total_ok=%u ready=%d hist=%s\n", all, ready, sayHist); return l;
}
// P09:5 — шансы оригинала (бросок при постановке в очередь): 20 %: GIC, GOC, Arrive, Air, Mcrash, Damage, Pass, Passed, HitByC, HitP, CarWay, Break;
// 15 %: Burn; без броска: Bcrash, Dcar, Mfail, Mvic, Tail, Card, Mstart, Time, Fall, NewAI, Springboard, дверь. События без источника в оригинале
// (MissP, Dodge, AIDestroy, LongJump) молчат; Turbo — только по таймеру рывка (sndStepFoot)
static const float svChance[SND_SAY_COUNT] = {1.f, .2f, .2f, .15f, 1.f, .2f, .2f, .2f, 0.f, .2f, 1.f, .2f, 1.f, 1.f, .2f, .2f,
                                              1.f, 1.f, 1.f, 1.f, 1.f, 1.f, .2f, .2f, .2f, 1.f, 0.f, 1.f, 0.f, 0.f};
#ifndef SV_SAYQ7_OLD
#define SV_SAYQ7_OLD 0   // круг 7 (сверка с оригиналом, P09:12): четыре ступени очереди (неизвестная < случайная < желательная < обязательная), Mstart живёт без срока, переполнение отбрасывает новую, та же реплика, что играет, не ставится
#endif
static float svLife(int e) {   // жизнь строки в очереди, с (оригинал: по умолчанию 2,5; Mfail и Mvic 4; Dcar 5; Mstart без срока)
  if (!SV_SAYQ7_OLD && e == SND_SAY_MSTART) return 1e9f;
  return e == SND_SAY_MVIC || e == SND_SAY_MFAIL ? 4.f : e == SND_SAY_DCAR ? 5.f : 2.5f;
}
// ступень в очереди (круг 7): у оригинала 0 неизвестная (событий нет в таблице: Bcrash, Time, Fall, Springboard, Door, NewAI, Turbo), 1 случайная (с броском),
// 2 желательная (Dcar, Mfail, Mvic, Tail), 3 обязательная (Card, Mstart)
static short svTier(int e) {
  if (SV_SAYQ7_OLD) return svChance[e] >= 1.f ? 2 : 0;
  switch (e) {
    case SND_SAY_DCAR: case SND_SAY_MFAIL: case SND_SAY_MVIC: case SND_SAY_TAIL: return 2;
    case SND_SAY_CARD: case SND_SAY_MSTART: return 3;
    case SND_SAY_BURN: case SND_SAY_MCRASH: case SND_SAY_DAMAGE: case SND_SAY_BREAK: case SND_SAY_AIR: case SND_SAY_HITP: case SND_SAY_CARWAY:
    case SND_SAY_ARRIVE: case SND_SAY_GOC: case SND_SAY_GIC: case SND_SAY_PASS: case SND_SAY_PASSED: case SND_SAY_HITBYC: return 1;
    default: return 0;
  }
}
static int sayNowEv = -1;   // событие реплики, что звучит сейчас
static int sayStart(int e) {   // выдать реплику события e (поток диалогов свободен)
  int n = sayN[e], k;
  if (SV_SAYTAB_OLD) { k = (int)(frand() * n) % n; if (n > 1 && k == sayLast[e]) k = (k + 1) % n; }
  else k = sayLast[e] == 0xFF ? (int)(frand() * n) % n : (sayLast[e] + 1) % n;   // P09:13: по кругу от случайного начала
  sayLast[e] = (unsigned char)k; sayT[e] = 0; sinceLine = 0; sayOk[e]++; sayNowEv = e;
  audioStream(AUDIO_ST_DIALOG, sayTrack[e][k], 0);
  sayLastName = audioTrackName(sayTrack[e][k]);
  { size_t hl = strlen(sayHist), nl = strlen(sayLastName); if (hl + nl + 2 < sizeof sayHist) { memcpy(sayHist + hl, sayLastName, nl); sayHist[hl + nl] = ','; sayHist[hl + nl + 1] = 0; } }
  if (sfxLog) { char t[64]; snprintf(t, sizeof t, "SAY %d %s", e, sayLastName); sfxLog(t); }   // журнал смоука: событие и дорожка (раньше только e >= SND_SAY_CARD)
  return 1;
}
int sndSay(int e, int force) {
  if (!ready || e < 0 || e >= SND_SAY_COUNT) return 0;
  sayCall[e]++;
  if (!sayN[e]) { sayNoPool[e]++; return 0; }
  if (sndPcSayOld && e >= SND_SAY_MSTART && e <= SND_SAY_GIC) return 0;   // правка 345: красный контроль — как до правки
  if (!SV_SAYTAB_OLD) {   // сверка g08-sound (P09:5,12,13,33)
    if (e == SND_SAY_MISSP || e == SND_SAY_DODGE || e == SND_SAY_AIDESTROY || e == SND_SAY_LONGJUMP) return 0;
    // круг 3 (проверка): Turbo разрешён — единственный зовущий это таймер рывка 5 с в main_loop.h (g10b), внутреннего таймера sndStepFoot больше нет в цепочке
    if (force != 1 && force != 2 && svChance[e] < 1.f && frand() >= svChance[e]) { sayRoll[e]++; return 0; }
    if (audioStreamTrack(AUDIO_ST_DIALOG) < 0) sayNowEv = -1;
    if (audioStreamTrack(AUDIO_ST_DIALOG) >= 0) {   // занято: ждёт в очереди жизнь строки, дубликат не ставят
      sayBusy[e]++;
      if (!SV_SAYQ7_OLD && e == sayNowEv) return 0;   // та же реплика уже звучит — в очередь не ставят
      for (int i = 0; i < sqn; i++) if (sq[i].ev == e) return 0;
      // круг 3 (проверка P09:12): очередь по ступеням важности — обязательные (события без броска) впереди случайных; новая встаёт после равных и перед слабыми;
      // при переполнении вытесняет самую слабую (последнюю), если та слабее новой
      short tier = svTier(e); int at = sqn;
      while (at > 0 && sq[at - 1].tier < tier) at--;
      if (SV_SAYQ7_OLD) { if (sqn >= SQ_N) { if (sq[sqn - 1].tier >= tier) return 0; sqn--; if (at > sqn) at = sqn; } }
      else if (sqn >= SQ_N) return 0;   // круг 7: очередь полна (16) — новая отбрасывается, как в оригинале (вытеснения слабейшей там на деле нет)
      for (int k = sqn; k > at; k--) sq[k] = sq[k - 1];
      sq[at].ev = (short)e; sq[at].tier = tier; sq[at].life = svLife(e); sqn++;
      return 0;
    }
    return sayStart(e);
  }
  if (sinceLine < 5.f) { sayGap[e]++; return 0; }
  if (audioStreamTrack(AUDIO_ST_DIALOG) >= 0) { sayBusy[e]++; return 0; }
  if (!force && (sayT[e] < sayCool[e] || frand() >= sayChance[e])) { sayRoll[e]++; return 0; }
  return sayStart(e);
}
const char *sndSayLastName(void) { return sayLastName ? sayLastName : "-"; }

// ---- реплики соперников (аудит 09.10): дорожки v_<семейство>_<код>_* пакета (tools/sound_export.py rival_codes)
static const char *const vKey[SND_V_COUNT] = {"mvic", "mfail", "damage", "tail", "hitcar"};
static const float vCool[SND_V_COUNT] = {0, 0, 10, 10, 10};   // правило порта: удар и «соперник впереди» — не чаще 10 с
static int codeOf(const char *driver, const char *code, int len) {   // буквы кода по порядку в имени водителя, первая совпадает
  if (!driver || !*driver || len <= 0 || (driver[0] | 32) != (code[0] | 32)) return 0;
  int k = 0; for (const char *d = driver; *d && k < len; d++) if ((*d | 32) == (code[k] | 32)) k++;
  return k == len;
}
static int rivalPool(const char *driver, int kind, short *out, int max) {
  int n = 0; char pre[16]; snprintf(pre, sizeof pre, "v_%s_", vKey[kind]); size_t pl = strlen(pre);
  for (int t = 0, nt = audioTrackCount(); t < nt && n < max; t++) {
    if (audioTrackBus(t) != 2) continue;
    const char *nm = audioTrackName(t); if (strncmp(nm, pre, pl)) continue;
    const char *c = nm + pl, *e = strchr(c, '_'); if (!e) continue;
    if (codeOf(driver, c, (int)(e - c))) out[n++] = (short)t;
  }
  return n;
}
// 15.10 (TEST18, разбивка UIPH/MVHUD: mvhud 2,0–2,9 мс на кадр в погонях и гонках, 0,03 вне миссий): msn_view.c voTick звал sndRivalHas на каждую машину миссии КАЖДЫЙ кадр, а он
// перебирал все дорожки звука (SND_V_COUNT видов × все дорожки с snprintf и сравнением строк). Ответ зависит только от имени водителя и набора дорожек — кэш по имени, сбрасывается
// при смене набора (число дорожек, первая и последняя). SND_RIVAL_CACHE_OLD=1 — прежнее
#ifndef SND_RIVAL_CACHE_OLD
#define SND_RIVAL_CACHE_OLD 0
#endif
int sndRivalHas(const char *driver) {
  if (!ready || SND_VO_OLD) return 0;
  static struct { char d[24]; int has; } cache[16]; static int ncache, cnt; static const char *first, *last;
  int nt = SND_RIVAL_CACHE_OLD ? 0 : audioTrackCount();
  if (!SND_RIVAL_CACHE_OLD) {
    const char *f = nt ? audioTrackName(0) : 0, *l = nt ? audioTrackName(nt - 1) : 0;
    if (nt != cnt || f != first || l != last) { ncache = 0; cnt = nt; first = f; last = l; }
    for (int i = 0; i < ncache; i++) if (!strncmp(cache[i].d, driver, sizeof cache[i].d - 1) && strlen(driver) < sizeof cache[i].d) return cache[i].has;
  }
  int has = 0; short p[4]; for (int k = 0; k < SND_V_COUNT && !has; k++) if (rivalPool(driver, k, p, 4)) has = 1;
  if (!SND_RIVAL_CACHE_OLD && ncache < 16 && strlen(driver) < sizeof cache[0].d) { snprintf(cache[ncache].d, sizeof cache[ncache].d, "%s", driver); cache[ncache].has = has; ncache++; }
  return has;
}
int sndRivalSay(const char *driver, int kind, int force) {
  if (!ready || SND_VO_OLD || kind < 0 || kind >= SND_V_COUNT) return 0;
  if ((SV_SAYTAB_OLD && sinceLine < 5.f) || audioStreamTrack(AUDIO_ST_DIALOG) >= 0) return 0;   // сверка g08 (P09:12): общей паузы 5 с в оригинале нет
  if (SV_SAYTAB_OLD && !force && vT[kind] < vCool[kind]) return 0;
  short pool[SAY_MAX]; int n = rivalPool(driver, kind, pool, SAY_MAX); if (!n) return 0;
  int k = (int)(frand() * n) % n; if (n > 1 && pool[k] == vLast) k = (k + 1) % n;
  vLast = pool[k]; vT[kind] = 0; sinceLine = 0;
  audioStream(AUDIO_ST_DIALOG, pool[k], 0);
  sayLastName = audioTrackName(pool[k]);
  if (sfxLog) { char t[64]; snprintf(t, sizeof t, "SAY %s", sayLastName); sfxLog(t); }
  return 1;
}
const char *sndGetOut(int on) {   // правка 345: выход из машины начался (фронт) — реплика водителя W_GOC_ (шанс GOC_CHANCE)
  static int was; int edge = on && !was; was = on;
  if (!edge) return 0;
  return sndSay(SND_SAY_GOC, 0) ? sayLastName : "";
}

// [PS2] W_GIC Hom: 4 из 11 посадок, atlas_findings.tsv m0.2
// Шанс для семейства W_GIC остальных героев пока правило порта; Апу не подменяется другим событием
const char *sndGetIn(int on) {
  int edge = on && getInWas == 0; getInWas = on;
  if (!edge) return 0;
  return sndSay(SND_SAY_GIC, 0) ? sayLastName : "";
}

// правки 180/184: голос прохожего по имени модели — ПРАВИЛО ПОРТА (в данных диска связи модели с голосом нет): класс по имени
// (fem/olady/busw/hooker/nurse — женский, girl — девочка, boy — мальчик, остальные — мужской), какой из двух — по сумме букв имени
// Правка 240: персонажи миссий (game_msn.c, имя как в скриптах) — тем же правилом: женщины marge/patty/selma/zfem1, девочка lisa,
// мальчики bart/milhouse/nelson/ralph/jimbo, остальные — мужской голос. Свои реплики персонажей (<имя>/p_hitbyc_*.rsd на диске)
// в звуковой пакет не выгружаются (tools/sound_export.py STREET — только generic_*) — пока голос класса
static int nameIn(const char *m, const char *const *list) { for (; *list; list++) if (!strcmp(m, *list)) return 1; return 0; }
int sndPedVoice(const char *m) {
  if (!m) return 0;
  static const char *const women[] = {"marge", "patty", "selma", "zfem1", 0}, *const girls[] = {"lisa", 0},
                           *const boys[] = {"bart", "milhouse", "nelson", "ralph", "jimbo", 0};
  if (!SV_VOICE_OLD) {   // сверка g08 (P09:7): таблица моделей оригинала -> голос (индекс pedVoc); персонажи миссий — прежним правилом; прочие молчат
    static const char *const v0[] = {"bum", "busm2", "const2", "joger2", "male2", "male4", "male6", "nuclear", "sail2", "sail4", 0},
      *const v1[] = {"busm1", "const1", "farmr1", "male1", "male3", "male5", "mobstr", "sail1", "sail3", 0},
      *const v2[] = {"busw1", "fem2", "fem4", "joger1", "olady2", 0}, *const v3[] = {"fem1", "fem3", "hooker", "olady1", "olady3", 0},
      *const v4[] = {"boy", "boy3", "rednk2", 0}, *const v5[] = {"boy2", "rednk1", "frankenstein", 0}, *const v6[] = {"girl1", "witch", 0},
      *const v7[] = {"girl2", 0};
    const char *const *tab[8] = {v0, v1, v2, v3, v4, v5, v6, v7};
    for (int k = 0; k < 8; k++) if (nameIn(m, tab[k])) return k;
    if (!nameIn(m, women) && !nameIn(m, girls) && !nameIn(m, boys)) return -1;   // вне таблицы голоса нет
  }
  unsigned h = 0; for (const char *c = m; *c; c++) h += (unsigned char)*c;
  int cls = !strncmp(m, "girl", 4) || nameIn(m, girls) ? 3 : !strncmp(m, "boy", 3) || nameIn(m, boys) ? 2 :
            (!strncmp(m, "fem", 3) || !strncmp(m, "olady", 5) || !strncmp(m, "busw", 4) || !strncmp(m, "hooker", 6) || !strncmp(m, "nurse", 5) ||
             nameIn(m, women)) ? 1 : 0;
  return cls * 2 + (int)(h & 1u);
}
static int streetSay(const short *tr, unsigned char n, int roll) {
  if (!ready || !n || sndStreetOff || streetT < 1.5f || audioStreamTrack(AUDIO_ST_DIALOG) >= 0) return 0;
  if (roll && !SV_STREETCH_OLD && frand() >= .2f) return 0;   // сверка g08 (P09:6): прохожий и водитель говорят с шансом 20 % на каждое событие
  int k = (int)(frand() * n) % n;
#ifndef SND_STREET_REPEAT_OLD
#define SND_STREET_REPEAT_OLD 0   // 1 — прохожие могут сказать ту же реплику два раза подряд (как было)
#endif
  static short lastStreet = -1; if (!SND_STREET_REPEAT_OLD && n > 1 && tr[k] == lastStreet) k = (k + 1) % n;   // №685: та же реплика подряд не звучит
  lastStreet = tr[k];
  audioStream(AUDIO_ST_DIALOG, tr[k], 0); streetT = 0.f; sinceLine = 0.f;
  if (sfxLog) { char t_[64]; snprintf(t_, sizeof t_, "STREETSAY %s", audioTrackName(tr[k])); sfxLog(t_); }   // журнал смоука: какая дорожка улицы взята
  return 1;
}
int sndPedSay(int kind, int v) {
  if (kind < 0 || kind >= SND_PED_KINDS || v < 0 || v > 7) return 0;
  if (!pedN[kind][v]) v ^= 1;   // у голоса нет реплик этого вида — второй голос того же класса
  return streetSay(pedTrack[kind][v], pedN[kind][v], 1);
}
int sndDriverSay(int kind, int v) {
  if (kind < 0 || kind >= SND_DRV_KINDS) return 0;
  v &= 3; if (!drvN[kind][v]) v ^= 1;
  return streetSay(drvTrack[kind][v], drvN[kind][v], 1);
}

#ifndef SND_STREET_SPATIAL_OLD
#define SND_STREET_SPATIAL_OLD 0
#endif
static int streetAt(int started,float x,float y,float z) {
  if(!started)return 0;
  streetTrack=audioStreamTrack(AUDIO_ST_DIALOG);streetSerial=audioStreamRequestSerial(AUDIO_ST_DIALOG);
  if(!SND_STREET_SPATIAL_OLD)audioStreamAt(AUDIO_ST_DIALOG,x,y,z,SV_STREETCH_OLD?4.f:50.f,SV_STREETCH_OLD?60.f:100.f);   // сверка g08 (P09:6): полная громкость до 50 м, тишина на 100 м
  return 1;
}
// Named source folders identify characters exactly; generic voices remain the
// fallback when a character has no authored line or the voice stream is busy.
static int namedPool(const char *prefix, short *pool, int max) {
  int n = 0; size_t len = strlen(prefix);
  for (int t = 0; t < audioTrackCount() && n < max; t++)
    if (audioTrackBus(t) == 2 && !strncmp(audioTrackName(t), prefix, len)) pool[n++] = t;
  return n;
}
int sndPedNamedSayAt(int kind, const char *name, float x, float y, float z) {
  if (!SND_VO_OLD && name && kind >= 0 && kind < SND_PED_KINDS) {
    char prefix[64]; short pool[64]; snprintf(prefix, sizeof prefix, "%s%s_", pedPre[kind], name);
    if (streetAt(streetSay(pool, namedPool(prefix, pool, 64), 1), x, y, z)) return 1;
  }
  return sndPedSayAt(kind, sndPedVoice(name), x, y, z);
}
int sndDoorbellNear(float x, float y, float z) {
  if (SND_VO_OLD || !ready) return -1;
  for (unsigned i = 0; i < ndoor; i++) {
    const XDoor *d = doors + i; float v[3] = {x-d->p[0], y-d->p[1], z-d->p[2]};
    if (!d->kind) { if (v[0]*v[0]+v[1]*v[1]+v[2]*v[2] <= d->half[0]*d->half[0]) return i; }
    else { int in = 1; for (int k = 0; k < 3; k++)
      if (fabsf(v[0]*d->axes[k*3]+v[1]*d->axes[k*3+1]+v[2]*d->axes[k*3+2]) > d->half[k]) in = 0;
      if (in) return i; }
  }
  return -1;
}
int sndDoorbellSay(int index) {
  if (SND_VO_OLD || index < 0 || (unsigned)index >= ndoor) return 0;
  short pool[64]; int n = namedPool(doors[index].prefix, pool, 64);
  int ok = streetSay(pool, n, 0);   // звонок в дверь — «немедленная», без броска
  return streetAt(ok, doors[index].p[0], doors[index].p[1], doors[index].p[2]);
}
int sndPedSayAt(int kind,int voice,float x,float y,float z) {return streetAt(sndPedSay(kind,voice),x,y,z);}
int sndDriverSayAt(int kind,int voice,float x,float y,float z) {return streetAt(sndDriverSay(kind,voice),x,y,z);}
int sndStreetMove(float x,float y,float z) {
  if(!ready||streetTrack<0||audioStreamTrack(AUDIO_ST_DIALOG)!=streetTrack||audioStreamRequestSerial(AUDIO_ST_DIALOG)!=streetSerial){streetTrack=-1;return 0;}
  if(!SND_STREET_SPATIAL_OLD)audioStreamAt(AUDIO_ST_DIALOG,x,y,z,SV_STREETCH_OLD?4.f:50.f,SV_STREETCH_OLD?60.f:100.f);   // сверка g08 (P09:6): полная громкость до 50 м, тишина на 100 м
  return 1;
}
void sndStreetStop(void) {
  if(ready&&streetTrack>=0&&audioStreamTrack(AUDIO_ST_DIALOG)==streetTrack&&audioStreamRequestSerial(AUDIO_ST_DIALOG)==streetSerial)audioStream(AUDIO_ST_DIALOG,-1,.15f);
  streetTrack=-1;
}

// ---------------------------------------------------------------- music / mode
static float musicFadeOver = -1.f;   // правка 593: выход из клуба — своё затухание (PS2 ~4 с)
static void music(int t, float fade) { audioMusic(t >= 0 ? tId[t] : -1, musicFadeOver >= 0.f ? musicFadeOver : fade); }
void sndMusic(int st) {
  if (!ready || st < 0 || st >= SND_MUS_COUNT) return;
  if (mOwned && st != SND_MUS_OFF) return;   // музыку ведёт миссия (sndMusicEvent): пешком/в машине решает она
  if (clubOn && musicFadeOver < 0.f) { clubOn = 0; audioStreamSet(AUDIO_ST_MUSIC, 1.f, 0.f); if (sfxLog) sfxLog("SNDCLUB end by music change"); }   // ревью 16.10: чужая смена музыки заканчивает клуб — громкость 1,0 и туба снова доступна (clubExit сам ставит musicFadeOver)
  int cur = audioStreamTrack(AUDIO_ST_MUSIC);
  mReturn = -1;
  switch (st) {
    case SND_MUS_OFF: music(-1, 1.f); break;
    case SND_MUS_FREEROAM:
      if (mState == SND_MUS_FREEROAM && cur >= 0 && (cur == tId[T_SD] || cur == tId[T_SD_INTRO])) break;
      music(tId[T_SD_INTRO] >= 0 ? T_SD_INTRO : T_SD, 0.5f); break;   // без вступления (уровни 2, 3, 5, 6) — сразу петля езды
    case SND_MUS_ONFOOT: {   // выход из машины: случайный из вариантов, которые есть у уровня (у 1, 2, 4, 6 — три, у 3 и 7 — один)
      int pick[3], n = 0; for (int k = 0; k < 3; k++) if (tId[T_SD_SUS1 + k] >= 0) pick[n++] = T_SD_SUS1 + k;
      music(n ? pick[(int)(frand() * n) % n] : -1, 0.15f);
    } break;
    case SND_MUS_MISSION: music(T_M_PICKUP, 0.3f); break;
    case SND_MUS_MISSION_WIN: music(T_M_POS, 0.15f); mReturn = SND_MUS_FREEROAM; break;
    case SND_MUS_MISSION_LOSE: music(T_M_NEG, 0.15f); mReturn = SND_MUS_FREEROAM; break;
    case SND_MUS_MISSION_ONFOOT: music(frand() < .5f ? T_M_SUS1 : T_M_SUS2, 0.15f); break;
    case SND_MUS_HITRUN: music(T_HR_INTRO, 0.3f); break;
    case SND_MUS_HITRUN_CAUGHT: music(T_HR_NEG, 0.15f); mReturn = SND_MUS_FREEROAM; break;
    case SND_MUS_KWIKEMART: music(T_KWIK, 0.5f); break;
    case SND_MUS_STORE: music(T_MUZAK, 0.5f); break;
    case SND_MUS_FRONTEND: modeSnap = 1; busLive = 0;   // сверка g08: меню — шины сразу, без хвоста игровой паузы
        // FE_region: поток Simpsons_Theme = simpsons_theme.rsd (строки всех l?_music.rms), петля
      if (mState == SND_MUS_FRONTEND && cur >= 0 && cur == tId[T_FE]) break;
      music(T_FE, 0.5f); break;
  }
  mState = st;
}
int sndMusicState(void) { return mState; }

// музыка миссий: таблица MEVT (tools/sound_export.py mission_music). Концовки win/lose — один проход, потом свободная езда
// (как SND_MUS_MISSION_WIN/LOSE); get_out_of_car — один проход, потом тишина до следующего события миссии
int sndSusLoopOld;   // правка 303: служебное — концовка «вышел из машины» петлёй, как в данных
static int sndMusicEventIn(const char *ev, const char **track, int inMission);
int sndMusicEvent(const char *ev, const char **track) { return sndMusicEventIn(ev, track, 0); }
static int sndMusicEventIn(const char *ev, const char **track, int inMission) {
  if (track) *track = "-";
  if (!ready || !ev) return -1;
  for (unsigned i = 0; i < nmevt; i++) {
    const XMEvt *e = &mevts[i];
    if (strncmp(e->event, ev, sizeof e->event) != 0) continue;
    if (!e->n) return 0;
    int t = e->track[e->n > 1 ? (int)(frand() * e->n) % e->n : 0];
    if (t < 0) return 0;
    if (track) *track = audioTrackName(t);
    if (e->kind == MEV_INSERT || e->kind == MEV_ZONE) {   // сверка g08-sound (P09 Д3, Д4, 18): вставка поверх тубы/езды, потом музыка возвращается; на этапе «музыка миссии» не вмешивается
      if (SV_MUSZONE_OLD || (mOwned && !inMission)) return 0;
      if (e->kind == MEV_INSERT) { audioMusicOnce(t, 0.15f); mReturn = onFootNow ? -1 : SND_MUS_FREEROAM; }
      else { audioMusic(t, 0.5f); mReturn = -1; }
      return 1;
    }
    int end = e->kind == MEV_WIN || e->kind == MEV_LOSE;
    // Правка Роба 303 (04.10: «после выхода из машины снова и снова мелодия — играет две секунды и начинается заново»): концовка
    // get_out_of_car (…_end_sus) у M2/M3/M5/M6/M7 в данных — петлёй (logic_repeat), короткий отрывок начинался сначала каждые
    // ~2 с. Один проход, как у M0/M1; дальше тишина и тубы прогулки (правка 179). sndSusLoopOld (flag susloopold 1) — прежнее
    if (e->kind == MEV_SUS && !sndSusLoopOld) audioMusicOnce(t, 0.15f);
    else audioMusic(t, end || e->kind == MEV_SUS ? 0.15f : 0.3f);
    mState = end ? (e->kind == MEV_WIN ? SND_MUS_MISSION_WIN : SND_MUS_MISSION_LOSE) : SND_MUS_MISSION;
    mReturn = end ? SND_MUS_FREEROAM : -1;
    mOwned = !end;
    return 1;
  }
  return -1;
}
void sndMusicMission(int own) {
  if (!ready) return;
  if (own) { mOwned = 1; return; }
  if (!mOwned) return;
  mOwned = 0; mReturn = -1; mState = SND_MUS_OFF; sndMusic(onFootNow ? SND_MUS_ONFOOT : SND_MUS_FREEROAM);
}
int sndMusicOwned(void) { return mOwned; }
void sndMusicRelease(void) { mOwned = 0; mReturn = -1; }
void sndOnFoot(int on) { onFootNow = on; }
static int voiceTrack = -1;   // реплика разговора миссии в потоке диалогов (sndVoice)
void sndVoice(int t) { if (ready) { audioStream(AUDIO_ST_DIALOG, t, t < 0 ? 0.1f : 0.f); voiceTrack = t; } }
int sndVoiceBusy(void) { return ready && voiceTrack >= 0 && audioStreamTrack(AUDIO_ST_DIALOG) == voiceTrack; }
int sndVoicePlaying(int t) { return ready && t >= 0 && audioStreamTrack(AUDIO_ST_DIALOG) == t; }
int sndModeCur(void) { return curMode; }

// Правка Роба 55 (29.09): разговор миссии с полосами (экран LetterBox) звучит набором шин тюнера диска «Letterbox»
// (global.spt: музыка 0.29, эффекты 0.5, мотор 0.28, фон 0.5, голос 1) поверх текущего режима — реплики слышно, музыка
// места не останавливается и после разговора играет дальше на прежней громкости (режим под полосами не теряется)
static void applyMode(int m);
void sndMode(int m) {
  if (!ready || m < 0 || m > SND_MODE_CREDITS) return;
  curMode = m;
  applyMode(pauseMode ? SND_MODE_PAUSE : lbMode ? SND_MODE_LETTERBOX : m);
}
// Правка Роба 87: пауза — набор шин тюнера диска «Pause» (global.spt: музыка, эффекты, мотор, голос, фон = 0 — тишина) поверх всего
void sndPause(int on) {
  on = !!on; if (!ready || on == pauseMode) return;
  pauseMode = on; if (!nmode) audioPauseAll(on);   // №1144: в пакете нет таблицы режимов (бонус-игра) — пауза общим множителем
  applyMode(on ? SND_MODE_PAUSE : lbMode ? SND_MODE_LETTERBOX : curMode);
}
void sndLetterbox(int on) {
  on = !!on; if (!ready || on == lbMode) return;
  lbMode = on; if (!pauseMode) applyMode(on ? SND_MODE_LETTERBOX : curMode);
}
int sndModeApplied(void) { return lbMode ? SND_MODE_LETTERBOX : curMode; }
// ---- шины режимов (сверка g08-sound: P09:14,15, P10-18, M2 Д1, M1 Д4). Итог шины = её громкость в «Normal» (пользовательская, global.spt)
// × наименьший множитель из всех включённых режимов; изменение идёт линейно за 750 мс. Режимы: текущий (sndMode), «Пешком» (Гомер вне машины),
// пауза и загрузочная пауза, полосы разговора, «Dialogue» на время любой реплики, «Store» (музыка Muzak: будка, лавка, гардероб; sndStore для
// остальных), «JustMusic» (переход в помещение). SV_MODEMIX_OLD=1 — один режим по приоритету, мгновенно, строки таблицы как готовые громкости
static const char *const svModeNames[] = {"Normal", "Pause", "MissionScreen", "Letterbox", "Dialogue", "Store", "OnFoot",
                                          "Minigame", "JustMusic", "Credits"};
static const XMode *modeRow(const char *name) {
  for (unsigned i = 0; i < nmode; i++) if (!strncmp(modes[i].name, name, 16)) return &modes[i];
  return 0;
}
static void modeLayer(float *mult, const char *name) {
  const XMode *r = modeRow(name); if (!r) return;
  for (int b = 0; b < 5; b++) if (r->v[b] < mult[b]) mult[b] = r->v[b];
}
static void modeTargets(float *t) {
  float mult[5] = {1.f, 1.f, 1.f, 1.f, 1.f};
  const XMode *n = modeRow("Normal");
  if (curMode != SND_MODE_NORMAL) modeLayer(mult, svModeNames[curMode]);
  if (onFootNow && curMode != SND_MODE_ONFOOT) modeLayer(mult, "OnFoot");
  if (pauseMode || loadDuckOn) modeLayer(mult, "Pause");
  if (lbMode) modeLayer(mult, "Letterbox");
  if (curMode != SND_MODE_DIALOGUE && audioStreamTrack(AUDIO_ST_DIALOG) >= 0) modeLayer(mult, "Dialogue");   // любая реплика
  { int cur = audioStreamTrack(AUDIO_ST_MUSIC);
    (void)cur;
    if (storeKind) {
      modeLayer(mult, "Store");
      if (!BOOTHMUTE1135_OLD && storeKind == 1) mult[0] = 0.f;   // №1135: пока открыта будка (или лавка машин), мировые эффекты (оса, удары, сирены, моторы трафика) молчат; голос и музыка будки идут, звуки меню на своей шине
    } }   // круг 3 (проверка): «магазин» включают будка и лавка машин, экран провала, итог мини-игры; в данных реплика на полной громкости, гардероб состояния не включает
  if (justMusicOn) modeLayer(mult, "JustMusic");
  for (int b = 0; b < 5; b++) t[b] = (n ? n->v[b] : 1.f) * mult[b];
}
#ifndef SV_FADE7_OLD
#define SV_FADE7_OLD 0   // круг 7: 1 — шаг шины один на весь размах (переход на часть размаха короче 750 мс)
#endif
static void modeTick(void) {
  if (SV_MODEMIX_OLD || !ready || !nmode) return;
  unsigned now = audioClockUs(); float dt = modeClkUs ? (float)(now - modeClkUs) * 1e-6f : 1.f; modeClkUs = now;
  float t[5]; modeTargets(t);
  int snap = !busLive || dt > 0.4f || modeSnap; modeSnap = 0;
  if (dt > 0.1f) dt = 0.1f;
  // круг 7 (сверка с оригиналом, P09:15, P10-18): любой переход шины идёт ровно 750 мс при любой его величине — шаг считается при смене цели как |текущий уровень - цель| / 0,75 с
  // (раньше шаг был один на весь размах 0..1, и переход на половину размаха длился вдвое короче). SV_FADE7_OLD=1 — прежнее
  static float busTgt[5], busStp[5];
  for (int b = 0; b < 5; b++) {
    float d = t[b] - busNow[b];
    if (!SV_FADE7_OLD && fabsf(t[b] - busTgt[b]) > 1e-4f) { busTgt[b] = t[b]; busStp[b] = fabsf(d) / 0.75f; }
    float step = SV_FADE7_OLD ? dt / 0.75f : busStp[b] * dt;
    busNow[b] = snap ? t[b] : (d > step ? busNow[b] + step : d < -step ? busNow[b] - step : t[b]);
  }
  busLive = 1;
  audioBus(AUDIO_BUS_SFX, busNow[0]); audioBus(AUDIO_BUS_CAR, busNow[1]); audioBus(AUDIO_BUS_MUSIC, busNow[2]);
  audioBus(AUDIO_BUS_AMBIENCE, busNow[3]); audioBus(AUDIO_BUS_DIALOG, busNow[4]);
}
// магазин/гардероб/провал миссии: kind 0 — выключить, 1 — будка и лавка машин (реплика слышна), 2 — провал миссии, 3 — гардероб (всё, кроме музыки)
void sndStore(int kind) {
  // kind 0 — выкл, 1 и 2 — одинаковые шины «Store»; с экрана провала (2) на выходе реплики обрываются отдельной командой; 3 (гардероб) в оригинале состояния нет — как 0
  if (kind == 3) kind = 0;
  if (!kind && storeKind == 2 && ready) audioStream(AUDIO_ST_DIALOG, -1, 0.1f);
  storeKind = kind; if (ready) modeTick();
}
void sndTubaNow(void) { tubaT = 0.f; }   // сверка g10b-loop круг 5 (СВЕРКА_С_ИСХОДНИКОМ 4.4, P09:17): комната без своей музыки и выход на улицу — туба прогулки сразу, не по таймеру
void sndJustMusic(int on) { justMusicOn = !!on; if (ready) modeTick(); }   // переход в помещение и из него (от начала до конца перехода)
void sndLoadDuck(int on) { loadDuckOn = !!on; if (ready) { modeSnap = 0; modeTick(); } }   // газета не из меню: звук мира как в паузе
void sndModeSnap(void) { modeSnap = 1; busLive = 0; }
static void applyMode(int m) {
  applyCalls++;
  static const char *const names[] = {"Normal", "Pause", "MissionScreen", "Letterbox", "Dialogue", "Store", "OnFoot",
                                      "Minigame", "JustMusic", "Credits"};
  if (!SV_MODEMIX_OLD && nmode) {
    modeTick(); lastApplied = m; lastAppliedMus = busNow[2];
    if (sfxLog) { char t[96]; snprintf(t, sizeof t, "SNDMODE %s music=%.2f sfx=%.2f pause=%d lb=%d cur=%d", names[m], busNow[2], busNow[0], pauseMode, lbMode, curMode); sfxLog(t); }
    return;
  }
  for (unsigned i = 0; i < nmode; i++)
    if (!strncmp(modes[i].name, names[m], 16)) {
      audioBus(AUDIO_BUS_SFX, modes[i].v[0]); audioBus(AUDIO_BUS_CAR, modes[i].v[1]);
      audioBus(AUDIO_BUS_MUSIC, modes[i].v[2]); audioBus(AUDIO_BUS_AMBIENCE, modes[i].v[3]);
      audioBus(AUDIO_BUS_DIALOG, modes[i].v[4]);
      lastApplied = m; lastAppliedMus = modes[i].v[2];
      if (sfxLog) { char t[96]; snprintf(t, sizeof t, "SNDMODE %s music=%.2f sfx=%.2f pause=%d lb=%d cur=%d", names[m], modes[i].v[2], modes[i].v[0], pauseMode, lbMode, curMode); sfxLog(t); }   // правка 279
      return;
    }
}
int sndModeDebug(char *out, int n) { return snprintf(out, n, "applied=%d mus=%.2f calls=%d nmode=%u pause=%d lb=%d", lastApplied, lastAppliedMus, applyCalls, nmode, pauseMode, lbMode); }
void sndMaster(float v) { if (ready) audioMaster(v * SND_HEADROOM); }
// громкость игрока из меню Sound оригинала (MUSIC / EFFECTS / VEHICLE / VOICE, 0..1): поверх шин тюнера диска
void sndUserVolumes(float music, float effects, float vehicle, float voice) {
  audioUserVolume(AUDIO_BUS_MUSIC, music); audioUserVolume(AUDIO_BUS_SFX, effects); audioUserVolume(AUDIO_BUS_AMBIENCE, effects);
  audioUserVolume(AUDIO_BUS_UI, effects);   // правка 114: звуки меню — громкость эффектов
  audioUserVolume(AUDIO_BUS_CAR, vehicle); audioUserVolume(AUDIO_BUS_DIALOG, voice);
}

// ---------------------------------------------------------------- per frame: gates, emitters, timers
// segment a->b against zone i in its local frame; returns entry parameter 0..1 or -1
static float gateHit(unsigned i, const float a[3], const float b[3]) {
  const XZone *zn = &zones[i];
  float la[3], lb[3];
  for (int k = 0; k < 3; k++) {
    const float *ax = zn->ax + 3 * k;
    la[k] = ax[0] * (a[0] - zn->t[0]) + ax[1] * (a[1] - zn->t[1]) + ax[2] * (a[2] - zn->t[2]);
    lb[k] = ax[0] * (b[0] - zn->t[0]) + ax[1] * (b[1] - zn->t[1]) + ax[2] * (b[2] - zn->t[2]);
  }
  if (zn->type == 0) {                                     // sphere: end point inside
    float r = zn->half[0], d = lb[0] * lb[0] + lb[1] * lb[1] + lb[2] * lb[2];
    return d <= r * r ? 1.f : -1.f;
  }
  float t0 = 0, t1 = 1;
  for (int k = 0; k < 3; k++) {                            // slab test
    float d = lb[k] - la[k], h = zn->half[k];
    if (fabsf(d) < 1e-6f) { if (la[k] < -h || la[k] > h) return -1; continue; }
    float u = (-h - la[k]) / d, v = (h - la[k]) / d;
    if (u > v) { float s = u; u = v; v = s; }
    if (u > t0) t0 = u; if (v < t1) t1 = v;
    if (t0 > t1) return -1;
  }
  return t0;
}

// Правка Роба 422 (09.10: «птицы / фон улицы всё ещё громкие»): ВКУС ВЛАДЕЛЬЦА, НЕ PS2 — замер В25 (правка 161, tests/amb161) держит
// фон улицы на уровне записи PS2; Роб просит тише. Шаг −3 дБ только на атмосфере улицы (зоны ambience.rms, старт уровня); атмосфера
// комнат (правка 345, roomAmb), музыка, мотор и эффекты — без изменений. AMB422_OLD=1 — прежнее (как PS2; красный tests/amb161)
#ifndef AMB422_OLD
#define AMB422_OLD 0
#endif
#define AMB_OUT_DB (AMB422_OLD ? 0.f : -3.f)   // правило порта (вкус Роба 09.10), шаг −3 дБ
float sndTestAmbOutDb(void) { return AMB_OUT_DB; }   // tests/amb161: поправка фона улицы этой сборки
static int roomAmb;
// сверка g08-sound (P09:1,28): уличная атмосфера звучит, только пока Гомер пешком (или в помещении). Посадка гасит её за 0,5 с, выход включает
// дорожку запомненной зоны; зоны, пересечённые в машине, обновляют дорожку без звука; в погоне Hit & Run зоны не запоминаются и после выхода
// фон не возвращается (до новой посадки). SV_AMBFOOT_OLD=1 — поток идёт всегда
#define AMB_FOOT_S 0.5f
static void ambGainApply(void) {
  float g = (roomAmb ? 1.f : powf(10.f, AMB_OUT_DB / 20.f)) * (SV_AMBFOOT_OLD ? 1.f : ambFootG);
  audioStreamSet(AUDIO_ST_AMBIENCE, g, 0.f);
}
static void setAmbience(int zone, int track, float fade) {
  ambZone = zone;
  if (track != ambTrack) { ambTrack = track; audioAmbience(track, fade); }
  ambGainApply();   // 422: улица −3 дБ, комната как была
}

// Правка 345: атмосфера комнаты. [PS2] атлас m0d: в Kwik-E-Mart оригинал читает AMBIENCE.RCF room_small_01 (кадр 6296 — Гомер уже
// в комнате, 63 кадра после △ у двери); в доме Симпсонов — тоже room_small_01 (agent345/ps2 house2, кадр 521, в комнате с 507). [диск] события L1_Kwik_E_Mart / L1_School / L1_House / L2_DMV композиции ambience.rms без
// локаторов — включаются входом в комнату. Выход — атмосфера улицы, что звучала до входа ([правило порта]: выход оригинала не снят)
int sndRoomAmbOld;
static int roomOutZone = -1, roomOutTrack = -1;
const char *sndRoom(const char *room) {
  if (!ready) return "-";
  int t = -1;
  if (room && !sndRoomAmbOld)
    for (unsigned i = 0; i < nramb; i++) {
      const char *a = rambs[i].room, *b = room; int n = 0;
      while (n < 28 && *a && *b && ((*a | 32) == (*b | 32))) { a++; b++; n++; }
      if (n < 28 && !*a && !*b) { char nm[29]; memcpy(nm, rambs[i].track, 28); nm[28] = 0; t = audioTrack(nm); break; }
    }
  // сверка g08-sound (P09:17, P09 Д9): комнаты вне списка из пяти имён (бар Мо, Android's Dungeon, обсерватория, комната Барта…) играют атмосферу и музыку
  // дома (индекс по умолчанию — дом): комнатная петля room_small_01 и кусок Tuba_005/012 (дорожки of_house, событие OF_House); Kwik-E-Mart — своя музыка (main_loop)
  int kwik = room && !strncasecmp(room, "Kwik", 4);
  if (t < 0 && room && !sndRoomAmbOld && !SV_ROOM_OLD) t = audioTrack("amb_room_small_01");
  if (room && !SV_ROOM_OLD && !kwik && !mOwned && !houseMus) {
    int h = audioTrack("of_house"); if (h >= 0) { audioMusicOnce(h, 0.5f); houseMus = 1; }
  } else if (!room && houseMus) { houseMus = 0; audioMusic(-1, 0.5f); tubaT = 0.f; }   // вышел: сразу туба прогулки
  if (t >= 0) {
    if (!roomAmb) { roomOutZone = ambZone; roomOutTrack = ambTrack; roomAmb = 1; }
    setAmbience(-1, t, 0.5f);
  } else if (!room && roomAmb) { roomAmb = 0; setAmbience(roomOutZone, roomOutTrack, 0.5f); }
  return ambTrack >= 0 ? audioTrackName(ambTrack) : "-";
}

// сверка g08-sound (P09:22): атмосфера старта миссии по таблице уровень × номер миссии, если Гомер пешком на старте или на прыжке к этапу. «Дом» —
// комнатная петля; L1: m4 комната АЭС 2 (amb_pp_two), m7 загородное шоссе (amb_countryside_day — имя региона по описанию в отчёте). Остальные —
// атмосфера по умолчанию (sndLevelStart). Вызывать из старта миссии (game_msn / main_levels): level 1..7, mission 0..7, onFoot
void sndMissionAmbience(int level, int mission, int onFoot) {
  if (!ready || SV_AMBFOOT_OLD || !onFoot) return;
  static const unsigned char house[8] = {0, 0, 0x80, 0x80, 0x81, 0x80, 0x80, 0x99};   // уровни 1..7: биты номеров миссий (L2 m7, L3 m7, L4 m0 m7, L5 m7, L6 m7, L7 m0 m3 m4 m7)
  const char *nm = 0;
  if (level == 1) nm = mission == 4 ? "amb_pp_two" : mission == 7 ? "amb_countryside_day" : 0;
  else if (level >= 2 && level <= 7 && (house[level] >> mission & 1)) nm = "amb_room_small_01";
  if (!nm) return;
  int t = audioTrack(nm); if (t < 0) return;
  if (strcmp(nm, "amb_room_small_01")) { setAmbience(-1, t, 0.5f); return; }
  if (!roomAmb) { roomOutZone = ambZone; roomOutTrack = ambTrack; roomAmb = 1; }
  setAmbience(-1, t, 0.5f);
}

void sndLevelStart(float px, float py, float pz) {
  if (!ready) return;
  // атмосфера старта — первая дорожка атмосферы пакета: tools/sound_export.py кладёт первой атмосферу старта уровня
  // (AMB_DEFAULT; у уровня 1 — amb_l1_suburbs_day, как было по имени до 29.09)
  int t = -1;
  for (int k = 0, n = audioTrackCount(); k < n; k++) if (audioTrackBus(k) == 1) { t = k; break; }
  ambFootG = onFootNow ? 1.f : 0.f; ambLatch = 0; modeSnap = 1; busLive = 0;   // сверка g08-sound: старт уровня — шины и фон сразу как должны быть
  ambTrack = -2; roomAmb = 0; setAmbience(-1, t, 0.5f);   // правка 345: старт уровня — улица
  if (clubOn) { clubOn = 0; audioStreamSet(AUDIO_ST_MUSIC, 1.f, 0.f); }   // правка 593
  ppx = px; ppy = py; ppz = pz; havePrev = 1;
}

static void clubExit(void) {   // правка 593: выход из клуба — затухание 4 с и обычная музыка (PS2: тема сходит за ~4–5 с)
  clubOn = 0; audioStreamSet(AUDIO_ST_MUSIC, 1.f, 0.f); musicFadeOver = CLUB_FADE_OUT;
  sndMusic(onFootNow ? SND_MUS_ONFOOT : SND_MUS_FREEROAM); musicFadeOver = -1.f; if (sfxLog) sfxLog("SNDCLUB exit");
}

void sndFrame(float dt, float cx, float cy, float cz, float rx, float rz, float px, float py, float pz) {
  if (!ready) return;
  if (dt < 0) dt = 0; if (dt > 0.25f) dt = 0.25f;
  if (!SV_LISTEN_OLD) {   // P09:27, P10-24: слушатель — камера, но не дальше 10 м от аватара (дальше подтягивается к нему на 10 м)
    float lx = cx - px, ly = cy - py, lz = cz - pz, ld = sqrtf(lx * lx + ly * ly + lz * lz);
    if (ld > 10.f) { float k = 10.f / ld; cx = px + lx * k; cy = py + ly * k; cz = pz + lz * k; }
  }
  audioListener(cx, cy, cz, rx, rz);
  stepClock += dt; boomT += dt;
  if (swT >= 0.f && (swT -= dt) < 0.f) { sfxWhy = "switch"; sndPlay2D(sndId("switch"), 1.f); sfxWhy = "fx"; }   // щелчок переключателя через 0,3 с
  if (svBoom.seq != lastBoomSeq) {   // взрыв машины (окно g01-cars → sverka_shared.h): трафик, ИИ и полиция — generic_car_explode 15–60 м, Break/Dcar (P10-32, P09 Д11/Д12)
    lastBoomSeq = svBoom.seq;
    if (!SV_CRASH_OLD && svBoom.kind != 0 && boomT > 0.3f) { collPlay("wreck", rExplode, svBoom.x, svBoom.y, svBoom.z, 1.f, 1); boomReplies(); boomT = 0.f; }
  }
  modeTick();
  if (!SV_AMBFOOT_OLD) {   // фон улицы: только пешком или в помещении
    // вышел в погоне, на этапе «музыка всегда» или при особом куске музыки (клуб, Stonecutters, Apu, Duff, Scary) — фон не возвращается:
    // до посадки, а при особом куске — до смены зоны (кусок кончается со следующей зоной)
    { int hold = mState == SND_MUS_HITRUN || alwaysMus, sp = specialOn || clubOn || vivOn;
      if (!onFootNow) ambLatch = 0;
      else if (hold || sp) ambLatch = 1;
      else if (ambLatch && !hold && !sp && ambLatchSp) ambLatch = 0;
      ambLatchSp = onFootNow && sp && !hold; }
    float want = ((onFootNow && !ambLatch) || roomAmb) ? 1.f : 0.f, st = dt / AMB_FOOT_S;
    ambFootG += clampf(want - ambFootG, -st, st); ambGainApply();
  }
  if (!SV_SAYTAB_OLD && sqn) {   // очередь реплик: жизнь строки, выдача по порядку, когда поток диалогов свободен
    for (int i = 0; i < sqn; ) { sq[i].life -= dt; if (sq[i].life <= 0.f) { for (int k = i + 1; k < sqn; k++) sq[k - 1] = sq[k]; sqn--; } else i++; }
    if (sqn && audioStreamTrack(AUDIO_ST_DIALOG) < 0) { int e = sq[0].ev; for (int k = 1; k < sqn; k++) sq[k - 1] = sq[k]; sqn--; sayStart(e); }
  }
  crashCool -= dt; objCool -= dt; sinceLine += dt; streetT += dt;
  for (int e = 0; e < SND_SAY_COUNT; e++) sayT[e] += dt;
  if (hitpWait > 0.f) {   // №183: очередь реплики героя после крика прохожего
    hitpWait -= dt;
    if (onFootNow) hitpWait = 0.f;
    else if (audioStreamTrack(AUDIO_ST_DIALOG) < 0) { hitpWait = 0.f; sinceLine = 99.f; sndSay(SND_SAY_HITP, 0); }
  }
  if (dmgStage == 3 && !onFootNow) burnT -= dt;   // №180/№184: повтор реплики горящей машины (sndCarDamage)
  for (int k = 0; k < SND_V_COUNT; k++) vT[k] += dt;
  // music: endings go back to free roam once they have played
  if (mReturn >= 0 && audioStreamTrack(AUDIO_ST_MUSIC) < 0) { int r = mReturn; mReturn = -1; sndMusic(r == SND_MUS_FREEROAM && onFootNow ? SND_MUS_ONFOOT : r); }
  { // правка 182: Vivaldi в тоннеле/зале Stonecutters
    const char *an = ambTrack >= 0 ? audioTrackName(ambTrack) : "";
    int inSc = vivId >= 0 && !mOwned && (MUSIC_FOOT_OLD || !onFootNow) && ((SV_MUSZONE_OLD && !strcmp(an, "amb_stonecutters_tunnel")) || !strcmp(an, "amb_stonecutters_hall"));   // сверка g08 (P09:18): Vivaldi — только зона 38 (зал), не тоннель 37
    int cur = audioStreamTrack(AUDIO_ST_MUSIC), hr = cur >= 0 && (cur == tId[T_HR_INTRO] || cur == tId[T_HR_MAIN] || cur == tId[T_HR_NEG]);
    if (inSc && !vivOn && !hr) { vivOn = 1; mReturn = -1; audioMusic(vivId, 1.f); }
    else if (vivOn && !inSc) { vivOn = 0; sndMusic(onFootNow ? SND_MUS_ONFOOT : SND_MUS_FREEROAM); }
  }
  // правка 179: туба пешком — своя музыка молчит, не миссия, не Vivaldi
  if (onFootNow && nTuba && !mOwned && !vivOn && !clubOn && audioStreamTrack(AUDIO_ST_MUSIC) < 0) {
    if ((tubaT -= dt) <= 0.f) {
      int k = (int)(frand() * nTuba) % nTuba; if (nTuba > 1 && k == tubaLast) k = (k + 1) % nTuba;
      tubaLast = k; audioMusic(tubaId[k], 0.f); tubaT = TUBA_MIN + frand() * (TUBA_MAX_S - TUBA_MIN);
    }
  } else if (!onFootNow) tubaT = TUBA_MIN + frand() * (TUBA_MAX_S - TUBA_MIN);
  // ambience gates crossed by the player this frame; the last one along the path wins
  if (!havePrev) { ppx = px; ppy = py; ppz = pz; havePrev = 1; }
  float a[3] = {ppx, ppy, ppz}, b[3] = {px, py, pz};
  float mv = (px - ppx) * (px - ppx) + (pz - ppz) * (pz - ppz);
  if (mv > 1e-6f && mv < 50.f * 50.f && !roomAmb && (SV_AMBFOOT_OLD || mState != SND_MUS_HITRUN)) {   // a teleport is not a walk through gates; правка 345: в комнате — её атмосфера; сверка g08 (P09 Д17): в погоне зоны не запоминаются
    int best = -1; float bt = -1;
    for (unsigned i = 0; i < nzone; i++) {
      float t = zones[i].event == CLUB_EVENT && clubId >= 0 ? -1.f : gateHit(i, a, b);   // правка 593: зона клуба — музыка, не атмосфера
      if (t >= 0 && t >= bt && zoneTrack[i] >= 0) { bt = t; best = (int)i; }
    }
    if (best >= 0 && zoneTrack[best] != ambTrack) setAmbience(best, zoneTrack[best], 0.6f);
    if (best >= 0 && !SV_MUSZONE_OLD) {   // сверка g08-sound (P09:18,31): особые куски музыки по зонам атмосферы
      // подавление повтора: событие пропускается, если это та же зона, что только что, либо она уже включалась меньше 0,5 с назад (таймер на зону)
      int ev = (int)zones[best].event & 127; float since = stepClock - zoneAt[ev];
      if (ev != zoneLast && since >= 0.5f) {
        zoneLast = ev; zoneAt[ev] = stepClock;
        // зоны 11 (крыша Kwik-E-Mart) и 35 (наружный Duff) играют и в миссиях; остальные — только в свободной езде
        static const struct { int ev; const char *name; int anyTime; } sp[] = {{11, "OF_Apu_Oasis", 1}, {56, "OF_Apu_Oasis", 0}, {54, "ScaryMusic01", 0},
                                                                               {35, "DuffBeer", 1}, {69, "DuffBeer", 0}, {62, "StoneCutters", 0}};
        int hit = 0;
        for (unsigned k = 0; k < sizeof sp / sizeof *sp && !hit; k++)
          if (sp[k].ev == ev && sndMusicEventIn(sp[k].name, 0, sp[k].anyTime) == 1) hit = 1;
        if (hit) specialOn = 1;
        else if (specialOn) { specialOn = 0; sndMusic(onFootNow ? SND_MUS_ONFOOT : SND_MUS_FREEROAM); }   // следующая зона атмосферы — обычная музыка
      }
    }
  }
  { // правка 593: клуб Legitimate Businessmen. Зоны события 61 — тонкие плиты-ворота (L2: 2,4 м и 2,8 м толщиной): вход в плиту включает Social_Club;
    // повторное пересечение плиты — снова клуб (без переключения); конец — удаление от плит на CLUB_EXIT_R м, телепорт или миссия
    if (clubOn && mv >= 50.f * 50.f) clubExit();
    if (clubOn) {   // нет музыкальных событий вокруг клуба (локаторы L2/L5: кроме 61 только атмосфера и пауза), выход — удаление от обеих плит на CLUB_EXIT_R м
      float md = 1e30f;
      for (unsigned i = 0; i < nzone; i++) if (zones[i].event == CLUB_EVENT) { float dx = px - zones[i].t[0], dz = pz - zones[i].t[2], d2 = dx * dx + dz * dz; if (d2 < md) md = d2; }
      if (md > (float)CLUB_EXIT_R * (float)CLUB_EXIT_R) clubExit();
    }
    if (clubId >= 0 && !mOwned && !roomAmb && mv > 1e-6f && mv < 50.f * 50.f && !clubOn) {
      int hit = 0;
      for (unsigned i = 0; i < nzone && !hit; i++) if (zones[i].event == CLUB_EVENT) hit = gateHit(i, a, b) >= 0 && gateHit(i, a, a) < 0;   // только вход в плиту
      if (hit) { clubOn = 1; mReturn = -1; audioMusic(clubId, CLUB_FADE_IN); audioStreamSet(AUDIO_ST_MUSIC, CLUB_GAIN, 0.f); if (sfxLog) sfxLog("SNDCLUB enter"); }
    }
  }
  ppx = px; ppy = py; ppz = pz;
  // positional loop: the nearest audible emitter (they are far apart in the level; one stream)
  if (animStreamT > 0.f) { animStreamT -= dt; if (animStreamT > 0.f) return; emitCur = -2; }   // круг 6: дорожка подвижного объекта занимает позиционный поток, петля излучателя ждёт
  int best = -1; float bd = 1e18f;
  for (unsigned i = 0; i < nemit; i++) {
    const XEmit *e = &emits[i];
    float dx = e->x - cx, dy = e->y - cy, dz = e->z - cz, d = dx * dx + dy * dy + dz * dz;
    if (d < e->mx * e->mx && d < bd) { bd = d; best = (int)i; }
  }
  if (best != emitCur) {
    // сверка g08-sound (P09:26): вероятность проигрыша (PlaybackProbability: Хэллоуин-ловушки 0,3, гудок трафика 0,5) бросается один раз при входе в
    // объём, играет при успехе на полной громкости (pad пакета = вероятность·1000 + 1; 0 — старый пакет, вероятность в громкости)
    emitMute = 0;
    if (best >= 0 && !SV_POSLOOP_OLD && emits[best].pad > 0 && frand() * 1000.f >= (float)(emits[best].pad - 1)) emitMute = 1;
    if (best >= 0 && !emitMute && (emitCur < 0 || emits[best].track != emits[emitCur].track)) audioStream(AUDIO_ST_POSITIONAL, emits[best].track, 0.3f);
    if (best < 0 || emitMute) audioStream(AUDIO_ST_POSITIONAL, -1, 1.f);
    emitCur = best;
  }
  if (best >= 0 && !emitMute) {
    audioStreamSet(AUDIO_ST_POSITIONAL, emits[best].gain, 0.f);
    audioStreamAt(AUDIO_ST_POSITIONAL,emits[best].x,emits[best].y,emits[best].z,emits[best].mn,emits[best].mx);
  }
}

void sndStats(SndStats *s) {
  memset(s, 0, sizeof *s);
  if (!ready) return;
  AudioStats a; audioStats(&a);
  s->musicState = mState; s->ambZone = ambZone; s->posEmitter = emitCur;
  s->modeApplied = pauseMode ? SND_MODE_PAUSE : lbMode ? SND_MODE_LETTERBOX : curMode;
  s->dialogPlaying = a.track[AUDIO_ST_DIALOG] >= 0;
  s->voices = a.voices; s->underruns = a.underruns; s->loops = a.loops; s->dropped = a.dropped; s->cpu = a.cpu; s->mixUsMax = a.mixUsMax;
  s->music = a.track[0] >= 0 ? audioTrackName(a.track[0]) : "-";
  s->ambience = a.track[1] >= 0 ? audioTrackName(a.track[1]) : "-";
  s->dialog = a.track[2] >= 0 ? audioTrackName(a.track[2]) : "-";
  s->positional = a.track[3] >= 0 ? audioTrackName(a.track[3]) : "-";
}
