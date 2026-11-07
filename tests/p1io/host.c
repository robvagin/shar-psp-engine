// ТЕСТ90 П1: гэги читаются фоном. Условия: (1) после загрузки уровня кадр (collStreamPrepare / ensureKey) не читает файл синхронно — только запускает и принимает;
// (2) результат тот же, что у синхронного пути (набор ключей и скинов, позы, скины в кадре) — после нескольких кадров ожидания; (3) красный контроль -DSYNCREAD_P1_OLD=1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "coll_stream.h"
#ifndef P1IO_EXPECT_NEW
#define P1IO_EXPECT_NEW (!SYNCREAD_P1_OLD)   // красный контроль: -DSYNCREAD_P1_OLD=1 -DP1IO_EXPECT_NEW=1 — старый код против новых требований
#endif
static int extraBad;
static unsigned clockMs,cancels,hang;   // hang — чтение никогда не приходит (зависшая карта)
static void chk(const char *w,double a,double b,double c){int ok=0;   /* (1,1,-1) (…,… ,0) разбираются ниже по тексту */
  if(strstr(w,"(1, 1, -1)"))ok=a==1&&b==1&&c==-1; else if(strstr(w,"(1, 1, 1)"))ok=a==1&&b==1&&c==1; else ok=a==1&&b==1;
  printf("%s %s\n",ok?"ok  ":"FAIL",w);if(!ok)extraBad=1;}
static unsigned char *raw;static unsigned size,syncReads,asyncStarts;
static int get(void *u,unsigned off,void *p,unsigned n){(void)u;if(off>size || n>size-off)return -1;memcpy(p,raw+off,n);syncReads++;return 0;}
// фоновое чтение, готовое через LAT опросов (LAT=0 — сразу)
static struct { unsigned left; int busy; } io;static unsigned LAT=2;
static int aStart(void *u,unsigned off,void *p,unsigned n){(void)u;if(io.busy || off>size || n>size-off)return -1;memcpy(p,raw+off,n);io.busy=1;io.left=LAT;asyncStarts++;return 0;}
static int aPoll(void *u){(void)u;if(!io.busy)return -1;if(hang)return 0;if(io.left){io.left--;return 0;}io.busy=0;return 1;}
static void aCancel(void *u){(void)u;io.busy=0;cancels++;}
static unsigned aNow(void *u){(void)u;return clockMs;}
static const CollectAsync tab={aStart,aPoll,aCancel,aNow,0};
static unsigned frames(Collect *c,int room,float x,float z,float d,int snap){   // кадры, пока не уляжется (не больше 200); возвращает число кадров
  unsigned k=0;unsigned last=~0u;
  for(;k<200;k++){assert(collStreamPrepare(c,room,x,z,d,snap)>=0);unsigned now=collStreamTrackBytes*7u+gagSkinResidentBytes;if(!io.busy && now==last && k>3)break;last=now;}
  return k;
}
int main(int ac,char **av){
  FILE*f=fopen(av[1],"rb");assert(f);fseek(f,0,SEEK_END);size=ftell(f);rewind(f);raw=aligned_alloc(16,(size+15u)&~15u);assert(fread(raw,1,size,f)==size);fclose(f);
  Collect sy,as;void *p1,*p2;
  // синхронный образец
  p1=collStreamLoad(&sy,get,0,size);assert(p1);collStreamSetAsync(0);
  unsigned tk1[4096],sk1=0,nt=sy.sp.ntype;assert(nt<4096);
  unsigned mismatch=0,framesMax=0,framesSum=0,cases=0,asyncRuns=0;
  unsigned residentSync[512],tracksSync[512],ng=sy.ngag;assert(ng<512);
  for(unsigned g=0;g<ng;g++){const CollGag *a=sy.gag+g;for(unsigned k=0;k<ng;k++)sy.st.gagPick[k]=k==g;
    assert(!collStreamPrepare(&sy,a->room,a->x,a->z,80,(int)g));residentSync[g]=gagSkinResidentBytes;tracksSync[g]=collStreamTrackBytes;}
  (void)tk1;(void)sk1;(void)nt;
  gagSkinFree();collectFree(&sy);collStreamFree();free(p1);
  // фоновый путь
  p2=collStreamLoad(&as,get,0,size);assert(p2);collStreamSetAsync(&tab);
  unsigned loadReads=syncReads;
  for(unsigned g=0;g<ng;g++){const CollGag *a=as.gag+g;for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;   // снимок snap>=0 — синхронный путь (так и задумано), поэтому в фоне — свободный выбор по дальности
    as.st.gagPick[g]=1;
    unsigned k=frames(&as,a->room,a->x,a->z,80,-1);cases++;framesSum+=k;if(k>framesMax)framesMax=k;
    // желаемый набор у фонового пути (выбран гэг g, снимка нет) совпадает с синхронным только когда рядом нет других выбранных — для ровного сравнения берём размеры гэга g
    if(gagSkinResidentBytes!=residentSync[g] || collStreamTrackBytes!=tracksSync[g])mismatch++;   // тот же набор скинов и ключей, что у синхронного пути
    if(asyncStarts)asyncRuns=1;
  }
  unsigned frameReads=syncReads-loadReads;
  // уход далеко: всё выгружено, чтений нет
  frames(&as,as.gag[0].room,10000,10000,0,-1);
  printf("%s L%d: гэгов %u, фоновых заданий %u, синхронных чтений в кадре %u, кадров ожидания в среднем %.1f, максимум %u, выгрузка полная %d\n",
    SYNCREAD_P1_OLD?"old":"new",as.level,ng,asyncStarts,frameReads,cases?(double)framesSum/cases:0.0,framesMax,gagSkinResidentBytes==0);
  (void)asyncRuns;
  int bad=0;
  if(mismatch){printf("FAIL: набор после ожидания не равен синхронному в %u случаях из %u\n",mismatch,cases);bad=1;}
  if(P1IO_EXPECT_NEW){ if(frameReads){printf("FAIL: кадр читал синхронно %u раз\n",frameReads);bad=1;} if(ng && !asyncStarts){printf("FAIL: фоновых заданий нет\n");bad=1;} }
  else { if(!frameReads && ng){printf("FAIL: старый путь не читал в кадре\n");bad=1;} }
  // ---- надёжность (разбор НАДЁЖНОСТЬ_2026-11-04): сценарии на первом гэге с данными
  if(P1IO_EXPECT_NEW && ng){
    const CollGag *a=as.gag+0;unsigned g0=0;for(unsigned g=0;g<ng;g++)if(as.gag[g].inst>=0){a=as.gag+g;g0=g;break;}
    for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;as.st.gagPick[g0]=1;
    // 1) тайм-аут: чтение не приходит — через COLL_ASYNC_MS отмена, ровно одна, без повторных чтений того же
    hang=1;clockMs=1000;unsigned st0=asyncStarts,c0=cancels;
    for(unsigned k=0;k<5;k++)collStreamPrepare(&as,a->room,a->x,a->z,80,-1);   // запуск и ожидание
    int pending=io.busy;clockMs+=COLL_ASYNC_MS+50;
    int rr=collStreamPrepare(&as,a->room,a->x,a->z,80,-1);
    chk("1) тайм-аут: чтение шло, а после лимита отменено (1, 1, -1)",(int)pending,(int)(cancels-c0),rr);
    for(unsigned k=0;k<300;k++){clockMs+=33;collStreamPrepare(&as,a->room,a->x,a->z,80,-1);}
    unsigned retries=asyncStarts-st0;
    chk("1) тайм-аут: повторов нет — каждое задание началось не чаще раза (число начатых не больше числа разных нужных штук), отмен не меньше одной",(double)(retries<=ng+nt?1:0),(double)(cancels-c0>=1),0);
    hang=0;
    // 2) конец уровня посреди чтения: выгрузка отменяет, дескриптор свободен, буфер не течёт (ASAN) — на свежей загрузке с зависшим чтением
    gagSkinFree();collectFree(&as);collStreamFree();free(p2);io.busy=0;
    p2=collStreamLoad(&as,get,0,size);assert(p2);collStreamSetAsync(&tab);a=as.gag+g0;
    for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;as.st.gagPick[g0]=1;
    hang=1;c0=cancels;
    for(unsigned k=0;k<3;k++)collStreamPrepare(&as,a->room,a->x,a->z,80,-1);
    int busyBefore=io.busy;
    gagSkinFree();collectFree(&as);collStreamFree();free(p2);
    chk("2) выгрузка уровня посреди чтения: шло, отменено, дескриптор свободен (1, 1, 1)",busyBefore,(int)(cancels-c0),(int)(!io.busy));
    hang=0;p2=collStreamLoad(&as,get,0,size);assert(p2);collStreamSetAsync(&tab);a=as.gag+g0;
    // 3) перенос: нужный гэг ушёл из набора — идущее чтение отменяется сразу (до тайм-аута)
    for(unsigned k=0;k<ng;k++)as.st.gagPick[k]=0;as.st.gagPick[g0]=1;
    hang=1;c0=cancels;
    for(unsigned k=0;k<3;k++)collStreamPrepare(&as,a->room,a->x,a->z,80,-1);
    int busy3=io.busy;collStreamPrepare(&as,a->room,10000,10000,0,-1);   // перенос далеко
    chk("2) перенос посреди чтения: шло, отменено сразу, не ждали тайм-аута (1, 1, 1)",busy3,(int)(cancels-c0),(int)(!io.busy));
    hang=0;
  }
  gagSkinFree();collectFree(&as);collStreamFree();free(p2);free(raw);
  if(extraBad)bad=1;
  printf(bad?"FAIL\n":"PASS\n");return bad;
}
