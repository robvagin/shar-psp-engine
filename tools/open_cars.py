#!/usr/bin/env python3
"""Модели-96, правка 1240: какие машины с открытым верхом над сиденьем водителя. Мера по модели диска art/cars/<имя>.p3d: самая высокая
поверхность кузова над ковриком сиденья (квадрат ±0,3 м от сустава dl) минус высота сиденья. Колёса не считаются.
  open_cars.py            -> печатает таблицу (имя, запас над сиденьем, вердикт)
  open_cars.py --write F  -> пишет список открытых машин в файл данных F (по одному имени в строке; # — комментарий)
Порог OPEN_BELOW — выше головы сидящей Мардж с причёской стоймя (см. docs/handoff/МОДЕЛИ96_...)."""
import sys, json, re
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import psp_export as pe
import car_dump as cd

OPEN_BELOW = 1.05     # м над сиденьем: ниже — верха над головой нет
HALF = 0.30

def load(carp, wheels=False):
    """-> (треугольники n×3×3 в позе покоя, сиденье dl, колёса отдельно не режем)"""
    root = pe.read(carp)
    comp = next(c for c in root.children if c.id == 0x4512)
    _, p = pe.string(comp.data); skname, _ = pe.string(comp.data, p)
    J = cd.skeleton(next(c for c in root.children if c.id == 0x4500 and pe.string(c.data)[0] == skname))
    jn = {j[0]: i for i, j in enumerate(J)}
    jpos = np.array([[j[3][3, 0], j[3][3, 1], -j[3][3, 2]] for j in J])
    if 'dl' not in jn: return None, None
    dl = jpos[jn['dl']]
    nm = 'psp_open_tmp'
    cv = pe.Converter(); cv.export(carp, pe.TN(nm), vehicle=True)
    f = pe.OUT / (pe.TN(nm) + '.json'); meta = json.loads(f.read_text()); raw = (pe.OUT / (pe.TN(nm) + '.bin')).read_bytes()
    for ext in ('json', 'bin'): (pe.OUT / f'{pe.TN(nm)}.{ext}').unlink()
    tris = []
    for obj in meta['objects']:
        if (not wheels and re.fullmatch(r'w\d', obj['name'])) or obj['name'] not in jn: continue
        j = jn[obj['name']]
        for prim in meta['meshes'].get(obj['mesh'], []):
            at = prim['attributes']
            pos = np.frombuffer(raw, '<f4', at['position'][1], at['position'][0]).reshape(-1, 3).astype(np.float64) + jpos[j]
            ix = np.frombuffer(raw, '<u2', at['indices'][1], at['indices'][0])
            ix = np.stack([ix[:-2], ix[1:-1], ix[2:]], 1)                  # индексы — полосы треугольников (как в car_prims)
            ix = ix[(ix[:, 0] != ix[:, 1]) & (ix[:, 1] != ix[:, 2]) & (ix[:, 0] != ix[:, 2])]
            tris.append(pos[ix])
    return (np.concatenate(tris) if tris else np.zeros((0, 3, 3))), dl

def measure(carp):
    T, dl = load(carp)
    if dl is None: raise ValueError('нет сустава сиденья dl')
    top = -9.0; cover = 0
    if True:
        if True:
            for A, B, C in T:
                n = max(2, int(max(np.linalg.norm(B - A), np.linalg.norm(C - A), np.linalg.norm(C - B)) / .03))
                u, v = np.meshgrid(np.linspace(0, 1, n + 1), np.linspace(0, 1, n + 1)); k = u + v <= 1
                pts = A + u[k, None] * (B - A) + v[k, None] * (C - A)
                m = (np.abs(pts[:, 0] - dl[0]) <= HALF) & (np.abs(pts[:, 2] - dl[2]) <= HALF)
                if m.any(): top = max(top, float(pts[m, 1].max())); cover += 1
    return top - dl[1] if cover else None

def main():
    rows = []
    for p in sorted((pe.GAME / 'art' / 'cars').glob('*.p3d')):
        try: h = measure(p)
        except Exception as e: rows.append((p.stem, None, 'ошибка ' + str(e)[:40])); continue
        rows.append((p.stem, h, ''))
    return rows

if __name__ == '__main__':
    rows = main()
    for n, h, e in sorted(rows, key=lambda r: (r[1] is None, r[1] or 0)):
        print(f'{n:14s} {"-" if h is None else f"{h:6.2f}"} {"ОТКРЫТА" if (h is None or h < OPEN_BELOW) and not e else ""} {e}')
    if '--write' in sys.argv:
        out = Path(sys.argv[sys.argv.index('--write') + 1])
        out.write_text('# открытые машины (нет верха над сиденьем водителя): tools/open_cars.py; Мардж в них с причёской стоймя (MARGE_HAIR_OLD=1 — как было)\n' +
                       ''.join(n + '\n' for n, h, e in sorted(rows) if not e and (h is None or h < OPEN_BELOW)))
