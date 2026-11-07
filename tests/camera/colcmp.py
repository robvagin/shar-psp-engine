#!/usr/bin/env python3
"""№1166: коллизия COL1 двух level.shr по массивам (то, по чему закон объектива считает стены): python3 colcmp.py A.shr B.shr.
Для каждого массива: совпадает или сколько записей и каких байт различается; «материал» = байт 63 коробки (ColBox.pad) и байт 39 цилиндра (ColCyl.pad[0]): 0 твёрдый, 1 листва, 2 призрак (машину не держит).
Код выхода 0 — геометрия (всё, кроме материала) совпадает, 1 — иначе. Заголовок (смещение блока JMP1) и блок JMP1 при другом порядке секций не сравниваются побайтно."""
import struct, sys
import numpy as np
def load(p):
    b = open(p, 'rb').read(); off = struct.unpack_from('<I', b, 92)[0]
    assert b[off:off + 4] == b'COL1', 'нет COL1 по смещению заголовка @92'
    W, H, nt, nb, nc, ns, nr = struct.unpack_from('<IIIIIII', b, off + 16); return b, off, (W, H, nt, nb, nc, ns, nr)
A, B = load(sys.argv[1]), load(sys.argv[2])
print('A: W,H,tri,box,cyl,seg,ref =', A[2]); print('B: W,H,tri,box,cyl,seg,ref =', B[2])
if A[2] != B[2]: print('ЧИСЛА ПРИМИТИВОВ РАЗНЫЕ'); sys.exit(1)
W, H, nt, nb, nc, ns, nr = A[2]; pa, pb = A[1] + 48, B[1] + 48; geo = True
for n, l, rs, mat in [('tri', nt * 48, 48, None), ('box', nb * 64, 64, 63), ('cyl', nc * 48, 48, 39), ('seg', ns * 32, 32, None), ('cellStart', (W * H + 1) * 4, 0, None), ('refs', nr * 4, 0, None)]:
    x, y = A[0][pa:pa + l], B[0][pb:pb + l]; pa += l; pb += l
    if x == y: print('%-10s совпадает' % n); continue
    d = np.nonzero(np.frombuffer(x, np.uint8) != np.frombuffer(y, np.uint8))[0]
    if rs:
        offs = sorted(set(int(i) % rs for i in d)); recs = len(set(int(i) // rs for i in d))
        only_mat = offs == [mat] if mat is not None else False
        print('%-10s РАЗНИЦА в %d записях, байты записи %s%s' % (n, recs, offs, ' — только материал' if only_mat else ' — ГЕОМЕТРИЯ'))
        geo &= only_mat
    else: print('%-10s РАЗНИЦА в %d байтах — ГЕОМЕТРИЯ' % (n, len(d))); geo = False
print('ИТОГ:', 'геометрия коллизии совпадает' if geo else 'геометрия коллизии РАЗНАЯ'); sys.exit(0 if geo else 1)
