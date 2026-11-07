// Embedded CHR1 gag skins, sampled from collect's trigger clock; rigid props
// and source billboard tracks are drawn by the existing SPR1 path.
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "gag_skin.h"
#include "skin.h"
#ifndef SKIN_HOST
#include <malloc.h>
#include <pspkernel.h>
#endif
typedef struct { unsigned off, size; unsigned short gag, pad; unsigned reserved; float place[12]; } GagSkinRec;
static const GagSkinRec *records;
static SkinModel *models;
static SkinPlayer *player;
static unsigned count;
static CollectRead sourceRead; static void *sourceUser; static unsigned sourceSize;
static unsigned char **owned;static unsigned char *badRec;
unsigned gagSkinResidentBytes;
void gagSkinFree(void) { free(badRec); badRec=0; if (owned) { for (unsigned i=0;i<count;i++) free(owned[i]); free(owned); } owned=0; sourceRead=0; sourceUser=0; sourceSize=0; gagSkinResidentBytes=0; free(models); free(player); models = 0; player = 0; records = 0; count = 0; }
static int inside(unsigned size, unsigned off, unsigned n, unsigned stride) {
  return off <= size && n <= (size - off) / stride;
}
static int bindModel(unsigned i,const unsigned char *d,unsigned size) {
    const SkinHeader *h=(const SkinHeader *)d;
    if (size<128 || memcmp(h->magic,"CHR1",4) || h->version!=1 || h->size!=size || !h->nJoints || h->nJoints>SKIN_MAXJ || !h->nClips ||
        !inside(size,h->offJoints,h->nJoints,128) || !inside(size,h->offBatches,h->nBatches,48) ||
        !inside(size,h->offTex,h->nTex,48) || !inside(size,h->offClips,h->nClips,80)) return -1;
    SkinModel *m = models + i; m->base = d; m->h = h;
    m->j = (const SkinJoint *)(d + h->offJoints); m->b = (const SkinBatch *)(d + h->offBatches);
    m->tex = (const SkinTex *)(d + h->offTex); m->c = (const SkinClip *)(d + h->offClips); m->blink = 0;
    for (unsigned b = 0; b < h->nBatches; b++) {
      const SkinBatch *bt = m->b + b;
      if (!bt->nb || bt->nb > 8 || bt->tex >= h->nTex || !bt->stride || !inside(h->size, bt->offVerts, bt->nverts, bt->stride)) return -1;
      for (unsigned j = 0; j < bt->nb; j++) if (bt->bones[j] >= h->nJoints) return -1;
    }
    for (unsigned t = 0; t < h->nTex; t++) {
      const SkinTex *tx = m->tex + t;
      if (!inside(h->size, tx->offClut, 256, 4) || !inside(h->size, tx->offPix, tx->w, tx->h ? tx->h : 1)) return -1;
    }
    for (unsigned k = 0; k < h->nClips; k++) {
      if (!inside(h->size, m->c[k].offTracks, h->nJoints, 16)) return -1;
      const SkinTrackRec *tracks = (const SkinTrackRec *)(d + m->c[k].offTracks);
      for (unsigned j = 0; j < h->nJoints; j++) {
        const SkinTrackRec *tr = tracks + j;
        if (!inside(h->size, tr->offRot, ((tr->nRot * 2 + 3u) & ~3u) + tr->nRot * 8u, 1) ||
            !inside(h->size, tr->offTrans, ((tr->nTrans * 2 + 3u) & ~3u) + tr->nTrans * 12u, 1)) return -1;
      }
    }
    return 0;
}
int gagSkinInit(const Collect *c) {
  gagSkinFree();
#ifndef GAG_SKIN_OLD
  const unsigned *u = (const unsigned *)c->pak; unsigned n = u[14], off = u[15];
  if (!n) return 0;
  if (!inside(c->size, off, n, 64) || (off & 15)) return -1;
  records = (const GagSkinRec *)(c->pak + off);
  models = calloc(n, sizeof *models);
#ifdef SKIN_HOST
  player = aligned_alloc(16, (sizeof *player + 15u) & ~15u);
#else
  player = memalign(16, sizeof *player);
#endif
  if (!models || !player) { gagSkinFree(); return -1; }
  for (unsigned i = 0; i < n; i++) {
    const GagSkinRec *r = records + i;
    if (r->gag >= c->ngag || r->size < 128 || (r->off & 15) || !inside(c->size, r->off, r->size, 1)) goto bad;
    const unsigned char *d = c->pak + r->off; const SkinHeader *h = (const SkinHeader *)d;
    if (memcmp(h->magic, "CHR1", 4) || h->version != 1 || h->size != r->size || !h->nJoints || h->nJoints > SKIN_MAXJ || !h->nClips ||
        !inside(h->size, h->offJoints, h->nJoints, 128) || !inside(h->size, h->offBatches, h->nBatches, 48) ||
        !inside(h->size, h->offTex, h->nTex, 48) || !inside(h->size, h->offClips, h->nClips, 80)) goto bad;
    if (bindModel(i,d,r->size)) goto bad;
  }
  count = n; return (int)n;
bad: gagSkinFree(); return -1;
#else
  (void)c; return 0;
#endif
}
int gagSkinInitSource(const Collect *c,CollectRead read,void *user,unsigned fileSize) {
  gagSkinFree();
#ifndef GAG_SKIN_OLD
  const unsigned *u=(const unsigned *)c->pak; unsigned n=u[14],off=u[15];
  if (!n) return 0;
  if (!inside(c->size,off,n,64) || (off&15)) return -1;
  records=(const GagSkinRec *)(c->pak+off);
  models=calloc(n,sizeof *models);owned=calloc(n,sizeof *owned);
#ifdef SKIN_HOST
  player=aligned_alloc(16,(sizeof *player+15u)&~15u);
#else
  player=memalign(16,sizeof *player);
#endif
  if(!models || !owned || !player){gagSkinFree();return -1;}
  count=n;sourceRead=read;sourceUser=user;sourceSize=fileSize;
  for(unsigned i=0;i<n;i++) if(records[i].gag>=c->ngag || records[i].size<128 || (records[i].off&15) || !inside(fileSize,records[i].off,records[i].size,1)){gagSkinFree();return -1;}
#endif
  return (int)count;
}
int gagSkinPrepare(const Collect *c,const unsigned char *wanted) {
  if(!sourceRead)return 0;
  for(unsigned i=0;i<count;i++) if(owned[i] && !wanted[records[i].gag]){gagSkinResidentBytes-=records[i].size;free(owned[i]);owned[i]=0;memset(models+i,0,sizeof *models);}
  for(unsigned i=0;i<count;i++) if(wanted[records[i].gag] && !owned[i]){
    const GagSkinRec *r=records+i;
#ifdef SKIN_HOST
    unsigned char *d=aligned_alloc(16,(r->size+15u)&~15u);
#else
    unsigned char *d=memalign(16,r->size);
#endif
    if(!d || sourceRead(sourceUser,r->off,d,r->size) || bindModel(i,d,r->size)){free(d);memset(models+i,0,sizeof *models);return -1;}
    owned[i]=d;gagSkinResidentBytes+=r->size;
#ifndef SKIN_HOST
    sceKernelDcacheWritebackRange(d,r->size);
#endif
  }
  return 0;
}
void gagSkinTrim(const unsigned char *wanted) {
  if(!sourceRead)return;
  for(unsigned i=0;i<count;i++) if(owned[i] && !wanted[records[i].gag]){gagSkinResidentBytes-=records[i].size;free(owned[i]);owned[i]=0;memset(models+i,0,sizeof *models);}
}
int gagSkinNext(const unsigned char *wanted,unsigned *idx,unsigned *off,unsigned *size) {
  if(!sourceRead)return 0;
  for(unsigned i=0;i<count;i++) if(wanted[records[i].gag] && !owned[i] && !(badRec && badRec[i])){*idx=i;*off=records[i].off;*size=records[i].size;return 1;}
  return 0;
}
int gagSkinGagOf(unsigned idx){return idx<count?records[idx].gag:-1;}
void gagSkinMarkBad(unsigned idx){if(idx>=count)return;if(!badRec)badRec=calloc(count,1);if(badRec)badRec[idx]=1;}
int gagSkinAdopt(unsigned idx,unsigned char *buf) {
  if(!sourceRead || idx>=count || owned[idx])return -1;
  if(bindModel(idx,buf,records[idx].size)){memset(models+idx,0,sizeof *models);return -1;}
  owned[idx]=buf;gagSkinResidentBytes+=records[idx].size;
#ifndef SKIN_HOST
  sceKernelDcacheWritebackRange(buf,records[idx].size);
#endif
  return 0;
}
unsigned gagSkinDraw(const Collect *c, int room, float ex, float ez, float distance) {
  unsigned draws = 0;
#ifndef GAG_SKIN_OLD
  for (unsigned i = 0; i < count; i++) {
    const GagSkinRec *r = records + i; const CollGag *g = c->gag + r->gag;
    if (g->room != room || (g->random && !c->st.gagPick[r->gag])) continue;
    float dx = g->x - ex, dz = g->z - ez;
    if (dx * dx + dz * dz > distance * distance) continue;
    if (!models[i].base) continue;
    skinInit(player, models + i); player->clip = 0; player->loop = g->cycle == COLL_CYCLE;
    player->t = c->st.gagT[r->gag];
#ifndef GAG_LOOP873_OLD
    // №873 (Роб 23.10: «дед стоит, ничего не делает»; №869: «ударяется один раз, стоит, а очки продолжают»): время гэга растёт без конца, а заворот
    // по длине клипа делает только skinTick (гэги его не зовут) — гэг «по кругу» застывал в последнем кадре после первого прохода. Теперь — по кругу.
    { float len_ = models[i].c[0].length; if (player->loop && len_ > 0.f) { player->t = fmodf(player->t, len_); if (player->t < 0.f) player->t += len_; } }
#endif
    if (c->st.gagSt[r->gag] == 2) player->t = models[i].c[0].length;
    skinPose(player);
    ScePspFMatrix4 world;
    float *m = (float *)&world;
    for (int col = 0; col < 4; col++) for (int row = 0; row < 4; row++)
      m[col * 4 + row] = row == 3 ? (col == 3 ? 1.f : 0.f) : r->place[row * 4 + col];
    m[12] += g->x; m[13] += g->y; m[14] += g->z;
#ifndef SKIN_HOST
    skinDraw(player, &world);
#endif
    draws++;
  }
#endif
  return draws;
}
