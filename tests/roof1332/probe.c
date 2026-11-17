// 1332: пеший Гомер запрыгивает на крышу стоящей машины (реальные homer.c, msncar.c mcSolid, col.c, car.c; без езды).
//   probe LEVEL.shr HOMER.shr MSNCARS.shr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "homer.h"
#include "msncar.h"
void bbNoteDiag(const char *a) { (void)a; }
static ColWorld world; static Car car; static MsnCars M;
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(16, (sz + 15) & ~15u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static int solidFn(void *u, float *x, float *z, float y, float r, float *top, float *nx, float *nz) { return mcSolid((const MsnCars *)u, x, z, y, r, top, nx, nz); }
int main(int argc, char **argv) {
  if (argc < 4) return 2;
  FILE *f = fopen(argv[1], "rb"); unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96); fseek(f, 0, SEEK_END); unsigned fsize = ftell(f);
  unsigned end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f); if (colInit(&world, cm, end - colOff)) return 1;
  SkinModel *sm = skinLoad(argv[2]);
  FILE *g = fopen(argv[3], "rb"); fseek(g, 0, SEEK_END); unsigned gs = ftell(g); unsigned char *d = readAt(g, 0, gs); fclose(g);
  unsigned n = *(unsigned *)(d + 12), doff = *(unsigned *)(d + 16), ioff = *(unsigned *)(d + 24), isz = *(unsigned *)(d + 28);
  const unsigned esz = 16 + (16 + 8 + 40 + 48 + 4 + 12) + 32;
  float gx = 0, gz = 0, gy = 0; int found = 0;   // ровное место 10x10 м
  for (float z = -300; z < 300 && !found; z += 3) for (float x = 0; x < 500 && !found; x += 3) {
    float y0; if (!colGround(&world, x, z, 50, -20, &y0, 0)) continue; int ok = 1;
    for (float dz = -6; dz <= 6 && ok; dz += 2) for (float dx = -6; dx <= 6; dx += 2) { float y; if (!colGround(&world, x + dx, z + dz, 50, -20, &y, 0) || fabsf(y - y0) > .03f) { ok = 0; break; } }
    if (ok) { gx = x; gz = z; gy = y0; found = 1; } }
  if (!found) { puts("нет ровного места"); return 2; }
  printf("ровное место (%.1f %.1f) y=%.3f\n", gx, gz, gy);
  const char *names[] = {"cSedan", "famil_v", "mrplo_v", "otto_v", "plowk_v", "cVan", "cDuff", "pizza", "gramp_v", "ambul", "schoolbu", 0};   // schoolbu — автобус трафика (argv[4] — traffic.shr уровня 1)
  int bad = 0; const int own = getenv("OWN") != 0; if (getenv("OLD")) homerOwnRoof1332Old = 1; if (getenv("OLDD")) homerDecks1332Old = 1;
  if (getenv("DECKS")) { FILE *df = fopen(getenv("DECKS"), "rb"); static char db[16384]; if (df) { size_t n_ = fread(db, 1, sizeof db - 1, df); db[n_] = 0; fclose(df); homerDecksParse(db); } }   // DECKS=cardecks.txt — этажи (1332 Б)   // OWN=1 — своя машина игрока (HomerCar: carRoofAt / pushFromCar), иначе чужая (mcSolid)
  printf(own ? "режим: СВОЯ машина игрока\n" : "режим: чужая машина (mcSolid)\n");
  for (int k = 0; names[k]; k++) {
    int idx = -1; for (unsigned i = 0; i < n; i++) if (!strncmp((char *)d + doff + i * esz, names[k], 16)) idx = (int)i;
    CarParams P;
    if (!strcmp(names[k], "schoolbu")) {   // машина трафика: ходовые из .con по имени, геометрия из записи модели TRF1 (как traffic.c)
      if (argc < 5) continue; FILE *tf = fopen(argv[4], "rb"); if (!tf) continue; fseek(tf, 0, SEEK_END); unsigned ts = ftell(tf); unsigned char *td = readAt(tf, 0, ts); fclose(tf); int got = 0;
      for (unsigned off = 0; off + 128 <= ts && !got; off += 4) if (!strncmp((char *)td + off, "schoolbu", 8) && !td[off + 8]) {
        const float *f = (const float *)(td + off + 16 + 8); carParamsFor("schoolbu", &P);   // lift, wheelRad, halfW, halfL, bodyOfs, wheelX, wheelF, wheelR, bodyLo, bodyHi
        P.halfW = f[2]; P.halfL = f[3]; P.bodyOfs = f[4]; P.wheelX = f[5]; P.wheelF = f[6]; P.wheelR = f[7]; P.wheelRad = f[1]; P.bodyLo = f[8]; P.bodyHi = f[9]; got = 1; }
      free(td); if (!got) continue;
    } else { if (idx < 0) continue; if (carParamsFromInfo(d + ioff + idx * isz, &P) != 1) { printf("%s: нет info\n", names[k]); continue; } }
    carInit(&car, &P, &world, gx, gy, gz, 0.f);
    memset(&M, 0, sizeof M); M.n = 1; M.c[0].on = 1; M.c[0].car = car;
    const Car *c = &M.c[0].car; float roof = c->y + c->p.bodyHi;
    // Гомер у левого борта, лицом к машине; повторить с разных расстояний
    int okAny = 0, tried = 0, okN = 0, okByO[3] = {0, 0, 0}; float hiStand = 0; float bestY = -1e9f;
    static const float OFFS[3] = {.5f, 1.0f, 1.8f}, THR[3] = {2.5f, 0.5f, -3.f}; static const int REL[5] = {15, 25, 40, 60, 90};
    static const float AL[3] = {0.f, 1.5f, -1.5f};   // подход к борту: у середины, к носу (+1,5 м), к корме (−1,5 м) — у высоких машин этажи разные по длине
    for (int va = 0; va < 3; va++) for (int vo = 0; vo < 3; vo++) for (int vt = 0; vt < 3; vt++) for (int vr = 0; vr < 5; vr++) {
      static Homer H; if (homerInit(&H, sm)) return 1; if (!own) { H.solid = solidFn; H.solidUser = &M; }
      float off = c->p.halfW + HOMER_RADIUS + OFFS[vo];
      float sx = gx - off, sz = gz + AL[va];
      homerPlace(&H, &world, sx, gy + .05f, sz, 1.5708f);
      HomerInput in; memset(&in, 0, sizeof in); in.camYaw = 1.5708f; in.my = 1.f;
      HomerCar hc; memset(&hc, 0, sizeof hc); if (own) homerCarFrom(&hc, c, 0.f);
      const float dt = 1.f / 60.f; int jumped2 = 0, tj = -1; float topY = -1e9f;
      for (int i = 0; i < 60 * 6; i++) {
        in.jump = 0;
        if (i == 20) { in.jump = 1; tj = i; }
        if (tj >= 0 && i > tj + 3 && !jumped2 && H.jumps == 1 && !H.grounded && H.vy <= THR[vt]) { in.jump = 1; jumped2 = 1; }
        if (tj >= 0 && i > tj + REL[vr]) in.my = 0.f;
        homerUpdate(&H, &world, &hc, &in, dt); homerPose(&H);
        if (H.y > topY) topY = H.y;
      }
      float dx = H.x - c->x, dz = H.z - c->z; int onTop = H.grounded && H.y > gy + 1.0f && fabsf(dx) < c->p.halfL + .3f && fabsf(dz) < c->p.halfW + .3f; if (onTop && H.y - gy > hiStand) hiStand = H.y - gy;   // стоит на машине выше метра над землёй
      if (getenv("PV") && !strcmp(names[k], getenv("PV")) && va == 0 && vo == 0 && vt == 0) printf("   v(va%d vo%d vt%d vr%d): H=(%.2f %.2f %.2f) gr=%d top=%.2f car=(%.2f %.2f)\n", va, vo, vt, vr, H.x, H.y - gy, H.z, H.grounded, topY - gy, c->x, c->z);
      tried++; if (onTop) { okAny = 1; okN++; okByO[vo]++; } if (topY > bestY) bestY = topY;
    }
    {   // без прыжка: идёт в борт 4 с — не должен оказаться на крыше (ступенька не пускает шагом на высокий кузов)
      static Homer H; homerInit(&H, sm); if (!own) { H.solid = solidFn; H.solidUser = &M; }
      homerPlace(&H, &world, gx - c->p.halfW - HOMER_RADIUS - 1.f, gy + .05f, gz, 1.5708f);
      HomerInput in; memset(&in, 0, sizeof in); in.camYaw = 1.5708f; in.my = 1.f; HomerCar hc; memset(&hc, 0, sizeof hc); if (own) homerCarFrom(&hc, c, 0.f);
      for (int i = 0; i < 240; i++) { homerUpdate(&H, &world, &hc, &in, 1.f / 60.f); homerPose(&H); }
      int climbed = H.y > gy + .5f; printf("  ходьба в борт без прыжка: %s (y над землёй %.2f, x %.2f z %.2f машина x %.2f, на земле %d, %s)\n", climbed ? "ВЗОБРАЛСЯ" : "остался на земле", H.y - gy, H.x, H.z, c->x, H.grounded, names[k]); if (climbed) bad++;
    }
    printf("  у борта %d, с 1 м %d, с разбега %d; попыток %d, на машине %d, самое высокое место стояния %.2f над землёй (ступни выше всего были на %.2f)\n", okByO[0], okByO[1], okByO[2], tried, okN, hiStand, bestY - gy);
    printf("%-9s крыша %.2f над землёй (bodyHi %.2f, y машины %.2f над землёй)  запрыгнул: %s\n", names[k], roof - gy, c->p.bodyHi, c->y - gy, okAny ? "да" : "НЕТ");
    if (!okAny) bad++;
  }
  return bad ? 1 : 0;
}
