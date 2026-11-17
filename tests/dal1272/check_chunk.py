"""Даль-1272: stream() кусками (главный цикл по половине клеток) против полного прохода (файл streamchunkold).  python3 -I check_chunk.py NEW_smoke.txt OLD_smoke.txt
Метрика пика - STREAMSEC секция «цикл клеток» (индекс 5; диагностика смоука CITYMEM/STREAMWHY не входит, её в релизе нет): медиана и p90 по 2-с окнам максимумов, p99 максимумов.
PASS: дыр в круге 150 м и внутри дальности не больше, чем в старом; медиана максимумов цикла ниже на треть и больше; среднее цикла не хуже."""
import re, sys
def read(p):
    r = dict(f150=0, fdd=0, mx=[], av=[])
    for l in open(p, errors='replace'):
        if l.startswith('DAL1272'):
            d = dict(re.findall(r'(\w+)=([\d.]+)', l)); r['f150'] += int(d['f150']); r['fdd'] += int(d['fdd'])
        elif l.startswith('STREAMSEC'):
            m = re.search(r'avg_us=([\d,]+) max_us=([\d,]+)', l); a = list(map(int, m.group(1).split(','))); b = list(map(int, m.group(2).split(',')))
            r['av'].append(a[5]); r['mx'].append(b[5])
    return r
def q(v, p): v = sorted(v); return v[int(p * (len(v) - 1))] if v else 0
n, o = read(sys.argv[1]), read(sys.argv[2])
for lab, r in (('кусками     ', n), ('полный проход', o)):
    print(f"{lab}: цикл клеток среднее {sum(r['av'])/max(1,len(r['av'])):.0f} мкс, максимум окна: p50 {q(r['mx'],.5)}, p90 {q(r['mx'],.9)}, p99 {q(r['mx'],.99)}; дыр в 150 м {r['f150']}, внутри дальности {r['fdd']}")
cut = 1 - q(n['mx'], .5) / max(1, q(o['mx'], .5)); print(f"медиана максимумов цикла упала на {100*cut:.0f} %")
ok = n['f150'] <= o['f150'] and n['fdd'] <= o['fdd'] and cut >= 0.33 and sum(n['av']) <= sum(o['av'])
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
