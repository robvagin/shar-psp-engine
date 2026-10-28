#!/usr/bin/env python3
# №998: разбор прогонов эмулятора. check.py NEW_smoke.txt OLD_smoke.txt  — таблица «столб, тип, было/стало», код 0 если гейт зелёный.
# Строки SPHITS идут парами на каждый столб сценария (после удара и в конце): берём вторую в паре; столб упал, если его номер в ids и наклон >= 80°.
import re, sys
sys.path.insert(0, __file__.rsplit('/', 1)[0])
POLES = [(204, 8), (205, 8), (207, 8), (203, 8), (214, 8), (218, 8), (92, 6), (95, 6), (96, 6), (97, 6), (98, 6)]
GRAZE = (204, 205, 207)   # заезд вскользь: прежний код оставляет стоять
def falls(path):
    L = [l for l in open(path, errors='replace') if l.startswith('SPHITS')]
    res = {}
    for k, (i, t) in enumerate(POLES):
        if 2 * k + 1 >= len(L): res[i] = None; continue
        l = L[2 * k + 1]; ids = [int(x) for x in re.search(r'ids=([\d,]*)', l).group(1).split(',') if x]
        deg = int(re.search(r'maxdeg=(\d+)', l).group(1)); res[i] = (i in ids and deg >= 80, deg, int(re.search(r'v=([\d.-]+)', L[2 * k]).group(1).split('.')[0]) if re.search(r'v=([\d.-]+)', L[2 * k]) else 0)
    return res
new, old = falls(sys.argv[1]), falls(sys.argv[2])
ok = True
print('  столб  тип  было (TEST71, flag pole998 0)   стало')
for i, t in POLES:
    n, o = new[i], old[i]
    if n is None or o is None: print('  %4d   %d   нет данных' % (i, t)); ok = False; continue
    print('  %4d   %d   %-30s %s' % (i, t, ('упал %d°' % o[1]) if o[0] else 'стоит (%d°)' % o[1], ('упал %d°' % n[1]) if n[0] else 'СТОИТ (%d°)' % n[1]))
    ok &= n[0]                                   # новый код: падают все
    if i in GRAZE: ok &= not o[0]                # красный контроль: заезд вскользь в красный фонарь типа 8 — старый код не сбивает
    else: ok &= o[0]                             # прямой удар (и баннеры типа 6) падал и раньше — правка его не ломает
print('  зелёный: все 11 упали; красный: заезд вскользь в 204/205/207 старый код не сбил, прямые удары падали и раньше' if ok else '  FAIL: таблица выше')
sys.exit(0 if ok else 1)
