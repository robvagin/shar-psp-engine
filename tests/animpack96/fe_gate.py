#!/usr/bin/env python3
"""Модели-96: Гомер главного меню (fe.shr: смещение @60, размер @64, CHR1) новый против старого, гейт gate.py. Красный контроль --scale 0.01. fe_gate.py OLD_fe.shr NEW_fe.shr"""
import sys, struct, tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import gate
def main():
    k = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else 1.0
    with tempfile.TemporaryDirectory() as td:
        ps = []
        for i, f in enumerate(sys.argv[1:3]):
            b = Path(f).read_bytes(); off, size = struct.unpack_from('<II', b, 60); p = Path(td) / f'h{i}.chr'; p.write_bytes(b[off:off + size]); ps.append(p)
        ok, msg, _ = gate.compare(str(ps[0]), str(ps[1]), k)
        print(f'Гомер меню: {ps[0].stat().st_size:,} -> {ps[1].stat().st_size:,} Б; {msg}'); print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
main()
