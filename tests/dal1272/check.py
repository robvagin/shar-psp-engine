"""Даль-1272: сравнение прогонов «новое» и «старое» (dal1272old) по строкам DAL1272 / STREAMCOST / STAT.  python3 -I check.py NEW_smoke.txt OLD_smoke.txt
PASS: в новом нет кадров с дырой в круге 150 м (f150 = 0); в новом кадров с дырой внутри текущей дальности (fdd) не больше трети старых;
в старом дыры в круге 150 м есть (красный контроль: гейт видит старое поведение). Цена: среднее stream() мкс и минимум кучи печатаются."""
import re, sys
def read(p):
    r = dict(fr=0, f150=0, f200=0, fdd=0, sumdd=0, maxdd=0, us=[], heap=99.0, ddmin=999., ddmax=0., dds=[])
    for l in open(p, errors='replace'):
        d = dict(re.findall(r'(\w+)=([\d.]+)', l))
        if l.startswith('DAL1272'):
            r['fr'] += int(d['frames']); r['f150'] += int(d['f150']); r['f200'] += int(d['f200']); r['fdd'] += int(d['fdd']); r['sumdd'] += int(d['sumdd']); r['maxdd'] = max(r['maxdd'], int(d['maxdd']))
            r['dds'].append(float(d['dd'])); r['ddmin'] = min(r['ddmin'], float(d['dd'])); r['ddmax'] = max(r['ddmax'], float(d['dd']))
        elif l.startswith('STREAMCOST'): r['us'].append(int(d['avg_us']))
        elif l.startswith('STAT ') and ' heapmin ' in l:
            t = l.split(); r['heap'] = min(r['heap'], float(t[t.index('heapmin') + 1])) if t[1] != 'start' else r['heap']
    return r
n, o = read(sys.argv[1]), read(sys.argv[2])
for lab, r in (('новое ', n), ('старое', o)):
    print(f"{lab}: кадров {r['fr']}  дыра в 150 м {r['f150']}  дыра в 200 м {r['f200']}  дыра внутри дальности {r['fdd']} (клеток-кадров {r['sumdd']}, макс {r['maxdd']})  dd {r['ddmin']:.0f}-{r['ddmax']:.0f} (среднее {sum(r['dds'])/max(1,len(r['dds'])):.0f})  stream() {sum(r['us'])/max(1,len(r['us'])):.0f} мкс  куча мин {r['heap']:.2f} МБ")
ok = n['f150'] == 0 and n['fdd'] * 3 <= o['fdd'] and o['f150'] > 0
print('красный контроль (старое видит дыры в 150 м):', 'да' if o['f150'] > 0 else 'НЕТ')
print('PASS' if ok else 'FAIL'); sys.exit(0 if ok else 1)
