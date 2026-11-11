#!/usr/bin/env python3
"""Будка-93 / Build 96 (правка Роба 1250): машина из будки на L4 (координаты будки gen_booth). gen_boothcar.py N [old] [R] > сценарий.txt
N — flag carfail96: сколько попыток загрузки модели вызванной машины отказывают (0 — норма; 1..3 лечится лестницей; 5 — не лечится: своя машина уровня, не невидимая). old — flag carvis96 0 (прежнее: невидимая).
hardold — flag carhard96 0.
hide — после выбора машины flag carhide96 1 (модель «не пришла» в кадре), lv=6 — будка L6.
R — сколько раз нажать ◄ в будке (marge_v -> lisa_v: куплена; ► ведёт в apu_v, он закрыт) перед выбором (по умолчанию 1)."""
import sys
N = int(sys.argv[1]); OLD = 'old' in sys.argv[2:]; R = next((int(x) for x in sys.argv[2:] if x.isdigit()), 1)
LV = next((int(x[3:]) for x in sys.argv[2:] if x.startswith('lv=')), 4); HIDE = 'hide' in sys.argv[2:]; MS = next((int(x[3:]) for x in sys.argv[2:] if x.startswith('ms=')), -1)   # ms=N: на L6 первая миссия с принудительной машиной выключает будки (оригинал: △ молчит) — выбрать другую миссию
bx, bz = (144.0, 81.8) if LV == 4 else (91.05, 140.01)   # L6: будка 0 (91.05, 140.01), координаты из PHONEX (flag phonego k)
o = ["scene C_BOOT", "flag cfgdefaults 1", "wait 6", f"scene L{LV}_load", f"flag golevel {LV}", "wait 45", "flag stop", "flag player", "wait 3"] + (["flag msn", "wait 3"] if LV == 4 or MS >= 0 else []) + ([f"flag msnsel {MS}", "wait 5"] if MS >= 0 else [])
if OLD: o.append("flag carvis96 0")
if "hardold" in sys.argv[2:]: o.append("flag carhard96 0")   # прежнее: надпись и подмена машины
o += ["scene BOOTH", "flag mem1060 1", "wait 1.5"] + [f"foot {bx + (0.5 if LV == 4 else 0.3)} {bz + (1.2 if LV == 4 else 0.9)} 0{' -6.5' if LV != 4 else ''}", "wait 3"] + [ "press triangle 0.2", "wait 10", "shot booth_open"]
o += ["press left 0.2", "wait 0.6"] * R + ["shot booth_pick", f"flag carfail96 {N}", "press cross 0.2", "wait 6", "shot car_after1"] + (["flag carhide96 1", "wait 14", "shot car_hidden"] if HIDE else ["wait 8", "shot car_after2"]) + ["wait 3", "end"]
print("\n".join(o))
