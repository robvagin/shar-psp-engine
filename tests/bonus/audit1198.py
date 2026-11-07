#!/usr/bin/env python3
"""Аудит типографики 05.11 (№1195, №1197, п. 6): экран выбора трассы и итоги бонус-игры.
  python3 tests/bonus/audit1198.py     красные контроли (обязаны упасть): BONUS_LAPS1195_OLD=1, BONUS_SUMMARY_CROWN1197_OLD=1, BONUS_SHADOW1198_OLD=1"""
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/bonus'))
import ui_export as U, screens_export as S, ui_typo as UT
fc = U.FontCache('psp'); a = S.mg_assets(); cfg = UT.load(); bad = 0
scr = {sc: UT.apply(sc, S.build(sc, a, fc), a, fc, cfg) for sc in ('mmtrack', 'msbase', 'msrow0')}
raw = {t['name']: tuple(t['dst'][2:]) for t in scr['msrow0'] if 'King' in t['name']}   # размер картинки, как она ушла в пакет (post размер картинки не меняет)
S.post(scr, fc)
def chk(ok, msg):
    global bad
    print('%s %s' % ('ok  ' if ok else 'FAIL', msg)); bad |= not ok
nl = [t for t in scr['mmtrack'] if t['name'].startswith('NumLaps') and t.get('strings')]
chk(nl and all(' ' not in x for t in nl for x in t['strings']), '«× N» без пробела между знаком и цифрой (%s)' % (nl[0]['strings'][:2] if nl else None))
for nm in ('TotalTime', 'BestLap'):
    h_ = next(t for t in scr['msbase'] if t['name'] == nm); c_ = next(t for t in scr['msrow0'] if t['name'] == nm + 'KingIcon'); v_ = next(t for t in scr['msrow0'] if t['name'] == nm + '0')
    chk(c_['dst'][3] <= 9 and c_['dst'][1] >= h_['dst'][1] + h_['dst'][3] - 1 and c_['dst'][1] + c_['dst'][3] <= v_['dst'][1] + 2, 'корона %s: высота %.0f, %.0f..%.0f (шапка до %.0f, цифры с %.0f)' % (nm, c_['dst'][3], c_['dst'][1], c_['dst'][1] + c_['dst'][3], h_['dst'][1] + h_['dst'][3], v_['dst'][1]))
rk = next(t for t in scr['msrow0'] if t['name'] == 'RankKingIcon'); r0 = next(t for t in scr['msrow0'] if t['name'] == 'Rank0')
chk(rk['dst'][3] <= 9 and rk['dst'][1] + rk['dst'][3] <= r0['dst'][1] + 1, 'корона места: высота %.0f, низ %.0f, подпись места с %.0f' % (rk['dst'][3], rk['dst'][1] + rk['dst'][3], r0['dst'][1]))
chk(raw and all(abs(w - 13) <= 1 and abs(h - 9) <= 1 for w, h in raw.values()), 'размер картинок корон в пакете: %s' % raw)
sm = [t for dl in scr.values() for t in dl if t.get('type') == U.T_TEXT and str(t.get('font', '')).split('@')[0] in ('bart_16bit', 'msx_thin')]
chk(sm and all(t.get('sh') == (1, 1, [0, 0, 0, 255]) for t in sm), 'тень мелкого текста: непрозрачная, 1 px (%d строк)' % len(sm))
print('FAIL' if bad else 'PASS'); sys.exit(bad)
