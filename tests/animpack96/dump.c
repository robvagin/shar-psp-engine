// Модели-96: поза каждого клипа каждой модели в каждом кадре (и между кадрами) через НАСТОЯЩИЙ читатель skin.c (хост-сборка).
// dump FILE.shr OUT.bin  -> u32 nClips, u32 nJoints; на клип: u32 nSamples, затем на отсчёт nJoints*(q[4],t[3]) + rootMotion[3] (float).
// Отсчёты: 4 на кадр (0, .25, .5, .75) и конец клипа.
#include <stdio.h>
#include <stdlib.h>
#include "skin.h"
int main(int argc, char **argv) {
  if (argc < 3) return 2;
  SkinModel *m = skinLoad(argv[1]); if (!m) { fprintf(stderr, "load fail %s\n", argv[1]); return 1; }
  FILE *o = fopen(argv[2], "wb"); unsigned nc = m->h->nClips, nj = m->h->nJoints;
  fwrite(&nc, 4, 1, o); fwrite(&nj, 4, 1, o);
  SkinXf xf[SKIN_MAXJ];
  for (unsigned c = 0; c < nc; c++) {
    const SkinClip *cl = &m->c[c]; unsigned ns = (unsigned)((cl->frames - 1.f) * 4.f) + 1u;
    fwrite(&ns, 4, 1, o);
    for (unsigned i = 0; i < ns; i++) {
      float t = i / (4.f * cl->fps), rm[3];
      skinSample(m, (int)c, t, xf); rm[0] = rm[1] = rm[2] = 0.f; if (m->h->rootJoint < nj) skinRootMotion(m, (int)c, t, rm);   // rootJoint 0xffffffff у гэгов и меню: корня-движения нет
      for (unsigned j = 0; j < nj; j++) { fwrite(xf[j].q, 4, 4, o); fwrite(xf[j].t, 4, 3, o); }
      fwrite(rm, 4, 3, o);
    }
  }
  fclose(o); return 0;
}
