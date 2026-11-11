#!/usr/bin/env python3
"""Пропечка-96 (Fable шаг 11, COLLSPLIT): проверка пакета гэгов CLT1 перед ленивой загрузкой вершин по типу (viewer/coll_stream.c).

  python3 tools/collect_check.py collect-l4.shr [...]      каждый файл: вершины каждого типа - сплошной отрезок общего блоба, типы не пересекаются,
                                                           сумма отрезков = блоб вершин; печатает резидент до/после (что останется в блоке)
  python3 tools/collect_check.py --selftest                красный контроль: на копии в памяти ломает один тип (сдвиг куска в чужой отрезок) - проверка обязана упасть

Формат файла не меняется (CLT1 v3): движок читает отрезок типа прямо из файла со смещения «начало SPR1 + 64 + vOff»."""
import struct, sys, collections
A = lambda x: (x + 15) & ~15

def parse(d):
    h = struct.unpack('<20I', d[:80]); sp = h[5]; s = struct.unpack('<16I', d[sp:sp + 64])
    ntype, ninst, npiece, njoint, nkey, nvis, nstate, nevent, ncall, ncol, nbill, vb, ib, ndyn, nname = s[1:16]
    sizes = [vb, ib, npiece * 40, ntype * 84, njoint * 52, nkey * 28, nvis * 4, nstate * 80, nevent * 4, ncall * 8, ncol * 64, nbill * 40, ninst * 32, ndyn, nname * 32]
    off = sp + 64; ptr = []
    for z in sizes: ptr.append(off); off = A(off + z)
    pieces = [struct.unpack('<4f2i4I', d[ptr[2] + i * 40:ptr[2] + i * 40 + 40]) for i in range(npiece)]
    types = [struct.unpack('<10H2H4B4I2f2I6f', d[ptr[3] + i * 84:ptr[3] + i * 84 + 84]) for i in range(ntype)]
    return h, s, vb, ib, pieces, types, ptr, sizes

def check(d, name):
    h, s, vb, ib, pieces, types, ptr, sizes = parse(d); bad = []
    rng = []
    for t, T in enumerate(types):
        ps = pieces[T[0]:T[0] + T[1]]
        for p in ps:
            if p[8] + p[6] * 16 > vb: bad.append(f'тип {t}: кусок выходит за блоб вершин')
        ps = [p for p in ps if p[6]]
        if not ps: continue
        lo = min(p[8] for p in ps); hi = max(p[8] + p[6] * 16 for p in ps); used = sum(p[6] * 16 for p in ps)
        q = sorted((p[8], p[8] + p[6] * 16) for p in ps)
        for x, y in zip(q, q[1:]):
            if y[0] < x[1]: bad.append(f'тип {t}: куски наложены ({x} и {y})')
            elif y[0] > x[1]: bad.append(f'тип {t}: дыра между кусками ({x} и {y})')
        if hi - lo != used: bad.append(f'тип {t}: в отрезке [{lo},{hi}) дыры или наложение кусков ({hi - lo} != {used})')
        rng.append((lo, hi, t))
    rng.sort()
    for a, b in zip(rng, rng[1:]):
        if b[0] < a[1]: bad.append(f'типы {a[2]} и {b[2]} делят вершины')
    tot = sum(hi - lo for lo, hi, t in rng)
    if tot != vb: bad.append(f'сумма отрезков {tot} != блоб вершин {vb}')
    kb = A(s[5] * 28); end = h[15] + h[14] * 64; res = end - kb
    print(f'{name}: типов с вершинами {len(rng)}, вершин {vb} Б; резидент {res} -> {res - A(vb)} Б' + (' OK' if not bad else ' FAIL'))
    for b in bad: print('  ', b)
    return not bad

if __name__ == '__main__':
    if sys.argv[1:] == ['--selftest']:
        import glob
        f = (glob.glob('build/levels/l4/collect-l4.shr') + glob.glob('../shar-psp/build/levels/l4/collect-l4.shr') + glob.glob('build/collect-l1.shr'))
        if not f: print('selftest: нет пакета'); sys.exit(2)
        d = bytearray(open(f[0], 'rb').read()); h, s, vb, ib, pieces, types, ptr, sizes = parse(bytes(d))
        # ломаем: у второго куска первого типа с вершинами сдвигаем vOff на 16 Б (дыра и наложение)
        t0 = next(T for T in types if T[1] >= 2 and pieces[T[0]][6] and pieces[T[0] + 1][6])
        o = ptr[2] + (t0[0] + 1) * 40 + 32; v = struct.unpack('<I', d[o:o + 4])[0]; d[o:o + 4] = struct.pack('<I', v + 16)
        ok_good = check(bytes(open(f[0], 'rb').read()), 'целый'); ok_bad = check(bytes(d), 'сломанный')
        print('selftest', 'PASS' if ok_good and not ok_bad else 'FAIL (красный контроль не покраснел)'); sys.exit(0 if ok_good and not ok_bad else 1)
    rc = 0
    for f in sys.argv[1:]:
        if not check(open(f, 'rb').read(), f): rc = 1
    sys.exit(rc)
