// Правка A2.7 (Стена 107): конец пройденной миссии, которая заканчивается в свободной езде (L1 sr2, уличная гонка): машины гонки не пропадают мгновенно в кадре уборки (уходят через очередь
// вне кадра и дальше 5 м); красный -DMC_ENDLEAVE_OLD=1 (mcClear). Этапы проходятся служебным пропуском; игрок стоит рядом с первой машиной, чтобы очередь не успела её снять.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
static int nCars(void) { int n = 0; for (int i = 0; i < W.msn.cars.n; i++) if (W.msn.cars.c[i].on) n++; return n; }
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) return 2;
  bwNew(&W); GameMsn *M = &W.msn; msn_start_bonus(&M->rt, 9);
  for (int f = 0; f < 60 * 60 && !(M->rt.phase == 1 && nCars()); f++) { bwStep(&W, 0, 0, 0); if (M->rt.phase == 0 && f % 15 == 14) msn_service_pass(&M->rt); }
  for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  int before = nCars(); if (!before) { printf("RESULT no_cars\n"); return 1; }
  const McCar *c0 = 0; for (int i = 0; i < M->cars.n; i++) if (M->cars.c[i].on) { c0 = &M->cars.c[i]; break; }
  int cl = 0;
  for (int f = 0; f < 60 * 200; f++) {
    for (int i = 0; i < M->cars.n; i++) if (M->cars.c[i].on) { c0 = &M->cars.c[i]; break; }
    if (c0 && c0->on) bwPlace(&W, c0->car.x + 8.f, c0->car.y + .6f, c0->car.z, 0.f);
    bwStep(&W, 0, 0, 0); if (f % 15 == 14) msn_service_pass(&M->rt);
    if (M->rt.status == MSN_MISSION_DONE || msn_free_roam(&M->rt)) { cl = f; break; } }
  int atEnd = nCars(); int freeNow = msn_free_roam(&M->rt);
  for (int f = 0; f < 60 * 6; f++) { for (int i = 0; i < M->cars.n; i++) if (M->cars.c[i].on) { c0 = &M->cars.c[i]; break; } if (c0 && c0->on) bwPlace(&W, c0->car.x + 8.f, c0->car.y + .6f, c0->car.z, 0.f); bwStep(&W, 0, 0, 0); }
  int after = nCars(); int lq = 0; for (int i = 0; i < M->cars.n; i++) if (M->cars.c[i].on && M->cars.c[i].leaveQ) lq++;
  printf("cars before=%d at_done=%d after_cleanup=%d queued=%d free_roam=%d status=%d frame=%d\n", before, atEnd, after, lq, freeNow, M->rt.status, cl);
  printf("RESULT cars_after_end=%d\n", after); return 0; }
