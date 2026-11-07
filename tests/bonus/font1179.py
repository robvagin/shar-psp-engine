#!/usr/bin/env python3
"""№1179 (повтор №1145): грани, которые ищет бонус-гонка по точному имени (viewer/bonus_play.h, uiFontFindExact), должны существовать в пакетах так, как они там названы.
В TEST88 код искал «bart_16bit», а в пакете грань называется «bart_16bit@1»: поиск не находил, HUD гонки оставался прежним шрифтом с плашкой.
  python3 tests/bonus/font1179.py ui.shr bonus/mini.shr [--old]     --old: проверить прежние имена (красный контроль, обязан упасть)"""
import re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
src = (ROOT / 'viewer/bonus_play.h').read_text(encoding='utf-8')
pairs = re.findall(r'uiFontFindExact\(BONUSFONT1179_OLD \? "([^"]+)" : "([^"]+)"\)', src)
assert len(pairs) == 2, pairs      # bart_16bit (HUD, подписи) и msx_thin (строки итогов)
packs = {'bart': Path(sys.argv[1]).read_bytes(), 'msx': Path(sys.argv[2]).read_bytes()}   # bart — в основном пакете (ui.shr), msx_thin — в пакете бонус-игры
old = '--old' in sys.argv
bad = 0
for (o, n), key in zip(pairs, ('bart', 'msx')):
    name = o if old else n
    ok = (b'\0' + name.encode() + b'\0') in packs[key]
    print('%s %-14s %s в %s' % ('ok  ' if ok else 'FAIL', name, 'есть' if ok else 'НЕТ', 'ui.shr' if key == 'bart' else 'mini.shr')); bad |= not ok
print('FAIL' if bad else 'PASS'); sys.exit(bad)
