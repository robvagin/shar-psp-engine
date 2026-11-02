// №1136: дорожка потока через настоящий микшер порта (шаг 1,0, усиление 1,0) -> сырой s16 стерео: сверяется с декодом ffmpeg (tools/audio_export.py ffmpeg_decode).  trackdec PACKDIR/ имя_дорожки выход.raw [кадров]
#include "../../viewer/audio.c"
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  int N = argc > 4 ? atoi(argv[4]) : 40000;
  if (audioInit(argv[1]) < 0) return 2;
  int t = audioTrack(argv[2]); if (t < 0) { printf("нет дорожки %s среди %d\n", argv[2], ntrack); return 3; }
  audioTestForceUnity(1); audioTestMusicOnce(t);
  FILE *f = fopen(argv[3], "wb"); static short mb[512 * 2]; int got = 0;
  for (int k = 0; got < N; k++) { audioHostMix(mb, 512); fwrite(mb, 4, 512, f); got += 512; }
  fclose(f); printf("дорожка %s (%d) записана, %d кадров\n", argv[2], t, got); return 0;
}
