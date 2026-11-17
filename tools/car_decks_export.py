#!/usr/bin/env python3
"""1332 (Б): «этажи» верха машин оригинала для Гомера пешком -> текстовый файл cardecks.txt рядом с EBOOT (читает homer.c homerDecksParse).
В p3d машины оригинала лежит набор коллизионных объёмов (OBB, chunk 0x7010004): у высоких машин несколько уровней (кабина выше, кузов или капот ниже); наша опора была ОДНА плоская на верхе всей модели (bodyHi).
Для машин с bodyHi >= 2.3 пишется до 6 крупных объёмов (верх выше 0,9 м, площадь >= 0,3): смещение вдоль машины от центра кузова, полудлина, полуширина и высота верха над землёй. Числа относительные,\nфайл не зависит от привязки геометрии. Ресурсы игры не публикуем: файл — данные сборки набора, не в git.
    python3 tools/car_decks_export.py OUT.txt [SET_DIR]       (SET_DIR — набор с msncars-l*.shr и traffic.shr для lift и bodyHi; по умолчанию набор Будки test107tour)"""
import sys, struct, glob, os, re
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, 'hit-and-run-web/tools'))
import p3d
from pathlib import Path
MIN_HI = 2.3
def find(c, idv, out):
    if c.id == idv: out.append(c)
    for k in c.children: find(k, idv, out)
def volumes(nm):
    root = p3d.read(Path(ROOT) / f'hit-and-run-web/source/game/art/cars/{nm}.p3d'); co = []; find(root, 0x7010004, co); res = []
    for b in co:
        ext = struct.unpack_from('<3f', b.data); vs = [struct.unpack_from('<3f', k.data) for k in b.children if k.id == 0x7010007]
        if len(vs) != 4: continue
        c, a1, a2, a3 = vs; A = (a1, a2, a3)
        top = c[1] + sum(abs(A[i][1]) * ext[i] for i in range(3)); bot = c[1] - sum(abs(A[i][1]) * ext[i] for i in range(3))
        hx = sum(abs(A[i][0]) * ext[i] for i in range(3)); hz = sum(abs(A[i][2]) * ext[i] for i in range(3))
        res.append(dict(zc=c[2], top=top, bot=bot, hl=hz, hw=hx))
    return res
def tables(S):
    tab = {}; mfmt = '<16s4H10f12ff3I'; esz = 16 + struct.calcsize(mfmt) + 32
    for f in sorted(glob.glob(S + '/l*/msncars-l*.shr')):
        d = open(f, 'rb').read(); mg, ver, lv, n, doff, tot, ioff, isz = struct.unpack_from('<4s7I', d, 0)
        for i in range(n):
            o = doff + i * esz; nm = d[o:o + 16].split(b'\0')[0].decode(); m = struct.unpack_from(mfmt, d, o + 16)
            tab[nm.lower()] = dict(lift=m[5], bodyHi=m[14])
    for f in [S + '/traffic.shr'] + sorted(glob.glob(S + '/l*/traffic.shr')):
        if not os.path.exists(f): continue
        d = open(f, 'rb').read()
        for off in range(0, min(len(d) - 128, 4000000), 4):
            nm = d[off:off + 16]; z = nm.find(b'\0')
            if z < 3 or not all(32 < c < 127 for c in nm[:z]) or any(nm[z:]): continue
            v = struct.unpack_from('<16s4H10f12ff3I', d, off); lift, wr, hw, hl, bo, wx, wf, wrr, blo, bhi = v[5:15]
            if 0.3 < hw < 2.5 and 0.5 < hl < 6 and 0.5 < bhi < 6 and 0 < wr < 1: tab.setdefault(nm[:z].decode().lower(), dict(lift=lift, bodyHi=bhi))
    return tab
def main():
    out = sys.argv[1]; S = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser('~/Downloads/shar-psp-int107/build/test107tour/PSP/GAME/SHARTEST15')
    tab = tables(S); lines = ['# 1332 cardecks: имя(до 11) число  [смещение_вдоль_от_центра_кузова полудлина полуширина верх_над_землёй]...']
    done = set()
    for nm in sorted(tab):
        t = tab[nm]
        if t['bodyHi'] < MIN_HI: continue
        p = Path(ROOT) / f'hit-and-run-web/source/game/art/cars/{nm}.p3d'
        if not p.exists():
            c = [q for q in glob.glob(str(p.parent / '*.p3d')) if os.path.basename(q)[:11].lower() == nm[:11]]
            if not c: continue
            nm_f = os.path.basename(c[0])[:-4]
        else: nm_f = nm
        try: vs = volumes(nm_f)
        except Exception: continue
        vs = [dict(v, top=v['top'] + t['lift']) for v in vs if v['top'] - v['bot'] > 0.6]
        vs = [v for v in vs if v['top'] > 0.8 and v['hl'] > 0.4 and v['hw'] > 0.4]
        if not vs: continue
        sel = []
        for v in sorted(vs, key=lambda v: -v['hl'] * v['hw']):
            if v['top'] > 0.9 and v['hl'] * v['hw'] >= 0.3 and all(abs(v['zc'] - q['zc']) > 0.15 or abs(v['top'] - q['top']) > 0.1 or abs(v['hl'] - q['hl']) > 0.1 for q in sel): sel.append(v)
        sel = sel[:6]
        if not sel: continue
        zc_u = (min(v['zc'] - v['hl'] for v in sel) + max(v['zc'] + v['hl'] for v in sel)) / 2      # центр объёмов вдоль машины = центр коробки кузова; z диска не отзеркален (наш мир зеркалит z), поэтому вперёд по машине = +z диска
        key = nm_f.lower()[:11]
        if key in done: continue
        done.add(key)
        lines.append('%s %d %s' % (key, len(sel), ' '.join('%.3f %.3f %.3f %.3f' % (v['zc'] - zc_u, v['hl'], v['hw'], v['top']) for v in sel)))
    Path(out).write_text('\n'.join(lines) + '\n'); print(out, len(lines) - 1, 'машин')
main()
