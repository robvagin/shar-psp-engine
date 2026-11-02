// №1111: озвучка диалогов на языке копии игры. Подключает audio.c; пакет уровня + dialog_<язык>.sap рядом.
//   lang PACKDIR/ LANG(0..3) [expected_bytes_file]    печатает LANGTRACKS=n FIRST=<имя> IDX=<номер> и проверки; код 0 — всё прошло
// Проверки (LANG=1): дорожек с переводом > 800; их строки читаются из файла языка (метка 0x80), первые 2048 байт данных первой такой дорожки равны эталону из Python
// (языковой .rsd, моно расширен до стерео); дорожка играет через поток диалогов без нехватки данных и не тишина. LANG=0: ни одной дорожки языка, строки английские.
#include "../../viewer/audio.c"
static int fails;
static void check(const char *n, int ok) { printf("%s %s\n", ok ? "OK" : "FAIL", n); fails += !ok; }
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  int lang = atoi(argv[2]);
  audioSetLanguage(lang);
  if (audioInit(argv[1]) < 0) { printf("FAIL audioInit\n"); return 2; }
  int n = audioLanguageTracks(), first = -1;
  for (int i = 0; i < ntrack; i++) if (T[i].flags & TRK_LANG) { first = i; break; }
  printf("LANGTRACKS=%d FIRST=%s IDX=%d\n", n, first >= 0 ? T[first].name : "-", first);
  if (lang == 0 || LANGVO1111_OLD) { check("без языка (или LANGVO1111_OLD=1) ни одной дорожки языка", n == 0 && first < 0); audioShutdown(); return fails != 0; }
  if (n == 0 && argc > 3 && !strcmp(argv[3], "none")) { check("нет файла языка в пакете — английский, ни одной дорожки языка", n == 0); audioShutdown(); return fails != 0; }
  check("дорожек с озвучкой языка больше 800", n > 800);
  check("строки языка читаются из файла языка (метка)", first >= 0 && AF_OK(lfd));
  if (first >= 0 && argc > 3) {
    FILE *f = fopen(argv[3], "rb"); unsigned char ex[2048], got[2048];
    if (f && fread(ex, 1, 2048, f) == 2048) {
      int g = afRead(lfd, T[first].off, got, 2048);
      check("первые 2048 байт данных дорожки языка равны эталону (языковой .rsd)", g == 2048 && !memcmp(ex, got, 2048));
    } else check("эталон прочитан", 0);
    if (f) fclose(f);
  }
  static short mb[512 * 2]; long peak = 0;
  audioStream(AUDIO_ST_DIALOG, first, 0.f);
  for (int k = 0; k < 120; k++) { audioHostMix(mb, 512); for (int i = 0; i < 1024; i++) { long a = mb[i] < 0 ? -mb[i] : mb[i]; if (a > peak) peak = a; } }
  AudioStats st; audioStats(&st);
  check("дорожка языка играет через поток диалогов (не тишина)", peak > 500 && st.track[AUDIO_ST_DIALOG] == first);
  check("без нехватки данных потока", underruns == 0);
  audioShutdown();
  printf(fails ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", fails);
  return fails != 0;
}
