#!/usr/bin/env python3
"""Модели-96, проба «дальние клетки» (просьба Роба, пропечка п.1 из ПРОПЕЧКА_ЧТО_ЕЩЁ_2026-11-09): только картинка для решения, в игру не вливается.
Берёт level.shr (SHR7), у всех клеток города дальше R0 м от точки обзора заменяет треугольники упрощёнными (meshoptimizer, MIT):
вершины остаются ТЕ ЖЕ (uv, цвет, позиция — байты записи не меняются), убираются только треугольники и вершины; обход сохраняется.
Запись на месте: куски без индексов (флаг 16) хранят ni вершин подряд — пишем меньше вершин в начало блоба и уменьшаем nv/ni, смещения клеток не двигаются.
  farcell_probe.py LEVEL.shr OUT.shr --eye X Z [--r0 120] [--ratio 0.2] [--err 0.35] [--rmax 250] [--lock 1]
Печатает: треугольники и КБ геометрии клеток до/после — по кольцам 0–120 (не трогаем), 120–180, 180–250, дальше."""
import sys, struct, argparse, ctypes
from pathlib import Path
import numpy as np
import meshoptimizer as mo

_lib = mo.simplifier.lib
_P = ctypes.POINTER
_lib.meshopt_simplifyWithAttributes.argtypes = [_P(ctypes.c_uint), _P(ctypes.c_uint), ctypes.c_size_t, _P(ctypes.c_float), ctypes.c_size_t, ctypes.c_size_t,
    _P(ctypes.c_float), ctypes.c_size_t, _P(ctypes.c_float), ctypes.c_size_t, _P(ctypes.c_ubyte), ctypes.c_size_t, ctypes.c_float, ctypes.c_uint, _P(ctypes.c_float)]
_lib.meshopt_simplifyWithAttributes.restype = ctypes.c_size_t
_lib.meshopt_simplifyScale.argtypes = [_P(ctypes.c_float), ctypes.c_size_t, ctypes.c_size_t]; _lib.meshopt_simplifyScale.restype = ctypes.c_float
F_NOIDX, F_V12 = 16, 32
OPT_LOCK_BORDER, OPT_ABSOLUTE = 1, 4


WUV = 0.15
WELD = 'raw'
def simp(raw, vs, sc, ratio, err, lock):
    """raw (n,vs) вершины подряд по 3 на треугольник -> новые вершины (m,vs), m кратно 3.
    WELD='raw': вершины склеены по полной записи (позиция+uv+цвет), швы текстуры и цвета — границы, их не сворачивать.
    WELD='pos': склейка по позиции; запись угла берётся у ближайшего исходного треугольника, в котором эта вершина есть"""
    n = len(raw)
    if n < 3 * 12: return raw
    key = raw if WELD == 'raw' else np.ascontiguousarray((raw[:, 6:12] if vs == 12 else raw[:, 8:14]))
    u, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1).astype(np.uint32)
    rep = raw[first]
    xyz = (rep[:, 6:12] if vs == 12 else rep[:, 8:14]).copy().view('<i2').reshape(-1, 3).astype(np.float32) * np.float32(sc / 32768.0)
    uv = rep[:, 0:4].copy().view('<u2').reshape(-1, 2).astype(np.float32) / 256.0
    t = inv.reshape(-1, 3); ok = (t[:, 0] != t[:, 1]) & (t[:, 1] != t[:, 2]) & (t[:, 0] != t[:, 2]); t0 = t[ok]
    if len(t0) < 12: return raw
    idx = np.ascontiguousarray(t0.reshape(-1)); dst = np.zeros(len(idx), np.uint32)
    pos = np.ascontiguousarray(xyz); att = np.ascontiguousarray(uv); w = np.array([WUV, WUV], np.float32)
    target = max(3 * 4, int(len(idx) * ratio) // 3 * 3); res = ctypes.c_float(0)
    nout = _lib.meshopt_simplifyWithAttributes(dst.ctypes.data_as(_P(ctypes.c_uint)), idx.ctypes.data_as(_P(ctypes.c_uint)), len(idx),
        pos.ctypes.data_as(_P(ctypes.c_float)), len(pos), 12, att.ctypes.data_as(_P(ctypes.c_float)), 8, w.ctypes.data_as(_P(ctypes.c_float)), 2, None, target,
        ctypes.c_float(err), (OPT_LOCK_BORDER if lock else 0) | OPT_ABSOLUTE, ctypes.byref(res))
    out = dst[:nout].reshape(-1, 3)
    if WELD == 'raw': return rep[out.reshape(-1)]
    # запись угла: у ближайшего (по центру) исходного треугольника, содержащего эту вершину; если такого нет — запись представителя
    cen_o = xyz[t0].mean(1); cen_n = xyz[out].mean(1); res_rows = np.empty((len(out) * 3, vs), np.uint8)
    src_rows = raw.reshape(-1, 3, vs)[ok]                          # записи углов исходных треугольников (после отброса вырожденных)
    for k in range(len(out)):
        d2 = ((cen_o - cen_n[k]) ** 2).sum(1); order = np.argsort(d2)[:64]
        for c in range(3):
            v = out[k, c]; hit = np.where((t0[order] == v).any(1))[0]
            if len(hit): j = order[hit[0]]; cc = int(np.where(t0[j] == v)[0][0]); res_rows[k * 3 + c] = src_rows[j, cc]
            else: res_rows[k * 3 + c] = rep[v]
    return res_rows


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('level'); ap.add_argument('out')
    ap.add_argument('--eye', nargs=2, type=float, required=True); ap.add_argument('--r0', type=float, default=120.); ap.add_argument('--ratio', type=float, default=0.2)
    ap.add_argument('--err', type=float, default=0.35); ap.add_argument('--rmax', type=float, default=250.); ap.add_argument('--lock', type=int, default=1); ap.add_argument('--wuv', type=float, default=0.15); ap.add_argument('--weld', default='raw'); ap.add_argument('--drop', type=int, default=0)
    a = ap.parse_args(); global WUV, WELD; WUV = a.wuv; WELD = a.weld
    b = bytearray(Path(a.level).read_bytes()); assert b[:4] == b'SHR7'
    ntex, npc = struct.unpack_from('<II', b, 4); ptab, = struct.unpack_from('<I', b, 76); ncell, ctab = struct.unpack_from('<II', b, 80)
    cells = [struct.unpack_from('<6fIIIII', b, ctab + 48 * i) for i in range(ncell)]
    ex, ez = a.eye; ring_edges = [0, a.r0, 180., a.rmax, 1e9]; names = ['0-%d' % a.r0, '%d-180' % a.r0, '180-%d' % a.rmax, 'дальше']
    st = np.zeros((4, 4))   # кольцо: треугольники до, после, КБ до, после
    for i in range(npc):
        o = ptab + 80 * i
        bb = struct.unpack_from('<6f', b, o); tex, fl, nv, ni, own = struct.unpack_from('<iIIII', b, o + 24)
        ox, oy, oz, sc = struct.unpack_from('<4f', b, o + 44); off, nf, ci = struct.unpack_from('<III', b, o + 60)
        if own != 0 or ni == 0 or not fl & F_NOIDX: continue
        vs = 12 if fl & F_V12 else 16; cb = cells[ci]
        cx, cz = (cb[0] + cb[3]) / 2, (cb[2] + cb[5]) / 2; r = float(np.hypot(cx - ex, cz - ez))
        ring = 0 if r < ring_edges[1] else 1 if r < ring_edges[2] else 2 if r < ring_edges[3] else 3
        base = cb[9] + off
        raw = np.frombuffer(bytes(b[base:base + ni * vs]), np.uint8).reshape(ni, vs)
        new = raw
        if r >= a.r0 and (ring_edges[3] < 1e8):
            new = raw[:0] if a.drop else simp(raw, vs, sc, a.ratio, a.err, a.lock)
            if len(new) != ni:
                b[base:base + len(new) * vs] = new.tobytes()
                struct.pack_into('<II', b, o + 32, len(new), len(new))
        st[ring] += (ni // 3, len(new) // 3, ni * vs / 1024, len(new) * vs / 1024)
    Path(a.out).write_bytes(bytes(b))
    print('кольцо м от точки   треугольники до -> после   (доля)    КБ геометрии до -> после')
    for k in range(4): print('%-14s %9d -> %-9d (%4.0f %%)   %9.0f -> %-9.0f' % (names[k], st[k, 0], st[k, 1], 100 * st[k, 1] / max(st[k, 0], 1), st[k, 2], st[k, 3]))
    tot = st[1:3].sum(0); print('120–250 м всего: %d -> %d треугольников (%.0f %%), %.0f -> %.0f КБ' % (tot[0], tot[1], 100 * tot[1] / max(tot[0], 1), tot[2], tot[3]))


if __name__ == '__main__': main()
