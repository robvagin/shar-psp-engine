#!/usr/bin/env python3
"""Миссионер на 7 уровней (наряд «боты», ночь 04.11.2026; проект docs/handoff/БОТЫ_НОЧЬ_ПРОЕКТ_2026-11-04.md §7 п.1, 8, 9).
Все задания L1–L7 включая бонусы (bm1) и уличные гонки (sr1–3, gr1), по 5 вариантов на задание. Только процессор Мака, без AI и сети.
Бюджет задания по теории времени: игровых секунд = max(1500, 3 × теория + 120), потолок 1800; стена процесса (страховка от зависания) = бюджет × 1.5, не меньше 600 с.
Теория: missioner --theory (путь по дорогам / скорость машины игрока, только блоки с таймером). Задание без таймера: пол 1500 с.

  python3 tools/bots/missions7.py --levels 1-7 --variants 5 --procs 6 --nice 10            прогон (после слова Роба, ночью)
  python3 tools/bots/missions7.py --levels 2 --only m1,sr1 --variants 1 --procs 2 --nice 20   короткая проба
  python3 tools/bots/missions7.py --resume DIR                                              продолжить: готовые строки не пересчитываются
  python3 tools/bots/missions7.py --report DIR [--report-to ~/Downloads/SHAR_боты_отчёт.md]    пересобрать отчёт
  python3 tools/bots/missions7.py --controls [--levels 1-7]                                 красные контроли (числами в controls.json)

Результаты: build/bots/missions7/<время>/rows.jsonl (строка дописывается сразу после прогона), runs/<L>_<id>_v<k>/ (m*.in повтор, stdout),
MANIFEST.json (SHA-256 бинарника и входов), bin/ (замороженный бинарник), in/ (замороженные входы), missions.md, matrix.
Классы итога: пройдена / перенос (пройдена с переносом, разобрать) / игра / водитель / нет данных / намеренно / бот (сбой процесса).
Вариант k: 0 — базовый; 1..4 — seed + задержка старта MISSIONER_DELAY (0/3/7 с) + быстрый водитель в варианте 3 + гонка по полосам в варианте 4."""
import argparse, concurrent.futures as cf, datetime, hashlib, json, os, shutil, subprocess, sys, threading, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/bots'))
HOME = Path.home()
DELAY = (0, 0, 3, 7, 0)
CAP, FLOOR = 1800, 1500   # кампания шла по 1500 с на миссию; настоящая страховка от зависания — стена процесса
KIND = {0: 'сюжет'}   # по флагам: 1 уличная гонка (sr), 4 ставка (gr), 8 бонус; 128 — отдельный признак пакета


# ---------------------------------------------------------------------------------------------------- питание
# 05.11: ночь стала в 04:06 — Мак на батарее ушёл в Low Power Sleep при заряде 1 %. От батареи новые задания не берём и не стартуем; ждём сеть, раз в 10 мин строка в журнал
_pwr = dict(last=0.0, allow=False)


def on_battery():
    try:
        o = subprocess.run(['pmset', '-g', 'batt'], capture_output=True, text=True, timeout=20).stdout
    except Exception:
        return False
    return 'Battery Power' in o


def wait_for_power(out):
    """блокирует, пока Мак на батарее; True — пришлось ждать"""
    waited = False
    while not _pwr['allow'] and on_battery():
        waited = True
        if time.time() - _pwr['last'] > 600:
            _pwr['last'] = time.time()
            line = f"- {time.strftime('%Y-%m-%d %H:%M')} · bots · нет питания, жду сеть (pmset: Battery Power); новые варианты не берутся, готовые строки целы\n"
            try:
                with open(out / 'power.log', 'a') as f: f.write(line)
                with open(ROOT / 'docs/handoff/ПАРА_ЖУРНАЛ.md', 'a') as f: f.write(line)
            except OSError:
                pass
            print(line.strip(), flush=True)
        time.sleep(60)
    return waited


# ---------------------------------------------------------------------------------------------------- вход
def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()


def data_root(arg):
    import allrisk_mod
    return allrisk_mod.data_root(arg)


def load_allrisk():
    import importlib.util
    spec = importlib.util.spec_from_file_location('allrisk_mod', ROOT / 'tools/bots/allrisk.py')
    m = importlib.util.module_from_spec(spec); sys.modules['allrisk_mod'] = m; spec.loader.exec_module(m)
    return m


def levels_arg(s):
    out = []
    for part in s.split(','):
        if '-' in part:
            a, b = part.split('-'); out += list(range(int(a), int(b) + 1))
        elif part:
            out.append(int(part))
    return out


DEFAULT_SET = HOME / 'Downloads/shar-psp-integ/build/test89/PSP/GAME/SHARTEST15'   # набор, в который играет Роб (TEST89 40b5b3c7): входы ночи именно из него


def set_files(S, data, n):
    """раскладка набора TEST (уровень 1 в корне, остальные в lN/) -> тот же словарь, что allrisk.level_files; дороги — из data/sandbox"""
    S = Path(S)
    if n == 1:
        f = dict(level=S / 'level.shr', player=S / 'homer.shr', msn=S / 'level01.msn', traffic=S / 'traffic.shr', msncars=S / 'msncars-l1.shr',
                 carzone=S / 'carzone-l1.shr', collect=S / 'collect-l1.shr')
    else:
        L = S / f'l{n}'
        f = dict(level=L / 'level.shr', player=L / 'player.shr', msn=L / f'level0{n}.msn', traffic=L / 'traffic.shr', msncars=L / f'msncars-l{n}.shr',
                 carzone=L / f'carzone-l{n}.shr', collect=L / f'collect-l{n}.shr')
    f['roads'] = Path(data) / f'sandbox/roads-l{n}.bin'
    return f


def freeze_inputs(data, levels, dest, setdir=None):
    """Копия входов в dest/in (ночь не зависит от дневной пересборки); music.sap — только оглавление, как у night.py. -> {L: [level,player,roads,msn]}"""
    ar = load_allrisk(); import night
    man, paths = {}, {}
    for L in levels:
        f = set_files(setdir, data, L) if setdir else ar.level_files(data, L)
        d = dest / 'in' / f'L{L}'; d.mkdir(parents=True, exist_ok=True)
        got = {}
        for k, src in f.items():
            src = Path(src)
            if k == 'roads':
                tgt = dest / 'in/sandbox' / src.name; tgt.parent.mkdir(parents=True, exist_ok=True)
            else:
                tgt = d / src.name
            if src.exists():
                shutil.copy2(src, tgt); man[str(tgt.relative_to(dest))] = sha(tgt); got[k] = tgt
        for rel in (f'interiors-l{L}.bin', 'skins.txt', 'audio/music.sap', 'audio/sfx.sab'):   # skins.txt: гардероб для buyskin (world.c loadSkinSpots), без него этап без маркера
            src = Path(f['level']).parent / rel
            if not src.exists():
                continue
            tgt = d / rel; tgt.parent.mkdir(parents=True, exist_ok=True)
            night.copy_in(src, tgt, rel if rel == 'audio/music.sap' else rel)
            man[str(tgt.relative_to(dest))] = sha(tgt)
        # колоды имён и прочее рядом с уровнем (animobj и т.п.) читает bwData по имени: всё, что в папке уровня с суффиксом -l<N>
        for extra in Path(f['level']).parent.glob(f'*-l{L}*'):
            tgt = d / extra.name
            if extra.is_file() and not tgt.exists() and extra.suffix in ('.shr', '.bin', '.json', '.tsv'):
                if extra.stat().st_size < 50 << 20:
                    shutil.copy2(extra, tgt); man[str(tgt.relative_to(dest))] = sha(tgt)
        paths[L] = [str(got['level']), str(got['player']), str(got['roads']), str(got['msn'])]
    return paths, man


# ---------------------------------------------------------------------------------------------------- запуск миссионера
def env_for(level, variant, extra=None):
    e = dict(os.environ, BW_LEVEL=str(level), BW_FRAMES='1')
    for k in [k for k in e if k.startswith('MISSIONER_') and k not in ('MISSIONER_NO_SECOND', 'MISSIONER_NO_TELEPORT')]:
        del e[k]
    if variant:
        e['MISSIONER_SEED'] = str(1000 * level + variant)
        if DELAY[variant % 5]:
            e['MISSIONER_DELAY'] = str(DELAY[variant % 5])
        if variant == 3:
            e['MISSIONER_FAST'] = '1'
        if variant == 4:
            e['MISSIONER_RACE_LANES'] = '1'
    e.update(extra or {})
    return e


def mrun(binp, args, extra_args, env, nice, wall, cwd=None):
    t0 = time.time()
    try:
        r = subprocess.run(['nice', '-n', str(nice), str(binp), *args, *extra_args], capture_output=True, text=True, env=env, timeout=wall, cwd=cwd)
        return r.returncode, r.stdout, r.stderr[-400:], time.time() - t0, False
    except subprocess.TimeoutExpired as e:
        so = e.stdout.decode(errors='replace') if isinstance(e.stdout, bytes) else (e.stdout or '')
        return -9, so, 'стена', time.time() - t0, True


def catalog(binp, args, level, nice):
    rc, out, _, _, _ = mrun(binp, args, ['--catalog'], env_for(level, 0), nice, 300)
    miss, plan = {}, {}
    for ln in out.splitlines():
        f = ln.split('\t')
        if f[0] == 'CAT':
            miss[int(f[1])] = dict(id=f[2], flags=int(f[3]))
        elif f[0] == 'PLAN':
            plan.setdefault(int(f[1]), []).append((int(f[3]), int(f[4]), f[5]))
    return miss, plan


def theory(binp, args, level, nice):
    rc, out, _, _, _ = mrun(binp, args, ['--theory'], env_for(level, 0), nice, 300)
    th, blocks = {}, {}
    for ln in out.splitlines():
        f = ln.split('\t')
        if f[0] == 'T' and len(f) > 11:
            if float(f[9]) >= 0:
                th[int(f[1])] = th.get(int(f[1]), 0.0) + float(f[9])
            a, b = f[3].split('-')
            blocks.setdefault(int(f[1]), []).append(dict(a=int(a), b=int(b), limit=float(f[4]), kind=f[5], car=f[6], kmh=float(f[7]), length=float(f[8]), theory=float(f[9]), ratio=float(f[10]), note=f[12] if len(f) > 12 else '', straight=float(f[11])))
    return th, blocks


def budget(th):
    return int(min(CAP, max(FLOOR, 3 * th + 120)))


def parse(out):
    r = dict(M=None, trace=[], tps=[], waits=[], frames=0, mech={}, race=[], fails=[])
    for ln in out.splitlines():
        f = ln.split('\t')
        if f[0] == 'M' and len(f) > 15:
            r['M'] = dict(index=int(f[1]), id=f[2], title=f[3], result=f[4], why=f[5], time=float(f[6]), margin=float(f[7]), damage=float(f[8]), stages=int(f[9]),
                          attempts=int(f[10]), x=float(f[12]), z=float(f[13]), wall=int(f[14]), cls=f[15], nav=int(f[16]) if len(f) > 16 else 0,
                          second=int(f[17]) if len(f) > 17 else 0, tp=int(f[18]) if len(f) > 18 else 0, waitjump=int(f[19]) if len(f) > 19 else 0)
        elif f[0] == 'STAGE' and len(f) > 4:
            r['trace'].append([int(f[2]), int(f[3]), f[4]])
        elif f[0] == 'TP' and len(f) > 10:
            r['tps'].append(dict(frm=[float(f[2]), float(f[3]), float(f[4])], to=[float(f[5]), float(f[6]), float(f[7])], foot=int(f[8]), t=float(f[9]), stage=f[10]))
        elif f[0] == 'MECH' and len(f) > 2:
            if f[1] == 'sum':
                for c in f[3:]:
                    k, n, x, y, z = c.split(':'); r['mech'][k] = dict(n=int(n), x=float(x), y=float(y), z=float(z))
            elif f[1] == 'race':
                r['race'].append(dict(t=int(f[2]), lap=f[3], item=f[4], rivals=[dict(zip(('name', 'wp', 'fin'), c.split(':'))) for c in f[5:]]))
        elif f[0] == 'FAIL' and len(f) > 12:
            r['fails'].append(dict(stage=f[2], obj=f[3], reason=f[4], left=float(f[5]), damage=float(f[6]), lap=f[7], point=f[8], frac=float(f[9]), x=float(f[10]), z=float(f[11])))
        elif f[0] == 'WAIT':
            r['waits'].append(f[2:])
        elif f[0] == 'F' and len(f) > 1:
            r['frames'] += int(f[1])
    return r


def code_of(x, z):
    try:
        import place
        return place.code(x, z)
    except Exception:
        return f'{x:.0f},{z:.0f}'


def verdict(row):
    """итог строки -> (итог, класс)"""
    if row['rc'] not in (0, -9) and not row.get('M'):
        return 'сбой', 'бот'
    if row['wallkill']:
        return 'не измерено', 'нет данных (стена процесса)'
    m = row.get('M')
    if not m:
        return 'сбой', 'бот'
    if m['result'] == 'пройдена с переносом':
        return 'пройдена с переносом', 'перенос'
    if m['result'] == 'пройдена':
        return 'пройдена', 'пройдена'
    cls = m['cls']
    if m['why'] == 'время вышло':
        return 'время вышло', 'нет данных (бюджет)'
    if m['result'] == 'провал':
        return 'провал', 'намеренно' if cls == 'намеренно' else cls
    return m['result'], cls


# ---------------------------------------------------------------------------------------------------- прогон
class Run:
    def __init__(self, a):
        self.a = a; self.lock = threading.Lock()

    def build(self, out):
        bindir = out / 'bin'; bindir.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ, BINS='missioner', OUT_DIR=str(bindir), DEVELOPER_DIR=os.environ.get('DEVELOPER_DIR', '/Library/Developer/CommandLineTools'))
        r = subprocess.run(['nice', '-n', str(self.a.nice), str(ROOT / 'tools/bots/build.sh')], cwd=ROOT, env=env, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit('сборка миссионера: ' + r.stderr[-500:])
        return bindir / 'missioner'

    def jobs(self, out, binp, paths, levels):
        jobs, plans, names = [], {}, {}
        for L in levels:
            miss, plan = catalog(binp, paths[L], L, self.a.nice)
            th, blocks = theory(binp, paths[L], L, self.a.nice)
            for idx, m in miss.items():
                if self.a.only and m['id'] not in self.a.only.split(','):
                    continue
                plans[(L, idx)] = dict(id=m['id'], flags=m['flags'], plan=plan.get(idx, []), theory=th.get(idx, 0.0), blocks=blocks.get(idx, []))
                for v in range(self.a.variants):
                    jobs.append((L, idx, v))
        return jobs, plans

    def one(self, binp, paths, out, plans, job, done):
        L, idx, v = job
        pl = plans[(L, idx)]; B = int(self.a.secs) if self.a.secs else budget(pl['theory']); wall = max(600, B * self.a.wall_factor)
        key = f'L{L}_{pl["id"]}_v{v}'
        if key in done:
            return None
        while True:   # не мешать остальному: load не выше предела
            if self.a.until and time.time() > self.a.until:
                return None
            if os.getloadavg()[0] <= self.a.max_load:
                break
            time.sleep(30)
        d = out / 'runs' / key; d.mkdir(parents=True, exist_ok=True)
        extra = ['--mission', str(idx), '--secs', str(B), '--out', str(d)]
        if self.a.impossible_timer:
            extra.append('--impossible-timer')
        if self.a.broken_goal:
            extra.append('--broken-goal')
        env = env_for(L, v, self.a.env)
        rc, so, se, wt, wk = mrun(binp, paths[L], extra, env, self.a.nice, wall)
        (d / 'stdout.txt').write_text(so)
        p = parse(so)
        row = dict(key=key, level=L, index=idx, id=pl['id'], flags=pl['flags'], variant=v, budget=B, wall=wall, rc=rc, wallkill=wk, wallclock=round(wt, 1), stderr=se,
                   env={k: val for k, val in env.items() if k.startswith(('MISSIONER_', 'BW_LEVEL'))}, M=p['M'], trace=p['trace'], tps=p['tps'], waits=p['waits'],
                   frames=p['frames'], mech=p['mech'], race=p['race'][-80:], fails=p['fails'][-30:], replay=f'{binp} {" ".join(paths[L])} {" ".join(extra)}', theory=round(pl['theory'], 1))
        row['result'], row['cls'] = verdict(row)
        with self.lock:
            with open(out / 'rows.jsonl', 'a') as f:
                f.write(json.dumps(row, ensure_ascii=False) + '\n')
        print(f'{key}: {row["result"]} ({row["cls"]}) {p["M"]["time"] if p["M"] else "-"} с игры, {wt:.0f} с стены', flush=True)
        return row


def load_rows(out):
    f = out / 'rows.jsonl'
    return [json.loads(l) for l in f.read_text().splitlines() if l.strip()] if f.exists() else []


# ---------------------------------------------------------------------------------------------------- провалы по правилу миссии
def best_frac(rows, L, idx):
    fr = [f['frac'] for r in rows if (r['level'], r['index']) == (L, idx) for f in r.get('fails', []) if f['frac'] >= 0]
    return max(fr) if fr else -1.0


def rule_fails(rows, plans):
    """задание не пройдено, итог «провал по правилу миссии» (таймер, урон): лимит таймера стадии из скрипта, теория (путь по дорогам / скорость машины игрока), доля пути,
    причина. «Подозрение на игру»: теория >= 0,8 × лимита (идеальный водитель почти не успевает) или урон выделяется. Остальное: слабость бота с причиной"""
    by = {}
    for r in rows:
        by.setdefault((r['level'], r['index']), []).append(r)
    items = []
    for (lv, idx), rs in sorted(by.items()):
        if any(r['result'].startswith('пройдена') for r in rs):
            continue
        rule = [r for r in rs if r.get('M') and r['M']['result'] == 'провал']
        if not rule:
            continue
        pl = plans.get((lv, idx), {}); r0 = rule[0]
        fl = [f for r in rule for f in r.get('fails', [])]
        last = r0['trace'][-1] if r0['trace'] else [0, 0, '?']
        stage = fl[-1]['stage'] if fl else f'{last[0]}.{last[1]}'
        k = int(stage.split('.')[1]) if '.' in stage else 0
        blk = next((b for b in pl.get('blocks', []) if b['a'] <= k <= b['b']), None)
        reasons = {}
        for f in fl:
            reasons[f['reason']] = reasons.get(f['reason'], 0) + 1
        frac = best_frac(rows, lv, idx)
        dmg = max([f['damage'] for f in fl] or [r['M']['damage'] for r in rule])
        items.append(dict(lv=lv, id=pl.get('id', r0['id']), title=r0['M']['title'], stage=stage, obj=(fl[-1]['obj'] if fl else last[2]), blk=blk, reasons=reasons, frac=frac, dmg=dmg, n=len(rule), left=min([f['left'] for f in fl] or [r0['M']['margin']])))
    dm = sorted(i['dmg'] for i in items); med = dm[len(dm) // 2] if dm else 0
    L = ['', '## Провалы по правилу миссии: игра или слабость бота', '',
         'Задания, где игра провалила бота правилом миссии (таймер, урон) после трёх попыток стадии. Лимит и теория: из скрипта миссии и данных (путь по графу дорог / скорость машины игрока, '
         'нижняя граница: разгон мгновенный, повороты без торможения). «Подозрение на игру» означает теорию ≥ 0,8 × лимита (даже идеальный водитель почти не успевает) или урон заметно выше '
         f'остальных (медиана {med:.2f}). Доля пути: 1 − лучшее расстояние до цели / стартовое, по журналу FAIL (строки с 02:50 05.11; раньше «нет»). Остальное: слабость бота.', '']
    sus, weak = [], []
    for i in items:
        b = i['blk']
        if b and i['obj'] == 'race' and b.get('straight', 0) > 0 and b['kmh'] > 0:   # гонка: путь по графу дорог в 1,4–1,9 раза длиннее линии по точкам гонки (g11 05.11): теория по прямой между точками, нижняя граница
            b = dict(b, theory=b['straight'] / (b['kmh'] / 3.6)); b['ratio'] = b['theory'] / max(b['limit'], 1.0); b['car'] = b['car'] + ', по точкам гонки'
        ratio = b['ratio'] if b and b['theory'] >= 0 else None
        susp = (ratio is not None and ratio >= 0.8) or (i['dmg'] >= max(0.5, 1.5 * med) and 'damage' in ''.join(i['reasons']).lower())
        why = []
        if ratio is not None and ratio >= 0.8: why.append(f'теория {b["theory"]:.0f} с из лимита {b["limit"]:.0f} с')
        if susp and not why: why.append(f'урон {i["dmg"]:.2f}')
        row = (f"| L{i['lv']} | {i['id']} {i['title']} | {i['stage']} {i['obj']} | {b['limit']:.0f} | {b['theory']:.0f} ({b['car']}, {b['kmh']:.0f} км/ч) | {ratio:.2f} | " if b and b['theory'] >= 0 else
               f"| L{i['lv']} | {i['id']} {i['title']} | {i['stage']} {i['obj']} | {b['limit']:.0f} | цель едет, теория не считается | | " if b else
               f"| L{i['lv']} | {i['id']} {i['title']} | {i['stage']} {i['obj']} | нет блока с таймером | | | ")
        row += f"{', '.join(f'{k} ×{v}' for k, v in i['reasons'].items()) or 'нет журнала FAIL'} | {i['dmg']:.2f} | {('%.0f %%' % (100 * i['frac'])) if i['frac'] >= 0 else 'нет'} | {i['n']} | {'; '.join(why)} |"
        (sus if susp else weak).append(row)
    hdr = ['| Уровень | Задание | Стадия | Лимит таймера, с | Теория | Теория / лимит | Причины провалов | Урон | Лучшая доля пути | Прогонов | Почему |', '|---|---|---|---|---|---|---|---|---|---|---|']
    L += ['### Подозрение на игру', ''] + hdr + (sus or ['| нет | | | | | | | | | | |']) + ['', '### Слабость бота', ''] + hdr + (weak or ['| нет | | | | | | | | | | |'])
    return L


# ---------------------------------------------------------------------------------------------------- механики
def mechanics(rows, matrix, plans):
    """раздел «Механики» (слово Роба №1205): из журналов тех же прогонов; работает / не работает / не проверено (почему) и код места примера"""
    L = ['', '## Механики', '', 'Из журналов рантайма тех же прогонов (строки MECH миссионера, с 02:20 05.11; более ранние строки журнала механик не несут). «Работает» значит, что событие мира '
         'наблюдалось (счётчик вырос), «не проверено» значит, что бот не создал условий или счётчика в журнале нет.', '']
    with_m = [r for r in rows if r.get('mech')]
    L.append(f'Строк с журналом механик: {len(with_m)} из {len(rows)}.'); L.append('')
    lv = sorted({r['level'] for r in rows})
    names = [('coins', 'Сбор монет при наезде (счётчик монет растёт)'), ('kicks', 'Удар по предметам (props: сбиваются)'), ('sp', 'Разбиваемое (стейтпропы ломаются, состояние меняется)'),
             ('rooms', 'Двери: вход в комнату'), ('traffic', 'Удары по трафику и пешеходам (события трафика)')]
    L += ['| Механика | ' + ' | '.join(f'L{l}' for l in lv) + ' |', '|---|' + '---|' * len(lv)]
    for key, title in names:
        cells = []
        for l in lv:
            rs = [r for r in with_m if r['level'] == l and r['mech'].get(key, {}).get('n', 0) > 0]
            if rs:
                m = max(rs, key=lambda r: r['mech'][key]['n'])['mech'][key]
                cells.append(f"работает ({m['n']}, {code_of(m['x'], m['z'])})")
            elif any(r['level'] == l for r in with_m):
                cells.append('не наблюдалось')
            else:
                cells.append('не проверено (нет журнала)')
        L.append(f'| {title} | ' + ' | '.join(cells) + ' |')
    # стадии: покупки, двери, подъёмники, ИИ-машины миссий
    L += ['', '| Механика по стадиям | ' + ' | '.join(f'L{l}' for l in lv) + ' |', '|---|' + '---|' * len(lv)]
    for title, kinds in (('Покупка машины/костюма', ('buycar', 'buyskin')), ('Вход и выход (interior, gooutside)', ('interior', 'gooutside')),
                         ('ИИ-машины миссий: погоня, таран, слежка', ('losetail', 'destroy', 'dump', 'follow')), ('Доставка и сбор (delivery, pickupitem)', ('delivery', 'pickupitem'))):
        cells = []
        for l in lv:
            tot = sum(matrix.get((k, l), [0, 0, 0])[2] for k in kinds); ok = sum(matrix.get((k, l), [0, 0, 0])[0] for k in kinds); tp = sum(matrix.get((k, l), [0, 0, 0])[1] for k in kinds)
            cells.append('нет в уровне' if not tot else f'{ok} из {tot}' + (f' (+{tp} с переносом)' if tp else '') + (' работает' if ok else ' не пройдено'))
        L.append(f'| {title} | ' + ' | '.join(cells) + ' |')
    # гонки
    L += ['', '### Гонки: ход соперников и место игрока', '', '| Уровень | Гонка | Соперники | Меняется ли порядок | Соперник доехал | Итог игрока | Пример (повтор) |', '|---|---|---|---|---|---|---|']
    seen = set()
    for r in sorted(rows, key=lambda r: (r['level'], r['index'], r['variant'])):
        if not r.get('race') or (r['flags'] & 15) == 0 or (r['level'], r['id']) in seen:
            continue
        ser = r['race']; names_ = sorted({c['name'] for s_ in ser for c in s_['rivals']})
        if not names_:
            L.append(f"| L{r['level']} | {r['id']} | нет (заезд на время) | не применимо | не применимо | {r['result']} | {r['key']} |"); seen.add((r['level'], r['id'])); continue
        def prog(s_):
            a, b = s_['lap'].split('/'); k, n = s_['item'].split('/'); return int(a) * int(n) + int(k)
        lead = []
        for s_ in ser:
            me = prog(s_); best = max(int(c['wp']) for c in s_['rivals']) if s_['rivals'] else 0
            lead.append(1 if me > best else -1 if me < best else 0)
        changes = sum(1 for a, b in zip(lead, lead[1:]) if a and b and a != b)
        moved = any(int(c['wp']) > 0 for s_ in ser for c in s_['rivals']); fin = [c['name'] for s_ in ser for c in s_['rivals'] if c['fin'] != '0']
        L.append(f"| L{r['level']} | {r['id']} | {', '.join(names_)}; движутся: {'да' if moved else 'нет'} | {'да, смен лидера ' + str(changes) if changes else 'нет (лидер один)'} | {'да: ' + fin[0] if fin else 'нет'} | {r['result']} | {r['key']} |")
        seen.add((r['level'], r['id']))
    L += ['', 'Место игрока на финише и показ результата гонки в этих журналах не записываются: у рантайма миссий нет поля места, решает условие race. Требует кадра интерфейса (эмулятор), не проверено ботом.']
    L += ['', 'Не проверено ботом в этих прогонах: карточки, осы, гэги (нужен allrisk --mode progress), подъёмники ANM как механика (только как ступень лестницы), боссы L7 (пункт 7 плана).']
    return L


# ---------------------------------------------------------------------------------------------------- отчёт
def report(out, report_to=None):
    rows = load_rows(out)
    meta = json.loads((out / 'META.json').read_text()) if (out / 'META.json').exists() else {}
    plans = {tuple(map(int, k.split(':'))): v for k, v in meta.get('plans', {}).items()}
    by = {}
    for r in rows:
        by.setdefault((r['level'], r['index']), []).append(r)
    L = [f'# Боты: задания от А до Я, {meta.get("stamp", out.name)}', '',
         'Только процессор Мака, без AI и сети. Итог строки: пройдена / перенос (пройдена с переносом, разобрать) / игра / водитель / нет данных / намеренно / бот (сбой процесса). '
         '«Игра» означает, что два независимых водителя не доехали, это подозрение, а не приёмка. «Перенос» означает, что бот не проехал и его поставили на узел дорог '
         '(код места в списке переносов), задание проверено дальше, но этот кусок не пройден честно. «Намеренно» значит, что игра провалила задание правилом миссии (таймер, урон) после трёх попыток; '
         'это не всегда дефект игры: бот на таймере и в гонках медленнее человека, такие строки читать с цифрами «запас таймера» и «круг/точка» в stdout.txt прогона.', '']
    if meta:
        L += [f'Входы из набора: {meta.get("set_note", "?")} ({meta.get("set", "?")}).', f'Вход заморожен: бинарник {meta.get("binary_sha", "?")[:12]}, входов {meta.get("n_inputs", "?")}, коммит {meta.get("commit", "?")[:8]}, варианты на задание {meta.get("variants", "?")}, '
              f'бюджет по теории времени: max({FLOOR}, 3 × теория + 120) игровых секунд, потолок {CAP}.', '']
    uw = json.loads((out / 'useless.json').read_text()) if (out / 'useless.json').exists() else {}
    total = {'пройдена': 0, 'перенос': 0, 'игра': 0, 'водитель': 0, 'намеренно': 0, 'нет данных': 0, 'бот': 0}
    tbl, transfers, gameLines = [], [], []
    matrix = {}   # (тип, уровень) -> [чисто, перенос, всего]
    best = {}
    for (lv, idx), rs in sorted(by.items()):
        pl = plans.get((lv, idx), {})
        npass = sum(1 for r in rs if r['result'] == 'пройдена')
        ntp = sum(1 for r in rs if r['result'] == 'пройдена с переносом')
        title = next((r['M']['title'] for r in rs if r['M']), '')
        mid = rs[0]['id']
        classes = sorted({r['cls'] for r in rs if r['result'] not in ('пройдена',)})
        if npass:
            final = 'пройдена'
        elif ntp:
            final = 'перенос'
        else:
            pri = ['игра', 'водитель', 'намеренно', 'бот']
            cs = [r['cls'] for r in rs]
            final = next((c for c in pri if c in cs), 'нет данных')
        total[final if final in total else 'нет данных'] += 1
        bestr = next((r for r in rs if r['result'] == 'пройдена'), None) or next((r for r in rs if r['result'] == 'пройдена с переносом'), None) or rs[0]
        why = next((r['M']['why'] for r in rs if r['M'] and r['result'] not in ('пройдена',)), '') if final != 'пройдена' else ''
        place = ''
        failing = [r for r in rs if r['M'] and r['result'] not in ('пройдена',)]
        if failing and failing[0]['M']['result'] in ('застрял', 'время вышло', 'провал'):
            place = code_of(failing[0]['M']['x'], failing[0]['M']['z'])
        tm = bestr['M']['time'] if bestr['M'] else 0
        if final != 'пройдена' and uw.get(f'L{lv}:{mid}'): why = (why + '; ' if why else '') + 'повтор бесполезен: ' + uw[f'L{lv}:{mid}']
        tbl.append(f'| L{lv} | {mid} | {title} | {final} | {npass}/{len(rs)}{f" (+{ntp} с переносом)" if ntp else ""} | {tm:.0f} | {bestr["budget"]} | {place} | {why} |')
        for r in rs:
            for t in r['tps']:
                transfers.append(f'| L{lv} {mid} v{r["variant"]} | {code_of(t["frm"][0], t["frm"][2])} | {code_of(t["to"][0], t["to"][2])} | {"пешком" if t["foot"] else "машина"} | {t["stage"]} | {t["t"]:.0f} | {r["key"]} |')
        for r in rs:
            if r['result'] in ('застрял',) and r['cls'] == 'игра':
                gameLines.append(f'- игра: L{lv} {mid} v{r["variant"]}: {r["M"]["why"] if r["M"] else ""}; место {code_of(r["M"]["x"], r["M"]["z"]) if r["M"] else "?"}; повтор {r["key"]}')
        # матрица: стадии по типам. Стадия пройдена, если вход в неё сменился следующей (или миссия пройдена)
        plan = pl.get('plan', [])
        order = [(ph, k) for (ph, k, _) in plan]
        typ = {(ph, k): o for (ph, k, o) in plan}
        for (ph, k) in order:
            cell = matrix.setdefault((typ[(ph, k)], lv), [0, 0, 0]); cell[2] += 1
        done_clean, done_tp = set(), set()
        for r in rs:
            seq = [(t[0], t[1]) for t in r['trace']]
            last = seq[-1] if seq else None
            passed = set(order) if r['result'] in ('пройдена', 'пройдена с переносом') else {s for s in order if last and s in seq and order.index(s) < order.index(last)} if last in order else set()
            clean = r['M'] and r['M']['tp'] == 0 and not r['M']['waitjump']
            (done_clean if clean else done_tp).update(passed)
        for s in order:
            cell = matrix[(typ[s], lv)]
            if s in done_clean:
                cell[0] += 1
            elif s in done_tp:
                cell[1] += 1
    L += ['## Итоги', '', 'Заданий: %d. ' % len(by) + ', '.join(f'{k}: {v}' for k, v in total.items() if v), '',
          '| Уровень | Задание | Название | Итог | Вариантов пройдено | Время лучшего, с игры | Бюджет, с игры | Место | Почему не пройдено |', '|---|---|---|---|---|---|---|---|---|'] + tbl + ['']
    if gameLines:
        L += ['## Игра: подозрения для окон', ''] + gameLines + ['']
    if transfers:
        L += ['## Переносы (код места, откуда застрял и куда перенесён)', '', '| Задание | Застрял | Перенесён | Кто | Стадия | Время, с | Повтор |', '|---|---|---|---|---|---|---|'] + transfers + ['']
    L += ['## Матрица типов целей × уровни (стадий пройдено чисто / с переносом / всего, лучший вариант)', '']
    levels = sorted({k[1] for k in matrix})
    types = sorted({k[0] for k in matrix})
    L += ['| Тип цели | ' + ' | '.join(f'L{l}' for l in levels) + ' | Всего |', '|---|' + '---|' * (len(levels) + 1)]
    for t in types:
        cells, tt = [], [0, 0, 0]
        for l in levels:
            c = matrix.get((t, l))
            if c:
                cells.append(f'{c[0]}/{c[1]}/{c[2]}'); tt = [tt[i] + c[i] for i in range(3)]
            else:
                cells.append('')
        L.append(f'| {t} | ' + ' | '.join(cells) + f' | {tt[0]}/{tt[1]}/{tt[2]} |')
    L += rule_fails(rows, plans)
    L += mechanics(rows, matrix, plans)
    L += ['', f'Типов целей в матрице: {len(types)}. Строки прогона: {len(rows)}. Сырьё: {out}']
    txt = '\n'.join(L) + '\n'
    (out / 'missions.md').write_text(txt)
    (out / 'matrix.json').write_text(json.dumps({f'{k[0]}|L{k[1]}': v for k, v in matrix.items()}, ensure_ascii=False, indent=1))
    if report_to:
        Path(report_to).expanduser().write_text(txt)
    return txt


# ---------------------------------------------------------------------------------------------------- красные контроли
def controls(a):
    ar = load_allrisk()
    data = ar.data_root(a.data)
    out = Path(a.out) if a.out else ROOT / 'build/bots/missions7' / (time.strftime('%Y%m%d-%H%M%S') + '-controls')
    out.mkdir(parents=True, exist_ok=True)
    levels = levels_arg(a.levels)
    R = Run(a); binp = R.build(out); paths, man = freeze_inputs(data, levels, out, a.set)
    res = dict(stamp=out.name, binary_sha=sha(binp))
    # 1) невозможный таймер: каждое задание с таймером обязано дать «провал, намеренно»
    tm = []
    for L in levels:
        miss, plan = catalog(binp, paths[L], L, a.nice); rc, o, _, _, _ = mrun(binp, paths[L], ['--theory'], env_for(L, 0), a.nice, 300)
        timed = sorted({int(l.split('\t')[1]) for l in o.splitlines() if l.startswith('T\t') and l.split('\t')[5] == 'таймер'})   # «ставка» (gr1) считает время вверх: невозможный таймер на неё не действует
        for idx in timed:
            tm.append((L, idx, miss[idx]['id']))
    def timer_job(j):
        L, idx, mid = j
        rc, so, se, wt, wk = mrun(binp, paths[L], ['--mission', str(idx), '--secs', '300', '--impossible-timer'], env_for(L, 0), a.nice, 300)
        p = parse(so)['M']
        return dict(level=L, id=mid, result=p['result'] if p else 'нет строки', cls=p['cls'] if p else '', why=p['why'] if p else '', wall=round(wt), killed=wk)
    with cf.ThreadPoolExecutor(a.procs) as ex:
        res['impossible_timer'] = list(ex.map(timer_job, tm))
    okT = [r for r in res['impossible_timer'] if r['result'] == 'провал' and r['cls'] == 'намеренно']
    bad = [f"L{r['level']} {r['id']} ({r['result']}: {r['why']})" for r in res['impossible_timer'] if r not in okT]
    res['impossible_timer_summary'] = f'{len(okT)} из {len(res["impossible_timer"])} заданий с таймером дали «провал, намеренно»' + (('; не дошли до стадии с таймером: ' + ', '.join(bad)) if bad else '')
    # 2) сломанная цель: первое сюжетное задание каждого уровня с целью на 60 м над маркером. Ждём «застрял» или «время вышло», не «пройдена» и не зависание
    def broken_job(L):
        miss, plan = catalog(binp, paths[L], L, a.nice)
        idx = min(i for i, m in miss.items() if not (m['flags'] & 15))
        rc, so, se, wt, wk = mrun(binp, paths[L], ['--mission', str(idx), '--secs', '240', '--broken-goal'], env_for(L, 0), a.nice, 400)
        p = parse(so)['M']
        return dict(level=L, id=miss[idx]['id'], result=p['result'] if p else 'нет строки', cls=p['cls'] if p else '', why=p['why'] if p else '', wall=round(wt), killed=wk, tp=p['tp'] if p else -1)
    with cf.ThreadPoolExecutor(a.procs) as ex:
        res['broken_goal'] = list(ex.map(broken_job, levels))
    okB = [r for r in res['broken_goal'] if r['result'] in ('застрял', 'время вышло', 'провал') and not r['killed']]
    res['broken_goal_summary'] = f'{len(okB)} из {len(res["broken_goal"])} уровней: цель-призрак не «пройдена» и без вечного ожидания'
    # 3) MSN_LOOSE_ARRIVE (наряд): приезд эволюции отрезка без проверки высоты. Цель-призрак на 60 м над маркером: рабочий бинарник — класс «игра» (или без класса «застрял»),
    #    сборка -DMSN_LOOSE_ARRIVE=1 — ложный приезд, класс «водитель». Один уровень (первый из --levels), числа пары в controls.json
    lv0 = levels[0]
    env = dict(os.environ, BINS='missioner', OUT_DIR=str(out / 'bin_loose'), SUF='_loose', DEVELOPER_DIR=os.environ.get('DEVELOPER_DIR', '/Library/Developer/CommandLineTools'))
    subprocess.run(['nice', '-n', str(a.nice), str(ROOT / 'tools/bots/build.sh'), '-DMSN_LOOSE_ARRIVE=1'], cwd=ROOT, env=env, capture_output=True, text=True, check=True)
    loose = out / 'bin_loose/missioner_loose'
    miss0, _ = catalog(binp, paths[lv0], lv0, a.nice); idx0 = min(i for i, m in miss0.items() if not (m['flags'] & 15))
    pair = {}
    for name, bp in (('рабочий', binp), ('loose', loose)):
        rc, so, se, wt, wk = mrun(bp, paths[lv0], ['--mission', str(idx0), '--secs', '240', '--broken-goal'], env_for(lv0, 0), a.nice, 600)
        p = parse(so)['M']; pair[name] = dict(result=p['result'] if p else 'нет строки', cls=p['cls'] if p else '', why=p['why'] if p else '', wall=round(wt), killed=wk)
    res['loose_arrive'] = dict(level=lv0, mission=miss0[idx0]['id'], **pair)
    res['loose_arrive_summary'] = f"L{lv0} {miss0[idx0]['id']} цель-призрак: рабочий «{pair['рабочий']['result']}/{pair['рабочий']['cls']}», LOOSE «{pair['loose']['result']}/{pair['loose']['cls']}»"
    (out / 'controls.json').write_text(json.dumps(res, ensure_ascii=False, indent=1))
    print(res['loose_arrive_summary'])
    print(res['impossible_timer_summary']); print(res['broken_goal_summary']); print('->', out / 'controls.json')
    return 0 if len(okT) == len(res['impossible_timer']) and len(okB) == len(res['broken_goal']) else 1


# ---------------------------------------------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--levels', default='1-7'); ap.add_argument('--variants', type=int, default=5); ap.add_argument('--procs', type=int, default=6)
    ap.add_argument('--nice', type=int, default=10); ap.add_argument('--first', type=int, default=2, help='сперва ширина: столько вариантов каждому заданию всех уровней, потом по кругу добавлять не прошедшим'); ap.add_argument('--wall-factor', type=float, default=1.5); ap.add_argument('--extra-variants', type=int, default=0, help='не прошедшим чисто: ещё столько вариантов'); ap.add_argument('--only'); ap.add_argument('--data'); ap.add_argument('--set', type=Path, default=DEFAULT_SET, help='набор TEST с входами (по умолчанию TEST89 SHARTEST15); --set "" — build/ основного дерева'); ap.add_argument('--set-note', default='набор TEST89 40b5b3c7'); ap.add_argument('--out'); ap.add_argument('--resume')
    ap.add_argument('--report'); ap.add_argument('--report-to'); ap.add_argument('--until', help='HH:MM, новые задания после срока не стартуют')
    ap.add_argument('--secs', type=float, help='игровые секунды на задание вместо бюджета по теории (проба)')
    ap.add_argument('--max-load', type=float, default=float(os.cpu_count() or 8), help='новое задание ждёт, пока load average выше этого')
    ap.add_argument('--allow-battery', action='store_true', help='пробы: не ждать сеть'); ap.add_argument('--impossible-timer', action='store_true'); ap.add_argument('--broken-goal', action='store_true'); ap.add_argument('--controls', action='store_true')
    ap.add_argument('--env', type=json.loads, default={}, help='доп. окружение миссионера JSON, например {"MISSIONER_NO_TELEPORT":"1"}')
    a = ap.parse_args()
    if str(a.set) in ('', '.'): a.set = None
    if a.report:
        print(report(Path(a.report), a.report_to)); return 0
    if a.controls:
        return controls(a)
    if a.until:
        h, m = map(int, a.until.split(':')); n = datetime.datetime.now(); t = n.replace(hour=h, minute=m, second=0, microsecond=0)
        a.until = (t if t > n else t + datetime.timedelta(days=1)).timestamp()
    _pwr['allow'] = a.allow_battery
    ar = load_allrisk(); data = ar.data_root(a.data)
    levels = levels_arg(a.levels)
    # 06.11: входы и SDK проверяются ДО ожидания сети — ночь 06.11 ждала сеть до 11:54 и упала на KeyError 'roads' (запуск без git в PATH: data_root не нашёл build/)
    miss = [f'L{L} {k}: {p}' for L in levels for k, p in (set_files(a.set, data, L) if a.set else ar.level_files(data, L)).items() if k in ('level', 'player', 'roads', 'msn') and not Path(p).exists()]
    if miss: print('входы не найдены (задай --data или запусти с git в PATH):\n  ' + '\n  '.join(miss[:12]), flush=True); return 2
    if not os.environ.get('SDKROOT') and Path('/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk').exists(): os.environ['SDKROOT'] = '/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk'   # сборка миссионера
    if not a.allow_battery: wait_for_power(Path(a.resume or a.out or ROOT / 'build/bots/missions7'))
    out = Path(a.resume or a.out or ROOT / 'build/bots/missions7' / time.strftime('%Y%m%d-%H%M%S')); out.mkdir(parents=True, exist_ok=True)
    R = Run(a)
    if a.resume and (out / 'bin/missioner').exists() and (out / 'META.json').exists():
        binp = out / 'bin/missioner'; paths = {int(k): v for k, v in json.loads((out / 'META.json').read_text())['paths'].items()}   # те же замороженные входы
    else:
        binp = R.build(out); paths, man = freeze_inputs(data, levels, out, a.set)
        (out / 'MANIFEST.json').write_text(json.dumps(dict(binary={str(binp): sha(binp)}, set=str(a.set) if a.set else 'build/ основного дерева (НЕ набор Роба)', set_note=a.set_note, inputs=man), indent=1))
    jobs, plans = R.jobs(out, binp, paths, levels)
    meta = dict(stamp=out.name, set=str(a.set) if a.set else 'build/ основного дерева', set_note=a.set_note, commit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), binary_sha=sha(binp),
                n_inputs=len(json.loads((out / 'MANIFEST.json').read_text())['inputs']) if (out / 'MANIFEST.json').exists() else 0,
                variants=a.variants, levels=levels, paths={str(k): v for k, v in paths.items()}, plans={f'{k[0]}:{k[1]}': v for k, v in plans.items()},
                started=time.strftime('%Y-%m-%d %H:%M'), procs=a.procs, nice=a.nice)
    (out / 'META.json').write_text(json.dumps(meta, ensure_ascii=False, indent=1))
    done = {r['key'] for r in load_rows(out)}
    print(f'заданий {len(plans)}, прогонов {len(jobs)}, готово {len(done)}, процессов {a.procs}, nice {a.nice}, выход {out}', flush=True)
    n = [0]
    def work(j):
        r = R.one(binp, paths, out, plans, j, done)
        n[0] += 1
        if r and n[0] % 10 == 0:
            report(out, a.report_to)
        return r
    # Планировщик без барьера (решение дирижёра 05.11, час 4): процесс освободился — сразу следующий прогон. Сперва ширина: все задания всех уровней по --first вариантов
    # (v-major), затем задания без чистого прохождения и без «повтор бесполезен» — те, у кого меньше вариантов, и среди них ближние к цели (доля пути) первыми,
    # до variants + extra_variants. Пока есть работающие прогоны, ждёт: их итог может открыть новый вариант.
    order = sorted(plans)
    inflight = set(); slock = threading.Lock(); tot = a.variants + a.extra_variants
    def useless(L, idx):
        rs = [r for r in load_rows(out) if (r['level'], r['index']) == (L, idx) and r['result'] not in ('пройдена',) and r['M']]
        sig = {}
        for r in rs:
            last = r['trace'][-1] if r['trace'] else [0, 0, '?']
            k = (r['result'], r['M']['why'], last[0], last[1], code_of(r['M']['x'], r['M']['z']))
            sig.setdefault(k, []).append(r)
        for k, v in sig.items():
            if len(v) >= 2:
                obj = v[0]['trace'][-1][2] if v[0]['trace'] else '?'
                no_strat = ' (нет стратегии: ' + obj + ')' if obj in ('destroy', 'dump', 'destroyboss') else ''
                return True, f'{k[0]}: {k[1]}, стадия {k[2]}.{k[3]} {obj}, место {k[4]}{no_strat}'
        return False, ''
    def key_of(L, idx, v): return f'L{L}_{plans[(L, idx)]["id"]}_v{v}'
    def next_job():
        with slock:
            rows = load_rows(out); done.update(r['key'] for r in rows)
            have = lambda L, idx: {v for v in range(tot) if key_of(L, idx, v) in done or key_of(L, idx, v) in inflight}
            for v in range(min(a.first, a.variants)):   # ширина
                for (L, idx) in order:
                    if key_of(L, idx, v) not in done and key_of(L, idx, v) not in inflight:
                        inflight.add(key_of(L, idx, v)); return (L, idx, v)
            clean = {(r['level'], r['index']) for r in rows if r['result'] == 'пройдена'}
            skip = {}
            cand = []
            for (L, idx) in order:
                if (L, idx) in clean: continue
                hv = have(L, idx)
                if len(hv) >= tot: continue
                if any(key_of(L, idx, v) in inflight for v in range(tot)): continue   # у задания уже идёт прогон
                u, why = useless(L, idx)
                if u: skip[f'L{L}:{plans[(L, idx)]["id"]}'] = why; continue
                cand.append((len(hv), -best_frac(rows, L, idx), L, idx, hv))
            if skip: (out / 'useless.json').write_text(json.dumps(skip, ensure_ascii=False, indent=1))
            if not cand: return None
            cand.sort(); _, _, L, idx, hv = cand[0]
            v = next(v for v in range(tot) if v not in hv)
            inflight.add(key_of(L, idx, v)); return (L, idx, v)
    def worker():
        while True:
            wait_for_power(out)   # от батареи — не брать новое
            if a.until and time.time() > a.until: return
            j = next_job()
            if j is None:
                with slock:
                    if not inflight: return
                time.sleep(20); continue
            try:
                work(j)
            finally:
                with slock: inflight.discard(key_of(*j))
    ths = [threading.Thread(target=worker) for _ in range(a.procs)]
    for t in ths: t.start()
    for t in ths: t.join()
    txt = report(out, a.report_to)
    print(txt[:3000]); print('->', out / 'missions.md')
    return 0


if __name__ == '__main__':
    sys.exit(main())
