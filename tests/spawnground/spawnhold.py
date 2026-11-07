"""A1 Опора-93: python3 -I tests/spawnground/spawnhold.py SMOKE.txt green|red
Сценарий tests/spawnground/spawnhold.txt (L4, SHIP_LEVELS, SMOKE_BALLAST=5, SMOKE_HEADLESS=1; EBOOT с -DSPAWNHOLD_OLD=1 — красный).
green: ирис room_out раскрывается не раньше, чем герой отпущен (release), и есть строка SPAWNHOLD ms<=6000. red: ирис раскрылся раньше release больше чем на 1 с (герой невидим при открытом ирисе)."""
import re, sys
L = open(sys.argv[1], errors='replace').read().splitlines()
mode = sys.argv[2]
op = next((float(re.search(r' t=([\d.]+)', l).group(1)) for l in L if l.startswith('IRIS why=room_out phase=open')), None)
rel = next((float(re.search(r' t=([\d.]+)', l).group(1)) for l in L if l.startswith('WATCHDOG spawn actor=hero') and ' release ' in l), None)
hold = [l for l in L if l.startswith('SPAWNHOLD ')]
ms = [int(re.search(r'ms=(\d+)', l).group(1)) for l in hold]
print(f'ирис открылся t={op}; герой отпущен t={rel}; SPAWNHOLD {ms}')
if op is None or rel is None: print('FAIL нет строк ириса или release'); sys.exit(1)
gap = rel - op
if mode == 'green':
    ok = gap <= 0.1 and hold and max(ms) <= 6000
else:
    ok = gap > 1.0 and not hold
print(('PASS' if ok else 'FAIL'), f'(герой невидим при открытом ирисе {max(gap,0):.2f} с)')
sys.exit(0 if ok else 1)
