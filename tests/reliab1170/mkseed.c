// №1170: готовая запись слота (SHV3, 1536 Б) для сценария гейта в эмуляторе (SMOKE_SAVEFILES=…/seed.bin=save00.bin): mkseed OUT.bin [seq]
#include <stdio.h>
#include <stdlib.h>
#include "savegame.h"
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  SaveGame g; uint8_t b[SV_BYTES]; svClear(&g); g.seq = argc > 2 ? (unsigned)atoi(argv[2]) : 1; g.coins = 321; g.year = 2026; g.month = 11; g.day = 4; g.hour = 12; g.minute = 0;
  svWrite(&g, b); FILE *f = fopen(argv[1], "wb"); if (!f) return 1; fwrite(b, 1, SV_BYTES, f); fclose(f); return 0;
}
