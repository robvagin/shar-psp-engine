#!/usr/bin/env python3
"""Build 95 П2: лавка продавца (Барни, L1) при отказе памяти. gen_sellshop.py N [old] > сценарий.txt (flag shopfail N; old — shopheal1233old: тихая отмена, красный контроль)"""
import sys
N = int(sys.argv[1]); OLD = 'old' in sys.argv[2:]
o = ["scene shop", "flag msn", "wait 3", "flag coinadd 200"]
if OLD: o.append("flag shopheal1233old 1")
o += [f"flag shopfail {N}"]
z = -320.0
for i in range(22): o += [f"foot 349.2 {z:.2f} -1.5708 -2.1", "wait 0.35", "press triangle 0.1", "wait 0.15"]; z += 0.55   # Гил идёт (~0,86 м/с): догоняем и жмём △
o += ["wait 4", "shot shop", "end"]
print("\n".join(o))
