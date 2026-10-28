#!/usr/bin/env python3
"""№1007 «Стыки зон разного тона»: пары соседних заливок земли, задуманных одним фоном (асфальт, парковка, площадка), но разошедшихся по итоговому тону после цветокоррекции мультика.

  python3 tools/zones1007_list.py level.shr flat912.bin FINALS.txt [выход zones1007.bin] [--report]

FINALS.txt — строки FLATT и ZONES1007SET из smoke.txt движка («flag flatdump 1; flag zones1007 0; flag flat912 0; flag flat912 1» на этом уровне): итоговый тон заливки каждой текстуры (tint) и средний тон асфальта уровня (asph).
Геометрия — как её читает GE (tools/shr7read.py): горизонтальные треугольники земли сэмплируются в сетку 1 м (верхний слой), у каждой клетки своя текстура; граница между текстурами — пары соседних клеток.
Заливка Z (земля, заливка, не газон/бордюр/уголок) берёт соседа-асфальт, если граница с асфальтом ≥ ASPH_SHARE доли её границы и длиной ≥ MIN_LEN м, а итоговый тон отличается от асфальта на ΔE76 > DE_MIN.
Цель — самый длинный сосед-асфальт: заливка (номер текстуры), если она есть, иначе средний тон асфальта (0xFFFF). Доля сдвига такая, чтобы ΔE после ≤ DE_TARGET (рисунок, полосы разметки и шум не трогаются: меняется только цвет заливки).
Формат ZN07: 'ZN07', n, подпись (как у flat912.bin), n × 8 байт {tex u16, цель u16, доля 0..255, 0, 0, 0}."""
import math, re, struct, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import flat_list, shr7read

ASPH_SHARE, MIN_LEN, DE_MIN, DE_TARGET = 0.2, 8, 20.0, 6.0
MIN_AREA, MIN_HALFW = 120, 2.5
GRID = 1.0

def lab(rgb):
    c = np.asarray(rgb, float) / 255.0
    c = np.where(c > .04045, ((c + .055) / 1.055) ** 2.4, c / 12.92)
    x = (c @ [.4124, .3576, .1805]) / .95047; y = c @ [.2126, .7152, .0722]; z = (c @ [.0193, .1192, .9505]) / 1.08883
    f = lambda t: np.where(t > .008856, np.cbrt(t), 7.787 * t + 16 / 116)
    return np.array([116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z))])
def de(a, b): return float(np.linalg.norm(lab(a) - lab(b)))

drawn, dinfo = {}, {}
def parse_finals(p):
    tint, asph = {}, None; global drawn, dinfo; drawn, dinfo = {}, {}
    for l in open(p, errors='ignore'):
        m = re.match(r'FLATT (\d+) .* tint=(\d+),(\d+),(\d+)', l)
        if m: tint[int(m[1])] = (int(m[2]), int(m[3]), int(m[4]))
        m = re.match(r'FLATD (\d+) .* g=(\d) asph=(\d+) curb=(\d+) .* toned=(\d+),(\d+),(\d+)', l)
        if m and m[2] == '1': drawn[int(m[1])] = (int(m[5]), int(m[6]), int(m[7])); dinfo[int(m[1])] = (int(m[3]), int(m[4]))
        m = re.match(r'ZONES1007SET .* asph=(\d+),(\d+),(\d+)\((\d)\)', l)
        if m and m[4] == '1': asph = (int(m[1]), int(m[2]), int(m[3]))
    return tint, asph

def grid_of(level, ground_tex):
    d = shr7read.load(level)
    P, T = d['P'], d['TEX']
    n = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); ar = np.linalg.norm(n, axis=1) / 2
    up = np.abs(n[:, 1]) > .5 * np.linalg.norm(n, axis=1) + 1e-9
    sel = up & np.isin(T, list(ground_tex)) & (ar > 1e-6)
    PIk = d['PI'][sel]; P, T, ar = P[sel], T[sel], ar[sel]
    rng = np.random.default_rng(1007)
    xs, zs, ys, ts, ps = [], [], [], [], []
    for i in range(len(P)):
        k = int(min(400, max(1, math.ceil(ar[i] / (GRID * GRID * .5)))))
        r1, r2 = rng.random(k), rng.random(k); f = r1 + r2 > 1; r1[f] = 1 - r1[f]; r2[f] = 1 - r2[f]
        q = P[i, 0][None] + r1[:, None] * (P[i, 1] - P[i, 0])[None] + r2[:, None] * (P[i, 2] - P[i, 0])[None]
        xs.append(q[:, 0]); ys.append(q[:, 1]); zs.append(q[:, 2]); ts.append(np.full(k, T[i])); ps.append(np.full(k, PIk[i]))
    x, y, z, t, pp = (np.concatenate(a) for a in (xs, ys, zs, ts, ps))
    gx = np.floor(x / GRID).astype(np.int64) + 100000; gz = np.floor(z / GRID).astype(np.int64) + 100000
    order = np.lexsort((y, gz, gx))   # внутри клетки по возрастанию высоты; берём последний (верхний слой)
    gx, gz, t, pp = gx[order], gz[order], t[order], pp[order]
    key = gx * 1000003 + gz; last = np.r_[key[1:] != key[:-1], True]
    return dict(zip(key[last].tolist(), t[last].tolist())), dict(zip(key[last].tolist(), pp[last].tolist()))

def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]; report = '--report' in sys.argv
    level, flat, finals = a[0], a[1], a[2]; out = a[3] if len(a) > 3 else None
    f, tex, sig = flat_list.load_level(level)
    fb = Path(flat).read_bytes(); assert fb[:4] == b'FLT1'
    ntex = struct.unpack_from('<I', fb, 4)[0]; fsig = struct.unpack_from('<I', fb, 8)[0]
    assert fsig == sig, 'подпись flat912.bin не от этого level.shr'
    rows = [fb[12 + 8 * i:12 + 8 * i + 8] for i in range(ntex)]
    tint, asph = parse_finals(finals)
    isfill = lambda t: t in tint and rows[t][0] == 1
    gnd = {t for t in range(ntex) if tex[t]['ground']}
    cand = {t for t in gnd if t in tint}
    asphlike = lambda t: (rows[t][7] >> 4) != 0 or (t in tint and asph is not None and de(tint[t], asph) < 8)
    ground_tex = {t for t in gnd if (t in tint) or (rows[t][7] >> 4) != 0 or rows[t][0] == 0}
    cells, cellp = grid_of(level, ground_tex)
    # сглаживание: мелкая крапа (верхние слои, перекрытия кусков) мешает связности; текстура клетки — большинство в окне 3×3
    ks_ = np.fromiter(cells.keys(), np.int64, len(cells)); gz_ = ks_ % 1000003; gx_ = ks_ // 1000003
    x0_, z0_ = gx_.min() - 2, gz_.min() - 2; W_, H_ = int(gx_.max() - x0_ + 3), int(gz_.max() - z0_ + 3)
    G = np.full((W_, H_), -1, np.int16); G[(gx_ - x0_).astype(int), (gz_ - z0_).astype(int)] = np.fromiter(cells.values(), np.int16, len(cells))
    stack = np.stack([np.roll(np.roll(G, i, 0), j, 1) for i in (-1, 0, 1) for j in (-1, 0, 1)])
    cnt = np.stack([(stack == stack[n]).sum(0) for n in range(9)]); best = cnt.argmax(0)
    M = np.take_along_axis(stack, best[None], 0)[0]; M = np.where(G >= 0, M, -1)
    sm = {int(k): int(M[int(x) - x0_ if False else int(x - x0_), int(z - z0_)]) for k, x, z in zip(ks_, gx_, gz_)}
    cells = {k: (t if sm[k] < 0 else sm[k]) for k, t in cells.items()}
    pair = {}
    for k, t in cells.items():
        for dk in (1000003, 1):
            u = cells.get(k + dk)
            if u is not None and u != t: pair[(t, u)] = pair.get((t, u), 0) + 1; pair[(u, t)] = pair.get((u, t), 0) + 1
    if '--map' in sys.argv:   # --map X0 Z0 X1 Z1 PNG: ложные цвета итогового тона земли (заливка — tint, рисунок — тон после цветокоррекции), подписи номерами
        from PIL import Image, ImageDraw
        i = sys.argv.index('--map'); x0, z0, x1, z1 = (float(v) for v in sys.argv[i + 1:i + 5]); png = sys.argv[i + 5]; S = 6
        im = Image.new('RGB', (int((x1 - x0) / GRID) * S, int((z1 - z0) / GRID) * S), (0, 0, 0)); dr = ImageDraw.Draw(im); lab_ = {}
        for k, t in cells.items():
            gz = (k % 1000003) - 100000; gx = (k // 1000003) - 100000
            if not (x0 <= gx * GRID < x1 and z0 <= gz * GRID < z1): continue
            col = tint.get(t) or drawn.get(t) or (255, 0, 255); px, py = int((gx * GRID - x0) / GRID) * S, int((gz * GRID - z0) / GRID) * S
            dr.rectangle([px, py, px + S - 1, py + S - 1], fill=col); lab_.setdefault(t, []).append((px, py))
        for t, v in lab_.items():
            if len(v) > 12: dr.text(v[len(v) // 2], str(t), fill=(255, 255, 0))
        im.save(png); print('карта', png, {t: ('fill' if t in tint else 'drawn') for t in lab_})
        return
    if "--debug" in sys.argv: print("dbg cells", len(cells), "pieces", len(set(cellp.values())), "tint", len(tint), "drawn", len(drawn), "asph", asph)
    res = []; WALK = (168, 157, 185); MIX = 0.6
    asph_drawn = lambda u: u in drawn and (rows[u][7] >> 4) != 0
    byp = {}
    for k, pcs in cellp.items(): byp.setdefault(pcs, []).append(k)
    texcells = {}
    for k, t in cells.items(): texcells.setdefault(t, set()).add(k)
    for Z in sorted(cand):
        r = rows[Z]
        if r[0] != 1 or Z not in tint or (r[7] & 15) in (2, 3) or r[4] in (0x99, 0x87): continue   # не заливка земли / уголок / переход №999
        c = tint[Z]
        if c[1] > c[0] + 12 and c[1] > c[2] + 12: continue    # газон остаётся зелёным
        if de(c, WALK) < 8: continue                            # холодный серый тротуар №985 — слово Роба
        d0 = de(c, asph)
        if '--debug' in sys.argv: print('  cand', Z, c, 'dE asph', round(d0, 1), 'dE walk', round(de(c, WALK), 1), 'cells', len(texcells.get(Z, ())))
        if d0 <= DE_MIN: continue
        ratio = [((c[i] + (asph[i] - c[i]) * MIX) / max(c[i], 1)) for i in range(3)]; mx = max(max(ratio), 1.0)
        mul = [int(round(min(1.0, x / mx) * 255)) for x in ratio]
        S = texcells.get(Z, set()); seen = set()
        for k0 in S:
            if k0 in seen: continue
            comp = []; st = [k0]; seen.add(k0)
            while st:
                k = st.pop(); comp.append(k)
                for dk in (1000003, -1000003, 1, -1):
                    u = k + dk
                    if u in S and u not in seen: seen.add(u); st.append(u)
            area = len(comp)
            if area < MIN_AREA: continue
            cs = set(comp); bnd = 0; asp = 0
            for k in comp:
                for dk in (1000003, -1000003, 1, -1):
                    u = k + dk
                    if u in cs: continue
                    bnd += 1; t2 = cells.get(u)
                    if t2 is not None and asph_drawn(t2): asp += 1
            if '--debug' in sys.argv: print('  dbg tex', Z, 'comp area', area, 'bnd', bnd, 'asp', asp, 'halfw', round(area / max(bnd, 1), 2))
            if bnd == 0 or area / bnd < MIN_HALFW: continue         # узкая лента (бордюр): не площадка
            if asp < MIN_LEN or asp / bnd < ASPH_SHARE: continue
            pcsets = {}
            for k in comp: pcsets[cellp[k]] = pcsets.get(cellp[k], 0) + 1
            if '--debug' in sys.argv: print('    pieces', {int(p_): (n_, len(byp[p_])) for p_, n_ in pcsets.items()})
            pcs_ok = sorted(p_ for p_, n_ in pcsets.items() if n_ >= .6 * len(byp[p_]))   # кусок — в площадке, если не меньше 60 % его клеток внутри
            cx = sum((k // 1000003) - 100000 for k in comp) / area * GRID; cz = sum((k % 1000003) - 100000 for k in comp) / area * GRID
            print(f'  площадка tex {Z} центр {cx:.1f} {cz:.1f} площадь {area}')
            for p_ in pcs_ok: res.append((int(p_), mul, Z, d0, asp / bnd, area, c, asph))
    for pcs, mul, Z, d0, sh, area, c, tc in res:
        print(f'  кусок {pcs:5d} tex {Z:3d} площадь {area:5d} м² граница с асфальтом {sh:.0%} ΔE {d0:5.1f} тон {c} -> множитель {tuple(mul)}')
    print(f'{Path(level).parent.name}: кусков-площадок {len(res)}')
    if out:
        npc = struct.unpack_from('<I', f, 8)[0]
        b = struct.pack('<4sIII', b'ZP07', npc, sig, len(res)) + b''.join(struct.pack('<IBBBB', pcs, mul[0], mul[1], mul[2], 0) for pcs, mul, *_ in res)
        Path(out).write_bytes(b)

main()
