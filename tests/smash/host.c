// Правка Роба 297 (04.10, трамплин у L1 K2-6 к горе покрышек: «квадратные стоги сена — у оригинала разбиваются, у нас
// неразбиваемые»). Настоящие viewer/car.c, col.c, props.c, stateprops.c на данных уровня, без эмулятора.
// Семейство — все стейтпропы уровня, у которых начальное состояние ловит удар машины (VEHICLE_HIT / HIT / DESTROYED), с объёмом
// и не насквозь (cls 2 — кусты-шары — пропускаются); миссионные с тегом (щитки m4) — нет. Для каждого экземпляра:
//   лоб  — 12 направлений × 5 смещений вбок (±1 м от центра объёма) × 8 и 15 м/с, предметы «ломается» (cls 4: тюки, ящики,
//          заборы, будки, автоматы, колонки) с объёмом-коробкой: в центр разбит не меньше 98 % касаний, краем/углом — 97 %;
//          столбы/шары и «на шарнире» (cls 10) в лоб не судим — печатаются
//   сверху — посадка с 1.5 м над верхом объёма на 0 / 3 / 6 м/с в 4 стороны (как с трамплина на тюк): не меньше 90 % касаний
//   мимо — стоящая вплотную машина и толчок 1 м/с: ни одного разбитого при скорости кузова в момент удара ≤ 1.5 м/с и
//          вертикальной ≤ 1 м/с (скатился под горку к тюку на склоне трамплина или упал с уступа — удар настоящий)
// Печатает итог по уровню; код выхода 1 — нарушено. Красный: сборка с -DSP_HIT_MINAXIS=1 (прежняя ось удара) обязана упасть.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0;   // props.c: кеш Гомера (homer.c), в гейте не нужен
static Props props; static StateProps sp; static ColWorld world; static Car car;

static void quatR3(const float q[4], float R[9]) {   // как stateprops.c: мир = R*v + T, R по столбцам
  float w = q[0], x = q[1], y = q[2], z = q[3];
  R[0] = 1 - 2 * (y * y + z * z); R[3] = 2 * (x * y - w * z); R[6] = 2 * (x * z + w * y);
  R[1] = 2 * (x * y + w * z); R[4] = 1 - 2 * (x * x + z * z); R[7] = 2 * (y * z - w * x);
  R[2] = 2 * (x * z - w * y); R[5] = 2 * (y * z + w * x); R[8] = 1 - 2 * (x * x + y * y);
}
static int carEvent(const StateProps *s, unsigned i) {   // начальное состояние ловит событие машины
  const SpType *T = &s->type[s->inst[i].type]; const SpState *S = &s->state[T->firstState];
  for (unsigned k = 0; k < S->nEvent; k++) { int e = s->event[S->firstEvent + k].event; if (e == SP_EV_VEHICLE_HIT || e == SP_EV_HIT || e == SP_EV_DESTROYED) return 1; }
  return 0;
}
// один заезд: 1 — разбит (сменил состояние / убран / событие машины), 0 — касался, но цел, -1 — не коснулся или прежде
// ударилась о твёрдое (мир между машиной и пропом — не предмет гейта)
// y: верх поиска земли под стартом (на земле) или, при drop, высота кузова в воздухе
static int drive(unsigned i, float x, float y, float z, float h, float v, float vy, int thr, float secs, int drop) {
  spFree(&sp); if (spInit(&sp, props.spr, props.sprSize)) return -2;
  carInit(&car, 0, &world, x, y, z, h);
  if (drop) { car.y = y; car.ry = y; }
  carSetSpeed(&car, v); car.vel[1] = vy;
  CarInput in = {thr ? 1.f : 0.f, 0, 0, 0}; int touched = 0;
  for (int n = 0; n < (int)(secs * 30.f); n++) {
    carUpdate(&car, &world, &in, 1.f / 30.f);
    if (!touched && car.hardV > 1.f) return -1;   // до пропа ударилась о твёрдое (стена, забор, ствол) — заезд не про проп
    float c0 = sp.run[i].cool; sp.run[i].cool = 0.f;   // касание этого кадра видно по взведённому «остыванию»
    int h0 = sp.hits, st0 = sp.run[i].state;
    spUpdate(&sp, &world, &car, NULL, 1.f / 30.f);
    if (sp.run[i].cool > 0.f) { touched = 1; if (sp.hits > h0 && c0 <= 0.f) return 1; }   // событие машины этому экземпляру (в т.ч. в то же состояние)
    else sp.run[i].cool = fmaxf(0.f, c0 - 1.f / 30.f);
    if (sp.run[i].state != st0 || !sp.run[i].alive) return 1;
  }
  return touched ? 0 : -1;
}

int main(int argc, char **argv) {
  if (argc < 2) return 2; FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || !props.spr ||
      spInit(&sp, props.spr, props.sprSize)) { printf("нет стейтпропов в %s\n", argv[1]); return 0; }
  const char *only = argc > 2 ? argv[2] : 0;   // необязательно: номер экземпляра (разбор)
  unsigned ninst = sp.ninst; int nfam = 0, fT = 0, fB = 0, hT = 0, hB = 0, cT = 0, cB = 0, dT = 0, dB = 0, falseB = 0, nudgeIntact = 0, worstI = -1; float worst = 2.f;
  for (unsigned i = 0; i < ninst; i++) {
    const SpInst *I = &sp.inst[i]; const SpType *T = &sp.type[I->type];
    if (T->kind || T->cls == 2 || !T->nCol || I->tag || !carEvent(&sp, i)) continue;
    if (only && (int)i != atoi(only)) continue;
    nfam++;
    float R[9]; quatR3(I->q, R);
    float c[3] = {I->x + R[0] * T->c[0] + R[3] * T->c[1] + R[6] * T->c[2], I->y + R[1] * T->c[0] + R[4] * T->c[1] + R[7] * T->c[2],
                  I->z + R[2] * T->c[0] + R[5] * T->c[1] + R[8] * T->c[2]};
    float top = c[1] + fabsf(R[1]) * T->h[0] + fabsf(R[4]) * T->h[1] + fabsf(R[7]) * T->h[2];
    float ext = sqrtf(T->h[0] * T->h[0] + T->h[2] * T->h[2]) + 1.f;
    int iT = 0, iB = 0, circle = sp.col[T->firstCol].kind != 0 || T->cls != 4;   // не судим в лоб: столб/шар (скользящий удар краем —
    // не удар, по радиусу) и предметы «на шарнире» cls 10 (фонари L5–L7: их удар — отдельное семейство); сверху — судим всех
    for (int a = 0; a < 12; a++) for (int o = -2; o <= 2; o++) for (int s = 0; s < 2; s++) {   // лоб
      float ang = a * 0.5235988f, fx = sinf(ang), fz = cosf(ang), lat = o * .5f, d = ext + 6.f;
      int r = drive(i, c[0] - fx * d + fz * lat, c[1] + 3.f, c[2] - fz * d - fx * lat, ang, s ? 15.f : 8.f, 0.f, 1, 1.6f, 0);
      if (r >= 0) { if (circle) { cT++; cB += r; } else if (!o) { hT++; hB += r; } else { iT++; iB += r; } }
    }
    fT += iT; fB += iB;
    if (iT && (float)iB / iT < worst) { worst = (float)iB / iT; worstI = (int)i; }
    for (int a = 0; a < 4; a++) for (int s = 0; s < 3; s++) {   // сверху
      float ang = a * 1.5708f, v = s * 3.f, back = v * .55f;
      int r = drive(i, c[0] - sinf(ang) * back, top + 1.5f, c[2] - cosf(ang) * back, ang, v, 0.f, 0, 1.5f, 1);
      if (r >= 0) { dT++; dB += r; }
    }
    for (int a = 0; a < 4; a++) {   // мимо: стоит вплотную (0 м/с) и толчок 1 м/с — не разбивать
      float ang = a * 1.5708f, fx = sinf(ang), fz = cosf(ang);
      for (float gap = ext; gap > .5f; gap -= .25f) {   // ближайшее место у объёма, где машина ещё не касается
        float x = c[0] - fx * (gap + 2.7f), z = c[2] - fz * (gap + 2.7f);
        if (drive(i, x, c[1] + 3.f, z, ang, 0.f, 0.f, 0, .2f, 0) >= 0) continue;
        // сверка g11 круг 6 (P16:Д6): у ломающегося стейтпропа порога по силе нет — любое касание машиной ломает; значит толчок 1 м/с, КОТОРЫЙ КОСНУЛСЯ, обязан разбить (nudgeIntact == 0).
        // Прежнее «ложных от толчка» (разбил = плохо) теперь только справка (falseB), в приговор не входит. SPIMP_SV_OLD=1 (порог импульса и у ломающихся) оставляет часть целыми
        int r1 = drive(i, x, c[1] + 3.f, z, ang, 1.f, 0.f, 0, 1.2f, 0); if (r1 == 0) nudgeIntact++;
        if (r1 == 1 && fabsf(car.v) <= 1.5f && fabsf(car.vel[1]) <= 1.f) { falseB++;   // скатился под горку или упал с уступа — удар настоящий, не толчок
          if (getenv("SMASH_V")) printf("  ложный: экз. %u тип %u cls %u масса %.0f сторона %d удар %.2f м/с машина v %.2f vy %.2f\n", i, I->type, T->cls, T->mass, a, sp.lastImpact, car.v, car.vel[1]); }
        break;
      }
    }
    if (only) printf("inst %u type %u cls %u mass %.0f: лоб %d/%d, сверху всего %d/%d\n", i, I->type, T->cls, T->mass, iB, iT, dB, dT);
  }
  float fr = fT ? (float)fB / fT : 1.f, hr = hT ? (float)hB / hT : 1.f, dr = dT ? (float)dB / dT : 1.f;
  int bad = (hT && hr < .98f) || (fT && fr < .97f) || (dT && dr < .90f) || nudgeIntact;
  printf("%s %s: экземпляров %d; cls 4 в центр разбит %d из %d (%.1f %%); cls 4 краем/углом (±0.5–1 м) %d из %d (%.1f %%, худший экземпляр %d: %.0f %%); "
         "столбы, шары и cls 10 в лоб %d из %d (не судим); сверху %d из %d (%.1f %%); разбито толчком 1 м/с (справка, порога нет) %d; коснулись толчком 1 м/с и целы %d\n", bad ? "FAIL" : "PASS", argv[1], nfam,
         hB, hT, 100.f * hr, fB, fT, 100.f * fr, worstI, 100.f * (worst > 1.f ? 1.f : worst), cB, cT, dB, dT, 100.f * dr, falseB, nudgeIntact);
  return bad;
}
