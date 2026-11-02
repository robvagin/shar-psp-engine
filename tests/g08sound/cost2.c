// Цена на главном потоке: игровой слой звука за кадр (sndFrame: шины режимов, ворота атмосферы, излучатели, очередь реплик; sndCarState; sndEngine) на настоящем пакете уровня.
// Игрок едет по кругу 120 м через все ворота зон и излучатели; микшер идёт между кадрами. Пересчёт на PSP ×5,8.   cost2 PACKDIR/ [кадров]
#include "../../viewer/sound.c"
#include <time.h>
static double wallUs(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec * 1e-3; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  int N = argc > 2 ? atoi(argv[2]) : 6000;
  if (sndInit(argv[1]) < 0) { printf("sndInit: %s\n", sndError()); return 2; }
  sndUserVolumes(1, 1, 1, 1); sndSeed(7); sndLevelStart(0, 0, 0); sndCarSetup(33.f); sndPlayerCar("famil_v"); sndOnFoot(0); sndMode(SND_MODE_NORMAL);
  static short mb[512 * 2]; double ck = 0, sF = 0, sC = 0, sMax = 0; int nck = 100000;
  for (int i = 0; i < nck; i++) { double a = wallUs(), b = wallUs(); ck += b - a; } ck /= nck;
  for (int f = 0; f < N; f++) {
    float t = f * 0.0333f, px = 120.f * sinf(t * .05f), pz = 120.f * cosf(t * .05f), v = 20.f + 12.f * sinf(t * .3f);
    for (int k = 0; k < 3; k++) audioHostMix(mb, 512);
    audioListener(px, 2.f, pz, 1.f, 0.f);
    double a0 = wallUs();
    sndCarState(33.f, 0.f, 0, 0, 1.f, 0, 0); sndEngine(v, .8f, SND_ENG_ON);
    double a1 = wallUs();
    sndFrame(.0333f, px, 3.f, pz, 1.f, 0.f, px, 0.f, pz);
    double a2 = wallUs();
    sC += a1 - a0 - ck; sF += a2 - a1 - ck; if (a2 - a1 - ck > sMax) sMax = a2 - a1 - ck;
  }
  printf("зон %u, излучателей %u, режимов %u\n", nzone, nemit, nmode);
  printf("  sndCarState + sndEngine   %7.3f мкс на кадр  (PSP ×5,8 %6.2f)\n  sndFrame                  %7.3f мкс на кадр  (PSP %6.2f), худший кадр %.2f (PSP %.1f)\n",
         sC / N, sC / N * 5.8, sF / N, sF / N * 5.8, sMax, sMax * 5.8);
  return 0;
}
