// Сверка с исходником, окно g02-traffic (02.11.2026): проверки на CPU Мака, без эмулятора. Подключает viewer/traffic.c напрямую (видны static-функции).
// Настоящие данные уровня 1: build/level.shr (COL1) + build/traffic.shr (TRF1). Каждая проверка печатает «CHECK <имя> OK|FAIL»; в наборе выключателей
// -DTRF_SV_*_OLD=1 (красный контроль) проверка своего правила обязана упасть. Запуск: tests/sverka_g02/g02.sh [level.shr] [traffic.shr]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../../viewer/traffic.c"

void uiFlash(void) {}   // car.c ссылается слабо (ui.c); на Маке заглушка
static ColWorld world; static Traffic T; static Car player; static int fails;
static const float DT = 1.f / 60.f;
static void check(const char *n, int ok, const char *fmt, ...) {
  char d[300]; __builtin_va_list ap; __builtin_va_start(ap, fmt); vsnprintf(d, sizeof d, fmt, ap); __builtin_va_end(ap);
  printf("CHECK %-14s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++;
}
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) { unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u); fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; } return m; }
static float ground(float x, float z, float y) { float g; return colGround(&world, x, z, y + 2.f, y - 50.f, &g, 0) ? g : y; }

// прямая односторонняя дорога длиной > minLen (все точки полосы 0 на хорде в 0,6 м); -1 нет
static int straightRoad(float minLen, int notIn, int needNext) {
  for (uint32_t r = 0; r < T.hd->nRoad; r++) {
    if (!roadOk(&T, (int)r) || T.road[r].length < minLen) continue;
    const TrPt *P = lanePts(&T, (int)r, 0); int n = T.road[r].nPt; float ax = P[n - 1].x - P[0].x, az = P[n - 1].z - P[0].z, L = sqrtf(ax * ax + az * az), mx = 0;
    for (int i = 0; i < n; i++) { float d = fabsf(((P[i].x - P[0].x) * az - (P[i].z - P[0].z) * ax) / L); if (d > mx) mx = d; }
    if (mx < .6f && (int)r != notIn && (!needNext || T.junc[T.road[r].endJ].nOut > 0)) return (int)r;
  }
  return -1;
}
static float roadDir(int r, float s) { float a[3], b[3]; lanePos(&T, r, 0, s, a); lanePos(&T, r, 0, s + 2.f, b); return atan2f(b[0] - a[0], b[2] - a[2]); }
// чистая сцена: одна машина трафика (слот 0) на полосе, остальных нет, пешеходов и стоящих нет; игрок стоит у середины, камера смотрит вдоль дороги
static TrCar *scene(int r, int lane, float s, float v, float playerS, float playerOff) {
  trafficReset(&T, &world); T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR; T.npark = 0; T.maxPeds = 0; T.seed = 0x2545F491u;
  for (int i = 0; i < TR_MAXCAR; i++) T.car[i].mode = TRM_OFF;
  T.fresh = 0; T.scanT = 1e9f; T.nextra = 0;   // сами не рождаем; чужих машин от прошлой проверки нет
  TrCar *c = &T.car[0]; memset(c, 0, sizeof *c); c->model = T.slot[0].model; c->road = r; c->lane = lane; c->s = s; c->junc = -1;
  float p[3], a[3]; lanePos(&T, r, lane, s, p); lanePos(&T, r, lane, s + LOOK, a);
  carInit(&c->car, &T.params[c->model], &world, p[0], p[1], p[2], atan2f(a[0] - p[0], a[2] - p[2]));
  c->mode = TRM_DRIVE; c->v = v;
  float q[3]; lanePos(&T, r, lane, playerS, q); float h = roadDir(r, playerS);
  carInit(&player, NULL, &world, q[0] + cosf(h) * playerOff, ground(q[0], q[2], q[1]), q[2] - sinf(h) * playerOff, h);
  return c;
}
static void frame(float h) {   // игрок на месте; камера сзади по ходу, смотрит вдоль h
  CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT);
  const Car *an = T.car[0].mode == TRM_DRIVE ? &T.car[0].car : &player;   // камера смотрит на машину 0 (иначе её снимут как «вне кадра» через 5 с)
  float pp[3] = {player.x, player.y, player.z}, eye[3] = {an->x - sinf(h) * 10.f, an->y + 3.f, an->z - cosf(h) * 10.f}, fw[3] = {sinf(h), 0.f, cosf(h)};
  trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
}
static unsigned mixSeed(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x ? x : 1u; }   // перемешивание: ряды 100, 113, 126… дают связанные броски у простого генератора
static int popHonks(void) { int n = 0; TrEvent e; while (trafficPopEvent(&T, &e)) n += e.type == TR_EV_CAR_HONK; return n; }

// выезды (налево / прямо / направо) из полосы lane дороги r0 на собранном в памяти перекрёстке с тремя выездами; lanesOv > 0 подменяет число полос дороги
static int junctionTurns(int r0, int lane, int lanesOv, int cnt[3]) {
  uint16_t *outw = (uint16_t *)T.out; TrJunc *J = (TrJunc *)&T.junc[T.road[r0].endJ]; float d0[3]; laneDir(&T, r0, lane, 1, d0);
  int pick[3] = {-1, -1, -1}; float bst[3] = {9, 9, 9}; const float tg[3] = {1.57f, 0.f, -1.57f};
  for (uint32_t o = 0; o < T.hd->nRoad; o++) {
    if (!roadOk(&T, (int)o) || (int)o == r0 || (int)o == (int)T.road[r0].rev || T.road[o].endJ == T.road[r0].startJ) continue;
    float d1[3]; laneDir(&T, (int)o, 0, 0, d1); float ang = atan2f((d0[0] * d1[2] - d0[2] * d1[0]) * (float)leftSign(&T), d0[0] * d1[0] + d0[2] * d1[2]);
    for (int k = 0; k < 3; k++) if (fabsf(ang - tg[k]) < bst[k]) { bst[k] = fabsf(ang - tg[k]); pick[k] = (int)o; } }
  uint16_t save[3]; unsigned fo = J->firstOut; unsigned short sn = J->nOut; for (int k = 0; k < 3; k++) save[k] = outw[fo + k];
  for (int k = 0; k < 3; k++) outw[fo + k] = (uint16_t)pick[k]; J->nOut = 3; unsigned char sl = T.road[r0].lanes; if (lanesOv) ((TrRoad *)&T.road[r0])->lanes = (unsigned char)lanesOv;
  int n = 0; cnt[0] = cnt[1] = cnt[2] = 0;
  for (int k = 0; k < 600; k++) { TrCar c; memset(&c, 0, sizeof c); c.road = r0; c.lane = lane; c.junc = -1; T.seed = mixSeed(12345u + k * 77u);
    if (chooseNext(&T, &c) == 1) { float e0[3], e1[3]; laneDir(&T, c.road, c.lane, 1, e0); laneDir(&T, c.nroad, c.nlane, 0, e1);
      float ang = atan2f((e0[0] * e1[2] - e0[2] * e1[0]) * (float)leftSign(&T), e0[0] * e1[0] + e0[2] * e1[2]); cnt[ang > .6f ? 0 : ang < -.6f ? 2 : 1]++; n++; } }
  J->nOut = sn; for (int k = 0; k < 3; k++) outw[fo + k] = save[k]; ((TrRoad *)&T.road[r0])->lanes = sl; return n;
}
// уворот: игрок со скоростью pv едет навстречу по прямой дороге с расстояния dist, машина трафика cv; 1 — ушла в физику с уворотом
static int dodgeCase(int R, float hd, float pv, float cv, float dist) {
  TrCar *c = scene(R, 0, 15.f, cv, 15.f, 40.f); float fx = sinf(hd), fz = cosf(hd), q[3]; lanePos(&T, R, 0, 15.f, q);
  float px = q[0] + dist * fx + 1.f * cosf(hd), pz = q[2] + dist * fz - 1.f * sinf(hd);
  carInit(&player, NULL, &world, px, ground(px, pz, q[1]), pz, hd + 3.14159265f); player.vel[0] = -fx * pv; player.vel[2] = -fz * pv; player.v = pv; int went = 0;
  for (int i = 0; i < 60 * 3; i++) { player.x += player.vel[0] * DT; player.z += player.vel[2] * DT; c->v = cv;
    float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3.f, player.z}, fw[3] = {-fx, 0.f, -fz}; T.scanT = 1e9f; trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
    if (c->mode == TRM_PHYS && c->dodgeT > 0.f) { went = 1; break; } if (c->mode != TRM_DRIVE) break; }
  return went;
}
int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *trf = argc > 2 ? argv[2] : "build/traffic.shr";
  FILE *f = fopen(lvl, "rb"); if (!f) return 2; unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104) return 2;
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned end = (unsigned)ftell(f); if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f); if (!cm || colInit(&world, cm, end - colOff)) return 2;
  FILE *g = fopen(trf, "rb"); if (!g) return 2; fseek(g, 0, SEEK_END); unsigned ts = (unsigned)ftell(g); unsigned char *buf = readAt(g, 0, ts); fclose(g);
  if (!buf || trafficInitMem(&T, buf, ts, 0)) return 2;
  int R = straightRoad(150.f, -1, 1); if (R < 0) { printf("FAIL: нет прямой дороги\n"); return 2; }
  float len = T.road[R].length, hd = roadDir(R, 20.f);
  printf("прямая дорога %d, длина %.0f м, полос %d, плотность %d\n", R, len, T.road[R].lanes, T.road[R].maxCars);

  // ---- 42 (СВЕРКА_МАШИНЫ 8.4): разгон 3,5 м/с² и желаемая 60 км/ч
  { TrCar *c = scene(R, 0, 5.f, 0.f, 35.f, 12.f); for (int i = 0; i < 180; i++) frame(hd);
    check("42 разгон", fabsf(c->v - 3.5f * 3.f) < .5f, "скорость через 3 с с места %.2f м/с (оригинал 3,5 м/с² = 10,5; прежнее порта 3,9 = 11,7)", c->v);
    for (int i = 0; i < 360 && c->s < 120.f; i++) frame(hd);
    check("42 скорость", fabsf(c->v - 16.667f) < .3f, "ход на прямой %.2f м/с (оригинал 60 км/ч = 16,67; прежнее: данные дороги 50 × 1,185 = 16,46)", c->v); }

  // ---- 22/23/43 (T1, T2, 8.5): остановка перед препятствием по расстоянию между центрами 5,0 м (машина игрока стоит на дороге)
  { TrRoad *RR = (TrRoad *)&T.road[R]; unsigned char md = RR->maxCars; RR->maxCars = 0;   // соседняя полоса «занята»: смены полосы нет, проверяется только запас
    TrCar *c = scene(R, 0, 5.f, 10.f, 35.f, 12.f);
    static Car obst; float s0 = 150.f, q[3]; lanePos(&T, R, 0, s0, q); float h = roadDir(R, s0);
    carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], h);   // стоит на полосе впереди (машина миссии: трафик тормозит перед ней, как перед игроком)
    for (int i = 0; i < 900 && !(c->v < .05f && i > 60); i++) { T.extra[0] = &obst; T.nextra = 1; frame(hd); }
    float bx, bz; bodyCenter(&c->car, &bx, &bz), bx = bx; float cd = hypotf(q[0] - bx, q[2] - bz); (void)bz;
    check("22 запас", c->mode == TRM_DRIVE && fabsf(cd - SV_STOP_CAR) < 1.2f, "остановилась в %.2f м между центрами (оригинал 5,0; прежнее: бампер 3 м + полдлины ≈ 6–8)", cd); RR->maxCars = md; }

  // ---- 29/59 (P16-01, Д11): стоп-знак — очередь дорог, первая получает зелёный через 3,5 с
  { int J = -1, RJ = -1;
    for (uint32_t j = 0; j < T.hd->nJunc && RJ < 0; j++) if (T.junc[j].behaviour == 1) for (uint32_t r = 0; r < T.hd->nRoad; r++) if (roadOk(&T, (int)r) && T.road[r].endJ == j && T.road[r].length > 60.f) { J = (int)j; RJ = (int)r; break; }
    if (RJ < 0) { check("29 стоп-знак", 0, "нет перекрёстка типа 1"); }
    else {
      float L = laneLen(&T, RJ, 0); TrCar *c = scene(RJ, 0, L - 40.f, 8.f, L - 60.f, 14.f); float h = roadDir(RJ, L - 60.f);
      float tStop = -1, tGo = -1;
      for (int i = 0; i < 60 * 14; i++) { frame(h); float tm = i * DT;
        if (tStop < 0 && c->v < .05f && c->s > L - 6.f) tStop = tm;
        if (tStop >= 0 && tGo < 0 && c->v > .5f) tGo = tm; }
      check("29 стоп-знак", tStop > 0 && tGo > 0 && tGo - tStop > 3.0f && tGo - tStop < 4.6f, "стоит у края %.1f с, ждёт %.2f с до трогания (оригинал: очередь 3,5 с; прежнее порта: 2 с)", tStop, tGo - tStop); }
  }

  // ---- 45? нет. 7.2/P03-12: рождение пешехода на границе круга 45 м вокруг точки «игрок + 30 м»
  { int gi = -1; trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    float eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1}, pp[3] = {px, player.y, pz};
    int first = 0, nearB = 0, tot = 0; float worst = 0;
    for (int f = 0; f < 60 * 12; f++) {
      int was[TR_MAXPED]; for (int i = 0; i < TR_MAXPED; i++) was[i] = T.ped[i].mode;
      CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT);
      trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      for (int i = 0; i < TR_MAXPED; i++) if (was[i] == TRP_OFF && T.ped[i].mode == TRP_WALK) {
        float cz = T.loadT < 3.f ? pz : pz + 30.f, R2 = T.loadT < 3.f ? 90.f : 45.f; float d = hypotf(T.ped[i].x - px, T.ped[i].z - cz);
        tot++; if (fabsf(d - R2) < 1.5f) nearB++; else worst = fmaxf(worst, fabsf(d - R2)); (void)first; }
    }
    (void)gi;
    check("14 граница", tot >= 3 && nearB * 10 >= tot * 8, "рождений %d, на границе круга (±1,5 м) %d (оригинал: на границе 45 м вокруг игрока+30 м; на старте 90 м; прежнее: случайно в 15..45 м)", tot, nearB); }

  // ---- 7.4/P03-16: паника — пинок пугает пешеходов в 12 м
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    int p = trafficPedPlace(&T, &world, 0, px + 3.f, pz, 0.f); TrPed *e = &T.ped[0]; e->test = 0; e->path = 0; e->seg = 0; e->dir = 1;
    int n = trafficKick(&T, px, e->y, pz, 1.f, 1.f, 0.f); (void)n; (void)p;
    TrPed *e2 = &T.ped[1]; memset(e2, 0, sizeof *e2); e2->model = e->model; e2->x = px - 5.f; e2->z = pz; e2->y = e->y; e2->mode = TRP_WALK; e2->path = 0; e2->seg = 0; e2->dir = 1; e2->partner = -1; e2->speed = 1.f;
    skinInit(&e2->sp, &T.peds[e2->model]);
    trafficScare(&T, px, pz);
    check("6 паника", e2->mode == TRP_PANIC, "пешеход в 5 м от игрока после пинка/гудка: режим %d (паника = %d; прежнее: паники нет)", e2->mode, TRP_PANIC); }

  // ---- 8 (7.6): машина игрока 2 м/с — сбивает (порог 1 м/с, было 3)
  { float vx[3]; float x = 0, z = 0; Car k; carInit(&k, NULL, &world, 220.f, ground(220.f, 172.f, 50.f), 172.f, 0.f); k.vel[0] = 0; k.vel[2] = 2.f; k.v = 2.f;
    x = 220.f; z = 172.f + k.p.bodyOfs + k.p.halfL - .1f; int r = trafficCarHitsBody(&k, &x, k.y, &z, TR_PED_R, vx);
    check("8 порог", r == 2, "машина 2 м/с касается человека: результат %d (2 = сбит; 1 = оттолкнут — прежнее при порог 3 м/с)", r); }

  // ---- 11 (P01-09): смерть не с одного удара
  { Car k; carInit(&k, NULL, &world, 220.f, ground(220.f, 172.f, 50.f), 172.f, 0.f); float seen = 0.5f, win = 0.f;
    k.damage = 1.f; k.destroyed = 1; svNoOneHit(&k, &seen, &win, DT);
    check("11 2 процента", k.damage > .99f && k.damage < 1.f && !k.destroyed && win > .9f, "урон 0,5 → 1,0 одним ударом: остаётся %.3f, жива=%d, окно %.1f с (оригинал: 0,001 прочности и окно 1 с)", k.damage, !k.destroyed, win);
    k.damage = 1.f; k.destroyed = 1; svNoOneHit(&k, &seen, &win, DT);
    check("11 окно", k.damage < 1.f && !k.destroyed, "второй удар в окне не засчитан: урон %.3f", k.damage); }

  // ---- 13 (P03-05): остов не у schoolbu, у minivana есть
  { int sb = -1, mv = -1; for (uint32_t i = 0; i < T.hd->nModel; i++) { if (!strncmp(T.model[i].name, "schoolbu", 8)) sb = (int)i; if (!strncmp(T.model[i].name, "minivan", 7)) mv = (int)i; }
    check("13 остов", sb >= 0 && !svHuskOk(&T, sb) && (mv < 0 || svHuskOk(&T, mv)), "schoolbu (%d) без остова=%d, minivan (%d) с остовом=%d", sb, sb >= 0 ? !svHuskOk(&T, sb) : -1, mv, mv >= 0 ? svHuskOk(&T, mv) : -1); }

  // ---- 31/24 (T3, P16-04): выезд по номеру полосы: нулевая и единственная полоса — прямо или направо, налево никогда (если есть из чего выбирать).
  // В данных ур. 1 у каждого перекрёстка не больше одного выезда, кроме разворота, поэтому перекрёсток собирается в памяти: три выезда из других дорог — налево, прямо, направо
  { int cnt[3] = {0}, tried = 0; uint16_t *outw = (uint16_t *)T.out; int r0 = -1;
    for (uint32_t r = 0; r < T.hd->nRoad && r0 < 0; r++) if (roadOk(&T, (int)r) && T.road[r].lanes == 1 && T.junc[T.road[r].endJ].nOut >= 2) r0 = (int)r;
    TrJunc *J = (TrJunc *)&T.junc[T.road[r0].endJ]; float d0[3]; laneDir(&T, r0, 0, 1, d0);
    int pickL = -1, pickS = -1, pickR = -1; float bL = 9, bS = 9, bR = 9;
    for (uint32_t o = 0; o < T.hd->nRoad; o++) {
      if (!roadOk(&T, (int)o) || (int)o == r0 || (int)o == (int)T.road[r0].rev || T.road[o].endJ == T.road[r0].startJ) continue;
      float d1[3]; laneDir(&T, (int)o, 0, 0, d1);
      float ang = atan2f((d0[0] * d1[2] - d0[2] * d1[0]) * (float)leftSign(&T), d0[0] * d1[0] + d0[2] * d1[2]);
      if (fabsf(ang - 1.57f) < bL) { bL = fabsf(ang - 1.57f); pickL = (int)o; }
      if (fabsf(ang) < bS) { bS = fabsf(ang); pickS = (int)o; }
      if (fabsf(ang + 1.57f) < bR) { bR = fabsf(ang + 1.57f); pickR = (int)o; }
    }
    uint16_t save[3]; unsigned fo = J->firstOut; unsigned short sn = J->nOut; for (int k = 0; k < 3; k++) save[k] = outw[fo + k];
    outw[fo] = (uint16_t)pickL; outw[fo + 1] = (uint16_t)pickS; outw[fo + 2] = (uint16_t)pickR; J->nOut = 3;
    for (int k = 0; k < 600; k++) {
      TrCar c; memset(&c, 0, sizeof c); c.road = r0; c.lane = 0; c.junc = -1; T.seed = 12345u + k * 77u; tried++;
      if (chooseNext(&T, &c) == 1) {
        float e0[3], e1[3]; laneDir(&T, c.road, c.lane, 1, e0); laneDir(&T, c.nroad, c.nlane, 0, e1);
        float ang = atan2f((e0[0] * e1[2] - e0[2] * e1[0]) * (float)leftSign(&T), e0[0] * e1[0] + e0[2] * e1[2]);
        cnt[ang > .6f ? 0 : ang < -.6f ? 2 : 1]++; }
    }
    J->nOut = sn; for (int k = 0; k < 3; k++) outw[fo + k] = save[k];
    check("31 полоса 0", cnt[0] == 0 && cnt[1] > 100 && cnt[2] > 100, "выезды налево/прямо/направо из полосы 0: налево %d, прямо %d, направо %d из %d (оригинал: налево никогда, прямо и направо поровну; прежнее: поровну из всех трёх)", cnt[0], cnt[1], cnt[2], tried); }

  // ---- 24 (T3): последняя полоса — прямо или налево, направо никогда; средняя — только прямо
  { int r2 = -1; for (uint32_t r = 0; r < T.hd->nRoad && r2 < 0; r++) if (roadOk(&T, (int)r) && T.road[r].lanes == 2 && T.junc[T.road[r].endJ].nOut >= 2) r2 = (int)r;
    int cl[3] = {0}, cm[3] = {0}; if (r2 >= 0) { junctionTurns(r2, 1, 0, cl); junctionTurns(r2, 1, 3, cm); }
    check("24 полосы", r2 >= 0 && cl[2] == 0 && cl[0] > 100 && cl[1] > 100 && cm[0] == 0 && cm[2] == 0 && cm[1] > 500, "последняя полоса: налево %d, прямо %d, направо %d (направо 0); средняя: налево %d, прямо %d, направо %d (только прямо) (оригинал T3; прежнее: поровну из всех выездов)", cl[0], cl[1], cl[2], cm[0], cm[1], cm[2]); }

  // ---- 9/25 (8.1, T4): уворот от машины игрока: едет на нас быстрее 40 км/ч, мы быстрее 30 — поворачиваем В СТОРОНУ ОТ игрока (полный газ и руль 0,8 с), дальний свет мигает, потом ручник
  { int okAway = 0, okMode = 0, okFlash = 0, tr = 0;
    for (int side = -1; side <= 1; side += 2) {
      TrCar *c = scene(R, 0, 15.f, 13.f, 15.f, 40.f); float h = roadDir(R, 20.f), fx = sinf(h), fz = cosf(h), lx = cosf(h), lz = -sinf(h);   // (lx, lz) — одна из сторон
      float q[3]; lanePos(&T, R, 0, 15.f, q); float x0 = c->car.x, z0 = c->car.z; float px = q[0] + 90.f * fx + side * 1.f * lx, pz = q[2] + 90.f * fz + side * 1.f * lz;
      carInit(&player, NULL, &world, px, ground(px, pz, q[1]), pz, h + 3.14159265f); player.vel[0] = -fx * 22.f; player.vel[2] = -fz * 22.f; player.v = 22.f;
      int went = 0; float flash = 0;
      for (int i = 0; i < 60 * 8; i++) {
        player.x += player.vel[0] * DT; player.z += player.vel[2] * DT;
        float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3.f, player.z}, fw[3] = {-fx, 0.f, -fz};   // камера игрока смотрит на встречную машину
        trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
        if (c->mode == TRM_PHYS && c->dodgeT > 0.f) { went = 1; flash = fmaxf(flash, c->flashT); }
        if (went && c->dodgeT <= 0.f) break; }
      float dev = (c->car.x - x0) * lx + (c->car.z - z0) * lz;   // смещение вдоль (lx, lz) от начала
      tr++; okMode += went; okFlash += flash > 0.f; okAway += went && dev * (float)side < -.3f; }
    check("9 уворот", okMode == 2 && okFlash == 2 && okAway == 2, "ушла в физику %d из %d, дальний свет %d из %d, в сторону от игрока %d из %d (оригинал 8.1; прежнее: только остановка)", okMode, tr, okFlash, tr, okAway, tr); }

  // ---- 10/26 (8.2, T5): смена полосы: раз в 5 с с шансом 1/2 (двухполосная дорога, помех нет)
  { int changes = 0, wasChg = 0; TrCar *c = scene(R, 0, 5.f, 12.f, 35.f, 12.f);
    for (int i = 0; i < 60 * 3 && c->mode == TRM_DRIVE; i++) { frame(hd); if (c->chgK > 0.f && !wasChg) changes++; wasChg = c->chgK > 0.f;
      if (c->s > 120.f) { scene(R, c->lane, 5.f, 12.f, 35.f, 12.f); c = &T.car[0]; } }
    for (int rep = 0; rep < 12; rep++) { c = scene(R, 0, 5.f, 14.f, 35.f, 12.f); T.seed += 97u * rep;
      for (int i = 0; i < 60 * 9 && c->mode == TRM_DRIVE && c->s < 150.f; i++) { frame(hd); if (c->chgK > 0.f && !wasChg) changes++; wasChg = c->chgK > 0.f; } }
    check("10 смена", changes >= 2, "смен полосы за 12 заездов по 9 с: %d (оригинал: бросок раз в 5 с, шанс 1/2; прежнее: нет смены)", changes); }

  // ---- 16 (P03-15): гудок игрока — машина трафика в 25 м отвечает гудком через 0,5–0,8 с (шанс 10 % каждый кадр, пока гудок зажат)
  { int honks = 0;
    for (int rep = 0; rep < 20; rep++) { TrCar *c = scene(R, 0, 20.f, 8.f, 30.f, 12.f); T.seed += 131u * rep; (void)c;
      for (int i = 0; i < 90; i++) { trafficPlayerHorn(&T, i < 40, player.x, player.z); frame(hd); honks += popHonks() > 0; } }
    check("16 гудок", honks >= 5, "ответных гудков из 20 попыток: %d (оригинал: ответ есть; прежнее: гудит только после тарана)", honks); }

  // ---- круг 2: 3.4 повторный пинок по уже сбитому события не даёт; P09:3 «пробка» CarWay вместе с гудком; P11-02 машины трафика для предметов
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; T.nev = 0;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    trafficPedPlace(&T, &world, 0, px + 1.f, pz, 0.f); TrPed *e = &T.ped[0]; e->test = 0;
    int n1 = 0, n2 = 0; TrEvent ev;
    trafficKick(&T, px, e->y, pz, 1.5f, 1.f, 0.f); while (trafficPopEvent(&T, &ev)) n1 += ev.type == TR_EV_PED_KICK;
    e->mode = TRP_WALK;   // встал
    trafficKick(&T, px, e->y, pz, 1.5f, 1.f, 0.f); while (trafficPopEvent(&T, &ev)) n2 += ev.type == TR_EV_PED_KICK;
    check("3.4 пинок", n1 == 1 && n2 == 0, "событий пинка: первый %d, повторный по тому же %d (оригинал: 1 и 0; прежнее: 1 и 1)", n1, n2); }
  { TrCar *c = scene(R, 0, 5.f, 8.f, 35.f, 12.f); static Car obst; float q[3]; lanePos(&T, R, 0, 20.f, q);
    carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], hd); (void)c; int way = 0, honk = 0; TrEvent ev;
    for (int i = 0; i < 120; i++) { frame(hd); player.x = q[0]; player.z = q[2]; T.car[0].v = T.car[0].v; while (trafficPopEvent(&T, &ev)) { way += ev.type == TR_EV_CAR_WAY; honk += ev.type == TR_EV_CAR_HONK; } }
    check("P09:3 пробка", way >= 1 && honk >= 1, "игрок загородил машину трафика: событий «пробка» %d, гудков %d (оригинал: оба, раз в 5 с; прежнее: нет)", way, honk); }
  { TrCar *c = scene(R, 0, 60.f, 8.f, 35.f, 12.f); Car *out[12]; int n = trafficCarsNear(&T, c->car.x, c->car.z, 45.f, out, 12);
    check("P11-02 машины", n == 1 && out[0] == &c->car, "для предметов отдано машин трафика рядом: %d (оригинал: ломает любая машина; прежнее: 0)", n); }

  // ---- круг 3: смена полосы при помехе — только если расстояние между центрами минус 6 м не меньше 15 м (цель дальше 21 м), без паузы 5 с
  { int near_ = 0, far_ = 0;
    for (int k = 0; k < 2; k++) {
      TrCar *c = scene(R, 0, 5.f, 16.f, 35.f, 12.f); static Car obst; float oS = 5.f + (k ? 23.f : 14.f), q[3]; lanePos(&T, R, 0, oS, q);
      carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], hd); obst.vel[0] = obst.vel[1] = obst.vel[2] = 0.f; int started = 0;
      for (int i = 0; i < 40; i++) { T.extra[0] = &obst; T.nextra = 1; c->chgCool = 5.f; frame(hd); if (c->chgK > 0.f) started = 1; if (c->mode != TRM_DRIVE) break; }
      if (k) far_ = started; else near_ = started; }
    check("10 помеха 21 м", far_ == 1 && near_ == 0, "помеха в 23 м: смена полосы %d (нужна), в 14 м: %d (не нужна) (оригинал: цель дальше 21 м; прежнее: наоборот, только ближе ~18 м)", far_, near_); }

  // ---- круг 3: разговор — любые двое на разных тропах ближе 2 м (медленные) в 35 м от игрока; сам он не кончается
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    trafficPedPlace(&T, &world, 0, px + 5.f, pz, 0.f); TrPed *a = &T.ped[0]; a->test = 0; a->path = 0; a->seg = 0; a->dir = 1; a->speed = 1.f; a->mode = TRP_WALK; a->partner = -1;
    TrPed *b = &T.ped[1]; memset(b, 0, sizeof *b); b->model = a->model; b->x = px + 6.5f; b->z = pz; b->y = a->y; b->mode = TRP_WALK; b->path = 1; b->seg = 0; b->dir = 1; b->speed = 1.f; b->partner = -1; skinInit(&b->sp, &T.peds[b->model]);
    float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1}; int talked = 0, still = 0;
    for (int f = 0; f < 60 * 40; f++) { CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT); trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      if (a->mode == TRP_TALK && b->mode == TRP_TALK) { talked = 1; if (f > 60 * 30) still = 1; } }
    check("39 разговор", talked && still, "двое на разных тропах: заговорили %d, всё ещё говорят через 30 с %d (оригинал: любой пешеход рядом; разговор сам не кончается; прежнее: только одна тропа и конец в 1 из 3)", talked, still); }

  // ---- круг 3: заражение паникой — только идущий и только ближе 1,5 / 2 м
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    trafficPedPlace(&T, &world, 0, px + 8.f, pz, 0.f); TrPed *a = &T.ped[0]; a->test = 0; a->mode = TRP_WALK; a->path = 0; a->seg = 0; a->dir = 1; a->speed = 1.f; a->partner = -1;
    TrPed *b = &T.ped[1]; memset(b, 0, sizeof *b); b->model = a->model; b->x = px + 5.5f; b->z = pz; b->y = a->y; b->mode = TRP_WALK; b->path = 1; b->seg = 0; b->dir = 1; b->speed = 1.f; b->partner = -1; skinInit(&b->sp, &T.peds[b->model]);
    TrPed *b2 = &T.ped[2]; *b2 = *b; b2->x = px + 9.5f; b2->z = pz + 1.f; b2->path = 2;   // b2 — в 1,5 м от паникующего
    pedPanic(&T, a, px, pz); float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1};
    trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
    check("6 заражение", b->mode != TRP_PANIC && b2->mode == TRP_PANIC, "пешеход в 2,5 м от паникующего: режим %d (не паника), в 1,5 м: режим %d (паника = %d); оригинал заражает на 1,5 / 2 м, а не на 3 м", b->mode, b2->mode, TRP_PANIC); }

  // ---- круг 5, P03-19: стоящие машины при загрузке зоны — на L1 два типа: до 1 машины на загрузку зоны, всего не больше 2, каждая в своей зоне
  { T.npark = (int)T.hd->nPark; trafficReset(&T, &world); T.maxCars = 0; T.maxPeds = 0;   // предыдущие проверки обнуляли число мест
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz + 500.f}, fw[3] = {0, 0, 1};   // камера далеко и смотрит прочь: ни одно место не в кадре
    int first = 0, maxA = 0, ok = 1;
    for (int f = 0; f < 3; f++) { CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT); trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); }
    for (int i = 0; i < T.npark; i++) first += !T.park[i].gone;
    for (int z = 0; z < 8; z++) { T.zoneMask = 1u << z;
      for (int f = 0; f < 3; f++) { trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); }
      int a = 0; for (int i = 0; i < T.npark; i++) { a += !T.park[i].gone; if (!T.park[i].gone && T.parkd[i].zmask && !(T.parkd[i].zmask & T.zoneMask)) ok = 0; }
      if (a > maxA) maxA = a; }
    check("P03-19 зона", first == 1 && maxA <= 2 && maxA >= 1 && ok, "при первой загрузке машин %d (нужна 1), за 8 смен зоны не больше %d (≤ 2), стоят только в загруженной зоне=%d (прежнее: заняты обе сразу и места не меняются)", first, maxA, ok); }

  // ---- круг 5: 3.3 импульс пинка 400 под 45° (масса по умолчанию 100): скорость 2,83 м/с вбок и вверх; P15-06 рот: у говорящего движется, у слушающего нет
  { T.npark = 0; trafficReset(&T, &world); T.maxCars = 0; T.maxPeds = TR_MAXPED;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    trafficPedPlace(&T, &world, 0, px + 1.f, pz, 0.f); TrPed *e = &T.ped[0]; e->test = 0;
    trafficKick(&T, px, e->y, pz, 1.5f, 1.f, 0.f);
    check("3.3 пинок", fabsf(e->vx - 2.828f) < .1f && fabsf(e->vy - 2.828f) < .1f, "скорость отлёта после пинка: вбок %.2f, вверх %.2f (оригинал: импульс 400 / масса 100 под 45° = 2,83 и 2,83; прежнее: 5 и 3)", e->vx, e->vy); }
  { trafficReset(&T, &world); T.maxCars = 0; T.maxPeds = TR_MAXPED; T.npark = 0;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    trafficPedPlace(&T, &world, 0, px + 5.f, pz, 0.f); TrPed *a = &T.ped[0]; a->test = 0; a->path = 0; a->seg = 0; a->dir = 1; a->speed = 1.f; a->mode = TRP_WALK; a->partner = -1;
    TrPed *b = &T.ped[1]; memset(b, 0, sizeof *b); b->model = a->model; b->x = px + 6.f; b->z = pz; b->y = a->y; b->mode = TRP_WALK; b->path = 1; b->seg = 0; b->dir = 1; b->speed = 1.f; b->partner = -1; skinInit(&b->sp, &T.peds[b->model]);
    float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1}; float ja = 0.f, jb = 0.f;
    for (int f = 0; f < 60 * 5; f++) { CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT); trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      if (a->mode == TRP_TALK && b->mode == TRP_TALK) { ja = fmaxf(ja, a->speaker ? a->sp.jawOpen : b->sp.jawOpen); jb = fmaxf(jb, a->speaker ? b->sp.jawOpen : a->sp.jawOpen); } }
    check("P15-06 рот", ja > .02f && jb == 0.f, "рот открывается у говорящего до %.2f рад, у слушающего %.2f (оригинал: двигает рот говорящий; прежнее: рта нет)", ja, jb); }

  // ================================================================ КРУГ 6: перепроверка. Каждая проверка: число оригинала и «соседнее ложное» значение (ширина вместо полуширины, порог наоборот и т.п.)
  // ---- 7 / 48 (7.5, P02-40): допуск уворота пешехода — поперёк 2,0 м в каждую сторону (1,5 + 0,5; а не 1,0), вперёд путь за 1 с + 2,0 м; прыжок вбок — в сторону, где он стоит
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; T.seed = 777u;
    float px = 220.f, pz = 172.f; Car k; carInit(&k, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f); k.vel[0] = 0.f; k.vel[2] = 12.f; k.v = 12.f;
    float bx, bz; bodyCenter(&k, &bx, &bz); TrPed e; memset(&e, 0, sizeof e); e.mode = TRP_STAND; e.y = k.y;
    struct { float lat, a; int want; } cs[] = {{1.7f, 6.f, 1}, {2.3f, 6.f, 0}, {0.f, 13.5f, 1}, {0.f, 14.6f, 0}, {0.f, -1.0f, 0}, {1.9f, 13.9f, 0}};   // позади — нет; в углу прямоугольника (дальше 14 м по кругу) — нет   // путь за 1 с = 12 м, плюс 2 м = 14
    int okw = 1; char bad[80] = ""; for (int i = 0; i < 6; i++) { e.x = bx + cs[i].lat; e.z = bz + cs[i].a; if (pedWillHit(&e, &k, 0) != cs[i].want) { okw = 0; snprintf(bad, sizeof bad, "ошибся случай %d", i); } }
    check("7 допуск", okw, "ширина поперёк 2,0 м: ped на 1,7 м — угроза, на 2,3 — нет; вперёд 12 + 2 м: 13,5 — да, 14,6 — нет %s (прежнее: поперёк 1,0 м)", bad);
    // реакция: идущий в 1,7 м от оси сначала становится; через 0,3 с и дальше — съёживание или прыжок вбок в сторону, где стоит
    int stood = 0, side = 0, sideOk = 0, reacted = 0;
    for (int sg = -1; sg <= 1; sg += 2) for (int rep = 0; rep < 12; rep++) {
      TrPed *w = &T.ped[0]; memset(w, 0, sizeof *w); w->model = 0; w->x = bx + sg * 1.7f; w->z = bz + 6.f; w->y = k.y; w->mode = TRP_WALK; w->speed = 1.f; w->partner = -1; skinInit(&w->sp, &T.peds[0]);
      T.seed = mixSeed(100u + rep * 13u + (unsigned)(sg + 2) * 7919u); pedReactCar(&T, 0, &k, 1, DT); if (w->mode == TRP_STAND) stood++;
      for (int f = 0; f < 40 && w->mode == TRP_STAND; f++) pedReactCar(&T, 0, &k, 1, DT);
      if (w->mode == TRP_CRINGE || w->mode == TRP_SIDE) reacted++;
      if (w->mode == TRP_SIDE) { side++; sideOk += w->pDirX * (float)sg > .9f; } }
    check("7 реакция", stood == 24 && reacted == 24 && side >= 3 && sideOk == side, "сначала встал %d из 24, затем съёжился/прыгнул %d из 24, прыжков вбок %d, из них в сторону, где стоит пешеход: %d (оригинал: стоп, 0,3 с, бросок 50/50; прежнее: без реакции)", stood, reacted, side, sideOk); }

  // ---- 12 (P02-37): игрок ближе 1,41 м — стоп лицом к нему; в 1,5 м — идёт дальше
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f); float pp[3] = {px, player.y, pz}; int fs[2];
    for (int k = 0; k < 2; k++) { TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = px + (k ? 1.5f : 1.3f); e->z = pz + 5.f * 0.f; e->y = player.y; e->mode = TRP_WALK; e->speed = 1.f; e->partner = -1; e->path = 0; e->seg = 0; e->dir = 1; skinInit(&e->sp, &T.peds[0]);
      pedStep(&T, &world, 0, &player, 0, pp, DT); fs[k] = e->mode == TRP_FACE; }
    check("12 лицом", fs[0] == 1 && fs[1] == 0, "в 1,3 м стоп лицом к игроку: %d; в 1,5 м идёт дальше: %d (оригинал 1,41 м; прежнее: нет остановки)", fs[0], !fs[1]); }

  // ---- 46 (P02-38): точка тропы пройдена, когда до неё меньше 0,5 м
  { int P = -1; for (int i = 0; i < (int)T.hd->nPath && P < 0; i++) { const float *a = pathP(&T, i, 0), *b = pathP(&T, i, 1); if (T.path[i].n >= 2 && hypotf(b[0] - a[0], b[2] - a[2]) > 3.f) P = i; }
    int sg[2] = {-1, -1};
    for (int k = 0; k < 2 && P >= 0; k++) { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
      const float *a = pathP(&T, P, 0), *b = pathP(&T, P, 1); float L = hypotf(b[0] - a[0], b[2] - a[2]), ux = (b[0] - a[0]) / L, uz = (b[2] - a[2]) / L, off = k ? .6f : .4f;
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = b[0] - ux * off; e->z = b[2] - uz * off; e->y = b[1]; e->h = atan2f(ux, uz); e->mode = TRP_WALK; e->speed = 1.f; e->partner = -1; e->path = P; e->seg = 0; e->dir = 1; skinInit(&e->sp, &T.peds[0]);
      carInit(&player, NULL, &world, e->x + 300.f, ground(e->x + 300.f, e->z, 50.f), e->z, 0.f); float pp[3] = {player.x, player.y, player.z}; pedStep(&T, &world, 0, &player, 0, pp, DT); sg[k] = e->seg; }
    check("46 тропа", P >= 0 && sg[0] == 1 && sg[1] == 0, "в 0,4 м до точки: следующий отрезок (%d, нужен 1); в 0,6 м: ещё не дошёл (%d, нужен 0) (оригинал: пройдена в 0,5 м; прежнее: только в самой точке)", sg[0], sg[1]); }

  // ---- 50 (P02-42) и 8 (7.6): лежащий от машины трафика быстрее 1 м/с — толчок 1,4 м/с по нормали; едущий по рельсам сбивает, отлёт вверх (6…11) × скорость / 28
  { float dsp[2]; for (int k = 0; k < 2; k++) { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
      Car c; carInit(&c, NULL, &world, 220.f, ground(220.f, 172.f, 50.f), 172.f, 0.f); float sp = k ? .8f : 5.f; c.vel[0] = 0.f; c.vel[2] = sp; c.v = sp;
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = 220.f; e->z = 172.f + c.p.bodyOfs + c.p.halfL - .1f; e->y = c.y; e->mode = TRP_DOWN; e->partner = -1; skinInit(&e->sp, &T.peds[0]);
      float x0 = e->x, z0 = e->z; pedHitBy(&T, 0, &c, DT); dsp[k] = (e->mode == TRP_DOWN) ? hypotf(e->x - x0, e->z - z0) : 9.f; }
    check("50 толчок", fabsf(dsp[0] - 1.4f * DT) < .004f && dsp[1] < 1e-4f, "лежащий при 5 м/с сдвинут на %.4f м за кадр (1,4 м/с = %.4f), при 0,8 м/с: %.4f (оригинал: толчок только быстрее 1 м/с)", dsp[0], 1.4f * DT, dsp[1]); }
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; T.seed = 4242u; float lo = 1e9f, hi = -1e9f, hmax = 0.f; int n = 0;
    for (int rep = 0; rep < 40; rep++) { Car c; carInit(&c, NULL, &world, 220.f, ground(220.f, 172.f, 50.f), 172.f, 0.f); c.vel[0] = 0.f; c.vel[2] = 14.f; c.v = 14.f;
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = 220.f; e->z = 172.f + c.p.bodyOfs + c.p.halfL - .1f; e->y = c.y; e->mode = TRP_WALK; e->partner = -1; e->speed = 1.f; skinInit(&e->sp, &T.peds[0]);
      pedHitBy(&T, 0, &c, DT); if (e->mode == TRP_FLY) { lo = fminf(lo, e->vy); hi = fmaxf(hi, e->vy); hmax = fmaxf(hmax, hypotf(e->vx, e->vz)); n++; } }
    check("8 отлёт трафика", n == 40 && lo >= 6.f * 14.f / 28.f - .02f && hi <= 11.f * 14.f / 28.f + .02f && hi - lo > .5f && hmax <= 2.05f, "сбит %d из 40, вертикальная скорость %.2f…%.2f м/с, вбок не больше %.2f м/с (оригинал: (6…11) × 14 / 28 = 3,0…5,5 вверх и курс машины + нормаль = не больше 2 вбок; прежнее: вбок со скоростью машины 14)", n, lo, hi, hmax); }

  // ---- 7.7 (ps2-twin, запись PS2): сбитый лежит ничком до подъёма ~11,4 с от удара (570 кадров развёртки), клип get_up дальше; прежнее — 3 с лежания
  { float tot[3] = {0, 0, 0}, lie[3] = {0, 0, 0}; int stood = 0;
    for (int rep = 0; rep < 3; rep++) { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; T.seed = 777u + rep;
      Car c; carInit(&c, NULL, &world, 220.f, ground(220.f, 172.f, 50.f), 172.f, 0.f); float sp = rep == 0 ? 10.f : rep == 1 ? 5.f : 15.f; c.vel[0] = 0.f; c.vel[2] = sp; c.v = sp;
      carInit(&player, NULL, &world, 520.f, ground(520.f, 172.f, 50.f), 172.f, 0.f); float pp[3] = {player.x, player.y, player.z};
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = 220.f; e->z = 172.f + c.p.bodyOfs + c.p.halfL - .1f; e->y = c.y; e->mode = TRP_WALK; e->partner = -1; e->speed = 1.f; skinInit(&e->sp, &T.peds[0]);
      pedHitBy(&T, 0, &c, DT); float t = 0.f, downAt = -1.f; int got = 0;
      for (int f = 0; f < 30 * 30; f++) { pedStep(&T, &world, 0, &player, 0, pp, DT); t += DT;
        if (e->mode == TRP_DOWN && downAt < 0.f) downAt = t;
        if (e->mode == TRP_GETUP) { tot[rep] = t; lie[rep] = t - downAt; got = 1; break; } if (e->mode == TRP_OFF) break; }
      stood += got; }
    int ok = stood == 3; for (int k = 0; k < 3; k++) ok = ok && tot[k] > 10.8f && tot[k] < 12.2f && lie[k] > 10.2f;
    check("7.7 лежание", ok, "сбит на 10 / 5 / 15 м/с: до подъёма с удара %.1f / %.1f / %.1f с, лежит после приземления %.1f / %.1f / %.1f с (запись PS2 на 10 м/с: ~11,4 с от удара; прежнее: лежит 3,0 с)", tot[0], tot[1], tot[2], lie[0], lie[1], lie[2]); }

  // ---- 3 / 5 / 51 / 53 (7.1, 7.3, P03-14, P03-17): 7 пешеходов (не 6), разных моделей не больше 4, на тропе не больше 3, друг от друга не ближе 4 м; скорость 1,0 / 1,5 / 3,0 = 40 / 40 / 20 %
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; T.loadT = 0.f; T.seed = 31337u;
    float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f); float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1};
    for (int a = 0; a < 4000; a++) spawnPedsSv(&T, &world, pp, eye, fw);
    int alive = 0, kinds = 0, seen[TR_MAXPEDMODEL] = {0}, perPath[256] = {0}, maxPath = 0; float dmin = 1e9f;
    for (int i = 0; i < TR_MAXPED; i++) if (T.ped[i].mode != TRP_OFF) { alive++; if (!seen[T.ped[i].model]) { seen[T.ped[i].model] = 1; kinds++; } if (T.ped[i].path < 256) { perPath[T.ped[i].path]++; if (perPath[T.ped[i].path] > maxPath) maxPath = perPath[T.ped[i].path]; }
      for (int j = i + 1; j < TR_MAXPED; j++) if (T.ped[j].mode != TRP_OFF) dmin = fminf(dmin, hypotf(T.ped[i].x - T.ped[j].x, T.ped[i].z - T.ped[j].z)); }
    int dirs = 1; for (int i = 0; i < TR_MAXPED; i++) if (T.ped[i].mode != TRP_OFF && T.ped[i].homeSpeed > 0.f && T.ped[i].dir != 1) dirs = 0;   // по новому правилу направление одно (страховка PSP — со случайным)
    check("3 семь пешеходов", dirs && alive == 7 && kinds <= 4 && maxPath <= 3 && dmin >= 3.9f, "живых %d (нужно 7; прежнее 6), разных моделей %d (≤ 4), на одной тропе не больше %d (≤ 3), ближайшие друг к другу %.1f м (≥ 4)", alive, kinds, maxPath, dmin);
    int cnt[3] = {0, 0, 0}, tot = 0; T.seed = 99991u;
    for (int rep = 0; rep < 600; rep++) { for (int i = 0; i < TR_MAXPED; i++) T.ped[i].mode = TRP_OFF; T.loadT = 0.f; T.seed = mixSeed(rep + 5u);
      for (int a = 0; a < 200; a++) { spawnPedsSv(&T, &world, pp, eye, fw); if (T.ped[0].mode != TRP_OFF) break; }
      if (T.ped[0].mode != TRP_OFF && T.ped[0].homeSpeed > 0.f) { tot++; float s = T.ped[0].speed; cnt[s < 1.2f ? 0 : s < 2.f ? 1 : 2]++; } }
    check("5 скорость", tot >= 300 && fabsf(cnt[0] / (float)tot - .4f) < .1f && fabsf(cnt[1] / (float)tot - .4f) < .1f && fabsf(cnt[2] / (float)tot - .2f) < .08f, "из %d рождений: 1,0 м/с %d, 1,5 м/с %d, бег 3,0 м/с %d (оригинал 40/40/20 %%; прежнее: скорость модели)", tot, cnt[0], cnt[1], cnt[2]); }

  // ---- 18 (P03-21): первые 3 с трафик не удаляется; потом дальше 75 м от точки «игрок + 40 м по взгляду» убирается независимо от видимости; ближе — остаётся
  { int res[3]; for (int k = 0; k < 3; k++) { TrCar *c = scene(R, 0, 30.f, 0.f, 30.f, 12.f); float h = roadDir(R, 30.f), fx = sinf(h), fz = cosf(h);
      float ax = c->car.x, az = c->car.z, dist = k == 2 ? 20.f : 90.f;   // игрок впереди машины: центр удаления = игрок + 40 м; k=2 → центр в 60 м от машины, k=0,1 → в 130 м
      float ppx = ax + fx * dist, ppz = az + fz * dist; carInit(&player, NULL, &world, ppx, ground(ppx, ppz, 50.f), ppz, h);
      T.loadT = k == 0 ? 0.f : 3.5f; T.fresh = 0; float pp[3] = {ppx, player.y, ppz}, eye[3] = {ppx - fx * 6.f, player.y + 3.f, ppz - fz * 6.f}, fw[3] = {fx, 0.f, fz};
      for (int f = 0; f < 30; f++) { T.scanT = 1e9f; T.car[0].v = 0.f; if (k == 0) T.loadT = 0.f; trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); }
      res[k] = T.car[0].mode != TRM_OFF; }
    check("18 удаление", res[0] == 1 && res[1] == 0 && res[2] == 1, "через 30 кадров: в первые 3 с (центр в 130 м) машина на месте %d (нужно 1), после 3 с в 130 м: на месте %d (нужно 0), после 3 с в 60 м: на месте %d (нужно 1) (оригинал: 75 м без видимости; прежнее: убирало с первого кадра)", res[0], res[1], res[2]); }

  // ---- 23 / 22 (T1, T2): коридор перед машиной — машина: центр не дальше 3 м от оси, пешеход: не дальше 2 м; запас остановки пешеходу 3,0 м между центрами
  { enum { NL = 10 }; float latc[NL] = {2.2f, 2.6f, 2.9f, 3.15f, 3.5f, 1.4f, 1.7f, 1.9f, 2.15f, 2.4f}; float vend[NL]; float stopPed = 0;
    for (int k = 0; k < NL; k++) { int isPed = k >= 5; float lat = latc[k];
      TrRoad *RR = (TrRoad *)&T.road[R]; unsigned char md = RR->maxCars; RR->maxCars = 0;
      TrCar *c = scene(R, 0, 5.f, 10.f, 35.f, 12.f); float s0 = 90.f, q[3]; lanePos(&T, R, 0, s0, q); float h = roadDir(R, s0), lx = cosf(h), lz = -sinf(h);
      static Car obst; T.nextra = 0;
      if (!isPed) { carInit(&obst, NULL, &world, q[0] + lx * lat, ground(q[0], q[2], q[1]), q[2] + lz * lat, h); }
      else { T.maxPeds = 1; trafficPedPlace(&T, &world, 0, q[0] + lx * lat, q[2] + lz * lat, h); }
      float vmin = 99.f; for (int i = 0; i < 60 * 14 && c->mode == TRM_DRIVE && c->s < s0 + 5.f; i++) { if (!isPed) { T.extra[0] = &obst; T.nextra = 1; } frame(hd); if (i > 120) vmin = fminf(vmin, c->v); if (c->v < .05f && i > 60) break; }
      vend[k] = vmin; if (k == 7) { float bx, bz; bodyCenter(&c->car, &bx, &bz); stopPed = hypotf(q[0] + lx * lat - bx, q[2] + lz * lat - bz); } RR->maxCars = md; }
    if (getenv("G02DBG")) { for (int k = 0; k < NL; k++) fprintf(stderr, "dbg lat %.2f %s v=%.1f\n", latc[k], k >= 5 ? "пеш" : "маш", vend[k]); }
    check("23 коридор", vend[2] < .1f && vend[3] > 6.f && vend[7] < .1f && vend[8] > 6.f, "машина в 2,9 м от оси: min скорость %.1f (стоит), в 3,15 м: %.1f (проезжает); пешеход в 1,9 м: %.1f (стоит), в 2,15 м: %.1f (проезжает) (оригинал 3 м / 2 м от оси)", vend[2], vend[3], vend[7], vend[8]);
    check("22 запас пешеход", fabsf(stopPed - SV_STOP_PED) < 1.2f, "остановилась в %.2f м от центра пешехода (оригинал 3,0; прежнее: бампер 3 м + полдлины)", stopPed); }

  // ---- 10 порог на границе: помеха в 20 м — смены нет, в 22 м — есть (оригинал: расстояние центров − 6 м ≥ 15 м)
  { int st[2]; for (int k = 0; k < 2; k++) { TrCar *c = scene(R, 0, 5.f, 16.f, 35.f, 12.f); static Car obst; float q[3]; lanePos(&T, R, 0, 5.f + (k ? 22.f : 20.f), q);
      carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], hd); obst.vel[0] = obst.vel[1] = obst.vel[2] = 0.f; int started = 0;
      for (int i = 0; i < 40; i++) { T.extra[0] = &obst; T.nextra = 1; c->chgCool = 5.f; frame(hd); if (c->chgK > 0.f) started = 1; if (c->mode != TRM_DRIVE) break; } st[k] = started; }
    check("10 граница 21 м", st[1] == 1 && st[0] == 0, "помеха в 22 м: смена %d (нужна), в 20 м: %d (не нужна) (оригинал: 21 м)", st[1], st[0]); }

  // ---- 39 границы разговора: медленные 2 м, быстрые 1,5 м; дальше — не заговорят
  { int tk[4]; float lim[4] = {1.f, 1.f, 1.5f, 1.5f}, dd[4] = {1.9f, 2.2f, 1.4f, 1.8f};
    for (int k = 0; k < 4; k++) { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
      TrPed *a = &T.ped[0]; memset(a, 0, sizeof *a); a->model = 0; a->x = px + 6.f; a->z = pz + 4.f; a->y = player.y; a->mode = TRP_WALK; a->speed = lim[k]; a->partner = -1; a->path = 0; skinInit(&a->sp, &T.peds[0]);
      TrPed *b = &T.ped[1]; *b = *a; b->x = a->x + dd[k]; b->path = 1; b->partner = -1; float pp[3] = {px, player.y, pz}; pedTalkScan(&T, pp); tk[k] = a->mode == TRP_TALK; }
    check("39 границы", tk[0] == 1 && tk[1] == 0 && tk[2] == 1 && tk[3] == 0, "медленные: 1,9 м — говорят %d, 2,2 м — нет %d; быстрые: 1,4 м — говорят %d, 1,8 м — нет %d (оригинал 2 м / 1,5 м)", tk[0], !tk[1], tk[2], !tk[3]); }

  // ---- 34 (P18-09): плотность дороги делится на число полос, остаток — последней
  { TrRoad *RR = (TrRoad *)&T.road[R]; unsigned char ml = RR->lanes, mc = RR->maxCars; int d2[2], d3[3], d1;
    RR->lanes = 2; RR->maxCars = 5; d2[0] = laneDens(&T, R, 0); d2[1] = laneDens(&T, R, 1);
    RR->lanes = 3; d3[0] = laneDens(&T, R, 0); d3[1] = laneDens(&T, R, 1); d3[2] = laneDens(&T, R, 2); RR->lanes = 1; d1 = laneDens(&T, R, 0); RR->lanes = ml; RR->maxCars = mc;
    check("34 плотность", d2[0] == 2 && d2[1] == 3 && d3[0] == 1 && d3[1] == 1 && d3[2] == 3 && d1 == 5, "5 машин на 2 полосы: %d + %d; на 3 полосы: %d + %d + %d; на одну: %d (оригинал: поровну, остаток последней; неверно было бы %d на каждую)", d2[0], d2[1], d3[0], d3[1], d3[2], d1, 5); }

  // ---- 62 / 24 / 61 / 31 (Д3, T3, Д2, P16-04): внутренние точки кривой перекрёстка на трети расстояния; нет места ни на одной полосе выезда — стоит и повторяет выбор раз в 5 с
  { int RJ = -1; for (uint32_t r = 0; r < T.hd->nRoad && RJ < 0; r++) if (roadOk(&T, (int)r) && T.road[r].length > 60.f && T.junc[T.road[r].endJ].nOut > 0 && T.junc[T.road[r].endJ].behaviour != 1) {
        int ok = 0; for (int k = 0; k < T.junc[T.road[r].endJ].nOut; k++) { int o = T.out[T.junc[T.road[r].endJ].firstOut + k]; ok |= roadOk(&T, o) && o != (int)T.road[r].rev && T.road[o].endJ != T.road[r].startJ; } if (ok) RJ = (int)r; }
    if (RJ < 0) check("62 треть", 0, "нет подходящего перекрёстка");
    else {
      float L = laneLen(&T, RJ, 0); TrCar *c = scene(RJ, 0, L - 20.f, 8.f, L - 60.f, 14.f); T.seed = 5150u; int rc = chooseNext(&T, c);
      float d = hypotf(hypotf(c->bz[3][0] - c->bz[0][0], c->bz[3][1] - c->bz[0][1]), c->bz[3][2] - c->bz[0][2]);
      float h1 = hypotf(hypotf(c->bz[1][0] - c->bz[0][0], c->bz[1][1] - c->bz[0][1]), c->bz[1][2] - c->bz[0][2]), h2 = hypotf(hypotf(c->bz[3][0] - c->bz[2][0], c->bz[3][1] - c->bz[2][1]), c->bz[3][2] - c->bz[2][2]);
      check("62 треть", rc == 1 && d > 3.f && fabsf(h1 / d - 1.f / 3.f) < .01f && fabsf(h2 / d - 1.f / 3.f) < .01f, "внутренние точки кривой от концов: %.3f и %.3f расстояния между концами (нужна 1/3 = 0,333; прежнее 0,4)", h1 / d, h2 / d);
      // нет места: все выезды заняты (плотность 0) — выбор не берётся (−1) и повторяется раз в 5 с; потом место появилось — берёт не раньше конца паузы
      { const TrJunc *J = &T.junc[T.road[RJ].endJ]; unsigned char sv[16]; int no = J->nOut < 16 ? J->nOut : 16; for (int k = 0; k < no; k++) { TrRoad *Ro = (TrRoad *)&T.road[T.out[J->firstOut + k]]; sv[k] = Ro->maxCars; Ro->maxCars = 0; }
        TrCar *c2 = scene(RJ, 0, L - 25.f, 8.f, L - 60.f, 14.f); float h = roadDir(RJ, L - 60.f), tFirst = -1, tTake = -1; int freed = 0;
        for (int i = 0; i < 60 * 12; i++) { frame(h); float tm = i * DT;
          if (tFirst < 0 && c2->retryT > 0.f) tFirst = tm;
          if (tFirst >= 0 && !freed && tm > tFirst + 1.5f) { for (int k = 0; k < no; k++) ((TrRoad *)&T.road[T.out[J->firstOut + k]])->maxCars = sv[k]; freed = 1; }
          if (freed && tTake < 0 && c2->next > 0) tTake = tm; }
        for (int k = 0; k < no; k++) ((TrRoad *)&T.road[T.out[J->firstOut + k]])->maxCars = sv[k];
        check("61 повтор 5 с", tFirst > 0 && tTake > 0 && tTake - tFirst > 4.5f && tTake - tFirst < 5.6f, "первый отказ в %.2f с, выезд взят в %.2f с (через %.2f с; место освободили через 1,5 с) (оригинал: повтор выбора раз в 5 с; прежнее: брал сразу)", tFirst, tTake, tTake - tFirst); }
    } }

  // ---- 21 / 44 (P03-25, 8.7): после удара машина лежит, возврат на свою полосу не раньше чем через 3 с; помеха на полосе — ждёт заново 3 с
  { float tBack[2] = {-1, -1};
    for (int k = 0; k < 2; k++) { TrCar *c = scene(R, 0, 60.f, 0.f, 40.f, 14.f); static Car obst; float q[3]; lanePos(&T, R, 0, 60.f + 10.f, q); carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], hd);
      c->mode = TRM_PHYS; c->stillT = 0.f; c->outLaneT = 0.f; c->ramStrong = 0; c->noRejoin = 0; int removed = 0;
      for (int i = 0; i < 60 * 12 && tBack[k] < 0; i++) { float tm = i * DT; if (k && !removed) { T.extra[0] = &obst; T.nextra = 1; if (tm > 4.f) { removed = 1; T.nextra = 0; } } else if (!k) T.nextra = 0; else T.nextra = 0;
        frame(hd); if (c->mode == TRM_DRIVE) tBack[k] = tm; if (getenv("G02DBG") && (i % 60 == 59) && k == 0) { int r_, l_; float s_; fprintf(stderr, "dbg t=%.1f mode=%d stillT=%.2f outLaneT=%.2f noRejoin=%d destroyed=%d check=%d wheels=%d\n", tm, c->mode, c->stillT, c->outLaneT, c->noRejoin, c->car.destroyed, svRejoinCheck(&T, c, &r_, &l_, &s_), c->car.wheels); } } }
    check("21 возврат 3 с", tBack[0] > 2.9f && tBack[0] < 3.8f && tBack[1] > 5.9f && tBack[1] < 7.2f, "лежит и свободно: вернулась через %.2f с; на полосе помеха до 4 с: через %.2f с (оригинал: не раньше 3 с, при помехе таймер заново 3 с; прежнее: 1,8 с / оглушение 13,8 с)", tBack[0], tBack[1]); }

  // ---- 21 (P03-25) уточнение по оригиналу: лежит ближе 10 м до конца дороги — вернуться на полосу нельзя совсем; дальше 10 м — возвращается
  { float L = laneLen(&T, R, 0); int back[2]; for (int k = 0; k < 2; k++) { TrCar *c = scene(R, 0, k ? L - 40.f : L - 6.f, 0.f, k ? L - 80.f : L - 60.f, 25.f); c->mode = TRM_PHYS; c->stillT = 0.f; c->outLaneT = 0.f; c->ramStrong = 0; c->noRejoin = 0; back[k] = 0;
      for (int i = 0; i < 60 * 8 && !back[k]; i++) { frame(hd); if (c->mode == TRM_DRIVE) back[k] = 1; if (c->mode == TRM_OFF) break; } }
    check("21 у конца дороги", back[0] == 0 && back[1] == 1, "лежит в 6 м до конца дороги: вернулась %d (нужно 0), в 40 м: %d (нужно 1) (оригинал: цель в 10 м впереди, нет дороги — не оживает)", back[0], back[1]); }

  // ---- 41 (8.3): пешеходу трафик не сигналит; машине игрока в коридоре ближе 10 м — сигналит
  { TrRoad *RR = (TrRoad *)&T.road[R]; unsigned char md = RR->maxCars; RR->maxCars = 0; int hp = 0, wp = 0, hc = 0, wc = 0; TrEvent ev;
    { TrCar *c = scene(R, 0, 5.f, 2.f, 35.f, 12.f); float q[3]; lanePos(&T, R, 0, 12.f, q); T.maxPeds = 1; trafficPedPlace(&T, &world, 0, q[0], q[2], hd); (void)c;
      for (int i = 0; i < 60 * 6; i++) { frame(hd); while (trafficPopEvent(&T, &ev)) { hp += ev.type == TR_EV_CAR_HONK; wp += ev.type == TR_EV_CAR_WAY; } } }
    { TrCar *c = scene(R, 0, 5.f, 8.f, 35.f, 12.f); static Car obst; float q[3]; lanePos(&T, R, 0, 20.f, q); carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], hd); (void)c;
      for (int i = 0; i < 60 * 6; i++) { frame(hd); player.x = q[0]; player.z = q[2]; while (trafficPopEvent(&T, &ev)) { hc += ev.type == TR_EV_CAR_HONK; wc += ev.type == TR_EV_CAR_WAY; } } }
    check("41 пешеходу", hp == 0 && wp == 0 && hc >= 1 && wc >= 1, "у пешехода в 3 м: гудков %d, «пробка» %d (нужно 0, 0); перед машиной игрока: гудков %d, «пробка» %d (нужно ≥ 1) (оригинал: пешеходам не сигналит, машине игрока ближе 10 м — да)", hp, wp, hc, wc); RR->maxCars = md; }

  // ---- 55 / 20 (P03-24, P03-23): рождение трафика: скорость желаемая, но 0, если точка ближе 10 м до конца дороги; кольцо вокруг центра 55…75 м
  { int nOk = 0, nEnd = 0, bad = 0, ring = 0, tot = 0;
    for (int rep = 0; rep < 160; rep++) { trafficReset(&T, &world); T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR; T.npark = 0; T.maxPeds = 0; T.seed = mixSeed(rep + 11u); T.loadT = 10.f; T.fresh = 0;
      int r = straightRoad(60.f + (rep % 5) * 30.f, -1, 0); if (r < 0) r = R; float s = 20.f + (rep * 37) % ((int)T.road[r].length - 40), h = roadDir(r, s), q[3]; lanePos(&T, r, 0, s, q);
      carInit(&player, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], h); float pp[3] = {q[0], player.y, q[2]}, eye[3] = {q[0] - sinf(h) * 6.f, player.y + 3.f, q[2] - cosf(h) * 6.f}, fw[3] = {sinf(h), 0, cosf(h)};
      T.scanT = 0.f; trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      float cen[2] = {pp[0] + fw[0] * 40.f, pp[2] + fw[2] * 40.f};
      for (int i = 0; i < TR_MAXCAR; i++) { TrCar *c = &T.car[i]; if (c->mode != TRM_DRIVE || c->elem != 0) continue; tot++;
        float rem = laneLen(&T, c->road, c->lane) - c->s, dd = hypotf(c->car.x - cen[0], c->car.z - cen[1]); ring += dd > 59.f && dd < 71.f;
        if (rem < 10.f) { nEnd++; bad += c->v > .2f; } else { nOk++; bad += fabsf(c->v - 16.667f) > .3f; } } }
    check("55 скорость рождения", tot > 40 && bad == 0 && nOk > 20, "рождено %d: ближе 10 м до конца дороги %d (скорость 0), остальных %d (скорость 16,67); неверной скорости: %d (оригинал 60 км/ч, у конца дороги 0)", tot, nEnd, nOk, bad);
    check("20 кольцо", tot > 40 && ring * 10 >= tot * 9, "рождено %d, из них в кольце 65±5 м от центра (игрок + 40 м): %d (оригинал: пересечение полосы с кругом 65 м)", tot, ring); }

  // ---- 34 (P18-09), действие плотности: дорога с плотностью 0 — на ней машины не рождаются
  { static unsigned char sv[4096]; int nr = (int)T.hd->nRoad < 4096 ? (int)T.hd->nRoad : 4096; for (int r = 0; r < nr; r++) { sv[r] = T.road[r].maxCars; ((TrRoad *)&T.road[r])->maxCars = 0; }
    int born = 0; for (int rep = 0; rep < 12; rep++) { trafficReset(&T, &world); T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR; T.npark = 0; T.maxPeds = 0; T.seed = mixSeed(rep + 3u); T.loadT = 10.f; T.fresh = 0; T.scanT = 0.f;
      int r = straightRoad(60.f, -1, 0); float q[3]; lanePos(&T, r, 0, 40.f, q); float h = roadDir(r, 40.f); carInit(&player, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], h);
      float pp[3] = {q[0], player.y, q[2]}, eye[3] = {q[0] - sinf(h) * 6.f, player.y + 3.f, q[2] - cosf(h) * 6.f}, fw[3] = {sinf(h), 0, cosf(h)}; trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      for (int i = 0; i < TR_MAXCAR; i++) born += T.car[i].mode != TRM_OFF; }
    for (int r = 0; r < nr; r++) ((TrRoad *)&T.road[r])->maxCars = sv[r];
    check("34 плотность 0", born == 0, "на дорогах с плотностью 0 рождено машин: %d (нужно 0) (оригинал: плотность дороги ограничивает рождение; прежнее: рождались везде)", born); }

  // ---- 9 пороги (8.1, T4): игрок быстрее 40 км/ч (11,1 м/с), мы быстрее 30 км/ч (8,33 м/с), дальше 10 м
  { int a = dodgeCase(R, hd, 22.f, 13.f, 90.f), pLow = dodgeCase(R, hd, 10.f, 13.f, 90.f), pOk = dodgeCase(R, hd, 12.f, 13.f, 90.f), cLow = dodgeCase(R, hd, 22.f, 8.f, 90.f), cOk = dodgeCase(R, hd, 22.f, 9.f, 90.f), close = dodgeCase(R, hd, 22.f, 13.f, 8.f);
    check("9 пороги", a && pOk && cOk && !pLow && !cLow && !close, "игрок 22/12 м/с: уход %d/%d; игрок 10 м/с (< 40 км/ч): %d (нет); машина 9 м/с: %d, 8 м/с (< 30 км/ч): %d (нет); игрок в 8 м (< 10 м): %d (нет) (оригинал 40 / 30 км/ч, дальше 10 м)", a, pOk, pLow, cOk, cLow, close); }

  // ---- 30 (P16-02): перекрёсток без управления — машина не ждёт, пока кто-то освободит перекрёсток (идёт 12 м/с, не останавливается)
  { int RJ = -1; for (uint32_t j = 0; j < T.hd->nJunc && RJ < 0; j++) if (T.junc[j].behaviour == 0) for (uint32_t r = 0; r < T.hd->nRoad; r++) if (roadOk(&T, (int)r) && T.road[r].endJ == j && T.road[r].length > 60.f && T.junc[j].nOut > 0) { RJ = (int)r; break; }
    float vmin = 99.f; int entered = 0;
    if (RJ >= 0) { float L = laneLen(&T, RJ, 0); TrCar *c = scene(RJ, 0, L - 45.f, 14.f, L - 70.f, 14.f); float h = roadDir(RJ, L - 70.f); T.car[0].chgCool = 1e9f;
      for (int i = 0; i < 60 * 8 && c->mode == TRM_DRIVE; i++) { frame(h); if (c->elem == 1) { entered = 1; vmin = fminf(vmin, c->v); } if (c->elem == 1 && c->s > c->bzLen * .6f) break; } }
    check("30 без управления", RJ >= 0 && entered && vmin > 3.4f, "перекрёсток без управления: въехала %d, наименьшая скорость в перекрёстке %.1f м/с (нужна > 3,4: не стоит и не ждёт; потолок типа 0 — 12, на острой кривой ниже по №1092) (оригинал P16-02; прежнее: ждала освобождения перекрёстка)", entered, vmin); }

  // ---- 43 (8.5): торможение к препятствию равномерное: замедление v² / (2 × (расстояние центров − 5 м)) с глубины 25 м (1,5 с × 16,7 м/с) — около 6,9 м/с²
  { TrRoad *RR = (TrRoad *)&T.road[R]; unsigned char md = RR->maxCars; RR->maxCars = 0; TrCar *c = scene(R, 0, 5.f, 16.667f, 35.f, 12.f); static Car obst; float s0 = 110.f, q[3]; lanePos(&T, R, 0, s0, q);
    carInit(&obst, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], hd); float t1 = -1, t2 = -1, v1 = 0, v2 = 0;
    for (int i = 0; i < 60 * 12 && c->mode == TRM_DRIVE; i++) { T.extra[0] = &obst; T.nextra = 1; frame(hd); float tm = i * DT; if (t1 < 0 && c->v < 14.f) { t1 = tm; v1 = c->v; } if (t2 < 0 && c->v < 3.f) { t2 = tm; v2 = c->v; break; } }
    float a = t1 >= 0 && t2 > t1 ? (v1 - v2) / (t2 - t1) : 0.f;
    check("43 торможение", fabsf(a - 6.94f) < .9f, "среднее замедление при торможении с 14 до 3 м/с: %.2f м/с² (оригинал v² / 2 / (25 − 5) = 6,94; прежнее: 6,0)", a); RR->maxCars = md; }

  // ---- №1092: поперечное ускорение на кривых перекрёстков не больше ~4,5 м/с² (в TEST83 до 20 и больше при радиусе 6–8 м)
  { float worst = 0.f; int curves = 0, sharp = 0;
    for (uint32_t r = 0; r < T.hd->nRoad && curves < 14; r++) {
      if (!roadOk(&T, (int)r) || T.road[r].length < 50.f || T.junc[T.road[r].endJ].nOut < 1 || T.junc[T.road[r].endJ].behaviour == 1) continue;
      float L = laneLen(&T, (int)r, 0); TrCar *c = scene((int)r, 0, L - 45.f, 14.f, L - 90.f, 14.f > 0 ? 25.f : 0.f); c->chgCool = 1e9f; float h = roadDir((int)r, L - 90.f), hist[16]; int hn = 0, inJ = 0; T.seed = mixSeed((unsigned)r + 9u);
      for (int i = 0; i < 60 * 9 && c->mode == TRM_DRIVE; i++) { frame(h); hist[hn & 15] = c->car.h; hn++;
        if (c->elem == 1) { inJ = 1; if (hn > 16 && c->v > .5f) { float a = c->v * fabsf(wrapPi(c->car.h - hist[hn & 15])) / (15.f * DT); if (a > worst) worst = a; } } else if (inJ) break; }
      if (inJ) { curves++; if (bzMinR(c->bz) < 14.f) sharp++; } }
    check("1092 поворот", curves >= 4 && worst < 5.5f, "кривых перекрёстка проверено %d (острых, радиус < 14 м: %d), наибольшее поперечное ускорение %.1f м/с² (нужно < 5,5; предел 4 + запас на стык рельса и кривой; TEST83: 18 и больше)", curves, sharp, worst); }

  // ---- №1098: пешеход идёт по тропе, поперёк стоит машина (игрока): обходит сбоку и идёт дальше, а не упирается и разворачивается
  { int P = -1; for (int i = 0; i < (int)T.hd->nPath && P < 0; i++) { const float *a = pathP(&T, i, 0), *b = pathP(&T, i, 1); float g; if (T.path[i].n >= 2 && hypotf(b[0] - a[0], b[2] - a[2]) > 16.f && colGround(&world, (a[0] + b[0]) / 2, (a[2] + b[2]) / 2, a[1] + 3.f, a[1] - 6.f, &g, 0)) P = i; }
    float tPass = -1.f, minAlong = 99.f; int turned = 0;
    if (P >= 0) { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED;
      const float *a = pathP(&T, P, 0), *b = pathP(&T, P, 1); float L = hypotf(b[0] - a[0], b[2] - a[2]), ux = (b[0] - a[0]) / L, uz = (b[2] - a[2]) / L, mx = a[0] + ux * 8.f, mz = a[2] + uz * 8.f;
      static Car blockC; float hh = atan2f(ux, uz) + 1.5707963f; carInit(&blockC, NULL, &world, mx, ground(mx, mz, a[1] + 1.f), mz, hh);   // поперёк тропы: длинной стороной
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = a[0] + ux * 3.f; e->z = a[2] + uz * 3.f; e->y = a[1]; e->h = atan2f(ux, uz); e->mode = TRP_WALK; e->speed = 1.3f; e->partner = -1; e->path = P; e->seg = 0; e->dir = 1; skinInit(&e->sp, &T.peds[0]);
      float pp[3] = {a[0] - 200.f, a[1], a[2]};   // Гомер далеко
      for (int f = 0; f < 60 * 25 && e->mode != TRP_OFF; f++) { pedStep(&T, &world, 0, &blockC, 0, pp, DT); float al = (e->x - mx) * ux + (e->z - mz) * uz; if (al < minAlong) minAlong = al; if (e->dir < 0) turned = 1; if (tPass < 0 && al > 3.f) tPass = f * DT; } }
    check("1098 обход", P >= 0 && tPass > 0 && tPass < 14.f && !turned, "пешеход упёрся в машину поперёк тропы: прошёл за машину через %.1f с (нужно < 14), разворот по тропе: %d (нужно 0) (оригинал: обходит; прежнее: стоит 1,5 с и идёт назад)", tPass, turned); }

  // ---- слой персонаж ↔ машина: прохожий идёт по тропе, поперёк стоит МАШИНА ТРАФИКА / стоящая: не проходит сквозь, стоит 3 с, обходит
  { int P = -1; for (int i = 0; i < (int)T.hd->nPath && P < 0; i++) { const float *a = pathP(&T, i, 0), *b = pathP(&T, i, 1); float g; if (T.path[i].n >= 2 && hypotf(b[0] - a[0], b[2] - a[2]) > 16.f && colGround(&world, (a[0] + b[0]) / 2, (a[2] + b[2]) / 2, a[1] + 3.f, a[1] - 6.f, &g, 0)) P = i; }
    int through[2] = {0, 0}; float tPass[2] = {-1.f, -1.f}, tStand[2] = {0.f, 0.f};
    for (int kind = 0; kind < 2 && P >= 0; kind++) {   // 0 — машина трафика с нулевой скоростью, 1 — стоящая машина места
      trafficReset(&T, &world); T.npark = (int)T.hd->nPark; T.maxCars = kind ? 0 : (T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR); T.maxPeds = TR_MAXPED; T.nextra = 0;
      const float *a = pathP(&T, P, 0), *b = pathP(&T, P, 1); float L = hypotf(b[0] - a[0], b[2] - a[2]), ux = (b[0] - a[0]) / L, uz = (b[2] - a[2]) / L, mx = a[0] + ux * 8.f, mz = a[2] + uz * 8.f, hh = atan2f(ux, uz) + 1.5707963f;
      for (int i = 0; i < TR_MAXCAR; i++) T.car[i].mode = TRM_OFF; for (int i = 0; i < T.npark; i++) { T.park[i].gone = 1; }
      const Car *obst;
      if (kind == 0) { TrCar *c = &T.car[0]; memset(c, 0, sizeof *c); c->model = T.slot[0].model; carInit(&c->car, &T.params[c->model], &world, mx, ground(mx, mz, a[1] + 1.f), mz, hh); c->mode = TRM_DRIVE; c->v = 0.f; c->road = -1; obst = &c->car; }
      else { TrParked *Pk = &T.park[0]; carInit(&Pk->car, &T.params[T.parkd[0].model], &world, mx, ground(mx, mz, a[1] + 1.f), mz, hh); Pk->gone = 0; Pk->off = 0; Pk->state = 0; obst = &Pk->car; }
      CarBox bx; carBox(obst, &bx);
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e); e->model = 0; e->x = a[0] + ux * 3.f; e->z = a[2] + uz * 3.f; e->y = a[1]; e->h = atan2f(ux, uz); e->mode = TRP_WALK; e->speed = 1.3f; e->partner = -1; e->path = P; e->seg = 0; e->dir = 1; skinInit(&e->sp, &T.peds[0]);
      float pp[3] = {a[0] - 200.f, a[1], a[2]}; Car far_; carInit(&far_, NULL, &world, a[0] - 300.f, a[1], a[2], 0.f); int standing = 0;
      for (int f = 0; f < 60 * 25 && e->mode != TRP_OFF; f++) {
        if (kind == 0) { T.car[0].v = 0.f; T.car[0].mode = TRM_DRIVE; } else { T.park[0].gone = 0; }
        float x0 = e->x, z0 = e->z; pedStep(&T, &world, 0, &far_, 0, pp, DT);
        float dx = e->x - bx.x, dz = e->z - bx.z; if (fabsf(dx * bx.fx + dz * bx.fz) < bx.hl + TR_PED_R - .05f && fabsf(-dx * bx.fz + dz * bx.fx) < bx.hw + TR_PED_R - .05f) through[kind] = 1;
        if (hypotf(e->x - x0, e->z - z0) < 1e-5f) standing++;
        float al = (e->x - mx) * ux + (e->z - mz) * uz; if (tPass[kind] < 0 && al > 3.f) tPass[kind] = f * DT; }
      tStand[kind] = standing * DT; }
    check("слой пеший-машина", P >= 0 && !through[0] && !through[1] && tPass[0] > 0 && tPass[1] > 0 && tPass[0] < 14.f && tPass[1] < 14.f && tStand[0] > 2.7f && tStand[1] > 2.7f,
          "машина трафика на тропе: сквозь прошёл %d, за машину через %.1f с, стоял %.1f с; стоящая машина места: сквозь %d, через %.1f с, стоял %.1f с (оригинал: касание машины — стоп 3,0 с, дальше идёт; прежнее: проходил сквозь)", through[0], tPass[0], tStand[0], through[1], tPass[1], tStand[1]); }

  // ---- 4 / 52 (7.2, P03-16): пешеход дальше 50 м от центра (игрок + 30 м) снимается после 3 с, ближе — остаётся; паника от пинка и гудка — в 12 м, в 14 м нет
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; T.loadT = 5.f; float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1};   // центр (220, 202)
    TrPed *a = &T.ped[0], *b = &T.ped[1]; for (int k = 0; k < 2; k++) { TrPed *e = &T.ped[k]; memset(e, 0, sizeof *e); e->model = 0; e->x = px; e->z = 202.f + (k ? 46.f : 54.f); e->y = player.y; e->mode = TRP_WALK; e->speed = 1.f; e->partner = -1; e->path = k; skinInit(&e->sp, &T.peds[0]); }
    T.maxPeds = 2; trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); int ra = a->mode == TRP_OFF, rb = b->mode != TRP_OFF; T.maxPeds = TR_MAXPED;
    check("4 снятие в 50 м", ra && rb, "пешеход в 54 м от центра снят %d (нужно 1), в 46 м остался %d (нужно 1) (оригинал: дальше 50 м от точки игрок + 30 м; прежнее: 60–90 м от игрока)", ra, rb);
    for (int k = 0; k < 2; k++) { TrPed *e = &T.ped[k]; memset(e, 0, sizeof *e); e->model = 0; e->x = px + (k ? 14.f : 11.f); e->z = pz; e->y = player.y; e->mode = TRP_WALK; e->speed = 1.f; e->partner = -1; e->path = k; skinInit(&e->sp, &T.peds[0]); }
    trafficScare(&T, px, pz); check("52 радиус паники", T.ped[0].mode == TRP_PANIC && T.ped[1].mode != TRP_PANIC, "пешеход в 11 м: режим %d (паника = %d), в 14 м: %d (не паника) (оригинал 12 м)", T.ped[0].mode, TRP_PANIC, T.ped[1].mode); }

  // ---- 58 (Д8): одна попытка рождения пешехода на каждый второй кадр, первые 3 с — на каждый
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1}; T.loadT = 0.f; trfPedTries = 0; unsigned a0 = 0, a1;
    for (int f = 0; f < 120; f++) trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);   // 2 с: каждый кадр
    a0 = trfPedTries; T.loadT = 5.f; trfPedTries = 0; for (int f = 0; f < 120; f++) trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); a1 = trfPedTries;
    check("58 частота", a0 == 120 && a1 >= 59 && a1 <= 61, "за 120 кадров в первые 3 с попыток %u (нужно 120), потом %u (нужно 60) (оригинал: каждый второй кадр; прежнее: раз в 0,25 с)", a0, a1); }

  // ---- 15 / 49 (P03-13, P02-41): уличная гонка — пешеходов нет и новые не рождаются; без гонки рождаются
  { int alive[2]; for (int k = 0; k < 2; k++) { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
      float pp[3] = {px, player.y, pz}, eye[3] = {px, player.y + 3.f, pz - 7.f}, fw[3] = {0, 0, 1}; T.seed = 777u;
      for (int f = 0; f < 60; f++) trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);   // сначала пешеходы есть
      if (k) trafficStreetRace(&T, 1);
      for (int f = 0; f < 60 * 10; f++) trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      alive[k] = 0; for (int i = 0; i < TR_MAXPED; i++) alive[k] += T.ped[i].mode != TRP_OFF; }
    check("15 гонка", alive[0] >= 1 && alive[1] == 0, "без гонки пешеходов %d (≥ 1), в гонке %d (0) (оригинал: в гонке все сняты, новых нет)", alive[0], alive[1]); }

  // ---- 2 (3.1): пинок задевает одного — ближайшего к центру удара
  { trafficReset(&T, &world); T.npark = 0; T.maxCars = 0; T.maxPeds = TR_MAXPED; float px = 220.f, pz = 172.f; carInit(&player, NULL, &world, px, ground(px, pz, 50.f), pz, 0.f);
    for (int k = 0; k < 2; k++) { TrPed *e = &T.ped[k]; memset(e, 0, sizeof *e); e->model = 0; e->x = px + (k ? 1.4f : 1.0f); e->z = pz; e->y = player.y; e->mode = TRP_WALK; e->partner = -1; e->speed = 1.f; skinInit(&e->sp, &T.peds[0]); }
    int n = trafficKick(&T, px, player.y, pz, 1.8f, 1.f, 0.f); int fly0 = T.ped[0].mode == TRP_FLY, fly1 = T.ped[1].mode == TRP_FLY;
    check("2 один пинок", n == 1 && fly0 && !fly1, "задеты %d (нужен 1), ближний летит %d, дальний %d (оригинал: один, ближайший; прежнее: оба)", n, fly0, fly1); }

  // ---- 66 (8.9): трафик едет по рельсам — без удара остаётся на полосе и не уходит в физику
  { TrCar *c = scene(R, 0, 5.f, 14.f, 35.f, 12.f); float worst = 0; int phys = 0; T.car[0].chgCool = 1e9f;
    for (int i = 0; i < 60 * 8 && c->s < 150.f; i++) { frame(hd); if (c->mode != TRM_DRIVE) phys = 1; float q[3]; lanePos(&T, c->road, c->lane, c->s, q); worst = fmaxf(worst, hypotf(c->car.x - q[0], c->car.z - q[2])); }
    check("66 рельсы", !phys && worst < 1.2f, "8 с без ударов: режим езды сохранён (%d), наибольшее отклонение от полосы %.2f м (оригинал: по рельсам; физика только после удара)", !phys, worst); }

  // ---- 1 (P16-06): навигация: путь строится один раз при старте цели; зелёные стрелки на перекрёстках впереди, красные позади
  { int RJ = straightRoad(150.f, -1, 1); float L = laneLen(&T, RJ, 0), q[3], g[3]; lanePos(&T, RJ, 0, 40.f, q); int rj2 = -1; const TrJunc *J = &T.junc[T.road[RJ].endJ];
    for (int k = 0; k < J->nOut && rj2 < 0; k++) { int o = T.out[J->firstOut + k]; if (roadOk(&T, o) && o != (int)T.road[RJ].rev) rj2 = o; }
    TrNavArrow ar[32]; int built = 0, na = 0, red0 = 0; float h = roadDir(RJ, 40.f);
    if (rj2 >= 0) { lanePos(&T, rj2, 0, T.road[rj2].length * .5f, g); built = trafficNavBuild(&T, q[0], q[2], h, g[0], g[2]); na = trafficNavArrows(&T, q[0], q[2], ar, 32); for (int i = 0; i < na; i++) red0 += ar[i].red; }
    float q2[3]; lanePos(&T, rj2 >= 0 ? rj2 : RJ, 0, T.road[rj2 >= 0 ? rj2 : RJ].length * .3f, q2); int na2 = rj2 >= 0 ? trafficNavArrows(&T, q2[0], q2[2], ar, 32) : 0, red2 = 0; for (int i = 0; i < na2; i++) red2 += ar[i].red; (void)L;
    check("1 навигация", built == 1 && na >= 1 && red0 == 0 && na2 >= 1 && red2 >= 1, "путь построен %d, стрелок у старта %d (красных %d, нужно 0), после проезда перекрёстка: стрелок %d, красных %d (≥ 1) (оригинал: зелёные впереди, красные на пройденных; прежнее: одна стрелка у ближайшего перекрёстка)", built, na, red0, na2, red2); }

  // ---- цена кадра (по G02BENCH=1): среднее время trafficUpdate на Маке при полном потоке (5 машин, 7 пешеходов, стоящие) — для сравнения «после» и «до» (все выключатели _OLD)
  if (getenv("G02BENCH")) {
    T.npark = (int)T.hd->nPark; trafficReset(&T, &world); T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR; T.maxPeds = TR_MAXPED; T.seed = 2024u;
    float h = roadDir(R, 40.f), q[3]; lanePos(&T, R, 0, 40.f, q); carInit(&player, NULL, &world, q[0], ground(q[0], q[2], q[1]), q[2], h);
    float pp[3] = {q[0], player.y, q[2]}, eye[3] = {q[0] - sinf(h) * 8.f, player.y + 3.f, q[2] - cosf(h) * 8.f}, fw[3] = {sinf(h), 0, cosf(h)};
    for (int f = 0; f < 600; f++) trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
    struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a); int N = 6000; for (int f = 0; f < N; f++) trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); clock_gettime(CLOCK_MONOTONIC, &b);
    printf("BENCH trafficUpdate %.1f мкс на кадр (Мак), машин %d, пешеходов %d\n", ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / 1e3 / N, ({ int n = 0; for (int i = 0; i < TR_MAXCAR; i++) n += T.car[i].mode != TRM_OFF; n; }), ({ int n = 0; for (int i = 0; i < TR_MAXPED; i++) n += T.ped[i].mode != TRP_OFF; n; })); }

  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
