// Цена сторожа и постановки на Маке (канон п.11–12), микрозамер: cost_host LEVEL PLAYER [N] -> строки C<TAB>имя<TAB>мкс на вызов. PSP = Мак × 5,8 (CPU, shar-psp-perf-truth)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "world.h"
#include "watchdog.h"
static BotWorld W;
static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }
typedef struct { float bb[6]; int state; } FCell;
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  int N = argc > 3 ? atoi(argv[3]) : 200000;
  if (bwData(argv[1], argv[2], 0)) return 2;
  bwNew(&W); wdSetLog(0);
  for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  const ColWorld *cw = bwCol();
  double t0, t1; volatile int sink = 0;
  // 1. кадр сторожа целиком (все виды), герой на ногах рядом с машиной
  t0 = now_us(); for (int i = 0; i < N; i++) wdStep(1.f / 30.f); t1 = now_us();
  printf("C\twdStep (все виды за кадр)\t%.3f\n", (t1 - t0) / N);
  // 2. шаг мира целиком с сторожем и без (разница = цена сторожа в составе кадра)
  int M = N / 20 > 2000 ? N / 20 : 2000;
  wdEnable(1); t0 = now_us(); for (int i = 0; i < M; i++) bwStep(&W, 0, 0, 0); t1 = now_us(); double on = (t1 - t0) / M;
  wdEnable(0); t0 = now_us(); for (int i = 0; i < M; i++) bwStep(&W, 0, 0, 0); t1 = now_us(); double off = (t1 - t0) / M; wdEnable(1);
  printf("C\tbwStep сторож вкл\t%.3f\nC\tbwStep сторож выкл\t%.3f\n", on, off);
  // 3. проверка опоры при постановке (colGround на точку) и полная постановка
  float x = W.homer.x, z = W.homer.z, y = W.homer.y, g;
  t0 = now_us(); for (int i = 0; i < N; i++) sink += colGround(cw, x + (i & 7) * .01f, z, y + 2.f, y - 100.f, &g, 0); t1 = now_us();
  printf("C\tcolGround (проверка опоры, 1 вызов)\t%.3f\n", (t1 - t0) / N);
  t0 = now_us(); for (int i = 0; i < N / 10; i++) homerPlace(&W.homer, cw, x + (i & 7) * .01f, y, z, 0.f); t1 = now_us();
  printf("C\thomerPlace (постановка)\t%.3f\n", (t1 - t0) / (N / 10));
  // 4. проверка сцены (main_loop.h sceneReadyMain): перебор 1138 клеток — на персонажа и на кадр ожидания
  enum { NC = 1138 }; static FCell cl[NC]; for (int i = 0; i < NC; i++) { cl[i].bb[0] = (i % 40) * 20.f; cl[i].bb[3] = cl[i].bb[0] + 20.f; cl[i].bb[2] = (i / 40) * 20.f; cl[i].bb[5] = cl[i].bb[2] + 20.f; cl[i].state = 2; }
  t0 = now_us(); for (int i = 0; i < N / 10; i++) { float px = 400.f + (i & 15), pz = 300.f; int ready = 1;
    for (unsigned k = 0; k < NC; k++) { const FCell *c = &cl[k]; if (px < c->bb[0] || px > c->bb[3] || pz < c->bb[2] || pz > c->bb[5]) continue; if (c->state != 2) ready = 0; } sink += ready; } t1 = now_us();
  printf("C\tsceneReady (1138 клеток, 1 точка)\t%.3f\n", (t1 - t0) / (N / 10));
  // 5. дно мира в кадре сторожа (P0.1 Опора-93): герой в комнате (свой мир), машина в городе; число полных проходов по треугольникам за 600 кадров.
  //    Прежний кеш на один мир: ~2 прохода за кадр (1200); новый: не больше 2 за всё время (город и комната по разу)
  { extern unsigned wdgFloorPasses; static ColWorld room; room = *cw; W.G.pHomerWorld = &room; W.homer.state = HS_FOOT; W.homer.grounded = 1;
    unsigned p0 = wdgFloorPasses; t0 = now_us(); for (int i = 0; i < 600; i++) wdStep(1.f / 30.f); t1 = now_us(); unsigned p1 = wdgFloorPasses - p0;
    printf("C\tdno мира: проходов за 600 кадров (герой в комнате, машина в городе)\t%u\nC\twdStep комната+город\t%.3f\n", p1, (t1 - t0) / 600); W.G.pHomerWorld = 0; }
  return sink == 12345;
}
