// 1312 (108, Даль): гейт цвета стрелок пути. Игрок идёт шагом 1 м по пути через перекрёсток (дорога r0 → o1 → o2 уровня), у первой стрелки (на 6 м дороги o1) считаем:
// bad  = шаги, где стрелка красная, а игрок ещё до неё («красная, а впереди»);  early = красная раньше, чем 20 м позади; mid = рисуется в зоне 11..20 м позади (должна скрываться);
// ok   = новая логика: красная только дальше 20 м, скрыта на 11..20 м, зелёная до 11 м позади и впереди. Старое (NAVARROW1312_OLD=1): bad > 0, новое: bad = 0 и ok = все шаги.
// Сборка: cc -std=gnu99 -O2 -w -DSKIN_HOST [-DNAVARROW1312_OLD=1] -Iviewer tests/arrow1312/arrow1312.c viewer/skin.c viewer/car.c viewer/col.c viewer/watchdog.c -lm -o /tmp/arrow1312
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../viewer/traffic.c"
int (*sceneReadyHook)(const ColWorld *w, float x, float z, int *failed);
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) { puts("нет файла"); return 2; } fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, ((size_t)sz + 63) & ~(size_t)63); if (fread(b, 1, sz, f) != (size_t)sz) return 2; fclose(f);
  static Traffic T; memset(&T, 0, sizeof T); if (trafficInitMem(&T, b, (unsigned)sz, 0)) { puts("не traffic.shr"); return 2; }
  long steps = 0, bad = 0, early = 0, mid = 0, ok = 0, paths = 0, wrongRoadSeen = 0;
  for (uint32_t r0 = 0; r0 < T.hd->nRoad; r0++) {
    if (!roadOk(&T, (int)r0) || T.road[r0].length < 30.f) continue; const TrJunc *J0 = &T.junc[T.road[r0].endJ];
    for (int k1 = 0; k1 < J0->nOut; k1++) { int o1 = T.out[J0->firstOut + k1]; if (!roadOk(&T, o1) || o1 == (int)T.road[r0].rev || T.road[o1].length < 50.f) continue;
      const TrJunc *J1 = &T.junc[T.road[o1].endJ];
      for (int k2 = 0; k2 < J1->nOut; k2++) { int o2 = T.out[J1->firstOut + k2]; if (!roadOk(&T, o2) || o2 == T.road[o1].rev || T.road[o2].length < 40.f) continue;
        float q[3], g[3], q2[3]; lanePos(&T, (int)r0, 0, 5.f, q); lanePos(&T, (int)r0, 0, 8.f, q2); lanePos(&T, o2, 0, T.road[o2].length * .5f, g);
        float h = atan2f(q2[0] - q[0], q2[2] - q[2]);
        if (trafficNavBuild(&T, q[0], q[2], h, g[0], g[2]) <= 0 || T.nav.n < 3 || T.nav.road[0] != (int)r0 || T.nav.road[1] != o1) continue;
        paths++; float arrowS = T.road[r0].length + 6.f, total = 0; for (int i = 0; i < 2; i++) total += T.road[T.nav.road[i]].length; total += 30.f;
        for (float s = 5.f; s <= total && s < arrowS + 35.f; s += 1.f) {
          int ri = 0; float so = s; while (ri < T.nav.n - 1 && so > T.road[T.nav.road[ri]].length) { so -= T.road[T.nav.road[ri]].length; ri++; }
          float p[3]; lanePos(&T, T.nav.road[ri], 0, so, p);
          TrNavArrow ar[24]; int n = trafficNavArrows(&T, p[0], p[2], ar, 24); if (n < 1) continue;
          float d = s - arrowS; steps++;   // d > 0: игрок позади стрелки по ходу
          int red = ar[0].red, hide = ar[0].hide;
          if (red && d < 0.f) bad++;
          if (red && d < 20.f) early++;
          if (!hide && !red && d > 20.f) wrongRoadSeen++;
          if (!trfNavArrow1312Old) { if (d > 11.5f && d < 19.5f && !hide) mid++; int want = d <= 10.5f ? 0 : d >= 20.5f ? 2 : -1; int st = red ? 2 : hide ? 1 : 0; if (want < 0 || st == want) ok++; }
        } } } }
  printf("путей %ld шагов %ld | красная, а впереди: %ld | красная раньше 20 м позади: %ld | не скрыта на 11..20 м: %ld | не красная дальше 20 м: %ld | по новому правилу верно: %ld\n", paths, steps, bad, early, mid, wrongRoadSeen, ok);
  return trfNavArrow1312Old ? (bad > 0 ? 0 : 1) : (bad == 0 && early == 0 && mid == 0 && ok == steps && paths > 0 ? 0 : 1);
}
