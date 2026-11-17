// Правка Ф2-5 (Стена 107): предел трафика на этапах L1 m3/m5 после вступления (m3sd/m5sd ставят SetMaxTraffic(2)): оригинал держит его, у нас этап без команды возвращал 5.
// Вступление m5 запускается (SetMaxTraffic 2 на первом этапе), потом основная фаза (restart с чекпоинта, как повтор после провала); печатает roadsT->maxCars по этапам.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "world.h"
#include "traffic.h"
static BotWorld W;
static int cap(void) { const Traffic *T = bwTraffic(); return T ? T->maxCars : -1; }
int main(int argc, char **argv) {
  if (argc < 5 || bwData(argv[1], argv[2], argv[3])) return 2;
  int mi = atoi(argv[4]);
  bwNew(&W); GameMsn *M = &W.msn; printf("start %d\n", cap());
  msn_start_mission(&M->rt, mi, 0); { int pc = -1; for (int f = 0; f < 120; f++) { bwStep(&W, 0, 0, 0); if (cap() != pc) { pc = cap(); printf("  intro f=%d stage %d.%d cap=%d\n", f, M->rt.phase, M->rt.stage, pc); } } }
  int sd = cap(); printf("intro stage %d.%d cap=%d\n", M->rt.phase, M->rt.stage, sd);
  M->rt.cp_phase = 1; M->rt.cp_stage = 0; msn_start_mission(&M->rt, mi, 1); int lastSt = -9, mn = 99, mx = -1;
  for (int f = 0; f < 60 * 6; f++) { bwStep(&W, 0, 0, 0); if (M->rt.stage != lastSt) { lastSt = M->rt.stage; printf("main stage %d.%d cap=%d\n", M->rt.phase, M->rt.stage, cap()); } int c = cap(); if (c < mn) mn = c; if (c > mx) mx = c; }
  printf("RESULT intro_cap=%d main_min=%d main_max=%d\n", sd, mn, mx);
  return 0; }
