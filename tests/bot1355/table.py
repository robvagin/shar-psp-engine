#!/usr/bin/env python3
"""Таблица прохода бота (1355): по smoke.txt каждого прогона OUT/out_L<N>_<миссия>/ — на каждый этап: цель, есть ли (npc/car/видна), метка верна (markerErr <= 3 м и метка включена), засчитан (MSN done / MSN pass).
  table.py OUTDIR"""
import re, sys
from pathlib import Path
rows = []
for d in sorted(Path(sys.argv[1]).glob('out_*')):
    f = d / 'smoke.txt'
    if not f.exists(): continue
    tag = d.name[4:]; stages = {}; order = []; done = set(); passed = set(); mission = ''; complete = 0; fails = 0
    for l in f.read_text(errors='replace').splitlines():
        m = re.match(r'MSN stage (\S+)\.(\d+)\.(\d+) obj=(\S+) text="([^"]*)"', l)
        if m and m.group(1) and not mission.startswith('?'): mission = mission or m.group(1)
        if m:
            k = (m.group(1), int(m.group(2)), int(m.group(3)))
            if k not in stages: stages[k] = dict(obj=m.group(4), text=m.group(5), audits=[]); order.append(k)
        m = re.match(r'AUDIT1355 mission=(\S+) stage=(\d+)\.(\d+) obj=(\S+) name=(\S+) target=(\d)\(([-\d]+),([-\d]+)\) marker=(\d)\(([-\d]+),([-\d]+)\) markerErr=([-\d.]+) playerDist=([-\d.]+) car=(-?\d) npc=(-?\d) visible=(-?\d) status=(\d)', l)
        if m:
            k = (m.group(1), int(m.group(2)), int(m.group(3))); stages.setdefault(k, dict(obj=m.group(4), text='', audits=[]))
            if k not in order: order.append(k)
            stages[k]['audits'].append(dict(name=m.group(5), has=int(m.group(6)), marker=int(m.group(9)), err=float(m.group(12)), car=int(m.group(14)), npc=int(m.group(15)), vis=int(m.group(16)), status=int(m.group(17)), pd=float(m.group(13))))
        m = re.match(r'MSN done (\d+)\.(\d+)', l)
        if m: done.add((int(m.group(1)), int(m.group(2))))
        m = re.match(r'MSN pass (\S+) (\d+)\.(\d+)', l)
        if m: passed.add((m.group(1), int(m.group(2)), int(m.group(3))))
        if l.startswith('MSN complete') or l.startswith('MSN mission-complete'): complete = 1
        if l.startswith('MSN failed'): fails += 1
    viol = []
    print(f'## {tag}: этапов {len(order)}, засчитано {len([k for k in order if (k[1], k[2]) in done])}, самим помощником (pass) {len(passed)}, провалов {fails}, миссия завершена {complete}')
    print('| миссия | этап | цель | есть ли | метка верна | засчитан |'); print('|---|---|---|---|---|---|')
    for k in order:
        s = stages[k]; a = [x for x in s['audits'] if x['status'] == 1]
        running = bool(a)
        if not a: a = s['audits']
        name = a[0]['name'] if a else '-'
        thing = []; 
        if a:
            has = any(x['has'] for x in a); thing.append('цель есть' if has else 'цели нет')
            if any(x['car'] == 1 for x in a): thing.append('машина')
            elif any(x['car'] == 0 for x in a): thing.append('машины НЕТ')
            if any(x['npc'] == 1 for x in a): thing.append('персонаж')
            elif any(x['npc'] == 0 for x in a): thing.append('персонажа НЕТ')
            vis = any(x['vis'] == 1 for x in a); thing.append('видна' if vis else 'не видна')
            ok = [x for x in a if x['has'] and x['marker'] and 0 <= x['err'] <= 3.0]; bad = [x for x in a if x['has'] and x['marker'] and x['err'] > 3.0]; none = [x for x in a if x['has'] and not x['marker']]
            mk = 'верна' if ok and not bad else ('СМЕЩЕНА %.0f м' % max(x['err'] for x in bad) if bad else (('метки нет' if running else 'этап уже засчитан') if none else '—'))
            if running and bad: viol.append((k, max(x['err'] for x in bad)))
            if running and none and not ok: viol.append((k, -1))
        else: thing = ['нет строк']; mk = '—'
        cnt = 'да' if (k[1], k[2]) in done else 'нет'
        if k in passed: cnt += ' (помощник)'
        print(f'| {k[0]} | {k[1]}.{k[2]} {s["obj"]} | {name} | {", ".join(thing)} | {mk} | {cnt} |')
    print('нарушений метки на идущих этапах: %d%s' % (len(viol), ' ' + str(viol) if viol else ''))
    print()
