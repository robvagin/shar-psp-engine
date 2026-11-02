#!/usr/bin/env python3
"""№1106 (зеркала у Мо и в комнате Барта): сверка на Маке, что отражённый проход «от глаза» (viewer/main_rooms.h roomDrawMirrored) даёт ту же картинку, что прежний, и без щелей.
 1) Алгебра (float64): V·Rf·M = Vrot·Rl·(M − e'), где Rf — отражение через плоскость зеркала, Rl — его линейная часть, e' — отражённый глаз. Красный контроль --wrong-eye: глаз не отражён → не равно.
 2) Модель GE (как tests/rooms/tvgap1083.py: float24 с усечением, склейка P·(V·M) на кусок): общие вершины кусков, видимые в отражении. Прежний вариант (абсолютный перенос) расходится, новый нет.
 3) Цена: куски, которые идут в отражённый проход (непрозрачные, без отсечения по пирамиде), их треугольники.
  python3 tests/rooms/mirror1106.py interiors-l2-gfx.bin --room Moes [--plane x=54.75]
  python3 tests/rooms/mirror1106.py interiors-l7-gfx.bin --room BartRoom --plane z=447
Код выхода 0 — алгебра сошлась, «от глаза» не хуже 0,25 px и лучше «абсолютного» (или абсолютный тоже в норме)."""
import sys, os
import numpy as np
sys.path.insert(0, os.path.dirname(__file__))
import tvgap1083 as T

W, H = T.W, T.H


def piece_world(m, p):
    for vs in (12, 16):
        if p['off'] + p['ni'] * vs > len(m): continue
        q = np.frombuffer(m, np.int16, p['ni'] * vs // 2, p['off']).reshape(p['ni'], vs // 2)[:, (3, 4, 5) if vs == 12 else (4, 5, 6)]
        w = q.astype(np.float64) / 32768 * p['S'] + p['c']
        if (w.min(0) >= np.array(p['lo']) - 0.01).all() and (w.max(0) <= np.array(p['hi']) + 0.01).all(): return vs, q, w
    return None, None, None


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    path = args[0]
    room = sys.argv[sys.argv.index('--room') + 1] if '--room' in sys.argv else 'Moes'
    plane = sys.argv[sys.argv.index('--plane') + 1] if '--plane' in sys.argv else ('z=447' if room.lower() == 'bartroom' else 'x=54.75')
    ax = 0 if plane[0] == 'x' else 2; pv = float(plane[2:])
    m = T.load_room(path, room); P_ = T.pieces(m)
    for p in P_:
        p['vs'], p['q'], p['w'] = piece_world(m, p)
    ok_p = [p for p in P_ if p['w'] is not None]
    allw = np.concatenate([p['w'] for p in ok_p]); lo, hi = allw.min(0), allw.max(0)
    op = [p for p in P_ if not p['fl'] & 3]
    print(f'комната {room}: куски {len(P_)}, непрозрачных (идут в отражённый проход) {len(op)}, их треугольников {sum(p["ni"] for p in op) // 3} (из {sum(p["ni"] for p in P_) // 3})')
    print(f'  габарит x {lo[0]:.1f}..{hi[0]:.1f}, y {lo[1]:.1f}..{hi[1]:.1f}, z {lo[2]:.1f}..{hi[2]:.1f}; плоскость зеркала {plane}')
    # камера: внутри комнаты на стороне от зеркала, взгляд на плоскость зеркала
    eye = (lo + hi) / 2; eye[1] = lo[1] + (hi[1] - lo[1]) * 0.45
    side = -1 if abs(lo[ax] - pv) > abs(hi[ax] - pv) else 1   # комната по какую сторону плоскости
    if side < 0: eye[ax] = lo[ax] + (hi[ax] - lo[ax]) * 0.3
    else: eye[ax] = hi[ax] - (hi[ax] - lo[ax]) * 0.3
    tgt = eye.copy(); tgt[ax] = pv
    V = T.look(eye, tgt); Pm = T.persp()
    Rf = np.eye(4); Rf[ax, ax] = -1; Rf[ax, 3] = 2 * pv
    Rl = np.eye(4); Rl[ax, ax] = -1
    Vabs = V @ Rf                              # прежний: вид с переносом, умноженный на отражение
    Vr = V.copy(); Vr[:3, 3] = 0               # новый: вид без переноса x линейное отражение
    Vnew = Vr @ Rl
    e2 = eye.copy(); e2[ax] = 2 * pv - eye[ax]   # отражённый глаз
    if '--wrong-eye' in sys.argv: e2 = eye.copy()   # красный контроль: глаз не отражён
    # 1) алгебра
    rng = np.random.default_rng(1106); Mpts = rng.uniform(lo, hi, (2000, 3))
    a = (Vabs @ np.c_[Mpts, np.ones(len(Mpts))].T).T
    b = (Vnew @ np.c_[Mpts - e2, np.ones(len(Mpts))].T).T
    d_alg = np.abs(a - b).max()
    print(f'алгебра (float64): расхождение {d_alg:.2e}', 'PASS' if d_alg < 1e-6 else 'FAIL')
    ok_alg = d_alg < 1e-6
    # 3) цена: сколько кусков и треугольников проходит отсечение по ОТРАЖЁННОМУ ящику (MIRRORCULL1106_OLD=0) при взгляде на зеркало и вдоль стены
    def frustum(Vv):
        Mx = Pm @ Vv; pl = [Mx[3] + Mx[0], Mx[3] - Mx[0], Mx[3] + Mx[1], Mx[3] - Mx[1], Mx[3] + Mx[2], Mx[3] - Mx[2]]
        return [x / np.linalg.norm(x[:3]) for x in pl]
    def box_vis(pls, b):
        for x in pls:
            c = np.array([b[3] if x[0] > 0 else b[0], b[4] if x[1] > 0 else b[1], b[5] if x[2] > 0 else b[2]])
            if x[:3] @ c + x[3] < -0.5: return False
        return True
    for nm_, tg in (('на зеркало', tgt), ('вдоль стены', eye + (np.eye(3)[2 - ax] * 5.0))):
        pls = frustum(T.look(eye, tg)); n_p = n_t = 0
        for p in op:
            b = list(p['lo']) + list(p['hi']); r = list(b); r[ax] = 2 * pv - b[ax + 3]; r[ax + 3] = 2 * pv - b[ax]
            if box_vis(pls, r): n_p += 1; n_t += p['ni'] // 3
        print(f'цена (взгляд {nm_}): с отсечением отражённого ящика {n_p} кусков, {n_t} треугольников (без отсечения {len(op)} и {sum(p["ni"] for p in op) // 3})')
    # 2) GE: общие вершины непрозрачных кусков
    seen = {}
    for p in op:
        if p['w'] is None: continue
        for k in range(len(p['w'])):
            seen.setdefault(tuple(np.round(p['w'][k], 3)), []).append((p, k))

    def screen(p, k, rel):
        k_ = np.float32(p['S']); o = p['c'].astype(np.float32)
        M = np.eye(4, dtype=np.float32); M[0, 0] = M[1, 1] = M[2, 2] = k_
        if rel: M[:3, 3] = o - e2.astype(np.float32); Vm = Vnew.astype(np.float32)
        else: M[:3, 3] = o; Vm = Vabs.astype(np.float32)
        WM = T.mm(Pm.astype(np.float32), T.mm(Vm, M))
        q = np.append(p['q'][k, :3] if False else p['q'][k], 1)
        q = np.array([p['q'][k][0], p['q'][k][1], p['q'][k][2], 1], np.float32); q[:3] = q[:3] / np.float32(32768)
        c = T.mv(WM, q)
        if c[3] <= 0.2: return None
        ndc = c[:2] / c[3]
        if abs(ndc[0]) > 1.5 or abs(ndc[1]) > 1.5: return None
        return np.array([(ndc[0] * .5 + .5) * W, (1 - (ndc[1] * .5 + .5)) * H])
    # индекс вершины в q: у шага 12 и 16 разный сдвиг; берём из piece_world (q уже выбран по нужным столбцам)
    res = {}
    keys = [k for k, v in seen.items() if len({id(a[0]) for a in v}) >= 2]
    rng.shuffle(keys); keys = keys[:4000]
    for rel in (False, True):
        worst, bad, n = 0.0, 0, 0
        for key in keys:
            pts = [screen(p, k, rel) for p, k in seen[key]]
            if any(x is None for x in pts): continue
            d = max(np.linalg.norm(x - y) for x in pts for y in pts); n += 1; worst = max(worst, d); bad += d > 0.25
        res[rel] = (n, worst, bad)
        print(f'{"от глаза (вид без переноса, центр − отражённый глаз)" if rel else "абсолютный (круг 5)"}: общих видимых вершин {n}, расхождение до {worst:.3f} px, больше 0,25 px: {bad}')
    ok_ge = res[True][0] > 0 and res[True][1] <= 0.25
    ok = ok_alg and ok_ge
    if '--wrong-eye' in sys.argv: ok = not ok_alg; print('красный контроль (глаз не отражён):', 'алгебра не сошлась, как должно' if ok else 'НЕ поймано')
    print('RESULT', 'PASS' if ok else 'FAIL'); return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
