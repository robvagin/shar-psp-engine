"""1Б: разбор журнала прогона эмулятора (smoke.txt): куча и пул клеток.
  python3 -I tests/cellpool1b/an.py ИМЯ=smoke.txt [ИМЯ=smoke.txt ...]
По каждому прогону: HEAPB (свободно, наибольший блок, раз в секунду), LEVEL ev=load/free (heap, mx), CELLPOOL (последняя строка), MEMTRIM, MEMFAIL/CELLMEM,
пропуски кадров и «видимые недогруженные клетки» (колонка missvis строки секунды), FPS p5/медиана. Печатает таблицу для сравнения."""
import sys, re, statistics as st

def pct(a, p):
    if not a: return float('nan')
    a = sorted(a); k = (len(a) - 1) * p / 100; f = int(k); c = min(f + 1, len(a) - 1); return a[f] + (a[c] - a[f]) * (k - f)

def an(path):
    r = dict(blk=[], free=[], loadmx=[], loadheap=[], freemx=[], fps=[], miss=0, missvis=0, memtrim=0, memtrimcells=0, memfail=0, cellmem=0, allocfail=0, pool=None, cfg=None, rows=0, levels=0, heapleft=None)
    for l in open(path, errors='replace'):
        if l.startswith('HEAPB '):
            m = re.search(r'bytes=(\d+) blk=(\d+)', l)
            if m: r['free'].append(int(m[1]) / 1048576); r['blk'].append(int(m[2]) / 1048576)
        elif l.startswith('LEVEL ev=load'):
            m = re.search(r'heap=([\d.]+).*?mx=([\d.]+)', l)
            if m: r['loadheap'].append(float(m[1])); r['loadmx'].append(float(m[2])); r['levels'] += 1
        elif l.startswith('LEVEL ev=free'):
            m = re.search(r'heap=([\d.]+).*?mx=([\d.]+)', l)
            if m: r['freemx'].append(float(m[2]))
        elif l.startswith('CELLPOOL '): r['pool'] = l.strip()
        elif l.startswith('CELLPOOLCFG'): r['cfg'] = l.strip()
        elif l.startswith('MEMTRIM freed='):
            m = re.search(r'freed=(\d+)', l); r['memtrim'] += 1; r['memtrimcells'] += int(m[1]) if m else 0
        elif l.startswith('MEMFAILSZ'): r['memfail'] += 1
        elif l.startswith('HEAPLEFT'): r['heapleft'] = l.strip()
        elif re.match(r'^\d+\.\d+ \S+ [\d.]+ [\d.]+ [\d.]+ [\d.]+ -?\d+ \d+ [\d.]+ (long|view60) ', l):
            f = l.split(); r['fps'].append(float(f[2])); r['miss'] += float(f[10]); r['missvis'] += float(f[11]); r['allocfail'] = max(r['allocfail'], int(f[13])); r['rows'] += 1
    return r

def row(n, r):
    b = r['blk']; f = r['free']
    print(f"{n:<10} строк {r['rows']:5d} | куча свободно мин {min(f):5.2f} мед {st.median(f):5.2f} | блок мин {min(b):5.2f} p10 {pct(b,10):5.2f} мед {st.median(b):5.2f} конец {b[-1]:5.2f} | загрузок {r['levels']} mx после загрузки: "
          + ' '.join('%.1f' % x for x in r['loadmx']) + f" | mx после выгрузки мин {min(r['freemx']) if r['freemx'] else float('nan'):.1f} | MEMTRIM эпизодов {r['memtrim']} клеток {r['memtrimcells']} | MEMFAIL {r['memfail']} отказ выделения клеток {r['allocfail']} | пропусков {r['miss']:.0f}, недогруж. {r['missvis']:.0f} | FPS p5 {pct(r['fps'],5):.1f}")
    if r['pool']: print('           ' + r['pool'])

if __name__ == '__main__':
    for a in sys.argv[1:]:
        n, p = a.split('=', 1); row(n, an(p))
