#!/usr/bin/env python3
"""Build 95 longgame: check_long.py smoke.txt [MIN_MX0_МБ=3] [МАКС_УПРАВЛЕНИЕ_С=25]. Зелёный: MEMFAILSZ 0, WATCHDOG mem ... fail 0, npc! 0, раздробление FRAGBLK было,
конец уровня как у игрока: LEVELDONE n=1 -> LVSAVEASK open -> closed -> ровно одна LOADNEWS -> LEVEL ev=load n=2, после XITION level WATCHDOG spawn 0, XITION level ms <= 25 с
(эталон PS2: 23 с от Accept), наибольший кусок при старте уровня 2 (mx0) >= MIN МБ. Печатает цифры и PASS / FAIL."""
import re, sys
L = open(sys.argv[1], errors='replace').read().split('\n')
LV = next((int(x[3:]) for x in sys.argv[2:] if x.startswith("--L") and x[3:].isdigit()), 1)   # --L5: конец уровня 5 -> 6; ролик конца: 2 fmv3, 5 fmv5, 6 fmv6
FMV = {2: 'fmv3', 5: 'fmv5', 6: 'fmv6'}.get(LV); MID = {4: 'fmv4'}.get(LV)   # MID: ролик в середине уровня (L4 m5)
A = [x for x in sys.argv[2:] if not x.startswith('--')]
MIN_MX0 = float(A[0]) if A else 3.0; MAX_CTL = float(A[1]) if len(A) > 1 else 25.0
ok = True
def say(k, v, good=None):
    global ok
    if good is False: ok = False
    print(f"  {k}: {v}{'' if good is None else ('  ok' if good else '  ПРОВАЛ')}")
def idx(p, start=0):
    for i in range(start, len(L)):
        if re.search(p, L[i]): return i
    return -1
def cnt(p, a=0, b=None): return sum(1 for l in L[a:b] if re.search(p, l))
kv = lambda l: dict(p.split('=', 1) for p in l.split() if '=' in p)
fb = [kv(l) for l in L if l.startswith('FRAGBLK ')]
NOFRAG = '--nofrag' in sys.argv   # зелёный контроль без раздробления (куча целая): MEMFAILSZ обязан быть 0 и без fragblk
say('раздробление FRAGBLK (f / наибольший кусок МБ)', f"{fb[0].get('f')}/{fb[0].get('largest')}" if fb else 'нет строки', bool(fb) or NOFRAG)
mf_all = [kv(l) for l in L if l.startswith('MEMFAILSZ ')]
fbsz = int(float(fb[0].get('f', 0)) / 2 * 1048576) if fb else -1   # последний отказ самой раздробляющей заливки fragblk (блок F/2) — служебный, не игровой
mf = list(mf_all)
for i, m in enumerate(mf):
    if fbsz > 0 and abs(int(m.get('bytes', 0)) - fbsz) <= 1: del mf[i]; break
from collections import Counter
hist = Counter((m.get('ra'), m.get('bytes')) for m in mf)
say('MEMFAILSZ (без одного служебного отказа fragblk)', len(mf), len(mf) == 0)
for (ra, b), n in hist.most_common(4): print(f"      ra={ra} bytes={b} x{n}")
say('WATCHDOG mem ... fail', cnt(r'WATCHDOG mem .*\bfail'), cnt(r'WATCHDOG mem .*\bfail') == 0)
say('npc!', cnt(r'npc!'), cnt(r'npc!') == 0)
say('PILOT end (кругов автопилота)', cnt(r'^PILOT end'), cnt(r'^PILOT end') >= 1)
say('вход в комнату/дверь (HOMER room=)', cnt(r'HOMER .* room=(?!-)'), None)
say('DEATH (смерть)', cnt(r'^DEATH'), None)
ld = idx(r'^LEVELDONE n=%d' % LV); say('LEVELDONE n=%d' % LV, 'есть' if ld >= 0 else 'нет', ld >= 0)
if ld >= 0:
    sc = {m.group(1): float(m.group(2)) for l in L for m in [re.match(r'SCENE (\S+) ([\d.]+)', l)] if m}
    if 'LG_L1' in sc and 'LG_end' in sc: say('игровых секунд долгой фазы (LG_L1 -> LG_end)', f"{sc['LG_end'] - sc['LG_L1']:.0f}", None)
    ao = idx(r'^LVSAVEASK open', ld); ac = idx(r'^LVSAVEASK closed', ao if ao >= 0 else ld)
    say('вопрос о записи: open -> closed', f"{ao >= 0}/{ac >= 0}", ao >= 0 and ac > ao)
    l2 = idx(r'^LEVEL ev=load n=%d' % (LV + 1), ld)
    say('LEVEL ev=load n=%d' % (LV + 1), 'есть' if l2 >= 0 else 'нет', l2 >= 0)
    if l2 >= 0:
        ln = cnt(r'^LOADNEWS', ld, l2 + 1); say('LOADNEWS между концом и загрузкой L2 (ровно одна)', ln, ln == 1)
        m = kv(L[l2]); mx0 = int(m.get('mx0', 0)) / 1048576.0; say('mx0 при старте следующего уровня, МБ', f"{mx0:.2f}", mx0 >= MIN_MX0)
        xi = idx(r'^XITION level', l2)
        if xi >= 0:
            ms = int(kv(L[xi]).get('ms', 0)); say('XITION level ms (управление), с', f"{ms / 1000.0:.1f}", ms <= MAX_CTL * 1000)
            sp = [l for l in L[xi:] if l.startswith('WATCHDOG spawn')]
            bad = [l for l in sp if (l.split() + ['', '', '', '', ''])[4] not in ('scene', 'release', 'hold') or float((re.search(r'waited=([\d.]+)', l) or [0, 0])[1]) >= 1.5]
            say('WATCHDOG spawn после XITION level (всего / не штатных: не scene|release|hold или ожидание >= 1,5 с)', f'{len(sp)}/{len(bad)}', not bad)
        else: say('XITION level', 'нет', False)
if FMV:
    mv = [kv(l) for l in L if l.startswith('MSNFMV ') and f'name={FMV}' in l]
    say(f'ролик {FMV} сыгран (MSNFMV played=1)', mv[-1] if mv else 'нет строки', bool(mv) and mv[-1].get('played') == '1' and int(mv[-1].get('shown', 0)) >= 25)
if MID:
    mv = [kv(l) for l in L if l.startswith('MSNFMV ') and f'name={MID}' in l]
    say(f'ролик {MID} (середина уровня) сыгран', mv[-1] if mv else 'нет строки', bool(mv) and mv[-1].get('played') == '1' and int(mv[-1].get('shown', 0)) >= 25)
if LV == 4: say('DEATH (смерть) на L4', cnt(r'^DEATH'), cnt(r'^DEATH') >= 1)
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
