#!/usr/bin/env python3
"""№1070–1076 приёмка по журналу сценария tests/flow1070/r2.txt (L7 -> L6 m4 -> две смены миссии на L6 -> L5 -> L6 -> L7).
  check.py new|old smoke.txt — new: всё правило выполнено; old (красный, сборка -DMS_FLOW1070_OLD=1 -DCAR_NEAR1073_OLD=1 -DNPC_WAIT1076_OLD=1):
  ни одно не выполнено. Выход 0 — режим подтверждён.
  1070/1071: после каждой смены миссии на том же уровне (MSNSWITCH why=pause) — MSFLOW1070 music и поток level_intro
  1073: L6 m4 (MSNSWITCH to=m4) — CARNEAR1073 keep (машина у будки, не на смотровой)
  1076: после экрана загрузки (LOADSHOW .. следующий SHOT) ни одного чтения модели персонажа/машины (BB op npc+ / car+)"""
import sys, re
mode, path = sys.argv[1], sys.argv[2]
L = open(path, encoding='utf-8', errors='replace').read().splitlines()
sw = [i for i, l in enumerate(L) if l.startswith('MSNSWITCH why=pause')]
mus = 0
for i in sw:
    nxt = next((j for j in range(i + 1, len(L)) if L[j].startswith('MSNSWITCH')), len(L))
    seg = L[i:nxt]
    if any(l.startswith('MSFLOW1070 music') for l in seg) and any(l.startswith('STREAM s=music track=level_intro') for l in seg): mus += 1
car = sum(1 for l in L if l.startswith('CARNEAR1073 keep') and 'dy=-3.9' in l)
late, on = [], False
for l in L:
    if l.startswith('LOADSHOW'): on = True
    elif l.startswith('SHOT'): on = False
    elif on and l.startswith('BB'):
        m = re.search(r' op +(npc\+|car\+) +(\S+)', l)
        if m: late.append(m.group(1) + m.group(2))
end = any(l == 'END' for l in L)
print(f'  смен миссии на уровне: {len(sw)}, со сбросом музыки и level_intro: {mus}')
print(f'  L6 m4 машина у будки (CARNEAR1073 keep): {car}')
print(f'  дочитано после экрана загрузки: {len(late)} {sorted(set(late))}')
print(f'  END: {end}')
if mode == 'new': ok = end and len(sw) >= 2 and mus == len(sw) and car >= 1 and not late
else: ok = end and len(sw) >= 2 and mus == 0 and car == 0 and len(late) > 0
print('OK' if ok else 'НЕТ'); sys.exit(0 if ok else 1)
