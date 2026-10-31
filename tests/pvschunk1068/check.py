#!/usr/bin/env python3
"""№1068: check.py MODE smoke.txt НАБОР — по каждой загрузке уровня после флагов сценария (LEVELUP → LEVEL ev=load): есть ли PVS1 (PVSLOAD ok=), каким путём
(запас участка / кусками / одним блоком кучи) и совпал ли пул в памяти с level.shr (FNV-1a, PVSCHUNK sum=). Печатает ktris/draws (медиана RFAST до следующей смены) —
для сравнения кадров между режимами (accept.sh). Код 0 — ожидаемое:
  new / cleannew  L1, L4: PVS кусками (chunks ≥ 2), сумма = файлу; прочие: PVS в запасе участка (arena=1)
  red             прежний код при отказе крупного куска: хотя бы на одной из L1/L4 уровень без PVS (ok=0) — красный краснеет
  cleanold        прежний код на целой куче: PVS у всех, кусков нет"""
import re, struct, sys, statistics
mode, f, SET = sys.argv[1], sys.argv[2], sys.argv[3]
def file_sum(n):
    p = f"{SET}/level.shr" if n == 1 else f"{SET}/l{n}/level.shr"
    with open(p, 'rb') as fd:
        h = fd.read(96); off = struct.unpack_from('<I', h, 88)[0]; fd.seek(off); ph = fd.read(32)
        W, H, npool = struct.unpack_from('<III', ph, 16); fd.seek(off + 32 + W * H * 8); pool = struct.unpack(f'<{npool}H', fd.read(npool * 2))
    s = 2166136261
    for v in pool: s = ((s ^ v) * 16777619) & 0xFFFFFFFF
    return s
L = open(f, errors='replace').read().splitlines()
ups = [i for i, l in enumerate(L) if l.startswith('LEVELUP')] + [len(L)]
print(f"смен уровня по сценарию: {len(ups) - 1}")
bad = 0; red_hit = 0; seen = 0
for a, b in zip(ups[:-1], ups[1:]):
    seg = L[a:b]; ld = [x for x in seg if x.startswith('LEVEL ev=load')]
    if not ld: print("  смена без загрузки уровня"); bad += 1; continue
    n = int(re.search(r'n=(\d+)', ld[0]).group(1)); seen += 1
    pl = [x for x in seg if x.startswith(f'PVSLOAD level={n} ')]; pc = [x for x in seg if x.startswith(f'PVSCHUNK level={n} ')]
    q = dict(kv.split('=') for kv in pl[-1].split()[1:]) if pl else {}
    ok, ch, ar = q.get('ok') == '1', int(q.get('chunks', 0)), q.get('arena') == '1'
    sm = None
    if pc: sm = int(dict(kv.split('=') for kv in pc[-1].split()[1:])['sum'], 16) == file_sum(n)
    i0 = seg.index(ld[0]); rf = [x for x in seg[i0:] if x.startswith('RFAST')]
    kt = statistics.median(float(re.search(r'ktris=([\d.]+)', x).group(1)) for x in rf) if rf else -1
    dr = statistics.median(int(re.search(r'draws=(\d+)', x).group(1)) for x in rf) if rf else -1
    how = ('нет строки PVSLOAD' if not pl else 'БЕЗ PVS' if not ok else f'кусками ({ch})' if ch else 'в запасе участка' if ar else 'одним блоком из кучи')
    if sm is not None: how += ', сумма пула ' + ('= файлу' if sm else 'НЕ РАВНА файлу')
    big = n in (1, 4); e = []
    if mode in ('new', 'cleannew'):
        if big and not (ok and ch >= 2 and sm): e.append('нет PVS кусками')
        if not big and not (ok and ar): e.append('прочий уровень не в запасе участка')
    elif mode == 'cleanold':
        if not ok or ch: e.append('прежний код: нет PVS одним блоком')
    elif mode == 'red' and big and pl and not ok: red_hit += 1
    print(f"  L{n}: PVS1 {q.get('bytes', '?')} Б — {how}; кадр ktris {kt} draws {dr}" + (f"  НАРУШЕНИЕ: {', '.join(e)}" if e else ''))
    print(f"FRAME L{n} {kt} {dr}", file=sys.stderr)
    bad += len(e)
if mode == 'red':
    print('OK красный краснеет' if red_hit else 'ALARM красный не покраснел'); sys.exit(0 if red_hit else 1)
print("итого нарушений:", bad); print('OK' if bad == 0 and seen else 'ALARM'); sys.exit(0 if bad == 0 and seen else 1)
