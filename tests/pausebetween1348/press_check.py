#!/usr/bin/env python3
import re, sys, os
import numpy as np
from PIL import Image
out = sys.argv[1]
def smoke(n): return open(os.path.join(out, 'out_%s' % n, 'smoke.txt'), errors='replace').read()
def huds(t): return [(m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))) for m in re.finditer(r'^MSNHUD cur=(\w+) story=\w+ phase=(\d+) stage=(\d+) status=(\d+)', t, re.M)]
def bright(n, s):
    a = np.asarray(Image.open(os.path.join(out, 'out_%s' % n, '%s.bmp' % s)).convert('L'))[:272, :480].astype(float); return a.mean(), a.std()
BAD = re.compile(r'^WATCHDOG (?!zone3|colring)', re.M)
ok = True
def chk(name, cond, why):
    global ok; ok &= bool(cond); print('  %-58s %s' % (why, 'ok' if cond else 'FAIL'))
def wd(t): return sorted(re.sub(r' t=[\d.]+', '', m.group(0)) for m in re.finditer(r'^WATCHDOG (?!zone3|colring).*$', t, re.M))
for n in ('restart_btw', 'restart_run', 'cancel_btw', 'cancel_run'):
    t = smoke(n); h = huds(t); print(n, 'MSNHUD:', h[-1] if h else None, '| WATCHDOG:', wd(t) or '-')
    chk(n, len(re.findall(r'^PAUSEROWS .*RESTART MISSION', t, re.M)) == 1, 'пауза миссии (RESTART MISSION) открылась один раз')
    if n.startswith('cancel'):
        for sh in ('s1', 's2'):
            m, sd = bright(n, sh); chk(n, m > 25 and sd > 12, 'кадр %s не чёрный (яркость %.0f, разброс %.0f)' % (sh, m, sd))
        chk(n, not wd(t), 'нет WATCHDOG кроме zone3/colring')
# RESTART: из BETWEEN должен вести себя так же, как из обычной паузы RUNNING (сам перезапуск и чёрный экран при нём — отдельная тема: на данных набора 108 перезапуск
# из ЛЮБОЙ паузы даёт 5 с чёрного и WATCHDOG fade-stuck, на int108b без 1347/1348 то же)
b1, b2 = bright('restart_btw', 's1'), bright('restart_run', 's1'); c1, c2 = bright('restart_btw', 's2'), bright('restart_run', 's2')
chk('r', abs(b1[0] - b2[0]) < 10 and abs(c1[0] - c2[0]) < 10, 'RESTART из BETWEEN даёт те же кадры, что из RUNNING (яркость s1 %.0f/%.0f, s2 %.0f/%.0f)' % (b1[0], b2[0], c1[0], c2[0]))
chk('r', wd(smoke('restart_btw')) == wd(smoke('restart_run')), 'RESTART из BETWEEN даёт те же WATCHDOG, что из RUNNING')
if wd(smoke('restart_run')) or b2[0] < 25: print('  ВНИМАНИЕ (не от 1348): перезапуск из паузы миссии даёт чёрный экран до сторожа fade-stuck; %s' % wd(smoke('restart_run')))
t = smoke('restart_btw'); h = huds(t)
chk('r', len(re.findall(r'^MSN pause-restart', t, re.M)) == 1, 'RESTART: ровно один перезапуск (нет цикла)')
chk('r', h and h[-1][1] == 1 and h[-1][3] == 1, 'RESTART: миссия идёт (phase=1 status=1)')
t2 = smoke('cancel_btw'); h2 = huds(t2); t3 = smoke('cancel_run'); h3 = huds(t3)
chk('c', len(re.findall(r'^MSN pause-abort', t2, re.M)) == 1, 'CANCEL из BETWEEN: ровно один выход (нет цикла)')
chk('c', h2 and h3 and h2[-1][1:3] == h3[-1][1:3], 'CANCEL из BETWEEN даёт то же состояние, что из RUNNING (phase/stage: %s против %s)' % (h2[-1][1:3] if h2 else None, h3[-1][1:3] if h3 else None))
print('PAUSEBETWEEN1348_PRESS', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
