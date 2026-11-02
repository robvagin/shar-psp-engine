// №1100: из чего состоит цена звука на главном потоке за буфер (11,6 мс): mixPre (подготовка задания), mixPost (предел громкости и вывод) на Маке с настоящим пакетом уровня,
// 5 потоков и до 10 голосов. Пересчёт на PSP ×5,8 против измеренных в туре TEST82: pre 81, post 139 мкс на буфер. Подключает audio.c.   cost3 PACKDIR/ [буферов]
#include "../../viewer/audio.c"
#include <time.h>
static double wallUs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec * 1e-3; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  int N = argc > 2 ? atoi(argv[2]) : 20000;
  if (audioInit(argv[1]) < 0) return 2;
  audioPlayerCar("famil_v");
  int nt = audioTrackCount(), tm = -1, ta = -1, td = -1, tp = -1;
  for (int t = 0; t < nt; t++) { int b = audioTrackBus(t); if (b == 0 && tm < 0) tm = t; if (b == 1 && ta < 0) ta = t; if (b == 2 && td < 0) td = t; }
  audioMusic(tm, 0); audioAmbience(ta, 0); audioStream(AUDIO_ST_DIALOG, td, 0);
  static short mb[512 * 2];
  int ids[8]; const char *nm[8] = {"fire", "skid", "car_jump", "hydrant_spray", "wasp_idle", "coin_collect_01", "horn_beep", "ferrari"}; int ni = 0;
  for (int i = 0; i < 8; i++) { int id = audioFind(nm[i]); if (id >= 0) ids[ni++] = id; }
  for (int k = 0; k < 400; k++) {   // прогрев: потоки наполняются
    if (k % 20 == 0) for (int i = 0; i < ni; i++) audioPlayAt(ids[i], 1.f * i, 0.f, 3.f, 3.f, 100.f);
    audioEngineEx(0, 0, 0, 0, 1.f); audioEngine(60.f, 120.f, .8f, AUDIO_ENGINE_ON); audioHostMix(mb, 512);
  }
  AudioStats st; audioStats(&st); printf("голосов %d, потоков с данными: %d %d %d %d %d\n", st.voices, st.fill[0], st.fill[1], st.fill[2], st.fill[3], st.fill[4]);
  double ck = 0; for (int i = 0; i < 100000; i++) { double a = wallUs(), b = wallUs(); ck += b - a; } ck /= 100000;
  double sPre = 0, sPost = 0, sRun = 0;
  for (int k = 0; k < N; k++) {
    if (k % 7 == 0) { audioEngineEx(0, 0, 0, 0, 1.f); audioEngine(60.f, 120.f, .8f, AUDIO_ENGINE_ON); }
    for (int s = 0; s < NSTREAM; s++) streamService(&streams[s]);
    double a = wallUs(); mixPre(J); J->data = DATA; double b = wallUs(); mixDspRun(J); wbAll = 1; double c = wallUs(); mixPost(J, mb); double d = wallUs();
    sPre += b - a - ck; sRun += c - b - ck; sPost += d - c - ck;
  }
  printf("на буфер: mixPre %.2f мкс (PSP ×5,8 %.0f; в туре 81), mixPost %.2f мкс (PSP ×5,8 %.0f; в туре 139), счёт голосов %.1f мкс (на ME)\n", sPre / N, sPre / N * 5.8, sPost / N, sPost / N * 5.8, sRun / N);
  return 0;
}
