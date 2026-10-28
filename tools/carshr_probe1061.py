#!/usr/bin/env python3
"""№1061: разбор level.shr (SHR7, порядок секций Загрузки 2): текстуры, куски машины (owner 1), суставы CAR1 — для подмены колёс машины игрока"""
import struct, sys
from pathlib import Path


def parse(d):
    assert d[:4] == b'SHR7'
    ntex, npc = struct.unpack_from('<II', d, 4); hd = struct.unpack_from('<16f', d, 12)
    hw, hh = struct.unpack_from('<ii', d, 12 + 14 * 4)
    tableOff, ncell, cellOff, pvs, col, prop, car = struct.unpack_from('<7I', d, 76)
    p = 104; p += hw * hh * 8; p = (p + 15) & ~15; p += hw * hh * 4; p = (p + 15) & ~15   # lmapDenseEnd
    texOff = p; recs = []
    for k in range(ntex):
        w, h, lv = struct.unpack_from('<HHH', d, p); fmt, pre, small = d[p + 6], d[p + 7], d[p + 8]
        q = p + 16 + 1024; lw, lh = w, h
        for l in range(lv):
            rb = max(16, (lw + 1) >> 1) if fmt == 4 else (16 if (pre and lw < 16) else lw); rows = ((lh + 7) & ~7) if pre else lh
            q += rb * rows; q = (q + 15) & ~15; lw = lw >> 1 or 1; lh = lh >> 1 or 1
        recs.append((p, q, w, h, lv, fmt, pre, small)); p = q
    return dict(ntex=ntex, npc=npc, hw=hw, hh=hh, tableOff=tableOff, ncell=ncell, cellOff=cellOff, pvs=pvs, col=col, prop=prop, car=car, texOff=texOff, texEnd=p, recs=recs)


def pieces(d, h):
    out = []
    for i in range(h['npc']):
        r = struct.unpack_from('<6fiiiii4f5I', d, h['tableOff'] + 80 * i)
        out.append(dict(i=i, bb=r[:6], tex=r[6], flags=r[7], nv=r[8], ni=r[9], owner=r[10], org=r[11:15], off=r[15], nf=r[16], cell=r[17], pad=(r[18], r[19])))
    return out


if __name__ == '__main__':
    d = Path(sys.argv[1]).read_bytes(); h = parse(d)
    print({k: v for k, v in h.items() if k != 'recs'}); print('texEnd', h['texEnd'], 'tableOff', h['tableOff'], 'ntex', len(h['recs']))
    ps = pieces(d, h); car = [p for p in ps if p['owner'] == 1]
    print('кусков', len(ps), 'машины', len(car))
    for p in car: print(p)
    cj = struct.unpack_from('<8I', d, h['car']); njoint = cj[2]
    q = h['car'] + 32 + 48
    print('суставы:', [d[q + 28 * j:q + 28 * j + 12].split(b'\0')[0].decode() for j in range(njoint)])
    for c in sorted({p['cell'] for p in car}):
        print('cell', c, struct.unpack_from('<6f6I', d, h['cellOff'] + 48 * c))
