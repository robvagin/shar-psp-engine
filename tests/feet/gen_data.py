#!/usr/bin/env python3
"""Модели-96: собрать ТОЛЬКО файлы с моделями (CHR1) по правилам tools/ship_rules.txt в каталог DATA (структура набора: корень, chars/, l{N}/) — для гейтов tests/feet.
Каждый экспорт идёт через tools/char_export.py / traffic_export.py, то есть с запеканием поправки стоп (tools/footbake.c). FOOTBAKE_OFF=1 — без запекания (красный контроль).
  gen_data.py DATA [--levels 1 2 ...]"""
import os, re, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]; os.chdir(ROOT)
out = Path(sys.argv[1]); levels = [int(x) for x in sys.argv[sys.argv.index('--levels') + 1:]] if '--levels' in sys.argv else list(range(1, 8))
want = re.compile(r'^(chars/[^/]+\.chr|homer[^/]*\.shr|traffic\.shr|l\{N\}/(player|skin\d|traffic)\.shr)$')
env = dict(os.environ, SHR_TEX_CAP256='1', SHR_SMALL_MIPS='1', SHR_LAYERS='1')
fail = 0; n = 0
for line in (ROOT / 'tools/ship_rules.txt').read_text().splitlines():
    if line.startswith('#') or '|' not in line: continue
    f = [x.strip() for x in line.split('|')]
    if len(f) < 4 or not want.match(f[0]): continue
    cmd = re.sub(r'^(?:\w+=\S+\s+)+', '', f[3]).split('#')[0].strip()
    for N in (levels if '{N}' in f[0] else [0]):
        name = f[0].replace('{N}', str(N)); dst = out / name; dst.parent.mkdir(parents=True, exist_ok=True)
        c = cmd.replace('{N}', str(N)).replace('{out}', str(dst)).replace('python ', sys.executable + ' ', 1)
        r = subprocess.run(c, shell=True, env=env, capture_output=True, text=True); n += 1
        if r.returncode or not dst.exists(): fail += 1; print('FAIL', name, (r.stderr or r.stdout)[-200:].strip())
print('gen_data: %d файлов, сбоев %d' % (n, fail)); sys.exit(1 if fail else 0)
