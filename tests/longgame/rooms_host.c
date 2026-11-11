// longgame: двери комнат и будки уровня на хосте. rooms_host LEVEL PLAYER -> room N door ok x y z exit x y z; phone N x y z
#include <stdio.h>
#include "world.h"
#include "col.h"
int main(int argc, char **argv) {
  if (argc < 3 || bwData(argv[1], argv[2], 0)) return 2; { float a, b, c, d; bwSpawn(&a, &b, &c, &d); }
  int n = bwRooms(); printf("rooms %d\n", n);
  for (int i = 0; i < n; i++) { float d[3] = {0, 0, 0}, e[3] = {0, 0, 0}; int ok = bwRoomDoor(i, d); bwRoomExit(i, e);
    printf("room %d door %d %.2f %.2f %.2f exit %.2f %.2f %.2f\n", i, ok, d[0], d[1], d[2], e[0], e[1], e[2]); }
  const ColWorld *w = bwCol(); printf("phones %u\n", w->nphone);
  for (unsigned i = 0; i < w->nphone && i < 4; i++) printf("phone %u %.1f %.1f %.1f\n", i, w->phone[i].p[0], w->phone[i].p[1], w->phone[i].p[2]);
  return 0;
}
