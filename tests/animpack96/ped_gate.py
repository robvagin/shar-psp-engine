#!/usr/bin/env python3
"""Модели-96: пешеходы (общий блоб traffic, tools/traffic_export.ped_blob): ключи клипов нового блоба против старого, дорожка за дорожкой,
поза в каждом кадре и посередине (поворот, град; перенос, мм). Красный контроль: --scale 0.01.   ped_gate.py LEVEL"""
import sys, struct, math, numpy as np, os, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import char_export as ce, animpack as A, traffic_export as T

def headers(b):
    out = []; p = 0
    for off in range(0, len(b) - 128, 16):
        if b[off:off + 4] == b'CHR1' and struct.unpack_from('<I', b, off + 4)[0] in (1, 2): out.append(off)
    return out

def tracks(b, offH):
    magic, ver, size, nj, nbt, ntx, ncl, nbl, offJ, offB, offT, offC = struct.unpack_from('<4s11I', b, offH)
    res = {}
    for c in range(ncl):
        o = offC + 80 * c; name = b[o:o + 48].split(b'\0')[0].decode(); frames, fps = struct.unpack_from('<ff', b, o + 48); offTr, = struct.unpack_from('<I', b, o + 64)
        for j in range(nj):
            nr, nt, orr, ot, fl = struct.unpack_from('<HHIII', b, offTr + 16 * j)
            rot = A.decode_rot(fl & 3, b, orr, nr) if nr else None
            tr = A.decode_trans((fl >> 2) & 3, b, ot, nt) if nt else None
            res[(name, j)] = (frames, rot, tr)
    return res, nj, offJ, b

def strip_bytes(b, offH):
    """байты вершин каждой пачки в порядке полосы (с раскрытием индексов), плюс остальные поля пачки"""
    magic, ver, size, nj, nbt, ntx, ncl, nbl, offJ, offB, offT, offC = struct.unpack_from('<4s11I', b, offH); out = []
    for i in range(nbt):
        o = offB + 48 * i; vt, prim, nv, ov, tex, nb = struct.unpack_from('<IIIIHH', b, o); bones = b[o + 20:o + 28]; stride, fl = struct.unpack_from('<II', b, o + 28)
        oi, stored, _ = struct.unpack_from('<III', b, o + 36)
        if fl & 4:
            assert ver == 2 and vt & (2 << 11)
            raw = np.frombuffer(b, np.uint8, stored * stride, ov).reshape(stored, stride); idx = np.frombuffer(b, '<u2', nv, oi); assert idx.max() < stored
            vb = raw[idx].tobytes(); vt &= ~(2 << 11)
        else: vb = b[ov:ov + nv * stride]
        out.append((vt, prim, nv, tex, nb, bytes(bones), stride, fl & 3, vb))
    return out

def sample(rot, tr, f, rest_q, rest_t):
    q, t = rest_q, rest_t
    if rot is not None: a, bq, u = ce._key(rot[0], rot[1], f); q = ce.qmix(a, bq, u)
    if tr is not None: a, bt, u = ce._key(tr[0], tr[1], f); t = a + (bt - a) * u
    return q, t

def main():
    level = int(sys.argv[1]); k = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1.0
    _, pg = T.level_script(level); names = []
    for g in sorted(pg):
        for n, _ in pg[g]:
            if n not in names: names.append(n)
    def build(old):
        A.OLD = old; T.PEDIDX = not old; return T.ped_blob(names)[0]
    bo = build(True); bn = build(False); A.OLD = False; T.PEDIDX = True
    if '--corrupt' in sys.argv:                       # красный контроль геометрии: один байт вершины испорчен
        bn = bytearray(bn); hh = headers(bn)[0]; ob_ = struct.unpack_from('<I', bn, hh + 36)[0]; ov_ = struct.unpack_from("<I", bn, ob_ + 12)[0]; bn[ov_ + 20] ^= 0x40; bn = bytes(bn)
    ho, hn = headers(bo), headers(bn); assert len(ho) == len(hn) and ho, 'headers'
    worst_r = worst_t = 0.; n = 0; geom_bad = 0
    for a, b in zip(ho, hn):
        if strip_bytes(bo, a) != strip_bytes(bn, b): geom_bad += 1
        to, nj, offJ, _ = tracks(bo, a); tn, *_ = tracks(bn, b)
        for key, (frames, ro, tro) in to.items():
            _, rn, trn = tn[key]; j = key[1]
            rq = np.array(struct.unpack_from('<4f', bo, offJ + 128 * j + 40)); rt = np.array(struct.unpack_from('<3f', bo, offJ + 128 * j + 56))
            for f in np.arange(0, frames - 1 + 1e-9, 0.25):
                qa, ta = sample(ro, tro, f, rq, rt); qb, tb = sample(rn, trn, f, rq, rt)
                worst_r = max(worst_r, math.degrees(2 * math.acos(min(1, abs(float(np.dot(qa, qb))))))); worst_t = max(worst_t, float(np.abs(ta - tb).max()) * 1000); n += 1
    gtxt = 'да' if not geom_bad else 'НЕТ у %d моделей' % geom_bad
    ok = worst_r <= 0.2 * k and worst_t <= 2.0 * k and geom_bad == 0
    print(f'уровень {level}: {len(ho)} моделей, {n} проб; макс. поворот {worst_r:.3f}° (порог {0.2 * k:.3f}), перенос {worst_t:.3f} мм (порог {2.0 * k:.3f}); вершины полос побайтно равны: {gtxt}; блоб {len(bo):,} -> {len(bn):,} Б')
    print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
main()
