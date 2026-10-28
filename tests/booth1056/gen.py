#!/usr/bin/env python3
"""№1056: сценарий матрицы «будка: каждая машина списка» на уровне L. Запуск: gen.py L > сценарий.txt
Порядок: загрузка уровня, будка открывается (PHONE view open), закрывается; каждая машина вызывается как из будки
(flag phonecar_<имя> = mvPhoneCar, строка PHONECAR <имя> ok=<0/1>); в конце будка открывается снова с чужой машиной под Гомером."""
import sys
L = int(sys.argv[1])
BOOTH = {1: (30.6, 205.1), 7: (30.6, 205.1), 2: (-8.8, 3.6), 5: (-8.8, 3.6), 3: (91.0, 140.0), 6: (91.0, 140.0), 4: (144.0, 81.8)}   # будка 0 (PHONEGO в журнале прогона); карты L1=L7, L2=L5, L3=L6
names = "famil_v cletu_v elect_v plowk_v cDuff cVan honor_v gramp_v moe_v mrplo_v cLimo cFire_v lisa_v skinn_v bookb_v otto_v cDonut cNerd marge_v comic_v krust_v willi_v cKlimo cCurator apu_v frink_v bbman_v carhom_v wiggu_v cCola bart_v snake_v burns_v scorp_v cArmor cSedan homer_v smith_v fone_v zombi_v hbike_v cHears".split()
print("scene start\nwait 6")
if L > 1:
    print(f"scene L{L}_load\nflag golevel {L}\nwait 25")
else:
    print("scene L1_load\nwait 10")
bx, bz = BOOTH[L]
open_booth = f"flag phonego 0\nwait 2\nfoot {bx + 0.5} {bz + 1.2} 0\nwait 1.5\npress triangle 0.2\nwait 4"
print("flag msn\n" + open_booth + "\nshot booth_open\npress triangle 0.2\nwait 3")
print("scene MATRIX")
for n in names:
    print(f"flag phonecar_{n}\nwait 1.2")
print("scene BOOTH_AFTER\nflag player\nwait 2\n" + open_booth + "\nshot booth_after\npress triangle 0.2\nwait 2\nend")
