#!/usr/bin/env python3
"""Гейт Т1.5 (docs/handoff/ДВА_ТЕСТИРОВЩИКА_2026-09-28.md §3): класс итога миссионера. Отрезки от спавна уровня 1.
  проходимый — к улице у Kwik-E-Mart (188, 279.9; обычный водитель доезжает); первый водитель нарочно слабый (руль наоборот):
      класс обязан быть «водитель», не «игра»; контроль без второго водителя (как до Т1.5) — «игра» (красный).
  непроходимый — точка в воздухе, 40 м над улицей (200, 45, 250): класс обязан быть «игра», не «водитель»; контроль —
      сборка -DMSN_LOOSE_ARRIVE=1 (приезд по плоскости, без высоты) — «водитель» (красный)."""
import os, subprocess, sys, time
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]; B = ROOT / 'build/bots'
A = [str(ROOT / p) for p in ('build/level.shr', 'build/homer.shr', 'build/sandbox/roads-l1.bin', 'build/missions/level01.msn')]
if os.environ.get('GATE_A'): A = os.environ['GATE_A'].split(':')   # 04.11: входы ночи (level:homer:roads:msn), а не build/ дерева
PASS, WALL = ('188', '3.5', '279.9'), ('200', '45', '250')


def cls(binp, leg, **env):
    o = subprocess.run([str(binp), *A, '--leg', *leg, '--secs', '40'], capture_output=True, text=True, env=dict(os.environ, MISSIONER_WEAK1='1', **env)).stdout
    f = o.strip().split('\t'); return f[2] if len(f) > 2 else '?'


def main():
    subprocess.run(['nice', '-n', '20', str(ROOT / 'tools/bots/build.sh')], check=True, capture_output=True, env=dict(os.environ, BINS='missioner'))
    subprocess.run(['nice', '-n', '20', str(ROOT / 'tools/bots/build.sh'), '-DMSN_LOOSE_ARRIVE=1'], check=True, capture_output=True, env=dict(os.environ, BINS='missioner', SUF='_loose'))
    good, loose = B / 'missioner', B / 'missioner_loose'
    p_ctl, p_ok = cls(good, PASS, MISSIONER_NO_SECOND='1'), cls(good, PASS)
    w_ctl, w_ok = cls(loose, WALL), cls(good, WALL)
    L = [f"проходимый отрезок, слабый первый водитель: контроль (без второго) — «{p_ctl}» {'КРАСНЫЙ' if p_ctl == 'игра' else '— КОНТРОЛЬ НЕ ЛОВИТ'}; рабочий — «{p_ok}» {'зелёный' if p_ok == 'водитель' else 'КРАСНЫЙ'}",
         f"непроходимый (в воздухе): контроль (приезд без высоты) — «{w_ctl}» {'КРАСНЫЙ' if w_ctl == 'водитель' else '— КОНТРОЛЬ НЕ ЛОВИТ'}; рабочий — «{w_ok}» {'зелёный' if w_ok == 'игра' else 'КРАСНЫЙ'}"]
    ok = p_ctl == 'игра' and p_ok == 'водитель' and w_ctl == 'водитель' and w_ok == 'игра'
    L.append(f"RESULT: {'OK' if ok else 'FAIL'}")
    out = ROOT / 'build/bots/runs/gate-t15' / time.strftime('%Y%m%d-%H%M%S'); out.mkdir(parents=True); (out / 'gate.txt').write_text('\n'.join(L) + '\n')
    print('\n'.join(L)); print('->', out); return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
