#!/usr/bin/env python3
"""Будка-93 / 1239: ролик при нехватке памяти. gen_movie.py N [old] > сценарий.txt  (N: flag moviefail — 1 лечится вторым открытием под прелоадером, 3 — «MOVIE SKIPPED»; old: молчаливый пропуск, красный контроль)"""
import sys
N = int(sys.argv[1]); OLD = 'old' in sys.argv[2:]
o = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6", "scene L1_start", "wait 6", "flag player", "flag msn", "wait 3"]
if OLD: o.append("flag movie1239old 1")
o += [f"flag moviefail {N}", "scene MOVIE", "flag movie 5", "wait 12", "shot after", "end"]
print("\n".join(o))
