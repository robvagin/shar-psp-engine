// Цена на главном потоке: вся логика мировых голосов за кадр (хост-стенд на Маке, настоящий пакет уровня). Подключает audio.c; микшер не крутится (он считает на ME/в потоке звука),
// мерится то, что делает главный поток в кадре: мотор игрока (модель, 3 голоса), 5 слотов трафика и 5 слотов ИИ миссий (audioSvEngine: выбор, дистанция до слушателя,
// высота, положение голоса), сама передача положения слушателя. Пересчёт на PSP: ×5,8 (навык shar-psp-perf-truth).
//   cost PACKDIR/ [кадров]
#include "../../viewer/audio.c"
#include <time.h>
static double wallUs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec * 1e-3; }
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static void stat(const char *n, double *v, int N) {
  double s = 0; for (int i = 0; i < N; i++) s += v[i]; qsort(v, N, sizeof *v, cmpd);
  printf("%-34s среднее %6.2f мкс, p99 %6.2f, макс %7.2f   (PSP ×5,8: среднее %6.1f мкс, p99 %6.1f)\n", n, s / N, v[(int)(N * .99)], v[N - 1], s / N * 5.8, v[(int)(N * .99)] * 5.8);
}
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  int N = argc > 2 ? atoi(argv[2]) : 20000;
  if (audioInit(argv[1]) < 0) { printf("audioInit не удался\n"); return 2; }
  audioPlayerCar("famil_v");
  int ready = audioSvReady(); printf("мотор мира по новой схеме: %s\n", ready ? "да" : "нет (клипов нет в пакете)");
  const float TOP = 33.f * 3.6f;
  // настоящий ход: каждый кадр игры (33 мс) — три буфера микшера (как идёт звук), между ними замеряются секции главного потока. Часы сами стоят ~40 нс — калибровка вычитается
  static short mb[512 * 2]; double sL = 0, sE = 0, sW = 0, ck = 0; int nck = 100000;
  for (int i = 0; i < nck; i++) { double a = wallUs(), b = wallUs(); ck += b - a; } ck /= nck;
  for (int f = 0; f < N; f++) {
    float t = f * 0.0333f, v = 20.f + 15.f * sinf(t * .3f);
    for (int k = 0; k < 3; k++) audioHostMix(mb, 512);
    double a0 = wallUs();
    audioListener(40.f * sinf(t * .1f), 2.f, 40.f * cosf(t * .1f), cosf(t * .1f), -sinf(t * .1f));
    double a1 = wallUs();
    audioEngineEx(0.f, 0, 0, 0, 1.f); audioEngine(v * 3.6f, TOP, .8f, AUDIO_ENGINE_ON);
    double a2 = wallUs();
    for (int s = 0; s < AUDIO_SV_SLOTS; s++) {
      float ph = t * .2f + s * 1.3f, x = 60.f * sinf(ph), z = 60.f * cosf(ph), sp = 8.f + 6.f * sinf(t * .5f + s);
      audioSvEngine(0, s, 1, -1, x, 0.f, z, sp, 16.7f, .8f + .1f * s, .0333f);
      audioSvEngine(1, s, 1, -1, -z, 0.f, x, sp, 25.f, 1.f, .0333f);
    }
    double a3 = wallUs();
    sL += a1 - a0 - ck; sE += a2 - a1 - ck; sW += a3 - a2 - ck;
  }
  double tLis = sL / N, tEng = sE / N, tW = sW / N, tEmpty = 0;
  { int on = 0; for (int i = 0; i < AUDIO_SV_SLOTS; i++) on += (svT[i].h != 0) + (svA[i].h != 0); AudioStats st; audioStats(&st);
    printf("проверка, что работа идёт: голосов мира %d из 10, высота мотора игрока %.2f, передача %d, car=%p\n", on, st.enginePitch, st.gear, (void *)car); }
  printf("кадров %d, голосов у главного в конце: %d\n", N, ({ AudioStats st; audioStats(&st); st.voices; }));
  double all = tLis + tEng + tW;
  printf("на кадр (среднее по %d кадрам, холостой цикл с синусами вычтен %.3f мкс):\n", N, tEmpty);
  printf("  слушатель                        %7.3f мкс   (PSP ×5,8 %6.2f)\n  мотор игрока (модель, 3 голоса)  %7.3f мкс   (PSP %6.2f)\n  слоты трафика и ИИ, 10 голосов   %7.3f мкс   (PSP %6.2f)\n  ВСЁ                              %7.3f мкс   (PSP %6.2f мкс = %.3f мс)\n",
         tLis - tEmpty, (tLis - tEmpty) * 5.8, tEng - tEmpty, (tEng - tEmpty) * 5.8, tW - tEmpty, (tW - tEmpty) * 5.8, all, all * 5.8, all * 5.8 / 1000);
  return 0;
}
