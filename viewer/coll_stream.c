// Runtime split of the unchanged CLT1 file. Only dense SPR1 keys and CHR1
// blobs are detached; all logic/visibility/collision and card/wasp data remain.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "coll_stream.h"
#ifndef SKIN_HOST
#include <malloc.h>
#include <pspkernel.h>
#endif
int gagSkinStreamOld=GAG_SKIN_STREAM_OLD;
unsigned collStreamResidentBytes,collStreamTrackBytes;
static CollectRead source;static void *sourceUser;
static unsigned keyOffset;
static float **keys;static unsigned ntypes;
static int *typeGag;static const StateProps *liveSp;
#define A16(x) (((x)+15u)&~15u)
unsigned collStreamAllocFails,collStreamLastFail;int collFailTest;   // ТЕСТ90 память: отказы выделения пакета гэгов (настоящие и тестовые «flag collfail N» — следующие N выделений без памяти); по счётчику main_collect.h решает о повторе
static void *aligned(unsigned n) {
  void *p;
  if(collFailTest>0){collFailTest--;p=0;}
  else
#ifdef SKIN_HOST
  p=aligned_alloc(16,A16(n));
#else
  p=memalign(16,n);
#endif
  if(!p){collStreamAllocFails++;collStreamLastFail=n;}
  return p;
}
// ТЕСТ90 П1: одно фоновое задание за раз; кадр только запускает чтение и усваивает готовое (разбор — доли мс), само чтение идёт мимо кадра
static CollectAsync asy;static int asyOn;
static struct { int busy,kind; unsigned i,bytes,t0; unsigned char *buf; } job;static unsigned char *failKey;   // типы, чьи ключи не пришли (тайм-аут, ошибка): не просить снова   // kind 0 — ключи типа i, 1 — гэг-скин i
static unsigned char *reqKey;   // типы, запрошенные ensureKey в кадре (кадр не читает: ключи придут через несколько кадров)
unsigned collStreamAsyncStarts,collStreamAsyncDone,collStreamAsyncDefer,collStreamAsyncCancels,collStreamAsyncFails;
void collStreamSetAsync(const CollectAsync *a){if(a && !SYNCREAD_P1_OLD){asy=*a;asyOn=asy.start&&asy.poll;}else{asyOn=0;memset(&asy,0,sizeof asy);}}
static void jobCancel(void){   // прервать идущее чтение; буфер освобождается только после конца отмены (иначе ядро писало бы в чужую память)
  if(!job.busy)return;
  if(asy.cancel)asy.cancel(asy.user);
  free(job.buf);memset(&job,0,sizeof job);collStreamAsyncCancels++;
}
static unsigned nowMs(void){return asy.nowMs?asy.nowMs(asy.user):0;}
static void jobFail(void){   // не пришло или ошибка: пометить, чтобы кадр не гонял то же чтение каждый раз
  if(job.kind==0){if(!failKey)failKey=calloc(ntypes?ntypes:1,1);if(failKey)failKey[job.i]=1;}else gagSkinMarkBad(job.i);
  collStreamAsyncFails++;
}
static void asyncDrop(void){
  jobCancel();
  memset(&job,0,sizeof job);free(reqKey);reqKey=0;free(failKey);failKey=0;
}
void collStreamFree(void) {
  asyncDrop();asyOn=0;
  if(keys)for(unsigned i=0;i<ntypes;i++)free(keys[i]);
  free(keys);free(typeGag);keys=0;typeGag=0;ntypes=0;source=0;sourceUser=0;liveSp=0;
  collStreamResidentBytes=collStreamTrackBytes=0;
}
static int loadKeys(const StateProps *sp,unsigned i) {
  const SpType *t=sp->type+i;
  unsigned bytes=(unsigned)(t->frames&65535u)*(t->frames>>16)*28u;
  if(!bytes || keys[i])return 0;
  keys[i]=aligned(bytes);if(!keys[i])return -1;
  if(source(sourceUser,keyOffset+t->keyOff*28u,keys[i],bytes)){free(keys[i]);keys[i]=0;return -1;}
  collStreamTrackBytes+=bytes;return 0;
}
static int ensureKey(unsigned i){
  if(!liveSp || i>=ntypes)return -1;
  if(keys[i])return 0;
  if(failKey && failKey[i])return -1;
  if(asyOn){   // кадр не читает: запрос в очередь, позы этого типа — запасным путём, ключи придут следом
    if(!reqKey)reqKey=calloc(ntypes,1);
    if(reqKey && !reqKey[i]){reqKey[i]=1;collStreamAsyncDefer++;}
    return -1;
  }
  return loadKeys(liveSp,i);
}
void *collStreamLoad(Collect *c,CollectRead read,void *user,unsigned size) {
  collStreamFree();
  unsigned h[20],s[16];
  if(size<80 || read(user,0,h,80) || memcmp(h,"CLT1",4) || h[1]<3 || h[5]>size-64 || read(user,h[5],s,64) || memcmp(s,"SPR1",4))return 0;
  if(h[6]>size-h[5] || h[14]>size/64u || s[12]>size || s[13]>size ||
      s[1]>size/84u || s[3]>size/40u || s[4]>size/52u || s[5]>size/28u)return 0;
  uint64_t off=(uint64_t)h[5]+64+A16(s[12])+A16(s[13])+A16(s[3]*40u)+A16(s[1]*84u)+A16(s[4]*52u);
  uint64_t keyBytes=(uint64_t)s[5]*28u;keyBytes=(keyBytes+15u)&~15ull;
  uint64_t end=(uint64_t)h[15]+(uint64_t)h[14]*64u;
  if(off+keyBytes>size || end>size || end<off+keyBytes || keyBytes>h[6] || (off&15) || (h[15]&15))return 0;
  unsigned n=(unsigned)(end-keyBytes),kb=(unsigned)keyBytes;
  unsigned char *b=aligned(n);if(!b)return 0;
  if(read(user,0,b,(unsigned)off) || read(user,(unsigned)(off+keyBytes),b+off,(unsigned)(end-off-keyBytes))){free(b);return 0;}
  unsigned *u=(unsigned *)b;
  u[6]-=kb;u[8]-=kb;u[10]-=kb;u[12]-=kb;u[15]-=kb;
  if(collectInitResident(c,b,n,1)){free(b);return 0;}
  source=read;sourceUser=user;keyOffset=(unsigned)off;ntypes=c->sp.ntype;
  keys=calloc(ntypes,sizeof *keys);typeGag=malloc(ntypes*sizeof *typeGag);
  if(!keys || !typeGag)goto bad;
  for(unsigned i=0;i<ntypes;i++)typeGag[i]=-1;
  for(unsigned g=0;g<c->ngag;g++)if(c->gag[g].inst>=0)typeGag[c->sp.inst[c->gag[g].inst].type]=(int)g;
  c->sp.typeKey=(const float *const *)keys;c->sp.ensureKey=ensureKey;liveSp=&c->sp;
  for(unsigned i=0;i<ntypes;i++)if(typeGag[i]<0 && loadKeys(&c->sp,i))goto bad;
  if(gagSkinInitSource(c,read,user,size)<0)goto bad;
  collStreamResidentBytes=n;return b;
bad:gagSkinFree();collectFree(c);collStreamFree();free(b);return 0;
}
int collStreamPrepare(Collect *c,int room,float ex,float ez,float distance,int snap) {
  if(!source)return 0;
  unsigned char wanted[COLL_MAXGAG]={0};
  for(unsigned g=0;g<c->ngag;g++){
    const CollGag *a=c->gag+g;float dx=a->x-ex,dz=a->z-ez;
    // Same draw set, with a 10 metre prefetch margin (port I/O policy).
    wanted[g]=a->room==room && (!a->random || (snap>=0 ? (int)g==snap : c->st.gagPick[g])) &&
        dx*dx+dz*dz<=(distance+10.f)*(distance+10.f);
  }
  for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && !wanted[typeGag[i]] && keys[i]){
    const SpType *t=c->sp.type+i;collStreamTrackBytes-=(t->frames&65535u)*(t->frames>>16)*28u;
    free(keys[i]);keys[i]=0;
    for(unsigned j=0;j<c->sp.ninst;j++)if(c->sp.inst[j].type==i)c->sp.run[j].poseValid=0;
  }
  if(asyOn && snap<0){   // фоновый путь: в кадре — запуск и усвоение, чтение не ждём (снимок гэга snap>=0 — синхронно, как всегда: проверки ждут кадр)
    gagSkinTrim(wanted);
    if(job.busy){   // задание уже не нужно (ушли далеко, перенос, конец миссии) — отменить сразу
      int needed=job.kind==0 ? ((typeGag[job.i]>=0 && wanted[typeGag[job.i]]) || typeGag[job.i]<0) : (gagSkinGagOf(job.i)>=0 && wanted[gagSkinGagOf(job.i)]);
      if(!needed){jobCancel();return 0;}
      int r=asy.poll(asy.user);
      if(r==0){   // не готово — подождать кадр, но не дольше COLL_ASYNC_MS
        if(asy.nowMs && nowMs()-job.t0>COLL_ASYNC_MS){jobFail();jobCancel();return -1;}
        return 0;
      }
      int ok=0;
      if(r>0){
        if(job.kind==0){keys[job.i]=(float *)job.buf;collStreamTrackBytes+=job.bytes;ok=1;}
        else ok=!gagSkinAdopt(job.i,job.buf);
      }
      if(!ok){jobFail();free(job.buf);}
      memset(&job,0,sizeof job);
      if(!ok)return -1;
      collStreamAsyncDone++;
    }
    // следующее нужное: сначала ключи (по запросам ensureKey и по видимым гэгам), потом скины
    for(unsigned i=0;i<ntypes && !job.busy;i++){
      const SpType *t=c->sp.type+i;unsigned bytes=(unsigned)(t->frames&65535u)*(t->frames>>16)*28u;
      if(!bytes || keys[i] || (failKey && failKey[i]))continue;
      if(!((typeGag[i]>=0 && wanted[typeGag[i]]) || (reqKey && reqKey[i])))continue;
      if(reqKey)reqKey[i]=0;
      unsigned char *b=aligned(bytes);if(!b)return -1;
      if(asy.start(asy.user,keyOffset+t->keyOff*28u,b,bytes)){free(b);return -1;}
      job.busy=1;job.kind=0;job.i=i;job.bytes=bytes;job.buf=b;job.t0=nowMs();collStreamAsyncStarts++;
    }
    unsigned gi,goff,gsz;
    if(!job.busy && gagSkinNext(wanted,&gi,&goff,&gsz)){
      unsigned char *b=aligned(gsz);if(!b)return -1;
      if(asy.start(asy.user,goff,b,gsz)){free(b);return -1;}
      job.busy=1;job.kind=1;job.i=gi;job.bytes=gsz;job.buf=b;job.t0=nowMs();collStreamAsyncStarts++;
    }
    return 0;
  }
  for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && wanted[typeGag[i]] && loadKeys(&c->sp,i))return -1;
  return gagSkinPrepare(c,wanted);
}
