#!/usr/bin/env python3
"""Будка-93 / 1233 T02: смена уровня, когда экран загрузки не открывается (flag scrfail1065 N: 4 — обе попытки и повтор после выгрузки уровня). gen_lvl.py N [old] [tf=K] [ldrold] > сценарий.txt. tf=K — K отказов чтения уровня (flag tffail), ldrold — прежнее тело (flag lvldr96 0)"""
import sys
N = int(sys.argv[1]); OLD = 'old' in sys.argv[2:]; TF = next((int(x[3:]) for x in sys.argv[2:] if x.startswith('tf=') and False), 0); TF = next((int(x[3:]) for x in sys.argv[2:] if x.startswith('tf=')), 0); LDR = 'ldrold' in sys.argv[2:]
o = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6", "scene L1_start", "wait 6"]
if OLD: o.append("flag loadcord1233old 1")
if LDR: o.append("flag lvldr96 0")
if TF: o.append(f"flag tffail {TF}")
o += [f"flag scrfail1065 {N}", "scene TO_L4", "flag golevel 4", "wait 40", "shot l4_after", "end"]
print("\n".join(o))
