#!/usr/bin/env python3
"""Исход прогона эмулятора по папке smoke (tools/smoke.sh кладёт туда smoke.txt, run_meta.txt, blackbox.txt, stall.txt).
Отличает «игра сломалась» от «эмулятор не успел» (урок frame_gate TEST89 04.11: load 40–60, nice 20, «упал/завис» = TIMEOUT, а игра шла ровно):
  ENDED    в журнале есть END: сцена дошла до конца, дальше решают сами проверки (код 0)
  TIMEOUT  END нет, но игра шла до самого обрыва: журнал рос до конца, сторожа нет — эмулятор не успел, это НЕ ЗАВИСАНИЕ и НЕ FAIL, «не измерено» (код 3)
  HANG     END нет и главный поток стоит: FAULT hang, файл сторожа stall.txt / blackbox.txt с первой строкой STALL, либо журнал не растёт
           дольше STALL_GAP секунд (по умолчанию 90) до обрыва (код 4)
  NOSTART  журнала нет или в нём ни одной строки кадра: игра не стартовала (код 5)
  EXITED   END нет, эмулятор вышел раньше предела (вылет), журнал рос до выхода (код 6)
Использование: run_outcome.py ПАПКА_SMOKE [--json] [--suggest СЦЕНАРИЙ]
  --suggest: вместо исхода печатает ПРЕДЕЛ в секундах для повтора по измеренной скорости эмуляции (игровых секунд на секунду): 1,3 × (сумма wait сценария + 5) / скорость + 60, не меньше 2 × прежний, не больше 1500."""
import os, re, sys, json
from pathlib import Path

CODES = {'ENDED': 0, 'TIMEOUT': 3, 'HANG': 4, 'NOSTART': 5, 'EXITED': 6}


def meta(d):
    m = {}
    f = d / 'run_meta.txt'
    if f.exists():
        for kv in f.read_text(errors='replace').split():
            if '=' in kv:
                k, v = kv.split('=', 1)
                try: m[k] = float(v)
                except ValueError: m[k] = v
    return m


def classify(d, stall_gap=None):
    d = Path(d)
    sm = d / 'smoke.txt'
    m = meta(d)
    gap_lim = float(stall_gap if stall_gap is not None else os.environ.get('STALL_GAP', 90))
    log = sm.read_text(errors='replace').splitlines() if sm.exists() else []
    rows = [l for l in log if l and (l[0].isdigit())]          # строки кадра начинаются с числа-времени
    ended = any(l.startswith('END') for l in log)
    why = {'rows': len(rows), 'lines': len(log), 'wall': m.get('wall_s'), 'maxt': m.get('maxt'), 'gap': m.get('gap_s'), 'load1': m.get('load1'), 'ncpu': m.get('ncpu')}
    last_t = None
    for l in reversed(rows):
        try: last_t = float(l.split()[0]); break
        except ValueError: pass
    why['game_t'] = last_t
    if ended: return 'ENDED', why, 'END в журнале'
    if not rows: return 'NOSTART', why, 'в журнале нет ни одной строки кадра (игра не стартовала)'
    if any(l.startswith('FAULT hang') for l in log): return 'HANG', why, 'FAULT hang: главный поток остановлен'
    for fn in ('stall.txt', 'blackbox.txt'):
        f = d / fn
        if f.exists():
            head = f.read_text(errors='replace').split('\n', 1)[0]
            if head.startswith('STALL'): return 'HANG', why, f'сторож записал {fn}: {head[:80]}'
    gap = m.get('gap_s')
    if gap is not None and gap > gap_lim: return 'HANG', why, f'журнал не рос последние {gap:.0f} с (предел {gap_lim:.0f} с) до обрыва'
    wall, maxt = m.get('wall_s'), m.get('maxt')
    if wall is not None and maxt is not None and wall < maxt - 5:
        return 'EXITED', why, f'эмулятор вышел через {wall:.0f} с при пределе {maxt:.0f} с без END'
    spd = (f'{last_t / wall:.2f} игровых с на секунду' if last_t and wall else 'скорость неизвестна')
    return 'TIMEOUT', why, f'игра шла до обрыва ({spd}, игровое время {last_t:.0f} с, журнал рос до последних {gap if gap is not None else "?"} с): не успел эмулятор, не зависание'


def suggest(d, scn):
    k, why, msg = classify(d)
    need = sum(float(l.split()[1]) for l in open(scn, errors='replace') if l.split()[:1] == ['wait']) + 5
    t, w, mx = why.get('game_t'), why.get('wall'), why.get('maxt') or 0
    spd = (t / w) if t and w else None
    lim = int(1.3 * need / spd + 60) if spd else int(2 * mx)
    return max(int(2 * mx), min(lim, 1500))


def main():
    a = [x for x in sys.argv[1:] if not x.startswith('--')]
    if '--suggest' in sys.argv:
        scn = sys.argv[sys.argv.index('--suggest') + 1]; a = [x for x in a if x != scn]
        print(suggest(a[0], scn)); return 0
    if not a: sys.exit(__doc__)
    k, why, msg = classify(a[0])
    print(f'ИСХОД: {k} · {msg}')
    if '--json' in sys.argv: print(json.dumps(why, ensure_ascii=False))
    return CODES[k]


if __name__ == '__main__': sys.exit(main())
