#!/usr/bin/env python3
"""Модели-96: память до/после по уровням (позиции вершин f32 -> s16). mem_table.py OLD_DIR NEW_DIR
На уровне N в памяти: игрок (player + костюмы по выбору, берём самый большой) + пакет пешеходов (traffic<N>.shr) + персонажи миссий (chars/*.chr, грузятся по одному)
+ Гомер меню (fe.shr, не на уровне, а в меню)."""
import sys
from pathlib import Path
old, new = Path(sys.argv[1]), Path(sys.argv[2])
sz = lambda d, n: (d / n).stat().st_size
print('| уровень | игрок (самый большой костюм) | пакет пешеходов traffic | игрок + пешеходы |')
print('|---|---|---|---|')
to = tn = 0
for L in range(1, 8):
    names = sorted(p.name for p in old.glob(f'l{L}_*.shr'))
    po = max(sz(old, n) for n in names); pn = max(sz(new, n) for n in names)
    xo, xn = sz(old / 'tr', f'traffic{L}.shr'), sz(new / 'tr', f'traffic{L}.shr')
    print(f'| L{L} | {po:,} → {pn:,} ({pn - po:+,}) | {xo:,} → {xn:,} ({xn - xo:+,}) | {po + xo:,} → {pn + xn:,} ({pn + xn - po - xo:+,}) |'.replace(',', ' '))
    to += po + xo; tn += pn + xn
co = sum(p.stat().st_size for p in (old / 'chars').glob('*.chr')); cn = sum(p.stat().st_size for p in (new / 'chars').glob('*.chr'))
n = len(list((old / 'chars').glob('*.chr'))); mo = max(p.stat().st_size for p in (old / 'chars').glob('*.chr')); mn = max(p.stat().st_size for p in (new / 'chars').glob('*.chr'))
print(f'\nПерсонажи миссий ({n} моделей, на диске): {co:,} → {cn:,} ({cn - co:+,}); самый большой {mo:,} → {mn:,}'.replace(',', ' '))
if (old / 'fe.shr').is_file(): print(f'fe.shr (меню): {sz(old, "fe.shr"):,} → {sz(new, "fe.shr"):,} ({sz(new, "fe.shr") - sz(old, "fe.shr"):+,})'.replace(',', ' '))
