#!/usr/bin/env python3
"""Ночные боты (№876, Роб 23.10): сценарии безголового эмулятора на текущем наборе TEST.
  python3 tools/night/make_scenarios.py OUTDIR [--points 100]
Пишет OUTDIR/explore_L{N}.txt (фото-исследователь: точки бота-репортёра build/scout/l{N}.bin, 4 направления камеры; diaglog 0), OUTDIR/explore_diag_L{N}.txt (короткий круг тех же точек с журналом выброса GE, diaglog 1; --diagpoints 12),
OUTDIR/gags_L{N}.txt (гэги с персонажами: gaglist, затем каждый гэг дважды gagplay — вход в комнату, потом игра, 2 снимка),
OUTDIR/soak.txt (долгая игра: круг L1→L7, на каждом уровне езда; память — строки LEVEL ev=load/STAT),
OUTDIR/missions_L{N}.txt (бот «Миссии и догонялки», Роб 24.10: сюжетные миссии и бонусные гонки/Hit & Run по порядку из .msn, погоня на езде;
  дорожка laneD в night_run.sh, разбор — раздел «Миссии» в night_report.py)."""
import struct, sys
from pathlib import Path
import numpy as np
ROOT = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
NP = int(sys.argv[sys.argv.index('--points') + 1]) if '--points' in sys.argv else 100
SEED = int(sys.argv[sys.argv.index('--seed') + 1]) if '--seed' in sys.argv else 0   # круг ночного цикла: другие точки (первая точка выбора — случайная)
GAGS = {1: 33, 2: 30, 3: 30, 4: 33, 5: 30, 6: 30, 7: 33}   # верхняя граница числа гэгов (лишние индексы движок игнорирует)

def points(L, n):
    b = (ROOT / f'build/scout/l{L}.bin').read_bytes(); nt, npc, npt = struct.unpack_from('<III', b, 4)
    off = 16 + nt * 36 + nt * 4 + npc * 28; P = np.frombuffer(b, '<f4', npt * 3, off).reshape(-1, 3)
    if len(P) <= n: return P
    s0 = int(np.random.default_rng(SEED * 7 + L).integers(len(P))) if SEED else 0
    sel = [s0]; d = np.linalg.norm(P[:, [0, 2]] - P[s0, [0, 2]], axis=1)   # самые удалённые друг от друга
    for _ in range(n - 1):
        i = int(np.argmax(d)); sel.append(i); d = np.minimum(d, np.linalg.norm(P[:, [0, 2]] - P[i, [0, 2]], axis=1))
    return P[sel]

ND = int(sys.argv[sys.argv.index('--diagpoints') + 1]) if '--diagpoints' in sys.argv else 12   # №922: точек в коротком круге с журналом выброса GE (diaglog 1)
# №922 (разбор 24.10): flag diaglog 1 стоит ≈0,3 с на секунду игры (DIAGS diag_us, ≈10 мс на кадр) — все «медленные кадры» исследователя были его ценой. Основной круг идёт с diaglog 0
# (кадры, недогруз клеток), выброс GE меряет отдельный короткий круг explore_diag_L{N} (те же имена точек P###, первые ND точек выбора), diaglog 1
head = lambda L, diag=0: ['wait 12'] + ([] if L == 1 else ['scene LOAD', f'flag golevel {L}', 'wait 40']) + ['flag cfgdefaults 1', f'flag diaglog {diag}']
MONLY = '--missions-only' in sys.argv   # 06.11 (Роб: «быстрый прогон начала и конца каждой миссии»): только missions_L*, без точек разведчика (build/scout) и без погони
for L in ([] if MONLY else range(1, 8)):
    s = head(L) + ['flag camin 1']
    for i, p in enumerate(points(L, NP)):
        s += [f'scene P{i:03d}', f'foot {p[0]:.1f} {p[2]:.1f} 0', 'wait 1.6']
        for k in range(4): s += ['shot e%03d_%d' % (i, k), 'press r 1.0', 'wait 1.15']
    (out / f'explore_L{L}.txt').write_text('\n'.join(s + ['end']) + '\n')
    sd = head(L, 1) + ['flag camin 1']
    for i, p in list(enumerate(points(L, NP)))[:ND]:
        sd += [f'scene P{i:03d}', f'foot {p[0]:.1f} {p[2]:.1f} 0', 'wait 1.6']
        for k in range(4): sd += ['shot e%03d_%d' % (i, k), 'press r 1.0', 'wait 1.15']
    (out / f'explore_diag_L{L}.txt').write_text('\n'.join(sd + ['end']) + '\n')
    g = head(L) + ['flag gaglist 1', 'wait 0.5']
    for k in range(GAGS[L]):
        g += [f'scene G{k:02d}', f'flag gagplay {k}', 'wait 1.5', f'flag gagplay {k}', 'wait 1.0', f'shot g{k:02d}_a', 'wait 1.5', f'shot g{k:02d}_b']
    (out / f'gags_L{L}.txt').write_text('\n'.join(g + ['end']) + '\n')
    # езда (Роб 24.10: «чтобы на машинах ездили по всему уровню», данные для оптимизации): 8 точек по всему уровню, автопилот flag drive 45 с (№914 сажает в машину уровня),
    # снимок каждые 9 с (даль, верхушки домов), журнал раз в секунду: работа кадра, загрузки клеток, память
    st = head(L)
    for i, p in enumerate(points(L, 8)[::-1]):
        st += [f'scene D{i:02d}', f'foot {p[0]:.1f} {p[2]:.1f} 0', 'wait 2', 'flag drive 45']
        for k in range(5): st += ['wait 9', f'shot d{i:02d}_{k}']
        st += ['flag stop', 'wait 2']
    (out / f'drive_L{L}.txt').write_text('\n'.join(st + ['end']) + '\n')

# ---- бот «Миссии и догонялки» (Роб 24.10): каждая миссия уровня стартует с начала, дальше ЦИКЛ ПРОХОДА: flag msngo («подойти к текущей цели»: пешком — к собеседнику/двери, в машине —
#   машину на цель) каждые ~3 с, а реплики, брифинг, △ у собеседника и двери и посадку за руль делает помощник движка flag msnbot 1 (MSNBOT_OLD; без него △ в машине выходит из неё).
#   Этап засчитывает настоящий рантайм миссий по положению: недоходимая цель / зависший этап = этап не меняется несколько циклов подряд (MSN stage в журнале).
#   Порядок из level0N.msn: бонусные sr1 sr2 sr3 (уличные гонки, flag msnmainat IDX*100 — с гонки), gr1 (гонка на ставку, flag coins 99), bm1 (бонусная миссия, этап 1), затем сюжетные m* (flag msnsel):
#   последняя сюжетная миссия уровня при прохождении переводит игру на следующий уровень (LEVELDONE), после неё сценарий кончается. Последняя миссия L7 (финал, SetGameOver)
#   не берётся — её путь tests/content/ending_l7_checkpoint.txt + tests/smoke_scenes/l7final_hw.txt.
#   Погоня (Hit & Run): flag hnr 100 + автопилот езды №914 в 3 точках уровня — полиция/трафик/ИИ-машины; цена кадра — строки STAT/SLOWFRAME сцены CH_i.
STORY = {1: 8, 2: 8, 3: 7, 4: 7, 5: 8, 6: 8, 7: 8}   # сюжетных миссий в level0N.msn (m0..m7 у L1, m1..m7/m8 у остальных), бонусных везде 5 (sr1 sr2 sr3 gr1 bm1)
CYC = int(sys.argv[sys.argv.index('--cycles') + 1]) if '--cycles' in sys.argv else 16   # циклов прохода на миссию (~3 с игрового времени каждый)
cycle = ['flag msngo', 'wait 3']
PASS_S = float(sys.argv[sys.argv.index('--pass') + 1]) if '--pass' in sys.argv else 0.0   # 06.11: помощник засчитывает этап, стоящий дольше PASS_S с (flag msnbotpass; MSN pass … why=stuck) — прогон «начало-конец миссий»
BONUS = [('sr1', 0, []), ('sr2', 0, []), ('sr3', 0, []), ('gr1', 0, ['flag coins 99']), ('bm1', 1, [])]   # (метка, этап для flag msnmainat, подготовка)
if PASS_S > 0: BONUS[-1] = ('bm1', 0, [])   # 06.11: с помощником — с первого этапа, как игрок: этап 1 продолжает таймер этапа 0, прыжок на него давал «Timed out» через 0,1 с (L3, L7)
for L in range(1, 8):
    nst = STORY[L] - (1 if L == 7 else 0)
    m = head(L) + ['flag msn', 'flag msnbot 1'] + ([f'flag msnbotpass {PASS_S}'] if PASS_S > 0 else []) + ['wait 3']
    steps = [(t, ['flag cfgdefaults 1'] + pre + [f'flag msnmainat {(STORY[L] + k) * 100 + st}']) for k, (t, st, pre) in enumerate(BONUS)] + [('m%d' % k, [f'flag msnsel {k}']) for k in range(nst)]
    for tag, start in steps:
        m += [f'scene MS_{tag}_start'] + start + ['wait 3', f'shot ms_{tag}_a']
        for c in range(CYC):
            m += [f'scene MS_{tag}_c{c:02d}'] + cycle
            if c % 4 == 3: m += [f'shot ms_{tag}_c{c:02d}']
        m += ['flag msn', 'wait 1.5']
    for i, p in enumerate([] if MONLY else points(L, 3)[::-1]):   # погоня: розыск 100 %, автопилот, трафик и полиция вокруг
        # №922: прежде `foot` ставил Гомера пешком в точку, а flag drive пересаживал его в машину на спавне — второй прыжок на сотни метров (JUMPRUSH ≈1,9 МБ, ph5 130–300 мс в первую секунду погони;
        # настоящая езда 90 с без пиков). Теперь: посадка в машину (flag drive 2 — короткая езда, PILOT seat), машина goto в точку одним прыжком в сцене CHJ_i (подгрузка клеток после прыжка в счёт погони
        # не идёт), и только потом розыск и езда в сцене CH_i
        m += ['flag msnbot 0', 'flag drive 2', 'wait 3', 'flag stop', f'scene CHJ_{i}', f'goto {p[0]:.1f} {p[2]:.1f} 0', 'wait 3', f'scene CH_{i}', 'flag repair', 'flag hnr 100', 'flag drive 40', 'wait 10', f'shot ch{i}_a', 'wait 15', f'shot ch{i}_b', 'wait 15', f'shot ch{i}_c', 'flag stop', 'flag hnr 0', 'wait 2']
    (out / f'missions_L{L}.txt').write_text('\n'.join(m + ['end']) + '\n')
s = ['wait 12', 'flag cfgdefaults 1']
for rnd in ([] if MONLY else range(5)):
    for L in range(1, 8):
        s += [f'scene R{rnd}_L{L}', f'flag golevel {L}', 'wait 40', 'press triangle 0.2', 'wait 3', 'flag drive 20', 'wait 120', 'flag stop', 'press triangle 0.2', 'wait 4']
(out / 'soak.txt').write_text('\n'.join(s + ['end']) + '\n')
print('сценарии:', sorted(p.name for p in out.glob('*.txt')))
