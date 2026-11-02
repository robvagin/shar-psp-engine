#!/usr/bin/env python3
"""Приёмка круга 2/4 окна g12-data (02.11.2026), без эмулятора: python3 tests/g12/round4_gate.py [build/sv12b]
Проверяет: таблицы JDIR и DYN в svdata-lN.shr (P01-07, P12-10), клипы дверей пассажира и радости водителя в player.shr (P05 Д3, P05-1),
отчёты sv_overlay.py всех 7 уровней (таблица треугольников основы TEST81 == нового экспорта, ни одного отказа), байт множителя ставки
в msncars (P06-3). Красные контроли: те же проверки при SVDATA_JDIR_OLD=1 / SVDATA_DYN_OLD=1 / HOMER_CLIPS_OLD=1 обязаны упасть."""
import os, struct, subprocess, sys, re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]; D = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/sv12b'
PY = sys.executable; fails = []


def check(name, ok):
    print(('PASS ' if ok else 'FAIL ') + name)
    if not ok: fails.append(name)


def svd(level, env=None):
    out = ROOT / f'build/.gate_sv{level}.shr'
    subprocess.run([PY, str(ROOT / 'tools/svdata_export.py'), '--level', str(level), '--out', str(out)], check=True, capture_output=True, env={**os.environ, **(env or {})})
    return out.read_bytes()


for lv, nj in ((1, 16), (3, 5)):
    b = svd(lv); h = struct.unpack_from('<16I', b, 0)
    ok = h[11] == nj and all(struct.unpack_from('<16s3f3fII', b, h[12] + i * 48)[7] == 1 for i in range(nj))
    d = [struct.unpack_from('<3f', b, h[12] + i * 48 + 28) for i in range(nj)]
    ok = ok and all(abs(sum(x * x for x in v) - 1) < 1e-4 for v in d)
    check(f'P01-07 L{lv}: {nj} курсов зон прыжка, единичные, валидны', ok)
    h0 = struct.unpack_from('<16I', svd(lv, {'SVDATA_JDIR_OLD': '1'}), 0)
    check(f'P01-07 L{lv}: красный контроль SVDATA_JDIR_OLD=1 даёт 0 курсов', h0[11] == 0)
    nd = h[13]; recs = [struct.unpack_from('<16s4I', b, h[14] + i * 32) for i in range(nd)]
    names = [r[0].split(b'\0')[0] for r in recs if r[1] == 0xFFFFFFFF]; masks = [r for r in recs if r[1] != 0xFFFFFFFF]
    check(f'P12-10 L{lv}: наборов {len(masks)}, регионов {len(names)}, маски в пределах числа регионов', masks and names and all(0 < r[2] < (1 << len(names)) for r in masks))
    check(f'P12-10 L{lv}: красный контроль SVDATA_DYN_OLD=1 даёт 0 записей', struct.unpack_from('<16I', svd(lv, {'SVDATA_DYN_OLD': '1'}), 0)[13] == 0)


def clips(env=None):
    r = subprocess.run([PY, str(ROOT / 'tools/homer_export.py'), '--level', '2', '--out', str(ROOT / 'build/.gate_pl.shr')], capture_output=True, text=True, env={**os.environ, **(env or {})})
    m = re.search(r'(\d+) clips', r.stdout); return int(m.group(1)) if m else -1


n_new, n_old = clips(), clips({'HOMER_CLIPS_OLD': '1'})
check(f'P05 Д3: клипов Гомера/Барта 68 (дверь пассажира 12 + радость за рулём 2 + взгляд назад) против 53 прежних: {n_new}/{n_old}', n_new == 68 and n_old == 53)
rep_ok = True
for lv in range(1, 8):
    f = D / f'l{lv}' / f'overlay-l{lv}.txt'
    t = f.read_text(encoding='utf-8') if f.exists() else ''
    good = 'ПОБАЙТНО ОДИНАКОВА' in t and 'ОТКАЗ' not in t and 'COL1 основы НЕ тронут' in t
    check(f'sv_overlay L{lv}: треугольники совпали, отказов нет, COL1 не тронут', good)
for lv in range(1, 8):   # msncars до круга (build/sv12) против нового: отличаются только байты множителя ставки (10/20/30...), длина та же
    pa, pb = ROOT / f'build/sv12/l{lv}/msncars-l{lv}.shr', D / f'l{lv}' / f'msncars-l{lv}.shr'
    if not (pa.exists() and pb.exists()): check(f'P06-3 L{lv}: msncars есть', False); continue
    a, b = pa.read_bytes(), pb.read_bytes()
    d = [i for i in range(min(len(a), len(b))) if a[i] != b[i]] if len(a) == len(b) else None
    check(f'P06-3 L{lv}: msncars отличаются только байтом ставки ({len(d) if d else 0} машин, значения {sorted({b[i] for i in d})[:4] if d else []})',
          bool(d) and all(a[i] == 0 and b[i] % 10 == 0 and 0 < b[i] <= 255 for i in d) and len({i % 128 for i in d}) == 1)
print('ИТОГ:', 'ЗЕЛЁНЫЙ' if not fails else f'КРАСНЫЙ, {len(fails)}')
sys.exit(1 if fails else 0)
