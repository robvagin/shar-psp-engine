#include <stdio.h>
#include <math.h>
#include "carcoins.h"
static int bad;
#if CAR_BOOM_COINS20_OLD
#define BOOMCAP 10   // сверка g11b P03-27: потолок потери при гибели машины 20 (CAR_BOOM_COINS20_OLD=1 — прежние 10)
#else
#define BOOMCAP 20
#endif
#define CHECK(x) do{if(!(x)){bad++;printf("FAIL line%d %s\n",__LINE__,#x);}}while(0)
static int active(const Coins*c){int n=0;for(unsigned i=0;i<c->n;i++)n+=c->c[i].active;return n;}
int main(void){
 const int wallets[]={0,4,10,35};
 for(unsigned a=0;a<sizeof wallets/sizeof*wallets;a++){
  Coins cs;CHECK(!coinsInit(&cs,NULL,0));cs.collected=wallets[a];cs.dropAuto=1;
  Car car={0};int armed=0;carBoomCoins(&car,&cs,NULL,&armed);CHECK(armed==1);car.destroyed=1;car.deadT=CAR_BOOM_DELAY-.01f;carBoomCoins(&car,&cs,NULL,&armed);CHECK(cs.collected==wallets[a]);
  int n=wallets[a]<BOOMCAP?wallets[a]:BOOMCAP;car.deadT=CAR_BOOM_DELAY;carBoomCoins(&car,&cs,NULL,&armed);
  CHECK(cs.collected==wallets[a]-n);CHECK(active(&cs)==n);CHECK(cs.dropAuto==1);
  int sides[4]={0};for(unsigned i=0;i<cs.n;i++)if(cs.c[i].active){Coin*c=&cs.c[i];CHECK(c->airborne && c->loose && !c->autoCollect && c->vy>=0);sides[(c->vx>0)*2+(c->vz>0)]++;}
  if(n>=10)for(int i=0;i<4;i++)CHECK(sides[i]>0);
  for(int i=0;i<120;i++)carBoomCoins(&car,&cs,NULL,&armed);CHECK(active(&cs)==n);
  // Actual loose-coin motion, fade/expiry, pickup after landing; no substitute model.
  Coins expires;CHECK(!coinsInit(&expires,NULL,0));expires.collected=wallets[a];int arm2=1;carBoomCoins(&car,&expires,NULL,&arm2);
  for(int i=0;i<17*60;i++)coinsUpdate(&expires,100,100,100,0,1.f/60);CHECK(active(&expires)==0);coinsFree(&expires);
  for(int i=0;i<2*60;i++)coinsUpdate(&cs,100,100,100,0,1.f/60);
  for(unsigned i=0;i<cs.n;i++)if(cs.c[i].active){float x=cs.c[i].x,y=cs.c[i].y,z=cs.c[i].z;for(int f=0;f<90;f++)coinsUpdate(&cs,x,y,z,0,1.f/60);}
  CHECK(cs.collected==wallets[a]);coinsFree(&cs);
 }
 // A loaded exploded husk must not spill a fresh wallet.
 Coins cs;CHECK(!coinsInit(&cs,NULL,0));cs.collected=35;Car car={0};car.destroyed=1;car.deadT=CAR_BOOM_DELAY;int armed=0;
 carBoomCoins(&car,&cs,NULL,&armed);CHECK(cs.collected==35 && active(&cs)==0);
 // Repair/new normal frame arms exactly one subsequent explosion.
 car.destroyed=0;carBoomCoins(&car,&cs,NULL,&armed);car.destroyed=1;carBoomCoins(&car,&cs,NULL,&armed);CHECK(cs.collected==35-BOOMCAP && active(&cs)==BOOMCAP);
 coinsFree(&cs);printf("%s wallet0/4/10/35 once, scatter/pickup/expiry, husk-load, repair failures=%d\n",bad?"FAIL":"PASS",bad);return !!bad;
}
