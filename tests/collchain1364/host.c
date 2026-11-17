// 1364 (аудит простоев П1): гэги района читаются фоном по одному заданию; цепочка берёт следующее в том же кадре. Хост-замер с моделью карты: чтение n байт кончается через 300 мкс + n/16 мкс (16 МБ/с),
// часы модели идут только от sleepUs и от конца кадра (33 333 мкс). Печатает по уровню: гэгов, заданий, кадров до оседания (среднее/максимум/сумма) и итоговые байты (ключи, скины) — они у нового и
// старого путей обязаны совпасть. Красный: -DCOLLCHAIN1364_OLD=1 (одно задание за кадр).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "coll_stream.h"
#ifndef COLLCHAIN1364_OLD
#define COLLCHAIN1364_OLD 0
#endif
void wdFire(const char *k,const char *a,const char *i){(void)k;(void)a;(void)i;}   // заглушки сторожа для хост-сборки (homer.c, car.c)
void wdNote(const char *k,const char *a,const char *i){(void)k;(void)a;(void)i;}
static unsigned char *raw;static unsigned size,asyncStarts;
static unsigned long long fakeUs;
static int get(void *u,unsigned off,void *p,unsigned n){(void)u;if(off>size || n>size-off)return -1;memcpy(p,raw+off,n);return 0;}
static struct { int busy; unsigned long long doneAt; unsigned n; } io;
static unsigned sleeps,polls,fast;   // 1364б: сон в коде гэгов запрещён (sleeps обязан быть 0); fast=1 — карта-мгновенный ответ (кеш, эмулятор), по умолчанию карта PSP 16 МБ/с
static int aStart(void *u,unsigned off,void *p,unsigned n){(void)u;if(io.busy || off>size || n>size-off)return -1;memcpy(p,raw+off,n);io.busy=1;io.n=n;io.doneAt=fast?fakeUs:fakeUs+300+n/16;asyncStarts++;return 0;}
static int aPoll(void *u){(void)u;polls++;if(!io.busy)return -1;if(fakeUs<io.doneAt)return 0;io.busy=0;return 1;}   // результат отдаётся один раз: после 1 дескриптор не опрашивается (как sceIoPollAsync)
static void aCancel(void *u){(void)u;io.busy=0;}
static unsigned aNow(void *u){(void)u;return (unsigned)(fakeUs/1000);}
static unsigned aNowUs(void *u){(void)u;return (unsigned)fakeUs;}
static void aSleep(void *u,unsigned us){(void)u;sleeps++;fakeUs+=us;}
static const CollectAsync tab={aStart,aPoll,aCancel,aNow,0,aNowUs,aSleep};
int main(int ac,char **av){
  fast=getenv("CARD_FAST")!=0;FILE*f=fopen(av[1],"rb");assert(f);fseek(f,0,SEEK_END);size=ftell(f);rewind(f);raw=aligned_alloc(16,(size+15u)&~15u);assert(fread(raw,1,size,f)==size);fclose(f);
  Collect as;void *p=collStreamLoad(&as,get,0,size);assert(p);collStreamSetAsync(&tab);
  unsigned ng=as.ngag,cases=0,framesSum=0,framesMax=0;unsigned long long bytesSum=0;
  for(unsigned g=0;g<ng;g++){const CollGag *a=as.gag+g;for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;as.st.gagPick[g]=1;
    unsigned k=0,last=~0u,stable=0;
    for(;k<400;k++){fakeUs+=33333;unsigned c0=(unsigned)fakeUs;(void)c0;assert(collStreamPrepare(&as,a->room,a->x,a->z,80,-1)>=0);
      unsigned now=collStreamTrackBytes*7u+gagSkinResidentBytes+collStreamTexBytes*3u+collStreamVertBytes*5u;
      if(!io.busy && now==last){if(++stable>=2)break;}else stable=0;last=now;}
    cases++;framesSum+=k;if(k>framesMax)framesMax=k;bytesSum+=collStreamTrackBytes+gagSkinResidentBytes+collStreamTexBytes+collStreamVertBytes;
    fakeUs+=33333;collStreamPrepare(&as,a->room,10000,10000,0,-1);for(int j=0;j<40 && io.busy;j++){fakeUs+=33333;collStreamPrepare(&as,a->room,10000,10000,0,-1);}   // уйти далеко: выгрузить
  }
  // один «район»: выбраны сразу все гэги (как въезд в район)
  for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=1;
  unsigned dist=0,last=~0u,stable=0,starts0=asyncStarts;
  for(;dist<2000;dist++){fakeUs+=33333;assert(collStreamPrepare(&as,as.gag[0].room,as.gag[0].x,as.gag[0].z,120,-1)>=0);
    unsigned now=collStreamTrackBytes*7u+gagSkinResidentBytes+collStreamTexBytes*3u+collStreamVertBytes*5u;
    if(!io.busy && now==last){if(++stable>=2)break;}else stable=0;last=now;}
  printf("%s %s L%d: гэгов %u, заданий района %u, кадров до оседания района %u (%.2f с при 30 к/с), по одному гэгу в среднем %.1f макс %u, байт %llu район %u, сонов %u, опросов %u\n",
    fast?"fast":"card",COLLCHAIN1364_OLD?"old":"new",as.level,ng,asyncStarts-starts0,dist,dist/30.0,cases?(double)framesSum/cases:0.0,framesMax,bytesSum,collStreamTrackBytes+gagSkinResidentBytes+collStreamTexBytes+collStreamVertBytes,sleeps,polls);
  return 0;
}
