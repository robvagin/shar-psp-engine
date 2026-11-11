#!/usr/bin/env python3
"""Типичные точки обзора по 7 уровням (Данные-93): 6-10 на уровень для стенда картинок Пропечки и «дальних клеток» Моделей-96.
Оси порта (мировые x,y,z, как у goto/cam смоука, без отзеркаливания); y вверх, метры.
Источники: опись механики (старты и цели миссий, docs/state/mechanic_inventory.json), SHR7 уровня (карта опоры и стены):
высоты, мосты (второй слой опоры), стены на высоте глаза. Для каждой точки направление = самый длинный свободный луч
(стены на высоте глаза и склоны рельефа его обрывают); free_m - длина этого луча, м.
  viewpoints_93.py [--set DIR] [--out docs/state/viewpoints_93.json]
Выход: json {axes, levels:{N:{map, points:[{id, kind, name, eye, fwd, yaw_deg, fov, free_m, ground_y, src, cam, goto}]}}}.
cam - готовая строка сценария смоука: cam x y z fx fy fz fov (после goto x z 0 на том же месте)."""
import argparse, json, math, struct
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SET = Path.home() / 'Downloads/shar-psp-t93/build/test96/PSP/GAME/SHARTEST15'
INV = ROOT / 'docs/state/mechanic_inventory.json'
EYE_H = 2.8          # глаз над опорой, м (камера за машиной ~ так)
MAXRUN = 400.0       # дальше не меряем
NDIR = 24
KIND_RU = {'spawn': 'старт уровня', 'mission_start': 'старт миссии', 'mission_target': 'цель миссии', 'hill': 'холм, вид вдаль',
           'bridge': 'мост', 'street': 'главная улица, вид вдоль', 'open': 'открытое место'}


def load_level(n):
    p = SET / ('level.shr' if n == 1 else 'l%d/level.shr' % n)
    with open(p, 'rb') as f:
        hdr = f.read(104)
        ox, oz, cell = struct.unpack_from('<3f', hdr, 56)
        W, H = struct.unpack_from('<II', hdr, 68)
        bb = struct.unpack_from('<6f', hdr, 12); spawn = struct.unpack_from('<4f', hdr, 36)
        hm = np.frombuffer(f.read(W * H * 8), '<i2').reshape(H, W, 4)
        wl = np.frombuffer(f.read(W * H * 4), '<i2').reshape(H, W, 2)
    return dict(ox=ox, oz=oz, cell=cell, W=W, H=H, bb=bb, spawn=spawn, g0=hm[..., 0].astype(np.int32), g1=hm[..., 1].astype(np.int32),
                wmin=wl[..., 0].astype(np.int32), wmax=wl[..., 1].astype(np.int32), path=str(p))


class Lv:
    def __init__(s, n):
        s.n = n; s.d = load_level(n); d = s.d
        s.has0 = d['g0'] != -32768; s.has1 = d['g1'] != -32768
        s.wall = d['wmin'] <= d['wmax']
    def ij(s, x, z):
        return int(math.floor((z - s.d['oz']) / s.d['cell'])), int(math.floor((x - s.d['ox']) / s.d['cell']))
    def inside(s, i, j): return 0 <= i < s.d['H'] and 0 <= j < s.d['W']
    def ground(s, x, z, ref=None):
        """высота опоры, м (из двух слоёв ближе к ref, если задан), None - вне карты опоры"""
        i, j = s.ij(x, z)
        if not s.inside(i, j): return None
        c = []
        if s.has0[i, j]: c.append(s.d['g0'][i, j] / 100.0)
        if s.has1[i, j]: c.append(s.d['g1'][i, j] / 100.0)
        if not c: return None
        return max(c) if ref is None else min(c, key=lambda v: abs(v - ref))
    def run(s, x, y, z, fx, fz, maxd=MAXRUN, road=True):
        """длина свободного луча от глаза (x,y,z) вдоль горизонтального (fx,fz).
        road=True: пока под лучом непрерывная опора (дорога, тротуар, газон без скачка выше 0,6 м на клетку; пропуск до 1 клетки)
        и нет стены карты на высоте глаза; здания стоят вне опоры, поэтому на них луч обрывается.
        road=False: только рельеф - обрыв, когда опора выше глаза (для видов с холма поверх города)"""
        st = s.d['cell']; n = int(maxd / st); cm = y * 100.0; prev = y - EYE_H; gap = 0; last = 0
        for k in range(1, n + 1):
            px = x + fx * st * k; pz = z + fz * st * k
            i, j = s.ij(px, pz)
            if not s.inside(i, j): return k * st
            if s.wall[i, j] and s.d['wmin'][i, j] <= cm <= s.d['wmax'][i, j]: return k * st
            gs = []
            if s.has0[i, j]: gs.append(s.d['g0'][i, j] / 100.0)
            if s.has1[i, j]: gs.append(s.d['g1'][i, j] / 100.0)
            if gs and max(gs) * 100 > cm + 30: return k * st
            if not road: continue
            if not gs:
                gap += 1
                if gap > 1: return last * st
                continue
            g = min(gs, key=lambda v: abs(v - prev))
            if abs(g - prev) > 0.6 and not (abs(g - prev) < 1.2 and gap): return last * st
            prev = g; gap = 0; last = k
        return maxd
    def best_dir(s, x, y, z, prefer=None):
        best = None
        for k in range(NDIR):
            a = 2 * math.pi * k / NDIR; fx, fz = math.sin(a), math.cos(a)
            r = s.run(x, y, z, fx, fz)
            sc = r + (25.0 if prefer is not None and math.cos(a - prefer) > 0.9 else 0.0)
            if best is None or sc > best[0]: best = (sc, r, fx, fz)
        return best[1], best[2], best[3]


def mk(lv, pid, kind, name, x, z, src, ground=None, pitch=-0.03, fov=60, eye_h=EYE_H, prefer=None, ref=None, toward=None):
    g = lv.ground(x, z, ref) if ground is None else ground
    if g is None: return None
    y = g + eye_h
    if toward is not None:   # вид поверх города в сторону точки toward (центр опоры), рельеф без требования дороги
        dx, dz = toward[0] - x, toward[1] - z; dl = math.hypot(dx, dz) or 1.0; dx, dz = dx / dl, dz / dl
        best = None   # из направлений в сторону города (до 80 градусов от центра) - с самым далёким рельефом
        for k in range(NDIR):
            a = 2 * math.pi * k / NDIR; ax, az = math.sin(a), math.cos(a)
            if ax * dx + az * dz < 0.17: continue
            rr = lv.run(x, y, z, ax, az, road=False)
            if best is None or rr > best[0]: best = (rr, ax, az)
        r, fx, fz = best
    else:
        r, fx, fz = lv.best_dir(x, y, z, prefer)
    hn = math.sqrt(1 - pitch * pitch) if abs(pitch) < 1 else 1
    f = [round(fx * hn, 3), round(pitch, 3), round(fz * hn, 3)]
    return {'id': pid, 'kind': kind, 'name': name, 'eye': [round(x, 1), round(y, 2), round(z, 1)], 'fwd': f,
            'yaw_deg': round(math.degrees(math.atan2(fx, fz)) % 360, 1), 'fov': fov, 'free_m': round(r, 0), 'ground_y': round(g, 2), 'src': src,
            'cam': 'cam %.1f %.2f %.1f %.3f %.3f %.3f %d' % (x, y, z, f[0], f[1], f[2], fov), 'goto': 'goto %.1f %.1f 0' % (x, z)}


def far_pick(cands, have, n, mind=120.0):
    """жадно: n кандидатов, каждый дальше mind от уже выбранных (have - список (x,z)), первый - самый удалённый от выбранных"""
    out = []; pts = list(have)
    for _ in range(n):
        best = None
        for c in cands:
            d = min([math.hypot(c[0] - p[0], c[1] - p[1]) for p in pts] or [1e9])
            if d >= mind and (best is None or d > best[0]): best = (d, c)
        if not best: break
        out.append(best[1]); pts.append((best[1][0], best[1][1]))
    return out


def chamfer(free):
    """приблизительное расстояние (в клетках) от каждой свободной клетки до ближайшей занятой; две проходки"""
    H, W = free.shape; INF = 1e6
    d = np.where(free, INF, 0.0)
    for i in range(H):
        for j in range(W):
            if d[i, j] == 0: continue
            v = d[i, j]
            if i: v = min(v, d[i - 1, j] + 1)
            if j: v = min(v, d[i, j - 1] + 1)
            if i and j: v = min(v, d[i - 1, j - 1] + 1.4)
            if i and j < W - 1: v = min(v, d[i - 1, j + 1] + 1.4)
            d[i, j] = v
    for i in range(H - 1, -1, -1):
        for j in range(W - 1, -1, -1):
            if d[i, j] == 0: continue
            v = d[i, j]
            if i < H - 1: v = min(v, d[i + 1, j] + 1)
            if j < W - 1: v = min(v, d[i, j + 1] + 1)
            if i < H - 1 and j < W - 1: v = min(v, d[i + 1, j + 1] + 1.4)
            if i < H - 1 and j: v = min(v, d[i + 1, j - 1] + 1.4)
            d[i, j] = v
    return d


def build_level(L, inv_level):
    lv = Lv(L); d = lv.d; pts = []; chosen = []   # chosen: (x,z) уже занятые места
    def add(p):
        if p: pts.append(p); chosen.append((p['eye'][0], p['eye'][2]))
    nid = lambda: 'L%d_p%02d' % (L, len(pts) + 1)
    # --- старт уровня
    sx, sy, sz, sh = d['spawn']
    add(mk(lv, nid(), 'spawn', 'старт уровня', sx, sz, 'spawn SHR7', prefer=sh))
    # --- старты и цели миссий из описи
    starts, targets = [], []
    for m in inv_level['missions']:
        for r in m['start'].get('player_reset', []):
            x, y, z = r['pos']; g = lv.ground(x, z, y)
            if g is not None and abs(g - y) < 4.0: starts.append((x, z, '%s %s' % (m['id'], r.get('loc', '')), y))
        for s in m['stages']:
            t = s.get('target') or {}
            if t.get('pos') and s['mech'] in ('goto', 'race', 'delivery', 'dump', 'collect', 'follow', 'talkto', 'gooutside', 'dialogue', 'destroy', 'pickupitem', 'getin'):
                x, y, z = t['pos']; g = lv.ground(x, z, y)
                if g is not None and abs(g - y) < 4.0: targets.append((x, z, '%s %s %s' % (m['id'], s['rt_key'], s['mech']), y))
    for c in far_pick(starts, chosen, 2, 150):
        add(mk(lv, nid(), 'mission_start', 'старт миссии (%s)' % c[2], c[0], c[1], 'опись ' + c[2], ref=c[3]))
    for c in far_pick(targets, chosen, 2, 150):
        add(mk(lv, nid(), 'mission_target', 'цель миссии (%s)' % c[2], c[0], c[1], 'опись ' + c[2], ref=c[3]))
    # --- рельеф: холмы по высоте опоры
    g0 = np.where(lv.has0, d['g0'], -32768)
    ij = np.argwhere(lv.has0)
    order = np.argsort(-g0[lv.has0])[:4000]
    hills = [(d['ox'] + (ij[k][1] + .5) * d['cell'], d['oz'] + (ij[k][0] + .5) * d['cell']) for k in order]
    # --- мосты: клетки второго слоя опоры, кластеры по 40 м
    br = np.argwhere(lv.has1); clus = {}
    for i, j in br:
        clus.setdefault((i // 27, j // 27), []).append((i, j))
    bridges = sorted(clus.values(), key=len, reverse=True)
    # --- улицы: самые длинные свободные лучи вдоль опоры
    st = max(4, int(6 / d['cell'])); streets = []
    for i in range(0, d['H'], st):
        for j in range(0, d['W'], st):
            if not lv.has0[i, j]: continue
            x = d['ox'] + (j + .5) * d['cell']; z = d['oz'] + (i + .5) * d['cell']; y = d['g0'][i, j] / 100.0 + EYE_H
            r, fx, fz = lv.best_dir(x, y, z)
            streets.append((r, x, z))
    streets.sort(reverse=True)
    # --- открытые места: наибольший вписанный круг опоры
    dsm = 4; free = lv.has0[:d['H'] // dsm * dsm, :d['W'] // dsm * dsm].reshape(d['H'] // dsm, dsm, d['W'] // dsm, dsm).all(axis=(1, 3))
    dt = chamfer(free); opens = []
    for k in np.argsort(-dt, axis=None)[:3000]:
        i, j = divmod(int(k), dt.shape[1]); opens.append((d['ox'] + (j * dsm + dsm / 2) * d['cell'], d['oz'] + (i * dsm + dsm / 2) * d['cell'], dt[i, j] * dsm * d['cell']))
    ctr = tuple(np.argwhere(lv.has0).mean(axis=0)[::-1] * d['cell'] + np.array([d['ox'], d['oz']]))   # центр масс опоры (x,z)
    # --- выбор: холм, мост, улица, открытое; потом добор до 6-10
    h = far_pick([(x, z) for x, z in hills], chosen, 2, 200)
    for x, z in h[:1]: add(mk(lv, nid(), 'hill', 'холм, вид вдаль', x, z, 'карта опоры, высшая точка', pitch=-0.06, eye_h=3.0, toward=ctr))
    done_br = 0
    for cl in bridges:
        if done_br >= 1 or len(cl) < 8: break
        a = np.array(cl, float); c = a.mean(axis=0)
        x = d['ox'] + (c[1] + .5) * d['cell']; z = d['oz'] + (c[0] + .5) * d['cell']
        if min([math.hypot(x - p[0], z - p[1]) for p in chosen] or [1e9]) < 60: continue
        gy = max(lv.ground(x, z) or -1e9, d['g1'][int(c[0]), int(c[1])] / 100.0 if lv.has1[int(c[0]), int(c[1])] else -1e9)
        add(mk(lv, nid(), 'bridge', 'мост (%d клеток 2-го слоя)' % len(cl), x, z, 'карта опоры, второй слой', ground=gy)); done_br += 1
    s2 = far_pick([(x, z, r) for r, x, z in streets[:2500]], chosen, 2, 200)
    for x, z, r in s2[:1]: add(mk(lv, nid(), 'street', 'главная улица, вид вдоль', x, z, 'карта опоры, длинная прямая %d м' % r))
    o2 = far_pick(opens, chosen, 2, 150)
    for x, z, r in o2[:1]: add(mk(lv, nid(), 'open', 'открытое место (свободно около %d м)' % r, x, z, 'карта опоры, вписанный круг'))
    # добор до 6-10: вторая улица, второй холм, второе открытое, ещё старты/цели
    extra = [('street', s2[1:]), ('hill', h[1:]), ('open', o2[1:])]
    for kind, lst in extra:
        for c in lst:
            if len(pts) >= 10: break
            if kind == 'street': add(mk(lv, nid(), 'street', 'главная улица, вид вдоль', c[0], c[1], 'карта опоры, длинная прямая %d м' % c[2]))
            elif kind == 'hill': add(mk(lv, nid(), 'hill', 'холм, вид вдаль', c[0], c[1], 'карта опоры, высшая точка', pitch=-0.06, eye_h=3.0, toward=ctr))
            else: add(mk(lv, nid(), 'open', 'открытое место (свободно около %d м)' % c[2], c[0], c[1], 'карта опоры, вписанный круг'))
    if len(pts) < 6:
        for c in far_pick(targets + starts, chosen, 10 - len(pts), 80):
            add(mk(lv, nid(), 'mission_target', 'место миссии (%s)' % c[2], c[0], c[1], 'опись ' + c[2], ref=c[3]))
    return {'map': {'bbox': [round(v, 1) for v in d['bb']], 'support_grid': {'origin': [round(d['ox'], 1), round(d['oz'], 1)], 'cell_m': d['cell'], 'w': d['W'], 'h': d['H']}, 'file': Path(d['path']).name if L == 1 else 'l%d/level.shr' % L},
            'points': pts}


def main():
    global SET
    ap = argparse.ArgumentParser(); ap.add_argument('--set'); ap.add_argument('--out', default=str(ROOT / 'docs/state/viewpoints_93.json'))
    a = ap.parse_args()
    if a.set: SET = Path(a.set)
    inv = json.loads(INV.read_text()); out = {}
    for IL in inv['levels']:
        L = IL['level']; out[str(L)] = build_level(L, IL)
        print('L%d: %d точек: %s' % (L, len(out[str(L)]['points']), ', '.join(p['kind'] for p in out[str(L)]['points'])))
    j = {'axes': 'оси порта: x вправо, y вверх, z вперёд (мировые, как у goto/cam смоука); метры; fwd - единичный вектор взгляда; yaw_deg = atan2(fx,fz), 0 = +z, 90 = +x',
         'about': 'Данные-93: типичные точки обзора по 7 уровням; глаз = опора + %.1f м (холм 3.0); fwd - самый длинный свободный луч; free_m - его длина; cam - строка сценария смоука' % EYE_H,
         'levels': out}
    Path(a.out).write_text(json.dumps(j, ensure_ascii=False, indent=1) + '\n'); print('->', a.out)


if __name__ == '__main__':
    main()
