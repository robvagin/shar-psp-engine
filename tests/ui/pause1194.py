"""№1194 (Роб: «межстрок уродский» в паузе в задании): шаг строк PauseMission = шагу PauseSunday, список по центру рамки.
  python3 tests/ui/pause1194.py            код 0 — норма; PAUSEMSN1194_OLD=1 — красный контроль (обязан упасть, шаг 39)"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tools'))
import ui_export as U, ui_typo as UT
fc = U.FontCache('psp'); a = U.Assets(); cfg = UT.load()
def rows(sc):
    items, icons = U.build_items(sc); dl, _ = U.adapt(items, icons, 'psp', a, fc); dl = UT.apply(sc, dl, a, fc, cfg)
    r = sorted(t['dst'][1] for t in dl if t.get('type') == 3 and t.get('menu', -1) >= 0 and t['name'] != 'ResetCar')
    f = next(t for t in dl if t['name'] == 'Frame_Bgd')['dst']
    return r, f
rp, fp = rows('pause'); rm, fm = rows('pausemsn')
pp = {b - a_ for a_, b in zip(rp, rp[1:])}; pm = {b - a_ for a_, b in zip(rm, rm[1:])}
top = rm[0] - fm[1]; bot = fm[1] + fm[3] - (rm[-1] + 22)
print('pause шаг', pp, 'pausemsn шаг', pm, 'поля сверху/снизу', top, bot)
bad = 0
if {min(pp)} != pm: print('FAIL шаг не как у PauseSunday'); bad += 1
if abs(top - bot) > 12: print('FAIL список не по центру рамки'); bad += 1
print('RESULT', 'FAIL' if bad else 'OK'); sys.exit(1 if bad else 0)
