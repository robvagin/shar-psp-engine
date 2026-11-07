// №1170 (риск 7 разбора надёжности): записи ВСЕХ прежних раскладок читаются и переходят в SHV3 с тем же прогрессом, человек при обновлении не теряет игру.
// Раскладки (git log viewer/savegame.h): SHSV версия 1 (28.09, 256 Б, без «купленного»), SHSV версия 2 (29.09, 256 Б, с «купленным»; файл save<NN>.bin до 05.10),
// SHSV версия 2 + хвост SHSX (05.10, 768 Б: разовые предметы, ext), SHV3 (02.11, 1536 Б, нынешняя). Плюс буфер утилиты (прежняя запись внутри нулей до 2048 Б) и настоящий файл прежней сборки.
// Красный контроль: -DSAVEOLDREAD_OLD=1 (чтение прежних выключено) обязано дать «Damaged» и провал.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "savegame.h"
static int fails;
#include <stdarg.h>
static void check(const char *name, int ok, const char *fmt, ...) { char d[200]; va_list ap; va_start(ap, fmt); vsnprintf(d, sizeof d, fmt, ap); va_end(ap); printf("CHECK %-22s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
int main(void) {
  SaveGame a; svClear(&a); a.seq = 9; a.coins = 4321; a.level = a.msnLevel = 2; a.msnStory = 1; a.msnStage = 2; a.msnCompleted = 0x15; a.skin = 3; a.music = 400; a.voice = 100; a.radar = 1;
  a.cards[0] = 0x55; a.gags[1] = 0x21; a.owned[1] = 0x6; a.once[1].n = 20; a.once[1].sig = 0xCAFE; a.once[1].st[3] = 0x41; a.ext[SV_EXT_CAM] = SV_EXT_CAM_SET;
  struct { const char *name; int ver; unsigned n; int hasOwned; int hasTail; } L[] = {
    {"SHSV v1 (28.09, 256 Б)", 1, SV_BYTES_V2, 0, 0}, {"SHSV v2 (29.09, 256 Б)", 2, SV_BYTES_V2, 1, 0}, {"SHSV v2+SHSX (05.10, 768 Б)", 3, SV_BYTES_OLD, 1, 1}};
  for (int i = 0; i < 3; i++) {
    uint8_t buf[SV_BYTES_OLD], nb[SV_BYTES], nb2[SV_BYTES]; SaveGame q, q2;
    int w = svWriteVer(&a, buf, L[i].n, L[i].ver), r = svRead(buf, L[i].n, &q);
    int head = r == SV_OK && q.seq == 9 && q.coins == 4321 && q.msnLevel == 2 && q.msnStory == 1 && q.msnStage == 2 && q.msnCompleted == 0x15 && q.skin == (L[i].hasOwned ? 3 : 0) &&   /* v1 кончалась настройками: ни купленного, ни костюма */
               q.cards[0] == 0x55 && q.gags[1] == 0x21 && q.music == 400 && q.voice == 100 && q.radar == 1 && (L[i].hasOwned ? q.owned[1] == 0x6 : q.owned[1] == 0);
    int tail = !L[i].hasTail || (q.once[1].n == 20 && q.once[1].sig == 0xCAFE && q.once[1].st[3] == 0x41 && q.ext[SV_EXT_CAM] == SV_EXT_CAM_SET);
    int wr = r == SV_OK && svWrite(&q, nb) == SV_BYTES && !memcmp(nb, "SHV3", 4) && svRead(nb, SV_BYTES, &q2) == SV_OK && q2.coins == 4321 && q2.seq == 9 && svWrite(&q2, nb2) == SV_BYTES && !memcmp(nb, nb2, SV_BYTES);
    char nm[64]; snprintf(nm, sizeof nm, "layout.%d", i + 1);
    check(nm, w == (int)L[i].n && head && tail && wr, "%s: читается %d, поля %d, хвост %d, SHV3 туда-обратно %d", L[i].name, r == SV_OK, head, tail, wr);
    // буфер утилиты: прежняя запись внутри нулей (readSlot читает sizeof ioBuf = 2048 Б)
    static uint8_t io[2048]; memset(io, 0, sizeof io); memcpy(io, buf, L[i].n); SaveGame u; int ru = svRead(io, sizeof io, &u);
    snprintf(nm, sizeof nm, "util_buf.%d", i + 1);
    check(nm, ru == SV_OK && u.coins == 4321 && u.seq == 9, "%s в буфере утилиты 2048 Б: %d", L[i].name, ru == SV_OK);
  }
  // настоящий файл прежней сборки (до 05.10)
  { FILE *f = fopen("tests/savegame/fixtures/save_v2_before334.bin", "rb"); uint8_t fb[1024]; size_t n = f ? fread(fb, 1, sizeof fb, f) : 0; if (f) fclose(f);
    SaveGame q; uint8_t nb[SV_BYTES]; int ok = n >= SV_BYTES_V2 && svRead(fb, (unsigned)n, &q) == SV_OK && svWrite(&q, nb) == SV_BYTES && !memcmp(nb, "SHV3", 4);
    check("real_v2_file", ok, "настоящий save00.bin прежней сборки: %d", ok); }
  // порча прежней записи по-прежнему «Damaged» (читаем не «всё подряд»)
  { uint8_t buf[SV_BYTES_V2]; SaveGame q; svWriteVer(&a, buf, sizeof buf, 2); buf[40] ^= 0x10; check("damaged_still", svRead(buf, sizeof buf, &q) == SV_DAMAGED, "один испорченный бит прежней записи: Damaged"); }
  printf("RESULT: %s — провалов %d\n", fails ? "FAIL" : "OK", fails);
  return fails ? 1 : 0;
}
