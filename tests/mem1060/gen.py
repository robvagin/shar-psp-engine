#!/usr/bin/env python3
"""№1060: сценарий «будка в тесной / раздробленной куче». gen.py L FRAG LEFT [CAR [MEM]]  > сценарий.txt
L — уровень 1..7; FRAG — flag fragheap N (МБ, 0 — нет): наибольший свободный кусок ≤ N МБ перед загрузкой уровня;
LEFT — flag boothstarve N (МБ; -1 — не включать; 1000 — отдать): потребитель уровня забирает всё освобождённое, оставляя свободными N МБ; отдаёт при выгрузке уровня; MEM — flag mem1060 (0 — выключатель, красный контроль); CAR=1 — после открытия выбрать следующую машину стрелкой и принять."""
import sys
L, FR, HOG = int(sys.argv[1]), int(sys.argv[2]), float(sys.argv[3]); CAR = int(sys.argv[4]) if len(sys.argv) > 4 else 0; MEM = sys.argv[5] if len(sys.argv) > 5 else '1'; FE = sys.argv[6] if len(sys.argv) > 6 else '1'
BOOTH = {1: (30.6, 205.1), 7: (30.6, 205.1), 2: (-8.8, 3.6), 5: (-8.8, 3.6), 3: (91.0, 140.0), 6: (91.0, 140.0), 4: (144.0, 81.8)}
bx, bz = BOOTH[L]
o = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6"]
if FR: o.append(f"flag fragheap {FR}")
o += [f"scene L{L}_load", f"flag golevel {L}", "wait 45", "flag stop", "flag player", "wait 3"]
o += ["flag msn", "wait 3", "scene BOOTH", f"flag mem1060 {MEM}", f"flag mem1060fe {FE}"]
if HOG >= 0: o.append(f"flag boothstarve {HOG}")
o += ["wait 1.5", f"foot {bx + 0.5} {bz + 1.2} 0", "wait 3", "flag mem1060log 1", "press triangle 0.2", "wait 12", "shot booth_open"]
if CAR: o += ["press right 0.2", "wait 1", "press right 0.2", "wait 1"]
o += ["press cross 0.2", "wait 14", "shot booth_after", "flag boothstarve 1000", "wait 10", "flag phonecar_cDuff", "wait 4", "flag mem1060log 2", "shot after_restore", "end"]
print("\n".join(o))
