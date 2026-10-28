#!/usr/bin/env python3
"""№999: сценарий «было / стало» по переходам на брусчатке (L6, L2, L5): камера над переходом, брусчатка рядом в кадре. gen_scenario.py [accept] > scenario.txt"""
import math, sys
SP = {  # уровень: [(имя, x, y, z)]  (центр кадра, y — высота земли)
 6: [('X1', -102, -33.1, 114.7), ('X2', -186, -40.2, 150.0), ('X3', -2.7, 6.7, -6.5), ('X4', -128, -32.5, 101.0)],
 1: [('X1', 145.7, 3.6, 85.1)],
 2: [('X1', 6.4, 6.9, 89.1), ('X2', 100.8, 20.5, -281.3)],
 3: [('X1', -275.8, -53.3, -285.9)],
 4: [('X1', 245.2, -16.3, -175.3)],
 5: [('X1', 6.4, 6.9, 89.1), ('X2', 100.8, 20.5, -281.3)],
 7: [('X1', -4.6, 0.5, 604.6)],
}
OFF = (7.0, 6.0, 7.0)
acc = len(sys.argv) > 1 and sys.argv[1] == 'accept'
if acc: SP = {1: [], 2: [], 3: [], 4: [], 5: [], 6: SP[6][:2], 7: []}
print('scene C_BOOT\nflag cfgdefaults 1\nwait 6')
for L, spots in SP.items():
    print(f'scene C_L{L}\nflag golevel {L}\nwait {40 if spots else 12}')
    for nm, x, y, z in spots:
        dx, dy, dz = OFF; n = math.sqrt(dx * dx + dy * dy + dz * dz)
        print(f'scene S_L{L}_{nm}\nfoot {x} {z} 0\nwait 4\ncam {x + dx:.1f} {y + dy:.1f} {z + dz:.1f} {-dx / n:.3f} {-dy / n:.3f} {-dz / n:.3f}\nwait 8\nshot A_L{L}_{nm}')
print('end')
