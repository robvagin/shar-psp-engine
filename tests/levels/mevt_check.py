#!/usr/bin/env python3
"""ТЕСТ90 (g04-homer): таблица MEVT пакета sfx.sab содержит события уличных гонок. tests/levels/mevt_check.py sfx.sab [...]
Для каждой гонки StreetRace01..03: start, win, lose, get_out_of_car — запись есть, дорожек n >= 1 (start — не меньше одной), индексы дорожек не отрицательные.
Выход 0 — всё есть; 1 — чего-то нет (старый пакет: музыка гонок не собрана, движок пишет «своей музыки в композиции нет»)."""
import struct, sys

def mevt(path):
    b = open(path, 'rb').read()
    i = b.find(b'SXT1')
    if i < 0: return None
    n = struct.unpack_from('<I', b, i + 4)[0]
    for k in range(n):
        tag, off, cnt, stride = struct.unpack_from('<4sIII', b, i + 8 + 16 * k)
        if tag == b'MEVT' and stride == 36:
            out = {}
            for j in range(cnt):
                r = b[i + off + 36 * j: i + off + 36 * (j + 1)]
                name = r[:28].split(b'\0')[0].decode('ascii', 'replace')
                t0, t1, t2, nn, kind = struct.unpack_from('<3hBB', r, 28)
                out[name] = (nn, [x for x in (t0, t1, t2)][:nn], kind)
            return out
    return None

def main():
    bad = 0
    for p in sys.argv[1:]:
        ev = mevt(p)
        if ev is None: print('FAIL', p, ': нет таблицы MEVT'); bad = 1; continue
        miss = []
        for r in (1, 2, 3):
            for k in ('start', 'win', 'lose', 'get_out_of_car'):
                e = f'StreetRace0{r}_{k}'
                if e not in ev or ev[e][0] < 1 or any(t < 0 for t in ev[e][1]): miss.append(e)
        have = sum(1 for e in ev if e.startswith('StreetRace'))
        print(('ok  ' if not miss else 'FAIL'), p, f': событий гонок {have} из 12, MEVT всего {len(ev)}' + (f', нет: {", ".join(miss[:4])}...' if miss else ''))
        bad |= bool(miss)
    return bad
sys.exit(main())
