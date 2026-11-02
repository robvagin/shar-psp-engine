// Хост-тест площадок переброса (viewer/svdata.c, круг 7 g12-data, P18-12): cc -Iviewer tests/g12/svtp_test.c viewer/svdata.c -lm -o /tmp/svtp && /tmp/svtp svdata-l3.shr
// С -DSVTELEPORT_OLD=1 площадок нет: тест обязан упасть.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "svdata.h"
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("ПАДЕНИЕ: "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  FILE *g = fopen(argv[1], "rb"); if (!g) { printf("нет файла\n"); return 2; } static unsigned char b[1 << 16]; size_t n = fread(b, 1, sizeof b, g); fclose(g);
  int lvl = argc > 2 ? atoi(argv[2]) : 3;
  CHECK(svdataParse(b, (unsigned)n, lvl) == 1, "файл не принят");
  CHECK(svTeleportCount() == 3, "площадок %d, на L3/L6 по диску три (R5T01, lightport, Z5T1)", svTeleportCount());
  uint32_t na, oa; memcpy(&na, b + 36, 4); memcpy(&oa, b + 40, 4);
  int seen = 0;
  for (uint32_t i = 0; i < na; i++) {
    const unsigned char *r = b + oa + i * 120; uint32_t role; memcpy(&role, r + 116, 4); if (role != 7) continue;
    float c[3], to[3], hd; memcpy(c, r + 52, 12); memcpy(to, r + 32, 12); memcpy(&hd, r + 112, 4);
    int k = svTeleportAt(c); CHECK(k >= 0, "центр площадки %s не внутри неё", r);
    float p[3], h2; if (k >= 0) { CHECK(svTeleportTarget(k, p, &h2) && fabsf(p[0] - to[0]) < 1e-3f && fabsf(p[2] - to[2]) < 1e-3f && fabsf(h2 - hd) < 1e-6f, "цель площадки %s не совпала с данными", r); seen++; }
    float far_[3] = {c[0] + 500.f, c[1], c[2]}; CHECK(svTeleportAt(far_) < 0, "площадка %s срабатывает за 500 м", r);
    float up[3] = {c[0], c[1] + 60.f, c[2]}; CHECK(svTeleportAt(up) < 0, "площадка %s срабатывает на 60 м выше (высота объёма не учтена)", r);
  }
  CHECK(seen == 3, "проверено площадок %d", seen);
  float o[3], h3; CHECK(!svTeleportTarget(7, o, &h3) && !svTeleportTarget(-1, o, &h3), "неверный номер площадки принят");
  CHECK(svdataParse(b, (unsigned)n, lvl + 1) == 0 && svTeleportCount() == 0, "файл другого уровня оставил площадки");
  printf(bad ? "КРАСНЫЙ: %d\n" : "ЗЕЛЁНЫЙ\n", bad); return bad != 0;
}
