#!/usr/bin/env python3
# Fable шаг 14 (клинья 1236): сценарии для сборки MEM_SCRIBBLE2 (5 прогонов безголово). gen.py ИМЯ > сценарий.txt ; ИМЯ: drive | house | kwik | msnend | levels
import sys
N = sys.argv[1]
pre = ["scene S_BOOT", "flag cfgdefaults 1", "wait 4", "scene S_L1", "flag golevel 1", "wait 40", "flag stop", "flag player 1"]
o = list(pre); k = [0]
def shot(): k[0] += 1; o.append(f"shot s{k[0]:03d}")
def door(foot_in, foot_out1, foot_out2):
    o.extend(["flag repair", "wait 1", "flag player 1", f"foot {foot_in}", "wait 3", "press triangle 0.3", "wait 9"]); shot(); o.extend(["wait 3"]); shot()
    o.extend([f"foot {foot_out1}", "wait 1", f"foot {foot_out2}", "wait 0.5", "press triangle 0.2", "wait 2"]); shot(); o.extend(["wait 3"]); shot()
if N == 'drive':
    o.append("scene S_DRIVE"); o.append("flag drive 200")
    for _ in range(100): o.append("wait 2"); shot()
elif N == 'house':
    for c in range(5): o.append(f"scene S_HOUSE{c}"); door("228.94 183.30 0.0000 4.23", "501.13 392.0 0 -19.5", "500.5 395.0 0 -19.5")
elif N == 'kwik':
    for c in range(5): o.append(f"scene S_KWIK{c}"); door("213.97 301.70 0.0000 5.56", "499.43 300.0 0 -19.5", "499.43 305.25 0 -19.5")
elif N == 'msnend':
    o += ["scene S_MSN", "flag msn", "wait 2", "flag msnsel 0", "wait 8", "flag msngo", "wait 3"]
    for _ in range(30): o += ["press cross 0.2", "wait 1.5", "flag msnpass", "wait 3"]; shot()
elif N == 'levels':
    for lv in (2, 3, 1, 4, 1):
        o.append(f"scene S_LV{lv}"); o.append(f"flag golevel {lv}"); o.append("wait 6"); shot()
        for _ in range(12): o.append("wait 3"); shot()
o.append("end"); print("\n".join(o))
