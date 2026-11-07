#!/usr/bin/env python3
"""Итог прогона «начало-конец миссий» (06.11, Роб): по журналам runs/missions_L*/smoke.txt — у каждой миссии сценария (сцены MS_<метка>_start..):
название, сколько этапов пройдено (MSN done), дошла ли до MSN complete, на каком этапе и цели встала; сбои уровня (STALL, отказы памяти, нет END).
  python3 tools/night/msnflow_report.py NIGHT_DIR [--prev ПРЕЖНИЙ_NIGHT_DIR]  -> таблица в stdout; с --prev — что стало хуже/лучше по миссиям"""
import re, sys
from pathlib import Path
def parse(night):
    res = {}
    for p in sorted(Path(night).glob('runs/missions_L*/smoke.txt')):   # L5 или половинки L5a/L5b (06.11 msnflow7)
        L = p.parent.name[len('missions_L'):]
        cur = None; segs = []; bad = {'stall': 0, 'memfail': 0}
        txt = p.read_text(errors='replace').splitlines()
        end = any(l == 'END' for l in txt); hl = p.parent / 'headless.log'; tout = hl.exists() and 'TIMEOUT' in hl.read_text(errors='replace')   # TIMEOUT — прогон упёрся в лимит времени эмулятора, хвост сценария не выполнялся
        for l in txt:
            m = re.match(r'SCENE MS_(\w+?)_start', l)
            if m: cur = dict(tag=m.group(1), name='', id='', done=0, complete=0, last='', run=1, passed=0, fail=''); segs.append(cur); continue
            if l.startswith('BBSTALL') or l.startswith('STALL'): bad['stall'] += 1
            if l.startswith('MEMFAILSZ'): bad['memfail'] += 1
            if cur is None: continue
            m = re.match(r'MSN mission (\S+) "([^"]*)"', l)
            if m and not cur['name']: cur['id'] = m.group(1); cur['name'] = m.group(2)   # проверяемая миссия — первая, что стартовала в сегменте (у L2–L7 msnsel k — это m(k+1))
            if not cur['name']: continue
            mid = cur['id']
            if l.startswith('MSN done') and cur.get('run', 1): cur['done'] += 1
            if l.startswith('MSN complete'): cur['complete'] = 1
            if l.startswith('MSN pass') and cur.get('run', 1): cur['passed'] += 1
            m = re.match(r'MSN failed \d+ (.*?) t=', l)
            if m and cur.get('run', 1): cur['fail'] = m.group(1)
            m = re.match(r'MSN stage (\S+) obj=(\S+) text="([^"]*)"', l)
            if m: cur['run'] = m.group(1).split('.')[0] == mid
            if m and cur['run']: cur['last'] = f'{m.group(1)} {m.group(2)} «{m.group(3)[:40]}»'
        res[L] = dict(segs=segs, bad=bad, end=end, tout=tout)
    return res
def key(L, s): return f'L{L} {s["id"] or s["tag"]}'
if __name__ == '__main__':
    night = sys.argv[1]; R = parse(night)
    prev = parse(sys.argv[sys.argv.index('--prev') + 1]) if '--prev' in sys.argv else None
    tot = comp = 0
    for L, r in R.items():
        print(f'## L{L}: конец сценария {"да" if r["end"] else "НЕТ"}{" (лимит времени прогона)" if r["tout"] else ""}, STALL {r["bad"]["stall"]}, отказов памяти {r["bad"]["memfail"]}')
        for s in r['segs']:
            if r['tout'] and not s['name']: print(f'  {s["tag"]:4} не успела: прогон упёрся в лимит времени'); continue
            tot += 1; comp += s['complete']
            st = 'ПРОЙДЕНА' if s['complete'] else f'встала: {s["last"] or "не началась"}' + (f' — провал «{s["fail"]}»' if s['fail'] else '')
            print(f'  {s["id"] or s["tag"]:4} {s["name"][:32]:32} этапов {s["done"]:2} (засчитано помощником {s["passed"]})  {st}')
    print(f'Итого: миссий {tot}, до конца {comp}')
    if prev:
        P = {key(L, s): s for L, r in prev.items() for s in r['segs']}
        for L, r in R.items():
            for s in r['segs']:
                p = P.get(key(L, s))
                if p and p['complete'] and not s['complete']: print('ХУЖЕ:', key(L, s), s['name'], '— было до конца, теперь', s['last'])
                elif p and not p['complete'] and s['complete']: print('ЛУЧШЕ:', key(L, s), s['name'])
