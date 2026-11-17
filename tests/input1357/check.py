#!/usr/bin/env python3
import re, sys, os
out = sys.argv[1]
def smoke(v): return open(os.path.join(out, 'out_in1357_%d' % v, 'smoke.txt'), errors='replace').read()
ok = True
def chk(cond, why):
    global ok; ok &= bool(cond); print('  %-100s %s' % (why, 'ok' if cond else 'FAIL'))
def first(rx, t, after=0.0):
    for m in re.finditer(rx, t, re.M):
        if float(m.group(1)) >= after: return float(m.group(1))
    return None
new, old = smoke(0), smoke(1)
cn, co = len(re.findall(r'^TALKCANCEL', new, re.M)), len(re.findall(r'^TALKCANCEL', old, re.M))
print('отмен миссии серией △: новое %d, старое %d' % (cn, co))
chk(co >= 1, 'красный контроль: на старом серия △ отменяет миссию (TALKCANCEL %d)' % co)
chk(cn == 0, 'новое: серия △ до появления подсказок миссию не отменяет (TALKCANCEL %d)' % cn)
chk(len(re.findall(r'^TALKSKIP start=1', new, re.M)) >= 1, 'новое: ✕ после появления подсказок принят (TALKSKIP start=1)')
def after(t): m = re.findall(r'^BRIEFIN1357 continue after_loaded_ms=(\d+)', t, re.M); return int(m[0]) if m else None
an, ao = after(new), after(smoke(2))
print('карточка: Continue после появления карточки, мс: новое %s, старое (без отмены △) %s' % (an, ao))
chk(an is not None and an >= 380, 'новое: Continue не раньше 0,4 с после появления карточки (%s мс)' % an)
chk(ao is not None and ao < 250, 'красный контроль: на старом Continue сразу после загрузки (%s мс)' % ao)
print('INPUT1357', 'PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
