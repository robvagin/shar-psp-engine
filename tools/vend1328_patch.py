#!/usr/bin/env python3
"""№1328: перевыпуск ТОЛЬКО таблиц состояний стейтпропов (SPR1) внутри готового level.shr, всё остальное — байт в байт.
Нужен, когда файлы уровней выпущены другой версией экспортёра, и целиком перевыпустить уровень нельзя (список текстур, подпись notoon, ключ lmap остались бы прежними).
  python3 tools/vend1328_patch.py N IN.shr OUT.shr     патч уровня N (по номеру записи видимости, последний блок машины состояний; см. tools/prp2_export.py pick_stateprop / vis_by_index)
  python3 tools/vend1328_patch.py N IN.shr --verify    сверка: таблицы состояний, которые даёт экспортёр по-старому (VEND1328_OLD=1), равны записанным в IN — тогда подмена таблиц безопасна
  python3 tools/vend1328_patch.py --diff OLD.shr NEW.shr   диапазоны отличий (хвост за секцией стейтпропов сравнивается со сдвигом)
VEND1328_OLD=1 в окружении — не патчить таблицы состояний; GLASS1352_OLD=1 — не трогать цвет вершин стекла автомата. Оба — OUT = копия IN.
№1352: вторая часть патча — цвет вершин стекла автомата с Красти (кусок из 6 вершин у типа с вызовом «монеты вперёд») ставится «без тона» 0xFFFDFDFD, как на L1 (диск красит стекло под время суток уровня: зелёный, розовый, синий…); размер файла не меняется, меняются 4 байта цвета у 6 вершин.
Что меняется: начало таблиц состояний/событий/вызовов в SPR1 (типы стейтпропов уровня, их записи у экспортёра по-старому совпадают с файлом байт в байт — это проверяет --verify) берётся от экспортёра, записи типов, добавленных psp_export (ключи, карточки), сдвигаются по номерам; в записях типов — поля первого состояния/числа состояний;
если таблицы стали длиннее (автомат L2: 6 состояний вместо 4), SPR1 растёт на кратное 16, и сдвигаются: поля PRP2 @36/@40/@44/@52/@60 (то, что за SPR1), заголовок level.shr @88/@100, 'at' в REGF перед PVS1, @36 каждой записи клеточной таблицы.
Тогда у level.shr другой размер: lmap.shr надо перепечь (tools/lmap_pack.sh), его ключ — заголовок и размер."""
import sys, os, struct, io, contextlib
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools')); sys.path.insert(0, str(ROOT / 'hit-and-run-web/tools'))

FM = dict(prec='<4f2i4I', trec='<10H2H4B4I2f2I3f3f', jrec='<hhI4f3f3f4x', key='<7f', vis='<I', erec='<BBH', crec='<BBHf', col='<BBH3f9f3f', bill='<4Bi6fII', irec='<3f4fHH')
NAMES = ['vert', 'idx', 'piece', 'type', 'joint', 'key', 'vis', 'state', 'event', 'call', 'col', 'bill', 'inst', 'dyn', 'fxname']
HEAD_COUNTS = ['ntype', 'ninst', 'npiece', 'njoint', 'nkey', 'nvis', 'nstate', 'nevent', 'ncall', 'ncol', 'nbill', 'vlen', 'ilen', 'ndyn', 'nfxn']

def esize(P):
    import prp2_export as X
    s = dict(piece=struct.calcsize(FM['prec']), type=struct.calcsize(FM['trec']), joint=struct.calcsize(FM['jrec']), key=28, vis=4,
             state=struct.calcsize('<I4B2HfHHHHf') + 12 * X.MAX_FX + 4, event=4, call=8, col=struct.calcsize(FM['col']), bill=struct.calcsize(FM['bill']), inst=struct.calcsize(FM['irec']), dyn=1, fxname=32)
    return s

def parse_spr1(b):
    """-> (counts, parts{имя: (смещение, длина)}, конец частей) для SPR1 без обращения к текстурам"""
    assert b[:4] == b'SPR1'
    c = dict(zip(HEAD_COUNTS, struct.unpack_from('<15I', b, 4)))
    es = esize(None)
    ln = dict(vert=c['vlen'], idx=c['ilen'], piece=c['npiece'] * es['piece'], type=c['ntype'] * es['type'], joint=c['njoint'] * es['joint'], key=c['nkey'] * es['key'], vis=c['nvis'] * 4,
              state=c['nstate'] * es['state'], event=c['nevent'] * 4, call=c['ncall'] * 8, col=c['ncol'] * es['col'], bill=c['nbill'] * es['bill'], inst=c['ninst'] * es['inst'], dyn=c['ndyn'], fxname=c['nfxn'] * 32)
    off = 64; parts = {}
    for n in NAMES:
        parts[n] = (off, ln[n]); off += ln[n]; off += -off % 16
    return c, parts, off

def stub_tables(level, old):
    """таблицы состояний экспортёра для уровня без геометрии (куски не нужны): (srecs, erecs, crecs, trecs, число типов)"""
    import prp2_export as X
    os.environ['VEND1328_OLD'] = '1' if old else '0'
    art = (ROOT / 'hit-and-run-web/source/game/art').resolve()
    with contextlib.redirect_stdout(io.StringIO()):
        sp = X.StateProps(level, art, lambda f, m: [], lambda f, s: -1, verbose=False)
        b = sp.section({}, ())
    c, p, _ = parse_spr1(b)
    g = lambda n: b[p[n][0]:p[n][0] + p[n][1]]
    return dict(state=g('state'), event=g('event'), call=g('call'), type=g('type'), c=c)

def level_parts(d):
    tableOff, ncell, cellOff, pvs, col, prop, car = struct.unpack_from('<7I', d, 76)
    assert d[prop:prop + 4] == b'PRP2'
    so, ss, tot = struct.unpack_from('<3I', d, prop + 32)
    return dict(ncell=ncell, cellOff=cellOff, pvs=pvs, col=col, prop=prop, car=car, spr=prop + so, sprLen=ss, prpLen=tot)

def patch(level, d):
    P = level_parts(d); es = esize(None)
    spr = d[P['spr']:P['spr'] + P['sprLen']]
    c, parts, endp = parse_spr1(spr)
    tail = spr[endp:]
    old = stub_tables(level, True); new = stub_tables(level, False)
    k = new['c']['ntype']
    if k > c['ntype'] or old['c']['ntype'] != k: raise SystemExit('число типов экспортёра %d больше, чем в файле %d: патч невозможен' % (k, c['ntype']))
    sec = {n: spr[parts[n][0]:parts[n][0] + parts[n][1]] for n in NAMES}
    oS, oE, oC = len(old['state']), len(old['event']), len(old['call'])
    dS = (len(new['state']) - oS) // es['state']; dE = (len(new['event']) - oE) // 4; dC = (len(new['call']) - oC) // 8
    # типы 0..k-1 — типы стейтпропов уровня, их таблицы идут в начале; типы после k (ключи, монеты, карточки…) добавляет psp_export, их записи — за ними
    suf = bytearray(sec['state'][oS:])
    for q in range(0, len(suf), es['state']):
        fe, fc = struct.unpack_from('<2H', suf, q + 18); struct.pack_into('<2H', suf, q + 18, fe + dE, fc + dC)
    tr = [bytearray(sec['type'][i * es['type']:(i + 1) * es['type']]) for i in range(c['ntype'])]
    for i in range(c['ntype']):
        if i < k: fs, ns = struct.unpack_from('<2H', new['type'], i * es['type'] + 8); struct.pack_into('<2H', tr[i], 8, fs, ns)
        else: fs, = struct.unpack_from('<H', tr[i], 8); struct.pack_into('<H', tr[i], 8, fs + dS)
    sec['type'] = b''.join(bytes(t) for t in tr)
    sec['state'] = new['state'] + bytes(suf); sec['event'] = new['event'] + sec['event'][oE:]; sec['call'] = new['call'] + sec['call'][oC:]
    c2 = dict(c); c2['nstate'] = c['nstate'] + dS; c2['nevent'] = c['nevent'] + dE; c2['ncall'] = c['ncall'] + dC
    head = b'SPR1' + struct.pack('<15I', *[c2[x] for x in HEAD_COUNTS]); head += b'\0' * (64 - len(head))
    out = bytearray(head)
    for n in NAMES:
        out += sec[n]; out += b'\0' * (-len(out) % 16)
    out += tail
    D = len(out) - len(spr)
    assert D % 16 == 0
    prp = bytearray(d[P['prop']:P['prop'] + P['prpLen']])
    so = P['spr'] - P['prop']
    after = bytes(prp[so + len(spr):])
    prp = bytearray(prp[:so]) + out + after
    struct.pack_into('<I', prp, 36, len(out)); struct.pack_into('<I', prp, 40, P['prpLen'] + D)
    for off in (44, 52, 60):   # смещения таблиц после SPR1
        v = struct.unpack_from('<I', prp, off)[0]
        if v and v >= so + len(spr): struct.pack_into('<I', prp, off, v + D)
    res = bytearray(d[:P['prop']]) + prp + d[P['prop'] + P['prpLen']:]
    if D:
        for h in (88, 100):
            v = struct.unpack_from('<I', res, h)[0]
            if v: struct.pack_into('<I', res, h, v + D)
        pvs = P['pvs'] + D; regf = pvs - 16
        if res[regf:regf + 4] == b'REGF':
            at = struct.unpack_from('<I', res, regf + 4)[0]; struct.pack_into('<I', res, regf + 4, at + D)
        for i in range(P['ncell']):
            q = P['cellOff'] + 48 * i + 36; v = struct.unpack_from('<I', res, q)[0]; struct.pack_into('<I', res, q, v + D)
    return bytes(res), D, c, c2


def glass_patch(level, d):
    """№1352: цвет вершин стекла автомата -> нейтральный. Возвращает (новые байты, число изменённых вершин)."""
    P = level_parts(d); es = esize(None)
    spr = bytearray(d[P['spr']:P['spr'] + P['sprLen']]); c, parts, _ = parse_spr1(bytes(spr))
    tr, st, ca, pc, vb = parts['type'][0], parts['state'][0], parts['call'][0], parts['piece'][0], parts['vert'][0]
    changed = 0
    for i in range(c['ntype']):
        t = tr + i * es['type']; fp, npc, fs, ns = struct.unpack_from('<2H', spr, t)[0], struct.unpack_from('<H', spr, t + 2)[0], struct.unpack_from('<H', spr, t + 8)[0], struct.unpack_from('<H', spr, t + 10)[0]
        vend = False
        for k in range(ns):
            q = st + (fs + k) * es['state']; ncall = spr[q + 6]; fc, = struct.unpack_from('<H', spr, q + 20)
            for j in range(ncall):
                if spr[ca + (fc + j) * 8] == 2: vend = True   # SP_OP_COINS_Z
        if not vend: continue
        for k in range(npc):
            o = pc + (fp + k) * es['piece']; nv = struct.unpack_from('<I', spr, o + 24)[0]; voff = struct.unpack_from('<I', spr, o + 32)[0]
            if nv != 6: continue
            for v in range(6):
                a = vb + voff + v * 16 + 4
                if struct.unpack_from('<I', spr, a)[0] != 0xFFFDFDFD: struct.pack_into('<I', spr, a, 0xFFFDFDFD); changed += 1
    return d[:P['spr']] + bytes(spr) + d[P['spr'] + P['sprLen']:], changed

def verify(level, d):
    P = level_parts(d); es = esize(None)
    spr = d[P['spr']:P['spr'] + P['sprLen']]; c, parts, _ = parse_spr1(spr)
    old = stub_tables(level, True); k = old['c']['ntype']; ok = k <= c['ntype']
    print('  типов в файле %d, из них стейтпропов уровня (от экспортёра) %d; остальные добавлены psp_export (ключи, карточки…) и не трогаются' % (c['ntype'], k))
    for n in ('state', 'event', 'call'):
        a = spr[parts[n][0]:parts[n][0] + len(old[n])]; same = a == old[n]
        print('  %-6s первые %d Б таблицы файла и экспортёр(по-старому): %s' % (n, len(old[n]), 'равны' if same else 'РАЗНЫЕ')); ok &= same
    o = parts['type'][0]
    tf = all(spr[o + i * es['type'] + 8:o + i * es['type'] + 12] == old['type'][i * es['type'] + 8:i * es['type'] + 12] for i in range(k))
    print('  типы: fs/ns первых %d типов: %s' % (k, 'равны' if tf else 'РАЗНЫЕ')); ok &= tf
    return ok

def diff(a, b):
    pa, pb = level_parts(a), level_parts(b)
    sa, sb = pa['spr'], pb['spr']; ea, eb = sa + pa['sprLen'], sb + pb['sprLen']
    D = len(b) - len(a)
    print('размер %d -> %d (сдвиг хвоста %+d Б), секция SPR1 @%d..%d -> @%d..%d' % (len(a), len(b), D, sa, ea, sb, eb))
    def runs(x, y, base):
        r = []; i = 0; n = min(len(x), len(y))
        while i < n:
            if x[i] != y[i]:
                j = i
                while j < n and (x[j] != y[j] or (j + 1 < n and x[j + 1] != y[j + 1])): j += 1
                r.append((base + i, base + j)); i = j
            else: i += 1
        return r
    pre = runs(a[:sa], b[:sb], 0)
    post = runs(a[ea:], b[eb:], ea)
    cls = dict(header=[], prp2=[], cell=[], other=[], regf=[])
    for lo, hi in pre:
        if hi <= 128: cls['header'].append((lo, hi))
        elif pa['prop'] <= lo < pa['prop'] + 64: cls['prp2'].append((lo, hi))
        elif any(pa['cellOff'] + 48 * i + 36 <= lo and hi <= pa['cellOff'] + 48 * i + 40 for i in range(pa['ncell'])): cls['cell'].append((lo, hi))
        else: cls['other'].append((lo, hi))
    for lo, hi in post:
        (cls['regf'] if (D and lo == pa['pvs'] - 16 + 4) else cls['other']).append((lo, hi))
    ca, pa_, ena = parse_spr1(a[sa:ea]); cb, pb_, enb = parse_spr1(b[sb:eb])
    es = esize(None); same, chg = [], []; vert_note = ''; chg_v = False
    for n in NAMES:
        x = a[sa + pa_[n][0]:sa + pa_[n][0] + pa_[n][1]]; y = b[sb + pb_[n][0]:sb + pb_[n][0] + pb_[n][1]]
        if n == 'vert' and len(x) == len(y) and x != y:   # №1352: у вершин меняется только цвет (@4..@8 из 16 Б)
            nchg = sum(1 for o in range(0, len(x), 16) if x[o:o + 4] + x[o + 8:o + 16] == y[o:o + 4] + y[o + 8:o + 16] and x[o + 4:o + 8] != y[o + 4:o + 8])
            bad = sum(1 for o in range(0, len(x), 16) if x[o:o + 4] + x[o + 8:o + 16] != y[o:o + 4] + y[o + 8:o + 16])
            vert_note = '; вершин с новым цветом %d, с другими полями %d' % (nchg, bad)
            x = b''.join(x[o:o + 4] + x[o + 8:o + 16] for o in range(0, len(x), 16)); y = b''.join(y[o:o + 4] + y[o + 8:o + 16] for o in range(0, len(y), 16))
            chg_v = True
        if n == 'type':   # у типов меняются только fs/ns (@8..@12)
            x = b''.join(x[i * es['type']:i * es['type'] + 8] + x[i * es['type'] + 12:(i + 1) * es['type']] for i in range(ca['ntype']))
            y = b''.join(y[i * es['type']:i * es['type'] + 8] + y[i * es['type'] + 12:(i + 1) * es['type']] for i in range(cb['ntype']))
        (same if x == y else chg).append(n)
        if n == 'vert' and chg_v: chg.append('vert(цвет)') if 'vert(цвет)' not in chg else None
    tail_same = a[sa + ena:ea] == b[sb + enb:eb]
    print('внутри SPR1 (@%d..%d): изменены только массивы %s; байт в байт: %s, хвост BQG1: %s%s' % (sa, ea, chg, same, 'равен' if tail_same else 'ОТЛИЧАЕТСЯ', vert_note))
    if set(chg) - {'state', 'event', 'call', 'vis', 'vert(цвет)'} or not tail_same or (chg_v and 'другими полями 0' not in vert_note): other = [(sa, ea)]
    else: other = []
    print('вне SPR1: заголовок level.shr %s; поля заголовка PRP2 %s; клеточная таблица @36: %d записей; REGF.at %s; прочих отличий: %d %s' % (
        cls['header'], cls['prp2'], len(cls['cell']), cls['regf'], len(cls['other']), cls['other'][:6]))
    return cls['other'] + other, D

def main():
    a = sys.argv[1:]
    if a[:1] == ['--diff']:
        other, D = diff(open(a[1], 'rb').read(), open(a[2], 'rb').read()); sys.exit(1 if other else 0)
    n = int(a[0]); d = open(a[1], 'rb').read()
    if '--verify' in a: sys.exit(0 if verify(n, d) else 1)
    res = d; msg = []
    if os.environ.get('VEND1328_OLD') != '1' and n in (2, 3, 4, 5, 7):
        if not verify(n, d): raise SystemExit('таблицы файла не совпадают с экспортёром по-старому — патч небезопасен')
        res, D, c, c2 = patch(n, d)
        msg.append('L%d: состояний %d -> %d, событий %d -> %d, вызовов %d -> %d; размер %+d Б' % (n, c['nstate'], c2['nstate'], c['nevent'], c2['nevent'], c['ncall'], c2['ncall'], D))
    if os.environ.get('GLASS1352_OLD') != '1':
        res, k = glass_patch(n, res); msg.append('L%d: цвет стекла автомата -> без тона у %d вершин' % (n, k))
    open(a[2], 'wb').write(res)
    print('записано %s; %s' % (a[2], '; '.join(msg) if msg else 'без изменений (оба выключателя)'))

if __name__ == '__main__':
    main()
