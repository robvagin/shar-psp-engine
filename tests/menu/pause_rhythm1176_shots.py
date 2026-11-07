#!/usr/bin/env python3
"""№1176: замер ритма страниц паузы по снимкам эмулятора (480x272): рамка (жёлтая линия по центру), поле до первой и последней строки (белые/жёлтые буквы), промежутки между строками.
  python3 -I tests/menu/pause_rhythm1176_shots.py main_old_E.png ... """
import sys
from PIL import Image
import numpy as np
def meas(fn):
    a = np.asarray(Image.open(fn).convert('RGB'), int)
    st = a[:, 232:248]
    yel_s = ((st[:, :, 0] > 220) & (st[:, :, 1] > 170) & (st[:, :, 2] < 110)).sum(1)
    rows = np.where(yel_s >= 10)[0]
    top, bot = int(rows[0]), int(rows[-1])
    x = a[:, 150:330]
    txt = ((x[:, :, 0] > 225) & (x[:, :, 1] > 225) & (x[:, :, 2] > 225)).sum(1) + ((x[:, :, 0] > 220) & (x[:, :, 1] > 170) & (x[:, :, 2] < 110)).sum(1)
    inner = [y for y in range(top + 6, bot - 5) if txt[y] > 2]
    g = []
    for y in inner:
        if g and y - g[-1][1] <= 1: g[-1][1] = y
        else: g.append([y, y])
    gaps = [g[i + 1][0] - g[i][1] - 1 for i in range(len(g) - 1)]
    return top, bot, g[0][0] - top, bot - g[-1][1], len(g), gaps
if __name__ == '__main__' and sys.argv[1:2] != ['--gate']:
    for fn in sys.argv[1:]:
        top, bot, mt, mb, n, gaps = meas(fn)
        print(f'{fn}: рамка {top}..{bot} (выс {bot - top + 1}); поле сверху {mt}, снизу {mb}; строк с яркими буквами {n}; промежутки {gaps}')
# сравнение страниц на экране: python3 -I tests/menu/pause_rhythm1176_shots.py --gate main.png options.png  (код выхода 1 при нарушении)
# главная страница идёт по панели Роба (pause_items.pitch = 26): между яркими строками CONTINUE и OPTIONS четыре шага, высота букв 11 px, значит промежуток соседних строк = 26 - 11 = 15.
# У OPTIONS должны быть тот же промежуток и поле от линии рамки, равное полю главной страницы с точностью 1 px; промежуток не больше поля
def gate(main, opt):
    _, _, mt, mb, _, mg = meas(main); _, _, ot, ob, _, og = meas(opt)
    main_gap = round((mg[0] + 11) / 4) - 11   # CONTINUE -> OPTIONS: четыре шага
    fails = []
    if set(og) != {main_gap}: fails.append(f'промежуток строк OPTIONS {og} не равен промежутку главной страницы {main_gap}')
    if abs(ot - mt) > 1 or abs(ob - mb) > 1: fails.append(f'поле OPTIONS {ot}/{ob} отличается от главной {mt}/{mb} больше чем на 1 px')
    if max(og) > min(ot, ob): fails.append(f'промежуток {max(og)} больше поля {min(ot, ob)}')
    print(f'главная: поле {mt}/{mb}, промежуток {main_gap}; OPTIONS: поле {ot}/{ob}, промежутки {og}')
    return fails
if len(sys.argv) == 4 and sys.argv[1] == '--gate':
    f = gate(sys.argv[2], sys.argv[3]); print('PAUSE RHYTHM SHOTS GATE:', 'FAIL ' + '; '.join(f) if f else 'PASS'); sys.exit(1 if f else 0)
