#!/usr/bin/env python3
"""Гейт шага 11 (FABLE_АУДИТ §3.4 п.2): сценарий «20 минут езды по L4 с заходами в дом, Kwik-E-Mart и будку» без раздробления кучи (естественная куча, ballast 5).
gen_l4gate.py [250] > сценарий.txt. Без аргумента: дальность по умолчанию (гейт б). С аргументом 250: после загрузки L4 дальность принудительно 250 м (flag pano917cap 1 250, гейт в).
Основа — tests/longgame/gen_long.py 1200 0 no L4 (заходы дом, Kwik-E-Mart, будка, пауза, две смерти; конец L4 с роликом fmv4 после езды; езда оканчивается сценой LG_m5)."""
import subprocess, sys
from pathlib import Path
H = Path(__file__).resolve().parent
src = subprocess.run([sys.executable, '-I', str(H.parent / 'longgame' / 'gen_long.py'), '1200', '0', 'no', 'L4'], capture_output=True, text=True, check=True).stdout.splitlines()
out = []
for l in src:
    out.append(l)
    if len(sys.argv) > 1 and sys.argv[1] == '250' and l.startswith('flag golevel'):
        out += ['wait 45', 'flag pano917cap 1 250']   # после загрузки; следующий wait сценария продолжает ожидание
print('\n'.join(out))
