#!/usr/bin/env python3
"""Модели-96: костюмы-гэги со скелетом (пакет collect-lN.shr, секция скинов: u32 число @56, u32 смещение @60; запись 64 Б: смещение CHR1, размер, гэг).
Каждый CHR1 нового пакета против старого: гейт gate.py (настоящий skin.c). Красный контроль --scale 0.01.   gag_gate.py OLD.shr NEW.shr"""
import sys, struct, tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import gate

def skins(p):
    b = Path(p).read_bytes(); ns, so = struct.unpack_from('<II', b, 56); out = []
    for i in range(ns):
        off, size, idx = struct.unpack_from('<IIH', b, so + 64 * i); out.append((idx, b[off:off + size]))
    return out

def main():
    old, new = sys.argv[1], sys.argv[2]; k = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1.0
    a, b = skins(old), skins(new); assert len(a) == len(b) and a, 'число скинов'
    exe = gate.build(); bad = 0; wr = wt = wp = 0.; so = sn = 0
    with tempfile.TemporaryDirectory() as td:
        for i, ((ga, da), (gb, db)) in enumerate(zip(a, b)):
            assert ga == gb
            po, pn = Path(td) / 'o.chr', Path(td) / 'n.chr'; po.write_bytes(da); pn.write_bytes(db); so += len(da); sn += len(db)
            ok, msg, w = gate.compare(str(po), str(pn), k, exe)
            if not ok: bad += 1; print(f'скин {i} гэг {ga}: {msg}')
            wr, wt, wp = max(wr, w['rot']), max(wt, w['tr'], w['root']), max(wp, w['pos'])
    print(f'скинов гэгов {len(a)}: макс. поворот {wr:.3f}°, перенос {wt:.3f} мм, положение сустава {wp:.3f} мм; байт {so:,} -> {sn:,}; нарушений {bad}')
    print('PASS' if not bad else 'FAIL'); sys.exit(1 if bad else 0)

main()
