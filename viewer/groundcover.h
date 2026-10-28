#ifndef SHAR_GROUNDCOVER_H
#define SHAR_GROUNDCOVER_H
#include <math.h>
#include "col.h"
#ifndef GROUND_COVER_OLD
#define GROUND_COVER_OLD 0
#endif
// Rob №437: visual underlay, never collision. Colours come from textured ground.
typedef struct { float y; unsigned rgb; int ready; } GroundCover;
typedef struct { unsigned c; float x,y,z; } GroundCoverVertex;
// Правка 508: перебор всех треугольников мира стоял на экране загрузки без кадров (141 мс в эмуляторе) — groundCoverPump (ставит загрузчик) зовут каждые 8192 треугольников
static void (*groundCoverPump)(void);
static inline void groundCoverReset(GroundCover *g, const ColWorld *w) {
  g->ready=0; g->y=0;
  float bottom=1e30f;
  if(w)for(unsigned i=0;i<w->ntri;i++) {
    const ColTri *t=&w->tri[i]; if(groundCoverPump&&!(i&8191))groundCoverPump();
    for(int k=0;k<3;k++){const float*p=k==0?t->a:k==1?t->b:t->c;if(p[1]<bottom)bottom=p[1];}
  }
  if(w)for(unsigned i=0;i<w->nbox;i++) {
    const ColBox*b=&w->box[i];float y=b->c[1]-fabsf(b->ax[1])*b->h[0]-fabsf(b->ay[1])*b->h[1]-fabsf(b->az[1])*b->h[2];
    if(y<bottom)bottom=y;
  }
  if(w)for(unsigned i=0;i<w->ncyl;i++) {
    const ColCyl*b=&w->cyl[i];float y=b->c[1]-fabsf(b->axis[1])*b->halfLen-b->r;
    if(y<bottom)bottom=y;
  }
  if(bottom<1e30f)g->y=bottom-.25f; // port rule: below all finite solid bounds, not an infinite fence extrusion
}
// №1042 (Роб: «никаких белых стен, никакого тумана, ни белого, ни цвета неба»): дымка края дальности выключена совсем — подложка рисуется без тумана, город чисто до выбранной дальности, за ним небо и силуэты.
// FOGOFF1042_OLD=1 / файл fogoff1042old / «flag fogoff1042 0» — дымка подложки как в TEST72 (от .75 дальности, цвет неба или средний цвет подложки)
#ifndef FOGOFF1042_OLD
#define FOGOFF1042_OLD 0
#endif
static int fogOff1042On = !FOGOFF1042_OLD;
static inline int groundCoverFog(const GroundCover *g){return !GROUND_COVER_OLD&&g->ready&&!fogOff1042On;}
// Infinite under-world plane projected as its horizon half-plane. A finite quad
// would miss distant cracks: its ray intersection can be beyond the far clip.
// Depth 3 is behind city geometry and ahead of sky's authored depth range 2..0.
static inline int groundCoverBackground(const GroundCover *g,float fy,float ry,float uy,float focal,GroundCoverVertex*v) {
  if(GROUND_COVER_OLD||!g->ready||!isfinite(focal)||focal<=0)return 0;
  float p[4][2]={{0,0},{480,0},{480,272},{0,272}};int count=0;
  for(int i=0;i<4;i++){
    float* a=p[i],*b=p[(i+1)%4];
    float da=fy+(a[0]-240)*ry/focal-(a[1]-136)*uy/focal;
    float db=fy+(b[0]-240)*ry/focal-(b[1]-136)*uy/focal;
    if(da<=0)v[count++]=(GroundCoverVertex){g->rgb|0xff000000u,a[0],a[1],3};
    if((da<=0)!=(db<=0)){float t=da/(da-db);v[count++]=(GroundCoverVertex){g->rgb|0xff000000u,a[0]+t*(b[0]-a[0]),a[1]+t*(b[1]-a[1]),3};}
  }
  return count;
}
#endif
