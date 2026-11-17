#!/usr/bin/env python3
"""1366: знак загрузки только на закрытом ирисе (transit.c). Снимки: OUT/out_lm1360_0 (новое) и out_lm1360_1 (LOADMARK1360_OLD=1) с iris-сценарием tests/loadmark1360/run.sh;
OUT/out_scnew и out_scold (экраны вокруг игры tools/screens_scenario.txt, то же с loadmark1360old 1), out_snnew/out_snold (то же с газетой).
Новое на ирисе: жёлтая рамка и жёлтые фигуры на чёрном (внутри рамки чёрное, не сплошной круг), символ не выше 24 px и пульсирует, белая LOADING левее, по центру символа по высоте.
Старое: знака нет. Остальные экраны (старт, меню, газета, загрузка уровня) — правый нижний угол совпадает с прогоном OLD (знака нет).   python3 tests/loadmark1366/check.py OUT | --selftest"""
import os, sys, glob
import numpy as np
from PIL import Image
def load(p): return np.asarray(Image.open(p).convert('RGB'))[:272, :480].astype(int)
def yel(im): return (im[..., 0] > 190) & (im[..., 1] > 160) & (im[..., 2] < 120)
def wht(im): return (im[..., 0] > 200) & (im[..., 1] > 200) & (im[..., 2] > 200)
def bbox(m, x0, y0):
    ys, xs = np.where(m); return (xs.min() + x0, xs.max() + x0, ys.min() + y0, ys.max() + y0, len(xs)) if len(xs) else None
ok = True
def chk(c, why):
    global ok; ok &= bool(c); print('  %-110s %s' % (why, 'ok' if c else 'FAIL'))
def mark_in(im):
    m = yel(im)[200:260, 420:480]; return bbox(m, 420, 200)   # символ 30 px: правый нижний угол (поля 18 справа, 14 снизу); строка отладки внизу не в счёт
def iris(frames):
    sym, txt = [], []
    for im in frames:
        s = mark_in(im)
        if s and (s[1] - s[0]) >= 26:
            sym.append(s); t = bbox(wht(im)[200:258, 330:430], 330, 200); txt.append(t)
    return sym, txt
def run(out):
    new = [load(f) for f in sorted(glob.glob(os.path.join(out, 'out_lm1360_0', 'm*.bmp')))]
    old = [load(f) for f in sorted(glob.glob(os.path.join(out, 'out_lm1360_1', 'm*.bmp')))]
    sym, txt = iris(new); print('кадров со знаком на ирисе: %d из %d' % (len(sym), len(new)))
    chk(len(sym) >= 5, 'новое: знак виден на закрытом ирисе')
    if sym:
        h = [s[3] - s[2] + 1 for s in sym]; w = [s[1] - s[0] + 1 for s in sym]
        chk(29 <= max(h) <= 32 and 29 <= max(w) <= 32, 'новое: символ 30 px (наибольший кадр 29..32; кадр с неполным символом — ирис уходит) (%d..%d x %d..%d)' % (min(w), max(w), min(h), max(h)))
        rm = [480 - s[1] - 1 for s in sym]; bm = [272 - s[3] - 1 for s in sym]
        chk(17 <= min(rm) and max(rm) <= 21 and 13 <= min(bm) and max(bm) <= 17, 'новое: поля справа ~18 и снизу ~14 (%d..%d, %d..%d; пульс уменьшает символ)' % (min(rm), max(rm), min(bm), max(bm)))
        chk(min(h) <= .95 * max(h) or min(w) <= .95 * max(w), 'новое: пульс есть')
        fill = [s[4] / float(((s[1] - s[0] + 1) * (s[3] - s[2] + 1))) for s in sym]
        chk(max(fill) < .55, 'новое: внутри рамки пусто, фигуры не сплошной круг (доля жёлтого %.2f < 0,55; сплошной круг ≈ 0,78)' % max(fill))
        tt = [t for t in txt if t]
        chk(len(tt) >= len(sym) - 1 and all(t[4] >= 20 for t in tt), 'новое: белая LOADING есть (%d кадров)' % len(tt))
        cy = [(s[2] + s[3]) / 2 for s in sym]; ty = [(t[2] + t[3]) / 2 for t in txt if t]
        chk(ty and max(abs(a - b) for a, b in zip(cy, ty)) <= 3, 'новое: LOADING по центру символа по высоте')
        chk(all(t[1] < s[0] for s, t in zip(sym, txt) if t), 'новое: надпись левее символа')
        gp = [s[0] - t[1] - 1 for s, t in zip(sym, txt) if t]
        chk(min(gp) >= 10 and max(gp) <= 14, 'новое: правый край букв в ~12 px от рамки символа (%d..%d)' % (min(gp), max(gp)))
        ref = os.environ.get('LM_REF') or os.path.expanduser('~/Downloads/SHAR/1366_снимки/ЭТАЛОН_B_символ30_отступ12.png')
        if os.path.exists(ref):
            R = load(ref); box = (slice(200, 258), slice(280, 480)); best = 0; bi = 0
            for k, im in enumerate(new):
                a = im[box]; r = R[box]; agree = (np.abs(a - r).sum(axis=2) < 60).mean()
                ink = (a.sum(axis=2) > 120) | (r.sum(axis=2) > 120); inkagree = ((np.abs(a - r).sum(axis=2) < 120) & ink).sum() / max(1, ink.sum())
                if agree > best: best, bi, bink = agree, k, inkagree
            chk(best >= .95 and bink >= .75, 'новое: совпадение с эталоном Роба (вариант B) в области 200x58 справа внизу: %.1f %% (кадр m%d), по чернилам %.0f %%' % (best * 100, bi, bink * 100))
        else: print('  (эталон варианта B не найден: сравнение пропущено)')
    so, _ = iris(old); chk(len(so) == 0, 'красный контроль: на OLD знака нет (кадров %d)' % len(so))
    n = 0
    for pre in ('sc', 'sn'):
        for f in sorted(glob.glob(os.path.join(out, 'out_%snew' % pre, '*.bmp'))):
            g = os.path.join(out, 'out_%sold' % pre, os.path.basename(f))
            if not os.path.exists(g): continue
            a, b = load(f), load(g); d = (np.abs(a - b).sum(axis=2) > 30)[190:272, 350:480].sum(); n += 1
            chk(d <= 8 and mark_in(a) is None or (mark_in(a) is not None and mark_in(b) is not None), '%s: правый нижний угол как в OLD (разница %d px), знака нет' % (os.path.basename(f), d))
    chk(n >= 8, 'сверено экранов: %d' % n)
    red = sorted(glob.glob(os.path.join(out, 'out_snred', '*.bmp'))); hit = [os.path.basename(f) for f in red if mark_in(load(f)) is not None]
    if red: chk(len(hit) >= 2, 'красный контроль: поведение 1360 (LOADMARK1366_OLD=1) даёт знак на экранах: %s' % ' '.join(hit))
    else: chk(False, 'красный контроль: нет прогона out_snred')
if '--selftest' in sys.argv:
    im = np.zeros((272, 480, 3), int); print('чистый кадр: знак', mark_in(im))
    im[230:250, 440:458] = (241, 209, 72); print('подложенный жёлтый квадрат: знак', mark_in(im) is not None)
    sys.exit(0 if mark_in(np.zeros((272, 480, 3), int)) is None and mark_in(im) else 1)
run(sys.argv[1]); print('LOADMARK1366', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
