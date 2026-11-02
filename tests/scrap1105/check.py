#!/usr/bin/env python3
"""№1105 (SCRAP BOOK целиком): проверка кадров прогона tests/scrap1105/scenario.txt (tools/smoke.sh, кадры лежат в папке прогона).
  check.py RUN_DIR [--expect-fail]
 1. sb_book: две строки меню (OPEN BOOK / GAME STATS) в центре; наклейки Красти (золото слева внизу) нет — игра не на 100 %;
 2. sb_contents_l1 / sb_contents_l2: планка уровня меняется (разные кадры сверху), картинки разделов тоже (L / R листают уровни);
 3. sb_contents_cards: выбран раздел карточек (кадр отличается от sb_contents_l2);
 4. sb_stats: список Game Stats — не меньше 10 жёлтых строк (заголовок, девять строк и Game Complete) в левой колонке.
--expect-fail: код 0, только если проверка упала (красный: прогон с SCRAPBOOK1105_OLD=1 — прежний альбом без этих экранов).
Код 0 — PASS."""
import sys
from pathlib import Path
import numpy as np
from PIL import Image


def img(p): return np.asarray(Image.open(p).convert('RGB')).astype(int)


def yellow_rows(a, x0, x1, y0=20, y1=250):
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    m = (r > 200) & (g > 170) & (b < 90)
    prof = m[y0:y1, x0:x1].sum(1) > 2
    n, prev = 0, False
    for v in prof:
        if v and not prev: n += 1
        prev = v
    return n


def diff(a, b, box):
    x0, y0, x1, y1 = box
    return float(np.abs(a[y0:y1, x0:x1] - b[y0:y1, x0:x1]).mean())


def main():
    run = Path(sys.argv[1]); fail = '--expect-fail' in sys.argv; errs = []
    try:
        bk, c1, c2, cc, st = (img(run / f'{n}.png') for n in ('sb_book', 'sb_contents_l1', 'sb_contents_l2', 'sb_contents_cards', 'sb_stats'))
    except FileNotFoundError as e:
        print('FAIL нет кадра', e); sys.exit(0 if fail else 1)
    gold = ((bk[..., 0] > 200) & (bk[..., 1] > 150) & (bk[..., 2] < 80))[170:250, 40:120].sum()
    rows = yellow_rows(bk, 150, 330, 100, 200) + 0
    white = ((bk[..., 0] > 200) & (bk[..., 1] > 200) & (bk[..., 2] > 200))[130:200, 180:300].sum(1) > 2
    wr = int(((white[1:] & ~white[:-1]).sum()) + (1 if white[0] else 0))
    print(f'  sb_book: белых строк в центре {wr}, золота слева внизу {gold} px')
    if wr < 2: errs.append(f'sb_book: строк меню {wr} (ждали 2: OPEN BOOK, GAME STATS)')
    if gold > 400: errs.append(f'sb_book: наклейка Красти видна ({gold} px золота) при неполной игре')
    d_bar, d_pic = diff(c1, c2, (150, 30, 340, 70)), diff(c1, c2, (60, 70, 420, 220))
    print(f'  L1 / L2: планка {d_bar:.2f}, картинки {d_pic:.2f}')
    if d_bar < 1.0: errs.append('планка уровня не меняется по R')
    if d_pic < 1.0: errs.append('картинки разделов не меняются по R')
    d_sel = diff(c2, cc, (60, 70, 420, 260)); print(f'  выбор «карточки»: {d_sel:.2f}')
    if d_sel < 0.3: errs.append('выбор раздела карточек не виден')
    n = yellow_rows(st, 60, 200); print(f'  sb_stats: жёлтых строк {n}')
    if n < 10: errs.append(f'sb_stats: {n} жёлтых строк (ждали 10 и больше)')
    for e in errs: print('FAIL', e)
    ok = not errs
    if fail: ok = not ok
    print(('PASS' if not errs else 'FAIL') + ' · tests/scrap1105/check.py')
    sys.exit(0 if ok else 1)


main()
