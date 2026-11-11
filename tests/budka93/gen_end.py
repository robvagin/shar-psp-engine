#!/usr/bin/env python3
"""Будка-93 / 1234: конец уровня N как у игрока: последняя сюжетная миссия (flag msnfinal) -> этапы засчитаны -> Level Complete -> ✕ -> «Do you want to save?» -> ответ -> газета -> уровень N+1.
gen_end.py N [yes|no] [heavy] > сценарий.txt   yes — ДА (слот, запись, подтверждение); no — НЕТ; heavy — тесная куча (fragheap 3, boothstarve 1.5), как после долгой игры"""
import sys
N = int(sys.argv[1]); a = sys.argv[2:]; YES = 'no' not in a; HEAVY = 'heavy' in a
o = ["flag saves", "flag cfgdefaults 1"]
if HEAVY: o.append("flag fragheap 3")
o += ["wait 4", f"scene L{N}_load"]
if N > 1: o.append(f"flag golevel {N}")
o += ["wait 45" if N > 1 else "wait 6", "flag player", "flag msn", "wait 4"]
if HEAVY: o.append("flag boothstarve 1.5")
o += ["flag realend 2", "flag msnfinal", "wait 3", "shot card", "press cross 0.3", "wait 3", "press cross 0.3", "wait 3"]
for i in range(14): o += ["flag msnpass", "wait 2"]
o += ["wait 14", "shot lc", "press cross 0.3", "wait 4", "shot ask"]
if YES: o += ["press cross 0.3", "wait 3", "press cross 0.3", "wait 6", "press cross 0.3", "wait 6", "shot saved"]
else: o += ["press down 0.3", "wait 1", "press cross 0.3", "wait 4"]
o += ["wait 40", "shot next", "wait 20", "shot next2", "end"]
print("\n".join(o))
