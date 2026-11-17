// 108 Б.10: ворота района кузовом четырёх углов (viewer/zonegate.h zoneGateInBody). Хост-гейт без данных:
//   cc -std=c99 -O2 -I viewer tests/zonegate/body_host.c -lm -o build/zonegate_body && build/zonegate_body   (красный: -DZG_BODY_BREAK)
#include <stdio.h>
#include "zonegate.h"
int main(void) {
  ZoneGate g; memset(&g, 0, sizeof g); g.half[0] = 1.f; g.half[1] = 5.f; g.half[2] = 6.f; g.ax[0] = 1.f; g.ax[4] = 1.f; g.ax[8] = 1.f; g.load = 2; g.unload = 1;   // объём x -1..1, y -5..5, z -6..6 с центром в начале
  int bad = 0;
  // 1) центр в объёме: срабатывает и точкой, и кузовом
  if (!zoneGateInBody(&g, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 2.f)) { puts("FAIL центр внутри"); bad++; }
  // 2) машина едет вдоль z к воротам сбоку x: центр на x = 3 (вне объёма и вне запаса 1 + 1,5), кузов полуширина 2, курс +z — боковые углы касаются x = 1: кузов касается, точка нет
  if (zoneGateIn(&g, 3.f, 0.f, 0.f, 1.5f)) { puts("FAIL подготовка: точка с запасом уже внутри"); bad++; }
  if (!zoneGateInBody(&g, 2.9f, 0.f, 0.f, 0.f, 1.f, 2.f, 2.f)) { puts("FAIL кузов боковым углом касается объёма, а не срабатывает"); bad++; }
  // 3) далеко: ни тем ни другим
  if (zoneGateInBody(&g, 20.f, 0.f, 0.f, 0.f, 1.f, 1.f, 2.f)) { puts("FAIL далеко, а сработало"); bad++; }
  // 4) нос в объёме при центре вне и вне запаса 1,5: машина по x к воротам, курс +x, полудлина 2: центр x = -2.9, нос x = -0.9 внутри (|p| ≤ half 1 → нужно ≥ -1)
  if (zoneGateIn(&g, -3.4f, 0.f, 0.f, 1.5f)) { puts("FAIL подготовка 4"); bad++; }
  if (!zoneGateInBody(&g, -2.9f, 0.f, 0.f, 1.f, 0.f, 1.f, 2.f)) { puts("FAIL носом касается объёма, не сработало"); bad++; }
  // 5) прежнее правило срабатывало РАНЬШЕ кузова, где кузов узкий и короткий: машина маленькая (hl 0,3), центр x = -2,2 (точка с запасом 1,5 внутри: |−2,2| ≤ 1+1,5), кузов не касается
  if (!zoneGateIn(&g, -2.2f, 0.f, 0.f, 1.5f)) { puts("FAIL подготовка 5"); bad++; }
  if (zoneGateInBody(&g, -2.2f, 0.f, 0.f, 1.f, 0.f, 0.3f, 0.3f)) { puts("FAIL кузов не касается, а сработало (раньше, как точка с запасом)"); bad++; }
  printf("%s\n", bad ? "FAIL" : "PASS"); return bad ? 1 : 0;
}
