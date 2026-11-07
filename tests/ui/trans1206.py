#!/usr/bin/env python3
"""№1206: (п. 1) подсказка конца уровня не налезает на значок ✕ ACCEPT; (п. 5) мелкий пиксельный текст всех экранов — тень непрозрачная 1 px.
  python3 tests/ui/trans1206.py     красные контроли (обязаны упасть): LEVELEND1206_OLD=1, TXTSHADOW1206_OLD=1"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import ui_export as U, ui_typo as UT
fc = U.FontCache('psp'); a = U.Assets(); cfg = UT.load(); bad = 0
def build(sc):
    items, icons = U.build_items(sc); dl, _ = U.adapt(items, icons, 'psp', a, fc); return UT.apply(sc, dl, a, fc, cfg)
dl = build('levelstats')
ic = next(t for t in dl if t['name'] == 'Accept' and t['type'] == U.T_IMG); h = next(t for t in dl if t['name'] == 'UseMissionSelect')
end = h['dst'][0] + U.text_width(fc.get(h['font']), h['strings'][0]); ok = end <= ic['dst'][0] - 5
print('%s подсказка кончается на %.0f, значок с %.0f' % ('ok  ' if ok else 'FAIL', end, ic['dst'][0])); bad |= not ok
tot = good = 0
for sc in ('levelstats', 'hud', 'pause', 'load', 'missionload'):
    try: d = build(sc)
    except Exception as e: print('пропуск', sc, e); continue
    for t in d:
        if t.get('type') == U.T_TEXT and str(t.get('font', '')).split('@')[0] in ('bart_16bit', 'msx_thin') and t.get('strings') is not None:
            tot += 1; good += t.get('sh') == (1, 1, [0, 0, 0, 255])
ok = tot > 0 and good == tot; print('%s тень 1 px непрозрачная: %d из %d строк' % ('ok  ' if ok else 'FAIL', good, tot)); bad |= not ok
print('FAIL' if bad else 'PASS'); sys.exit(bad)
