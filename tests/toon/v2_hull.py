#!/usr/bin/env python3
"""№428 «мультик» × CHR1 v2: оболочка-контур (toonHull) на настоящих моделях героев, хост-сборка настоящего skin.c.
Для каждой пачки с индексами (flags&4) оболочка должна: лежать внутри своего выделения (границы), рисоваться теми же индексами,
а вершина по индексу — это вершина пачки, сдвинутая по нормали не больше SKIN_TOON_W. Красный контроль: -DSKIN_TOON_V2_OLD=1
(прежнее: копия nverts вершин, без индексов) обязан провалить. Запуск: tests/toon/v2_hull.py [--stage DIR]"""
import argparse, subprocess, re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
ap = argparse.ArgumentParser(); ap.add_argument('--stage', type=Path, default=Path.home() / 'Downloads/shar-psp/build/ship_stage'); ap.add_argument('--models', type=Path, nargs='*', help='свои файлы вместо набора ship_stage (Модели-96: s16)'); args = ap.parse_args()
out = ROOT / 'build/toon-v2-hull'; out.mkdir(parents=True, exist_ok=True)
source = (ROOT / 'viewer/skin.c').read_text().split('// ---------------------------------------------------------------- drawing', 1)[1]
source = source.replace('\n#ifndef SKIN_HOST', '', 1).rsplit('\n#endif', 1)[0]
constants = sorted(set(re.findall(r'\bGU_[A-Z0-9_]+\b', source)))
functions = sorted(set(re.findall(r'\b(sce(?:Gu|Kernel)\w+)\(', source)) - {'sceGuDrawArray', 'sceGuCheckList'})
shim = '#define SKIN_HOST\n#include "skin.c"\n#undef SKIN_HOST\n#include <stdio.h>\n#include <stdlib.h>\ntypedef struct { float x,y,z; } ScePspFVector3;\n'
shim += '\n'.join('#define ' + s + ' ' + str(i) for i, s in enumerate(constants)) + '\n'
shim += 'static unsigned char *hullBase; static unsigned hullSize; static void *memalign(unsigned a,unsigned n){ void *p=aligned_alloc(a,(n+a-1)&~(a-1)); hullBase=p; hullSize=n; return p; }\n'
shim += '\n'.join('static void ' + s + '() {}' for s in functions) + '\n'
shim += 'static void sceGuDrawArray(int p, unsigned vt, unsigned n, const void *idx, const void *verts);\nstatic unsigned sceGuCheckList(void) { return 0; }\nvoid skinToonFlush(void);\n'
shim += source + '\n'
CHECK = r'''
static const SkinModel *model; static unsigned calls, hullDraws, idxDraws, bad, checkedVerts; static int firstBad = -1;
static int idxMax(const void *idx, unsigned n) { const unsigned short *s = idx; int m = -1; for (unsigned i = 0; i < n; i++) if (s[i] > m) m = s[i]; return m; }
static void sceGuDrawArray(int p, unsigned vt, unsigned n, const void *idx, const void *verts) {
  const unsigned char *v = verts; int isHull = hullBase && v >= hullBase && v < hullBase + hullSize;
  if (!isHull) return;                                   // настоящая пачка — не наша забота
  hullDraws++;
  const SkinBatch *b = 0; unsigned off = 0, bi = 0;       // пачка по порядку: смещение в оболочке = сумма хранимых размеров прежних пачек
  for (bi = 0; bi < model->h->nBatches; bi++) { const SkinBatch *c = &model->b[bi];
    if (off == (unsigned)(v - hullBase) && n == c->nverts && !(c->flags & 2)) { b = c; break; }
    if (!(c->flags & 2) || 1) off += (TOON_STORED(c) * c->stride + 15) & ~15u; }
  if (!b) { bad++; if (firstBad < 0) firstBad = -2; return; }
  unsigned sv = TOON_STORED(b), slot = (sv * b->stride + 15) & ~15u;
  int no, po; if (!vfmtOffsets(b->vtype, b->stride, &no, &po)) return;
  const unsigned short *ix = (b->flags & 4) ? (const unsigned short *)(model->base + b->pad[0]) : 0;
  if (b->flags & 4) { idxDraws++; if (idx != (const void *)ix) { bad++; if (firstBad < 0) firstBad = (int)bi; } }
  if (v + slot > hullBase + hullSize) { bad++; if (firstBad < 0) firstBad = (int)bi; }                                   // выход за выделение
  for (unsigned k = 0; k < n; k++) {
    unsigned oi = ix ? ix[k] : k;                          // вершина пачки по индексу
    const unsigned char *orig = model->base + b->offVerts + oi * b->stride;
    unsigned hi = (idx ? ((const unsigned short *)idx)[k] : k);
    if (hi >= sv || oi >= sv) { bad++; if (firstBad < 0) firstBad = (int)bi; continue; }       // индекс вне хранимых вершин
    const unsigned char *hv = v + hi * b->stride; float po_[3], ph_[3]; vposRd(po_, orig + po, b->vtype, skinPosScale(model->h)); vposRd(ph_, hv + po, b->vtype, skinPosScale(model->h));   /* Модели-96: позиция float или s16 */
    float dx = ph_[0]-po_[0], dy = ph_[1]-po_[1], dz = ph_[2]-po_[2], d = sqrtf(dx*dx+dy*dy+dz*dz); checkedVerts++;
    if (!(d <= toonWk(0) * TOON_SMOOTH_K * 1.05f + 0.0005f)) { bad++; if (firstBad < 0) firstBad = (int)bi; }       // оболочка — та же вершина, сдвинутая по нормали
  }
}
'''
shim += CHECK + r'''
int main(int argc, char **argv) {
  unsigned totalBad = 0, totalIdx = 0, totalHull = 0;
  for (int i = 1; i < argc; i++) {
    SkinModel *m = skinLoad(argv[i]); if (!m) return 2; model = m; calls = hullDraws = idxDraws = bad = checkedVerts = 0; hullBase = 0; firstBad = -1;
    SkinPlayer p; skinInit(&p, m); ScePspFMatrix4 w; skinPlace(&w, 0, 0, 0, 0); skinSetToon(1); if (!skinToonEnabled()) { puts("FAIL toon not available"); return 3; }
    skinToonFlush(); skinDrawPal(m, p.pal, &w, -1);
    unsigned nIdx = 0; for (unsigned b = 0; b < m->h->nBatches; b++) if ((m->b[b].flags & 4) && !(m->b[b].flags & 2)) nIdx++;
    printf("%s batches=%u indexed=%u hullDraws=%u idxDraws=%u checked=%u bad=%u firstBad=%d\n", argv[i], m->h->nBatches, nIdx, hullDraws, idxDraws, checkedVerts, bad, firstBad);
    totalBad += bad; totalIdx += nIdx; totalHull += hullDraws; skinSetToon(0); skinToonFlush(); skinFree(m);
  }
  if (!totalIdx) { puts("FAIL: на моделях нет индексных пачек CHR v2, проверка слепая"); return 4; }
  if (!totalHull) { puts("FAIL: оболочка не рисовалась"); return 5; }
  printf("%s\n", totalBad ? "BAD" : "OK"); return totalBad ? 1 : 0;
}
'''
shim = shim.replace('TOON_STORED(c)', 'toonStored(c)').replace('TOON_STORED(b)', 'toonStored(b)')
(out / 'host.c').write_text(shim)
models = args.models or [args.stage / x for x in ['homer.shr', 'l2/player.shr', 'l3/player.shr', 'l5/player.shr', 'l7/player.shr']]
assert all(p.is_file() for p in models), models
ok = True
for name, flags, expected in [('new', [], 0), ('old', ['-DSKIN_TOON_V2_OLD=1'], 1)]:
    subprocess.run(['cc', '-std=gnu11', '-O0', '-g', '-fsanitize=address', '-Wno-deprecated-non-prototype', '-Iviewer', *flags, str(out / 'host.c'), '-lm', '-o', str(out / name)], cwd=ROOT, check=True)
    r = subprocess.run([str(out / name), *map(str, models)], capture_output=True, text=True); (out / (name + '.txt')).write_text(r.stdout + r.stderr)
    last = (r.stdout.strip().splitlines() or ['?'])[-1]
    print(f'{name}: код {r.returncode} (ждали {expected}) · {last}'); print('  ' + r.stdout.strip().replace('\n', '\n  '))
    if r.returncode != expected: ok = False; print(r.stderr[-600:])
print('PASS: на CHR v2 оболочка по индексам, в границах; прежнее (SKIN_TOON_V2_OLD=1) красное' if ok else 'FAIL')
sys.exit(0 if ok else 1)
