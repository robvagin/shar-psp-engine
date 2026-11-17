// 108 О6 + О9: горячий массив предметов (props.h PropHot). Стенд на настоящих данных уровня: игрок 1500 кадров едет по городу (удары, падения, засыпание), чужие машины, толчок RADIATE, взрыв,
// перезагрузка зоны, возврат (propsRespawnStep с «видимостью» по расстоянию), удар по пропу вызывает обломок у каждого третьего. Журнал: хэш полного состояния ВСЕХ предметов (все поля PropState) и списка
// «рядом» каждый кадр. Печатает итоговый хэш журнала, расхождения горячего массива (сверка каждый кадр), счётчики медленного пути и время шага и возврата с холодным кэшем (между замерами
// кэш забивается 64 МБ, как на PSP, где 16 КБ кэша данных не помнят предметов прошлого кадра). Параметры: LEVEL.shr [L]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include "col.h"
#include "props.h"
#include "car.h"
#include "sverka_shared.h"
#ifdef NOHOT   // сборка в прежнем дереве (rel/test107): горячего массива там нет, журнал должен совпасть с новым побайтно
static int propsHotChk, propsHot108Old = 1; static unsigned propsHotBad, propsHotMiss;
static unsigned propsHotVerify(const Props *p) { (void)p; return 0; }
#endif
unsigned homerSolidFrame = 0;
void uiFlash(void) {}
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
extern unsigned propsHitIter, propsPassIter;
static Props props; static ColWorld world; static Car player, oc[14];
static unsigned char *junk; static volatile unsigned char sink;
static void flush(void) { memset(junk, 1, 64u << 20); sink = junk[12345]; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec * 1e-3; }
static int onKick(void *u, unsigned i, float dx, float dz) { (void)u; (void)dx; (void)dz; return i % 3 == 0; }
static float seeX, seeZ;
static int seen(void *u, float x, float y, float z, float r) { (void)u; (void)y; float dx = x - seeX, dz = z - seeZ; return dx * dx + dz * dz < (90.f + r) * (90.f + r); }
// модель кэша PSP (строка 64 Б, на проходе кэш холодный): сколько РАЗНЫХ строк затрагивает проход шага и проход возврата. Прежний путь читает по предмету поля шага (hidden, hitCool, asleep, fresh, noRespawn, x, z)
// и возврата (noRespawn, hidden, asleep, fresh, away) из записи PropState 184 Б; новый — запись горячего массива (16 Б), а запись PropState только у предметов с признаком. Считается по настоящим адресам.
#include <stddef.h>
static unsigned linesOf(const void *base, const size_t *offs, int n) { uintptr_t seen[16]; int ns = 0; for (int k = 0; k < n; k++) { uintptr_t l = ((uintptr_t)base + offs[k]) >> 6; int f = 0; for (int j = 0; j < ns; j++) if (seen[j] == l) f = 1; if (!f) seen[ns++] = l; } return (unsigned)ns; }
#ifdef NOHOT
static unsigned long long modelLines(const Props *p, int newPath, int resp) { (void)p; (void)newPath; (void)resp; return 0; }
#else
static unsigned long long modelLines(const Props *p, int newPath, int resp) {
  static const size_t stepF[] = {offsetof(PropState, hidden), offsetof(PropState, hitCool), offsetof(PropState, asleep), offsetof(PropState, fresh), offsetof(PropState, noRespawn), offsetof(PropState, x), offsetof(PropState, z), offsetof(PropState, fadeT)};
  static const size_t respF[] = {offsetof(PropState, noRespawn), offsetof(PropState, hidden), offsetof(PropState, asleep), offsetof(PropState, fresh), offsetof(PropState, away)};
  unsigned long long tot = 0; uintptr_t lastHot = ~(uintptr_t)0;
  for (unsigned i = 0; i < p->ninst; i++) {
    if (!newPath) { tot += resp ? linesOf(&p->st[i], respF, 5) : linesOf(&p->st[i], stepF, 7); continue; }
    uintptr_t l = (uintptr_t)&p->hot[i] >> 6; if (l != lastHot) { tot++; lastHot = l; }
    unsigned fl = p->hot[i].fl; if (resp ? (fl & PH_RESP) : (fl & PH_STEP) && !(fl & PH_HID)) tot += resp ? linesOf(&p->st[i], respF, 5) : linesOf(&p->st[i], stepF, 8);
  }
  return tot;
}
#endif
// О7: каждое изменение подписи (x, y, z, ориентация, asleep, hidden) обязано сопровождаться ростом версии записи — иначе запечённый лист не заметит изменения (propbake.h pbScanOne пропускает чтение при той же версии)
typedef struct { float v[7]; int a, h; unsigned ver; } SigSnap;
static SigSnap *prevSnap;
#ifdef NOHOT
static unsigned verCheck(const Props *p) { (void)p; return 0; }
#else
static unsigned verCheck(const Props *p) {
  unsigned bad = 0;
  for (unsigned i = 0; i < p->ninst; i++) {
    const PropState *st = &p->st[i]; SigSnap c = {{st->x, st->y, st->z, st->q[0], st->q[1], st->q[2], st->q[3]}, st->asleep, st->hidden, p->hot[i].fl >> 16};
    if (prevSnap[i].ver == c.ver && (memcmp(prevSnap[i].v, c.v, sizeof c.v) || prevSnap[i].a != c.a || prevSnap[i].h != c.h)) bad++;
    prevSnap[i] = c;
  }
  return bad;
}
#endif
static uint32_t fnv(uint32_t h, const void *d, size_t n) { const unsigned char *p = d; while (n--) { h ^= *p++; h *= 16777619u; } return h; }
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  junk = malloc(64u << 20);
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2; fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (fread(data, 1, size, f) != (size_t)size) return 2;
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  colInit(&world, data + co, ce - co);
  if (propsInit(&props, data + po, *(unsigned *)(data + po + 40))) return 3;
  props.onKick = onKick;
  float gy = 0; colGround(&world, 220, 172, 20, -50, &gy, 0);
  carInit(&player, 0, &world, 220, gy + 1, 172, 1.57f);
  for (int i = 0; i < 14; i++) {
    float a = i * 0.9f, r = i < 9 ? 15.f + 5.f * i : 110.f + 10.f * i, x = 220 + cosf(a) * r, z = 172 + sinf(a) * r, g = 0;
    if (!colGround(&world, x, z, 40, -50, &g, 0)) g = gy;
    carInit(&oc[i], 0, &world, x, g + 1, z, a);
  }
  const int frames = 1500; double tStep = 0, tResp = 0; unsigned long long lnO[2] = {0, 0}, lnN[2] = {0, 0}; unsigned nStep = 0, nResp = 0, missS = 0, missR = 0; uint32_t jr = 2166136261u; unsigned kicked = 0, kmax = 0, bad = 0;
  propsHotChk = 0; prevSnap = calloc(props.ninst, sizeof(SigSnap)); unsigned verBad = 0; verCheck(&props);
  for (int fr = 0; fr < frames; fr++) {
    if (fr % 150 == 0) {   // новая цель: каждые 150 кадров машина встаёт за 18 м до очередного предмета и едет в него (на любом уровне удары есть)
      unsigned t = (unsigned)(fr / 150 * 13 + 3) % props.ninst; float ang = 0.9f * (float)(fr / 150), fx = sinf(ang), fz = cosf(ang), px = props.def[t].x, pz = props.def[t].z, g = 0.f;
      if (colGround(&world, px - fx * 18.f, pz - fz * 18.f, props.def[t].y + 6.f, props.def[t].y - 12.f, &g, 0)) { carInit(&player, 0, &world, px - fx * 18.f, g + 1.f, pz - fz * 18.f, ang); carSetSpeed(&player, 12.f); }
    }
    CarInput in = {1.f, 0, 0.3f * sinf(fr * 0.02f), 0};
    carUpdate(&player, &world, &in, 1.f / 30.f);
    seeX = player.x; seeZ = player.z;
    if (fr == 100 || fr == 700 || fr % 90 == 40) propsRadiate(&props, player.x, player.y, player.z, fr % 90 == 40 ? 60.f : 25.f, 600.f);
    if (fr == 400) { svBoom.seq++; svBoom.radius = 30.f; svBoom.x = player.x + 10.f; svBoom.y = player.y; svBoom.z = player.z; svBoom.force = 4000.f; svBoom.byPlayer = 1; }
    if (fr == 1200) propsZoneReload(&props);
    flush(); double t0 = now(); unsigned m0 = propsHotMiss;
    propsUpdate(&props, &world, &player, 1.f / 30.f);
    double t1 = now(); tStep += t1 - t0; nStep++; missS += propsHotMiss - m0;
    for (int i = 0; i < 14; i++) {
      if (i % 2) { carSetSpeed(&oc[i], 8.f); oc[i].x += 0.27f * cosf(i); oc[i].z += 0.27f * sinf(i); }
      propsHitCar(&props, &world, &oc[i], NULL);
    }
    flush(); t0 = now(); m0 = propsHotMiss;
    propsRespawnStep(&props, player.x, player.z, 1.f / 30.f, seen, NULL);
    t1 = now(); tResp += t1 - t0; nResp++; missR += propsHotMiss - m0;
    // журнал кадра: все поля всех предметов и список «рядом»
    uint32_t h = 2166136261u;
    for (unsigned i = 0; i < props.ninst; i++) h = fnv(h, &props.st[i], sizeof(PropState));
    h = fnv(h, &props.nearN, sizeof props.nearN); h = fnv(h, props.nearList, props.nearN * sizeof(unsigned));
    jr = fnv(jr, &h, sizeof h);
    bad += propsHotVerify(&props);
    verBad += verCheck(&props);
    { unsigned k_ = 0, h_ = 0; for (unsigned i = 0; i < props.ninst; i++) { k_ += props.st[i].kicked; h_ += props.st[i].hidden; } if (k_ > kmax) kmax = k_; }
    lnN[0] += modelLines(&props, !propsHot108Old, 0); lnN[1] += modelLines(&props, !propsHot108Old, 1);
  }
  for (unsigned i = 0; i < props.ninst; i++) kicked += props.st[i].kicked;
  printf("lines_step %.1f\nlines_resp %.1f\n", (double)lnN[0] / frames, (double)lnN[1] / frames);
  printf("предметов %u\njournal %08x\nverbad %u\nkicked %u\nkicked_max %u\nhotbad %u\nstep_us %.1f\nresp_us %.1f\nstep_slow %.1f\nresp_slow %.1f\n", props.ninst, jr, verBad, kicked, kmax, bad, tStep / nStep, tResp / nResp, (double)missS / nStep, (double)missR / nResp);
  return 0;
}
