// SHAR PSP: проигрыватель роликов SMV1 (viewer/movie.h; формат и конвертер — tools/movie_export.py).
// Потоки: "movie_io" 0x18 — читает куски подряд в кольцо из MV_SLOTS ячеек (ячейку освобождают и кадр, и звук);
// "movie_audio" 0x12 — PCM кусков в свой канал sceAudio блоками по 1024 сэмпла (часы). Главный поток: разжатие
// нужного кадра sceJpeg (выход RGBA в своём буфере, шаг строки — по метке MV_SENT) и спрайт 480x272 1:1.
#include <string.h>
#include <stdlib.h>
#include "movie.h"
#ifndef MOVIE_HOST
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspjpeg.h>
#include <psputility.h>
#include <pspgu.h>
#include <malloc.h>
#include "fpuguard.h"   // правка 252: ловушки FPU потоков ролика выключены (viewer/fpuguard.h)
#include <stdio.h>
#include "audio.h"      // правка 276: audioMeHold — пока открыт sceJpeg (кадры считает ME), звук на ME не идёт
#endif

#define MV_W 480
#define MV_H 272
#define MV_SPF 1764                   // 44100 / 25: сэмплов звука на кадр
#define MV_MAXCHUNK (256u << 10)

static unsigned rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static unsigned mvCrc(const unsigned char *p, unsigned n) {   // = zlib.crc32
  static unsigned t[256];
  if (!t[1]) for (unsigned i = 0; i < 256; i++) { unsigned c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320u ^ (c >> 1) : c >> 1; t[i] = c; }
  unsigned c = ~0u; while (n--) c = t[(c ^ *p++) & 255] ^ (c >> 8);
  return ~c;
}
int mvHeader(const unsigned char *h, unsigned fileSize, MvHdr *o) {
  if (memcmp(h, "SMV1", 4) || mvCrc(h, 60) != rd32(h + 60)) return -1;
  if (rd16(h + 4) != MV_W || rd16(h + 6) != MV_H || rd16(h + 8) != 25 || rd16(h + 10) != 1 || rd32(h + 16) != 44100 ||
      rd16(h + 20) != 2 || rd16(h + 22) != MV_SPF) return -1;
  o->frames = rd32(h + 12); o->total = rd32(h + 24); o->maxChunk = rd32(h + 28); o->idxOff = rd32(h + 32);
  o->dataOff = rd32(h + 36); o->flags = rd32(h + 48); o->size = rd32(h + 52); o->icrc = rd32(h + 56);
  if (!o->frames || o->frames > (1u << 20) || o->maxChunk < 16 || o->maxChunk > MV_MAXCHUNK || o->idxOff != 64 ||
      o->dataOff != 64 + 4 * (o->frames + 1) || o->size != fileSize || o->dataOff >= fileSize ||
      o->total != ((o->flags & 1) ? o->frames * MV_SPF : 0)) return -1;
  return 0;
}
int mvIndex(const MvHdr *o, const unsigned char *idx) {
  if (mvCrc(idx, 4 * (o->frames + 1)) != o->icrc || rd32(idx) != o->dataOff || rd32(idx + 4 * o->frames) != o->size) return -1;
  for (unsigned i = 0; i < o->frames; i++) {
    unsigned a = rd32(idx + 4 * i), b = rd32(idx + 4 * i + 4);
    if (b <= a + 12 || b - a > o->maxChunk) return -1;
  }
  return 0;
}
int mvChunk(const MvHdr *o, const unsigned char *c, unsigned len, unsigned *jl, unsigned *pcm) {
  if (len < 12) return -1;
  unsigned j = rd32(c), p = rd32(c + 4);
  if (j < 4 || j > len || p != ((o->flags & 1) ? MV_SPF : 0) || 8 + ((j + 3) & ~3u) + 4 * p != len) return -1;
  if (c[8] != 0xff || c[9] != 0xd8 || c[8 + j - 2] != 0xff || c[8 + j - 1] != 0xd9) return -1;   // SOI ... EOI
  *jl = j; *pcm = p; return 0;
}

#ifndef MOVIE_HOST
// сверка g09b-menu круг 2 (g11b-collect, M3:4): громкость звука ролика = ползунок голоса (оригинал); 0..32767 (PSP_AUDIO_VOLUME_MAX). Выставляет вызывающий перед movieOpen
// (main_frontend.h movieVolSync). MOVIEVOL_OLD=1 — звук ролика всегда на максимуме, как было
#ifndef MOVIEVOL_OLD
#define MOVIEVOL_OLD 0
#endif
int movieVolume = PSP_AUDIO_VOLUME_MAX;
#define MV_SLOTS 16                   // кусков в кольце: 0,64 с вперёд (intro: наибольший кусок 62 КБ -> ~1 МБ)
#define MV_ABLK 1024                  // сэмплов на sceAudioOutputBlocking: 23 мс
#define MV_PRE 8                      // кусков в запасе до старта звука
#define MV_SENT 0x5a17c0deu           // метка буфера кадра: осталась в (480,0) -> шаг строки 512, иначе 480
#define MV_RGBA (512 * MV_H * 4)

struct Movie {
  short abuf[2][MV_ABLK * 2];         // первым: выравнивание 64 (memalign) для sceAudio
  MvHdr h; unsigned char *idx, *ring; unsigned *rgba;
  SceUID fd, rth, ath; int ch;
  volatile int rNext, vNext, aChunk, stop, err, paused, rDone, aOn;
  volatile unsigned aOff, aPos;       // aPos — сэмплов звука отдано в канал (часы)
  volatile int slot[MV_SLOTS];        // какой кусок лежит в ячейке (-1 — пишется/пусто)
  int cur, stride; unsigned lastDec, t0, pauseT, pausedUs, rBytes, rUs;
  MovieStats st;
};
static int mvJpeg, mvAv;              // sceJpeg открыт; avcodec загружен нами
static unsigned mvNow(void) { return sceKernelGetSystemTimeLow(); }
static unsigned char *mvSlot(const Movie *m, int i) { return m->ring + (unsigned)(i % MV_SLOTS) * m->h.maxChunk; }

int movieSystem(int on) {
  if (on) {
    if (mvJpeg) return 0;
    audioMeHold(1);   // правка 276: с флаг-файлом meaudio — дождаться, что ME звуком не занят (без файла — сразу)
    mvAv = sceUtilityLoadAvModule(PSP_AV_MODULE_AVCODEC) >= 0;   // уже загружен кем-то — тоже годится
    int r = sceJpegInitMJpeg();
    if (r >= 0) { r = sceJpegCreateMJpeg(MV_W, MV_H); if (r < 0) sceJpegFinishMJpeg(); }
    if (r < 0) { if (mvAv) sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC); mvAv = 0; audioMeHold(0); return r; }
    mvJpeg = 1; return 0;
  }
  if (mvJpeg) { sceJpegDeleteMJpeg(); sceJpegFinishMJpeg(); mvJpeg = 0; }
  if (mvAv) { sceUtilityUnloadAvModule(PSP_AV_MODULE_AVCODEC); mvAv = 0; }
  audioMeHold(0);   // правка 276
  return 0;
}

static int mvReader(SceSize a, void *p) {
  Movie *m = *(Movie **)p; (void)a; fpuThreadInit();   // правка 252
  while (!m->stop && m->rNext < (int)m->h.frames) {
    int i = m->rNext, lo = m->vNext < m->aChunk ? m->vNext : m->aChunk;
    if (i - lo >= MV_SLOTS) { sceKernelDelayThread(2000); continue; }   // кольцо полно: ждём кадр и звук
    unsigned off = rd32(m->idx + 4 * i), len = rd32(m->idx + 4 * i + 4) - off, jl, pl, t = mvNow();
    unsigned char *d = mvSlot(m, i);
    m->slot[i % MV_SLOTS] = -1; __asm__ volatile("" ::: "memory");
    if (sceIoRead(m->fd, d, len) != (int)len || mvChunk(&m->h, d, len, &jl, &pl)) { m->err = MV_EREAD; break; }
    t = mvNow() - t; m->rUs += t; m->rBytes += len; if (t > m->st.readMsMax * 1000.f) m->st.readMsMax = t / 1000.f;
    __asm__ volatile("" ::: "memory"); m->slot[i % MV_SLOTS] = i; m->rNext = i + 1;
  }
  m->rDone = 1; return 0;
}

static int mvAudio(SceSize a, void *p) {
  Movie *m = *(Movie **)p; int cur = 0; (void)a; fpuThreadInit();   // правка 252
  while (!m->stop) {
    fpuThreadPoll(FPU_T_MOVIE);
    short *o = m->abuf[cur]; unsigned n = 0;
    while (!m->paused && n < MV_ABLK && m->aChunk < (int)m->h.frames) {
      int c = m->aChunk;
      if (m->slot[c % MV_SLOTS] != c) { m->st.underruns++; break; }   // кусок не прочитан: тишина, часы стоят
      const unsigned char *s = mvSlot(m, c); unsigned jl = rd32(s), pl = rd32(s + 4), k = pl - m->aOff;
      if (k > MV_ABLK - n) k = MV_ABLK - n;
      memcpy(o + 2 * n, s + 8 + ((jl + 3) & ~3u) + 4 * m->aOff, 4 * k); n += k;
      if ((m->aOff += k) >= pl) { m->aOff = 0; __asm__ volatile("" ::: "memory"); m->aChunk = c + 1; }
    }
    if (n < MV_ABLK) memset(o + 2 * n, 0, 4 * (MV_ABLK - n));
    sceKernelDcacheWritebackRange(o, sizeof m->abuf[0]);
    sceAudioOutputBlocking(m->ch, MOVIEVOL_OLD ? PSP_AUDIO_VOLUME_MAX : movieVolume, o);
    m->aPos += n; cur ^= 1;
  }
  return 0;
}

void movieClose(Movie *m, MovieStats *st) {
  if (!m) return;
  m->stop = 1;
  SceUID th[2] = {m->ath, m->rth};
  for (int k = 0; k < 2; k++) if (th[k] >= 0) {
    SceUInt to = 1000000;
    if (sceKernelWaitThreadEnd(th[k], &to) < 0) sceKernelTerminateDeleteThread(th[k]); else sceKernelDeleteThread(th[k]);
  }
  if (m->ch >= 0) { for (int k = 0; k < 50 && sceAudioGetChannelRestLen(m->ch) > 0; k++) sceKernelDelayThread(2000); sceAudioChRelease(m->ch); }
  if (m->fd >= 0) sceIoClose(m->fd);
  if (st) {
    *st = m->st; st->frames = m->h.frames; st->audio = m->aPos; st->stride = m->stride; st->err = m->err;
    st->readKBs = m->rUs ? m->rBytes / 1024.f / (m->rUs / 1e6f) : 0.f;
  }
  free(m->idx); free(m->rgba);
#ifndef MOVIE_TEST_LEAK   // гейт кучи красным: сборка с -DMOVIE_TEST_LEAK теряет кольцо чтения (tools/intro_check.py (4))
  free(m->ring);
#endif
  free(m);
}

static Movie *movieOpen1(const char *fn, int *errOut) {
  unsigned char h[64]; MvHdr hd; int e = MV_EFILE; Movie *m = 0;
  SceUID fd = sceIoOpen(fn, PSP_O_RDONLY, 0);
  if (fd < 0) goto fail;
  unsigned size = (unsigned)sceIoLseek32(fd, 0, PSP_SEEK_END); sceIoLseek32(fd, 0, PSP_SEEK_SET);
  e = MV_EHDR; if (sceIoRead(fd, h, 64) != 64 || mvHeader(h, size, &hd)) { sceIoClose(fd); goto fail; }
  e = MV_EDEC; if (!mvJpeg) { sceIoClose(fd); goto fail; }
  e = MV_EMEM; if (!(m = memalign(64, sizeof *m))) { sceIoClose(fd); goto fail; }
  memset(m, 0, sizeof *m); m->h = hd; m->fd = fd; m->rth = m->ath = m->ch = -1; m->cur = -1;
  for (int i = 0; i < MV_SLOTS; i++) m->slot[i] = -1;
  m->aChunk = (hd.flags & 1) ? 0 : 1 << 30;   // без звука кольцо держит только кадр
  unsigned il = 4 * (hd.frames + 1);
  m->idx = malloc(il); m->ring = memalign(64, MV_SLOTS * hd.maxChunk); m->rgba = memalign(64, MV_RGBA);
  if (!m->idx || !m->ring || !m->rgba) goto close;
  e = MV_EHDR; if (sceIoRead(fd, m->idx, il) != (int)il || mvIndex(&m->h, m->idx)) goto close;
  for (unsigned i = 0; i < MV_RGBA / 4; i++) m->rgba[i] = MV_SENT;
  sceKernelDcacheWritebackInvalidateRange(m->rgba, MV_RGBA);   // метка в памяти, в кэше ничего: ME пишет мимо него
  e = MV_ETHREAD;
  if ((m->rth = sceKernelCreateThread("movie_io", mvReader, 0x18, 0x2000, 0, 0)) < 0) goto close;
  sceKernelStartThread(m->rth, sizeof m, &m);
  int pre = hd.frames < MV_PRE ? (int)hd.frames : MV_PRE;
  for (unsigned t = mvNow(); m->rNext < pre && !m->rDone && mvNow() - t < 3000000; ) sceKernelDelayThread(2000);
  e = MV_EREAD; if (m->err || m->rNext < pre) goto close;
  if (hd.flags & 1) {
    e = MV_EAUDIO;
    if ((m->ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, MV_ABLK, PSP_AUDIO_FORMAT_STEREO)) < 0) goto close;
    if ((m->ath = sceKernelCreateThread("movie_audio", mvAudio, 0x12, 0x2000, 0, 0)) < 0) goto close;
    m->aOn = 1; sceKernelStartThread(m->ath, sizeof m, &m);
  }
  m->t0 = mvNow(); if (errOut) *errOut = MV_OK;
  return m;
close:
  movieClose(m, 0);
fail:
  if (errOut) *errOut = e;
  return 0;
}

#ifndef MOVIERETRY1206_OLD
#define MOVIERETRY1206_OLD 0   // 1 — ролик открывается один раз (как в TEST89)
#endif
void (*movieNote)(const char *);   // №1206: строка о повторе (ящик и журнал) — задаёт main
// №1206 (все переходы надёжно): сбой чтения при открытии ролика (заголовок, индекс, первые куски, поток, память) — до 2 повторов с паузой 250 мс и ПОВТОРНЫМ открытием файла;
// потом, как раньше, ролик не играет (msnMovie пропускает его), а не виснет. Файла нет совсем — повтора нет.
Movie *movieOpen(const char *fn, int *errOut) {
  int e = MV_OK; Movie *m = movieOpen1(fn, &e);
  if (!MOVIERETRY1206_OLD) for (int k = 1; !m && k <= 2; k++) {
    SceIoStat st; if (e == MV_EFILE && sceIoGetstat(fn, &st) < 0) break;   // файла нет — повтор не поможет
    if (e == MV_EDEC) break;                                                 // нет декодера (модуль прошивки) — повтор не поможет
    char l[120]; snprintf(l, sizeof l, "MOVIE retry %d err=%d name=%.50s\n", k, e, fn); if (movieNote) movieNote(l);
    sceKernelDelayThread(250000);
    m = movieOpen1(fn, &e);
  }
  if (errOut) *errOut = m ? MV_OK : e;
  return m;
}

static unsigned mvClock(const Movie *m) {   // сэмплы от начала ролика
  if (m->aOn) { unsigned a = m->aPos; return a > MV_ABLK ? a - MV_ABLK : 0; }   // минус блок в очереди канала
  unsigned us = (m->paused ? m->pauseT : mvNow()) - m->t0 - m->pausedUs;
  return (unsigned)((unsigned long long)us * 44100u / 1000000u);
}

int movieUpdate(Movie *m) {
  if (m->err) return 0;
  if (m->paused) return 1;
  int n = m->h.frames, due = mvClock(m) / MV_SPF, f = m->rNext - 1;
  if (due >= n) due = n - 1;
  if (f > due) f = due;
  if (f > m->cur) {   // новейший прочитанный кадр, который уже пора показать (опоздавшие пропускаем)
    unsigned char *c = mvSlot(m, f); unsigned jl = rd32(c), t = mvNow();
    sceKernelDcacheWritebackRange(c + 8, jl);   // ME читает JPEG из памяти
    int r = sceJpegDecodeMJpeg(c + 8, jl, (u8 *)m->rgba, 0);
    t = mvNow() - t; if (t > m->st.decMsMax * 1000.f) m->st.decMsMax = t / 1000.f;
    if (r < 0 || (r >> 16) != MV_W || (r & 0xffff) != MV_H) { m->err = MV_EDEC; return 0; }
    if (!m->stride) m->stride = ((volatile unsigned *)((unsigned)m->rgba | 0x40000000u))[MV_W] == MV_SENT ? 512 : MV_W;
    m->st.dropped += f - m->cur - 1; m->st.shown++; m->cur = f; m->lastDec = mvNow();
    __asm__ volatile("" ::: "memory"); m->vNext = f + 1;
  } else if (due > m->cur) m->st.late++;
  // конец: последний кадр отстоял свои 40 мс и звук весь ушёл в канал
  return !(m->cur == n - 1 && (!m->aOn || m->aChunk >= n) && mvNow() - m->lastDec >= 40000);
}

int movieFrame(const Movie *m) { return m->cur; }

void moviePause(Movie *m, int on) {
  if (on && !m->paused) { m->pauseT = mvNow(); m->paused = 1; }
  else if (!on && m->paused) { m->pausedUs += mvNow() - m->pauseT; m->paused = 0; }
}

typedef struct { unsigned short u, v; short x, y, z; } MvVert;
void movieDraw(const Movie *m) {
  sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_BLEND); sceGuDisable(GU_ALPHA_TEST); sceGuDisable(GU_FOG);
  sceGuClearColor(0xff000000); sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
  if (m->cur < 0 || !m->stride) return;
  sceGuEnable(GU_TEXTURE_2D); sceGuTexMode(GU_PSM_8888, 0, 0, 0); sceGuTexImage(0, 512, 512, m->stride, m->rgba);
  sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB); sceGuTexFilter(GU_NEAREST, GU_NEAREST); sceGuTexWrap(GU_CLAMP, GU_CLAMP);
  sceGuTexFlush();
  for (int x = 0; x < MV_W; x += 64) {   // полосами по 64 px — кэш текстур
    MvVert *v = (MvVert *)sceGuGetMemory(2 * sizeof(MvVert));
    v[0] = (MvVert){(unsigned short)x, 0, (short)x, 0, 0}; v[1] = (MvVert){(unsigned short)(x + 64), MV_H, (short)(x + 64), MV_H, 0};
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, v);
  }
  sceGuDisable(GU_TEXTURE_2D); sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA); sceGuTexWrap(GU_REPEAT, GU_REPEAT);
}
#endif
