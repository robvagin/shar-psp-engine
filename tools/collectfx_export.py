#!/usr/bin/env python3
"""Эффекты целей миссий SetCollectibleEffect (все уровни) -> viewer/collectfx_data.h (таблица для viewer/collectfx.c).

  python3 tools/collectfx_export.py            # перезаписать viewer/collectfx_data.h
  python3 tools/collectfx_export.py --check    # 0 — заголовок совпадает с диском, 1 — устарел (вторая копия правды не живёт)

Только диск Роба (read-only). Скрипты миссий уровней 1–7: SetCollectibleEffect("<имя>") — 321 раз wrench_collect (точки
гонок, цели goto carsphere/triggersphere), bonestorm_explosion (таран/доставка ящиков), carfish (L3 m6), lsrexpld (L6 m6),
"none". Эффект — составной объект 0x4512 того же имени в файле, который грузят скрипты (LoadP3DFile: art/wrench.p3d уровня,
art/missions/level01/boxexpld.p3d, level03/carfish.p3d, level06/lsrexpld.p3d): квады свечения (BillboardQuadGroup,
анимация BQG_: ширина, высота, цвет по кадрам — tools/msnart_export.export_file, как у маркеров) и системы частиц (0x4518;
сами системы — в fx.shr уровня, tools/fx_export.py). Положение частей — суставы скелета эффекта в позе кадра 0
(у wrench_collect свечения на 2.0 м, частицы на 1.4 м над точкой цели). Анимация квадов разовая: дорожка обрезана на
последнем видимом кадре (сложение — цвет > 0, прозрачность — альфа > 0).
"""
import argparse, struct, sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools')); sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'tools/missions'))
from p3d import read, walk, string        # noqa: E402  (MIT)
import mfk                                # noqa: E402
import msnart_export as MX                # noqa: E402

GAME = ROOT / 'hit-and-run-web/source/game'
OUT = ROOT / 'viewer/collectfx_data.h'
LEVELS = range(1, 8)
DEFAULT_EFFECT = 'mission_col'            # 1340: составной объект art/missions/generic/missgen.p3d (общий файл миссий)
FX_BLEND = {2: 1, 3: 2}                   # BLMD шейдера -> смешивание fxSprite (1 сложение, 2 вычитание, иначе 0 прозрачность)


def effects_used():
    """{имя эффекта: [уровни]} по всем скриптам миссий (комментарии mfk не считаются)"""
    out = {}
    for lv in LEVELS:
        for f in sorted((GAME / f'scripts/missions/level{lv:02d}').glob('*.mfk')):
            calls, _ = mfk.parse_file(f)
            for c in calls:
                if c.op == 'SetCollectibleEffect' and c.args:
                    n = str(c.args[0])
                    if n.lower() != 'none': out.setdefault(n, set()).add(lv)
    out.setdefault(DEFAULT_EFFECT, set()).update(LEVELS)   # 1340: эффект по умолчанию предмета без SetCollectibleEffect (в оригинале эффект по умолчанию при подборе)
    return {k: sorted(v) for k, v in sorted(out.items())}


def find_composite(name):
    """файл (как в скрипте), где лежит составной объект name, среди LoadP3DFile уровней"""
    seen = []
    for lv in LEVELS:
        for p in MX.load_scripts(lv):
            if p in seen or not (GAME / p).exists(): continue
            seen.append(p)
            root = read(GAME / p)
            if any(c.id == 0x4512 and string(c.data)[0] == name for c in root.children): return p, root
    return None, None


def build():
    eff = effects_used(); defs = []
    for name, levels in eff.items():
        path, root = find_composite(name)
        if not path: raise SystemExit(f'{name}: составного объекта нет в файлах LoadP3DFile уровней {levels}')
        e = MX.export_file(path, only=(name,))
        d = e.draws[0] if e.draws else dict(bills=[], frames=[], njoint=0)
        texname = {i: n for n, i in e.texidx.items()}
        J = [np.asarray(d['frames'][j], np.float64) for j in range(d['njoint'])] if d['njoint'] else []   # кадр 0: R столбцы + t
        def at(joint, c=(0., 0., 0.)):
            if joint >= len(J): return np.asarray(c, np.float64)
            R = J[joint][:9].reshape(3, 3).T; return R @ np.asarray(c, np.float64) + J[joint][9:12]
        bills = []
        for b in d['bills']:
            if b['mode'] == 0: raise SystemExit(f'{name}: квад NOAX (в плоскости сустава) — движок эффекта рисует только к камере')
            tr = np.asarray(b['track'], np.float64)
            vis = tr[:, 2:5].max(1) > .004 if FX_BLEND.get(b['blend'], 0) else tr[:, 5] > .004
            n = int(np.nonzero(vis)[0].max()) + 2 if vis.any() else 0   # +1 — кадр, где гаснет до нуля (интерполяция)
            n = min(n, len(tr))
            if not n: continue
            bills.append(dict(tex=texname[b['tex']], blend=FX_BLEND.get(b['blend'], 0), off=at(b['joint'], b['c']), fps=b['fps'], track=tr[:n]))
        cd = next(c for c in root.children if c.id == 0x4512 and string(c.data)[0] == name)
        ps = []
        for el in walk(cd):
            if el.id != 0x4518: continue
            dn, p = string(el.data); _, joint = struct.unpack_from('<2I', el.data, p)
            ps.append(dict(sys=dn, off=at(joint)))
        dur = max([len(b['track']) / b['fps'] for b in bills] + [0.])
        defs.append(dict(name=name, file=path, levels=levels, bills=bills, ps=ps, dur=dur))
    return defs


def f4(x):
    s = f'{float(x) + 0.:.5g}'
    return s + ('' if any(ch in s for ch in '.e') else '.') + 'f'


def header(defs):
    L = ['// Эффекты целей миссий SetCollectibleEffect — СОЗДАНО tools/collectfx_export.py по диску (не править руками;',
         '// проверка: python3 tools/collectfx_export.py --check). Движок: viewer/collectfx.c. Координаты движка, м; дорожка квада —',
         '// по кадрам fps: ширина, высота, r, g, b, a (0..1). Текстуры и системы частиц — по имени из fx.shr уровня.',
         '#ifndef COLLECTFX_DATA_H', '#define COLLECTFX_DATA_H', '']
    for k, d in enumerate(defs):
        L.append(f'// {d["name"]}: {d["file"]} (уровни {",".join(map(str, d["levels"]))}), {d["dur"]:.2f} с')
        for i, b in enumerate(d['bills']):
            L.append(f'static const float cfxTr{k}_{i}[][6] = {{')
            for row in b['track']: L.append('  {' + ', '.join(f4(x) for x in row) + '},')
            L.append('};')
        if d['bills']:
            L.append(f'static const CfxBill cfxBill{k}[] = {{')
            for i, b in enumerate(d['bills']):
                L.append(f'  {{"{b["tex"]}", {b["blend"]}, {{{", ".join(f4(x) for x in b["off"])}}}, {f4(b["fps"])}, {len(b["track"])}, cfxTr{k}_{i}}},')
            L.append('};')
        if d['ps']:
            L.append(f'static const CfxPs cfxPs{k}[] = {{')
            for p in d['ps']: L.append(f'  {{"{p["sys"]}", {{{", ".join(f4(x) for x in p["off"])}}}}},')
            L.append('};')
        L.append('')
    L.append('static const CfxDef cfxDefs[] = {')
    for k, d in enumerate(defs):
        L.append(f'  {{"{d["name"]}", {f4(d["dur"])}, {len(d["bills"])}, {f"cfxBill{k}" if d["bills"] else "0"}, '
                 f'{len(d["ps"])}, {f"cfxPs{k}" if d["ps"] else "0"}}},')
    L += ['};', f'#define CFX_NDEF {len(defs)}', '#endif', '']
    return '\n'.join(L)


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('--check', action='store_true'); ap.add_argument('--out', default=str(OUT)); a = ap.parse_args()
    defs = build(); text = header(defs)
    for d in defs:
        print(f'  {d["name"]:20s} {d["file"]}: квадов {len(d["bills"])} ({", ".join(b["tex"] for b in d["bills"])}), '
              f'частиц {len(d["ps"])} ({", ".join(p["sys"] for p in d["ps"])}), {d["dur"]:.2f} с, уровни {d["levels"]}')
    out = Path(a.out)
    if a.check:
        ok = out.exists() and out.read_text() == text
        print(f'{out}: ' + ('совпадает с диском' if ok else 'УСТАРЕЛ — python3 tools/collectfx_export.py')); return 0 if ok else 1
    out.write_text(text); print(f'{out}: эффектов {len(defs)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
