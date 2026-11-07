"""№1177: цена кадра города по журналу тура PSP (smoke.txt): свод по сценам и регрессия «лист клетки / вызов / риск у камеры».
  python3 -I tools/far1177/tour_dl_regress.py build/psp-tour/<папка>/smoke.txt [сцена,сцена,...]
Читает строки DCITY (dlcity мкс; tq/risky/gg/farbuild — части) и RFAST (calls — все вызовы листов клеток за кадр, draws, ktris) того же времени.
«Остальное» = dlcity − (tq + far + fall + risky + gg + farbuild): цена самого обхода клеток и вызовов листов. Регрессия — по всем сценам L1/L5.
Закон: числа PSP (журнал с карты), не Мак ×5,8."""
import re, sys, collections, numpy as np
p = sys.argv[1]; sc = sys.argv[2].split(',') if len(sys.argv) > 2 else ['F_GO9_L1', 'F_GO2_L5', 'F_MEHAND_L5_OFF', 'F_AVE_L5', 'F_971_L5', 'F_FLAT_L5', 'F_GO4_L5']
rf = {}; D = collections.defaultdict(list); X = []; Y = []
for l in open(p, errors='replace'):
    if l.startswith('RFAST'):
        m = re.search(r't=([\d.]+) sc=(\S+)', l); g = lambda k: float(re.search(r' %s=([\d.]+)' % k, l).group(1))
        rf[int(float(m.group(1)))] = (g('calls'), g('draws'), g('ktris'))
    elif l.startswith('DCITY'):
        m = re.search(r't=([\d.]+) sc=(\S+) tq=(\d+)/(\d+) far=(\d+)/(\d+) hyb=\d+/\d+ fall=(\d+)/(\d+) risky=(\d+)/(\d+) gg=(\d+)/(\d+) farlist=(\d+) farbuild=(\d+)/(\d+) dlcity=(\d+) stat=(\d+)', l)
        if not m: continue
        t = float(m.group(1)); v = [float(x) for x in m.groups()[2:]]
        tq_n, tq_us, fn, fu, fln, flu, rk_n, rk_us, gg_n, gg_us, farlist, fb_n, fb_us, dl, stat = v
        r = rf.get(int(t)) or rf.get(int(t) - 1) or rf.get(int(t) + 1)
        if not r: continue
        calls, draws, kt = r; other = dl - rk_us - gg_us - tq_us - fu - flu - fb_us
        if m.group(2) in sc: D[m.group(2)].append([dl, rk_us, gg_us, tq_us, other, farlist, max(calls - farlist, 0), draws, kt])
        X.append([1, farlist, max(calls - farlist, 0), tq_n, rk_n, gg_n, draws / 100, kt]); Y.append(other)
print('%-16s %7s %7s %6s %6s %8s %7s %7s %7s' % ('сцена', 'dlcity', 'risky', 'gg', 'tq', 'остальн.', 'дальн.л', 'ближн.л', 'draws'))
for s in sc:
    if D[s]: a = np.mean(D[s], 0); print('%-16s %7.0f %7.0f %6.0f %6.0f %8.0f %7.1f %7.1f %7.0f' % (s, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]))
X = np.array(X); Y = np.array(Y); b = np.linalg.lstsq(X, Y, rcond=None)[0]; pr = X @ b
print('\nрегрессия по %d кадрам (R2 %.2f): остальное, мкс = %.0f + %.1f·дальн.листов + %.1f·ближн.листов + %.1f·tq + %.1f·рискованных клеток + %.1f·gg + %.1f·(draws/100) + %.1f·ктрис' % ((len(Y), 1 - ((Y - pr) ** 2).sum() / ((Y - Y.mean()) ** 2).sum()) + tuple(b)))
