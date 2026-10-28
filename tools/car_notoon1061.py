#!/usr/bin/env python3
"""№1061: notoon.txt под level.shr после tools/car_tire_splice1061.py (+1 текстура колёс в конце таблицы, ntex и подпись сменились).
  car_notoon1061.py SET CAR_OUT     (SET — набор с прежними level.shr и notoon.txt, CAR_OUT — папка подмены той же раскладки: L1 в корне, l<N>/ для N >= 2)
Проверяет: прежние индексы текстур не сдвинулись (ntex_new = ntex_old + k, у не-колёс поле tex куска то же), списки (notoon и листва) ссылаются только на прежние индексы;
новая текстура колёс входит в список, если в нём была прежняя текстура этих колёс (иначе нет). Пишет CAR_OUT/[l<N>/]notoon.txt: заголовок «ntex N sig X» новый, тело то же (+ индекс новой текстуры, если надо)."""
import re, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import carshr_probe1061 as pr, car_tire_splice1061 as sp   # noqa: E402
from notoon_check import shr_sig   # noqa: E402
S, C = Path(sys.argv[1]), Path(sys.argv[2])
for L in (1, 3, 4, 5, 6, 7):
    rel = Path('.') if L == 1 else Path(f'l{L}')
    b = (S / rel / 'level.shr').read_bytes(); n = (C / rel / 'level.shr').read_bytes(); hb, hn = pr.parse(b), pr.parse(n)
    pb, pn = pr.pieces(b, hb), pr.pieces(n, hn); ws = {p['i'] for _, p in sp.wheel_pieces(b, hb, pb)}
    k = hn['ntex'] - hb['ntex']; assert k >= 1, 'ntex не вырос'
    for p, q in zip(pb, pn):
        if p['i'] not in ws: assert p['tex'] == q['tex'], f'кусок {p["i"]}: tex сдвинулся'
    old_w = {pb[i]['tex'] for i in ws}; new_w = {pn[i]['tex'] for i in ws}
    assert all(t >= hb['ntex'] for t in new_w), 'колесо не на новой текстуре'
    txt = (S / rel / 'notoon.txt').read_text().split('\n'); head = txt[0].split(); assert head[:2] == ['ntex', str(hb['ntex'])], 'прежний список не к прежнему level.shr'
    lists = [set(map(int, l.split())) for l in (txt[1], txt[3]) if l.strip()]
    assert all(i < hb['ntex'] for s_ in lists for i in s_), 'в списке индекс вне прежних'
    inl = [(w, [j for j, s_ in enumerate(lists) if w in s_]) for w in sorted(old_w)]
    add = any(j for _, j in inl)   # прежняя текстура колёс в одном из списков -> новая тоже
    ntex, sig = shr_sig(C / rel / 'level.shr')
    if add:
        which = {j for _, js in inl for j in js}
        raise SystemExit(f'L{L}: прежняя текстура колёс {old_w} в списках {which}: нужна ручная правка')
    out = 'ntex %d sig %08x\n' % (ntex, sig) + '\n'.join(txt[1:]); (C / rel / 'notoon.txt').write_text(out)
    print(f'L{L}: прежние индексы на месте (ntex {hb["ntex"]} -> {hn["ntex"]}), прежние текстуры колёс {sorted(old_w)} в списках: нет, новая {sorted(new_w)} в список не входит; заголовок {head[1]} {head[3]} -> {ntex} {sig:08x}')
