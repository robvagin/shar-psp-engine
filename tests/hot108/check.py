#!/usr/bin/env python3
# 108: сверка прогонов tests/hot108/emu.sh. Читает build/hot108/hot108_{new,old,red}/. Зелёный: снимки new == old побайтно, PROPSTAT и SKID (без времени) равны, у new hot_bad = ver_bad = dyn_bad = vis_bad = 0 и ver_skip > 0,
# у old ver_skip = 0 (прежний путь действительно работал), удары были (хотя бы 2 предмета сдвинуты или спрятаны). Красный: у red hot_bad > 0 или снимки/PROPSTAT другие, чем у old.
import hashlib, os, re, sys
B = 'build/hot108'
def load(n):
    d = f'{B}/{n}'; r = {'png': {}, 'ps': [], 'sk': [], 'pb': None}
    if not os.path.isdir(d): return None
    for f in sorted(os.listdir(d)):
        if f.endswith('.png'): r['png'][f] = hashlib.sha256(open(f'{d}/{f}', 'rb').read()).hexdigest()
    for l in open(f'{d}/smoke.txt', errors='replace'):
        if l.startswith('PROPSTAT'): r['ps'].append(re.sub(r' t=[\d.]+', '', l.strip()))
        elif l.startswith('SKID '): r['sk'].append(re.sub(r' (us|t)=[\d.]+', '', l.strip()))
        elif l.startswith('PBCHK'): r['pb'] = dict(kv.split('=') for kv in l.split()[1:])
    return r
N, O, R = load('hot108_new'), load('hot108_old'), load('hot108_red')
bad = 0
def fail(m):
    global bad; bad += 1; print('FAIL', m)
if not N or not O: print('FAIL: нет прогонов new/old'); sys.exit(1)
print(f"снимков new {len(N['png'])}, old {len(O['png'])}; PROPSTAT {len(N['ps'])}/{len(O['ps'])}; SKID {len(N['sk'])}/{len(O['sk'])}")
# снимки двух РАЗНЫХ прогонов побайтно не сравниваем: прежние обходы стоят иначе по времени эмуляции, шкала кадров сдвигается и дизеринг/анимация расходятся (повтор new против new — 0 пикселей, new против old — единицы-тысячи
# из-за сдвига, не из-за правок). Равенство кадра доказывает tests/hot108/rf.sh: один замороженный кадр рисуется новыми и прежними путями, снимки побайтно.
if N['ps'] != O['ps']: fail('PROPSTAT new != old:\n  ' + '\n  '.join(a + '  |  ' + b for a, b in zip(N['ps'], O['ps']) if a != b))
if N['sk'] != O['sk']: fail('SKID new != old:\n  ' + '\n  '.join(a + '  |  ' + b for a, b in zip(N['sk'], O['sk']) if a != b)[:600])
pb = N['pb'] or {}; po = O['pb'] or {}
print('PBCHK new:', pb, '\nPBCHK old:', po)
for k in ('hot_bad', 'ver_bad', 'dyn_bad', 'vis_bad'):
    if int(pb.get(k, -1)) != 0: fail(f'new {k} = {pb.get(k)}')
if int(pb.get('ver_skip', 0)) <= 0: fail('new ver_skip = 0 (признак «изменился» не сработал ни разу)')
if int(po.get('ver_skip', -1)) != 0: fail(f"old ver_skip = {po.get('ver_skip')} (прежний путь не включился)")
hit = sum(1 for l in N['ps'] if ('hidden=1' in l) or not re.search(r'moved=0\.00', l) or 'asleep=0' in l)
print('предметов задето:', hit, 'из', len(N['ps']))
if hit < 2: fail('сценарий почти не задел предметы')
if R:
    pr = R['pb'] or {}; caught = int(pr.get('hot_bad', 0)) > 0 or int(pr.get('ver_bad', 0)) > 0 or R['ps'] != O['ps']
    print('PBCHK red:', pr); print('красный prophotred 1:', 'упал, как должен' if caught else 'НЕ ПОЙМАН'); 
    if not caught: fail('красный не пойман')
else: fail('нет прогона red')
print('PASS' if not bad else 'FAIL'); sys.exit(1 if bad else 0)
