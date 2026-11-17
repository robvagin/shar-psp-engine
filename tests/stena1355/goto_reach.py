#!/usr/bin/env python3
"""1355 · все goto/gooutside-этапы уровней 1–7: значок (позиция локатора) и объём триггера.
Для каждого: внутри ли значок в объёме; если нет, проходит ли дорога от старта этапа до значка через объём (A* по графу дорог build/sandbox/roads-lN.bin,
как в tools/twins/route.py). Объём на дороге к значку = этап засчитывается по пути к значку (так задумано в данных диска: ворота на маршруте).
Запуск из ОСНОВНОЙ папки проекта (там donut и build/sandbox):  cd ~/Downloads/shar-psp && python3 <WT>/tests/stena1355/goto_reach.py [--red] [--md OUT.md]
--red: красный контроль — объёмы пяти целей сдвинуты на 600 м, гейт обязан упасть. Выход 1 — есть цель, до объёма которой по дороге не доехать
(из списка известных не доказанных — KNOWN)."""
import sys, math, re, json, argparse
from pathlib import Path
WT = Path(__file__).resolve().parents[2]
sys.path[:0] = ['tools/twins', 'tools/missions', 'tools', 'hit-and-run-web/tools']   # инструменты основной папки (cwd): там donut с таблицами команд
import route as R
import compile as C
SCR = Path('hit-and-run-web/source/game/scripts/missions')
ROADS = '/Users/USER/Downloads/shar-psp/build/sandbox/roads-l%d.bin'
START_OVERRIDE = {(7, 'm7', 3): 'm7_barrel'}   # этап идёт после гонки и сбора бочки: старт — у бочки, не у машины миссии
KNOWN = {(4, 'm3', 1), (4, 'm5', 7), (4, 'm1', 0)}   # не доказано (предыдущая точка этапа неизвестна; метка у старта) — отдельный разбор
RED = {(5, 'm2', 0), (5, 'm6', 0), (7, 'm6', 0), (7, 'm7', 0), (7, 'm7', 3)}

def inside(t, p, ytol=0.0):
    kind, center, half, axes = t
    d = [p[i] - center[i] for i in range(3)]
    if kind == 0: return math.sqrt(sum(x * x for x in d)) <= half[0]
    return all(abs(sum(d[i] * axes[k][i] for i in range(3))) <= half[k] + (ytol if k == 1 else 0) for k in range(3))

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--red', action='store_true'); ap.add_argument('--md'); a = ap.parse_args()
    out = []; bad = []
    graphs = {}
    for lv in range(1, 8):
        c = C.Compiler(lv); c.compile(); P = c.pack; S = lambda i: P.strings[i]
        byname = {}
        for L in P.locs: byname.setdefault(S(L['name']).lower(), L)
        for m in P.missions:
            mid = S(m[0]); f = SCR / f'level0{lv}' / f'{mid}i.mfk'; start = None
            if f.exists():
                mm = re.search(r'SetMissionResetPlayer(?:InCar|OutCar)?\(\s*"([^"]+)"', f.read_text(errors='replace'))
                if mm and mm.group(1).lower() in byname: start = byname[mm.group(1).lower()]['pos']
            for ph_i in (m[5], m[6]):
                if ph_i == 0xFFFF: continue
                ph = P.phases[ph_i]
                for si, st in enumerate(P.stages[ph[6]:ph[6] + ph[7]]):
                    obj = C.CMD.OBJECTIVES[st[9] - 1] if st[9] else '?'
                    if obj not in ('goto', 'gooutside'): continue
                    for k in range(st[0], st[0] + st[1]):
                        op, n, ctx, a0, line, fl = P.cmds[k]
                        if C.CMD.SPECS[op - 1].name != 'SetDestination': continue
                        t, loc, v = P.args[a0]
                        if not loc: continue
                        L = P.locs[loc - 1]; pos = L['pos']; trs = list(P.trigs[L['first']:L['first'] + L['ntrig']])
                        key = (lv, mid, si) if ph_i == m[6] else (lv, mid, -1 - si)
                        if a.red and key in RED:
                            trs = [(kd, (ce[0] + 600, ce[1], ce[2]), ha, ax) for kd, ce, ha, ax in trs]
                        row = dict(lv=lv, m=mid, ph=1 if ph_i == m[6] else 0, st=si, loc=S(L['name']), pos=[round(x) for x in pos])
                        if not trs: row['v'] = 'радиус (без объёма)'
                        elif any(inside(t_, pos, 20) for t_ in trs): row['v'] = 'значок внутри объёма'
                        else:
                            s = start
                            ov = START_OVERRIDE.get((lv, mid, si))
                            if ov and ov.lower() in byname: s = byname[ov.lower()]['pos']
                            res = None
                            if s:
                                for link in (12.0, 20.0, 30.0):
                                    nodes, adj = graphs.setdefault((lv, link), R.graph(path=ROADS % lv, link=link))
                                    p = R.astar(nodes, adj, R.nearest(nodes, s[0], s[2]), R.nearest(nodes, pos[0], pos[2]))
                                    if not p: continue
                                    pts = [nodes[i] for i in p]
                                    ln = sum(math.dist((x[0], x[2]), (y[0], y[2])) for x, y in zip(pts, pts[1:]))
                                    if ln <= 2.5 * math.dist((s[0], s[2]), (pos[0], pos[2])) or link == 30.0:
                                        dense = R.resample(pts, 2.0)[0]
                                        hit = [i for i, t_ in enumerate(trs) if any(inside(t_, q, 20) for q in dense)]
                                        res = (hit, round(ln)); break
                            row['v'] = f'по дороге через объём {res[0]} (путь {res[1]} м)' if res and res[0] else 'ОБЪЁМ НЕ НА ДОРОГЕ'
                            row['d'] = round(min(math.dist(pos, t_[1]) for t_ in trs))
                            if not (res and res[0]) and (lv, mid, si) not in KNOWN: bad.append(row)
                        out.append(row)
    n_in = sum(r['v'] == 'значок внутри объёма' for r in out); n_rd = sum(r['v'].startswith('по дороге') for r in out)
    print(f'целей {len(out)}: значок внутри объёма {n_in}, объём на дороге к значку {n_rd}, без объёма {sum(r["v"].startswith("радиус") for r in out)}, не доказано {len([r for r in out if r["v"].startswith("ОБЪЁМ")])}')
    for r in out:
        if r['v'] not in ('значок внутри объёма',) and not r['v'].startswith('радиус'): print(f"L{r['lv']} {r['m']} ph{r['ph']} st{r['st']} {r['loc']}: {r['v']}" + (f" (значок—центр {r['d']} м)" if 'd' in r else ''))
    if a.md:
        Path(a.md).write_text('| L | миссия | этап | локатор | значок | вердикт |\n|---|---|---|---|---|---|\n' + '\n'.join(f"| {r['lv']} | {r['m']} | {r['ph']}.{r['st']} | {r['loc']} | {r['pos'][0]},{r['pos'][2]} | {r['v']} |" for r in out if r['v'] != 'значок внутри объёма') + '\n')
    print('ГЕЙТ goto_reach:', 'FAIL' if bad else 'PASS', [f"L{r['lv']} {r['m']} {r['ph']}.{r['st']}" for r in bad])
    sys.exit(1 if bad else 0)
main()
