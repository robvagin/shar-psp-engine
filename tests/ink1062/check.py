#!/usr/bin/env python3
"""№1062 accept60, гейт кадрами: контур только у героя, NPC, машин, будки и предметов; дома, дороги, край асфальта, линии мира, фон — без контура.
  python3 check.py НОВЫЙ_КАТАЛОГ СТАРЫЙ_КАТАЛОГ [--expect fail]     (каталоги out_*/ с ik*.png и smoke.txt; прогоны с flag camdet 1 идут кадр в кадр, разница кадров = убранные линии)
Проверки: (1) кадры «машина» L1, L3, L4, L7: убрано не меньше 150 точек (линии окружения: край асфальта, дома) и не больше 2 % кадра (больше ничего не тронуто);
(2) вокруг героя/машины (герой: x 190..300, y 64..180; машина: x 180..300, y 150..262) разница ≤ 0,5 %: контур героя и машины на месте; кадры «герой» L1, L4, L7 без разницы (≤ 0,5 % всего кадра: рядом линий окружения нет);
(3) журнал: у нового нет TOONLINES/ROADLINES (линии не читались), у старого есть; суммы TOON skin= и car= (контур героев, NPC и машин) ≥ 95 % старых; число обведённых пропов (PTINST, будка среди них) то же.
Красный: тот же скрипт для «нового» прогона с flag ink1062old 1 против старого — разницы нет, проверка 1 краснеет"""
import re, sys
from PIL import Image
import numpy as np
a = [x for x in sys.argv[1:] if not x.startswith('--')]; exp_fail = '--expect' in sys.argv and sys.argv[sys.argv.index('--expect') + 1] == 'fail'
N, O = a[0], a[1]; ok = True
def chk(name, cond, msg):
    global ok; print(('  ok  ' if cond else '  FAIL ') + name + ': ' + msg); ok = ok and cond
def diff(nm):
    A = np.asarray(Image.open(f'{O}/{nm}.png').convert('RGB')).astype(int); B = np.asarray(Image.open(f'{N}/{nm}.png').convert('RGB')).astype(int)
    d = np.abs(A - B).max(axis=2) > 40; d[:64] = False; return d
for lv in (1, 3, 4, 7):
    d = diff(f'ik{lv}_car'); n = int(d.sum()); cen = d[150:262, 180:300].mean() * 100
    chk(f'L{lv} машина', 150 <= n <= 0.02 * d.size, f'убрано {n} точек линий окружения ({n / d.size * 100:.2f} % кадра; нужно 150 … 2 %)')
    chk(f'L{lv} машина центр', cen <= 0.5, f'вокруг героя/машины изменилось {cen:.2f} % (контур на месте, ≤ 0.5 %)')
for lv in (1, 3, 4, 7):
    d = diff(f'ik{lv}_hero'); cen = d[64:180, 190:300].mean() * 100
    chk(f'L{lv} герой', cen <= 0.5, f'вокруг героя изменилось {cen:.2f} % (контур героя на месте, ≤ 0.5 %)')
d = diff('ik1_booth'); cen = d[64:180, 190:300].mean() * 100
chk('L1 будка', cen <= 0.5, f'вокруг героя и будки изменилось {cen:.2f} % (контур будки и героя на месте, ≤ 0.5 %); убрана кромка асфальта {int(d.sum())} точек')
def log(p):
    s = open(p, errors='replace').read(); tl = len(re.findall(r'^(TOONLINES level|ROADLINES level)', s, re.M)); sk = sum(int(x) for x in re.findall(r'^TOON mode=\d skin=(\d+)', s, re.M)); cr = sum(int(x) for x in re.findall(r'^TOON mode=\d skin=\d+ car=(\d+)', s, re.M)); pt = len(re.findall(r'^PTINST ', s, re.M)); wd = sum(int(x) for x in re.findall(r'^INK1062 world=(\d+)', s, re.M)); hl = sum(int(x) for x in re.findall(r'^INK1062 world=\d+ hull=(\d+)', s, re.M))
    return tl, sk, cr, pt, wd, hl
tn, skn, crn, ptn, wdn, hln = log(f'{N}/smoke.txt'); to, sko, cro, pto, wdo, hlo = log(f'{O}/smoke.txt')
chk('линии мира', tn == 0 and to > 0, f'у нового строк загрузки TOONLINES/ROADLINES {tn} (нужно 0), у старого {to}')
chk('герои и NPC', skn >= 0.95 * sko > 0, f'TOON skin: новый {skn}, старый {sko}')
chk('мелкие куски города', wdn == 0 and wdo > 0, f'оболочек мелких кусков города (фон): новый {wdn} (нужно 0), старый {wdo}')
chk('машины', hln - wdn >= 0.95 * (hlo - wdo) > 0, f'оболочек машин (все вызовы минус куски города): новый {hln - wdn}, старый {hlo - wdo}')
chk('предметы и будка', ptn == pto > 0, f'обведённых пропов и стейтпропов (PTINST): новый {ptn}, старый {pto}')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok != exp_fail else 1)
