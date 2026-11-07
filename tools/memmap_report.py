#!/usr/bin/env python3
"""Кто держит кучу: расшифровка строк MEMMAP / MEMMAPRA из blackbox.txt или smoke.txt (TEST90, viewer/memmap.h).
  python3 tools/memmap_report.py ФАЙЛ_ЯЩИКА_ИЛИ_ЖУРНАЛА --elf ELF_ЭТОЙ_СБОРКИ      (ELF выпуска: build/crash/elf/<SHA>.elf или viewer/build*/sharview.elf)
Что печатает: последнюю строку MEMMAP (свободно, наибольший кусок, живых блоков, классы размера) и по каждой группе вызывающего (ra) — функцию из ELF, число блоков, килобайты и владельца:
уровень (level/lva/coll/fx), клетки (loadCell/cell/stream), NPC (skin/chars/ped/traffic), миссия/art (msn/mv/mcr/gag), комнаты (room), звук (audio/snd/sab), интерфейс (ui/pak), прочее.
Адрес в журнале — рабочий; смещение от базы = ra − ref + адрес memmapReport в ELF (ref печатается в первой строке)."""
import argparse, re, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
OWN = [('клетки', r'loadCell|cellsRead|cpAlloc|unloadCell|cellPool|ggBatch|streamCell|cellsKeep|propBatch'), ('звук', r'audio|snd|sab|mixer|amalloc'), ('интерфейс', r'^ui|pak|uiScr|uiInit|font'),
       ('комнаты', r'room|interior'), ('NPC', r'skin|chars|ped|traffic|homer|carLt|npc'), ('миссия/art', r'msn|mv[A-Z]|mcr|gag|coll|msnart|phone|shop'), ('уровень', r'level|lva|col|prop|fx|svdata|drip|tex|pvs|road')]
def owner(fn):
    for name, rx in OWN:
        if re.search(rx, fn, re.I): return name
    return 'прочее'
def main():
    ap = argparse.ArgumentParser(); ap.add_argument('log'); ap.add_argument('--elf', required=True); ap.add_argument('--nm', default=str(ROOT / 'pspdev/bin/psp-nm')); ap.add_argument('--a2l', default=str(ROOT / 'pspdev/bin/psp-addr2line')); a = ap.parse_args()
    lines = [l for l in Path(a.log).read_text(errors='replace').splitlines() if 'MEMMAP' in l]
    if not lines: print('FAIL: нет строк MEMMAP (сборка без TEST90 MEMMAP или файл memmapold)'); return 1
    # последний блок: строка MEMMAP и две MEMMAPRA после неё
    idx = max(i for i, l in enumerate(lines) if re.search(r'MEMMAP t=', l)); blk = lines[idx:idx + 3]
    head = blk[0]; print(head[head.index('MEMMAP'):])
    ref = int(re.search(r'ref=([0-9a-f]+)', head).group(1), 16)
    nm = subprocess.run([a.nm, a.elf], capture_output=True, text=True).stdout
    m = re.search(r'^([0-9a-f]+) [Tt] memmapReport$', nm, re.M)
    if not m: print('FAIL: в ELF нет memmapReport (не тот ELF)'); return 1
    e0 = int(m.group(1), 16); tot = {}
    for l in blk[1:]:
        for ra, n, kb in re.findall(r'ra=([0-9a-f]+) n=(\d+) kb=(\d+)', l):
            off = int(ra, 16) - ref + e0
            fn = subprocess.run([a.a2l, '-e', a.elf, '-f', '-C', '-p', hex(off)], capture_output=True, text=True).stdout.strip().split(' ')[0]
            o = owner(fn); tot[o] = tot.get(o, 0) + int(kb); print(f'  {fn:40} {o:10} n={n:>5} {kb:>6} КБ (ra {ra})')
    print('по владельцам (только показанные группы):', ', '.join(f'{k} {v} КБ' for k, v in sorted(tot.items(), key=lambda x: -x[1])))
    return 0
if __name__ == '__main__': sys.exit(main())
