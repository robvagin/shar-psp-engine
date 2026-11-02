// Правка 240 (Роб 03.10 14:51: «не сбиваются люди вообще никакие»): люди под машиной отлетают. Игра без экрана
// (tools/bots/world.c) — тот же viewer/game_msn.c и viewer/traffic.c, что EBOOT, данные уровня 1; координат руками нет, кроме
// места проверки прохожего (то же, что у сценария эмулятора «goto 188 279.9 1.5708» + «flag pedahead»).
//   ped    — прохожий трафика в 15 м на оси машины, газ 2,5 с: сбит (одно событие), в полёте его центр ни разу не внутри кузова
//            (глубже 0,1 м) и за 3 с ни разу не под машиной (лежащего машина толкает и подбрасывает снова). Прежнее: отлёт 0,8 скорости
//            машины — машина догоняет сбитого, он «уходит под машину»
//   corner — то же, прохожий чуть за краем кузова (полуширина + 0,15 м, удар углом): в полёте дальше 1 м вбок от края
//   npc    — каждый персонаж миссий в городе (фоновые, продавцы, бонусные, персонаж миссии): машина газом прямо на него. Подход
//            ищется: прямая 16 м (13 м), контрольный проезд без персонажа ничего не задел и 0,4 с после места не врезался, к встрече
//            быстрее 8 м/с. Проверка: событие «сбит» (Hit & Run), отлетел дальше 2 м, центр не внутри кузова (лёжа и вставая — не
//            в счёт); новый код (GAME_NPC_KNOCK): через 4 с стоит на земле; стоявший — машина уехала — через 44 с снова на своём месте
// Красный (tests/npc/knock.sh): -DGAME_NPC_KNOCK=0 -DTR_KNOCK_OLD=1 — прежнее (или копия viewer/ до правки)
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static int fails, checks;
static void check(const char *name, int ok, const char *d) { printf("CHECK %-6s %s  %s\n", name, ok ? "OK  " : "FAIL", d); checks++; if (!ok) fails++; }
static BotWorld W; static BotSnap S;
static Traffic *TR(void) { return (Traffic *)bwTraffic(); }
static void quiet(int peds) {   // без чужих машин и прохожих: события в счёте — только от проверяемого человека
  Traffic *t = TR(); if (!t) return;
  t->maxCars = 0; for (int i = 0; i < TR_MAXCAR; i++) t->car[i].mode = TRM_OFF;
  t->maxPeds = peds; for (int i = 0; i < TR_MAXPED; i++) t->ped[i].mode = TRP_OFF;
}
static float boxDist(const Car *k, float x, float z) {   // от точки до кузова в xz: < 0 — точка внутри (глубина)
  CarBox b; carBox(k, &b);
  float dx = x - b.x, dz = z - b.z, a = dx * b.fx + dz * b.fz, s = -dx * b.fz + dz * b.fx, ea = fabsf(a) - b.hl, es = fabsf(s) - b.hw;
  if (ea < 0.f && es < 0.f) return fmaxf(ea, es);
  return hypotf(fmaxf(ea, 0.f), fmaxf(es, 0.f));
}
static float ground(float x, float z, float y) { float g; return colGround(bwCol(), x, z, y + 2.f, y - 60.f, &g, 0) ? g : -1e9f; }   // опора под y + 2

// ---------------------------------------------------------------- прохожий трафика
static void pedRun(const char *tag, float side) {
  bwLoad(&W, &S); quiet(1);
  float x0 = 188.f, z0 = 279.9f, h = 1.5708f;
  bwPlace(&W, x0, ground(x0, z0, 10.f), z0, h);
  for (int f = 0; f < 30; f++) bwStep(&W, 0, 0, 0);
  float fx = sinf(h), fz = cosf(h), px = W.car.x + fx * 15.f + fz * side, pz = W.car.z + fz * 15.f - fx * side;
  if (trafficPedPlace(TR(), bwCol(), 0, px, pz, h + 3.14159f) != 0) { check(tag, 0, "прохожий не поставлен"); return; }
  const TrPed *e = &TR()->ped[0]; unsigned ev0 = W.st.trEvents;
  int fly = 0, inside = 0, under = 0, hitF = -1, landF = -1; float deep = 0.f, hitV = 0.f, lat = 0.f, fwd = 0.f, latMax = 0.f, minD = 1e9f;
  for (int f = 0; f < 180; f++) {
    bwStep(&W, f < 150 ? GP_CROSS : 0, 0, 0);
    if (hitF >= 0 && e->mode != TRP_OFF && boxDist(&W.car, e->x, e->z) < -.1f) under++;   // после удара — под машиной (лежит, встаёт)
    if (e->mode == TRP_FLY) {
      if (hitF < 0) { hitF = f; hitV = sqrtf(W.car.vel[0] * W.car.vel[0] + W.car.vel[2] * W.car.vel[2]); }
      fly++; float d = boxDist(&W.car, e->x, e->z); if (d < minD) minD = d;
      if (d < -.1f) { inside++; if (-d > deep) deep = -d; }   // глубже 0,1 м
      float cf = sinf(W.car.h), cz = cosf(W.car.h), dx = e->x - W.car.x, dz = e->z - W.car.z, l = fabsf(-dx * cz + dz * cf);
      if (l > latMax) latMax = l;
    } else if (hitF >= 0 && landF < 0) {   // приземлился: где относительно машины
      landF = f; float cf = sinf(W.car.h), cz = cosf(W.car.h), dx = e->x - W.car.x, dz = e->z - W.car.z;
      fwd = dx * cf + dz * cz; lat = -dx * cz + dz * cf;
    }
  }
  unsigned ev = W.st.trEvents - ev0; char d[700];
  snprintf(d, sizeof d, "%s: удар на %.1f м/с, событий %u, полёт %d кадров (%.2f с), центр внутри кузова %d кадров (глубже всего %.2f м), "
           "ближе всего к кузову %.2f м; приземлился в %.1f м %s центра машины и %.1f м вбок; за 3 с под машиной %d кадров", tag, hitV, ev, fly,
           fly / 60.f, inside, deep, minD, fabsf(fwd), fwd >= 0.f ? "впереди" : "позади", fabsf(lat), under);
  check(tag, hitF >= 0 && ev == 1 && inside == 0 && under == 0, d);
  if (side != 0.f) { snprintf(d, sizeof d, "%s: вбок от оси машины в полёте до %.2f м (кузов — полуширина ~1 м)", tag, latMax);
    check(tag, latMax > fabsf(side) + 1.f, d); }
}

// ---------------------------------------------------------------- персонажи миссий
typedef struct { float h, v, L; int ok; } Approach;
// прямая 16 м (нет — 13 м) до персонажа: контрольный проезд без него ничего не задел и 0,4 с после места не врезался, к встрече
// быстрее 8 м/с, ось машины через персонажа (вбок < 0,5 м); из годных — самая быстрая
static Approach approach(int i, MsnVec3 p) {
  Approach best = {0, 0, 0, 0};
  for (int a = 0; a < 72; a++) {
    if (a == 36 && best.ok) break;
    float L = a < 36 ? 16.f : 13.f, h = (a % 36) * 6.2831853f / 36.f, sx = p.x - sinf(h) * L, sz = p.z - cosf(h) * L, gy = ground(sx, sz, p.y + 1.f);
    if (gy < -1e8f || fabsf(gy - p.y) > 1.2f) continue;
    bwLoad(&W, &S); quiet(0); W.msn.npc[i].pos.x += 1000.f;
    bwPlace(&W, sx, gy, sz, h); float v = -1.f, lat = 99.f; unsigned ev0 = W.st.trEvents;
    for (int f = 0; f < 150; f++) {
      bwStep(&W, GP_CROSS, 0, 0);
      float cf = sinf(W.car.h), cz = cosf(W.car.h), dx = p.x - W.car.x, dz = p.z - W.car.z, ahead = dx * cf + dz * cz;
      if (ahead < W.car.p.halfL) { v = sqrtf(W.car.vel[0] * W.car.vel[0] + W.car.vel[2] * W.car.vel[2]); lat = fabsf(-dx * cz + dz * cf);
        if (fabsf(W.car.y - p.y) > 1.f) v = -1.f;
        for (int k = 0; k < 24 && v > 0.f; k++) {
          bwStep(&W, GP_CROSS, 0, 0); if (sqrtf(W.car.vel[0] * W.car.vel[0] + W.car.vel[2] * W.car.vel[2]) < .8f * v) v = -1.f; }
        break; }
    }
    if (W.st.trEvents != ev0) continue;
    if (v > 8.f && lat < .5f && v > best.v) { best.h = h; best.v = v; best.L = L; best.ok = 1; }
  }
  return best;
}
static void npcRun(int i) {
  bwLoad(&W, &S);
  const GameNpc *n0 = &W.msn.npc[i]; char name[24]; snprintf(name, sizeof name, "%s%s", n0->name, n0->bonus ? "#b" : "");
  MsnVec3 p = n0->pos; int kind = n0->kind, wasWalking = n0->walking; char d[700]; (void)wasWalking;
  int lead = 0;
  if (wasWalking) {   // идёт: цель — где он будет через ~2,1 с (машина с места проезжает 16 м)
    bwLoad(&W, &S); quiet(0); lead = 128;   // машина стоит, где стояла в снимке (у дома Симпсонов)
    for (int f = 0; f < lead; f++) bwStep(&W, 0, 0, 0);
    p = W.msn.npc[i].pos;
  }
  Approach A = approach(i, p);
  if (!A.ok) { snprintf(d, sizeof d, "%-10s kind=%d (%.1f %.1f %.1f): прямого подхода 16/13 м нет — не проверен", name, kind, p.x, p.y, p.z); printf("SKIP   %s\n", d); return; }
  bwLoad(&W, &S); quiet(0);
  float sx = p.x - sinf(A.h) * A.L, sz = p.z - cosf(A.h) * A.L; bwPlace(&W, sx, ground(sx, sz, p.y + 1.f), sz, A.h);
  const GameNpc *n = &W.msn.npc[i]; unsigned ev0 = W.st.trEvents; float disp = 0.f, deep = 0.f; int inside = 0;
  int past = -1;   // газ, пока машина не проехала место персонажа на 3 м, потом тормоз
  for (int f = 0; f < 240; f++) {
    float cf = sinf(W.car.h), cz = cosf(W.car.h), ahead = (p.x - W.car.x) * cf + (p.z - W.car.z) * cz;
    if (past < 0 && ahead < -W.car.p.halfL - 3.f) past = f;
    float sp = sqrtf(W.car.vel[0] * W.car.vel[0] + W.car.vel[2] * W.car.vel[2]);
    bwStep(&W, past < 0 ? GP_CROSS : sp > 1.f ? GP_CIRCLE : 0, 0, 0);   // тормоз до остановки (стоя ○ — задний ход: не жать)
    float dd = hypotf(n->pos.x - p.x, n->pos.z - p.z); if (dd > disp) disp = dd;
    float b = boxDist(&W.car, n->pos.x, n->pos.z);
#ifdef GAME_NPC_KNOCK
    if (n->knock == TRP_DOWN || n->knock == TRP_GETUP) b = 1.f;   // лежит / встаёт под стоящей машиной — не полёт (машина вытолкнет встающего)
#endif
    if (b < -.1f) { inside++; if (-b > deep) deep = -b; }   // глубже 0,1 м
  }
  unsigned ev = W.st.trEvents - ev0;
  snprintf(d, sizeof d, "%-10s kind=%d (%.1f %.1f %.1f) удар %.1f м/с курс %.0f° с %.0f м: событий %u, отлетел до %.1f м, центр внутри кузова %d кадров (до %.2f м)",
           name, kind, p.x, p.y, p.z, A.v, A.h * 57.2958f, A.L, ev, disp, inside, deep);
  check("npc", ev >= 1 && disp > 2.f && inside == 0, d);
#ifdef GAME_NPC_KNOCK
  int waitF = (int)((TR_PED_DOWN + 1.f + 3.f) * 60.f);   // №: лежание (ps2-twin 7.7) + клип вставания + запас; прежнее (3 с) — 7 с
  for (int f = 0; f < waitF; f++) bwStep(&W, 0, 0, 0);
  float g = ground(n->pos.x, n->pos.z, n->pos.y - 1.5f);   // опора под ступнями (от +0,5 м вниз: навес над головой — не пол)
  snprintf(d, sizeof d, "%-10s через %.0f с: состояние %d (0 — стоит), над землёй %.2f м, %s", name, waitF / 60.f, n->knock, n->pos.y - g, n->homing ? "идёт на место" : wasWalking ? "идёт по маршруту" : "стоит");
  check("getup", n->knock == 0 && fabsf(n->pos.y - g) < .3f, d);
  if (!wasWalking) {   // игрок уехал: машина — на 40 м назад по прямой подхода (стоящая на месте персонажа держала бы его)
    bwPlace(&W, p.x - sinf(A.h) * 40.f, p.y + 1.f, p.z - cosf(A.h) * 40.f, A.h);
    for (int f = 0; f < 2400 && (n->homing || n->knock); f++) bwStep(&W, 0, 0, 0);
    float back = hypotf(n->pos.x - p.x, n->pos.z - p.z);
    snprintf(d, sizeof d, "%-10s стоял: через 44 с от своего места %.2f м", name, back);
    check("home", back < 1.f, d);
  }
#endif
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "knock_host LEVEL.shr HOMER.shr level01.msn\n"); return 2; }
  setvbuf(stdout, NULL, _IONBF, 0);
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  bwNew(&W); bwSaveAlloc(&W, &S);
  if (!bwTraffic()) { printf("FAIL: нет traffic.shr рядом с уровнем\n"); return 1; }
  for (int f = 0; f < 120 * 60; f++) bwStep(&W, 0, 0, 0);   // 2 мин: фоновые дошли до конца маршрутов (все стоят)
  bwSave(&W, &S);
  pedRun("ped", 0.f);
  bwLoad(&W, &S); pedRun("corner", W.car.p.halfW + .15f);   // удар углом: прохожий чуть за краем кузова
  int town = 0;
  for (int i = 0; i < W.msn.nnpc; i++) {
    bwLoad(&W, &S); const GameNpc *n = &W.msn.npc[i];
    if (n->interior[0] || n->pos.y < -10.f) { printf("ROOM   %-10s kind=%d — в комнате (машины там нет)\n", n->name, n->kind); continue; }
    town++; npcRun(i);
  }
  printf("RESULT %s: %d проверок, провалов %d (персонажей в городе %d)\n", fails ? "FAIL" : "PASS", checks, fails, town);
  return fails ? 1 : 0;
}
