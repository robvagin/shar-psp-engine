#!/usr/bin/env python3
"""Коллизия-96, этап 1: COL1 уровня -> colring.shr (формат COLR1, страницы по пространственному порядку).

Номера примитивов (тип << 29 | номер) НЕ меняются: ссылки клеток остаются с прежними номерами, добавлена таблица slot[номер] -> место в
пространственном порядке (Мортон по тайлам), по месту примитивы уложены страницами по PAGE байт. Клетки лежат по тайлам 16x16 (64 м):
блоб тайла = cs[257] локальных смещений + ссылки (порядок ссылок внутри клетки прежний, итерация col.c та же). К тайлу приложен список страниц,
которые нужны всем его клеткам (префетч кольца).
Запуск:  col_ring_build.py LEVEL.shr OUT.colring [--page 16384]   -> печатает статистику, пишет файл
         col_ring_build.py LEVEL.shr --verify FILE.colring        -> восстановление COL1 из файла побайтно (массивы, cellStart, refs)
Формат (LE): 0 'COLR', u32 ver=1, f32 ox oz cell, u32 W H ntri nbox ncyl nseg nref, u32 TL page TW TH, u32 crc32(core COL1),
 u32 offSlot[4], offData[4], offTile, offPages, offBlob, total  (всё от начала файла, кратно 16); дальше таблицы.
 Tile table: TW*TH записей u32 blobOff(от offBlob), nrefs, pagesOff(индекс в массив страниц), npages. Страницы: u32 (type<<24 | page).
"""
import struct, sys, zlib
import numpy as np

SZ = (48, 64, 48, 32)
HFMT = '<4sI3f2I5I4II4I4I4I'   # 128 Б с добавкой
TL = 16

def load_col1(path):
    f = open(path, 'rb').read()
    off = struct.unpack_from('<I', f, 92)[0]
    _, ox, oz, cell, W, H, nt, nb, nc, ns, nr, fl = struct.unpack_from('<4s3f8I', f, off)
    p = off + 48
    arr = []
    for n, s in zip((nt, nb, nc, ns), SZ):
        arr.append(np.frombuffer(f, np.uint8, n * s, p).reshape(n, s)); p += n * s
    cs = np.frombuffer(f, '<u4', W * H + 1, p).astype(np.int64); p += (W * H + 1) * 4
    rf = np.frombuffer(f, '<u4', nr, p)
    core = f[off:p + nr * 4]
    return dict(f=f, off=off, ox=ox, oz=oz, cell=cell, W=W, H=H, n=(nt, nb, nc, ns), nr=nr, arr=arr, cs=cs, rf=rf, core=core)

def centers(c):
    out = []
    for t in range(4):
        a = c['arr'][t]; n = len(a)
        fl = a.view('<f4') if (a.shape[1] % 4 == 0) else None
        fl = np.ascontiguousarray(a).view('<f4').reshape(n, -1)
        if t == 0: x = (fl[:, 0] + fl[:, 3] + fl[:, 6]) / 3; z = (fl[:, 2] + fl[:, 5] + fl[:, 8]) / 3
        elif t == 1 or t == 2: x = fl[:, 0]; z = fl[:, 2]
        else: x = (fl[:, 0] + fl[:, 2]) / 2; z = (fl[:, 1] + fl[:, 3]) / 2
        out.append((x, z))
    return out

def morton(x, z):
    x = np.clip(x, 0, 1023).astype(np.int64); z = np.clip(z, 0, 1023).astype(np.int64)
    r = np.zeros_like(x)
    for b in range(10): r |= ((x >> b) & 1) << (2 * b) | ((z >> b) & 1) << (2 * b + 1)
    return r

def build(c, page):
    W, H, cell = c['W'], c['H'], c['cell']
    slots = []
    for t, (x, z) in enumerate(centers(c)):
        if t > 0:   # ящики, капсулы, заборы остаются резидентными и в прежнем порядке: col.c читает соседство заборов по соседним номерам (fenceFlags), живые объёмы ANM1 переставляются на месте
            slots.append(np.arange(len(x), dtype=np.uint32)); continue
        gx = np.floor((x - c['ox']) / cell / 4).astype(int); gz = np.floor((z - c['oz']) / cell / 4).astype(int)   # блоки по 4 клетки (16 м)
        order = np.argsort(morton(gx, gz), kind='stable')
        s = np.empty(len(order), np.uint32); s[order] = np.arange(len(order), dtype=np.uint32)
        slots.append(s)
    TW, TH = (W + TL - 1) // TL, (H + TL - 1) // TL
    cs, rf = c['cs'], c['rf']
    cnt = np.diff(cs)
    blobs = []; tiletab = []; pages = []; boff = 0
    for ty in range(TH):
        for tx in range(TW):
            cells = [gz * W + gx for gz in range(ty * TL, min((ty + 1) * TL, H)) for gx in range(tx * TL, min((tx + 1) * TL, W))]
            # локальный порядок: строки по TL клеток; клетки вне карты (край) пустые
            loc = np.zeros(TL * TL + 1, np.uint32); refs = []
            li = 0
            for lz in range(TL):
                for lx in range(TL):
                    gx, gz = tx * TL + lx, ty * TL + lz
                    if gx < W and gz < H:
                        cc = gz * W + gx; refs.append(rf[cs[cc]:cs[cc + 1]])
                    li += 1; loc[li] = loc[li - 1] + (len(refs[-1]) if gx < W and gz < H else 0)
            r = np.concatenate(refs) if refs else np.zeros(0, np.uint32)
            need = set()
            for t in range(4):
                m = (r >> 29) == t
                if not m.any(): continue
                ids = np.unique(r[m] & 0x1fffffff); sl = slots[t][ids].astype(np.int64)
                lo = (sl * SZ[t]) // page; hi = (sl * SZ[t] + SZ[t] - 1) // page
                for pg in np.unique(np.concatenate([lo, hi])): need.add((t << 24) | int(pg))
            pl = sorted(need)
            blob = loc.astype('<u4').tobytes() + np.asarray(r, '<u4').tobytes()
            blob += b'\0' * (-len(blob) % 16)
            tiletab.append((boff, len(r), len(pages), len(pl))); pages += pl; blobs.append(blob); boff += len(blob)
    return slots, TW, TH, tiletab, pages, blobs

def write(c, page, out):
    slots, TW, TH, tiletab, pages, blobs = build(c, page)
    body = []; offs = {}
    pos = 128
    def put(name, b):
        nonlocal pos
        pad = -pos % 16; body.append(b'\0' * pad); pos += pad
        offs[name] = pos; body.append(b); pos += len(b)
    for t in range(4): put(('slot', t), slots[t].astype('<u4').tobytes())
    for t in range(4):
        data = np.empty_like(c['arr'][t]); data[slots[t]] = c['arr'][t]   # slot-ordered
        put(('data', t), data.tobytes())
    put('tile', b''.join(struct.pack('<4I', *e) for e in tiletab))
    put('pages', np.asarray(pages, '<u4').tobytes())
    put('blob', b''.join(blobs))
    pad = -pos % 16; body.append(b'\0' * pad); pos += pad
    crc = zlib.crc32(c['core']) & 0xffffffff
    head = struct.pack(HFMT, b'COLR', 1, c['ox'], c['oz'], c['cell'], c['W'], c['H'], *c['n'], c['nr'], TL, page, TW, TH, crc,
        *[offs[('slot', t)] for t in range(4)], *[offs[('data', t)] for t in range(4)], offs['tile'], offs['pages'], offs['blob'], pos)
    head += b'\0' * (128 - len(head)); assert len(head) == 128, len(head)
    open(out, 'wb').write(head + b''.join(body))
    return slots, tiletab, pages, pos

def read_back(path):
    g = open(path, 'rb').read()
    assert g[:4] == b'COLR'
    v = struct.unpack_from(HFMT, g, 0)
    _, ver, ox, oz, cell, W, H, nt, nb, nc, ns, nr, TLc, page, TW, TH, crc = v[:17]
    o = v[17:]
    return g, dict(ox=ox, oz=oz, cell=cell, W=W, H=H, n=(nt, nb, nc, ns), nr=nr, TL=TLc, page=page, TW=TW, TH=TH, crc=crc, slot=o[0:4], data=o[4:8], tile=o[8], pages=o[9], blob=o[10], total=o[11])

def verify(c, path):
    g, h = read_back(path)
    ok = True
    for t in range(4):
        n = h['n'][t]
        s = np.frombuffer(g, '<u4', n, h['slot'][t])
        d = np.frombuffer(g, np.uint8, n * SZ[t], h['data'][t]).reshape(n, SZ[t])
        if not np.array_equal(d[s], c['arr'][t]): print('FAIL данные типа', t); ok = False
        if len(np.unique(s)) != n or s.max(initial=0) >= max(n, 1): print('FAIL slot не перестановка', t); ok = False
    W, H, TL_, TW = h['W'], h['H'], h['TL'], h['TW']
    tt = np.frombuffer(g, '<u4', TW * h['TH'] * 4, h['tile']).reshape(-1, 4)
    cs = np.zeros(W * H + 1, np.int64); rf = []
    cnt = np.zeros(W * H, np.int64); per = {}
    for ty in range(h['TH']):
        for tx in range(TW):
            bo, nrf, po, npg = tt[ty * TW + tx]
            loc = np.frombuffer(g, '<u4', TL_ * TL_ + 1, h['blob'] + bo)
            r = np.frombuffer(g, '<u4', nrf, h['blob'] + bo + (TL_ * TL_ + 1) * 4)
            for lz in range(TL_):
                for lx in range(TL_):
                    gx, gz = tx * TL_ + lx, ty * TL_ + lz
                    if gx < W and gz < H:
                        li = lz * TL_ + lx; per[gz * W + gx] = r[loc[li]:loc[li + 1]]
    # списки страниц тайлов пересчитать из блобов и slot: должны совпасть с записанными
    pg_all = np.frombuffer(g, '<u4', int(tt[:, 3].sum()), h['pages'])
    sl = [np.frombuffer(g, '<u4', h['n'][t], h['slot'][t]).astype(np.int64) for t in range(4)]
    for ti in range(len(tt)):
        bo, nrf, po, npg = tt[ti]
        r = np.frombuffer(g, '<u4', nrf, h['blob'] + bo + (TL_ * TL_ + 1) * 4)
        need = set()
        for t in range(4):
            m = (r >> 29) == t
            if not m.any(): continue
            s_ = sl[t][np.unique(r[m] & 0x1fffffff)]
            for pg in np.unique(np.concatenate([(s_ * SZ[t]) // h['page'], (s_ * SZ[t] + SZ[t] - 1) // h['page']])): need.add((t << 24) | int(pg))
        if sorted(need) != list(pg_all[po:po + npg]): print('FAIL список страниц тайла', ti); ok = False; break
    flat = np.concatenate([per[i] for i in range(W * H)]) if W * H else np.zeros(0, np.uint32)
    if not np.array_equal(flat, c['rf']): print('FAIL refs'); ok = False
    cc = np.array([len(per[i]) for i in range(W * H)]); cs = np.concatenate([[0], np.cumsum(cc)])
    if not np.array_equal(cs, c['cs']): print('FAIL cellStart'); ok = False
    if h['crc'] != (zlib.crc32(c['core']) & 0xffffffff): print('FAIL crc'); ok = False
    return ok

if __name__ == '__main__':
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(2)
    c = load_col1(a[0])
    if '--verify' in a:
        ok = verify(c, a[a.index('--verify') + 1]); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
    page = int(a[a.index('--page') + 1]) if '--page' in a else 16384
    out = a[1]
    slots, tiletab, pages, total = write(c, page, out)
    core = len(c['core'])
    print(f'COL1 core {core/1e6:.3f} МБ -> {out}: файл {total/1e6:.3f} МБ, тайлов {len(tiletab)}, страниц в списках {len(pages)}')
