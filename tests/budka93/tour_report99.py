#!/usr/bin/env python3
"""Будка-93: итог тура по smoke.txt (тур выпуска, ballast 5) по уровням. tour_report99.py smoke.txt
Сегменты — по строкам «LEVEL ev=load n=N» (повторные заходы на тот же уровень — отдельные сегменты). По сегменту: fps мин/средний (строки таблицы, col 3), readerr/allocfail (прирост), BB bad (макс), LVA miss, WATCHDOG по видам,
SKYDRAW с notres>0, owner load, MEMFAILSZ, STALL/GE NESTED."""
import re, sys, collections
L = open(sys.argv[1], errors='replace').read().split('\n')
segs = []; cur = None
def new(n, i): 
    global cur; cur = dict(n=n, i=i, fps=[], rd=[], af=[], bad=0, lva=None, wd=collections.Counter(), sky=0, skyl=None, own=0, mf=0, st=0, t0=None, t1=None); segs.append(cur)
new(0, 0)
row = re.compile(r'^(\d+\.\d+) (\S+) (\d+\.\d+) (\d+\.\d+) (\d+\.\d+) (\d+\.\d+) (\d+) (\d+) (\d+\.\d+) (\S+) (\d+) (\d+) (\d+) (\d+) ')
for i, l in enumerate(L):
    m = re.match(r'LEVEL ev=load n=(\d+) ', l)
    if m: new(int(m.group(1)), i); continue
    c = cur
    m = row.match(l)
    if m:
        t = float(m.group(1)); c['fps'].append(float(m.group(3))); c['rd'].append(int(m.group(13))); c['af'].append(int(m.group(14))); c['t0'] = t if c['t0'] is None else c['t0']; c['t1'] = t; continue
    if l.startswith('BB '):
        m = re.search(r'\bbad (\d+)', l)
        if m: c['bad'] = max(c['bad'], int(m.group(1)))
    elif l.startswith('LVA level='):
        m = re.search(r'miss=(\d+)', l); c['lva'] = int(m.group(1)) if m else None
    elif l.startswith('WATCHDOG '):
        w = l.split()
        if w[1] == 'owner': c['own'] += 1
        k = w[1] + (':' + w[2].split('=')[1] if w[1] in ('mem',) and len(w) > 2 and w[2].startswith('who=') else '')
        if w[1] in ('spawn',) and 'actor=hero' in l: k = 'spawn:hero'
        c['wd'][k] += 1
    elif l.startswith('SKYDRAW '):
        m = re.search(r'notres=(\d+)', l)
        if m and int(m.group(1)) > 0: c['sky'] += 1; c['skyl'] = int(m.group(1))
    elif l.startswith('MEMFAILSZ'): c['mf'] += 1
    elif re.search(r'STALL|GE NESTED', l): c['st'] += 1
print('сег  ур  с(игровое)  fps мин/сред   readerr  allocfail  bad  LVAmiss  sky>0  owner  MEMFAILSZ  STALL   WATCHDOG по видам')
for k, c in enumerate(segs):
    if not c['fps'] and not c['wd']: continue
    f = c['fps']; rd = (max(c['rd']) - min(c['rd'])) if c['rd'] else 0; af = (max(c['af']) - min(c['af'])) if c['af'] else 0
    dur = (c['t1'] - c['t0']) if c['t0'] is not None else 0
    wd = ' '.join('%s=%d' % (a, b) for a, b in sorted(c['wd'].items(), key=lambda x: -x[1])[:7])
    print('%3d  L%-2d %8.0f   %5.1f/%5.1f   %6d  %8d  %4d  %6s  %5d  %5d  %9d  %5d   %s' % (k, c['n'], dur, min(f) if f else 0, sum(f) / len(f) if f else 0, rd, af, c['bad'], c['lva'], c['sky'], c['own'], c['mf'], c['st'], wd))
tot = collections.Counter()
for c in segs: tot.update(c['wd'])
print('ИТОГО WATCHDOG по видам:', ' '.join('%s=%d' % x for x in sorted(tot.items(), key=lambda x: -x[1])))
print('END в журнале:', 'да' if any(l == 'END' for l in L) else 'нет (тур идёт или оборван)')
