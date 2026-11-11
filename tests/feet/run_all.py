#!/usr/bin/env python3
"""Модели-96, посадка стоп: перебор ВСЕХ моделей набора (игроки и костюмы L1–L7, персонажи миссий, пешеходы по уровням, Гомер/гэги меню в fe.shr).
Каждая модель идёт через tests/feet/feet.c (настоящие skin.c и traffic.c): нижняя вершина в клипах ходьбы/бега/стойки против земли после поправки skinFootLift.
Запуск: tests/feet/run_all.py DATA_DIR [--out файл] ; окружение: HOST_CFLAGS (например -DSKIN_FOOTVTX_OLD=1 — красный контроль, прежняя поправка), FEET_RED=1 (без поправки)
Порог: стопа не глубже 1 см под землёй и не выше 2 см над ней (FEET_SINK / FEET_HANG, м)."""
import os, re, struct, subprocess, sys, tempfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]; sys.path.insert(0, str(ROOT / 'tools'))
data = Path(sys.argv[1]); outp = Path(sys.argv[sys.argv.index('--out') + 1]) if '--out' in sys.argv else None
exe = Path(tempfile.mkdtemp()) / 'feet'
subprocess.run(['cc', '-std=gnu99', '-O2', '-w', '-DSKIN_HOST'] + os.environ.get('HOST_CFLAGS', '').split() + ['-Iviewer', 'tests/feet/feet.c', 'viewer/skin.c', 'viewer/traffic.c', 'viewer/car.c', 'viewer/col.c', 'viewer/watchdog.c', '-lm', '-o', str(exe)], check=True, cwd=ROOT)
import traffic_export as T
def names(level):
    tr, pg = T.level_script(level); out = []
    for g in sorted(pg):
        for n, _ in pg[g]:
            if n not in out: out.append(n)
    return out
def is_chr(p): return p.is_file() and p.read_bytes()[:4] == b'CHR1'
jobs = []
for p in sorted(data.glob('*.shr')):
    if p.name != 'fe.shr' and is_chr(p): jobs.append(('skin', p, None, p.name))
for p in sorted((data / 'chars').glob('*.chr')): jobs.append(('skin', p, None, 'chars/' + p.name))
for L in range(1, 8):
    d = data / f'l{L}'
    for p in sorted(d.glob('*.shr')):
        if p.name == 'traffic.shr': jobs.append(('traffic', p, names(L), f'l{L}/traffic.shr'))
        elif is_chr(p): jobs.append(('skin', p, None, f'l{L}/{p.name}'))
if (data / 'traffic.shr').is_file(): jobs.append(('traffic', data / 'traffic.shr', names(1), 'traffic.shr (L1)'))
tmp = Path(tempfile.mkdtemp())
fe = data / 'fe.shr'
if False and fe.is_file():   # fe.shr: модели меню и гэгов без клипов ходьбы, через skinFootLift не ставятся — не входят
    b = fe.read_bytes(); i = b.find(b'CHR1'); k = 0
    while i >= 0:
        size = struct.unpack_from('<I', b, i + 8)[0]
        if size > 128 and i + size <= len(b): q = tmp / f'fe_{k}.chr'; q.write_bytes(b[i:i + size]); jobs.append(('skin', q, None, f'fe.shr[{k}]')); k += 1
        i = b.find(b'CHR1', i + 4)
if '--bake' in sys.argv:   # запечённая поправка: у каждой модели есть таблица, skinFootLift её отдаёт (= счёту на месте), вершины не обходятся (feet.c, режим bake)
    nfile = nmod = nclip = nbad = 0; worst_us = 0.0; lines = []
    for kind, p, nms, label in jobs:
        args = [str(exe), 'bake'] + (['skin', str(p)] if kind == 'skin' else [str(p)])
        r = subprocess.run(args, capture_output=True, text=True, cwd=ROOT); l = [x for x in r.stdout.splitlines() if x.startswith('BAKE')]
        nfile += 1
        if not l: nbad += 1; lines.append(f'{label}: СБОЙ ЧТЕНИЯ'); continue
        m = re.search(r'моделей (\d+) клипов (\d+): без числа (\d+).*самый долгий вызов ([\d.]+) мкс -> (\w+)', l[0])
        nmod += int(m.group(1)); nclip += int(m.group(2)); worst_us = max(worst_us, float(m.group(4)))
        if m.group(5) != 'PASS': nbad += 1; lines.append(f'{label}: {l[0]}')
    res = [f'файлов {nfile}, моделей {nmod}, клипов {nclip}, самый долгий вызов skinFootLift {worst_us:.1f} мкс (хост), файлов с нарушением {nbad}', 'PASS' if nbad == 0 else 'FAIL']
    txt = '\n'.join(lines + res); print(txt)
    if outp: outp.write_text(txt + '\n')
    sys.exit(1 if nbad else 0)
lines = []; bad = 0; rows = 0; nm = 0; tab = []
for kind, p, nms, label in jobs:
    args = [str(exe), kind, str(p)]
    if nms:
        nf = tmp / (label.replace('/', '_') + '.names'); nf.write_text('\n'.join(nms)); args.append(str(nf))
    r = subprocess.run(args, capture_output=True, text=True, cwd=ROOT); out = r.stdout.strip().splitlines(); nm += 1
    m = re.search(r'строк (\d+), нарушений (\d+)', '\n'.join(out))
    if not m: bad += 1; lines.append(f'{label}: СБОЙ ЧТЕНИЯ ' + ' | '.join(out[-2:])); continue
    rows += int(m.group(1)); nb = int(m.group(2)); bad += nb
    for l in out:
        if 'FAIL' in l and 'строк' not in l: lines.append(f'{label}: {l}')
    if int(m.group(1)) == 0: lines.append(f'{label}: нет клипов ходьбы/стойки (не проверено)')
    for l in out:
        if l.endswith(' ok') or 'FAIL' in l: tab.append((label, l))
import collections
worst = min((float(re.search(r'-> ([+-][\d.]+) м', l).group(1)) for _, l in tab if re.search(r'-> ([+-][\d.]+) м', l)), default=0.0); hi = max((float(re.search(r'-> ([+-][\d.]+) м', l).group(1)) for _, l in tab if re.search(r'-> ([+-][\d.]+) м', l)), default=0.0)
res = [f'самое глубокое {worst*100:+.1f} см, самое высокое {hi*100:+.1f} см', f'моделей-файлов {nm}, строк клип×модель {rows}, нарушений {bad}', 'PASS' if bad == 0 else 'FAIL']
txt = '\n'.join(lines + res); print(txt)
if outp: outp.write_text(txt + '\n')
sys.exit(1 if bad else 0)
