#!/usr/bin/env python3
"""№1069: сценарий «смена уровня, когда куча не даёт банку звука цельного куска». gen.py MODE УРОВНИ... > сценарий.txt
MODE: new (flag sndslotold 0) / red (flag sndslotold 1, прежний код) — с flag sndcapsim 1.9: банк звука из кучи больше 1,9 МБ не выделяется,
как на PSP после смены уровня (тур TEST79: наибольший кусок 1,91 МБ, L5 sfx.sab 2,13 МБ). Эмулятор кучу так не дробит — проверка отказом, как №1065/№1068.
cleannew / cleanold — без отказа (контроль: звук у всех уровней обоими путями; цена постоянного места — свободная куча после загрузки).
УРОВНИ — порядок golevel; первая загрузка L1 при старте — до флагов, не оценивается (на ней постоянное место и выделяется)."""
import sys
MODE = sys.argv[1]; LV = sys.argv[2:]
old = '1' if MODE in ('red', 'cleanold') else '0'
o = ["scene C_BOOT", "flag cfgdefaults 1", f"flag sndslotold {old}"] + ([] if MODE.startswith('clean') else ["flag sndcapsim 1.9"]) + ["wait 6"]
for i, L in enumerate(LV):
    o += ([f"scene L{L}_load"] if i == 0 else []) + [f"flag golevel {L}", "wait 40"]
o += ["flag stop", "end"]
print("\n".join(o))
