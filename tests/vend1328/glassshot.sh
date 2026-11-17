#!/bin/bash
# №1352: крупный снимок стекла автомата (Красти) на уровне N — для пипетки. tests/vend1328/glassshot.sh НАБОР EBOOT УРОВЕНЬ ВЫХОД.png [флаги движка через пробел: «flag имя знач» строками в FLAGS]
# Камера в 1,6 м перед стеклом, смотрит в центр, fov 30; машина игрока в 6 м (чтобы не загораживала). Рядом с ВЫХОД.png кладёт ВЫХОД.json: углы стекла, камера, цвет вершин.
set -u; cd "$(dirname "$0")/../.."; BASE="$1"; EB="$2"; N="$3"; OUTP="$4"; PY=.venv/bin/python; OUT=build/vend1328/gs; mkdir -p "$OUT"
f="$BASE/level.shr"; [ "$N" != 1 ] && f="$BASE/l$N/level.shr"; [ -n "${LEVELFILE:-}" ] && f="$LEVELFILE"
[ -x "$OUT/probe" ] || cc -std=c11 -O1 -w -I viewer tests/vend1328/probe.c viewer/props.c viewer/stateprops.c viewer/col.c viewer/car.c viewer/coins.c -lm -o "$OUT/probe"
"$OUT/probe" "$f" glass v > "$OUT/g$N.txt"
S="$OUT/set$N"; rm -rf "$S"; cp -Rc "$BASE" "$S"; cp "$EB" "$S/EBOOT.PBP"; [ -n "${LEVELFILE:-}" ] && { [ "$N" = 1 ] && cp "$f" "$S/level.shr" || cp "$f" "$S/l$N/level.shr"; }
sc="$OUT/sc$N.txt"
$PY - "$OUT/g$N.txt" "$N" "$OUTP" > "$sc" <<'PY'
import sys, math, json
L = open(sys.argv[1]).read().split('\n'); n = int(sys.argv[2])
gv = [list(map(float, l.split()[2:5])) + [float(l.split()[6]), float(l.split()[8])] for l in L if l.startswith('GV ')]
col = [l.split()[-1] for l in L if l.startswith('GLASS')][0]
xs = [g[0] for g in gv]; ys = [g[1] for g in gv]; zs = [g[2] for g in gv]
G = [sum(xs) / 6, sum(ys) / 6, sum(zs) / 6]
# нормаль плоскости: из двух рёбер; лицо — в сторону экземпляра, у которого стекло (по GI: стекло спереди от начала)
gi = [float(x) for x in [l for l in L if l.startswith('GI ')][0].split()[1:4]]
e1 = [gv[1][i] - gv[0][i] for i in range(3)]; e2 = [gv[2][i] - gv[0][i] for i in range(3)]
nrm = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
ln = math.sqrt(sum(x * x for x in nrm)); nrm = [x / ln for x in nrm]
if sum(nrm[i] * (G[i] - gi[i]) for i in (0, 2)) < 0: nrm = [-x for x in nrm]   # наружу от машины
D = float(__import__('os').environ.get('GD', '1.6')); FOV = float(__import__('os').environ.get('GF', '30'))
eye = [G[i] + D * nrm[i] for i in range(3)]; look = [-D * nrm[i] for i in range(3)]
car = [G[0] + (D + 5) * nrm[0], G[2] + (D + 5) * nrm[2]]; h = math.atan2(-nrm[0], -nrm[2])
json.dump(dict(gv=gv, G=G, nrm=nrm, eye=eye, look=look, fov=FOV, color=col, level=n), open(sys.argv[3].replace('.png', '.json'), 'w'))
sc = ["flag nosave"]
if n != 1: sc += [f"flag golevel {n}", "wait 40"]
sc += ["scene a", "wait 3", f"goto {car[0]:.2f} {car[1]:.2f} {h:.4f}", "wait 14"]
sc += [x for x in __import__('os').environ.get('FLAGS', '').split(';') if x.strip()]
sc += [f"cam {eye[0]:.3f} {eye[1]:.3f} {eye[2]:.3f} {look[0]:.3f} {look[1]:.3f} {look[2]:.3f} {FOV}", "wait 1.5", "flag glass1352", "shot gm", "wait 1", "end"]
print("\n".join(sc))
PY
R=$(SHIP_LEVELS="2 3 4 5 6 7" SMOKE_HEADLESS=1 SMOKE_BALLAST=5 SMOKE_MAXT=300 SMOKE_SCN="$sc" tools/smoke.sh "$S" 2>&1 | sed -n 's/^отчёт: \(.*\)\/summary\.txt.*/\1/p')
case "$R" in /*) ;; *) R="$PWD/$R" ;; esac; rm -rf "$S"
cp "$(ls "$R"/*gm*.png | head -1)" "$OUTP"
$PY - "$R/smoke.txt" "${OUTP%.png}.json" <<'PY'
import sys, json
L = [l for l in open(sys.argv[1], errors='replace') if l.startswith('GLASS1352')]
if L:   # углы стекла в позе кадра снимка (у автомата качание в покое) — от движка, не от пробы
    v = L[-1].split(' v=')[1].strip().rstrip(';').split(';'); gv = [[float(x) for x in q.split(',')] for q in v]
    J = json.load(open(sys.argv[2])); J['gv'] = gv; J['color'] = L[-1].split('color=')[1].split()[0]; J['gv_from'] = 'engine'; json.dump(J, open(sys.argv[2], 'w'))
PY
echo "снимок $OUTP"
