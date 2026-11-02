// Кошелёк, принадлежность, продавцы (см. shop.h).
#include <string.h>
#include <math.h>
#include "shop.h"

static void cpy(char *d, const char *s, size_t n) { strncpy(d, s ? s : "", n - 1); d[n - 1] = 0; }
static int eqi(const char *a, const char *b) {   // имена скриптов сравниваются без регистра (msn_streq)
  if (!a || !b) return 0;
  for (; *a && *b; a++, b++) { int x = (unsigned char)*a, y = (unsigned char)*b; if (x >= 'A' && x <= 'Z') x += 32; if (y >= 'A' && y <= 'Z') y += 32; if (x != y) return 0; }
  return *a == *b;
}

void shopInit(Shop *s, Coins *coins, int level) { memset(s, 0, sizeof *s); s->coins = coins; s->at = -1; s->level = level & 7; }
static uint32_t *ownedP(Shop *s) { return s->coins ? (uint32_t *)&s->coins->owned[s->level] : &s->ownedLocal; }
uint32_t shopOwnedMask(const Shop *s) { return s->coins ? s->coins->owned[s->level] : s->ownedLocal; }

int shopAddItem(Shop *s, const char *name, int kind, const char *quest, int price, const char *seller) {
  int i = shopFind(s, name); if (i >= 0) return i;
  if (s->nitem >= SHOP_ITEMS || !name) return -1;
  ShopItem *t = &s->item[s->nitem]; memset(t, 0, sizeof *t);
  cpy(t->name, name, sizeof t->name); cpy(t->quest, quest, sizeof t->quest); cpy(t->seller, seller, sizeof t->seller);
  t->kind = kind; t->price = price;
  return s->nitem++;
}

void shopAddSeller(Shop *s, const char *shop, const char *npc, float x, float y, float z, float radius, const float *car) {
  int i = 0; while (i < s->nseller && !eqi(s->seller[i].shop, shop)) i++;
  if (i == s->nseller) { if (s->nseller >= SHOP_SELLERS) return; s->nseller++; }
  ShopSeller *v = &s->seller[i]; memset(v, 0, sizeof *v);
  cpy(v->shop, shop, sizeof v->shop); cpy(v->npc, npc, sizeof v->npc);
  v->x = x; v->y = y; v->z = z; v->radius = radius;
  if (car) { v->car[0] = car[0]; v->car[1] = car[1]; v->car[2] = car[2]; v->hasCar = 1; }
}

int shopFind(const Shop *s, const char *name) {
  for (int i = 0; i < s->nitem; i++) if (eqi(s->item[i].name, name)) return i;
  return -1;
}

int shopOwns(const Shop *s, int kind, const char *name) {
  int i = shopFind(s, name); if (i < 0) return 0;
  const ShopItem *t = &s->item[i];
  if (kind && t->kind != kind) return 0;
  if (eqi(t->quest, "defaultcar") || eqi(t->quest, "defaultskin")) return 1;
  return (shopOwnedMask(s) >> i) & 1u;
}

void shopGive(Shop *s, const char *name) { int i = shopFind(s, name); if (i >= 0) *ownedP(s) |= 1u << i; }

int shopCoins(const Shop *s) { return s->coins ? s->coins->collected : s->wallet; }

void shopCoinsAdd(Shop *s, int delta, int why) {
  int c = shopCoins(s), n = c + delta; if (n < 0) n = 0;
  if (s->coins) s->coins->collected = n; else s->wallet = n;
  s->lastDelta = n - c; s->lastWhy = why;
  if (n < c) s->spent += c - n; else s->earned += n - c;
}

int shopBuy(Shop *s, int item) {
  if (item < 0 || item >= s->nitem) return SHOP_BAD;
  const ShopItem *t = &s->item[item];
  if (shopOwns(s, t->kind, t->name)) return SHOP_OWNED;
  if (shopCoins(s) < t->price) return SHOP_NOMONEY;
  shopCoinsAdd(s, -t->price, 0); *ownedP(s) |= 1u << item; s->buys++;
  return SHOP_OK;
}

// Сверка g09a-hud (круг 1, P13:13): в списке продавца машин только непроданные. SHOP_SOLDHIDE_OLD=1 — прежнее
#ifndef SHOP_SOLDHIDE_OLD
#define SHOP_SOLDHIDE_OLD 0
#endif
int shopSellerNear(const Shop *s, float x, float y, float z) {
  int best = -1; float bd = 1e9f;
  for (int i = 0; i < s->nseller; i++) {
    const ShopSeller *v = &s->seller[i];
    float dx = x - v->x, dy = y - v->y, dz = z - v->z, d = sqrtf(dx * dx + dz * dz);
    if (fabsf(dy) > 2.f || d > v->radius + SHOP_REACH || d >= bd) continue;
    if (!SHOP_SOLDHIDE_OLD && shopSellerItem(s, i, 0) < 0) continue;   // продавать нечего — лавка не открывается
    best = i; bd = d;
  }
  return best;
}

// Сверка g09a-hud (круг 1, P13:13): в списке продавца машин только непроданные — купил, машина пропала из списка. SHOP_SOLDHIDE_OLD=1 — прежнее («уже купленное показывается, но не покупается»)
int shopSellerItem(const Shop *s, int seller, int k) {
  if (seller < 0 || seller >= s->nseller) return -1;
  for (int i = 0; i < s->nitem; i++)
    if (eqi(s->item[i].quest, "forsale") && eqi(s->item[i].seller, s->seller[seller].shop)
        && (SHOP_SOLDHIDE_OLD || s->item[i].kind != SHOP_CAR || !shopOwns(s, SHOP_CAR, s->item[i].name)) && k-- == 0) return i;
  return -1;
}

int shopSellerCount(const Shop *s, int seller) { int n = 0; while (shopSellerItem(s, seller, n) >= 0) n++; return n; }

void shopStep(Shop *s, int dir) {
  int n = shopSellerCount(s, s->at);
  if (n > 0) s->sel = ((s->sel + dir) % n + n) % n;
}

void shopSetAttr(Shop *s, const char *name, const float a[4]) {
  int i = shopFind(s, name); if (i < 0) return;
  for (int k = 0; k < 4; k++) s->item[i].attr[k] = a[k];
  s->item[i].hasAttr = 1;
}

int shopForsaleCar(const Shop *s, int item) {
  if (item < 0 || item >= s->nitem) return -1;
  int n = 0;
  for (int i = 0; i < s->nitem; i++) {
    const ShopItem *t = &s->item[i];
    if (!eqi(t->quest, "forsale") || t->kind != SHOP_CAR) continue;
    if (i == item) return n;
    n++;
  }
  return -1;
}

int shopStars(const Shop *s, int item, int k) {
  if (item < 0 || item >= s->nitem || k < 0 || k > 3 || !s->item[item].hasAttr) return -1;
  int v = (int)floorf(s->item[item].attr[k] * 2.f + .5f);   // 1.5 -> 3 полузвезды
  return v < 0 ? 0 : v > 10 ? 10 : v;
}
