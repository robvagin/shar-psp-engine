"""Даль-1272: stream() раз в 2 кадра против каждого кадра (файл streamhalfold).  python3 -I check_half.py NEW_smoke.txt OLD_smoke.txt
PASS: в новом дыр внутри дальности и в круге 150 м не больше, чем в старом и не больше 0 сверх старого (оба ноль или новое не хуже); среднее и p90 по строкам STREAMCOST (avg_us за 2 с) ниже;
красный контроль: в старом stream() не дешевле (проверка идёт сравнением, старое = эталон «каждый кадр»). Пик max_us печатается, раз в 2 кадра он не меняется (полный проход остаётся полным)."""
import re, sys
def read(p):
    r = dict(f150=0, fdd=0, us=[], mx=[], skip=0)
    for l in open(p, errors='replace'):
        d = dict(re.findall(r'(\w+)=([\d.]+)', l))
        if l.startswith('DAL1272'): r['f150'] += int(d['f150']); r['fdd'] += int(d['fdd'])
        elif l.startswith('STREAMCOST'): r['us'].append(int(d['avg_us'])); r['mx'].append(int(d['max_us']))
    return r
def p90(v): v = sorted(v); return v[int(.9 * (len(v) - 1))] if v else 0
n, o = read(sys.argv[1]), read(sys.argv[2])
for lab, r in (('раз в 2 кадра', n), ('каждый кадр  ', o)):
    print(f"{lab}: stream() среднее {sum(r['us'])/max(1,len(r['us'])):.0f} мкс, p90 по 2-с окнам {p90(r['us'])} мкс, пик (мед. max_us) {sorted(r['mx'])[len(r['mx'])//2] if r['mx'] else 0} мкс; дыр в 150 м {r['f150']}, внутри дальности {r['fdd']}")
ok = n['f150'] <= o['f150'] and n['fdd'] <= o['fdd'] and sum(n['us']) / max(1, len(n['us'])) < sum(o['us']) / max(1, len(o['us'])) and p90(n['us']) < p90(o['us'])
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
