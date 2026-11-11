#!/usr/bin/env python3
"""Коллизия-96, этап 2: COL1 уровня -> colring.shr (формат COLR2) для кольца коллизии в игре (viewer/colring.c).

Номера примитивов (тип << 29 | номер) НЕ меняются. Треугольники лежат страницами по TP штук в пространственном порядке (Мортон), таблица
slot[номер]; ящики, капсулы и заборы лежат подряд в прежнем порядке и ВСЕГДА резидентны (col.c читает соседство заборов по соседним номерам,
живые объёмы ANM1 переставляются на месте). Клетки сгруппированы по тайлам 16x16 (64 м): блоб тайла = loc[257] + ссылки клеток (порядок тот же).
К тайлу приложен список страниц треугольников, нужных его клеткам. Блок JMP1 и блок SRF1 (если был в секции) лежат отдельно, резидентно.
Запуск:  col_ring_build.py LEVEL.shr OUT.colring           -> печатает статистику, пишет файл (без запечённого разбора)
         col_ring_build.py LEVEL.shr --verify FILE.colring -> восстановление COL1 побайтно (массивы, cellStart, refs, списки страниц, хеши)
Формат (LE), заголовок 256 Б, потом блоки кратно 16:
  u32[0] 'COLR', 1 ver=2, 2-4 f32 ox oz cell, 5 W, 6 H, 7 ntri, 8 nbox, 9 ncyl, 10 nseg, 11 nref, 12 TL, 13 TW, 14 TH, 15 TP (треугольников на страницу),
  16 npages, 17 crc32 ядра COL1 (ключ запечённого разбора), 18 mcHash (mcProbeHash сырой секции, как считает движок), 19 secSize (размер секции COL1 как его читает движок),
  20 fingerprint (svSurfaceFingerprint), 21 f32 minTriY, 22 flags (JMP1 смещение в исходной секции, как ColHdr.flags),
  23 offSlot, 24 offSmall (box|cyl|seg), 25 offTri (страницы), 26 offTile, 27 offTilePg, 28 offBlob, 29 offJmp, 30 lenJmp, 31 offSrf, 32 lenSrf, 33 offBake, 34 lenBake,
  35 need250pg (пик страниц кольца 250 м), 36 need250blob (пик байт блобов кольца 250 м), 37 total, 38 need340pg, 39 need340blob (то же для кольца 340 м)
 Tile table: на тайл 5 u32: blobOff (от offBlob), blobBytes, nrefs, pgOff (индекс в массив страниц), npg. Страницы: u32 номер страницы.
"""
import struct, sys, zlib
import numpy as np

SZ = (48, 64, 48, 32)
TL = 16
TP = 256
HDR = 256

def load_col1(path):
    f = open(path, 'rb').read()
    off = struct.unpack_from('<I', f, 92)[0]
    pvs, prp = struct.unpack_from('<I', f, 88)[0], struct.unpack_from('<I', f, 96)[0]
    end = 0
    for v in (pvs, prp):
        if v and (not end or v < end): end = v
    if not end: end = len(f)
    _, ox, oz, cell, W, H, nt, nb, nc, ns, nr, fl = struct.unpack_from('<4s3f8I', f, off)
    p = off + 48
    arr = []
    for n, s in zip((nt, nb, nc, ns), SZ):
        arr.append(np.frombuffer(f, np.uint8, n * s, p).reshape(n, s)); p += n * s
    cs = np.frombuffer(f, '<u4', W * H + 1, p).astype(np.int64); p += (W * H + 1) * 4
    rf = np.frombuffer(f, '<u4', nr, p); p += nr * 4
    core = f[off:p]
    return dict(f=f, off=off, end=end, ox=ox, oz=oz, cell=cell, W=W, H=H, n=(nt, nb, nc, ns), nr=nr, arr=arr, cs=cs, rf=rf, core=core, flags=fl, corep=p)

def mc_probe_hash(b):
    n = len(b); h = (2166136261 ^ n) & 0xffffffff
    nw = n // 4
    w = np.frombuffer(b, '<u4', nw).tolist()
    for x in w:
        h = ((h ^ x) * 16777619) & 0xffffffff
        h ^= h >> 15
    for i in range(nw * 4, n): h = ((h ^ b[i]) * 16777619) & 0xffffffff
    return h

def fingerprint(sec, ntri):
    f = 2166136261
    for i in range(44): f = ((f ^ sec[i]) * 16777619) & 0xffffffff
    for k in range(256 if ntri else 0):
        row = k * ntri // 256; o = 48 + row * 48
        for i in range(16): f = ((f ^ sec[o + i]) * 16777619) & 0xffffffff
    return f or 1

def centers_tri(c):
    fl = np.ascontiguousarray(c['arr'][0]).view('<f4').reshape(-1, 12)
    return (fl[:, 0] + fl[:, 3] + fl[:, 6]) / 3, (fl[:, 2] + fl[:, 5] + fl[:, 8]) / 3, fl

def morton(x, z):
    x = np.clip(x, 0, 1023).astype(np.int64); z = np.clip(z, 0, 1023).astype(np.int64)
    r = np.zeros_like(x)
    for b in range(10): r |= ((x >> b) & 1) << (2 * b) | ((z >> b) & 1) << (2 * b + 1)
    return r

def build(c):
    W, H, cell = c['W'], c['H'], c['cell']
    x, z, fl = centers_tri(c)
    gx = np.floor((x - c['ox']) / cell / 4).astype(int); gz = np.floor((z - c['oz']) / cell / 4).astype(int)   # блоки по 4 клетки (16 м)
    order = np.argsort(morton(gx, gz), kind='stable')
    slot = np.empty(len(order), np.uint32); slot[order] = np.arange(len(order), dtype=np.uint32)
    TW, TH = (W + TL - 1) // TL, (H + TL - 1) // TL
    cs, rf = c['cs'], c['rf']
    blobs = []; tiletab = []; pages = []; boff = 0; tile_pg = []
    for ty in range(TH):
        for tx in range(TW):
            loc = np.zeros(TL * TL + 1, np.uint32); refs = []; li = 0
            for lz in range(TL):
                for lx in range(TL):
                    gxx, gzz = tx * TL + lx, ty * TL + lz
                    if gxx < W and gzz < H:
                        cc = gzz * W + gxx; refs.append(rf[cs[cc]:cs[cc + 1]]); n_ = len(refs[-1])
                    else: n_ = 0
                    li += 1; loc[li] = loc[li - 1] + n_
            r = np.concatenate(refs) if refs else np.zeros(0, np.uint32)
            m = (r >> 29) == 0
            pl = np.unique(slot[r[m] & 0x1fffffff] // TP) if m.any() else np.zeros(0, np.int64)
            blob = loc.astype('<u4').tobytes() + np.asarray(r, '<u4').tobytes(); blob += b'\0' * (-len(blob) % 16)
            tiletab.append((boff, len(blob), len(r), len(pages), len(pl))); pages += [int(v) for v in pl]; blobs.append(blob); boff += len(blob)
            tile_pg.append((set(int(v) for v in pl), len(blob)))
    return slot, TW, TH, tiletab, pages, blobs, tile_pg

def ring_need(c, TW, TH, tile_pg, R, step=4):
    W, H, cell = c['W'], c['H'], c['cell']; best_pg = 0; best_bl = 0
    for pz in range(0, H, step):
        for px in range(0, W, step):
            cx, cz = (px + .5) * cell, (pz + .5) * cell; s = set(); bl = 0
            for tz in range(TH):
                for tx in range(TW):
                    x0 = tx * TL * cell; x1 = x0 + TL * cell; z0 = tz * TL * cell; z1 = z0 + TL * cell
                    dx = max(x0 - cx, 0, cx - x1); dz = max(z0 - cz, 0, cz - z1)
                    if dx * dx + dz * dz <= R * R:
                        p_, b_ = tile_pg[tz * TW + tx]; s |= p_; bl += b_
            best_pg = max(best_pg, len(s)); best_bl = max(best_bl, bl)
    return best_pg, best_bl

def write(c, out, bake=b''):
    slot, TW, TH, tiletab, pages, blobs, tile_pg = build(c)
    f = c['f']; sec = f[c['off']:c['end']]
    ntri = c['n'][0]; npages = (ntri + TP - 1) // TP
    corep = c['corep'] - c['off']
    # SRF1: сразу за ссылками, со следующего кратного 16
    sp = (corep + 15) & ~15; srf = b''
    if sp + 16 <= len(sec) and sec[sp:sp + 4] == b'SRF1':
        v = struct.unpack_from('<3I', sec, sp + 4)
        if v[0] == 1 and v[1] == ntri and sp + 16 + ntri <= len(sec): srf = bytes(sec[sp:sp + 16 + ntri])
    jmp = b''
    if c['flags']: jmp = bytes(sec[c['flags']:])
    small = b''.join(bytes(np.ascontiguousarray(c['arr'][t]).tobytes()) for t in (1, 2, 3))
    tri_sorted = np.empty_like(c['arr'][0]); tri_sorted[slot] = c['arr'][0]
    tri_data = tri_sorted.tobytes() + b'\0' * ((npages * TP - ntri) * 48)
    x, z, fl = centers_tri(c)
    miny = float(np.min(fl[:, [1, 4, 7]])) if ntri else 0.0
    need_pg, need_bl = ring_need(c, TW, TH, tile_pg, 250.0)
    need_pg = int(need_pg * 1.1) + 2; need_bl = int(need_bl * 1.1)   # запас 10 %: фокус идёт между узлами сетки замера
    need340_pg, need340_bl = ring_need(c, TW, TH, tile_pg, 340.0)   # дальность рисунка 250 + загрузка клеток 75 + запас: радиус, который игра держит при drawDist 250
    need340_pg = int(need340_pg * 1.1) + 2; need340_bl = int(need340_bl * 1.1)
    body = []; offs = {}; pos = HDR
    def put(name, b):
        nonlocal pos
        padn = -pos % 16; body.append(b'\0' * padn); pos += padn
        offs[name] = pos; body.append(b); pos += len(b)
    put('slot', slot.astype('<u4').tobytes()); put('small', small); put('tri', tri_data)
    put('tile', b''.join(struct.pack('<5I', *e) for e in tiletab)); put('tilepg', np.asarray(pages, '<u4').tobytes())
    put('blob', b''.join(blobs)); put('jmp', jmp); put('srf', srf); put('bake', bake)
    padn = -pos % 16; body.append(b'\0' * padn); pos += padn
    crc = zlib.crc32(c['core']) & 0xffffffff
    mch = mc_probe_hash(bytes(sec)); fp = fingerprint(sec, ntri) if ntri else 0
    hb = bytearray(HDR)
    struct.pack_into('<4sI3f', hb, 0, b'COLR', 2, c['ox'], c['oz'], c['cell'])
    struct.pack_into('<5I', hb, 20, c['W'], c['H'], *c['n'][:3])   # W H ntri nbox ncyl
    struct.pack_into('<II', hb, 40, c['n'][3], c['nr'])   # nseg nref
    struct.pack_into('<5I', hb, 48, TL, TW, TH, TP, npages)
    struct.pack_into('<4I', hb, 68, crc, mch, len(sec), fp)
    struct.pack_into('<fI', hb, 84, miny, c['flags'])
    struct.pack_into('<7I', hb, 92, offs['slot'], offs['small'], offs['tri'], offs['tile'], offs['tilepg'], offs['blob'], offs['jmp'])
    struct.pack_into('<5I', hb, 120, len(jmp), offs['srf'], len(srf), offs['bake'], len(bake))
    struct.pack_into('<3I', hb, 140, need_pg, need_bl, pos)
    struct.pack_into('<2I', hb, 152, need340_pg, need340_bl)   # u32[38], u32[39]: пик страниц и байт блобов для кольца 340 м
    open(out, 'wb').write(bytes(hb) + b''.join(body))
    return dict(total=pos, tiles=len(tiletab), pages=npages, need_pg=need_pg, need_bl=need_bl, need340_pg=need340_pg, need340_bl=need340_bl, small=len(small), jmp=len(jmp), srf=len(srf), crc=crc, mch=mch)

def read_hdr(g):
    # раскладка write(): magic ver ox oz cell @0..16; W,H,ntri,nbox,ncyl @20..; nseg,nref @40; TL,TW,TH,TP,npages @48; crc,mch,secSize,fp @68; minY,flags @84;
    # offSlot..offJmp @92..; lenJmp,offSrf,lenSrf,offBake,lenBake @120..; need250pg,need250blob,total @140
    h = {}
    h['ver'] = struct.unpack_from('<I', g, 4)[0]
    h['ox'], h['oz'], h['cell'] = struct.unpack_from('<3f', g, 8)
    h['W'], h['H'], h['ntri'], h['nbox'], h['ncyl'] = struct.unpack_from('<5I', g, 20)
    h['nseg'], h['nref'] = struct.unpack_from('<2I', g, 40)
    h['TL'], h['TW'], h['TH'], h['TP'], h['npages'] = struct.unpack_from('<5I', g, 48)
    h['crc'], h['mch'], h['secSize'], h['fp'] = struct.unpack_from('<4I', g, 68)
    h['minY'], h['flags'] = struct.unpack_from('<fI', g, 84)
    (h['offSlot'], h['offSmall'], h['offTri'], h['offTile'], h['offTilePg'], h['offBlob'], h['offJmp']) = struct.unpack_from('<7I', g, 92)
    (h['lenJmp'], h['offSrf'], h['lenSrf'], h['offBake'], h['lenBake']) = struct.unpack_from('<5I', g, 120)
    h['need250pg'], h['need250blob'], h['total'] = struct.unpack_from('<3I', g, 140)
    h['need340pg'], h['need340blob'] = struct.unpack_from('<2I', g, 152)
    return h

def verify(c, path):
    g = open(path, 'rb').read(); h = read_hdr(g); ok = True
    if g[:4] != b'COLR' or h['ver'] != 2: print('FAIL заголовок'); return False
    ntri = h['ntri']; n = c['n']
    slot = np.frombuffer(g, '<u4', ntri, h['offSlot'])
    if len(np.unique(slot)) != ntri or (ntri and slot.max() >= ntri): print('FAIL slot не перестановка'); ok = False
    tri = np.frombuffer(g, np.uint8, h['npages'] * h['TP'] * 48, h['offTri'])[:ntri * 48].reshape(ntri, 48)
    if not np.array_equal(tri[slot], c['arr'][0]): print('FAIL треугольники'); ok = False
    small = np.frombuffer(g, np.uint8, n[1] * 64 + n[2] * 48 + n[3] * 32, h['offSmall'])
    want = b''.join(bytes(np.ascontiguousarray(c['arr'][t]).tobytes()) for t in (1, 2, 3))
    if small.tobytes() != want: print('FAIL ящики/капсулы/заборы'); ok = False
    W, H, TL_, TW = h['W'], h['H'], h['TL'], h['TW']
    tt = np.frombuffer(g, '<u4', TW * h['TH'] * 5, h['offTile']).reshape(-1, 5)
    pg_all = np.frombuffer(g, '<u4', int(tt[:, 4].sum()), h['offTilePg'])
    per = {}
    for ty in range(h['TH']):
        for tx in range(TW):
            bo, bb, nrf, po, npg = tt[ty * TW + tx]
            loc = np.frombuffer(g, '<u4', TL_ * TL_ + 1, h['offBlob'] + bo)
            r = np.frombuffer(g, '<u4', nrf, h['offBlob'] + bo + (TL_ * TL_ + 1) * 4)
            for lz in range(TL_):
                for lx in range(TL_):
                    gx, gz = tx * TL_ + lx, ty * TL_ + lz
                    if gx < W and gz < H:
                        li = lz * TL_ + lx; per[gz * W + gx] = r[loc[li]:loc[li + 1]]
            m = (r >> 29) == 0
            need = sorted(set(int(v) for v in (slot[r[m] & 0x1fffffff] // h['TP']))) if m.any() else []
            if need != list(pg_all[po:po + npg]): print('FAIL список страниц тайла', ty, tx); ok = False
    flat = np.concatenate([per[i] for i in range(W * H)])
    if not np.array_equal(flat, c['rf']): print('FAIL refs'); ok = False
    cc = np.array([len(per[i]) for i in range(W * H)]); cs = np.concatenate([[0], np.cumsum(cc)])
    if not np.array_equal(cs, c['cs']): print('FAIL cellStart'); ok = False
    if h['crc'] != (zlib.crc32(c['core']) & 0xffffffff): print('FAIL crc'); ok = False
    sec = c['f'][c['off']:c['end']]
    if h['mch'] != mc_probe_hash(bytes(sec)): print('FAIL mcHash'); ok = False
    if h['lenJmp'] and g[h['offJmp']:h['offJmp'] + h['lenJmp']] != sec[c['flags']:]: print('FAIL JMP1'); ok = False
    return ok

if __name__ == '__main__':
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(2)
    c = load_col1(a[0])
    if '--verify' in a:
        ok = verify(c, a[a.index('--verify') + 1]); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
    bake = open(a[a.index('--bake') + 1], 'rb').read() if '--bake' in a else b''
    st = write(c, a[1], bake)
    print(f"COL1 core {len(c['core'])/1e6:.3f} МБ -> {a[1]}: файл {st['total']/1e6:.3f} МБ, тайлов {st['tiles']}, страниц {st['pages']}, малые {st['small']/1e6:.3f} МБ, "
          f"JMP1 {st['jmp']} Б, SRF1 {st['srf']} Б; кольцо 250 м: страниц {st['need_pg']} ({st['need_pg']*TP*48/1e6:.2f} МБ), блобов {st['need_bl']/1e6:.2f} МБ; кольцо 340 м: страниц {st['need340_pg']} ({st['need340_pg']*TP*48/1e6:.2f} МБ), блобов {st['need340_bl']/1e6:.2f} МБ; crc {st['crc']:08x} mc {st['mch']:08x}")
