// Гейт трафика на Маке (viewer/traffic.c на настоящих данных: build/level.shr COL1 + build/traffic.shr TRF1 + build/homer.shr).
//   tests/traffic/host.sh [level.shr] [traffic.shr] [homer.shr]
// Один прогон = все проверки на настоящих данных (должны пройти) + те же проверки на подброшенных плохих данных
// (каждая подброска обязана уронить свою проверку). Код выхода 0 = настоящие зелёные И все подброски красные.
// Проверки: полосы на земле и не в стенах; езда 4 x 30 с (машины есть, едут, проезжают перекрёстки, не в стенах, не
// висят); стоящие машины (число, модель по правилу порта, на месте, не в мире, не друг в друге); твёрдость (Гомер о
// стоящую машину и о пешехода — стоп, машина игрока толкает стоящую, сбивает пешехода, удар по машине трафика).
// Места для проверок твёрдости — не координаты из головы: у точки Kwik E Mart из level.mfk (AddTeleportDest 209, -285)
// ищем первое место, где контрольный прогон без препятствия проходит свободно.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "traffic.h"
#include "homer.h"

static ColWorld world; static SkinModel *homerM; static Traffic T; static Homer H; static Car player;
static int fails; static char failed[1024];
static const float DT = 1.f / 30.f;
static const float KWIK[2] = {209.f, 285.f};   // level.mfk AddTeleportDest("Kwik E Mart", 209, 3.6, -285), z зеркалим

static void check(const char *what, float got, float lo, float hi, const char *expect) {
  int ok = got >= lo && got <= hi;
  if (!ok) { fails++; if (strlen(failed) + strlen(what) + 4 < sizeof failed) { strcat(failed, what); strcat(failed, "; "); } }
  printf("  %-4s %-52s %9.3f   (%s)\n", ok ? "ok" : "FAIL", what, got, expect);
}
static unsigned char *readAt(FILE *f, unsigned off, unsigned sz) {
  unsigned char *m = aligned_alloc(64, (sz + 63) & ~63u);
  fseek(f, off, SEEK_SET); if (fread(m, 1, sz, f) != sz) { free(m); return 0; }
  return m;
}
static float ground(float x, float z, float y) { float g; return colGround(&world, x, z, y + 2.f, y - 50.f, &g, 0) ? g : y; }
static float sat(const Car *a, const Car *b) {   // перекрытие кузовов в xz (0 — нет)
  CarBox A, B; carBox(a, &A); carBox(b, &B);
  float ax[4][2] = {{A.fx, A.fz}, {-A.fz, A.fx}, {B.fx, B.fz}, {-B.fz, B.fx}}, dx = A.x - B.x, dz = A.z - B.z, depth = 1e9f;
  if (fabsf(a->y - b->y) > 2.5f) return 0.f;
  for (int k = 0; k < 4; k++) {
    float ux = ax[k][0], uz = ax[k][1];
    float ea = A.hw * fabsf(-A.fz * ux + A.fx * uz) + A.hl * fabsf(A.fx * ux + A.fz * uz);
    float eb = B.hw * fabsf(-B.fz * ux + B.fx * uz) + B.hl * fabsf(B.fx * ux + B.fz * uz);
    float ov = ea + eb - fabsf(dx * ux + dz * uz); if (ov <= 0.f) return 0.f; if (ov < depth) depth = ov;
  }
  return depth;
}
static float wrapPi_(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
static const TrPt *lp(int r, int l) { return T.pt + T.road[r].firstPt + (unsigned)l * T.road[r].nPt; }

// ---------------------------------------------------------------- полосы
static void lanes(void) {
  int n = 0, off = 0, samp = 0, wall = 0;
  for (unsigned r = 0; r < T.hd->nRoad; r++) {
    if (T.road[r].flags & 3) continue;
    for (int l = 0; l < T.road[r].lanes; l++) {
      const TrPt *P = lp((int)r, l);
      for (int k = 0; k < T.road[r].nPt; k++) {
        float g; n++;
        if (!colGround(&world, P[k].x, P[k].z, P[k].y + 1.f, P[k].y - 1.f, &g, 0) || fabsf(g - P[k].y) > .75f) off++;
      }
      for (int k = 0; k + 1 < T.road[r].nPt; k++) {
        float dx = P[k + 1].x - P[k].x, dz = P[k + 1].z - P[k].z, L = sqrtf(dx * dx + dz * dz); if (L < .1f) continue;
        for (float s = 0.f; s < L; s += 2.f) {
          CarBox b; memset(&b, 0, sizeof b); float u = s / L;
          b.x = P[k].x + dx * u; b.z = P[k].z + dz * u; b.fx = dx / L; b.fz = dz / L; b.hw = .9f; b.hl = 1.2f;
          b.y = P[k].y + (P[k + 1].y - P[k].y) * u; b.lo = .5f; b.hi = 1.5f;
          colProbeCar(&world, &b); samp++; if (b.maxDepth > .25f) wall++;
        }
      }
    }
  }
  check("lanes: points off the ground (share)", n ? (float)off / n : 1.f, 0.f, .03f, "<= 3%: lane on the road surface");
  check("lanes: 2 m samples inside walls (share)", samp ? (float)wall / samp : 1.f, 0.f, .03f, "<= 3%: lane clear of walls");
}

// ---------------------------------------------------------------- езда
static void drive(void) {
  // телепорты level.mfk (дом, Kwik, церковь, школа) и машина игрока поперёк полосы у Kwik (зад торчит в полосу 12)
  static const float at[5][2] = {{220.f, 172.f}, {209.f, 285.f}, {193.8f, 570.f}, {-11.f, 586.f}, {209.f, 279.f}};
  int frames = 0, carFrames = 0, offg = 0, inwall = 0, junc = 0, rams = 0, overlap = 0;
  float dist = 0.f, active = 0.f, stopMax = 0.f, stop[TR_MAXCAR] = {0};
  int prevElem[TR_MAXCAR]; for (int i = 0; i < TR_MAXCAR; i++) prevElem[i] = -1;
  for (int a = 0; a < 5; a++) for (int rep = 0; rep < (a == 4 ? 6 : 1); rep++) {
    carInit(&player, NULL, &world, at[a][0], ground(at[a][0], at[a][1], 50.f), at[a][1], 0.f);
    trafficReset(&T, &world); T.seed += (unsigned)rep * 7919u;
    for (int f = 0; f < 900; f++) {
      CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT);
      float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3.f, player.z - 7.f}, fw[3] = {0, 0, 1};
      int was[TR_MAXCAR]; for (int i = 0; i < T.maxCars; i++) was[i] = T.car[i].mode;
      trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      TrEvent e; while (trafficPopEvent(&T, &e)) {}
      frames++; active += T.activeCars;
      for (int i = 0; i < T.maxCars; i++) if (was[i] == TRM_DRIVE && T.car[i].mode == TRM_PHYS) rams++;   // игрок стоит: удар — вина трафика
      for (int i = 0; i < T.maxCars; i++) for (int j = i + 1; j < T.maxCars; j++)
        if (T.car[i].mode == TRM_DRIVE && T.car[j].mode == TRM_DRIVE && fmaxf(T.car[i].v, T.car[j].v) > 2.5f && sat(&T.car[i].car, &T.car[j].car) > .1f) overlap++;   // №1159: «проехала сквозь» — хоть одна едет быстрее 2,5 м/с; два почти стоящих носом в хвост (запас остановки по центрам 5 м у длинных машин) — не считаем
      for (int i = 0; i < T.maxCars; i++) {
        TrCar *c = &T.car[i];
        if (c->mode != TRM_DRIVE) { prevElem[i] = -1; stop[i] = 0.f; continue; }
        carFrames++; dist += c->v * DT;
        if (prevElem[i] == 1 && c->elem == 0) junc++;
        prevElem[i] = c->elem;
        float g; if (!colGround(&world, c->car.x, c->car.z, c->car.y + .5f, c->car.y - .5f, &g, 0)) offg++;
        CarBox b; carBox(&c->car, &b); b.hw *= .8f; b.hl *= .8f; colProbeCar(&world, &b); if (b.maxDepth > .3f) inwall++;
        stop[i] = c->v < .1f ? stop[i] + DT : 0.f; if (a < 4 && stop[i] > stopMax) stopMax = stop[i];   // у Kwik (5-е место) стоят за машиной игрока — это верно
      }
    }
  }
  check("drive: cars on average (of pool)", frames ? active / frames : 0.f, 3.f, 5.f, ">= 3 of 5 around the player");
  check("drive: metres driven by all cars, 120 s", dist, 1500.f, 1e9f, ">= 1500 (5 cars x 50 km/h would be ~8000)");
  check("drive: junctions crossed", (float)junc, 20.f, 1e9f, ">= 20");
  check("drive: frames without ground under a car (share)", carFrames ? (float)offg / carFrames : 1.f, 0.f, .01f, "<= 1%");
  check("drive: frames with a car inside a wall (share)", carFrames ? (float)inwall / carFrames : 1.f, 0.f, .02f, "<= 2%");
  check("drive: longest stop of one car, s", stopMax, 0.f, 30.f, "< 30 (junction waits, queues)");
  check("drive: traffic rammed the standing player car", (float)rams, 0.f, 0.f, "0 (brakes; box, not circle)");
  check("drive: frames with two driving cars overlapping", (float)overlap, 0.f, 0.f, "0");
}

// ---------------------------------------------------------------- стоящие машины
// №1159: с круга 5 (TEST82) trafficReset оставляет все стоящие места пустыми («в зонном режиме сначала пусто»), а места выбирает и ставит загрузка зоны в trafficUpdate.
// Гейт, который брал машины сразу после сброса, видел пустые места: «Гомер проходит сквозь стоящую», «стоящую не сдвинуть», «ничего не нашлось». Прогоняем несколько кадров
// с камерой далеко (стоящие вне кадра ставятся сразу), машин и пешеходов трафика гасим — остаются только стоящие места. Из мест игра берёт часть (выбор «сколько стоит» —
// tests/traffic/park_few.c), а гейту нужны все места сразу: после первого кадра выбор переставляем на «все» и даём загрузке поставить их.
static void settleParked(void) {
  float pp[3] = {1e5f, 50.f, 1e5f}, eye[3] = {1e5f, 53.f, 1e5f}, fw[3] = {1.f, 0.f, 0.f}; int mc = T.maxCars, mp = T.maxPeds;
  T.maxCars = 0; T.maxPeds = 0;
  for (int k = 0; k < 8; k++) {
    carInit(&player, NULL, &world, pp[0], pp[1], pp[2], 0.f); trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
    if (k == 0) for (int i = 0; i < T.npark; i++) T.park[i].sel = !T.park[i].off;
  }
  T.maxCars = mc; T.maxPeds = mp;
}
static void parked(void) {
  trafficReset(&T, &world); settleParked();
  int badModel = 0, moved = 0, pen = 0, wheels = 0, pairs = 0;
  for (int i = 0; i < T.npark; i++) {
    if (T.park[i].off) continue;   // №стоящие: пустое место (TRAFFIC_PARK_FEW_OLD=1 — заняты все)
    const TrPark *k = &T.parkd[i]; const Car *c = &T.park[i].car;
    int m = trafficParkModel(&T, k->name), slotPark = 0;
    for (unsigned s = 0; s < T.hd->nSlot; s++) if (T.slot[s].model == k->model && T.slot[s].park) slotPark = 1;
    if (m != k->model || !slotPark) badModel++;
    if (hypotf(c->x - k->x, c->z - k->z) > 1.f || fabsf(wrapPi_(c->h - k->h)) > .1f) moved++;
    CarBox b; carBox(c, &b); b.hw *= .9f; b.hl *= .9f; colProbeCar(&world, &b); if (b.maxDepth > .15f) pen++;
    if (c->wheels < 3) wheels++;
    for (int j = i + 1; j < T.npark; j++) if (!T.park[j].off && sat(c, &T.park[j].car) > .1f) pairs++;
  }
  check("park: spots in data", (float)T.npark, 1.f, 1e9f, "CarStart flag 1 (level 1: 93)");
  check("park: model != FNV-1a rule / not a park model", (float)badModel, 0.f, 0.f, "0");
  check("park: shoved off the spot by placement (share)", T.npark ? (float)moved / T.npark : 1.f, 0.f, .05f, "<= 5%");   // №1159: порог сдвига 1,0 м (был 0,75): Z2PC8 у Kwik вытесняется из стены на 0,86 м (car.c/col.c с TEST83, не traffic.c)
  check("park: body inside the world (share)", T.npark ? (float)pen / T.npark : 1.f, 0.f, .05f, "<= 5%");
  check("park: fewer than 3 wheels on the ground (share)", T.npark ? (float)wheels / T.npark : 1.f, 0.f, .05f, "<= 5%");
  check("park: overlapping pairs", (float)pairs, 0.f, 0.f, "0");
}

// ---------------------------------------------------------------- твёрдость
// Гомер бежит 3 с курсом h от (x, z): насколько продвинулся вдоль курса
static float trkX, trkZ, trkMin, trkLat; static int trk;   // слежение: ближе всего к точке (пешеходу), боковой уход
static float homerRun(float x, float z, float h, float *minGap, const float box[5]) {
  homerPlace(&H, &world, x, ground(x, z, 50.f) + .5f, z, h);
  HomerInput in; memset(&in, 0, sizeof in); in.camYaw = h; in.my = 1.f;
  float fx = sinf(h), fz = cosf(h), best = 0.f; trkMin = 1e9f; trkLat = 0.f;
  for (int i = 0; i < 90; i++) {
    homerUpdate(&H, &world, NULL, &in, DT);
    if (trk) { float d = hypotf(H.x - trkX, H.z - trkZ); if (d < trkMin) trkMin = d;
      float l = fabsf(-(H.x - x) * fz + (H.z - z) * fx); if (l > trkLat) trkLat = l; }
    float p[3] = {H.x, H.y, H.z}, eye[3] = {H.x - fx * 4.f, H.y + 2.f, H.z - fz * 4.f}, fw[3] = {fx, 0, fz};
    trafficUpdate(&T, &world, &player, 0, p, eye, fw, DT);
    float a = (H.x - x) * fx + (H.z - z) * fz; if (a > best) best = a;
    if (box && minGap) {   // box: центр x z, курс, полуширина, полудлина -> знаковое расстояние до кузова
      float dx = H.x - box[0], dz = H.z - box[1], bf = sinf(box[2]), bz = cosf(box[2]);
      float la = fabsf(dx * bf + dz * bz) - box[4], ls = fabsf(-dx * bz + dz * bf) - box[3];
      float g = (la > 0.f && ls > 0.f) ? hypotf(la, ls) : fmaxf(la, ls); if (g < *minGap) *minGap = g;
    }
  }
  return best;
}
static void solid(void) {
  trafficReset(&T, &world); settleParked();
  for (int i = 0; i < TR_MAXCAR; i++) T.car[i].mode = TRM_OFF; T.maxCars = 0;   // без движущихся машин (им своё место)
  int savePeds = T.maxPeds; T.maxPeds = 1;                                        // один служебный пешеход
  carInit(&player, NULL, &world, KWIK[0] + 400.f, 50.f, KWIK[1], 0.f);            // машина игрока далеко
  // 1. Гомер о стоящую машину: ближайшие к Kwik места, поперёк кузова с обеих сторон; контроль — машины нет
  float blocked = -1.f, gap = -1.f; int tested = 0;
  {
    int order[256], no = 0;
    for (int i = 0; i < T.npark && no < 256; i++) if (!T.park[i].off) order[no++] = i;
    for (int i = 0; i < no; i++) for (int j = i + 1; j < no; j++) {
      float di = hypotf(T.parkd[order[i]].x - KWIK[0], T.parkd[order[i]].z - KWIK[1]), dj = hypotf(T.parkd[order[j]].x - KWIK[0], T.parkd[order[j]].z - KWIK[1]);
      if (dj < di) { int t = order[i]; order[i] = order[j]; order[j] = t; }
    }
    for (int q = 0; q < no && q < 12 && blocked < 0.f; q++) {
      int i = order[q]; Car *c = &T.park[i].car; CarBox b; carBox(c, &b);
      for (int side = -1; side <= 1 && blocked < 0.f; side += 2) {
        float rx = -b.fz * side, rz = b.fx * side, sx = b.x + rx * (b.hw + 3.f), sz = b.z + rz * (b.hw + 3.f), h = atan2f(-rx, -rz);
        // старт на том же полу, что и машина: верхняя опора в точке старта (с 28.09 там может стоять грузовик Duff с
        // подъёмником у Z2PC2 — AnimCollision) не должна поднимать Гомера над кузовом
        if (fabsf(ground(sx, sz, 50.f) - ground(c->x, c->z, 50.f)) > 1.f) continue;
        float cx = c->x; c->x += 1000.f; float free_ = homerRun(sx, sz, h, NULL, NULL); c->x = cx;   // контроль: машины нет
        if (free_ < b.hw * 2.f + 4.f) continue;                                                       // путь занят чем-то ещё
        tested = 1; float mg = 1e9f, box[5] = {b.x, b.z, atan2f(b.fx, b.fz), b.hw, b.hl};
        blocked = homerRun(sx, sz, h, &mg, box); gap = mg;
        printf("  (Homer -> parked %s: control run %.1f m, with car %.1f m, closest %.2f m)\n", T.parkd[i].name, free_, blocked, mg);
        blocked = blocked < 3.f + .1f ? 1.f : 0.f;
      }
    }
  }
  check("solid: Homer stopped by a parked car", tested ? blocked : 0.f, 1.f, 1.f, "1: runs 3 s into the side, stays outside");
  check("solid: Homer closest to the parked body, m", tested ? gap : -1.f, HOMER_RADIUS - .06f, 1.f, ">= radius 0.3");
  // 2. Гомер о пешехода: свободная прямая у Kwik (контроль без пешехода), пешеход в 3 м
  float px = 0, pz = 0, ph = 0; int found = 0;
  for (int r = 0; r < 30 && !found; r += 3) for (int a = 0; a < 16 && !found; a++) {   // прямая: 7 м вперёд, вбок < 5 см
    float x = KWIK[0] + r * cosf(a * .3927f), z = KWIK[1] + r * sinf(a * .3927f), h = a * .3927f;
    T.ped[0].mode = TRP_OFF; trk = 1; trkX = trkZ = 1e9f;
    float f = homerRun(x, z, h, NULL, NULL); trk = 0;
    if (f > 7.f && trkLat < .05f) { px = x; pz = z; ph = h; found = 1; }
  }
  float homerPass = -1.f, pedMoved = -1.f, closest = -1.f;
  if (found) {
    float qx = px + sinf(ph) * 3.f, qz = pz + cosf(ph) * 3.f;
    trafficPedPlace(&T, &world, 0, qx, qz, ph + 3.14159f);
    trk = 1; trkX = qx; trkZ = qz;
    float adv = homerRun(px, pz, ph, NULL, NULL); trk = 0;
    homerPass = adv; pedMoved = hypotf(T.ped[0].x - qx, T.ped[0].z - qz); closest = trkMin;
    printf("  (Homer -> pedestrian at %.1f %.1f: advanced %.2f m of 3.00, limit %.2f; closest centres %.2f m, side drift %.2f m)\n",
           qx, qz, adv, 3.f - HOMER_RADIUS - TR_PED_R, trkMin, trkLat);
  }
  check("solid: straight free run found near Kwik E Mart", (float)found, 1.f, 1.f, "1");
  check("solid: Homer advance into a pedestrian 3 m ahead, m", found ? homerPass : 99.f, 0.f, 3.f - HOMER_RADIUS - TR_PED_R + .05f, "<= 3 - 0.3 - 0.35");
  check("solid: Homer and pedestrian closest centres, m", found ? closest : 0.f, HOMER_RADIUS + TR_PED_R - .05f, 1e9f, ">= 0.3 + 0.35");
  check("solid: pedestrian not pushed by Homer, m", found ? pedMoved : 99.f, 0.f, .05f, "0");
  // 2а. то же, пешеход на 0.25 м в стороне от линии бега (как в эмуляторе 28.09: круги соскальзывали — Гомер обходил)
  float offAdv = 99.f, offClose = 0.f;
  if (found) {
    float qx = px + sinf(ph) * 3.f + cosf(ph) * .25f, qz = pz + cosf(ph) * 3.f - sinf(ph) * .25f;
    trafficPedPlace(&T, &world, 0, qx, qz, ph + 3.14159f);
    trk = 1; trkX = qx; trkZ = qz; offAdv = homerRun(px, pz, ph, NULL, NULL); trk = 0; offClose = trkMin;
    printf("  (Homer -> pedestrian 0.25 m off his line: advanced %.2f m of 3.00, closest centres %.2f m, side drift %.2f m)\n", offAdv, trkMin, trkLat);
  }
  check("solid: Homer advance, pedestrian 0.25 m off the line, m", found ? offAdv : 99.f, 0.f, 3.f - .4f, "stops (does not slide past)");
  check("solid: same, closest centres, m", found ? offClose : 0.f, HOMER_RADIUS + TR_PED_R - .05f, 1e9f, ">= 0.65");
  // 2б. пешеход идёт по своему пути на стоящего Гомера: останавливается, сквозь не проходит
  float pedAdv = 99.f, pedClose = 0.f; int walkOk = 0;
  {
    int bp = -1, bk = 0; float bd = 1e9f;
    for (unsigned p = 0; p < T.hd->nPath; p++) for (unsigned k = 0; k < T.path[p].n; k++) {
      const float *a = T.pathPt + 3 * (T.path[p].first + k), *b = T.pathPt + 3 * (T.path[p].first + (k + 1) % T.path[p].n);
      if (hypotf(b[0] - a[0], b[2] - a[2]) < 8.f) continue;
      float d = hypotf(a[0] - KWIK[0], a[2] - KWIK[1]); if (d < bd) { bd = d; bp = (int)p; bk = (int)k; }
    }
    if (bp >= 0) {
      const float *a = T.pathPt + 3 * (T.path[bp].first + bk), *b = T.pathPt + 3 * (T.path[bp].first + (bk + 1) % T.path[bp].n);
      float L = hypotf(b[0] - a[0], b[2] - a[2]), ux = (b[0] - a[0]) / L, uz = (b[2] - a[2]) / L;
      trafficReset(&T, &world); settleParked(); T.maxCars = 0;
      TrPed *e = &T.ped[0]; memset(e, 0, sizeof *e);
      e->model = 0; e->path = bp; e->seg = bk; e->dir = 1; e->speed = T.walkV[0]; e->x = a[0]; e->y = a[1]; e->z = a[2]; e->h = atan2f(ux, uz);
      skinInit(&e->sp, &T.peds[0]); skinPlay(&e->sp, T.clip[0][TRC_WALK], 1, 0.f); e->mode = TRP_WALK;
      float hx = a[0] + ux * 4.f, hz = a[2] + uz * 4.f; homerPlace(&H, &world, hx, a[1] + 1.f, hz, 0.f);
      HomerInput in; memset(&in, 0, sizeof in); in.camYaw = 0.f; pedClose = 1e9f; pedAdv = 0.f;
      for (int i = 0; i < 150; i++) {
        homerUpdate(&H, &world, NULL, &in, DT);
        float p[3] = {H.x, H.y, H.z}, eye[3] = {H.x, H.y + 2, H.z - 4}, fw[3] = {0, 0, 1};
        trafficUpdate(&T, &world, &player, 0, p, eye, fw, DT);
        if (e->mode != TRP_WALK && e->mode != TRP_STAND) break;   // лицом к Гомеру (TRP_FACE) — конец отсчёта
        pedClose = fminf(pedClose, hypotf(e->x - H.x, e->z - H.z)); pedAdv = fmaxf(pedAdv, (e->x - a[0]) * ux + (e->z - a[2]) * uz);
      }
      walkOk = e->mode == TRP_WALK || e->mode == TRP_STAND || e->mode == TRP_FACE;   // №1159: в 1,4 м от Гомера пешеход встаёт лицом к нему (P02-37) — тоже «остановился»
      printf("  (pedestrian walks path %d seg %d (%.1f m) into Homer 4 m ahead: advanced %.2f m, closest %.2f m)\n", bp, bk, L, pedAdv, pedClose);
    }
  }
  check("solid: pedestrian walking into Homer stops short, m", walkOk ? pedAdv : 99.f, 0.f, 4.f - HOMER_RADIUS - TR_PED_R + .05f, "<= 4 - 0.65");
  check("solid: pedestrian and Homer closest centres, m", walkOk ? pedClose : 0.f, HOMER_RADIUS + TR_PED_R - .05f, 1e9f, ">= 0.65");
  trafficReset(&T, &world); settleParked(); T.maxCars = 0;
  // 3. машина игрока о стоящую: вдоль кузова сзади 14 м на 10 м/с; контроль — стоящей нет. №1159: без газа машина теперь сама сбрасывает ход (за 2 с не доезжала до стоящей даже без неё) —
  // держим ход газом 0,6 (около 10 м/с)
  float shove = -1.f, overlap = -1.f, through = -1.f; int ct = 0;
  {
    for (int i = 0; i < T.npark && ct == 0; i++) {
      if (T.park[i].off) continue;
      Car *c = &T.park[i].car;   // правка 337: стоящих стало меньше (у Kwik-E-Mart пусто, как на записи PS2) — любая стоящая уровня
      for (int dir = -1; dir <= 1 && ct == 0; dir += 2) {
        CarBox b; carBox(c, &b); float fx = b.fx * dir, fz = b.fz * dir, sx = b.x - fx * (b.hl + 12.f), sz = b.z - fz * (b.hl + 12.f), h = atan2f(fx, fz);
        float cx = c->x; c->x += 1000.f;
        carInit(&player, NULL, &world, sx, ground(sx, sz, c->y + 3.f), sz, h); carSetSpeed(&player, 10.f);
        float pp[3], eye[3], fw[3] = {fx, 0, fz};
        for (int f = 0; f < 60; f++) { CarInput ci = {.6f, 0, 0, 0}; carUpdate(&player, &world, &ci, DT);
          pp[0] = player.x; pp[1] = player.y; pp[2] = player.z; eye[0] = player.x; eye[1] = player.y + 3.f; eye[2] = player.z;
          trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); }
        float passC = (player.x - b.x) * fx + (player.z - b.z) * fz;
        c->x = cx;
        if (passC < b.hl + 2.f) continue;                                  // контроль не проехал — путь занят
        trafficReset(&T, &world); settleParked(); T.maxCars = 0; T.ped[0].mode = TRP_OFF;
        Car before = *c; ct = 1;
        carInit(&player, NULL, &world, sx, ground(sx, sz, c->y + 3.f), sz, h); carSetSpeed(&player, 10.f);
        float worst = 0.f, far_ = -1e9f;
        for (int f = 0; f < 120; f++) { CarInput ci = {.6f, 0, 0, 0}; carUpdate(&player, &world, &ci, DT);
          pp[0] = player.x; pp[1] = player.y; pp[2] = player.z; eye[0] = player.x; eye[1] = player.y + 3.f; eye[2] = player.z;
          trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
          if (f > 60) worst = fmaxf(worst, sat(&player, c));
          far_ = fmaxf(far_, (player.x - before.x) * fx + (player.z - before.z) * fz); }
        shove = hypotf(c->x - before.x, c->z - before.z); overlap = worst; through = far_;
        printf("  (car -> parked %s: control passed %.1f m beyond, shove %.2f m, overlap %.2f m, player front reach %.2f m)\n",
               T.parkd[i].name, passC, shove, worst, far_);
      }
    }
  }
  check("solid: a clear approach to a parked car found", (float)ct, 1.f, 1.f, "1");
  check("solid: parked car shoved by the player's car, m", ct ? shove : 0.f, .2f, 50.f, "> 0.2 (hit and pushed)");
  check("solid: bodies overlap after the hit, m", ct ? overlap : 9.f, 0.f, .15f, "<= 0.15");
  check("solid: player car centre stays behind parked centre, m", ct ? through : 9.f, -1e9f, 0.f, "< 0 (did not pass through)");
  // 4. машина игрока сбивает пешехода (та же свободная прямая, 10 м/с)
  int flew = 0, ev = 0;
  if (found) {
    float qx = px + sinf(ph) * 3.f, qz = pz + cosf(ph) * 3.f;
    trafficReset(&T, &world); settleParked(); T.maxCars = 0; trafficPedPlace(&T, &world, 0, qx, qz, ph + 3.14159f);
    float sx = qx - sinf(ph) * 12.f, sz = qz - cosf(ph) * 12.f;
    carInit(&player, NULL, &world, sx, ground(sx, sz, 50.f), sz, ph); carSetSpeed(&player, 10.f);
    for (int f = 0; f < 60; f++) { CarInput ci = {0, 0, 0, 0}; carUpdate(&player, &world, &ci, DT);
      float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3.f, player.z}, fw[3] = {sinf(ph), 0, cosf(ph)};
      trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
      if (T.ped[0].mode == TRP_FLY) flew = 1;
      TrEvent e; while (trafficPopEvent(&T, &e)) ev += e.type == TR_EV_PED_HIT; }
  }
  check("solid: pedestrian hit by the car flies", (float)flew, 1.f, 1.f, "1 (flail)");
  check("solid: PED_HIT events", (float)ev, 1.f, 1.f, "1");
  T.maxPeds = savePeds; T.maxCars = T.hd->nSlot < TR_MAXCAR ? (int)T.hd->nSlot : TR_MAXCAR;
  // 5. удар по машине трафика: навстречу на 8 м/с
  trafficReset(&T, &world);
  carInit(&player, NULL, &world, 220.f, ground(220.f, 172.f, 50.f), 172.f, 0.f);
  int phys = 0, drv = -1;
  for (int f = 0; f < 90 && drv < 0; f++) { float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3, player.z - 7}, fw[3] = {0, 0, 1};
    CarInput ci = {0, 0, 0, 1}; carUpdate(&player, &world, &ci, DT); trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT);
    for (int i = 0; i < T.maxCars; i++) if (T.car[i].mode == TRM_DRIVE && T.car[i].v > 3.f) drv = i; }
  if (drv >= 0) {
    Car *k = &T.car[drv].car; float h = k->h + 3.14159265f, sx = k->x + sinf(k->h) * 9.f, sz = k->z + cosf(k->h) * 9.f;
    carInit(&player, NULL, &world, sx, k->y + .5f, sz, h); carSetSpeed(&player, 8.f);
    for (int f = 0; f < 60 && !phys; f++) { CarInput ci = {0, 0, 0, 0}; carUpdate(&player, &world, &ci, DT);
      float pp[3] = {player.x, player.y, player.z}, eye[3] = {player.x, player.y + 3, player.z}, fw[3] = {sinf(h), 0, cosf(h)};
      trafficUpdate(&T, &world, &player, 1, pp, eye, fw, DT); phys = T.car[drv].mode == TRM_PHYS || T.car[drv].mode == TRM_WRECK; }
  }
  check("solid: traffic car hit head-on becomes a rigid body", (float)phys, 1.f, 1.f, "1 (carHitCar -> PHYS)");
}

// ---------------------------------------------------------------- подброски
enum { V_REAL, V_LANES, V_PARKROT, V_PARKDUP, V_PARKMODEL, V_PEDH0, V_NOSOLID, V_N };
static const char *VNAME[V_N] = {"real data", "lanes shifted 20 m", "parked heading +90 deg", "parked spot duplicated",
                                 "parked model not a park model", "pedestrian height 0", "traffic not solid"};
static const char *VEXPECT[V_N] = {"", "lanes:", "park: overlapping", "park: overlapping", "park: model", "solid: Homer and pedestrian closest",
                                   "solid: Homer stopped"};
static void plant(int v, unsigned char *buf) {
  TrHeader *h = (TrHeader *)buf;
  if (v == V_LANES) { TrPt *p = (TrPt *)(buf + h->offPt); for (unsigned i = 0; i < h->nPt; i++) p[i].x += 20.f; }
  if (v == V_PARKROT) { TrPark *k = (TrPark *)(buf + h->offPark); for (unsigned i = 0; i < h->nPark; i++) k[i].h += 1.5707963f; }
  if (v == V_PARKDUP && h->nPark > 1) { TrPark *k = (TrPark *)(buf + h->offPark); char nm[12]; memcpy(nm, k[1].name, 12); k[1] = k[0]; memcpy(k[1].name, nm, 12);
    k[1].model = (uint16_t)k[0].model; }
  if (v == V_PARKMODEL) { TrPark *k = (TrPark *)(buf + h->offPark); TrSlot *s = (TrSlot *)(buf + h->offSlot); int m = -1;
    for (unsigned i = 0; i < h->nSlot; i++) if (!s[i].park) m = s[i].model;
    for (unsigned i = 0; i < h->nPark && m >= 0; i++) k[i].model = (uint16_t)m; }
  if (v == V_PEDH0) { TrPedModel *p = (TrPedModel *)(buf + h->offPed); for (unsigned i = 0; i < h->nPed; i++) p[i].height = 0.f; }
}

int main(int argc, char **argv) {
  const char *lvl = argc > 1 ? argv[1] : "build/level.shr", *trf = argc > 2 ? argv[2] : "build/traffic.shr", *hom = argc > 3 ? argv[3] : "build/homer.shr";
  FILE *f = fopen(lvl, "rb"); if (!f) { printf("no %s\n", lvl); return 2; }
  unsigned char hdr[104]; if (fread(hdr, 1, 104, f) != 104 || memcmp(hdr, "SHR7", 4)) { printf("bad level\n"); return 2; }
  unsigned pvsOff = *(unsigned *)(hdr + 88), colOff = *(unsigned *)(hdr + 92), propOff = *(unsigned *)(hdr + 96);
  fseek(f, 0, SEEK_END); unsigned fsize = (unsigned)ftell(f);
  unsigned end = fsize; if (pvsOff > colOff && pvsOff < end) end = pvsOff; if (propOff > colOff && propOff < end) end = propOff;
  unsigned char *cm = readAt(f, colOff, end - colOff); fclose(f);
  if (!cm || colInit(&world, cm, end - colOff)) { printf("COL1 failed\n"); return 2; }
  FILE *g = fopen(trf, "rb"); if (!g) { printf("no %s\n", trf); return 2; }
  fseek(g, 0, SEEK_END); unsigned tsize = (unsigned)ftell(g); unsigned char *orig = readAt(g, 0, tsize); fclose(g);
  unsigned char *buf = aligned_alloc(64, (tsize + 63) & ~63u);
  homerM = skinLoad(hom); if (!homerM || homerInit(&H, homerM)) { printf("no %s\n", hom); return 2; }
  H.solid = trafficSolid; H.solidUser = &T;   // как в main.c (цепочка твёрдых тел: здесь только трафик)
  int realFails = 0, red = 0, skipped = 0, blind = 0, nPk = 0;
  for (int v = 0; v < V_N; v++) {
    memcpy(buf, orig, tsize); plant(v, buf);
    fails = 0; failed[0] = 0;
    printf("== %s%s\n", v ? "PLANTED: " : "", VNAME[v]);
    int r = trafficInitMem(&T, buf, tsize, 0);
    if (r) { printf("  FAIL trafficInitMem %d\n", r); fails++; strcat(failed, "load; "); }
    else {
      if (v == V_NOSOLID) T.noSolid = 1;
      if (v == V_REAL) printf("  data: %u junctions, %u roads, %u lane points, %u car models, %d parked spots, %d pedestrian models, %u paths\n",
                              T.hd->nJunc, T.hd->nRoad, T.hd->nPt, T.hd->nModel, T.npark, T.npeds, T.hd->nPath);
      if (v == V_REAL || v == V_LANES) { lanes(); drive(); }
      if (v == V_REAL || v == V_PARKROT || v == V_PARKDUP || v == V_PARKMODEL) parked();
      if (v == V_REAL || v == V_PEDH0 || v == V_NOSOLID) solid();
      blind = v == V_PARKROT && T.npark < 40; nPk = T.npark;   // №1159: поворот +90° виден только по пересечению соседей и стен; мест 10 из 93 (экспорт «стоящих меньше») — между ними пусто
      trafficFree(&T);
    }
    if (v == V_REAL) { realFails = fails; printf("  -> real data: %s\n", fails ? "RED (must be green)" : "GREEN"); }
    else {
      int hit = strstr(failed, VEXPECT[v]) != NULL; red += hit;
      if (!hit && blind) { skipped++; printf("  -> planted: BLIND on sparse data (%d parked spots < 40: rotated cars touch nothing) — not counted as caught, not as missed\n", nPk); }
      else printf("  -> planted: %s (failed: %s)\n", hit ? "RED as it must be" : "NOT CAUGHT", failed[0] ? failed : "nothing");
    }
  }
  int ok = !realFails && red + skipped == V_N - 1;
  printf("GATE traffic host: real %s, planted caught %d/%d (blind %d) -> %s\n", realFails ? "RED" : "GREEN", red, V_N - 1, skipped, ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
