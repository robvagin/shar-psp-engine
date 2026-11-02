#!/usr/bin/env python3
"""Окно g07b-msnview, круг 6 (P16-07, оговорка 14 «строгий разбор данных»): режим стрелки цели в tools/missions/compile.py = то, что принимает оригинал.
Оригинал принимает слова both/b, neither/n, intersection/i, "nearest road" (нижний или верхний регистр целиком); всё остальное — не режим (стрелки по умолчанию, оба вида).
Проверяем на всех скриптах миссий L1–L7 диска: каждое слово последнего аргумента AddObjective, которое оригинал не узнаёт, у нас тоже не узнаётся (опечатка niether).
Красный контроль: --old возвращает прежнюю таблицу ('niether' = neither) и обязан упасть.
  navmodes_check.py [--old] [каталог scripts/missions]"""
import re, sys, importlib.util, pathlib
here = pathlib.Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('mcompile', here / 'tools/missions/compile.py'); mc = importlib.util.module_from_spec(spec); spec.loader.exec_module(mc)
old = '--old' in sys.argv
args = [a for a in sys.argv[1:] if a != '--old']
root = pathlib.Path(args[0]) if args else pathlib.Path.home() / 'Downloads/shar-psp/hit-and-run-web/source/game/scripts/missions'
if old: mc.NAV_MODES['niether'] = 1   # прежняя таблица
tok = lambda t: mc.nav_of_token(t)
# как принимает оригинал (точные строки; сравнение по нижнему регистру у нас шире — смешанного регистра в данных нет, проверяем ниже)
ORIG = {'both': 2, 'b': 2, 'neither': 1, 'n': 1, 'intersection': 4, 'i': 4, 'nearest road': 3}
def orig(t): return ORIG.get(t) if (t == t.lower() or t == t.upper()) else None
seen = {}; mixed = []
for f in sorted(root.rglob('*.mfk')):
    for m in re.finditer(r'AddObjective\s*\(([^)]*)\)', f.read_text(errors='replace'), re.I):
        parts = [p.strip().strip('"') for p in m.group(1).split(',')]
        for w in parts[1:]:
            wl = w.lower()
            if w != wl and w != w.upper() and wl in ORIG: mixed.append((f.name, w))
            if wl in ('neither', 'both', 'intersection', 'nearest road', 'niether', 'b', 'n', 'i') or tok(wl) is not None: seen.setdefault((f.parent.name + '/' + f.name, w), (tok(w.lower()), orig(w.lower())))
bad = [(k, v) for k, v in seen.items() if (v[0] if v[0] is not None else None) != v[1] and not (v[0] is None and v[1] is None)]
typo = [k for k in seen if k[1].lower() == 'niether']
print('слов режима в данных: %d, расхождений с оригиналом: %d, опечатка niether: %s, смешанный регистр: %d' % (len(seen), len(bad), typo, len(mixed)))
for k, v in bad: print('  РАСХОЖДЕНИЕ', k, 'у нас', v[0], 'в оригинале', v[1])
ok = not bad and typo and all(tok(k[1].lower()) is None for k in typo) and not mixed
print('OK' if ok else 'FAIL')
sys.exit(0 if ok else 1)
