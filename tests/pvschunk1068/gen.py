#!/usr/bin/env python3
"""№1068: сценарий «загрузка уровней, когда куча не даёт крупного куска». gen.py MODE УРОВНИ... > сценарий.txt
MODE: new (flag pvschunkold 0) / red (flag pvschunkold 1, прежний код) — с flag pvscapsim 2: мягкое выделение из кучи больше 2 МБ отказывает, как на PSP
после смен уровня (тур TEST78b: наибольший кусок 2,59 МБ). Эмулятор flag fragheap так не дробит (его malloc даёт кусок 9–13 МБ) — проверка отказом, как №1065.
cleannew / cleanold — без отказа (контроль: на целой куче оба пути дают PVS, кадр одинаковый). УРОВНИ — порядок golevel; первая загрузка L1 при старте — до флагов, не оценивается."""
import sys
MODE = sys.argv[1]; LV = sys.argv[2:]
old = '1' if MODE in ('red', 'cleanold') else '0'
o = ["scene C_BOOT", "flag cfgdefaults 1", f"flag pvschunkold {old}"] + ([] if MODE.startswith('clean') else ["flag pvscapsim 2"]) + ["wait 6"]
first = True
for L in LV:
    if first: o += [f"scene L{L}_load", f"flag golevel {L}", "wait 40"]
    else: o += [f"flag golevel {L}", "wait 40"]
    first = False
o += ["flag stop", "end"]
print("\n".join(o))
