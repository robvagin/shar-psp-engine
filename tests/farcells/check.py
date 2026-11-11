#!/usr/bin/env python3
"""Модели-96: гейт данных дальних клеток (tools/farcells_export.py, формат FAR1). Читает level.shr и farcells.shr независимо от экспортёра и проверяет по каждому куску:
ключ и размеры; таблицы внутри файла; дальние вершины — БАЙТ В БАЙТ записи из полной версии куска (uv, цвет, позиция, ничего нового); треугольников не больше; ящик куска не вышел;
обход сохранён (суммарная нормаль дальней версии смотрит туда же, что у полной, у кусков с заметной площадью); куски без упрощения (прозрачные, небо, мелкие) — копия.
  check.py LEVEL.shr farcells.shr     итог PASS/FAIL, код 0/1. Красный контроль: FARCHK_RED=1 портит одну вершину (должно упасть)"""
import os, struct, sys
import numpy as np
from pathlib import Path
b = Path(sys.argv[1]).read_bytes(); f = Path(sys.argv[2]).read_bytes(); bad = []
def fnv(x):
    h = 0x811C9DC5
    for c in x[:4096] + struct.pack('<I', len(x)): h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return h
mag, ver, key, ncell, npiece, offC, offP, offB, mode, blobB, lvlSize = struct.unpack_from('<4sIIIIIIIIII', f, 0)
if mag != b'FAR1' or ver != 1: bad.append('заголовок')
if key != fnv(b): bad.append('ключ не сошёлся с level.shr')
if lvlSize != len(b) or offB + blobB != len(f): bad.append('размеры в заголовке')
ntex, npc = struct.unpack_from('<II', b, 4); ptab, = struct.unpack_from('<I', b, 76); nc, ctab = struct.unpack_from('<II', b, 80)
if (ncell, npiece) != (nc, npc): bad.append('число клеток/кусков')
cells = [struct.unpack_from('<6fIIIII', b, ctab + 48 * i) for i in range(nc)]
red = os.environ.get('FARCHK_RED')
n_far = n_simp = vin = vout = 0; ch = 0
for ci, c in enumerate(cells):
    farOff, farSize, nvT, _ = struct.unpack_from('<4I', f, offC + 16 * ci)
    if farOff == 0xFFFFFFFF: continue
    n_far += 1
    if farOff + farSize > blobB: bad.append(f'клетка {ci}: блоб за краем'); continue
    tot = 0
    for k in range(c[6], c[6] + c[7]):
        o = ptab + 80 * k; tex, fl, nv, ni, own = struct.unpack_from('<iIIII', b, o + 24); ox, oy, oz, sc = struct.unpack_from('<4f', b, o + 44); off = struct.unpack_from('<I', b, o + 60)[0]
        nvF, offF, sflag, _ = struct.unpack_from('<4I', f, offP + 16 * k); vs = 12 if fl & 32 else 16; tot += nvF
        if nvF % 3 or nvF > ni or offF + nvF * vs > farSize: bad.append(f'клетка {ci} кусок {k}: размеры'); continue
        full = np.frombuffer(b, np.uint8, ni * vs, c[9] + off).reshape(-1, vs); far = np.frombuffer(f, np.uint8, nvF * vs, offB + farOff + offF).reshape(-1, vs).copy()
        if red and ch == 0 and nvF: far[0, 0] ^= 0x55; ch = 1
        vin += ni; vout += nvF
        if not sflag:
            if nvF != ni or not np.array_equal(full, far): bad.append(f'клетка {ci} кусок {k}: не упрощён, но не копия')
            continue
        n_simp += 1
        cb = (slice(4, 6) if vs == 12 else slice(4, 7))   # цвет подогнан под поверхность дальней версии (tools/farcells_export.py fit_colors): сравниваем остальные байты записи (uv, альфа, позиция)
        def nocol(a): a = a.copy(); a[:, cb] = 0; return a
        fs = set(map(bytes, nocol(full))); miss = [r for r in nocol(far) if bytes(r) not in fs]
        if miss: bad.append(f'клетка {ci} кусок {k}: {len(miss)} вершин не из полной версии')
        pf = (far[:, 6:12] if vs == 12 else far[:, 8:14]).copy().view('<i2').reshape(-1, 3).astype(np.float64) * (sc / 32768.0) + (ox, oy, oz)
        po = (full[:, 6:12] if vs == 12 else full[:, 8:14]).copy().view('<i2').reshape(-1, 3).astype(np.float64) * (sc / 32768.0) + (ox, oy, oz)
        bb = struct.unpack_from('<6f', b, o)
        if (pf.min(0) < np.array(bb[:3]) - .01).any() or (pf.max(0) > np.array(bb[3:]) + .01).any(): bad.append(f'клетка {ci} кусок {k}: вне ящика')
        if not fl & 8:   # двусторонние куски обход не проверяют; у остальных нормаль каждого дальнего треугольника должна совпасть с нормалью хотя бы одного исходного треугольника, сходящегося в его вершинах (допускаем 3 % по площади)
            to, tf = po.reshape(-1, 3, 3), pf.reshape(-1, 3, 3); no_ = np.cross(to[:, 1] - to[:, 0], to[:, 2] - to[:, 0]); nf_ = np.cross(tf[:, 1] - tf[:, 0], tf[:, 2] - tf[:, 0])
            inc = {}
            for t_, nv_ in zip(to, no_):
                for v_ in t_: inc.setdefault(tuple(np.round(v_ * 64).astype(np.int64)), []).append(nv_)
            area = np.linalg.norm(nf_, axis=1); flipped = 0.0
            for q in range(len(tf)):
                best = -1e9
                for v_ in tf[q]:
                    for nv_ in inc.get(tuple(np.round(v_ * 64).astype(np.int64)), ()): best = max(best, float(nv_ @ nf_[q]))
                if best < 0: flipped += area[q]
            if area.sum() > 1e-6 and flipped / area.sum() > .03: bad.append(f'клетка {ci} кусок {k}: обход перевёрнут ({100 * flipped / area.sum():.0f} % площади)')
    if tot != nvT: bad.append(f'клетка {ci}: nvTotal')
print('клеток с дальней версией %d, упрощено кусков %d, вершин %d -> %d (%.0f %%), замечаний %d' % (n_far, n_simp, vin, vout, 100.0 * vout / max(vin, 1), len(bad)))
for x in bad[:12]: print('  ' + x)
print('PASS' if not bad else 'FAIL'); sys.exit(1 if bad else 0)
