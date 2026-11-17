// A1.1 (сверка Fable 13.11, №1271-сверка): машина входит в объём цели не центром, а габаритом кузова (оригинал: шар объёма + 1,5 м от центра машины ИЛИ углы кузова, отсев sqrt(полуширина + полувысота)).
// По ВСЕМ шаровым объёмам локаторов уровня: машина в r+1,2 м от центра объёма — внутри (новое), пешком в той же точке — снаружи, машина в r+2,2 м — снаружи (за отсевом, как в оригинале).
// Красный контроль: -DMSN_CARREACH_OLD=1 (только точка): первая проверка падает.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../viewer/mission/msn_run.c"
int main(int ac, char **av) {
  int bad = 0, n = 0;
  for (int a = 1; a < ac; a++) {
    FILE *f = fopen(av[a], "rb"); if (!f) { printf("нет %s\n", av[a]); return 2; } fseek(f, 0, SEEK_END); size_t sz = ftell(f); rewind(f);
    void *b = aligned_alloc(16, (sz + 15) & ~15ul); if (fread(b, 1, sz, f) != sz) return 2; fclose(f);
    static MsnFile mf; char err[128]; if (msn_open(&mf, b, sz, err, sizeof err)) { printf("open %s: %s\n", av[a], err); return 2; }
    int cnt = 0;
    for (uint32_t li = 0; li < mf.nlocs; li++) {
      const MsnLoc *l = &mf.locs[li]; if (l->ntrig != 1) continue; const MsnTrig *t = &mf.trigs[l->first_trig]; if (t->kind != 0 || t->half[0] < 0.5f || t->half[0] > 4.f) continue;
      MsnWorld w; memset(&w, 0, sizeof w); w.car_valid = 1; w.car_hw = 1.1f; w.car_hl = 2.4f; w.car_hh = 0.75f; w.car_fx = 0.f; w.car_fz = 1.f; w.car_cy = 0.75f;
      float R = t->half[0];
      w.player.x = t->center[0] + R + 1.2f; w.player.y = t->center[1]; w.player.z = t->center[2];
      int car_near = msn_loc_contains_car(&mf, l, &w, 6.5f);
      w.on_foot = 1; int foot_near = msn_loc_contains_car(&mf, l, &w, 2.6f); w.on_foot = 0;
      w.player.x = t->center[0] + R + 2.2f; int car_far = msn_loc_contains_car(&mf, l, &w, 6.5f);
      cnt++; n++;
      if (!car_near || foot_near || car_far) { bad++; if (bad < 6) printf("FAIL L%d loc %u R=%.2f: машина в r+1,2 = %d (нужно 1), пешком = %d (0), машина в r+2,2 = %d (0)\n", mf.hdr->level, li, R, car_near, foot_near, car_far); }
    }
    printf("L%d: шаровых объёмов проверено %d\n", mf.hdr->level, cnt);
  }
  printf("%s: проверено %d, провалов %d\n", MSN_CARREACH_OLD ? "old" : "new", n, bad); return bad ? 1 : 0;
}
