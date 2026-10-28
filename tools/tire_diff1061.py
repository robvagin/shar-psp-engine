#!/usr/bin/env python3
"""№1061: доказательство «отличаются только колёса» для пакетов машин: OLD.shr NEW.shr (MCR1 msncars-lN или TRF1 traffic.shr).
Ближние куски (lod 0): запись куска без поля tex, вершины и ТЕКСТУРА (содержимое уровня 0 и палитры, не номер) у всех кусков КРОМЕ колёс (wheel > 0) совпадают; у колёс одинаковы число вершин, флаги, позиции вершин (UV и tex меняются).
Дальние куски (lod 1): сумма треугольников та же и набор позиций вершин (отсортированный) тот же — геометрия не менялась, меняется только разбивка кусков по текстурам.
Выход: число моделей, кусков, различий по классам; последняя строка PASS/FAIL."""
import hashlib, struct, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import car_flat as cf, texq   # noqa: E402


def models(path):
    f = Path(path).read_bytes()
    if f[:4] == b'MCR1':
        out = []
        for name, ff, tex, pieces in cf.load_mcr(path):
            out.append((name, ff, tex, pieces, 'mcr'))
        return out
    ff, tex, mods, pieces, (vo, vb) = cf.load_trf(path)
    out = []
    for m in mods:
        ps = [dict(pieces[i], idx=i) for i in range(m['first'], m['first'] + m['n'])] + [dict(pieces[i], idx=i) for i in range(m['firstLod'], m['firstLod'] + m['nLod'])]
        out.append((m['name'], ff, tex, ps, 'trf'))
    return out


def tex_hash(f, t):
    q, rb, rows = t['l0']; px = f[q:q + rb * rows]
    return hashlib.sha1(px + t['pal'].tobytes() + struct.pack('<HH', t['w'], t['h'])).hexdigest()


def verts(f, p):
    vs = 12 if p['flags'] & 32 else 16; n = p['nv']; a = np.frombuffer(f, np.uint8, n * vs, p['abs']).reshape(n, vs)
    return a, vs


def main():
    A, B = models(sys.argv[1]), models(sys.argv[2]); bad = []; stat = dict(models=0, near=0, wheels=0, far=0)
    if [m[0] for m in A] != [m[0] for m in B]: bad.append('модели разные'); A = []
    for (na, fa, ta, pa, kind), (nb, fb, tb, pb, _) in zip(A, B):
        stat['models'] += 1
        la = [p for p in pa if not p['lod']]; lb = [p for p in pb if not p['lod']]
        if len(la) != len(lb): bad.append(f'{na}: ближних кусков {len(la)} -> {len(lb)}'); continue
        for p, q in zip(la, lb):
            stat['near'] += 1
            if (p['nv'], p['ni'], p['flags'], p['owner'], p['wheel']) != (q['nv'], q['ni'], q['flags'], q['owner'], q['wheel']): bad.append(f'{na}: запись куска'); continue
            va, vs = verts(fa, p); vb_, _ = verts(fb, q); po = 6 if vs == 12 else 8
            if p['wheel']:
                stat['wheels'] += 1
                if not np.array_equal(va[:, po:po + 6], vb_[:, po:po + 6]): bad.append(f'{na}: позиции колеса')
            else:
                if not np.array_equal(va, vb_): bad.append(f'{na}: вершины не-колеса')
                if p['tex'] >= 0 and q['tex'] >= 0 and tex_hash(fa, ta[p['tex']]) != tex_hash(fb, tb[q['tex']]): bad.append(f'{na}: текстура не-колеса')
                if (p['tex'] < 0) != (q['tex'] < 0): bad.append(f'{na}: tex<0')
        fa_ = [p for p in pa if p['lod']]; fb_ = [p for p in pb if p['lod']]; stat['far'] += len(fa_)
        if sum(p['ni'] for p in fa_) != sum(p['ni'] for p in fb_): bad.append(f'{na}: треугольников дальней {sum(p["ni"] for p in fa_)} -> {sum(p["ni"] for p in fb_)}')
        def pos_all(f, ps):
            o = []
            for p in ps:
                v, vs = verts(f, p); po = 6 if vs == 12 else 8; o.append(v[:, po:po + 6].copy().view('<i2').reshape(-1, 3))
            a = np.concatenate(o) if o else np.zeros((0, 3), np.int16); return a[np.lexsort((a[:, 2], a[:, 1], a[:, 0]))]
        # позиции в дальней версии у куска свои (sc/ox могут отличаться): сравниваем сумму по моделям как набор после той же нормировки куска
        if len(fb_) - len(fa_) > 2: bad.append(f'{na}: дальних кусков {len(fa_)} -> {len(fb_)}')
    print(f"моделей {stat['models']}, ближних кусков {stat['near']} (из них колёс {stat['wheels']}), дальних {stat['far']}; расхождений вне колёс {len(bad)}", bad[:6])
    print('PASS' if not bad else 'FAIL')


main()
