#!/usr/bin/env python3
"""Модели-96, позиции вершин s16: сравнение снимков из эмулятора (один и тот же сценарий, данные «до» и «после») попиксельно.
  shotdiff.py DIR_OLD DIR_NEW [--out DIFF.png] [--tol N] [--pct P]
Снимок окна PPSSPP: полоса заголовка сверху и строка счётчика снизу (время, кадр) выпадают из сравнения (счётчик всегда разный).
Пиксель «разный», если любой канал отличается больше --tol (по умолчанию 24 из 255). Порог: доля разных пикселей <= --pct % (по умолчанию 0,5).
Красный контроль: --shift N сдвигает «после» на N пикселей — должен падать."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image
a, b = Path(sys.argv[1]), Path(sys.argv[2])
arg = lambda k, d: type(d)(sys.argv[sys.argv.index(k) + 1]) if k in sys.argv else d
tol, pct, shift = arg('--tol', 24), arg('--pct', 0.5), arg('--shift', 0); outp = arg('--out', ''); ftop, fbot = arg('--top', 0.058), arg('--bot', 0.965)   # доли высоты: сверху заголовок окна, снизу счётчик (безголовый кадр 480x272: --top 0 --bot 0.948)
rows = []; worst = 0.0; tiles = []
for f in sorted(a.glob('*.png')):
    g = b / f.name
    if not g.is_file(): print(f'НЕТ {g}'); sys.exit(1)
    x = np.asarray(Image.open(f).convert('RGB')).astype(int); y = np.asarray(Image.open(g).convert('RGB')).astype(int)
    if shift: y = np.roll(y, shift, axis=1)
    if x.shape != y.shape: print(f'размеры {f.name}: {x.shape} {y.shape}'); sys.exit(1)
    H = x.shape[0]; top = int(H * ftop); bot = int(H * fbot)               # заголовок окна и строка счётчика
    d = np.abs(x - y)[top:bot].max(axis=2); frac = 100.0 * (d > tol).mean(); worst = max(worst, frac)
    rows.append((f.stem, frac, int(d.max()), float(d.mean())))
    m = np.zeros(d.shape + (3,), np.uint8); m[..., 0] = np.clip(d * 4, 0, 255); m[..., 1] = (d > tol) * 255; tiles.append(m)
print(f'{"снимок":12s} {"разных пикселей %":>18s} {"макс.":>6s} {"средн.":>7s}')
for n, fr, mx, me in rows: print(f'{n:12s} {fr:18.3f} {mx:6d} {me:7.3f}')
ok = worst <= pct
print(f'худший снимок: {worst:.3f} % разных пикселей (порог {pct} %, допуск канала {tol}/255); снимков {len(rows)}'); print('PASS' if ok else 'FAIL')
if outp and tiles:
    k = 4; hh, ww = tiles[0].shape[:2]; sc = 4; rowsimg = []
    ts = [np.asarray(Image.fromarray(t).resize((ww // sc, hh // sc))) for t in tiles]
    while len(ts) % k: ts.append(np.zeros_like(ts[0]))
    rowsimg = [np.concatenate(ts[i:i + k], axis=1) for i in range(0, len(ts), k)]
    Image.fromarray(np.concatenate(rowsimg, axis=0)).save(outp)
sys.exit(0 if ok else 1)
