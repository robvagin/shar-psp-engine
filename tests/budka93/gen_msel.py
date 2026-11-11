#!/usr/bin/env python3
"""Будка-93 / 1228: настоящий путь MISSION SELECT в паузе (как tests/menu896), L5, строка ROW (4 = m5 «Never Trust a Snake»). gen_msel.py [ROW] [FRAG HOG] [old] > сценарий.txt
Карточка брифинга: после ✕ ждём и жмём ✕ (Continue) несколько раз. old: flag dooriris1226old не нужен; выключатель T03 — flag t03old 1 (после правки)."""
import sys
a = [x for x in sys.argv[1:] if x not in ('old', 'double') and not x.startswith('fail')]; OLD = 'old' in sys.argv[1:]; DBL = 'double' in sys.argv[1:]; FAIL = next((int(x[4:]) for x in sys.argv[1:] if x.startswith('fail')), 0)
ROW = int(a[0]) if a else 4; FR = float(a[1]) if len(a) > 2 else 0; HOG = float(a[2]) if len(a) > 2 else -1
o = ["flag menu", "flag lvset 5", "wait 1.5", "press cross", "wait 70", "flag uimsndone 31", "wait 1"]
if FR: o.append(f"flag fragheap {FR:g}")
if HOG >= 0: o.append(f"flag boothstarve {HOG:g}")
if OLD: o.append("flag t03old 1")
if FAIL: o.append(f"flag scrfail1065 {FAIL}")
if DBL: o.append("flag t03double 1")
o += ["scene MSEL", "press start", "wait 1", "press down", "wait 0.4", "press cross", "wait 1"] + ["press down", "wait 0.3"] * ROW
o += ["shot ms_list", "press cross"] + (["wait 14"]) + ["shot after", "wait 4", "end"]
print("\n".join(o))
