// Правка 433 (Роб 06.11, L4): шкала Hit & Run упала до 0, а копы в кадре и ближе 110 м гнались дальше.
// Модель кадра повторяет порядок hnrPursuit (main_frontend.h): помеченное состояние = шкала 0, защёлка отпущена, полиция есть; в нём
// копы по старому правилу уходят только дальше 110 м или после 5 с вне кадра; по новому — ещё и после HNR_MARK_CAP_S.
// Сценарии: n копов, все ближе 110 м и в кадре весь прогон (коп на хвосте). Счёт: через сколько секунд погоня снята (-1 — не снята за 600 с).
#include "menu_rules.h"
#include <stdio.h>
static float run(int n, float heatAfter, int latchedAfter) {
  float markT = 0.f, outT[8] = {0}; int alive = n; const float dt = 1.f / 30.f;
  for (int f = 0; f < 600 * 30 && alive > 0; f++) {
    float t = f * dt; float heat = t < 3.f ? 1.f : heatAfter; int latched = t < 3.f ? 1 : latchedAfter;   // 3 с полная шкала, потом спад до heatAfter
    int marked = alive > 0 && !latched && heat <= 0.f;
    if (marked) {
      for (int i = 0; i < n; i++) outT[i] = 0.f;   // все в кадре: счёт вне кадра с нуля (mrHnrOutStep при visible)
      if (mrHnrMarkCap(&markT, 1, dt)) alive = 0;
    } else mrHnrMarkCap(&markT, 0, dt);
    if (alive == 0) return t;
  }
  return -1.f;
}
int main(void) {
  int fail = 0, ns[] = {1, 3, 6};
  for (int k = 0; k < 3; k++) {
    float r = run(ns[k], 0.f, 0);
    int ok = r > 3.f + HNR_MARK_CAP_S - .1f && r < 3.f + HNR_MARK_CAP_S + .2f;   // ожидание одно и то же; на прежнем правиле (HNRMARKCAP_OLD=1) оно обязано упасть
    printf("%s n=%d шкала 0: погоня снята через %.1f с после спада\n", ok ? "OK  " : "FAIL", ns[k], r < 0.f ? -1.f : r - 3.f); fail |= !ok;
    r = run(ns[k], .03f, 0);   // отрицательный контроль: шкала 3 % (не 0) — погоня обязана продолжаться
    ok = r < 0.f; printf("%s n=%d шкала 3%%: погоня продолжается (не снята)\n", ok ? "OK  " : "FAIL", ns[k]); fail |= !ok;
    r = run(ns[k], 0.f, 1);   // защёлка держит — не помеченное состояние
    ok = r < 0.f; printf("%s n=%d защёлка: погоня продолжается\n", ok ? "OK  " : "FAIL", ns[k]); fail |= !ok;
  }
  { float t = 0.f; int a = 0; for (int i = 0; i < 150; i++) a |= mrHnrMarkCap(&t, 1, 1.f / 30.f); a |= mrHnrMarkCap(&t, 0, 1.f / 30.f);   // 5 с помечена, шкала снова вверх
    int b = 0; for (int i = 0; i < 150; i++) b |= mrHnrMarkCap(&t, 1, 1.f / 30.f);
    int ok = !a && !b; printf("%s счёт с нуля, когда шкала снова ненулевая\n", ok ? "OK  " : "FAIL"); fail |= !ok; }
    // «никто не исчезает в кадре»: после предела копы бросают погоню и тормозят (8 м/с²), игрок уезжает 20 м/с; кадр игрока ≈ 70 м позади; снятие — прежнее правило (mrHnrOutStep 5 с вне кадра или дальше 110 м)
  for (int k = 0; k < 3; k++) {
    float gap = 30.f, vc = 20.f, vp = 20.f, outT = 0.f, tLimbo = -1.f, markT2 = 0.f; const float dt = 1.f / 30.f; int removed = 0, visAtRemove = -1; float tRem = -1.f;
    for (int f = 0; f < 600 * 30 && !removed; f++) {
      float t = f * dt; int limbo = tLimbo >= 0.f;
      if (!limbo && mrHnrMarkCap(&markT2, 1, dt)) tLimbo = t;
      if (t > 20.f) vp = 20.f; else vp = 20.f;   // игрок едет прямо
      if (limbo || HNRMARKCAP_OLD == 0 && tLimbo >= 0.f) vc = fmaxf(0.f, vc - 8.f * dt);
      if (limbo && vc == 0.f && t > 600.f) break;
      gap += (vp - vc) * dt; int vis = gap < 70.f;
      if (mrHnrOutStep(&outT, vis, dt) || gap > 110.f) { removed = 1; visAtRemove = vis; tRem = t; }
      if (ns[k] == 0) break;
    }
    int ok = removed && !visAtRemove && tLimbo > 0.f; printf("%s предел → тормоз → снят вне кадра (n=%d): предел %.1f с, снят %.1f с, в кадре в момент снятия: %d\n", ok ? "OK  " : "FAIL", ns[k], tLimbo, tRem, visAtRemove); fail |= !ok;
  }
  printf("RESULT %s\n", fail ? "FAIL" : "PASS"); return fail;
}
