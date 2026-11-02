// №1100: окно колец потоков в задании звука ≤ 1024 Б (вместо 2048) не меняет звук ни на бит. Подключает audio.c, настоящий пакет уровня: музыка, фон, речь, позиционная петля и 10 голосов, 8000 буферов
// (93 с). Хеш всего вывода и счётчики: прогон с audioMeHand(1) и с audioMeHand(0) обязаны дать один хеш; окно потока ни разу не больше 1024 Б и не упёрлось в край (нехватки окна нет).
//   hand PACKDIR/ 0|1 [буферов]    печатает HASH=… MAXUSED=… WINB=… UNDERRUN=…
#include "../../viewer/audio.c"
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  int on = atoi(argv[2]), N = argc > 3 ? atoi(argv[3]) : 8000;
  if (audioInit(argv[1]) < 0) return 2;
  audioTestSeed(99); audioMeHand(on);
  audioPlayerCar("famil_v");
  int nt = audioTrackCount(), tm = -1, ta = -1, td = -1;
  for (int t = 0; t < nt; t++) { int b = audioTrackBus(t); if (b == 0 && tm < 0) tm = t; if (b == 1 && ta < 0) ta = t; if (b == 2 && td < 0) td = t; }
  audioMusic(tm, 0); audioAmbience(ta, 0); audioStream(AUDIO_ST_DIALOG, td, 0);
  const char *nm[6] = {"fire", "skid", "car_jump", "hydrant_spray", "wasp_idle", "coin_collect_01"}; int ids[6], ni = 0;
  for (int i = 0; i < 6; i++) { int id = audioFind(nm[i]); if (id >= 0) ids[ni++] = id; }
  static short mb[512 * 2]; unsigned long long h = 1469598103934665603ULL; unsigned maxUsed = 0, win = 0, nwin = 0;
  for (int k = 0; k < N; k++) {
    if (k % 50 == 0) for (int i = 0; i < ni; i++) audioPlayAt(ids[i], 1.f * i, 0.f, 3.f, 3.f, 100.f);
    if (k == 3000) audioMusic(nt > 3 ? 3 : tm, 0.5f);     // смена дорожки на ходу (другая частота)
    if (k % 7 == 0) { audioEngineEx(0, 0, 0, 0, 1.f); audioEngine(40.f + k % 90, 120.f, .8f, AUDIO_ENGINE_ON); }
    audioHostMix(mb, 512);
    for (int i = 0; i < 1024; i++) { h ^= (unsigned short)mb[i]; h *= 1099511628211ULL; }
    for (int s = 0; s < NSTREAM; s++) { if (J->w[s].used > maxUsed) maxUsed = J->w[s].used; win += J->w[s].len; nwin++; }
  }
  printf("HASH=%016llx MAXUSED=%u WINB=%.0f UNDERRUN=%d MEHAND=%d\n", h, maxUsed, nwin ? (double)win * NSTREAM / nwin : 0.0, underruns, audioMeHandOn());
  return 0;
}
