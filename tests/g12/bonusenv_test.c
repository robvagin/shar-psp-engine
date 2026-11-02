// Хост-тест №1109: кольцо-панорама вокруг бонус-трассы. Для каждой трассы b0N и панорамы уровня N (pano917.bin набора): выбор панорамы по старту трассы БЕЗ привязки и С привязкой к центру ближайшей
// панорамы (как viewer/bonus_play.h bnsEnvPano). Привязанная трасса обязана получить слой почти в полную силу; без привязки хотя бы у части трасс слоя нет.
// cc -Iviewer tests/g12/bonusenv_test.c -lm -o /tmp/bonusenv && /tmp/bonusenv SET_DIR   (с -DBNSENV_NOANCHOR=1 привязки нет: тест обязан упасть)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pano917core.h"
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("ПАДЕНИЕ: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(int argc, char **argv) {
  if (argc < 2) return 2; int noAnchorWeak = 0, anchored = 0;
  for (int n = 1; n <= 7; n++) {
    char fn[512]; snprintf(fn, sizeof fn, n > 1 ? "%s/l%d/pano917.bin" : "%s/pano917.bin", argv[1], n); FILE *f = fopen(fn, "rb"); if (!f) { printf("нет %s\n", fn); return 2; }
    static unsigned char b[1 << 16]; size_t len = fread(b, 1, sizeof b, f); fclose(f); unsigned cnt; memcpy(&cnt, b + 4, 4); if (32 + 32 * cnt > len) { printf("файл панорам %d длиннее буфера\n", n); return 2; }
    PnoE *e = (PnoE *)(b + 32);
    snprintf(fn, sizeof fn, "%s/bonus/b%02d.shr", argv[1], n); f = fopen(fn, "rb"); if (!f) { printf("нет %s\n", fn); return 2; } unsigned char h[104]; if (fread(h, 1, 104, f) != 104) return 2; fclose(f);
    float sp[3]; memcpy(sp, h + 36, 12);
    int ids[PNO_SLOTS]; float fr[2]; int pre;
    int c0 = pno917Pick(e, cnt, sp[0], sp[2], ids, fr, &pre); float w0 = c0 ? fr[0] + fr[1] : 0.f;
    float bd = 1e30f; unsigned bi = 0; for (unsigned i = 0; i < cnt; i++) { float dx = e[i].x - sp[0], dz = e[i].z - sp[2], d = dx * dx + dz * dz; if (d < bd) { bd = d; bi = i; } }
#ifdef BNSENV_NOANCHOR
    float ax = sp[0], az = sp[2];
#else
    float ax = e[bi].x, az = e[bi].z;
#endif
    int c1 = pno917Pick(e, cnt, ax, az, ids, fr, &pre); float w1 = c1 ? fr[0] + fr[1] : 0.f;
    printf("b0%d: панорам %u, до ближайшей %.0f м; вес слоя без привязки %.2f, с привязкой %.2f\n", n, cnt, sqrtf(bd), w0, w1);
    CHECK(w1 > 0.97f, "b0%d: с привязкой вес слоя %.2f (нужно почти 1)", n, w1);
    if (w0 < 0.5f) noAnchorWeak++; anchored++;
  }
  CHECK(noAnchorWeak >= 4, "без привязки слабый слой только у %d трасс из 7 (ожидалось ≥ 4: привязка нужна)", noAnchorWeak);
  printf(bad ? "КРАСНЫЙ: %d\n" : "ЗЕЛЁНЫЙ\n", bad); return bad != 0;
}
