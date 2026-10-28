#!/usr/bin/env python3
"""№947б (бывш. №920): курс ходьбы держится, пока стик не менялся игроком, и когда камеру перескочила сама игра (перескок у стены; CAMFLIP). По CTRLYAW/CAMF (flag camlog) сцен tests/ctrl916/ctrl920.txt:
  TW_down: стик вниз 6 с, идём на камеру, она разворачивается за спину (камера повернулась больше 60°);
  WJ_cont: прыжки вдоль стены с зажатым стиком, камера отходит от стены (больше 60°), прыжков не меньше 3;
  WJ_drop: то же, стик на 0,1 с уходит в ноль каждую секунду.
В каждой сцене курс (скорость Гомера в мире, пока он идёт быстрее 1 м/с) не отклоняется от первого больше LIM° (по умолчанию 10), в том числе в прыжке и после приземления.
В WJ_drop стик на 0,1 с уходит в ноль уже после перескока: без заморозки курса (flag flipbusyold 1) ход брался от ушедшего вида. Красный контроль: flag flipbusyold 1 -> WJ_drop FAIL:  python3 ctrl920.py smoke.txt --expect fail
"""
import re, sys
a = sys.argv[1:]; LIM = float(a[a.index('--lim') + 1]) if '--lim' in a else 10.
exp_fail = '--expect' in a and a[a.index('--expect') + 1] == 'fail'
w = lambda x: (x + 180) % 360 - 180
sc = None; R = {}; Y = {}
for l in open(a[0], errors='replace'):
    m = re.match(r'SCENE (\S+)', l)
    if m: sc = m.group(1); continue
    m = re.search(r'CTRLYAW t=(\S+) held=\S+ view=(\S+) camYaw=(\S+) busy=\d bt=\S+ spd=(\S+) on=\d frame=(\S+) walk=(\S+) pos=', l)
    if m and sc: R.setdefault(sc, []).append([float(x) for x in m.groups()][:1] + [float(m.group(3)), float(m.group(4)), float(m.group(5)), float(m.group(6)), float(m.group(2))])   # t, camYaw, spd, frame, walk, view
    m = re.search(r'CAMF t=(\S+) .*? homer=(\S+?),(\S+?),(\S+?) ', l)
    if m and sc: Y.setdefault(sc, []).append((float(m.group(1)), float(m.group(3))))
def jumps(ys):
    """число прыжков: подъём Гомера не меньше 0,8 м над самой низкой точкой за последние 2 с"""
    n = 0; up = False
    for i, (t, y) in enumerate(ys):
        lo = min(v for tt, v in ys[max(0, i - 60):i + 1]) if i else y
        if y - lo > .8 and not up: n += 1; up = True
        elif y - lo < .3: up = False
    return n
ok = True
for name, need_jumps in (('TW_down', 0), ('WJ_cont', 3), ('WJ_drop', 3)):
    # №1059: ходьба = скорость > 3 м/с; при 0,9–1,7 м/с Гомер скользит вдоль торца стены, и мировой курс уходит вдоль стены (80° при кадре стика −170° без изменений, разбор 27.10) — это не смена кадра, поэтому
    # курс считается по ходьбе, а смена кадра стика (колонка frame) проверяется отдельно по всем отсчётам со скоростью > 1
    rows = R.get(name, []); mv = [r for r in rows if r[2] > 3.0]; fr = [r for r in rows if r[2] > 1.0]
    if not mv: print(name + ': нет ходьбы'); ok = False; continue
    c0 = mv[0][4]; dev = max(abs(w(r[4] - c0)) for r in mv)
    cam = max(abs(w(r[1] - mv[0][1])) for r in mv); nj = jumps(Y.get(name, []))
    # №1059: TW_down — ход на камеру (стик вниз): по PS2 камера стоит (< 15°), курс 180° к виду держится; WJ — перескок камеры у стены (игра), курс держится до отпускания стика
    fdev = max(abs(w(r[3] - fr[0][3])) for r in fr) if fr else 0.
    if name == 'TW_down':
        # №1059: ход на камеру. PS2 (wc1059_down200): камера стоит, ход − вид 180,0° ± 0,3°, мировой курс −177,8 → −177,2° (0,6°). В порте камера смотрит на героя, и при боковом смещении в несколько
        # сантиметров вид сдвигается на 2–12° (параллакс у самого глаза), мировой курс идёт за ним; поэтому проверяется то, что показывает PS2: ход − вид = 180° ±10° и камера не уходит за спину (< 15°)
        rel = [abs(w(r[4] - r[5] - 180.)) for r in mv]; rdev = max(rel) if rel else 0.
        good = rdev <= LIM and cam < 15. and nj >= need_jumps
        print(f'{name}: ход − вид {180 - rdev:.0f}…180° (отклонение от 180° не больше {rdev:.0f}°, предел {LIM:.0f}°); камера повернулась на {cam:.0f}° (PS2 стоит); мировой курс сместился на {dev:.0f}° (параллакс)' + ('' if good else '  <-- не годится'))
        ok = ok and good; continue
    good = dev <= LIM and fdev <= LIM and cam > 60. and nj >= need_jumps
    # №1059 (замечание приёмки): условия сцены — перескок камеры игрой (камера повернулась больше 60°) и не меньше нужного числа прыжков. Если их нет, а курс и кадр в пределах, гейт судить не может:
    # «нет условий» (всё равно не PASS, чтобы смена состояния не прошла молча); «не годится» — только когда курс или кадр вышли за предел
    nocond = (cam <= 60. or nj < need_jumps) and dev <= LIM and fdev <= LIM
    tag = '' if good else ('  <-- нет условий (камера не перескочила или мало прыжков: гейт не может судить)' if nocond else '  <-- не годится')
    print(f'{name}: курс {c0:.0f}°, наибольшее отклонение {dev:.0f}° (предел {LIM:.0f}°); камера повернулась на {cam:.0f}°; кадр стика сместился на {fdev:.0f}°; прыжков {nj}' + tag)
    ok = ok and good
print('PASS' if ok else 'FAIL')
sys.exit(0 if ok != exp_fail else 1)
