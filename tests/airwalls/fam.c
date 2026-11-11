// Семейство: коллизия (ящики COL1) в воздухе без видимой геометрии рядом. Сборка: cc -std=c11 -O2 -w -DSKIN_HOST -I viewer tests/airwalls/fam.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -Wl,-undefined,dynamic_lookup -o fam; запуск: fam level.shr МЕТКА [радиус 1.5] [высота 1.0]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "props.h"
#include "stateprops.h"
__attribute__((weak)) unsigned homerSolidFrame = 0;
static Props props; static StateProps sp; static ColWorld world; static unsigned char *file; static long fileSize;
// ---------------------------------------------------------------- рисуемая геометрия (как tools/cartest/walls.c)
typedef struct { float bb[6]; int32_t tex; uint32_t flags, nv, ni, owner; float ox, oy, oz, sc; uint32_t off, nf, cell, pad[2]; } Rec;
typedef struct { float bb[6]; uint32_t first, n, owner, off, size, pad; } CellRec;
static float *T; static unsigned NT; static unsigned *gS, *gL; static int GW, GH; static float GOX, GOZ; static const float GC = 2.f;
static void loadRender(void) {
  uint32_t npc, tableOff, cellOff; memcpy(&npc, file + 8, 4); memcpy(&tableOff, file + 76, 4); memcpy(&cellOff, file + 84, 4);
  const Rec *recs = (const Rec *)(file + tableOff); const CellRec *cells = (const CellRec *)(file + cellOff);
  unsigned cap = 1 << 20; T = malloc(sizeof(float) * 9 * cap);
  for (unsigned i = 0; i < npc; i++) {
    const Rec *r = &recs[i]; if (r->owner != 0 || (r->flags & 4) || !(r->flags & 16)) continue;   // город, не небо, без индексов
    int vs = (r->flags & 32) ? 12 : 16; const uint8_t *b = file + cells[r->cell].off + r->off;
    for (unsigned k = 0; k + 2 < r->ni; k += 3) {
      if (NT == cap) { cap *= 2; T = realloc(T, sizeof(float) * 9 * cap); }
      for (int j = 0; j < 3; j++) {
        const int16_t *q = (const int16_t *)(b + (size_t)(k + j) * vs + (vs == 12 ? 6 : 8));
        T[9 * NT + 3 * j] = r->ox + q[0] * (r->sc / 32768.f); T[9 * NT + 3 * j + 1] = r->oy + q[1] * (r->sc / 32768.f);
        T[9 * NT + 3 * j + 2] = r->oz + q[2] * (r->sc / 32768.f);
      }
      NT++;
    }
  }
  GOX = world.ox; GOZ = world.oz; GW = (int)(world.W * world.cell / GC) + 1; GH = (int)(world.H * world.cell / GC) + 1;
  gS = calloc((size_t)GW * GH + 1, 4);
  for (int pass = 0; pass < 2; pass++) {
    for (unsigned t = 0; t < NT; t++) {
      const float *a = T + 9 * t;
      int cx0 = (int)floorf((fminf(a[0], fminf(a[3], a[6])) - GOX) / GC), cx1 = (int)floorf((fmaxf(a[0], fmaxf(a[3], a[6])) - GOX) / GC);
      int cz0 = (int)floorf((fminf(a[2], fminf(a[5], a[8])) - GOZ) / GC), cz1 = (int)floorf((fmaxf(a[2], fmaxf(a[5], a[8])) - GOZ) / GC);
      if (cx0 < 0) cx0 = 0; if (cz0 < 0) cz0 = 0; if (cx1 >= GW) cx1 = GW - 1; if (cz1 >= GH) cz1 = GH - 1;
      for (int z = cz0; z <= cz1; z++) for (int x = cx0; x <= cx1; x++) { size_t c = (size_t)z * GW + x; if (pass == 0) gS[c + 1]++; else gL[gS[c]++] = t; }
    }
    if (pass == 0) { for (size_t c = 0; c < (size_t)GW * GH; c++) gS[c + 1] += gS[c]; gL = malloc(sizeof(unsigned) * (gS[(size_t)GW * GH] + 1)); }
    else { for (size_t c = (size_t)GW * GH; c > 0; c--) gS[c] = gS[c - 1]; gS[0] = 0; }
  }
}

static int load(const char *path){FILE*f=fopen(path,"rb"); if(!f) return 2; fseek(f,0,SEEK_END); fileSize=ftell(f); rewind(f); file=aligned_alloc(16,(fileSize+15)&~15L); if(fread(file,1,fileSize,f)!=(size_t)fileSize) return 2; fclose(f);
  unsigned co=*(unsigned*)(file+92),po=*(unsigned*)(file+96),pv=*(unsigned*)(file+88),ce=(unsigned)fileSize; if(po>co&&po<ce)ce=po; if(pv>co&&pv<ce)ce=pv;
  if(colInit(&world,file+co,ce-co)||propsInit(&props,file+po,*(unsigned*)(file+po+40))||spInit(&sp,props.spr,props.sprSize)) return 2; loadRender(); return 0;}
static int nearTris(const float lo[3],const float hi[3]){ int cx0=(int)floorf((lo[0]-GOX)/GC),cx1=(int)floorf((hi[0]-GOX)/GC),cz0=(int)floorf((lo[2]-GOZ)/GC),cz1=(int)floorf((hi[2]-GOZ)/GC); if(cx0<0)cx0=0; if(cz0<0)cz0=0; if(cx1>=GW)cx1=GW-1; if(cz1>=GH)cz1=GH-1; int n=0;
  for(int z=cz0;z<=cz1;z++)for(int x=cx0;x<=cx1;x++){size_t c=(size_t)z*GW+x; for(unsigned k=gS[c];k<gS[c+1];k++){const float*a=T+9*gL[k]; float mn[3],mx[3]; for(int d=0;d<3;d++){mn[d]=fminf(a[d],fminf(a[3+d],a[6+d])); mx[d]=fmaxf(a[d],fmaxf(a[3+d],a[6+d]));}
    if(mx[0]>=lo[0]&&mn[0]<=hi[0]&&mx[1]>=lo[1]&&mn[1]<=hi[1]&&mx[2]>=lo[2]&&mn[2]<=hi[2]) n++;}} return n;}
static int nearProps(float x,float y,float z,float r){int n=0; for(unsigned i=0;i<props.ninst;i++){ if(fabsf(props.def[i].x-x)<r&&fabsf(props.def[i].y-y)<r+4&&fabsf(props.def[i].z-z)<r) n++;} return n;}
int main(int argc,char**argv){ if(argc<3) return 2; float R=argc>3?atof(argv[3]):1.5f, clr=argc>4?atof(argv[4]):1.0f; if(load(argv[1])) {puts("load fail");return 1;}
  unsigned cnt=0,cap=0;
  for(unsigned i=0;i<world.nbox;i++){const ColBox*b=&world.box[i]; float ex=fabsf(b->ax[0])*b->h[0]+fabsf(b->ay[0])*b->h[1]+fabsf(b->az[0])*b->h[2], ey=fabsf(b->ax[1])*b->h[0]+fabsf(b->ay[1])*b->h[1]+fabsf(b->az[1])*b->h[2], ez=fabsf(b->ax[2])*b->h[0]+fabsf(b->ay[2])*b->h[1]+fabsf(b->az[2])*b->h[2];
    float g,n3[3]; int has=colGround(&world,b->c[0],b->c[2],b->c[1]-ey-0.05f,b->c[1]-ey-80.f,&g,n3); float bot=b->c[1]-ey; float cl=has?bot-g:999.f; if(cl<clr) continue; cap++;
    float lo[3]={b->c[0]-ex-R,b->c[1]-ey-R,b->c[2]-ez-R},hi[3]={b->c[0]+ex+R,b->c[1]+ey+R,b->c[2]+ez+R}; int nt=nearTris(lo,hi), np=nearProps(b->c[0],b->c[1],b->c[2],R+3.f);
    if(nt==0) { cnt++; float mh=fmaxf(b->h[0],fmaxf(b->h[1],b->h[2])); printf("%s box %u c=(%.1f %.1f %.1f) size=%.1fx%.1fx%.1f y=[%.1f..%.1f] clear=%.1f props_near=%d%s\n",argv[2],i,b->c[0],b->c[1],b->c[2],2*b->h[0],2*b->h[1],2*b->h[2],bot,b->c[1]+ey,has?cl:-1.f,np,np?" (prop рядом)":""); (void)mh; } }
  printf("%s SUMMARY boxes=%u airborne(clear>=%.1f)=%u invisible(no city tri within %.1f)=%u\n",argv[2],world.nbox,clr,cap,R,cnt); return 0;}
