// №1144: пауза бонус-игры (пакет без таблицы режимов): музыка стихает за 750 мс, потоки стоят на месте, после паузы продолжение с того же места. Подключает sound.c; пакет бонус-игры (tools/bonus/audio_export.py).
//   bonuspause BONUSDIR/
#include "../../viewer/sound.c"
#define FR 512
static int fails;
static void check(const char *n, int ok) { printf("%s %s\n", ok ? "OK" : "FAIL", n); fails += !ok; }
static short bm[FR * 2];
static int peakOf(float sec) { int pk = 0, n = (int)(sec * 44100 / FR); for (int k = 0; k < n; k++) { audioHostMix(bm, FR); for (int i = 0; i < 2 * FR; i++) { int a = bm[i] < 0 ? -bm[i] : bm[i]; if (a > pk) pk = a; } } return pk; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  if (sndInit(argv[1]) < 0) { printf("FAIL sndInit %s\n", sndError()); return 2; }
  sndUserVolumes(1, 1, 1, 1);
  int mus = -1; for (int t = 0, n = audioTrackCount(); t < n; t++) if (audioTrackBus(t) == 0) { mus = t; break; }
  if (mus < 0) { printf("FAIL нет музыки в пакете бонус-игры\n"); return 2; }
  check("в пакете бонус-игры нет таблицы режимов (поэтому пауза раньше ничего не делала)", nmode == 0);
  audioMusic(mus, 0); peakOf(1.f);
  int p0 = peakOf(0.5f); AudioStats a0; audioStats(&a0);
  sndPause(1); peakOf(0.9f);
  AudioStats a1; audioStats(&a1); int p1 = peakOf(0.5f); AudioStats a2; audioStats(&a2);
  printf("пик до паузы %d, в паузе %d; позиция потока %.3f -> %.3f с\n", p0, p1, a1.trackPos[0], a2.trackPos[0]);
  check("до паузы музыка играет (пик > 500)", p0 > 500);
  check("в паузе музыка не слышна (пик < 2 % прежнего)", p1 * 50 < p0);
  check("в паузе поток стоит на месте (позиция не ушла дальше 0,05 с за полсекунды)", a2.trackPos[0] - a1.trackPos[0] < 0.05f);
  sndPause(0); int p2 = peakOf(1.2f); AudioStats a3; audioStats(&a3);
  printf("после паузы пик %d, позиция %.3f с\n", p2, a3.trackPos[0]);
  check("после паузы музыка слышна снова (пик > 70 % прежнего)", p2 * 10 > p0 * 7);
  check("после паузы продолжение с того же места (позиция ушла вперёд всего на ≈ 1,2 с от остановки, не на 3 с)", a3.trackPos[0] - a2.trackPos[0] < 1.8f && a3.trackPos[0] > a2.trackPos[0]);
  sndShutdown();
  printf(fails ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", fails); return fails != 0;
}
