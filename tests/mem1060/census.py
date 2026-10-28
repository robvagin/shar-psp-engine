#!/usr/bin/env python3
"""№1060 перепись: журнал тура (smoke.txt, строки HEAPB t= bytes= blk= free=) -> для каждого вида события худшая куча (свободно, наибольший блок) в окне [t-0.5; t+4] с.
Запуск: census.py smoke.txt  — таблица в stdout. События: загрузка уровня (LEVEL ev=load), комната (ROOM ev=enter), миссия и её персонажи (MSNSTART, MSNNPC load), ролик (MOVIEWALL), будка (PHONE view open), весь тур."""
import re, sys, collections
f = sys.argv[1]
hp = []                                        # (t, free, blk)
ev = collections.defaultdict(list)             # вид -> [t]
lastt = 0.0; lv = []
rx_h = re.compile(r'^HEAPB t=([\d.]+) .*?bytes=(\d+) blk=(\d+)')
rx_t = re.compile(r' t=([\d.]+)')
for ln in open(f, errors='replace'):
    m = rx_h.match(ln)
    if m: hp.append((float(m.group(1)), int(m.group(2)), int(m.group(3)))); lastt = hp[-1][0]; continue
    t = rx_t.search(ln); tt = float(t.group(1)) if t and not ln.startswith(('MENU', 'FEG', 'MOVIEWALL')) else lastt
    if ln.startswith('LEVEL ev=load'):
        ev['загрузка уровня'].append(tt); m2 = re.search(r'heap0=(\d+) heap=(\d+) mx0=(\d+) mx=(\d+)', ln)
        if m2: lv.append(tuple(int(x) for x in m2.groups()))
    elif ln.startswith('ROOM ev=enter'): ev['вход в комнату/дом'].append(tt)
    elif ln.startswith('MSNSTART'): ev['старт миссии'].append(lastt)
    elif ln.startswith('MSNNPC load'): ev['персонаж миссии (модель)'].append(lastt)
    elif ln.startswith('MOVIEWALL'): ev['ролик'].append(float(t.group(1)) if t else lastt)
    elif ln.startswith('PHONE view open'): ev['будка'].append(tt)
    elif ln.startswith('BONUS') and 'src=' not in ln: ev['бонус-игра'].append(tt)
MB = 1048576.0
print('| ситуация | раз | куча мин, МБ | наибольший блок мин, МБ | запас до порога 1 МБ блока |')
print('|---|---|---|---|---|')
def row(name, ts):
    if not ts: print(f'| {name} | 0 | не встречалось в туре | | |'); return
    fr, bk = 1e18, 1e18
    for t in ts:
        for (h, a, b) in hp:
            if t - 0.5 <= h <= t + 4.0: fr = min(fr, a); bk = min(bk, b)
    if fr == 1e18: print(f'| {name} | {len(ts)} | нет отсчётов HEAPB | | |'); return
    print(f'| {name} | {len(ts)} | {fr/MB:.2f} | {bk/MB:.2f} | {(bk-MB)/MB:+.2f} |')
for k in ['загрузка уровня', 'вход в комнату/дом', 'старт миссии', 'персонаж миссии (модель)', 'ролик', 'будка', 'бонус-игра']: row(k, ev[k])
if lv: print(f'\nЗагрузка уровня по строкам LEVEL ({len(lv)} загрузок): куча до {min(a for a,_,_,_ in lv)/MB:.2f} МБ мин, куча после {min(b for _,b,_,_ in lv)/MB:.2f}, наибольший блок до {min(c for _,_,c,_ in lv)/MB:.2f}, после {min(d for _,_,_,d in lv)/MB:.2f}')
if hp: print(f'| весь тур | {len(hp)} отсчётов | {min(a for _,a,_ in hp)/MB:.2f} | {min(b for _,_,b in hp)/MB:.2f} | |')
