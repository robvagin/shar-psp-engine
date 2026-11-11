// Mac-side harness: runs viewer/audio.c (the exact PSP mixer code, AUDIO_HOST build) without a PSP.
//   audio_host dump <packdir> <outdir>   raw renders for tools/audio_export.py verify
//   audio_host demo <packdir> <out.wav>  20 s scripted drive (engine through the gears, horn, crash, skid, music)
//   audio_host bench <packdir>           host mixing cost per buffer (for relative comparison only)
// Build: make -C tests/audio/host
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio.h"

#define B 512
static short buf[B * 2];
static FILE *seamf; static unsigned seamBase;
static void hook(unsigned fr) { if (seamf) fprintf(seamf, "%u\n", fr - seamBase); }

static FILE *openw(const char *dir, const char *name) {
  char p[512]; snprintf(p, sizeof p, "%s/%s", dir, name);
  FILE *f = fopen(p, "wb"); if (!f) { perror(p); exit(1); }
  return f;
}
static void mix(void) { audioHostMix(buf, B); }
static void badDump(const char *name) { fprintf(stderr, "dump incomplete: %s\n", name); exit(1); }

static void dump(const char *pack, const char *out) {
  AudioStats st;
  // 1) bit-exact: every resident file, one pass, unity gain/step
  audioTestForceUnity(1);
  int nf = audioTestFileCount();
  for (int i = 0; i < nf; i++) {
    int ch, rate; unsigned ns; audioTestFileInfo(i, &ch, &rate, &ns);
    char name[64]; snprintf(name, sizeof name, "sfx_%03d.raw", i);
    FILE *f = openw(out, name);
    audioTestPlayFile(i);
    unsigned limit = (ns + B - 1) / B + 128;
    for (unsigned guard = 0; guard < limit; guard++) {
      mix();
      for (int k = 0; k < B; k++) fwrite(&buf[k * 2], 2, ch == 2 ? 2 : 1, f);
      audioStats(&st); if (!st.voices) break;
    }
    fclose(f);
    if (st.voices) badDump(name);
  }
  // 2) bit-exact: every stream track through the ring buffer + segment queue, one pass
  int nt = audioTrackCount();
  for (int t = 0; t < nt; t++) {
    int loop, next, bus; unsigned ns, ls, le;
    audioTestTrackInfo(t, &loop, &next, &bus, &ns, &ls, &le);
    char name[64]; snprintf(name, sizeof name, "mus_%s.raw", audioTrackName(t));
    FILE *f = openw(out, name);
    audioTestMusicOnce(t);
    int started = 0;
    unsigned limit = (ns + B - 1) / B + 128;
    for (unsigned guard = 0; guard < limit; guard++) {
      mix(); audioStats(&st);
      if (!started && st.track[0] == t) started = 1;
      if (started) fwrite(buf, 4, B, f);
      if (started && st.track[0] < 0) break;
      if (!started && guard == 127) break;
    }
    fclose(f);
    if (!started || st.track[0] >= 0) badDump(name);
  }
  audioTestMusicOnce(-1);
  audioTestForceUnity(0);
  for (int b = 0; b < AUDIO_NBUS; b++) audioBus(b, 1);
  audioMaster(1);
  // 3) through the real 44.1 kHz resampler: first 3 tracks, first 3 one-shot files
  for (int t = 0; t < nt && t < 3; t++) {
    int loop, next, bus; unsigned ns, ls, le;
    audioTestTrackInfo(t, &loop, &next, &bus, &ns, &ls, &le);
    char name[64]; snprintf(name, sizeof name, "mix_%s.raw", audioTrackName(t));
    FILE *f = openw(out, name);
    audioTestMusicOnce(t);
    int started = 0;
    unsigned limit = (unsigned)(audioTrackSeconds(t) * 44100.f / B) + 128;
    for (unsigned guard = 0; guard < limit; guard++) {
      mix(); audioStats(&st);
      if (!started && st.track[0] == t) started = 1;
      if (started) fwrite(buf, 4, B, f);
      if (started && st.track[0] < 0) break;
      if (!started && guard == 127) break;
    }
    fclose(f);
    if (!started || st.track[0] >= 0) badDump(name);
  }
  audioTestMusicOnce(-1);
  for (int i = 0, done = 0; i < nf && done < 3; i++) {
    int ch, rate; unsigned ns; audioTestFileInfo(i, &ch, &rate, &ns);
    char name[64]; snprintf(name, sizeof name, "mix_sfx_%03d.raw", i);
    FILE *f = openw(out, name);
    audioTestPlayFile(i);
    unsigned limit = (unsigned)(ns * (44100.0 / rate) / B) + 128;
    for (unsigned guard = 0; guard < limit; guard++) { mix(); fwrite(buf, 4, B, f); audioStats(&st); if (!st.voices) break; }
    fclose(f); done++;
    if (st.voices) badDump(name);
  }
  // 4) loop seams through the mixer: looping resources at pitch 1.0 and 1.75, looping tracks (+ intro chain)
  audioTestSeamHook = hook;
  int nr = audioSoundCount();
  for (int id = 0; id < nr; id++) {
    if (!audioTestResLoops(id)) continue;
    for (int pi = 0; pi < 2; pi++) {
      float pitch = pi ? 1.75f : 1.f;
      char name[96]; snprintf(name, sizeof name, "loop_%s_p%03d.raw", audioSoundName(id), (int)(pitch * 100));
      FILE *f = openw(out, name);
      snprintf(name, sizeof name, "loop_%s_p%03d.seams", audioSoundName(id), (int)(pitch * 100));
      seamf = openw(out, name); seamBase = audioTestFramesOut();
      int h = audioPlay(id, 1, 0, pitch);
      int n0 = audioTestSeamCount();
      for (int guard = 0; guard < 4000 && audioTestSeamCount() - n0 < 5; guard++) { mix(); fwrite(buf, 4, B, f); }
      for (int k = 0; k < 20; k++) { mix(); fwrite(buf, 4, B, f); }
      audioStop(h); mix(); mix();
      fclose(f); fclose(seamf); seamf = 0;
    }
  }
  for (int t = 0; t < nt; t++) {
    int loop, next, bus; unsigned ns, ls, le;
    audioTestTrackInfo(t, &loop, &next, &bus, &ns, &ls, &le);
    if (!loop) continue;
    int first = t;                                  // start from an intro that chains into this loop, if any
    for (int u = 0; u < nt; u++) { int l2, n2, b2; unsigned a, b_, c; audioTestTrackInfo(u, &l2, &n2, &b2, &a, &b_, &c); if (n2 == t) first = u; }
    char name[96]; snprintf(name, sizeof name, "loop_%s.raw", audioTrackName(t));
    FILE *f = openw(out, name);
    snprintf(name, sizeof name, "loop_%s.seams", audioTrackName(t));
    seamf = openw(out, name);
    int s = bus == 1 ? 1 : 0;
    if (s) audioAmbience(first, 0); else audioMusic(first, 0);
    int started = 0; unsigned want = 0, got = 0;
    for (int guard = 0; guard < 2000000; guard++) {
      mix(); audioStats(&st);
      if (!started && st.track[s] >= 0) {
        started = 1; seamBase = audioTestFramesOut() - B;
        unsigned introS = 0;
        if (first != t) { int l2, n2, b2; unsigned a, b_, c; audioTestTrackInfo(first, &l2, &n2, &b2, &a, &b_, &c); introS = a; }
        want = (unsigned)((introS + (le - ls)) * (44100.0 / 24000.0)) + 44100 * 3;   // one full pass + 3 s
      }
      if (started) { fwrite(buf, 4, B, f); got += B; if (got >= want) break; }
    }
    if (s) audioAmbience(-1, 0); else audioMusic(-1, 0);
    for (int k = 0; k < 8; k++) mix();
    fclose(f); fclose(seamf); seamf = 0;
  }
  audioTestSeamHook = 0;
  audioStats(&st);
  printf("dump done: %d files, %d tracks, underruns %d, dropped %d, seams %d\n", nf, nt, st.underruns, st.dropped,
         audioTestSeamCount());
}


// ---- Звук-96: гейт «банк по требованию». sbtest <pack> [break]
//  1) эталон: банк целиком (audioSbOld), каждый файл в одиночку, хеш смешанного вывода и снимок данных
//  2) режим по требованию, кэш мал: те же файлы в случайном порядке три круга (вытеснения), хеш каждого = эталон
//  3) нагрузка: до 24 голосов сразу случайных ресурсов с вытеснениями; у каждого живого голоса его данные в памяти и байт в байт равны эталону
//  4) память: банк в режиме по требованию заметно меньше целиком; ни одного отказа (fail=0)
//  «break»: загрузка файлов отключена (audioSbBreak) — тест обязан стать красным (код возврата 1), иначе гейт слепой
static unsigned long long fnv(unsigned long long h, const void *p, size_t n) { const unsigned char *q = p; while (n--) { h ^= *q++; h *= 1099511628211ull; } return h; }
static unsigned long long renderFile(int fi, int ch) {
  unsigned long long h = 1469598103934665603ull; AudioStats st; int ns, rate; unsigned nsamp; (void)ns; (void)rate; (void)nsamp;
  if (!audioTestPlayFile(fi)) return 0;
  for (int g = 0; g < 4000; g++) { mix(); h = fnv(h, buf, sizeof buf); audioStats(&st); if (!st.voices) break; }
  (void)ch; return h;
}
static int sbtest(const char *dir, int brk) {
  int bad = 0; char line[256];
  audioShutdown(); audioSbSlotFree(); audioSbBreak(0); audioSbOld(1);
  if (audioInit(dir) < 0) { fprintf(stderr, "init old: %s\n", audioError()); return 1; }
  int nf = audioTestFileCount(); unsigned arenaOld = audioSbArena();
  unsigned long long *ref = calloc(nf, sizeof *ref);
  unsigned dsz = 0; for (int i = 0; i < nf; i++) { unsigned o, b; audioTestFileLoc(i, &o, &b); if (o + b > dsz) dsz = o + b; }
  unsigned char *snap = malloc(dsz); memcpy(snap, audioTestData(), dsz);
  audioTestForceUnity(1);
  for (int i = 0; i < nf; i++) { int ch, rate; unsigned ns; audioTestFileInfo(i, &ch, &rate, &ns); ref[i] = renderFile(i, ch); }
  audioShutdown(); audioSbSlotFree();
  audioSbOld(0); audioSbBreak(0);
  if (audioInit(dir) < 0) { fprintf(stderr, "init new: %s\n", audioError()); return 1; }
  audioSbLine(line, sizeof line); printf("SNDB init %s\n", line);
  if (!strstr(line, "on=1")) { printf("FAIL: режим по требованию не включился\n"); return 1; }
  unsigned arenaNew = audioSbArena();
  printf("банк в памяти: целиком %u КБ, по требованию %u КБ, экономия %.2f МБ\n", arenaOld >> 10, arenaNew >> 10, (arenaOld - arenaNew) / 1048576.0);
  if (arenaNew + (900u << 10) > arenaOld) { printf("FAIL: экономия меньше 0,9 МБ\n"); bad++; }
  audioTestForceUnity(1);
  if (brk) audioSbBreak(1);
  unsigned rng = 12345; int mism = 0;
  for (int round = 0; round < 3; round++) {
    int *ord = malloc(nf * sizeof *ord); for (int i = 0; i < nf; i++) ord[i] = i;
    for (int i = nf - 1; i > 0; i--) { rng = rng * 1664525u + 1013904223u; int j = (rng >> 8) % (i + 1); int t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    for (int k = 0; k < nf; k++) { int fi = ord[k], ch, rate; unsigned ns; audioTestFileInfo(fi, &ch, &rate, &ns);
      unsigned long long h = renderFile(fi, ch); if (h != ref[fi]) { mism++; if (mism < 5) printf("  расхождение: файл %d круг %d\n", fi, round); } }
    free(ord);
  }
  printf("один файл за раз: расхождений с эталоном %d\n", mism); if (mism) bad++;
  // 3) нагрузка: много голосов одновременно
  audioTestForceUnity(0); int nr = audioSoundCount(); int hs[64], nh = 0, memBad = 0, steps = 0, unmet = 0, avoidable = 0; unsigned cacheKb = 0; { const char *q = strstr(line, "cache_kb="); if (q) cacheKb = (unsigned)atoi(q + 9); }
  for (int step = 0; step < 4000; step++) {
    rng = rng * 1664525u + 1013904223u; int id = (rng >> 8) % nr;
    if (nh < 24 && ((rng >> 4) & 3)) {
      int fi0[32]; unsigned dof0[32]; int m0 = audioTestVoiceFiles(fi0, dof0, 32); unsigned live = 0, seen[32]; int ns0 = 0;   // сколько байт кэша держат живые голоса
      for (int k = 0; k < m0; k++) { int dup = 0; for (int q = 0; q < ns0; q++) dup |= seen[q] == (unsigned)fi0[k]; if (dup) continue; seen[ns0++] = fi0[k]; unsigned o, b; audioTestFileLoc(fi0[k], &o, &b); live += (b + 63u) & ~63u; }
      int h = audioPlay(id, 0.5f, 0, 1); if (h) hs[nh++] = h; else if (m0 < 24) { unmet++; if (live + (70u << 10) <= cacheKb * 1024u * 6 / 10) avoidable++; } }
    if (nh && ((rng >> 12) & 7) == 0) { int q = (rng >> 16) % nh; audioStop(hs[q]); hs[q] = hs[--nh]; }
    mix(); steps++;
    int fi[32]; unsigned dof[32]; int m = audioTestVoiceFiles(fi, dof, 32);
    for (int k = 0; k < m; k++) { unsigned o, b; audioTestFileLoc(fi[k], &o, &b);
      if (!audioSbResident(fi[k]) || memcmp(audioTestData() + dof[k], snap + o, b)) { memBad++; if (memBad < 5) printf("  данные голоса не на месте: файл %d шаг %d\n", fi[k], step); } }
  }
  printf("нагрузка: %d шагов, голосов с неверными данными %d, отказов от перегруза %d (из них можно было избежать %d)\n", steps, memBad, unmet, avoidable); if (memBad) bad++;
  // 3б) обычная игра: звуки по одному с паузой (кэш не насыщен): задержка запрошен→звучит не больше двух буферов, ничего не потеряно
  for (int q = 0; q < nh; q++) audioStop(hs[q]); nh = 0; for (int q = 0; q < 40; q++) mix();
  audioSbStatReset(); int lat = 0;
  for (int step = 0; step < 600; step++) {
    rng = rng * 1664525u + 1013904223u; int id = (rng >> 8) % nr;
    if (audioTestResLoops(id)) continue;
    audioPlay(id, 0.5f, 0, 1); for (int q = 0; q < 6; q++) mix();
  }
  { char l2[300]; audioSbLine(l2, sizeof l2); const char *q = strstr(l2, "dly_max_us="); lat = q ? atoi(q + 11) : 0; printf("обычная игра: наибольшая задержка запрошен→звучит %d мкс (%d буфера)\n", lat, (lat + 11609) / 11610); }
  if (lat > 2 * 11610 + 100 && !brk) { printf("FAIL: задержка старта в обычной игре больше двух буферов\n"); bad++; }
  audioSbLine(line, sizeof line); printf("SNDB end %s\n", line);

  { unsigned fail = 0, dly = 0; const char *q = strstr(line, " fail="); if (q) fail = (unsigned)atoi(q + 6); q = strstr(line, "dly_max_us="); if (q) dly = (unsigned)atoi(q + 11);
    printf("отказов (звук не сыгран) %u, наибольшая задержка запрошен→звучит %u мкс\n", fail, dly);
    if (fail && !brk) { printf("FAIL: звук не сыгран из-за кэша (%u)\n", fail); bad++; }
    if (dly > 200000u && !brk) { printf("FAIL: задержка старта в насыщении больше 200 мс\n"); bad++; } }
  if (!strstr(line, "evict=") || !strncmp(strstr(line, "evict=") + 6, "0 ", 2)) { printf("FAIL: вытеснений не было, проверка кэша слепая\n"); bad++; }
  printf(bad ? "SNDBANK96 RED (%d)\n" : "SNDBANK96 GREEN\n", bad);
  free(ref); free(snap); return bad ? 1 : 0;
}

static void wavHeader(FILE *f, unsigned frames) {
  unsigned bytes = frames * 4, v;
  fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  v = 16; fwrite(&v, 4, 1, f); unsigned short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
  v = 44100; fwrite(&v, 4, 1, f); v = 44100 * 4; fwrite(&v, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
}

static void demo(const char *wav) {
  FILE *f = fopen(wav, "wb"); if (!f) { perror(wav); exit(1); }
  wavHeader(f, 0);
  audioMusic(audioTrack("sunday_drive_intro"), 0);
  audioAmbience(audioTrack("amb_suburb_day"), 0);
  int crash = audioFind("medium_car_crash");
  float v = 0, top = 120; unsigned frames = 0; int crashed = 0; AudioStats st;
  for (float t = 0; t < 20.f; t += B / 44100.f) {
    float thr = (t > 1 && t < 9) ? 1.f : 0.f, dt = B / 44100.f;
    unsigned fl = AUDIO_ENGINE_ON;
    if (t >= 14 && t < 15) v -= 60 * dt;                              // brake
    else if (t >= 15 && t < 17.5f) { thr = 0.6f; fl |= AUDIO_ENGINE_REVERSE; v = v > -20 ? v - 15 * dt : v; }
    else v += (thr * 22.f * (1 - v / top) - (v > 0 ? 3.f : 0)) * dt;
    if (t >= 14 && v < 0 && t < 15) v = 0;
    if (t > 17.5f) { thr = 0; v *= 0.95f; }
    audioEngine(v * 1.f, top, thr, fl);
    audioHorn(t > 10 && t < 10.8f);
    if (t > 12 && !crashed) { audioPlay(crash, 1, 0.3f, 1); crashed = 1; }
    audioSkid(t > 12.5f && t < 13.5f ? 0.8f : 0, 0);
    mix(); fwrite(buf, 4, B, f); frames += B;
    audioStats(&st);
    if (((int)(t * 44100) / B) % 43 == 0)
      printf("t %5.2f  speed %6.1f  gear %2d  pitch %.2f  env %.2f  voices %d  music %s\n", t, v, st.gear, st.enginePitch,
             st.engineVol, st.voices, st.track[0] >= 0 ? audioTrackName(st.track[0]) : "-");
  }
  fseek(f, 0, SEEK_SET); wavHeader(f, frames); fclose(f);
  printf("wrote %s (%.1f s)\n", wav, frames / 44100.f);
}

static void bench(void) {
  audioMusic(audioTrack("sunday_drive_intro"), 0);
  audioAmbience(audioTrack("amb_suburb_day"), 0);
  int ids[6] = {audioFind("skid"), audioFind("famil_horn"), audioFind("fire"), audioFind("dirt_skid"), audioFind("horn"), audioFind("car_jump")};
  for (int k = 0; k < 6; k++) audioPlay(ids[k], 0.5f, 0, 1.3f);
  audioEngine(60, 120, 1, AUDIO_ENGINE_ON);
  for (int k = 0; k < 20; k++) mix();
  AudioStats st; audioStats(&st);
  double sum = 0; int n = 2000;
  for (int k = 0; k < n; k++) { mix(); audioStats(&st); sum += st.mixUs; }
  printf("host: %d voices, %.1f us per %d-frame mix (%.3f%% of real time on this Mac)\n", st.voices, sum / n, B,
         sum / n / (B * 1e6 / 44100) * 100);
}

int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "usage: %s dump|demo|bench <packdir/> [out]\n", argv[0]); return 2; }
  char dir[512]; snprintf(dir, sizeof dir, "%s%s", argv[2], argv[2][strlen(argv[2]) - 1] == '/' ? "" : "/");
  if (audioInit(dir) < 0) { fprintf(stderr, "audioInit: %s\n", audioError()); return 1; }
  if (!strcmp(argv[1], "dump") && argc > 3) dump(dir, argv[3]);
  else if (!strcmp(argv[1], "demo") && argc > 3) demo(argv[3]);
  else if (!strcmp(argv[1], "bench")) bench();
  else if (!strcmp(argv[1], "sbinfo")) { char l[256]; audioSbLine(l, sizeof l); printf("%s arena_kb=%u\n", l, audioSbArena() >> 10); }
  else if (!strcmp(argv[1], "sbtest")) return sbtest(dir, argc > 3 && !strcmp(argv[3], "break"));
  else { fprintf(stderr, "bad args\n"); return 2; }
  return 0;
}
