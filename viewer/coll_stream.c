// Runtime split of the unchanged CLT1 file. Only dense SPR1 keys and CHR1
// blobs are detached; all logic/visibility/collision and card/wasp data remain.
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "coll_stream.h"
#ifndef SKIN_HOST
#include <malloc.h>
#include <pspkernel.h>
#include "zone3_api.h"   // П5 Зоны-95
#endif
int gagSkinStreamOld=GAG_SKIN_STREAM_OLD;
int collSplitOld=COLLSPLIT_OLD;unsigned collHoldMs;   // Пропечка-96 (Fable шаг 11): вершины гэгов не лежат в блоке пакета, а читаются по типу вместе с ключами; 1 - прежнее (всё в блоке)
unsigned collStreamVertBytes,collStreamVertLoads,collStreamVertSaved;   // вершин сейчас в памяти / чтений за уровень / байт, не взятых в блок
static int splitOn;static unsigned vertFileOff;
// Пропечка-96 COLLSPLIT, текстуры: файл CLT1 тот же, текстуры гэгов (записи по 16 Б заголовок + палитра + уровни) читаются по номеру вместе с вершинами типа, держатся, пока их хочет хоть один нужный гэг
typedef struct { unsigned char *buf; unsigned lo,hi; unsigned tmask[COLL_STREAM_TW]; } VertRec;
typedef struct { unsigned char *buf; unsigned off,size; unsigned char perm; } TexRec;
static VertRec *vrec;static TexRec *trec;static unsigned ntexs,texSplit;static unsigned char *failVert;   // failVert: [0,ntypes) вершины типа, [ntypes,ntypes+ntexs) текстуры
static unsigned tneed[COLL_STREAM_TW];   // текстуры, нужные желаемым гэгам последнего prepare
void (*collStreamTexBind)(unsigned k,unsigned char *rec,unsigned size);void (*collStreamTexUnbind)(unsigned k);
unsigned collStreamTexBytes,collStreamTexSaved;   // по типу: буфер вершин (0 - нет), диапазон [lo,hi) в общем блобе вершин, тип не пришёл
unsigned collStreamResidentBytes,collStreamTrackBytes;
static CollectRead source;static void *sourceUser;
static unsigned keyOffset;
static float **keys;static unsigned ntypes;
static int *typeGag;static const StateProps *liveSp;
#define A16(x) (((x)+15u)&~15u)
unsigned collStreamAllocFails,collStreamLastFail;int collFailTest;   // ТЕСТ90 память: отказы выделения пакета гэгов (настоящие и тестовые «flag collfail N» — следующие N выделений без памяти); по счётчику main_collect.h решает о повторе
#ifndef GAGPLAN99_OLD
#define GAGPLAN99_OLD 0
#endif
int gagPlan99Off=GAGPLAN99_OLD;unsigned collStreamPlanN,collStreamPlanKB;   // 99: гэги комнаты, взятые сразу из кучи (место не вмещает)
static int curRoom=-1;   // комната последнего collStreamPrepare: >=0 — игрок внутри, ключи и кожи гэгов комнаты живут, пока он там
// П5 Зоны-95: ключи и кожи гэгов КОМНАТЫ берут память в области «место» (как блоб комнаты), городские — в куче; выход из комнаты их освобождает (wanted=0), free() возвращает в область
static void *aligned(unsigned n);
static void *alignedRoom(unsigned n){
#ifndef SKIN_HOST
  // 99 (Инспектор L3, Обсерватория: WATCHDOG mem who=gag, «место» 70 КБ из 70 КБ, просят 160 КБ): блоб комнаты 1,8 МБ + пак гэгов 1,07 МБ + панорама 0,13 МБ = 3,03 МБ при «месте» 3,07 МБ - запаса 40 КБ. Не лезет - гэг СРАЗУ
  // берёт кучу (план без лестницы и без проливов-тревог: выход из комнаты освобождает его free()); влезает - как было. GAGPLAN99_OLD=1 / файл gagplan99old - как было
  if(curRoom>=0){
    if(!gagPlan99Off&&zone3AreaOnX(Z3A_PLACE)&&zone3MaxFreeX(Z3A_PLACE)<n+192u){collStreamPlanN++;collStreamPlanKB+=(n+1023u)>>10;return aligned(n);}
    void *p=zone3AllocX(Z3A_PLACE,"gag",n,16);if(!p){collStreamAllocFails++;collStreamLastFail=n;}return p;}
#endif
  return aligned(n);
}
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
#ifndef GAG_LOAD_M
#define GAG_LOAD_M 100.f   // гэг города подгружается ближе 100 м
#define GAG_KEEP_M 150.f   // и отдаётся дальше 150 м (гистерезис оригинала)
#endif
static unsigned char wantedLast[COLL_MAXGAG];   // желаемые гэги последнего collStreamPrepare (для collStreamPending)
static CollectAsync asy;static int asyOn;static unsigned char allocBlocked;   // Будка-93 (1257): последний запрос памяти под чтение отказан (collStreamPending не ждёт того, что нечем читать)
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
  if(job.kind==0){if(!failKey)failKey=calloc(ntypes?ntypes:1,1);if(failKey)failKey[job.i]=1;}
  else if(job.kind==2 || job.kind==3){if(!failVert)failVert=calloc(ntypes+ntexs+1,1);if(failVert)failVert[job.kind==2?job.i:ntypes+job.i]=1;}
  else gagSkinMarkBad(job.i);
  collStreamAsyncFails++;
}
static void asyncDrop(void){
  allocBlocked=0;jobCancel();
  memset(&job,0,sizeof job);free(reqKey);reqKey=0;free(failKey);failKey=0;
}
void collStreamFree(void) {
  asyncDrop();asyOn=0;
  if(keys)for(unsigned i=0;i<ntypes;i++)free(keys[i]);
  if(vrec)for(unsigned i=0;i<ntypes;i++)free(vrec[i].buf);
  if(trec)for(unsigned k=0;k<ntexs;k++)if(trec[k].buf){if(collStreamTexUnbind)collStreamTexUnbind(k);free(trec[k].buf);}
  free(vrec);free(failVert);vrec=0;trec=0;failVert=0;ntexs=texSplit=0;collStreamTexBytes=0;memset(tneed,0,sizeof tneed);splitOn=0;collStreamVertBytes=0;
  free(keys);free(typeGag);keys=0;typeGag=0;ntypes=0;source=0;sourceUser=0;liveSp=0;
  collStreamResidentBytes=collStreamTrackBytes=0;
}
// Будка-93 (1257, SPAWNHOLD 7000 мс why=gag в эмуляторе): счёт «начато минус принято» не знал про отменённые и отказанные задания (jobCancel / jobFail без Done): после первой же отмены он оставался > 0
// навсегда (счётчики не сбрасывались и между уровнями), и ирис каждой комнаты ждал предел. Теперь ждём реальное: идущее чтение и то, что память позволяет начать (отказ аллокатора - не ждём: нечем читать).
// GAGPEND1257_OLD=1 / файл gagpend1257old / flag gagpend1257 0 - счёт как был
#ifndef GAGPEND1257_OLD
#define GAGPEND1257_OLD 0
#endif
int collGagPend1257Old=GAGPEND1257_OLD;
unsigned collStreamPending(void){   // Будка-93 (1226): чтений гэгов в пути (задание идёт) и ключей в очереди (ещё не начаты); 0 — всё, что просила комната, на месте
  unsigned n=collGagPend1257Old?collStreamAsyncStarts-collStreamAsyncDone:(job.busy?1u:0u);
  if(!collGagPend1257Old && !job.busy && allocBlocked)return 0;
  if(reqKey && liveSp)for(unsigned i=0;i<ntypes;i++){   // как условие выбора задания ниже: без дорожки, готовое и отказанное не ждём
    const SpType *t=liveSp->type+i;unsigned bytes=(unsigned)(t->frames&65535u)*(t->frames>>16)*28u;
    if(reqKey[i] && bytes && !keys[i] && !(failKey && failKey[i]))n++;
  }
  if(splitOn)for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && wantedLast[typeGag[i]] && !vrec[i].buf && vrec[i].hi>vrec[i].lo && !(failVert && failVert[i]))n++;   // вершины нужных гэгов ещё не пришли
  if(texSplit)for(unsigned k=0;k<ntexs;k++)if((tneed[k>>5]>>(k&31)&1) && !trec[k].buf && !(failVert && failVert[ntypes+k]))n++;   // текстуры нужных гэгов ещё не пришли
  return n;
}
static int loadKeys(const StateProps *sp,unsigned i) {
  const SpType *t=sp->type+i;
  unsigned bytes=(unsigned)(t->frames&65535u)*(t->frames>>16)*28u;
  if(!bytes || keys[i])return 0;
  keys[i]=alignedRoom(bytes);if(!keys[i])return -1;
  if(source(sourceUser,keyOffset+t->keyOff*28u,keys[i],bytes)){free(keys[i]);keys[i]=0;return -1;}
  collStreamTrackBytes+=bytes;return 0;
}
static void vertsWriteback(void *p,unsigned n){
#ifndef SKIN_HOST
  sceKernelDcacheWritebackRange(p,n);
#else
  (void)p;(void)n;
#endif
}
static int loadTex(unsigned k);
static int loadVerts(unsigned i){   // синхронно: вершины типа i из файла (общий блоб вершин лежит в файле как есть, вершины типа - сплошной отрезок) и его текстуры
  if(!splitOn)return 0;
  for(unsigned w=0;w<COLL_STREAM_TW;w++){unsigned m=vrec[i].tmask[w];for(unsigned b=0;m;b++,m>>=1)if((m&1) && loadTex(w*32+b))return -1;}
  if(vrec[i].buf || vrec[i].hi<=vrec[i].lo)return 0;
  if(failVert && failVert[i])return -1;
  unsigned n=vrec[i].hi-vrec[i].lo;
  unsigned char *b=alignedRoom(n);if(!b)return -1;
  if(source(sourceUser,vertFileOff+vrec[i].lo,b,n)){free(b);return -1;}
  vertsWriteback(b,n);vrec[i].buf=b;collStreamVertBytes+=n;collStreamVertLoads++;return 0;
}
static int loadTex(unsigned k){   // синхронно: запись текстуры k (формат SHR7: заголовок 16 Б, палитра 1024 Б, уровни swizzle)
  if(!texSplit || trec[k].buf)return 0;
  if(failVert && failVert[ntypes+k])return -1;
  unsigned char *b=alignedRoom(trec[k].size);if(!b)return -1;
  if(source(sourceUser,trec[k].off,b,trec[k].size)){free(b);return -1;}
  vertsWriteback(b,trec[k].size);trec[k].buf=b;collStreamTexBytes+=trec[k].size;
  if(collStreamTexBind)collStreamTexBind(k,b,trec[k].size);return 0;
}
static void freeTex(unsigned k){
  if(!trec[k].buf)return;
  if(collStreamTexUnbind)collStreamTexUnbind(k);   // сначала движок забывает адрес (и кэш привязки GE), потом память отдаётся
  collStreamTexBytes-=trec[k].size;free(trec[k].buf);trec[k].buf=0;
}
void collStreamTexRebind(void){if(texSplit&&collStreamTexBind)for(unsigned k=0;k<ntexs;k++)if(trec[k].buf)collStreamTexBind(k,trec[k].buf,trec[k].size);}
static int typeTexReady(unsigned t){
  if(!texSplit)return 1;
  for(unsigned w=0;w<COLL_STREAM_TW;w++){unsigned m=vrec[t].tmask[w];
    for(unsigned b=0;m;b++,m>>=1)if((m&1) && !trec[w*32+b].buf)return 0;}
  return 1;
}
// База вершин типа для piece->vOff (общий блоб): base+vOff попадает в буфер типа. 0 - вершин или текстур типа сейчас нет (не рисовать; придут вместе с ключами)
const void *collStreamVerts(unsigned type){
  if(!splitOn)return liveSp?liveSp->vertBase:0;
  if(type>=ntypes)return 0;
  if(!typeTexReady(type))return 0;
  if(vrec[type].hi<=vrec[type].lo)return (const void *)(uintptr_t)0x10;   // у типа нет кусков с вершинами
  if(!vrec[type].buf)return 0;
  return (const void *)((uintptr_t)vrec[type].buf-(uintptr_t)vrec[type].lo);
}
int collStreamSplitOn(void){return splitOn;}
int collStreamTexSplitOn(void){return texSplit!=0;}
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
// Build 95 (Опора-93, Z1): пакет гэгов (1,0-1,7 МБ) - в блок уровня (lvaAllocSoft), а не в кучу; движок ставит хуки, блок отдаёт lvaFree (участок или куча). Хуки не заданы - прежнее
void *(*collStreamAllocHook)(unsigned n);void (*collStreamFreeHook)(void *p);
static unsigned char *packAlloc(unsigned n){void *p=collStreamAllocHook?collStreamAllocHook(n):0;return p?(unsigned char *)p:(unsigned char *)aligned(n);}
static void packFree(void *p){if(collStreamFreeHook)collStreamFreeHook(p);else free(p);}
void *collStreamLoad(Collect *c,CollectRead read,void *user,unsigned size) {
  collStreamFree();
  unsigned h[20],s[16];
  if(size<80 || read(user,0,h,80) || memcmp(h,"CLT1",4) || h[1]<3 || h[5]>size-64 || read(user,h[5],s,64) || memcmp(s,"SPR1",4))return 0;
  if(h[6]>size-h[5] || h[14]>size/64u || s[12]>size || s[13]>size ||
      s[1]>size/84u || s[3]>size/40u || s[4]>size/52u || s[5]>size/28u)return 0;
  unsigned vkb=collSplitOld?0u:A16(s[12]);   // вершины гэгов (s[12] байт после 64 Б заголовка SPR1) - вне блока
  uint64_t off=(uint64_t)h[5]+64+A16(s[12])+A16(s[13])+A16(s[3]*40u)+A16(s[1]*84u)+A16(s[4]*52u);
  uint64_t keyBytes=(uint64_t)s[5]*28u;keyBytes=(keyBytes+15u)&~15ull;
  uint64_t end=(uint64_t)h[15]+(uint64_t)h[14]*64u;
  if(off+keyBytes>size || end>size || end<off+keyBytes || keyBytes>h[6] || (off&15) || (h[15]&15))return 0;
  // текстуры гэгов ([h[4],h[5]) файла: записи подряд) - тоже вне блока, если все записи прошли разбор по заголовкам ровно до начала SPR1
  unsigned tb=0;TexRec *tr=0;unsigned nt=h[3];
  if(vkb && nt && nt<=COLL_STREAM_TW*32u && h[4]>=80 && h[4]<h[5] && !(h[4]&15)){
    tr=calloc(nt,sizeof *tr);unsigned p=h[4],k=0;
    if(tr)for(;k<nt;k++){
      unsigned char t[16];unsigned w,hh,lv,t4,pre,q;
      if(p+16+1024>h[5] || read(user,p,t,16))break;
      w=t[0]|t[1]<<8;hh=t[2]|t[3]<<8;lv=t[4]|t[5]<<8;pre=t[7]==1;t4=t[6]==4&&pre;
      if(!w||!hh||!lv||lv>16)break;
      q=p+16+1024;unsigned lw=w,lh=hh;
      for(unsigned l=0;l<lv;l++){unsigned rb=t4?(((lw+1)>>1)<16?16:(lw+1)>>1):(pre&&lw<16?16:lw);q+=rb*(pre?(lh+7)&~7u:lh);q=A16(q);lw>>=1;lh=lh>1?lh>>1:1;}
      if(q>h[5])break;
      tr[k].off=p;tr[k].size=q-p;p=q;
    }
    if(tr && k==nt && p==h[5])tb=h[5]-h[4];else{free(tr);tr=0;}
  }
  unsigned n=(unsigned)(end-keyBytes-vkb-tb),kb=(unsigned)keyBytes+vkb+tb;
  unsigned char *b=packAlloc(n);if(!b){free(tr);return 0;}
  unsigned vp=h[5]+64,aLen=tb?h[4]:h[5],tail=(unsigned)off-vp-vkb;   // vp - начало вершин, aLen - голова файла до текстур (или до SPR1), tail - остаток таблиц до ключей
  if(read(user,0,b,aLen) || read(user,h[5],b+aLen,64) || (tail && read(user,vp+vkb,b+aLen+64,tail)) || read(user,(unsigned)(off+keyBytes),b+aLen+64+tail,(unsigned)(end-off-keyBytes))){packFree(b);free(tr);return 0;}
  unsigned *u=(unsigned *)b;
  u[5]-=tb;u[6]-=keyBytes+vkb;u[8]-=kb;u[10]-=kb;u[12]-=kb;u[15]-=kb;
  if(collectInitResident(c,b,n,vkb?3:1)){packFree(b);free(tr);return 0;}
  collStreamVertSaved=vkb;collStreamTexSaved=tb;
  source=read;sourceUser=user;keyOffset=(unsigned)off;ntypes=c->sp.ntype;
  keys=calloc(ntypes,sizeof *keys);typeGag=malloc(ntypes*sizeof *typeGag);
  if(!keys || !typeGag)goto bad;
  for(unsigned i=0;i<ntypes;i++)typeGag[i]=-1;
  for(unsigned g=0;g<c->ngag;g++)if(c->gag[g].inst>=0)typeGag[c->sp.inst[c->gag[g].inst].type]=(int)g;
  c->sp.typeKey=(const float *const *)keys;c->sp.ensureKey=ensureKey;liveSp=&c->sp;
  if(vkb){   // диапазоны вершин по типам (куски типа лежат подряд, типы не пересекаются: tools/collect_check.py)
    vrec=calloc(ntypes,sizeof *vrec);if(!vrec)goto bad;
    if(tb){trec=tr;ntexs=nt;texSplit=1;tr=0;}else{free(tr);tr=0;}
    for(unsigned t=0;t<ntypes;t++){
      const SpType *T=c->sp.type+t;unsigned lo=0xFFFFFFFFu,hi=0;
      for(unsigned k=0;k<T->nPiece;k++){const SpPiece *P=c->sp.piece+T->firstPiece+k;if(!P->nv)continue;
        if(P->vOff<lo)lo=P->vOff;if(P->vOff+P->nv*16u>hi)hi=P->vOff+P->nv*16u;}
      if(hi>lo){vrec[t].lo=lo;vrec[t].hi=hi;}
      if(texSplit){   // текстуры типа: куски и билборды (номер в пакете = tex - COLL_CTEX0)
        for(unsigned k=0;k<T->nPiece;k++){int x=c->sp.piece[T->firstPiece+k].tex-COLL_CTEX0;if(x>=0&&(unsigned)x<ntexs)vrec[t].tmask[x>>5]|=1u<<(x&31);}
        for(unsigned k=0;k<T->nBill;k++){int x=c->sp.bill[T->firstBill+k].tex-COLL_CTEX0;if(x>=0&&(unsigned)x<ntexs)vrec[t].tmask[x>>5]|=1u<<(x&31);}
      }
    }
    vertFileOff=vp;splitOn=1;
  }
  if(texSplit)for(unsigned t=0;t<ntypes;t++)if(typeGag[t]<0)for(unsigned k=0;k<ntexs;k++)if(vrec[t].tmask[k>>5]>>(k&31)&1)trec[k].perm=1;   // текстуры типов без гэга (карточки, осы, эффекты героя) живут весь уровень
  for(unsigned i=0;i<ntypes;i++)if(typeGag[i]<0 && (loadKeys(&c->sp,i) || loadVerts(i)))goto bad;
  if(gagSkinInitSource(c,read,user,size)<0)goto bad;
  collStreamResidentBytes=n;return b;
bad:free(tr);gagSkinFree();collectFree(c);collStreamFree();packFree(b);return 0;
}
int collStreamPrepare(Collect *c,int room,float ex,float ez,float distance,int snap) {
  curRoom=room;
  if(!source)return 0;
  unsigned char wanted[COLL_MAXGAG]={0};
  for(unsigned g=0;g<c->ngag;g++){
    const CollGag *a=c->gag+g;float dx=a->x-ex,dz=a->z-ez;
    // Same draw set, with a 10 metre prefetch margin (port I/O policy).
    float R=distance+10.f;
    if(splitOn && room<0){   // оригинал: уличный гэг строится ближе 100 м и снимается дальше 150 м (между - как был). Роб 08.11: на карте постоянно не нужны
      float rl=R<GAG_LOAD_M?R:GAG_LOAD_M,rk=R<GAG_KEEP_M?R:GAG_KEEP_M;
      R=wantedLast[g]?rk:rl;
    }
    wanted[g]=a->room==room && (!a->random || (snap>=0 ? (int)g==snap : c->st.gagPick[g])) &&
        dx*dx+dz*dz<=R*R;
  }
  memcpy(wantedLast,wanted,sizeof wantedLast);
  for(unsigned g=0;g<c->ngag;g++)c->gagBlock[g]=splitOn && c->gag[g].inst>=0 && !collStreamVerts(c->sp.inst[c->gag[g].inst].type);
  for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && !wanted[typeGag[i]] && keys[i]){
    const SpType *t=c->sp.type+i;collStreamTrackBytes-=(t->frames&65535u)*(t->frames>>16)*28u;
    free(keys[i]);keys[i]=0;
    for(unsigned j=0;j<c->sp.ninst;j++)if(c->sp.inst[j].type==i)c->sp.run[j].poseValid=0;
  }
  if(texSplit){   // текстуры: нужны желаемым гэгам; ушедшим - отдать
    memset(tneed,0,sizeof tneed);
    for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && wanted[typeGag[i]])for(unsigned w=0;w<COLL_STREAM_TW;w++)tneed[w]|=vrec[i].tmask[w];
    for(unsigned k=0;k<ntexs;k++)if(trec[k].buf && !trec[k].perm && !(tneed[k>>5]>>(k&31)&1))freeTex(k);
  }
  if(splitOn)for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && !wanted[typeGag[i]] && vrec[i].buf){   // вершины ушедшего гэга: как ключи и кожи
    if(job.busy && job.kind==2 && job.i==i)continue;
    collStreamVertBytes-=vrec[i].hi-vrec[i].lo;free(vrec[i].buf);vrec[i].buf=0;
  }
  if(asyOn && snap<0){   // фоновый путь: в кадре — запуск и усвоение, чтение не ждём (снимок гэга snap>=0 — синхронно, как всегда: проверки ждут кадр)
    gagSkinTrim(wanted);
    if(job.busy){   // задание уже не нужно (ушли далеко, перенос, конец миссии) — отменить сразу
      int needed=job.kind==3 ? (tneed[job.i>>5]>>(job.i&31)&1) : job.kind!=1 ? ((typeGag[job.i]>=0 && wanted[typeGag[job.i]]) || (job.kind==0 && typeGag[job.i]<0)) : (gagSkinGagOf(job.i)>=0 && wanted[gagSkinGagOf(job.i)]);
      if(!needed){jobCancel();return 0;}
      if(collHoldMs && nowMs()-job.t0<collHoldMs)return 0;   // служебное (красный контроль): чтение задерживается, гэг в радиусе не успевает
      int r=asy.poll(asy.user);
      if(r==0){   // не готово — подождать кадр, но не дольше COLL_ASYNC_MS
        if(asy.nowMs && nowMs()-job.t0>COLL_ASYNC_MS){jobFail();jobCancel();return -1;}
        return 0;
      }
      int ok=0;
      if(r>0){
        if(job.kind==0){keys[job.i]=(float *)job.buf;collStreamTrackBytes+=job.bytes;ok=1;}
        else if(job.kind==3){vertsWriteback(job.buf,job.bytes);trec[job.i].buf=job.buf;collStreamTexBytes+=job.bytes;if(collStreamTexBind)collStreamTexBind(job.i,job.buf,job.bytes);ok=1;}
        else if(job.kind==2){vertsWriteback(job.buf,job.bytes);vrec[job.i].buf=job.buf;collStreamVertBytes+=job.bytes;collStreamVertLoads++;ok=1;}
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
      unsigned char *b=alignedRoom(bytes);if(!b){allocBlocked=1;return -1;}
      if(asy.start(asy.user,keyOffset+t->keyOff*28u,b,bytes)){free(b);return -1;}
      job.busy=1;job.kind=0;job.i=i;job.bytes=bytes;job.buf=b;job.t0=nowMs();collStreamAsyncStarts++;allocBlocked=0;
    }
    for(unsigned i=0;i<ntypes && splitOn && !job.busy;i++){   // вершины нужных гэгов: после ключей, до кож
      if(typeGag[i]<0 || !wanted[typeGag[i]] || vrec[i].buf || vrec[i].hi<=vrec[i].lo || (failVert && failVert[i]))continue;
      unsigned n=vrec[i].hi-vrec[i].lo;unsigned char *b=alignedRoom(n);if(!b){allocBlocked=1;return -1;}
      if(asy.start(asy.user,vertFileOff+vrec[i].lo,b,n)){free(b);return -1;}
      job.busy=1;job.kind=2;job.i=i;job.bytes=n;job.buf=b;job.t0=nowMs();collStreamAsyncStarts++;allocBlocked=0;
    }
    for(unsigned k=0;k<ntexs && texSplit && !job.busy;k++){   // текстуры нужных гэгов
      if(!(tneed[k>>5]>>(k&31)&1) || trec[k].buf || (failVert && failVert[ntypes+k]))continue;
      unsigned char *b=alignedRoom(trec[k].size);if(!b){allocBlocked=1;return -1;}
      if(asy.start(asy.user,trec[k].off,b,trec[k].size)){free(b);return -1;}
      job.busy=1;job.kind=3;job.i=k;job.bytes=trec[k].size;job.buf=b;job.t0=nowMs();collStreamAsyncStarts++;allocBlocked=0;
    }
    unsigned gi,goff,gsz;
    if(!job.busy && gagSkinNext(wanted,&gi,&goff,&gsz)){
      unsigned char *b=alignedRoom(gsz);if(!b){allocBlocked=1;return -1;}
      if(asy.start(asy.user,goff,b,gsz)){free(b);return -1;}
      job.busy=1;job.kind=1;job.i=gi;job.bytes=gsz;job.buf=b;job.t0=nowMs();collStreamAsyncStarts++;allocBlocked=0;
    }
    return 0;
  }
  for(unsigned i=0;i<ntypes;i++)if(typeGag[i]>=0 && wanted[typeGag[i]] && (loadKeys(&c->sp,i) || loadVerts(i)))return -1;
  return gagSkinPrepare(c,wanted);
}
