// №1114, перевод записей: файл прежнего формата ("SHSV", 256/768 Б) -> SHV3 (1536 Б) настоящим savegame.c (то же чтение, что в игре, и та же запись).
// Использование: svconv ВХОД ВЫХОД   (код 0 — переведено; 3 — вход уже SHV3, выход копия; 2 — вход не читается). Собирает tools/save_v3_convert.py.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "savegame.h"
int main(int argc, char **argv) {
  if (argc < 3) { fprintf(stderr, "svconv ВХОД ВЫХОД\n"); return 2; }
  FILE *f = fopen(argv[1], "rb"); if (!f) { perror(argv[1]); return 2; }
  static uint8_t in[4096]; unsigned n = (unsigned)fread(in, 1, sizeof in, f); fclose(f);
  int old = svIsOldFormat(in, n);
  SaveGame s; int r = svRead(in, n, &s);
  if (r != SV_OK) { fprintf(stderr, "%s: %u Б, не читается (svRead=%d)\n", argv[1], n, r); return 2; }
  static uint8_t out[SV_BYTES]; if (svWrite(&s, out) != SV_BYTES) { fprintf(stderr, "запись не собралась\n"); return 2; }
  SaveGame back; if (svRead(out, SV_BYTES, &back) != SV_OK || back.seq != s.seq || back.coins != s.coins || back.msnLevel != s.msnLevel || back.msnCompleted != s.msnCompleted) { fprintf(stderr, "проверка чтения обратно не прошла\n"); return 2; }
  FILE *g = fopen(argv[2], "wb"); if (!g) { perror(argv[2]); return 2; } fwrite(out, 1, SV_BYTES, g); fclose(g);
  printf("%s: %s %u Б -> SHV3 %d Б; seq=%u %04u-%02u-%02u %02u:%02u уровень %u миссия %u/%u монеты %d\n", argv[1], old ? "SHSV" : "SHV3", n, SV_BYTES, s.seq, s.year, s.month, s.day, s.hour, s.minute, s.level, s.msnLevel, s.msnStory, s.coins);
  return old ? 0 : 3;
}
