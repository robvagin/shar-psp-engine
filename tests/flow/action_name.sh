#!/bin/bash
# Подпись △ «Talk To <Имя>»: имя как в тексте задания (кадры Механика 09.11: было «NRIVIERA», «BURNS», «FRINK», «GRANDPA», «UNF»). Данных не нужно.
# Зелёный: имена по таблице. Красный: -DACTIONNAME_OLD=1 (имя из кода данных) должен упасть.
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"; mkdir -p build/flow
cat > build/flow/action_name.c <<'C'
#include <stdio.h>
#include <string.h>
#include "flowrules.h"
int main(void) {
  static const char *const W[][2] = { {"nriviera", "Dr. Nick"}, {"burns", "Mr. Burns"}, {"brn_unf", "Mr. Burns"}, {"frink", "Professor Frink"}, {"grandpa", "Grampa"}, {"cbg", "Comic Book Guy"},
    {"wiggum", "Chief Wiggum"}, {"m3_nriviera", "Dr. Nick"}, {"marge", "Marge"}, {"otto", "Otto"}, {"ned", "Ned"}, {"m4_gil", "Gil"}, {"moleman", "Hans Moleman"} };
  int bad = 0; char b[40];
  for (unsigned i = 0; i < sizeof W / sizeof *W; i++) { const char *g = flowTalkName(W[i][0], b, sizeof b);
    int ok = !strcmp(g, W[i][1]); if (!ok) bad++; printf("%-12s -> %-16s (ждём %s) %s\n", W[i][0], g, W[i][1], ok ? "OK" : "FAIL"); }
  return bad ? 1 : 0; }
C
cc -std=c11 -Wall -I viewer build/flow/action_name.c -o build/flow/an_green && cc -std=c11 -DACTIONNAME_OLD=1 -I viewer build/flow/action_name.c -o build/flow/an_red || exit 1
build/flow/an_green || { echo FAIL green; exit 1; }
build/flow/an_red >/dev/null && { echo "КРАСНЫЙ НЕ ПОЙМАН"; exit 1; } || echo "красный ACTIONNAME_OLD упал, как должен"
echo PASS
