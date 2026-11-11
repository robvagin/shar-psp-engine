#!/usr/bin/env python3
"""Гейт 100: уровень без упора в ёмкость области города. check.py key.txt [red] [уровень, по умолчанию 5]
Зелёный: после загрузки уровня ни одного WATCHDOG cellcap и ни одного WATCHDOG mem who=cells (z2Oom, city-more), дальность за езду не ниже 150 м, первые 10 с после загрузки не ниже 200 м
(без провала к 150), есть строки CITYCAP. Красный (red, с файлом citycap100old): ожидается упор (cellcap или city-more), тогда проверка завершится FAIL - так и надо."""
import re, sys
f = sys.argv[1]; args = sys.argv[2:]; lv = next((a for a in args if a.isdigit()), "5")
L = open(f, errors="replace").read().splitlines()
load = next((i for i, l in enumerate(L) if l.startswith("LEVEL ev=load n=%s " % lv)), None)
if load is None: print("FAIL нет загрузки L" + lv); sys.exit(1)
L = L[load:]
oom = [l for l in L if l.startswith("WATCHDOG cellcap") or ("WATCHDOG mem who=cells" in l and "city-more" in l)]
dd = [(float(m.group(1)), float(m.group(2))) for l in L if (m := re.match(r"DDREG t=(\S+) dd=(\d+)", l))]
cc = [l for l in L if l.startswith("CITYCAP")]
ok = True
print("упоров:", len(oom), "dd min %.0f max %.0f n=%d" % (min(d for _, d in dd), max(d for _, d in dd), len(dd)) if dd else "dd нет", "CITYCAP строк", len(cc))
if not dd: ok = False
if oom: print("FAIL упор:", oom[0][:120]); ok = False
if dd and min(d for _, d in dd) < 150: print("FAIL дальность ниже 150"); ok = False
if dd:
    t0 = dd[0][0]; early = [d for t, d in dd if t - t0 < 10]
    print("первые 10 с после загрузки: min %.0f" % min(early))
if not cc: print("FAIL нет строк CITYCAP"); ok = False
print("PASS" if ok else "FAIL"); sys.exit(0 if ok else 1)
