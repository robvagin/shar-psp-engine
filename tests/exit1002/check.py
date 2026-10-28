#!/usr/bin/env python3
# №1002: кадры «выхода из дома» в туре PSP (smoke.txt TEST70) и в эмуляторе. Использование:
#   check.py psp SMOKE            каждый SLOWFRAME > 150 мс сцен F_GO* стоит рядом (±0,1 с) со строкой «PILOT seat» (flag drive) — последняя строка PASS/FAIL
#   check.py emu SMOKE            таблица кадров EXIT1002 (мс Мака, ×5,8 = PSP): телепорт-выход и выход дверью
import re, sys
def rows(p):
    return [l.rstrip("\n") for l in open(p, errors="ignore")]
mode, p = sys.argv[1], sys.argv[2]
L = rows(p)
if mode == "psp":
    seats = [float(m[1]) for l in L if (m := re.match(r"PILOT seat .* t=([\d.]+)", l))]
    ok = True; n = 0
    for l in L:
        m = re.match(r"SLOWFRAME t=([\d.]+) work=([\d.]+) .* scene=(F_GO\S+)", l)
        if m and float(m[2]) > 150:
            n += 1; t = float(m[1]); near = any(abs(t - s) <= 0.1 for s in seats)
            print(f"{m[3]} t={t} work={m[2]} мс  PILOT seat рядом: {'да' if near else 'НЕТ'}"); ok &= near
    ok &= n >= 3
    print("PASS" if ok else "FAIL")
elif mode == "iris":   # кадры выхода дверью: чёрная выдержка ириса (w=0) и кадры раскрытия (w>0); работа кадра раскрытия ≤ 12 мс Мака (≈ 70 мс PSP; пол кадра стоячей сцены 8–9)
    w = None; wt = -9.0; worst = {}; ok = True; any_exit = 0
    for l in L:
        m = re.match(r"IRIS why=room_out phase=(hold|open) f=[\d.]+ w=([\d.]+) r=[\d.]+ t=([\d.]+)", l)
        if m: w = float(m[2]); wt = float(m[3]); continue
        m = re.match(r"EXIT1002 left=\d+ t=([\d.]+) work=([\d.]+) .* scene=(\S+) marks (.*?) \|", l)
        if m and w is not None and abs(float(m[1]) - wt) < 0.02:
            sc = m[3]; iris = float(m[4].split()[3].split(":")[1]); wk = float(m[2]) - (iris if iris > 300 else 0)
            k = (sc, "раскрытие" if w > 0 else "выдержка"); worst[k] = max(worst.get(k, 0), wk); w = None
    for (sc, ph), v in sorted(worst.items()):
        print(f"{sc} {ph}: худший кадр {v:.1f} мс Мака ≈ {v * 5.8:.0f} мс PSP")
        if ph == "раскрытие" and v > 12: ok = False
    ok &= any(ph == "раскрытие" for _, ph in worst)
    print("PASS" if ok else "FAIL")
else:
    sc = ""
    for l in L:
        m = re.match(r"EXIT1002 left=(\d+) t=([\d.]+) work=([\d.]+) cpu=([\d.]+) logic=([\d.]+) scene=(\S+) marks (.*?) \| (.*)", l)
        if not m: continue
        left, t, work, cpu, logic = int(m[1]), float(m[2]), float(m[3]), float(m[4]), float(m[5])
        mk = [float(x.split(":")[1]) for x in m[7].split()]
        if left >= 13 and (work > 300 and mk[0] > 100): continue   # кадры загрузки уровня в безголовом эмуляторе (холодный старт, не игра)
        iris = mk[3] if mk[3] > 300 else 0.0   # ирис закрывается внутри кадра выхода: время ириса не работа
        w = work - iris
        print(f"{m[6]} t={t} left={left} работа {w:.1f} мс Мака ≈ {w * 5.8:.0f} мс PSP (ирис {iris:.0f}) | {m[8]}")
