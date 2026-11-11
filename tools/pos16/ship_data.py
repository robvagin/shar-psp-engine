#!/usr/bin/env python3
"""Модели-96, s16: собрать 82 файла персонажей (игроки, костюмы, персонажи миссий, пешеходы, меню) ПО ПРАВИЛАМ tools/ship_rules.txt (те же команды и флаги качества)
в каталог набора выпуска по тем же путям, что в наборе (homer.shr, l2/player.shr, chars/*.chr, traffic.shr, l2/traffic.shr, fe.shr ...), и записать список с sha256.
  ship_data.py OUT_DIR MANIFEST.txt [--old]      (--old: CHARPOS16_OLD=1, те же файлы в старом виде f32)"""
import hashlib, os, re, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
out, man = Path(sys.argv[1]), Path(sys.argv[2]); old = '--old' in sys.argv
want = re.compile(r'^(homer\.shr|homer-h_\w+\.shr|traffic\.shr|fe\.shr|chars/[\w.]+|l\{N\}/(player|skin[123]|traffic)\.shr)$')
rules = []
for ln in (ROOT / 'tools/ship_rules.txt').read_text().splitlines():
    if ln.startswith('#') or '|' not in ln: continue
    f = [x.strip() for x in ln.split('|')]
    if len(f) >= 4 and want.match(f[0]): rules.append((f[0], re.sub(r'\s+#.*$', '', f[3])))
jobs = []
for name, cmd in rules:
    if '{N}' in name:
        for n in range(2, 8): jobs.append((name.replace('{N}', str(n)), cmd.replace('{N}', str(n))))
    else: jobs.append((name, cmd))
env = dict(os.environ, PATH=f'{ROOT}/.venv/bin:' + os.environ['PATH']);
if old: env['CHARPOS16_OLD'] = '1'
rows = []
for name, cmd in jobs:
    dst = out / name; dst.parent.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(['nice', '-n', '20', 'bash', '-c', cmd.replace('{out}', str(dst))], cwd=ROOT, env=env, capture_output=True, text=True)
    if r.returncode or not dst.is_file(): print(f'FAIL {name}: {r.stderr[-300:]}'); sys.exit(1)
    rows.append((hashlib.sha256(dst.read_bytes()).hexdigest(), dst.stat().st_size, name)); print(name, dst.stat().st_size, flush=True)
man.write_text(f'# Модели-96, позиции вершин s16: {len(rows)} файлов, tools/pos16/ship_data.py, коммит mod96 (см. git log), sha256 размер путь\n' + ''.join(f'{h}  {s}  {n}\n' for h, s, n in sorted(rows, key=lambda x: x[2])))
print('ИТОГО', len(rows))
