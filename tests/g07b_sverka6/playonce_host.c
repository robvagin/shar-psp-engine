// Окно g07b-msnview, круг 6 (P05:18): объект PlayOnce — первое △ играет клип один раз вперёд и навсегда снимает триггер (настоящий viewer/animobj.c, объект собран в памяти).
// Соседнее ложное: игрок вышел из триггера и вернулся — △ больше ничего не даёт (раньше признак «сработал» затирался выходом из объёма и клип играл заново).
// Красный контроль: -DAO_PLAYONCE_OLD=1 (не обрабатывается) и -DPLAYONCE_LEAVE_OLD=1 (признак затирается выходом) обязаны упасть.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "col.h"
#include "props.h"
#include "stateprops.h"
#include "animobj.h"
unsigned homerSolidFrame;   // заглушка: props.c ждёт её у homer.c (в этом тесте Гомера нет)
static int fails, nchecks;
static void check(const char *name, int ok, const char *d) { nchecks++; printf("CHECK %-12s %s  %s\n", name, ok ? "OK  " : "FAIL", d); if (!ok) fails++; }
int main(void) {
  static AnimObjs A; static StateProps S; static AoObj O; static SpRun run[2]; char d[200];
  memset(&A, 0, sizeof A); memset(&S, 0, sizeof S); memset(&O, 0, sizeof O); memset(run, 0, sizeof run);
  O.action = AO_PLAYONCE; O.trig = 1; O.nfr = 61; O.fps = 30.f; O.h[0] = O.h[1] = O.h[2] = 2.f;   // сфера радиусом 2 м в начале координат, клип 60 кадров = 2 с
  A.obj = &O; A.nobj = 1; A.bA = A.bB = -1; S.run = run;
  float dt = .02f; int press;
  // игрок пешком в объёме: объект предлагает △
  aoStep(&A, &S, NULL, 0, 0, 0, 1, 0, dt);
  snprintf(d, sizeof d, "в объёме: объект предлагает △ (want=%d, ждём 0)", A.want); check("offer", aoWantsAction(&A) == 0, d);
  press = A.want; aoStep(&A, &S, NULL, 0, 0, 0, 1, press >= 0, dt);   // △ отдаётся объекту, который предложил его в прошлом кадре (так делает main_loop)
  snprintf(d, sizeof d, "△: клип пошёл (playing=%d, запусков %u, ждём 1 и 1)", A.run[0].playing, A.plays); check("press_plays", A.run[0].playing && A.plays == 1, d);
  for (int s = 0; s < 120; s++) aoStep(&A, &S, NULL, 0, 0, 0, 1, 0, dt);
  snprintf(d, sizeof d, "через 2,4 с клип доиграл и стоит на последнем кадре (playing=%d, кадр %.1f из 60)", A.run[0].playing, A.run[0].f); check("ends", !A.run[0].playing && A.run[0].f >= 59.9f, d);
  A.want = -1; aoStep(&A, &S, NULL, 0, 0, 0, 1, 0, dt);
  snprintf(d, sizeof d, "после клипа в объёме объект △ больше не предлагает (want=%d, ждём -1)", A.want); check("no_offer_after", aoWantsAction(&A) == -1, d);
  aoStep(&A, &S, NULL, 50, 0, 0, 1, 0, dt); aoStep(&A, &S, NULL, 50, 0, 0, 1, 0, dt);       // вышел из триггера
  A.want = -1; aoStep(&A, &S, NULL, 0, 0, 0, 1, 0, dt);                                   // вернулся
  snprintf(d, sizeof d, "вышел и вернулся: △ не предлагается (want=%d, ждём -1)", A.want); check("leave_return", aoWantsAction(&A) == -1, d);
  A.want = 0; aoStep(&A, &S, NULL, 0, 0, 0, 1, 1, dt);                                       // даже если △ отдали насильно
  snprintf(d, sizeof d, "повторное △: клип не играет заново (playing=%d, запусков %u, ждём 0 и 1)", A.run[0].playing, A.plays); check("no_replay", !A.run[0].playing && A.plays == 1, d);
  float c[3], r; snprintf(d, sizeof d, "кольцо выключателя после срабатывания снято (%d, ждём 0)", aoSwitchRing(&A, 0, c, &r)); check("ring_off", !aoSwitchRing(&A, 0, c, &r), d);
  printf("%s: %d проверок, %d красных\n", fails ? "FAIL" : "OK", nchecks, fails);
  return fails ? 1 : 0;
}
