// №1068: проверка viewer/pvschunk.h на настоящих level.shr. Для каждой клетки PVS1 список из пула кусками побайтно равен списку из целого пула.
// Красные контроли (обязаны упасть): --red-lookup (поиск со сдвинутыми границами кусков), --red-plan (список через весь пул, второй — поперёк разреза).
// cc -std=c99 -O2 -Wall -o host tests/pvschunk/host.c && ./host [--red-lookup|--red-plan] level.shr ...   код 0 — всё совпало, 1 — расхождение / план не сложился
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../viewer/pvschunk.h"
static int check(const char *path, int red) {
  FILE *f = fopen(path, "rb"); if (!f) { printf("FAIL %s: нет файла\n", path); return 1; }
  unsigned char h[96]; unsigned pvsOff, ph[8];
  if (fread(h, 1, 96, f) != 96) { fclose(f); printf("FAIL %s: заголовок\n", path); return 1; }
  memcpy(&pvsOff, h + 88, 4); fseek(f, (long)pvsOff, SEEK_SET);
  if (fread(ph, 4, 8, f) != 8 || memcmp(ph, "PVS1", 4)) { fclose(f); printf("FAIL %s: нет PVS1\n", path); return 1; }
  unsigned W = ph[4], H = ph[5], npool = ph[6], nent = W * H;
  unsigned *ent = malloc(nent * 8u); unsigned short *pool = malloc(npool * 2u);
  if (fread(ent, 8, nent, f) != nent || fread(pool, 2, npool, f) != npool) { fclose(f); printf("FAIL %s: короткий файл\n", path); return 1; }
  fclose(f);
  if (red == 2) {   // испортить таблицу: первый живой список — на весь пул; второй — поперёк разреза, который план поставил бы без него
    int done = 0; for (unsigned i = 0; i < nent && done < 1; i++) if (ent[2 * i + 1] && ent[2 * i + 1] != 0xFFFFFFFFu) { ent[2 * i] = 0; ent[2 * i + 1] = npool; done++; }
  }
  unsigned b[PVSCH_MAX + 1]; int k = pvsChPlan(ent, nent, npool, PVSCH_U16, b);
  if (k <= 0) { printf("%s %s: план не сложился (k=%d)\n", red == 2 ? "RED-OK" : "FAIL", path, k); free(ent); free(pool); return red == 2 ? 0 : 1; }
  if (red == 2) { printf("RED-FAIL %s: испорченная таблица разрезана (k=%d)\n", path, k); free(ent); free(pool); return 1; }
  unsigned short *p[PVSCH_MAX]; unsigned maxb = 0;
  for (int j = 0; j < k; j++) { unsigned c = b[j + 1] - b[j]; p[j] = malloc(c * 2u + 2); memcpy(p[j], pool + b[j], c * 2u); if (c * 2u > maxb) maxb = c * 2u; }
  unsigned bq[PVSCH_MAX + 1]; memcpy(bq, b, sizeof b); if (red == 1) for (int j = 1; j < k; j++) bq[j] += 1;   // красный: границы сдвинуты на номер
  unsigned cells = 0, lists = 0, bad = 0, nonEmpty = 0;
  for (unsigned i = 0; i < nent; i++) {
    unsigned s = ent[2 * i], n = ent[2 * i + 1]; if (n == 0xFFFFFFFFu) continue; cells++;
    if (!n) continue; nonEmpty++;
    const unsigned short *a = pvsChAt(p, red == 1 ? bq : b, k, s);
    if (memcmp(a, pool + s, n * 2u)) bad++;
    lists += n;
  }
  for (int j = 0; j < k; j++) free(p[j]);
  free(ent); free(pool);
  int fail = red == 1 ? bad == 0 : bad != 0;
  printf("%s %s: пул %u номеров (%.2f МБ), кусков %d, крупнейший %.0f КБ, клеток %u, списков %u, номеров %u, расхождений %u\n",
         red == 1 ? (bad ? "RED-OK" : "RED-FAIL") : (bad ? "FAIL" : "PASS"), path, npool, npool * 2 / 1048576.0, k, maxb / 1024.0, cells, nonEmpty, lists, bad);
  return fail;
}
int main(int argc, char **argv) {
  int red = 0, rc = 0, i = 1;
  if (argc > 1 && !strcmp(argv[1], "--red-lookup")) { red = 1; i++; } else if (argc > 1 && !strcmp(argv[1], "--red-plan")) { red = 2; i++; }
  if (i >= argc) { printf("использование: host [--red-lookup|--red-plan] level.shr ...\n"); return 2; }
  for (; i < argc; i++) rc |= check(argv[i], red);
  return rc;
}
