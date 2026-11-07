"""№1177б: таблица дали dal877.bin уровня: узлы, нужная даль (0 150 м, 1 200, 2 250), карта и высоты глаза.  python3 -I tools/far1177/dal_map.py dal877.bin [map]"""
import struct, sys, numpy as np
def load(p):
    d = open(p, 'rb').read(); m, sg, x0, z0, st, W, H = struct.unpack_from('<4sIfffII', d, 0); n = W * H
    pk = np.frombuffer(d, '<u2', n, 28).reshape(H, W).astype(np.int32); yt = np.frombuffer(d, '<i2', n, 28 + 2 * n).reshape(H, W).astype(np.int32)
    need = np.full((H, W), -1, np.int32)
    for k in range(8):
        v = (pk >> (2 * k)) & 3; v = np.where(v == 3, -1, v); need = np.maximum(need, np.where(pk == 0xFFFF, -1, v))
    return dict(x0=x0, z0=z0, st=st, W=W, H=H, pk=pk, yt=yt, need=need)
if __name__ == "__main__" and sys.argv[1] != "--raise":
    T = load(sys.argv[1]); nd = T['need']
    print('узлов', T['W'] * T['H'], 'без данных', (nd < 0).sum(), '| 150 м:', (nd == 0).sum(), '200 м:', (nd == 1).sum(), '250 м:', (nd == 2).sum(), 'шаг', T['st'], 'x0 %.1f z0 %.1f' % (T['x0'], T['z0']))
    if len(sys.argv) > 2:
        sym = {-1: ' ', 0: '1', 1: '2', 2: '.'}
        for j in range(0, T['H'], 2): print(''.join(sym[int(nd[j, i])] for i in range(T['W'])), ' z=%.0f' % (T['z0'] + j * T['st']))

def raise_hwy(src, dst, eye_min=20.0):
    """вариант для Роба: узлы автострады (высота глаза бота >= eye_min м: эстакады и рампы) получают 250 м по всем сторонам с данными; остальные не меняются.
    python3 -I tools/far1177/dal_map.py --raise IN.bin OUT.bin [20]"""
    d = bytearray(open(src, 'rb').read()); m, sg, x0, z0, st, W, H = struct.unpack_from('<4sIfffII', d, 0); n = W * H
    pk = np.frombuffer(bytes(d[28:28 + 2 * n]), '<u2').reshape(H, W).astype(np.int32); yt = np.frombuffer(bytes(d[28 + 2 * n:28 + 4 * n]), '<i2').reshape(H, W) * 0.1
    ch = 0
    for j in range(H):
        for i in range(W):
            v = int(pk[j, i])
            if v == 0xFFFF or yt[j, i] < eye_min: continue
            o = v
            for k in range(8):
                c = (v >> (2 * k)) & 3
                if c < 2: v = (v & ~(3 << (2 * k))) | (2 << (2 * k))
            if v != o: pk[j, i] = v; ch += 1
    d[28:28 + 2 * n] = pk.astype('<u2').tobytes(); open(dst, 'wb').write(bytes(d)); return ch
if len(sys.argv) > 3 and sys.argv[1] == '--raise':
    print('узлов изменено', raise_hwy(sys.argv[2], sys.argv[3], float(sys.argv[4]) if len(sys.argv) > 4 else 20.0))
