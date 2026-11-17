// 1296 (Роб, 108): L2, первая миссия (m1, «TALK TO MILHOUSE»): быстрый Cancel разговора — что с миссией и точкой Милхауса. Игра без экрана (tools/bots, тот же код логики, что EBOOT).
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "world.h"
static BotWorld W;
static GameNpc *mil(void) { return (GameNpc *)gameMsnNpc(&W.msn, "milhouse"); }
static int talking(void) { return W.msn.talk.on || W.msn.rt.obj == MSN_OBJ_DIALOGUE; }
static void npcs(const char *tag) { for (int i = 0; i < W.msn.nnpc; i++) { const GameNpc *n = &W.msn.npc[i]; if (!strcmp(n->name, "milhouse")) printf("   %s npc[%d] milhouse kind=%d bonus=%d at=%.1f,%.1f walking=%d%s\n", tag, i, n->kind, n->bonus, n->pos.x, n->pos.z, n->walking, gameMsnNpc(&W.msn, "milhouse") == n ? "  <- npcFind" : ""); } }
static void dump(const char *tag) {
  const MsnRuntime *r = &W.msn.rt; GameNpc *n = mil();
  printf("C1296 %-10s cur=%d phase=%d stage=%d obj=%d status=%d nreg=%d milhouse=%s at=%.1f,%.1f talking=%d\n", tag, r->cur, r->phase, r->stage, r->obj, r->status, r->nreg, n ? "yes" : "no", n ? n->pos.x : 0.f, n ? n->pos.z : 0.f, talking());
}
extern int msnBonusTwin1296Old;
// 1296/1300: у Милхауса на L2 два лица — собеседник m1 и выдаватель гонки sr1. Пока m1 жива: бонусный скрыт, △ у собеседника не открывает гонку (msn_bonus_near = -1), у бонусного места — тоже. После отмены (Abort) — то же.
static int bonusNear(float x, float y, float z) { MsnWorld w; memset(&w, 0, sizeof w); w.on_foot = 1; w.player.x = x; w.player.y = y; w.player.z = z; return msn_bonus_near(&W.msn.rt, &w); }

static GameNpc *milOf(int bonus) { for (int i = 0; i < W.msn.nnpc; i++) { GameNpc *n = &W.msn.npc[i]; if (!strcmp(n->name, "milhouse") && (n->bonus != 0) == bonus) return n; } return 0; }
static int talkCancelCase(void) {   // настоящий разговор (△ рядом), потом gameMsnTalkCancel — как △ Cancel на карточке разговора; потом △ через 2 с у собеседника: должна открыться m1, не гонка
  bwNew(&W); for (int f = 0; f < 60; f++) bwStep(&W, 0, 0, 0);
  for (int k = 0; k < 40 && !talking(); k++) { GameNpc *n = (GameNpc *)gameMsnNpc(&W.msn, "milhouse"); float hx = n->pos.x - 1.0f, hz = n->pos.z; homerPlace(&W.homer, bwCol(), hx, W.homer.y + .3f, hz, atan2f(n->pos.x - hx, n->pos.z - hz)); bwStep(&W, GP_TRIANGLE, 0, 0); for (int f = 0; f < 3 && !talking(); f++) bwStep(&W, 0, 0, 0); }
  if (!talking()) { printf("RESULT FAIL разговор не начался\n"); return 1; }
  { GameNpc *m0 = milOf(0), *b0 = milOf(1); printf("в разговоре: миссионный at=%.1f,%.1f; бонусный at=%.1f,%.1f\n", m0->pos.x, m0->pos.z, b0->pos.x, b0->pos.z); }
  for (int f = 0; f < 48; f++) bwStep(&W, 0, 0, 0);   // 0,8 с
  gameMsnTalkCancel(&W.msn);
  for (int i = 0; i < W.msn.nnpc; i++) { const GameNpc *n = &W.msn.npc[i]; if (!strcmp(n->name, "milhouse")) printf("   npc[%d] kind=%d bonus=%d at=%.1f,%.1f\n", i, n->kind, n->bonus, n->pos.x, n->pos.z); }
  GameNpc *m = milOf(0), *b = milOf(1);
  printf("после TalkCancel: миссионный milhouse %s at=%.1f,%.1f; бонусный at=%.1f,%.1f\n", m ? "есть" : "НЕТ", m ? m->pos.x : 0.f, m ? m->pos.z : 0.f, b ? b->pos.x : 0.f, b ? b->pos.z : 0.f);
  for (int f = 0; f < 120; f++) bwStep(&W, 0, 0, 0);
  m = milOf(0); b = milOf(1);
  printf("через 2 с: миссионный %s at=%.1f,%.1f; бонусный at=%.1f,%.1f; cur=%d\n", m ? "есть" : "НЕТ", m ? m->pos.x : 0.f, m ? m->pos.z : 0.f, b ? b->pos.x : 0.f, b ? b->pos.z : 0.f, W.msn.rt.cur);
  { MsnWorld w; memset(&w, 0, sizeof w); w.on_foot = 1; w.player.x = W.homer.x; w.player.y = W.homer.y; w.player.z = W.homer.z; int bn = msn_bonus_near(&W.msn.rt, &w); printf("bonus_near у Гомера (%.1f,%.1f) = %d\n", W.homer.x, W.homer.z, bn);
    int moved = fabsf(b->pos.x - 141.6f) < 3.f && fabsf(b->pos.z + 38.9f) < 3.f;   // бонусный двойник переехал на место собеседника m1
    int ok = msnBonusTwin1296Old ? (moved && bn >= 0) : (!moved && bn == -1);   // OLD обязан показать прежнее: двойник переехал, △ открывает гонку
    printf("RESULT %s (режим %s: двойник %s, bonus_near %d)\n", ok ? "OK" : "FAIL", msnBonusTwin1296Old ? "OLD" : "новое", moved ? "переехал" : "на месте", bn); return ok ? 0 : 1; }
}
int main(int argc, char **argv) {
  if (argc < 4 || bwData(argv[1], argv[2], argv[3])) { fprintf(stderr, "cancel_host: нет данных\n"); return 2; }
  int old = argc > 4 && atoi(argv[4]); msnBonusTwin1296Old = old; int bad = 0;
  if (argc > 5 && !strcmp(argv[5], "talk")) return talkCancelCase();
  bwNew(&W); for (int f = 0; f < 180; f++) bwStep(&W, 0, 0, 0);
  GameNpc *mn = 0, *bn = 0; for (int i = 0; i < W.msn.nnpc; i++) { GameNpc *n = &W.msn.npc[i]; if (!strcmp(n->name, "milhouse")) { if (n->bonus) bn = n; else mn = n; } }
  if (!mn || !bn) { printf("RESULT FAIL нет двух Милхаусов\n"); return 1; }
  float bx = bn->pos.x, bz = bn->pos.z;
  for (int phase = 0; phase < 2; phase++) {
    if (phase == 1) { gameMsnAbort(&W.msn); for (int f = 0; f < 120; f++) bwStep(&W, 0, 0, 0); mn = 0; bn = 0; for (int i = 0; i < W.msn.nnpc; i++) { GameNpc *n = &W.msn.npc[i]; if (!strcmp(n->name, "milhouse")) { if (n->bonus) bn = n; else mn = n; } } if (!mn || !bn) { printf("RESULT FAIL после отмены нет двух Милхаусов\n"); return 1; } }
    int nearM = bonusNear(mn->pos.x - 0.9f, mn->pos.y, mn->pos.z), nearB = bonusNear(bx - 0.9f, bn->pos.y, bz);
    printf("C1296 %s: у собеседника m1 bonus_near=%d (ждём -1), у места гонки bonus_near=%d (новое -1, прежнее ≥0), twinHide бонусного=%d (ждём 1)  режим %s\n", phase ? "после Abort" : "старт", nearM, nearB, bn->twinHide, old ? "OLD" : "новое");
    if (old ? (nearB < 0 || bn->twinHide) : (nearM != -1 || nearB != -1 || bn->twinHide != 1)) bad++;   // OLD обязан показать прежнее: у места гонки △ открывает гонку
  }
  printf("RESULT %s\n", bad ? "FAIL" : "OK"); return bad ? 1 : 0;
}
