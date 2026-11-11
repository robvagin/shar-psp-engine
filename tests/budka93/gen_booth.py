#!/usr/bin/env python3
"""Будка-93 / 1225, 1227: будка L4 в тесной куче с включёнными камерами старта (flag startcam 1). gen_booth.py FRAG HOG [old] > сценарий.txt
old: BOOTH1225_OLD (flag booth1225old 1) — прежняя будка (сцена + пересборка, наезд камеры старта на выходе)."""
import sys
FR, HOG = float(sys.argv[1]), float(sys.argv[2]); OLD = 'old' in sys.argv[3:]; PF = next((int(x[3:]) for x in sys.argv[3:] if x.startswith('pf=')), 0)   # pf=N: flag phonefail N, первые N попыток открыть пакет отказывают (не зависит от того, сколько кучи освободили зоны)
bx, bz = 144.0, 81.8
o = ["scene C_BOOT", "flag cfgdefaults 1", "flag startcam 1", "wait 6"]
if FR: o.append(f"flag fragheap {FR:g}")
o += ["scene L4_load", "flag golevel 4", "wait 45", "flag stop", "flag player", "wait 3", "flag msn", "wait 3"]
if OLD: o.append("flag booth1225old 1")
o += ["scene BOOTH", "flag mem1060 1", "flag mem1060fe 1", f"flag boothstarve {HOG:g}"] + ([f"flag phonefail {PF}"] if PF else []) + [ "wait 1.5", f"foot {bx + 0.5} {bz + 1.2} 0", "wait 3",
      "flag mem1060log 1", "press triangle 0.2", "wait 12", "shot booth_open", "press cross 0.2", "wait 3", "flag camtest1225 1", "wait 11", "shot booth_after", "flag boothstarve 1000", "wait 10", "shot booth_end", "end"]
print("\n".join(o))
