#!/usr/bin/env python3
"""№1061 перепись «до/после» по всем машинам диска (art/cars/*.p3d): как GE увидит протектор колеса (треугольники поперёк оси w0..w3) на мипах 0..3 —
пакет текстуры так, как его пишет экспорт (палитра, мипы, затухание мипов), билинейная выборка по развёртке. Цвет протектора: средний RGB и «не чёрный»
(яркость > 64 или заметный тёплый/цветной оттенок: макс-мин канала > 24). Старое — TIRE1061_OLD, новое — правило №1061; считает число текстур колёс и их байт.
  SHR_TEX_CAP256=1 SHR_SMALL_MIPS=1 .venv/bin/python tools/tire_census1061b.py OUT_DIR [имя ...]   -> OUT_DIR/tire_census1061b.tsv, summary.txt"""
import json, os, re, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import psp_export as pe, texq   # noqa: E402
from convert import Converter, GAME, OUT   # noqa: E402


def bil(im, U):
    H, W = im.shape[:2]; x = (U[:, 0] % 1) * W - .5; y = (U[:, 1] % 1) * H - .5
    x0 = np.floor(x).astype(int); y0 = np.floor(y).astype(int); fx = (x - x0)[:, None]; fy = (y - y0)[:, None]
    g = lambda xx, yy: im[yy % H, xx % W]
    return g(x0, y0) * (1 - fx) * (1 - fy) + g(x0 + 1, y0) * fx * (1 - fy) + g(x0, y0 + 1) * (1 - fx) * fy + g(x0 + 1, y0 + 1) * fx * fy


def wheel_stats(meta, raw, old):
    pe.TIRE_OLD = old; cache = {}; blob = []
    prims = pe.car_prims(meta, raw, cache, blob, False, None)
    res = []; tex = set()
    for name, t, fl, P, UV, C in prims:
        if not re.fullmatch(r'w[0-3]', name): continue
        tex.add(t); T = P.reshape(-1, 3, 3); U = UV.reshape(-1, 3, 2)
        n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); side = n[:, 0] ** 2 >= .49 * ((n * n).sum(1) + 1e-12)
        lv = [np.asarray(l, np.float64)[..., :3] for l in texq.decode(blob[t])]
        W = np.array([(a, b, 1 - a - b) for a in np.linspace(0, 1, 7) for b in np.linspace(0, 1, 7) if a + b <= 1])
        row = []
        for im in lv[:4]:
            cols = []
            for sel in (~side, side):
                if not sel.any(): cols.append(None); continue
                q = np.einsum('wk,tkc->twc', W, U[sel]).reshape(-1, 2); cols.append(bil(im, q).mean(0))
            row.append(cols)
        res.append((name, row))
        break   # колёса одной машины одинаковы по текстуре: достаточно w0 (первого найденного)
    nb = sum(len(blob[t]) for t in tex)
    return res, len(tex), nb


def notblack(c):
    return c is not None and (c.mean() > 64 or (c.max() - c.min()) > 24)


def main():
    out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True); names = sys.argv[2:]
    conv = Converter(); rows = []; sm = dict(old_bad=0, new_bad=0, n=0, old_tex=0, new_tex=0, old_b=0, new_b=0)
    for p in sorted((GAME / 'art' / 'cars').glob('*.p3d')):
        if names and p.stem not in names: continue
        tmp = f'tc1061b_{os.getpid()}'
        try:
            conv.export(p, tmp, vehicle=True); meta = json.loads((OUT / f'{tmp}.json').read_text()); raw = (OUT / f'{tmp}.bin').read_bytes()
        except Exception as e:
            rows.append(f'{p.stem}\t-\tошибка {type(e).__name__}'); continue
        finally:
            for ext in ('json', 'bin'):
                if (OUT / f'{tmp}.{ext}').exists(): (OUT / f'{tmp}.{ext}').unlink()
        ro, to, bo = wheel_stats(meta, raw, True); rn, tn, bn = wheel_stats(meta, raw, False)
        if not ro or not rn: rows.append(f'{p.stem}\t-\tнет колёс'); continue
        f = lambda c: '-' if c is None else '%02x%02x%02x' % tuple(np.clip(c, 0, 255).astype(int))
        bad_o = [i for i, cl in enumerate(ro[0][1]) if notblack(cl[0])]; bad_n = [i for i, cl in enumerate(rn[0][1]) if notblack(cl[0])]
        rows.append(p.stem + '\t' + ' '.join(f(cl[0]) for cl in ro[0][1]) + '\t' + ' '.join(f(cl[0]) for cl in rn[0][1]) + f'\t{"".join(map(str, bad_o)) or "-"}\t{"".join(map(str, bad_n)) or "-"}\t{to}/{bo}\t{tn}/{bn}')
        sm['n'] += 1; sm['old_bad'] += bool(bad_o); sm['new_bad'] += bool(bad_n); sm['old_tex'] += to; sm['new_tex'] += tn; sm['old_b'] += bo; sm['new_b'] += bn
        print(rows[-1], flush=True)
    hdr = 'машина\tпротектор до (мипы 0-3)\tпротектор после\tне чёрный до (мипы)\tне чёрный после\tтекстур/байт до\tтекстур/байт после'
    (out / 'tire_census1061b.tsv').write_text(hdr + '\n' + '\n'.join(rows) + '\n')
    txt = f"машин {sm['n']}; протектор не чёрный хотя бы на одном мипе: до {sm['old_bad']}, после {sm['new_bad']}; текстур колёс (на машину) до {sm['old_tex']}, после {sm['new_tex']}; байт до {sm['old_b']}, после {sm['new_b']}"
    (out / 'summary.txt').write_text(txt + '\n'); print(txt)


if __name__ == '__main__':
    main()
