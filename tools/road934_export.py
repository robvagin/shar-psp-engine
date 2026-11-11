#!/usr/bin/env python3
"""№934 (слово Роба 24.10 16:02: «за 200–250 м пропадают дороги и мосты»). Офлайн: дальние версии кусков дорог и мостов уровня -> road934.shr (формат RD34, viewer/road934.h).

  road934_export.py LEVEL.shr OUT.shr [--names texnames.json] [--texsrc LEVEL.shr.texsrc.json] [--block 128] [--reduce 0] [--report FILE.json]

Что берётся (всё по данным уровня; имена текстур — как в tools/roadlines_export.py):
  дорога  — непрозрачные лежачие (|ny| >= 0.5) треугольники текстур семейства ROAD_RE (rd<N>_*, asphalt*), без переходов дорога-трава и пробных;
  мост    — непрозрачные треугольники текстур bridge_* и girder (пролёты, балки, днище: любая ориентация; перила bridgerailing и бетон не берём).
Дальняя версия: только полотно и силуэт пролётов, без текстур. Вершины сварены в 2 см, сетка группы (одна текстура после подмены разметки на основной асфальт: зебра, полосы
парковки — цвет основного покрытия, иначе на 300 м светлые заплатки) сварена (по умолчанию не упрощается: fast-simplification, MIT, как в tools/traffic_export.py, даже в режиме lossless двигает край дороги — маски эталона и слоя совпадают на 0,92–0,95 против 0,998 без упрощения; ключ --reduce),
цвет вершины = средний цвет текстуры × запечённый свет вершин исходных треугольников рядом (как tools/silh_export.py), заливка без текстур.
Блоки по сетке --block (м) по центру треугольника; внутри блока треугольники по возрастанию высоты (мост рисуется после полотна под ним).
Упрощение (по умолчанию eps 0,5 м, tools/road934_simplify.py): вершины только существующие, край держит ограничитель; было 23 тыс. треугольников и 331 КБ на уровень, стало 3–4 тыс. и 40–64 КБ.
Формат RD35: 'RD35', nblocks u32, ntri u32, nverts u32, O x,y,z float, E float (32 Б) | блоки по 32 Б: рамка minx,miny,minz,maxx,maxy,maxz (float), ioff u32, ni u32 |
вершины 8 Б, общие у всех блоков (цвет RGB565 как GU_COLOR_5650 u16, x, y, z int16: (v - O) / E * 32768; E кратен 64 м) | индексы u16 (ioff — смещение от начала файла). Одна матрица модели на кадр."""
import argparse, collections, json, os, re, struct, sys
from pathlib import Path
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import roadlines_export as R
import shr7read

RD_SC = 256.0
BLOCK_DEFAULT = float(os.environ.get('ROAD934_BLOCK', '160'))   # сетка блоков, м: блок = один вызов видеочипа, отбор по рамке блока
EPS_BRIDGE_DEFAULT = float(os.environ.get('ROAD934_EPS_BRIDGE', '0.5'))
EPS_DEFAULT = float(os.environ.get('ROAD934_EPS', '0.5'))   # граница ошибки упрощения, м: дальше 270 м это меньше 0,5 px; 0 = без упрощения (прежнее)
BRIDGE_RE = re.compile(r'^(bridge_|girder)', re.I)
MARK_RE = re.compile(r'crosswalk|striped|parking', re.I)   # разметка и стоянки: цвет основного покрытия того же рода


def tex_means(b):
    ntex, npc = struct.unpack_from('<II', b, 4)
    W, H = struct.unpack_from('<II', b, 68)
    p = 104 + W * H * 8; p += -p % 16; p += W * H * 4; p += -p % 16
    out = []
    for _ in range(ntex):
        w, h, lv = struct.unpack_from('<HHH', b, p); fmt, pre = b[p + 6], b[p + 7]
        pal = np.frombuffer(b, np.uint8, 1024, p + 16).reshape(256, 4).astype(np.float64)
        q = p + 16 + 1024; bpp = 4 if fmt == 4 else 8; lw, lh = w, h; l0 = q; sz0 = None
        for li in range(lv):
            rows = (lh + 7) & ~7 if pre else lh
            rb = max(16, (lw * bpp + 7) // 8) if pre else lw * bpp // 8
            if li == 0: sz0 = rb * rows
            q += rb * rows; q += -q % 16; lw >>= 1; lh = max(1, lh >> 1)
        raw = np.frombuffer(b, np.uint8, sz0, l0)
        idx = np.concatenate([raw & 15, raw >> 4]) if bpp == 4 else raw
        hist = np.bincount(idx, minlength=256).astype(np.float64)
        wgt = hist * (pal[:, 3] >= 128)
        if wgt.sum() == 0: wgt = hist
        out.append((pal[:, :3] * wgt[:, None]).sum(0) / max(1.0, wgt.sum()))
        p = q; p += -p % 16
    return np.array(out) if out else np.zeros((0, 3))


def select(path, names_path, texsrc):
    """-> (P (N,3,3), группа цвета на треугольник, цвет группы (G,3), цвет света вершин на треугольник (N,3), класс 0 дорога/1 мост, имена)"""
    b = Path(path).read_bytes(); ntex = struct.unpack_from('<I', b, 4)[0]
    nm = R.tex_names(texsrc or (path + '.texsrc.json'), names_path, ntex)
    tm = tex_means(b)
    d = shr7read.load(path, far=False, owner=0)
    P, TEX, FL, OFF, VS = d['P'], d['TEX'], d['FL'], d['OFF'], d['VS']
    opaque = (FL & 7) == 0
    road_t = np.array([bool(R.ROAD_RE.search(x)) and not R.ROAD_NOT_RE.search(x) for x in nm])
    br_t = np.array([bool(BRIDGE_RE.search(x)) for x in nm])
    n1 = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0]); ny = np.abs(n1[:, 1]) / (np.linalg.norm(n1, axis=1) + 1e-9)
    big = np.linalg.norm(n1, axis=1) * .5 > 1e-4
    road = opaque & road_t[TEX] & (ny >= .5) & big
    bridge = opaque & br_t[TEX] & big
    sel = road | bridge
    buf = np.frombuffer(b, np.uint8)
    vc = np.zeros((len(P), 3))
    for k in range(3):
        o = OFF[sel] + k * VS[sel]; vc[sel] += buf[o[:, None] + np.arange(4, 7)[None, :]]
    vc /= 3.0
    # подмена разметки: зебра/полосы/парковка -> основное покрытие того же рода (country с country, остальное с rd2_norm / asphalt_norm уровня)
    def base_of(i):
        n = nm[i][:-4] if nm[i].endswith('.bmp') else nm[i]
        if not MARK_RE.search(n): return i
        want = ('rd2_country' if 'country' in n else 'rd2_norm' if 'brown' not in n else 'asphalt_normbrown')
        for j, x in enumerate(nm):
            if x.startswith(want + '.') or x == want: return j
        for j, x in enumerate(nm):
            if x.startswith('asphalt_norm'): return j
        return i
    gmap = np.array([base_of(i) for i in range(ntex)])
    G = gmap[TEX]
    return P[sel], G[sel], tm, vc[sel], br_t[TEX][sel].astype(np.int64), nm, int(road.sum()), int(bridge.sum())


def weld(Q, tol=0.02):
    V = Q.reshape(-1, 3); key = np.round(V / tol).astype(np.int64)
    u, first, inv = np.unique(key, axis=0, return_index=True, return_inverse=True)
    pts = V[first].astype(np.float32); F = inv.reshape(-1, 3).astype(np.int32)
    ok = (F[:, 0] != F[:, 1]) & (F[:, 1] != F[:, 2]) & (F[:, 0] != F[:, 2])
    return pts, F[ok]


def simplify(pts, F, reduce):
    """reduce < 0 — без потерь (только схлопывание ребёр с нулевой ошибкой: компланарные куски полосы 7 м), > 0 — квадрики на долю треугольников (двигает вершины, край дороги гуляет на десятки см)"""
    if reduce == 0 or len(F) < 12: return pts, F
    import fast_simplification as fs
    if reduce < 0: o, f = fs.simplify(pts, F, target_reduction=0.99, agg=3, lossless=True)
    else: o, f = fs.simplify(pts, F, target_reduction=min(0.95, reduce), agg=3)
    f = np.asarray(f, np.int32); o = np.asarray(o, np.float32)
    ok = (f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])
    return o, f[ok]


class Near:
    """ближайший исходный треугольник той же группы по центру в XZ (хеш-сетка 6 м): цвет света, к которому привязываем упрощённый"""
    def __init__(self, cen, col, cs=6.0):
        self.cs = cs; self.cen = cen; self.col = col; self.g = collections.defaultdict(list)
        for i, c in enumerate(cen): self.g[(int(c[0] // cs), int(c[2] // cs))].append(i)
    def get(self, c):
        ix, iz = int(c[0] // self.cs), int(c[2] // self.cs); best = -1; bd = 1e18
        for r in (1, 2, 4):
            for dx in range(-r, r + 1):
                for dz in range(-r, r + 1):
                    for i in self.g.get((ix + dx, iz + dz), ()):
                        e = self.cen[i] - c; dd = e[0] * e[0] + e[2] * e[2] + 0.25 * e[1] * e[1]
                        if dd < bd: bd = dd; best = i
            if best >= 0: break
        return self.col[best] if best >= 0 else np.array([128., 128., 128.])


def no_roads(path, names_path, texsrc, nm):
    """Дорог и мостов 0: громкая остановка с причиной (код 1) вместо тихого падения в пустом np.concatenate (GitHub #3).
    Причины по порядку: нет side-файла level.shr.texsrc.json; ни одно имя текстуры не нашлось (все '#N'); имена есть, но подходящих треугольников нет."""
    ts = texsrc or (str(path) + '.texsrc.json'); nres = sum(1 for x in nm if not x.startswith('#'))
    if not Path(ts).exists():
        why = (f'не найден side-файл {ts}: без него у текстур нет имён и дорогу не отличить (level.shr.texsrc.json пишет tools/psp_export.py рядом с level.shr; '
               f'шаг установки или копирования файлов не должен его терять; --texsrc указывает путь вручную)')
    elif nm and nres == 0:
        why = (f'side-файл {ts} есть, но ни одно из {len(nm)} имён текстур не нашлось в словаре {names_path or "texnames.json"}: все текстуры остались как #N '
               f'(диск распакован другим инструментом или другой версии, либо словарь собран не с этого диска; пересоберите build/roads/texnames.json)')
    else:
        why = (f'имена текстур найдены ({nres} из {len(nm)}), но ни одного непрозрачного треугольника с текстурой дороги или моста нет '
               f'(проверьте, что level.shr — настоящий уровень, а не пустой или повреждённый)')
    raise SystemExit(f'road934_export: дорог 0 и мостов 0, {path} не обработан. Причина: {why}.\n'
                     f'road934_export: no roads and no bridges found in {path}; nothing written. Likely cause: the texture-name side file ({ts}) is missing or none of the texture names matched (all #N).')


def build(path, names_path='', texsrc='', block=160.0, reduce=0.0, report=None, eps=0.0, bw=40.0, eps_br=None):
    if eps_br is None: eps_br = eps
    Pq, G, tm, vc, isbr, nm, nroad, nbr = select(path, names_path, texsrc)
    print('треугольников: дорога', nroad, 'мост', nbr)
    if nroad + nbr == 0: no_roads(path, names_path, texsrc, nm)   # GitHub #3: раньше здесь шла пустая склейка np.concatenate и падала без причины
    tri_xyz, tri_col = [], []   # упрощённые треугольники (3,3) и цвет вершин (3,3)
    stats = {}
    for cls in (0, 1):
        for g in np.unique(G[isbr == cls]):
            m = (isbr == cls) & (G == g)
            if not m.any(): continue
            pts, F = weld(Pq[m])
            n0 = len(F)
            if eps > 0:   # свой упрощатель с границей ошибки (tools/road934_simplify.py): вершины только существующие, край держится ограничителем
                import road934_simplify as S
                pts2, F2 = S.simplify(pts, F, eps_br if cls else eps, bw)
            else: pts2, F2 = simplify(pts, F, reduce)
            base = tm[g] if g < len(tm) else np.array([128., 128., 128.])
            cen0 = Pq[m].mean(1); col0 = base * (vc[m] / 255.0)
            nr = Near(cen0, col0)
            T = pts2[F2].astype(np.float64)          # (n,3,3)
            tcol = np.array([nr.get(t.mean(0)) for t in T])
            # цвет вершин: сглаживаем по общим вершинам (площадь не взвешиваем: треугольники одной группы близки по цвету)
            vsum = np.zeros((len(pts2), 3)); vcnt = np.zeros(len(pts2))
            np.add.at(vsum, F2.reshape(-1), np.repeat(tcol, 3, axis=0)); np.add.at(vcnt, F2.reshape(-1), 1)
            vcol = vsum / np.maximum(vcnt, 1)[:, None]
            tri_xyz.append(T); tri_col.append(vcol[F2])
            stats[f'{"bridge" if cls else "road"}:{nm[g]}'] = [int(m.sum()), n0, int(len(F2))]
            print(f'  {"мост" if cls else "дорога"} {nm[g]:28s} исходных {int(m.sum()):6d} сварено {n0:6d} -> {len(F2):6d}')
    if not tri_xyz: no_roads(path, names_path, texsrc, nm)
    T = np.concatenate(tri_xyz); C = np.concatenate(tri_col)
    cen = T.mean(1); ytop = cen[:, 1]
    # общая рамка уровня: вершины int16 от центра рамки O, полудиапазон E (кратен 64 м) — одна матрица модели на кадр, вершины общие у всех блоков
    lo = T.reshape(-1, 3).min(0); hi = T.reshape(-1, 3).max(0); O = (lo + hi) * .5
    E = float(np.ceil(max((hi - lo).max() * .5 + 1.0, 64.0) / 64.0) * 64.0)
    O = O.astype(np.float32).astype(np.float64)
    bi = np.floor(cen[:, 0] / block).astype(np.int64); bj = np.floor(cen[:, 2] / block).astype(np.int64)
    vkey = {}; VX = []; VC = []; I = bytearray(); tab = []; nitot = 0
    for (kx, kz) in sorted(set(zip(bi.tolist(), bj.tolist()))):
        ix = np.nonzero((bi == kx) & (bj == kz))[0]
        ix = ix[np.argsort(ytop[ix], kind='stable')]
        X = T[ix].reshape(-1, 3); CC = C[ix].reshape(-1, 3)
        q = np.round((X - O) / E * 32768.0)
        assert np.abs(q).max() <= 32767, 'вершина вне диапазона int16'
        q = q.astype(np.int64); cc = np.clip(np.round(CC), 0, 255).astype(np.int64)
        c565 = ((cc[:, 0] >> 3) | ((cc[:, 1] >> 2) << 5) | ((cc[:, 2] >> 3) << 11)).astype(np.int64)   # GU_COLOR_5650: R в младших 5 битах
        idx = np.empty(len(q), np.int64)
        for k in range(len(q)):
            key = (int(q[k, 0]), int(q[k, 1]), int(q[k, 2]), int(c565[k]))
            j = vkey.get(key)
            if j is None: j = len(VX); vkey[key] = j; VX.append(key)
            idx[k] = j
        bmin = X.min(0); bmax = X.max(0)
        I += idx.astype('<u2').tobytes()
        tab.append((float(bmin[0]), float(bmin[1]), float(bmin[2]), float(bmax[0]), float(bmax[1]), float(bmax[2]), nitot, len(idx))); nitot += len(idx)
    nb = len(tab); nv = len(VX); assert nv < 65536, 'вершин больше 65535'
    voff = 32 + 32 * nb; voff += -voff % 16; ioff = voff + 8 * nv; ioff += -ioff % 4
    out = bytearray(struct.pack('<4sIII4f', b'RD35', nb, len(T), nv, float(O[0]), float(O[1]), float(O[2]), E))
    for (x0, y0, z0, x1, y1, z1, i0, ni) in tab: out += struct.pack('<6f2I', x0, y0, z0, x1, y1, z1, ioff + i0 * 2, ni)
    out += b'\0' * (voff - len(out))
    for (qx, qy, qz, c) in VX: out += struct.pack('<H3h', c, qx, qy, qz)
    out += b'\0' * (ioff - len(out)); out += I
    rep = dict(blocks=nb, tris=int(len(T)), verts=nv, bytes=len(out), groups=stats, road_in=nroad, bridge_in=nbr, reduce=reduce, eps=eps, block=block, E=E)
    print(f'блоков {nb}, треугольников {len(T)}, вершин {nv}, байт {len(out)}, E {E:.0f} м')
    return bytes(out), rep


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('level'); ap.add_argument('out')
    ap.add_argument('--names', default=os.environ.get('ROAD934_NAMES', '')); ap.add_argument('--texsrc', default='')
    ap.add_argument('--block', type=float, default=BLOCK_DEFAULT); ap.add_argument('--reduce', type=float, default=0.0, help='0 не упрощать (по умолчанию: упрощение fast-simplification двигает край дороги, IoU масок 0,92–0,95 против 0,998), <0 «без потерь» (тоже двигает), >0 доля треугольников для квадрик')
    ap.add_argument('--eps', type=float, default=EPS_DEFAULT, help='>0 — упрощение tools/road934_simplify.py с границей ошибки eps (м); 0 — не упрощать (прежнее, ROAD934_EPS=0): 23 тыс. треугольников и до 331 КБ на уровень'); ap.add_argument('--bw', type=float, default=40.0); ap.add_argument('--eps-bridge', type=float, default=EPS_BRIDGE_DEFAULT, help='граница ошибки для мостов и пролётов (м): ферма на 300 м и дальше — силуэт, мелочь не нужна')
    ap.add_argument('--report', default='')
    a = ap.parse_args()
    names = a.names or next((p for p in (str(Path(__file__).resolve().parents[1] / 'build' / 'roads' / 'texnames.json'), 'build/roads/texnames.json', '/Users/USER/Downloads/shar-psp-roads-w/build/roads/texnames.json') if Path(p).exists()), '')
    if not names:   # словарь sha256 -> имена текстур диска собирается один раз из .p3d диска (~15 с) и кладётся в build/roads/texnames.json (в git не идёт)
        import notoon_list as N
        names = str(Path(__file__).resolve().parents[1] / 'build' / 'roads' / 'texnames.json'); Path(names).parent.mkdir(parents=True, exist_ok=True)
        N.names_by_digest(names)
    if not names or not Path(names).exists(): raise SystemExit('нет texnames.json (--names или ROAD934_NAMES): без имён текстур дорогу не отличить')
    data, rep = build(a.level, names, a.texsrc, a.block, a.reduce, eps=a.eps, bw=a.bw, eps_br=a.eps_bridge if a.eps > 0 else None)
    Path(a.out).write_bytes(data)
    if a.report: Path(a.report).write_text(json.dumps(rep, ensure_ascii=False, indent=1))


if __name__ == '__main__':
    main()
