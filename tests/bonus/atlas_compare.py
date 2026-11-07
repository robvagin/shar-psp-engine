#!/usr/bin/env python3
"""TEST90: добавленные в атлас bart_16bit знаки (ÈÉ º ª и др.) не портят HUD основной игры: сравнение пакета до (BONUSHUDCH_OLD=1) и после.
Для каждой грани обоих пакетов: каждый знак, который был в старом, есть в новом с теми же w, left, adv и теми же точками рисунка (по u, v, высоте ячейки, из страницы шрифтов, расплетённой из swizzle);
все прочие текстуры пакета (спрайты, иконки, карта радара) — байт в байт; число знаков могло только вырасти.
  python3 tests/bonus/atlas_compare.py ui_старый.shr ui_новый.shr       код 1 — старый знак изменился или сдвинулся по смыслу рисунка"""
import struct, sys

def load(p):
    d = open(p, 'rb').read(); v = struct.unpack_from('<19I', d, 4)
    ntex, offTex, nfont, offFont, nstr, offStr = v[1], v[2], v[7], v[8], v[9], v[10]
    tex = [struct.unpack_from('<4H6I', d, offTex + 32 * i) for i in range(ntex)]   # psm, flags, w, h, offPix, offClut, nclut, bufw, rows, res
    fonts = {}
    for i in range(nfont):
        o = offFont + i * 2064; t, lh, base, pad, name, res = struct.unpack_from('<4HII', d, o)
        so = struct.unpack_from('<I', d, offStr + 4 * name)[0]; nm = d[so:so + 48].split(b'\0')[0].decode('latin-1')
        g = [struct.unpack_from('<2HbbBB', d, o + 16 + 8 * c) for c in range(256)]   # u, v, w, left, adv, flags (w как знаковый — в пакете без знака; берём байт ниже)
        gw = [d[o + 16 + 8 * c + 4] for c in range(256)]
        fonts[(nm, i)] = dict(tex=t, lh=lh, pad=pad, g=g, gw=gw)
    return d, tex, fonts

def unswz4(d, off, w, h):
    wb = w // 2; out = bytearray(wb * h); bpr = wb // 16; p = off
    for by in range(0, h, 8):
        for bx in range(bpr):
            for r in range(8):
                o = (by + r) * wb + bx * 16; out[o:o + 16] = d[p:p + 16]; p += 16
    return bytes(out)

def pix(img, wb, x, y):
    b = img[y * wb + x // 2]; return (b >> 4) if x & 1 else (b & 15)

a, ta, fa = load(sys.argv[1]); b, tb, fb = load(sys.argv[2]); bad = 0
imgs = {}
def page(d, tex, i, tag):
    if (tag, i) not in imgs:
        psm, fl, w, h, offPix = tex[i][0], tex[i][1], tex[i][2], tex[i][3], tex[i][4]
        imgs[(tag, i)] = (unswz4(d, offPix, w, h), w // 2)
    return imgs[(tag, i)]
names_b = {nm: k for k, (nm, i) in enumerate(fb)} if False else {nm: (nm, i) for (nm, i) in fb}
for (nm, i), fa_ in fa.items():
    key = names_b.get(nm)
    if not key: print('FAIL нет грани', nm); bad = 1; continue
    fb_ = fb[key]; ia, wa = page(a, ta, fa_['tex'], 'a'); ib, wb_ = page(b, tb, fb_['tex'], 'b'); lh = fa_['lh']; gh = fa_['pad'] or lh
    old = new = diff = 0
    for c in range(256):
        ua, va, _, la, ada, fla = fa_['g'][c]; wa_ = fa_['gw'][c]; ub, vb, _, lb, adb, flb = fb_['g'][c]; wbb = fb_['gw'][c]
        if fla & 1:
            old += 1
            if not (flb & 1) or (wa_, la, ada) != (wbb, lb, adb): diff += 1; continue
            same = all(pix(ia, wa, ua + x, va + y) == pix(ib, wb_, ub + x, vb + y) for y in range(gh) for x in range(wa_))
            if not same: diff += 1
        elif flb & 1:
            new += 1
    ok = diff == 0
    print('%s %-18s знаков было %3d, добавлено %2d, изменилось %d' % ('ok  ' if ok else 'FAIL', nm, old, new, diff)); bad |= not ok
same_tex = 0
for i, (x, y) in enumerate(zip(ta, tb)):
    is_font = any(f['tex'] == i for f in fa.values())
    if is_font: continue
    sz = x[2] * x[3] // (2 if x[0] == 4 else 1) if x[0] in (4, 5, 6) else 0
    pa = a[x[4]:x[4] + (x[8] * x[2] // 2 if x[0] == 4 else x[8] * x[2])]; pb = b[y[4]:y[4] + (y[8] * y[2] // 2 if y[0] == 4 else y[8] * y[2])]
    ok = pa == pb; same_tex += ok
    if not ok: print('FAIL текстура %d (%dx%d psm %d) отличается' % (i, x[2], x[3], x[0])); bad = 1
print('прочих текстур байт в байт одинаковых: %d из %d' % (same_tex, len(ta) - len({f['tex'] for f in fa.values()})))
print('FAIL' if bad else 'PASS'); sys.exit(bad)
