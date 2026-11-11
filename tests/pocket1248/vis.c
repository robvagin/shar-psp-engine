// №1248 (Кольцо-96, на основе tests/airwalls/fam.c Опоры): ВСЕ примитивы коллизии (ящики, цилиндры, сегменты, крутые треугольники) в радиусе R от точки — есть ли рядом нарисованные треугольники города / пропы.
// vis LEVEL X Z R [PAD]: строка на примитив; "НЕВИДИМ" — ни одного рисуемого треугольника города в ящике примитива + PAD (1,5 м) и ни одного пропа в PAD+3 м. Раньше: Семейство: коллизия (ящики COL1) в воздухе без видимой геометрии рядом. Сборка: cc -std=c11 -O2 -w -DSKIN_HOST -I viewer tests/airwalls/fam.c viewer/col.c viewer/props.c viewer/stateprops.c viewer/coins.c -lm -Wl,-undefined,dynamic_lookup -o fam; запуск: fam level.shr МЕТКА [радиус 1.5] [высота 1.0]
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
static int cmpf(const void*a,const void*b){float x=*(const float*)a,y=*(const float*)b; return x<y?-1:x>y;}
// профиль РИСУНКА: у каждой точки линии — высоты всех нарисованных треугольников города, накрывающих её в плане (без стен n.y<0.3), от нижней к верхней; рядом — высота коллизии (colGround сверху вниз с TOP)
static int profMode(int argc,char**argv){ float x0=atof(argv[3]),z0=atof(argv[4]),x1=atof(argv[5]),z1=atof(argv[6]),top=argc>7?atof(argv[7]):3.f; float L=hypotf(x1-x0,z1-z0); int n=(int)L; if(n<1)n=1;
  for(int k=0;k<=n;k++){ float t=(float)k/n,x=x0+(x1-x0)*t,z=z0+(z1-z0)*t; float ys[64]; int ny=0; int cx=(int)floorf((x-GOX)/GC),cz=(int)floorf((z-GOZ)/GC); if(cx>=0&&cz>=0&&cx<GW&&cz<GH){ size_t c=(size_t)cz*GW+cx;
      for(unsigned q=gS[c];q<gS[c+1]&&ny<64;q++){ const float*a=T+9*gL[q]; float d=(a[3]-a[0])*(a[8]-a[2])-(a[5]-a[2])*(a[6]-a[0]); if(fabsf(d)<1e-6f) continue; float u=((x-a[0])*(a[8]-a[2])-(z-a[2])*(a[6]-a[0]))/d, v=((a[3]-a[0])*(z-a[2])-(a[5]-a[2])*(x-a[0]))/d; if(u<-1e-4f||v<-1e-4f||u+v>1.0001f) continue; float y=a[1]+u*(a[4]-a[1])+v*(a[7]-a[1]);
        float nx=(a[4]-a[1])*(a[8]-a[2])-(a[5]-a[2])*(a[7]-a[1]); (void)nx; if(y<top+0.01f&&y>top-12.f) ys[ny++]=y; } }
    qsort(ys,ny,sizeof(float),cmpf); float g=-999,n3[3]; int has=colGround(&world,x,z,top,-1e6f,&g,n3);
    printf("(%.1f %.1f) коллизия %s%.2f | рисунок:",x,z,has?"":"нет ",has?g:0.f); float last=-1e9f; int shown=0; for(int i=ny-1;i>=0&&shown<5;i--){ if(last-ys[i]<0.05f&&last>-1e8f) continue; printf(" %.2f",ys[i]); last=ys[i]; shown++; } printf("\n"); }
  return 0; }
int main(int argc,char**argv){ if(argc>=7&&!strcmp(argv[2],"prof")){ if(load(argv[1])) return 1; return profMode(argc,argv);} if(argc<5) return 2; float X=atof(argv[2]),Z=atof(argv[3]),R=atof(argv[4]),P=argc>5?atof(argv[5]):1.5f; if(load(argv[1])){puts("load fail");return 1;}
  int inv=0,tot=0;
  for(unsigned i=0;i<world.nbox;i++){const ColBox*b=&world.box[i]; if(fabsf(b->c[0]-X)>R||fabsf(b->c[2]-Z)>R) continue;
    float ex=fabsf(b->ax[0])*b->h[0]+fabsf(b->ay[0])*b->h[1]+fabsf(b->az[0])*b->h[2], ey=fabsf(b->ax[1])*b->h[0]+fabsf(b->ay[1])*b->h[1]+fabsf(b->az[1])*b->h[2], ez=fabsf(b->ax[2])*b->h[0]+fabsf(b->ay[2])*b->h[1]+fabsf(b->az[2])*b->h[2];
    float lo[3]={b->c[0]-ex-P,b->c[1]-ey-P,b->c[2]-ez-P},hi[3]={b->c[0]+ex+P,b->c[1]+ey+P,b->c[2]+ez+P}; int nt=nearTris(lo,hi),np=nearProps(b->c[0],b->c[1],b->c[2],P+3.f); tot++; if(!nt&&!np) inv++;
    printf("BOX %u c(%.1f %.1f %.1f) экстент(%.1f %.1f %.1f) y[%.1f..%.1f] рис.треуг %d пропов %d %s\n",i,b->c[0],b->c[1],b->c[2],2*ex,2*ey,2*ez,b->c[1]-ey,b->c[1]+ey,nt,np,(!nt&&!np)?"НЕВИДИМ":""); }
  for(unsigned i=0;i<world.ncyl;i++){const ColCyl*c=&world.cyl[i]; if(fabsf(c->c[0]-X)>R||fabsf(c->c[2]-Z)>R) continue; float e=c->r+c->halfLen; float lo[3]={c->c[0]-e-P,c->c[1]-e-P,c->c[2]-e-P},hi[3]={c->c[0]+e+P,c->c[1]+e+P,c->c[2]+e+P}; int nt=nearTris(lo,hi),np=nearProps(c->c[0],c->c[1],c->c[2],P+3.f); tot++; if(!nt&&!np) inv++;
    printf("CYL %u c(%.1f %.1f %.1f) r %.2f half %.2f рис.треуг %d пропов %d %s\n",i,c->c[0],c->c[1],c->c[2],c->r,c->halfLen,nt,np,(!nt&&!np)?"НЕВИДИМ":""); }
  for(unsigned i=0;i<world.nseg;i++){const ColSeg*s=&world.seg[i]; if(fabsf(s->a[0]-X)>R||fabsf(s->a[1]-Z)>R) continue; float lo[3]={fminf(s->a[0],s->b[0])-P,s->y0-P,fminf(s->a[1],s->b[1])-P},hi[3]={fmaxf(s->a[0],s->b[0])+P,s->y1+P,fmaxf(s->a[1],s->b[1])+P}; int nt=nearTris(lo,hi),np=nearProps(s->a[0],(s->y0+s->y1)/2,s->a[1],P+3.f); tot++; if(!nt&&!np) inv++;
    printf("SEG %u a(%.1f %.1f) b(%.1f %.1f) y[%.1f..%.1f] рис.треуг %d пропов %d %s\n",i,s->a[0],s->a[1],s->b[0],s->b[1],s->y0,s->y1,nt,np,(!nt&&!np)?"НЕВИДИМ":""); }
  printf("ИТОГО примитивов в радиусе %.0f м: %d, из них без рисунка: %d\n",R,tot,inv); return 0;}
