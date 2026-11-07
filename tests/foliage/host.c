// Vehicle impacts against the original bushes by the first neighbourhood glass shortcut; landings on a bush (item 7).
// Правка Роба 71 (повтор 01.10, «кусты выбрасывают в разные стороны; в оригинале с кустами проблем не было»): куст —
// неподвижная мягкая преграда по данным (ATC 10: упругость 18 %, трение 195 %), подскоков нет ни сбоку, ни при посадке;
// прежние ожидания «куст подбрасывает» перевёрнуты. Красный контроль: сборка с -DFOLIAGE_PREV (прежняя реакция) падает.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "car.h"
static ColWorld w;
static Car car;
static int isolated(const ColCyl *source) {
  struct { ColHdr h; ColTri t[2]; ColCyl c; unsigned start[2],refs[3]; } fixture={0};
  memcpy(fixture.h.magic,"COL1",4);fixture.h.ox=fixture.h.oz=-30;fixture.h.cell=60;
  fixture.h.W=fixture.h.H=1;fixture.h.ntri=2;fixture.h.ncyl=1;fixture.h.nref=3;
  fixture.t[0]=(ColTri){{-30,0,-30},{30,0,30},{30,0,-30},{0,1,0}};
  fixture.t[1]=(ColTri){{-30,0,-30},{-30,0,30},{30,0,30},{0,1,0}};
  fixture.c=*source;fixture.c.c[0]=fixture.c.c[2]=0;fixture.c.c[1]=1;
  fixture.start[1]=3;fixture.refs[0]=0;fixture.refs[1]=1;fixture.refs[2]=COL_CYL<<29;
  ColWorld unit;if(colInit(&unit,&fixture,sizeof fixture))return 1;
  float nx=-source->axis[2],nz=source->axis[0],l=hypotf(nx,nz);nx/=l;nz/=l;
  int failures=0;
  for(int hard=0;hard<2;hard++)for(int side=-1;side<=1;side+=2)for(int fps=30;fps<=60;fps+=30){
    fixture.c.pad[0]=hard?0:1;
    carInit(&car,0,&unit,nx*side*6,0,nz*side*6,atan2f(-nx*side,-nz*side));carSetSpeed(&car,12);
    CarInput in={1,0,0,0};float height=0,maxvy=0;
    for(int n=0;n<fps*3;n++){carUpdate(&car,&unit,&in,1.f/fps);height=fmaxf(height,car.y);maxvy=fmaxf(maxvy,car.vel[1]);}
    printf("isolated %s side=%d fps=%d hops=%u height=%.3f max_vy=%.3f\n",hard?"hard":"foliage",side,fps,car.foliageBounces,height,maxvy);
    if(!isfinite(height)||height>3.5f||maxvy>9.f)failures++;
    if(car.foliageBounces)failures++;                                   // подскоков нет ни у твёрдого, ни у куста
    if(!hard&&(height>.3f||maxvy>1.f))failures++;                      // куст гасит удар: не подбрасывает (было 1.5 м, 6.8 м/с)
    {float s=(car.x*nx+car.z*nz)*side;                                    // правка 173: куст машина проезжает насквозь, твёрдое — нет
     // правка 421 (09.10): 173 — только про кусты; изгородь (лежачая капсула листвы, l*_hedgeunit) у оригинала держит машину
     // (сверка PS2: выталкивает из капсулы, разгон встаёт) — не проезжается. Куст (стоячая листва, l2/l5_shrubsml) — насквозь
#ifndef FOLIAGE_PS2_1154_OLD
#define FOLIAGE_PS2_1154_OLD 0
#endif
     int hedge=!FOLIAGE_PS2_1154_OLD;   // №1204 (Роб, 05.11, запись PS2 L7 J9-5): лежачая капсула изгороди держит машину в лоб (col.c FOLIAGE_PS2_1154_OLD=1 — проезжается, как в TEST85…90)
     if(!hard&&!hedge&&s>-2.f){printf("FAIL foliage not crossed: side=%d fps=%d s=%.2f\n",side,fps,s);failures++;}
     if(!hard&&hedge&&s<0.f){printf("FAIL hedge crossed: side=%d fps=%d s=%.2f\n",side,fps,s);failures++;}
     if(hard&&s<0.f){printf("FAIL hard crossed: side=%d fps=%d s=%.2f\n",side,fps,s);failures++;}}
  }
  colFree(&unit);return failures;
}
// Посадка днищем (пункт 7): падение с 2/5/10 м на верх того же куста; тот же куст с материалом HARD — отрицательный
// контроль (посадка твёрдая, 0 подбросов). С правки 71 (01.10) подскоков нет и у куста, отскок не выше твёрдого.
// Подброс = наивысшая точка после первого касания минус высота касания.
static float drop(ColWorld *unit,float top,float h,int fps,unsigned *hops,float *apex1){
  carInit(&car,0,unit,0,top,0,0.3f);car.y+=h;car.ry=car.y;         // carInit ставит на опору: поднимаем, как sandbox_fixture
  CarInput in={0};float y0=-1,peak=-1,yh=0,a1=0;int air=1,phase=0;   // phase 1: в первом подбросе, 2: он закончился
  for(int n=0;n<fps*4;n++){
    carUpdate(&car,unit,&in,1.f/fps);
    if(air&&car.grounded){air=0;if(y0<0)y0=car.y;}
    if(y0>=0&&car.y>peak)peak=car.y;                                  // наивысшая точка после первого касания
    // вершина первого подброса от верха куста (не от конца кадра: на 30 и 60 кадрах он в разное время)
    if(phase==0&&car.foliageBounces>=1){phase=1;yh=top;a1=car.y-yh;}
    else if(phase==1){if(car.y-yh>a1)a1=car.y-yh;if(car.grounded&&car.vel[1]<=0.f&&car.y-yh<a1-.05f)phase=2;}
  }
  *hops=car.foliageBounces;if(apex1)*apex1=a1;return y0<0?-1.f:peak-y0;
}
static int landings(const ColCyl *source){
  int failures=0;float hardB[3][2]={{0}},folB[3][2]={{0}};
  // 1) строгий гейт: плоский ящик-куст 5x2x5 м (как 9 ящиков FOLIAGE уровня) против такого же твёрдого
  struct { ColHdr h; ColTri t[2]; ColBox b; unsigned start[2],refs[3]; } bx={0};
  memcpy(bx.h.magic,"COL1",4);bx.h.ox=bx.h.oz=-30;bx.h.cell=60;bx.h.W=bx.h.H=1;bx.h.ntri=2;bx.h.nbox=1;bx.h.nref=3;
  bx.t[0]=(ColTri){{-30,0,-30},{30,0,30},{30,0,-30},{0,1,0}};
  bx.t[1]=(ColTri){{-30,0,-30},{-30,0,30},{30,0,30},{0,1,0}};
  bx.b=(ColBox){{0,1,0},{1,0,0},{0,1,0},{0,0,1},{2.5f,1,2.5f},0};
  bx.start[1]=3;bx.refs[0]=0;bx.refs[1]=1;bx.refs[2]=COL_BOX<<29;
  for(int hard=0;hard<2;hard++){
    bx.b.pad=hard?0.f:1.f;
    ColWorld unit;if(colInit(&unit,&bx,sizeof bx))return 1;
    float top;if(!colGround(&unit,0,0,50,-50,&top,0)){colFree(&unit);return 1;}
    for(int hi=0;hi<3;hi++){
      float h=hi==0?2.f:hi==1?5.f:10.f;unsigned h30,h60;float a30,a60;
      float b30=drop(&unit,top,h,30,&h30,&a30),b60=drop(&unit,top,h,60,&h60,&a60);
      printf("landing box %s drop=%.0fm first_hop30=%.3f first_hop60=%.3f bounce30=%.3f bounce60=%.3f hops=%u/%u\n",
             hard?"hard":"foliage",h,a30,a60,b30,b60,h30,h60);
      if(hard&&(b30<0||b60<0))failures++;                                 // на твёрдое садится
      if(!hard&&(b30>=0||b60>=0)){printf("FAIL landed on a bush top (pass-through expected)\n");failures++;}   // правка 173: куст насквозь до земли
      if(h30||h60)failures++;                                             // подскоков нет: ни твёрдо, ни куст (правка 71)
      if(hard)hardB[hi][0]=b30,hardB[hi][1]=b60;else folB[hi][0]=b30,folB[hi][1]=b60;
    }
    colFree(&unit);
  }
  // посадка на куст не выше посадки на тот же твёрдый ящик (куст не пружина)
  for(int hi=0;hi<3;hi++)for(int f=0;f<2;f++)if(folB[hi][f]>hardB[hi][f]+.01f){printf("FAIL foliage landing bouncier than hard (%d %d)\n",hi,f);failures++;}
  // 2) наблюдение на настоящей изгороди (капсула r 0.7, 6 м): машина вдоль оси, на середину; круглый верх —
  // машина скатывается, поэтому высоты не гейт, требуется только: куст подбрасывает, твёрдая — нет
  struct { ColHdr h; ColTri t[2]; ColCyl c; unsigned start[2],refs[3]; } fixture={0};
  memcpy(fixture.h.magic,"COL1",4);fixture.h.ox=fixture.h.oz=-30;fixture.h.cell=60;
  fixture.h.W=fixture.h.H=1;fixture.h.ntri=2;fixture.h.ncyl=1;fixture.h.nref=3;
  fixture.t[0]=(ColTri){{-30,0,-30},{30,0,30},{30,0,-30},{0,1,0}};
  fixture.t[1]=(ColTri){{-30,0,-30},{-30,0,30},{30,0,30},{0,1,0}};
  fixture.c=*source;fixture.c.c[0]=fixture.c.c[2]=0;fixture.c.c[1]=1;
  fixture.start[1]=3;fixture.refs[0]=0;fixture.refs[1]=1;fixture.refs[2]=COL_CYL<<29;
  for(int hard=0;hard<2;hard++){
    fixture.c.pad[0]=hard?0:1;
    ColWorld unit;if(colInit(&unit,&fixture,sizeof fixture))return 1;
    float top;if(!colGround(&unit,0,0,50,-50,&top,0)){colFree(&unit);return 1;}
    for(int hi=0;hi<3;hi++){
      float h=hi==0?2.f:hi==1?5.f:10.f;unsigned h60;float b60=drop(&unit,top,h,60,&h60,0);
      printf("landing hedge %s drop=%.0fm bounce60=%.3f hops=%u (observation)\n",hard?"hard":"foliage",h,b60,h60);
      if(h60!=0)failures++;
    }
    colFree(&unit);
  }
  return failures;
}
int main(int argc,char **argv) {
  if(argc!=2)return 2;FILE *f=fopen(argv[1],"rb");if(!f)return 2;
  fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
  unsigned char *data=aligned_alloc(16,(size+15)&~15);if(!data||fread(data,1,size,f)!=(size_t)size)return 2;fclose(f);
  unsigned co=*(unsigned*)(data+92),po=*(unsigned*)(data+96),pv=*(unsigned*)(data+88),ce=(unsigned)size;
  if(po>co&&po<ce)ce=po;if(pv>co&&pv<ce)ce=pv;
  if(colInit(&w,data+co,ce-co))return 2;
  unsigned count=0,best=0;float dmin=1e20f;
  for(unsigned i=0;i<w.ncyl;i++)if(w.cyl[i].pad[0]==1.f){
    count++;float d=hypotf(w.cyl[i].c[0]-161.64f,w.cyl[i].c[2]-150.56f);if(d<dmin){dmin=d;best=i;}
  }
  if(!count||dmin>5){printf("FAIL source bush fixture missing\n");return 1;}
  ColCyl *c=(ColCyl*)&w.cyl[best];float nx=-c->axis[2],nz=c->axis[0],l=hypotf(nx,nz);nx/=l;nz/=l;
  int failures=isolated(c);
  int landF=landings(c);failures+=landF;
  // та же крыша рядом с кустом на реальном уровне: ближайшая твёрдая опора выше земли у куста на 1.5 м и больше
  {float g0;colGround(&w,c->c[0],c->c[2],c->c[1]+30,-100,&g0,0);int roofs=0;
   for(float r=4;r<40&&!roofs;r+=1)for(int a=0;a<16&&!roofs;a++){
     float x=c->c[0]+r*cosf(a*.3927f),z=c->c[2]+r*sinf(a*.3927f),gy,ng[3];int mat;
     if(!colGroundMat(&w,x,z,g0+30,g0+1.5f,&gy,ng,&mat)||mat!=COL_MAT_HARD||ng[1]<.9f)continue;
     roofs=1;carInit(&car,0,&w,x,gy,z,0.3f);car.y+=5.f;car.ry=car.y;CarInput in={0};float y0=-1,peak=-1;int air=1;
     for(int n=0;n<240;n++){carUpdate(&car,&w,&in,1.f/60);if(air&&car.grounded){air=0;if(y0<0)y0=car.y;}
       if(y0>=0&&car.y>peak)peak=car.y;}
     printf("hard roof near the bush (%.1f %.1f y %.2f): drop 5m bounce=%.3f hops=%u\n",x,z,gy,peak-y0,car.foliageBounces);
     if(car.foliageBounces||peak-y0>.15f)failures++;}
   if(!roofs){printf("FAIL no hard roof near the bush\n");failures++;}}
  for(int hard=0;hard<2;hard++)for(int side=-1;side<=1;side+=2)for(int speed=0;speed<=14;speed+=7){
    // The hard-material run is a negative control using exactly the same collision geometry.
    for(unsigned i=0;i<w.ncyl;i++)if(w.cyl[i].pad[0]==1.f||w.cyl[i].pad[0]==2.f)((ColCyl*)&w.cyl[i])->pad[0]=hard?2.f:1.f;
    float x=c->c[0]+nx*side*(c->r+5),z=c->c[2]+nz*side*(c->r+5),g;
    if(!colGround(&w,x,z,c->c[1]+2.f,-100,&g,0)){
      int higher=colGround(&w,x,z,100,-100,&g,0);
      printf("no low ground xz=%.3f,%.3f top=%.3f higher=%d y=%.3f source=%u\n",x,z,c->c[1]+.2f,higher,higher?g:0,best);return 2;
    }
    carInit(&car,0,&w,x,g,z,atan2f(-nx*side,-nz*side));carSetSpeed(&car,(float)speed);
    float peak=car.y,maxvy=0;CarInput in={speed?1.f:0,0,0,speed?0.f:1.f};
    // окно замера, как у hedges.c: центр машины ближе r + полудлина + 1.5 м к оси куста. Правка 304: без ложной вязкости машина
    // на газу доезжает до стены дома в 6.7 м за кустом (удар 16.8 м/с, подброс 3.7 м/с) — это удар о стену, не реакция куста
    float axl=hypotf(c->axis[0],c->axis[2]),ux=c->axis[0]/axl,uz=c->axis[2]/axl,hl=c->halfLen*axl,D=c->r+car.p.halfL+1.5f;
    for(int n=0;n<180;n++){carUpdate(&car,&w,&in,1.f/60.f);
      float qx=car.x-c->c[0],qz=car.z-c->c[2],u=qx*ux+qz*uz;u=u>hl?hl:u<-hl?-hl:u;
      if(hypotf(qx-ux*u,qz-uz*u)<=D){if(car.y>peak)peak=car.y;if(car.vel[1]>maxvy)maxvy=car.vel[1];}
      if(!isfinite(car.y)||!isfinite(car.vel[1])||fabsf(car.vel[1])>30)failures++;
    }
    printf("%s side=%d speed=%d hops=%u height=%.3f max_vy=%.3f damage=%.3f reverts=%d\n",hard?"hard":"foliage",side,speed,car.foliageBounces,peak-g,maxvy,car.damage,car.reverts);
    if(hard||speed==0){if(car.foliageBounces)failures++;}
    // куст на уровне: удар гасится, не подбрасывает (было до 1.5 м и 6.9 м/с)
    else if(car.foliageBounces||peak-g>1.f||maxvy>2.5f)failures++;
  }
  printf("%s foliage tests, %u tagged capsules\n",failures?"FAIL":"PASS",count);
  colFree(&w);free(data);return failures?1:0;
}
