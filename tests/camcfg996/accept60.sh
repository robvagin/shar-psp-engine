#!/bin/bash
# №996 приёмка: страница CAMERA — Height и Distance (5 шагов, середина как было), настройки читаются из старых файлов, камера у стен не лезет глубже. accept60.sh NEW_EBOOT [red]
#   Нужны OVL с pmenu.shr нового вида (tools/menu_export.py) и SET (набор тура). Прогоны эмулятора: A — камера (пешком и в машине), B — старый settings.bin (ver 10) + смена строк в меню + запись, C — чтение записанного.
#   Мера A (журнал CAMU996): пешком Height — отступ глаза над ступнями 1,69 / 2,04 / 2,38 / 2,73 / 3,07 (±0,06), Distance — 4,52 / 5,48 / 6,45 / 7,42 / 8,39 (±0,08); середина = как TEST70 (6,450 и 2,380);
#   в машине расстояния Distance относятся как .70 / .85 / 1 / 1,15 / 1,30 (±0,05), высота растёт с шагом; у стен (W3, W4, W6) на краю (High + Far) стена держит камеру (расстояние до Гомера меньше 6,7 м при 8,39 желаемых, лучи в стену есть), глаз за стеной или над пустотой — 0 кадров.
#   Мера B/C: в старом файле (ver 10, 88 байт) громкость 700/800/900/1000 и Ink прочитаны, Height и Distance = Normal; после смены в меню и выхода файл (ver 12, 100 байт, №1003: в конце Height, Distance, Car Shadows) содержит High / Near и Car Shadows Off, и прогон C читает их обратно.
#   «red» — красный контроль: прогон A с файлом camcfg996old (множители всегда 1) -> шаги не меняют камеру -> FAIL. Последняя строка PASS / FAIL
D=$(cd "$(dirname "$0")" && pwd); I=$(cd "$D/../.." && pwd); RUN=${RUN:-$I/tools/emu_run.sh}; export OUT=${OUT:-/tmp/camcfg996_out}; export EMU_OWNER=${EMU_OWNER:-Мультик}; export EMU_MINE=${EMU_MINE:-2}
export OVL=${OVL:-/tmp/c996/ovl}; export SET=${SET:-$HOME/Downloads/shar-psp-integ/build/test70tour/PSP/GAME/SHARTEST15}
EB=$1; RED=0; [ "${2:-}" = red ] && RED=1
[ -f "$EB" ] || { echo "нет EBOOT $EB"; echo FAIL; exit 1; }
mkdir -p $OUT; rm -rf $OUT/out_acc_* $OUT/rr_acc_* $OUT/ovl_b $OUT/ovl_c
cp -R "$OVL" $OUT/ovl_b; cp -R "$OVL" $OUT/ovl_c
python3 -I - $OUT/ovl_b/settings.bin <<'PYEOF'
import struct, sys
d = b'SHST' + struct.pack('<7i', 10, 700, 800, 900, 1000, 1, 1) + struct.pack('<2I', 0, 3) + struct.pack('<4i', 130, 1, 1, 0) + struct.pack('<7i', 120, 6, 2, 40, 2, 3, 1) + struct.pack('<i', 1)
open(sys.argv[1], 'wb').write(d)
PYEOF
( [ $RED = 1 ] && export FLAGS=camcfg996old; $RUN $D/scenario_A.txt acc_a "$EB" 500 >/dev/null 2>&1 ) &
( OVL=$OUT/ovl_b KEEP_RR=1 $RUN $D/scenario_B.txt acc_b "$EB" 200 >/dev/null 2>&1; cp $OUT/rr_acc_b/settings.bin $OUT/ovl_c/settings.bin 2>/dev/null; OVL=$OUT/ovl_c $RUN $D/scenario_C.txt acc_c "$EB" 200 >/dev/null 2>&1 ) &
wait
python3 -I - $OUT $OUT/rr_acc_b/settings.bin <<'PYEOF'
import sys, re, struct
out, sb = sys.argv[1], sys.argv[2]
ok_all = True
def chk(ok, s):
    global ok_all; ok_all &= bool(ok); print('  %s %s' % ('ok  ' if ok else 'FAIL', s))
def rd(n):
    try: return open('%s/out_%s/smoke.txt' % (out, n), encoding='utf8', errors='replace').read()
    except OSError: return ''
A = rd('acc_a')
if not A: print('нет smoke.txt прогона A'); print('FAIL'); sys.exit(1)
# последовательность: команды flag (CAMCFG996) и снимки камеры CAMU996 по времени
ev = []
for m in re.finditer(r'^CAMCFG996 t=([\d.]+) on=\d height=(\d) dist=(\d)', A, re.M): ev.append((float(m.group(1)), 'F', int(m.group(2)), int(m.group(3))))
for m in re.finditer(r'^CAMU996 t=([\d.]+) foot=(\d) d=([\d.]+) h=(-?[\d.]+) hits=(\d+) wall=([\d.]+)', A, re.M): ev.append((float(m.group(1)), 'U', int(m.group(2)), float(m.group(3)), float(m.group(4)), int(m.group(5)), float(m.group(6))))
sc = [(float(m.group(2)), m.group(1)) for m in re.finditer(r'^SCENE (\S+) ([\d.]+)', A, re.M)]
ev.sort(key=lambda e: e[0])
def last_after(foot, t0, t1):   # последний снимок камеры с признаком foot в окне
    r = [e for e in ev if e[1] == 'U' and e[2] == foot and t0 <= e[0] < t1]
    return r[-1] if r else None
def scene_range(name):
    for i, (t, n) in enumerate(sc):
        if n == name: return t, (sc[i + 1][0] if i + 1 < len(sc) else 1e9)
    return None
def series(name, foot):   # (height, dist) -> снимок через 2 с после смены
    rg = scene_range(name); res = {}
    if not rg: return res
    fl = [e for e in ev if e[1] == 'F' and rg[0] <= e[0] < rg[1]]
    for k, f in enumerate(fl):
        t1 = fl[k + 1][0] if k + 1 < len(fl) else rg[1]
        u = last_after(foot, f[0] + 1.8, t1)
        if u: res[(f[2], f[3])] = u
    return res
S = series('A_FOOT', 1)
hs = [S.get((h, 2)) for h in range(5)]; ds = [S.get((2, d)) for d in range(5)]
want_h = [1.69, 2.035, 2.38, 2.725, 3.07]; want_d = [4.515, 5.4825, 6.45, 7.4175, 8.385]
if all(hs) and all(ds):
    chk(all(abs(hs[i][4] - want_h[i]) <= .06 for i in range(5)), 'пешком Height: высота глаза над ступнями %s (нужно %s ±0,06)' % ([round(u[4], 2) for u in hs], want_h))
    chk(all(abs(ds[i][3] - want_d[i]) <= .08 for i in range(5)), 'пешком Distance: расстояние %s (нужно %s ±0,08)' % ([round(u[3], 2) for u in ds], [round(x, 2) for x in want_d]))
    chk(abs(hs[2][3] - 6.45) < .01 and abs(hs[2][4] - 2.38) < .01, 'середина = как TEST70: %.3f / %.3f (6,450 / 2,380)' % (hs[2][3], hs[2][4]))
else: chk(False, 'пешком: нет снимков камеры по шагам')
C = series('A_CAR', 0)
ch = [C.get((h, 2)) for h in range(5)]; cd = [C.get((2, d)) for d in range(5)]
if all(ch) and all(cd):
    kd = [cd[i][3] / cd[2][3] for i in range(5)]
    chk(all(abs(kd[i] - [.7, .85, 1, 1.15, 1.3][i]) <= .05 for i in range(5)), 'машина Distance: отношения %s (нужно .70 .85 1 1,15 1,30 ±0,05)' % [round(x, 2) for x in kd])
    hh = [u[4] for u in ch]
    chk(all(hh[i] < hh[i + 1] - .1 for i in range(4)), 'машина Height: высота глаза растёт с шагом %s' % [round(x, 2) for x in hh])
else: chk(False, 'машина: нет снимков камеры по шагам')
for nm in ('W3', 'W4', 'W6'):
    m, f = scene_range(nm + '_mid'), scene_range(nm + '_far')
    if not m or not f: chk(False, nm + ': нет сцены'); continue
    um, uf = last_after(1, m[0] + 3.0, m[1]), last_after(1, f[0] + 3.0, f[1])
    if not um or not uf: chk(False, nm + ': нет снимков'); continue
    chk(um[5] > 0 and uf[5] > 0 and uf[3] < .8 * 8.385, '%s у стены: середина %.2f м, дальний край %.2f м — стена держит камеру (дальше 6,7 м не уходит; лучей в стену: %d и %d)' % (nm, um[3], uf[3], um[5], uf[5]))
bl = [int(x) for x in re.findall(r'^CAMW t=[\d.]+ frames=\d+ blocked=(\d+)', A, re.M)]
chk(bool(bl) and max(bl) == 0, 'глаз за стеной или над пустотой: кадров %s (нужно 0)' % (max(bl) if bl else 'нет данных'))
B = rd('acc_b'); Cc = rd('acc_c')
rx = re.compile(r'^OPT pack=\S+ scr=\d+ sel=\d+ r=-?\d+ rows=(\S+) vol=(\d+),(\d+),(\d+),(\d+) .* ink=(\d+) camh=(\d) camd=(\d)', re.M)
ob = rx.findall(B); oc = rx.findall(Cc)
if ob:
    f = ob[0]
    chk(f[1:5] == ('700', '800', '900', '1000') and f[5] == '1' and f[6] == '2' and f[7] == '2', 'старый файл ver 10 прочитан: громкость %s, Ink %s, Height %s, Distance %s (нужно 700..1000, 1, 2, 2)' % ('/'.join(f[1:5]), f[5], f[6], f[7]))
    l = ob[-1]; chk(l[6] == '4' and l[7] == '0', 'после смены в меню: Height %s (High = 4), Distance %s (Near = 0)' % (l[6], l[7]))
else: chk(False, 'B: нет строк OPT в журнале')
try:
    sbd = open(sb, 'rb').read(); ver, h_, d_, cs_ = struct.unpack_from('<i', sbd, 4)[0], struct.unpack_from('<i', sbd, len(sbd) - 12)[0], struct.unpack_from('<i', sbd, len(sbd) - 8)[0], struct.unpack_from('<i', sbd, len(sbd) - 4)[0]   # ver 12 (№1003): в конце Height, Distance, Car Shadows
    chk(len(sbd) == 100 and ver in (12, 13) and (h_, d_) == (4, 0) and cs_ == 0 and sbd[8:12] == struct.pack('<i', 700), 'записанный файл: %d байт, ver %d, Height %d, Distance %d, Car Shadows %d, громкость сохранена' % (len(sbd), ver, h_, d_, cs_))
except Exception as e: chk(False, 'записанный файл не найден (%s)' % e)
if oc: chk(oc[0][6] == '4' and oc[0][7] == '0' and oc[0][1] == '700', 'C: записанный файл прочитан обратно: Height %s, Distance %s, громкость %s' % (oc[0][6], oc[0][7], oc[0][1]))
else: chk(False, 'C: нет строк OPT в журнале')
print('PASS' if ok_all else 'FAIL'); sys.exit(0 if ok_all else 1)
PYEOF
