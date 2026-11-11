#!/usr/bin/env python3
"""Будка-93 / 1233 T09: гардероб при отказе памяти. gen_shop.py N [old] > сценарий.txt  (N — flag shopfail: 1 лечится второй попыткой, 3 — честное сообщение; old — тихая отмена, красный контроль)"""
import sys
N = int(sys.argv[1]); OLD = 'old' in sys.argv[2:]
o = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6", "scene L1_start", "wait 8", "flag player", "flag msn", "wait 3"]
if OLD: o.append("flag shopheal1233old 1")
if 'cleanold' in sys.argv[2:]: o.append("flag cleanreload96 0")   # Build 96: без ступени «чистая перезагрузка» (SIMPLIFY как был)
o += [f"flag shopfail {N}", "scene WARD", "flag wardtestopen 1", "wait 2", "shot ward", "end"]
print("\n".join(o))
