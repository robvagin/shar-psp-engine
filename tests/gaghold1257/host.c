// Будка-93 (1257): collStreamPending не должен оставаться > 0 после отменённого фонового чтения (SPAWNHOLD why=gag 7000 мс в эмуляторе).
// Сценарий: гэг выбран, чтение началось (медленное), игрок ушёл - задание отменено; через несколько кадров всё тихо: pending обязан быть 0.
// Красный контроль: -DGAGPEND1257_OLD=1 (счёт «начато минус принято») должен упасть.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "coll_stream.h"
static unsigned char *raw;static unsigned size;
static int get(void *u,unsigned off,void *p,unsigned n){(void)u;if(off>size || n>size-off)return -1;memcpy(p,raw+off,n);return 0;}
static struct { unsigned left; int busy; } io;static unsigned LAT=50;
static int aStart(void *u,unsigned off,void *p,unsigned n){(void)u;if(io.busy || off>size || n>size-off)return -1;memcpy(p,raw+off,n);io.busy=1;io.left=LAT;return 0;}
static int aPoll(void *u){(void)u;if(!io.busy)return -1;if(io.left){io.left--;return 0;}io.busy=0;return 1;}
static void aCancel(void *u){(void)u;io.busy=0;}
static unsigned aNow(void *u){(void)u;return 0;}
static const CollectAsync tab={aStart,aPoll,aCancel,aNow,0};
int main(int ac,char **av){
  FILE*f=fopen(av[1],"rb");assert(f);fseek(f,0,SEEK_END);size=ftell(f);rewind(f);raw=aligned_alloc(16,(size+15u)&~15u);assert(fread(raw,1,size,f)==size);fclose(f);
  Collect as;void *p=collStreamLoad(&as,get,0,size);assert(p);collStreamSetAsync(&tab);
#ifdef GAGPEND1257_OLD
  if(GAGPEND1257_OLD)collGagPend1257Old=1;
#endif
  unsigned ng=as.ngag;int bad=0,cases=0;
  for(unsigned g=0;g<ng && cases<8;g++){const CollGag *a=as.gag+g;
    for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;as.st.gagPick[g]=1;
    for(int k=0;k<3;k++)collStreamPrepare(&as,a->room,a->x,a->z,80,-1);   // чтение пошло (LAT кадров), не закончилось
    if(!io.busy)continue;
    cases++;
    unsigned mid=collStreamPending();
    for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;
    for(int k=0;k<6;k++)collStreamPrepare(&as,a->room,10000,10000,0,-1);   // ушли далеко: задание отменено, запросов больше нет
    unsigned after=collStreamPending();
    printf("гэг %u: в пути %u, после ухода %u %s\n",g,mid,after,after?"FAIL":"ok");if(after)bad=1;
  }
  if(!cases){printf("FAIL: ни одного чтения в пути (набор без гэгов?)\n");return 1;}
  printf("%s: случаев %d\n",bad?"FAIL":"PASS",cases);return bad;
}
