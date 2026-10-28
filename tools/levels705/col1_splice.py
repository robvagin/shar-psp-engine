#!/usr/bin/env python3
"""№705 (Глюки): подмена только секции COL1 в level.shr нового порядка секций (tools/shr_order.py): BASE (файл набора) + COL1 из DONOR (свежий экспорт).
  col1_splice.py BASE.shr DONOR.shr OUT.shr
Раскладка BASE: [резидент..таблица клеток][COL1 @92][PRP @96][CAR @100][REG1+REGF][PVS1 @88][блобы клеток].
Что меняется при другой длине COL1 (Δ): заголовок @88 @96 @100 += Δ, 'at' в сноске REGF (за 16 Б до PVS1) += Δ, смещения блобов в таблице клеток (@36 записи по 48 Б) += Δ.
Содержимое остальных секций не трогается (как в shr_order). DONOR — экспорт в любом порядке: COL1 = [@92 .. @96)."""
import struct, sys


def hdr(d):
    if d[:4] != b'SHR7': raise SystemExit('не SHR7')
    tableOff, ncell, cellOff, pvs, col, prop, car = struct.unpack_from('<7I', d, 76)
    return ncell, cellOff, pvs, col, prop, car


def main():
    if len(sys.argv) != 4: raise SystemExit(__doc__)
    base = open(sys.argv[1], 'rb').read(); donor = open(sys.argv[2], 'rb').read()
    ncell, cellOff, pvs, col, prop, car = hdr(base)
    resEnd = cellOff + 48 * ncell
    offs = [struct.unpack_from('<I', base, cellOff + 48 * i + 36)[0] for i in range(ncell)]
    if not (col == resEnd and min(offs) >= max(pvs, col)): raise SystemExit('BASE не в новом порядке секций (tools/shr_order.py --check)')
    nc, _, _, dcol, dprop, _ = hdr(donor)
    new_col = donor[dcol:dprop]; old_len = prop - col; delta = len(new_col) - old_len
    if len(new_col) % 16 or old_len % 16: raise SystemExit('COL1 не кратна 16 Б: %d / %d' % (len(new_col), old_len))
    if new_col[:4] != base[col:col + 4]: raise SystemExit('метка COL1 не совпала: %r / %r' % (new_col[:4], base[col:col + 4]))
    out = bytearray(base[:col]) + new_col + base[prop:]
    for h in (88, 96, 100):
        v = struct.unpack_from('<I', out, h)[0]
        if v: struct.pack_into('<I', out, h, v + delta)
    regf = (pvs if pvs else len(base)) - 16 + delta
    if out[regf:regf + 4] == b'REGF':
        at = struct.unpack_from('<I', out, regf + 4)[0]; struct.pack_into('<I', out, regf + 4, at + delta)
    for i in range(ncell):
        p = cellOff + 48 * i + 36; v = struct.unpack_from('<I', out, p)[0]; struct.pack_into('<I', out, p, v + delta)
    assert len(out) == len(base) + delta
    open(sys.argv[3], 'wb').write(out)
    print('col1_splice: COL1 %d -> %d Б (Δ %+d); итог %d Б' % (old_len, len(new_col), delta, len(out)))


main()
