"""№ДАЛЬ 1Д: ориентиры дали — боковая таблица dalimp.bin уровня (viewer/dalimp.h).
  python3 -I tools/far1177/dalimp.py LEVEL.shr OUT.bin [--signs tools/far1177/signs_LN.json] [--hmin 35] [--sw 10] [--maxf 1500]
  python3 -I tools/far1177/dalimp.py --cost LEVEL.shr DAL877.bin DALIMP.bin     оценка цены по узлам таблицы дали (мс на кадр, модель PSP)
Таблица дали dal877 режет город до 150–200 м там, где «даль занимает ≤ 1 % экрана». Для ориентиров этого мало: башня или щит вдали занимают меньше 1 %,
но глаз их ищет. Ориентир = кусок города, который выше hmin м (башни, высокие здания) или крупная вывеска (рисунок из списка вывесок №1178,
ширина или высота ≥ sw м). Клетка, где есть ориентир, рисуется до полной дали (регулятор, 250 м) даже там, где таблица режет: движок рисует клетку листом целиком,
поэтому в таблицу идёт клетка; дорогие клетки (дальних треугольников > maxf) не берутся. Мосты и дальние дороги уже рисует слой road934, их здесь нет.
Файл: 'DIM1', u32 подпись таблицы кусков (pvs2Sig, как у dal877/PVS2), u32 n, n × u32 номер КУСКА (не клетки: номера клеток меняются при пересборке сильнее; клетку движок берёт у куска)."""
import sys, json, struct
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1])); import budget as B, opq863 as O
sys.path.insert(0, str(Path(__file__).resolve().parent)); import signs as G, dal_map as D

def classify(lv, sign_json=None, hmin=35., sw=10., maxf=1500):
    b = Path(lv).read_bytes(); L = B.load(lv); R = L['recs']; npc, = struct.unpack_from('<I', b, 8); ptab, = struct.unpack_from('<I', b, 76)
    own = np.array([r['owner'] for r in R]); bb = np.array([r['bb'] for r in R]); tex = np.array([r['tex'] for r in R]); fl = np.array([r['flags'] for r in R])
    nf = np.array([r['nf'] for r in R]) // 3; cell = np.array([r['cell'] for r in R]); sz = bb[:, 3:] - bb[:, :3]; h = sz[:, 1]; w = np.hypot(sz[:, 0], sz[:, 2])
    town = (own == 0) & (nf > 0) & ~((fl & 2) != 0) & (cell != 0xFFFF)
    sids = set()
    if sign_json:
        _, sg = G.sigs(b); d = json.loads(Path(sign_json).read_text()); sids = {i for s in d['signs'] for i in sg.get(s['sig'], [])}
    tall = town & (h >= hmin); sign = town & np.isin(tex, list(sids)) & ((np.maximum(w, h)) >= sw)
    lm = tall | sign
    # дальние треугольники клетки: все куски клетки (лист рисуется целиком)
    ft = {}
    for i in range(npc):
        if own[i] == 0 and not (fl[i] & 2): ft[cell[i]] = ft.get(cell[i], 0) + nf[i]
    keep = np.array([lm[i] and ft.get(cell[i], 0) <= maxf for i in range(npc)])
    return dict(b=b, L=L, npc=npc, ptab=ptab, sig=O.table_sig(b, npc, ptab), idx=np.nonzero(keep)[0], tall=tall, sign=sign, keep=keep, cell=cell, ft=ft, bb=bb, h=h)

def write(out, c):
    Path(out).write_bytes(b'DIM1' + struct.pack('<II', c['sig'], len(c['idx'])) + b''.join(struct.pack('<I', int(i)) for i in c['idx']))

def cost(lv, dal, dim, hmin=35., sw=10., maxf=1500, sign_json=None, fov=40.):
    c = classify(lv, sign_json, hmin, sw, maxf); T = D.load(dal); cells = {}
    for i in c['idx']: cells.setdefault(int(c['cell'][i]), 1)
    # центры клеток-ориентиров
    b = c['b']; ncell, ctab = struct.unpack_from('<II', b, 80); cc = []; cf = []
    for ci in cells:
        r = struct.unpack_from('<6fIIIII', b, ctab + 48 * ci); cc.append(((r[0] + r[3]) / 2, (r[2] + r[5]) / 2, r[3] - r[0])); cf.append(c['ft'].get(ci, 0))
    cc = np.array(cc); cf = np.array(cf)
    ms = []; nc = []
    for j in range(T['H']):
        for i in range(T['W']):
            v = int(T['pk'][j, i])
            if v == 0xFFFF: continue
            x = T['x0'] + i * T['st']; z = T['z0'] + j * T['st']
            dx = cc[:, 0] - x; dz = cc[:, 1] - z; dist = np.hypot(dx, dz); ang = np.degrees(np.arctan2(dx, dz))
            for k in range(8):
                code = (v >> (2 * k)) & 3
                if code >= 2: continue
                cap = 150. if code == 0 else 200.
                dd = (ang - k * 45. + 180.) % 360. - 180.
                m = (np.abs(dd) <= fov) & (dist > cap) & (dist <= 250.)
                if m.any(): ms.append(.32 * cf[m].sum() / 1000. + .045 * m.sum()); nc.append(int(m.sum()))
                else: ms.append(0.); nc.append(0)
    ms = np.array(ms); nc = np.array(nc)
    return c, dict(nodes_dirs=len(ms), added_mean=float(ms.mean()), added_p90=float(np.percentile(ms, 90)), added_p99=float(np.percentile(ms, 99)), added_max=float(ms.max()), share_nonzero=float((ms > 0).mean()), cells_mean=float(nc.mean()), cells_max=int(nc.max()))

if __name__ == '__main__':
    a = sys.argv[1:]
    def opt(n, d, cast=float): return cast(a[a.index(n) + 1]) if n in a else d
    sj = a[a.index('--signs') + 1] if '--signs' in a else None
    if a[0] == '--cost':
        c, r = cost(a[1], a[2], a[3], opt('--hmin', 35.), opt('--sw', 10.), opt('--maxf', 1500, int), sj)
        print('ориентиров-кусков %d (высоких %d, вывесок %d), клеток %d' % (len(c['idx']), int((c['tall'] & c['keep']).sum()), int((c['sign'] & c['keep']).sum()), len(set(int(c['cell'][i]) for i in c['idx']))))
        print('цена на кадр (модель: 0,32 мс/тыс. дальних тр. + 45 мкс/лист), по узлам таблицы и сторонам, где она режет:'); print(json.dumps(r, ensure_ascii=False))
    else:
        c = classify(a[0], sj, opt('--hmin', 35.), opt('--sw', 10.), opt('--maxf', 1500, int)); write(a[1], c)
        print('dalimp: кусков %d (высоких %d, вывесок %d), клеток %d, файл %d Б -> %s' % (len(c['idx']), int((c['tall'] & c['keep']).sum()), int((c['sign'] & c['keep']).sum()), len(set(int(c['cell'][i]) for i in c['idx'])), 12 + 4 * len(c['idx']), a[1]))
