#!/usr/bin/env python3
"""1355: бот-проходчик по миссиям уровня (L4 по умолчанию): сценарии безголового эмулятора, по одному на миссию.
  python3 tests/bot1355/gen.py OUTDIR [--level 4] [--cycles 22] [--pass 25]
Миссия начинается с начала (flag msnsel k для сюжетных, flag msnmainat IDX*100 для уличных гонок sr1–sr3, flag coins 99 для gr1, bm1 с этапа 0). Цикл прохода (~7 с): flag msngofar 35 (игрок подъезжает к цели с расстояния
35 м, не в точку), строка AUDIT1355 (цель, метка, радар, расстояния, загружена ли), flag msngo (подойти), ждём, ещё AUDIT1355. Реплики, брифинг, △ у собеседника и двери, посадку делает помощник движка flag msnbot 1; этап,
стоящий дольше --pass секунд, помощник засчитывает сам (MSN pass ... why=stuck) — тогда в таблице «засчитан сам»."""
import sys
from pathlib import Path
out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
L = int(sys.argv[sys.argv.index('--level') + 1]) if '--level' in sys.argv else 4
CYC = int(sys.argv[sys.argv.index('--cycles') + 1]) if '--cycles' in sys.argv else 22
PASS = sys.argv[sys.argv.index('--pass') + 1] if '--pass' in sys.argv else '25'
STORY = {1: 8, 2: 8, 3: 7, 4: 7, 5: 8, 6: 8, 7: 8}
BONUS = [('sr1', 0, []), ('sr2', 0, []), ('sr3', 0, []), ('gr1', 0, ['flag coins 99']), ('bm1', 0, [])]
head = ['scene F_L1', 'flag cfgdefaults 1', 'flag repair', 'wait 6', f'scene F_GO{L}_L{L}', f'flag golevel {L}', 'wait 25', 'flag msn', 'flag msnbot 1', f'flag msnbotpass {PASS}', 'wait 3']
cycle = ['flag msngofar 35', 'wait 2', 'flag msnaudit', 'flag msngo', 'wait 3', 'flag msnaudit', 'wait 2']
jobs = [(t, ['flag cfgdefaults 1'] + pre + [f'flag msnmainat {(STORY[L] + k) * 100 + st}']) for k, (t, st, pre) in enumerate(BONUS)] + [(f'm{k + 1}', [f'flag msnsel {k}']) for k in range(STORY[L] - (1 if L == 7 else 0))]
for tag, start in jobs:
    s = head + [f'scene MS_{tag}_start'] + start + ['wait 3', 'flag msnaudit']
    for c in range(CYC): s += [f'scene MS_{tag}_c{c:02d}'] + cycle
    (out / f'L{L}_{tag}.txt').write_text('\n'.join(s + ['end']) + '\n')
print(len(jobs), 'сценариев в', out)
