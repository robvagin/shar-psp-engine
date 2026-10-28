#!/usr/bin/env python3
"""№1061: доказательство «только машина» для tools/car_tire_splice1061.py: BASE.shr OUT.shr
OUT без вставленных текстур (между концом таблицы текстур и её продолжением) равен BASE везде, кроме белого списка: заголовок @4 (+k) и @76 @84 @88 @92 @96 @100 (+Δ),
REGF at (+Δ), смещения блобов в таблице клеток (+Δ), поле tex кусков колёс, байты UV (первые 4 Б вершины) вершин кусков колёс. Печатает PASS/FAIL и число байт вне белого списка."""
import struct, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import carshr_probe1061 as pr   # noqa: E402
import car_tire_splice1061 as sp   # noqa: E402
sys.argv, argv = sys.argv[:1], sys.argv
B = Path(argv[1]).read_bytes(); O = Path(argv[2]).read_bytes()
h = pr.parse(B); ho = pr.parse(O); delta = len(O) - len(B); ps = pr.pieces(B, h); pso = pr.pieces(O, ho)
X = O[:h['texEnd']] + O[h['texEnd'] + delta:]
assert len(X) == len(B), (len(X), len(B))
ws = sp.wheel_pieces(B, h, ps); wi = {p['i'] for _, p in ws}
ok = np.ones(len(B), bool)   # True = должно совпасть
def allow(a, n): ok[a:a + n] = False
allow(4, 4)
for o in (76, 84, 88, 92, 96, 100): allow(o, 4)
pvs = h['pvs']; regf = (pvs if pvs else len(B)) - 16
if B[regf:regf + 4] == b'REGF': allow(regf + 4, 4)
for i in range(h['ncell']): allow(h['cellOff'] + 48 * i + 36, 4)
for i in wi:
    allow(h['tableOff'] + 80 * i + 24, 4)
    start, vs, po = sp.vert_view(B, h, ps[i])
    for k in range(ps[i]['nv']): allow(start + k * vs, 4)
d = np.frombuffer(B, np.uint8) != np.frombuffer(X, np.uint8)
bad = int((d & ok).sum()); changed = int(d.sum())
# значения белого списка
errs = []
if struct.unpack_from('<I', O, 4)[0] <= struct.unpack_from('<I', B, 4)[0]: errs.append('ntex не вырос')
for o in (76, 84, 88, 92, 96, 100):
    vb, vo = struct.unpack_from('<I', B, o)[0], struct.unpack_from('<I', O, o)[0]
    if vb and vo != vb + delta: errs.append(f'@{o}: {vb} -> {vo}, ожидалось +{delta}')
for i in range(h['ncell']):
    vb = struct.unpack_from('<I', B, h['cellOff'] + 48 * i + 36)[0]; vo = struct.unpack_from('<I', X, h['cellOff'] + 48 * i + 36)[0]
    if vo != vb + delta: errs.append(f'клетка {i}: {vb} -> {vo}'); break
for p, q in zip(ps, pso):
    if p['i'] in wi: continue
    if p != q: errs.append(f'кусок {p["i"]} изменён вне колёс'); break
for i in wi:
    if pso[i]['nv'] != ps[i]['nv'] or pso[i]['flags'] != ps[i]['flags']: errs.append(f'колесо {i}: nv/flags')
if ho['texEnd'] != ho['tableOff']: errs.append('конец таблицы текстур не у таблицы кусков')
if ho['texEnd'] != h['texEnd'] + delta: errs.append('конец таблицы текстур не сдвинулся на Δ')
print(f'Δ {delta} Б, байт отличается {changed}, вне белого списка {bad}, ошибок значений {len(errs)}', errs[:5])
print('PASS' if bad == 0 and not errs else 'FAIL')
