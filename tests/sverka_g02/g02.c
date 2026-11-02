// Сверка с исходником, окно g02-traffic (02.11.2026): проверки на CPU Мака, без эмулятора. Подключает viewer/traffic.c напрямую (видны static-функции).
// Настоящие данные уровня 1: build/level.shr (COL1) + build/traffic.shr (TRF1). Каждая проверка печатает «CHECK <имя> OK|FAIL»; в наборе выключателей
// -DTRF_SV_*_OLD=1 (красный контроль) проверка своего правила обязана упасть. Запуск: tests/sverka_g02/g02.sh [level.shr] [traffic.shr]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
  T.fresh = 0; T.scanT = 1e9f;   // сами не рождаем
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
static int popHonks(void) { int n = 0; TrEvent e; while (trafficPopEvent(&T, &e)) n += e.type == TR_EV_CAR_HONK; return n; }

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

  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
