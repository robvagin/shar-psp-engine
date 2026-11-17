#!/usr/bin/env python3
"""№1328: полный проход по ВСЕМ стейтпропам L1-L7. Правда оригинала: машина состояний предмета — ПОСЛЕДНИЙ блок StatePropData у сущности; запись видимости i включает i-й рисунок (по номеру, не по имени).
  python3 tests/vend1328/vis_all.py                 -> экспорт сверяется с диском: видимость каждого состояния каждого типа == по номеру; блок машины == последний
  python3 tests/vend1328/vis_all.py --expect L N    -> печатает строки TV (видимость по состояниям, в порядке типов SPR1) уровня N для сверки с пробой level.shr: файл
  VEND1328_OLD=1 — красный контроль: экспорт по-старому обязан не сойтись.
Код выхода 0 — сошлось."""
import sys, os, io, contextlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))
import prp2_export as P
from p3d import read, walk
art = ROOT / 'hit-and-run-web/source/game/art'
art = art.resolve()
rows = []
orig = P.StateProps._add_stateprop
def hook(self, name, row):
    orig(self, name, row)
    T = self.types[-1]
    sps = [c for c in walk(row['w']) if c.id == 0x8020000]
    last = P.parse_stateprop(sps[-1])
    nprops = len(T['props'])
    for k, st in enumerate(last['states']):
        v = [x for _, x in st['vis']]
        if len(v) == len(T['slots']): truth = v
        elif len(v) < len(T['slots']) and not any(n in T['slots'] for n, _ in st['vis']):
            truth = v[:-1] + [v[-2]] * (nprops - len(v) + 1) + [v[-1]]; truth = truth[:len(T['slots'])]
        else: truth = [dict(st['vis']).get(s, 1 if i < nprops else 0) for i, s in enumerate(T['slots'])]   # лишняя запись: по имени
        tv = sum(1 << i for i, x in enumerate(truth) if x)
        rows.append((self.level, name, k, len(sps), len(last['states']), len(T['states']), tv & 0xFFFFFFFF, T['states'][k]['vis'] & 0xFFFFFFFF if k < len(T['states']) else None))
P.StateProps._add_stateprop = hook
def run(levels):
    del rows[:]
    for L in levels:
        with contextlib.redirect_stdout(io.StringIO()):
            P.StateProps(L, art, lambda f, m: [], lambda f, s: -1, verbose=False)
    return list(rows)
if sys.argv[1:2] == ['--expect']:
    L = int(sys.argv[2]); R = run([L]); cur = None; line = []
    out = {}
    for r in R: out.setdefault((r[1]), []).append(r[7])
    for name, vs in out.items(): print('TV ' + name + ' ' + ' '.join('%08x' % v for v in vs))
    sys.exit(0)
R = run(range(1, 8))
bad_vis = [r for r in R if r[6] != r[7]]
bad_blk = [r for r in R if r[4] != r[5]]
print('стейтпропов-состояний %d, типов %d; видимость не по номеру: %d; блок не последний: %d' % (len(R), len({(r[0], r[1]) for r in R}), len(bad_vis), len(bad_blk)))
for r in sorted(set((r[0], r[1]) for r in bad_vis + bad_blk)): print('  расходится: L%d %s' % r)
sys.exit(1 if (bad_vis or bad_blk) else 0)
