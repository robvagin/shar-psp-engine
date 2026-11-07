// радиус разговора в комнате (msn_talk_reach_w) против наименьшего расстояния, на которое Гомер подходит к Мо у стойки (tools/cartest/talkreach.c, число приходит аргументом).
// Печать: радиус в комнате и вне. Использование: host MIN_ПОДХОД_М
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mission/msn.h"
int main(int argc, char **argv) {
  float approach = argc > 1 ? atof(argv[1]) : 2.26f; MsnRuntime rt; memset(&rt, 0, sizeof rt); rt.talk_radius = 1.3f; MsnWorld in, out; memset(&in, 0, sizeof in); memset(&out, 0, sizeof out); in.interior = "Moes";
  float r_in = msn_talk_reach_w(&rt, NULL, &in), r_out = msn_talk_reach_w(&rt, NULL, &out);
  printf("радиус в комнате %.2f, вне комнаты %.2f; Гомер подходит к Мо не ближе %.2f: %s\n", r_in, r_out, approach, r_in > approach ? "разговор достижим" : "РАЗГОВОР НЕДОСТИЖИМ");
  if (r_out > 1.31f || r_out < 1.29f) { printf("FAIL: вне комнаты радиус изменился\n"); return 1; }
  return r_in > approach ? 0 : 1;
}
