#!/usr/bin/env python3
"""№1061: почему колесо машины не получило свою текстуру (причина пропуска tire_tex) — по именам машин"""
import json, os, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import psp_export as pe   # noqa: E402
from convert import Converter, GAME, OUT   # noqa: E402
conv = Converter()
for nm in sys.argv[1:]:
    tmp = f'tw1061_{os.getpid()}'; conv.export(GAME / 'art' / 'cars' / f'{nm}.p3d', tmp, vehicle=True)
    meta = json.loads((OUT / f'{tmp}.json').read_text()); raw = (OUT / f'{tmp}.bin').read_bytes()
    for e in ('json', 'bin'): (OUT / f'{tmp}.{e}').unlink()
    pe.TIRE_STAT.update(wheels=0, fixed=0, skipped=0); pe.TIRE_WHY.clear(); pe.car_prims(meta, raw, {}, [], False, None)
    print('WHY', nm, dict(pe.TIRE_STAT), sorted(set(pe.TIRE_WHY)), flush=True)
