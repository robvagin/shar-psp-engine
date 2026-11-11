#!/usr/bin/env python3
"""Будка-93 / Build 96: конец уровня N через последнюю сюжетную миссию (ролик fmvK, Level Complete, газета, уровень N+1) на новом пути levelup. gen_fmvend.py N FMV [MSNSEL] > сценарий.txt
Основа tests/levels/fmv_levelend.txt (L2, fmv3, msnsel 7)."""
import sys
N = int(sys.argv[1]); F = sys.argv[2]; M = sys.argv[3] if len(sys.argv) > 3 else '7'
print(f"""flag menu
flag lvset {N}
wait 1.5
press cross
wait 7
flag msnsel {M}
wait 4
shot fmv{F}
waitfor MSNFMVSHOT 30
press start 0.3
waitfor MSNFMV name=fmv{F} 90
flag lvshot 1.5
waitfor LEVEL ev=load n={N+1} 180
wait 2
flag lvlog
shot l{N+1}_after_fmv
end""")
