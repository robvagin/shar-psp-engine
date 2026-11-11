#!/usr/bin/env python3
"""Модели-96, позиции вершин f32 -> s16: перебор ВСЕХ персонажей (игроки L1–L7 с костюмами, персонажи миссий, пешеходы по уровням).
Каждая пара файлов СТАРЫЙ/НОВЫЙ идёт через tests/pos16/cmp.c (настоящие skin.c и traffic.c). Итог: таблица, число битых моделей (должно быть 0).
Запуск: tests/pos16/run_all.py OLD_DIR NEW_DIR [--red]      (--red: POS16_RED=1, красный контроль, обязан дать FAIL)"""
import os, re, struct, subprocess, sys, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
old, new = Path(sys.argv[1]), Path(sys.argv[2]); red = '--red' in sys.argv
exe = Path(tempfile.mkdtemp()) / 'cmp'
subprocess.run(['cc', '-std=gnu99', '-O2', '-w', '-DSKIN_HOST', '-Iviewer', 'tests/pos16/cmp.c', 'viewer/skin.c', 'viewer/traffic.c', 'viewer/car.c', 'viewer/col.c', 'viewer/watchdog.c', '-lm', '-o', str(exe)], cwd=ROOT, check=True)
env = dict(os.environ); 
if red: env['POS16_RED'] = '1'
jobs = [('skin', p) for p in sorted(old.glob('*.shr')) if p.name != 'fe.shr'] + [('skin', p) for p in sorted((old / 'chars').glob('*.chr'))] + [('traffic', p) for p in sorted((old / 'tr').glob('traffic*.shr'))]
def fe_models(path):
    """CHR1 внутри fe.shr (Гомер меню и гэги меню): по метке и размеру в заголовке"""
    b = path.read_bytes(); out = []; i = b.find(b'CHR1')
    while i >= 0:
        size = struct.unpack_from('<I', b, i + 8)[0]
        if size > 128 and i + size <= len(b): out.append(b[i:i + size])
        i = b.find(b'CHR1', i + 4)
    return out
rows = []; bad = 0; tot_old = tot_new = 0; maxp = maxn = maxl = 0.; nmodels = 0
for kind, a in jobs:
    b = new / a.relative_to(old)
    if not b.is_file(): print(f'НЕТ нового: {b}'); bad += 1; continue
    r = subprocess.run([str(exe), kind, str(a), str(b)], capture_output=True, text=True, env=env, cwd=ROOT)
    out = r.stdout.strip().splitlines(); last = out[-1] if out else '?'
    m = re.search(r'моделей (\d+).*файл (\d+) -> (\d+) Б; макс. сдвиг позиции ([\d.]+) мм.*нормали ([\d.]+)°.*как векторов ([\d.]+) %', '\n'.join(out))
    ok = last == 'PASS' and r.returncode == 0
    if not ok: bad += 1; print(f'FAIL {a.relative_to(old)}:\n  ' + '\n  '.join(out[-6:]))
    if m:
        n, so, sn, pm, nm = int(m[1]), int(m[2]), int(m[3]), float(m[4]), float(m[5]); nmodels += n; tot_old += so; tot_new += sn; maxp = max(maxp, pm); maxn = max(maxn, nm); maxl = max(maxl, float(m[6]))
        rows.append((str(a.relative_to(old)), n, so, sn, pm, nm, ok))
if (old / 'fe.shr').is_file():                       # Гомер меню и гэги меню: каждый CHR1 из fe.shr отдельным файлом
    ma, mb = fe_models(old / 'fe.shr'), fe_models(new / 'fe.shr')
    if len(ma) != len(mb) or not ma: print(f'FAIL fe.shr: CHR1 {len(ma)} / {len(mb)}'); bad += 1
    td = Path(tempfile.mkdtemp())
    for k, (x, y) in enumerate(zip(ma, mb)):
        (td / f'a{k}.chr').write_bytes(x); (td / f'b{k}.chr').write_bytes(y)
        r = subprocess.run([str(exe), 'skin', str(td / f'a{k}.chr'), str(td / f'b{k}.chr')], capture_output=True, text=True, env=env, cwd=ROOT)
        out = r.stdout.strip().splitlines(); last = out[-1] if out else '?'
        m = re.search(r'моделей (\d+).*файл (\d+) -> (\d+) Б; макс. сдвиг позиции ([\d.]+) мм.*нормали ([\d.]+)°.*как векторов ([\d.]+) %', '\n'.join(out)); ok = last == 'PASS' and r.returncode == 0
        if not ok: bad += 1; print(f'FAIL fe.shr CHR1 #{k}:\n  ' + '\n  '.join(out[-6:]))
        if m:
            n, so, sn, pm, nm = int(m[1]), int(m[2]), int(m[3]), float(m[4]), float(m[5]); nmodels += n; tot_old += so; tot_new += sn; maxp = max(maxp, pm); maxn = max(maxn, nm)
            rows.append((f'fe.shr[{k}]', n, so, sn, pm, nm, ok))
print(f'{"файл":28s} {"мод.":>4s} {"было Б":>10s} {"стало Б":>10s} {"мм":>7s} {"град":>7s}')
for r in rows: print(f'{r[0]:28s} {r[1]:4d} {r[2]:10d} {r[3]:10d} {r[4]:7.3f} {r[5]:7.3f} {"" if r[6] else "FAIL"}')
print(f'ИТОГО: файлов {len(rows)}, моделей {nmodels}, {tot_old} -> {tot_new} Б ({tot_new - tot_old:+d}), худший сдвиг {maxp:.3f} мм, худшая нормаль {maxn:.3f}°, разница нормалей как векторов до {maxl:.3f} %, битых {bad}')
print('PASS' if bad == 0 and rows else 'FAIL'); sys.exit(0 if bad == 0 and rows else 1)
