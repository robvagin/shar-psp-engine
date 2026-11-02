// Полигон бонус-игры на Маке (правка Роба 420, пункт 4): трасса мини-игры — испытательный полигон для гонщиков-ботов.
// Настоящие модули движка: viewer/car.c (физика), viewer/col.c (COL1 трассы b0N.shr), viewer/msncar.c (водитель ИИ машин миссий —
// тот же, что ведёт соперников в EBOOT), viewer/bonus.c (правила: финиш, чекпойнты, круги, места). Данные — tools/bonus/export.py.
//   race_host b0N.shr b0N.bns msncars-b.shr [--laps N] [--reverse] [--secs S] [--seed K] [--json out.json] [-v]
//              [--break-cp]      красный: машины не проходят чекпойнты (объёмы сдвинуты на 1 км) — круги не засчитываются
//              [--break-steer]   красный: руль ИИ выключен — машины упираются, «застряли» растёт, круги не идут
// Машины: 3 соперника из данных (ходовые bonus/bgT_K.con, K = 1..3) + 4-я — машина игрока под тем же водителем ИИ (ходовые .con
// самой машины) — у оригинала до 4 игроков; машины — персонажей MiniMenu.pag (lisa, bart, homer, marge, apu: *_v), ПРАВИЛО ПОРТА
// (модели соперников в данных не названы; вопрос в knowledge/questions.tsv). Шаг 1/60 с, детерминированно.
// Итог: строка на машину (круги, лучший круг, общее время, удары о стены и машины, возвраты, место) и RESULT: PASS/FAIL
// (все доехали за --secs; ни одна не возвращалась на путь чаще 3 раз за круг; круг не короче 5 с — не «срезает» чекпойнты).
// С 09.10 (часть «подборы, ловушки, огонь, DEATH»): в гонке действуют подборы nitro/wrench, ловушки (объект ANM1 трассы — его живые
// объёмы COL1 двигаются), подброс B04G, свет зоны, DEATH трассы (viewer/bonus.c brPick/brBoost/brTrapStep/brLaunch/brLight/brDeath);
// счётчики — строка EFFECTS. Водитель ИИ жмёт газ всегда (нитро: газ 1 — правило полигона).
//   --probe   детерминированная проверка каждого подбора, объёма DEATH, датчика ловушки, сферы B04G и зоны света трассы (машина
//             ставится в объём, эффект меряется), строки PROBE и RESULT. Красный — сборка с -DBONUS_PICKUPS_OLD=1.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "msncar.h"
#include "bonus.h"
#include "props.h"
#include "stateprops.h"
#include "animobj.h"

int mcDbgRoad(int r, int *checked, int *bad, int *ledge);
#define OVL_DEPTH .75f   // №1147: «пересечение кузовов» — глубина перекрытия в плане больше 0,75 м (мельче — контакт, который столкновение разводит за кадры); одна на крыше другой (car.c STACK_DY 0,5 высоты) — не пересечение
static BonusRace B; static Traffic RT; static int roadsOn;   // №1116: дороги трассы (хвост BRD1 .bns), соперники по ним
static int planHook(void *u, const McCar *c, const float g[3], float (*out)[3], int cap) {
  (void)u; (void)g; float p[3] = {c->car.x, c->car.y, c->car.z}; return brPlan(&B, c->wp, p, out, cap);
}
static ColWorld world; static McGeo geo[64]; static int ngeo;
static Props PR; static StateProps SP; static AnimObjs AO; static int haveAo;   // SPR1/ANM1 трассы: ловушки (pistons, spikes)
static int pchecks, pfails;
#define PCHK(cond, ...) do { pchecks++; if (cond) printf("PROBE ok   " __VA_ARGS__); else { pfails++; printf("PROBE FAIL " __VA_ARGS__); } } while (0)
static float pathHeading(const float p[3]) {   // курс вдоль плотного пути хода у ближней точки
  int best = 0; float bd = 1e30f;
  for (int i = 0; i < B.npath; i++) { const float *q = B.path[i].p; float d = (q[0] - p[0]) * (q[0] - p[0]) + (q[2] - p[2]) * (q[2] - p[2]); if (d < bd) { bd = d; best = i; } }
  if (B.npath < 2) return 0.f;
  const float *a = B.path[best].p, *c = B.path[(best + 1) % B.npath].p;
  return atan2f(c[0] - a[0], c[2] - a[2]);
}
static float liveMaxY(int t) {   // наибольшая высота центра живых объёмов объекта ловушки t (COL1 ящики/цилиндры)
  const AoObj *o = &AO.obj[B.trap[t].obj]; float m = -1e9f;
  for (unsigned v = o->firstVol; v < (unsigned)o->firstVol + o->nVol; v++) {
    const AoVol *V = &AO.vol[v]; float y = V->kind == 0 ? world.box[V->col].c[1] : world.cyl[V->col].c[1];
    if (y > m) m = y;
  }
  return m;
}
// детерминированная проверка эффектов трассы (машина 0 ставится в каждый объём данных)
static void probe(Car *k) {
  const float dt = 1.f / 60.f; Car save = *k; char nm[17]; int nitroN = 0; B.human = 0;   // машина 0 — как игрок (DEATH всегда на точку данных)
  for (unsigned q = 0; q < B.t.npick; q++) {
    const BnsPick *P = &B.t.pk[q]; memcpy(nm, P->name, 12); nm[12] = 0;
    *k = save; B.pickTaken = ~(1u << q);   /* только этот подбор: сферы nitro и wrench у b07 перекрываются */ B.boostT[0] = 0.f; k->x = P->p[0]; k->y = P->p[1] - .5f; k->z = P->p[2];
    if (P->kind == BNS_PK_WRENCH) {
      k->damage = .8f; int r = brPick(&B, 0, k);
      PCHK(r == (int)q && k->damage == 0.f, "wrench %-8s взят=%d урон 0.80 -> %.2f\n", nm, r, k->damage);
      PCHK(!brPickShown(&B, (int)q), "wrench %-8s пропал после подбора\n", nm);
      brPickTick(&B, BR_PICK_RESPAWN_S - .1f); int early = brPickShown(&B, (int)q); brPickTick(&B, .2f);
      PCHK(!early && brPickShown(&B, (int)q), "wrench %-8s вернулся через %.0f с (раньше %d)\n", nm, BR_PICK_RESPAWN_S, early);
    } else {
      int r = brPick(&B, 0, k);
      PCHK(r == (int)q && B.boostT[0] > 0.f, "nitro  %-8s взят=%d нитро %.1f с\n", nm, r, B.boostT[0]);
      nitroN++;
    }
  }
  if (nitroN) {   // разгон нитро — один замер на трассу: старт car1 (прямая у финиша, курс данных), 8 м/с, газ, 0,5 с с нитро и без
    float sp[3], sh; brStart(&B, 0, sp, &sh); *k = save; carPlace(k, &world, sp[0], sp[1], sp[2], sh); carSetSpeed(k, 8.f);
    Car a = *k, c = *k; CarInput in; memset(&in, 0, sizeof in); in.throttle = 1.f; B.boostT[0] = BR_NITRO_S; int on = 0;
    for (int f = 0; f < 30; f++) { carUpdate(&a, &world, &in, dt); on += brBoost(&B, 0, &a, 1.f, dt); carUpdate(&c, &world, &in, dt); }
    float va = sqrtf(a.vel[0] * a.vel[0] + a.vel[2] * a.vel[2]), vc = sqrtf(c.vel[0] * c.vel[0] + c.vel[2] * c.vel[2]);
    PCHK(on > 0 && va - vc > 1.5f, "nitro  разгон у старта за 0,5 с: %.1f м/с с нитро, %.1f без (+%.1f)\n", va, vc, va - vc);
  }
  B.pickTaken = 0; B.boostT[0] = 0.f;
  for (unsigned z = 0; z < world.ndeath; z++) {
    const ColPad *D = &world.death[z]; memcpy(nm, D->name, 16); nm[16] = 0;
    *k = save; k->x = D->c[0]; k->y = D->c[1] - .5f; k->z = D->c[2]; float hd = 0.f; int hasH = colDeathHeading(&world, z, &hd);
    int r = brDeath(&B, 0, k, &world);
    float dx = k->x - D->apex[0], dz = k->z - D->apex[2]; int still = colDeathAtEx(&world, k->x, k->y + .5f, k->z, .5f, 0) >= 0;
    float dh = atan2f(sinf(k->h - hd), cosf(k->h - hd));
    float ph = pathHeading(D->apex); int along = cosf(k->h - ph) > 0.f;   // курс данных или развёрнутый — по ходу гонки
    PCHK(r && dx * dx + dz * dz < .01f && !still && (!hasH || ((fabsf(dh) < .01f || fabsf(fabsf(dh) - 3.14159265f) < .01f) && along)),
         "death  %-8s объём %u -> возврат (%.1f, %.1f, %.1f) курс %.2f (данные %.2f, путь хода %.2f), снова в зоне %d\n", nm, z, k->x, k->y, k->z, k->h, hd, pathHeading(D->apex), still);
  }
  if (B.t.h->ntrapobj) PCHK(B.ntrap == (int)B.t.h->ntrapobj && haveAo, "ловушка: объектов ANM1 %d (ждали %u)\n", B.ntrap, B.t.h->ntrapobj);
  for (unsigned q = 0; q < B.t.nhaz && B.ntrap; q++) {
    const BnsHaz *H = &B.t.hz[q]; if (H->kind != BNS_HZ_TRAP) continue;
    for (int t = 0; t < B.ntrap; t++) { B.trap[t].state = BR_TRAP_IDLE; B.trap[t].f = 0.f; }
    brTrapApply(&B, &AO, &SP, &world); float y0 = liveMaxY(0), out[1][3] = {{1e4f, 0.f, 1e4f}};
    float in1[1][3] = {{H->c[0], H->c[1], H->c[2]}};
    unsigned fired = brTrapStep(&B, (const float (*)[3])in1, 1, dt); int f = 0;
    for (; f < 300 && B.trap[0].state == BR_TRAP_RISE; f++) brTrapStep(&B, (const float (*)[3])out, 1, dt);
    brTrapApply(&B, &AO, &SP, &world); float y1 = liveMaxY(0);
    for (f = 0; f < 600 && B.trap[0].state != BR_TRAP_IDLE; f++) brTrapStep(&B, (const float (*)[3])out, 1, dt);
    brTrapApply(&B, &AO, &SP, &world); float y2 = liveMaxY(0);
    PCHK(fired && y1 - y0 > 3.f && fabsf(y2 - y0) < .05f, "trap   %-8s датчик -> объект выходит: живые объёмы y %.2f -> %.2f -> %.2f (кадр %.0f)\n",
         H->name, y0, y1, y2, B.trap[0].last);
  }
  for (unsigned q = 0; q < B.t.nhaz; q++) {
    const BnsHaz *H = &B.t.hz[q]; if (H->kind != BNS_HZ_LAUNCH) continue;
    *k = save; carPlace(k, &world, H->c[0], H->c[1] + 1.f, H->c[2], k->h); k->x = H->c[0]; k->z = H->c[2]; B.launched = 0;   /* carPlace выталкивает из коллизии у ямы поршня до 4 м — ставим в центр сферы */
    for (int t = 0; t < B.ntrap; t++) { B.trap[t].state = BR_TRAP_IDLE; B.trap[t].f = 0.f; }
    int idle = brLaunch(&B, 0, k);
    for (int t = 0; t < B.ntrap; t++) B.trap[t].state = BR_TRAP_RISE;
    int r = brLaunch(&B, 0, k); float vy = k->vel[1], y0 = k->y; CarInput in; memset(&in, 0, sizeof in);
    for (int f = 0; f < 24; f++) carUpdate(k, &world, &in, dt);
    PCHK(!idle && r && vy >= (float)H->arg && k->y - y0 > 2.f, "launch %-8s машина y %.2f (сфера y %.2f r %.1f) поршни внизу: %d; выходят: vy %.1f (данные %u), за 0,4 с +%.1f м\n",
         H->name, y0, H->c[1], H->h[0], idle, vy, H->arg, k->y - y0);
  }
  for (int t = 0; t < B.ntrap; t++) { B.trap[t].state = BR_TRAP_IDLE; B.trap[t].f = 0.f; }
  for (unsigned q = 0; q < B.t.nhaz; q++) {
    const BnsHaz *H = &B.t.hz[q]; if (H->kind != BNS_HZ_LIGHT) continue;
    float in1[3] = {H->c[0], H->c[1], H->c[2]}, o[3] = {H->c[0] + H->h[0] + 3.f, H->c[1], H->c[2]};
    uint32_t a = brLight(&B, in1), b = brLight(&B, o);
    PCHK(a == H->arg && !b, "light  %-8s в зоне %08X (данные %08X), рядом %08X, множитель ABGR %08X\n", H->name, a, H->arg, b, brLightMul(a));
  }
  *k = save;
}
static unsigned char *readFile(const char *p, unsigned *n) {
  FILE *f = fopen(p, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(64, ((size_t)s + 63) & ~(size_t)63);
  if (!b || fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
  fclose(f); *n = (unsigned)s; return b;
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "usage: race_host b0N.shr b0N.bns msncars-b.shr [--laps N] [--reverse] [--secs S] [--json f] [-v]\n"); return 2; }
  FILE *trace = 0; int ai4 = 0, noCollide = 0, infoOnly = 0, dbgPlan = 0; float dbgP[6]; int dbgRoute = -1, NR = 4, oldRoads = 0, noPath = 0, noKeep = 0, noGoal = 0, oldDriver = 1, noCurve = 0, npcCollision = 0; int laps = BR_LAPS_DEF, rev = 0, verbose = 0, breakCp = 0, breakSteer = 0; float secs = 0.f; const char *json = 0; int probeOnly = 0, noPick = 0, noDeath = 0, noTrap = 0;
  for (int i = 4; i < argc; i++) {
    if (!strcmp(argv[i], "--laps") && i + 1 < argc) laps = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--reverse")) rev = 1;
    else if (!strcmp(argv[i], "--secs") && i + 1 < argc) secs = (float)atof(argv[++i]);
    else if (!strcmp(argv[i], "--json") && i + 1 < argc) json = argv[++i];
    else if (!strcmp(argv[i], "--break-cp")) breakCp = 1;
    else if (!strcmp(argv[i], "--nopath")) noPath = 1;
    else if (!strcmp(argv[i], "--wreck")) noKeep = 1;
    else if (!strcmp(argv[i], "--nogoal")) noGoal = 1;
    else if (!strcmp(argv[i], "--route") && i + 1 < argc) dbgRoute = atoi(argv[++i]);   // служебное: раз в 0,5 с печатать маршрут машины i
    else if (!strcmp(argv[i], "--plan") && i + 5 < argc) { dbgPlan = 1; for (int q = 0; q < 6; q++) dbgP[q] = (float)atof(argv[++i]); }   // служебное: x z h gx gz (y берётся с пола)
    else if (!strcmp(argv[i], "--info")) infoOnly = 1;   // служебное: есть ли у трассы блок дорог и принимает ли его загрузчик (tests/g03_1116)
    else if (!strcmp(argv[i], "--oldroads")) oldRoads = 1;   // №1116, красный: соперники по плотному пути порта, а не по дорогам трассы (BONUS_ROADS_OLD)
    else if (!strcmp(argv[i], "--trackdriver")) oldDriver = 0;   // WIP 09.10: водитель трассы brDrive (по умолчанию — водитель машин миссий viewer/msncar.c, как в EBOOT)
    else if (!strcmp(argv[i], "--msndriver")) oldDriver = 1; // explicit current production driver
    else if (!strcmp(argv[i], "--nocurve")) noCurve = 1;    // красный: водитель трассы без профиля скорости (газ в пол)   // прежнее: водитель считает точки сам (пропуск чекпойнта = лишний круг)   // прежнее правило машин миссий: разбитая встаёт   // красный: прямо к точкам WPR/WPL, как без пути по опоре
    else if (!strcmp(argv[i], "--break-steer")) breakSteer = 1;
    else if (!strcmp(argv[i], "--ai4")) ai4 = 1;   // №1147: четвёртая машина ведётся как соперник (ходовые bgN_1), а не с ходовыми по умолчанию
    else if (!strcmp(argv[i], "--noncollide")) noCollide = 1;   // №1147, красный: соперники сквозь друг друга (как до правки)
    else if (!strcmp(argv[i], "--npc-collide")) npcCollision = 1; // отрицательный контроль изоляции AI↔AI
    else if (!strcmp(argv[i], "-v")) verbose = 1;
    else if (!strcmp(argv[i], "--n") && i + 1 < argc) { NR = atoi(argv[++i]); if (NR < 1) NR = 1; if (NR > 4) NR = 4; }
    else if (!strcmp(argv[i], "--trace") && i + 1 < argc) trace = fopen(argv[++i], "w");
    else if (!strcmp(argv[i], "--probe")) probeOnly = 1;
    else if (!strcmp(argv[i], "--nopick")) noPick = 1;     // служебное: подборы в гонке выключены (разбор, что влияет на ИИ)
    else if (!strcmp(argv[i], "--nodeath")) noDeath = 1;   // служебное: DEATH трассы в гонке выключен
    else if (!strcmp(argv[i], "--notrap")) noTrap = 1;     // служебное: ловушки в гонке выключены
    else if (!strcmp(argv[i], "--noeffects")) noPick = noDeath = noTrap = 1;   // водитель и правила гонки без подборов, DEATH и ловушек (tests/bonus/race.sh)
  }
  unsigned n; unsigned char *lv = readFile(argv[1], &n);
  if (!lv || memcmp(lv, "SHR7", 4)) { fprintf(stderr, "нет %s\n", argv[1]); return 2; }
  unsigned co = *(unsigned *)(lv + 92), po = *(unsigned *)(lv + 96), pv = *(unsigned *)(lv + 88), ce = n;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, lv + co, ce - co)) { fprintf(stderr, "COL1 не читается\n"); return 2; }
  if (po > co && po < n) {   // PRP2 -> SPR1 -> ANM1 (ловушки трассы: живые объёмы COL1), как tests/levelobj/host.c
    unsigned ps = *(unsigned *)(lv + po + 40);
    if (!propsInit(&PR, lv + po, ps) && PR.spr && !spInit(&SP, PR.spr, PR.sprSize)) haveAo = aoInit(&AO, lv + po, ps, &SP, &world) == 0;
  }
  unsigned nb; unsigned char *bn = readFile(argv[2], &nb); BnsTrack T;
  if (!bn || bnsParse(&T, bn, nb)) { fprintf(stderr, "BNS1 не читается: %s\n", argv[2]); return 2; }
  if (breakCp) { BnsCp *c = (BnsCp *)T.cp; for (unsigned i = 0; i < T.h->ncp; i++) c[i].c[1] += 1000.f; }
  unsigned nm; unsigned char *mp = readFile(argv[3], &nm);
  if (mp && nm >= 32 && !memcmp(mp, "MCR1", 4)) { unsigned dir = *(unsigned *)(mp + 16); ngeo = mcGeoFromPack(mp, mp + dir, nm - dir, geo, 64); mcSetGeo(geo, ngeo); }
  int track = (int)T.h->track;
  static const char *cars[4] = {"lisa_v", "bart_v", "marge_v", "homer_v"};   // правило порта: машины персонажей MiniMenu.pag
  brInit(&B, &T, laps, rev, NR, -1);
  if (!oldRoads && !noGoal && !BONUS_ROADS_OLD && !noPath && T.rd && trafficInitMem(&RT, (void *)T.rd, T.rdSize, 0) == 0 && (rev ? T.h->nwpr : T.h->nwpf) >= 2) { roadsOn = 1; mcSprint = 1; }
  else if (!noPath) mcPlanHook = planHook;
  static MsnCars M; mcInit(&M);
  for (int i = 0; i < NR; i++) {
    float p[3], h; brStart(&B, i, p, &h);
    char con[32]; if (i < 3 || ai4) snprintf(con, sizeof con, "bonus/bg%d_%d", track, i < 3 ? i + 1 : 1); else con[0] = 0;   // --ai4: четвёртая машина (стенд вместо игрока) — на ходовых соперника №1
    McCar *c = mcSpawn(&M, &world, roadsOn ? &RT : NULL, cars[i], p, h, 1, "race", (i < 3 || ai4) ? con : NULL, "bot");
    if (!c) { fprintf(stderr, "mcSpawn %s\n", cars[i]); return 2; }
    B.grow[i] = c->car.p.halfW;   // кузов входит в объём (не только центр)
    if (verbose) printf("car %d %s con=%s conOk=%d geo=%d top=%.0f km/h at %.1f %.1f %.1f h %.2f\n", i, cars[i], con, c->conOk, c->geoOk,
                        c->car.p.topSpeed * 3.6f, c->car.x, c->car.y, c->car.z, c->car.h);
  }
  if (roadsOn) { const BnsWp *w_ = rev ? T.wpr : T.wpf; int nw_ = (int)(rev ? T.h->nwpr : T.h->nwpf); for (int k = 0; k < nw_; k++) mcWaypoint(&M, k, w_[k].p, BR_LAPS_MAX + 1); }
  else for (int k = 0; k < B.nwp; k++) mcWaypoint(&M, k, B.wp[k], BR_LAPS_MAX + 1);
  for (int i = 0; i < NR; i++) brDriverInit(&B, i, &M.c[i].car.p, &world);   // круги считает brUpdate; водитель — до конца
  if (infoOnly) {
    Traffic X; int rc = T.rd ? trafficInitMem(&X, (void *)T.rd, T.rdSize, 0) : -99;
    printf("ROADS track=b%02d bytes=%u load=%d junc=%u road=%u wpf=%u wpr=%u\nRESULT: %s\n", track, T.rdSize, rc, rc ? 0u : X.hd->nJunc, rc ? 0u : X.hd->nRoad, T.h->nwpf, T.h->nwpr, rc ? "FAIL" : "PASS");
    return rc ? 1 : 0;
  }
  if (dbgPlan && roadsOn) {
    float out[32][3]; int off = 0; float g[3] = {dbgP[3], 5.f, dbgP[4]};
    for (int pass = 0; pass < 2; pass++) {
      int n = mcPlan(&RT, dbgP[0], 5.f, dbgP[1], dbgP[2], g, 1, out, 32, &off);
      printf("plan pass %d: n=%d off=%d:", pass, n, off); for (int q = 0; q < n; q++) printf(" (%.0f,%.0f)", out[q][0], out[q][2]); printf("\n");
      for (unsigned r = 0; r < RT.hd->nRoad; r++) { int ck, bd, lg; mcDbgRoad((int)r, &ck, &bd, &lg); printf("  road %u checked %d bad %d ledge %d\n", r, ck, bd, lg); }
    }
    return 0;
  }
  if (haveAo) brTrapBind(&B, &AO);
  if (probeOnly) {
    printf("TRACK b%02d %s probe: подборов %u, зон %u, DEATH %u, объектов ловушек %d (ANM1 %d)\n", track, rev ? "reverse" : "forward",
           B.t.npick, B.t.nhaz, world.ndeath, B.ntrap, haveAo);
    probe(&M.c[0].car);
    printf("PROBE checks=%d fails=%d\nRESULT: %s\n", pchecks, pfails, pfails || !pchecks ? "FAIL" : "PASS");
    return pfails || !pchecks ? 1 : 0;
  }
  int picks[4] = {0}, deaths[4] = {0}, launches[4] = {0}, lights[4] = {0}, inLight[4] = {0};
  if (secs <= 0.f) secs = 60.f + 45.f * B.laps;   // предел прогона: отсчёт + 45 с на круг (правило полигона)
  const float dt = 1.f / 60.f;
  int ovlN[4][4] = {{0}}, ovlNow[4][4] = {{0}}, ovlTot = 0; float ovlMax = 0.f;   // №1147: пересечения кузовов пар (глубина больше OVL_DEPTH)
  int resets[4] = {0}, wallHits[4] = {0}, carHits[4] = {0}; float wallPrev[4] = {0}, carPrev[4] = {0}, maxV[4] = {0};
  float lapT[4][BR_LAPS_MAX + 1]; memset(lapT, 0, sizeof lapT); float lapStart[4] = {0};
  int frames = (int)(secs / dt), f;
  for (f = 0; f < frames && B.state != BR_DONE; f++) {
    McEnv e; memset(&e, 0, sizeof e); e.w = &world; e.noNpcCollide = AIAI1147_OLD ? !npcCollision : noCollide; e.hold = B.state == BR_COUNT; if (roadsOn) e.tr = &RT;
    if (breakSteer) for (int i = 0; i < M.n; i++) M.c[i].car.p.maxWheelTurn = 0.f;
    for (int i = 0; i < NR; i++) {   // цель водителя — следующий чекпойнт по правилу гонки; доехавшая — стоит
      McCar *c = &M.c[i];
      if (B.r[i].finished) { c->mode = MC_PARKED; continue; }
      int g = brGoal(&B, i); if (!roadsOn && !noGoal && c->wp != g) { c->wp = g; c->nroute = 0; }
    }
    if (oldDriver) mcUpdate(&M, &e, dt, 0, 0);
    else {   // водитель трассы (brDrive): ввод -> физика каждой машины, потом удары между ними
      Car *cs[4];
      for (int i = 0; i < NR; i++) { McCar *c = &M.c[i]; Car *k = &c->car; cs[i] = k;
        CarInput in; if (B.r[i].finished || B.state != BR_RACE) { memset(&in, 0, sizeof in); in.handbrake = 1.f; in.brake = fabsf(k->v) > .5f ? 1.f : 0.f; }
        else if (noPath) { float g[3]; memcpy(g, B.wp[brGoal(&B, i) % B.nwp], 12); float er = atan2f(g[0] - k->x, g[2] - k->z) - k->h;
          while (er > 3.1416f) er -= 6.2832f; while (er < -3.1416f) er += 6.2832f; memset(&in, 0, sizeof in); in.steer = fmaxf(-1.f, fminf(1.f, -er * 2.2f)); in.throttle = 1.f; }
        else brDrive(&B, i, k, dt, &in);
        if (noCurve) { in.brake = 0.f; if (in.throttle > 0.f || B.state == BR_RACE) in.throttle = 1.f; }
        float dmg0 = k->damage; carUpdate(k, &world, &in, dt); k->damage = dmg0;
        c->wheelAng += k->v / fmaxf(.2f, k->p.wheelRad) * dt; }
      if (npcCollision) brCollide(cs, NR, &world);
    }
    { const Car *pc_[4]; for (int i = 0; i < NR; i++) pc_[i] = &M.c[i].car; brSetPeers(&B, pc_, NR); }   // №1147
    for (int i = 0; i < NR; i++) for (int j = i + 1; j < NR; j++) {   // пересечение кузовов: SAT по осям обоих, глубина по меньшему перекрытию
      Car *a = &M.c[i].car, *b = &M.c[j].car; CarBox A, Bx; carBox(a, &A); carBox(b, &Bx);
      float dx = Bx.x - A.x, dz = Bx.z - A.z, depth = 1e9f;
      if (fabsf(a->y - b->y) < .5f * (a->y > b->y ? b->p.bodyHi : a->p.bodyHi) && dx * dx + dz * dz < 100.f) {
        const float ax_[4][2] = {{A.fx, A.fz}, {-A.fz, A.fx}, {Bx.fx, Bx.fz}, {-Bx.fz, Bx.fx}};
        for (int q = 0; q < 4; q++) {
          float u0 = ax_[q][0], u1 = ax_[q][1];
          float ra = A.hl * fabsf(A.fx * u0 + A.fz * u1) + A.hw * fabsf(-A.fz * u0 + A.fx * u1);
          float rb = Bx.hl * fabsf(Bx.fx * u0 + Bx.fz * u1) + Bx.hw * fabsf(-Bx.fz * u0 + Bx.fx * u1);
          float ov = ra + rb - fabsf(dx * u0 + dz * u1); if (ov < depth) depth = ov; }
      }
      int now = depth < 1e8f && depth > OVL_DEPTH;
      if (now && !ovlNow[i][j]) { ovlN[i][j]++; ovlTot++; if (verbose) printf("t=%.1f пересечение %s-%s глубина %.2f\n", f * dt, cars[i], cars[j], depth); }
      ovlNow[i][j] = now; if (now && depth > ovlMax) ovlMax = depth;
    }
    for (int i = 0; i < NR; i++) {   // подборы, нитро, подброс, свет, DEATH (viewer/bonus.c — тот же код, что EBOOT)
      Car *k = &M.c[i].car; float lp[3] = {k->x, k->y + .5f, k->z};
      if (B.state == BR_RACE && !B.r[i].finished) {
        int q = noPick ? -1 : brPick(&B, i, k);
        if (q >= 0) { picks[i]++; if (verbose) printf("t=%.1f %s подбор %.12s\n", f * dt, cars[i], B.t.pk[q].name); }
        brBoost(&B, i, k, oldDriver ? M.c[i].lastIn.throttle : 1.f, dt);   // газ водителя этого кадра (водитель трассы — 1)
        if (brLaunch(&B, i, k)) { launches[i]++; if (verbose) printf("t=%.1f %s подброс поршнем\n", f * dt, cars[i]); }
      }
      float dx0_ = k->x, dy0_ = k->y, dz0_ = k->z, dv0_ = k->v; if (!noDeath && brDeath(&B, i, k, &world)) { deaths[i]++; M.c[i].nroute = 0; if (verbose) printf("t=%.1f %s DEATH -> возврат (упала у %.1f %.1f %.1f, v %.1f)\n", f * dt, cars[i], dx0_, dy0_, dz0_, dv0_); }
      int L = brLight(&B, lp) != 0; if (L && !inLight[i]) { lights[i]++; B.nLight++; } inLight[i] = L;
    }
    brPickTick(&B, dt);
    float pos[4][3];
    for (int i = 0; i < NR; i++) {
      Car *k = &M.c[i].car; if (!noKeep) brKeepAlive(k); pos[i][0] = k->x; pos[i][1] = k->y + .5f; pos[i][2] = k->z;
      float v = sqrtf(k->vel[0] * k->vel[0] + k->vel[2] * k->vel[2]); if (v > maxV[i]) maxV[i] = v;
      if (k->hardV > 3.f && wallPrev[i] <= 3.f) wallHits[i]++; wallPrev[i] = k->hardV;
      if (k->carHitV > 1.f && carPrev[i] <= 1.f) carHits[i]++; carPrev[i] = k->carHitV;
      if (brStuck(&B, i, pos[i], dt)) {   // правило порта: на последнюю пройденную точку пути, носом к следующей
        McCar *c = &M.c[i]; float q[3], qh;
        if (!brRespawn(&B, i, pos[i], q, &qh)) {   // пути нет — на последнюю пройденную остановку
          int w = c->wp > 0 ? c->wp - 1 : 0, nx = c->wp; const float *a = M.wp[w % M.nwp], *bb = M.wp[nx % M.nwp];
          memcpy(q, a, 12); qh = atan2f(bb[0] - a[0], bb[2] - a[2]); }
        carPlace(k, &world, q[0], q[1] + .5f, q[2], qh);
        c->nroute = 0; resets[i]++;
        if (verbose) printf("t=%.1f %s возврат на путь у %.1f %.1f (к остановке %d), застряла у %.1f %.1f %.1f\n", f * dt, cars[i], q[0], q[2], c->wp % M.nwp, pos[i][0], pos[i][1], pos[i][2]);
      }
    }
    if (getenv("WPDBG")) { static int lw[4] = {-1, -1, -1, -1}; for (int i = 0; i < NR; i++) if (M.c[i].wp != lw[i]) { lw[i] = M.c[i].wp; const BnsCp *cp = &B.t.cp[B.order[B.r[i].next]]; const Car *k = &M.c[i].car;
      printf("t=%.1f %s wp->%d at %.1f %.1f %.1f next=%d cp(%s) %.1f %.1f %.1f inside=%d lap=%d\n", f * dt, cars[i], M.c[i].wp, k->x, k->y, k->z, B.r[i].next, cp->name, cp->c[0], cp->c[1], cp->c[2], bnsInside(cp, (float[3]){k->x, k->y + .5f, k->z}, B.grow[i]), B.r[i].lap); } }
    if (dbgRoute >= 0 && dbgRoute < NR && f % 30 == 0) { McCar *c = &M.c[dbgRoute]; printf("t=%.1f car %d at %.1f %.1f %.1f v %.1f h %.2f wp %d nroute %d seg %d off %d turning %d back %.1f route:", f * dt, dbgRoute, c->car.x, c->car.y, c->car.z, c->car.v, c->car.h, c->wp, c->nroute, c->seg, c->offroad, c->turning, c->backT);
      for (int q = 0; q < c->nroute && q < 14; q++) printf(" (%.0f,%.0f)", c->route[q][0], c->route[q][2]); printf("\n"); }
    if (trace && f % 15 == 0) for (int i = 0; i < NR; i++) fprintf(trace, "%.2f\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t%d\t%d\n", f * dt, i,
                                                                   M.c[i].car.x, M.c[i].car.y, M.c[i].car.z, M.c[i].car.v, M.c[i].wp, B.r[i].next),
                                                                   fprintf(trace, "#\t%d\t%d\t%.2f\t%.2f\t%.3f\n", i, B.drv[i].idx, B.drv[i].vt, B.drv[i].backT, M.c[i].car.h);
    if (!noTrap && brTrapStep(&B, (const float (*)[3])pos, NR, dt) && verbose) printf("t=%.1f ловушка: объект выходит\n", f * dt);
    if (haveAo) brTrapApply(&B, &AO, &SP, &world);
    unsigned lm = brUpdate(&B, (const float (*)[3])pos, dt);
    for (int i = 0; i < NR; i++) if (lm & (1u << i)) {
      int L = B.r[i].lap; if (L <= BR_LAPS_MAX) lapT[i][L] = B.time - lapStart[i]; lapStart[i] = B.time;
      if (verbose) printf("t=%.1f %s круг %d за %.2f с, место %d\n", B.time, cars[i], L, lapT[i][L], B.r[i].place);
    }
  }
  int fails = 0; char why[512] = "";
  printf("TRACK b%02d %s laps=%d wp=%d cp=%d secs=%.1f frames=%d\n", track, rev ? "reverse" : "forward", B.laps, B.nwp, B.ncp, f * dt, f);
  if (getenv("WPDBG") && roadsOn) for (unsigned r = 0; r < RT.hd->nRoad; r++) { int ck, bd, lg; mcDbgRoad((int)r, &ck, &bd, &lg); printf("ROAD %u checked %d bad %d ledge %d\n", r, ck, bd, lg); }
  printf("ПЕРЕСЕЧЕНИЙ КУЗОВОВ %d (макс глубина %.2f м)\n", ovlTot, ovlMax);
  if (!AIAI1147_OLD && !noCollide && ovlTot) { fails++; snprintf(why + strlen(why), sizeof why - strlen(why), "пересечений кузовов %d; ", ovlTot); }
  FILE *J = json ? fopen(json, "w") : 0;
  if (J) fprintf(J, "{\"track\":%d,\"reverse\":%d,\"laps\":%d,\"racers\":[", track, rev, B.laps);
  for (int i = 0; i < NR; i++) {
    BrRacer *r = &B.r[i]; McCar *c = &M.c[i];
    printf("  %-8s %-14s круги %d/%d лучший %.2f общее %.2f %s место %d | стены %d машины %d возвраты %d (свои %u) откаты %d макс %.0f км/ч\n",
           cars[i], c->con[0] ? c->con : "(своя .con)", r->lap, B.laps, r->bestLap, r->finished ? r->finishT : r->totalT,
           r->finished ? "доехал" : "НЕ ДОЕХАЛ", r->place, wallHits[i], carHits[i], resets[i], c->resets, c->car.reverts, maxV[i] * 3.6f);
    if (J) { fprintf(J, "%s{\"car\":\"%s\",\"con\":\"%s\",\"laps\":%d,\"finished\":%d,\"best\":%.3f,\"total\":%.3f,\"place\":%d,\"walls\":%d,\"cars\":%d,\"resets\":%d,\"vmax\":%.1f,\"lap_times\":[",
                     i ? "," : "", cars[i], c->con, r->lap, r->finished, r->bestLap, r->finished ? r->finishT : r->totalT, r->place, wallHits[i], carHits[i], resets[i], maxV[i] * 3.6f);
             for (int L = 1; L <= r->lap && L <= BR_LAPS_MAX; L++) fprintf(J, "%s%.3f", L > 1 ? "," : "", lapT[i][L]); fprintf(J, "]}"); }
    printf("           подборы %d гибели %d подбросы %d свет %d\n", picks[i], deaths[i], launches[i], lights[i]);
    if (!r->finished) { fails++; snprintf(why + strlen(why), sizeof why - strlen(why), "%s не доехал; ", cars[i]); }
    if (resets[i] > 3 * B.laps) { fails++; snprintf(why + strlen(why), sizeof why - strlen(why), "%s возвратов %d > 3 на круг; ", cars[i], resets[i]); }
    if (r->finished && r->bestLap < 5.f) { fails++; snprintf(why + strlen(why), sizeof why - strlen(why), "%s круг %.1f с < 5 с; ", cars[i], r->bestLap); }
  }
  if (J) { fprintf(J, "],\"effects\":{\"nitro\":%u,\"wrench\":%u,\"death\":%u,\"trap\":%u,\"launch\":%u,\"light\":%u}}\n",
                   B.nNitro, B.nWrench, B.nDeath, B.nTrap, B.nLaunch, B.nLight); fclose(J); }
  printf("EFFECTS nitro=%u wrench=%u death=%u trap=%u launch=%u light=%u traps=%d\n", B.nNitro, B.nWrench, B.nDeath, B.nTrap, B.nLaunch, B.nLight, B.ntrap);
  if (trace) fclose(trace);
  printf("RESULT: %s%s\n", fails ? "FAIL " : "PASS", why);
  return fails ? 1 : 0;
}
