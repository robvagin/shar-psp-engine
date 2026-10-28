#!/usr/bin/env python3
# №998: сценарии эмулятора — удар игровой машины в каждый красный столб перекрёстка L5 F3-2.
# Использование: gen_scenarios.py ВЫХОД.txt [old]   (old — добавляет «flag pole998 0» после загрузки уровня)
import sys
import math
# номер экземпляра, тип, ствол x, ствол z, курс (рад), боковое смещение заезда от ствола (м): тип 8 — 204, 205, 207 заезд вскользь (смещения подобраны в игре: прежний код оставляет столб стоять, tests/pole998/gen_sweep.py),
# 203, 214, 218 и все типа 6 — прямо в ствол; старт — 12 м до ствола, газ ~2,4 с
POLES = [(204, 8, -181.56, -497.42, 4.7124, -1.3), (205, 8, -81.21, -447.73, 3.1416, -1.15), (207, 8, -51.84, -418.48, 0.0, -1.15),
  (203, 8, -161.48, -508.43, 1.5708, 0.0), (214, 8, -226.12, -504.66, 0.0, 0.0), (218, 8, -49.95, -462.10, 0.0, 0.0),
  (92, 6, -201.43, -527.16, 1.5708, 0.0), (95, 6, -124.65, -392.07, 0.0, 0.0), (96, 6, -198.25, -418.31, 0.0, 0.0),
  (97, 6, -181.68, -447.62, 1.5708, 0.0), (98, 6, -124.80, -477.90, 0.0, 0.0)]
out, old = sys.argv[1], len(sys.argv) > 2
s = "# №998: удар игровой машины (такси, газ ~2,4 с с 12 м) по красным столбам перекрёстка L5 F3-2; SPHITS ids — номера упавших стейтпропов\nscene F_L1\nflag cfgdefaults 1\nflag repair\nwait 4\nscene F_L5\nflag golevel 5\nwait 40\n"
if old: s += "flag pole998 0\n"
for i, t, tx, tz, h, lat in POLES:
    x = tx - math.sin(h) * 12 + lat * math.cos(h); z = tz - math.cos(h) * 12 - lat * math.sin(h)
    x, z = round(x, 2), round(z, 2)
    s += f"flag repair\nwait 2\nscene H_{i}\ngoto {x} {z} {h}\nwait 2.5\nflag autostraight 2.4\nwait 1.3\nflag sphits\nshot p{i}_a\nwait 0.7\nshot p{i}_b\nwait 1.0\nshot p{i}_c\nwait 1.5\nshot p{i}_d\nwait 2.0\nflag sphits\nshot p{i}_e\n"
s += "end\n"
open(out, "w").write(s)
