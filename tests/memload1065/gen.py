#!/usr/bin/env python3
"""№1065: сценарий «смена уровней подряд на раздробленной куче». gen.py FRAG MODE УРОВНИ... > сценарий.txt
FRAG — flag fragheap N (МБ; 0 — нет): перед КАЖДОЙ загрузкой наибольший свободный кусок ≤ N МБ; MODE: new (flag memload1065 1), red (flag memload1065 0, выключатель),
newsim1 / newsim2 (new + flag scrfail1065 1 / 2 и sndfail1065 1: следующие открытия load.shr отказывают один / два раза и банк звука один раз — повтор без тесной кучи); redsim2 — то же на выключателе. УРОВНИ — порядок golevel."""
import sys
FR, MODE = int(sys.argv[1]), sys.argv[2]; LV = sys.argv[3:]
mem = '0' if (MODE.startswith('red') or MODE.endswith('red')) else '1'
sim = 2 if MODE.endswith('sim2') else (1 if MODE.endswith('sim1') else 0)
nat = '0.8' if MODE.startswith('nat') else ''   # natnew / natred: настоящая тесная куча перед сменой уровня (boothstarve 0.8 МБ свободно; отдаётся при выгрузке уровня), без имитации отказов
o = ["scene C_BOOT", "flag cfgdefaults 1", f"flag memload1065 {mem}", "wait 6"]
first = True
for L in LV:
    if FR: o.append(f"flag fragheap {FR}")
    if nat and not first: o.append(f"flag boothstarve {nat}")
    if sim and not first: o += [f"flag scrfail1065 {sim}", "flag sndfail1065 1"]
    if first: o += [f"scene L{L}_load", f"flag golevel {L}", "wait 40"]
    else: o += [f"flag golevel {L}", "wait 40"]
    first = False
o += ["flag stop", "end"]
print("\n".join(o))
