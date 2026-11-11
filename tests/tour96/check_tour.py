#!/usr/bin/env python3
"""Гейт по разбору Данных тура 95c (РАЗБОР_ТУР_TEST95c_ПАМЯТЬ.md): tests/tour96/check_tour.py smoke.txt [smoke_сравнение.txt]
Красное, если хоть где-то: (1) LVA miss > 0 (строка "LVA miss" или miss>0 в строке "LVA level="); (2) клетки bad (в радиусе, не загружены; поле bad строки BB)
больше 0 позже 3 с после начала сцены; (3) city fail растёт при прицепленном городе (ZONE3: city free=a/b, b>0 = город прицеплен; рост fail= между соседними строками).
Отдельно сцены уровней L5, L6, L7 (уровень сцены = самый частый L<N> в строках BB сцены). Второй файл (тур 95c) разбирается тем же кодом для сравнения. Выход 0 = зелёно, 1 = красно."""
import re, sys, collections

def parse(path):
    scenes = []  # (name, t0)
    lva_miss = []; lva_lines = []; bb = []; z3 = []
    for ln in open(path, errors='replace'):
        if ln.startswith('SCENE '):
            p = ln.split()
            try: scenes.append((p[1], float(p[2])))
            except (IndexError, ValueError): pass
        elif ln.startswith('LVA miss'):
            m = re.search(r'bytes=(\d+) retry=(\d+) t=([\d.]+)', ln)
            lva_miss.append((float(m.group(3)) if m else -1, int(m.group(1)) if m else 0, ln.strip()))
        elif ln.startswith('LVA level='):
            m = re.search(r'level=(\d+).*? miss=(\d+)', ln)
            if m: lva_lines.append((int(m.group(1)), int(m.group(2))))
        elif ln.startswith('BB '):
            m = re.match(r'BB\s+(\d+)\s+\S+\s+L(\d)\s.*?cells\s+(\d+)\s+bad\s+(\d+)', ln)
            if m: bb.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))))
        elif ln.startswith('ZONE3 t='):
            m = re.match(r'ZONE3 t=([\d.]+).*? city free=([\d.]+)/([\d.]+) live=(\d+) used_kb=(\d+) peak_kb=(\d+) fail=(\d+)', ln)
            if m: z3.append((float(m.group(1)), float(m.group(3)), int(m.group(7))))
    return scenes, lva_miss, lva_lines, bb, z3

def scene_at(scenes, t):
    lo, hi = 0, len(scenes) - 1; r = -1
    while lo <= hi:
        mid = (lo + hi) // 2
        if scenes[mid][1] <= t: r = mid; lo = mid + 1
        else: hi = mid - 1
    return r

def analyse(path):
    scenes, lva_miss, lva_lines, bb, z3 = parse(path)
    n = len(scenes); S = [dict(name=scenes[i][0], t0=scenes[i][1], bad=[], lv=collections.Counter(), cityfail=0, cityon=False) for i in range(n)]
    for t, lv, cells, bad in bb:
        i = scene_at(scenes, t)
        if i < 0: continue
        if t >= scenes[i][1] + 3: S[i]['bad'].append(bad); S[i]['lv'][lv] += 1
    prev = None
    for t, cityb, fail in z3:
        i = scene_at(scenes, t)
        if prev is not None and i >= 0:
            if cityb > 0 or prev[1] > 0:   # город прицеплен на этой или прошлой строке
                d = fail - prev[2]
                if d > 0: S[i]['cityfail'] += d; S[i]['cityon'] = True
        prev = (t, cityb, fail)
    for s in S:
        s['level'] = s['lv'].most_common(1)[0][0] if s['lv'] else 0
        b = s['bad']; s['badmean'] = sum(b) / len(b) if b else 0.0; s['badmax'] = max(b) if b else 0; s['badn'] = sum(1 for x in b if x > 0)
    return S, lva_miss, lva_lines

def report(path, tag):
    S, lm, ll = analyse(path)
    red = []
    lvaline = [x for x in ll if x[1] > 0]
    if lm or lvaline: red.append(f'LVA miss: {len(lm)} строк "LVA miss", {len(lvaline)} строк LVA level= с miss>0')
    badsc = [s for s in S if s['badn'] > 0]
    if badsc: red.append(f'клетки bad > 0 после 3 с: {len(badsc)} сцен из {len(S)}')
    cf = [s for s in S if s['cityfail'] > 0]
    if cf: red.append(f'city fail растёт при прицепленном городе: {sum(s["cityfail"] for s in cf)} в {len(cf)} сценах')
    print(f'== {tag}: {path}\nсцен {len(S)}; LVA miss {len(lm)}; сцен с bad>0 {len(badsc)}; city fail всего {sum(s["cityfail"] for s in S)}')
    for lv in (5, 6, 7):
        L = [s for s in S if s['level'] == lv]
        print(f'  L{lv}: сцен {len(L)}, с bad>0 {sum(1 for s in L if s["badn"])}, bad среднее по сценам {sum(s["badmean"] for s in L)/max(1,len(L)):.1f}, максимум {max([s["badmax"] for s in L] or [0])}, city fail {sum(s["cityfail"] for s in L)}')
    for s in sorted(badsc, key=lambda s: -s['badmean'])[:25]:
        print(f'    bad {s["name"]} L{s["level"]}: среднее {s["badmean"]:.1f} максимум {s["badmax"]} (строк с bad>0: {s["badn"]})')
    for m in lm[:20]: print('    ' + m[2])
    for s in sorted(cf, key=lambda s: -s['cityfail'])[:15]: print(f'    cityfail {s["name"]} L{s["level"]}: +{s["cityfail"]}')
    return red, S

if __name__ == '__main__':
    red, S = report(sys.argv[1], 'тур')
    if len(sys.argv) > 2: report(sys.argv[2], 'сравнение')
    print('КРАСНОЕ: ' + '; '.join(red) if red else 'ЗЕЛЁНОЕ'); sys.exit(1 if red else 0)
