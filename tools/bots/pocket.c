// №1248 (Кольцо-96): «карман» L7 G5-2 (−36,5; −0,15; −297,2): что в коллизии вокруг и как машина въезжает/выезжает.
//   pocket LEVEL HOMER dump [x z R]          — высоты земли по сетке 1 м (над точкой) и все примитивы коллизии в радиусе
//   pocket LEVEL HOMER drive [V] [-v]        — заезд с 4 (и 8) сторон на скорости V м/с через точку кармана: мин. скорость, что задето
//   pocket LEVEL HOMER escape                — машина ставится в карман: выезжает ли за 20 с газом вперёд/назад/с рулём (4 курса)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "placecode.h"
// стенд меряет геометрию и физику езды, не урон: с Опора-93 (DMGMERCY, 2ec52501) машина от ударов о стены взрывается и остов не едет — урон гасится каждый кадр (иначе «бот не выехал» = «машина разбита»)
#define bwStep(w, b, st, th) (void)((w)->car.damage = 0.f, (w)->car.destroyed = 0, bwStep(w, b, st, th))
static BotWorld W;
static float PX = -36.5f, PZ = -297.2f;
static float ground(float x, float z, float yTop, int *mat) { float gy = -1e9f; int m = 0; if (!colGroundMat(bwCol(), x, z, yTop, -1e6f, &gy, 0, &m)) gy = -1e9f; if (mat) *mat = m; return gy; }
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], 0)) { fprintf(stderr, "usage: pocket LEVEL HOMER dump|drive|escape ...\n"); return 2; }
  memset(&W, 0, sizeof W); if (bwNew(&W)) return 2; ColWorld *cw = (ColWorld *)bwCol(); const char *mode = argv[3];
  if (!strcmp(mode, "dump")) {
    float R = 14.f; if (argc > 5) { PX = (float)atof(argv[4]); PZ = (float)atof(argv[5]); } if (argc > 6) R = (float)atof(argv[6]);
    float g0 = ground(PX, PZ, PX * 0 + 30.f, 0); printf("точка (%.1f %.1f): земля %.2f\n", PX, PZ, g0);
    printf("высоты земли над точкой (первая поверхность сверху от y=%.0f), см относительно центра; строки z вверх, столбцы x вправо, шаг 1 м; '.' нет земли\n", g0 + 30.f);
    for (float z = PZ - R; z <= PZ + R + .01f; z += 1.f) { printf("z%7.1f ", z); for (float x = PX - R; x <= PX + R + .01f; x += 1.f) { float g = ground(x, z, g0 + 30.f, 0); if (g < -1e8f) printf("  .  "); else printf("%4d ", (int)floorf((g - g0) * 100.f + .5f)); } printf("\n"); }
    printf("тип примитивов в радиусе %.0f м:\n", R);
    for (uint32_t i = 0; i < cw->nbox; i++) { const ColBox *b = &cw->box[i]; if (fabsf(b->c[0] - PX) < R + 6 && fabsf(b->c[2] - PZ) < R + 6) printf("BOX %u c(%.2f %.2f %.2f) h(%.2f %.2f %.2f) ay(%.2f %.2f %.2f)\n", i, b->c[0], b->c[1], b->c[2], b->h[0], b->h[1], b->h[2], b->ay[0], b->ay[1], b->ay[2]); }
    for (uint32_t i = 0; i < cw->ncyl; i++) { const ColCyl *c = &cw->cyl[i]; if (fabsf(c->c[0] - PX) < R + 6 && fabsf(c->c[2] - PZ) < R + 6) printf("CYL %u c(%.2f %.2f %.2f) axis(%.2f %.2f %.2f) r %.2f half %.2f flat %u\n", i, c->c[0], c->c[1], c->c[2], c->axis[0], c->axis[1], c->axis[2], c->r, c->halfLen, c->flat); }
    for (uint32_t i = 0; i < cw->nseg; i++) { const ColSeg *s = &cw->seg[i]; if (fabsf(s->a[0] - PX) < R + 6 && fabsf(s->a[1] - PZ) < R + 6) printf("SEG %u a(%.2f %.2f) b(%.2f %.2f) n(%.2f %.2f) y %.2f..%.2f\n", i, s->a[0], s->a[1], s->b[0], s->b[1], s->n[0], s->n[1], s->y0, s->y1); }
    int nt = 0, nsteep = 0; for (uint32_t i = 0; i < cw->ntri; i++) { const ColTri *t = &cw->tri[i]; float cx = (t->a[0] + t->b[0] + t->c[0]) / 3, cz = (t->a[2] + t->b[2] + t->c[2]) / 3; if (fabsf(cx - PX) < R && fabsf(cz - PZ) < R) { nt++; if (t->n[1] < COL_WALK_NY) { nsteep++; if (nsteep <= 40) printf("TRI-стена %u a(%.1f %.1f %.1f) b(%.1f %.1f %.1f) c(%.1f %.1f %.1f) n(%.2f %.2f %.2f)\n", i, t->a[0], t->a[1], t->a[2], t->b[0], t->b[1], t->b[2], t->c[0], t->c[1], t->c[2], t->n[0], t->n[1], t->n[2]); } } }
    printf("треугольников в радиусе %d, из них непроходимых (стенки) %d\n", nt, nsteep); return 0; }
  if (!strcmp(mode, "reach")) {   // карта проходимости кузова: клетки 0,5 м, 8 курсов, вода/стены по colProbeCar; заливка от центра кармана (шаг по высоте не больше 0,4 м)
    float R = 40.f, cs = .5f; int N = (int)(2 * R / cs) + 1; if (argc > 4) PX = (float)atof(argv[4]); if (argc > 5) PZ = (float)atof(argv[5]); if (argc > 6) R = (float)atof(argv[6]); N = (int)(2 * R / cs) + 1;
    float *gy = malloc(sizeof(float) * N * N); unsigned char *blk = malloc((size_t)N * N * 8), *seen = calloc((size_t)N * N, 1); int *q = malloc(sizeof(int) * N * N);
    float g0 = ground(PX, PZ, 1.5f, 0);
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) { float x = PX - R + i * cs, z = PZ - R + j * cs; gy[j * N + i] = ground(x, z, g0 + 1.2f, 0);
      for (int hd = 0; hd < 8; hd++) { blk[(j * N + i) * 8 + hd] = 1; if (gy[j * N + i] < -1e8f) continue;
        W.car.vel[0] = W.car.vel[1] = W.car.vel[2] = 0; carPlace(&W.car, cw, x, gy[j * N + i] + .3f, z, hd * 0.785398f); CarBox cb; carBox(&W.car, &cb); colProbeCar(cw, &cb);
        blk[(j * N + i) * 8 + hd] = cb.maxDepth > .03f || fabsf(W.car.y - gy[j * N + i]) > .6f; } }
    int ci = (int)(R / cs + .5f), cj = ci, qh = 0, qt = 0; q[qt++] = cj * N + ci; seen[cj * N + ci] = 1; int edge = 0, cnt = 0; float amin = 1e9f;
    while (qh < qt) { int c = q[qh++], i = c % N, j = c / N; cnt++; if (i < 2 || j < 2 || i > N - 3 || j > N - 3) edge++;
      for (int dj = -1; dj <= 1; dj++) for (int di = -1; di <= 1; di++) { if (!di && !dj) continue; int ni = i + di, nj = j + dj; if (ni < 0 || nj < 0 || ni >= N || nj >= N || seen[nj * N + ni]) continue;
        float h = atan2f((float)di, (float)dj); int hd = ((int)floorf(h / 0.785398f + 8.5f)) & 7; int ok = !blk[(nj * N + ni) * 8 + hd] && fabsf(gy[nj * N + ni] - gy[j * N + i]) < .4f; if (!ok) continue; seen[nj * N + ni] = 1; q[qt++] = nj * N + ni; } }
    printf("центр (%.1f %.1f) земля %.2f; достижимых клеток %d (%.0f м2), у края окна ±%.0f м: %d -> %s\n", PX, PZ, g0, cnt, cnt * cs * cs, R, edge, edge ? "ВЫХОД ЕСТЬ" : "ЗАМКНУТО");
    int cstep = getenv("PK_STEP") ? atoi(getenv("PK_STEP")) : 1; for (int j = N - 1; j >= 0; j -= 2 * cstep) { printf("z%7.1f ", PZ - R + j * cs); for (int i = 0; i < N; i += cstep) { char ch = seen[j * N + i] ? 'o' : gy[j * N + i] < -1e8f ? ' ' : '#'; if (i == ci && j == cj) ch = '@'; putchar(ch); } putchar('\n'); }
    (void)amin; return 0; }
  if (!strcmp(mode, "run")) {   // заезд с N сторон на скорости V (м/с) через точку, газ R, потом разворот и выезд назад; печать: мин. скорость в радиусе 12 м и что задето
    float V = argc > 4 ? (float)atof(argv[4]) : 15.f; int verbose = argc > 5 && !strcmp(argv[5], "-v"); if (getenv("PK_X")) { PX = (float)atof(getenv("PK_X")); PZ = (float)atof(getenv("PK_Z")); }
    int nbad = 0;
    for (int d = (getenv("PK_D") ? atoi(getenv("PK_D")) : 0); d < (getenv("PK_D") ? atoi(getenv("PK_D")) + 1 : 8); d++) {
      float hd = d * 0.785398f, fx = sinf(hd), fz = cosf(hd), sx = PX - fx * (getenv("PK_DIST") ? (float)atof(getenv("PK_DIST")) : 45.f), sz = PZ - fz * (getenv("PK_DIST") ? (float)atof(getenv("PK_DIST")) : 45.f); float g = ground(sx, sz, 3.f, 0);
      if (g < -1e8f) { printf("курс %3d°: старт (%.1f %.1f) без земли — пропуск\n", d * 45, sx, sz); continue; }
      bwPlace(&W, sx, g + .3f, sz, hd); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
      W.car.vel[0] = fx * V; W.car.vel[2] = fz * V; W.car.v = V; float vmin = 1e9f, xmin = 0, zmin = 0; unsigned lastIdx = ~0u; char hits[200] = ""; int nh = 0; int fmin = 0;
      for (int f = 0; f < 60 * 8; f++) {
        bwStep(&W, getenv("PK_COAST") ? 0u : GP_R, 0.f, 0.f); float vh = hypotf(W.car.vel[0], W.car.vel[2]), dx = W.car.x - PX, dz = W.car.z - PZ;
        if (W.car.wallV > 1.5f && (W.car.wallIdx != lastIdx) && nh < 6) { char t[60]; snprintf(t, sizeof t, " [f%d тип%d #%u мат%d v%.1f]", f, W.car.wallType, W.car.wallIdx, W.car.wallMat, W.car.wallV); strcat(hits, t); nh++; lastIdx = W.car.wallIdx; }
        if (hypotf(dx, dz) < 12.f && vh < vmin) { vmin = vh; xmin = W.car.x; zmin = W.car.z; fmin = f; }
        if (getenv("PK_FINE") && f >= atoi(getenv("PK_FINE")) && f < atoi(getenv("PK_FINE")) + 30) { CarBox cb; carBox(&W.car, &cb); colProbeCar(cw, &cb); printf("   f%3d x%.2f y%.3f vy%.2f v %.2f gr %d wh %d hitSp %.2f hardV %.2f pen %.3f rev %d crashes %d contacts %d t%d | probe n%d lo %.2f hi %.2f floorY %.3f gx %.3f", f, W.car.x, W.car.y, W.car.vel[1], vh, W.car.grounded, W.car.wheels, W.car.hitSpeed, W.car.hardV, W.car.pen, W.car.reverts, W.car.crashes, W.car.contacts, W.car.contacts ? W.car.ctype[0] : -1, cb.n, cb.lo, cb.hi, cb.y, cb.gx); for (int q = 0; q < cb.n && q < 3; q++) printf(" [t%d #%u d%.2f n(%.2f %.2f)]", cb.type[q], cb.index[q], cb.depth[q], cb.nx[q], cb.nz[q]); printf(" foliCover %.2f foliV %.2f folHits %d bushT %.2f\n", W.car.foliCover, W.car.foliV, colFoliageHits(), W.car.bushPassT); }
        if (verbose && f % 10 == 0) printf("   f%3d (%.1f %.2f %.1f) v %.1f wall %.1f\n", f, W.car.x, W.car.y, W.car.z, vh, W.car.wallV);
      }
      float dist = hypotf(W.car.x - PX, W.car.z - PZ); int stuck = vmin < .4f * V || dist < 8.f;
      printf("курс %3d° V0 %.0f: мин. скорость у точки %.1f (кадр %d, в (%.1f %.1f)), через 8 с в (%.1f %.1f) дистанция %.0f м v %.1f%s%s\n", d * 45, V, vmin, fmin, xmin, zmin, W.car.x, W.car.z, dist, hypotf(W.car.vel[0], W.car.vel[2]), stuck ? "  <== ТОРМОЗИТ" : "", hits);
      nbad += stuck; }
    printf("ИТОГ: тормозит %d из 8\n", nbad); return nbad != 0; }
  if (!strcmp(mode, "escape")) {   // машина стоит в кармане (сетка стартов x у y); водитель-бот едет к выходу (TX, TZ); упёрся (v<1 дольше 0,5 с) — назад с рулём в другую сторону 1,2 с, потом вперёд; успех: дошёл до x > XOUT за 40 с
    float TX = getenv("PK_TX") ? (float)atof(getenv("PK_TX")) : -10.f, TZ = getenv("PK_TZ") ? (float)atof(getenv("PK_TZ")) : -297.f, XOUT = getenv("PK_XOUT") ? (float)atof(getenv("PK_XOUT")) : -17.f; int ok = 0, n = 0, verbose = argc > 4 && !strcmp(argv[4], "-v");
    for (float sz = -306.f; sz <= -291.f; sz += 5.f) for (float sx = -42.f; sx <= -24.f; sx += 6.f) for (int hdk = 0; hdk < 4; hdk++) {
      float hd = hdk * 1.570796f, g = ground(sx, sz, 3.f, 0); if (g < -1e8f) continue; bwPlace(&W, sx, g + .3f, sz, hd); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
      int stall = 0, rev = 0; float revSteer = 0; int done = -1; n++;
      for (int f = 0; f < 60 * 40 && done < 0; f++) {
        float vh = hypotf(W.car.vel[0], W.car.vel[2]); float want = atan2f(TX - W.car.x, TZ - W.car.z), fh = atan2f(sinf(W.car.h), cosf(W.car.h)); float e = want - fh; while (e > 3.14159f) e -= 6.28318f; while (e < -3.14159f) e += 6.28318f;
        float st = e > 0 ? 1.f : -1.f; if (fabsf(e) < .1f) st = e * 10.f; unsigned b = GP_R; float steer = -st;   // знак руля подобран ниже по результату
        if (rev > 0) { b = GP_L; steer = revSteer; rev--; if (!rev) stall = 0; }
        else { if (vh < 1.f) stall++; else stall = 0; if (stall > 30) { rev = 72; revSteer = -steer; stall = 0; } }
        if (getenv("PK_FLIP")) steer = -steer;
        bwStep(&W, b, steer, 0.f);
        if (W.car.x > XOUT) done = f; }
      if (done >= 0) ok++; if (verbose || done < 0) printf("старт (%.0f %.0f) курс %3d°: %s, конец (%.1f %.1f) v %.1f\n", sx, sz, hdk * 90, done >= 0 ? "выехала" : "НЕ ВЫЕХАЛА", W.car.x, W.car.z, hypotf(W.car.vel[0], W.car.vel[2])); }
    printf("ИТОГ: выехала %d из %d стартов\n", ok, n); return ok != n; }
  if (!strcmp(mode, "prof")) {   // профиль земли вдоль линии (x0 z0 x1 z1), шаг 1 м: первая поверхность ниже yTop (по умолчанию 3 м над 0)
    float x0 = (float)atof(argv[4]), z0 = (float)atof(argv[5]), x1 = (float)atof(argv[6]), z1 = (float)atof(argv[7]), top = argc > 8 ? (float)atof(argv[8]) : 3.f; float L = hypotf(x1 - x0, z1 - z0); int n = (int)L;
    for (int k = 0; k <= n; k++) { float t = n ? (float)k / n : 0, x = x0 + (x1 - x0) * t, z = z0 + (z1 - z0) * t; int m; float g = ground(x, z, top, &m); float n3[3] = {0, 1, 0}, gg; colGround(bwCol(), x, z, top, -1e6f, &gg, n3);
      printf("(%.1f %.1f) y %s%.2f n.y %.2f mat %d\n", x, z, g < -1e8f ? "нет" : "", g < -1e8f ? 0.f : g, n3[1], m); }
    return 0; }
  if (!strcmp(mode, "sp")) {   // стейтпропы (SPR1) в радиусе R от точки: тип, класс (2 = куст-шар), положение, коробки коллизии (SpCol), жив ли
    float R = argc > 6 ? (float)atof(argv[6]) : 12.f; if (argc > 5) { PX = (float)atof(argv[4]); PZ = (float)atof(argv[5]); } int n = 0;
    if (getenv("SP_STEPS")) { bwPlace(&W, PX, ground(PX, PZ, 30.f, 0) + .3f, PZ, 0.f); for (int f = 0; f < atoi(getenv("SP_STEPS")); f++) bwStep(&W, 0, 0.f, 0.f); }   // SP_STEPS=N: N кадров мира (с игроком в точке) до печати — состояние после включения
    for (unsigned i = 0; i < W.sp.ninst; i++) { const SpInst *in = &W.sp.inst[i]; if (fabsf(in->x - PX) > R || fabsf(in->z - PZ) > R) continue; const SpType *T = &W.sp.type[in->type]; const SpRun *r = &W.sp.run[i]; n++;
      printf("SP %u тип %u кл%d (%.1f %.1f %.1f) жив %d col %d кусков %d вид %d", i, in->type, T->cls, in->x, in->y, in->z, r->alive, r->colOn, T->nPiece, T->kind);
      for (int q = 0; q < T->nCol && q < 3; q++) { const SpCol *c = &W.sp.col[T->firstCol + q]; printf(" [col%d k%d h(%.2f %.2f %.2f)]", q, c->kind, c->h[0], c->h[1], c->h[2]); } printf("\n"); }
    printf("стейтпропов в радиусе: %d\n", n); return 0; }
  if (!strcmp(mode, "reset")) {   // RESET CAR (gameReset) из точек кармана: куда ставит; успех — дальше DMIN м от кармана и не в пределах окна стенок (x>-19)
    int bad = 0, n = 0;
    for (float sz = -306.f; sz <= -291.f; sz += 5.f) for (float sx = -42.f; sx <= -22.f; sx += 5.f) { float g = ground(sx, sz, 3.f, 0); if (g < -1e8f) continue; bwPlace(&W, sx, g + .3f, sz, 0.f); for (int f = 0; f < 40; f++) bwStep(&W, 0, 0.f, 0.f);
      gameReset(&W.G); for (int f = 0; f < 60; f++) bwStep(&W, 0, 0.f, 0.f); n++; int in = W.car.x > -45.f && W.car.x < -19.5f && W.car.z > -309.f && W.car.z < -288.f; bad += in;
      printf("из (%.0f %.0f) -> (%.1f %.1f) %s\n", sx, sz, W.car.x, W.car.z, in ? "ОСТАЛАСЬ В КАРМАНЕ" : "вне"); }
    printf("ИТОГ: сброс оставил в кармане %d из %d\n", bad, n); return bad != 0; }
  if (!strcmp(mode, "roads")) {   // дороги трафика рядом с точкой: концы, число дорог, выходящих из конца (0 — тупик), ближайшие точки полос
    const Traffic *t = (const Traffic *)bwTraffic(); float R = argc > 6 ? (float)atof(argv[6]) : 40.f; if (argc > 5) { PX = (float)atof(argv[4]); PZ = (float)atof(argv[5]); }
    if (!t || !t->hd) { puts("нет трафика"); return 2; }
    for (uint32_t r = 0; r < t->hd->nRoad; r++) { const TrRoad *R_ = &t->road[r]; int near = 0; float bd = 1e9f, bx = 0, bz = 0;
      for (int l = 0; l < R_->lanes; l++) { const TrPt *P = (const TrPt *)((const char *)t->pt + 0); (void)P; }
      for (uint32_t k = 0; k < R_->nPt; k++) { const TrPt *p = &t->pt[R_->firstPt + k]; float d = hypotf(p->x - PX, p->z - PZ); if (d < bd) { bd = d; bx = p->x; bz = p->z; } if (d < R) near = 1; }
      if (!near) continue; int outS = 0, outE = 0; for (uint32_t q = 0; q < t->hd->nRoad; q++) { if (t->road[q].startJ == R_->endJ && q != r) outE++; if (t->road[q].startJ == R_->startJ && q != r) outS++; }
      printf("дорога %u: длина %.1f, полос %d, startJ %u endJ %u rev %d; других дорог из конца %d, из начала %d; ближняя точка осевой (%.1f %.1f) в %.1f м\n", r, R_->length, R_->lanes, R_->startJ, R_->endJ, (int)R_->rev == 0xFFFF ? -1 : (int)R_->rev, outE, outS, bx, bz, bd); }
    return 0; }
  if (!strcmp(mode, "rpts")) {   // rpts ROAD [шаг]: точки осевой дороги (x y z), каждая step-я
    const Traffic *t = (const Traffic *)bwTraffic(); uint32_t r = (uint32_t)atoi(argv[4]); int st = argc > 5 ? atoi(argv[5]) : 1; if (!t || r >= t->hd->nRoad) return 2; const TrRoad *R_ = &t->road[r];
    for (uint32_t k = 0; k < R_->nPt; k += st) { const TrPt *p = &t->pt[R_->firstPt + k]; printf("дорога %u pt %u (%.1f %.2f %.1f)\n", r, k, p->x, p->y, p->z); } return 0; }
  if (!strcmp(mode, "bushroll")) {   // bushroll [N] [V]: сквозь мягкие кусты-шары (кл. 2, высота <= 1,5 м) на малом ходу, 8 курсов x поперечный сдвиг -1/0/+1 м; печать: крен/клевок и колёса против такого же прохода с выключенным кустом (colOn=0)
    int N = argc > 4 ? atoi(argv[4]) : 40; float V = argc > 5 ? (float)atof(argv[5]) : 5.f; int cand[4096], nc = 0;
    for (unsigned i = 0; i < W.sp.ninst && nc < 4096; i++) { const SpType *T = &W.sp.type[W.sp.inst[i].type]; if (T->cls == 2 && T->nCol > 0 && T->h[1] <= 1.5f && W.sp.run[i].alive) cand[nc++] = (int)i; }
    if (getenv("BR_TRACE")) { cand[0] = atoi(getenv("BR_TRACE")); nc = 1; }
    size_t runSz = sizeof(SpRun) * W.sp.ninst; SpRun *run0 = malloc(runSz); memcpy(run0, W.sp.run, runSz);   // состояние стейтпропов до проходов: каждый проход с чистого листа (куст не сдвинут и не трясётся от прошлого)
    int step = nc > N ? nc / N : 1, used = 0, passes = 0, bad = 0, stuck = 0; float worstD = 0; char worst[200] = "";
    for (int ci = 0; ci < nc && used < N; ci += step, used++) { int i = cand[ci]; const SpInst *in = &W.sp.inst[i];
      for (int d = 0; d < 8; d++) for (int lo = -1; lo <= 1; lo++) {
        float hd = d * 0.785398f, fx = sinf(hd), fz = cosf(hd), rx = fz, rz = -fx, cx = in->x + rx * lo, cz = in->z + rz * lo, sx = cx - fx * 4.f, sz = cz - fz * 4.f, g = ground(sx, sz, in->y + 3.f, 0); if (g < -1e8f) continue;
        float roll[2], pit[2]; int two[2], pas = 0, pasC = 0;
        for (int mode2 = 0; mode2 < 2; mode2++) {   // 0 куст включён, 1 выключен (контроль: уклон и ямы дороги)
          memcpy(W.sp.run, run0, runSz); unsigned char keep = W.sp.run[i].colOn; if (mode2) W.sp.run[i].colOn = 0; bwPlace(&W, sx, g + .3f, sz, hd); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f); W.sp.run[i].colOn = mode2 ? 0 : keep;
          W.car.vel[0] = fx * V; W.car.vel[2] = fz * V; W.car.v = V; float rm = 0, pm = 0; int tw = 0, twm = 0;
          for (int f = 0; f < 60 * 5; f++) { bwStep(&W, f < 90 ? GP_R : 0, 0.f, 0.f); if (getenv("BR_TRACE") && i == atoi(getenv("BR_TRACE")) && d == atoi(getenv("BR_D")) && lo == atoi(getenv("BR_LO")) && f % 6 == 0 && f < 150) printf("  [%s] f%3d x%.2f z%.2f y%.2f v%.2f roll%6.1f pitch%6.1f wh%d\n", mode2 ? "без куста" : "куст", f, W.car.x, W.car.z, W.car.y, hypotf(W.car.vel[0], W.car.vel[2]), W.car.roll * 57.2958f, W.car.pitch * 57.2958f, W.car.wheels); float r = fabsf(W.car.roll) * 57.2958f, p = fabsf(W.car.pitch) * 57.2958f; if (r > rm) rm = r; if (p > pm) pm = p; if (W.car.wheels <= 2) { tw++; if (tw > twm) twm = tw; } else tw = 0; }
          { int pp = ((W.car.x - cx) * fx + (W.car.z - cz) * fz) > 2.5f; if (!mode2) pas = pp; else pasC = pp; }
          roll[mode2] = rm; pit[mode2] = pm; two[mode2] = twm; W.sp.run[i].colOn = keep; }
        if (getenv("BR_TRACE")) printf("  курс %3d сдвиг %2d: прошла %d, крен %.1f/%.1f клевок %.1f/%.1f два колеса %d/%d\n", d * 45, lo, pas, roll[0], roll[1], pit[0], pit[1], two[0], two[1]);
        if (roll[1] > 3.f || pit[1] > 3.f || two[1] > 0) continue;   // контроль без куста сам кренится (склон, яма, соседняя геометрия) — проход не годится для оценки куста
        passes++; float dr = roll[0] - roll[1]; int bd = (dr > 3.f) || (two[0] > 0) || (pit[0] > 3.f); bad += bd; if (!pas && pasC) stuck++;
        if (bd && dr + pit[0] > worstD) { worstD = dr + pit[0]; snprintf(worst, sizeof worst, "куст #%d (%.1f %.1f) курс %d° сдвиг %d м: крен %.1f° (без куста %.1f°), клевок %.1f° (без %.1f°), на двух колёсах %d кадр, прошла %d", i, in->x, in->z, d * 45, lo, roll[0], roll[1], pit[0], pit[1], two[0], pas); } } }
    printf("BUSHROLL кустов %d, ровных проходов %d, v %.1f: хуже контроля (крен +3°, клевок > 3° или хоть кадр на двух колёсах) %d; не проехала при проехавшем контроле %d; %s\n", used, passes, V, bad, stuck, worst); return bad != 0; }
  if (!strcmp(mode, "roadbox")) {   // дороги трафика, чья осевая проходит ВНУТРИ ящика коллизии (точка на 0..3 м над дорогой лежит в объёме): ящик закрывает проезд; печать: дорога, ящик, сколько точек внутри
    const Traffic *t = (const Traffic *)bwTraffic(); if (!t || !t->hd) { puts("нет трафика"); return 2; } int nr = 0;
    for (uint32_t r = 0; r < t->hd->nRoad; r++) { const TrRoad *R_ = &t->road[r]; int hit[64], nh = 0;
      for (uint32_t k = 0; k + 1 < R_->nPt; k++) for (int sub = 0; sub < 8; sub++) { const TrPt *p = &t->pt[R_->firstPt + k], *q = &t->pt[R_->firstPt + k + 1]; float u = sub / 8.f, x = p->x + (q->x - p->x) * u, y = p->y + (q->y - p->y) * u + 1.f, z = p->z + (q->z - p->z) * u;
        for (uint32_t b = 0; b < cw->nbox; b++) { const ColBox *B = &cw->box[b]; float d[3] = {x - B->c[0], y - B->c[1], z - B->c[2]};
          float lx = d[0] * B->ax[0] + d[1] * B->ax[1] + d[2] * B->ax[2], ly = d[0] * B->ay[0] + d[1] * B->ay[1] + d[2] * B->ay[2], lz = d[0] * B->az[0] + d[1] * B->az[1] + d[2] * B->az[2];
          if (fabsf(lx) < B->h[0] && fabsf(ly) < B->h[1] && fabsf(lz) < B->h[2]) { int f = 0; for (int h = 0; h < nh; h++) if (hit[h] == (int)b) f = 1; if (!f && nh < 64) hit[nh++] = (int)b; } } }
      if (nh) { nr++; printf("дорога %u (длина %.0f, начало (%.0f %.0f)): внутри ящиков", r, R_->length, t->pt[R_->firstPt].x, t->pt[R_->firstPt].z); for (int h = 0; h < nh; h++) printf(" b%d", hit[h]); printf("\n"); } }
    printf("ИТОГО дорог с осевой внутри ящика: %d\n", nr); return 0; }
  if (!strcmp(mode, "resetat")) {   // resetat x z hdeg [y-верх]: машина в точке, RESET CAR (gameReset), 5 с газа вперёд; печать: куда поставила, курс, сколько проехала и не упёрлась ли
    float x = (float)atof(argv[4]), z = (float)atof(argv[5]), hd = (float)atof(argv[6]) * 0.0174533f, g = ground(x, z, argc > 7 ? (float)atof(argv[7]) : 8.f, 0); if (g < -1e8f) { puts("RESETAT нет земли"); return 2; }
    bwPlace(&W, x, g + .3f, z, hd); for (int f = 0; f < 40; f++) bwStep(&W, 0, 0.f, 0.f); gameReset(&W.G); if (getenv("RA_DBG")) printf("  сразу после RESET (%.1f %.1f y %.2f) курс %.0f\n", W.car.x, W.car.z, W.car.y, W.car.h / 0.0174533f); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
    if (getenv("RA_DBG")) printf("  впереди %.1f м, позади %.1f м\n", gameResetAhead(&W.G, W.car.x, W.car.y, W.car.z, W.car.h), gameResetAhead(&W.G, W.car.x, W.car.y, W.car.z, W.car.h + 3.14159f));
    float x0 = W.car.x, z0 = W.car.z, h0 = W.car.h; float vmax = 0; for (int f = 0; f < 300; f++) { bwStep(&W, GP_R, 0.f, 0.f); float vh = hypotf(W.car.vel[0], W.car.vel[2]); if (vh > vmax) vmax = vh; }
    float dist = hypotf(W.car.x - x0, W.car.z - z0); printf("RESETAT из (%.1f %.1f): поставила (%.1f %.1f) курс %.0f°, за 5 с газа проехала %.1f м, v макс %.1f, конец (%.1f %.1f) v %.1f: %s\n", x, z, x0, z0, h0 / 0.0174533f, dist, vmax, W.car.x, W.car.z, hypotf(W.car.vel[0], W.car.vel[2]), dist < 10.f ? "УПЁРЛАСЬ" : "едет"); return dist < 10.f; }
  if (!strcmp(mode, "ray")) {   // ray x y z x2 y2 z2 [foliage]: доля пути до первого препятствия colRayFrac
    float a[3] = {(float)atof(argv[4]), (float)atof(argv[5]), (float)atof(argv[6])}, b[3] = {(float)atof(argv[7]), (float)atof(argv[8]), (float)atof(argv[9])}; printf("RAY frac %.3f\n", colRayFrac(cw, a, b, 0)); return 0; }
  if (!strcmp(mode, "sptype")) {   // sptype N: тип стейтпропа — куски (слот рисунка, вершин, индексов, текстура, смещение), состояния (vis-маска, автопереход), объёмы
    unsigned ty = (unsigned)atoi(argv[4]); if (ty >= W.sp.ntype) return 2; const SpType *T = &W.sp.type[ty];
    printf("тип %u cls %d atc %d масса %.1f вид %d кусков %d состояний %d объёмов %d коробка c(%.2f %.2f %.2f) h(%.2f %.2f %.2f)\n", ty, T->cls, T->atc, T->mass, T->kind, T->nPiece, T->nState, T->nCol, T->c[0], T->c[1], T->c[2], T->h[0], T->h[1], T->h[2]);
    for (int k = 0; k < T->nPiece; k++) { const SpPiece *P = &W.sp.piece[T->firstPiece + k]; printf("  кусок %d: слот %d сустав %d нв %u ни %u tex %d смещ (%.2f %.2f %.2f) sc %.3f флаги %08x\n", k, (P->flags >> 16) & 255, (P->flags >> 8) & 255, P->nv, P->ni, P->tex, P->ox, P->oy, P->oz, P->sc, (unsigned)P->flags); }
    for (int k = 0; k < T->nBill; k++) { const SpBill *B = &W.sp.bill[T->firstBill + k]; printf("  билборд %d: слот %d режим %d сустав %d tex %d t(%.2f %.2f %.2f) w %.2f h %.2f dist %.1f rgba %08x флаги %08x\n", k, B->slot, B->mode, B->joint, B->tex, B->t[0], B->t[1], B->t[2], B->w, B->h, B->dist, B->rgba, B->flags); }
    for (int k = 0; k < T->nState; k++) { const SpState *S = &W.sp.state[T->firstState + k]; printf("  состояние %d: vis %08x autoTo %d nEvent %d nCall %d poseMode %d", k, S->vis, S->autoTo, S->nEvent, S->nCall, S->poseMode); for (int q = 0; q < S->nCall; q++) { const SpCall *C = &W.sp.call[S->firstCall + q]; printf(" [вызов op %d arg %d t %.2f]", C->op, C->arg, C->time); } for (int q = 0; q < S->nEvent; q++) { const SpEvent *E = &W.sp.event[S->firstEvent + q]; printf(" [событие %d -> состояние %d]", E->event, E->to); } printf("\n"); }
    return 0; }
  if (!strcmp(mode, "invisp")) {   // стейтпропы с объёмами коллизии, у которых в НАЧАЛЬНОМ состоянии ничего не рисуется (vis = 0, нет билбордов, нет окон fx): невидимая твёрдость; печать: тип, экземпляров, где
    int nt = 0, ni = 0;
    for (unsigned ty = 0; ty < W.sp.ntype; ty++) { const SpType *T = &W.sp.type[ty]; if (!T->nCol || T->kind || !T->nState) continue; const SpState *S = &W.sp.state[T->firstState];
      if (S->vis || S->nFx || (!getenv("INVISP_ALL") && (S->nEvent || S->nCall || S->autoTo != 0xFF))) { if (getenv("INVISP_V") && !S->vis) printf("  (отсев тип %u: nBill %d nFx %d)\n", ty, T->nBill, S->nFx); continue; } int n = 0; float fx = 0, fz = 0; for (unsigned i = 0; i < W.sp.ninst; i++) if (W.sp.inst[i].type == ty) { if (!n) { fx = W.sp.inst[i].x; fz = W.sp.inst[i].z; } n++; }
      if (!n) continue; nt++; ni += n; printf("тип %u кл%d atc %d масса %.1f: экз %d, первый (%.0f %.0f), объёмов %d, кусков %d, состояний %d, vis1 %08x, событий %d вызовов %d автопереход %d\n", ty, T->cls, T->atc, T->mass, n, fx, fz, T->nCol, T->nPiece, T->nState, T->nState > 1 ? W.sp.state[T->firstState + 1].vis : 0u, S->nEvent, S->nCall, S->autoTo); }
    printf("ИТОГО типов %d, экземпляров %d\n", nt, ni); return 0; }
  if (!strcmp(mode, "resetsweep")) {   // resetsweep [шаг м = 12] [секунд газа = 4]: машина в точках ВСЕХ осей дорог трафика курсом по дороге -> RESET CAR -> оценка места постановки; виды: внутри (кузов в твёрдом: carPlace сдвинул > 1 м), стена (луч 8 м впереди < 1), вода/обрыв (DEATH под машиной или нет опоры), упёрлась (за N с газа < 10 м)
    const Traffic *t = (const Traffic *)bwTraffic(); if (!t || !t->hd) { puts("нет трафика"); return 2; } float stepM = argc > 4 ? (float)atof(argv[4]) : 12.f; int secs = argc > 5 ? atoi(argv[5]) : 4;
    int nMoved = 0, n = 0, nIn = 0, nWall = 0, nDeath = 0, nStuck = 0, nAny = 0, shown[5] = {0, 0, 0, 0, 0};
    for (uint32_t r = 0; r < t->hd->nRoad; r++) { const TrRoad *R_ = &t->road[r]; float acc = stepM;
      for (uint32_t k = 0; k + 1 < R_->nPt; k++) { const TrPt *p0 = &t->pt[R_->firstPt + k], *p1 = &t->pt[R_->firstPt + k + 1]; float dx = p1->x - p0->x, dz = p1->z - p0->z, L = hypotf(dx, dz); if (L < .5f) continue; float hd = atan2f(dx, dz);
        for (float u = (acc >= stepM ? 0.f : stepM - acc); u < L; u += stepM) { float x = p0->x + dx * u / L, z = p0->z + dz * u / L, yy = p0->y + (p1->y - p0->y) * u / L;
          float g = ground(x, z, yy + 2.5f, 0); if (g < -1e8f) continue; n++;
          if (getenv("RS_TRACE")) { fprintf(stderr, "S %d дорога %u (%.1f %.1f)\n", n, r, x, z); fflush(stderr); }
          bwPlace(&W, x, g + .3f, z, hd); for (int f = 0; f < 20; f++) bwStep(&W, 0, 0.f, 0.f); float rp[4]; int have = gameResetRoad && gameResetRoad(&W.car, rp), inside = 0;
          if (have) { bwPlace(&W, rp[0], rp[1] + .5f, rp[2], rp[3]); inside = hypotf(W.car.x - rp[0], W.car.z - rp[2]) > 1.0f; bwPlace(&W, x, g + .3f, z, hd); for (int f = 0; f < 20; f++) bwStep(&W, 0, 0.f, 0.f); }   // точка RESET (до выталкивания) лежит в твёрдом: carPlace сдвинул > 1 м
          gameReset(&W.G); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
          float fx = sinf(W.car.h), fz = cosf(W.car.h), a[3] = {W.car.x, W.car.y + 1.f, W.car.z}, b[3] = {a[0] + fx * 14.f, a[1], a[2] + fz * 14.f};
          float wd = gameResetAhead(&W.G, W.car.x, W.car.y, W.car.z, W.car.h); (void)a; (void)b; int wall = wd < 6.f, death = gameDeathAt(&W.G) >= 0 || W.car.wheels == 0;
          { float fx0 = W.car.x, fz0 = W.car.z, fh0 = W.car.h; bwPlace(&W, fx0, W.car.y + .2f, fz0, fh0); inside = hypotf(W.car.x - fx0, W.car.z - fz0) > .8f; }   // итог: поставленная машина не выталкивается из твёрдого
          if (have && hypotf(W.car.x - rp[0], W.car.z - rp[2]) > 3.5f) { nMoved++; if (getenv("RS_MOVED")) { char pc2[16]; placeCode(x, z, pc2, sizeof pc2); printf("MV дорога %u из (%.1f %.1f) %s: номинал (%.1f %.1f) -> поставила (%.1f %.1f) %.1f м\n", r, x, z, pc2, rp[0], rp[2], W.car.x, W.car.z, hypotf(W.car.x - rp[0], W.car.z - rp[2])); } }
          float x0 = W.car.x, z0 = W.car.z; for (int f = 0; f < 60 * secs; f++) bwStep(&W, GP_R, 0.f, 0.f); int stuck = hypotf(W.car.x - x0, W.car.z - z0) < 10.f;
          int any = inside || wall || death; nIn += inside; nWall += wall; nDeath += death; nStuck += stuck; nAny += any;
          if (any || (getenv("RS_STUCK") && stuck)) { char pc[16]; placeCode(x, z, pc, sizeof pc); char kd[40] = ""; if (inside) strcat(kd, " ВНУТРИ"); if (wall) strcat(kd, " СТЕНА"); if (death) strcat(kd, " ВОДА/ОБРЫВ"); if (stuck) strcat(kd, " УПЁРЛАСЬ");
            if (getenv("RS_ALL") || shown[0] < 400) { shown[0]++; printf("RS дорога %u из (%.1f %.1f) %s ->%s (поставила (%.1f %.1f), до стены впереди %.1f м; конец газа (%.1f %.1f y %.1f) v %.1f колёс %d)\n", r, x, z, pc, kd, x0, z0, wd, W.car.x, W.car.z, W.car.y, hypotf(W.car.vel[0], W.car.vel[2]), W.car.wheels); } } }
        acc = fmodf(acc + L, stepM); } }
    printf("RESETSWEEP точек %d: внутри %d, стена < 6 м впереди %d, вода/обрыв %d; плохих %d (справка: газом за %d с проехала < 10 м %d; поставлена не в номинале (> 3,5 м от неё) %d)\n", n, nIn, nWall, nDeath, nAny, secs, nStuck, nMoved); return 0; }
  if (!strcmp(mode, "deadends")) {   // все короткие (<= 60 м) дороги с тупиковым концом: где, длина (область действия правки RESET №1248)
    const Traffic *t = (const Traffic *)bwTraffic(); if (!t || !t->hd) { puts("нет трафика"); return 2; } int n = 0;
    for (uint32_t r = 0; r < t->hd->nRoad; r++) { const TrRoad *R_ = &t->road[r]; if (R_->length > 60.f) continue; int rv = R_->rev == 0xFFFF ? -1 : (int)R_->rev; int dj[2] = {0, 0}; int js[2] = {R_->startJ, R_->endJ};
      for (int e = 0; e < 2; e++) { dj[e] = 1; for (uint32_t q = 0; q < t->hd->nRoad; q++) { if ((int)q == (int)r || (int)q == rv) continue; if (t->road[q].startJ == js[e] || t->road[q].endJ == js[e]) { dj[e] = 0; break; } } }
      if (dj[0] != dj[1]) { const TrPt *P = &t->pt[R_->firstPt]; n++; printf("тупик: дорога %u длина %.1f тупик на %s, начало (%.0f %.0f) конец (%.0f %.0f)\n", r, R_->length, dj[1] ? "конце" : "начале", P[0].x, P[0].z, P[R_->nPt - 1].x, P[R_->nPt - 1].z); } }
    printf("ИТОГО коротких тупиковых дорог: %d\n", n); return 0; }
  if (!strcmp(mode, "bushtypes")) {   // типы стейтпропов класса 2: размеры коробки кусков и объёмов, число экземпляров
    for (unsigned ty = 0; ty < W.sp.ntype; ty++) { const SpType *T = &W.sp.type[ty]; if (T->cls != 2) continue; int ni = 0; for (unsigned i = 0; i < W.sp.ninst; i++) if (W.sp.inst[i].type == ty) ni++;
      printf("тип %u кл2 atc %d масса %.1f экз %d коробка h(%.2f %.2f %.2f) c(%.1f %.1f %.1f) объёмов %d", ty, (int)T->atc, T->mass, ni, T->h[0], T->h[1], T->h[2], T->c[0], T->c[1], T->c[2], T->nCol);
      for (int q = 0; q < T->nCol && q < 3; q++) { const SpCol *c = &W.sp.col[T->firstCol + q]; printf(" [k%d h(%.2f %.2f %.2f) c(%.1f %.1f %.1f)]", c->kind, c->h[0], c->h[1], c->h[2], c->c[0], c->c[1], c->c[2]); } printf("\n"); }
    return 0; }
  if (!strcmp(mode, "thru")) {   // thru x z hdeg [dist] [secs]: машина за dist м до точки, курс hdeg (0 = +z, 90 = +x), газ 6 с; печать: прошла ли точку (проекция на курс > 1,5 м за ней), мин. скорость, где встала
    float x = (float)atof(argv[4]), z = (float)atof(argv[5]), hd = (float)atof(argv[6]) * 0.0174533f, dist = argc > 7 ? (float)atof(argv[7]) : 6.f, secs = argc > 8 ? (float)atof(argv[8]) : 6.f;
    float fx = sinf(hd), fz = cosf(hd), sx = x - fx * dist, sz = z - fz * dist, g = ground(sx, sz, getenv("PK_Y") ? (float)atof(getenv("PK_Y")) : 3.f, 0); if (g < -1e8f) { puts("THRU нет земли"); return 2; }
    if (getenv("PK_NOSP")) for (unsigned i = 0; i < W.sp.ninst; i++) if (fabsf(W.sp.inst[i].x - x) < 25.f && fabsf(W.sp.inst[i].z - z) < 25.f) W.sp.run[i].colOn = W.sp.run[i].col2On = 0;   // PK_NOSP=1: стейтпропы рядом (кусты и прочее) выключены — остаётся одна геометрия мира
    bwPlace(&W, sx, g + .3f, sz, hd); for (int f = 0; f < 30; f++) bwStep(&W, 0, 0.f, 0.f);
    float vmax = 0.f, vinside = 0.f; int passed = 0;
    for (int f = 0; f < (int)(secs * 60); f++) { bwStep(&W, GP_R, 0.f, 0.f); float vh = hypotf(W.car.vel[0], W.car.vel[2]); if (vh > vmax) vmax = vh; float pr = (W.car.x - x) * fx + (W.car.z - z) * fz; if (pr > 1.5f) { passed = 1; } if (fabsf(pr) < 1.5f && vh > vinside) vinside = vh; }
    float pr = (W.car.x - x) * fx + (W.car.z - z) * fz; printf("THRU (%.1f %.1f) курс %.0f°: %s, итог за точкой %.1f м, v макс %.1f, v у точки %.1f, конец (%.1f %.1f)\n", x, z, hd / 0.0174533f, passed ? "ПРОШЛА" : "НЕ ПРОШЛА", pr, vmax, vinside, W.car.x, W.car.z); return !passed; }
  return 3;
}
