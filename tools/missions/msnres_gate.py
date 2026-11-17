#!/usr/bin/env python3
"""Гейт списка ресурсов миссий (запекание З9): всё, что движок реально читал на миссии, есть в msnres-l<N>.shr этой миссии.

  python tools/missions/msnres_gate.py SMOKE.txt msnres-l1.shr [--drop art:путь|car:имя|chr:имя]... [--level-wide путь]...

SMOKE.txt — smoke.txt прогона (tools/smoke.sh): «MSNREQ art путь» (109: полный путь записи msnart, одна диагностическая строка), «BBOP car+ имя», «BBOP npc+ имя»
(чёрный ящик движка пишет их при каждом чтении с карты памяти) и «BBOP msn <миссия>.<фаза>.<этап>» (текущая миссия). Поведение игры строки не меняют.
Код выхода: 0 — запросов вне списка нет; 1 — есть (печатаются), 2 — в журнале нет ни одного запроса миссии (гейт ничего не проверил).
--drop — красный контроль: убрать запись из списка в памяти; гейт обязан покраснеть, если миссия эту запись просила.
Запросы до первой строки «msn» (уровневые: стрелки, терра, общие значки) и пути из --level-wide в сравнение не входят.
"""
import re, struct, sys
from pathlib import Path

K = {1: 'art', 2: 'car', 3: 'chr', 4: 'art'}

def load(path):
    f = Path(path).read_bytes()
    magic, ver, level, nm, ne, off, h, _ = struct.unpack_from('<4sIIIIIII', f, 0)
    assert magic == b'MRS1', 'не MRS1'
    ents = []
    for i in range(ne):
        kind, phase, stage, miss, size, key = struct.unpack_from('<BBBBI88s', f, off + i * 96)
        ents.append((K[kind], key.split(b'\0')[0].decode().lower()))
    ms = {}
    for i in range(nm):
        mid, first, n = struct.unpack_from('<8sII', f, 32 + 40 * i)
        ms[mid.split(b'\0')[0].decode()] = set(ents[first:first + n])
    return level, ms

def main():
    a = sys.argv[1:]; drops = []; lw = []
    while '--drop' in a:
        i = a.index('--drop'); drops.append(a[i + 1]); del a[i:i + 2]
    while '--level-wide' in a:
        i = a.index('--level-wide'); lw.append(a[i + 1].lower()); del a[i:i + 2]
    smoke, res = a[0], a[1]
    level, ms = load(res)
    for d in drops:
        kind, key = d.split(':', 1)
        for s in ms.values():
            s.discard((kind, key.lower()))
    cur = None; asked = ok = 0; out = []
    for line in Path(smoke).read_text(errors='replace').splitlines():
        m = re.match(r'(BBOP|MSNREQ) (\S+) (\S+)', line)
        if not m:
            continue
        if m.group(1) == 'MSNREQ':   # полный путь записи art (BBOP art+ режет имя на 34 знака)
            op, arg = 'art+', m.group(3)
        else:
            op, arg = m.group(2), m.group(3)
            if op == 'art+':   # то же чтение уже видно как MSNREQ
                continue
        if op == 'msn':
            cur = arg.split('.')[0]; continue
        if cur is None or op not in ('art+', 'car+', 'npc+'):
            continue
        kind = {'art+': 'art', 'car+': 'car', 'npc+': 'chr'}[op]
        key = arg.replace('\\', '/').lower()
        if key in lw:
            continue
        asked += 1
        if (kind, key) in ms.get(cur, set()) or (kind, key) in ms.get('L', set()):
            ok += 1
        else:
            out.append((cur, kind, key))
    print(f'MSNRES gate: запросов миссий {asked}, в списке {ok}, вне списка {len(out)}')
    for o in out:
        print('  вне списка:', *o)
    if not asked:
        print('  ПУСТО: журнал без запросов миссии, гейт ничего не доказал'); sys.exit(2)
    sys.exit(1 if out else 0)

if __name__ == '__main__':
    main()
