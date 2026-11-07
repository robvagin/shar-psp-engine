// Аудит полноты п.10 (g04-homer): радиус цели goto/gooutside берётся из данных этапа (объём триггера локатора), а не один на всех (6,5 м машина / 2,6 м пешком).
// Проверка по данным всех этапов уровня: у каждой цели есть свой объём, точка у края внутри — цель засчитана, точка вне объёма — нет (а при «одном радиусе» результат другой).
// Красный контроль: -DMSN_RADIUS_OLD=1 (один радиус на всех). Печатает таблицу: уровень, этап, локатор, объём, до (один радиус) и после (объём).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../viewer/mission/msn_run.c"
static int fails, rows;
static int hit(MsnRuntime *rt, const MsnStage *st, const MsnCmd *d, float x, float y, float z, int foot) {
  MsnWorld w; memset(&w, 0, sizeof w); w.player.x = x; w.player.y = y; w.player.z = z; w.on_foot = foot; w.action = 1;   // кнопка действия нажата: проверяем только радиус (этапы MUSTACTION ждут её)
  rt->st = st; rt->c_dest = d; return check_objective(rt, &w, 0.f);
}
int main(int ac, char **av) {
  for (int a = 1; a < ac; a++) {
    FILE *f = fopen(av[a], "rb"); if (!f) { printf("нет %s\n", av[a]); return 2; } fseek(f, 0, SEEK_END); size_t n = ftell(f); rewind(f);
    void *b = aligned_alloc(16, (n + 15) & ~15ul); if (fread(b, 1, n, f) != n) return 2; fclose(f);
    static MsnFile mf; char err[128]; if (msn_open(&mf, b, n, err, sizeof err)) { printf("open %s: %s\n", av[a], err); return 2; }
    static MsnRuntime rt; msn_init(&rt, &mf, NULL);
    int lvl = mf.hdr->level, cnt = 0;
    for (uint32_t si = 0; si < mf.nstages; si++) {
      const MsnStage *st = &mf.stages[si]; if (st->obj != MSN_OBJ_GOTO && st->obj != MSN_OBJ_GOOUTSIDE) continue;
      const MsnCmd *d = 0; for (unsigned i = 0; i < st->cmd_count; i++) { const MsnCmd *c = &mf.cmds[st->cmd_first + i]; if (c->op == MSN_OP_SETDESTINATION) { d = c; break; } }
      const MsnLoc *l = d ? msn_arg_loc(&mf, d, 0) : 0; rows++; cnt++;
      if (!l || l->ntrig == 0) { printf("FAIL L%d этап %u: у цели нет объёма триггера (%s)\n", lvl, si, d ? msn_arg_s(&mf, d, 0) : "-"); fails++; continue; }
      if (d->argc > 2 && msn_arg_f(&mf, d, 2) != 0.f) { printf("FAIL L%d этап %u: у цели есть масштаб (аргумент 3), переопределение радиуса не реализовано\n", lvl, si); fails++; }
      const MsnTrig *t = &mf.trigs[l->first_trig]; float ext = t->kind ? fminf(t->half[0], fminf(t->half[1], t->half[2])) : t->half[0];
      float cx = t->center[0], cy = t->center[1], cz = t->center[2];
      int c0 = hit(&rt, st, d, cx, cy, cz, 0);                                         // центр: засчитано (машиной)
      int in = hit(&rt, st, d, cx + (t->kind ? 0.5f * t->half[0] * t->axis[0][0] : 0.5f * t->half[0]), cy + (t->kind ? 0.5f * t->half[0] * t->axis[0][1] : 0), cz + (t->kind ? 0.5f * t->half[0] * t->axis[0][2] : 0), 0);   // 0,5 полуразмера вдоль первой оси — внутри
      float far = t->kind ? 400.f : t->half[0] * 2.2f;   // вне объёма: у сферы 2,2 радиуса (при одном радиусе 6,5 м машины такая точка внутри), у ящика далеко вдоль его оси (у цели могут быть ещё объёмы)
      float ox = t->kind ? t->axis[0][0] : 1.f, oy = t->kind ? t->axis[0][1] : 0.f, oz = t->kind ? t->axis[0][2] : 0.f;
      int out = hit(&rt, st, d, cx + ox * far, cy + oy * far, cz + oz * far, 0);
      int ok = c0 && in && !out;
      printf("%s L%d этап %3u %-30s %s %.2f %.2f %.2f | до: один радиус 6,5/2,6 | после: центр=%d внутри=%d вне=%d\n", ok ? "ok  " : "FAIL", lvl, si, msn_arg_s(&mf, d, 0), t->kind ? "box" : "sph", t->half[0], t->half[1], t->half[2], c0, in, out);
      (void)ext; if (!ok) fails++;
    }
    printf("L%d: целей %d\n", lvl, cnt);
  }
  printf("%s: целей %d, провалов %d\n", MSN_RADIUS_OLD ? "old" : "new", rows, fails); return fails ? 1 : 0;
}
