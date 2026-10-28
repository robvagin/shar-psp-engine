#!/usr/bin/env python3
"""скорость 250, приём 2: служебный счётчик CAMCLIPSTAT (только смоук) без чтения далёких кусков. check_cc.py ДИР_f0(выключатель) ДИР_f1(новый) ДИР_c1(сверка обоими способами)
Строка CAMCLIPSTAT (суммы near03, clip_pieces_max, wall_*, eye_inside) у нового такая же, как у выключателя; сверка не нашла расхождений; цена счётчика (DCITY, последнее поле) у нового вдвое ниже и меньше; на выключателе прежняя."""
import re, sys, os
ok = True
def need(c, m):
    global ok; print(('  ok   ' if c else '  FAIL ') + m); ok &= bool(c)
def st(d):
    T = open(os.path.join(d, 'smoke.txt'), errors='replace').read().splitlines()
    S = {}
    for l in T:
        if l.startswith('CAMCLIPSTAT'):
            for k, v in re.findall(r' (\w+)=(-?\d+)', l):
                if k not in ('frames', 'ggnear'): S[k] = max(S.get(k, 0), int(v)) if k.startswith('clip_pieces') else S.get(k, 0) + int(v)
    us = [int(l.split()[-1].split('=')[1]) for l in T if l.startswith('DCITY') and 'sc=F_L4_CR1 ' in l]
    return dict(S=S, us=sum(us) / max(len(us), 1), mis=sum(1 for l in T if 'CAMCLIPFAST mismatch' in l), end=any(l.startswith('END') for l in T))
a, b, c = st(sys.argv[1]), st(sys.argv[2]), st(sys.argv[3])
for n, x in (('выключатель', a), ('новый', b), ('сверка', c)): need(x['end'], f'{n}: прогон дошёл до END')
keys = ('near03', 'clip_pieces_max', 'eye_inside', 'wall_front', 'wall_vox')
need(all(a['S'].get(k) == b['S'].get(k) for k in keys), 'поля CAMCLIPSTAT нового = выключателя: ' + ', '.join(f"{k} {a['S'].get(k)}={b['S'].get(k)}" for k in keys))
need(a['S'].get('near03', 0) > 0 and a['S'].get('clip_pieces_max', 0) > 0, 'проба не пустая: ближние куски в кадре были (near03 > 0, clip_pieces_max > 0)')
need(c['mis'] == 0, f"сверка обоими способами: расхождений {c['mis']}")
need(b['us'] <= 0.5 * a['us'], f"цена счётчика: новый {b['us']:.0f} мкс Мака на кадр, выключатель {a['us']:.0f} (≈ {a['us']*5.8/1000:.2f} → {b['us']*5.8/1000:.2f} мс PSP)")
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
