#!/usr/bin/env python3
"""Гейт правки Роба 264 (данные): «основные логотипы, которые красиво прорисовываются, системно; PSP мелко или убрать; в конце —
El Barto, будто Барт тегнул, тег баллончиком из игры». Сборка tools/bootmarks.py: знаки The Simpsons, Fox, Disney, Gracie,
Radical (1 бит цветом GONE, волна), без PSP и Vivendi; последним — El Barto из файла Роба (1 бит цветом GONE, режим 6).
Красный: BMK_264_OLD=1 (набор правки 257) -> FAIL"""
import io, contextlib, os, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools')); sys.argv = sys.argv[:1]
with contextlib.redirect_stdout(io.StringIO()):
    import bootmarks as B
    _, marks = B.build2()
bad = []
print('знаков %d: %s' % (len(marks), ', '.join('%s %d×%d р%d' % (m[0], m[1].shape[1], m[1].shape[0], m[2]) for m in marks)))
if os.environ.get('BMK_294_OLD'): want = ['simpsons', 'fox', 'disney', 'gracie', 'radical', 'elbarto']
elif os.environ.get('BMK_396_OLD') or os.environ.get('BMK_379_OLD'): want = ['simpsons', 'fox', 'disney', 'gracie', 'radical', 'claude', 'codex', 'elbarto']
else: want = ['simpsons', 'fox', 'disney', 'gracie', 'codex', 'radical', 'claude', 'elbarto']   # правка 396: ChatGPT · Radical · Claude
if os.environ.get('ELBARTO1010_OLD') != '1': want = want + ['elbarto6']   # №1010: родной тег с бака L6 рядом с прежним (рисуется один)
idle = [m for m in marks if m[2] in (7, 8)]; marks = [m for m in marks if m[2] not in (7, 8)]   # правка 397: кадры покоя — отдельно
base = {m[0]: m for m in marks}
for name, idx, mode, cols in idle:   # кадр покоя: есть свой знак, тот же размер, 1 бит цветом GONE; цветок — 7, человечек — 8
    want_mode = {'codex': 7, 'claude': 8}.get(name)
    if name not in base or idx.shape != base[name][1].shape or mode != want_mode or set(int(c) for c in set(idx.flatten())) - {0, 1} or cols[1] != B.REST2:
        bad.append('%s: кадр покоя режима %d не по правилу 397' % (name, mode))
if not os.environ.get('BMK_397_OLD') and not (os.environ.get('BMK_396_OLD') or os.environ.get('BMK_294_OLD') or os.environ.get('BMK_379_OLD')):
    if sum(m[0] == 'codex' for m in idle) != 11 or sum(m[0] == 'claude' for m in idle) != 5: bad.append('кадров покоя: codex %d (ждали 11), claude %d (ждали 5)' % (sum(m[0] == 'codex' for m in idle), sum(m[0] == 'claude' for m in idle)))
names = [m[0] for m in marks]
if names != want: bad.append('набор/порядок %s, ждали %s' % (names, want))
for name, idx, mode, cols in marks:
    if name == 'elbarto':   # уточнение Роба 04.10 06:3x: его El Barto, пиксель, белым по чёрному (цвет GONE)
        if mode != 6: bad.append('El Barto: режим %d, ждали 6 (появление последним)' % mode)
        if set(int(c) for c in set(idx.flatten())) - {0, 1} or cols[1] != B.REST2: bad.append('El Barto: не 1 бит цветом GONE (белым по чёрному)')
        if idx.shape[1] < 70: bad.append('El Barto мелкий: %d px' % idx.shape[1])
    elif name == 'elbarto6':   # №1010/№1010б: текстура бака L6, 1 бит, режим 6; б — цветом знаков (без краски), пропорции бака, в месте прежнего тега
        if mode != 6 or set(int(c) for c in set(idx.flatten())) - {0, 1}: bad.append('El Barto L6: не 1 бит режима 6')
        ink = B.TAG_INK.get('elbarto6', 0); r, g, bl = ink & 255, (ink >> 8) & 255, (ink >> 16) & 255; h6, w6 = idx.shape
        if os.environ.get('ELBARTO1010B_OLD') == '1':
            if not (50 <= w6 <= 64 and 40 <= h6 <= 64): bad.append('El Barto L6: %d×%d, ждали текстуру 1:1 (буквы 59×48)' % (w6, h6))
            if not (r > 180 and g < 80 and bl < 80): bad.append('El Barto L6: краска не красная (%d, %d, %d)' % (r, g, bl))
        else:
            if ink or cols[1] != B.REST2: bad.append('El Barto L6: не серым знаков (краска %08x)' % ink)
            if not (100 <= w6 <= 112 and 40 <= h6 <= 50): bad.append('El Barto L6: %d×%d, ждали в месте прежнего тега 112×50' % (w6, h6))
            if w6 < 1.9 * h6: bad.append('El Barto L6: %d×%d сжат — на баке шире в 1,87 раза' % (w6, h6))
    elif set(int(c) for c in set(idx.flatten())) - {0, 1} or cols[1] != B.REST2 or mode != 0: bad.append('%s: не 1 бит цветом GONE волной' % name)
for b in bad: print('FAIL:', b)
print('RESULT:', 'FAIL' if bad else 'OK')
sys.exit(1 if bad else 0)
