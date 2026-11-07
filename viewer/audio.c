// SHAR PSP audio runtime. See audio.h for the API and tools/audio_export.py for the pack format.
//
// Why a software mixer over the original VAG data:
//  - the disc data IS PS-ADPCM; we keep it bit-for-bit (0 quality loss, 3.5:1 vs PCM, no transcode),
//    and the decoder below is bit-exact with ffmpeg's adpcm_psx (checked on the Mac by tests/audio/host);
//  - decode is ~6 integer ops per sample, resample+mix ~12 per output frame per voice: ~2-4% of the
//    333 MHz Allegrex for 8 voices (the thread measures itself, see AudioStats.cpu);
//  - sample-exact loops (loop points + stored ADPCM history) and gapless intro->loop chaining, which
//    MP3/ATRAC via the Media Engine cannot do (encoder delay/padding), and deterministic behaviour that
//    is identical in PPSSPP, on hardware and in the Mac test harness.
//  The same VAG blocks can later be fed to sceSasCore (pspsascore.h, hardware-assisted VAG voices) for
//  many simultaneous positional effects without re-exporting anything.
#include "audio.h"
#include "reliab.h"   // №1192: relNote
// Аудит 09.10 п. 22 (правка 161). AMB161_OLD=1 — прежнее: ×AMB_GAIN только при загрузке банка, а первый же набор шин режима
// (sound.c applyMode из sndInit, sndMode) присваивал шине фона значение тюнера диска — множитель 0,6 не действовал ни разу
// (замер tests/amb161: множитель фона пешком 0,70 = OnFoot 1,0 × мастер 0,7, а не 0,42). Теперь множитель живёт в audioBus.
// Значение — по замеру PS2 В25 (knowledge/facts/ps2-ambience-level.yaml, build/twins/o4/v25_audio/audio.wav): фон пешком к
// музыке у PS2 +4,4 дБ (верхняя граница: в записи кроме фона есть и другие звуки), у порта с тюнером диска +2,2 дБ; с ×0,6 было
// бы −2,2 дБ, на 6,6 дБ тише PS2. Поэтому множитель 1 (тюнер диска global.spt как есть); 0,6 «на слух» замер не подтверждает
#ifndef AMB161_OLD
#define AMB161_OLD 0
#endif
#ifndef AMB_GAIN
#if AMB161_OLD
#define AMB_GAIN .6f   // правка Роба 161 (01.10): шина фоновых звуков ×0.6 (−4.4 дБ)
#else
#define AMB_GAIN 1.f   // аудит 09.10 п. 22: тюнер диска как есть (замер PS2 В25, см. выше)
#endif
#endif
// Правка Роба 250 (03.10, «эффекты слишком громкие — свести, как в оригинале»). SND_250_OLD=1 — прежнее сведение (красный
// контроль гейта tests/sfx250). Здесь: щелчок gearshift на каждом повышении передачи — гипотеза порта (у car_tune.spt есть только
// провал высоты GearShiftPitchDrop); у оригинала этого щелчка нет: скан всей записи PS2 (43 мин, точный шаблон клипа) — ни одного
// совпадения выше 0,36, тот же скан монеты — до 0,90. У порта он звучал на каждом повышении (0,7 × шина мотора 0,88 = −2 дБ к музыке)
#ifndef SND_250_OLD
#define SND_250_OLD 0
#endif
#ifndef ENG_250_GAIN
#define ENG_250_GAIN .84f   // правка 250: мотор игрока к музыке по замеру оригинала (правило порта по одному замеру v13, ±1 дБ)
#endif
static int mix250 = !SND_250_OLD;   // правка 250 (второй проход 04.10): только уровень 1 — переключает sound.c sndInit
void audioMix250(int on) { mix250 = on != 0; }
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "mixdsp.h"

#define RATE 44100
#define FRAMES 512                 // per sceAudio output call: 11.6 ms
#define NVOICE 24                  // resident voices (+2 stream voices)
#define WIN (28 * 8)               // decoded window per voice, samples
#define NSTREAM AUDIO_NSTREAM       // 0 music, 1 ambience, 2 dialogue, 3 positional loop
static const unsigned ringSize[NSTREAM] = {128 * 1024, 128 * 1024, 64 * 1024, 64 * 1024, 64 * 1024}; // bounded independent car ring
#define CHUNK (16 * 1024)          // one memory stick read
#define NSEG 8

#ifndef PSP_POWER_CB_POWER_SWITCH
#define PSP_POWER_CB_POWER_SWITCH	0x80000000
#define PSP_POWER_CB_STANDBY		0x00080000
#define PSP_POWER_CB_RESUME_COMPLETE	0x00040000
#define PSP_POWER_CB_RESUMING		0x00020000
#define PSP_POWER_CB_SUSPENDING		0x00010000
#endif

#ifndef RESUME_AUDIO_OLD
#define RESUME_AUDIO_OLD 0
#endif
static int resumeAudioOld = RESUME_AUDIO_OLD;
void audioResumeOld(int on) { resumeAudioOld = on != 0; }

static volatile int pwrSleeping;
static volatile int pwrResumeReq;
static volatile int pwrResumeMix;
static volatile int pwrResumeIo;

#ifdef AUDIO_HOST
#include <time.h>
typedef FILE *AFile;
#define AF_OK(f) ((f) != NULL)
#define AF_NONE NULL
static AFile afOpen(const char *p) { return fopen(p, "rb"); }
static int afRead(AFile f, unsigned off, void *dst, int n) {
  if (!f) return -1;
  if (fseek(f, (long)off, SEEK_SET)) return -1;
  return (int)fread(dst, 1, (size_t)n, f);
}
static unsigned afSize(AFile f) { fseek(f, 0, SEEK_END); return (unsigned)ftell(f); }
static void afClose(AFile f) { fclose(f); }
static unsigned nowUs(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000000ull + t.tv_nsec / 1000);
}
static void *amalloc(unsigned n) { void *p = 0; return posix_memalign(&p, 64, n) ? 0 : p; }
#define BARRIER() __asm__ volatile("" ::: "memory")
#else
#include <pspkernel.h>
#include <pspaudio.h>
#include <malloc.h>
#include "fpuguard.h"   // правка 252: ловушки FPU потоков звука выключены, случаи — в чёрный ящик (viewer/fpuguard.h)
typedef SceUID AFile;
#define AF_OK(f) ((f) >= 0)
#define AF_NONE (-1)
static AFile afOpen(const char *p) { return sceIoOpen(p, PSP_O_RDONLY, 0); }
static int afRead(AFile f, unsigned off, void *dst, int n) {
  if (sceIoLseek32(f, (int)off, PSP_SEEK_SET) != (int)off) return -1;
  return sceIoRead(f, dst, n);
}
static unsigned afSize(AFile f) { return (unsigned)sceIoLseek32(f, 0, PSP_SEEK_END); }
static void afClose(AFile f) { sceIoClose(f); }
static unsigned nowUs(void) { return sceKernelGetSystemTimeLow(); }
static void *amalloc(unsigned n) { return memalign(64, n); }
#define BARRIER() __asm__ volatile("" ::: "memory")
#endif

// ---------------------------------------------------------------- pack structures (little-endian, see exporter)
typedef struct {
  char magic[4]; unsigned ver, nres, nfile, ncar, resOff, varOff, fileOff, carOff, dataOff, dataSize;
  float bus[6], peel[3];
  unsigned extOff, extSize;        // @80: extension blob (tools/sound_export.py "SXT1"), 0 = none
} SabHdr;
typedef struct { char name[24]; unsigned short firstVar, nVar; float trim, pmin, pmax; unsigned flags, bus; } SabRes;
typedef struct {
  unsigned off, nblk, nsamp, loopStart, loopEnd; unsigned short rate; unsigned char ch, pad; short hist[4];
} SabFile;
typedef struct {
  char name[16]; short engine, idle, horn, damaged, doorOpen, doorClose, skid, dirtSkid;
  float idlePitch, revCap, revLo, revHi, shift[5], lo[5], hi[5], drop[5];
  float attackMs, delayMs, decayMs, decayFinish, damper, clipRPM;
} SabCar;
typedef struct {
  char name[28]; unsigned off, nblk, nsamp, loopStart, loopEnd; unsigned short rate; unsigned char ch, flags;
  short next, bus; short hist[4];
} SapTrack;
typedef char chk_res[sizeof(SabRes) == 48 ? 1 : -1];
typedef char chk_file[sizeof(SabFile) == 32 ? 1 : -1];
typedef char chk_car[sizeof(SabCar) == 152 ? 1 : -1];
typedef char chk_trk[sizeof(SapTrack) == 64 ? 1 : -1];

// ---------------------------------------------------------------- state
typedef struct { int track; unsigned start, end; short hist[4]; } Seg;
typedef struct {
  unsigned char *ring; unsigned mask;       // ring size - 1 (power of two)
  volatile unsigned wr, rd;                 // byte counters: producer / consumer
  Seg seg[NSEG]; volatile unsigned sw, sr;  // segment queue: producer / consumer
  volatile int cmdTrack, cmdGen, ackGen;    // mixer -> reader: restart with this track
  volatile int queued, once;
  int pGen, pTrack, pActive; unsigned pBlk, pEnd;   // reader-private
  volatile int reqTrack, reqGen, reqArmed; volatile float reqFade;  // main -> mixer
  volatile unsigned reqStart, cmdStart; unsigned pendStart;   // №789: с какого сэмпла начать дорожку (main -> mixer -> reader; 0 — с начала)
  volatile float vol, pan;                  // main -> mixer: stream voice level / pan (audioStreamSet)
  int doneReq, state, pend, cur; float fade, fadeStep; // mixer-private
} Stream;
enum { S_IDLE, S_FADE, S_WAIT, S_PLAY };

enum { V_FREE, V_START, V_PLAY };
// Голос: управление (пишут игра и поток смешивания). Состояние счёта — в задании микшера (viewer/mixdsp.h MixVoice, mixJ.v[k]):
// правка 276 — его может считать Media Engine, а он не видит кэш главного процессора; общих строк кэша у задания и голосов нет
typedef struct {
  volatile int state, stopReq;
  int gen;
  volatile float vol, pan, pitch;
  volatile unsigned pointSeq; volatile MixPoint point;
  float trim; int bus;
  Stream *st; const SabFile *f;
  int ch, loop, rate;                       // вход старта (audioPlay): в задание — на V_START
} Voice;
typedef char chk_mix[FRAMES == MIX_FRAMES && NVOICE == MIX_NVOICE && NSTREAM == MIX_NSTREAM && NSEG == MIX_NSEG && WIN == MIX_WIN ? 1 : -1];

static unsigned char *bank; static const SabHdr *H;
// №1069 (Роб 30.10, тур TEST79: у Апу не было звука — L5 sfx.sab 2 131 776 Б одним куском, а наибольший кусок кучи после смены уровня
// 1,91 МБ при свободных 22,5; тот же отказ в TEST77). Банк звука — в постоянном месте: выделяется один раз при первом audioInit (звук
// уровня 1 при запуске игры, куча ещё цельная) размером с наибольший банк уровней; смена уровня читает новый банк в то же место. Банк больше
// места — место заново из кучи (как прежде). SNDSLOT_OLD=1 / файл sndslotold / «flag sndslotold 1» — как до правки: банк каждый раз из кучи
#ifndef SNDSLOT_OLD
#define SNDSLOT_OLD 0
#endif
#define SAB_SLOT_BYTES (2112u << 10)   // 2 162 688 Б: наибольший sfx.sab L1–L7 и бонуса — 2 154 944 (L2), вверх до 64 КБ
static int slotOld = SNDSLOT_OLD; static unsigned char *slot; static unsigned slotCap, bankCapSim;
void audioSlotOld(int on) { slotOld = on; }
void audioBankCapSim(unsigned bytes) { bankCapSim = bytes; }   // тест «flag sndcapsim N»: банк из кучи больше N байт не выделяется — как на PSP после смен уровня
static void *bankAlloc(unsigned n) { return bankCapSim && n > bankCapSim ? 0 : amalloc(n); }
static const SabRes *R; static const unsigned short *VAR; static const SabFile *F; static const SabCar *car;
// Authored aliases (witchcar_v_skid) share blank.rsd's file index in SAB.
static int blankFile = -1;
#ifndef CARSND_BLANK_ALIAS_OLD
#define CARSND_BLANK_ALIAS_OLD 0
#endif
// Правка 413а: профиль машины игрока (car_tune.spt + свой звук заноса) — по машине, которую он ведёт (audioPlayerCar); 0 — машина
// уровня. carCur — копия профиля, где гудок / разбитый мотор / двери, которых у профиля трафика в пакете нет, — от машины уровня
static SabCar carCur; static int carIdx;
static const short *covl; static unsigned ncovl; static int carOvl = -1;   // правка 413: OverlayClipName профилей (EXT 'COVL'), наложение машины игрока
#ifndef CAR_STREAM_OLD
#define CAR_STREAM_OLD 0
#endif
typedef struct { unsigned short first, count; } CarStreamProfile;
typedef struct { short track, pad; float trim; } CarStreamVariant;
static const CarStreamProfile *covs; static const CarStreamVariant *covf;
static unsigned ncovs,ncovf; static int carStream=-1,carStreamOn;
static const unsigned char *DATA;
static SapTrack *T; static int ntrack; static AFile mfd;
// Frontend NIS bank has stable storage: loading it never reallocates live T.
#define EXTRA_TRACK_BASE 0x4000
#define EXTRA_TRACK_MAX 48   // №1104: 12 реплик меню + 31 реплика титров (код Play Credits Dialog), было 16
static SapTrack ET[EXTRA_TRACK_MAX];static volatile int netrack;
static AFile emfd=AF_NONE;static char extraPath[256];static char musicPath[256];
// №1111 (языки): озвучка диалогов на языке копии игры лежит отдельным файлом dialog_<fr|ge|sp>.sap рядом с music.sap (tools/sound_export.py --lang). Дорожки английского пакета
// с переводом получают строку из него (метка 0x80 в flags: читать из этого файла); нет файла или перевода — английская дорожка. LANGVO1111_OLD=1 — язык озвучки не применяется
#ifndef LANGVO1111_OLD
#define LANGVO1111_OLD 0
#endif
#define TRK_LANG 0x80
static AFile lfd = AF_NONE; static char langPath[256]; static int langSel, langTracks;
void audioSetLanguage(int lang) { langSel = lang >= 0 && lang <= 3 ? lang : 0; }
int audioLanguageTracks(void) { return langTracks; }
int audioLangAvail(const char *dir, int lang) {   // №1111: в папке пакета есть файл озвучки этого языка (число дорожек в нём больше нуля)
  static const char *const LF[4] = {0, "%sdialog_fr.sap", "%sdialog_ge.sap", "%sdialog_sp.sap"};
  if (lang < 1 || lang > 3 || !dir) return 0;
  char p[256]; snprintf(p, sizeof p, LF[lang], dir); AFile f = afOpen(p); if (!AF_OK(f)) return 0;
  unsigned char h[64]; int ok = afRead(f, 0, h, 64) == 64 && !memcmp(h, "SAPL", 4) && *(unsigned *)(h + 8) > 0; afClose(f); return ok;
}
   // сколько дорожек сейчас с озвучкой языка (для журнала)
static const SapTrack *trackRow(int t) {
 if(t>=0&&t<ntrack)return T+t;
 return t>=EXTRA_TRACK_BASE&&t<EXTRA_TRACK_BASE+netrack?ET+(t-EXTRA_TRACK_BASE):NULL;
}
static int haveMusic;
static Voice voices[NVOICE + NSTREAM];
static Stream streams[NSTREAM];
static float busVol[AUDIO_NBUS]; static float master = 1.f;
static float userVol[AUDIO_NBUS] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};   // громкость игрока (меню Sound), поверх тюнера диска
static MixJob mixJ;                       // правка 276: задание микшера (состояние счёта всех голосов, сумма acc)
static MixJob *J = &mixJ;                 // текущее задание (ME не ответил — счёт переезжает в копию, mixJ больше не трогаем)
static unsigned strmSi[NSTREAM];          // позиция потока (сэмпл) после последнего буфера — для audioStats
static volatile int quit;
static char err[96];
static unsigned rng = 0x2545F491u;
static float lx, ly, lz, lrx = 1.f, lrz;
// A bounded set shares resident voice slots and the existing DSP job.
typedef struct { int eng, idle, fire, horn, skid, skidRes, gear; float pitch, env, off, dip, dipMul; unsigned last; MixPoint point; int ovl;   // ovl — наложение (413)
                int sh, shOn, coast; float shT, shP0, shD, coastV, coastP, prevSpd, fastT; } Engine;   // сверка g08-sound: голос повышения передачи, накат
static Engine E, worldEngine[AUDIO_WORLD_ENGINES];
static int worldSource[AUDIO_WORLD_ENGINES], worldProfile[AUDIO_WORLD_ENGINES];
static float clampf(float v,float a,float b);
static int spatialQ(float x) { x=clampf(x,-1048576.f,1048576.f)*16.f;return (int)(x+(x<0.f?-.5f:.5f)); }
static MixPoint spatialPoint(float x,float y,float z,float mn,float mx) {
  MixPoint p={1,{spatialQ(x),spatialQ(y),spatialQ(z)},0,0};
  p.max=spatialQ(clampf(mx,.0625f,2048.f));p.min=spatialQ(clampf(mn,0.f,2048.f));
  if(p.min>=p.max)p.min=p.max-1;return p;
}
static void setPoint(Voice *v,MixPoint p) {
  v->pointSeq++;BARRIER();v->point=p;BARRIER();v->pointSeq++;
}
static volatile int underruns, dropped, seams, ioMaxMs; static volatile unsigned ioBytes;
static float statCpu, statUs, statUsMax;
void (*audioNote)(int id, const char *why);   // правка 135.7 (sound.c sndLogHook)
static unsigned framesOut;
static int unity;
static float limGain = 1.f; static int limPrev = 4096; static unsigned limActive;   // output limiter
#ifdef AUDIO_HOST
void (*audioTestSeamHook)(unsigned frame);
#endif

void audioSeed(unsigned t) { rng = 0x2545F491u ^ (t * 2246822519u); if (!rng) rng = 1; }
static float frand(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (rng >> 8) * (1.f / 16777216.f); }
static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

// ---------------------------------------------------------------- mixing (правка 276: счёт — viewer/mixdsp.h, здесь управление)
// Буфер = mixPre (главный: потоки, старт/стоп голосов, усиления, копия колец музыки в задание) -> mixDspRun (сам счёт: здесь же
// или на Media Engine) -> mixPost (главный: съеденное из колец, освобождение голосов, ограничитель, выход). Порядок действий и
// числа — как в прежнем mixBuffer (сверка бит в бит: tests/me/run.py, шаг «Мак, микшер»).
static unsigned trackEnd(int t) { const SapTrack *r=trackRow(t);return (r->flags & 1) ? r->loopEnd : r->nsamp; }

// ---------------------------------------------------------------- правка 410, К1 (09.10, docs/handoff/ME_ПЛАН_2026-10-09.md): сброс кэша
// задания звука по частям. Было: перед каждой отдачей на ME главный сбрасывал задание целиком (43 776 Б, 684 строки кэша). Теперь — только
// строки, которые главный трогал с прошлого ответа ME: заголовок (data, listener, seams), сумма acc (её читает mixPost), окна потоков
// (заголовок, очередь кусков и заполненные байты win), заголовки голосов, у которых менялось хоть что-то, и голос целиком после
// dspReset (он пишет buf). Поправка скептика П3: строка с «on» сбрасывается у КАЖДОГО голоса, у которого «on» поменялся (звучал -> тишина),
// а у голоса, который и раньше молчал, «on» не пишется вовсе: в памяти уже 0 (onMem — что лежит в J->v[k].on; ME «on» не меняет).
// Кэш главного над заданием после сброса пуст — между ответом ME и следующей отдачей главный трогает только эти строки (инвариант);
// главный сам посчитал задание (ME занят, тайм-аут, путь без ME) или стёр его — следующий сброс целиком (wbAll). Гейт на Маке:
// tests/me/run.py «Мак, микшер», -DMIX_JOB_COPY=3 — модель несогласованного кэша (ME видит только сброшенное, главный после ответа видит
// свежим только сброшенное) бит в бит с прямым счётом + счётчики «on» у ME ≠ у главного и «запись мимо сброса» = 0; красные ME_WB_RED.
// ME_WB_OLD=1 — прежний путь целиком (сборка); файл mewbold рядом с EBOOT — прежний сброс целиком в этой же сборке (тур A/B)
// №1100 (Роб 02.11: «разгрузить процессор через ME»): рукопожатие звука с ME стоит главному ~410 мкс на буфер (pre 81, svc 190, post 139 в туре TEST82). На Маке счёт самого mixPre
// и mixPost занимает 0,3 и 1,2 мкс (×5,8 — единицы мкс на PSP): цену задают не вычисления, а число байт, которые главный трогает и сбрасывает из кэша, и вызовы ядра.
// Первое, что режется без риска: окно кольца потока в задании. Копировалось всё лежащее в кольце до MIX_SWIN = 2048 Б на каждый звучащий поток (до 5 потоков), а буфер потребляет
// не больше ~900 Б (512 кадров при 48 кГц и шаге 1,09 → 560 сэмплов, плюс упреждение распаковки MIX_WIN = 224 → 784 сэмпла = 28 блоков по 32 Б = 896 Б). Теперь окно не больше
// SWIN_NEED = 1024 Б: вдвое меньше memcpy, строк кэша в сбросе и слов в mixJobIn. Страховка: поток, у которого окна не хватило (underrun) три раза, возвращает полное окно
// для всех потоков (handMiss), включается/выключается «flag mehand 0/1» и файлом mehandold. MEHAND1100_OLD=1 — как было (2048)
#ifndef MEHAND1100_OLD
#define MEHAND1100_OLD 0
#endif
#ifndef SWIN_NEED
#define SWIN_NEED 1024
#endif
static int meHand = !MEHAND1100_OLD; static unsigned handMiss, handWinB, handWinMax;
void audioMeHand(int on) { meHand = on != 0 && !MEHAND1100_OLD; handMiss = 0; }
int audioMeHandOn(void) { return meHand; }
#include <stddef.h>
#ifndef ME_WB_OLD
#define ME_WB_OLD 0
#endif
#ifndef ME_WB_RED
#define ME_WB_RED 0   // только гейт на Маке: 1 — П3 (строку «on=0» замолчавшего голоса не сбрасывать), 2 — окна потоков: только заголовок
#endif
static int wbOld;                          // 1 — сброс целиком (файл mewbold, viewer/me/meprobe.c meAudioBoot)
void audioMeWbOld(int on) { wbOld = on != 0; }
static unsigned char onMem[MIX_NV];        // что лежит в J->v[k].on (пишет только главный)
static unsigned char vFl[MIX_NV];          // этот буфер: 1 — заголовок голоса тронут, 2 — голос целиком (dspReset пишет buf)
static int wbAll = 1;                      // следующий сброс — целиком
typedef struct { unsigned a, b; } WbR;     // [a, b) — смещения в задании, кратные 64
static WbR wbR[MIX_NV + MIX_NSTREAM + 4]; static int wbN;
static void wbAdd(unsigned a, unsigned b) {   // участки идут по возрастанию адреса: соседние и перекрытые сливаются
  a &= ~63u; b = (b + 63u) & ~63u; if (b > sizeof(MixJob)) b = sizeof(MixJob);
  if (wbN && a <= wbR[wbN - 1].b) { if (b > wbR[wbN - 1].b) wbR[wbN - 1].b = b; return; }
  wbR[wbN].a = a; wbR[wbN].b = b; wbN++;
}
static void wbBuild(const MixJob *j) {      // конец mixPre: что главный тронул в этом буфере
  wbN = 0;
  wbAdd(0, offsetof(MixJob, rec));                                            // data, seams, nrec, listener, right
  wbAdd(offsetof(MixJob, acc), offsetof(MixJob, acc) + sizeof j->acc);        // сумма: mixPost читает (и ограничитель может писать)
  for (int s = 0; s < MIX_NSTREAM; s++) {                                     // окно потока: заголовок всегда; очередь и байты — у звучащего
    unsigned o = offsetof(MixJob, w) + (unsigned)s * sizeof(MixWin);
    unsigned e = j->v[MIX_NVOICE + s].on && ME_WB_RED != 2 ? offsetof(MixWin, win) + j->w[s].len : offsetof(MixWin, seg);
    wbAdd(o, o + e);
  }
  for (int k = 0; k < MIX_NV; k++) if (vFl[k]) {
    unsigned o = offsetof(MixJob, v) + (unsigned)k * sizeof(MixVoice);
    wbAdd(o, o + (vFl[k] == 2 ? sizeof(MixVoice) : offsetof(MixVoice, buf)));
    vFl[k] = 0;
  }
}

static void dspReset(MixVoice *d) {
  d->bufLen = 1; d->buf[0][0] = d->buf[1][0] = 0; d->pos = 1u << 16;
  d->s1[0] = d->s1[1] = d->s2[0] = d->s2[1] = 0; d->finished = d->underrun = d->tail = d->segs = 0;
#if !ME_WB_OLD
  { ptrdiff_t k = d - J->v; if (k >= 0 && k < MIX_NV) vFl[k] = 2; }   // правка 410 К1: buf тронут — голос в сброс целиком
#endif
}

#ifndef SV_STOPFADE_OLD
#define SV_STOPFADE_OLD 0   // круг 5 (g08, P10-25): 1 — остановка голоса обрывом за один буфер, как было
#endif
#ifndef SV_STOPFADE7_OLD
#define SV_STOPFADE7_OLD 0   // круг 7: оригинал гасит остановленный голос ровно за 0,1 с (шаг 11,61 мс / 100 мс = 0,1161 за буфер); 1 — 0,12 за буфер (≈ 97 мс)
#endif
// №1144 (пауза бонус-игры): пакет бонус-игры без таблицы режимов (MODE), поэтому sndPause там ничего не делал и музыка в паузе играла. В оригинале пауза (набор «Pause» все шины в 0 за 750 мс + событие
// паузы композиции, которое останавливает музыку и по снятию продолжает с того же места). Здесь: общий множитель всех голосов pauseLvl уходит в 0 за 750 мс и возвращается; пока он 0, голоса потоков
// (музыка, фон, речь) стоят на месте (шаг 0) — после паузы музыка продолжается с того же места. Включает sound.c sndPause, когда в пакете нет таблицы режимов. BONUSPAUSEMUS1144_OLD=1 — как было
#ifndef BONUSPAUSEMUS1144_OLD
#define BONUSPAUSEMUS1144_OLD 0
#endif
static volatile int pauseReq; static float pauseLvl = 1.f;
void audioPauseAll(int on) { pauseReq = on != 0 && !BONUSPAUSEMUS1144_OLD; }
float audioPauseLevel(void) { return pauseLvl; }
static int gFadeL[NVOICE], gFadeR[NVOICE];   // уровень голоса в прошлом буфере (затухание остановки)
static void gains(const Voice *v, const MixVoice *d, float extra, int *tgl, int *tgr, unsigned *tstep) {
  float g = unity ? 1.f : clampf(v->vol, 0, 1) * v->trim * busVol[v->bus] * userVol[v->bus] * extra, pan = unity ? 0 : clampf(v->pan, -1, 1);
  if (v->bus != AUDIO_BUS_UI) g *= pauseLvl;   // №1144 (звуки меню паузы на своей шине идут, как в уровне)
  const float g0 = clampf(g, 0, 1);   // круг 7: уровень голоса до остановки — от него считается шаг затухания
  if (v->stopReq) g = 0;
  g = clampf(g, 0, 1);
  float l = pan > 0 ? 1 - pan : 1, r = pan < 0 ? 1 + pan : 1;
  *tgl = (int)(g * l * 32768.f); *tgr = (int)(g * r * 32768.f);
#if !SV_STOPFADE_OLD
  { // круг 5 (g08, P10-25): остановка голоса (гудок, сирена, шины, мотор) — затухание на несколько кадров (≈ 93 мс), а не обрыв за один буфер; новый голос начинает с целевого уровня.
    // Круг 7: в оригинале затухание ровно 1/10 с (шаг 0,1161 за буфер 11,61 мс); число 0,12 было нашим приближением
    long k = (long)(v - voices);
    if (k >= 0 && k < NVOICE) {
      if (v->stopReq) {
        int stl, str_;   // круг 7: оригинал убавляет множитель голоса от 1 до 0 за 0,1 с, то есть любой голос гаснет за те же 100 мс; шаг — доля 0,1161 от его уровня до остановки (по каналам)
        if (SV_STOPFADE7_OLD) stl = str_ = (int)(0.12f * 32768.f);
        else { stl = (int)(0.1161f * g0 * l * 32768.f); str_ = (int)(0.1161f * g0 * r * 32768.f); if (stl < 1) stl = 1; if (str_ < 1) str_ = 1; }
        *tgl = gFadeL[k] > stl ? gFadeL[k] - stl : 0; *tgr = gFadeR[k] > str_ ? gFadeR[k] - str_ : 0; }
      gFadeL[k] = *tgl; gFadeR[k] = *tgr;
    }
  }
#endif
  float p = unity ? 65536.f : d->rate * clampf(v->pitch, 0.05f, 4.f) * (65536.f / RATE);
  *tstep = (unsigned)(p + 0.5f);
  if (pauseLvl <= 0.f && v->st) *tstep = 0;   // №1144: в паузе потоки стоят на месте (после паузы продолжение с того же места)
}

static void streamControl(Stream *st, Voice *v, MixVoice *d) {
  if (st->reqGen != st->doneReq) {
    st->doneReq = st->reqGen; st->pend = st->reqTrack; st->pendStart = st->reqStart;
    float fs = st->reqFade < 0.012f ? 0.012f : st->reqFade;
    if (st->state == S_PLAY || st->state == S_WAIT || st->state == S_FADE) {
      st->state = S_FADE; st->fadeStep = FRAMES / (RATE * fs);
    } else st->state = S_FADE, st->fade = 0, st->fadeStep = 1;
  }
  if (st->state == S_FADE) {
    st->fade -= st->fadeStep;
    if (st->fade <= 0 && (v->state == V_FREE || (d->gl == 0 && d->gr == 0))) {   // silent now: restart the reader
      st->fade = 0; v->state = V_FREE; st->cur = -1;
      if (st->pend >= 0 && haveMusic) {
        st->cmdTrack = st->pend; st->cmdStart = st->pendStart; BARRIER(); st->cmdGen++; st->state = S_WAIT;
      } else st->state = S_IDLE;
    }
    if (st->fade < 0) st->fade = 0;
  }
  if (st->state == S_WAIT && st->ackGen == st->cmdGen && !st->reqArmed) {
    dspReset(d); d->si = d->end = 0; d->ch = 2; d->rate = trackRow(st->cmdTrack)->rate; d->stream = (int)(st - streams); v->st = st;
    d->gl = d->gr = 0; v->stopReq = 0; v->vol = 1; v->pan = 0; v->pitch = 1; v->trim = 1;
    { int s = (int)(st - streams); v->bus = s == AUDIO_ST_CAR_OVERLAY ? AUDIO_BUS_CAR : s == 0 ? AUDIO_BUS_MUSIC : s == 2 ? AUDIO_BUS_DIALOG : AUDIO_BUS_AMBIENCE;
      v->vol = st->vol; v->pan = st->pan; }
    int tl, tr; unsigned ts; st->fade = 1; gains(v, d, 1, &tl, &tr, &ts); d->gl = tl; d->gr = tr; d->step = ts;
    d->spatialStart=1;
    v->state = V_PLAY; st->state = S_PLAY; st->cur = st->cmdTrack;
  }
  if (st->state == S_PLAY && d->finished && d->bufLen <= 1) { v->state = V_FREE; st->state = S_IDLE; st->cur = -1; }
}

static void mixPre(MixJob *j) {
  { const float goal = pauseReq ? 0.f : 1.f, st_ = (float)FRAMES / ((float)RATE * 0.75f);   // №1144: 750 мс, как у набора «Pause» диска
    pauseLvl = pauseLvl < goal ? (goal - pauseLvl < st_ ? goal : pauseLvl + st_) : (pauseLvl - goal < st_ ? goal : pauseLvl - st_); }
  j->listener[0]=spatialQ(lx);j->listener[1]=spatialQ(ly);j->listener[2]=spatialQ(lz);
  j->right[0]=(int)(clampf(lrx,-1.f,1.f)*32767.f);j->right[1]=(int)(clampf(lrz,-1.f,1.f)*32767.f);
  for (int s = 0; s < NSTREAM; s++) streamControl(&streams[s], &voices[NVOICE + s], &j->v[NVOICE + s]);
  for (int k = 0; k < NVOICE + NSTREAM; k++) {
    Voice *v = &voices[k]; MixVoice *d = &j->v[k];
#if ME_WB_OLD
    d->on = 0;
    if (v->state == V_FREE) continue;
#else
    if (k >= NVOICE) vFl[k] |= 1;           // правка 410 К1: голос потока — заголовок всегда (streamControl его читает и пишет)
    if (v->state == V_FREE) {               // П3: «on=0» пишется и сбрасывается у голоса, который звучал; молчавший не трогаем
      if (onMem[k]) { d->on = 0; onMem[k] = 0; if (ME_WB_RED != 1) vFl[k] |= 1; }
      continue;
    }
#endif
    int tl, tr; unsigned ts;
    if (v->state == V_START) {
      const SabFile *f = v->f;
      dspReset(d); d->stream = -1; d->ch = v->ch; d->rate = v->rate; d->loop = v->loop; d->dataOff = f->off;
      d->loopStart = f->loopStart; d->loopEnd = f->loopEnd; for (int c = 0; c < 4; c++) d->lhist[c] = f->hist[c];
      d->si = 0; d->end = v->loop ? f->loopEnd : f->nsamp;
      gains(v, d, 1, &tl, &tr, &ts); d->gl = tl; d->gr = tr; d->step = ts;   // start at full level: keep attacks
      d->spatialStart=1;
      v->state = V_PLAY;
    }
    gains(v, d, v->st ? v->st->fade : 1.f, &tl, &tr, &ts);
    unsigned seq=v->pointSeq;BARRIER();MixPoint point=v->point;BARRIER();
    if(!(seq&1u) && seq==v->pointSeq)d->point=point;
    if(unity)d->point.on=0;
    d->tgl = tl; d->tgr = tr; d->tstep = ts; d->on = 1;
#if !ME_WB_OLD
    onMem[k] = 1; vFl[k] |= 1;
#endif
  }
  // кольца потоков: то, что уже прочитано с карты (с позиции чтения), и очередь кусков — копией в задание (счёт не трогает
  // Stream: его пишет поток чтения audio_io)
  for (int s = 0; s < NSTREAM; s++) {
    Stream *st = &streams[s]; MixWin *w = &j->w[s];
    w->cur = st->cur; w->len = w->used = w->nseg = w->segUsed = 0;
    if (!j->v[NVOICE + s].on || !st->ring) continue;
    unsigned sw = st->sw, wr = st->wr; BARRIER();
    unsigned n = sw - st->sr; if (n > NSEG) n = NSEG;
    for (unsigned q = 0; q < n; q++) {
      const Seg *g = &st->seg[(st->sr + q) % NSEG]; MixSeg *m = &w->seg[q];
      m->track = g->track; m->start = g->start; m->end = g->end; m->rate = trackRow(g->track) ? trackRow(g->track)->rate : 0;
      for (int c = 0; c < 4; c++) m->hist[c] = g->hist[c];
    }
    w->nseg = n;
    unsigned have = (wr - st->rd) & ~31u; if (have > MIX_SWIN) have = MIX_SWIN;
    if (meHand && have > SWIN_NEED) have = SWIN_NEED;   // №1100: больше, чем буфер может съесть, в задание не кладём
    handWinB += have; if (have > handWinMax) handWinMax = have;
    unsigned at = st->rd & st->mask, first = st->mask + 1 - at; if (first > have) first = have;
    memcpy(w->win, st->ring + at, first); if (have > first) memcpy(w->win + first, st->ring, have - first);
    w->len = have;
  }
#if !ME_WB_OLD
  wbBuild(j);
#endif
}

static int silNow;                         // правка 410: этот буфер — тишина при звучащем голосе
static void mixPost(MixJob *j, short *out) {
  for (int k = 0; k < NVOICE + NSTREAM; k++) {
    Voice *v = &voices[k]; MixVoice *d = &j->v[k];
#if ME_WB_OLD
    if (!d->on) continue;
#else
    if (!onMem[k]) continue;                // правка 410 К1: молчащие голоса не читаем (их строк нет в сбросе)
#endif
    if (v->st) { if (d->underrun && !d->finished) { underruns++; if (meHand && j->w[v->st - streams].len < SWIN_NEED && ++handMiss >= 3) { meHand = 0; } } v->vol = v->st->vol; v->pan = v->st->pan; continue; }   // №1100: окно кончилось раньше кольца три раза — полное окно
    if ((v->stopReq && d->gl == 0 && d->gr == 0) || (d->finished && d->bufLen <= 1)) { BARRIER(); v->state = V_FREE; }
  }
  for (int s = 0; s < NSTREAM; s++) {
    Stream *st = &streams[s]; const MixWin *w = &j->w[s];
    if (w->used) { BARRIER(); st->rd += w->used; }
    if (w->segUsed) { BARRIER(); st->sr += w->segUsed; }
    st->cur = w->cur; strmSi[s] = j->v[NVOICE + s].si;
  }
  seams += j->seams;
#ifdef AUDIO_HOST
  if (audioTestSeamHook)
    for (int r = 0; r < j->nrec && r < MIX_NSEAM; r++) {
      const MixSeamRec *q = &j->rec[r]; long long dd = ((long long)q->n << 16) - (long long)q->pos;
      audioTestSeamHook(framesOut + q->i + (unsigned)(dd > 0 ? dd / (long long)(q->step ? q->step : 1) : 0));
    }
#endif
  int *acc = j->acc;
  // master gain + peak limiter (the authored bus levels can sum above full scale: engine + music + ambience + hits).
  // Attack: gain drops to fit this buffer's peak within 32 frames; release ~0.3 s. Q12 gain math, no overflow.
  // Common case (no limiting, master 1): OR of |acc| bounds the peak in 3 instructions/sample, then a plain copy.
  int orv = 0;
  for (int i = 0; i < FRAMES * 2; i++) { int a = acc[i]; orv |= a ^ (a >> 31); }
  if (!orv) for (int k = 0; k < NVOICE + NSTREAM; k++) {   // правка 410: тишина при звучащем голосе (sil в AUDQ)
#if ME_WB_OLD
    const int on = j->v[k].on;
#else
    const int on = onMem[k];
#endif
    if (on && (j->v[k].tgl | j->v[k].tgr) && (j->v[k].gl | j->v[k].gr)) { silNow = 1; break; }
  }
  float m = unity ? 1.f : clampf(master, 0, 1);
  int peak = orv;
  if (orv * m > 32767.f || orv > 524287) {        // rare: exact peak, clamp against int overflow
    peak = 0;
    for (int i = 0; i < FRAMES * 2; i++) {
      int a = acc[i]; if (a > 524287) a = acc[i] = 524287; else if (a < -524287) a = acc[i] = -524287;
      if (a < 0) a = -a; if (a > peak) peak = a;
    }
  }
  float want = peak * m > 32767.f ? 32767.f / (peak * m) : 1.f;
  if (want < limGain) limGain = want; else limGain += (want - limGain) * 0.04f;
  if (limGain > 0.9995f) limGain = 1.f;
  int g1 = (int)(m * limGain * 4096.f + 0.5f), g0 = limPrev; limPrev = g1;
  if (unity) g0 = g1 = 4096;
  if (g0 == 4096 && g1 == 4096 && orv <= 32767) {
    for (int i = 0; i < FRAMES * 2; i++) out[i] = (short)acc[i];
  } else {
    int sh = g1 < g0 ? 3 : 10;                     // attack: reach the new gain within 4 frames (0.1 ms), no overshoot
    for (int i = 0; i < FRAMES * 2; i++) {
      int g = (i >> sh) ? g1 : g0 + (((g1 - g0) * i) >> sh);
      int x = (acc[i] * g) >> 12;
      out[i] = (short)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
    }
  }
  if (limGain < 0.999f) limActive++;
  framesOut += FRAMES;
}

#ifdef AUDIO_HOST
// правка 276, проверка «путь задания» на Маке (tests/me/run.py, шаг «Мак, микшер»): задание уезжает в другую память (как к ME), старое место
// затирается, счёт идёт там, итог возвращается (сборка -DMIX_JOB_COPY=1). MIX_JOB_RED=1 — красный контроль: копия не обновляется
// (ME прочёл бы старое — как без сброса кэша главного процессора)
#ifndef MIX_JOB_COPY
#define MIX_JOB_COPY 0
#endif
static int jobCopy = MIX_JOB_COPY;
#ifndef MIX_JOB_RED
#define MIX_JOB_RED 0
#endif
// правка 410 К1, -DMIX_JOB_COPY=3 — модель несогласованного кэша: K — память, которую видит ME. Отдача: в K попадают только сброшенные
// участки (wbR; wbAll — всё). Ответ: у главного свежими становятся только сброшенные участки (кэш над ними пуст), остальное — как было у
// главного (по инварианту он их не трогает). Счётчики (stderr при выходе, «WBSTAT»): строк на буфер, «on» у ME ≠ у главного после
// отдачи (П3), байт, которые главный поменял мимо сброса (их строка осталась бы грязной в кэше и затёрла бы ответ ME)
static unsigned long long wsBuf, wsLines, wsFull, wsOnBad, wsStray;
static void wsReport(void) {
  fprintf(stderr, "WBSTAT buffers=%llu lines_avg=%.1f full_lines=%u full=%llu onbad=%llu stray=%llu\n", wsBuf,
          wsBuf ? (double)wsLines / (double)wsBuf : 0.0, (unsigned)(sizeof(MixJob) / 64), wsFull, wsOnBad, wsStray);
}
static void mixRunModel(MixJob *j, MixJob *K) {
  static MixJob *S; static int primed;
  if (!S) { if (!(S = amalloc(sizeof *S))) { mixDspRun(j); return; } atexit(wsReport); }
  unsigned char *pj = (unsigned char *)j, *pk = (unsigned char *)K, *ps = (unsigned char *)S;
  int full = !primed || wbAll || wbOld || ME_WB_OLD; primed = 1;
  if (full) { memcpy(K, j, sizeof *K); wsLines += sizeof(MixJob) / 64; wsFull++; }
  else {
    unsigned at = 0;
    for (int r = 0; r <= wbN; r++) {                                    // мимо сброса главный ничего не менял
      unsigned e = r < wbN ? wbR[r].a : (unsigned)sizeof(MixJob);
      for (unsigned i = at; i < e; i++) wsStray += pj[i] != ps[i];
      if (r < wbN) { memcpy(pk + wbR[r].a, pj + wbR[r].a, wbR[r].b - wbR[r].a); wsLines += (wbR[r].b - wbR[r].a) / 64; at = wbR[r].b; }
    }
  }
  for (int k = 0; k < MIX_NV; k++) wsOnBad += K->v[k].on != j->v[k].on;
  wsBuf++; wbAll = 0;
  K->data = DATA; mixDspRun(K);
  if (full) memcpy(j, K, sizeof *j);
  else {
    for (int r = 0; r < wbN; r++) memcpy(pj + wbR[r].a, pk + wbR[r].a, wbR[r].b - wbR[r].a);
    memcpy(j->rec, K->rec, sizeof j->rec);                              // стыки читает только Мак (журнал стыков), не главный PSP
  }
  memcpy(S, j, sizeof *S);
}
static void mixRunCopy(MixJob *j) {
  static MixJob *K; if (!K && !(K = amalloc(sizeof *K))) { mixDspRun(j); return; }
  if (jobCopy == 3) { mixRunModel(j, K); return; }
  if (jobCopy == 2) {   // правка 313, схема ME_MEM_KERN: в «ядро» и обратно — только mixJobIn/mixJobOut (звучащие голоса, окна, acc)
    static int init; if (!init) { memset(K, 0xA5, sizeof *K); init = 1; }   // остальное в «ядре» — мусор, как у свежей памяти
    mixJobIn(K, j);
    for (int k = 0; k < MIX_NV; k++) if (j->v[k].on) { memset(&j->v[k], 0x5A, sizeof j->v[k]); j->v[k].on = 1; }   // у главного —
    memset(j->acc, 0x5A, sizeof j->acc);                                                                              // затёрто: итог
    unsigned used[MIX_NSTREAM]; for (int s = 0; s < MIX_NSTREAM; s++) used[s] = j->w[s].used;                       // обязан прийти
    K->data = DATA; mixDspRun(K); mixJobOut(j, K);                                                                    // из «ядра»
    if (MIX_JOB_RED) for (int s = 0; s < MIX_NSTREAM; s++) j->w[s].used = used[s];   // красный: съеденное из колец не вернулось
    return;
  }
  static int primed; if (!MIX_JOB_RED || !primed) { memcpy(K, j, sizeof *K); primed = 1; }
  memset(j, 0xA5, sizeof *j); K->data = DATA;
  mixDspRun(K);
  memcpy(j, K, sizeof *j);
}
#endif

static unsigned dspAcc, dspCnt; static float dspLast;   // правка 313: время самого счёта (mixDspRun) на главном, мкс на буфер
// правка 313 (замер, «flag meprof»): во что обходится перенос задания в ядро и обратно (схема ME_MEM_KERN: mixJobIn + mixJobOut) —
// раз в секунду на настоящем задании в запасную память, мкс
static volatile int profKern; static float kcopyLast; static MixJob *kcopyK; static unsigned kcopyN;
void audioProfKern(int on) { profKern = on; }
float audioKernCopyUs(void) { return kcopyLast; }
static void kcopyProbe(MixJob *j) {
  if (!profKern || (++kcopyN % 86u)) return;
  if (!kcopyK && !(kcopyK = amalloc(sizeof *kcopyK))) return;
  MixJob *t = amalloc(sizeof *t); if (!t) return;
  memcpy(t, j, sizeof *t);
  unsigned t0 = nowUs(); mixJobIn(kcopyK, j); mixJobOut(t, kcopyK); kcopyLast = (float)(nowUs() - t0);
  free(t);
}
static void mixBuffer(short *out) {
  mixPre(J); J->data = DATA;
  kcopyProbe(J);
  unsigned t0 = nowUs();
#ifdef AUDIO_HOST
  if (jobCopy) mixRunCopy(J); else
#endif
  mixDspRun(J);
  dspAcc += nowUs() - t0; dspCnt++;
#ifdef AUDIO_HOST
  if (jobCopy != 3)
#endif
  wbAll = 1;   // правка 410 К1: главный сам посчитал задание — все его строки у главного в кэше
  mixPost(J, out);
}
// правка 313 (замер для ME, main.c «flag meprof»): средний счёт смешивания и весь буфер на главном процессоре за прошлую секунду, мкс
void audioDspUs(float *dsp, float *buf) { *dsp = dspLast; *buf = statUs; }

// ---------------------------------------------------------------- stream reader (background thread)
static void pushSeg(Stream *st, int t, unsigned start, unsigned end, const short *hist) {
  Seg *s = &st->seg[st->sw % NSEG];
  s->track = t; s->start = start; s->end = end;
  for (int c = 0; c < 4; c++) s->hist[c] = hist ? hist[c] : 0;
  BARRIER(); st->sw++;
  if (t >= 0) { st->pTrack = t; st->pBlk = start / 28; st->pEnd = (end + 27) / 28; st->pActive = 1; }
  else st->pActive = 0;
}

static void streamFill(Stream *st) {
  while (st->pActive) {
    if (st->pBlk >= st->pEnd) {                     // this pass is fully buffered: decide what follows
      if (st->sw - st->sr >= NSEG) return;
      int q = st->queued; const SapTrack *tr = trackRow(st->pTrack);
      if (trackRow(q)) { st->queued = -1; pushSeg(st, q, 0, trackEnd(q), 0); }
      else if (!st->once && (tr->flags & 1)) pushSeg(st, st->pTrack, tr->loopStart, tr->loopEnd, tr->hist);
      else if (!st->once && tr->next >= 0 && tr->next < ntrack) pushSeg(st, tr->next, 0, trackEnd(tr->next), 0);
      else pushSeg(st, -1, 0, 0, 0);
      continue;
    }
    unsigned used = st->wr - st->rd, space = st->mask + 1 - used, w = st->wr & st->mask;
    if (space < 4096) return;
    unsigned bytes = (st->pEnd - st->pBlk) * 32;
    if (bytes > space) bytes = space;
    if (bytes > st->mask + 1 - w) bytes = st->mask + 1 - w;
    if (bytes > CHUNK) bytes = CHUNK;
    bytes &= ~31u;
    if (!bytes) return;
    unsigned t0 = nowUs();
    const AFile fdr = st->pTrack >= EXTRA_TRACK_BASE ? emfd : (trackRow(st->pTrack)->flags & TRK_LANG) ? lfd : mfd;   // №1111
    int got = afRead(fdr, trackRow(st->pTrack)->off + st->pBlk * 32, st->ring + w, (int)bytes);
    int ms = (int)((nowUs() - t0) / 1000); if (ms > ioMaxMs) ioMaxMs = ms;
    if (got != (int)bytes) {
#if !RESUME_AUDIO_OLD
      if (!resumeAudioOld && (pwrSleeping || pwrResumeReq || got < 0 || !AF_OK(fdr))) {
        if (got < 0 || !AF_OK(fdr)) {
          pwrResumeReq = 1;
          pwrResumeIo = 1;
        }
        return;
      }
#endif
      pushSeg(st, -1, 0, 0, 0); return;
    }
    ioBytes += bytes;
    BARRIER(); st->wr += bytes; st->pBlk += bytes / 32;
  }
}

static void checkPwrResumeIo(void) {
#if !RESUME_AUDIO_OLD
  if (!resumeAudioOld && pwrResumeIo) {
    pwrResumeIo = 0;
    if (musicPath[0]) {
      if (AF_OK(mfd)) afClose(mfd);
      mfd = AF_NONE;
      for (int r = 0; r < 50 && !quit && !pwrSleeping; r++) {
        mfd = afOpen(musicPath);
        if (AF_OK(mfd)) break;
#ifndef AUDIO_HOST
        sceKernelDelayThread(40000);
#endif
      }
    }
    if (langPath[0]) {   // №1111: файл озвучки языка
      if (AF_OK(lfd)) afClose(lfd);
      lfd = AF_NONE;
      for (int r = 0; r < 50 && !quit && !pwrSleeping; r++) {
        lfd = afOpen(langPath);
        if (AF_OK(lfd)) break;
#ifndef AUDIO_HOST
        sceKernelDelayThread(40000);
#endif
      }
    }
    if (extraPath[0]) {
      if (AF_OK(emfd)) afClose(emfd);
      emfd = AF_NONE;
      for (int r = 0; r < 50 && !quit && !pwrSleeping; r++) {
        emfd = afOpen(extraPath);
        if (AF_OK(emfd)) break;
#ifndef AUDIO_HOST
        sceKernelDelayThread(40000);
#endif
      }
    }
    pwrResumeReq = 0;
  }
#endif
}

static void streamService(Stream *st) {
  checkPwrResumeIo();
#if !RESUME_AUDIO_OLD
  if (!resumeAudioOld && pwrSleeping) return;
#endif
  if (st->cmdGen != st->pGen) {        // the mixer is waiting and not reading: safe to reset
    st->pGen = st->cmdGen;
    st->wr = st->rd = 0; st->sw = st->sr = 0; st->pActive = 0;
    if (trackRow(st->cmdTrack)) {
      unsigned end_ = st->once ? trackRow(st->cmdTrack)->nsamp : trackEnd(st->cmdTrack), from_ = st->cmdStart / 28u * 28u;   // №789: старт с блока (28 сэмплов)
      pushSeg(st, st->cmdTrack, from_ + 28u < end_ ? from_ : 0u, end_, 0); }
    streamFill(st);
    BARRIER(); st->ackGen = st->pGen;
    return;
  }
  if (st->ackGen == st->pGen) streamFill(st);
}

#ifdef AUDIO_HOST
static int mePwrCb(int unk, int flags, void *arg) {
  (void)unk; (void)arg;
  if (flags & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_STANDBY)) {
    pwrSleeping = 1;
  }
#if !RESUME_AUDIO_OLD
  else if (!resumeAudioOld && (flags & (PSP_POWER_CB_RESUME_COMPLETE | PSP_POWER_CB_RESUMING))) {
    pwrResumeReq = 1;
    pwrResumeIo = 1;
    pwrResumeMix = 1;
    pwrSleeping = 0;
  }
#endif
  return 0;
}
#endif

// ---------------------------------------------------------------- threads (PSP)
#ifndef AUDIO_HOST
static SceUID mixTh = -1, ioTh = -1, ioSema = -1; static int chan = -1;
static short outBuf[2][FRAMES * 2] __attribute__((aligned(64)));
static short *capBuf; static unsigned capFrames, capMax;   // audioCapture: copy of what goes to sceAudio
static volatile int outVol = PSP_AUDIO_VOLUME_MAX;             // hardware channel volume (audioOutputVolume)
// ---------------------------------------------------------------- правка 276: смешивание на Media Engine (флаг-файл meaudio)
// Служба ME (viewer/me/meprobe.c meSvc*, модуль sharme.prx) держит нашу задачу в ячейке ME. Буфер k: главный готовит задание
// (mixPre), сбрасывает свой кэш над ним и отдаёт его службе (клиент 0); пока звучит буфер k-1 (sceAudioOutputPannedBlocking,
// 11,6 мс), ME считает k; ответ забираем в следующем проходе (mixPost — ограничитель и выход). ME занят частицами (правка 313) —
// этот буфер считает главный (то же задание, та же функция). Нет ответа 50 мс — ME больше не трогаем: счёт переезжает в копию
// задания на главном; в ящике поле «me=off:timeout» (main.c bbMus). Схема памяти (ящик и задание в ядре или в памяти игры,
// правка 313) — mebox.h ME_MEM. Ролик (sceJpeg — тоже ME) и сон PSP — ME звуку не отдаём (audioMeHold, обратный вызов питания)
#include <psppower.h>
#include "me/mebox.h"
int meSvcScheme(void); int meSvcSubmit(int c, volatile MeBox *ubox, unsigned kind, void *job, unsigned words);
int meSvcPoll(int c, volatile MeBox *ubox, unsigned seq, unsigned *sum); unsigned meSvcQuiesce(void); void meSvcHold(int on);
static MixJob *meSpare;                   // запасное задание: ME не ответил — счёт переезжает сюда (память взята заранее, главным потоком)
static MeBox *meBox;                      // ящик звука в памяти игры (схема ME_MEM_USER); служба пишет и читает его мимо кэша
static volatile int meState;              // 0 — нет службы, 1 — звук на ME, 2 — выключен до конца запуска
static volatile int meHoldReq, meHoldAck, meSleep;
static unsigned meSeq, meSecJobs, meSecWait, meJobsLast, meWaitLast, meBusy;   // за секунду: заданий, наибольшее ожидание (мкс); ME занят
static char meWhy[40] = "-";
static int mePwrCb(int unk, int flags, void *arg) {
  (void)unk; (void)arg;
  if (flags & (PSP_POWER_CB_POWER_SWITCH | PSP_POWER_CB_SUSPENDING | PSP_POWER_CB_STANDBY)) {
    pwrSleeping = 1;
    meSleep = 1;                                                     // сон: дальше звук только на главном
    for (int i = 0; i < 25 && meState == 1 && mixTh >= 0; i++) sceKernelDelayThread(2000);   // поток звука отпустит ME
  }
#if !RESUME_AUDIO_OLD
  else if (!resumeAudioOld && (flags & (PSP_POWER_CB_RESUME_COMPLETE | PSP_POWER_CB_RESUMING))) {
    pwrResumeReq = 1;
    pwrResumeIo = 1;
    pwrResumeMix = 1;
    pwrSleeping = 0;
    meSleep = 0;
    if (ioSema >= 0) sceKernelSignalSema(ioSema, 1);
  }
#endif
  return 0;
}
static SceUID pwrTh = -1;
static int mePwrThread(SceSize a, void *b) {
  (void)a; (void)b;
  int cb = sceKernelCreateCallback("me_pwr", mePwrCb, NULL);
  if (cb >= 0) scePowerRegisterCallback(-1, cb);
  sceKernelSleepThreadCB();
  return 0;
}
static void audioPwrInit(void) {
  if (pwrTh >= 0) return;
  pwrTh = sceKernelCreateThread("audio_pwr", mePwrThread, 0x11, 0x1000, PSP_THREAD_ATTR_USER, NULL);
  if (pwrTh >= 0) sceKernelStartThread(pwrTh, 0, NULL);
}
unsigned audioMeJobAddr(void) { return (unsigned)J; }        // правка 313: где лежат задание и банк (схема «только нижняя память»)
unsigned audioMeDataAddr(void) { return (unsigned)DATA; }
int audioMeAttach(void) {
  meBox = memalign(64, 64); if (!meBox) return -1;
  memset(meBox, 0, 64); sceKernelDcacheWritebackInvalidateRange(meBox, 64);
  meSpare = memalign(64, sizeof *meSpare); if (!meSpare) return -1;
  sceKernelDcacheWritebackAll();                                     // банк звуков — в память (ME читает его мимо своего кэша или со сбросом)
  audioPwrInit();
  meState = 1;
  return 0;
}
#ifndef MEHOLD1192_OLD
#define MEHOLD1192_OLD 0   // 1 — ожидание ack не дольше 100 мс и молча (как в TEST89)
#endif
static unsigned meHoldMiss;   // сколько раз ролик пошёл дальше, а ME звуком так и не отпустил (ack не пришёл)
// №1192 (кандидат 2, разбор arch-fable): ролик на sceJpeg тоже работает на ME. Раньше ждали ack всего 100 мс и шли дальше молча — если ME ещё считал буфер звука, ролик и наше задание
// пересекались на ME. Теперь ждём до 1 с (в норме ack приходит за 2–10 мс, ожидание не удлиняется), а промах — строка «ME HOLD miss» в ящик и счётчик hm в AUDV: по журналу видно, что ME не отпустил
void audioMeHold(int on) {                // ролики: on=1 — дождаться, что ME звуком не занят (до 1 с)
  meSvcHold(on);
  if (!on) { meHoldReq = 0; meHoldAck = 0; return; }
  meHoldReq = 1;
  int lim = MEHOLD1192_OLD ? 50 : 500, i = 0;
  for (; i < lim && meState == 1 && mixTh >= 0 && !meHoldAck; i++) sceKernelDelayThread(2000);
  if (!MEHOLD1192_OLD && i >= lim && meState == 1 && mixTh >= 0 && !meHoldAck) {
    meHoldMiss++; relNote("ME HOLD miss: waited_ms=%d state=%d seq=%u miss=%u (ролик идёт, ME звуком не отпустил)", i * 2, meState, meSeq, meHoldMiss);
  }
}
int audioMeState(char *out, int n) {      // для строки AUDV и ящика (main.c bbMus): «on86/0.4» — заданий за секунду / ожидание, мс
  extern unsigned meSvcChained;          // очередь (облако 05.10): заданий звука, ушедших на ME следом за частицами — «/c<число>» в конце
  if (meState == 1) return snprintf(out, n, "%s%u/%.1f%s/c%u%s%.0u", meHoldReq ? "hold" : "on", meJobsLast, meWaitLast / 1000.f, meBusy ? "b" : "", meSvcChained, meHoldMiss ? "/hm" : "", meHoldMiss);
  return snprintf(out, n, "%s", meState == 2 ? meWhy : "-");
}
// правка 410 (шаг 0 плана ME): качество звука и цена отправки за секунду — строка AUDQ в smoke.txt (main.c), разбор tools/tour_report.py.
//   late — буфер ушёл в sceAudio, когда канал уже доиграл всё (sceAudioGetChannelRestLen = 0): дыра в звуке, это и есть треск;
//   rest_min — наименьший запас канала перед выводом, сэмплов; sil — буфер из одних нулей, хотя звучит голос с ненулевой громкостью
//   (подозрение на пустой ответ; может быть и тишина в самой записи); повтор буфера в этом коде невозможен: каждый проход пишет outBuf
//   заново (mixPost) до вывода; su — голос потока без данных (кольцо пусто);
//   wmax — худший буфер (мкс, по часам); cpu — буферов на главном при включённом ME (занят, тайм-аут); дальше разбивка отдачи на ME
//   (К1): wb — сброс кэша, svc — meSvcSubmit (kuKernelCall), col — ожидание ответа, pre/post — mixPre/mixPost, мкс на буфер; wbl — строк
//   кэша в сбросе на буфер (целиком 684), wbr — вызовов сброса на буфер, full — сбросов целиком за секунду
typedef struct { unsigned late, sil, su, cpu, kick, full, wbUs, svcUs, colUs, preUs, postUs, wbl, wbr, buf; int restMin; } AudQ;
static AudQ aq, aqLast; static unsigned aqLateTot, aqSilTot, aqBufTot, aqLateEvery; static unsigned aqHandWin;
int audioQualityLine(char *out, int n) {  // правка 410: строка AUDQ за прошлую секунду (main.c, tools/tour_report.py)
  const AudQ *q = &aqLast; float k = q->kick ? (float)q->kick : 1.f, b = q->buf ? (float)q->buf : 1.f;
  return snprintf(out, n, "late=%u/%u sil=%u/%u rest_min=%d su=%u wmax_us=%.0f buf=%u cpu=%u kick=%u wb_us=%.0f svc_us=%.0f col_us=%.0f "
                  "pre_us=%.0f post_us=%.0f wbl=%.0f wbr=%.1f full=%u wbold=%d",
                  q->late, aqLateTot, q->sil, aqSilTot, q->restMin, q->su, statUsMax, q->buf, q->cpu, q->kick, q->wbUs / k, q->svcUs / k,
                  q->colUs / k, q->preUs / k, q->postUs / k, q->wbl / k, q->wbr / k, q->full, wbOld || ME_WB_OLD);
  (void)b;
}
// №1100: строка AUDH раз в секунду (main_loop.h рядом с AUDQ): главный на рукопожатие с ME за буфер, мкс (сброс кэша wb + отдача svc + mixPre + mixPost), буферов на главном
// из-за отказа/занятости ME (cpu), байт окон потоков за буфер (win, наибольшее winmax), счётчик нехватки окна (miss), включён ли режим (mehand). Для сравнения «mehand 0/1» в туре
int audioHandLine(char *out, int n) {
  const AudQ *q = &aqLast; float k = q->kick ? (float)q->kick : 1.f, b = q->buf ? (float)q->buf : 1.f;
  static unsigned wb0, wm0; (void)wb0; (void)wm0;
  return snprintf(out, n, "mehand=%d hand_us=%.0f (wb %.0f svc %.0f pre %.0f post %.0f) col_us=%.0f cpu=%u/%u kick=%u win_b=%.0f miss=%u me=%s", meHand,
                  (q->wbUs + q->svcUs + q->preUs + q->postUs) / k, q->wbUs / k, q->svcUs / k, q->preUs / k, q->postUs / k, q->colUs / k, q->cpu, q->buf, q->kick,
                  (float)aqHandWin / b, handMiss, meState == 1 ? "on" : meState == 2 ? meWhy : "-");
}
// ME-доля, вариант «адаптивный помощник» (слово Роба 492, 10.10: «основной процессор тянет всё сам; ME подключается только в тяжёлые моменты»).
// Флаг-файл mepeak включает ворота mePeakGate: служба ME поднята (meaudio/mefx), но звук и частицы идут на ME только пока mePeak == 1. Включение —
// прошлый кадр дольше ME_PEAK_WORK_MS, или живых частиц больше ME_PEAK_FX, или погоня Hit & Run; выключение — через ME_PEAK_CALM_S спокойствия
// (гистерезис). ME_PEAK_OLD=1 — ворота открыты всегда (как прежние туры). Строки MEPEAK в smoke.txt: включился/выключился, причина, кадр до и средний за пик
#ifndef ME_PEAK_OLD
#define ME_PEAK_OLD 0
#endif
// Пороги (поправка 10.10 по SHARTEST5): прежний порог 13 мс стоял ниже обычного кадра (20-34 мс), ворота открылись на загрузке и больше не закрывались.
// Теперь работа кадра сглаживается (EMA, около 0,3 с, кадры дольше ME_PEAK_SKIP_MS — загрузка, посадка — не в счёт); включение при сглаженной работе
// выше ME_PEAK_ON_MS (предел 30 к/с = 33,3 мс, запас 5 мс), живых частиц больше ME_PEAK_FX (взрыв ~390) или погоне; выключение, когда сглаженная работа
// ниже ME_PEAK_OFF_MS и событий нет ME_PEAK_CALM_S подряд
#define ME_PEAK_ON_MS 28.0f
#define ME_PEAK_OFF_MS 25.0f
#define ME_PEAK_SKIP_MS 80.0f
#define ME_PEAK_FX 200
#define ME_PEAK_CALM_S 1.5f
int mePeakGate, mePeak;
void mePeakStep(float dt, float workMs, int fxLive, int chase, char *log, int logN) {   // раз в кадр после замера; log — строка для smoke.txt или пусто
  static float calm, onT, sumWork, ema; static unsigned nOn; static unsigned nWork; static float workBefore; static char why[24];
  if (log) log[0] = 0;
  if (!mePeakGate || ME_PEAK_OLD) { mePeak = 1; return; }
  if (workMs < ME_PEAK_SKIP_MS) ema = ema > 0.f ? ema + (workMs - ema) * 0.1f : workMs;
  int trig = ema > ME_PEAK_ON_MS || fxLive > ME_PEAK_FX || chase;
  if (mePeak) { onT += dt; sumWork += workMs; nWork++; }
  if (trig) {
    calm = 0.f;
    if (!mePeak) {
      mePeak = 1; onT = 0.f; sumWork = 0.f; nWork = 0; workBefore = ema; nOn++;
      snprintf(why, sizeof why, "%s%s%s", ema > ME_PEAK_ON_MS ? "work " : "", fxLive > ME_PEAK_FX ? "fx " : "", chase ? "chase" : "");
      if (log) snprintf(log, logN, "MEPEAK on n=%u why=%s ema=%.2f work=%.2f fx=%d chase=%d\n", nOn, why, ema, workMs, fxLive, chase);
    }
  } else if (mePeak && ema < ME_PEAK_OFF_MS && (calm += dt) >= ME_PEAK_CALM_S) {
    mePeak = 0;
    if (log) snprintf(log, logN, "MEPEAK off n=%u dur=%.1f work_avg=%.2f ema_before=%.2f\n", nOn, onT, nWork ? sumWork / nWork : 0.f, workBefore);
  } else if (mePeak && ema >= ME_PEAK_OFF_MS) calm = 0.f;
}
static unsigned meLastKickUs;             // №653: когда ушло последнее задание звука на ME (окно города)
static int meKick(void) {                 // 1 — задание ушло на ME; 0 — ME занят (буфер считает главный)
  if (mePeakGate && !mePeak && !ME_PEAK_OLD) { J->data = DATA; return 0; }   // вне пика всё считает главный процессор (ворота mepeak)
  J->data = (meSvcScheme() & 3) == ME_MEM_USER ? (const unsigned char *)(0x40000000u | (unsigned)DATA) : DATA;   // схема 0: банк мимо кэша ME
  unsigned t0 = nowUs();
#if !ME_WB_OLD
  if (!wbOld && !wbAll) {                 // правка 410 К1: только тронутые строки (wbBuild в mixPre)
    for (int r = 0; r < wbN; r++) sceKernelDcacheWritebackInvalidateRange((char *)J + wbR[r].a, wbR[r].b - wbR[r].a);
    for (int r = 0; r < wbN; r++) aq.wbl += (wbR[r].b - wbR[r].a) / 64;
    aq.wbr += (unsigned)wbN;
  } else
#endif
  { sceKernelDcacheWritebackInvalidateRange(J, sizeof *J); aq.wbl += sizeof *J / 64; aq.wbr++; aq.full++; }   // задание — в память, в кэше главного его строк нет
  wbAll = 0;
  unsigned t1 = nowUs();
  int q = meSvcSubmit(0, meBox, ME_KIND_MIX, J, sizeof *J);
  aq.wbUs += t1 - t0; aq.svcUs += nowUs() - t1; aq.kick++;
  if (q < 0) { J->data = DATA; meBusy++; return 0; }
  meSeq = (unsigned)q; meLastKickUs = t0; return 1;
}
// №653: окно для задания города (viewer/main_render.h ccBegin): ME не занят звуком и до следующего буфера звука хватает времени на задание + запас 1,5 мс.
// Буфер звука — раз в FRAMES/44100 = 11,61 мс; звук, пришедший за городом, всё равно цепляется следом (mebox.h), но ждать его не должен дольше запаса
int audMeCityWindow(unsigned needUs) {
  if (meState != 1 || !meBox) return 1;                  // звук не на ME: окно открыто
  volatile MeBox *u = (volatile MeBox *)(0x40000000u | (unsigned)meBox);
  if (u->sent != u->done) return 0;                      // задание звука у ME сейчас
  unsigned since = nowUs() - meLastKickUs;
  return since + needUs + 1500u < 11610u;
}
static int meCollect(unsigned *sum) {     // 1 — ответ есть; 0 — нет за 50 мс
  unsigned t0 = nowUs(), w = 0;
  while (!meSvcPoll(0, meBox, meSeq, sum)) {
    w = nowUs() - t0;
    if (w >= 50000u) return 0;
    sceKernelDelayThread(w < 2000u ? 100 : 500);
  }
  meSecJobs++; if (w > meSecWait) meSecWait = w;
  aq.colUs += nowUs() - t0;               // правка 410: ожидание ответа вместе с опросом
  return 1;
}
static void meOff(const char *why, unsigned job) {   // ME больше не трогаем; счёт — копия задания на главном
  meState = 2; snprintf(meWhy, sizeof meWhy, "off:%s@%u", why, job);
  if (meSpare && J != meSpare) { memcpy(meSpare, J, sizeof *meSpare); J = meSpare; }   // старое задание не трогаем: ME мог бы ещё писать в него
  J->data = DATA;
}
static int mixThread(SceSize a, void *b) {
  int cur = 0, pend = 0, prepped = 0; unsigned busy = 0, win0 = nowUs(), cnt = 0, worst = 0; fpuThreadInit();   // правка 252
  aqBufTot = 0; memset(&aq, 0, sizeof aq); aq.restMin = -1;   // правка 410: новый поток (смена уровня) — прогрев 2 буфера
  while (!quit) {
    fpuThreadPoll(FPU_T_MIX);
#if !RESUME_AUDIO_OLD
    if (!resumeAudioOld && pwrSleeping) {
      sceKernelDelayThread(20000);
      continue;
    }
    if (!resumeAudioOld && pwrResumeMix) {
      pwrResumeMix = 0;
      if (chan >= 0) {
        sceAudioChRelease(chan);
        chan = -1;
      }
      for (int i = 0; i < 20 && !quit; i++) {
        chan = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, FRAMES, PSP_AUDIO_FORMAT_STEREO);
        if (chan >= 0) break;
        sceKernelDelayThread(10000);
      }
      aqBufTot = 0; memset(&aq, 0, sizeof aq); aq.restMin = -1;
    }
#endif
    unsigned t0 = nowUs();
    if (pend) {                           // буфер, который ME посчитал, пока звучал прошлый
      pend = 0; unsigned sum = 0;
      int got = meCollect(&sum);
      if (got && sum != ME_MIX_BADSIZE) { J->data = DATA; unsigned p0 = nowUs(); mixPost(J, outBuf[cur]); aq.postUs += nowUs() - p0; }
      else {                              // не ответил (или чужая сборка модуля): этот буфер — на главном из того, что есть
        meOff(got ? "layout" : "timeout", meSeq);
        mixDspRun(J); wbAll = 1; aq.cpu++; mixPost(J, outBuf[cur]);
      }
    } else if (prepped) {                 // правка 313: задание готово, но ME был занят частицами — счёт здесь (как mixBuffer без mixPre)
      prepped = 0; J->data = DATA;
      unsigned d0 = nowUs(); mixDspRun(J); dspAcc += nowUs() - d0; dspCnt++;
      wbAll = 1; aq.cpu++;                // правка 410 К1: задание целиком в кэше главного — следующий сброс целиком
      mixPost(J, outBuf[cur]);
    } else mixBuffer(outBuf[cur]);
    if (meSleep && meState == 1) { meSvcQuiesce(); snprintf(meWhy, sizeof meWhy, "off:sleep@%u", meSeq); meState = 2; }   // сон: ME больше не трогаем
    if (meState == 1 && !meHoldReq && !quit) {   // следующий буфер — на ME
      unsigned p0 = nowUs(); mixPre(J); aq.preUs += nowUs() - p0;
      if (meKick()) pend = 1; else prepped = 1;
    }
    else if (meState >= 1 && meHoldReq && !meHoldAck) { meSvcQuiesce(); meHoldAck = 1; }
    unsigned d = nowUs() - t0; busy += d; cnt++; if (d > worst) worst = d;
    if (capBuf && capFrames + FRAMES <= capMax) { memcpy(capBuf + capFrames * 2, outBuf[cur], sizeof outBuf[cur]); capFrames += FRAMES; }
    sceKernelDcacheWritebackRange(outBuf[cur], sizeof outBuf[cur]);
    if (aqLateEvery && aqBufTot && !(aqBufTot % aqLateEvery)) sceKernelDelayThread(40000);   // красный контроль счётчика (файл audlate)
    if (chan >= 0) {
      int rest = sceAudioGetChannelRestLen(chan);   // правка 410: канал уже доиграл всё — этот буфер опоздал (дыра в звуке)
      if (aqBufTot >= 2 && rest == 0) { aq.late++; aqLateTot++; }
      if (aqBufTot >= 2 && rest >= 0 && (aq.restMin < 0 || rest < aq.restMin)) aq.restMin = rest;
      if (silNow) { silNow = 0; aq.sil++; aqSilTot++; }
      aq.buf++; aqBufTot++;
      sceAudioOutputPannedBlocking(chan, outVol, outVol, outBuf[cur]);
    } else {
      sceKernelDelayThread(11600);
    }
    cur ^= 1;
    int wait = 0; for (int s = 0; s < NSTREAM; s++) wait |= streams[s].state == S_WAIT;
    if (ioSema >= 0 && (wait || (cnt & 7) == 0)) sceKernelSignalSema(ioSema, 1);
    unsigned now = nowUs();
    if (now - win0 >= 1000000) {
      statCpu = busy * 100.f / (now - win0); statUs = cnt ? (float)busy / cnt : 0; statUsMax = worst;
      busy = 0; cnt = 0; worst = 0; win0 = now;
      meJobsLast = meSecJobs; meWaitLast = meSecWait; meSecJobs = meSecWait = 0;
      dspLast = dspCnt ? (float)dspAcc / dspCnt : 0; dspAcc = dspCnt = 0;
      { static int su0; int su = underruns; aq.su = (unsigned)(su - su0); su0 = su; }
      aqHandWin = handWinB; handWinB = 0;   // №1100
      aqLast = aq; memset(&aq, 0, sizeof aq); aq.restMin = -1;   // правка 410: секунда AUDQ
    }
  }
  if (pend) { unsigned sum; if (!meCollect(&sum)) meOff("timeout", meSeq); }   // выход (смена уровня): задание забрать, иначе память задания не наша
  if (meState == 1) meSvcQuiesce();
  return 0;
}
static int ioThread(SceSize a, void *b) {
  fpuThreadInit();   // правка 252
  while (!quit) {
    fpuThreadPoll(FPU_T_AIO);
    SceUInt to = 20000;
    sceKernelWaitSema(ioSema, 1, &to);
#if !RESUME_AUDIO_OLD
    if (!resumeAudioOld && pwrSleeping) {
      sceKernelDelayThread(20000);
      continue;
    }
#endif
    for (int s = 0; s < NSTREAM; s++) streamService(&streams[s]);
  }
  return 0;
}
void audioOutputVolume(float v) { outVol = (int)(clampf(v, 0, 1) * PSP_AUDIO_VOLUME_MAX); }
void audioCapture(short *buf, unsigned maxFrames) { capBuf = 0; BARRIER(); capFrames = 0; capMax = maxFrames; BARRIER(); capBuf = buf; }
unsigned audioCaptured(void) { return capFrames; }
#else
void audioHostMix(short *out, int frames) {
  for (int k = 0; k + FRAMES <= frames; k += FRAMES) {
    for (int s = 0; s < NSTREAM; s++) streamService(&streams[s]);
    unsigned t0 = nowUs();
    mixBuffer(out + k * 2);
    statUs = (float)(nowUs() - t0);
    for (int s = 0; s < NSTREAM; s++) streamService(&streams[s]);
  }
}
void audioTestForceUnity(int on) { unity = on; }
int audioTestFileCount(void) { return H ? (int)H->nfile : 0; }
int audioTestPlayFile(int fi) {
  if (!H || fi < 0 || fi >= (int)H->nfile) return 0;
  Voice *v = &voices[0];
  memset(v, 0, sizeof *v);
  v->f = &F[fi]; v->ch = F[fi].ch; v->rate = F[fi].rate; v->loop = 0;
  v->vol = 1; v->pan = 0; v->pitch = 1; v->trim = 1; v->bus = AUDIO_BUS_SFX; v->gen++;
  BARRIER(); v->state = V_START;
  return 1;
}
void audioTestMusicOnce(int track) {
#if ME_ONCE_OLD
  streams[0].once = track >= 0; if (track >= 0) audioMusic(track, 0);
#else
  if (track >= 0) audioMusicOnce(track, 0); else audioMusic(-1, 0);
#endif
}
int audioTestFileInfo(int fi, int *ch, int *rate, unsigned *nsamp) {
  if (!H || fi < 0 || fi >= (int)H->nfile) return 0;
  *ch = F[fi].ch; *rate = F[fi].rate; *nsamp = F[fi].nsamp; return 1;
}
int audioTestResLoops(int id) { return H && id >= 0 && id < (int)H->nres ? (int)(R[id].flags & 1) : 0; }
int audioTestTrackInfo(int t, int *loop, int *next, int *bus, unsigned *nsamp, unsigned *loopStart, unsigned *loopEnd) {
  const SapTrack *r=trackRow(t);if(!r)return 0;
  *loop = r->flags & 1; *next = r->next; *bus = r->bus; *nsamp = r->nsamp;
  *loopStart = r->loopStart; *loopEnd = r->loopEnd; return 1;
}
int audioTestSeamCount(void) { return seams; }
void audioTestSeed(unsigned s) { rng = s ? s : 0x2545F491u; }   // гейт tests/sfx250: тот же вариант клипа и высота в двух прогонах
unsigned audioTestFramesOut(void) { return framesOut; }
float audioTestAmbGain(void) { return AMB_GAIN; }   // tests/amb161: множитель фона этой сборки
void audioTestPwrEvent(int flags) { mePwrCb(0, flags, NULL); }
void audioTestSimulateFdInvalidate(void) {
  if (AF_OK(mfd)) { afClose(mfd); mfd = AF_NONE; }
  if (AF_OK(emfd)) { afClose(emfd); emfd = AF_NONE; }
  if (AF_OK(lfd)) { afClose(lfd); lfd = AF_NONE; }
}
#endif

// ---------------------------------------------------------------- init / lookup
static int fail(const char *m) { snprintf(err, sizeof err, "%s", m); return -1; }
const char *audioError(void) { return err; }

int audioInit(const char *dir) {
  char p[256];
  for (int b = 0; b < AUDIO_NBUS; b++) busVol[b] = 1;
  for (int s = 0; s < NSTREAM; s++) { streams[s].cur = -1; streams[s].queued = -1; streams[s].reqTrack = -1; streams[s].vol = 1; streams[s].pan = 0; }
  snprintf(p, sizeof p, "%ssfx.sab", dir);
  AFile f = afOpen(p);
  if (!AF_OK(f)) return fail("sfx.sab not found");
  unsigned sz = afSize(f);
  if (!slotOld && (!slot || sz > slotCap)) {   // №1069: место под банк — один раз; банк больше места — место заново
    free(slot); slotCap = sz > SAB_SLOT_BYTES ? sz : SAB_SLOT_BYTES; if (!(slot = bankAlloc(slotCap))) slotCap = 0; }
  bank = !slotOld && slot ? slot : bankAlloc(sz);   // места нет (куча не дала) — банк из кучи, как прежде
  if (!bank || afRead(f, 0, bank, (int)sz) != (int)sz) { afClose(f); return fail("sfx.sab read error / no memory"); }
  afClose(f);
#ifndef AUDIO_HOST
  sceKernelDcacheWritebackRange(bank, sz);   // правка 276: банк — в память (с флаг-файлом meaudio ME читает его мимо кэша)
#endif
  H = (const SabHdr *)bank;
  if (memcmp(H->magic, "SAB1", 4) || H->ver != 1) return fail("sfx.sab: wrong version");
  R = (const SabRes *)(bank + H->resOff); VAR = (const unsigned short *)(bank + H->varOff);
  blankFile = -1;
  for (unsigned i=0;i<H->nres;i++)
    if (!strncmp(R[i].name,"blank",sizeof R[i].name) && R[i].nVar==1)
      blankFile = VAR[R[i].firstVar];
  F = (const SabFile *)(bank + H->fileOff); car = H->ncar ? (const SabCar *)(bank + H->carOff) : 0; carIdx = 0;
  covl = 0; ncovl = 0; carOvl = -1;   // правка 413: таблица наложений из EXT (tools/audio_export.py 'COVL')
  covs=0;covf=0;ncovs=ncovf=0;carStream=-1;carStreamOn=0;
  if (H->extOff && H->extSize >= 8 && !memcmp(bank + H->extOff, "SXT1", 4)) {
    const unsigned char *e = bank + H->extOff; unsigned ns = *(const unsigned *)(e + 4);
    for (unsigned i = 0; i < ns && 8 + i * 16 + 16 <= H->extSize; i++) { const unsigned char *q = e + 8 + i * 16;
      unsigned off = *(const unsigned *)(q + 4), cnt = *(const unsigned *)(q + 8), st = *(const unsigned *)(q + 12);
      if(off>H->extSize || !st || cnt>(H->extSize-off)/st)continue;
      if (!memcmp(q, "COVL", 4) && st == 2) { covl = (const short *)(e + off); ncovl = cnt; }
#if !CAR_STREAM_OLD
      if(!memcmp(q,"COVS",4) && st==4 && !(off&3)){covs=(const CarStreamProfile *)(e+off);ncovs=cnt;}
      if(!memcmp(q,"COVF",4) && st==8 && !(off&3)){covf=(const CarStreamVariant *)(e+off);ncovf=cnt;}
#endif
    }
    if (covl && ncovl) carOvl = covl[0]; }   // машина уровня (у семи машин уровней наложения нет)
  DATA = bank + H->dataOff;
  master = H->bus[0];
  busVol[AUDIO_BUS_SFX] = H->bus[1]; busVol[AUDIO_BUS_CAR] = H->bus[2]; busVol[AUDIO_BUS_MUSIC] = H->bus[3];
  busVol[AUDIO_BUS_AMBIENCE] = H->bus[4] * AMB_GAIN;   // правка Роба 161: фон (птицы) тише, «в оригинале так громко не было»
#if !AMB161_OLD
  busVol[AUDIO_BUS_DIALOG] = H->bus[5];   // аудит 09.10: строка была внутри комментария выше (до applyMode шина голоса оставалась 1)
#endif
  busVol[AUDIO_BUS_UI] = H->bus[1];   // правка 114: уровень эффектов тюнера диска, без наборов режимов (пауза их глушит)

  snprintf(musicPath, sizeof musicPath, "%smusic.sap", dir);
  mfd = afOpen(musicPath);
  if (AF_OK(mfd)) {
    unsigned char h[16];
    if (afRead(mfd, 0, h, 16) == 16 && !memcmp(h, "SAP1", 4)) {
      ntrack = *(int *)(h + 8);
      T = amalloc(sizeof(SapTrack) * (ntrack ? ntrack : 1));
      if (T && afRead(mfd, *(unsigned *)(h + 12), T, (int)sizeof(SapTrack) * ntrack) == (int)sizeof(SapTrack) * ntrack) {
        haveMusic = 1;
        for (int s = 0; s < NSTREAM; s++) {
          streams[s].ring = amalloc(ringSize[s]); streams[s].mask = ringSize[s] - 1;
          if (!streams[s].ring) haveMusic = 0;
        }
      }
    }
    if (haveMusic && !LANGVO1111_OLD && langSel > 0) {   // №1111: озвучка языка поверх английских дорожек
      static const char *const LF[4] = {0, "%sdialog_fr.sap", "%sdialog_ge.sap", "%sdialog_sp.sap"};
      snprintf(langPath, sizeof langPath, LF[langSel], dir); langTracks = 0;
      AFile f = afOpen(langPath); unsigned char lh[64]; int ok = 0;
      if (AF_OK(f) && afRead(f, 0, lh, 64) == 64 && !memcmp(lh, "SAPL", 4)) {
        unsigned ver = *(unsigned *)(lh + 4), n = *(unsigned *)(lh + 8), io = *(unsigned *)(lh + 12), ro = *(unsigned *)(lh + 16), size = afSize(f);
        if (ver == 1 && n <= (unsigned)ntrack && (uint64_t)ro + (uint64_t)n * 64u <= size && io == 64) {
          unsigned short *ix = amalloc(sizeof(unsigned short) * (n ? n : 1)); SapTrack *rw = amalloc(sizeof(SapTrack) * (n ? n : 1));
          if (ix && rw && afRead(f, io, ix, (int)(2 * n)) == (int)(2 * n) && afRead(f, ro, rw, (int)(64 * n)) == (int)(64 * n)) {
            for (unsigned q = 0; q < n; q++) {   // строка берётся, только если имя совпало с английской дорожкой, строка целая и данные внутри файла
              const SapTrack *r = rw + q; if (ix[q] >= (unsigned)ntrack) continue;
              if (strncmp(r->name, T[ix[q]].name, 28) || r->ch != 2 || !r->rate || r->flags || r->bus != 2 || !r->nblk || (uint64_t)r->nsamp != (uint64_t)r->nblk * 28u) continue;
              if (r->off < ro + n * 64u || (r->off & 2047) || (uint64_t)r->off + (uint64_t)r->nblk * 32u > size) continue;
              T[ix[q]] = *r; T[ix[q]].flags |= TRK_LANG; langTracks++;
            }
            ok = 1;
          }
          free(ix); free(rw);
        }
      }
      if (ok && langTracks) lfd = f; else { if (AF_OK(f)) afClose(f); langPath[0] = 0; langTracks = 0; }
    }
    if (!haveMusic) snprintf(err, sizeof err, "music.sap invalid: effects only");
  } else snprintf(err, sizeof err, "music.sap not found: effects only");
#ifndef AUDIO_HOST
  chan = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, FRAMES, PSP_AUDIO_FORMAT_STEREO);
  if (chan < 0) return fail("sceAudioChReserve failed");
  quit = 0;
  audioPwrInit();
  { snprintf(p, sizeof p, "%saudlate", dir); AFile lf = afOpen(p);   // правка 410: красный контроль AUDQ late (файл audio/audlate) — пауза 40 мс раз в 64 буфера
    aqLateEvery = AF_OK(lf) ? 64u : 0u; if (AF_OK(lf)) afClose(lf); }
  ioSema = sceKernelCreateSema("audio_io", 0, 0, 1, NULL);
  mixTh = sceKernelCreateThread("audio_mix", mixThread, 0x12, 0x4000, PSP_THREAD_ATTR_USER, NULL);
  if (mixTh >= 0) sceKernelStartThread(mixTh, 0, NULL);
  if (haveMusic) {
    ioTh = sceKernelCreateThread("audio_io", ioThread, 0x16, 0x2000, PSP_THREAD_ATTR_USER, NULL);
    if (ioTh >= 0) sceKernelStartThread(ioTh, 0, NULL);
  }
  if (mixTh < 0) return fail("audio thread not created");
#endif
  return 0;
}

void audioShutdown(void) {
  quit = 1;
#ifndef AUDIO_HOST
  if (pwrTh >= 0) { sceKernelTerminateDeleteThread(pwrTh); pwrTh = -1; }
  if (mixTh >= 0) { sceKernelWaitThreadEnd(mixTh, NULL); sceKernelDeleteThread(mixTh); mixTh = -1; }
  if (ioTh >= 0) { sceKernelSignalSema(ioSema, 1); sceKernelWaitThreadEnd(ioTh, NULL); sceKernelDeleteThread(ioTh); ioTh = -1; }
  if (ioSema >= 0) { sceKernelDeleteSema(ioSema); ioSema = -1; }
  if (chan >= 0) { sceAudioChRelease(chan); chan = -1; }
#endif
  if (AF_OK(mfd)) afClose(mfd);
  if (AF_OK(emfd)) afClose(emfd);emfd=AF_NONE;netrack=0;extraPath[0]=0;
  if (AF_OK(lfd)) afClose(lfd);lfd=AF_NONE;langPath[0]=0;langTracks=0;
  mfd = AF_NONE;
  musicPath[0] = 0;
  pauseReq = 0; pauseLvl = 1.f;   // №1144
  memset(voices, 0, sizeof voices); memset(J, 0, sizeof *J); memset(strmSi, 0, sizeof strmSi);
  memset(onMem, 0, sizeof onMem); memset(vFl, 0, sizeof vFl); wbAll = 1;   // правка 410 К1: задание стёрто главным
  haveMusic = 0;
  memset(&E,0,sizeof E);memset(worldEngine,0,sizeof worldEngine);
  for(int i=0;i<AUDIO_WORLD_ENGINES;i++)worldSource[i]=worldProfile[i]=-1;
  // смена уровня (viewer/main.c levelUiSnd: у каждого уровня свой audio/ — музыка, звуки, машина): память звука — обратно в
  // кучу, audioInit читает пакет уровня заново (раньше память оставалась: звук не перезапускался)
  for (int s = 0; s < NSTREAM; s++) free(streams[s].ring);
  memset(streams, 0, sizeof streams);
  free(T); T = 0; ntrack = 0;
  if (AF_OK(lfd)) afClose(lfd); lfd = AF_NONE; langPath[0] = 0; langTracks = 0;   // №1111
  if (bank != slot) free(bank);   // №1069: постоянное место банка остаётся на следующий уровень
  bank = 0; H = 0; R = 0; VAR = 0; F = 0; car = 0; DATA = 0;
}

int audioSoundCount(void) { return H ? (int)H->nres : 0; }
const char *audioSoundName(int id) { return H && id >= 0 && id < (int)H->nres ? R[id].name : ""; }
int audioTrackCount(void) { return ntrack; }
const char *audioTrackName(int id) { const SapTrack *r=trackRow(id);return r?r->name:""; }
int audioFind(const char *name) {
  if (!H || !name) return -1;
  for (unsigned i = 0; i < H->nres; i++) if (!strncmp(R[i].name, name, 24)) return (int)i;
  return -1;
}
int audioTrack(const char *name) {
  for (int i = 0; i < ntrack; i++) if (!strncmp(T[i].name, name, 28)) return i;
  return -1;
}
int audioExtraStreams(const char *file) {
 if(!file||!haveMusic||ntrack>=EXTRA_TRACK_BASE||strlen(file)>=sizeof extraPath)return -1;
 if(netrack)return !strcmp(file,extraPath)?netrack:-2;
 AFile f=afOpen(file);if(!AF_OK(f))return -1;
 unsigned h[16];SapTrack rows[EXTRA_TRACK_MAX];int n=0;unsigned size=afSize(f);
 if(afRead(f,0,h,64)!=64||memcmp(h,"SAP1",4)||h[1]!=1||h[3]!=64)goto badExtra;
 n=(int)h[2];if(n<=0||n>EXTRA_TRACK_MAX||afRead(f,64,rows,n*sizeof *rows)!=(int)(n*sizeof *rows))goto badExtra;
 for(int i=0;i<n;i++) {
  const SapTrack *r=rows+i;
  if(!memchr(r->name,0,28)||!r->name[0]||r->ch!=2||!r->rate||r->flags||r->next!=-1||r->bus!=2||!r->nblk||(uint64_t)r->nsamp!=(uint64_t)r->nblk*28u||r->off<64u+64u*n||(r->off&2047)||(uint64_t)r->off+(uint64_t)r->nblk*32u>size)goto badExtra;
  for(int j=0;j<i;j++)if(!strcmp(r->name,rows[j].name))goto badExtra;
 }
 memcpy(ET,rows,n*sizeof *rows);emfd=f;snprintf(extraPath,sizeof extraPath,"%s",file);BARRIER();netrack=n;return n;
badExtra:afClose(f);return -1;
}
int audioExtraTrack(const char *name) {
 if(name)for(int i=0;i<netrack;i++)if(!strcmp(ET[i].name,name))return EXTRA_TRACK_BASE+i;
 return -1;
}

// ---------------------------------------------------------------- voices (main thread)
static Voice *byHandle(int h) {
  int k = (h & 0xff) - 1;
  if (h <= 0 || k < 0 || k >= NVOICE) return 0;
  Voice *v = &voices[k];
  return v->gen == (h >> 8) && v->state != V_FREE ? v : 0;
}
int audioPlay(int id, float vol, float pan, float pitch) { return audioPlayBus(id, vol, pan, pitch, -1); }
static int playPoint(int id, float vol, float pan, float pitch, int bus, const MixPoint *point) {
  if (!H || id < 0 || id >= (int)H->nres || !R[id].nVar) return 0;
  const SabRes *r = &R[id];
  for (int k = 0; k < NVOICE; k++) {
    Voice *v = &voices[k];
    if (v->state != V_FREE) continue;
    const SabFile *f = &F[VAR[r->firstVar + (r->nVar > 1 ? (int)(frand() * r->nVar) % r->nVar : 0)]];
    v->f = f; v->ch = f->ch; v->rate = f->rate; v->st = 0;
    v->loop = r->flags & 1; v->trim = r->trim; v->bus = bus >= 0 && bus < AUDIO_NBUS ? bus : r->bus < AUDIO_BUS_UI ? (int)r->bus : AUDIO_BUS_SFX;
    v->vol = vol; v->pan = pan; v->pitch = pitch * (r->pmin + (r->pmax - r->pmin) * frand());
    MixPoint empty={0};setPoint(v,point?*point:empty);
    v->stopReq = 0; v->gen = (v->gen + 1) & 0x7fffff;
    BARRIER(); v->state = V_START;
    return (v->gen << 8) | (k + 1);
  }
  dropped++;
  return 0;
}
int audioPlayBus(int id,float vol,float pan,float pitch,int bus){return playPoint(id,vol,pan,pitch,bus,0);}
void audioSet(int h, float vol, float pan, float pitch) {
  Voice *v = byHandle(h); if (!v) return;
  v->vol = vol; v->pan = pan; v->pitch = pitch;
}
void audioStop(int h) { Voice *v = byHandle(h); if (v) v->stopReq = 1; }
int audioPlaying(int h) { Voice *v = byHandle(h); return v && !v->stopReq; }

void audioListener(float x, float y, float z, float rx, float rz) {
  lx = x; ly = y; lz = z; float l = sqrtf(rx * rx + rz * rz); if (l > 1e-4f) { lrx = rx / l; lrz = rz / l; }
}
void audioSpatial(float x, float y, float z, float mn, float mx, float *vol, float *pan) {
  float dx = x - lx, dy = y - ly, dz = z - lz, d = sqrtf(dx * dx + dy * dy + dz * dz), dh = sqrtf(dx * dx + dz * dz);
  float g = d <= mn ? 1.f : d >= mx ? 0.f : 1.f - (d - mn) / (mx - mn);
  *vol = g * g;                                  // linear fade in distance, squared in amplitude: ~natural roll-off
  *pan = dh > 1e-3f ? (dx * lrx + dz * lrz) / dh * clampf((dh - 0.5f) / 3.f, 0, 1) * 0.8f : 0;   // centred up close
}
int audioPlayAt(int id, float x, float y, float z, float mn, float mx) {
  return audioPlayAtGain(id,1.f,x,y,z,mn,mx);
}
int audioPlayAtGain(int id,float vol,float x,float y,float z,float mn,float mx) {
  float g, pan; audioSpatial(x, y, z, mn, mx, &g, &pan);
  if(g*vol<=.005f)return 0;
  MixPoint p=spatialPoint(x,y,z,mn,mx);return playPoint(id,vol,0.f,1.f,-1,&p);
}
void audioSetAt(int h,float x,float y,float z,float mn,float mx) {
  Voice *v=byHandle(h);if(v)setPoint(v,spatialPoint(x,y,z,mn,mx));
}
int audioResLoops(int id) { return H && id >= 0 && id < (int)H->nres ? (int)(R[id].flags & 1) : 0; }   // №1174: ресурс банка — петля (голос живёт до audioStop)
void audioStreamAt(int stream,float x,float y,float z,float mn,float mx) {
  if((unsigned)stream<NSTREAM)setPoint(&voices[NVOICE+stream],spatialPoint(x,y,z,mn,mx));
}

void (*audioStreamNote)(int stream, int track, float fade, int held);   // правка 155: журнал (main.c, смоук)
static int sHold, sHeldN, sHeldHas[2], sHeldTrack[2]; static float sHeldFade[2];
static void streamReqModeAt(int s, int track, float fade, int armed, unsigned start);
static void streamReqMode(int s, int track, float fade,int armed) { streamReqModeAt(s, track, fade, armed, 0); }
static void streamReqModeAt(int s, int track, float fade, int armed, unsigned start) {
  if (s < 0 || s >= NSTREAM) return;
  Stream *st = &streams[s];
  if (!trackRow(track)) track = -1;
  int held = sHold && s < 2;   // правка 155: музыка и атмосфера уровня ждут (последний запрос на поток)
  if (audioStreamNote) audioStreamNote(s, track, fade, held);
  if (held) { sHeldHas[s] = 1; sHeldTrack[s] = track; sHeldFade[s] = fade; sHeldN++; return; }
  st->reqTrack = track; st->reqFade = fade;st->reqArmed=armed&&track>=0; st->reqStart = start; BARRIER(); st->reqGen++;
}
static void streamReq(int s,int track,float fade){streamReqMode(s,track,fade,0);}
int audioHoldStreams(int mode) {
  int n = sHeldN;
  if (mode > 0) { sHold = 1; sHeldN = 0; sHeldHas[0] = sHeldHas[1] = 0; return 0; }
  sHold = 0; sHeldN = 0;
  for (int s = 0; s < 2; s++) if (sHeldHas[s]) { sHeldHas[s] = 0; if (mode == 0) streamReq(s, sHeldTrack[s], sHeldFade[s]); }
  return n;
}
void audioMusic(int track, float fade) { streams[0].once = 0; streamReq(0, track, fade); }
// правка 303: один проход без петли и без перехода к next (концовка «вышел из машины» M2–M7: по данным — петля, на слух — «2 с и заново»)
void audioMusicOnce(int track, float fade) { streams[0].once = 1; streamReq(0, track, fade); }
// №789: дорожка музыки с места startSec (продолжить стингер газеты на звуке нового уровня); за концом — с начала
void audioMusicFrom(int track, float fade, float startSec) {
  const SapTrack *r = trackRow(track); unsigned at = r && startSec > 0.f ? (unsigned)(startSec * (float)r->rate) : 0u;
  streams[0].once = 0; streamReqModeAt(0, track, fade, 0, at);
}
void audioAmbience(int track, float fade) { streamReq(1, track, fade); }
void audioStream(int s, int track, float fade) {
  // New generic tracks are 2D; positional callers set their own point next
  // A world gag must not leave its location attached to a mission voice
  if ((unsigned)s < NSTREAM) { MixPoint p={0}; setPoint(&voices[NVOICE+s],p); }
  streamReq(s, track, fade);
}
void audioStreamSet(int s, float vol, float pan) {
  if (s < 0 || s >= NSTREAM) return;
  streams[s].vol = clampf(vol, 0, 1); streams[s].pan = clampf(pan, -1, 1);
}
int audioStreamPrepare(int s,int track) {
 if((unsigned)s>=NSTREAM||s<2||!haveMusic||!trackRow(track))return 0;
 MixPoint p={0};setPoint(&voices[NVOICE+s],p);streamReqMode(s,track,0,1);return 1;
}
int audioStreamPrepared(int s,int track) {
 if((unsigned)s>=NSTREAM)return 0;Stream *st=&streams[s];
 return st->reqTrack==track&&st->reqArmed&&st->doneReq==st->reqGen&&st->state==S_WAIT&&st->ackGen==st->cmdGen&&st->wr>st->rd;
}
int audioStreamStartPrepared(int s,int track) {
 if((unsigned)s>=NSTREAM)return 0;Stream *st=&streams[s];
 if(st->reqTrack!=track||!st->reqArmed||!trackRow(track))return 0;
 BARRIER();st->reqArmed=0;return 1;
}
unsigned audioStreamRequestSerial(int s) { return (unsigned)s < NSTREAM ? streams[s].reqGen : 0; }
int audioStreamTrack(int s) {                    // what the stream plays or is about to play (-1 = nothing)
  if (s < 0 || s >= NSTREAM) return -1;
  Stream *st = &streams[s];
  if (st->reqGen != st->doneReq) return st->reqTrack;
  if (st->state == S_FADE || st->state == S_WAIT) return st->pend;
  return st->state == S_PLAY ? st->cur : -1;
}
int audioTrackBus(int t) { const SapTrack *r=trackRow(t);return r?r->bus:-1; }
float audioTrackSeconds(int t) { const SapTrack *r=trackRow(t);return r&&r->rate?r->nsamp/(float)r->rate:0; }
const void *audioBankExt(unsigned *size) {
  if (!H || !H->extOff || !H->extSize) { if (size) *size = 0; return 0; }
  if (size) *size = H->extSize;
  return bank + H->extOff;
}
void audioMusicNext(int track) { if (track >= 0 && track < ntrack) streams[0].queued = track; }
void audioBus(int bus, float vol) {
  if (bus < 0 || bus >= AUDIO_NBUS) return;
#if !AMB161_OLD
  if (bus == AUDIO_BUS_AMBIENCE) vol *= AMB_GAIN;   // аудит 09.10 п. 22: набор шин режима больше не затирает множитель фона
#endif
  busVol[bus] = clampf(vol, 0, 1);
}
void audioMaster(float vol) { master = clampf(vol, 0, 1); }
void audioUserVolume(int bus, float vol) { if (bus >= 0 && bus < AUDIO_NBUS) userVol[bus] = clampf(vol, 0, 1); }
// правка 279 (04.10: «на PSP нет музыки ни в миссии, ни в погоне»): служебно — громкость пути музыки целиком (мастер × шина × игрок)
// и что играет поток музыки; строка AUDV в журнал раз в секунду (main.c) и в чёрный ящик
int audioMusicState(char *out, int n) {
  const Stream *m = &streams[AUDIO_ST_MUSIC];
  int l = snprintf(out, n, "master=%.2f bus=%.2f user=%.2f vol=%.2f cur=%s state=%d hold=%d buf=%u", master, busVol[AUDIO_BUS_MUSIC],
                  userVol[AUDIO_BUS_MUSIC], m->vol, m->cur >= 0 ? audioTrackName(m->cur) : "-", m->state, sHold, m->wr - m->rd);
#ifndef AUDIO_HOST
  if (l > 0 && l + 8 < n) { l += snprintf(out + l, n - l, " me="); l += audioMeState(out + l, n - l); }   // правка 276
#endif
  return l;
}


typedef struct { float skid, life, burn; int air, spin, parked, valid; } EngEx;   // сверка g08-sound: состояние машины игрока для модели мотора
static EngEx engEx;
#ifndef ENGINE_SV_OLD
#define ENGINE_SV_OLD 0
#endif
// skid01 — уровень заноса/буксования (skidSoundLevel), spinInPlace — буксование с места, air — все колёса вне земли, parked — игрок вышел
// (после конца анимации выхода: только холостой клип), life01 — остаток жизни машины 0..1. Раз в кадр до sndEngine (sound.c: sndCarState)
// круг 7 (g08, P10-11, сверка с оригиналом): занос — два случая. Уровень «бернаута» больше нуля (полный газ на малой скорости, ручник): высота = 0,5 + (1,3 − 0,5) · уровень
// (пределы по умолчанию, в car_tune.spt диска их нет); иначе скольжение: от 0,5 · предел оборотов до 0,9 · предел по газу. Раньше случай выбирался по «стоит на месте и газ > 0,3»
// и уровнем служила сила шин. SV_BURNPITCH_OLD=1 — прежнее
#ifndef SV_BURNPITCH_OLD
#define SV_BURNPITCH_OLD 0
#endif
static float engBurnLevel;
void audioEngineBurn(float level01) { engBurnLevel = level01 > 0.f ? (level01 > 1.f ? 1.f : level01) : 0.f; }
void audioEngineEx(float skid01, int spinInPlace, int air, int parked, float life01) {
  engEx.burn = engBurnLevel; engEx.skid = skid01; engEx.spin = spinInPlace; engEx.air = air; engEx.parked = parked; engEx.life = life01; engEx.valid = 1;
}
#ifdef AUDIO_HOST
unsigned audioClockUs(void) { return (unsigned)(framesOut * (1000000.0 / RATE)); }   // в тестах на Маке время идёт по выданным кадрам
#else
unsigned audioClockUs(void) { return nowUs(); }
#endif
// ---------------------------------------------------------------- player car (car_tune.spt model)
// Interpretation of the disc parameters (names only; the original code is not used):
//  ShiftPoint(g, s): gear g engages at speed fraction s of top speed (below ShiftPoint 1 = idle pitch);
//  GearPitchRange(g, lo, hi): engine clip pitch goes lo -> hi across gear g;
//  GearShiftPitchDrop(g, d): on the upshift out of gear g the pitch target dips by factor d for DelayTimeMsecs
//    (a short "clutch" dip; the glide makes it subtle) and the gearshift clip plays;
//  DownshiftDamperSize: hysteresis (speed fraction) before a downshift;
//  Attack/Delay/DecayTimeMsecs + DecayFinishTrim: engine volume envelope on throttle (off: wait Delay, fall to
//    DecayFinishTrim over Decay; on: back to full over Attack);
//  ReversePitchRange(lo, hi) over 0..ReversePitchCapKmh; IdleEnginePitch at standstill.
static void enginePosition(Engine *e,MixPoint point) {
  e->point=point;
  int handles[]={e->eng,e->idle,e->fire,e->horn,e->skid,e->ovl};
  for(unsigned i=0;i<sizeof handles/sizeof *handles;i++) { Voice *v=byHandle(handles[i]);if(v)setPoint(v,point); }
}
void audioEnginePosition(float x,float y,float z,int outside) {
  // сверка g08-sound (P10-16): оставленная машина — холостой клип, полная громкость до 15 м, тишина на 50 м (avatar_idle)
  MixPoint point=spatialPoint(x,y,z,(!ENGINE_SV_OLD && engEx.parked)?15.f:4.f,(!ENGINE_SV_OLD && engEx.parked)?50.f:60.f);point.on=outside!=0;enginePosition(&E,point);
}
static int enginePlay(Engine *e,int id,float vol,float pitch) { return playPoint(id,vol,0.f,pitch,-1,&e->point); }
// ============================================================ сверка с исходником, окно g08-sound (02.11.2026): мотор по данным
// Документ СВЕРКА_ПОЛНАЯ/P10: P10-3 (щелчка передачи нет), 5 (шкала = TopSpeed ведомой машины), 6/Д1 (повышение отдельным голосом, спад новой
// передачи вычитанием), 7 (громкость не зависит от газа), 8 (высота не быстрее 2 ед./с, в заносе 0,5), 9 (накат), 10 (в воздухе 0,5·предел .. 2,0),
// 11 (занос и буксование), 15 (громкость «fire» по остатку жизни), 16/Д8 (оставленная машина: только холостой, 15/50 м), 28 (выше TopSpeed),
// 29 (задний ход: порог 5 %, без сглаживания), 30 (холостой отдельным голосом до 1 %), Д9 (порядок режимов). ENGINE_SV_OLD=1 — прежняя модель.
#ifndef ENGINE_SV_OLD
#define ENGINE_SV_OLD 0
#endif
enum { EM_NORM, EM_IDLE, EM_REV, EM_AIR, EM_SKID, EM_PARK };
static float svSlew(float cur, float tgt, float rate, float dt) { float d = tgt - cur, m = rate * dt; return d > m ? cur + m : d < -m ? cur - m : tgt; }
static float svLimit(const SabCar *c) {   // предел оборотов: наибольшая высота передач, меньшая 5 (у famil_v 1,82)
  float m = 0; for (int g = 0; g < 5; g++) if (c->hi[g] < 5.f && c->hi[g] > m) m = c->hi[g];
  return m > 0.1f ? m : 1.82f;
}
static float svCurve(const SabCar *c, int g, float s, int ng) {   // высота по кривой передачи g (1..5) на доле скорости s, сверху не зажата
  float a = c->shift[g - 1], b = g < ng ? c->shift[g] : 1.f, den = b - a > 1e-3f ? b - a : 1e-3f, u = (s - a) / den;
  if (u < 0.f) u = 0.f;
  return c->lo[g - 1] + (c->hi[g - 1] - c->lo[g - 1]) * u;
}
static void engineStepSv(Engine *e, const SabCar *c, float kmh, float topKmh, float thr, unsigned flags, float dt, const EngEx *x) {
  if (!(flags & AUDIO_ENGINE_ON) || !c) {
    audioStop(e->eng); audioStop(e->idle); audioStop(e->fire); audioStop(e->sh);
    e->eng = e->idle = e->fire = e->sh = 0; e->gear = 0; e->pitch = 0; e->shOn = 0; e->coast = 0; e->prevSpd = 0;
    return;
  }
  const float top = topKmh > 1 ? topKmh : 1, spd = fabsf(kmh), s = spd / top, L = svLimit(c);
  const float eg = mix250 ? ENG_250_GAIN : 1.f;
  int ng = 1; while (ng < 5 && c->shift[ng] < 0.999f) ng++;
  const int skidding = x && x->skid > 0.02f;
  int mode;
  if (x && x->parked) mode = EM_PARK;
  else if ((flags & AUDIO_ENGINE_REVERSE) && s > 0.05f) mode = EM_REV;          // P10-29: больше 5 % TopSpeed
  else if (s > 0.01f) mode = (x && x->air) ? EM_AIR : skidding ? EM_SKID : EM_NORM;   // Д9: назад, воздух, занос, передачи
  else mode = skidding ? EM_SKID : EM_IDLE;
  const int split = c->idle >= 0 && c->idle != c->engine;
  float target = c->idlePitch, rate = 2.f;   // 1 единица за 500 мс
  if (e->shOn && mode != EM_NORM) { e->shOn = 0; audioStop(e->sh); e->sh = 0; }
  switch (mode) {
    case EM_REV: {
      float u = clampf(spd / (c->revCap > 1 ? c->revCap : 1), 0, 1);
      target = c->revLo + (c->revHi - c->revLo) * u; e->pitch = target; e->gear = -1; e->coast = 0; break; }   // P10-29: сразу
    case EM_AIR: { float lo = 0.5f * L; target = lo + clampf(thr, 0, 1) * (L - lo); rate = (L - lo) / 0.5f; e->gear = 0; e->coast = 0; break; }   // P10-10 (круг 3, проверка): 0,5·предел .. предел оборотов, отклик 500 мс на весь размах
    case EM_SKID:
      if (SV_BURNPITCH_OLD) target = x->spin ? 0.5f + clampf(x->skid, 0, 1) * 0.8f : 0.5f * L + clampf(thr, 0, 1) * 0.4f * L;   // P10-11 (до круга 7)
      else target = x->burn > 0.f ? 0.5f + (1.3f - 0.5f) * x->burn : 0.5f * L + (0.9f * L - 0.5f * L) * clampf(thr, 0, 1);   // круг 7: бернаут 0,5..1,3 по уровню; скольжение 0,5..0,9 предела по газу
      rate = 0.5f; e->gear = 0; e->coast = 0; break;
    case EM_PARK: case EM_IDLE: target = c->idlePitch; e->gear = 0; e->coast = 0; break;
    default: {
      int prev = e->gear < 1 ? 1 : e->gear, g = prev; const int moving = e->gear >= 1;
      while (g < ng && s >= c->shift[g]) g++;
      if (g == prev) while (g > 1 && s < c->shift[g - 1] - c->damper) g--;
      if (g > prev && moving) {   // повышение: основной голос замолкает, отдельный голос с той же записью спадает (P10-6)
        e->shOn = 1; e->shT = 0.f; e->coast = 0;
        float p0 = svCurve(c, prev, s, ng); if (p0 > 5.f) p0 = 5.f;
        e->shP0 = p0; e->shD = c->drop[g - 1];   // Д1: спад НОВОЙ передачи, вычитанием
        if (e->sh) audioStop(e->sh);
        e->sh = enginePlay(e, c->engine, eg, p0);
      } else if (g < prev && moving) { e->coast = 1; e->coastV = spd; e->coastP = e->pitch; }   // P10-9: накат
      e->gear = g;
      if (e->coast) {
        if (spd > e->prevSpd) { e->coast = 0; e->fastT = 0.1f; }   // круг 3 (проверка P10-9): любой рост скорости — возврат в обычный режим за 0,1 с
        else {
          float f = e->coastV > 1e-3f ? clampf(spd / e->coastV, 0, 1) : 0.f;
          target = c->idlePitch + (e->coastP - c->idlePitch) * f; e->pitch = target; break;
        }
      }
      target = svCurve(c, g, s, ng); if (target > L) target = L;   // P10-28: выше TopSpeed высота упирается в предел оборотов
      if (g > 1 && s < c->shift[g - 1]) target *= s / c->shift[g - 1];   // круг 3 (P10-9): зона демпфера — остаётся верхняя передача, высота умножается на долю скорости
    }
  }
  e->prevSpd = spd;
  if (mode != EM_REV && !e->coast) {
    if (e->pitch <= 0.f) e->pitch = x ? 0.001f : target;          // P10-8: у машины игрока высота стартует с нуля
    float r = rate; if (e->fastT > 0.f) { r *= 5.f; e->fastT -= dt; }
    if (!e->shOn) e->pitch = svSlew(e->pitch, target, r, dt);     // круг 3 (P10-6): внутренняя высота во время повышения не меняется
  }
  float vMain = eg, pMain = e->pitch;
  if (e->shOn) {
    float dec = (c->decayMs > 1 ? c->decayMs : 1) * 0.001f, del = c->delayMs * 0.001f, att = (c->attackMs > 1 ? c->attackMs : 1) * 0.001f;
    e->shT += dt;
    if (e->shT < dec) {
      float f = e->shT / dec, p = e->shP0 - e->shD * f; if (p < 0.05f) p = 0.05f;
      if (e->sh) audioSet(e->sh, (1.f - (1.f - c->decayFinish) * f) * eg, 0, p);
      vMain = 0.f; pMain = 1.f;
    } else {
      if (e->sh) { audioStop(e->sh); e->sh = 0; }
      float t2 = e->shT - dec;
      if (t2 < del) { vMain = 0.f; pMain = 1.f; }
      else if (t2 < del + att) vMain = (c->decayFinish + (1.f - c->decayFinish) * ((t2 - del) / att)) * eg;
      else e->shOn = 0;
    }
  }
  const int idleVoice = (mode == EM_IDLE || mode == EM_PARK) && split;   // P10-30: жёсткое переключение
  if (idleVoice) {
    if (e->eng) { audioStop(e->eng); e->eng = 0; }
    if (!audioPlaying(e->idle)) e->idle = enginePlay(e, c->idle, eg, pMain); else audioSet(e->idle, eg, 0, pMain);
  } else {
    if (e->idle) { audioStop(e->idle); e->idle = 0; }
    if (!audioPlaying(e->eng)) { if (e == &E && audioNote && !e->eng) audioNote(c->engine, "engine");
      e->eng = enginePlay(e, c->engine, vMain, pMain); }
    else audioSet(e->eng, vMain, 0, pMain);                        // P10-7: громкость не зависит от газа
  }
  if ((flags & AUDIO_ENGINE_DAMAGED) && c->damaged >= 0 && mode != EM_PARK) {   // P10-15: 0 при остатке 40 %, 1 при 0 %
    float fv = (x && x->valid) ? clampf((0.4f - x->life) / 0.4f, 0, 1) : 1.f;
    if (!audioPlaying(e->fire)) e->fire = enginePlay(e, c->damaged, fv, 1); else audioSet(e->fire, fv, 0, 1);
  } else if (e->fire) { audioStop(e->fire); e->fire = 0; }
}

static void engineStep(Engine *e,const SabCar *c,float kmh,float topKmh,float thr,unsigned flags,float dt,int gearClick) {
  if (!(flags & AUDIO_ENGINE_ON)) {
    audioStop(e->eng); audioStop(e->idle); audioStop(e->fire);
    e->eng = e->idle = e->fire = 0; e->gear = 0; e->pitch = 0;
    return;
  }
  float spd = fabsf(kmh), s = clampf(spd / (topKmh > 1 ? topKmh : 1), 0, 1);
  int ng = 1; while (ng < 5 && c->shift[ng] < 0.999f) ng++;
  float target;
  if (flags & AUDIO_ENGINE_REVERSE) {
    float u = clampf(spd / (c->revCap > 1 ? c->revCap : 1), 0, 1);
    target = c->revLo + (c->revHi - c->revLo) * u; e->gear = -1;
  } else if (s < c->shift[0]) {
    target = c->idlePitch; e->gear = 0;
  } else {
    int g = e->gear < 1 ? 1 : e->gear, prev = g;
    while (g < ng && s >= c->shift[g]) g++;
    while (g > 1 && s < c->shift[g - 1] - c->damper) g--;
    if (g > prev && e->gear >= 1) {
      e->dip = c->delayMs * 0.001f; e->dipMul = c->drop[prev - 1];
      if (gearClick && !mix250) {   // правка 250: у оригинала щелчка на повышении нет (скан записи PS2), остаётся провал высоты мотора
        static int gs = -2; if (gs == -2) gs = audioFind("gearshift");
        if (gs >= 0) { if (audioNote) audioNote(gs, "gear"); audioPlay(gs, 0.7f, 0, 1); }   // 135.7: строка SFX в журнал
      }
    }
    e->gear = g;
    float a = c->shift[g - 1], b = g < ng ? c->shift[g] : 1.f;
    target = c->lo[g - 1] + (c->hi[g - 1] - c->lo[g - 1]) * clampf((s - a) / (b - a > 1e-3f ? b - a : 1e-3f), 0, 1);
  }
  if (e->dip > 0) { target *= e->dipMul; e->dip -= dt; }
  if (e->pitch <= 0) e->pitch = target;
  e->pitch += (target - e->pitch) * (1.f - expf(-dt / 0.06f));
  if (thr > 0.05f) { e->off = 0; e->env += dt / (c->attackMs > 1 ? c->attackMs * 0.001f : 0.001f); }
  else { e->off += dt; if (e->off > c->delayMs * 0.001f) e->env -= dt / (c->decayMs > 1 ? c->decayMs * 0.001f : 0.001f) * (1 - c->decayFinish); }
  e->env = clampf(e->env, c->decayFinish, 1);
  // idle clip (if the car has a separate one) crossfades out as the car starts moving
  float idleMix = (c->idle >= 0 && c->idle != c->engine) ? clampf(1 - s / (c->shift[0] * 4 + 0.02f) - thr, 0, 1) : 0;
  // правка 250: мотор у оригинала к музыке 0,483 (газ, 0–10 м/с: остаток выхода SPU2 близнеца v13 без музыки −26,1 dBFS / клип
  // famil), у порта было 0,575 (трим 0,51 × шина 0,88 / 0,78) — на 1,5 дБ громче; ×0,84. Прежнее — SND_250_OLD=1
  const float eg = mix250 ? ENG_250_GAIN : 1.f;
  if (!audioPlaying(e->eng)) { if (e == &E && audioNote && !e->eng) audioNote(c->engine, "engine");   // 413: какой клип мотора у игрока пошёл
    e->eng = enginePlay(e, c->engine, e->env * (1 - idleMix) * eg, e->pitch); }
  else audioSet(e->eng, e->env * (1 - idleMix) * eg, 0, e->pitch);
  if (idleMix > 0) {
    if (!audioPlaying(e->idle)) e->idle = enginePlay(e, c->idle, idleMix * eg, c->idlePitch); else audioSet(e->idle, idleMix * eg, 0, c->idlePitch);
  } else if (e->idle) { audioStop(e->idle); e->idle = 0; }
  if ((flags & AUDIO_ENGINE_DAMAGED) && c->damaged >= 0) { if (!audioPlaying(e->fire)) e->fire = enginePlay(e, c->damaged, 1, 1); }
  else if (e->fire) { audioStop(e->fire); e->fire = 0; }
}
void audioEngine(float kmh, float topKmh, float thr, unsigned flags) {
  if (!car) return;
#ifdef AUDIO_HOST
  unsigned t = (unsigned)(framesOut * (1000000.0 / RATE));
#else
  unsigned t = nowUs();
#endif
  float dt = E.last ? (t - E.last) * 1e-6f : 0.016f; E.last = t;
  dt = clampf(dt, 0, 0.1f);
  const int parkSv = !ENGINE_SV_OLD && engEx.parked;   // сверка g08-sound (P10-16, Д8): оставленная машина — только холостой клип
  if (!ENGINE_SV_OLD) engineStepSv(&E,car,kmh,topKmh,thr,flags,dt,&engEx);
  else engineStep(&E,car,kmh,topKmh,thr,flags,dt,1);
  if (parkSv) { flags &= ~AUDIO_ENGINE_ON; if (E.skid) { audioStop(E.skid); E.skid = 0; } if (E.horn) { audioStop(E.horn); E.horn = 0; } }
  // правка 413: наложение машины (OverlayClipName — ракета gramR_v, телефоны cCell*, фургон cVan, пицца): SLES_518.97 запускает его
  // при активации звука машины игрока (вход, 0x230e48 -> 0x231254) петлёй ресурса; у порта — пока мотор машины игрока включён
  if ((flags & AUDIO_ENGINE_ON) && carOvl >= 0) {
    if (!audioPlaying(E.ovl)) { if (audioNote && !E.ovl) audioNote(carOvl, "overlay"); E.ovl = enginePlay(&E, carOvl, 1, 1); }
  } else if (E.ovl) { audioStop(E.ovl); E.ovl = 0; }
#if !CAR_STREAM_OLD
  if((flags&AUDIO_ENGINE_ON) && !carStreamOn) {
    if(covs && covf && (unsigned)carIdx<ncovs) {
      const CarStreamProfile *p=&covs[carIdx];
      if(p->count && p->first<=ncovf && p->count<=ncovf-p->first) {
        const CarStreamVariant *v=&covf[p->first+(unsigned)(frand()*p->count)%p->count];
        if(v->track>=0 && v->track<ntrack && T[v->track].bus==4 && (T[v->track].flags&1)) {
          carStream=v->track;audioStreamSet(AUDIO_ST_CAR_OVERLAY,v->trim,0);
          audioStream(AUDIO_ST_CAR_OVERLAY,carStream,0);
        }
      }
    }
    carStreamOn=1;
  } else if(!(flags&AUDIO_ENGINE_ON) && carStreamOn) {
    audioStream(AUDIO_ST_CAR_OVERLAY,-1,0);carStream=-1;carStreamOn=0;
  }
#endif
}
#ifndef CARSND_OLD
#define CARSND_OLD 0   // 1 — прежнее: мотор и занос всегда машины уровня (красный контроль tests/hover/carsnd.sh)
#endif
static int carSndOld = CARSND_OLD;
void audioCarSndOld(int on) { carSndOld = on != 0; }
int audioPlayerCar(const char *name) {   // -> номер профиля, которым звучит машина игрока
  if (!H || !H->ncar) return -1;
  const SabCar *profiles = (const SabCar *)(bank + H->carOff);
  int k = 0;
  if (!carSndOld && name && *name)
    for (unsigned i = 0; i < H->ncar; i++) if (!strncasecmp(profiles[i].name, name, sizeof profiles[i].name)) { k = (int)i; break; }
  if (k == carIdx && car) return k;
  if(carStreamOn){audioStream(AUDIO_ST_CAR_OVERLAY,-1,0);carStream=-1;carStreamOn=0;}
  audioStop(E.eng); audioStop(E.idle); audioStop(E.fire); audioStop(E.skid); audioStop(E.horn); audioStop(E.ovl);   // другой клип мотора — заново
  E.eng = E.idle = E.fire = E.skid = E.horn = E.ovl = 0; E.gear = 0; E.pitch = 0;
  carIdx = k; carOvl = (!carSndOld && covl && (unsigned)k < ncovl) ? covl[k] : -1;
  if (!k) { car = profiles; return 0; }
  carCur = profiles[k];
#ifdef CAR_PROFILE_FALLBACK_OLD
  if (carCur.horn < 0) carCur.horn = profiles[0].horn;
  if (carCur.damaged < 0) carCur.damaged = profiles[0].damaged;
  if (carCur.doorOpen < 0) carCur.doorOpen = profiles[0].doorOpen;
  if (carCur.doorClose < 0) carCur.doorClose = profiles[0].doorClose;
#endif
  car = &carCur; return k;
}
int audioCarProfile(const char *name) {
  if(!H || !name)return -1;
  const SabCar *profiles=(const SabCar *)(bank+H->carOff);
  for(unsigned i=0;i<H->ncar;i++)if(!strncmp(profiles[i].name,name,sizeof profiles[i].name))return (int)i;
  return -1;
}
void audioWorldEngine(int slot,int source,int profile,float x,float y,float z,float kmh,float topKmh,float thr,float dt) {
  if((unsigned)slot>=AUDIO_WORLD_ENGINES)return;
  Engine *e=&worldEngine[slot];
  if(source!=worldSource[slot] || profile!=worldProfile[slot] || source<0 || !H || profile<0 || profile>=(int)H->ncar) {
    audioStop(e->skid); // source changes must not leave the old tyre loop behind
    engineStep(e,NULL,0,1,0,0,0,0);memset(e,0,sizeof *e);
    worldSource[slot]=source;worldProfile[slot]=profile;
  }
  if(source<0 || !H || profile<0 || profile>=(int)H->ncar)return;
  enginePosition(e,spatialPoint(x,y,z,4.f,60.f));
  const SabCar *c=(const SabCar *)(bank+H->carOff)+profile;
  engineStep(e,c,kmh,topKmh,clampf(thr,0,1),AUDIO_ENGINE_ON,clampf(dt,0,.1f),0);
}
// ============================================================ сверка g08-sound: мотор трафика и ИИ миссий (P10-1, P10-2, P10-21, Д2)
// Трафик: до 5 машин, каждая — клип «ferrari» (cellcar.rsd, петля) по generic_traffic (полная громкость до 20 м, тишина на 100 м), высота =
// (0,5 + 0,5·скорость/желаемая) × множитель машины (0,8…1,2, выбран при появлении), не выше 1,5; играет и стоящая. Машины ИИ миссий: до 5, клип
// «mission_ai_vehicle» (sportscarB.rsd), loud_ai_vehicle 15…100 м, высота 0,3 + 0,7·скорость/TopSpeed, не выше 1,2. Оверлей трафика (cCell*, cVan,
// icecream, IStruck, nuctruck, pizza, votetruc) — позиционно на машине по тем же настройкам. Шин у них нет. Профили моделей не используются.
// Голосов в микшере 24 (ABI с Media Engine): SV_WORLD_VOICES ограничивает сумму петель мира. WORLD_ENGINE_SV_OLD=1 — прежние 3 источника с профилем модели
#ifndef WORLD_ENGINE_SV_OLD
#define WORLD_ENGINE_SV_OLD 0
#endif
#ifndef SV_WORLD_VOICES
#define SV_WORLD_VOICES 8
#endif
typedef struct { int h, ovl; float pitch, mult; } SvEng;
static SvEng svT[AUDIO_SV_SLOTS], svA[AUDIO_SV_SLOTS];
static int svResT = -2, svResA = -2;
int audioSvReady(void) {
  if (WORLD_ENGINE_SV_OLD || !H) return 0;
  if (svResT == -2) { svResT = audioFind("ferrari"); svResA = audioFind("mission_ai_vehicle"); }
  return svResT >= 0 && svResA >= 0;
}
static int svVoices(void) { int n = 0; for (int i = 0; i < AUDIO_SV_SLOTS; i++) n += (svT[i].h != 0) + (svT[i].ovl != 0) + (svA[i].h != 0); return n; }
// kind 0 трафик, 1 ИИ. on=0 — остановить слот. profile — профиль модели для оверлея (трафик), -1 нет. mult — множитель машины (трафик). Возврат 1 — слот звучит
int audioSvEngine(int kind, int slot, int on, int profile, float x, float y, float z, float speed, float ref, float mult, float dt) {
  if ((unsigned)slot >= AUDIO_SV_SLOTS || !audioSvReady()) return 0;
  SvEng *e = kind ? &svA[slot] : &svT[slot];
  const float mn = kind ? 15.f : 20.f, mx = 100.f;
  float ddx = x - lx, ddy = y - ly, ddz = z - lz;
  if (on && ddx * ddx + ddy * ddy + ddz * ddz > (mx + 25.f) * (mx + 25.f)) on = 0;   // P10-34: дальше максимума + 25 м голос не нужен
  if (!on) { if (e->h) { audioStop(e->h); e->h = 0; } if (e->ovl) { audioStop(e->ovl); e->ovl = 0; } return 0; }
  float r = ref > 0.1f ? speed / ref : 0.f; if (r < 0.f) r = -r;
  float target = kind ? 0.3f + 0.7f * r : (0.5f + 0.5f * r) * (mult > 0.f ? mult : 1.f);
  target = clampf(target, 0.05f, kind ? 1.2f : 1.5f);
  int res = kind ? svResA : svResT; float mn2 = mn;
  if (!kind) {   // круг 2 (g11b P10-31): гроб Хэллоуина — клип traffic_coffin, 10-100 м
    static int coffinProf = -2, coffinRes = -2;
    if (coffinProf == -2) { coffinProf = audioCarProfile("coffin"); coffinRes = audioFind("traffic_coffin"); }
    if (profile >= 0 && profile == coffinProf && coffinRes >= 0) { res = coffinRes; mn2 = 10.f; }
  }
  MixPoint p = spatialPoint(x, y, z, mn2, mx);
  if (!audioPlaying(e->h)) {
    if (svVoices() >= SV_WORLD_VOICES) return 0;
    e->pitch = kind ? target : 0.5f;                                    // в момент появления высота 0,5
    e->h = playPoint(res, 1.f, 0.f, e->pitch, -1, &p);
  } else {
    e->pitch = target;   // круг 3 (проверка P10-1, P10-2): у трафика и ИИ высота ставится сразу, без предела скорости
    Voice *v = byHandle(e->h); if (v) setPoint(v, p);
    audioSet(e->h, 1.f, 0.f, e->pitch);
  }
  if (!kind && covl && profile >= 0 && (unsigned)profile < ncovl && covl[profile] >= 0) {   // P10-21: оверлей трафика
    if (!audioPlaying(e->ovl)) { if (svVoices() < SV_WORLD_VOICES) e->ovl = playPoint(covl[profile], 1.f, 0.f, 1.f, -1, &p); }
    else { Voice *v = byHandle(e->ovl); if (v) setPoint(v, p); }
  }
  return e->h != 0;
}
void audioSvStopAll(void) { for (int i = 0; i < AUDIO_SV_SLOTS; i++) { audioSvEngine(0, i, 0, -1, 0, 0, 0, 0, 1, 1, 0); audioSvEngine(1, i, 0, -1, 0, 0, 0, 0, 1, 1, 0); } }

void audioHorn(int on) {
  if (!car || car->horn < 0) return;
  // сверка g08-sound (P10-14): гудок без петли (carhom_v, krust_v, bbman_v … 12 машин) играет один раз целиком за нажатие, не обрывается при
  // отпускании и не повторяется, пока кнопка держится. ENGINE_SV_OLD=1 — прежнее
  static int held; const int oneShot = !ENGINE_SV_OLD && !(R[car->horn].flags & 1);
  if (oneShot) {
    if (on && !held && !audioPlaying(E.horn)) { if (audioNote) audioNote(car->horn, "horn"); E.horn = enginePlay(&E, car->horn, 1, 1); }
    held = on; return;
  }
  held = on;
  if (on && !audioPlaying(E.horn)) { if (audioNote) audioNote(car->horn, "horn"); E.horn = enginePlay(&E, car->horn, 1, 1); }   // 413: журнал
  if (!on && E.horn) { audioStop(E.horn); E.horn = 0; }
}
#ifndef SV_SKIDPITCH_OLD
#define SV_SKIDPITCH_OLD 0   // сверка g08-sound круг 2 (от g01-cars P10-12): высота клипа шин в оригинале постоянна; 1 — прежнее 0,9 + 0,2·уровень
#endif
#define SKID_PITCH(a) (SV_SKIDPITCH_OLD ? 0.9f + 0.2f * (a) : 1.f)
static void engineSkid(Engine *e, const SabCar *profile, float amount, int dirt) {
  int res = profile ? (dirt ? profile->dirtSkid : profile->skid) : -1;
  // правка 413а: у hbike_v, witchcar, huskA (и грунт mono_v) занос в данных — blank.rsd: тишина, голос не занимать (клип короче
  // кадра — иначе перезапуск каждый кадр)
  if (res >= 0 && !carSndOld && !strncmp(R[res].name, "blank", sizeof R[res].name)) res = -1;
#if !CARSND_BLANK_ALIAS_OLD
  if (res >= 0 && !carSndOld && blankFile >= 0 && R[res].nVar) {
    int silent = 1;
    for (unsigned i=0;i<R[res].nVar;i++)
      if (VAR[R[res].firstVar+i] != blankFile) { silent=0; break; }
    if (silent) res = -1;
  }
#endif
  if (amount > 0.02f && res >= 0) {
    if (!audioPlaying(e->skid) || e->skidRes != res) {
      if (e->skid) audioStop(e->skid);
      if (audioNote) audioNote(res, "skid");   // правка 413а: какой клип заноса пошёл (журнал sfxlog, гейт tests/hover/carsnd.sh)
      e->skid = enginePlay(e, res, amount, SKID_PITCH(amount)); e->skidRes = res;
    } else audioSet(e->skid, clampf(amount, 0, 1), 0, SKID_PITCH(amount));
  } else if (e->skid) { audioStop(e->skid); e->skid = 0; }
}
void audioSkid(float amount, int dirt) { engineSkid(&E, car, amount, dirt); }
#ifndef NPC_CAR_SKID_OLD
#define NPC_CAR_SKID_OLD 0
#endif
#ifndef NPC_SKID_SV_OLD
#define NPC_SKID_SV_OLD 0   // сверка g08-sound (P10 Д2): у трафика и ИИ миссий звука шин нет вообще; 1 — прежнее (занос у трёх ближайших)
#endif
void audioWorldSkid(int slot, float amount, int dirt) {
  if ((unsigned)slot >= AUDIO_WORLD_ENGINES) return;
  if (!NPC_SKID_SV_OLD) { engineSkid(&worldEngine[slot], NULL, 0.f, dirt); return; }
  const SabCar *profile = NULL;
  if (!NPC_CAR_SKID_OLD && H && worldSource[slot] >= 0 &&
      worldProfile[slot] >= 0 && worldProfile[slot] < (int)H->ncar)
    profile = (const SabCar *)(bank + H->carOff) + worldProfile[slot];
  engineSkid(&worldEngine[slot], profile, amount, dirt);
}

void audioStats(AudioStats *s) {
  memset(s, 0, sizeof *s);
  s->cpu = statCpu; s->mixUs = statUs; s->mixUsMax = statUsMax;
  for (int k = 0; k < NVOICE + NSTREAM; k++) s->voices += voices[k].state != V_FREE;
  for (int k = 0; k < NVOICE; k++) s->loops += voices[k].state != V_FREE && voices[k].loop && !voices[k].stopReq;   // №1174
  s->underruns = underruns; s->dropped = dropped;
  for (int i = 0; i < NSTREAM; i++) {
    Stream *st = &streams[i];
    s->fill[i] = st->state == S_PLAY ? (int)((st->wr - st->rd) * 100ull / (st->mask + 1)) : 0;
    s->track[i] = st->state == S_PLAY ? st->cur : -1;
    s->trackPos[i] = (st->state == S_PLAY && trackRow(st->cur)) ? strmSi[i] / (float)trackRow(st->cur)->rate : 0;   // правка 276: позиция после буфера
  }
  s->ioBytes = ioBytes; s->ioMaxMs = ioMaxMs;
  s->gear = E.gear; s->enginePitch = E.pitch; s->engineVol = E.env;
  s->limiter = limGain; s->limiterBuffers = limActive;
}
