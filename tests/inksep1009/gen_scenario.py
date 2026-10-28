#!/usr/bin/env python3
"""№1009: сценарий «было / стало»: ближний объект перед машиной трафика (L1). gen_scenario.py [green|red] > scenario.txt
Машина трафика стоит (flag trfgo), глаз в 14 м к +x и 4 м выше. Перед ней на разном зазоре Гомер пешком (foot); потом flag repair — машина игрока перед машиной трафика (goto) и за ней.
На каждой позиции два кадра с одного места: o — flag inksep1009 0 (прежнее), n — flag inksep1009 1 (в красном сценарии тоже 0)."""
import sys
mode = sys.argv[1] if len(sys.argv) > 1 else 'green'
newv = 1 if mode == 'green' else 0
CX, CZ = 257.3, 161.1      # машина трафика в первой части (до flag repair)
DX, DZ = 276.0, 161.1      # после flag repair (строка TRFGO журнала: первая машина трафика к этому времени проехала дальше)
L = ['wait 12', 'flag cfgdefaults 1', 'flag golevel 1', 'wait 40', 'flag toon 1', 'wait 4', 'flag flat912 1', 'scene G_ON', 'foot 295.5 166 3.14', 'wait 22', 'flag trfgoh 4', 'flag trfgo 0.0 14', 'wait 2']
def pair(nm):
    return ['flag inksep1009 0', 'wait 1.2', f'shot o_{nm}', f'flag inksep1009 {newv}', 'wait 0.6', f'shot n_{nm}', 'flag inksep1009 1']
for g in (1.2, 2.2):   # Гомер пешком перед машиной: зазор от её переда до Гомера
    L += [f'foot {CX + 2.6 + g:.2f} {CZ:.2f} 1.571', 'wait 3'] + pair(f'h{int(g * 10)}')
if len(sys.argv) > 2 and sys.argv[2] == 'homer': L += ['end']; print('\n'.join(L)); sys.exit(0)
L += ['flag repair', 'wait 3', 'flag trfgo 0.0 14', 'wait 1']
for nm, dx, dz in (('cf', 6.0, 1.6), ('cb', -6.0, -1.6)):   # cf — машина игрока перед машиной трафика (ближе к глазу), cb — за ней
    L += [f'goto {DX + dx:.2f} {DZ + dz:.2f} 1.571', 'wait 1.5'] + pair(nm)
L += ['flag eyeon 0', 'wait 1', 'end']
print('\n'.join(L))
