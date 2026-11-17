#!/bin/bash
# 1342 (Роб, 108 на PSP: «у коллекционной карточки нет светяшки блинк-блинк, похожей на гаечный ключ»): над карточкой в мире — те же две фиолетовые звезды-блика (цвета card_idle диска
# (133,38,255) и (184,38,255), 6,26 м), что над ключом; при подборе — вспышка. Старое: только белое пятно.
#   tests/collect/cardglint1342.sh ПАКЕТ     — гоняет новое и красное (flag cardglint1342old 1), сверяет строки CARDGLINT1342 и пиксели
# Кадры: build/cardglint1342/{new,old}_{card_a,card_b,pick_a,pick_b,pick_c,wr_a}.png, сводный side.png (карточка новая | карточка старая | ключ). L1, карточка card17 у трейлеров (407.99,4.24,-413.09, на уровне земли; карточка у (224.3,7.2,201.5) висит высоко), ключ у (156.7,5.2,159.9).
cd "$(dirname "$0")/../.."; P=${1:?пакет}; O=build/cardglint1342; mkdir -p $O; T=${TMPDIR:-/tmp}/cg1342.$$; mkdir -p $T; fail=0
scn() { { echo "scene g1342"; [ "$1" = old ] && echo "flag cardglint1342old 1"; cat <<'S'
wait 20
foot 395 -413.09 1.57
cam 416 5.3 -413.09 -1 -0.04 0
wait 3
shot card_a
wait 0.7
shot card_b
foot 150 160 1.57
cam 164 6.4 159.9 -1 -0.05 0
wait 3
shot wr_a
foot 395 -413.09 1.57
cam 416 5.3 -413.09 -1 -0.04 0
wait 2
foot 407.99 -413.09 0 4.6
camrel 6 0.6 0 -1 0 0
wait 0.25
shot pick_a
wait 0.12
shot pick_b
wait 0.12
shot pick_c
end
S
  }; }
for m in new old; do scn $m > $T/$m.scn; SMOKE_BALLAST=5 SMOKE_HEADLESS=1 SMOKE_SCN=$T/$m.scn SMOKE_MAXT=200 tools/smoke.sh "$P" >/dev/null 2>&1; D=$(ls -td build/smoke/2* | head -1)
  for f in card_a card_b wr_a pick_a pick_b pick_c; do cp -c $D/$f.png $O/${m}_$f.png 2>/dev/null; done
  grep -a "^CARDGLINT1342" $D/smoke.txt > $T/$m.log; done
sn=$(grep -a -c "^CARDGLINT1342 stars=[1-9]" $T/new.log); fl=$(grep -a -c "^CARDGLINT1342 flash" $T/new.log); so=$(grep -a -c "^CARDGLINT1342" $T/old.log)
echo "строки: новое stars=$sn flash=$fl, красное $so"
[ "$sn" -ge 1 ] && [ "$fl" -ge 1 ] && [ "$so" = 0 ] || { echo "нет строк блика у нового или есть у красного"; fail=1; }
python3 - "$O" <<'PY' || fail=1
import sys, numpy as np
from PIL import Image
o=sys.argv[1]
def diff(f):
    A=np.asarray(Image.open(f'{o}/new_{f}.png').convert('RGB'),np.int16); B=np.asarray(Image.open(f'{o}/old_{f}.png').convert('RGB'),np.int16)
    return int((np.abs(A-B).max(-1)>20).sum())
ok=True
for f,lim in (('card_a',1500),('card_b',1500),('pick_a',3000),('pick_b',500)):
    d=diff(f); print(f'{f}: пикселей, где новое (блик/вспышка) отличается от старого: {d} (нужно >= {lim})')
    if d<lim: ok=False
ims=[Image.open(f'{o}/{x}.png').convert('RGB') for x in ('new_card_a','old_card_a','new_wr_a')]
w=sum(i.width for i in ims); s=Image.new('RGB',(w,ims[0].height)); x=0
for i in ims: s.paste(i,(x,0)); x+=i.width
s.save(f'{o}/side.png'); print('side.png: карточка новая | карточка старая | ключ')
sys.exit(0 if ok else 1)
PY
rm -rf $T
[ $fail = 0 ] && { echo PASS; exit 0; }; echo FAIL; exit 1
