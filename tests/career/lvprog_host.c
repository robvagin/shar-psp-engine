// Облако 05.10 (карьера): прогресс миссий всех 7 уровней в хвосте записи (viewer/savegame.h ext SV_EXT_LV) — без диска игры.
//   round   — уровни 1 и 2 записаны (маска, строки MISSION SELECT), запись -> чтение: те же; старший уровень 2
//   head    — голова (msnLevel/msnCompleted) остаётся главной для своего уровня
//   old     — запись до 05.10 (ext пуст), уровень 3: L1, L2 «пройдены» (svLvGet 1, маска 0xFFFFFFFF), L3 — маска головы, L4 — нет
//   compat  — голова (байты 0..255) не зависит от поля ext: прежние сборки читают запись как свою; хвост сходится (CRC)
//   keep    — svLvKeep переносит поле всех уровней (перенос между уровнями, main.c saveCapture / levelAdvance)
//   flow    — карьера: L1 пройден -> L2 (две миссии) -> назад на L1 (MISSION SELECT) -> запись -> чтение: L2 не потерян
// Красный (tests/career/lvprog.sh): -DSV_BROKEN_LV=1 — прогресс уровней не пишется (как до 05.10): round/flow обязаны упасть
#include <stdio.h>
#include <string.h>
#include "savegame.h"
#include "collect.h"
#include "mission/msn.h"
// заглушки модулей мира (svCapture/svApply/svOnce* здесь не зовутся)
unsigned collectCards(const Collect *c, int L) { (void)c; (void)L; return 0; }
uint32_t collectWasps(const Collect *c, int L) { (void)c; (void)L; return 0; }
uint64_t collectGags(const Collect *c, int L) { (void)c; (void)L; return 0; }
void collectSetProgress(Collect *c, int L, unsigned a, uint32_t b, uint64_t g) { (void)c; (void)L; (void)a; (void)b; (void)g; }
int spOnce(const StateProps *sp, unsigned i) { (void)sp; (void)i; return 0; }
void spRestore(StateProps *sp, unsigned i, int st) { (void)sp; (void)i; (void)st; }
static int fails;
static void check(const char *n, int ok, const char *d) { printf("CHECK %-6s %s  %s\n", n, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
static int rt(const SaveGame *a, SaveGame *b) { uint8_t buf[SV_BYTES]; return svWrite(a, buf) == SV_BYTES && svRead(buf, SV_BYTES, b) == SV_OK; }
int main(void) {
  char d[200]; SaveGame a, b; uint32_t m; unsigned r; int g;
  svClear(&a); a.level = a.msnLevel = 2; a.msnCompleted = 0x3;
  svLvPut(&a, 1, 0x1ff, SV_LV_ROWS_OK | 0x7f); svLvPut(&a, 2, 0x3, SV_LV_ROWS_OK | 0x01);
  int ok = rt(&a, &b); g = svLvGet(&b, 1, &m, &r);
  snprintf(d, sizeof d, "чтение %d; L1: %d маска %x строки %x (нужно 2 1ff ff); старший %d (2)", ok, g, (unsigned)m, r, svLvReached(&b));
  check("round", ok && g == 2 && m == 0x1ff && r == 0xff && svLvReached(&b) == 2, d);
  b.msnCompleted = 0x7; g = svLvGet(&b, 2, &m, &r);
  snprintf(d, sizeof d, "L2 = уровень головы: %d маска %x (нужно 2 7 — голова), строки %x (81)", g, (unsigned)m, r);
  check("head", g == 2 && m == 0x7 && r == 0x81, d);
  svClear(&a); a.level = a.msnLevel = 3; a.msnCompleted = 0x5;
  ok = rt(&a, &b); int g1 = svLvGet(&b, 1, &m, &r); uint32_t m1 = m; int g3 = svLvGet(&b, 3, &m, NULL); uint32_t m3 = m; int g4 = svLvGet(&b, 4, NULL, NULL);
  snprintf(d, sizeof d, "старая запись L3: L1 %d (%x), L3 %d (%x), L4 %d — нужно 1 (ffffffff), 2 (5), 0", g1, (unsigned)m1, g3, (unsigned)m3, g4);
  check("old", ok && g1 == 1 && m1 == 0xFFFFFFFFu && g3 == 2 && m3 == 5 && g4 == 0, d);
  { // №1114 (формат SHV3, старые записи не читаются): запись с полем уровня L1 и без него читается; без поля L1 «пройден, маска неизвестна» (1), с полем — записанная маска (2)
    uint8_t x[SV_BYTES], y[SV_BYTES]; svClear(&a); a.level = a.msnLevel = 2; svWrite(&a, x); svLvPut(&a, 1, 0xff, 0xff); svWrite(&a, y);
    SaveGame cx, cy; int rx = svRead(x, SV_BYTES, &cx) == SV_OK, ry = svRead(y, SV_BYTES, &cy) == SV_OK; uint32_t mx, my; svLvGet(&cx, 1, &mx, NULL); svLvGet(&cy, 1, &my, NULL);
    int gx = svLvGet(&cx, 1, NULL, NULL), gy = svLvGet(&cy, 1, NULL, NULL);
    snprintf(d, sizeof d, "без поля L1: чтение %d, статус %d (1); с полем: чтение %d, статус %d (2), маска %x (ff)", rx, gx, ry, gy, (unsigned)my);
    check("compat", rx && ry && gx == 1 && mx == 0xFFFFFFFFu && gy == 2 && my == 0xff, d); }
  svClear(&a); svLvPut(&a, 4, 0xabc, 0x83); svClear(&b); svLvKeep(&b, &a); g = svLvGet(&b, 4, &m, &r);
  snprintf(d, sizeof d, "перенос: L4 %d %x %x (нужно 2 abc 83)", g, (unsigned)m, r);
  check("keep", g == 2 && m == 0xabc && r == 0x83, d);
  { // как main.c: saveCapture = svLvKeep(прогресс игры) + svLvPut(текущий); levelAdvance — голова на новый уровень, маска его — из поля
    SaveGame keep; svClear(&keep);
    SaveGame s; svClear(&s); s.level = s.msnLevel = 1; s.msnCompleted = 0x1ff; svLvKeep(&s, &keep); svLvPut(&s, 1, s.msnCompleted, 0xff);   // L1 пройден
    keep = s; s.level = s.msnLevel = 2; s.msnCompleted = 0;                                                                            // -> L2
    s.msnCompleted = 0x3; svLvKeep(&s, &keep); svLvPut(&s, 2, s.msnCompleted, 0x81);                                                // L2: две миссии
    keep = s; uint32_t back = 0; int gb = svLvGet(&s, 1, &back, NULL); s.level = s.msnLevel = 1; s.msnCompleted = gb ? back : 0;    // MISSION SELECT -> L1
    svLvKeep(&s, &keep); svLvPut(&s, 1, s.msnCompleted, 0xff);
    SaveGame t; ok = rt(&s, &t); uint32_t l2 = 0; int g2 = svLvGet(&t, 2, &l2, &r);
    snprintf(d, sizeof d, "L1 -> L2 (2 миссии) -> L1, запись -> чтение: L1 маска %x (1ff), L2 %d маска %x строки %x (нужно 2 3 81), старший %d (2)",
             (unsigned)t.msnCompleted, g2, (unsigned)l2, r, svLvReached(&t));
    check("flow", ok && t.msnCompleted == 0x1ff && g2 == 2 && l2 == 3 && r == 0x81 && svLvReached(&t) == 2, d); }
  printf("RESULT %s провалов %d\n", fails ? "FAIL" : "PASS", fails);
  return fails ? 1 : 0;
}
