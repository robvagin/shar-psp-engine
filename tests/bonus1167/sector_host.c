// №1185: гейт кодов секторов трасс бонус-игры на Маке (настоящие bonus.c и viewer/bnssector.h, настоящие данные build/bonus/b0N.bns).
//   sector_host b01.bns … b07.bns     [-d  — печать границ секторов для листа-карты: «track k x z dx dz»]
// Проверки по каждой трассе: (1) секторов ровно ceil(длина/5 м), доля 4,9…5,0 м; (2) однозначность: код точки не зависит от порядка вызова и повторов; (3) идя по пути с шагом 0,25 м,
// номер не убывает и не перескакивает через сектор (соседи по ходу — соседи по номеру), первый 1, последний — последний; (4) середина каждого сектора даёт его же номер; (5) старт машин
// игрока — в пределах 12 м от пути. КРАСНЫЙ: сборка с -DBSC_BREAK (номер по прямой от начала) обязана упасть на (3) и (4).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bnssector.h"

int main(int argc, char **argv) {
  int dump = 0, fail = 0, nt = 0;
  for (int a = 1; a < argc; a++) {
    if (!strcmp(argv[a], "-d")) { dump = 1; continue; }
    FILE *f = fopen(argv[a], "rb"); if (!f) { printf("FAIL %s: нет файла\n", argv[a]); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); void *d = malloc((size_t)n); if (fread(d, 1, (size_t)n, f) != (size_t)n) return 2; fclose(f);
    BnsTrack t; if (bnsParse(&t, d, (unsigned)n)) { printf("FAIL %s: bnsParse\n", argv[a]); return 2; }
    static BscMap m; char sp[512]; snprintf(sp, sizeof sp, "%s", argv[a]); { char *dot = strrchr(sp, '.'); if (dot) strcpy(dot, ".sec"); }   // линия — файл .sec рядом с .bns (или в SEC_DIR)
    if (getenv("SEC_DIR")) { const char *b = strrchr(argv[a], '/'); snprintf(sp, sizeof sp, "%s/%s", getenv("SEC_DIR"), b ? b + 1 : argv[a]); char *dot = strrchr(sp, '.'); if (dot) strcpy(dot, ".sec"); }
    { FILE *sf = fopen(sp, "rb"); static unsigned char sb[BSC_MAXP * 12 + 16]; unsigned sn = sf ? (unsigned)fread(sb, 1, sizeof sb, sf) : 0; if (sf) fclose(sf);
      if (!sf || bscLoad(&m, sb, sn)) { printf("FAIL %s: нет или не читается %s\n", argv[a], sp); fail++; continue; } }
    int tr = (int)t.h->track, bad = 0; nt++;
    if (dump) { for (int k = 1; k <= m.nsec + 1; k++) { float p[3], dr[2]; bscBoundary(&m, k, p, dr); printf("B %d %d %.2f %.2f %.3f %.3f %.2f\n", tr, k, p[0], p[2], dr[0], dr[1], p[1]); } }
    if (m.step < 4.9f || m.step > 5.0f + 1e-3f) { printf("FAIL b%d: доля %.3f м вне 4,9…5,0\n", tr, m.step); bad++; }
    if (m.nsec != (int)ceilf(m.len / 5.f - 1e-4f)) { printf("FAIL b%d: секторов %d, ждали %d\n", tr, m.nsec, (int)ceilf(m.len / 5.f)); bad++; }
    int first = 0, last = 0, mism = 0, cross = 0, bad2 = 0;   // (3) обход пути: номер точки на пути = номер по ходу; отличие допустимо только там, где путь пересекает сам себя (точка лежит и на другом участке)
    for (int i = 0; i + 1 < m.n; i++) {
      float ex = m.q[i + 1][0] - m.q[i][0], ez = m.q[i + 1][2] - m.q[i][2], l = sqrtf(ex * ex + ez * ez); int ns = (int)(l / .25f) + 1;
      for (int s = 0; s < ns; s++) { float u = (float)s / (float)ns, x = m.q[i][0] + ex * u, y = m.q[i][1] + (m.q[i + 1][1] - m.q[i][1]) * u, z = m.q[i][2] + ez * u;
        float sw = m.cum[i] + (m.cum[i + 1] - m.cum[i]) * u; int kw = (int)floorf(sw / m.step) + 1; if (kw > m.nsec) kw = m.nsec;
        int k = bscSector(&m, x, y + .5f, z); if (!first) first = k; last = k;
        if (k != kw) { float dd, sa = bscArc(&m, x, y + .5f, z, &dd), pp[3], dr[2]; bscPointAt(&m, sa, pp, dr);
          int ok_ = (k == kw + 1 || k == kw - 1) && fabsf(sa - sw) < m.step * 1.01f;   /* граница между секторами, дробь метра */
          if (ok_) continue;
          float ox = pp[0] - x, oz = pp[2] - z; if (sqrtf(ox * ox + oz * oz) < .6f && fabsf(sa - sw) > m.step) cross++; else mism++; } } }
    if (first != 1 || last < m.nsec - 1) { printf("FAIL b%d: первый %d последний %d из %d\n", tr, first, last, m.nsec); bad++; }
    if (mism) { printf("FAIL b%d: на пути номер не совпал с номером по ходу в %d точках (не пересечение пути)\n", tr, mism); bad++; }
    int wrong = 0;   // (4) середины секторов — точки самого пути
    for (int k = 1; k <= m.nsec; k++) { float p[3], dr[2]; bscPointAt(&m, ((float)k - .5f) * m.step, p, dr);
      int k2 = bscSector(&m, p[0], p[1] + .5f, p[2]); if (k2 != k) { float dd, sa = bscArc(&m, p[0], p[1] + .5f, p[2], &dd), q[3], dq[2]; bscPointAt(&m, sa, q, dq); float ox = q[0] - p[0], oz = q[2] - p[2];
        if (sqrtf(ox * ox + oz * oz) > .6f) wrong++; } }
    if (wrong) { printf("FAIL b%d: середина сектора даёт чужой номер у %d из %d\n", tr, wrong, m.nsec); bad++; }
    (void)bad2;
    int amb = 0;   // (2) однозначность: одна точка — один ответ при любом порядке вызовов
    for (int r = 0; r < 200; r++) { float x = t.st[0].p[0] + (float)((r * 37) % 41 - 20), z = t.st[0].p[2] + (float)((r * 53) % 41 - 20); int a1 = bscSector(&m, x, 0.f, z); (void)bscSector(&m, z, 0.f, x); if (bscSector(&m, x, 0.f, z) != a1) amb++; }
    if (amb) { printf("FAIL b%d: ответ зависит от истории вызовов (%d)\n", tr, amb); bad++; }
    float dd; bscArc(&m, t.st[0].p[0], t.st[0].p[1], t.st[0].p[2], &dd); if (dd > 12.f) { printf("FAIL b%d: старт игрока в %.1f м от пути\n", tr, dd); bad++; }
    char c[16]; bscCode(&m, tr, t.st[0].p[0], t.st[0].p[1], t.st[0].p[2], c, sizeof c);
    printf("%s b%d: длина %.1f м, секторов %d по %.2f м, старт игрока %s (в %.1f м от пути), самопересечений пути %d точек\n", bad ? "FAIL" : "ok  ", tr, m.len, m.nsec, m.step, c, dd, cross);
    fail += bad > 0; free(d);
  }
  printf("%s: %d трасс, %d с ошибками\n", fail ? "ИТОГ FAIL" : "ИТОГ PASS", nt, fail);
  return fail != 0;
}
