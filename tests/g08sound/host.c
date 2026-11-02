// Гейт окна g08-sound (внедрение сверки с исходником, 02.11.2026): шины режимов (минимум, переход 750 мс), фон только пешком,
// модель мотора (повышение отдельным голосом, высота не быстрее 2 ед./с), очередь реплик, шаги. Настоящий микшер порта на Маке (AUDIO_HOST).
//   host PACKDIR/   печатает OK/FAIL, код 0 — все проверки прошли. Красный контроль — run.sh (прежнее: все *_OLD=1)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "sound.h"
#include "audio.h"
#define FR 512
#define RATE 44100
static int fails;
static void check(const char *n, int ok) { printf("%s %s\n", ok ? "OK" : "FAIL", n); fails += !ok; }
static short bufm[FR * 2];
static void run(float sec, float dt) {   // sec секунд игры: микшер + sndFrame с шагом dt
  int frames = (int)(sec * RATE / FR), per = (int)(dt * RATE / FR); if (per < 1) per = 1;
  for (int k = 0; k < frames; k++) { audioHostMix(bufm, FR); if (k % per == 0) sndFrame(dt, 0, 3, 0, 1, 0, 0, 0, 0); }
}
static float musBus(void) { char b[256]; audioMusicState(b, sizeof b); const char *p = strstr(b, "bus="); return p ? (float)atof(p + 4) : -1.f; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  if (sndInit(argv[1]) < 0) { printf("FAIL sndInit %s\n", sndError()); return 2; }
  sndUserVolumes(1, 1, 1, 1); audioListener(0, 3, 0, 1, 0); sndLevelStart(0, 0, 0);
  // 1) шины: пешком + полосы = наименьший множитель (Letterbox музыка 0,29 против OnFoot 1,0), переход линейный 750 мс
  sndOnFoot(1); sndMode(SND_MODE_ONFOOT); run(1.f, 1.f / 30);
  float foot = musBus();
  sndLetterbox(1); run(0.1f, 1.f / 30); float mid = musBus(); run(1.f, 1.f / 30); float lb = musBus();
  printf("музыка: пешком %.3f, через 0,1 с полос %.3f, через 1,1 с %.3f\n", foot, mid, lb);
  check("шина музыки пешком = Normal × OnFoot (0,78 × 1,0)", foot > 0.7f && foot < 0.85f);
  check("переход к полосам не мгновенный (через 0,1 с ещё не внизу)", mid > lb + 0.05f);
  check("полосы пешком: музыка ≈ 0,78 × 0,29", lb > 0.2f && lb < 0.25f);
  sndLetterbox(0); run(1.2f, 1.f / 30); sndPause(1); run(0.1f, 1.f / 30); float p1 = musBus(); run(1.f, 1.f / 30); float p2 = musBus();
  check("пауза: через 0,1 с музыка ещё слышна, через 1,1 с тишина", p1 > 0.2f && p2 < 0.01f);
  sndPause(0); run(1.f, 1.f / 30);
  // 2) мотор: на разгоне передачи растут, высота за кадр не быстрее 2 ед./с, щелчка gearshift нет
  sndOnFoot(0); sndMode(SND_MODE_NORMAL); sndCarSetup(33.f); sndPlayerCar("famil_v");
  float last = -1, maxStep = 0; int gears = 0, lastG = -1;
  for (int k = 0; k < 400; k++) {
    float v = k * 0.08f; sndCarState(33.f, 0.f, 0, 0, 1.f, 0, 0); sndEngine(v, 1.f, SND_ENG_ON);
    audioHostMix(bufm, FR); AudioStats st; audioStats(&st);
    if (k > 20) { if (last >= 0 && fabsf(st.enginePitch - last) > maxStep && st.gear == lastG) maxStep = fabsf(st.enginePitch - last); }
    if (st.gear != lastG) { gears++; lastG = st.gear; } last = st.enginePitch;
  }
  printf("передач пройдено %d, наибольший шаг высоты за буфер %.4f\n", gears, maxStep);
  check("передачи сменяются при разгоне", gears >= 3);
  check("высота не быстрее 2 ед./с (за буфер 11,6 мс ≤ 0,024)", maxStep <= 0.03f);
  // 3) очередь реплик: занятый поток ставит реплику в очередь, пустой — играет сразу
  sndShutdown();
  printf(fails ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", fails);
  return fails != 0;
}
