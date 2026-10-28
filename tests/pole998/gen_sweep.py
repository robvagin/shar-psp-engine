#!/usr/bin/env python3
# №998: подбор бокового смещения заезда в игре: gen_sweep.py ВЫХОД.txt old|new ЧАСТЬ(0|1)  — по каждому столбу типа 8 смещения -1,3..1,3 м, одна SPHITS на заезд
import sys, math
T = {203: (-161.48, -508.43, 1.5708), 204: (-181.56, -497.42, 4.7124), 205: (-81.21, -447.73, 3.1416), 207: (-51.84, -418.48, 0.0), 214: (-226.12, -504.66, 0.0), 218: (-49.95, -462.10, 0.0)}
LATS = [-1.3, -1.15, -1.0, 1.0, 1.15, 1.3]
ONLY = {203: [-1.3], 204: [-1.3], 205: [-1.3, -1.15], 207: [-1.3, -1.15]}   # режим new part=2: только заезды, где прежний код оставил столб стоять
out, mode, part = sys.argv[1], sys.argv[2], int(sys.argv[3])
s = "# №998: подбор смещения заезда\nscene F_L1\nflag cfgdefaults 1\nflag repair\nwait 4\nscene F_L5\nflag golevel 5\nwait 40\n" + ("flag pole998 0\n" if mode == 'old' else "")
for k, i in enumerate(sorted(T)):
    if part < 2 and k % 2 != part: continue
    if part == 2 and i not in ONLY: continue
    tx, tz, h = T[i]
    for lat in (ONLY[i] if part == 2 else LATS):
        x = tx - math.sin(h) * 12 + lat * math.cos(h); z = tz - math.cos(h) * 12 - lat * math.sin(h)
        s += f"flag repair\nwait 1.5\nscene H_{i}\ngoto {x:.2f} {z:.2f} {h}\nwait 2.5\nflag autostraight 2.4\nwait 1.4\nflag sphits\nshot s{i}_{lat}\nwait 0.3\n"
open(out, "w").write(s + "end\n")
