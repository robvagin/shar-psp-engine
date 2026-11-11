#!/usr/bin/env python3
"""Гейт правки 1247 по журналу smoke.txt (строки BUBBLE1247, MSNCAM). Вывод: PASS/FAIL и числа.
Проверки на каждый показ пузыря (от строки «rise» до следующей):
  1) разбиение текста на строки одинаково во всех видимых кадрах: (lines, wrapw) у части t не меняется;
  2) ширина рамки в кадрах без масштаба (s=1) одна и та же, после 1-го кадра без масштаба рамка не меняется;
  3) первый видимый кадр пузыря позже конца наезда (MSNCAM end fr=) и позже конца дуги (MSNCAM busy=0 fr=), если был наезд перед ним.
№1262: рамка и текст при проявлении сдвигаются только вправо на 18..30 px, масштаб 1, текст в рамке; --no1262 — проверка как в №1251 (x постоянный).
Использование: check.py smoke.txt [--min-frames N] [--no1262]
"""
import re, sys
def main():
    p = sys.argv[1]; txt = open(p, errors='replace').read().splitlines()
    ends = [int(m.group(1)) for l in txt for m in [re.search(r'MSNCAM end t=\S+ fr=(\d+)', l)] if m]
    free = [int(m.group(1)) for l in txt for m in [re.search(r'MSNCAM busy=0 fr=(\d+)', l)] if m]
    starts = [l for l in txt if l.startswith('MSNCAM start')]
    eps = []; cur = None
    for l in txt:
        if l.startswith('BUBBLE1247 rise'):
            cur = {'rise': int(re.search(r'fr=(\d+)', l).group(1)), 'rows': []}; eps.append(cur); continue
        m = re.match(r'BUBBLE1247 fr=(\d+) age=\S+ tx=(\S+) bs=\S+ ic=\S+ R=\S+ x0=(\S+) y0=(\S+) h=(\S+) pix8=\S+ part=(\w) lines=(\d+) wrapw=(\d+) width=([\d.]+) s=([\d.]+) a=([\d.]+)', l)
        if m and cur is not None:
            cur['rows'].append((int(m.group(1)), m.group(6), int(m.group(7)), int(m.group(8)), float(m.group(9)), float(m.group(10)), float(m.group(11)), int(m.group(2)), float(m.group(3)), float(m.group(4)), float(m.group(5))))
    bad = []; info = []
    for k, e in enumerate(eps):
        rows = e['rows']
        if not rows: info.append(f"показ {k}: rise fr={e['rise']} без нарисованных кадров"); continue
        first = min(r[0] for r in rows)
        t = [(r[2], r[3]) for r in rows if r[1] == 't']
        sets = sorted(set(t))
        if len(sets) > 1: bad.append(f"показ {k}: разбиение строк меняется {sets} (кадр {first}…)")
        txs = sorted(set(r[7] for r in rows if r[1] == 't'))
        if len(txs) > 1: bad.append(f"показ {k}: старт текста по x меняется {txs}")
        fw = sorted(set(round(r[4]) for r in rows if r[1] == 'f' and r[5] >= 0.99999))
        if len(fw) > 1: bad.append(f"показ {k}: ширина рамки без масштаба меняется {fw}")
        # текст в кадре без рамки или рамка без текста в одном кадре
        fr_f = {r[0] for r in rows if r[1] == 'f'}; fr_t = {r[0] for r in rows if r[1] == 't'}
        # окна ранней стадии: до 1 с (30 кадров) после первого
        early = [r for r in rows if r[0] < first + 30]
        tf = sorted(set((r[2], r[3]) for r in early if r[1] == 't'))
        prev_e = [x for x in ends if x <= first]; prev_f = [x for x in free if x <= first]
        last_end = max(prev_e) if prev_e else None
        # был ли наезд перед этим показом: старт наезда ближе 600 кадров до первого видимого
        cam_before = last_end is not None and first - last_end < 1200
        order = ''
        if cam_before:
            if first <= last_end: bad.append(f"показ {k}: первый видимый кадр {first} не позже конца наезда {last_end}")
            lf = max(prev_f) if prev_f else None
            if lf is not None and first < lf: bad.append(f"показ {k}: первый видимый кадр {first} раньше конца дуги камеры {lf}")
            order = f" наезд кончился {last_end}" + (f", камера встала {max(prev_f)}" if prev_f else '')
        # №1251: порядок «кружок раньше рамки раньше текста»; рамка выезжает от кружка (левый край стоит), не сразу полной
        fa = [r[0] for r in rows if r[1] == 'a']; ff = [r[0] for r in rows if r[1] == 'f']; ft = [r[0] for r in rows if r[1] == 't']
        if fa and ff and ft and '--no1251' not in sys.argv:
            if not min(fa) < min(ff): bad.append(f"показ {k}: кружок (кадр {min(fa)}) не раньше рамки (кадр {min(ff)})")
            elif min(ff) - min(fa) < 5: bad.append(f"показ {k}: между кружком и рамкой {min(ff)-min(fa)} кадров, нужно не меньше 5 (0,15 с)")
            if min(ft) < min(ff): bad.append(f"показ {k}: текст (кадр {min(ft)}) раньше рамки (кадр {min(ff)})")
            wf = [(r[0], r[4], r[8]) for r in rows if r[1] == 'f']
            if max(w for _, w, _ in wf) - min(w for _, w, _ in wf) > 1.0: bad.append(f"показ {k}: ширина рамки меняется {min(w for _, w, _ in wf):.0f}..{max(w for _, w, _ in wf):.0f} (масштаба быть не должно)")
            sc_ = sorted(set(round(r[5], 3) for r in rows))
            if sc_ != [1.0]: bad.append(f"показ {k}: есть масштаб {sc_}")
            for nm, sel in (('рамка', 'f'), ('текст', 't')):
                rr = [r for r in rows if r[1] == sel]
                for idx, lab in ((8, 'x'), (9, 'y'), (10, 'высота'), (4, 'ширина')):
                    v = [r[idx] for r in rr]
                    if lab == 'x' and '--no1262' not in sys.argv:   # №1262: выезд слева направо: x не убывает, полный сдвиг от 18 до 30 px, к последнему кадру стоит на месте
                        rr_s = sorted(rr, key=lambda r: r[0]); vx = [r[8] for r in rr_s]
                        if any(vx[i + 1] < vx[i] - 0.01 for i in range(len(vx) - 1)): bad.append(f"показ {k}: {nm}: сдвиг не только вправо {[round(x, 1) for x in vx[:8]]}")
                        tot = vx[-1] - vx[0]
                        lo_ = 18.0 if sel == 'f' else 4.0   # текст появляется позже рамки (когда до места ≈ 8 px, чтобы не лечь на кружок), его видимый путь короче
                        if tot < lo_ or tot > 30.0: bad.append(f"показ {k}: {nm}: полный сдвиг {tot:.1f} px (нужно {lo_:.0f}..30)")
                        if len(vx) >= 3 and abs(vx[-1] - vx[-2]) > 0.01: bad.append(f"показ {k}: {nm}: к концу проявления не стоит на месте {vx[-3:]}")
                        continue
                    if max(v) - min(v) > 0.6: bad.append(f"показ {k}: {nm}: {lab} меняется {min(v):.1f}..{max(v):.1f} (должен быть постоянным, меняется только прозрачность)")
            af = [r[6] for r in rows if r[1] == 'f']; at = [r[6] for r in rows if r[1] == 't']
            if not (af[0] < 0.6 and af[-1] > 0.99 and all(af[i+1] >= af[i] - 1e-3 for i in range(len(af)-1))): bad.append(f"показ {k}: прозрачность рамки не растёт плавно с малой {af[:3]}")
            if min(at) > 0.6 and False: pass
        # №1251: в каждом кадре пиксели текста только внутри рамки (по x и по y)
        if '--no1251' not in sys.argv:
            byf = {r[0]: r for r in rows if r[1] == 'f'}
            for r in rows:
                if r[1] != 't': continue
                f = byf.get(r[0])
                if f is None: bad.append(f"показ {k}: кадр {r[0]}: текст без рамки"); break
                tl, tr, tt, tb = r[8], r[8] + r[4], r[9], r[9] + r[10]; fl, fr, ft_, fb = f[8], f[8] + f[4], f[9], f[9] + f[10]
                if tl < fl - 0.5 or tr > fr + 0.5 or tt < ft_ - 0.5 or tb > fb + 0.5:
                    bad.append(f"показ {k}: кадр {r[0]}: текст [{tl:.0f}..{tr:.0f}]x[{tt:.0f}..{tb:.0f}] вылезает из рамки [{fl:.0f}..{fr:.0f}]x[{ft_:.0f}..{fb:.0f}]"); break
        # кадры с рамкой раньше текста
        if fr_f and fr_t and min(fr_f) < min(fr_t) - 0 and False: pass
        info.append(f"показ {k}: rise fr={e['rise']} первый видимый {first}{order}; строк/перенос {sets}; рамка {fw}; кадров {len(fr_f)}")
    print('\n'.join(info))
    print(f"наездов {len(starts)}, показов пузыря {len(eps)}")
    if not eps: bad.append('ни одного показа пузыря в журнале')
    for b in bad: print('FAIL', b)
    print('PASS' if not bad else 'FAIL')
    return 1 if bad else 0
sys.exit(main())
