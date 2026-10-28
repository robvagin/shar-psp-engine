#!/usr/bin/env python3
"""№971: боковые файлы набора данных совпадают с выходом генераторов на текущем коде (устаревшие не проскочат в выпуск).
  python3 tools/check_sidecars.py SET_DIR [DATA_DIR] [--only carflat,flat912,noise949,edge983,road934,horizon,zones1007] [--out ПАПКА] [--texsrc КОРЕНЬ[,КОРЕНЬ…]] [--names texnames.json]
SET_DIR — папка игры с исходниками генераторов (level.shr, notoon.txt, traffic.shr, msncars-lN.shr; l2 … l7/); DATA_DIR — где лежат проверяемые файлы той же раскладки (по умолчанию SET_DIR; у выпуска build/tN/data).
Файлы: carflat930.bin и carflatm930.bin (tools/car_flat.py), flat912.bin (tools/flat_list.py, как в наборе: без вида 3, асфальт родной), noise949.bin (tools/noise949_list.py), edge983.bin и pathlines983.shr (tools/edge983_list.py, №983), road934.shr (tools/road934_export.py: нужны
level.shr.texsrc.json уровня, --texsrc или ROAD934_TEXSRC: корни, где лежит lN/level.shr.texsrc.json, у уровня 1 level.shr.texsrc.json в корне, и словарь имён --names / ROAD934_NAMES), horizon.shr (tools/horizon_export.py, ~6 мин на уровень,
семь уровней параллельно). С --out заново построенные файлы остаются в папке той же раскладки (для подмены разошедшихся). Код возврата 0 — всё совпало.
Причина появления: в TEST61…TEST66 лежали боковые файлы машин, собранные до №956 и до №972; ship_rules.txt их пересобирает, а наборы test6N_set.sh брали готовые из build/t61/data.
marks912.bin здесь не проверяется: правило ship_rules.txt (marks_list.py --emit flat) устарело, см. отчёт."""
import os, shutil, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
KINDS = ['carflat', 'flat912', 'noise949', 'edge983', 'road934', 'horizon', 'zones1007']


def run(cmd, env=None):
    e = dict(os.environ); e.update(env or {})
    r = subprocess.run([sys.executable, '-I'] + [str(c) for c in cmd], capture_output=True, text=True, env=e, cwd=str(HERE.parent))
    return r.returncode == 0


def opt(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def main():
    pos = [a for i, a in enumerate(sys.argv[1:], 1) if not a.startswith('--') and sys.argv[i - 1] not in ('--only', '--out', '--texsrc', '--names')]
    D = Path(pos[0]); H = Path(pos[1]) if len(pos) > 1 else D; only = opt('--only', ','.join(KINDS)).split(',')
    tmp = Path(opt('--out') or tempfile.mkdtemp(prefix='sidecars_')); tmp.mkdir(parents=True, exist_ok=True)
    roots = [Path(x) for x in (opt('--texsrc') or os.environ.get('ROAD934_TEXSRC', '')).split(',') if x]
    names = opt('--names') or os.environ.get('ROAD934_NAMES', '')
    jobs = []
    for n in range(1, 8):
        rel = '' if n == 1 else f'l{n}/'; d = D / rel.rstrip('/') if n > 1 else D; (tmp / rel).mkdir(parents=True, exist_ok=True)
        def add(kind, name, src, cmd):
            if kind in only: jobs.append((kind, rel + name, src, tmp / rel / name, H / rel / name, cmd))
        add('carflat', 'carflat930.bin', d / 'traffic.shr', [HERE / 'car_flat.py', d / 'traffic.shr', '--sidecar', tmp / rel / 'carflat930.bin'])
        add('carflat', 'carflatm930.bin', D / rel / f'msncars-l{n}.shr', [HERE / 'car_flat.py', D / rel / f'msncars-l{n}.shr', '--sidecar', tmp / rel / 'carflatm930.bin'])
        add('flat912', 'flat912.bin', d / 'level.shr', [HERE / 'flat_list.py', d / 'level.shr', d / 'notoon.txt', tmp / rel / 'flat912.bin'])
        add('noise949', 'noise949.bin', d / 'level.shr', [HERE / 'noise949_list.py', d / 'level.shr', tmp / rel / 'noise949.bin'])
        add('edge983', 'edge983.bin', d / 'level.shr', [HERE / 'edge983_list.py', d / 'level.shr', d / 'toonlines.shr', tmp / rel / 'edge983.bin', '--only', 'edge'])   # №983
        if (d / 'toonlines.shr').exists(): add('edge983', 'pathlines983.shr', d / 'level.shr', [HERE / 'edge983_list.py', d / 'level.shr', d / 'toonlines.shr', tmp / rel / 'pathlines983.shr', '--only', 'path'])
        add('zones1007', 'zones1007p.bin', d / 'level.shr', [HERE / 'zones1007_list.py', d / 'level.shr', H / rel / 'flat912.bin', HERE.parent / 'tests' / 'zones1007' / 'finals' / f'l{n}.txt', tmp / rel / 'zones1007p.bin'])   # №1007б: кусок → множитель цвета вершин (тона — из дампа движка tests/zones1007/finals)
        ts = next((r / rel / 'level.shr.texsrc.json' for r in roots if (r / rel / 'level.shr.texsrc.json').exists()), None)
        add('road934', 'road934.shr', d / 'level.shr', [HERE / 'road934_export.py', d / 'level.shr', tmp / rel / 'road934.shr'] + (['--texsrc', ts] if ts else []) + (['--names', names] if names else []))
        add('horizon', 'horizon.shr', d / 'level.shr', [HERE / 'horizon_export.py', d / 'level.shr', '--out', tmp / rel / 'horizon.shr', '--cell', 60, '--lvl', n, '--near', 130, '--near2', 150, '--radius', 134])
    def one(j):
        kind, name, src, out, have, cmd = j
        if not src.exists(): return name, 'нет исходного ' + src.name
        if not have.exists(): return name, 'нет файла в наборе'
        if kind == 'road934' and '--texsrc' not in cmd: return name, 'нет level.shr.texsrc.json (--texsrc)'
        if not run(cmd) or not out.exists(): return name, 'генератор упал'
        return name, 'совпал' if out.read_bytes() == have.read_bytes() else 'УСТАРЕЛ (отличается от выхода генератора)'
    with ThreadPoolExecutor(14) as ex: res = list(ex.map(one, jobs))
    bad = 0
    for name, st in res:
        print(f'  {name:22s} {st}'); bad += st != 'совпал'
    print(f'файлов {len(res)}, не совпало {bad}; построенное в {tmp}'); sys.exit(1 if bad else 0)


main()
