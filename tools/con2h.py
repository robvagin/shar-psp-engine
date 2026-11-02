"""Параметры машин из скриптов диска -> viewer/carparams.h (статическая таблица, ключ — имя машины).

python tools/con2h.py [--src hit-and-run-web/source/game/scripts/cars] [--out viewer/carparams.h]

Читает все scripts/cars/*.con верхнего уровня (сами машины; в подпапках — настройки ИИ-водителей миссий, их не
берём). Разбор как у hit-and-run-web/tools/vehicle_tuning.py: комментарии // и /* */ отбрасываются, у повторов
берётся последний вызов. Команды без числового параметра (SetDriver, SetShadowAdjustments и т.п.) пропускаются.

Смысл и единицы — в комментарии, который пишется в carparams.h (FIELDS ниже). Источники: hit-and-run-web
(src/vehicle-physics.ts — их модель ссылается на разбор PAL-версии игры, адреса 0x2801e8/0x280388; MIT),
donut (GPL: там команды .con только объявлены, без физики), общеизвестное о движке.
"""
import argparse, os, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# (команда .con, поле C, значение по умолчанию, если команды нет: число или имя другого поля, комментарий)
FIELDS = [
    ('SetMass', 'mass', 1500.0, 'масса, кг. Все силы шин/мотора в модели пропорциональны массе, поэтому разгон и\n'
     '  повороты задают *Scale/TireGrip/Steering; масса ощущается в ударах (отскок, подброс, урон) и в столкновениях машин'),
    ('SetGasScale', 'gasScale', 4.5, 'тяга: ускорение м/с2 на ведущее (заднее) колесо при полном газе; 2 колеса ->\n'
     '  разгон 2*GasScale у земли. Сопротивление 2*GasScale*(v/Vmax)^2 уравновешивает тягу ровно на TopSpeed'),
    ('SetSlipGasScale', 'slipGasScale', 'gasScale', 'то же в заносе (режим slip): обычно чуть больше — занос не душит газ'),
    ('SetHighSpeedGasScale', 'hsGasScale', 'gasScale', 'тяга выше порога GasScaleSpeedThreshold (доля TopSpeed); нет — как GasScale'),
    ('SetGasScaleSpeedThreshold', 'gasSpeedThreshold', 1.0, 'порог для HighSpeedGasScale, доля TopSpeed (1 = никогда)'),
    ('SetBrakeScale', 'brakeScale', 8.0, 'тормоз: замедление (BrakeScale + 3) м/с2 на ведущее колесо -> 2*(B+3) на машину'),
    ('SetTopSpeedKmh', 'topSpeedKmh', 120.0, 'максимальная скорость на ровном, км/ч (равновесие тяги и сопротивления, не жёсткий\n'
     '  предел: с горки и от ударов можно быстрее)'),
    ('SetMaxWheelTurnAngle', 'maxWheelTurnDeg', 30.0, 'угол поворота передних колёс при полном руле, градусы'),
    ('SetHighSpeedSteeringDrop', 'hsSteeringDrop', 0.27, 'на максимальной скорости угол руля меньше на эту долю (линейно по v/Vmax)'),
    ('SetTireGrip', 'tireGrip', 2.48, 'предел силы шины: TireGrip * нагрузка на колесо (не меньше mg/4, не больше mg) — «g» сцепления.\n'
     '  Сила больше TireGrip*mg/4 переводит машину в занос (slip)'),
    ('SetNormalSteering', 'normalSteering', 95.0, 'жёсткость шины вбок в сцеплении: F = m/4 * Steering * sin(угол увода) Н\n'
     '  (ниже 15% TopSpeed — пропорционально скорости, чтобы стоящая машина не дёргалась)'),
    ('SetSlipSteering', 'slipSteering', 55.0, 'жёсткость шины вбок в заносе с ручником (режим 2)'),
    ('SetEBrakeEffect', 'eBrakeEffect', 0.4, 'ручник: передние колёса x(1+E), задние x(1-E) по жёсткости -> зад уходит'),
    ('SetSlipSteeringNoEBrake', 'slipSteeringNoEBrake', 'slipSteering', 'жёсткость вбок в заносе без ручника (режим 1: сила шины\n'
     '  превысила предел на газу — «пауэрслайд»)'),
    ('SetSlipEffectNoEBrake', 'slipEffectNoEBrake', 0.0, 'в режиме 1: передние x(1+E), задние x(1-E)'),
    ('SetCMOffsetX', 'cmOffsetX', 0.0, 'смещение центра масс от центра кузова, м (x — влево в осях машины)'),
    ('SetCMOffsetY', 'cmOffsetY', 0.0, 'по высоте, м (минус — ниже)'),
    ('SetCMOffsetZ', 'cmOffsetZ', 0.0, 'вдоль, м (плюс — к носу)'),
    ('SetSuspensionLimit', 'suspensionLimit', 0.7, 'ход подвески: доля радиуса колеса (limit = r * SuspensionLimit, м)'),
    ('SetSpringK', 'springK', 0.3, 'пружина колеса: k = SpringK * m*g / limit (Н/м), плюс прогрессия 5*k*x^2'),
    ('SetDamperC', 'damperC', 0.15, 'амортизатор колеса: c = DamperC * 2*sqrt(k*m) (Н*с/м)'),
    ('SetSuspensionYOffset', 'suspensionYOffset', 0.0, 'сдвиг точек крепления подвески по высоте, м (в оригинале — посадка\n'
     '  кузова над колёсами). У нас колёса вшиты в модель, посадка берётся из модели: читается, в физике не используется'),
    ('SetHitPoints', 'hitPoints', 3.0, 'прочность: урон от удара делится на HitPoints/3 (лёгкие/трафик ~0.5-1 ломаются быстро)'),
    ('SetBurnoutRange', 'burnoutRange', 0.09, 'пробуксовка: полный газ ниже этой доли TopSpeed — тяга x0.75, колёса крутятся'),
    ('SetMaxSpeedBurstTime', 'maxSpeedBurstTime', 1.0, 'с; в оригинале — сколько можно держать «рывок» выше Vmax (не используется)'),
    ('SetDonutTorque', 'donutTorque', 2.0, 'пончик: в пробуксовке при руле > 0.6 момент рыскания Donut*m*(1-v/Vmax)/2 Н*м'),
    ('SetWeebleOffset', 'weebleOffset', -0.75, '«неваляшка»: точка приложения веса ниже центра масс на столько, м (минус — ниже).\n'
     '  Качнувшаяся/вставшая на два колеса машина возвращается на колёса. Нет в .con — медиана по машинам'),
    ('SetWheelieRange', 'wheelieRange', 0.0, 'вилли: полный газ ниже этой доли TopSpeed сдвигает центр масс на WheelieOffset'),
    ('SetWheelieOffsetY', 'wheelieOffsetY', 0.0, 'сдвиг центра масс при вилли по высоте, м'),
    ('SetWheelieOffsetZ', 'wheelieOffsetZ', 0.0, 'сдвиг центра масс при вилли вдоль, м (минус — назад: нос поднимается)'),
    ('SetGamblingOdds', 'gamblingOdds', 1.0, 'шансы машины в гонке на ставку (круг 2, P01-20): выплата = ставка + trunc(шансы × ставка); 1 / 2 / 3'),
]


# Сверка g01-cars (02.11.2026, P01-21): начальные значения, когда команды нет в .con, у оригинала другие, чем взяты из famil_v.
# CON2H_DEFAULTS_OLD=1 — прежние умолчания (famil_v). Для реальных машин сказывается только: SetWeebleOffset у cklimo, dune_v, huska, rocke_v (−1,0 против −0,75),
# SetSlipSteeringNoEBrake и SetSlipEffectNoEBrake у huska (50 и 0,1 против 35 и 0)
ORIG_DEFAULTS = {'mass': 1000.0, 'gasScale': 4.0, 'brakeScale': 3.0, 'topSpeedKmh': 220.0, 'maxWheelTurnDeg': 35.0, 'hsSteeringDrop': 0.0,
                 'tireGrip': 4.0, 'normalSteering': 110.0, 'slipSteering': 35.0, 'eBrakeEffect': 0.5, 'slipSteeringNoEBrake': 50.0,
                 'slipEffectNoEBrake': 0.1, 'suspensionLimit': 0.4, 'springK': 0.5, 'damperC': 0.2, 'hitPoints': 2.0, 'burnoutRange': 0.3,
                 'donutTorque': 10.0, 'weebleOffset': -1.0}
if os.environ.get('CON2H_DEFAULTS_OLD') != '1':
    FIELDS = [(c, f, ORIG_DEFAULTS.get(f, d), doc) for (c, f, d, doc) in FIELDS]


def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    return re.sub(r'//[^\n]*', '', s)


def parse(path):
    vals = {}
    for m in re.finditer(r'([A-Za-z_]\w*)\s*\(([^()]*)\)', strip_comments(path.read_text(errors='replace'))):
        args = [a.strip() for a in m.group(2).split(',') if a.strip()]
        if len(args) == 1 and re.fullmatch(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)', args[0]):
            vals[m.group(1)] = float(args[0])
    return vals


def cflt(x):
    s = f'{x:.6g}'
    return s + ('f' if ('.' in s or 'e' in s) else '.f')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=str(ROOT / 'hit-and-run-web/source/game/scripts/cars'))
    ap.add_argument('--out', default=str(ROOT / 'viewer/carparams.h'))
    a = ap.parse_args()
    cars = sorted(p for p in Path(a.src).glob('*.con') if p.is_file())
    rows, missing = [], {}
    for p in cars:
        v = parse(p)
        if 'SetMass' not in v: continue
        row = {}
        for cmd, field, dflt, _ in FIELDS:
            if cmd in v: row[field] = v[cmd]
            else:
                row[field] = row[dflt] if isinstance(dflt, str) else dflt
                missing.setdefault(cmd, []).append(p.stem)
        rows.append((p.stem.lower(), row))
    L = ['// Параметры машин из scripts/cars/*.con диска. СГЕНЕРИРОВАНО tools/con2h.py — руками не править.',
         '// Подключается только в car.c (статическая таблица). Поиск: carParamsFor("famil_v", &p) в car.h.',
         '// Поля (единицы и смысл, как их читает car.c; см. шапку tools/con2h.py об источниках):']
    for cmd, field, dflt, doc in FIELDS:
        d = f'нет -> {dflt}' if isinstance(dflt, str) else f'нет -> {dflt:g}'
        L.append(f'//   {field:<20} {cmd}: ' + doc.replace('\n', '\n//   ' + ' ' * 21) + f' ({d})')
    L.append('// Команды, которых нет в части файлов (там взято значение по умолчанию):')
    allnames = [n for n, _ in rows]
    for cmd, names in sorted(missing.items()):
        if len(names) * 2 > len(rows):
            L.append(f'//   {cmd}: у всех, кроме ' + ', '.join(n for n in allnames if n not in [m.lower() for m in names]))
        else: L.append(f'//   {cmd}: ' + ', '.join(names))
    L += ['#ifndef SHAR_CARPARAMS_H', '#define SHAR_CARPARAMS_H', '', 'typedef struct {', '  const char *name;']
    names = [f for _, f, _, _ in FIELDS]
    for i in range(0, len(names), 6):
        L.append('  float ' + ', '.join(names[i:i + 6]) + ';')
    L += ['} CarCon;', '', 'static const CarCon carCons[] = {']
    for name, row in rows:
        vals = ', '.join(cflt(row[f]) for f in names)
        L.append(f'  {{"{name}", {vals}}},')
    L += ['};', '#define CAR_CONS ((int)(sizeof carCons / sizeof carCons[0]))', '']
    # Правка Роба 333 (05.10): SetIrisTransition(1) в .con машины — посадка и выход ирисом (без клипов двери). Запись PS2
    # yt-bziOipI4gDI: Plow King (plowk_v) посадка 869,25 с и выход 1085,2 с, ракетная rocke_v 1978,45 с, угон зелёного седана
    # трафика 2061,2 с — круг к центру и от центра; famil_v (флага нет) — посадка клипами 111 с. Признак — у машины, не у ИИ-водителя
    iris = [p.stem.lower() for p in cars if 'SetMass' in (v := parse(p)) and v.get('SetIrisTransition', 0.0) != 0.0]
    L += ['// SetIrisTransition(1): посадка и выход ирисом оригинала, без клипов двери (правка 333; car.c carIrisFor)',
          'static const char *const carIrisCons[] = {']
    for i in range(0, len(iris), 10):
        L.append('  ' + ', '.join(f'"{n}"' for n in iris[i:i + 10]) + ',')
    L += ['};', '#define CAR_IRIS_CONS ((int)(sizeof carIrisCons / sizeof carIrisCons[0]))', '']
    # ИИ-водители миссий (29.09): .con в подпапках (missions/level01/m1race.con, easyrace/ai_5th.con, pursuit/hardcop.con …) —
    # полный набор физики машины миссии (AddStageVehicle, 4-й аргумент). Ключ — путь от scripts/cars без .con, строчные, '/'.
    # Команды, которых нет, — тем же правилом, что у машин (FIELDS); первая таблица этим не меняется
    ai = []
    for p in sorted(q for q in Path(a.src).rglob('*.con') if q.is_file() and q.parent != Path(a.src)):
        v = parse(p)
        if 'SetMass' not in v: continue
        row = {}; mask = 0
        for i, (cmd, field, dflt, _) in enumerate(FIELDS):
            row[field] = v[cmd] if cmd in v else (row[dflt] if isinstance(dflt, str) else dflt)
            if cmd in v: mask |= 1 << i
        ai.append((p.relative_to(a.src).with_suffix('').as_posix().lower(), row, mask))
    L += ['// ИИ-водители машин миссий (AddStageVehicle(имя, локатор, ИИ, "<путь>.con", водитель)): путь от scripts/cars',
          '// без .con, строчные, "/" — carParamsForAi("Missions\\\\level01\\\\M1race.con", &p) в car.h. Поля — как у carCons',
          'static const CarCon carConsAi[] = {']
    for name, row, _m in ai:
        L.append(f'  {{"{name}", ' + ', '.join(cflt(row[f]) for f in names) + '},')
    L += ['};', '#define CAR_CONS_AI ((int)(sizeof carConsAi / sizeof carConsAi[0]))', '']
    # Правка Роба 10.10 («у каждой машины своя физика»): .con водителя — последовательность Set*, а не полный набор. Бит i (порядок
    # FIELDS = порядок полей CarCon после имени) стоит, если команда есть в файле; остальное остаётся от самой машины (car.c carParamsForAi)
    L += ['// Маска команд, заданных в .con водителя (бит i — i-е поле CarCon после имени); незаданное берётся из .con самой машины',
          'static const unsigned carConsAiMask[] = {']
    for i in range(0, len(ai), 8):
        L.append('  ' + ', '.join(f'0x{m:08x}u' for _n, _r, m in ai[i:i + 8]) + ',')
    L += ['};', '', '#endif', '']
    Path(a.out).write_text('\n'.join(L))
    print(f'{a.out}: {len(rows)} cars, {len(ai)} AI drivers, {len(names)} parameters each')


if __name__ == '__main__':
    main()
