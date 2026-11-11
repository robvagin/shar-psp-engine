#!/usr/bin/env python3
"""Модели-96, правка 1240: снимок Мардж за рулём — открытая (famil_v) и закрытая (sedana) машины, «до» (причёска как в клипе, назад) и «после»
(правило: открытая машина по tools/open_cars.txt — причёска в покое, т.е. стоймя). Вид сбоку. Мардж из l4_player.shr (то, что кладёт экспорт).
  snap.py L4_PLAYER.shr OUT.png [машина ...]      (по умолчанию famil_v sedana)"""
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce, psp_export as pe, open_cars as oc

from PIL import ImageFont
FONT = ImageFont.truetype('/System/Library/Fonts/Supplemental/Arial.ttf', 14)

def marge(shr):
    b = Path(shr).read_bytes(); i = b.find(b'CHR1'); t = Path(shr).with_suffix('.tmp.chr'); t.write_bytes(b[i:])
    try: return ce.read_chr(t)
    finally: t.unlink()

PELVIS_ON_SEAT = np.array([-0.049, 0.125, -0.334])   # таз Мардж относительно сустава сиденья dl: как ставит движок (tests/homer/seat_pelvis.sh, famil_v: таз −0,486 −0,061 −0,162; dl −0,437 −0,186 0,174)

def marge_tris(m, hair_rest, seat):
    names = [c['name'] for c in m['clips']]; jn = [j['name'] for j in m['joints']]
    loc = list(ce.sample_local(m, names.index('ndr_in_car_idle'), 0.))
    if hair_rest:
        for i, j in enumerate(m['joints']):
            if j['name'].startswith('hair_'): loc[i] = (j['q'], j['t'])
    pal, w = ce.palette(m, loc); sk = ce.skin(m, pal)
    ped = w[jn.index('Pelvis')][:3, 3]; out = []
    for P, N, UV, t, flags in sk:
        col = m['tex'][t]['rgba'].reshape(-1, 4)[:, :3].mean(0) / 255.
        Q = P - ped; Q = np.stack([-Q[:, 0], Q[:, 1], -Q[:, 2]], 1) + seat           # лицом вперёд машины (-Z), таз там, где его ставит движок
        for k in range(len(Q) - 2): out.append((Q[k:k + 3], col))
    return out

def panel(car_tris, mt, dl, label, size=(520, 330)):
    W, H = size; img = Image.new('RGB', size, (205, 215, 225)); d = ImageDraw.Draw(img); sc = 85.; ox, oy = W / 2, H * .72
    items = [(T, None) for T in car_tris if T[:, 0].max() > dl[0] - .12] + mt     # вырез ближней (левой) стенки: смотрим с левого борта
    pr = lambda P: (ox + sc * (P[:, 2] - dl[2]), oy - sc * (P[:, 1] - dl[1]))
    L = np.array([.4, .8, -.4]); L /= np.linalg.norm(L)
    for T, col in sorted(items, key=lambda it: -it[0][:, 0].mean()):
        n = np.cross(T[1] - T[0], T[2] - T[0]); n /= np.linalg.norm(n) + 1e-9; sh = abs(n @ L) * .6 + .4
        x, y = pr(T)
        fill = tuple(int(255 * min(1, sh * c)) for c in col) if col is not None else tuple(int(200 * sh * k) for k in (1, .9, .8))
        if col is None: fill = tuple(int(70 + 90 * sh) for _ in range(3))
        d.polygon(list(zip(x, y)), fill=fill)
    d.text((6, 4), label, fill=(0, 0, 0), font=FONT); return img

if __name__ == '__main__':
    shr, out = sys.argv[1], sys.argv[2]; cars = sys.argv[3:] or ['famil_v', 'sedana']
    m = marge(shr); opn = {l.strip() for l in (ROOT / 'tools/open_cars.txt').read_text().splitlines() if l.strip() and not l.startswith('#')}
    rows = []
    for c in cars:
        T, dl = oc.load(pe.GAME / 'art' / 'cars' / f'{c}.p3d', wheels=True); ct = list(T)
        is_open = c in opn
        for hair_rest, lab in ((False, 'до (причёска назад, как в клипе)'), (is_open, 'после')):
            rows.append(panel(ct, marge_tris(m, hair_rest, dl + PELVIS_ON_SEAT), dl, f'{c} — {"открытая" if is_open else "закрытая"} — {lab}'))
    cols = 2; sheet = Image.new('RGB', (520 * cols, 330 * ((len(rows) + 1) // 2)), (255, 255, 255))
    for i, r in enumerate(rows): sheet.paste(r, (520 * (i % cols), 330 * (i // cols)))
    sheet.save(out)
