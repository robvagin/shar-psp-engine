#!/usr/bin/env python3
# №1038: check.py SMOKE.txt DIR_ВЫХОД [red]  — в будке L/R работают как ◄ ►: после каждого нажатия в журнале строка PHONEV sel с той же машиной, что у ◄ ►; с flag boothlr1038 0 — R и L ничего не меняют.
# Зелёное: ► -> sel 1, ◄ -> sel 0, R -> sel 1 (та же машина и данные, что у ►), L -> sel 0 (как у ◄); кадры b_R и b_right одной страницы (средняя разница <= 4/255), b_L и b_left тоже; выключено: нет строк sel и кадр b_R_off = b_L (машина не сменилась).
# red — те же проверки на журнале выключенного режима: обязан упасть. Последняя строка PASS/FAIL
import re, sys
import numpy as np
from PIL import Image
smoke, d = sys.argv[1:3]; red = len(sys.argv) > 3
sel = {}; cur = []
for l in open(smoke, errors='replace'):
    m = re.match(r'PHONEV sel (\d+) name=(\S+) owned=(\d) lvl=(\d+) now=(\d) stats=(\d) accept=(\d)', l)
    if m: cur.append(m.groups())
    m = re.match(r'SHOT (b_\S+) ', l)
    if m: sel[m.group(1)] = cur; cur = []
ok = True
def chk(c, s):
    global ok; ok &= bool(c); print('  %s %s' % ('ok  ' if c else 'FAIL', s))
def last(n): return sel.get(n, [None])[-1] if sel.get(n) else None
for n, want in (('b_right', '1'), ('b_left', '0'), ('b_R', '1'), ('b_L', '0')):
    r = last(n); chk(r and r[0] == want, '%-8s sel %s (журнал: %s)' % (n, want, r[:2] if r else 'нет строки'))
chk(last('b_R') == last('b_right') and last('b_L') == last('b_left'), 'R даёт ту же машину и данные, что ►; L — что ◄')
def im(n): return np.asarray(Image.open('%s/%s.png' % (d, n)).convert('RGB'), float)
for a, b in (('b_R', 'b_right'), ('b_L', 'b_left')):
    m = float(np.abs(im(a) - im(b)).mean()); chk(m <= 4, 'кадры %s и %s одной страницы: средняя разница %.2f' % (a, b, m))
chk(not sel.get('b_R_off') and not sel.get('b_L_off'), 'с flag boothlr1038 0: R и L строк sel не дают (%d, %d)' % (len(sel.get('b_R_off', [])), len(sel.get('b_L_off', []))))
m = float(np.abs(im('b_R_off') - im('b_L')).mean()); chk(m <= 4, 'с flag 0: кадр после R такой же, как до (машина не сменилась): разница %.2f' % m)
print('  зелёный: в будке L и R листают машины как ◄ и ►, выключатель возвращает прежнее' if ok else '  FAIL: таблица выше')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
