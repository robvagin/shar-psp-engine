// 107b (Стена, Fable §2.3 РАЗБОР_FABLE2): долгоживущие строки чёрного ящика STUCK / NEAR / PAUSE / MSNEND по образцу хвоста ICON. Цель: следующий случай «упёрлись в невидимое» ящик называет сам:
// кто держит машину или Гомера (вид, имя, расстояние), что рядом в 20 м (в том числе голова колонны трафика и причина её остановки), нарисовано ли, есть ли модель, mcrBad, и чем кончилась миссия.
// Только чтение: игру не меняет (контакт берётся из уже посчитанного — carDbg1050, carHitRec108, homerSrc108). STUCKLOG108_OLD=1 / flag stucklog108old 1 — ничего не пишется и не собирается.
// Включается из main_frontend.h после bbNoteIcon (нужны car, homer, traffic, mcr, bbMsn, placeCode); вызовы: stuck108Frame (main_loop.h, раз в кадр), bbNoteMsnEnd (msn_view.c)
#ifndef STUCK108_H
#define STUCK108_H
#ifndef STUCKLOG108_OLD
#define STUCKLOG108_OLD 0
#endif
#define BBD_N 16         // записей хвоста (одно событие STUCK/PAUSE — до четырёх: основная, TRFWAIT, MCRSTATE, HNR)
#define BBD_LEN 560      // байт на запись
static int stuckLog108Old = STUCKLOG108_OLD;
#define STK_OFF() (stuckLog108Old || !carDbgDiag108)   // 107b (ревью): откат файлом rollback.txt гасит carDbgDiag108 — строки ящика тоже молчат
static int hnrActive;   // предварительное объявление (определение ниже, main_frontend.h): погоня Hit & Run идёт
static char bbDiag[BBD_N][BBD_LEN]; static unsigned bbDiagN; static unsigned stuckFiredN, pauseFiredN, msnEndN;
void bbNoteDiag(const char *kind, const char *text) {
  if (STK_OFF()) return;
  char *r = bbDiag[bbDiagN++ & (BBD_N - 1)]; snprintf(r, BBD_LEN, "%.0f %s %s\n", bbClock, kind, text);
  { char t[200]; snprintf(t, sizeof t, "%s %.150s", kind, text); bbNote(t); }   // и в кольцо: короткая первая часть
  if (smkOn) { char u[BBD_LEN + 24]; snprintf(u, sizeof u, "%s %s t=%.1f\n", kind, text, smkT); smkWrite(u); }
}
// контакт кузова в последних кадрах: статика коллизии района (carDbg1050) — копия до обнуления главным циклом
static struct { unsigned frame; int n; unsigned char type[4]; unsigned idx[4]; float nx[4], nz[4], d[4]; } stBody108;
static const char *const stBodyNm108[4] = {"tri", "box", "cyl", "seg"};
static void stuck108Body(void) {   // раз в кадр, до обнуления carDbg1050 (main_loop.h)
  if (STK_OFF() || !carDbg1050.bn) return;
  stBody108.frame = diagFrame108; stBody108.n = carDbg1050.bn < 4 ? carDbg1050.bn : 4;
  for (int k = 0; k < stBody108.n; k++) { stBody108.type[k] = carDbg1050.btype[k]; stBody108.idx[k] = carDbg1050.bidx[k]; stBody108.nx[k] = carDbg1050.bnx[k]; stBody108.nz[k] = carDbg1050.bnz[k]; stBody108.d[k] = carDbg1050.bd[k]; }
}
typedef struct { float d, x, z, v; int kind, idx, head; char text[110]; } StNear108;
static int stNearCmp108(const void *a, const void *b) { const StNear108 *p = a, *q = b; if (p->head != q->head) return q->head - p->head; return p->d < q->d ? -1 : p->d > q->d; }
static int mcrModelOk108(const char *name) { for (int k = 0; k < MCR_MAX; k++) if (mcr[k].blob && !strcasecmp(mcr[k].name, name)) return 1; return trModelOf(name) >= 0; }
static int mcrBadOf108(const char *name) {   // блок модели в каталоге пакета есть, но помечен битым (mcrBad): 1; нет в каталоге: 2; иначе 0
  if (!mcrDir) return 3; int e = -1; for (unsigned i = 0; i < mcrN; i++) if (!strncasecmp((const char *)mcrDir + i * 176, name, 16)) { e = (int)i; break; }
  return e < 0 ? 2 : mcrBad[e] ? 1 : 0;
}
// что в 20 м: машины трафика, стоящие, машины миссии, пешеходы; впереди — голова колонны (трафик, у которого впереди не трафик); не больше 6 записей
static int stuckNear108(StNear108 *o, int cap, float px, float pz) {
  int n = 0; const float R = 20.f; unsigned F = diagFrame108;
  if (haveTraffic) {
    const Car *pl = &car; int homerOn = onFoot();
    for (int i = 0; i < traffic.maxCars && n < cap; i++) { const TrCar *c = &traffic.car[i]; if (c->mode == TRM_OFF) continue;
      float dx = c->car.x - px, dz = c->car.z - pz, d = sqrtf(dx * dx + dz * dz); if (d > R) continue;
      char why[40]; trafficWhyStop108(&traffic, i, onFoot() ? NULL : pl, homerOn, homer.x, homer.z, why, sizeof why, NULL, NULL, NULL);
      int head = c->v < 1.f && why[0] && strncmp(why, "trf", 3) && strcmp(why, "free");
      const char *nm = traffic.model[c->model].name; int dr = F - c->drawnF108 <= 2u;
      StNear108 *e = &o[n++]; e->d = d; e->x = c->car.x; e->z = c->car.z; e->v = c->v; e->kind = 1; e->idx = i; e->head = head;
      snprintf(e->text, sizeof e->text, "trf#%d %.10s %.1fm v=%.1f %s%s dr=%d mdl=%d st=%.1f why=%s", i, nm, d, c->v, c->mode == TRM_DRIVE ? "drv" : c->mode == TRM_PHYS ? "phys" : "wreck", carShowHusk(&c->car) ? "/husk" : "", dr, 1, c->stillT, why); }
    for (int i = 0; i < traffic.npark && n < cap; i++) { const TrParked *p = &traffic.park[i]; if (p->gone) continue;
      float dx = p->car.x - px, dz = p->car.z - pz, d = sqrtf(dx * dx + dz * dz); if (d > R) continue;
      StNear108 *e = &o[n++]; e->d = d; e->x = p->car.x; e->z = p->car.z; e->v = 0; e->kind = 2; e->idx = i; e->head = 0;
      snprintf(e->text, sizeof e->text, "park#%d %.1fm st=%d%s dr=%d", i, d, p->state, p->wreck ? "/wreck" : "", F - p->drawnF108 <= 2u); }
  }
  { const MsnCars *m = mvCars(); if (m) for (int i = 0; i < m->n && n < cap; i++) { const McCar *c = &m->c[i]; if (!c->on) continue;
      float dx = c->car.x - px, dz = c->car.z - pz, d = sqrtf(dx * dx + dz * dz); if (d > R) continue;
      StNear108 *e = &o[n++]; e->d = d; e->x = c->car.x; e->z = c->car.z; e->v = hypotf(c->car.vel[0], c->car.vel[2]); e->kind = 3; e->idx = i; e->head = 1;   // машина миссии — всегда в списке
      snprintf(e->text, sizeof e->text, "msn#%d %.12s %.1fm v=%.1f m=%d dr=%d%s mdl=%d bad=%d", i, c->name, d, e->v, c->mode, i < 8 && F - mcrDrawnF108[i] <= 2u, i < 8 && mcrStandF108[i] ? "/stand" : "", mcrModelOk108(c->name), mcrBadOf108(c->name)); } }
  if (haveTraffic) for (int i = 0; i < traffic.maxPeds && n < cap; i++) { const TrPed *p = &traffic.ped[i]; if (p->mode == TRP_OFF) continue;
      float dx = p->x - px, dz = p->z - pz, d = sqrtf(dx * dx + dz * dz); if (d > 3.f) continue;   // пешеходы — только вплотную
      StNear108 *e = &o[n++]; e->d = d; e->x = p->x; e->z = p->z; e->v = p->speed; e->kind = 4; e->idx = i; e->head = 0;
      snprintf(e->text, sizeof e->text, "ped#%d %.1fm m=%d", i, d, p->mode); }
  qsort(o, (size_t)n, sizeof *o, stNearCmp108);
  return n;
}
// TRFWAIT: что держит стоящие машины трафика рядом (до 40 м): кто впереди — вид, имя, модель, расстояние; для машины миссии — есть ли у неё модель и mcrBad (невидимое тело)
static void trfWaitDump108(float px, float pz) {
  if (!haveTraffic) return;
  char b[BBD_LEN]; int o = snprintf(b, sizeof b, "L%d", gameLevel), n = 0; float hx = homer.x, hz = homer.z;
  for (int i = 0; i < traffic.maxCars && n < 6 && o < BBD_LEN - 110; i++) { const TrCar *c = &traffic.car[i]; if (c->mode == TRM_OFF || c->v >= 1.f) continue;
    float dx = c->car.x - px, dz = c->car.z - pz, d = sqrtf(dx * dx + dz * dz); if (d > 40.f) continue;
    char why[40]; int k = 0, ix = -1; float g = 0; trafficWhyStop108(&traffic, i, onFoot() ? NULL : &car, onFoot(), hx, hz, why, sizeof why, &k, &ix, &g);
    char who[56] = ""; if (k == 2 && mvCars() && ix >= 0 && ix < mvCars()->n) { const McCar *m = &mvCars()->c[ix]; snprintf(who, sizeof who, "(%.10s%s mdl=%d bad=%d dr=%d)", m->name, m->police ? " POLICE" : "", mcrModelOk108(m->name), mcrBadOf108(m->name), ix < 8 && diagFrame108 - mcrDrawnF108[ix] <= 2u); }
    else if (k == 5 && ix >= 0) snprintf(who, sizeof who, "(%.10s)", traffic.model[traffic.car[ix].model].name);
    else if (k == 6) snprintf(who, sizeof who, "(стоит)");
    o += snprintf(b + o, sizeof b - (size_t)o, " [trf#%d %.10s %.0fm %s %s%s w=%.1f]", i, traffic.model[c->model].name, d, c->mode == TRM_DRIVE ? "drv" : "phys", why, who, c->waitT); n++; }
  if (n) bbNoteDiag("TRFWAIT", b);
}
// MCRSTATE: все машины миссии — имя, on, модель есть/нет, mcrBad, нарисована в кадре, позиция; HNR: шкала, погоня, где полиция
static void mcrStateDump108(void) {
  const MsnCars *m = mvCars(); if (!m || !m->n) return;
  char b[BBD_LEN]; int o = snprintf(b, sizeof b, "L%d n=%d", gameLevel, m->n);
  for (int i = 0; i < m->n && o < BBD_LEN - 100; i++) { const McCar *c = &m->c[i];
    o += snprintf(b + o, sizeof b - (size_t)o, " [#%d %.10s on=%d mdl=%d bad=%d dr=%d%s %.0f,%.0f m=%d%s]", i, c->name, c->on, mcrModelOk108(c->name), mcrBadOf108(c->name), i < 8 && diagFrame108 - mcrDrawnF108[i] <= 2u, i < 8 && mcrStandF108[i] ? "/stand" : "", c->car.x, c->car.z, c->mode, c->police ? " POLICE" : ""); }
  bbNoteDiag("MCRSTATE", b);
}
static void hnrDump108(void) {
  char b[BBD_LEN]; int o = snprintf(b, sizeof b, "heat=%.2f latched=%d active=%d off=%d busted=%.1f police:", hnrHeat, G.hnrLatched, hnrActive, G.hnrOff, G.hnrBusted); int np = 0;
  const MsnCars *m = mvCars(); if (m) for (int i = 0; i < m->n && o < BBD_LEN - 70; i++) { const McCar *c = &m->c[i]; if (!c->on || !c->police) continue;
    o += snprintf(b + o, sizeof b - (size_t)o, " [#%d %.9s %.0f,%.0f mdl=%d]", i, c->name, c->car.x, c->car.z, mcrModelOk108(c->name)); np++; }
  if (!np) snprintf(b + o, sizeof b - (size_t)o, " нет");
  bbNoteDiag("HNR", b);
}
static float stuckT108, stuckLast108 = -100.f; static int stuckPrevPause108;
static void stuckDump108(const char *kind, int contact) {
  if (STK_OFF()) return;
  char b[BBD_LEN]; int o = 0, foot = onFoot();
  float x = foot ? homer.x : car.x, y = foot ? homer.y : car.y, z = foot ? homer.z : car.z, sp = foot ? homer.speed : hypotf(car.vel[0], car.vel[2]); char pc[24]; placeCode(x, z, pc, sizeof pc);
  o += snprintf(b, sizeof b, "L%d %s p=%.1f,%.1f,%.1f %s v=%.1f want=%.2f msn=%s", gameLevel, pc, x, y, z, foot ? "foot" : "car", sp, foot ? diagFootWant108 : stuckT108, bbMsn);
  if (contact) {
    if (!foot) {
      if (stBody108.n && diagFrame108 - stBody108.frame <= 90u) { o += snprintf(b + o, sizeof b - (size_t)o, " | body"); for (int k = 0; k < stBody108.n && o < 330; k++) o += snprintf(b + o, sizeof b - (size_t)o, " %s#%u n=%.1f,%.1f d=%.2f", stBodyNm108[stBody108.type[k] & 3], stBody108.idx[k], stBody108.nx[k], stBody108.nz[k], stBody108.d[k]); o += snprintf(b + o, sizeof b - (size_t)o, " age=%.1fs", (diagFrame108 - stBody108.frame) / 60.f); }
      for (int k = 0; k < 4; k++) { const CarHitRec108 *h = &carHitRec108[(carHitN108 - 1u - (unsigned)k) & 3]; if (carHitN108 <= (unsigned)k || h->a != &car || diagFrame108 - h->frame > 90u) continue;
        o += snprintf(b + o, sizeof b - (size_t)o, " | hit %.1f,%.1f hl=%.1f hw=%.1f age=%.1fs", h->bx, h->bz, h->hl, h->hw, (diagFrame108 - h->frame) / 60.f); break; }
    } else {
      if (homerSrc108 && diagFrame108 - homerSrcF108 <= 90u) o += snprintf(b + o, sizeof b - (size_t)o, " | solid=%s age=%.1fs", homerSrc108 == 1 ? "stprop" : homerSrc108 == 2 ? "prop" : "trf/ped/msn", (diagFrame108 - homerSrcF108) / 60.f);
      if (homer.wallHit) o += snprintf(b + o, sizeof b - (size_t)o, " | wall(col)");
    }
  }
  hnrDump108(); mcrStateDump108(); trfWaitDump108(x, z);   // основная запись — последней (хвост выводится от новых к старым: она не теряется)
  StNear108 nb[24]; int nn = stuckNear108(nb, 24, x, z);
  if (nn) o += snprintf(b + o, sizeof b - (size_t)o, " | near%d:", nn);
  for (int k = 0; k < nn && k < 6 && o < BBD_LEN - 120; k++) o += snprintf(b + o, sizeof b - (size_t)o, " [%s%s]", nb[k].head && nb[k].kind == 1 ? "HEAD " : "", nb[k].text);
  bbNoteDiag(kind, b);
}
void stuck108Frame(float want, float dt, int paused, int inputOk) {   // раз в кадр после шага машины/Гомера. want — тянут газ/стик 0..1; inputOk — идёт игра (не отсчёт/ролик/разговор)
  if (STK_OFF()) return;
  stuckT108 = want;
  if (paused && !stuckPrevPause108) { stuckDump108("PAUSE", 0); pauseFiredN++; }
  stuckPrevPause108 = paused;
  if (paused || !inputOk) { stuckT108 = 0.f; return; }
  static float still; int foot = onFoot(); float sp = foot ? homer.speed : hypotf(car.vel[0], car.vel[2]);
  if (want > 0.5f && sp < 0.5f) still += dt; else still = 0.f;
  if (still > 1.5f && bbClock - stuckLast108 >= 5.f) { stuckLast108 = bbClock; still = 0.f; stuckT108 = want; stuckDump108("STUCK", 1); stuckFiredN++; }
}
void bbNoteMsnEnd(const char *why) {
  if (STK_OFF()) return;
  static float lastT = -100.f; static char lastWhy[24];
  if (!strcmp(why, "cleanup") && bbClock - lastT < 1.f) return;   // общий путь CLEANUP не затирает названную причину
  lastT = bbClock; snprintf(lastWhy, sizeof lastWhy, "%s", why);
  char b[BBD_LEN]; int o = 0, foot = onFoot(); float x = foot ? homer.x : car.x, z = foot ? homer.z : car.z; char pc[24]; placeCode(x, z, pc, sizeof pc);
  o += snprintf(b, sizeof b, "why=%s L%d %s p=%.1f,%.1f %s msn=%s", why, gameLevel, pc, x, z, foot ? "foot" : "car", bbMsn);
  { const MsnCars *m = mvCars(); if (m) for (int i = 0; i < m->n && i < 5 && o < BBD_LEN - 90; i++) { const McCar *c = &m->c[i]; if (!c->on) continue; o += snprintf(b + o, sizeof b - (size_t)o, " [%.10s %.0f,%.0f]", c->name, c->car.x, c->car.z); } }
  msnEndN++; bbNoteDiag("MSNEND", b);
}
#endif
