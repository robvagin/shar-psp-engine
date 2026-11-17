// №1328: автомат с газировкой — что видно после ударов. На данных уровня (level.shr), настоящие props.c/stateprops.c, без эмулятора.
// Тип автомата: машина состояний с вызовом SPAWN_*_COINS_Z (op COINS_Z) и событиями HIT/VEHICLE_HIT. Для каждого: путь VEH (удар машиной) и путь HIT x3 (кулак).
// Печать: на каждом шаге состояние, живой ли, коллизия, число включённых слотов/кусков и их вершины (чтобы видеть, что целый корпус ушёл, а разбитый появился).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "props.h"
#include "stateprops.h"
unsigned homerSolidFrame = 0;
void wdFire(const char *k, const char *a, const char *i) { (void)k; (void)a; (void)i; }
static Props props; static StateProps sp; static ColWorld world;
static unsigned lastV, lastOn;
static void show(const char *tag, unsigned i) {
  const SpRun *r = &sp.run[i]; const SpType *T = &sp.type[sp.inst[i].type];
  unsigned on = 0, nvOn = 0, slots = spSlotCount(&sp, sp.inst[i].type); char m[200] = "";
  for (unsigned k = 0; k < T->nPiece; k++) if (spPieceOn(&sp, i, k)) { on++; const SpPiece *P = &sp.piece[T->firstPiece + k]; nvOn += P->nv; char t[24]; snprintf(t, sizeof t, " s%u:v%u", (P->flags >> 16) & 255, P->nv); strcat(m, t); float M[16]; spPose(&sp, i); spPieceMatrix(&sp, i, k, M); snprintf(t, sizeof t, "@(%.2f %.2f %.2f up %.2f)", M[12]-r->x, M[13]-r->y, M[14]-r->z, M[5]); strcat(m, t); }
  lastV = nvOn; lastOn = on;
  printf("  %-8s state %d alive %d colOn %d col2On %d coins %d куски вкл %u из %u вершин %u [%s ] слотов %u\n", tag, r->state, r->alive, r->colOn, r->col2On, sp.coins, on, T->nPiece, nvOn, m, slots);
}
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
  fseek(f, 0, SEEK_END); long size = ftell(f); rewind(f);
  unsigned char *data = aligned_alloc(16, (size + 15) & ~15); if (!data || fread(data, 1, size, f) != (size_t)size) return 2; fclose(f);
  unsigned co = *(unsigned *)(data + 92), po = *(unsigned *)(data + 96), pv = *(unsigned *)(data + 88), ce = (unsigned)size;
  if (po > co && po < ce) ce = po; if (pv > co && pv < ce) ce = pv;
  if (colInit(&world, data + co, ce - co) || propsInit(&props, data + po, *(unsigned *)(data + po + 40)) || !props.spr || spInit(&sp, props.spr, props.sprSize)) { printf("TYPES 0\n"); return 0; }
  if (argc > 2 && !strcmp(argv[2], "all")) {   // все типы SPR1: видимость по состояниям (младшие 32 бита) — для сверки с данными диска
    for (unsigned t = 0; t < sp.ntype; t++) { const SpType *T = &sp.type[t]; if (T->kind || !T->nState) continue;
      printf("TV %u", t); for (unsigned k = 0; k < T->nState; k++) printf(" %08x", sp.state[T->firstState + k].vis); printf("\n"); }
    return 0; }
  if (argc > 2 && !strcmp(argv[2], "types")) {   // все типы: номер, куски, состояния, события состояния 0, первый экземпляр (x y z, лицо)
    for (unsigned t = 0; t < sp.ntype; t++) { const SpType *T = &sp.type[t]; if (T->kind || !T->nState) continue;
      unsigned inst = ~0u, n = 0; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { if (inst == ~0u) inst = i; n++; }
      printf("TYPE %u cls %u pieces %u states %u inst %u", t, T->cls, T->nPiece, T->nState, n);
      const SpState *S = &sp.state[T->firstState]; printf(" s0ev"); for (unsigned e = 0; e < S->nEvent; e++) printf(" %u", sp.event[S->firstEvent + e].event);
      if (inst != ~0u) printf(" at %.1f %.1f %.1f face %.2f %.2f", sp.inst[inst].x, sp.inst[inst].y, sp.inst[inst].z, sp.run[inst].R[6], sp.run[inst].R[8]);
      printf("\n"); }
    return 0; }
  if (argc > 2 && !strcmp(argv[2], "glass")) {   // стекло автомата (кусок из 6 вершин слота 3 у типа с «монетами вперёд»): текстура, флаги, цвет вершин (ABGR)
    for (unsigned t = 0; t < sp.ntype; t++) { const SpType *T = &sp.type[t]; int vend = 0; if (T->kind) continue;
      for (unsigned s = 0; s < T->nState; s++) { const SpState *S = &sp.state[T->firstState + s]; for (unsigned c = 0; c < S->nCall; c++) if (sp.call[S->firstCall + c].op == SP_OP_COINS_Z) vend = 1; }
      if (!vend) continue;
      for (unsigned k = 0; k < T->nPiece; k++) { const SpPiece *P = &sp.piece[T->firstPiece + k]; if (P->nv != 6) continue;
        printf("GLASS type %u piece %u tex %d flags %08x color %08x\n", t, k, P->tex, P->flags, *(unsigned *)((char *)sp.vertBase + P->vOff + 4));
        unsigned inst = ~0u; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { inst = i; break; }
        if (inst != ~0u && argc > 3) { spPose(&sp, inst); float M[16]; spPieceMatrix(&sp, inst, k, M);
          for (int v = 0; v < 6; v++) { const short *q = (const short *)((char *)sp.vertBase + P->vOff + v * 16);   // u v c(4) x y z pad: 16 Б
            float x = q[4], y = q[5], z = q[6];
            printf("GV %d %.4f %.4f %.4f u %.5f v %.5f\n", v, (M[0] * x + M[4] * y + M[8] * z) / 32768 + M[12], (M[1] * x + M[5] * y + M[9] * z) / 32768 + M[13], (M[2] * x + M[6] * y + M[10] * z) / 32768 + M[14], (unsigned short)q[0] / 256.0, (unsigned short)q[1] / 256.0); }
          printf("GI %.3f %.3f %.3f\n", sp.run[inst].x, sp.run[inst].y, sp.run[inst].z); } } }
    return 0; }
  int nt = 0;
  for (unsigned t = 0; t < sp.ntype; t++) {
    const SpType *T = &sp.type[t]; int vend = 0;
    if (T->kind) continue;
    for (unsigned s = 0; s < T->nState; s++) { const SpState *S = &sp.state[T->firstState + s]; for (unsigned c = 0; c < S->nCall; c++) if (sp.call[S->firstCall + c].op == SP_OP_COINS_Z) vend = 1; }
    if (!vend) continue;
    unsigned inst = ~0u, n = 0; for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) { if (inst == ~0u) inst = i; n++; }
    if (inst == ~0u) continue;
    for (unsigned i = 0; i < sp.ninst; i++) if (sp.inst[i].type == t) printf("  INST %u at %.1f %.1f %.1f face %.2f %.2f\n", i, sp.inst[i].x, sp.inst[i].y, sp.inst[i].z, sp.run[i].R[6], sp.run[i].R[8]);
    nt++; printf("VEND type %u: экземпляров %u, состояний %u, кусков %u\n", t, n, T->nState, T->nPiece);
    for (unsigned s = 0; s < T->nState; s++) { const SpState *S = &sp.state[T->firstState + s]; printf("   s%u:", s);
      for (unsigned e = 0; e < S->nEvent; e++) printf(" ev%u->s%u", sp.event[S->firstEvent + e].event, sp.event[S->firstEvent + e].to);
      for (unsigned c = 0; c < S->nCall; c++) printf(" call%u(%u)", sp.call[S->firstCall + c].op, sp.call[S->firstCall + c].arg);
      printf(" vis=%08x auto=%d\n", S->vis, (int)S->autoTo); }
    for (int path = 0; path < 2; path++) {
      spFree(&sp); if (spInit(&sp, props.spr, props.sprSize)) return 3; sp.coins = 0;
      printf(" путь %s\n", path ? "HIT x3" : "VEHICLE_HIT"); show("старт", inst); if (!path) printf("RES START verts=%u\n", lastV);
      for (int k = 0; k < (path ? 3 : 1); k++) {
        int ok = spEvent(&sp, inst, path ? SP_EV_HIT : SP_EV_VEHICLE_HIT);
        for (int fr = 0; fr < 90; fr++) spUpdate(&sp, &world, 0, 0, 1.f / 30.f);
        char tg[16]; snprintf(tg, sizeof tg, "%s%d%s", path ? "HIT" : "VEH", k + 1, ok ? "" : "?"); show(tg, inst);
        if (k == (path ? 2 : 0)) printf("RES %s ok=%d state=%d col=%d coins=%d verts=%u\n", path ? "HIT" : "VEH", ok, sp.run[inst].state, sp.run[inst].colOn, sp.coins, lastV);
      }
    }
  }
  printf("TYPES %d\n", nt); return 0;
}
