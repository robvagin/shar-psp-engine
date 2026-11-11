#!/usr/bin/env python3
"""Гейт шага 11 (FABLE_АУДИТ §3.4 п.2, пересчёт 08.11). check_l4free.py РЕЖИМ smoke.txt, режимы:
  a   сцена l4_free250.txt, улица L4 после загрузки (F_L4FREE_A, дальность по умолчанию): свободно (min по секундам) >= 4,5 МБ И кусок (blk, min) >= 2 МБ.
      Строка LEVEL ev=load n=4 печатается справочно (свободно сразу после загрузки, mx0).
  b   заезд gen_l4gate.py (20 мин езды L4 с заходами в дом, Kwik-E-Mart, будку; окно LG_L1 .. LG_m5): MEMFAILSZ = 0; WATCHDOG mem со step >= 2 = 0; кусок в езде (PH6MEM blk, сцены LG_drive*, room=0) p10 >= 1 МБ.
  c   тот же заезд при принудительных 250 м (gen_l4gate.py 250): MEMFAILSZ = 0; лестница (все строки WATCHDOG mem) не чаще 3 раз.
Выход: 0 зелёно, 1 красно, 2 нет данных. Каждая проверка печатает число и порог."""
import re, sys, statistics as st

def load(p):
    sc = []; ph = []; mf = []; wd = []; lv = []
    for l in open(p, errors='replace'):
        if l.startswith('SCENE '):
            q = l.split(); sc.append((float(q[2]), q[1]))
        elif l.startswith('PH6MEM'):
            m = re.match(r'PH6MEM t=([\d.]+) blk=([\d.]+) free=([\d.]+) room=(\d+).*?drawDist=(\d+)', l)
            if m: ph.append((float(m.group(1)), float(m.group(2)), float(m.group(3)), int(m.group(4)), int(m.group(5))))
        elif l.startswith('MEMFAILSZ'):
            m = re.search(r'bytes=(\d+) ra=(\w+).*? t=([\d.]+)', l)
            if m: mf.append((float(m.group(3)), int(m.group(1)), m.group(2)))
        elif l.startswith('WATCHDOG mem '):
            m = re.search(r'who=(\S+) size=(\d+) step=(\d+).*? t=([\d.]+)', l)
            if m: wd.append((float(m.group(4)), int(m.group(3)), m.group(1)))
        elif l.startswith('LEVEL ev=load n=4'):
            m = re.search(r'heap=(\d+) mx0=(\d+)', l)
            if m: lv.append((int(m.group(1)) / 1048576, int(m.group(2)) / 1048576))
    return sc, ph, mf, wd, lv

def window(sc, first, last):
    t0 = next((t for t, n in sc if n == first), None); t1 = next((t for t, n in sc if n == last), None)
    return t0, t1

def pct(v, p):
    v = sorted(v); return v[min(len(v) - 1, int(p * len(v)))] if v else float('nan')

def main():
    mode, path = sys.argv[1], sys.argv[2]
    sc, ph, mf, wd, lv = load(path); res = []
    if mode == 'a':
        if lv: print('справочно LEVEL load L4: свободно %.2f МБ, mx0 %.2f МБ' % lv[-1])
        idx = [i for i, (t, n) in enumerate(sc) if n == 'F_L4FREE_A']
        if not idx: print('НЕТ ДАННЫХ: сцены F_L4FREE_A нет'); return 2
        t0 = sc[idx[0]][0]; t1 = sc[idx[0] + 1][0] if idx[0] + 1 < len(sc) else 1e9
        r = [x for x in ph if t0 + 8 <= x[0] < t1 and x[3] == 0]
        if not r: print('НЕТ ДАННЫХ: PH6MEM на улице'); return 2
        fr = min(x[2] for x in r); bl = min(x[1] for x in r)
        print('улица L4, дальность %s: свободно min %.2f МБ (порог >= 4,5), кусок min %.2f МБ (порог >= 2,0), секунд %d' % (sorted(set(x[4] for x in r)), fr, bl, len(r)))
        res += [fr >= 4.5, bl >= 2.0]
    else:
        t0, t1 = window(sc, 'LG_L1', 'LG_m5')
        if t0 is None or t1 is None: print('НЕТ ДАННЫХ: окно LG_L1 .. LG_m5 не найдено (заезд не дошёл до конца езды)'); return 2
        m = [x for x in mf if t0 <= x[0] < t1]; w = [x for x in wd if t0 <= x[0] < t1]
        print('окно езды %.0f .. %.0f с (%.1f мин)' % (t0, t1, (t1 - t0) / 60))
        print('MEMFAILSZ в окне: %d (порог 0)%s' % (len(m), ''.join('\n   bytes=%d ra=%s t=%.1f' % (b, ra, t) for t, b, ra in m[:12])))
        res.append(len(m) == 0)
        if mode == 'b':
            hi = [x for x in w if x[1] >= 2]
            print('WATCHDOG mem step >= 2: %d (порог 0); всего ступеней лестницы %d' % (len(hi), len(w)))
            drv = [i for i, (t, n) in enumerate(sc) if n.startswith('LG_drive')]
            blk = []
            for i in drv:
                a = sc[i][0]; b = sc[i + 1][0] if i + 1 < len(sc) else 1e9
                blk += [x[1] for x in ph if a + 3 <= x[0] < b and x[3] == 0]
            if not blk: print('НЕТ ДАННЫХ: PH6MEM в езде'); return 2
            p10 = pct(blk, .10)
            print('кусок в езде (PH6MEM blk, LG_drive*, вне комнат): p10 %.2f МБ (порог >= 1,0), min %.2f, медиана %.2f, секунд %d; drawDist %s' % (p10, min(blk), st.median(blk), len(blk), sorted(set(x[4] for x in ph if t0 <= x[0] < t1))))
            res += [len(hi) == 0, p10 >= 1.0]
        elif mode == 'c':
            print('лестница (все строки WATCHDOG mem): %d (порог <= 3)%s' % (len(w), ''.join('\n   who=%s step=%d t=%.1f' % (wh, s, t) for t, s, wh in w[:8])))
            res.append(len(w) <= 3)
            print('drawDist в окне: %s' % sorted(set(x[4] for x in ph if t0 <= x[0] < t1)))
        else: print('режим a|b|c'); return 2
    print('ЗЕЛЁНОЕ' if all(res) else 'КРАСНОЕ'); return 0 if all(res) else 1

sys.exit(main())
