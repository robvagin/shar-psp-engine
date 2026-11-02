#!/usr/bin/env python3
"""№1083 (= №1075, щель у телевизора в гостиной дома Симпсонов): сверка на Маке, что общая вершина двух кусков комнаты на экране PSP совпадает.

Модель GE — как в tools/seams_ge.py (--ge cat): float24 (мантисса 15 бит, усечение после каждого умножения и сложения), матрица куска склеена с видом и
проекцией один раз на кусок: W = P·(V·M), клип = W·v. Комната лежит в 500 м от начала: при переносе куска в мировых координатах (как было, «abs») склейка даёт у
каждого куска свою ошибку и общая вершина двух кусков расходится на экране; при виде без переноса и переносе куска = центр - глаз («rel», TVGAP1083_OLD=0, main_rooms.h roomDraw)
расхождение исчезает.
Вход: пакет комнат interiors-lN-gfx.bin (SHG1). Для комнаты SimpsonsHouse (по умолчанию) камера как у int_sh_path1: z 402,87, y -17,86, взгляд на телевизор.
  python3 tests/rooms/tvgap1083.py [interiors-l1-gfx.bin] [--room SimpsonsHouse]
Код выхода 0 — «rel» не хуже 0,25 пикселя и лучше «abs»; 1 — иначе (красный контроль: --abs-only проверяет, что «abs» действительно расходится)."""
import struct, sys
import numpy as np

W, H = 480, 272


def t24(x):
    """усечение до 15 бит мантиссы (float24 GE)"""
    a = np.asarray(x, np.float32); u = a.view(np.uint32) & np.uint32(0xFFFFFF00)
    return u.view(np.float32)


def mm(A, B):  # 4x4 · 4x4 с усечением после умножений и сложений (строки на столбцы, колонки-векторы)
    out = np.zeros((4, 4), np.float32)
    for i in range(4):
        for j in range(4):
            acc = np.float32(0)
            for k in range(4): acc = t24(acc + t24(np.float32(A[i, k]) * np.float32(B[k, j])))
            out[i, j] = acc
    return out


def mv(Wm, v):
    out = np.zeros(4, np.float32)
    for i in range(4):
        acc = np.float32(0)
        for k in range(4): acc = t24(acc + t24(Wm[i, k] * v[k]))
        out[i] = acc
    return out


def look(eye, tgt, up=(0, 1, 0)):
    f = np.array(tgt, np.float64) - eye; f /= np.linalg.norm(f)
    r = np.cross(f, up); r /= np.linalg.norm(r); u = np.cross(r, f)
    V = np.eye(4); V[0, :3], V[1, :3], V[2, :3] = r, u, -f
    V[:3, 3] = -V[:3, :3] @ eye
    return V


def persp(fovy=65.0, asp=16 / 9, n=0.2, f=250.0):
    t = 1 / np.tan(np.radians(fovy) / 2); P = np.zeros((4, 4))
    P[0, 0], P[1, 1], P[2, 2], P[2, 3], P[3, 2] = t / asp, t, (f + n) / (n - f), 2 * f * n / (n - f), -1
    return P


def load_room(path, name):
    b = open(path, 'rb').read()
    assert b[:4] == b'SHG1', 'не SHG1'
    nr = struct.unpack_from('<I', b, 4)[0]
    for r in range(nr):
        nm = b[16 + 48 * r:16 + 48 * r + 32].split(b'\0')[0].decode()
        off, size = struct.unpack_from('<2I', b, 16 + 48 * r + 32)
        if nm.lower() == name.lower(): return b[off:off + size]
    raise SystemExit(f'нет комнаты {name}')


def pieces(m):
    assert m[:4] == b'RMG1'
    ntex, npc, to, ro = struct.unpack_from('<4I', m, 4)
    out = []
    for i in range(npc):
        r = struct.unpack_from('<6fiIIII3ffIII2I', m, ro + 80 * i)
        lo, hi, tex, fl, nv, ni, nf, c, S, off = r[0:3], r[3:6], r[6], r[7], r[8], r[9], r[10], np.array(r[11:14]), r[14], r[15]
        v12 = bool(fl & 0x2000) if False else None
        out.append(dict(i=i, fl=fl, ni=ni, c=c, S=S, off=off, lo=lo, hi=hi))
    return out


def verts(m, p, vs):
    n = p['ni']; q = np.frombuffer(m, np.int16, n * vs // 2, p['off']).reshape(n, vs // 2)
    return q[:, 4:7] if vs == 16 else q[:, 3:6] if False else q[:, 2:5]


def main():
    path = next((a for a in sys.argv[1:] if not a.startswith('--')), 'build/interiors-l1-gfx.bin')
    room = sys.argv[sys.argv.index('--room') + 1] if '--room' in sys.argv else 'SimpsonsHouse'
    m = load_room(path, room); P_ = pieces(m)
    # шаг вершины: 16 Б, а при флаге F_V12 — 12 Б; значение флага берём по числу вершин и размеру данных
    V = look(np.array([500.0, -17.86, 402.87]), np.array([499.8, -19.0, 400.7])); Pm = persp()
    eye = np.array([500.0, -17.86, 402.87], np.float32)
    seen = {}
    for p in P_:
        # F_V12 в tools/psp_export.py = 1 << 11? определяем по длине до следующего куска
        pass
    # для надёжности читаем оба шага и выбираем тот, где вершины ложатся в бокс куска
    for p in P_:
        for vs in (12, 16):
            n = p['ni']
            if p['off'] + n * vs > len(m): continue
            q = np.frombuffer(m, np.int16, n * vs // 2, p['off']).reshape(n, vs // 2)[:, (3, 4, 5) if vs == 12 else (4, 5, 6)]
            w = q.astype(np.float64) / 32768 * p['S'] + p['c']
            if (w.min(0) >= np.array(p['lo']) - 0.01).all() and (w.max(0) <= np.array(p['hi']) + 0.01).all(): p['w'] = w; break
    for p in P_:
        if 'w' not in p: continue
        w = p['w']; sel = (w[:, 0] > 497) & (w[:, 0] < 503) & (w[:, 2] > 399) & (w[:, 2] < 402) & (w[:, 1] > -20.2) & (w[:, 1] < -18)
        for k in np.where(sel)[0]:
            seen.setdefault(tuple(np.round(w[k], 3)), []).append((p, k))

    def screen(p, k, rel):
        k_ = np.float32(p['S']); o = p['c'].astype(np.float32)
        M = np.eye(4, dtype=np.float32); M[0, 0] = M[1, 1] = M[2, 2] = k_; M[:3, 3] = (o - eye) if rel else o
        Vm = V.astype(np.float32).copy()
        if rel: Vm[:3, 3] = 0
        WM = mm(Pm.astype(np.float32), mm(Vm, M))
        q = np.append(np.frombuffer(m, np.int16, p['ni'] * 16 // 2, p['off']).reshape(p['ni'], 8)[k, 4:7] if p['vs'] == 16 else
                      np.frombuffer(m, np.int16, p['ni'] * 12 // 2, p['off']).reshape(p['ni'], 6)[k, 3:6], 1).astype(np.float32)
        q[:3] = q[:3] / np.float32(32768)
        c = mv(WM, q); ndc = c[:2] / c[3]
        return np.array([(ndc[0] * .5 + .5) * W, (1 - (ndc[1] * .5 + .5)) * H])

    for p in P_:
        if 'w' in p:
            for vs in (12, 16):
                if p['off'] + p['ni'] * vs <= len(m):
                    q = np.frombuffer(m, np.int16, p['ni'] * vs // 2, p['off']).reshape(p['ni'], vs // 2)[:, (3, 4, 5) if vs == 12 else (4, 5, 6)]
                    w = q.astype(np.float64) / 32768 * p['S'] + p['c']
                    if np.allclose(w, p['w']): p['vs'] = vs; break
    res = {}
    for rel in (False, True):
        worst, bad, n = 0.0, 0, 0
        for key, lst in seen.items():
            if len({id(a[0]) for a in lst}) < 2: continue
            pts = [screen(p, k, rel) for p, k in lst]; d = max(np.linalg.norm(a - b) for a in pts for b in pts)
            n += 1; worst = max(worst, d); bad += d > 0.25
        res[rel] = (n, worst, bad)
        print(f'{"rel (центр - глаз, вид без переноса)" if rel else "abs (как было)"}: общих вершин {n}, расхождение до {worst:.3f} px, больше 0,25 px: {bad}')
    ok = res[True][1] <= 0.25 and res[True][1] <= res[False][1]
    if '--abs-only' in sys.argv: ok = res[False][1] > 0.25; print('красный контроль:', 'abs расходится' if ok else 'abs НЕ расходится — тест ничего не доказывает')
    print('RESULT', 'PASS' if ok else 'FAIL'); return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
