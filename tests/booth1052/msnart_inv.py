# №1052: перепись рисунков msnart.shr: у каких «_shadow» и какой режим (blend 1 прозрачность, 2 сложение, 3 вычитание). Только чтение
import struct, sys
b = open(sys.argv[1], 'rb').read()
nrows, roff = struct.unpack_from('<II', b, 4)
for r in range(nrows):
    path, off, size = struct.unpack_from('<80sII', b, roff + 96 * r); path = path.split(b'\0')[0].decode()
    if size < 32: continue
    ntex, ndraw, nimg, offTex, offDraw, offImg = struct.unpack_from('<6I', b, off)
    for d in range(ndraw):
        name, nbill, offBill = struct.unpack_from('<32sII', b, off + offDraw + 64 * d); name = name.split(b'\0')[0].decode()
        if not (name.lower().endswith('_shadow') or 'phone' in name.lower()): continue
        bl = []
        for k in range(nbill):
            tex, blend, mode, zw = struct.unpack_from('<4I', b, off + offBill + 80 * k); bl.append(blend)
        print('%-44s %-26s nbill %d blends %s' % (path[-44:], name, nbill, bl))
