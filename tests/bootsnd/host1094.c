// №1094: реплика продавца комиксов на заставке вместо нот (viewer/bootbeep.h). Файл — из tools/bootvo_export.py (клип с диска как есть, PS-ADPCM).
//   host1094 bootvo.bin ref.raw     ref.raw — эталонная распаковка тем же экспортёром (Python, bit-exact с ffmpeg). Код 0 — всё прошло.
// Красный контроль (tests/bootsnd/run.sh): -DBOOTVO1094_OLD=1 (писк), -DBOOT_VO_GAIN=0.0f (тишина), порча файла.
#include <stdio.h>
#include <string.h>
#include "bootbeep.h"
static int fails;
static void check(const char *n, int ok) { printf("%s: %s\n", ok ? "да" : "нет", n); fails += !ok; }
static unsigned char *slurp(const char *fn, int *len) { FILE *f = fopen(fn, "rb"); if (!f) return 0; fseek(f, 0, SEEK_END); *len = (int)ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *d = malloc(*len + 1); if (fread(d, 1, *len, f) != (size_t)*len) { fclose(f); return 0; } fclose(f); return d; }
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  int len, rlen; unsigned char *d = slurp(argv[1], &len), *ref = slurp(argv[2], &rlen);
  if (!d || !ref) { printf("нет файлов\n"); return 2; }
  int rc = bootVoLoad(d, len);
  check("файл bootvo.bin распакован (BVO1, блоки кратны 16, частота 8-48 кГц)", rc == 0);
  if (rc) return 1;
  check("число сэмплов совпало с эталоном", bootVoN * 2 == rlen);
  int diff = 0; for (int i = 0; i < bootVoN && i * 2 + 1 < rlen; i++) { int e = (short)(ref[i * 2] | ref[i * 2 + 1] << 8); if (e != bootVoPcm[i]) diff++; }
  check("распаковка движка побитно равна эталону", diff == 0);
  printf("клип: %d Б, %d Гц, %d сэмплов, %.2f с\n", len, bootVoRate, bootVoN, bootVoN / (float)bootVoRate);
  BootSnd s; bootSndInit(&s, 0);
  check("заставка играет реплику (vo=1), а не ноты", s.vo == 1);
  short buf[BOOT_SND_CHUNK]; int total = 0, peakPre = 0, peak = 0, made = 0;
  for (int c = 0; total < s.n + 4 * BOOT_SND_CHUNK; c++) {
    int m = bootSndFill(&s, buf, BOOT_SND_CHUNK); made += m;
    for (int j = 0; j < BOOT_SND_CHUNK; j++) { int a = buf[j] < 0 ? -buf[j] : buf[j], at = total + j; if (at < s.beepAt) { if (a > peakPre) peakPre = a; } else if (a > peak) peak = a; }
    total += BOOT_SND_CHUNK;
  }
  printf("до начала (%d сэмплов, %d мс): пик %d; дальше: пик %d; отдано партитуры %d\n", s.beepAt, s.beepAt / 44, peakPre, peak, made);
  check("до момента, где был писк (1024 мс), тишина", peakPre == 0);
  check("с этого момента реплика слышна (пик > 3000)", peak > 3000);
  int expect = s.beepAt + (int)((long long)bootVoN * BOOT_BEEP_RATE / bootVoRate) + BOOT_BEEP_RATE / 1000 * BOOT_TUNE_TAIL_MS;
  check("длина партитуры = момент + клип + хвост", made == expect);
  // остановка START: затухание за один кусок
  bootSndInit(&s, 0); int tot2 = 0, stopped = 0;
  for (int c = 0; c < 200; c++) { if (c == 60) bootSndStop(&s); int m = bootSndFill(&s, buf, BOOT_SND_CHUNK); tot2 += BOOT_SND_CHUNK;
    if (c == 62) { int a = 0; for (int j = 0; j < BOOT_SND_CHUNK; j++) a |= buf[j]; stopped = a == 0; } (void)m; }
  check("остановка (START) глушит реплику через кусок", stopped);
  printf(fails ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", fails);
  return fails != 0;
}
