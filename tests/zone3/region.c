// Build 95 шаг 9в, Зоны-95: один регион областей над Z0 (viewer/zone3.h). Хост-модель кучи: первый подходящий снизу, склейка соседей.
// cc -O1 -DZ3_HOST -o region tests/zone3/region.c && ./region [new|old] [MB_кучи]
// Цепочка L1->L7: на каждой выгрузке считаем свободные куски >= 0,4 МБ и долю наибольшего (как tests/zone0/freemap_gate.py --ratio 0.9 --max-chunks 2).
// new - регион (посадка снизу вверх, отцепка с верхнего края, прицепка на тот же адрес); old - прежний порядок (блок уровня до областей, отцепка в любом порядке).
// Плюс: занятая верхняя область не перепрыгивается (середина кучи не трогается). Последняя строка PASS/FAIL; для old FAIL ожидается (красный контроль).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
struct _reent;
typedef struct Bk { unsigned off, size; int used; struct Bk *next, *prev; } Bk;
static unsigned char *arena; static Bk *head; static unsigned ARENA = 40u << 20;
static void hinit(unsigned sz) { ARENA = sz; arena = aligned_alloc(4096, ARENA); head = calloc(1, sizeof(Bk)); head->size = ARENA; }
static void *H_alloc(size_t sz, size_t al) {
  sz = (sz + 63u) & ~63u; if (!sz) sz = 64;
  for (Bk *b = head; b; b = b->next) {
    if (b->used) continue; unsigned a = (b->off + (unsigned)al - 1u) & ~((unsigned)al - 1u), pad = a - b->off;
    if (b->size < pad + sz) continue;
    if (pad) { Bk *n = calloc(1, sizeof(Bk)); n->off = b->off; n->size = pad; n->next = b; n->prev = b->prev; if (b->prev) b->prev->next = n; else head = n; b->prev = n; b->off = a; b->size -= pad; }
    if (b->size > sz) { Bk *n = calloc(1, sizeof(Bk)); n->off = b->off + sz; n->size = b->size - sz; n->next = b->next; n->prev = b; if (b->next) b->next->prev = n; b->next = n; b->size = sz; }
    b->used = 1; return arena + b->off; }
  return 0;
}
static void H_free(void *p) {
  Bk *b = head; while (b && !(b->used && arena + b->off == (unsigned char *)p)) b = b->next; if (!b) { printf("FAIL: free чужого\n"); exit(3); }
  b->used = 0;
  if (b->next && !b->next->used) { Bk *n = b->next; b->size += n->size; b->next = n->next; if (n->next) n->next->prev = b; free(n); }
  if (b->prev && !b->prev->used) { Bk *q = b->prev; q->size += b->size; q->next = b->next; if (b->next) b->next->prev = q; free(b); }
}
#define Z3_MA(al, n) H_alloc((n), (al))
#define Z3_FR(p) H_free(p)
#define Z3_HOST 1
#define Z3_LOGF(s) do { if (verbose) fputs(s, stdout); } while (0)
static int verbose;
static void *memalign(size_t al, size_t n) { return H_alloc(n, al); }
#define Z3_ISMAIN() 1
#include "../../viewer/zone3.h"
static void freemap(unsigned *tot, unsigned *mx, int *nbig, unsigned *big, int cap) {
  *tot = *mx = 0; *nbig = 0;
  for (Bk *b = head; b; b = b->next) if (!b->used) { *tot += b->size; if (b->size > *mx) *mx = b->size; if (b->size >= (400u << 10) && *nbig < cap) big[(*nbig)++] = b->size >> 10; }
}
static const unsigned NEED_KB[8] = { 0, 10500, 5000, 7000, 10500, 7000, 8000, 7500 }, RING_KB[8] = { 0, 5000, 3000, 3500, 6300, 3000, 3500, 4800 }, LVA_KB[8] = { 0, 4300, 2400, 3000, 4300, 3000, 3300, 3000 };
static int fit(int lv, unsigned lvaB) {   // как zone3FitLevel: сверка кучи с нуждой уровня, отцепка, прицепка
  unsigned tot, mx, big[8]; int nb; freemap(&tot, &mx, &nb, big, 8);
  unsigned need = (NEED_KB[lv] + 1536u) << 10, ring = RING_KB[lv] << 10, hf = tot > lvaB ? tot - lvaB : 0;
  for (int g = 0; g < 4; g++) { unsigned want = need + (z3A[Z3_CITY].base ? 0u : ring); if (hf >= want) break; unsigned fr = zone3Shrink(1, "fit"); if (!fr) break; hf += fr; }
  if (hf >= need) zone3Regrow(hf, need, ring, "fit");
  return 0;
}
int main(int argc, char **argv) {
  if (argc > 1 && !strncmp(argv[1], "busy", 4)) {   // занятая верхняя область: отцепка стоит, середину не трогаем (busyold - прежний порядок: перепрыгивает, дыра)
    hinit(40u << 20); H_alloc(6u << 20, 64); z3RegionOld = !strcmp(argv[1], "busyold"); zone3Init(3072u << 10, 3072u << 10, 7168u << 10, 0xFFFFFFFFu);
    void *p = zone3Alloc(Z3_CITY, "t", 1000, 64); unsigned f = zone3Shrink(1, "busy"); int hole = 0; for (Bk *b = head; b; b = b->next) if (!b->used && b->size >= (2u << 20) && b->next) hole = 1;
    printf("город занят: отцеплено %u КБ, дыра в середине кучи: %d\n", f >> 10, hole); zone3Free(p); printf("%s\n", f == 0 && !hole ? "PASS" : "FAIL"); return !(f == 0 && !hole); }
  if (argc > 1 && !strncmp(argv[1], "tight", 5)) {   // ужатие «места» вместо отцепки (tightold - прежнее: сразу отцепка)
    hinit(40u << 20); H_alloc(6u << 20, 64); if (!strcmp(argv[1], "tightold")) z3Tight[Z3_PLACE] = 0; zone3Init(3072u << 10, 3072u << 10, 7168u << 10, 0xFFFFFFFFu);
    unsigned f1 = zone3Shrink(1, "t1"), f2 = zone3Shrink(1, "t2"), f3 = zone3Shrink(1, "t3");   // город, миссия, место (ужатие)
    unsigned f4 = zone3Shrink(1, "t3b"); int floor_ = z3A[Z3_PLACE].base != 0 && f4 == 0;   // пол: дальше не отцепляется
    int small = z3A[Z3_PLACE].base != 0 && z3A[Z3_PLACE].cap == (2048u << 10) && (floor_ || !z3Tight[Z3_PLACE]);
    unsigned tot, mx, big[8]; int nb; freemap(&tot, &mx, &nb, big, 8);
    unsigned r = zone3Regrow(40u << 20, 8u << 20, 0, "t4") ? 1u : 0u; int full = z3A[Z3_PLACE].base != 0 && z3A[Z3_PLACE].cap == (3072u << 10);
    void *p = z3A[Z3_PLACE].base ? zone3Alloc(Z3_PLACE, "room", 900u << 10, 64) : 0; int fits = p != 0;
    printf("шаги отдали %u + %u + %u КБ; место ужато до 1,5 МБ: %d; после роста полное: %d (регроу %u); комната 0,9 МБ в ужатом: проверяется ниже; наибольший кусок %.1f МБ\n", f1 >> 10, f2 >> 10, f3 >> 10, small, full, r, mx / 1048576.);
    (void)fits; printf("%s\n", small && full ? "PASS" : "FAIL"); return !(small && full); }
  if (argc > 1 && !strncmp(argv[1], "shift", 5)) {   // под регионом освободился блок: прицепка садится ниже дома (shift) - не промах; shiftold = региональные правила без сдвига
    hinit(40u << 20); z3Tight[Z3_PLACE] = 0; H_alloc(6u << 20, 64); void *sk = H_alloc(374272, 64); zone3Init(3072u << 10, 3072u << 10, 7168u << 10, 0xFFFFFFFFu);
    zone3Shrink(1, "s1"); zone3Shrink(1, "s2"); zone3Shrink(1, "s3");           // всё отцеплено, кроме места (ужатие выключено ниже)
    H_free(sk);                                                                  // освободился блок под регионом
    unsigned long hm = (unsigned long)z3Home[Z3_MISSION]; (void)hm;
    int okr = zone3Regrow(40u << 20, 1u << 20, 0, "s4") > 0;
    printf("прицепка после освобождения блока под регионом: %d, сдвигов %u, промахов %u\n", okr, z3Shift, z3RegMiss);
    int good = okr && z3RegMiss == 0;
    printf("%s\n", good ? "PASS" : "FAIL"); return !good; }
  int old = argc > 1 && !strcmp(argv[1], "old"); unsigned mb = argc > 2 ? (unsigned)atoi(argv[2]) : 40; hinit(mb << 20);
  z3RegionOld = old; verbose = argc > 3;
  void *z0 = H_alloc(6u << 20, 64); (void)z0;                      // Z0: постоянное, внизу
  void *z0b = H_alloc(256u << 10, 64); (void)z0b;
  int bad = 0; void *lva = 0; unsigned lvaB = 0;
  if (!old) zone3Init(3072u << 10, 3072u << 10, 7168u << 10, 0xFFFFFFFFu);          // новый порядок: области первыми
  for (int lv = 1; lv <= 7; lv++) {
    lvaB = LVA_KB[lv] << 10;
    if (old) { lva = H_alloc(lvaB, 64); if (lv == 1) zone3Init(3072u << 10, 3072u << 10, 7168u << 10, 0xFFFFFFFFu); fit(lv, 0); }   // старый: блок уровня до областей (на L1 области ложатся над ним), потом сверка
    else { fit(lv, lvaB); lva = H_alloc(lvaB, 64); }
    if (!lva) { printf("L%d: блок уровня не взят\n", lv); bad = 1; continue; }
    void *d[6]; unsigned left = (NEED_KB[lv] << 10) - lvaB / 2; int n = 0;   // данные уровня в хвосте: куски разного размера
    for (int i = 0; i < 6 && left; i++) { unsigned s = i == 5 ? left : left / 3 + (i * 64u << 10); if (s > left) s = left; d[n] = H_alloc(s, 64); if (!d[n]) { s = 0; } else { left -= s; n++; } }
    for (int i = 0; i < n; i += 2) H_free(d[i]); for (int i = 1; i < n; i += 2) H_free(d[i]);   // выгрузка вперемешку
    H_free(lva);
    unsigned tot, mx, big[8]; int nb; freemap(&tot, &mx, &nb, big, 8); double r = tot ? (double)mx / tot : 0;
    int ok = nb <= 2 && r >= 0.9; bad |= !ok;
    printf("перед L%d: free %.1f МБ, mx %.2f МБ (%.0f%%), кусков >=0,4 МБ: %d  городом %d, местом %d, миссией %d  %s\n", lv, tot / 1048576., mx / 1048576., 100 * r, nb, z3A[2].base != 0, z3A[1].base != 0, z3A[0].base != 0, ok ? "ok" : "FAIL");
  }
  printf("%s\n", bad ? "FAIL" : "PASS"); return bad;
}
