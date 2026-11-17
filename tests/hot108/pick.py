#!/usr/bin/env python3
# 108: выбрать из level.shr (PRP1/PRP2) предметы для сценария эмулятора: разные типы, радиус 0,25..2,5 м, 40..160 м от спавна L1. Печатает «x z курс» точки в 8 м перед предметом.
import struct, math, sys
d = open(sys.argv[1], 'rb').read(); po = struct.unpack_from('<I', d, 96)[0]; h = d[po:po + 4]
npiece, ntype, ninst, vb, ib = struct.unpack_from('<5I', d, po + 4)
q = po + (64 if h == b'PRP2' else 32) + vb + ib + npiece * 40
types = [struct.unpack_from('<3f2I', d, q + 20 * i) for i in range(ntype)]; q += ntype * 20
defs = []
for i in range(ninst):
    v = struct.unpack_from('<7fI', d, q + 32 * i); defs.append((v[0], v[1], v[2], v[7]))
cand = sorted((math.hypot(x - 220, z - 172), i, x, y, z, t) for i, (x, y, z, t) in enumerate(defs) if 40 < math.hypot(x - 220, z - 172) < 160 and 0.25 < types[t][0] < 2.5)
seen = set(); n = int(sys.argv[2]) if len(sys.argv) > 2 else 5
for dist, i, x, y, z, t in cand:
    if t in seen: continue
    seen.add(t); ang = 0.9 * len(seen); print("%.1f %.1f %.4f %d" % (x - math.sin(ang) * 8, z - math.cos(ang) * 8, ang, i))
    if len(seen) >= n: break
