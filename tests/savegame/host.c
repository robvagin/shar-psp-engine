// Гейт сохранений на Маке (tests/savegame/host.sh): формат записи (viewer/savegame.c) и перенос прогресса игры через запись
// настоящими модулями движка (мир ботов tools/bots/world.c: миссии level01.msn, карточки/осы/гэги collect-l1.shr, монеты).
//   host LEVEL.shr HOMER.shr level01.msn [--nowrite]
// --nowrite — отрицательный контроль «сборка без записи»: запись не попадает в файл, после «перезапуска» прогресс слетает,
// гейт обязан упасть. Сборка с -DSV_BROKEN_CRC (без сверки CRC) обязана провалить проверку испорченного файла.
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#include "savegame.h"
#include "mission/msn.h"

static int fails;
static void check(const char *name, int ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void check(const char *name, int ok, const char *fmt, ...) {
  char d[300]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
  printf("CHECK %-28s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++;
}
static unsigned char *readAll(const char *fn, size_t *n) {
  FILE *f = fopen(fn, "rb"); if (!f) return 0;
  fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = aligned_alloc(16, (*n + 15) & ~(size_t)15); if (b && fread(b, 1, *n, f) != *n) { free(b); b = 0; }
  fclose(f); return b;
}
static unsigned rng = 1234567u;
static unsigned rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

// прогресс без того, что меняет загрузка по правилам игры (сюжетная миссия и её чекпоинт идут отдельной проверкой)
static void prog(const SaveGame *s, char *out, int n) {
  char t[300]; svProgressText(s, t, sizeof t);
  char *a = strstr(t, " story="), *b = strstr(t, " cards="); if (a && b) memmove(a, b, strlen(b) + 1);
  snprintf(out, n, "%s", t);
}

// правка 334: разовый предмет (stateprops.h spOnce) с первым событием удара в s0 -> событие; -1 нет
static int onceHit(StateProps *sp, unsigned i) {
  static const int ev[] = {SP_EV_VEHICLE_HIT, SP_EV_DESTROYED, SP_EV_HIT};
  for (int k = 0; k < 3; k++) {
    const SpType *T = &sp->type[sp->inst[i].type]; const SpState *S = &sp->state[T->firstState + sp->run[i].state];
    for (unsigned e = 0; e < S->nEvent; e++) if (sp->event[S->firstEvent + e].event == ev[k]) return ev[k];
  }
  return -1;
}
static int neverSeen(void *u, float x, float y, float z, float r) { (void)u; (void)x; (void)y; (void)z; (void)r; return 0; }
static void farAway(StateProps *sp, float secs) { for (float t = 0.f; t < secs; t += .05f) spRespawnStep(sp, 1e4f, 1e4f, .05f, neverSeen, 0); }
// осы: игрок стоит у точки осы k (пешком, город) n кадров; -> появлений осы k
static int waspNear(Collect *c, int k, int frames) {
  const CollWasp *w = &c->wasp[k]; CollPlayer p = {w->x, w->y - 1.f, w->z, 1, -1, 0}; int sp = 0; CollEvent e;
  for (int f = 0; f < frames; f++) { collectStep(c, &p, NULL, NULL, NULL, NULL, 1.f / 60.f); while (collectPop(c, &e)) sp += e.type == CE_WASP_SPAWN && e.idx == k; }
  return sp;
}
static void waspFar(Collect *c, int frames) {
  CollPlayer p = {1e4f, 0.f, 1e4f, 1, -1, 0}; CollEvent e;
  for (int f = 0; f < frames; f++) { collectStep(c, &p, NULL, NULL, NULL, NULL, 1.f / 60.f); while (collectPop(c, &e)) {} }
}

int main(int argc, char **argv) {
  if (argc < 4) { fprintf(stderr, "host LEVEL.shr HOMER.shr level01.msn [--nowrite]\n"); return 2; }
  int nowrite = argc > 4 && !strcmp(argv[4], "--nowrite");
  // ================================================= формат: запись -> чтение, порча, обрезка, границы
  SaveGame a, b; uint8_t buf[SV_BYTES], bad[SV_BYTES];
  svClear(&a); a.seq = 7; a.year = 2026; a.month = 9; a.day = 28; a.hour = 21; a.minute = 40; a.level = 1;
  a.msnLevel = 1; a.msnStory = 2; a.msnPhase = 1; a.msnStage = 3; a.msnCompleted = 0x13;
  for (int i = 1; i < SV_LEVELS; i++) { a.cards[i] = (uint8_t)rnd(); a.wasps[i] = rnd(); a.gags[i] = ((uint64_t)rnd() << 32) | rnd(); }
  a.coins = 1234; a.nworld = 207; for (unsigned i = 0; i < sizeof a.worldCoins; i++) a.worldCoins[i] = (uint8_t)rnd();
  a.music = 300; a.effects = 900; a.vehicle = 1000; a.voice = 0; a.radar = 0; a.owned[1] = 0x5; a.owned[3] = 0x2;   // v2: купленное
  a.skin = 2;                                                  // правка 107: надет второй костюм (h_fat)
  a.once[1].n = 52; a.once[1].sig = 0xBEEF; a.once[1].st[15] = 0x40; a.once[7].n = 44; a.once[7].st[0] = 0x31;   // правка 334: хвост
  // №1114: поля, что живут в ext (настройки и т. п.), заданы значениями, которые формат SHV3 хранит явно
  a.ext[SV_EXT_CAM] = SV_EXT_CAM_SET | SV_EXT_CAM_INV; a.ext[SV_EXT_CHASE] = SV_EXT_CHASE_SET | 2; a.ext[SV_EXT_OPTS] = SV_EXT_OPTS_SET | SV_EXT_OPTS_JUMP;
  a.ext[SV_EXT_TICKET] = SV_EXT_TICKET_SET | 5; a.ext[SV_EXT_MOVIES] = SV_EXT_MOVIES_SET | 1; a.ext[SV_EXT_MOVIES + 1] = 0xA5; a.ext[SV_EXT_MOVIES + 2] = 0x3C; a.ext[SV_EXT_MOVIES + 3] = 0x01;
  a.ext[83] = 0x80 | 37;                                       // секунды записи
  svLvPut(&a, 1, 0x13, SV_LV_ROWS_OK | 0x05); svLvPut(&a, 2, 0x7f, SV_LV_ROWS_OK | 0x7f); svLvPut(&a, 3, 0x0b, SV_LV_ROWS_OK | 0x03);
  { uint16_t h[3] = {0x1234, 0xBEEF, 7}; float f[3] = {.4f, 0.f, 1.f}; svCarHealthPut(&a, 3, h, f); }
  a.mrec[1][0].best = 245; a.mrec[1][0].attempts = 3; a.mrec[1][8].best = 90; a.mrec[1][12].best = 118; a.mrec[1][12].attempts = 2; a.mrec[1][12].flags = 2;
  a.mrec[7][11].best = 359999; a.mrec[4][5].flags = 4; a.mrec[4][5].attempts = 65535;
  int w = svWrite(&a, buf);
  int r = svRead(buf, sizeof buf, &b);
  uint8_t buf2[SV_BYTES]; svWrite(&b, buf2);
  check("format.roundtrip", w == SV_BYTES && r == SV_OK && !memcmp(buf, buf2, SV_BYTES), "запись %d Б, чтение %d, байты %s", w, r,
        memcmp(buf, buf2, SV_BYTES) ? "разные" : "те же");
  check("format.signature_size", SV_BYTES == 1536 && !memcmp(buf, "SHV3", 4) && buf[4] == 3 && buf[5] == 0, "подпись %.4s, версия %d, размер %d", (char *)buf, buf[4], SV_BYTES);
  check("format.fields", r == SV_OK && b.seq == 7 && b.year == 2026 && b.month == 9 && b.day == 28 && b.hour == 21 && b.minute == 40 && b.coins == 1234 && b.nworld == 207 &&
        !memcmp(b.worldCoins, a.worldCoins, sizeof a.worldCoins) && !memcmp(b.cards, a.cards, sizeof a.cards) && !memcmp(b.wasps, a.wasps, sizeof a.wasps) &&
        !memcmp(b.gags, a.gags, sizeof a.gags) && !memcmp(b.owned, a.owned, sizeof a.owned) && b.skin == 2 && b.msnCompleted == 0x13 && b.msnStory == 2 && b.msnStage == 3,
        "время, монеты, карточки/осы/гэги, купленное, костюм, миссия");
  int caught = 0, total = 0;
  for (int i = 0; i < SV_BYTES; i++) for (int k = 0; k < 8; k++) {
    memcpy(bad, buf, SV_BYTES); bad[i] ^= (uint8_t)(1u << k); total++;
    if (svRead(bad, SV_BYTES, &b) == SV_DAMAGED) caught++;
  }
  check("format.bitflips", caught == total, "испорченных записей опознано %d из %d (один бит в любом месте)", caught, total);
  int trunc = 0; for (unsigned n = 0; n < SV_BYTES; n += 17) trunc += svRead(buf, n, &b) == SV_DAMAGED;
  memset(bad, 0, SV_BYTES); int zero = svRead(bad, SV_BYTES, &b) == SV_DAMAGED;
  for (int i = 0; i < SV_BYTES; i++) bad[i] = (uint8_t)rnd(); int junk = svRead(bad, SV_BYTES, &b) == SV_DAMAGED;
  check("format.truncated_zero_junk", trunc == (SV_BYTES + 16) / 17 && zero && junk, "обрезка %d/%d, нули %d, мусор %d", trunc, (SV_BYTES + 16) / 17, zero, junk);
  { SaveGame c = a; c.coins = -5; svWrite(&c, bad); int neg = svRead(bad, SV_BYTES, &b) == SV_DAMAGED;
    c = a; c.music = 5000; svWrite(&c, bad); int vol = svRead(bad, SV_BYTES, &b) == SV_DAMAGED;
    memcpy(bad, buf, SV_BYTES); bad[4] = 4; int ver = svRead(bad, SV_BYTES, &b) == SV_DAMAGED;   // чужая версия — отказ даже с верным CRC? CRC не покрывает байт 4: проверка версии сама
    c = a; c.mrec[1][0].best = 400000; svWrite(&c, bad); int bt = svRead(bad, SV_BYTES, &b) == SV_DAMAGED;
    check("format.bounds_version", neg && vol && ver && bt, "монеты <0: %d, громкость 5000: %d, чужая версия: %d, время 111 ч: %d", neg, vol, ver, bt); }
  // перевод прежних записей (дирижёр 03.11.2026): "SHSV" 256 и 768 Б читаются, поля переносятся, нового нет (лучшие времена пусты), запись ложится в SHV3; порча по-прежнему «Damaged»
  { uint8_t o2[SV_BYTES_OLD], o3[SV_BYTES_OLD], nb[SV_BYTES], nb2[SV_BYTES]; SaveGame q, q2;
    int w2 = svWriteVer(&a, o2, sizeof o2, 2), w3 = svWriteVer(&a, o3, sizeof o3, 3);
    int r2 = svRead(o2, SV_BYTES_V2, &q), r3 = svRead(o3, SV_BYTES_OLD, &q2);
    int head = r2 == SV_OK && q.seq == 7 && q.coins == 1234 && q.msnLevel == 1 && q.msnStory == 2 && q.msnStage == 3 && q.msnCompleted == 0x13 && q.owned[1] == 0x5 && q.skin == 2 &&
               !memcmp(q.cards, a.cards, sizeof a.cards) && !memcmp(q.gags, a.gags, sizeof a.gags) && q.music == 300 && q.voice == 0 && q.radar == 0;
    int full = r3 == SV_OK && q2.seq == 7 && q2.coins == 1234 && q2.msnStory == 2 && q2.once[1].n == 52 && q2.once[1].sig == 0xBEEF && q2.once[1].st[15] == 0x40 && q2.once[7].st[0] == 0x31 &&
               q2.ext[SV_EXT_CAM] == (SV_EXT_CAM_SET | SV_EXT_CAM_INV) && q2.ext[SV_EXT_TICKET] == (SV_EXT_TICKET_SET | 5) && q2.ext[83] == (0x80 | 37);
    int none = 1; for (int L = 1; L < SV_LEVELS; L++) for (int k = 0; k < SV_MREC_N; k++) none &= q2.mrec[L][k].best == 0 && q2.mrec[L][k].attempts == 0;
    int wr = svWrite(&q2, nb) == SV_BYTES && !memcmp(nb, "SHV3", 4) && svRead(nb, SV_BYTES, &q) == SV_OK && svWrite(&q, nb2) == SV_BYTES && !memcmp(nb, nb2, SV_BYTES);
    uint32_t m1 = 0; unsigned rw = 0; int lv = svLvGet(&q, 1, &m1, &rw) == 2 && m1 == 0x13 && svLvReached(&q) == 3 && svLvGet(&q, 2, &m1, &rw) == 2 && m1 == 0x7f;
    uint16_t hh[8]; float ff[8]; int cars = svCarHealthGet(&q, hh, ff) == 3 && hh[1] == 0xBEEF;
    check("format.old_converted", w2 == SV_BYTES_V2 && w3 == SV_BYTES_OLD && svIsOldFormat(o3, SV_BYTES_OLD) && head && full && none && wr && lv && cars,
          "256 Б: поля головы %d; 768 Б: хвост, настройки, секунды %d; лучших времён нет %d; запись в SHV3 и обратно те же байты %d; прогресс уровней %d; машины %d", head, full, none, wr, lv, cars);
    // порча прежней записи: любой один бит головы и тела хвоста (кроме запаса) — «Damaged»
    int cau = 0, tot = 0; for (int i = 0; i < SV_BYTES_OLD; i++) for (int k = 0; k < 8; k++) {
      uint8_t bo[SV_BYTES_OLD]; memcpy(bo, o3, sizeof bo); bo[i] ^= (uint8_t)(1u << k); if (i >= 8 && i < 12) continue; tot++;
      if (svRead(bo, SV_BYTES_OLD, &q) == SV_DAMAGED) cau++; }
    uint8_t cut[SV_BYTES_OLD]; memcpy(cut, o3, sizeof cut); int ob = svRead(cut, 300, &q) == SV_DAMAGED && svRead(cut, 0, &q) == SV_DAMAGED;
    check("format.old_damaged", cau >= tot - 16 && ob, "порченых прежних записей опознано %d из %d (допуск 16 битов запаса/заголовка хвоста), оборванная — отказ %d", cau, tot, ob);
    // запись до 05.10 (без прогресса уровней в ext): «уровни ниже пройдены» переживает перевод
    SaveGame c = a; memset(c.ext, 0, sizeof c.ext); c.level = c.msnLevel = 3; c.msnCompleted = 5; uint8_t o4[SV_BYTES_V2]; svWriteVer(&c, o4, sizeof o4, 2);
    SaveGame p; int pr = svRead(o4, SV_BYTES_V2, &p) == SV_OK; int g0 = svLvGet(&p, 1, &m1, &rw); svWrite(&p, nb); SaveGame p2; int pr2 = svRead(nb, SV_BYTES, &p2) == SV_OK;
    int g1 = svLvGet(&p2, 1, &m1, &rw), g2 = svLvGet(&p2, 2, &m1, &rw), g3 = svLvGet(&p2, 3, &m1, &rw);
    uint32_t mlo = 0; svLvGet(&p2, 2, &mlo, NULL);
    check("format.old_pre0510_levels", pr && pr2 && g0 == 1 && g1 == 1 && g2 == 1 && g3 == 2 && m1 == 5 && mlo == 0xFFFFFFFFu && svLvReached(&p2) == 3, "запись до 05.10: уровни 1 и 2 «пройдены» до перевода (%d) и после (%d/%d, маска %x), 3 — маска головы", g0, g1, g2, (unsigned)mlo);
    size_t fn = 0; unsigned char *fx = readAll("tests/savegame/fixtures/save_v2_before334.bin", &fn);
    int rf = fx ? svRead(fx, (unsigned)fn, &q) : SV_DAMAGED;
    check("format.old_fixture", fx && rf == SV_OK && svWrite(&q, nb) == SV_BYTES && svRead(nb, SV_BYTES, &q2) == SV_OK && q2.seq == q.seq && q2.coins == q.coins && q2.msnCompleted == q.msnCompleted && !memcmp(q2.cards, q.cards, sizeof q.cards),
          "настоящий save00.bin прежней сборки (%u Б): читается и переходит в SHV3 с тем же прогрессом (seq %u, монеты %d)", (unsigned)fn, fx ? q.seq : 0, fx ? q.coins : 0);
    free(fx);
    // настоящие записи Роба (сейф сборок) — если лежат на этой машине: все поля прогресса те же после перевода и новой записи
    { const char *real[] = {"build/saves/2026-11-03_0118_before_TEST85/SHARVIEW/save00.bin", "build/saves/2026-11-03_0118_before_TEST85/SHARVIEW/save02.bin", "build/saves/2026-11-03_0118_before_TEST85/SHARVIEW/save03.bin"};
      int nr = 0, okr = 0; for (int i = 0; i < 3; i++) { size_t rn = 0; unsigned char *rb = readAll(real[i], &rn); if (!rb) continue; nr++;
        SaveGame o, t2; int ro = svRead(rb, (unsigned)rn, &o) == SV_OK; svWrite(&o, nb); int rt = svRead(nb, SV_BYTES, &t2) == SV_OK;
        okr += ro && rt && o.seq == t2.seq && o.coins == t2.coins && o.level == t2.level && o.msnLevel == t2.msnLevel && o.msnStory == t2.msnStory && o.msnStage == t2.msnStage && o.msnCompleted == t2.msnCompleted &&
               !memcmp(o.cards, t2.cards, sizeof o.cards) && !memcmp(o.wasps, t2.wasps, sizeof o.wasps) && !memcmp(o.gags, t2.gags, sizeof o.gags) && !memcmp(o.owned, t2.owned, sizeof o.owned) &&
               !memcmp(o.worldCoins, t2.worldCoins, sizeof o.worldCoins) && o.music == t2.music && o.radar == t2.radar && o.skin == t2.skin && !memcmp(o.once, t2.once, sizeof o.once) && svLvReached(&o) == svLvReached(&t2);
        free(rb); }
      check("format.old_real_rob", nr == 0 || okr == nr, "настоящие записи Роба из сейфа: %d из %d переведены с тем же прогрессом (нет файлов — пропуск)", okr, nr); }
  }
  // настройки и всё, что раньше жило в ext: пишется явно и возвращается в те же места памяти
  { SaveGame d; svRead(buf, sizeof buf, &d);
    check("format.settings", d.music == 300 && d.effects == 900 && d.vehicle == 1000 && d.voice == 0 && d.radar == 0 && d.ext[SV_EXT_CAM] == (SV_EXT_CAM_SET | SV_EXT_CAM_INV) &&
          d.ext[SV_EXT_CHASE] == (SV_EXT_CHASE_SET | 2) && d.ext[SV_EXT_OPTS] == (SV_EXT_OPTS_SET | SV_EXT_OPTS_JUMP) && d.ext[SV_EXT_TICKET] == (SV_EXT_TICKET_SET | 5) &&
          d.ext[SV_EXT_MOVIES] == (SV_EXT_MOVIES_SET | 1) && d.ext[SV_EXT_MOVIES + 1] == 0xA5 && d.ext[SV_EXT_MOVIES + 2] == 0x3C && d.ext[SV_EXT_MOVIES + 3] == 0x01 && d.ext[83] == (0x80 | 37),
          "звук, радар, камера, вид, прыжковые камеры, билет, ролики, подсказки, секунды записи");
    uint32_t m1, m2, m3; unsigned w1, w2, w3; int g1 = svLvGet(&d, 1, &m1, &w1), g2 = svLvGet(&d, 2, &m2, &w2), g3 = svLvGet(&d, 3, &m3, &w3), g4 = svLvGet(&d, 4, &m3, &w3);
    check("format.levels", g1 == 2 && g2 == 2 && g3 == 2 && g4 == 0 && m1 == 0x13 && m2 == 0x7f && w2 == (SV_LV_ROWS_OK | 0x7f) && m3 == 0 && svLvReached(&d) == 3 && svLvGet(&d, 3, &m3, &w3) == 2,
          "прогресс уровней 1..3 записан, 4 нет; старший уровень %d", svLvReached(&d));
    uint16_t hh[8]; float ff[8]; int nc = svCarHealthGet(&d, hh, ff);
    check("format.cars", nc == 3 && hh[0] == 0x1234 && hh[1] == 0xBEEF && hh[2] == 7 && fabsf(ff[0] - .4f) < .005f && ff[1] == 0.f && ff[2] == 1.f, "прочность личных машин: %d шт.", nc);
    check("format.best_times", d.mrec[1][0].best == 245 && d.mrec[1][0].attempts == 3 && d.mrec[1][8].best == 90 && d.mrec[1][12].best == 118 && d.mrec[1][12].attempts == 2 &&
          d.mrec[1][12].flags == 2 && d.mrec[7][11].best == 359999 && d.mrec[4][5].flags == 4 && d.mrec[4][5].attempts == 65535 && d.mrec[1][1].best == 0 && d.mrec[2][12].best == 0,
          "лучшие времена, попытки, флаги миссий; пустое — «нет»"); }
  { // живая таблица: меньше — лучше; рекорд один раз; загрузка заменяет таблицу
    svBestReset(); int n1 = svBestNote(2, 12, 130), n2 = svBestNote(2, 12, 140), n3 = svBestNote(2, 12, 120), n4 = svBestNote(2, 12, 120), n5 = svBestNote(9, 0, 5), n6 = svBestNote(2, 13, 5), n7 = svBestNote(2, 0, 0);
    svAttemptNote(2, 12); svAttemptNote(2, 12);
    check("best.live_rules", n1 == 1 && n2 == 0 && n3 == 1 && n4 == 0 && !n5 && !n6 && !n7 && svBestLive(2, 12) == 120 && svAttemptsLive(2, 12) == 2 && svBestLive(2, 11) == 0,
          "130 -> рекорд, 140 нет, 120 рекорд, 120 ещё раз нет; чужой уровень/слот/0 с — нет; лучшее %d, попыток %d", svBestLive(2, 12), svAttemptsLive(2, 12));
    SaveGame cap; svClear(&cap); svCapture(&cap, NULL, NULL, NULL); uint8_t bb[SV_BYTES]; svWrite(&cap, bb); SaveGame ld; svRead(bb, SV_BYTES, &ld);
    svBestReset(); svApply(&ld, NULL, NULL);
    check("best.capture_apply", svBestLive(2, 12) == 120 && svAttemptsLive(2, 12) == 2, "через запись: лучшее %d, попыток %d (ждём 120 и 2)", svBestLive(2, 12), svAttemptsLive(2, 12));
    SaveGame emp; svClear(&emp); svApply(&emp, NULL, NULL);
    check("best.load_replaces", svBestLive(2, 12) == 0 && svAttemptsLive(2, 12) == 0, "загрузка записи без времён очищает таблицу: %d/%d", svBestLive(2, 12), svAttemptsLive(2, 12)); }
  { char p1[300], p2[300]; SaveGame c = a; c.seq = 99; c.minute = 1; svProgressText(&a, p1, sizeof p1); svProgressText(&c, p2, sizeof p2);
    check("format.progress_text", !strcmp(p1, p2), "строка прогресса не зависит от номера и времени записи"); }

  // ================================================= игра: прогресс -> запись -> «выход» -> новая игра -> загрузка
  if (bwData(argv[1], argv[2], argv[3])) { printf("FAIL: данные мира\n"); return 1; }
  size_t msize; unsigned char *mpack = readAll(argv[3], &msize);
  static BotWorld W; bwNew(&W);
  check("game.world", W.haveMsn && W.haveColl && W.haveCoins, "миссии %d, карточки/осы/гэги %d, монеты %d (%u мира)", W.haveMsn, W.haveColl,
        W.haveCoins, W.coins.nworld);
  for (int f = 0; f < 120; f++) bwStep(&W, 0, 0.f, 0.f);   // 2 с игры
  // прогресс: m0 пройдена (отметка, как flag svmark), карточки 1 и 3, оса 2, гэг 0 — через API модулей; монеты
  W.msn.rt.completed |= 1u;
  collectSetProgress(&W.coll, W.coll.level, 0x5u, 0x4u, 0x1u);
  W.coins.collected = 57; if (W.coins.nworld > 10) { W.coins.c[3].active = 0; W.coins.c[10].active = 0; }
  shopGive(&W.msn.shop, "plowk_v");                      // v2: куплен Plow King (m3 «купи у Барни»)
  // правка 334: разовый предмет с монетами (первый по порядку экземпляров, у кого в s0 есть удар) разбит; монеты выпали (счётчик
  // стейтпропов sp.coins); фоновый возврат «вне вида» его не возвращает, повторный удар монет не даёт
  int oi = -1, oev = -1; unsigned onum = 0; svOnceSig(&W.sp, &onum);
  for (unsigned i = 0; i < W.sp.ninst && oi < 0; i++) if (spOnce(&W.sp, i) && (oev = onceHit(&W.sp, i)) >= 0) oi = (int)i;
  int c0 = W.sp.coins, oSt = -1, c1 = 0, c2 = 0, back = 0;
  if (oi >= 0) {
    spEvent(&W.sp, (unsigned)oi, oev); for (int f = 0; f < 180; f++) bwStep(&W, 0, 0.f, 0.f);
    c1 = W.sp.coins - c0; oSt = W.sp.run[oi].state;
    farAway(&W.sp, 30.f); back = W.sp.run[oi].state == 0;
    int e2 = onceHit(&W.sp, (unsigned)oi); if (e2 >= 0) spEvent(&W.sp, (unsigned)oi, e2);
    for (int f = 0; f < 180; f++) bwStep(&W, 0, 0.f, 0.f);
    c2 = W.sp.coins - c0 - c1;
  }
  check("game.once_in_game", oi >= 0 && c1 > 0 && !back && c2 == 0, "разовых на уровне %u; предмет %d: удар -> s%d, монет %d; 30 с вне вида: %s; "
        "повторный удар: монет %d", onum, oi, oSt, c1, back ? "ВЕРНУЛСЯ" : "разбит", c2);
  // осы в игре: оса 0 появилась у игрока, убита (DESTROYED) -> монеты один раз; ушёл, вернулся (2 раза) — не появляется снова
  int wsp0 = 0, wsp1 = 0, wc0 = W.coll.sp.coins, wc1 = 0, wc2 = 0, wst = -1;
  if (W.coll.nwasp) {
    wsp0 = waspNear(&W.coll, 0, 90); spEvent(&W.coll.sp, W.coll.wasp[0].inst, SP_EV_DESTROYED); waspNear(&W.coll, 0, 200);
    wc1 = W.coll.sp.coins - wc0; wst = W.coll.st.waspSt[0];
    for (int k = 0; k < 2; k++) { waspFar(&W.coll, 120); wsp1 += waspNear(&W.coll, 0, 200); }
    wc2 = W.coll.sp.coins - wc0 - wc1;
  }
  check("game.wasp_once_in_game", wsp0 == 1 && wst == 2 && wc1 > 0 && wsp1 == 0 && wc2 == 0, "оса 0: появилась %d раз, после удара состояние %d, монет %d; "
        "ушёл и вернулся 2 раза: появлений %d, монет ещё %d", wsp0, wst, wc1, wsp1, wc2);
  SaveGame s0; svClear(&s0); MsnProgress mp; msn_get_progress(&W.msn.rt, &mp);
  svCapture(&s0, &mp, &W.coll, &W.coins); s0.music = 300;
  if (!spOnceOld) svOnceCapture(&s0.once[1], &W.sp);      // правка 334 (main.c saveCapture); прежнее — не пишутся
  uint8_t file[SV_BYTES]; memset(file, 0, sizeof file);
  if (!nowrite) svWrite(&s0, file);                      // «карта памяти»: байты DATA.BIN
  else printf("NOWRITE: запись пропущена (отрицательный контроль)\n");
  bwFree(&W);                                            // выход из игры
  static BotWorld W2; bwNew(&W2);                        // запуск: новая игра (как после NEW GAME)
  SaveGame s1; int st = svRead(file, sizeof file, &s1);
  if (st == SV_OK) {                                     // LOAD GAME: как main.c menuStartGame (newGame -> saveApply, миссии — mvNewGameAt)
    MsnProgress rp; svMsnProgress(&s1, &rp); char err[160];
    gameMsnOpenAt(&W2.msn, &W2.G, mpack, msize, NULL, &rp, err, sizeof err);
    svApply(&s1, &W2.coll, &W2.coins);
    if (!spOnceOld) svOnceApply(&s1.once[1], &W2.sp);     // правка 334 (main.c saveApply)
  }
  SaveGame s2; svClear(&s2); MsnProgress mp2; msn_get_progress(&W2.msn.rt, &mp2);
  svCapture(&s2, &mp2, &W2.coll, &W2.coins); s2.music = st == SV_OK ? s1.music : 1000;
  if (!spOnceOld) svOnceCapture(&s2.once[1], &W2.sp);
  char p0[300], p2[300]; prog(&s0, p0, sizeof p0); prog(&s2, p2, sizeof p2);
  printf("  до:    %s\n  после: %s\n", p0, p2);
  check("game.load_read", st == SV_OK, "запись после перезапуска: %s", st == SV_OK ? "прочитана" : st == SV_EMPTY ? "пусто" : "испорчена");
  check("game.progress_same", !strcmp(p0, p2), "прогресс после выхода и загрузки %s", strcmp(p0, p2) ? "СЛЕТЕЛ" : "тот же");
  int nextOk = W2.msn.rt.cur >= 0 && W2.msn.rt.cur != 0 && (W2.msn.rt.completed & 1u);
  check("game.next_story", nextOk, "m0 пройдена -> после загрузки идёт миссия %d (не m0), маска %08x", W2.msn.rt.cur, (unsigned)W2.msn.rt.completed);
  int cardHidden = 1; for (unsigned i = 0; i < W2.coll.ncard; i++) { unsigned k = W2.coll.card[i].id % 10; if (k == 1 || k == 3) cardHidden &= W2.coll.st.cardGot[i] == 1; }
  check("game.cards_in_world", cardHidden, "взятые карточки 1 и 3 не висят в мире после загрузки");
  int coinsGone = W2.coins.nworld > 10 && !W2.coins.c[3].active && !W2.coins.c[10].active && W2.coins.c[4].active;
  check("game.world_coins", coinsGone, "собранные монеты мира 3 и 10 не лежат, 4 лежит");
  // правка 334: после загрузки разбитый предмет разбит (то же состояние, убран из мира), монеты при загрузке не выпали, удар — без
  // монет, вне вида не возвращается; подменённый отпечаток уровня (другая сборка) — запись уровня не применяется
  { int st2 = oi >= 0 ? W2.sp.run[oi].state : -1, al = oi >= 0 ? W2.sp.run[oi].alive : -1, l0 = W2.sp.coins;
    for (int f = 0; f < 180; f++) bwStep(&W2, 0, 0.f, 0.f);
    int l1 = W2.sp.coins; if (oi >= 0) { int e = onceHit(&W2.sp, (unsigned)oi); if (e >= 0) spEvent(&W2.sp, (unsigned)oi, e); farAway(&W2.sp, 30.f); }
    for (int f = 0; f < 180; f++) bwStep(&W2, 0, 0.f, 0.f);
    int l2 = W2.sp.coins, st3 = oi >= 0 ? W2.sp.run[oi].state : -1;
    check("game.once_after_load", oi >= 0 && st2 == oSt && st3 == oSt && l0 == 0 && l1 == 0 && l2 == 0,
          "предмет %d после загрузки: s%d (было s%d), в мире %d; монет при загрузке %d/%d, после удара и 30 с вне вида %d (s%d)", oi, st2, oSt, al, l0, l1, l2, st3);
    SaveGame t = s1; t.once[1].sig ^= 1; static BotWorld W4; bwNew(&W4);
    int mm = svOnceApply(&t.once[1], &W4.sp), intact = oi >= 0 && W4.sp.run[oi].state == 0 && W4.sp.run[oi].alive;
    check("game.once_other_build", mm == -1 && intact, "чужой отпечаток уровня: apply %d, предмет цел %d", mm, intact);
    bwFree(&W4);
    // осы после загрузки: убитые (маска записи: 2 и 0) не появляются у игрока
    int ws = W2.coll.nwasp > 2 ? waspNear(&W2.coll, 2, 200) + waspNear(&W2.coll, 0, 200) : -1;
    check("game.wasp_once_after_load", ws == 0 && W2.coll.st.waspSt[2] == 2 && W2.coll.st.waspSt[0] == 2, "убитые осы 0 и 2 после загрузки: появлений %d, состояния %d/%d",
          ws, W2.coll.st.waspSt[0], W2.coll.st.waspSt[2]); }
  check("game.owned_plowk", shopOwns(&W2.msn.shop, 1, "plowk_v") && !shopOwns(&W2.msn.shop, 1, "cVan"),
        "купленный Plow King принадлежит после загрузки (%d), фургон — нет (%d)", shopOwns(&W2.msn.shop, 1, "plowk_v"), shopOwns(&W2.msn.shop, 1, "cVan"));

  // ================================================= чекпоинт миссии и защита от чужой/испорченной записи
  { MsnProgress q = {1, 1, 1, 1, 2, {0}, 1u}; char err[160]; static BotWorld W3; bwNew(&W3);
    gameMsnOpenAt(&W3.msn, &W3.G, mpack, msize, NULL, &q, err, sizeof err);
    // сверка g07 (P14:16): запись оригинала хранит только уровень и номер миссии — загрузка начинает миссию заново с её начала (вступление, этап 0); точка повтора
    // (MSN_CP5_OLD) — этап-чекпоинт вступления, не этап из записи. Прежнее ожидание (с этапа 2 основной части) — красный контроль: сборка -DGAME_RESUME33_OLD=1
    check("resume.checkpoint", W3.msn.rt.cur == 1 && W3.msn.rt.phase == 0 && W3.msn.rt.stage <= 1, "m1 с начала миссии: миссия %d фаза %d этап %d",
          W3.msn.rt.cur, W3.msn.rt.phase, W3.msn.rt.stage);
    q.stage = 200; gameMsnOpenAt(&W3.msn, &W3.G, mpack, msize, NULL, &q, err, sizeof err);
    check("resume.bad_stage", W3.msn.rt.cur == 1 && W3.msn.rt.phase == 0 && W3.msn.rt.stage <= 1, "этап вне миссии -> старт m1 с начала (%d/%d/%d)",
          W3.msn.rt.cur, W3.msn.rt.phase, W3.msn.rt.stage);
    q.level = 2; q.stage = 2; gameMsnOpenAt(&W3.msn, &W3.G, mpack, msize, NULL, &q, err, sizeof err);
    check("resume.other_level", W3.msn.rt.cur == 0 && W3.msn.rt.completed == 0, "запись другого уровня -> новая игра уровня (m%d, маска %08x)",
          W3.msn.rt.cur, (unsigned)W3.msn.rt.completed);
    bwFree(&W3); }
  { // сверка g10-flow 02.11 (P01 Д1): прочность личных машин переживает запись -> файл -> чтение; запись без поля — «поля нет»
    SaveGame a, b; uint8_t buf[SV_BYTES]; svClear(&a); svClear(&b);
    uint16_t h[3] = {0x1234, 0xBEEF, 7}; float f[3] = {.4f, 0.f, 1.f};
    svCarHealthPut(&a, 3, h, f); int w = svWrite(&a, buf); int r = svRead(buf, SV_BYTES, &b);
    uint16_t h2[8]; float f2[8]; int n = svCarHealthGet(&b, h2, f2); uint16_t h3[8]; float f3[8];
    check("carhp.roundtrip", w > 0 && r == SV_OK && n == 3 && h2[0] == 0x1234 && h2[1] == 0xBEEF && h2[2] == 7 && fabsf(f2[0] - .4f) < .005f && f2[1] == 0.f && f2[2] == 1.f,
          "3 машины: n=%d h0=%04x f0=%.3f f1=%.3f f2=%.3f", n, h2[0], f2[0], f2[1], f2[2]);
    SaveGame c; svClear(&c); check("carhp.absent", svCarHealthGet(&c, h3, f3) == -1, "запись без поля — -1");
  }
  printf("RESULT %s %d\n", fails ? "FAIL" : "OK", fails);
  return fails ? 1 : 0;
}
