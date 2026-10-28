#!/usr/bin/env python3
"""№1061: колёса машины игрока в level.shr (SHR7, порядок секций Загрузки 2): подмена только машины, остальное побайтно.
  car_tire_splice1061.py BASE.shr OUT.shr [--report]
Что делает (BASE — готовый файл набора, текстуры колёс берёт из него же, не из диска игры):
  1. колёса = куски owner 1, вариант 0 (pad[1] == 0), сустав w0..w3 (имена из CAR1); вершины без индексов, 12 или 16 Б (V12/V16), u16 uv = uv*256, сдвиг на целые повторы у каждого треугольника;
  2. для каждого колеса psp_export.tire_prepare (то же правило, что у пакетов трафика №1061): своя вырезка с чёрной резиной, протектор в центр квадрата резины;
  3. новые текстуры дописываются в конец таблицы текстур (индексы ntex..), ntex += k; всё, что лежит дальше, сдвигается на Δ: заголовок @76 @84 @88 @92 @96 @100, REGF 'at',
     смещения блобов в таблице клеток (@36 каждой записи по 48 Б); у кусков колёс — поле tex и UV вершин на месте, число вершин и их порядок те же.
  4. проверка побайтности: OUT без вставленных текстур и без белого списка полей == BASE (tools/car_tire_check1061.py)
lmap.shr после подмены пересчитывается (ключ = хеш заголовка и размер файла), mcpr.shr не зависит (ключ — дороги трафика и сырой COL1)."""
import hashlib, os, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parent))
os.environ.setdefault('SHR_TEX_CAP256', '1'); os.environ.setdefault('SHR_SMALL_MIPS', '1'); os.environ.setdefault('SHR_LAYERS', '1'); os.environ.setdefault('SHR_T4', '1')
import psp_export as pe, texq, uvq   # noqa: E402
import carshr_probe1061 as pr   # noqa: E402

texq.CLUT4 = True   # как у экспортёра уровня (SHR_T4=1 при запуске psp_export.py)


def wheel_pieces(d, h, ps):
    car = struct.unpack_from('<8I', d, h['car']); nj = car[2]; q = h['car'] + 32 + 48
    names = [d[q + 28 * j:q + 28 * j + 12].split(b'\0')[0].decode() for j in range(nj)]
    out = []
    for p in ps:
        if p['owner'] != 1 or p['pad'][1] != 0: continue
        j = p['pad'][0]
        if 0 <= j < nj and len(names[j]) == 2 and names[j][0] == 'w' and names[j][1] in '0123': out.append((names[j], p))
    return out


def vert_view(d, h, p):
    vs = 12 if p['flags'] & 32 else 16; po = 6 if vs == 12 else 8
    cellOff = h['cellOff']; blob = struct.unpack_from('<I', d, cellOff + 48 * p['cell'] + 36)[0]
    start = blob + p['off']
    return start, vs, po


def main():
    base = bytearray(Path(sys.argv[1]).read_bytes()); out = Path(sys.argv[2]); d = bytes(base)
    h = pr.parse(d); ps = pr.pieces(d, h); ws = wheel_pieces(d, h, ps)
    cache = {}; blob = []; newtex = []   # key -> новый индекс (ntex + j)
    patches = []   # (смещение в BASE, байты)
    texpatch = {}  # индекс куска -> новый индекс текстуры
    report = []
    for nm, p in ws:
        if not (p['flags'] & 16) or p['nv'] != p['ni'] or p['nv'] % 3: report.append(f'{nm} кусок {p["i"]}: индексированный, пропуск'); continue
        start, vs, po = vert_view(d, h, p)
        a = np.frombuffer(d, np.uint8, p['nv'] * vs, start).reshape(p['nv'], vs)
        raw = a[:, :4].copy().view('<u2').reshape(-1, 2).astype(np.float64)          # (nv, 2) uv*256 (u16)
        pos = a[:, po:po + 6].copy().view('<i2').reshape(-1, 3).astype(np.float64)
        T = pos.reshape(-1, 3, 3); n = np.cross(T[:, 1] - T[:, 0], T[:, 2] - T[:, 0]); side = n[:, 0] ** 2 >= .49 * ((n * n).sum(1) + 1e-12)
        U = raw.reshape(-1, 3, 2) / 256.0
        # развёртка уже со сдвигом каждого треугольника на целые повторы (uvq.tri_i16): доли повторов
        if p['tex'] < 0 or p['tex'] >= len(h['recs']): report.append(f'{nm} кусок {p["i"]}: tex {p["tex"]} вне таблицы'); continue
        rec = h['recs'][p['tex']]; rd = d[rec[0]:rec[1]]
        lv0 = np.asarray(texq.decode(rd)[0], np.uint8)
        if not side.any(): report.append(f'{nm} кусок {p["i"]}: только протектор'); continue
        r = pe.tire_prepare(lv0, U, side)
        if r is None: report.append(f'{nm} кусок {p["i"]}: правило не применилось ({pe.TIRE_WHY[-1] if pe.TIRE_WHY else "?"})'); continue
        crop, uvi, key = r
        if key not in cache:
            tx = texq.build(Image.fromarray(crop, 'RGBA'), texq.CAP, 'kmeans', texq.CLUT4, texq.PRESWIZZLE)[0]
            if len(tx) % 16: tx += b'\0' * (-len(tx) % 16)
            cache[key] = len(newtex); newtex.append(tx)
        texpatch[p['i']] = cache[key]
        # UV на место: целый сдвиг каждого треугольника (uvq.tri_i16), биты u16 как int16
        uu, vv = uvq.tri_i16(uvi[:, 0], uvi[:, 1])
        uvb = np.stack([uu, vv], 1).astype('<i2').tobytes()
        buf = bytearray(d[start:start + p['nv'] * vs])
        for k in range(p['nv']): buf[k * vs:k * vs + 4] = uvb[k * 4:k * 4 + 4]
        patches.append((start, bytes(buf)))
        report.append(f'{nm} кусок {p["i"]}: tex {p["tex"]} -> новая #{cache[key]} ({key.split(":")[1]}), вершин {p["nv"]}')
    if not newtex:
        print('\n'.join(report)); raise SystemExit('нечего менять')
    k = len(newtex); delta = sum(len(t) for t in newtex); assert delta % 16 == 0
    ntex0 = h['ntex']
    # 1) патчи на месте (BASE-координаты)
    for off, b in patches: base[off:off + len(b)] = b
    for i, t in texpatch.items(): struct.pack_into('<i', base, h['tableOff'] + 80 * i + 24, ntex0 + t)
    # 2) сдвиг всего после таблицы текстур на delta
    texEnd = h['texEnd']
    outb = bytearray(base[:texEnd]) + b''.join(newtex) + bytes(base[texEnd:])
    struct.pack_into('<I', outb, 4, ntex0 + k)
    for hp in (76, 84, 88, 92, 96, 100):
        v = struct.unpack_from('<I', outb, hp)[0]
        if v: struct.pack_into('<I', outb, hp, v + delta)
    pvs = h['pvs']; regf = (pvs if pvs else len(base)) - 16 + delta
    if outb[regf:regf + 4] == b'REGF':
        at = struct.unpack_from('<I', outb, regf + 4)[0]; struct.pack_into('<I', outb, regf + 4, at + delta)
    cellOff = h['cellOff'] + delta
    for i in range(h['ncell']):
        p_ = cellOff + 48 * i + 36; v = struct.unpack_from('<I', outb, p_)[0]; struct.pack_into('<I', outb, p_, v + delta)
    out.write_bytes(outb)
    print('\n'.join(report)); print(f'car_tire_splice1061: новых текстур {k}, Δ {delta} Б, файл {len(base)} -> {len(outb)} Б, колёс {len(texpatch)}')


if __name__ == '__main__':
    main()
