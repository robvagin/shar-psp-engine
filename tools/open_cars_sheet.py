#!/usr/bin/env python3
"""Лист кадров машин для сверки «открытая / закрытая» глазами: open_cars_sheet.py OUT.png имя [имя ...]
Слева вид сбоку, справа спереди. Красный столбик — верх причёски стоймя (1,28 м над сиденьем dl), зелёный — причёска назад (1,05 м), точка — сиденье."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, str(Path(__file__).resolve().parent))
import psp_export as pe, open_cars as oc

def view(T, dl, size=(300, 170)):
    """вид сбоку (ортогонально, смотрим вдоль оси x) и спереди (вдоль z) рядом: что над сиденьем"""
    W, H = size; img = Image.new('RGB', (W * 2, H), (205, 215, 225)); d = ImageDraw.Draw(img)
    sc = 55.0; L = np.array([.4, .8, -.4]); L /= np.linalg.norm(L)
    n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-9; sh = np.abs(n @ L) * .6 + .35
    for k, (ax, flip) in enumerate(((2, 1), (0, 1))):                # k=0: сбоку (по горизонтали z), k=1: спереди (по горизонтали x)
        ox = k * W + W / 2; oy = H * .62
        def pr(P): return (ox + sc * (P[..., ax] - dl[ax]) * flip, oy - sc * (P[..., 1] - dl[1]))
        depth = T[:, :, 0 if ax == 2 else 2].mean(1)
        for i in np.argsort(depth if ax == 2 else -depth):
            x, y = pr(T[i]); g = int(255 * min(1, sh[i])); d.polygon(list(zip(x, y)), fill=(g, int(g * .9), int(g * .8)))
        for h, col in ((1.28, (220, 0, 0)), (1.05, (0, 150, 0))):
            d.line([(ox, oy), (ox, oy - sc * h)], fill=col, width=2)
        d.ellipse([ox - 3, oy - 3, ox + 3, oy + 3], fill=(0, 0, 255))
    return img

if __name__ == '__main__':
    out, names = sys.argv[1], sys.argv[2:]; tiles = []
    for n in names:
        T, dl = oc.load(pe.GAME / 'art' / 'cars' / f'{n}.p3d', wheels=True)
        im = view(T, dl); ImageDraw.Draw(im).text((4, 3), n, fill=(0, 0, 0)); tiles.append(im)
    cols = 3; rows = (len(tiles) + cols - 1) // cols; sheet = Image.new('RGB', (600 * cols, 170 * rows), (255, 255, 255))
    for i, t in enumerate(tiles): sheet.paste(t, (600 * (i % cols), 170 * (i // cols)))
    sheet.save(out)
