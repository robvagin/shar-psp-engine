#!/usr/bin/env python3
"""Модели-96: каждый .chr каталога НОВЫЙ против СТАРОГО (одноимённые файлы), гейт gate.py. dir_gate.py OLD_DIR NEW_DIR [--scale K]"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import gate
def main():
    od, nd = Path(sys.argv[1]), Path(sys.argv[2]); k = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1.0
    exe = gate.build(); bad = 0; n = 0; so = sn = 0; wr = wt = wp = 0.
    for f in sorted(od.glob('*.chr')):
        g = nd / f.name
        if not g.exists(): print('нет нового', f.name); bad += 1; continue
        ok, msg, w = gate.compare(str(f), str(g), k, exe); n += 1; so += f.stat().st_size; sn += g.stat().st_size
        wr, wt, wp = max(wr, w['rot']), max(wt, w['tr'], w['root']), max(wp, w['pos'])
        if not ok: bad += 1; print(f.name, msg)
    print(f'{n} моделей: макс. поворот {wr:.3f}°, перенос {wt:.3f} мм, положение сустава {wp:.3f} мм; байт {so:,} -> {sn:,}; нарушений {bad}')
    print('PASS' if not bad and n else 'FAIL'); sys.exit(1 if bad or not n else 0)
main()
