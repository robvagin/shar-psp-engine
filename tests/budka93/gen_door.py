#!/usr/bin/env python3
"""Будка-93 / 1226: вход и выход через дверь L4. gen_door.py ДВЕРЬ FRAG HOG  > сценарий.txt
ДВЕРЬ: kwik | house. FRAG — flag fragheap N (МБ, 0 — нет); HOG — flag boothstarve N (МБ, -1 — не включать): тесная раздробленная куча, как после долгой беготни."""
import sys
D = sys.argv[1]; FR = float(sys.argv[2]); HOG = float(sys.argv[3]); MASH = "mash" in sys.argv[4:]; OLD = "old" in sys.argv[4:]; TFF = next((int(x[7:]) for x in sys.argv[4:] if x.startswith("tffail=")), 0); TOLD = "tfsmold" in sys.argv[4:]; NOFIT = "tfnofit" in sys.argv[4:]
P = {'kwik': ('213.91 299.04 0.0 5.56', 'KwikEMart'), 'house': ('228.90 181.60 0.0 3.8', 'SimpsonsHouse')}[D]
o = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6"]
if FR: o.append(f"flag fragheap {FR:g}")
o += ["scene L4_load", "flag golevel 4", "wait 45"]
if OLD: o += ["flag dooriris1226old 1", "flag gaghold1226old 1"]
if TOLD: o.append("flag tfsm95old 1")
if "cleanold" in sys.argv[4:]: o.append("flag cleanreload96 0")   # Build 96: без ступени «чистая перезагрузка» (SIMPLIFY как был)
if TFF: o.append(f"flag tffail {TFF}")
if NOFIT: o.append("flag tfnofit 1")
if HOG >= 0: o.append(f"flag boothstarve {HOG:g}")
o += [f"scene {P[1]}_door", f"foot {P[0]}", "wait 2", "shot door_before", "press up 0.5", "wait 0.4",
      "press triangle 0.2", "wait 2" if not MASH else "wait 0.2"] + ([] if not MASH else ["press triangle 0.15", "wait 0.25"] * 14) + ["shot in_1", "wait 6", "shot in_2", "flag boothstarve 1000", "wait 2"]
EXIT = {'kwik': "foot 499.43 305.25 0 -19.5", 'house': "foot 500.5 395.0 0 -19.5"}[D]
o += ["foot 499.43 300.0 0 -19.5" if D == "kwik" else "foot 501.13 392.0 0 -19.5", "wait 1", EXIT, "wait 0.5", "press triangle 0.2", "wait 2", "shot out_1", "wait 8", "shot out_2", "end"]
print("\n".join(o))
