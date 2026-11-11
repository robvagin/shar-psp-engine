#!/usr/bin/env python3
"""Итог смоука tests/flow/credits_exit_scn.txt (каталог прогона = аргумент): △ и ○ титры не закрывают, ✕ закрывает, подсказки Back (правый низ) нет.
Красный контроль: EBOOT, собранный с -DCREDITSX_OLD=1, обязан провалить."""
import sys, re
from pathlib import Path
from PIL import Image
d = Path(sys.argv[1]); log = (d / 'smoke.txt').read_bytes().decode('utf-8', 'replace')
sc = {m.group(1): float(m.group(2)) for m in re.finditer(r'CREDITS (cred_\w+|back) scroll=([\d.]+)', log)}
ok = True
def chk(name, cond, msg):
    global ok; ok = ok and cond; print(('OK   ' if cond else 'FAIL ') + name + ': ' + msg)
chk('tri_stays', 'cred_after_tri' in sc and 'cred_after_circ' in sc, 'после △ и после ○ титры ещё идут (строки CREDITS cred_after_tri / cred_after_circ: %s)' % ('есть' if 'cred_after_tri' in sc and 'cred_after_circ' in sc else 'нет'))
chk('cross_exits', 'back' in sc and sc.get('back', 0) > sc.get('cred_after_circ', 1e9) - .5, 'выход ✕ после ○ (CREDITS back scroll=%s)' % sc.get('back'))
im = Image.open(d / 'cred_a.png').convert('RGB').crop((350, 218, 440, 245)); mx = max(max(p) for p in im.getdata())
chk('no_back_hint', mx < 60, 'правый низ (подсказка Back): самый яркий пиксель %d (ждём < 60)' % mx)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
