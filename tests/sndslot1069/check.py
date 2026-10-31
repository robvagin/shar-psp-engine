#!/usr/bin/env python3
"""№1069: check.py MODE smoke.txt — по каждой загрузке уровня после флагов сценария (LEVELUP → LEVEL ev=load): есть ли звук уровня
(snd=1/N в строке LEVEL ev=load), был ли повтор №1065 (SNDRETRY1065) и отказ памяти (MEMFAILSZ). Печатает свободную кучу после загрузки
(stderr HEAP Ln КБ — цена постоянного места, accept.sh сравнивает cleannew с cleanold). Код 0 — ожидаемое:
  new / cleannew / cleanold  звук на каждом уровне, повторов и отказов нет
  red                        прежний код при отказе крупного куска: хотя бы один уровень без звука (snd=0) — красный краснеет"""
import re, sys
mode, f = sys.argv[1], sys.argv[2]
L = open(f, errors='replace').read().splitlines()
ups = [i for i, l in enumerate(L) if l.startswith('LEVELUP')] + [len(L)]
print(f"смен уровня по сценарию: {len(ups) - 1}")
bad = red_hit = seen = 0
for a, b in zip(ups[:-1], ups[1:]):
    seg = L[a:b]; ld = [x for x in seg if x.startswith('LEVEL ev=load')]
    if not ld: print("  смена без загрузки уровня"); bad += 1; continue
    n = int(re.search(r' n=(\d+)', ld[0]).group(1)); seen += 1
    s = re.search(r'snd=(\d+)/(\d+)', ld[0]); ok = s and s.group(1) == '1' and s.group(2) == str(n)
    heap = int(re.search(r' heap=(\d+)', ld[0]).group(1))
    rt = [x for x in seg if x.startswith('SNDRETRY1065')]; mf = [x for x in seg if x.startswith('MEMFAILSZ')]
    e = []
    if mode == 'red':
        if not ok: red_hit += 1
    else:
        if not ok: e.append('нет звука уровня')
        if rt: e.append('был повтор №1065')
        if mf: e.append(f'отказ памяти {len(mf)}')
    print(f"  L{n}: звук {'есть' if ok else 'НЕТ'} ({s.group(0) if s else 'нет snd='}), повторов {len(rt)}, отказов {len(mf)}, куча после загрузки {heap // 1024} КБ"
          + (f"  НАРУШЕНИЕ: {', '.join(e)}" if e else ''))
    print(f"HEAP L{n} {heap // 1024}", file=sys.stderr)
    bad += len(e)
if mode == 'red':
    print('OK красный краснеет' if red_hit else 'ALARM красный не покраснел'); sys.exit(0 if red_hit else 1)
print("итого нарушений:", bad); print('OK' if bad == 0 and seen else 'ALARM'); sys.exit(0 if bad == 0 and seen else 1)
