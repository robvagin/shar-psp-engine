#!/usr/bin/env python3
"""№ скорость 250: проверка выгрузки клеток при тесной куче. check_trim.py new|red|free ДИР_НОВОГО [ДИР_КРАСНОГО]
new: пик stream() ≤ 4 мс Мака (≈ 23 мс PSP) и ни одного периода >3 мс, MEMTRIM сработал (куча была тесной), тот же уровень MISSCELL/missvis, что у красного, p99 кадра сцены не хуже, кадры r1/r2 с красным: ≤ 2 % точек вне
строк HUD (смещение фазы анимации и дыма — не геометрия). red: на выключателе (flag memtrim1060 0) тот же маршрут даёт ≥ 5 периодов >3 мс и пик ≥ 8 мс. free: куча не тесная — MEMTRIM нет (цена приёма ноль)."""
import re, sys, os
mode, d1 = sys.argv[1], sys.argv[2]; d2 = sys.argv[3] if len(sys.argv) > 3 else None
ok = True
def need(c, m):
    global ok; print(('  ok   ' if c else '  FAIL ') + m); ok &= bool(c)
def stats(d):
    T = open(os.path.join(d, 'smoke.txt'), errors='replace').read().splitlines()
    mx = [int(re.search(r'max_us=(\d+)', l).group(1)) for l in T if l.startswith('STREAMCOST')]
    trim = [int(re.search(r'freed=(\d+)', l).group(1)) for l in T if l.startswith('MEMTRIM')]
    miss = sum(1 for l in T if l.startswith('MISSCELL'))
    mv = sum(int(m.group(1)) for l in T if l.startswith('STAT F_L4_CR1') for m in [re.search(r'missvis (\d+)', l)] if m)
    p99 = [float(m.group(1)) for l in T if l.startswith('STAT F_L4_CR1') for m in [re.search(r' p99 ([\d.]+)', l)] if m]
    return dict(end=any(l.startswith('END') for l in T), mx=max(mx) if mx else 0, spikes=sum(1 for v in mx if v > 3000), trim=len(trim), miss=miss, mv=mv, p99=sum(p99) / len(p99) if p99 else 0)
a = stats(d1)
need(a['end'], 'прогон дошёл до END')
if mode == 'new':
    need(a['mx'] <= 4000 and a['spikes'] == 0, f"пик stream() {a['mx']/1000:.1f} мс Мака (≈ {a['mx']*5.8/1000:.0f} мс PSP), периодов >3 мс: {a['spikes']}")
    need(a['trim'] >= 1, f"выгрузка при тесной куче сработала (MEMTRIM событий {a['trim']})")
    if d2:
        b = stats(d2)
        need(a['miss'] <= b['miss'] and a['mv'] <= b['mv'], f"видимых незагруженных не больше, чем у красного: MISSCELL {a['miss']} против {b['miss']}, missvis {a['mv']} против {b['mv']}")
        need(a['p99'] < b['p99'], f"p99 кадра в сцене ниже, чем у красного: {a['p99']:.1f} против {b['p99']:.1f} мс Мака")
        try:
            from PIL import Image, ImageChops
            for n in ('r1', 'r2'):
                x = Image.open(os.path.join(d1, n + '.png')).convert('RGB').crop((0, 40, 480, 272)); y = Image.open(os.path.join(d2, n + '.png')).convert('RGB').crop((0, 40, 480, 272))
                df = ImageChops.difference(x, y); bad = sum(1 for p in df.getdata() if max(p) > 8); tot = x.size[0] * x.size[1]
                need(bad / tot <= 0.02, f'кадр {n}: точек вне HUD, отличающихся от красного: {100.0 * bad / tot:.2f} % (не больше 2 %)')
        except Exception as e: need(False, f'сравнение кадров: {e}')
elif mode == 'red':
    need(a['spikes'] >= 5 and a['mx'] >= 8000, f"выключатель: пик stream() {a['mx']/1000:.1f} мс Мака, периодов >3 мс: {a['spikes']} (красный обязан быть красным)")
else:
    need(a['trim'] == 0, f"куча не тесная: выгрузки нет (MEMTRIM {a['trim']}), приём не включался; пик stream() {a['mx']/1000:.1f} мс — телепорт сценария (jumpRush), не выгрузка")
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
