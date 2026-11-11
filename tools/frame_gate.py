#!/usr/bin/env python3
"""Проверки гейта «КАДР ИГРОКА» (tools/frame_gate.sh). Вход: папка с resume/ и new/ (smoke.txt, снимки). Итог — одна строка PASS/FAIL, лист кадров frame_gate_sheet.png.
  а  кузов своей машины виден: розовые пиксели в зоне машины на снимках езды (>= 1,5 %) и геометрия car3d >= 8 кусков на кадр (GATECAT)
  б  у трафика есть кузова: геометрия trf >= 2 куска на каждую нарисованную машину трафика (сцена FG_static, не меньше 3 секунд с машинами)
  в  прохожие нарисованы с текстурой: пачки скелетных моделей ушли в GE (skin > 0 при peds > 0), с битой текстурой не более 5 %
  г  предметы у дороги есть: геометрия props >= 5 кусков на кадр на езде и стоянке
  д  счётчики по категориям не ниже минимумов: city >= 10 кусков на кадр, кадров/с >= 24, доля drawPieceM→drawGeo (BIGCUT Стен) >= 95 %; NEW GAME 30 с: city и props > 0, сценарий дошёл до END
  е  сохранение Роба цело: SHA файлов SAVEDATA до и после одинаковые
Счётчики GATECAT (геометрия, реально ушедшая в drawGeo по категориям) есть только в EBOOT с гейтом; без них б, в, г, д — «не доказано» (FAIL).
--expect-fail а,б: отрицательный контроль — строка PASS, если упали именно эти проверки (и только они из а, б)."""
import re, sys, os, argparse
from collections import defaultdict
from PIL import Image


def gatecat(path, scenes):
    rows = []
    for ln in open(os.path.join(path, 'smoke.txt'), errors='replace'):
        if not ln.startswith('GATECAT '): continue
        m = re.search(r'sc=(\S+) fr=(\d+)', ln)
        if not m or m.group(1) not in scenes: continue
        g = re.search(r' geo (.*?) kt ', ln); u = re.search(r' untex (.*?) cars=', ln); t = re.search(r' (cars=.*)$', ln)
        d = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', g.group(1))} if g else {}
        d.update({k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', t.group(1))} if t else {})
        d.update({'untex_' + k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', u.group(1))} if u else {})
        kt = re.search(r' kt car=(\d+)', ln)
        d['ktcar'] = int(kt.group(1)) if kt else 0; d['fr'] = max(int(m.group(2)), 1); d['sc'] = m.group(1); rows.append(d)
    return rows


def pink_frac(img):
    im = img.convert('RGB').crop((150, 110, 330, 262)); n = pink = 0
    for r, g, b in im.get_flattened_data() if hasattr(im, 'get_flattened_data') else im.getdata():
        n += 1
        if r > 170 and 60 < g < 150 and b > 100 and r - g > 60: pink += 1
    return pink / n


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('out'); ap.add_argument('--expect-fail', default='')
    a = ap.parse_args(); R = os.path.join(a.out, 'resume'); N = os.path.join(a.out, 'new')
    res, why = {}, {}
    def put(k, ok, msg): res[k] = ok; why[k] = msg
    # а
    shots = [os.path.join(R, f'{n}.png') for n in ('fg_street_a', 'fg_street_b', 'fg_street_c', 'fg_street_d', 'fg_static_a', 'fg_static_b') if os.path.exists(os.path.join(R, f'{n}.png'))]
    pf = max((pink_frac(Image.open(s)) for s in shots), default=0.0)
    ride = gatecat(R, {'FG_street', 'FG_static'}) if os.path.exists(os.path.join(R, 'smoke.txt')) else []
    carpf = (sum(r['car'] / r['fr'] for r in ride) / len(ride)) if ride else None
    put('а', pf >= 0.015 and (carpf is None or carpf >= 8), f'розовых пикселей в зоне машины {100 * pf:.1f} % (порог 1,5), геометрия car3d ' + (f'{carpf:.0f} на кадр (порог 8)' if carpf is not None else 'нет счётчика'))
    # б в г д
    st = [r for r in ride if r['sc'] == 'FG_static' and r['cars'] >= 1]
    street = [r for r in ride if r['sc'] in ('FG_street', 'FG_static')]
    if not ride:
        for k in 'бвгд': put(k, False, 'в журнале нет GATECAT: EBOOT без счётчиков гейта — не доказано')
    else:
        per = [r['trf'] / r['fr'] / r['cars'] for r in st]
        put('б', len(st) >= 3 and sum(per) / len(per) >= 2.0, f'трафик: секунд с машинами {len(st)} (нужно 3), кусков на машину ' + (f'{sum(per) / len(per):.1f} (порог 2)' if per else 'нет'))
        pd = [r for r in ride if r['peds'] > 0]; sk = sum(r['skin'] for r in pd); bad = sum(r['skinbad'] for r in pd)
        # №1159 (TEST86: нуль прохожих в езде оказался ошибкой сна съехавших машин, а не случайностью): в FG_static служебный прохожий (flag pedtest) стоит в 3 м перед Гомером — в каждой из последних 3 секунд сцены peds >= 1
        stp = [r for r in ride if r['sc'] == 'FG_static']
        stok = len(stp) >= 3 and all(r['peds'] >= 1 for r in stp[-3:])
        put('в', sk > 0 and bad <= .05 * sk and stok, f'прохожие: секунд с ними {len(pd)}, пачек скелетных моделей {sk}, с битой текстурой {bad}; в FG_static в последних 3 с с прохожим ' + (f'{sum(1 for r in stp[-3:] if r["peds"] >= 1)} из 3' if stp else 'нет секунд'))
        pp = sum(r['props'] / r['fr'] for r in street) / len(street) if street else 0
        put('г', pp >= 5, f'предметы у дороги: {pp:.0f} кусков на кадр (порог 5)')
        cty = sum(r['city'] / r['fr'] for r in street) / len(street) if street else 0
        newr = gatecat(N, {'FG_new_start'}) if os.path.exists(os.path.join(N, 'smoke.txt')) else []
        # 19.10 (TEST37): после END пишутся строки кадра выхода (RFAST, DCITY, HOMER, EXIT739) — длиннее 400 знаков, END выпадал из окна хвоста; теперь — строка END в журнале
        newend = os.path.exists(os.path.join(N, 'smoke.txt')) and any(l.startswith('END') for l in open(os.path.join(N, 'smoke.txt'), errors='replace'))
        nc = sum(r['city'] for r in newr); npp = sum(r['props'] for r in newr)
        fps = []; prev_t = None
        for ln in open(os.path.join(R, 'smoke.txt'), errors='replace'):
            if ln.startswith('END'): break   # 17.10: после END — кадр выхода (сброс журнала на карту, ~340 мс), не игра; строка RFAST после END его считала (TEST31 integ: fps 22,3 на последней секунде) — как integ 52848db0
            if not ln.startswith('RFAST'): continue
            tm = re.search(r' t=([\d.]+)', ln); t = float(tm.group(1)) if tm else None
            # 09.11 (TEST102): неполное окно — строка RFAST через 0,3 с после предыдущей (смена сцены) печатала кадры окна как «в секунду»: 9 кадров за 0,3 с = fps=9.0 при work_ms 8; окна короче 0,9 с не мерят частоту
            short = t is not None and prev_t is not None and t - prev_t < 0.9
            if t is not None: prev_t = t
            if short or not re.search(r'sc=FG_(street|static)', ln): continue
            m = re.search(r'fps=([\d.]+)', ln)
            if m: fps.append(float(m.group(1)))
        fmin = min(fps) if fps else 0
        pm = pg = 0   # доля кусков drawPieceM, ушедших в drawGeo (строки BIGCUT, считают Стены; tests/lib/drawn.sh): >= 95 %
        for ln in open(os.path.join(R, 'smoke.txt'), errors='replace'):
            if ln.startswith('BIGCUT'):
                mp, mg = re.search(r' pm=(\d+)', ln), re.search(r' pmgeo=(\d+)', ln)
                if mp and mg: pm += int(mp.group(1)); pg += int(mg.group(1))
        share = pg / pm if pm else 1.0
        put('д', cty >= 10 and nc > 0 and npp > 0 and newend and fmin >= 24 and share >= .95, f'city {cty:.0f} кусков на кадр (10), NEW GAME: city {nc} props {npp} END {int(newend)}, кадров/с мин {fmin:.0f} (24), drawPieceM→drawGeo {pg}/{pm} = {100 * share:.0f} % (95)')
    # е
    bf, af = os.path.join(a.out, 'savedata_before.sha'), os.path.join(a.out, 'savedata_after.sha')
    put('е', os.path.exists(bf) and os.path.exists(af) and open(bf).read() == open(af).read() and bool(open(bf).read().strip()), 'SAVEDATA до и после ' + ('одинаковы' if os.path.exists(bf) and os.path.exists(af) and open(bf).read() == open(af).read() else 'РАЗНЫЕ или нет'))
    # лист кадров
    names = [os.path.join(R, 'fg_garage_b.png'), os.path.join(R, 'fg_seat.png'), os.path.join(R, 'fg_street_a.png'), os.path.join(R, 'fg_street_c.png'), os.path.join(R, 'fg_static_a.png'), os.path.join(N, 'fg_new_1.png')]
    sheet = Image.new('RGB', (480 * 3, 272 * 2))
    for i, n in enumerate(names):
        if os.path.exists(n): sheet.paste(Image.open(n).convert('RGB'), (480 * (i % 3), 272 * (i // 3)))
    sheet.save(os.path.join(a.out, 'frame_gate_sheet.png'))
    failed = [k for k in 'абвгде' if not res.get(k)]
    exp = [x for x in a.expect_fail.replace(' ', '').split(',') if x]
    if exp: ok = all(k in failed for k in exp) and not any(k in failed for k in 'абвгде' if k not in exp and k in 'аб') and True
    else: ok = not failed
    line = ('КАДР ИГРОКА (ОТРИЦАТЕЛЬНЫЙ КОНТРОЛЬ, ждали падение ' + ','.join(exp) + '; упали ' + ','.join(failed) + '): ' if exp else 'КАДР ИГРОКА: ') + ('PASS' if ok else 'FAIL') + ' · ' + ' · '.join(f'{k} {"ok" if res[k] else "FAIL"}' for k in 'абвгде') + f' · лист {a.out}/frame_gate_sheet.png'
    print(line)
    with open(os.path.join(a.out, 'frame_gate.txt'), 'w') as f:
        f.write(line + '\n' + '\n'.join(f'{k}: {"ok" if res[k] else "FAIL"} — {why[k]}' for k in 'абвгде') + '\n')
    print('\n'.join(f'  {k}: {"ok" if res[k] else "FAIL"} — {why[k]}' for k in 'абвгде'))
    return 0 if ok else 1


if __name__ == '__main__': sys.exit(main())
