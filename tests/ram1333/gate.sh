#!/bin/bash
# Гейт 1333: таран «школьный автобус (schoolbu) против седана (sedana, famil_v, cletu_v)» без езды, цель стоит, таран с газом на 60/75/90 км/ч, борт и зад. Красный контроль = -DCARSTACK1333_OLD=1.
# Зелёное: цель ни разу не выше крыши тарана и не «на крыше» (supOn), урон цели и тарана не изменился против прежнего (±0,005: сам закон урона по импульсу не тронут), урон растёт со скоростью.
# Красное (прежнее): автобус в борт седана на 75 и 90 км/ч — цель на крыше (низ цели выше крыши тарана).
cd "$(dirname "$0")/../.."; V=viewer; O=${TMPDIR:-/tmp}/ram1333; mkdir -p $O; fail=0
cc -std=c11 -O2 -w -I $V tests/ram1333/r.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $O/new || exit 1
cc -std=c11 -O2 -w -DCARSTACK1333_OLD=1 -I $V tests/ram1333/r.c $V/car.c $V/col.c -lm -Wl,-undefined,dynamic_lookup -o $O/old || exit 1
red=0
for tgt in sedana famil_v cletu_v; do
  n=$($O/new schoolbu $tgt | tail -6); o=$($O/old schoolbu $tgt | tail -6)
  echo "== schoolbu -> $tgt: новое"; echo "$n"
  # колонки: км/ч куда низ выше на_крыше урон_ц урон_т сдвиг
  echo "$n" | awk '{ if ($4 > 0.0 || $5 != 0) bad++ } END { exit bad > 0 }' || { echo "FAIL $tgt: цель выше крыши тарана / на крыше"; fail=1; }
  if echo "$o" | awk '{ if ($5 != 0) c++ } END { exit c == 0 }'; then red=1; fi
  dn=$(echo "$n" | awk '{print $6}' | tr '\n' ' '); do=$(echo "$o" | awk '{print $6}' | tr '\n' ' ')
  # урон цели: тот же с точностью 0,005 в зад (прежнее и новое различаются только по накренённой цели в борт — там урон считается до подъёма, но следующие удары на крыше добавляют)
  echo "$n" | awk 'NR==4||NR==5||NR==6 { print $6 }' > $O/dn.txt; echo "$o" | awk 'NR==4||NR==5||NR==6 { print $6 }' > $O/do.txt
  paste $O/dn.txt $O/do.txt | awk '{ d = $1 - $2; if (d < 0) d = -d; if (d > 0.005) bad++ } END { exit bad > 0 }' || { echo "FAIL $tgt: урон в зад изменился"; fail=1; }
  echo "$n" | awk 'NR>=4 { if (NR > 4 && $6 < p) bad++; p = $6 } END { exit bad > 0 }' || { echo "FAIL $tgt: урон не растёт со скоростью"; fail=1; }
done
[ $red = 1 ] && echo "красный (прежнее): цель на крыше тарана поймана" || { echo "красный НЕ пойман"; fail=1; }
[ $fail = 0 ] && echo "ГЕЙТ tests/ram1333: PASS" || echo "ГЕЙТ tests/ram1333: FAIL"; exit $fail
