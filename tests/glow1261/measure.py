#!/usr/bin/env python3
"""Гейт №1261 (свечение двери): кадры нашего движка (максимум и минимум пульса) против кадра PS2 тех же места и расстояния.
  measure.py DIR_нашихКадров [--sheet OUT.png]       DIR: house_13m_max.png, house_13m_min.png (сцена tests/glow1261/scn_phase.txt; house_13m_off.png — тот же кадр без свечения, вычитается как фон)
Размер ореола — габарит (перцентили 1–99) пикселей свечения в метрах на расстоянии кадра (px·(d/f), f=500 px у PS2 480 строк, 226 у нашего кадра 272 строки при угле 62°): у PS2 — «лиловые» пиксели (B−R>18, B−G>25, G<R+35)
в окне у двери, у нас — пиксели, изменённые против того же кадра без свечения (house_13m_off). Условия: ширина и высота в максимуме в пределах ±30 % от PS2 (3,4 × 3,5 м), в минимуме изменённых пикселей ≤ 15 % максимума.
PS2: build/ps2_glow_1261/house_door_near_15m_still_0.png (максимум, 14,9 м)."""
import sys, numpy as np
from PIL import Image
def load(p): return np.asarray(Image.open(p).convert('RGB')).astype(int)
def mask(a): R, G, B = a[..., 0], a[..., 1], a[..., 2]; return (B - R > 18) & (B - G > 25) & (G < R + 35)
def stat(a, roi, d, f, base=None):
    x0, y0, x1, y1 = roi; m = mask(a)[y0:y1, x0:x1]; s = d / f
    if base is not None: m = m & ~mask(base)[y0:y1, x0:x1]   # фон без свечения (тот же кадр с flag glow1261old 1) вычитается
    return m.sum() * s * s, (a[y0:y1, x0:x1][m].mean(0) if m.any() else np.zeros(3))
d = sys.argv[1]; root = __file__.rsplit('/tests/', 1)[0]
def bbox_m(m, dist, f, roi_off=(0, 0)):
    ys, xs = np.nonzero(m)
    if len(ys) < 20: return 0.0, 0.0, (0, 0)
    x0, x1 = np.percentile(xs, [1, 99]); y0, y1 = np.percentile(ys, [1, 99]); s = dist / f
    return (x1 - x0 + 1) * s, (y1 - y0 + 1) * s, ((x0 + x1) / 2 + roi_off[0], (y0 + y1) / 2 + roi_off[1])
ps2 = load(root + '/build/ps2_glow_1261/house_door_near_15m_still_0.png')
px0, py0, px1, py1 = 195, 60, 395, 215   # окно шире ореола (иначе габарит обрезается окном)
wp, hp, _ = bbox_m(mask(ps2)[py0:py1, px0:px1], 14.9, 500.)
omax, omin, ooff = load(d + '/house_13m_max.png'), load(d + '/house_13m_min.png'), load(d + '/house_13m_off.png')
R = (150, 30, 360, 135)   # окно у двери нашего кадра: выше капота машины (нижние строки — анимация Гомера в машине, она менялась между кадрами и раздувала высоту: 165 % у Будки-93)
def chg(a): x0, y0, x1, y1 = R; return (np.abs(a - ooff).max(2) > 60)[y0:y1, x0:x1]   # порог 60: заливка и ободок, без слабого внешнего ореола
wo, ho, cen = bbox_m(chg(omax), 13.0, 226., (R[0], R[1])); amin = chg(omin).sum(); amax = chg(omax).sum()   # f нашего кадра: 136/tan(31°) при вертикальном угле 62°
ok = True
print('PS2 ореол: %.2f x %.2f м (ширина x высота, перцентили 1–99)' % (wp, hp))
print('наш максимум: %.2f x %.2f м (%.0f %% / %.0f %% PS2), центр по кадру x=%.0f y=%.0f' % (wo, ho, 100 * wo / wp, 100 * ho / hp, cen[0], cen[1]))
print('наш минимум: изменённых пикселей %d (%.1f %% максимума)' % (amin, 100 * amin / max(amax, 1)))
if not (0.7 * wp <= wo <= 1.3 * wp and 0.7 * hp <= ho <= 1.3 * hp): print('FAIL размер ореола вне ±30 %% от PS2 (%.2f..%.2f x %.2f..%.2f)' % (0.7 * wp, 1.3 * wp, 0.7 * hp, 1.3 * hp)); ok = False
if amin > 0.15 * amax: print('FAIL в минимуме пульса свечение не исчезает'); ok = False
if '--sheet' in sys.argv:
    out = sys.argv[sys.argv.index('--sheet') + 1]
    h = 272; p = np.asarray(Image.fromarray(ps2.astype('uint8')).resize((int(640 * h / 480), h), Image.LANCZOS)).astype(int)
    Image.fromarray(np.hstack([p, omax]).astype('uint8')).save(out)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
