#!/usr/bin/env python3
"""П3 (пропечка, Fable-спецификация 08.11, раздел В.1) · PVB1: список видимости PVS битами по клеткам города.
python3 tools/pvs_bits.py LEVEL.shr OUT.shr [--quiet]   (точный, version 2)   |   ... --group=G [--est]  (маска по группам, version 1)

Сегодня PVS1 (level.shr, tools/pvs.c) хранит на каждую ячейку сетки 20 м список номеров кусков (L4 2 753 824 Б, L1 2 303 734 Б).
PVB1 хранит на ту же ячейку битовую маску по клеткам города 40 м: бит клетки включён, если в списке PVS1 есть хоть один кусок этой клетки.
Движок разворачивает маску в список «все куски города этих клеток» (viewer/main_render.h pvsList), так что новая видимость — надмножество старой
(ни один кусок, видимый по PVS1, не пропадает); лишнее (куски видимых клеток, которых не было в списке) отсекает cullPiece по пирамиде и дальности, как раньше.
Одинаковые маски соседних ячеек хранятся один раз (словарь), в ячейке — u16 номер маски (0xFFFF — данных нет, как 0xFFFFFFFF в PVS1).
Файл pvb.shr рядом с уровнем (как pv2s.shr); подпись сверяется с таблицей кусков (pvs2Sig: FNV по nv и ni). PVS1 в level.shr остаётся: движок без файла читает его как раньше.
РЕЖИМ ПО УМОЛЧАНИЮ (version=2, точный): списки PVS1 без потери — одинаковые списки соседних ячеек хранятся один раз, список записан сериями подряд идущих номеров (пары «зазор, длина», LEB128). Развёртка в движке даёт ровно PVS1 (ноль лишних кусков), L4 0,55 МБ вместо 2,75.
--group=G — прежний режим version=1 (маска по группам из G кусков клетки, надмножество PVS1; нужен только для оценки цены: --est).
Формат version=2: 64 Б заголовка ('PVB1', 2, sig, npc, ncell, gw, gh, ox, oz, cell, range, nuniq, 0, 0, 0, nbytes) → u16[gw·gh] номер списка (0xFFFF — нет данных; выровнено до 4) → u32[nuniq+1] смещения в потоке → поток серий.
Формат version=1: 'PVB1', version=1, sig, npc, ncell, gw, gh, ox f32, oz f32, cell f32, range, nuniq, rowB, G (группа — G подряд идущих кусков клетки; 0 — вся клетка), ngrp (всего 64 Б) → u16[gw·gh] (выровнено до 4) → маски nuniq × rowB (выровнено до 4) → группы ngrp × {u32 первый кусок, u32 число}.
"""
import sys, struct
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import pvs_check

def piece_sig(d, toff, npc):
    h = 2166136261
    for f in (32, 36):   # nv, ni — как viewer/pvs2.h pvs2Sig
        for i in range(npc):
            v = struct.unpack_from('<I', d, toff + 80 * i + f)[0]
            h = ((h ^ v) * 16777619) & 0xffffffff
    return h

def leb(v):
    o = bytearray()
    while True:
        b = v & 0x7F; v >>= 7
        if v: o.append(b | 0x80)
        else: o.append(b); return bytes(o)

def unleb(buf, i):
    v = sh = 0
    while True:
        b = buf[i]; i += 1; v |= (b & 0x7F) << sh; sh += 7
        if not b & 0x80: return v, i

def build_rle(path, out, quiet=False):
    d = Path(path).read_bytes(); L = pvs_check.load(path); P = L['pvs']; assert P, 'в уровне нет PVS1'
    toff, ncell, coff = struct.unpack_from('<III', d, 76); npc = len(L['rec']); assert npc <= 65535
    W, H = P['W'], P['H']; idx = np.full(W * H, 0xFFFF, np.uint16); uniq = {}; streams = []
    for i, lst in enumerate(P['lists']):
        if lst is None: continue
        key = lst.astype('<u2').tobytes()
        if key not in uniq:
            l = lst.astype(np.int64); br = np.flatnonzero(np.diff(l) != 1) + 1 if len(l) else np.zeros(0, np.int64)
            st = np.concatenate([[0], br]) if len(l) else np.zeros(0, np.int64); en = np.concatenate([br, [len(l)]]) if len(l) else np.zeros(0, np.int64)
            o = bytearray(); pos = 0
            for a, b in zip(st.tolist(), en.tolist()):
                s0 = int(l[a]); o += leb(s0 - pos) + leb(b - a); pos = s0 + (b - a)
            uniq[key] = len(streams); streams.append(bytes(o)); assert len(streams) < 0xFFFF
        idx[i] = uniq[key]
    offs = np.concatenate([[0], np.cumsum([len(x) for x in streams])]).astype('<u4')
    hdr = bytearray(64)
    struct.pack_into('<4sIIIIII', hdr, 0, b'PVB1', 2, piece_sig(d, toff, npc), npc, ncell, W, H)
    struct.pack_into('<fffIIIIII', hdr, 28, P['ox'], P['oz'], P['cell'], P['range'], len(streams), 0, 0, 0, int(offs[-1]))
    body = idx.tobytes(); body += b'\0' * ((-len(body)) % 4); body += offs.tobytes() + b''.join(streams)
    blob = bytes(hdr) + body; Path(out).write_bytes(blob)
    check_file(out, L, P, L['rec']['owner'] == 0, L['rec']['ni'])
    if not quiet:
        old_sz = 32 + 8 * W * H + 2 * P['pool']
        print(f'{path}: сетка {W}x{H}, списков {len(streams)} из {sum(1 for l in P["lists"] if l is not None)}; PVS1 {old_sz} Б -> PVB1 v2 {len(blob)} Б ({100 * len(blob) / old_sz:.0f}%), точный')
    return len(blob)

def build(path, out, quiet=False, est=False, G=0):
    d = Path(path).read_bytes()
    L = pvs_check.load(path); P = L['pvs']
    assert P, 'в уровне нет PVS1'
    toff, ncell, coff = struct.unpack_from('<III', d, 76)
    npc = len(L['rec']); assert npc <= 65535, 'номера кусков не влезают в u16'
    cells = np.frombuffer(d, np.dtype([('bb', '<f4', 6), ('first', '<u4'), ('n', '<u4'), ('owner', '<u4'), ('off', '<u4'), ('size', '<u4'), ('pad', '<u4')]), ncell, coff)
    rec = L['rec']
    # куски города одной клетки лежат подряд: [first, first+n) — движок разворачивает бит именно так
    piece_cell = np.full(npc, 0xFFFFFFFF, np.uint32)
    for c in range(ncell):
        f, n = int(cells['first'][c]), int(cells['n'][c])
        assert f + n <= npc
        piece_cell[f:f + n] = c
    city = rec['owner'] == 0
    assert (piece_cell[city] != 0xFFFFFFFF).all(), 'есть куски города вне клеток'
    assert (piece_cell[city] == rec['cell'][city]).all(), 'rec.cell не совпадает с таблицей клеток'
    # группы: клетка c даёт ceil(n/G) групп подряд (G=0 — одна группа на клетку); бит группы = «хоть один её кусок виден по PVS1»
    gcount = np.array([1 if (G == 0 or int(cells['n'][c]) == 0) else -(-int(cells['n'][c]) // G) for c in range(ncell)], np.int64)
    gbase = np.concatenate([[0], np.cumsum(gcount)]).astype(np.int64); ngrp = int(gbase[-1])
    piece_grp = np.full(npc, -1, np.int64)
    for c in range(ncell):
        f, n = int(cells['first'][c]), int(cells['n'][c])
        if n: piece_grp[f:f + n] = gbase[c] + ((np.arange(n) // G) if G else 0)
    grp_first = np.zeros(ngrp, np.int64); grp_n = np.zeros(ngrp, np.int64)
    for c in range(ncell):
        f, n = int(cells['first'][c]), int(cells['n'][c])
        for k in range(int(gcount[c])):
            a = f + (k * G if G else 0); b = f + n if not G else min(f + n, a + G); grp_first[gbase[c] + k] = a; grp_n[gbase[c] + k] = max(0, b - a)
    rowB = (ngrp + 7) // 8
    W, H = P['W'], P['H']
    masks, idx = {}, np.full(W * H, 0xFFFF, np.uint16)
    uniq = []
    extra_pc = extra_tri = old_pc = old_tri = ncells_with = 0
    ni = rec['ni'].astype(np.int64)
    for i, lst in enumerate(P['lists']):
        if lst is None: continue
        cs = np.unique(piece_grp[lst])
        assert (cs >= 0).all()
        m = np.zeros(rowB, np.uint8)
        for c in cs.tolist(): m[c >> 3] |= 1 << (c & 7)
        key = m.tobytes()
        if key not in masks:
            masks[key] = len(uniq); uniq.append(key); assert len(uniq) < 0xFFFF
        idx[i] = masks[key]
        # лишнее против PVS1: куски города выбранных клеток, которых нет в списке
        full = np.concatenate([np.arange(grp_first[c], grp_first[c] + grp_n[c]) for c in cs.tolist()]) if len(cs) else np.zeros(0, np.int64)
        full = full[city[full]]
        assert np.isin(lst, full).all(), f'сетка {i}: куски PVS1 не вошли в развёртку маски'
        old_pc += len(lst); old_tri += int(ni[lst].sum()); extra_pc += len(full) - len(lst); extra_tri += int(ni[full].sum() - ni[lst].sum()); ncells_with += 1
    if est:   # кадры тест-прогона (pvs_check.bench): вызовы и треугольники на кадр с PVS1 и с PVB1 (развёртка масок) — честная цена потери отсечения внутри клетки
        exp = []
        for i, lst in enumerate(P['lists']):
            if lst is None: exp.append(None); continue
            cs = np.unique(piece_grp[lst]); full = np.concatenate([np.arange(grp_first[c], grp_first[c] + grp_n[c]) for c in cs.tolist()])
            exp.append(full[city[full]].astype(np.uint16))
        pvs_check.bench(L, {'PVS1 (сегодня)': P, 'PVB1 (биты)': dict(P, lists=exp)})
    hdr = bytearray(64)
    struct.pack_into('<4sIIIIII', hdr, 0, b'PVB1', 1, piece_sig(d, toff, npc), npc, ncell, W, H)
    struct.pack_into('<fffIIIII', hdr, 28, P['ox'], P['oz'], P['cell'], P['range'], len(uniq), rowB, G, ngrp)
    body = idx.tobytes(); body += b'\0' * ((-len(body)) % 4); body += b''.join(uniq); body += b'\0' * ((-len(body)) % 4)
    body += np.stack([grp_first, grp_n], 1).astype('<u4').tobytes()   # группы: первый кусок и число кусков (движок идёт по ним, таблицы клеток не нужно)
    blob = bytes(hdr) + body
    Path(out).write_bytes(blob)
    check_file(out, L, P, city, ni)
    if not quiet:
        old_sz = 32 + 8 * W * H + 2 * P['pool']
        print(f'{path}: сетка {W}x{H}, клеток {ncell}, групп {ngrp} (G={G}), масок {len(uniq)} (строка {rowB} Б); PVS1 {old_sz} Б -> PVB1 {len(blob)} Б ({100 * len(blob) / old_sz:.0f}%)')
        print(f'  куски на ячейку: было {old_pc / max(1, ncells_with):.0f}, лишних +{extra_pc / max(1, ncells_with):.0f} ({100 * extra_pc / max(1, old_pc):.0f}%); треугольники: лишних {100 * extra_tri / max(1, old_tri):.0f}% (отсекает cullPiece)')
    return len(blob)

def check_file(out, L, P, city, ni, drop_bit=False):
    """читает записанный файл так же, как движок (viewer/main_render.h pvbList), и сверяет с PVS1: надмножество, куски города, по возрастанию"""
    b = Path(out).read_bytes(); assert b[:4] == b'PVB1'
    ver, sig, npc, ncell, W, H = struct.unpack_from('<IIIIII', b, 4)
    if ver == 2:
        nuniq, nbytes = struct.unpack_from('<I', b, 44)[0], struct.unpack_from('<I', b, 60)[0]
        o = 64; idx = np.frombuffer(b, '<u2', W * H, o); o += W * H * 2; o += (-o) % 4
        offs = np.frombuffer(b, '<u4', nuniq + 1, o); o += (nuniq + 1) * 4; st = b[o:o + nbytes]; assert o + nbytes == len(b) and offs[-1] == nbytes, 'длина файла'
        bad = 0
        for i, lst in enumerate(P['lists']):
            if lst is None: assert idx[i] == 0xFFFF; continue
            a, e = int(offs[idx[i]]), int(offs[idx[i] + 1]); ids = []; pos = 0
            runs = []
            while a < e:
                gap, a = unleb(st, a); ln, a = unleb(st, a); s0 = pos + gap; runs.append((s0, ln)); pos = s0 + ln
            if drop_bit and runs: runs = runs[:-1]   # красный контроль: у каждого списка теряем последнюю серию
            for s0, ln in runs: ids.extend(range(s0, s0 + ln))
            if ids != lst.astype(np.int64).tolist(): bad += 1
        assert bad == 0, f'ПРОВЕРКА ПВБ v2: {bad} списков не совпали с PVS1'
        return 0
    ox, oz, cell = struct.unpack_from('<fff', b, 28); rng, nuniq, rowB, G, ngrp = struct.unpack_from('<IIIII', b, 40)
    o = 64; idx = np.frombuffer(b, '<u2', W * H, o); o += W * H * 2; o += (-o) % 4
    masks = np.frombuffer(b, np.uint8, nuniq * rowB, o).reshape(nuniq, rowB); o += nuniq * rowB; o += (-o) % 4
    grp = np.frombuffer(b, '<u4', ngrp * 2, o).reshape(ngrp, 2); assert o + ngrp * 8 == len(b), 'длина файла'
    bad = extra = 0
    for i, lst in enumerate(P['lists']):
        if lst is None: assert idx[i] == 0xFFFF; continue
        m = masks[idx[i]].copy()
        if drop_bit:   # красный контроль: у каждой ячейки гасим один включённый бит — проверка обязана это поймать
            on = np.flatnonzero(np.unpackbits(m, bitorder='little')[:ngrp]); 
            if len(on): j = int(on[0]); m[j >> 3] &= ~(1 << (j & 7)) & 0xFF
        ids = []
        for bi in range(rowB):
            v = int(m[bi])
            while v:
                lo = (v & -v).bit_length() - 1; v &= v - 1; g = bi * 8 + lo
                if g >= ngrp: break
                f, n = int(grp[g, 0]), int(grp[g, 1]); ids.extend(k for k in range(f, f + n) if city[k])
        ids = np.array(ids, np.int64)
        assert (np.diff(ids) > 0).all() if len(ids) > 1 else True, f'сетка {i}: не по возрастанию'
        miss = np.setdiff1d(lst.astype(np.int64), ids); bad += len(miss); extra += len(ids) - (len(lst) - len(miss))
    assert bad == 0, f'ПРОВЕРКА ПВБ: {bad} кусков PVS1 не видны по маскам'
    return extra

if __name__ == '__main__':
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    assert len(a) == 2, __doc__
    G = next((int(x.split('=')[1]) for x in sys.argv if x.startswith('--group=')), 0)
    if G or '--est' in sys.argv: build(a[0], a[1], '--quiet' in sys.argv, '--est' in sys.argv, G)
    else: build_rle(a[0], a[1], '--quiet' in sys.argv)
