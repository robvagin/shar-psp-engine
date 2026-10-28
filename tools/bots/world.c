// Игра без экрана для ботов (пункт Б0): см. world.h. Кадр — как игровая ветка цикла viewer/main.c (△, RESET CAR,
// машина, Гомер, пропы, стейтпропы, монеты, возврат вне вида, спад шкалы Hit & Run), логика — viewer/game.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "rooms.h"
#include "traffic.h"
#if __has_include("msn_audio.h")   // звук миссий есть в этой копии viewer/ (гейт Т1.4 собирает и из старых)
#include "msn_audio_host.h"
#define BW_HAVE_MSN_AUDIO 1
#endif

unsigned long long bwTotalFrames;   // все кадры процесса (не часть мира: save/load его не трогают)
int bwLastDeathZone = -1;   // observer only; no simulation/save state changes
static void printFrames(void) { printf("F\t%llu\n", bwTotalFrames); }
static unsigned char *lvl; static size_t lvlSize; static ColWorld col; static int haveColData;
static const void *propSec; static unsigned propSecN; static float carLift = 0.f, spawn[4];
static CarParams carP; static int carPOk = -9;
#ifdef BW_HAVE_AO
static int aoOff;   // BW_NO_AO=1: объекты ANM1 стоят (как до правки 116) — отрицательный контроль подъёмника m2
#else
enum { aoOff = 1 };
#endif   // машина уровня из CAR1 v2 (как main.c): 1 — прочитана, иначе famil_v
static SkinModel *homerM; static void *msnPack; static size_t msnSize; static void *collPack; static size_t collSize;
// ------------------------------------------------------------------ живой город (Т1.2): viewer/traffic.c, состояние — блоб снимка
static Traffic tr; static int haveTr; static unsigned char *tr0;   // tr0 — трафик новой игры (после trafficReset): bwNew возвращает его
#define TRPED_HEAD offsetof(TrPed, sp)          // пешеход без позы рисования (поза считается только при отрисовке)
#define TRSP_HEAD offsetof(SkinPlayer, xf)      // проигрыватель клипа: клип, время, смешивание, моргание
#define TRTAIL offsetof(Traffic, maxPeds)       // хвост структуры: группа, таймеры, seed, события, счётчики — без указателей
static size_t trBlobSize(void) {
  return haveTr ? sizeof tr.car + sizeof tr.maxCars + TR_MAXPED * (TRPED_HEAD + TRSP_HEAD) + sizeof(TrParked) * (size_t)tr.npark + (sizeof(Traffic) - TRTAIL) : 0;
}
static void trPut(unsigned char *b) {
  memcpy(b, tr.car, sizeof tr.car); b += sizeof tr.car; memcpy(b, &tr.maxCars, sizeof tr.maxCars); b += sizeof tr.maxCars;
  for (int i = 0; i < TR_MAXPED; i++) { memcpy(b, &tr.ped[i], TRPED_HEAD); b += TRPED_HEAD; memcpy(b, &tr.ped[i].sp, TRSP_HEAD); b += TRSP_HEAD; }
  memcpy(b, tr.park, sizeof(TrParked) * (size_t)tr.npark); b += sizeof(TrParked) * (size_t)tr.npark;
  memcpy(b, (unsigned char *)&tr + TRTAIL, sizeof(Traffic) - TRTAIL);
}
#ifndef BW_BREAK_TRAFFIC_LOAD
#define BW_BREAK_TRAFFIC_LOAD 0   // отрицательный контроль гейта Т1.2: загрузка не возвращает трафик
#endif
static void trGet(const unsigned char *b) {
  memcpy(tr.car, b, sizeof tr.car); b += sizeof tr.car; memcpy(&tr.maxCars, b, sizeof tr.maxCars); b += sizeof tr.maxCars;
  for (int i = 0; i < TR_MAXPED; i++) { memcpy(&tr.ped[i], b, TRPED_HEAD); b += TRPED_HEAD; memcpy(&tr.ped[i].sp, b, TRSP_HEAD); b += TRSP_HEAD; }
  memcpy(tr.park, b, sizeof(TrParked) * (size_t)tr.npark); b += sizeof(TrParked) * (size_t)tr.npark;
  memcpy((unsigned char *)&tr + TRTAIL, b, sizeof(Traffic) - TRTAIL);
}
const void *bwTraffic(void) { return haveTr ? &tr : 0; }
int bwTrafficCounts(int *cars, int *peds, int *parked) {
  if (!haveTr) { *cars = *peds = *parked = 0; return 0; }
  int c = 0, p = 0; for (int i = 0; i < tr.maxCars; i++) c += tr.car[i].mode != TRM_OFF; for (int i = 0; i < tr.maxPeds; i++) p += tr.ped[i].mode != TRP_OFF;
  *cars = c; *peds = p; *parked = tr.npark; return 1;
}

static void *readAll(const char *p, size_t *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); rewind(f);
  unsigned char *b = aligned_alloc(16, ((size_t)s + 15) & ~(size_t)15);
  if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (size_t)s; return b;
}
static void loadRooms(const char *level);
static void loadSkinSpots(const char *level);
// машины миссий (viewer/msncar.h): геометрия моделей из каталога пакета MCR1 (tools/msncar_export.py) — кузов, колёса,
// посадка; нет пакета — модели трафика того же имени, иначе famil_v (BW_MSNCARS=путь — другой пакет)
static McGeo carGeo[64]; static int nCarGeo;
static int haveMsnAudio;   // звук миссий загружен (длины реплик, события музыки): без него разговоры кончаются за 1 с
// П1 (L1_ФИНИШ 30.09): что из спутников миссий нашлось рядом с level.shr — песочница уровня сверяет, что мир ботов тот же, что у игры
void bwMissionPacks(int *carGeo, int *audio) { *carGeo = nCarGeo; *audio = haveMsnAudio; }
static void loadCarGeo(const char *level);
// Номер уровня (docs/РЕЦЕПТ_УРОВНЯ.md §3.3): из заголовка пакета миссий MSN1 (u16 @6), без миссий — BW_LEVEL, иначе 1.
// По нему — имена спутников рядом с level.shr: collect-l<N>.shr, interiors-l<N>.bin (как у EBOOT, viewer/main.c levelFile)
static int bwLevelNo = 1;
static void sibling(char *out, size_t n, const char *level, const char *fmt) {
  char nm[64]; snprintf(nm, sizeof nm, fmt, bwLevelNo);
  snprintf(out, n, "%s", level); char *sl = strrchr(out, '/'); snprintf(sl ? sl + 1 : out, n - (sl ? (size_t)(sl + 1 - out) : 0), "%s", nm);
}
int bwLevel(void) { return bwLevelNo; }
const char *bwCarName(void) { return carPOk == 1 ? carP.name : "famil_v"; }   // машина уровня (CAR1 v2)
int bwCarSource(void) { return carPOk; }
int bwData(const char *level, const char *homer, const char *msn) {
  if (getenv("BW_FRAMES")) atexit(printFrames);   // оркестраторам: строка "F\t<кадров>" в конце stdout (игровые минуты ночи)
  if (!(lvl = readAll(level, &lvlSize)) || lvlSize < 104 || memcmp(lvl, "SHR7", 4)) return -1;
  float hd[14]; memcpy(hd, lvl + 12, sizeof hd); memcpy(spawn, hd + 6, sizeof spawn);   // спавн x y z курс (заголовок SHR7)
  unsigned pv = *(unsigned *)(lvl + 88), co = *(unsigned *)(lvl + 92), po = *(unsigned *)(lvl + 96), c1 = *(unsigned *)(lvl + 100), ce = (unsigned)lvlSize;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (co && colInit(&col, lvl + co, ce - co) == 0) haveColData = 1;
  // правка Роба 108 (30.09, m4 у ботов «застрял пешком» у двери): в мире ботов анимированные объекты не шагали, створки дверей
  // AutomaticDoor (ANM1 действие 4) стояли закрытыми. В игре дверь открывается, когда игрок в её объёме (viewer/animobj.c
  // AO_AUTODOOR) — у ботов её живые объёмы COL1 считались открытыми (размер 0). Правка 116: объекты шагают (aoStep, как
  // EBOOT), дверь открывает сам клип; обход остался только для BW_NO_AO=1 / копии viewer/ без animobj
#ifdef BW_HAVE_AO
  aoOff = getenv("BW_NO_AO") != 0;
#endif
  if (aoOff && haveColData && po && (size_t)po + 60 <= lvlSize) {
    unsigned ao = *(unsigned *)(lvl + po + 52), asz = *(unsigned *)(lvl + po + 56);
    const unsigned char *A = lvl + po + ao;
    if (ao && asz >= 32 && (size_t)po + ao + asz <= lvlSize && !memcmp(A, "ANM1", 4)) {
      unsigned nobj = *(const unsigned *)(A + 8), nvol = *(const unsigned *)(A + 12), oo = *(const unsigned *)(A + 16), ov = *(const unsigned *)(A + 20);
      for (unsigned v = 0; v < nvol; v++) {
        const unsigned char *V = A + ov + 72 * v; unsigned kind = V[0], obj = *(const unsigned short *)(V + 2), ci = *(const unsigned *)(V + 4);
        if (obj >= nobj || A[oo + 80 * obj + 8] != 4) continue;
        if (kind == 0 && ci < col.nbox) { ColBox *b = (ColBox *)&col.box[ci]; b->h[0] = b->h[1] = b->h[2] = 0.f; }
        else if (kind == 1 && ci < col.ncyl) { ColCyl *c = (ColCyl *)&col.cyl[ci]; c->r = 0.f; c->halfLen = 0.f; }
      }
    }
  }
  if (po && (size_t)po + 64 <= lvlSize) { propSec = lvl + po; propSecN = *(unsigned *)(lvl + po + 40); }
  if (c1 && (size_t)c1 + 80 <= lvlSize && !memcmp(lvl + c1, "CAR1", 4)) {
    carLift = *(const float *)(lvl + c1 + 32);   // как carVisInit: lift
    unsigned ce1 = (unsigned)lvlSize; if (pv > c1 && pv < ce1) ce1 = pv;
    carPOk = carParamsFromCar1(lvl + c1, ce1 - c1, &carP);   // как main.c: имя машины, ходовые .con, кузов, посадка, камера
    if (carPOk < 0) fprintf(stderr, "машина: %s — famil_v\n", carParamsError(carPOk));
  }
  if (homer && !(homerM = skinLoad(homer))) return -2;
  if (msn && !(msnPack = readAll(msn, &msnSize))) return -4;
  { const char *e = getenv("BW_LEVEL"); if (e && atoi(e) >= 1 && atoi(e) <= 7) bwLevelNo = atoi(e); }
  if (msnPack && msnSize >= 8 && !memcmp(msnPack, "MSN1", 4)) { unsigned short lv; memcpy(&lv, (char *)msnPack + 6, 2); if (lv >= 1 && lv <= 7) bwLevelNo = lv; }
#ifdef BW_HAVE_MSN_AUDIO
  if (msn && !getenv("BW_NO_AUDIO")) {   // звук миссий: длины реплик и события музыки из build/audio/ (рядом с уровнем; BW_AUDIO=папка/)
    char path[512]; const char *e = getenv("BW_AUDIO");
    if (e) snprintf(path, sizeof path, "%s", e);
    else { snprintf(path, sizeof path, "%s", level); char *sl = strrchr(path, '/'); snprintf(sl ? sl + 1 : path, sizeof path - (sl ? (size_t)(sl + 1 - path) : 0), "audio/"); }
    if (msnAudioHostLoad(path) == 0) { gameMsnSetAudio(msnAudioHostBackend()); haveMsnAudio = 1; }
  }
#endif
  if (!getenv("BW_NO_ROOMS")) { loadRooms(level); loadSkinSpots(level); }   // BW_NO_ROOMS=1 — без комнат (отрицательный контроль гейта 11.2)
  if (!getenv("BW_NO_TRAFFIC") && haveColData) {   // Т1.2: build/traffic.shr рядом с уровнем (tools/traffic_export.py); BW_TRAFFIC=путь — другой
    char path[512]; const char *e = getenv("BW_TRAFFIC");
    if (e) snprintf(path, sizeof path, "%s", e);
    else { snprintf(path, sizeof path, "%s", level); char *sl = strrchr(path, '/'); snprintf(sl ? sl + 1 : path, sizeof path - (sl ? (size_t)(sl + 1 - path) : 0), "traffic.shr"); }
    if (trafficLoad(&tr, path) == 0) { haveTr = 1; trafficReset(&tr, &col); tr0 = malloc(trBlobSize() + 1); trPut(tr0); }
    else fprintf(stderr, "трафик: %s не читается — город без трафика\n", path);
  }
  if (haveTr) { gameMsnSetRoads(&tr); mcProbeRoads(&tr, &col); }   // машины миссий: путь по полосам трафика, перекрытые дороги (viewer/msncar.c)
  loadCarGeo(level);                  // кузова машин миссий из пакета моделей (msncars-l<N>.shr рядом с level.shr)
  if (!getenv("BW_NO_COLLECT")) {   // В5: пакет карточек, ос и гэгов рядом с уровнем (tools/collect_export.py); BW_COLLECT=путь — другой
    char path[512]; const char *e = getenv("BW_COLLECT");
    if (e) snprintf(path, sizeof path, "%s", e);
    else sibling(path, sizeof path, level, "collect-l%d.shr");
    collPack = readAll(path, &collSize);
  }
  return haveColData ? 0 : -3;
}
// ------------------------------------------------------------------ комнаты (пункт 11.2): пакет SHI1 рядом с уровнем
static Rooms rooms;   // разбор и переходы — viewer/rooms.c (тот же код, что в EBOOT)
static void loadRooms(const char *level) {   // interiors-l<N>.bin рядом с level.shr (tools/interior_export.py); нет или битый — комнат нет
  char path[512]; sibling(path, sizeof path, level, "interiors-l%d.bin");
  size_t n; unsigned char *b = readAll(path, &n); if (!b) return;
  int e = roomsParse(&rooms, b, n); if (e) { fprintf(stderr, "комнаты: %s — %s, без комнат\n", path, roomsError(e)); free(b); }
}
// Места выбора костюма (skins.txt рядом с level.shr, строки «spot x y z r») — для метки этапа buyskin (viewer/rooms.c roomsSpotTarget), как wardrobeSpot у EBOOT
static float bwSpot[8][4]; static int bwNSpot;
static int bwSpotAt(int i, float *o) { if (i < 0 || i >= bwNSpot) return 0; memcpy(o, bwSpot[i], sizeof bwSpot[i]); return 1; }
static int bwSkinTarget(float px, float py, float pz, float out[3]) {
  int cur = -1; float g, n[3];   // комната Гомера: под ним пол этой комнаты (как roomFloorAt у EBOOT)
  for (int k = 0; k < rooms.nroom && cur < 0; k++) if (colGround(&rooms.room[k].col, px, pz, py + 1.5f, py - 3.f, &g, n)) cur = k;
  return roomsSpotTarget(&rooms, cur, bwSpotAt, px, py, pz, out);
}
static void loadSkinSpots(const char *level) {
  char path[512]; sibling(path, sizeof path, level, "skins.txt");
  FILE *f = fopen(path, "r"); bwNSpot = 0; if (!f) return;
  char ln[160]; while (fgets(ln, sizeof ln, f) && bwNSpot < 8) { float a[4]; if (sscanf(ln, "spot %f %f %f %f", &a[0], &a[1], &a[2], &a[3]) == 4) memcpy(bwSpot[bwNSpot++], a, sizeof a); }
  fclose(f); gameMsnSetSkinTarget(bwSkinTarget);
}
static void loadCarGeo(const char *level) {
  char path[512]; const char *e = getenv("BW_MSNCARS");
  if (e) snprintf(path, sizeof path, "%s", e); else sibling(path, sizeof path, level, "msncars-l%d.shr");
  size_t n; unsigned char *b = readAll(path, &n); if (!b) return;
  unsigned dir = 0; if (n >= 32) memcpy(&dir, b + 16, 4);
  nCarGeo = n >= 32 && dir < n ? mcGeoFromPack(b, b + dir, (unsigned)(n - dir), carGeo, 64) : 0;   // каталог и таблица машин (MCR1 v2)
  free(b); mcSetGeo(carGeo, nCarGeo);
  if (!nCarGeo) fprintf(stderr, "машины миссий: %s не MCR1 — кузова трафика/famil_v\n", path);
}
int bwRooms(void) { return rooms.nroom; }
int bwRoomAt(float x, float y, float z) {   // точка в комнате: ниже −10 м и ближе 40 м к её старту (как у веб-порта); -1 — город
  if (y > -10.f) return -1;
  for (int i = 0; i < rooms.nroom; i++) if (hypotf(x - rooms.room[i].start[0], z - rooms.room[i].start[2]) < 40.f) return i;
  return -1;
}
int bwRoomDoor(int room, float out[3]) { for (int i = 0; i < rooms.ndoor; i++) if (rooms.door[i].room == room) { memcpy(out, rooms.door[i].p, 12); return 1; } return 0; }
void bwRoomExit(int room, float out[3]) { memcpy(out, rooms.room[room].exit, 12); }
const ColWorld *bwCol(void) { return &col; }
void bwSpawn(float *x, float *y, float *z, float *h) { *x = spawn[0]; *y = spawn[1]; *z = spawn[2]; *h = spawn[3]; }
void bwPlace(BotWorld *w, float x, float y, float z, float h) {
  if (!w->haveCol) return;
  carPlace(&w->car, &col, x, y, z, h); w->props.havePreviousCar = 0;
  if (w->haveHomer) { HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &w->car, w->lift); homerSetInCar(&w->homer, &hc); }
}

int bwWhere(float x, float y, float z, float r, int *nlayers, char obj[16]) {
  float ys[16], n3[3], top = 1e4f, px = x, pz = z; int k = 0, sup = -1;
  // опора — поверхность в окне [y - 2.5, y + 0.6] под центром; нет (край, центр над пустотой) — под одной из 4 точек в 1.5 м
  for (int t = 0; t < 5 && sup < 0; t++) {
    px = x + (t == 1 ? 1.5f : t == 2 ? -1.5f : 0.f); pz = z + (t == 3 ? 1.5f : t == 4 ? -1.5f : 0.f); top = 1e4f; k = 0;
    while (k < 16 && colGround(&col, px, pz, top, -1e4f, &ys[k], n3)) { top = ys[k] - 1.f; k++; }   // сверху вниз, слипшиеся в 1 м — одна опора
    for (int i = 0; i < k; i++) if (ys[i] <= y + .6f && ys[i] >= y - 2.5f) { sup = i; break; }
  }
  if (nlayers) *nlayers = k;
  CarBox b; memset(&b, 0, sizeof b); b.x = x; b.z = z; b.fz = 1.f; b.hw = b.hl = r; b.y = sup >= 0 ? ys[sup] : y; b.lo = .2f; b.hi = 2.f;
  int nc = colProbeCar(&col, &b), best = -1; float bd = 0;
  for (int c = 0; c < nc && c < COL_MAX_CONTACTS; c++) if (b.depth[c] > bd) { bd = b.depth[c]; best = c; }
  if (best < 0) snprintf(obj, 16, "-");
  else snprintf(obj, 16, "%c%u", "tbcs"[b.type[best] & 3], (unsigned)b.index[best]);
  return sup >= 0 ? k - 1 - sup : -1;
}

int bwReachable(float ax, float ay, float az, float bx, float by, float bz) {
  if (fabsf(ay - by) >= BW_REACH_DY) return 0;
  if (colCrossFence(&col, ax, az, bx, bz, 0, 0) || colCrossFence(&col, bx, bz, ax, az, 0, 0)) return 0;
  float dx = bx - ax, dz = bz - az; int n = (int)(hypotf(dx, dz) / .5f);
  for (int i = 1; i < n; i++) {
    float t = i / (float)n; CarBox q; memset(&q, 0, sizeof q);
    q.x = ax + dx * t; q.z = az + dz * t; q.fz = 1.f; q.hw = q.hl = .15f; q.y = ay + (by - ay) * t; q.lo = .5f; q.hi = 1.5f;
    colProbeCar(&col, &q); if (q.maxDepth > .05f) return 0;
  }
  return 1;
}

void bwPlantCar(float x, float y, float z, float h) {   // гейт Т1.2: стоящая машина места 0 — препятствием в точку
  if (!haveTr || !tr.npark) return;
  TrParked *P = &tr.park[0]; carInit(&P->car, &tr.params[tr.parkd[0].model], &col, x, y + .5f, z, h); P->state = 0; P->stillT = 0.f;
}
void bwHitTraffic(Car *c) {   // чужая машина бота о трафик и стоящие (тот же carHitCar, что у игрока в trafficUpdate; стоящая не просыпается)
  if (!haveTr) return;
  for (int i = 0; i < tr.maxCars; i++) if (tr.car[i].mode != TRM_OFF && hypotf(tr.car[i].car.x - c->x, tr.car[i].car.z - c->z) < 8.f) carHitCar(c, &tr.car[i].car, &col);
  for (int i = 0; i < tr.npark; i++) if (hypotf(tr.park[i].car.x - c->x, tr.park[i].car.z - c->z) < 8.f) carHitCar(c, &tr.park[i].car, &col);
}
static int solidAll(void *u, float *x, float *z, float y, float r, float *top, float *nx, float *nz) {   // как homerSolidAll в main.c
  BotWorld *w = u; int hit = 0;
  if (w->haveSP) hit |= homerSolidSprops(&w->sp, x, z, y, r, top, nx, nz);
  if (w->haveTraffic) hit |= trafficSolid(&tr, x, z, y, r, top, nx, nz);
  return hit;
}

static void hkKick(void *u, unsigned i, const float pos[3], float v, float dx, float dz) { (void)i; (void)pos; (void)v; (void)dx; (void)dz; ((BotWorld *)u)->st.propKicks++; }
static void hkSp(void *u, unsigned i, float v) { (void)i; (void)v; ((BotWorld *)u)->st.spChanges++; }
static void hkHomer(void *u, const HomerEvent *e) {   // как homerEvFx в main.c: пинок достаёт пешеходов
  BotWorld *w = u; w->st.homerEvents++;
  if (e->type == HOMER_EV_HIT && w->haveTraffic) trafficKick(&tr, e->x, e->y, e->z, e->r, e->dx, e->dz);
}
static void hkCoin(void *u) { ((BotWorld *)u)->st.coins++; }

int bwNew(BotWorld *w) {
  { const char *e_ = getenv("TRFCOST1001B"); if (e_) trfCost1001b = atoi(e_); }   // №1001б: TRFCOST1001B=0 — прежний обход препятствий в pathGap (для «до/после»)   // как newGame в main.c (пункт 12а), без звука и частиц
  memset(w, 0, sizeof *w);
  w->haveCol = haveColData; w->lift = carLift;
  if (w->haveCol) carInit(&w->car, carPOk == 1 ? &carP : NULL, &col, spawn[0], spawn[1], spawn[2], spawn[3]);
  Game *G = &w->G;
  *G = (Game){&col, &w->car, &w->props, &w->sp, &w->coins, &w->homer, &w->haveCol, &w->haveProps, &w->haveSP, &w->haveCoins, &w->haveHomer,
              &w->hnrHeat, &w->hnrCalm, &w->wasDestroyed, w->spSt, &w->spStInit, &w->lift,
              {hkKick, hkSp, 0, hkHomer, 0, hkCoin, 0, 0, w}};
  if (propSec && propsInit(&w->props, propSec, propSecN) == 0) { w->haveProps = 1; w->props.onKick = gamePropKick; w->props.kickUser = G; }
  if (w->haveProps && w->props.coinType >= 0 && w->props.ncoin && coinsInit(&w->coins, w->props.coinPos, w->props.ncoin) == 0) { w->haveCoins = 1; w->coins.dropAuto = 1; }
  if (w->haveProps && w->props.spr && spInit(&w->sp, w->props.spr, w->props.sprSize) == 0) w->haveSP = 1;
#ifdef BW_HAVE_AO
  // правка 116: ANM1 к экземплярам SPR1 и живым объёмам COL1 (как aoBind в main.c). Шаг dt 0 без игрока рядом ставит все
  // живые объёмы в кадр 0: коллизия общая на процесс, прошлый мир (повтор game_headless, новая миссия) мог их сдвинуть
  if (!aoOff && w->haveSP && w->haveCol && aoInit(&w->ao, propSec, propSecN, &w->sp, &col) == 0) {
    w->haveAo = 1; aoStep(&w->ao, &w->sp, &col, 1e6f, 0.f, 1e6f, 0, 0, 0.f); }
#endif
  if (homerM && homerInit(&w->homer, homerM) == 0) {
    w->haveHomer = 1;
    w->haveTraffic = haveTr;
    if (w->haveSP || w->haveTraffic) { w->homer.solid = solidAll; w->homer.solidUser = w; }
    if (w->haveCol) { HomerCar hc; memset(&hc, 0, sizeof hc); homerCarFrom(&hc, &w->car, w->lift); homerSetInCar(&w->homer, &hc); }
  }
  if (msnPack) { char err[160]; w->haveMsn = gameMsnOpen(&w->msn, G, msnPack, msnSize, NULL, err, sizeof err) == 0; if (!w->haveMsn) fprintf(stderr, "миссии: %s\n", err); }
  w->room = -1; w->lastDoor = -1; w->doorArmed = 1;
  w->haveTraffic = haveTr; if (haveTr) trGet(tr0);   // Т1.2: трафик новой игры (стоящие на местах, seed как после загрузки)
  if (collPack) { int e = collectInit(&w->coll, collPack, (unsigned)collSize); w->haveColl = e == 0; if (e) fprintf(stderr, "collect: %s\n", collectError(e)); }
  if (w->haveColl) G->pColl = &w->coll;
  return w->haveCol ? 0 : -1;
}
void bwFree(BotWorld *w) {
  if (w->haveProps) propsFree(&w->props); if (w->haveCoins) coinsFree(&w->coins); if (w->haveSP) spFree(&w->sp);
  if (w->haveColl) collectFree(&w->coll);
  memset(w, 0, sizeof *w);
}

// «вне вида» для возврата предметов: камера за машиной (7.5 м сзади, 3.2 м выше), видно в конусе ±50° по горизонтали
// до 0.92 дальности 250 м — как respawnSeen в main.c, но без точной пирамиды кадра
static float camX, camZ, camFx, camFz;
static int seenCone(void *u, float x, float y, float z, float r) {
  (void)u; (void)y;
  float dx = x - camX, dz = z - camZ, d = sqrtf(dx * dx + dz * dz), far = 250.f * .92f + r;
  if (d > far) return 0; if (d < r + 1.f) return 1;
  return (dx * camFx + dz * camFz) / d > cosf(50.f * 3.14159265f / 180.f) - r / d;
}
#include <time.h>
// №984 стенд city984: время по фазам кадра bwStep (мкс, только при bwProf = 1; профиль не влияет на состояние мира). 0 шаг машины, 1 мир после машины (Гомер, пропы, стейтпропы), 2 респавн, комнаты, миссия, 3 трафик, 4 Hit & Run, смерть
unsigned long long bwPhaseNs[8]; int bwProf;
static inline unsigned long long bwNowNs(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (unsigned long long)ts.tv_sec * 1000000000ull + (unsigned long long)ts.tv_nsec; }
#define BWP(i, ...) do { if (bwProf) { unsigned long long t0_ = bwNowNs(); __VA_ARGS__; bwPhaseNs[i] += bwNowNs() - t0_; } else { __VA_ARGS__; } } while (0)
void bwStep(BotWorld *w, unsigned buttons, float ax, float ay) {
  Game *G = &w->G; bwTotalFrames++; bwLastDeathZone = -1;
  GamePad p = {buttons, buttons & ~w->prevButtons, ax, ay}, p0 = p; w->prevButtons = buttons;
  if (w->haveMsn) gameMsnFreezePad(&w->msn, &p);   // отсчёт 3-2-1 этапа: игрок стоит (как main.c)
  // △ — одно действие за нажатие (как main.c): разговор/точка миссии рядом забирает его у посадки в машину, потом гэг (В5)
  if ((p.hit & GP_TRIANGLE) && w->haveColl && !(w->haveMsn && gameMsnWantsAction(&w->msn)) && collectWantsAction(&w->coll) >= 0) { G->collAction = 1; p.hit &= ~GP_TRIANGLE; }
#ifdef BW_HAVE_AO
  if ((p.hit & GP_TRIANGLE) && w->haveAo && gameOnFoot(G) && !(w->haveMsn && gameMsnWantsAction(&w->msn)) && aoWantsAction(&w->ao) >= 0) {   // △ у выключателя объекта (Duff, АЭС)
    w->aoAct = 1; p.hit &= ~GP_TRIANGLE; }
#endif
  if ((p.hit & GP_TRIANGLE) && w->haveHomer && w->haveCol && !(w->haveMsn && gameMsnWantsAction(&w->msn))) { gameTriangle(G); p.hit &= ~GP_TRIANGLE; }
  G->collRoom = w->room >= 0 ? collectRoomId(rooms.room[w->room].name) + 1 : 0;   // гэги комнат — только в своей комнате
  if ((p.hit & GP_START) && w->haveCol) { gameReset(G); w->props.havePreviousCar = 0; w->st.resets++; }   // RESET CAR
  if (w->haveCol) {
    CarInput in; gameCarInput(G, &p, &in); gameCarLimits(G, &in); gameExitAssist(G, &in); if (w->haveMsn) gameMsnFreezeCar(&w->msn, &in); BWP(0, gameCarStep(G, &in, BW_DT));
    if (w->car.wallV > w->st.maxWallV) w->st.maxWallV = w->car.wallV;
    BWP(1, gameAfterCar(G, &p, BW_DT));
#ifdef BW_HAVE_AO
    if (w->haveAo) {   // объекты уровня: действие, кадр клипа, живые объёмы COL1 (после spUpdate — поза этого кадра), как main.c
      int ft = gameOnFoot(G);
      aoStep(&w->ao, &w->sp, &col, ft ? w->homer.x : w->car.x, ft ? w->homer.y : w->car.y, ft ? w->homer.z : w->car.z, ft, w->aoAct, BW_DT); w->aoAct = 0; }
#endif
  }
  float fx = sinf(w->car.h), fz = cosf(w->car.h), px = w->car.x, pz = w->car.z;
  if (gameOnFoot(G)) { px = w->homer.x; pz = w->homer.z; fx = sinf(w->homer.camYaw); fz = cosf(w->homer.camYaw); }
  camX = px - fx * 7.5f; camZ = pz - fz * 7.5f; camFx = fx; camFz = fz;
  static int noRespawn = -1; if (noRespawn < 0) noRespawn = getenv("BW_NO_RESPAWN") != 0;   // отрицательный контроль гейта длинного мира
  if (!noRespawn) BWP(2, gameRespawn(G, camX, camZ, BW_DT, seenCone, 0));
  { RoomState rs = {w->room, w->lastDoor, w->doorArmed, w->exitArmed};   // комнаты: дверь -> старт комнаты; выход -> к двери (viewer/rooms.c)
    if (roomsStep(&rooms, &rs, G, NULL, NULL) == ROOM_EV_ENTER) w->st.rooms++;
    w->room = rs.room; w->lastDoor = rs.lastDoor; w->doorArmed = rs.doorArmed; w->exitArmed = rs.exitArmed; }
  if (w->haveMsn) { w->msn.interior = w->room >= 0 ? rooms.room[w->room].name : 0; gameMsnTick(&w->msn, &p0, BW_DT); }   // кнопка действия — нажатие этого кадра
  if (w->haveTraffic && w->room < 0) {   // Т1.2, как main.c: после машины и Гомера, в комнате город стоит; камера — конус за игроком
    int drv = !w->haveHomer || homerDriving(&w->homer);
    float pp[3] = {drv ? w->car.x : w->homer.x, drv ? w->car.y : w->homer.y, drv ? w->car.z : w->homer.z}, e3[3] = {camX, pp[1] + 3.2f, camZ}, f3[3] = {camFx, 0.f, camFz};
    BWP(3, trafficUpdate(&tr, &col, &w->car, drv, pp, e3, f3, BW_DT));
    TrEvent te; while (trafficPopEvent(&tr, &te)) {   // Hit & Run: те же правила, что в EBOOT (game.c gameHnrOffense)
      gameHnrOffense(G, te.type == TR_EV_CAR_WRECK ? HNR_CAR_WRECK : te.type == TR_EV_PED_HIT ? HNR_PED_HIT : te.type == TR_EV_PED_KICK ? HNR_PED_KICK : HNR_CAR_HIT);
      w->st.trEvents++; }
#ifdef GAME_DEATH
    static int noDeath = -1; if (noDeath < 0) { noDeath = getenv("BW_NO_DEATH") != 0;   // =1 — как до 01.10 (отрицательный контроль tests/falls)
      const char *e = getenv("BW_DEATH_OLD"); if (e && *e && *e != '0') gameDeathOld(1);   // правка 349: =1 — как до неё (первый объём локатора, курс прежний)
#ifdef GAME_DEATH_FOOT
      e = getenv("BW_DEATH_FOOT_OLD"); if (e && *e && *e != '0') gameDeathFootOld(1);
#endif
    }   // ДОБИВКА 18 (В86): =1 — пешком курс прежний, камера за спиной
    if (!noDeath) bwLastDeathZone = gameDeath(G);   // Observe the same unmodified call and its real returned zone.
#endif
  }
  BWP(4, gameHnr(G, BW_DT));
  w->st.frames++;
}

#ifdef BW_HAVE_AO   // живые объёмы ANM1 в общей коллизии: копия в снимке (загрузка возвращает и их, не только кадр клипа)
static size_t aoMemSize(const BotWorld *w) {
  size_t n = 0; if (w->haveAo) for (unsigned v = 0; v < w->ao.nvol; v++) n += w->ao.vol[v].kind == 0 ? sizeof(ColBox) : sizeof(ColCyl);
  return n;
}
static void aoPut(const BotWorld *w, unsigned char *b) {
  for (unsigned v = 0; v < w->ao.nvol; v++) { const AoVol *V = &w->ao.vol[v];
    if (V->kind == 0) { memcpy(b, &col.box[V->col], sizeof(ColBox)); b += sizeof(ColBox); } else { memcpy(b, &col.cyl[V->col], sizeof(ColCyl)); b += sizeof(ColCyl); } }
}
static void aoGet(const BotWorld *w, const unsigned char *b) {
  for (unsigned v = 0; v < w->ao.nvol; v++) { const AoVol *V = &w->ao.vol[v];
    if (V->kind == 0) { memcpy((ColBox *)&col.box[V->col], b, sizeof(ColBox)); b += sizeof(ColBox); } else { memcpy((ColCyl *)&col.cyl[V->col], b, sizeof(ColCyl)); b += sizeof(ColCyl); } }
}
#endif
static size_t spMemSize(const StateProps *sp) {   // как calloc в spInit (viewer/stateprops.c)
  size_t jtot = 0; for (unsigned i = 0; i < sp->ninst; i++) jtot += sp->type[sp->inst[i].type].nJoint;
  return (size_t)sp->ninst * sizeof(SpRun) + jtot * 12 * sizeof(float) + (size_t)sp->ntype * 32 * 3 + 16;
}
void bwSaveAlloc(const BotWorld *w, BotSnap *s) {
  memset(s, 0, sizeof *s);
  s->npst = w->haveProps ? w->props.ninst : 0; s->ncoin = w->haveCoins ? w->coins.n : 0; s->nspmem = w->haveSP ? spMemSize(&w->sp) : 0;
  s->pst = malloc(sizeof(PropState) * (s->npst + 1)); s->coin = malloc(sizeof(Coin) * (s->ncoin + 1)); s->spmem = malloc(s->nspmem + 1);
  s->ncollmem = w->haveColl && w->coll.haveSp ? spMemSize(&w->coll.sp) : 0; s->collmem = malloc(s->ncollmem + 1);
  s->ntrmem = w->haveTraffic ? trBlobSize() : 0; s->trmem = malloc(s->ntrmem + 1);
#ifdef BW_HAVE_AO
  s->naomem = aoMemSize(w);
#endif
  s->aomem = malloc(s->naomem + 1);
}
void bwSave(const BotWorld *w, BotSnap *s) {
  s->w = *w;
  if (s->npst) memcpy(s->pst, w->props.st, sizeof(PropState) * s->npst);
  if (s->ncoin) memcpy(s->coin, w->coins.c, sizeof(Coin) * s->ncoin);
  if (s->nspmem) memcpy(s->spmem, w->sp.mem, s->nspmem);
  if (s->ncollmem) memcpy(s->collmem, w->coll.sp.mem, s->ncollmem);
  if (s->ntrmem) trPut(s->trmem);
#ifdef BW_HAVE_AO
  if (s->naomem) aoPut(w, s->aomem);
#endif
}
#ifndef BW_BREAK_LOAD
#define BW_BREAK_LOAD 0   // отрицательный контроль гейта: 1 — не возвращать монеты при загрузке
#endif
void bwLoad(BotWorld *w, const BotSnap *s) {
  *w = s->w;   // указатели на массивы модулей те же: мир тот же, что был сохранён
  if (s->npst) memcpy(w->props.st, s->pst, sizeof(PropState) * s->npst);
  if (s->ncoin && !BW_BREAK_LOAD) memcpy(w->coins.c, s->coin, sizeof(Coin) * s->ncoin);
  if (s->nspmem) memcpy(w->sp.mem, s->spmem, s->nspmem);
  if (s->ncollmem) memcpy(w->coll.sp.mem, s->collmem, s->ncollmem);
  if (s->ntrmem && !BW_BREAK_TRAFFIC_LOAD) trGet(s->trmem);
#ifndef BW_BREAK_AO_LOAD
#define BW_BREAK_AO_LOAD 0   // отрицательный контроль tools/bots/aocheck.c: загрузка не возвращает живые объёмы ANM1
#endif
#ifdef BW_HAVE_AO
  if (s->naomem && !BW_BREAK_AO_LOAD) aoGet(w, s->aomem);
#endif
}
void bwSnapFree(BotSnap *s) { free(s->pst); free(s->coin); free(s->spmem); free(s->collmem); free(s->trmem); free(s->aomem); memset(s, 0, sizeof *s); }

static uint64_t fnv(uint64_t h, const void *p, size_t n) { const unsigned char *b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ULL; } return h; }
uint64_t bwHash(const BotWorld *w) {   // состояние без указателей (адреса разные между процессами и после malloc)
  uint64_t h = 1469598103934665603ULL;
  h = fnv(h, &w->car, sizeof w->car);
  Homer hm = w->homer; hm.m = 0; memset(&hm.cb, 0, sizeof hm.cb); hm.solid = 0; hm.solidUser = 0;
  h = fnv(h, &hm, offsetof(Homer, pal));   // Гомер без матриц рисования
  h = fnv(h, &w->hnrHeat, sizeof(float) * 2); h = fnv(h, &w->room, sizeof(int) * 4); h = fnv(h, w->spSt, sizeof w->spSt); h = fnv(h, &w->st, sizeof w->st);
  if (w->haveProps) h = fnv(h, w->props.st, sizeof(PropState) * w->props.ninst);
  if (w->haveCoins) { h = fnv(h, w->coins.c, sizeof(Coin) * w->coins.n); h = fnv(h, &w->coins.collected, sizeof(int)); h = fnv(h, &w->coins.rng, sizeof(unsigned)); }
  if (w->haveMsn) {   // ход миссий (без указателей в пакет)
    const MsnRuntime *r = &w->msn.rt; int v[6] = {(int)r->completed, r->story, r->cur, r->phase, r->stage, r->obj};
    h = fnv(h, v, sizeof v); h = fnv(h, &r->countdown, sizeof r->countdown); h = fnv(h, &w->msn.places, sizeof(unsigned) * 2);
    int tm[3] = {r->timer_on, r->timer_up, r->timer_last}; h = fnv(h, tm, sizeof tm); h = fnv(h, &r->remaining, sizeof r->remaining);   // таймер
    h = fnv(h, &w->msn.nnpc, sizeof(int)); h = fnv(h, w->msn.npc, sizeof(GameNpc) * (size_t)w->msn.nnpc);   // персонажи миссий (место, маршрут)
    h = fnv(h, r->done, sizeof r->done); h = fnv(h, r->released, sizeof r->released); h = fnv(h, r->relpos, sizeof r->relpos);   // предметы миссии
    h = fnv(h, &w->msn.cars, sizeof w->msn.cars);   // машины миссии с ИИ (структура без указателей)
  }
  if (w->haveColl) {   // В5: карточки, осы, гэги (состояние без указателей) + их SPR1
    h = fnv(h, &w->coll.st, sizeof w->coll.st);
    for (unsigned i = 0; i < w->coll.sp.ninst; i++) { const SpRun *r = &w->coll.sp.run[i]; h = fnv(h, r, offsetof(SpRun, jm)); }
  }
  if (w->haveTraffic) {   // Т1.2: машины, пешеходы (без указателя на модель), стоящие, хвост (таймеры, seed, события)
    h = fnv(h, tr.car, sizeof tr.car);
    for (int i = 0; i < TR_MAXPED; i++) { h = fnv(h, &tr.ped[i], TRPED_HEAD); h = fnv(h, (const unsigned char *)&tr.ped[i].sp + offsetof(SkinPlayer, clip), TRSP_HEAD - offsetof(SkinPlayer, clip)); }
    h = fnv(h, tr.park, sizeof(TrParked) * (size_t)tr.npark); h = fnv(h, (const unsigned char *)&tr + TRTAIL, sizeof(Traffic) - TRTAIL);
  }
#ifdef BW_HAVE_AO
  if (w->haveAo) { h = fnv(h, w->ao.run, sizeof(AoRun) * w->ao.nobj); h = fnv(h, &w->ao.want, sizeof w->ao.want); }
#endif
  if (w->haveSP) {
    for (unsigned i = 0; i < w->sp.ninst; i++) {   // без указателя jm, но с самими матрицами суставов
      const SpRun *r = &w->sp.run[i]; h = fnv(h, r, offsetof(SpRun, jm));
      h = fnv(h, r->jm, sizeof(float) * 12 * w->sp.type[w->sp.inst[i].type].nJoint);
    }
  }
  return h;
}
