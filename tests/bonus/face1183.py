#!/usr/bin/env python3
"""№1183: лица игроков в итогах бонус-гонки — по долям оригинала PS2 (эталон emu-ps2: первое место 0,146 высоты экрана, остальные 0,115; центр лиц по x один) и не выше шага строк (у PS2 лицо первого места равно шагу).
  python3 tests/bonus/face1183.py     красный контроль: BONUS_SUMMARY_FACE1183_OLD=1 (лицо 0,195 высоты — обязано упасть)"""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/bonus'))
import ui_export as U, screens_export as S
fc = U.FontCache('psp'); a = S.mg_assets(); bad = 0; cx = []
H = U.DST_H
rows = []
for k in range(4):
    for t in S.build('msrow%d' % k, a, fc):
        if t['name'] == 'Face%d' % k:
            x, y, w, h = t['dst']; rows.append((k, y + h / 2, h)); cx.append(x + w / 2)
pitch = rows[1][1] - rows[0][1]
for k, cy, h in rows:
    want = (0.146 if k == 0 else 0.115) * H
    ok = abs(h - want) <= 2.0 and h <= pitch + 1
    print('%s лицо %d: высота %.1f px (%.3f экрана), эталон %.1f px, шаг строк %.1f' % ('ok  ' if ok else 'FAIL', k, h, h / H, want, pitch)); bad |= not ok
ok = max(cx) - min(cx) <= 1.5
print('%s центры лиц по x: %.1f..%.1f' % ('ok  ' if ok else 'FAIL', min(cx), max(cx))); bad |= not ok
# TEST90: корона лучшего времени / круга не касается ни шапки столбца над ней, ни цифр под ней
import ui_typo as UT
cfg = UT.load(); scr = {sc: UT.apply(sc, S.build(sc, a, fc), a, fc, cfg) for sc in ('msbase', 'msrow0')}; S.post(scr, fc)
for nm in ('TotalTime', 'BestLap'):
    h_ = next(t for t in scr['msbase'] if t['name'] == nm); c_ = next(t for t in scr['msrow0'] if t['name'] == nm + 'KingIcon'); v_ = next(t for t in scr['msrow0'] if t['name'] == nm + '0')
    top, bot = c_['dst'][1], c_['dst'][1] + c_['dst'][3]; ok = top >= h_['dst'][1] + h_['dst'][3] - 1 and bot <= v_['dst'][1] + 2
    print('%s корона %s: %.0f..%.0f, шапка до %.0f, цифры с %.0f' % ('ok  ' if ok else 'FAIL', nm, top, bot, h_['dst'][1] + h_['dst'][3], v_['dst'][1])); bad |= not ok
print('FAIL' if bad else 'PASS'); sys.exit(bad)
