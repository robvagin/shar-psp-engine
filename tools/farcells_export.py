#!/usr/bin/env python3
"""Модели-96 (101): пекарня дальних клеток. level.shr -> l{N}/farcells.shr («FAR1»): для каждой клетки города упрощённая версия (те же вершины/UV/цвет/текстуры, меньше треугольников).
Игра: дальше 120 м от глаза грузит дальнюю версию клетки вместо полной (viewer/main_stream.h, Лестница-96); level.shr не меняется, файл необязательный (нет файла или не сошёлся ключ — всё как раньше).
  farcells_export.py LEVEL.shr OUT.shr [--mode 22|42]
    22 — края кусков не заперты (стенд швов Кольца-96: щели, НЕ брать); 42 — заперты общие вершины (95 швов на L1); 50 — заперты общие вершины и все открытые рёбра куска (швов нет по построению), по умолчанию.
Формат FAR1 (little-endian), подробно — docs/handoff/ДАЛЬНИЕ_КЛЕТКИ_ФОРМАТ_2026-11-09.md:
  заголовок 64 Б: 'FAR1', ver=1, key, ncell, npiece, offCells, offPieces, offBlob, mode, blobBytes, levelSize, 0...
  ключ: FNV-1a по первым 4096 байтам level.shr и размеру файла (как у colbake: не сошёлся — файл игнорируется)
  клетка (16 Б, по номеру клетки уровня): farOff (от offBlob; 0xFFFFFFFF — дальней версии нет), farSize, nvTotal, flags
  кусок (16 Б, по номеру куска уровня): nvFar, offFar (байт внутри дальнего блоба своей клетки), flags (1 — упрощён), pad
  дальний блоб клетки: вершины ВСЕХ кусков клетки подряд в порядке кусков, формат вершины как у полной версии (16 Б, 12 Б при флаге 32), без индексов, тот же origin и scale куска"""
import sys, struct, ctypes, argparse
from pathlib import Path
import numpy as np
import meshoptimizer as mo

_lib = mo.simplifier.lib; _P = ctypes.POINTER
_lib.meshopt_simplifyWithAttributes.argtypes = [_P(ctypes.c_uint), _P(ctypes.c_uint), ctypes.c_size_t, _P(ctypes.c_float), ctypes.c_size_t, ctypes.c_size_t,
    _P(ctypes.c_float), ctypes.c_size_t, _P(ctypes.c_float), ctypes.c_size_t, _P(ctypes.c_ubyte), ctypes.c_size_t, ctypes.c_float, ctypes.c_uint, _P(ctypes.c_float)]
_lib.meshopt_simplifyWithAttributes.restype = ctypes.c_size_t
F_NOIDX, F_V12 = 16, 32
ISLAND_M = 3.0
import os
WCOL = float(os.environ.get('FAR_WCOL', 0.0))   # вес цвета вершины в метрике упрощения (1,0 — как 1 м)
COL_MAX = float(os.environ.get('FAR_COLMAX', 9.0)); COL_MEAN = float(os.environ.get('FAR_COLMEAN', 9.0))   # потолок отклонения цвета в вершинах, доли 0..1 (≈ 15/255)
EDGE_MAX_M = float(os.environ.get('FAR_EDGE', 1e9))   # потолок длины ребра дальнего треугольника, м
DEV_M = 1.0   # потолок отклонения дальней поверхности от полной, м
NONE = 0xFFFFFFFF
MODES = {22: dict(ratio=.05, err=3.0, wuv=.05, shared=False, border=False), 42: dict(ratio=.15, err=1.0, wuv=.05, shared=True, border=False),
         50: dict(ratio=.05, err=3.0, wuv=.05, shared=True, border=True)}   # 50 — края кусков заперты (открытые рёбра и общие вершины): швов нет по построению; 22 и 42 по стенду Кольца-96 дают щели


def fnv_key(b):
    h = 0x811C9DC5
    for x in b[:4096] + struct.pack('<I', len(b)):
        h = ((h ^ x) * 0x01000193) & 0xFFFFFFFF
    return h


def hashpos(wp):
    q = np.round(wp * 64).astype(np.int64)
    return (q[:, 0] * 73856093) ^ (q[:, 1] * 19349663) ^ (q[:, 2] * 83492791)


def world(rows, vs, sc, o):
    xyz = (rows[:, 6:12] if vs == 12 else rows[:, 8:14]).copy().view('<i2').reshape(-1, 3).astype(np.float64)
    return xyz * (sc / 32768.0) + np.asarray(o)


def pt_tri_dist(P, A, B, C):
    """расстояние от точек P (m,3) до ближайшего из треугольников A,B,C (n,3) — Эриксон, «Real-Time Collision Detection» 5.1.5, векторно: (m,n) -> min по n"""
    P = P[:, None, :]; A = A[None]; B = B[None]; C = C[None]
    ab = B - A; ac = C - A; ap = P - A; d1 = (ab * ap).sum(2); d2 = (ac * ap).sum(2)
    bp = P - B; d3 = (ab * bp).sum(2); d4 = (ac * bp).sum(2); cp = P - C; d5 = (ab * cp).sum(2); d6 = (ac * cp).sum(2)
    vc = d1 * d4 - d3 * d2; vb = d5 * d2 - d1 * d6; va = d3 * d6 - d5 * d4
    den = va + vb + vc; den = np.where(np.abs(den) < 1e-18, 1e-18, den)
    out = A + ab * (vb / den)[..., None] + ac * (vc / den)[..., None]   # внутри треугольника (барицентрические)
    # области вершин и рёбер
    def seg(Q0, Q1):
        d = Q1 - Q0; t = np.clip(((P - Q0) * d).sum(2) / np.maximum((d * d).sum(2), 1e-18), 0, 1); return Q0 + d * t[..., None]
    inside = (va >= 0) & (vb >= 0) & (vc >= 0)
    best = np.where(inside[..., None], out, seg(A, B)); db = ((P - best) ** 2).sum(2)
    for Q0, Q1 in ((B, C), (C, A)):
        q = seg(Q0, Q1); dq = ((P - q) ** 2).sum(2); better = (dq < db) & ~inside; db = np.where(better, dq, db)
    return np.sqrt(db.min(1)), db.argmin(1)


def vcolors(rows, vs):
    if vs == 16: return rows[:, 4:7].astype(np.float32) / 255.0
    c = rows[:, 4:6].copy().view('<u2').reshape(-1).astype(np.uint32)
    return np.stack([(c & 31) / 31.0, ((c >> 5) & 63) / 63.0, (c >> 11) / 31.0], 1).astype(np.float32)


def bary(P, A, B, C):
    v0 = B - A; v1 = C - A; v2 = P - A
    d00 = (v0 * v0).sum(1); d01 = (v0 * v1).sum(1); d11 = (v1 * v1).sum(1); d20 = (v2 * v0).sum(1); d21 = (v2 * v1).sum(1)
    den = np.where(np.abs(d00 * d11 - d01 * d01) < 1e-18, 1e-18, d00 * d11 - d01 * d01)
    v = (d11 * d20 - d01 * d21) / den; w = (d00 * d21 - d01 * d20) / den; u = 1 - v - w
    b = np.clip(np.stack([u, v, w], 1), 0, None); return b / np.maximum(b.sum(1, keepdims=True), 1e-9)


def put_colors(rows, vs, col):
    c = np.clip(np.rint(col * 255.0), 0, 255).astype(np.uint8)
    if vs == 16: rows[:, 4:7] = c
    else:
        r = np.clip(np.rint(col[:, 0] * 31), 0, 31).astype(np.uint32); g = np.clip(np.rint(col[:, 1] * 63), 0, 63).astype(np.uint32); b = np.clip(np.rint(col[:, 2] * 31), 0, 31).astype(np.uint32)
        rows[:, 4:6] = (r | (g << 5) | (b << 11)).astype('<u2').view(np.uint8).reshape(-1, 2)
    return rows


def fit_colors(rows, vs, wp, cu, out, sc, o):
    """rows — углы дальних треугольников (по 3 на треугольник, порядок как у out (m,3), возможно с перестановкой обхода); wp, cu — позиции и цвета исходных (склеенных по позиции) вершин"""
    ids = np.unique(out); nf = len(ids)
    if nf > 900 or nf < 3: return rows
    pos = world(rows, vs, sc, o)   # позиции углов дальней версии
    # склеить углы дальней версии по позиции (как исходные): индекс вершины дальней версии на угол
    key = np.round(pos * 512).astype(np.int64); _, vid, inv = np.unique(key, axis=0, return_index=True, return_inverse=True); inv = inv.reshape(-1); nv_ = len(vid)
    if nv_ > 900: return rows
    tri = inv.reshape(-1, 3); fp = pos[vid]
    A = np.zeros((len(wp), nv_), np.float64)
    for a0 in range(0, len(wp), 256):
        _, j = pt_tri_dist(wp[a0:a0 + 256], fp[tri[:, 0]], fp[tri[:, 1]], fp[tri[:, 2]]); bb = bary(wp[a0:a0 + 256], fp[tri[j, 0]], fp[tri[j, 1]], fp[tri[j, 2]])
        for q in range(3): np.add.at(A, (np.arange(a0, a0 + len(j)), tri[j, q]), bb[:, q])
    c0 = vcolors(rows[vid], vs).astype(np.float64)   # начальные цвета (исходные записи)
    M = A.T @ A + 1e-3 * np.eye(nv_); C = np.linalg.solve(M, A.T @ cu.astype(np.float64) + 1e-3 * c0)
    C = np.clip(C, 0, 1); return put_colors(rows.copy(), vs, C[inv].astype(np.float32))


def simplify(raw, vs, sc, o, mode, shared, twosided=False, fine=False):
    """raw (n,vs) вершины подряд по 3 на треугольник -> новые записи (m,vs); те же байты записей, только меньше треугольников"""
    cfg = MODES[mode]; n = len(raw)
    key = np.ascontiguousarray(raw[:, 6:12] if vs == 12 else raw[:, 8:14])
    _, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    inv = inv.reshape(-1).astype(np.uint32); rep = raw[first]
    wp = world(rep, vs, sc, o)
    uv = rep[:, 0:4].copy().view('<u2').reshape(-1, 2).astype(np.float32) / 256.0
    t = inv.reshape(-1, 3); ok = (t[:, 0] != t[:, 1]) & (t[:, 1] != t[:, 2]) & (t[:, 0] != t[:, 2]); t0 = t[ok]
    if len(t0) < 12: return raw
    idx = np.ascontiguousarray(t0.reshape(-1)); dst = np.zeros(len(idx), np.uint32)
    cu = vcolors(rep, vs); pos = np.ascontiguousarray(wp.astype(np.float32)); att = np.ascontiguousarray(np.concatenate([uv, cu], 1).astype(np.float32)); w = np.array([cfg['wuv']] * 2 + [WCOL] * 3, np.float32)
    lock = None
    if cfg['shared']:
        lk = np.ascontiguousarray(np.array([shared.get(int(h), 0) > 1 for h in hashpos(wp)], np.uint8)); lock = lk.ctypes.data_as(_P(ctypes.c_ubyte))
    target = max(12, int(len(idx) * (.3 if fine else cfg['ratio'])) // 3 * 3)
    nout = _lib.meshopt_simplifyWithAttributes(dst.ctypes.data_as(_P(ctypes.c_uint)), idx.ctypes.data_as(_P(ctypes.c_uint)), len(idx), pos.ctypes.data_as(_P(ctypes.c_float)), len(pos), 12,
        att.ctypes.data_as(_P(ctypes.c_float)), 20, w.ctypes.data_as(_P(ctypes.c_float)), 5, lock, target, ctypes.c_float(.4 if fine else cfg['err']), (1 if cfg['border'] else 0) | 4, None)
    out = dst[:nout].reshape(-1, 3)
    if len(out) == 0: return raw
    # запись каждого угла: у ближайшего исходного треугольника, где эта вершина есть; иначе запись представителя
    cen_o = wp[t0].mean(1); cen_n = wp[out].mean(1); rows = np.empty((len(out) * 3, vs), np.uint8); src = raw.reshape(-1, 3, vs)[ok]
    nrm_o = np.cross(wp[t0][:, 1] - wp[t0][:, 0], wp[t0][:, 2] - wp[t0][:, 0])
    for k in range(len(out)):
        order = np.argsort(((cen_o - cen_n[k]) ** 2).sum(1))[:64]; ref = None
        for c in range(3):
            v = out[k, c]; hit = np.where((t0[order] == v).any(1))[0]
            if len(hit):
                j = order[hit[0]]; rows[k * 3 + c] = src[j, int(np.where(t0[j] == v)[0][0])]
                if ref is None: ref = nrm_o[j]
            else: rows[k * 3 + c] = rep[v]
        # обход: новый треугольник смотрит туда же, что исходный, на котором стоит его первая вершина (иначе GE отсёк бы его как изнанку)
        if ref is not None and not twosided:
            nn = np.cross(wp[out[k, 1]] - wp[out[k, 0]], wp[out[k, 2]] - wp[out[k, 0]])
            if float(nn @ ref) < 0: rows[[k * 3 + 1, k * 3 + 2]] = rows[[k * 3 + 2, k * 3 + 1]]
    # острова: связная часть исходной сетки (по общим вершинам), от которой в дальней версии не осталось ни одной вершины, — целый мелкий кусок поверхности унесён (стенд Кольца-96: «ушла поверхность»); такой кусок остаётся полным
    par = list(range(len(wp)))
    def find(x):
        while par[x] != x: par[x] = par[par[x]]; x = par[x]
        return x
    for t3 in t0:
        r0 = find(int(t3[0]))
        for q in (1, 2):
            r1 = find(int(t3[q]))
            if r1 != r0: par[r1] = r0
    alive = {find(int(v)) for v in np.unique(out)}
    if any(find(int(v)) not in alive for v in np.unique(t0)): return raw
    # цвет: цвета оставшихся вершин подгоняются методом наименьших квадратов так, чтобы поверхность дальней версии воспроизводила цвета исходных вершин (иначе усреднение сдвигает оттенок
    # газона: проба Пропечки-96 L1_p06, средняя 0,95/255); у куска с большим числом вершин подгонка пропускается (остаются исходные цвета)
    cols_all = vcolors(raw, vs); cu_all = cu[inv]   # цвет каждой исходной записи и цвет представителя в той же позиции
    if os.environ.get('FAR_FIT') == '1' and np.abs(cols_all - cu_all).max(1).mean() < 0.004: rows = fit_colors(rows, vs, wp, cu, out, sc, o)   # цвет непрерывный по позиции (земля, газон): подгонка; у плоскозакрашенных граней записи остаются исходными
    # рёбра: GE режет треугольники по защитной полосе (±2048), поэтому у кусков города рёбра были ≤ 28 м (docs/FORMAT_SHR7.md); крупные треугольники упрощения идут в дорогие срезы ggCut и рисуются иначе — потолок ребра EDGE_MAX_M
    fe3 = world(rows, vs, sc, o).reshape(-1, 3, 3); ed = np.maximum.reduce([np.linalg.norm(fe3[:, 1] - fe3[:, 0], axis=1), np.linalg.norm(fe3[:, 2] - fe3[:, 1], axis=1), np.linalg.norm(fe3[:, 0] - fe3[:, 2], axis=1)])
    if ed.max() > EDGE_MAX_M: return raw
    # потолок отклонения (просьба Кольца-96): центры треугольников полной версии не дальше DEV_M от дальней поверхности; иначе куск полный (вызывающий пробует мельче)
    fw = world(rows, vs, sc, o).reshape(-1, 3, 3); co = wp[t0].mean(1)
    for a0 in range(0, len(co), 256):
        if pt_tri_dist(co[a0:a0 + 256], fw[:, 0], fw[:, 1], fw[:, 2])[0].max() > DEV_M: return raw
    # цвет: в каждой исходной ЗАПИСИ (углы треугольников, выборка до 6000) цвет, интерполированный по дальней версии, отличается от исходного не больше COL_MAX, в среднем не больше COL_MEAN (доли 0..1);
    # иначе кусок полный (проба Пропечки-96 L1_p06: слабый сдвиг оттенка газона при упрощении)
    fcol = vcolors(rows, vs).reshape(-1, 3, 3); cmax = csum = 0.0; cn = 0
    sel = np.arange(len(raw)) if len(raw) <= 6000 else np.random.default_rng(1).choice(len(raw), 6000, replace=False)
    wr = world(raw[sel], vs, sc, o)
    for a0 in range(0, len(wr), 256):
        _, j = pt_tri_dist(wr[a0:a0 + 256], fw[:, 0], fw[:, 1], fw[:, 2]); bb = bary(wr[a0:a0 + 256], fw[j, 0], fw[j, 1], fw[j, 2])
        est = (fcol[j] * bb[:, :, None]).sum(1); err = np.abs(est - cols_all[sel][a0:a0 + 256]); cmax = max(cmax, float(err.max())); csum += float(err.sum()); cn += err.size
    if cmax > COL_MAX or csum / max(cn, 1) > COL_MEAN: return raw
    return rows


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('level'); ap.add_argument('out'); ap.add_argument('--mode', type=int, default=50, choices=sorted(MODES)); a = ap.parse_args()
    b = Path(a.level).read_bytes(); assert b[:4] == b'SHR7'
    ntex, npc = struct.unpack_from('<II', b, 4); ptab, = struct.unpack_from('<I', b, 76); ncell, ctab = struct.unpack_from('<II', b, 80)
    cells = [struct.unpack_from('<6fIIIII', b, ctab + 48 * i) for i in range(ncell)]
    P = []
    for i in range(npc):
        o = ptab + 80 * i; tex, fl, nv, ni, own = struct.unpack_from('<iIIII', b, o + 24); ox, oy, oz, sc = struct.unpack_from('<4f', b, o + 44); off, nf, ci = struct.unpack_from('<III', b, o + 60)
        vs = 12 if fl & F_V12 else 16
        P.append(dict(i=i, fl=fl, own=own, ni=ni, ci=ci, vs=vs, sc=sc, o=(ox, oy, oz), off=off, ok=(own == 0 and ni > 0 and bool(fl & F_NOIDX))))
    shared = {}
    if MODES[a.mode]['shared']:
        for p in P:
            if not p['ok']: continue
            raw = np.frombuffer(b, np.uint8, p['ni'] * p['vs'], cells[p['ci']][9] + p['off']).reshape(-1, p['vs'])
            for h in np.unique(hashpos(world(raw, p['vs'], p['sc'], p['o']))): shared[int(h)] = shared.get(int(h), 0) + 1
    ptab_out = [(0, 0, 0, 0)] * npc; ctab_out = [(NONE, 0, 0, 0)] * ncell; blob = bytearray(); st = [0, 0, 0, 0]   # куски, упрощено, вершин до, вершин после
    for ci, c in enumerate(cells):
        first, n, owner = c[6], c[7], c[8]
        ps = [P[k] for k in range(first, first + n)]
        if owner != 0 or not ps or any(not p['ok'] or p['ci'] != ci for p in ps): continue   # клетка не города или со странными кусками — только полная
        cb = bytearray()
        for p in ps:
            raw = np.frombuffer(b, np.uint8, p['ni'] * p['vs'], c[9] + p['off']).reshape(-1, p['vs'])
            simp = p['ni'] >= 36 and not (p['fl'] & 7) and not os.environ.get('FAR_IDENT')
            if simp and os.environ.get('FAR_ONLY'):   # служебно (разбор сдвига оттенка): big — только куски с ящиком шире 25 м по x или z, small — остальные
                bb_ = struct.unpack_from('<6f', b, ptab + 80 * p['i']); big_ = max(bb_[3] - bb_[0], bb_[5] - bb_[2]) > 25.0
                simp = big_ if os.environ['FAR_ONLY'] == 'big' else not big_   # прозрачные, альфа-тест, небо и мелкие куски — копией
            rows = simplify(raw, p['vs'], p['sc'], p['o'], a.mode, shared, bool(p['fl'] & 8)) if simp else raw
            if simp and rows is raw: rows = simplify(raw, p['vs'], p['sc'], p['o'], a.mode, shared, bool(p['fl'] & 8), fine=True)   # куск не уложился в потолок: вторая попытка мельче (ошибка 0,4 м), иначе полный
            while len(cb) % 16: cb.append(0)
            ptab_out[p['i']] = (len(rows), len(cb), 0 if rows is raw else 1, 0); cb += rows.tobytes()
            st[0] += 1; st[1] += rows is not raw; st[2] += p['ni']; st[3] += len(rows)
        while len(blob) % 16: blob.append(0)
        ctab_out[ci] = (len(blob), len(cb), sum(ptab_out[p['i']][0] for p in ps), 0); blob += cb
    offC = 64; offP = offC + 16 * ncell; offB = (offP + 16 * npc + 15) & ~15
    hdr = struct.pack('<4sIIIIIIIIII', b'FAR1', 1, fnv_key(b), ncell, npc, offC, offP, offB, a.mode, len(blob), len(b)).ljust(64, b'\0')
    body = b''.join(struct.pack('<4I', *r) for r in ctab_out) + b''.join(struct.pack('<4I', *r) for r in ptab_out)
    out = hdr + body; out = out.ljust(offB, b'\0') + bytes(blob)
    Path(a.out).write_bytes(out)
    print('%s: режим %d, клеток %d (с дальней версией %d), кусков %d, упрощено %d; вершины %d -> %d (%.0f %%); файл %.1f КБ (блоб %.0f КБ)' % (
        Path(a.out).name, a.mode, ncell, sum(1 for r in ctab_out if r[0] != NONE), st[0], st[1], st[2], st[3], 100.0 * st[3] / max(st[2], 1), len(out) / 1024, len(blob) / 1024))


if __name__ == '__main__': main()
